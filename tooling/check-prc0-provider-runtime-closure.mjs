import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import corpus from "./prc0-provider-runtime-closure-cases.json" with { type: "json" };
import {
  loadPRC0Source,
  runPRC0Case,
} from "./prc0-provider-runtime-closure-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "prc0-provider-runtime-closure-cases.json");
const snapshotPath = path.join(toolingDirectory, "prc0-provider-runtime-closure-results.snapshot.jsonl");
const targetDecisions = ["W-133", "W-903", "W-1075", "W-1124", "W-1147", "W-1196", "W-1328"];
const expectedGaps = {
  "W-133": ["W-1442"],
  "W-903": ["W-1443"],
  "W-1075": ["W-1444"],
  "W-1124": ["W-1445"],
  "W-1147": ["W-1446"],
  "W-1196": ["W-1447"],
  "W-1328": ["W-1333"],
};
const expectedMissingEvidence = [
  "w-compile",
  "w-run",
  "compiler",
  "runtime",
  "provider",
  "human-study",
  "model-study",
];
const manifestPath = path.resolve(toolingDirectory, "studies/prc0-provider-runtime-closure/manifest.json");
const bundlePath = path.resolve(toolingDirectory, "studies/prc0-provider-runtime-closure/bundle.json");
const expectedArtifactRoles = {
  corpus: "tooling/prc0-provider-runtime-closure-cases.json",
  machine: "tooling/prc0-provider-runtime-closure-machine.mjs",
  "root-checker": "tooling/check-prc0-provider-runtime-closure.mjs",
  "nested-checker": "tooling/tree-sitter-w/check-prc0.mjs",
  "source-contract-checker": "tooling/check-quantity-source-contract.mjs",
  snapshot: "tooling/prc0-provider-runtime-closure-results.snapshot.jsonl",
  oracle: "tooling/studies/prc0-provider-runtime-closure/oracle.test.mjs",
  bundle: "tooling/studies/prc0-provider-runtime-closure/bundle.json",
  study: "tooling/studies/prc0-provider-runtime-closure/study.json",
  "source-base": "reference/last-light/quantity_oracle.w",
  "source-ref-sr0": "reference/last-light/supervision.w",
  "source-ref-sr0-oracle": "reference/last-light/service_recovery_oracle.w",
  "source-ref-ru0": "reference/last-light/horizon_tool.w",
  "source-ref-pyn3": "reference/last-light/pyn3_oracle.w",
  "source-ref-pyn4": "reference/last-light/tensor_interop.w",
  "source-ref-lz0": "reference/last-light/lazy_oracle.w",
  "source-ref-asc0": "reference/last-light/allocator_oracle.w",
  "source-ref-si": "std/si/contracts.w",
  "reused-sr0-corpus": "tooling/service-recovery-cases.json",
  "reused-sr0-machine": "tooling/service-recovery-machine.mjs",
  "reused-ru0-corpus": "tooling/module-run-cases.json",
  "reused-ru0-machine": "tooling/module-run-machine.mjs",
  "reused-pyn3-corpus": "tooling/presentation-cases.json",
  "reused-pyn3-machine": "tooling/presentation-machine.mjs",
  "reused-pyn4-corpus": "tooling/dlpack-cases.json",
  "reused-pyn4-machine": "tooling/dlpack-machine.mjs",
  "reused-lz0-corpus": "tooling/lazy-behavior-cases.json",
  "reused-lz0-machine": "tooling/lazy-behavior-machine.mjs",
  "reused-asc0-corpus": "tooling/allocator-scope-cases.json",
  "reused-asc0-machine": "tooling/allocator-scope-machine.mjs",
  "reused-r1-units-bundle": "tooling/studies/r1-units/bundle.json",
  "reused-r1-units-oracle": "tooling/studies/r1-units/oracle.test.mjs",
};
const sourceRefSymbols = {
  "source-base": "durationBitsAreCanonical",
  "source-ref-sr0": "OrderCoordinatorApi",
  "source-ref-sr0-oracle": "ServiceRecoveryAction",
  "source-ref-ru0": "entry",
  "source-ref-pyn3": "TabularPreview",
  "source-ref-pyn4": "ImportedTensor",
  "source-ref-lz0": "waitForWinner",
  "source-ref-asc0": "AllocatorProviderProfile",
  "source-ref-si": "TemperatureDelta",
};

