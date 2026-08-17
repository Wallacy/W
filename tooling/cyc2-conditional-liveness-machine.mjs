import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "cyc2-conditional-liveness-cases.json");

const BASELINE_COMPOSITIONS = new Map([
  ["generation-id-cache", "generation-id-detaches-key"],
  ["owner-scoped-cache", "owner-lease-breaks-edge"],
  ["detached-value", "detached-value-no-back-edge"],
]);
const REJECTED = new Map([
  ["naive-weak-key", "ordinary-weak-insufficient"],
  ["ephemeron", "ephemeron-not-baseline"],
  ["transparent-collector", "transparent-collector-rejected"],
  ["hidden-finalizer", "finalizer-side-effect-rejected"],
]);
const REQUIRED_SEQUENCE = ["closeAdmission", "drain", "quiesce", "census"];

function digest(value) {
  return "sha256:" + crypto.createHash("sha256").update(JSON.stringify(value)).digest("hex");
}

function clone(value) {
  return value === undefined ? undefined : structuredClone(value);
}

function stable(value) {
  if (Array.isArray(value)) return value.map(stable);
  if (!value || typeof value !== "object") return value;
  return Object.fromEntries(Object.keys(value).sort().map((key) => [key, stable(value[key])]));
}

function sourceErrors(corpus) {
  const errors = [];
  for (const [index, sourceRef] of (corpus.sourceRefs ?? []).entries()) {
    const location = `sourceRefs[${index}]`;
    const file = path.resolve(repositoryRoot, sourceRef?.path ?? "");
    const relative = path.relative(repositoryRoot, file);
    if (!sourceRef?.path || !sourceRef?.symbol || !relative || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative) || !fs.existsSync(file)) {
      errors.push(`${location} is missing or outside the repository.`);
      continue;
    }
    const source = fs.readFileSync(file, "utf8");
    const count = source.split(sourceRef.symbol).length - 1;
    if (count !== 1) errors.push(`${location}.symbol must occur exactly once; found ${count}.`);
  }
  return errors;
}

function evaluateCriteria(criteria) {
  const required = [
    "observableKeyIdentity", "allCompositionsFail", "postDrainCensus", "deterministicCleanup",
    "boundedResource", "noHiddenForeignBoundary", "independentEvidence",
  ];
  return required.every((key) => criteria?.[key] === true);
}

export function evaluateCYC2Case(rawInput) {
  const input = clone(rawInput ?? {});
  const expected = input.expected ?? {};
  let status = "rejected";
  let code = "unknown-composition";
  const diagnostics = [];
  if (BASELINE_COMPOSITIONS.has(input.kind)) {
    const expectedCode = BASELINE_COMPOSITIONS.get(input.kind);
    const composition = input.composition ?? {};
    const valid = input.kind === "generation-id-cache"
      ? composition.idDetached === true && composition.explicitInvalidation === true && composition.valueToKeyStrong === false
      : input.kind === "owner-scoped-cache"
        ? composition.ownerLease === true && composition.explicitClose === true && composition.valueToKeyStrong === false
        : composition.detached === true && composition.keyIdentityRequired === false && composition.valueToKeyStrong === false;
    if (!valid) {
      code = "composition-facts-incomplete";
      diagnostics.push({ code, facts: { kind: input.kind } });
    } else if (JSON.stringify(input.sequence ?? []) !== JSON.stringify(REQUIRED_SEQUENCE)) {
      code = "close-drain-sequence-invalid";
      diagnostics.push({ code, facts: { expected: REQUIRED_SEQUENCE, actual: input.sequence ?? [] } });
    } else {
      status = "composable-baseline";
      code = expectedCode;
    }
  } else if (REJECTED.has(input.kind)) {
    status = "intentionally-rejected";
    code = REJECTED.get(input.kind);
    if (input.kind === "transparent-collector" && input.composition?.collectorSideEffects !== true) {
      diagnostics.push({ code: "collector-facts-incomplete" });
    }
  } else if (input.kind === "implementation-evidence") {
    status = "implementation-evidence-gap";
    code = "implementation-evidence-gap";
    if (!Array.isArray(input.evidenceMissing) || input.evidenceMissing.length === 0) diagnostics.push({ code: "evidence-list-missing" });
  } else if (input.kind === "future-reopen") {
    if (evaluateCriteria(input.criteria)) {
      status = "future-reopen-candidate";
      code = "future-ephemeron-review";
    } else {
      status = "reopen-blocked";
      code = "compositions-still-close";
    }
  }
  if (["candidate-research", "research", "Research"].includes(status)) {
    diagnostics.push({ code: "active-research-not-allowed" });
    status = "intentionally-rejected";
    code = "conditional-liveness-not-baseline";
  }
  const result = {
    id: String(input.id ?? ""),
    status,
    code,
    diagnostics,
    criteria: input.kind === "future-reopen" ? { qualifies: evaluateCriteria(input.criteria), ...(input.criteria ?? {}) } : null,
    collectorSideEffects: input.composition?.collectorSideEffects === true,
    digest: digest({ id: input.id, status, code, diagnostics, criteria: input.criteria ?? null }),
  };
  if (expected.status !== undefined) result.pass = status === expected.status && code === expected.code && diagnostics.length === 0;
  else result.pass = diagnostics.length === 0;
  return result;
}

