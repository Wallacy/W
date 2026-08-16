import crypto from "node:crypto";

export class Avf0Error extends Error {
  constructor(code, details = {}) {
    super(code);
    this.code = code;
    this.details = details;
  }
}

const DIGEST = /^sha256:[0-9a-f]{64}$/u;
const RESULT_KEYS = new Set(["expected", "status", "route", "result", "accepted", "available", "selected", "granted"]);

function fail(code, details = {}) {
  throw new Avf0Error(code, details);
}

function object(value, code) {
  if (!value || typeof value !== "object" || Array.isArray(value)) fail(code);
  return value;
}

function closed(value, allowed, code) {
  object(value, code);
  for (const key of Object.keys(value)) if (!allowed.has(key)) fail(code, { key });
  return value;
}

function rejectResultEcho(value, path = "input") {
  if (!value || typeof value !== "object") return;
  if (Array.isArray(value)) return value.forEach((entry, index) => rejectResultEcho(entry, `${path}[${index}]`));
  for (const [key, child] of Object.entries(value)) {
    if (RESULT_KEYS.has(key)) fail("callerResultEcho", { path: `${path}.${key}` });
    rejectResultEcho(child, `${path}.${key}`);
  }
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") {
    return Object.fromEntries(Object.keys(value).sort().map((key) => [key, canonical(value[key])]));
  }
  return value;
}

export function digestRecord(tag, value) {
  return `sha256:${crypto.createHash("sha256").update(`${tag}\0${JSON.stringify(canonical(value))}`, "utf8").digest("hex")}`;
}

function version(value, code) {
  const match = /^(\d+)\.(\d+)(?:\.(\d+))?$/u.exec(value ?? "");
  if (!match) fail(code);
  return match.slice(1).map((part) => Number(part ?? 0));
}

function compareVersion(left, right) {
  const a = version(left, "availabilityVersionInvalid");
  const b = version(right, "availabilityVersionInvalid");
  for (let index = 0; index < 3; index += 1) if (a[index] !== b[index]) return a[index] - b[index];
  return 0;
}

function subset(required = [], offered = []) {
  const available = new Set(offered);
  return required.every((entry) => available.has(entry));
}

export function reducePackageFeature(input) {
  closed(input, new Set(["declarations", "requests", "sourceForms"]), "packageFeatureInputInvalid");
  if (!Array.isArray(input.declarations) || !Array.isArray(input.requests) || !Array.isArray(input.sourceForms)) fail("packageFeatureInputInvalid");
  if (input.sourceForms.length > 0) fail("sourceFeatureConditionalRejected", { forms: input.sourceForms });
  const declarations = new Map();
  for (const declaration of input.declarations) {
    closed(declaration, new Set(["name", "enables"]), "featureDeclarationInvalid");
    if (typeof declaration.name !== "string" || declaration.name === "" || declarations.has(declaration.name) || !Array.isArray(declaration.enables)) fail("featureDeclarationInvalid");
    const enabled = [];
    for (const item of declaration.enables) {
      closed(item, new Set(["kind", "id", "operation"]), "featureEnableInvalid");
      if (!new Set(["dependency", "moduleSet", "resource", "action"]).has(item.kind)) fail("featureEnableKindRejected", { kind: item.kind });
      if (item.operation !== "add") fail("featureMustBeAdditive");
      if (typeof item.id !== "string" || item.id === "") fail("featureEnableInvalid");
      enabled.push(`${item.kind}:${item.id}`);
    }
    declarations.set(declaration.name, enabled);
  }
  const origins = [];
  const selection = new Set();
  for (const request of input.requests) {
    closed(request, new Set(["feature", "origin"]), "featureRequestInvalid");
    if (!declarations.has(request.feature)) fail("featureUnknown", { feature: request.feature });
    if (!new Set(["product", "dependency-edge"]).has(request.origin)) fail("hiddenFeatureDefaultRejected", { origin: request.origin });
    origins.push(`${request.origin}:${request.feature}`);
    for (const item of declarations.get(request.feature)) selection.add(item);
  }
  return {
    status: "accepted",
    route: "current",
    code: "packageFeatureSelected",
    graphSelection: [...selection].sort(),
    origins: origins.sort(),
  };
}