function clone(value) {
  return structuredClone(value);
}

function same(actual, expected) {
  return JSON.stringify(actual) === JSON.stringify(expected);
}

function digestValue(value) {
  return `sha256:${crypto.createHash("sha256").update(JSON.stringify(value)).digest("hex")}`;
}

function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function resolveInside(relativePath, baseDirectory = repositoryRoot) {
  if (typeof relativePath !== "string" || relativePath.length === 0) return null;
  const resolved = path.resolve(baseDirectory, relativePath);
  const relative = path.relative(repositoryRoot, resolved);
  if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) return null;
  return resolved;
}

function checkSourceReference(reference, baseDirectory, location, errors) {
  const candidateBases = [baseDirectory, repositoryRoot, toolingDirectory];
  const target = candidateBases
    .map((candidate) => resolveInside(reference?.path, candidate))
    .find((candidate) => candidate && fs.existsSync(candidate));
  if (!target) {
    errors.push(`${location}.path escapes the W repository or is missing.`);
    return;
  }
  if (!fs.statSync(target).isFile()) {
    errors.push(`${location}.path does not exist: ${reference.path}`);
    return;
  }
  if (typeof reference.symbol !== "string" || reference.symbol.length === 0) {
    errors.push(`${location}.symbol is required.`);
    return;
  }
  if (!fs.readFileSync(target, "utf8").includes(reference.symbol)) {
    errors.push(`${location}.symbol is absent from ${reference.path}.`);
  }
}

function sourceCaseKindMatches(item, sourceCase, errors) {
  const acceptedKind = ["accepted", "positive"].includes(sourceCase?.kind);
  const rejectedKind = ["rejected", "negative", "fault"].includes(sourceCase?.kind);
  if (item.kind === "current-contract" && !acceptedKind) {
    errors.push(`${item.id}: current-contract must reuse an accepted/positive source route.`);
  }
  if (item.kind === "rejected-route" && !rejectedKind) {
    errors.push(`${item.id}: rejected-route must reuse a rejected/negative/fault source route.`);
  }
}

function sourceValidation(item, errors) {
  const source = item.source;
  if (!source || typeof source.kind !== "string") {
    errors.push(`${item.id}: source descriptor is required.`);
    return null;
  }
  if (source.kind === "quantity") {
    const bundlePath = resolveInside(source.bundle);
    const oraclePath = resolveInside(source.oracle);
    const fixturePath = resolveInside(source.fixture);
    if (!bundlePath || !fs.existsSync(bundlePath)) errors.push(`${item.id}: missing quantity bundle.`);
    if (!oraclePath || !fs.existsSync(oraclePath)) errors.push(`${item.id}: missing quantity oracle.`);
    if (!fixturePath || !fs.existsSync(fixturePath)) errors.push(`${item.id}: missing quantity fixture.`);
    if (bundlePath && fs.existsSync(bundlePath)) {
      const bundle = JSON.parse(fs.readFileSync(bundlePath, "utf8"));
      if (bundle.status !== "design-oracle-input") errors.push(`${item.id}: quantity bundle status is not design-oracle-input.`);
      const variant = (bundle.variants ?? []).find((entry) => entry.id === source.variant);
      if (!variant) errors.push(`${item.id}: quantity variant ${source.variant} is absent from the bundle.`);
      else {
        const variantPath = resolveInside(variant.path, path.dirname(bundlePath));
        if (!variantPath || !fs.existsSync(variantPath) || !fs.statSync(variantPath).isFile()) {
          errors.push(`${item.id}: quantity variant path is missing: ${variant.path}.`);
        }
      }
    }
    if (fixturePath && fs.existsSync(fixturePath) && !fs.readFileSync(fixturePath, "utf8").includes(source.symbol ?? "")) {
      errors.push(`${item.id}: quantity oracle symbol ${source.symbol} is absent.`);
    }
    return { corpus: null, case: null };
  }
  const corpusPathResolved = resolveInside(source.corpus);
  if (!corpusPathResolved || !fs.existsSync(corpusPathResolved)) {
    errors.push(`${item.id}: source corpus is missing.`);
    return null;
  }
  const sourceCorpus = JSON.parse(fs.readFileSync(corpusPathResolved, "utf8"));
  if (sourceCorpus.status !== "design-oracle-input") errors.push(`${item.id}: source corpus is not design-oracle-input.`);
  const sourceCase = (sourceCorpus.cases ?? []).find((entry) => entry.id === source.caseId);
  if (!sourceCase) {
    errors.push(`${item.id}: source case ${source.caseId} is absent.`);
    return { corpus: sourceCorpus, case: null };
  }
  const sourceRoute = sourceCase.kind ?? sourceCase.expected?.status;
  const sourceBase = path.dirname(corpusPathResolved);
  for (const [index, reference] of (sourceCase.references ?? []).entries()) {
    checkSourceReference(reference, sourceBase, `${item.id}.source.references[${index}]`, errors);
  }
  if (sourceCase.source) checkSourceReference(sourceCase.source, sourceBase, `${item.id}.source.source`, errors);
  for (const [index, reference] of (Array.isArray(sourceCorpus.references) ? sourceCorpus.references : []).entries()) {
    checkSourceReference(reference, sourceBase, `${item.id}.source.corpusReferences[${index}]`, errors);
  }
  if (item.kind === "current-contract" && sourceRoute !== "accepted" && sourceRoute !== "positive") {
    errors.push(`${item.id}: current-contract must reuse an accepted/positive source route.`);
  }
  if (item.kind === "rejected-route" && sourceRoute !== "rejected" && sourceRoute !== "negative" && sourceRoute !== "fault") {
    errors.push(`${item.id}: rejected-route must reuse a rejected/negative/fault source route.`);
  }
  return { corpus: sourceCorpus, case: sourceCase };
}