export function validateCYC2Corpus(corpus) {
  const errors = [];
  if (corpus?.$schema !== "w-cyc2-conditional-liveness-cases-1") errors.push("CYC2 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("CYC2 corpus status is invalid.");
  if (corpus?.id !== "CYC2-conditional-liveness-closure") errors.push("CYC2 corpus identity is invalid.");
  if (!Array.isArray(corpus?.cases) || corpus.cases.length < 8) errors.push("CYC2 corpus must contain at least eight cases.");
  if (!Array.isArray(corpus?.reopenCriteria) || corpus.reopenCriteria.length < 6) errors.push("CYC2 reopen criteria are incomplete.");
  errors.push(...sourceErrors(corpus));
  const ids = new Set();
  const results = [];
  for (const [index, testCase] of (corpus?.cases ?? []).entries()) {
    const location = `cases[${index}]`;
    if (!/^CYC2-[A-Z]+-[a-z0-9]+(?:-[a-z0-9]+)*$/u.test(testCase?.id ?? "")) errors.push(`${location}.id is not CYC2 kebab-case.`);
    if (ids.has(testCase?.id)) errors.push(`${location}.id is duplicated.`);
    ids.add(testCase?.id);
    const result = evaluateCYC2Case(testCase);
    results.push(result);
    if (!result.pass) errors.push(`${testCase.id} expected ${testCase.expected?.status}/${testCase.expected?.code}, got ${result.status}/${result.code}.`);
    if (result.collectorSideEffects && !["intentionally-rejected"].includes(result.status)) errors.push(`${testCase.id} exposes collector side effects outside rejection.`);
  }
  return {
    errors,
    results,
    metrics: {
      caseCount: results.length,
      baselineCompositions: results.filter((result) => result.status === "composable-baseline").length,
      intentionallyRejected: results.filter((result) => result.status === "intentionally-rejected").length,
      implementationEvidenceGaps: results.filter((result) => result.status === "implementation-evidence-gap").length,
      reopenBlocked: results.filter((result) => result.status === "reopen-blocked").length,
      futureReopenCandidates: results.filter((result) => result.status === "future-reopen-candidate").length,
      activeResearch: results.filter((result) => ["candidate-research", "research", "Research"].includes(result.status)).length,
      collectorSideEffects: results.filter((result) => result.collectorSideEffects && result.status !== "intentionally-rejected").length,
    },
  };
}

export function buildCYC2Snapshot(corpus) {
  const checked = validateCYC2Corpus(corpus);
  const records = checked.results.map((result) => ({
    schema: "w-cyc2-conditional-liveness-results-1",
    id: result.id,
    status: result.status,
    code: result.code,
    criteria: result.criteria,
    collectorSideEffects: result.collectorSideEffects,
    digest: result.digest,
  }));
  return {
    text: records.map((record) => JSON.stringify(stable(record))).join("\n") + "\n",
    metrics: checked.metrics,
    results: checked.results,
  };
}

if (import.meta.main) {
  const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
  const checked = validateCYC2Corpus(corpus);
  if (checked.errors.length > 0) {
    console.error(checked.errors.join("\n"));
    process.exitCode = 1;
  } else {
    console.log(`CYC2 oracle: ${checked.metrics.caseCount} cases, ${checked.metrics.baselineCompositions} baseline compositions, ${checked.metrics.intentionallyRejected} intentionally rejected.`);
  }
}