function validateAvailabilityDeclaration(declaration) {
  closed(declaration, new Set(["symbol", "domain", "introduced", "deprecated", "obsoleted", "unavailable", "renamed", "requirements"]), "availabilityDeclarationInvalid");
  if (typeof declaration.symbol !== "string" || typeof declaration.domain !== "string") fail("availabilityDeclarationInvalid");
  if (declaration.introduced) version(declaration.introduced, "availabilityVersionInvalid");
  if (declaration.deprecated) version(declaration.deprecated, "availabilityVersionInvalid");
  if (declaration.obsoleted) version(declaration.obsoleted, "availabilityVersionInvalid");
  const requirements = declaration.requirements ?? { capabilities: [], effects: [] };
  closed(requirements, new Set(["capabilities", "effects"]), "availabilityRequirementsInvalid");
  if (!Array.isArray(requirements.capabilities) || !Array.isArray(requirements.effects)) fail("availabilityRequirementsInvalid");
  return { ...declaration, requirements };
}

export function reduceAvailability(input) {
  closed(input, new Set(["declaration", "target", "use", "authority"]), "availabilityInputInvalid");
  const declaration = validateAvailabilityDeclaration(object(input.declaration, "availabilityDeclarationInvalid"));
  const target = closed(input.target, new Set(["domain", "minimum"]), "availabilityTargetInvalid");
  const use = closed(input.use, new Set(["mode", "evidence"]), "availabilityUseInvalid");
  const authority = closed(input.authority, new Set(["capabilities", "effects"]), "availabilityAuthorityInvalid");
  if (target.domain !== declaration.domain) fail("availabilityDomainMismatch");
  version(target.minimum, "availabilityVersionInvalid");
  if (!Array.isArray(authority.capabilities) || !Array.isArray(authority.effects)) fail("availabilityAuthorityInvalid");
  if (!subset(declaration.requirements.capabilities, authority.capabilities)) fail("availabilityCannotGrantCapability");
  if (!subset(declaration.requirements.effects, authority.effects)) fail("availabilityCannotGrantEffect");
  if (declaration.unavailable === true) fail("symbolUnavailable");
  if (declaration.renamed && use.mode === "direct") fail("symbolRenamed", { renamed: declaration.renamed });
  if (declaration.obsoleted && compareVersion(target.minimum, declaration.obsoleted) >= 0) fail("symbolObsoleted");

  const targetSatisfies = !declaration.introduced || compareVersion(target.minimum, declaration.introduced) >= 0;
  const warning = declaration.deprecated && compareVersion(target.minimum, declaration.deprecated) >= 0 ? "deprecated" : null;
  if (use.mode === "direct") {
    if (!targetSatisfies) fail("availabilityBindingRequired");
    return { status: "accepted", route: "current", code: "directAvailabilityProved", warning, boundSymbol: declaration.symbol };
  }
  if (use.mode !== "bind") fail("availabilityUseInvalid");
  const evidence = closed(use.evidence, new Set(["source", "domain", "version", "generation", "providerDigest"]), "availabilityEvidenceInvalid");
  if (evidence.source !== "provider") fail("availabilityEvidenceNotAuthoritative");
  if (evidence.domain !== declaration.domain || typeof evidence.generation !== "string" || !DIGEST.test(evidence.providerDigest ?? "")) fail("availabilityEvidenceInvalid");
  if (compareVersion(evidence.version, declaration.introduced ?? "0.0") < 0) {
    return { status: "accepted", route: "research", code: "availabilityFallback", warning: null, boundSymbol: null, evidenceGeneration: evidence.generation };
  }
  if (declaration.obsoleted && compareVersion(evidence.version, declaration.obsoleted) >= 0) fail("symbolObsoleted");
  return { status: "accepted", route: "research", code: "availabilityBound", warning, boundSymbol: declaration.symbol, evidenceGeneration: evidence.generation };
}

function stableBucket(subject, key) {
  const bytes = crypto.createHash("sha256").update(`${key}\0${subject}`, "utf8").digest();
  return bytes.readUInt32BE(0) % 10_000;
}

function validateFeatureKey(key) {
  closed(key, new Set(["name", "semanticType", "values", "fallback", "contextFields", "owner", "expires"]), "runtimeFeatureKeyInvalid");
  if (typeof key.name !== "string" || key.name === "" || typeof key.semanticType !== "string" || key.semanticType === "") fail("runtimeFeatureKeyInvalid");
  if (!Array.isArray(key.values) || key.values.length === 0 || new Set(key.values).size !== key.values.length || !key.values.includes(key.fallback)) fail("runtimeFeatureFallbackInvalid");
  if (!Array.isArray(key.contextFields) || new Set(key.contextFields).size !== key.contextFields.length) fail("runtimeFeatureContextSchemaInvalid");
  if (typeof key.owner !== "string" || key.owner === "" || typeof key.expires !== "string" || !/^\d{4}-\d{2}-\d{2}$/u.test(key.expires)) fail("runtimeFeatureLifecycleInvalid");
  return key;
}