function checkDigestForPath(relativePath, expected, baseDirectory, location, errors) {
  const file = resolveInside(relativePath, baseDirectory);
  if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push(`${location}.path is missing or escapes the W repository.`);
    return null;
  }
  if (!/^sha256:[0-9a-f]{64}$/.test(expected ?? "")) {
    errors.push(`${location}.digest must use a lowercase sha256 digest.`);
  } else if (digestFile(file) !== expected) {
    errors.push(`${location}.digest is stale; expected ${digestFile(file)}.`);
  }
  return file;
}

export function validateBundle(bundle, { baseDirectory = path.dirname(bundlePath) } = {}) {
  const errors = [];
  if (!bundle || typeof bundle !== "object") return ["bundle must be an object"];
  if (bundle.$schema !== "w-substitution-study-bundle-1") errors.push("bundle schema");
  if (bundle.status !== "design-oracle-input") errors.push("bundle status");
  if (bundle.id !== "R1-prc0-provider-runtime-closure") errors.push("bundle id");
  if (bundle.entry !== "prc0Route") errors.push("bundle entry");
  const baseFile = checkDigestForPath(bundle.sourceBase?.path, bundle.sourceBase?.digest, baseDirectory, "bundle.sourceBase", errors);
  if (baseFile && !fs.readFileSync(baseFile, "utf8").includes(bundle.sourceBase?.symbol ?? "")) errors.push("bundle.sourceBase.symbol");
  if (!Array.isArray(bundle.sourceRefs) || bundle.sourceRefs.length !== 8) errors.push("bundle sourceRefs");
  const sourceKeys = new Set();
  for (const [index, ref] of (bundle.sourceRefs ?? []).entries()) {
    const file = checkDigestForPath(ref?.path, ref?.digest, baseDirectory, `bundle.sourceRefs[${index}]`, errors);
    if (file && typeof ref.symbol === "string" && !fs.readFileSync(file, "utf8").includes(ref.symbol)) errors.push(`bundle.sourceRefs[${index}].symbol`);
    const key = `${ref?.path}\0${ref?.symbol}`;
    if (sourceKeys.has(key)) errors.push(`bundle.sourceRefs[${index}] duplicate`);
    sourceKeys.add(key);
  }
  const variants = bundle.variants ?? [];
  if (!Array.isArray(variants) || variants.length !== 2) errors.push("bundle variants");
  const roles = variants.map((variant) => variant.role).sort();
  if (!same(roles, ["rejected-witness", "selected"])) errors.push("bundle variant roles");
  const variantIds = variants.map((variant) => variant.id).sort();
  if (!same(variantIds, ["current", "rejected"])) errors.push("bundle variant ids");
  for (const [index, variant] of variants.entries()) {
    const file = checkDigestForPath(variant.path, variant.digest, baseDirectory, `bundle.variants[${index}]`, errors);
    if (file && !fs.readFileSync(file, "utf8").includes(bundle.entry)) errors.push(`bundle.variants[${index}].entry`);
  }
  const taskKinds = (bundle.tasks ?? []).map((task) => task.kind);
  if (!same(taskKinds, ["explain", "recall", "repair", "change"])) errors.push("bundle task kinds");
  if (!same(bundle.presentationOrders, [["current", "rejected"], ["rejected", "current"]])) errors.push("bundle presentationOrders");
  if (bundle.blinding?.participantLabels?.current !== "A" || bundle.blinding?.participantLabels?.rejected !== "B") errors.push("bundle blinding labels");
  if (!same(bundle.blinding?.hide, ["id", "role", "path", "changedConstructs"])) errors.push("bundle blinding hide");
  const oracleFile = checkDigestForPath(bundle.oracle?.path, bundle.oracle?.digest, baseDirectory, "bundle.oracle", errors);
  if (oracleFile && !fs.readFileSync(oracleFile, "utf8").includes("PRC0 provider/runtime closure host oracle")) errors.push("bundle.oracle content");
  if (!Array.isArray(bundle.evidence?.current) || !Array.isArray(bundle.evidence?.missing)) errors.push("bundle evidence");
  for (const required of ["tree-sitter-parse", "host-oracle"]) if (!bundle.evidence?.current?.includes(required)) errors.push(`bundle evidence current ${required}`);
  for (const required of ["w-compile", "w-run", "human-study", "model-study"]) if (!bundle.evidence?.missing?.includes(required)) errors.push(`bundle evidence missing ${required}`);
  return errors;
}