function validateSnapshot(snapshot, key) {
  closed(snapshot, new Set(["state", "generation", "configurationDigest", "value", "rules"]), "featureSnapshotInvalid");
  if (!new Set(["fresh", "stale", "missing"]).has(snapshot.state)) fail("featureSnapshotInvalid");
  if (typeof snapshot.generation !== "string" || !DIGEST.test(snapshot.configurationDigest ?? "")) fail("featureSnapshotInvalid");
  if (snapshot.value !== undefined && !key.values.includes(snapshot.value)) fail("featureValueTypeMismatch");
  if (!Array.isArray(snapshot.rules ?? [])) fail("featureRulesInvalid");
  return snapshot;
}

function evaluateRules(key, snapshot, context) {
  const rules = [...(snapshot.rules ?? [])].sort((left, right) => left.priority - right.priority);
  if (new Set(rules.map((rule) => rule.priority)).size !== rules.length) fail("featureRulePriorityCollision");
  for (const rule of rules) {
    closed(rule, new Set(["priority", "conditions", "value", "rollout"]), "featureRuleInvalid");
    if (!Number.isSafeInteger(rule.priority) || !key.values.includes(rule.value) || !Array.isArray(rule.conditions)) fail("featureRuleInvalid");
    const matches = rule.conditions.every((condition) => {
      closed(condition, new Set(["field", "equals"]), "featureConditionInvalid");
      if (!key.contextFields.includes(condition.field)) fail("featureContextFieldUndeclared", { field: condition.field });
      return context[condition.field] === condition.equals;
    });
    if (!matches) continue;
    if (rule.rollout) {
      closed(rule.rollout, new Set(["field", "basisPoints"]), "featureRolloutInvalid");
      if (!key.contextFields.includes(rule.rollout.field) || !Number.isSafeInteger(rule.rollout.basisPoints) || rule.rollout.basisPoints < 0 || rule.rollout.basisPoints > 10_000) fail("featureRolloutInvalid");
      const subject = context[rule.rollout.field];
      if (typeof subject !== "string" || stableBucket(subject, key.name) >= rule.rollout.basisPoints) continue;
    }
    return { value: rule.value, reason: "rule" };
  }
  return { value: snapshot.value ?? key.fallback, reason: snapshot.value === undefined ? "fallback" : "configured" };
}

export function reduceRuntimeFeature(input) {
  closed(input, new Set(["key", "snapshot", "evaluation", "operations", "attempts"]), "runtimeFeatureInputInvalid");
  const key = validateFeatureKey(object(input.key, "runtimeFeatureKeyInvalid"));
  const snapshot = validateSnapshot(object(input.snapshot, "featureSnapshotInvalid"), key);
  const evaluation = closed(input.evaluation, new Set(["context"]), "featureEvaluationInvalid");
  const context = object(evaluation.context, "featureEvaluationInvalid");
  for (const field of Object.keys(context)) if (!key.contextFields.includes(field)) fail("featureContextFieldUndeclared", { field });
  if (!Array.isArray(input.operations) || !Array.isArray(input.attempts)) fail("runtimeFeatureInputInvalid");
  if (input.operations.includes("evaluate-and-log")) fail("implicitExposureEffectRejected");
  if (!input.operations.includes("evaluate")) fail("featureEvaluationMissing");
  for (const attempt of input.attempts) {
    if (new Set(["grant-capability", "grant-effect", "load-module", "enable-dependency", "change-abi", "change-interface", "narrow-availability", "execute-code"]).has(attempt)) {
      fail("runtimeFeatureAuthorityRejected", { attempt });
    }
    fail("runtimeFeatureAttemptUnknown", { attempt });
  }
  const schema = {
    name: key.name,
    semanticType: key.semanticType,
    values: key.values,
    fallback: key.fallback,
    contextFields: key.contextFields,
  };
  const featureSchemaKey = digestRecord("w.feature-schema/1", schema);
  if (snapshot.state !== "fresh") {
    return {
      status: "accepted",
      route: "composable",
      code: snapshot.state === "missing" ? "featureMissingFallback" : "featureStaleFallback",
      value: key.fallback,
      reason: "fallback",
      freshness: snapshot.state,
      featureSchemaKey,
      configurationDigest: snapshot.configurationDigest,
      exposureRequired: input.operations.includes("record-exposure"),
    };
  }
  const decision = evaluateRules(key, snapshot, context);
  return {
    status: "accepted",
    route: "composable",
    code: "featureEvaluated",
    value: decision.value,
    reason: decision.reason,
    freshness: "fresh",
    featureSchemaKey,
    configurationDigest: snapshot.configurationDigest,
    exposureRequired: input.operations.includes("record-exposure"),
  };
}

export function reduceComposition(input) {
  closed(input, new Set(["order", "availability", "feature"]), "compositionInputInvalid");
  if (input.order !== "availability-then-feature") fail("featureCannotNarrowAvailability");
  const availability = reduceAvailability(input.availability);
  if (availability.boundSymbol === null) return { status: "accepted", route: "research", code: "availabilityFallbackBeforeFeature", availability, feature: null };
  const feature = reduceRuntimeFeature(input.feature);
  return { status: "accepted", route: "research", code: "availabilityAndFeatureComposed", availability, feature };
}

export function deriveAvf0Case(testCase) {
  if (!testCase || typeof testCase.id !== "string" || typeof testCase.axis !== "string") throw new Error("AVF0 case requires id and axis");
  try {
    rejectResultEcho(testCase.input);
    const result = testCase.axis === "package"
      ? reducePackageFeature(testCase.input)
      : testCase.axis === "availability"
        ? reduceAvailability(testCase.input)
        : testCase.axis === "runtime"
          ? reduceRuntimeFeature(testCase.input)
          : testCase.axis === "composition"
            ? reduceComposition(testCase.input)
            : fail("axisInvalid");
    return { caseId: testCase.id, axis: testCase.axis, ...result };
  } catch (error) {
    if (!(error instanceof Avf0Error)) throw error;
    return { caseId: testCase.id, axis: testCase.axis, status: "rejected", route: "rejected", code: error.code, details: error.details };
  }
}

function merge(base, patch) {
  const output = structuredClone(base ?? {});
  for (const [key, value] of Object.entries(patch ?? {})) {
    if (value && typeof value === "object" && !Array.isArray(value) && output[key] && typeof output[key] === "object" && !Array.isArray(output[key])) {
      output[key] = merge(output[key], value);
    } else {
      output[key] = structuredClone(value);
    }
  }
  return output;
}

export function deriveAvf0(corpus) {
  return (corpus.cases ?? []).map((testCase) => deriveAvf0Case({
    ...testCase,
    input: merge(corpus.fixtures?.[testCase.fixture], testCase.patch),
  }));
}

export function validateAvf0(corpus) {
  const errors = [];
  if (corpus?.$schema !== "w-avf0-availability-feature-cases-1") errors.push("AVF0 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("AVF0 corpus status must be design-oracle-input.");
  if (!Array.isArray(corpus?.cases) || corpus.cases.length < 30) errors.push("AVF0 requires at least 30 cases.");
  const ids = new Set();
  const axes = new Set();
  for (const [index, testCase] of (corpus.cases ?? []).entries()) {
    if (!/^AVF0-[a-z0-9]+(?:-[a-z0-9]+)*$/u.test(testCase.id ?? "")) errors.push(`cases[${index}] id is invalid.`);
    if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}.`);
    ids.add(testCase.id);
    axes.add(testCase.axis);
    if (!testCase.expected || !["accepted", "rejected"].includes(testCase.expected.status) || typeof testCase.expected.code !== "string") errors.push(`${testCase.id} expected result is invalid.`);
    if (!testCase.source || !corpus.sources?.[testCase.source]) errors.push(`${testCase.id} source is missing.`);
    if (!testCase.fixture || !corpus.fixtures?.[testCase.fixture]) errors.push(`${testCase.id} fixture is missing.`);
    if (Object.prototype.hasOwnProperty.call(testCase, "input")) errors.push(`${testCase.id} must use fixture plus patch instead of caller-owned input.`);
  }
  for (const axis of ["package", "availability", "runtime", "composition"]) if (!axes.has(axis)) errors.push(`AVF0 axis ${axis} is missing.`);
  return { errors, results: deriveAvf0(corpus) };
}