function readJsonFile(file) {
  return JSON.parse(fs.readFileSync(file, "utf8"));
}

export function validateManifest(manifest, { bundleOverride = null } = {}) {
  const errors = [];
  if (!manifest || typeof manifest !== "object") return ["manifest must be an object"];
  const expectedKeys = ["$schema", "status", "id", "artifacts", "evidence", "stopCondition"];
  if (!same(Object.keys(manifest).sort(), [...expectedKeys].sort())) errors.push("manifest keys");
  if (manifest.$schema !== "w-prc0-provider-runtime-closure-manifest-1") errors.push("manifest schema");
  if (manifest.status !== "design-oracle-input" || manifest.id !== "PRC0") errors.push("manifest status/id");
  if (!Array.isArray(manifest.artifacts) || manifest.artifacts.length !== Object.keys(expectedArtifactRoles).length) errors.push("manifest artifact count");
  const ids = new Set();
  const roles = new Set();
  const paths = new Set();
  const byRole = new Map();
  for (const [index, artifact] of (manifest.artifacts ?? []).entries()) {
    const location = `manifest.artifacts[${index}]`;
    if (!artifact || typeof artifact !== "object" || !same(Object.keys(artifact).sort(), ["digest", "id", "path", "role"])) {
      errors.push(`${location} keys`);
      continue;
    }
    if (ids.has(artifact.id)) errors.push(`${location} duplicate id`);
    if (roles.has(artifact.role)) errors.push(`${location} duplicate role`);
    if (paths.has(artifact.path)) errors.push(`${location} duplicate path`);
    ids.add(artifact.id); roles.add(artifact.role); paths.add(artifact.path); byRole.set(artifact.role, artifact);
    if (expectedArtifactRoles[artifact.role] !== artifact.path) errors.push(`${location} role/path`);
    checkDigestForPath(artifact.path, artifact.digest, repositoryRoot, location, errors);
  }
  for (const [role, expectedPath] of Object.entries(expectedArtifactRoles)) {
    const artifact = byRole.get(role);
    if (!artifact) {
      errors.push(`manifest missing role ${role}`);
      continue;
    }
    if (artifact.path !== expectedPath) errors.push(`manifest role ${role} path`);
  }
  const bundle = bundleOverride ?? (fs.existsSync(bundlePath) ? readJsonFile(bundlePath) : null);
  errors.push(...validateBundle(bundle));
  const studyArtifact = byRole.get("study");
  if (studyArtifact && fs.existsSync(resolveInside(studyArtifact.path))) {
    const study = readJsonFile(resolveInside(studyArtifact.path));
    if (study.manifest?.path !== "manifest.json") errors.push("study must use path-only manifest reference");
  }
  const bundleArtifact = byRole.get("bundle");
  if (bundleArtifact && bundle && bundleArtifact.digest !== digestFile(bundlePath)) errors.push("manifest bundle digest");
  const bundleRefByPath = new Map([
    [bundle.sourceBase?.path, bundle.sourceBase?.digest],
    ...(bundle.sourceRefs ?? []).map((ref) => [ref.path, ref.digest]),
  ]);
  for (const role of ["source-base", "source-ref-sr0", "source-ref-sr0-oracle", "source-ref-ru0", "source-ref-pyn3", "source-ref-pyn4", "source-ref-lz0", "source-ref-asc0", "source-ref-si"]) {
    const artifact = byRole.get(role);
    const expectedDigest = bundleRefByPath.get(`../../../${artifact?.path.replaceAll("\\", "/")}`);
    if (artifact && expectedDigest && artifact.digest !== expectedDigest) errors.push(`manifest ${role} digest diverges from bundle sourceRef`);
  }
  if (!Array.isArray(manifest.evidence?.missing) || !same(manifest.evidence.missing, expectedMissingEvidence)) errors.push("manifest evidence boundary");
  return errors;
}

function validateClassificationGaps(input, errors) {
  if (!Array.isArray(input.plannedImplementationGaps) || input.plannedImplementationGaps.length !== 6) errors.push("plannedImplementationGaps");
  const plannedIds = new Set();
  for (const gap of input.plannedImplementationGaps ?? []) {
    if (plannedIds.has(gap.id)) errors.push(`planned gap duplicate ${gap.id}`);
    plannedIds.add(gap.id);
    if (!/^W-144[2-7]$/.test(gap.id) || gap.category !== "implementation-evidence-gap" || !Array.isArray(gap.missing) || !same(gap.missing, ["compiler", "runtime", "provider"])) errors.push(`planned gap ${gap.id} category/evidence`);
  }
  const classificationPath = path.join(toolingDirectory, "design-freeze-classification.json");
  if (fs.existsSync(classificationPath)) {
    const classification = readJsonFile(classificationPath);
    const existingW1333 = classification.entries?.find((entry) => entry.decisionId === "W-1333");
    if (existingW1333?.category !== "implementation-evidence-gap") errors.push("W-1333 must remain implementation-evidence-gap");
    for (const id of plannedIds) {
      const entry = classification.entries?.find((candidate) => candidate.decisionId === id);
      if (entry && entry.category !== "implementation-evidence-gap") errors.push(`${id} classification category`);
    }
  }
}

function actualField(item, actual, field) {
  const kind = item.source.kind;
  if (kind === "quantity") return actual[field];
  if (kind === "allocator") return actual.state?.[field];
  if (kind === "service-recovery") {
    const call = actual.state?.calls?.["call-order-1"];
    return {
      callPhase: call?.phase,
      durableOutcome: call?.durableOutcome,
      cleanupCount: call?.cleanupCount,
      journalRecords: actual.state?.journal?.records,
      activeTurn: actual.state?.activeTurn,
      status: actual.status,
      error: actual.error,
    }[field];
  }
  if (kind === "module-run") {
    return {
      phase: actual.state?.phase,
      cleanupDone: actual.state?.cleanup?.done,
      entry: actual.state?.run?.entry,
      status: actual.status,
      error: actual.error,
    }[field];
  }
  if (kind === "presentation") {
    return {
      phase: actual.state?.phase,
      previewCollected: actual.state?.preview?.collected,
      fallback: actual.state?.fallback,
      status: actual.status,
      error: actual.error,
    }[field];
  }
  if (kind === "dlpack") {
    return {
      phase: actual.state?.phase,
      releaseCalls: actual.state?.releaseCalls,
      deleterCalls: actual.state?.deleterCalls,
      leases: actual.state?.leases,
      status: actual.status,
      error: actual.error,
    }[field];
  }
  if (kind === "lazy") {
    return {
      phase: actual.state?.phase,
      publication: actual.state?.publication,
      initializerRuns: actual.state?.initializerRuns,
      waiterPhase: actual.state?.waiterPhases?.right,
      status: actual.status,
      error: actual.error,
    }[field];
  }
  return undefined;
}

export function validateCase(item, { checkSources = true } = {}) {
  const errors = [];
  if (!item || typeof item !== "object") return ["case must be an object"];
  if (!/^PRC0-W-(?:133|903|1075|1124|1147|1196|1328)-(?:current|adversarial)$/.test(item.id ?? "")) errors.push(`${item.id}: invalid case id.`);
  if (!["current-contract", "rejected-route"].includes(item.kind)) errors.push(`${item.id}: invalid route kind.`);
  if (!Array.isArray(item.decisions) || item.decisions.length !== 1 || !targetDecisions.includes(item.decisions[0])) errors.push(`${item.id}: case must name one target decision.`);
  if (checkSources) sourceValidation(item, errors);
  const actual = runPRC0Case(item);
  const expectedStatus = item.kind === "current-contract" ? "accepted" : "rejected";
  if (actual.status !== expectedStatus) errors.push(`${item.id}: expected ${expectedStatus}, derived ${actual.status}.`);
  for (const [field, expected] of Object.entries(item.assert ?? {})) {
    const derived = actualField(item, actual, field);
    if (!same(derived, expected)) errors.push(`${item.id}: ${field} expected ${JSON.stringify(expected)}, derived ${JSON.stringify(derived)}.`);
  }
  return errors;
}

function projectActual(item, actual) {
  const fields = Object.keys(item.assert ?? {});
  const assertions = Object.fromEntries(fields.map((field) => [field, actualField(item, actual, field)]));
  return {
    caseId: item.id,
    decisions: item.decisions,
    kind: item.kind,
    status: actual.status,
    error: actual.error ?? null,
    assertions,
    digest: digestValue({ status: actual.status, error: actual.error ?? null, assertions }),
  };
}

export function validateCorpus(input = corpus) {
  const errors = [];
  if (input.$schema !== "w-prc0-provider-runtime-closure-cases-1") errors.push("schema");
  if (input.status !== "design-oracle-input") errors.push("status");
  if (input.id !== "PRC0" || input.reuseOnly !== true) errors.push("id/reuseOnly");
  if (!same(input.decisions, targetDecisions)) errors.push("decisions must be the seven target gates in order");
  if (!same(input.evidence?.missing, expectedMissingEvidence)) errors.push("evidence.missing boundary changed");
  const forgedEvidence = ["compiler", "runtime", "provider", "human-study", "model-study"];
  if ((input.evidence?.current ?? []).some((entry) => forgedEvidence.includes(entry))) errors.push("evidence.current cannot claim implementation/provider readiness");
  if (!same(input.implementationGapMap, expectedGaps)) errors.push("implementationGapMap changed");
  validateClassificationGaps(input, errors);
  if (!Array.isArray(input.cases) || input.cases.length !== 14) errors.push("exactly fourteen PRC0 cases are required");
  const ids = new Set();
  const counts = new Map(targetDecisions.map((decision) => [decision, { current: 0, adversarial: 0 }]));
  const results = [];
  for (const item of input.cases ?? []) {
    if (ids.has(item.id)) errors.push(`${item.id}: duplicate case id`);
    ids.add(item.id);
    const decision = item.decisions?.[0];
    if (counts.has(decision)) counts.get(decision)[item.kind === "current-contract" ? "current" : "adversarial"] += 1;
    errors.push(...validateCase(item));
    results.push(projectActual(item, runPRC0Case(item)));
  }
  for (const decision of targetDecisions) {
    const count = counts.get(decision);
    if (count.current !== 1 || count.adversarial !== 1) errors.push(`${decision}: requires one current and one adversarial route`);
  }
  return { errors, results };
}

function mutationChecks() {
  const checks = {};
  const service = clone(corpus.cases.find((item) => item.id === "PRC0-W-133-current"));
  service.source.caseId = "SR0-accepted-provider-boot";
  checks.sourceRouteDriftRejected = validateCase(service).some((error) => error.includes("callPhase") || error.includes("journalRecords"));
  const quantity = clone(corpus.cases.find((item) => item.id === "PRC0-W-903-current"));
  quantity.assert.canonicalSeconds = 31;
  checks.assertionDriftRejected = validateCase(quantity, { checkSources: false }).some((error) => error.includes("canonicalSeconds"));
  const token = clone(corpus.cases.find((item) => item.id === "PRC0-W-903-adversarial"));
  token.operations[0].token = "s";
  checks.rejectedRouteDriftRejected = validateCase(token, { checkSources: false }).some((error) => error.includes("expected rejected"));
  const manifest = readJsonFile(manifestPath);
  const staleArtifact = clone(manifest);
  staleArtifact.artifacts.find((artifact) => artifact.role === "machine").digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
  checks.staleArtifactDigestRejected = validateManifest(staleArtifact).some((error) => error.includes("digest"));
  const wrongGap = clone(corpus);
  wrongGap.plannedImplementationGaps[0].category = "oracle-backed-current";
  checks.wrongGapCategoryRejected = validateCorpus(wrongGap).errors.some((error) => error.includes("planned gap"));
  const forgedProviderEvidence = clone(corpus);
  forgedProviderEvidence.evidence.missing = forgedProviderEvidence.evidence.missing.filter((entry) => entry !== "provider");
  checks.forgedProviderReadyRejected = validateCorpus(forgedProviderEvidence).errors.some((error) => error.includes("evidence.missing"));
  const duplicateDecision = clone(corpus);
  duplicateDecision.decisions[0] = duplicateDecision.decisions[1];
  checks.duplicateDecisionRejected = validateCorpus(duplicateDecision).errors.some((error) => error.includes("decisions"));
  const duplicateCase = clone(corpus);
  duplicateCase.cases[1].id = duplicateCase.cases[0].id;
  checks.duplicateCaseRejected = validateCorpus(duplicateCase).errors.some((error) => error.includes("duplicate case id"));
  const escapedSource = clone(manifest);
  escapedSource.artifacts.find((artifact) => artifact.role === "source-ref-sr0").path = "../../outside.w";
  checks.sourcePathEscapeRejected = validateManifest(escapedSource).some((error) => error.includes("escapes") || error.includes("role/path"));
  const replacedSourceRef = clone(readJsonFile(bundlePath));
  replacedSourceRef.sourceRefs[0].symbol = "not-a-contract";
  checks.replacedSourceRefRejected = validateBundle(replacedSourceRef).some((error) => error.includes("sourceRefs[0].symbol"));
  const bundleRoleDrift = clone(readJsonFile(bundlePath));
  bundleRoleDrift.variants[0].role = "alternative";
  checks.bundleRoleDriftRejected = validateBundle(bundleRoleDrift).some((error) => error.includes("variant roles"));
  const bundleSchemaDrift = clone(readJsonFile(bundlePath));
  bundleSchemaDrift.$schema = "w-prc0-provider-runtime-closure-bundle-1";
  checks.bundleSchemaDriftRejected = validateBundle(bundleSchemaDrift).some((error) => error.includes("bundle schema"));
  return checks;
}

export function main() {
  const { errors, results } = validateCorpus();
  if (fs.existsSync(manifestPath)) errors.push(...validateManifest(readJsonFile(manifestPath)));
  else errors.push("manifest is missing");
  const mutations = mutationChecks();
  for (const [name, passed] of Object.entries(mutations)) if (!passed) errors.push(`mutation ${name} did not detect drift`);
  const snapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n${JSON.stringify({ kind: "integrity-mutations", checks: mutations })}\n`;
  if (process.argv.includes("--write")) fs.writeFileSync(snapshotPath, snapshot);
  else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) errors.push("snapshot stale; run with --write");
  if (errors.length > 0) {
    process.stderr.write(`${errors.map((error) => `- ${error}`).join("\n")}\n`);
    process.exitCode = 1;
    return;
  }
  process.stdout.write(`PRC0 provider/runtime closure: ${results.length} cases, ${results.filter((result) => result.status === "accepted").length} accepted, ${results.filter((result) => result.status === "rejected").length} rejected; mutations ${Object.values(mutations).every(Boolean) ? "green" : "red"}.\n`);
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
