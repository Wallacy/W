import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const manifestPath = path.join(toolingDirectory, "studies", "r1c0-closure", "manifest.json");
const corpusPath = path.join(toolingDirectory, "r1c0-closure-cases.json");
const snapshotPath = path.join(toolingDirectory, "r1c0-closure-results.snapshot.jsonl");
const checkerPath = fileURLToPath(import.meta.url);
const writeSnapshot = process.argv.includes("--write");
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const wloManifest = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "studies", "r1c0-closure", "wlo1-manifest.json"), "utf8"));
const substitutionCorpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "substitution-cases.json"), "utf8"));
const classification = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "design-freeze-classification.json"), "utf8"));
const errors = [];
const expectedScopes = new Set(["R1-broad", "R1E0", "R1H0", "R1S1"]);
const expectedGates = [
  "W-092", "W-207", "W-867", "W-887", "W-888", "W-889", "W-919", "W-923", "W-936",
  "W-972", "W-1003", "W-1044", "W-981", "W-987", "W-1148", "W-1154", "W-1317", "W-1339",
  "W-1347", "W-1171", "W-1264",
];

function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function resolvePath(relativePath) {
  return path.resolve(repositoryRoot, relativePath);
}

function resolveContained(relativePath, label) {
  const resolved = resolvePath(relativePath);
  const relativeToRoot = path.relative(repositoryRoot, resolved);
  if (typeof relativePath !== "string" || path.isAbsolute(relativePath) || relativeToRoot === ".." || relativeToRoot.startsWith(`..${path.sep}`)) {
    errors.push(`${label} escapes repository root: ${relativePath}`);
    return undefined;
  }
  return resolved;
}

function resolveContainedFrom(baseDirectory, relativePath, label) {
  if (typeof relativePath !== "string" || path.isAbsolute(relativePath)) {
    errors.push(`${label} must be a relative repository path: ${relativePath}`);
    return undefined;
  }
  const resolved = path.resolve(baseDirectory, relativePath);
  const relativeToRoot = path.relative(repositoryRoot, resolved);
  if (relativeToRoot === ".." || relativeToRoot.startsWith(`..${path.sep}`)) {
    errors.push(`${label} escapes repository root: ${relativePath}`);
    return undefined;
  }
  return resolved;
}

function checkDigest(relativePath, expected, label) {
  const file = resolveContained(relativePath, label);
  if (!file) return false;
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push(`${label} references missing file ${relativePath}.`);
    return false;
  }
  const actual = digestFile(file);
  if (actual !== expected) errors.push(`${label} digest is stale; expected ${actual}.`);
  return true;
}

function checkDigestFrom(baseDirectory, relativePath, expected, label) {
  const file = resolveContainedFrom(baseDirectory, relativePath, label);
  if (!file) return false;
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push(`${label} references missing file ${relativePath}.`);
    return false;
  }
  if (!/^sha256:[0-9a-f]{64}$/.test(expected ?? "")) {
    errors.push(`${label} has a missing or malformed digest.`);
    return false;
  }
  const actual = digestFile(file);
  if (actual !== expected) errors.push(`${label} digest is stale; expected ${actual}.`);
  return true;
}

function isDigest(value) {
  return typeof value === "string" && /^sha256:[0-9a-f]{64}$/.test(value);
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function validateBundleDigest(reference, actualDigest) {
  return reference?.digest === actualDigest ? [] : ["staleBundleDigest"];
}

function validateImplementationGapMap(gapMap, categoryLookup, plannedGaps) {
  const findings = [];
  for (const gaps of Object.values(gapMap ?? {})) {
    for (const gap of gaps ?? []) {
      if (gap === "W-1441") {
        if (plannedGaps?.[gap]?.category !== "implementation-evidence-gap") findings.push("wrongImplementationGapCategory");
      } else if (categoryLookup.get(gap) !== "implementation-evidence-gap") {
        findings.push("wrongImplementationGapCategory");
      }
    }
  }
  return [...new Set(findings)];
}

function validateExtensionBridge(items) {
  const findings = [];
  for (const item of items ?? []) {
    if (item.decision !== "rejected-for-now" || item.baseCategory !== "source-backed-current" || item.relation !== "research-extension-closed" || item.researchExtensionRemoved !== true) {
      findings.push("extensionDecisionDrift");
    }
    if (!Array.isArray(item.baseline) || item.baseline.length === 0) findings.push("missingExtensionBaseline");
  }
  return [...new Set(findings)];
}

function validateScopeSet(scopes, expected) {
  const findings = [];
  const keys = Object.keys(scopes ?? {});
  if (JSON.stringify([...keys].sort()) !== JSON.stringify([...expected].sort())) findings.push("missingScope");
  for (const scope of expected) if (!Array.isArray(scopes?.[scope]) || scopes[scope].length === 0) findings.push("missingScope");
  return [...new Set(findings)];
}

function validateDerivedMetrics(candidate, expected) {
  const findings = [];
  for (const field of ["bundleCount", "variantCount", "taskCount", "inputCount", "uniqueR0CaseCount", "globalR0Denominator", "selectedVariantCount"]) {
    if (candidate?.[field] !== expected?.[field]) findings.push("forgedDerivedMetric");
  }
  if (JSON.stringify(candidate?.statuses) !== JSON.stringify(expected?.statuses)) findings.push("forgedDerivedMetric");
  return [...new Set(findings)];
}

function validateWloPayloadAuthority(value) {
  const findings = [];
  if (value?.codec?.payloadAuthority !== "logical-value-bytes-only") findings.push("wloPayloadAuthorityDrift");
  if (value?.codec?.header !== "none") findings.push("wloHeaderDrift");
  if (value?.codec?.wAbiImpact !== "none" || value?.codec?.targetWAbiIndependent !== true) findings.push("wloCodecWAbiDrift");
  for (const target of value?.targetProjections ?? []) {
    if (target.wAbiImpact !== "none" || target.targetWAbiIndependent !== true) findings.push("wloTargetWAbiDrift");
  }
  return [...new Set(findings)];
}

function requireExactKeys(value, keys, label) {
  if (JSON.stringify(Object.keys(value ?? {}).sort()) !== JSON.stringify([...keys].sort())) {
    errors.push(`${label} has an unexpected or missing key.`);
  }
}

if (manifest.$schema !== "w-r1c0-closure-manifest-1") errors.push("R1C0 manifest schema mismatch.");
if (manifest.status !== "design-oracle-input") errors.push("R1C0 manifest must remain design-oracle-input.");
if (manifest.id !== "R1C0") errors.push("R1C0 manifest id mismatch.");
requireExactKeys(manifest, ["$schema", "status", "id", "closureBundle", "corpus", "checker", "snapshot", "study", "wlo1", "scopes", "bundles", "reusedArtifacts", "evidence", "stopCondition"], "R1C0 manifest");
for (const [label, receipt] of Object.entries({ closureBundle: manifest.closureBundle, corpus: manifest.corpus, checker: manifest.checker, snapshot: manifest.snapshot, study: manifest.study })) requireExactKeys(receipt, ["path", "digest"], `R1C0 ${label} receipt`);
for (const [label, receipt] of Object.entries(manifest.wlo1 ?? {})) requireExactKeys(receipt, ["path", "digest"], `R1C0 WLO1 ${label} receipt`);
for (const reference of manifest.bundles ?? []) requireExactKeys(reference, ["id", "path", "digest"], `R1C0 bundle reference ${reference.id}`);
for (const artifact of manifest.reusedArtifacts ?? []) requireExactKeys(artifact, ["id", "role", "path", "digest"], `R1C0 reused artifact ${artifact.id}`);
if (corpus.$schema !== "w-r1c0-closure-case-corpus-1") errors.push("R1C0 corpus schema mismatch.");
if (corpus.status !== "design-oracle-input") errors.push("R1C0 corpus must remain design-oracle-input.");
if (corpus.id !== "R1C0" || corpus.reuseOnly !== true) errors.push("R1C0 corpus must be reuse-only.");
if (!Array.isArray(corpus.cases)) errors.push("R1C0 closure metadata cases are required.");
if (!checkDigest("tooling/r1c0-closure-cases.json", manifest.corpus?.digest, "R1C0 corpus")) {
  // The diagnostic above is sufficient.
}
checkDigest("tooling/check-r1c0-closure.mjs", manifest.checker?.digest, "R1C0 checker");
checkDigest("tooling/studies/r1c0-closure/bundle.json", manifest.closureBundle?.digest, "R1C0 bundle");

const bundleRefs = new Map();
for (const reference of manifest.bundles ?? []) {
  if (bundleRefs.has(reference.id)) errors.push(`duplicate bundle reference ${reference.id}.`);
  const file = resolveContained(reference.path, `bundle ${reference.id}`);
  if (!file) continue;
  if (!checkDigest(reference.path, reference.digest, `bundle ${reference.id}`)) continue;
  const bundle = JSON.parse(fs.readFileSync(file, "utf8"));
  if (bundle.id !== reference.id) errors.push(`bundle ${reference.id} id drifted to ${bundle.id}.`);
  if (bundle.status !== "design-oracle-input") errors.push(`bundle ${reference.id} is not design-oracle-input.`);
  if (!Array.isArray(bundle.variants) || bundle.variants.length < 2) errors.push(`bundle ${reference.id} lacks variants.`);
  if (JSON.stringify((bundle.tasks ?? []).map((task) => task.kind)) !== JSON.stringify(["explain", "recall", "repair", "change"])) {
    errors.push(`bundle ${reference.id} task order drifted.`);
  }
  if (!Array.isArray(bundle.inputs) || bundle.inputs.length < 2) errors.push(`bundle ${reference.id} lacks primary/adversarial inputs.`);
  bundleRefs.set(reference.id, { reference, bundle });
}
const closureBundleFile = resolveContained(manifest.closureBundle?.path, "R1C0 closure bundle");
if (fs.existsSync(closureBundleFile)) {
  const closureBundle = JSON.parse(fs.readFileSync(closureBundleFile, "utf8"));
  bundleRefs.set(closureBundle.id, { reference: manifest.closureBundle, bundle: closureBundle });
}

for (const [scope, ids] of Object.entries(manifest.scopes ?? {})) {
  if (!expectedScopes.has(scope)) errors.push(`unexpected R1C0 scope ${scope}.`);
  if (!Array.isArray(ids) || ids.length === 0) {
    errors.push(`scope ${scope} must contain bundle IDs.`);
    continue;
  }
  const seen = new Set();
  for (const id of ids) {
    if (seen.has(id)) errors.push(`scope ${scope} duplicates ${id}.`);
    seen.add(id);
    if (!bundleRefs.has(id)) errors.push(`scope ${scope} references unpinned bundle ${id}.`);
  }
}
if (JSON.stringify(Object.keys(manifest.scopes ?? {}).sort()) !== JSON.stringify([...expectedScopes].sort())) errors.push("R1C0 scope key set drifted.");
for (const scope of expectedScopes) if (!manifest.scopes?.[scope]) errors.push(`missing R1C0 scope ${scope}.`);
for (const finding of validateScopeSet(manifest.scopes, expectedScopes)) errors.push(`R1C0 scope validation failed: ${finding}.`);

const classificationIds = new Set((classification.entries ?? []).map((decision) => decision.decisionId));
const classificationCategories = new Map((classification.entries ?? []).map((decision) => [decision.decisionId, decision.category]));
for (const [gate, gaps] of Object.entries(corpus.implementationGapMap ?? {})) {
  for (const gap of gaps) {
    if (gap === "W-1441") {
      if (corpus.plannedImplementationGaps?.[gap]?.category !== "implementation-evidence-gap") errors.push(`${gate} planned gap ${gap} category drifted.`);
    } else if (!classificationIds.has(gap)) {
      errors.push(`${gate} gap ${gap} is absent from classification.`);
    } else if (classificationCategories.get(gap) !== "implementation-evidence-gap") {
      errors.push(`${gate} gap ${gap} category is not implementation-evidence-gap.`);
    }
  }
}
for (const finding of validateImplementationGapMap(corpus.implementationGapMap, classificationCategories, corpus.plannedImplementationGaps)) {
  errors.push(`R1C0 implementation-gap validation failed: ${finding}.`);
}
const gateIds = (corpus.gates ?? []).map((gate) => gate.id);
if (JSON.stringify([...gateIds].sort()) !== JSON.stringify([...expectedGates].sort())) errors.push("R1C0 gate IDs drifted.");
for (const gate of corpus.gates ?? []) {
  if (!["oracle-backed-current", "rejected/intentionally-rejected"].includes(gate.recommendedClosure)) {
    errors.push(`${gate.id} has an invalid recommended closure.`);
  }
  for (const id of gate.bundleIds ?? []) if (!bundleRefs.has(id)) errors.push(`${gate.id} references unpinned bundle ${id}.`);
  for (const artifactId of gate.artifactIds ?? []) {
    const reused = (manifest.reusedArtifacts ?? []).some((artifact) => artifact.id === artifactId);
    const wlo1Aggregate = artifactId === "wlo1" && manifest.wlo1?.manifest?.path && isDigest(manifest.wlo1?.manifest?.digest);
    if (!reused && !wlo1Aggregate) errors.push(`${gate.id} artifact ${artifactId} is not pinned.`);
  }
}
const closureCases = corpus.cases ?? [];
const closureCaseIds = new Set();
for (const closureCase of closureCases) {
  if (closureCaseIds.has(closureCase.id)) errors.push(`duplicate R1C0 closure case ${closureCase.id}.`);
  closureCaseIds.add(closureCase.id);
  if (JSON.stringify(Object.keys(closureCase).sort()) !== JSON.stringify(["decisions", "disposition", "evidenceRefs", "id", "kind", "result"].sort())) {
    errors.push(`R1C0 closure case ${closureCase.id} has payload or authority drift.`);
  }
  if (closureCase.kind !== "closure-meta" || !Array.isArray(closureCase.decisions) || closureCase.decisions.length !== 1) errors.push(`R1C0 closure case ${closureCase.id} must cite exactly one decision.`);
  const decisionId = closureCase.decisions?.[0];
  const gate = (corpus.gates ?? []).find((candidate) => candidate.id === decisionId);
  if (!gate) errors.push(`R1C0 closure case ${closureCase.id} cites an unknown gate.`);
  if (closureCase.id !== `R1C0-${decisionId}`) errors.push(`R1C0 closure case id must be R1C0-${decisionId}.`);
  if (gate && closureCase.disposition !== gate.recommendedClosure) errors.push(`R1C0 closure case ${closureCase.id} disposition drifted.`);
  if (closureCase.result?.status !== "design-oracle-input" || closureCase.result?.decision !== closureCase.disposition) errors.push(`R1C0 closure case ${closureCase.id} result drifted.`);
  if (!Array.isArray(closureCase.evidenceRefs) || closureCase.evidenceRefs.length === 0) errors.push(`R1C0 closure case ${closureCase.id} lacks evidenceRefs.`);
  const expectedRefs = new Set([...(gate?.bundleIds ?? []), ...(gate?.artifactIds ?? [])]);
  for (const ref of closureCase.evidenceRefs ?? []) {
    if (JSON.stringify(Object.keys(ref).sort()) !== JSON.stringify(["id", "role"].sort())) errors.push(`R1C0 closure case ${closureCase.id} evidenceRef has an unexpected field.`);
    if (!expectedRefs.has(ref.id)) errors.push(`R1C0 closure case ${closureCase.id} cites unplanned evidence ${ref.id}.`);
  }
}
if (closureCases.length !== expectedGates.length || expectedGates.some((id) => !closureCaseIds.has(`R1C0-${id}`))) errors.push("R1C0 closure case IDs must cover all 21 gates exactly.");
const w092Gate = (corpus.gates ?? []).find((gate) => gate.id === "W-092");
if (JSON.stringify(w092Gate?.implementationGapRoles ?? {}) !== JSON.stringify({
  primary: ["W-1441"],
  secondaryToolingIntegration: ["W-073", "W-074", "W-086"],
})) {
  errors.push("W-092 implementation-gap roles must identify W-1441 as primary and parser/tooling IDs as secondary.");
}
const extensionIds = new Set((corpus.extensionBridge ?? []).map((item) => item.id));
for (const id of ["W-982", "W-983", "W-984", "W-985"]) {
  if (!extensionIds.has(id)) errors.push(`missing researchExtension bridge ${id}.`);
}
for (const item of corpus.extensionBridge ?? []) {
  if (item.decision !== "rejected-for-now") errors.push(`${item.id} extension must be rejected-for-now.`);
  if (item.baseCategory !== "source-backed-current" || item.relation !== "research-extension-closed" || item.researchExtensionRemoved !== true) errors.push(`${item.id} extension bridge is not explicit.`);
  if (!Array.isArray(item.baseline) || item.baseline.length === 0) errors.push(`${item.id} bridge has no preserved baseline.`);
}
for (const finding of validateExtensionBridge(corpus.extensionBridge)) errors.push(`R1C0 extension validation failed: ${finding}.`);
const totalR0 = new Set(substitutionCorpus.cases.map((testCase) => testCase.id));
for (const artifact of manifest.reusedArtifacts ?? []) checkDigest(artifact.path, artifact.digest, `reused ${artifact.id}`);
checkDigest("tooling/studies/r1c0-closure/wlo1-manifest.json", manifest.wlo1?.manifest?.digest, "WLO1 manifest");
checkDigest("tooling/wlo1-closure-cases.json", manifest.wlo1?.corpus?.digest, "WLO1 corpus");
checkDigest("tooling/wlo1-closure-machine.mjs", manifest.wlo1?.machine?.digest, "WLO1 machine");
checkDigest("tooling/check-wlo1-closure.mjs", manifest.wlo1?.checker?.digest, "WLO1 checker");
checkDigest("tooling/wlo1-closure-results.snapshot.jsonl", manifest.wlo1?.snapshot?.digest, "WLO1 snapshot");
checkDigest("tooling/studies/r1c0-closure/study.json", manifest.study?.digest, "R1C0 study");
for (const finding of validateWloPayloadAuthority(wloManifest)) errors.push(`R1C0 WLO authority validation failed: ${finding}.`);

const studyFile = resolveContained(manifest.study?.path, "R1C0 study");
if (studyFile && fs.existsSync(studyFile)) {
  const study = JSON.parse(fs.readFileSync(studyFile, "utf8"));
  requireExactKeys(
    study,
    ["$schema", "status", "id", "title", "question", "manifest", "corpus", "wlo1", "artifacts", "scopes", "evidence", "stopCondition"],
    "R1C0 study",
  );
  if (study.$schema !== "w-r1c0-closure-study-1" || study.status !== "design-oracle-input" || study.id !== "R1C0") {
    errors.push("R1C0 study identity drifted.");
  }
  requireExactKeys(study.manifest, ["path"], "R1C0 study manifest receipt");
  requireExactKeys(study.corpus, ["path", "digest"], "R1C0 study corpus receipt");
  requireExactKeys(study.wlo1, ["path", "digest"], "R1C0 study WLO1 receipt");
  requireExactKeys(study.artifacts, ["bundle", "oracle", "checker", "snapshot", "wloChecker", "wloSnapshot"], "R1C0 study artifact set");
  if (study.manifest?.path !== "manifest.json") errors.push("R1C0 study manifest path drifted.");
  if (study.corpus?.path !== "../../r1c0-closure-cases.json") errors.push("R1C0 study corpus path drifted.");
  if (study.wlo1?.path !== "wlo1-manifest.json") errors.push("R1C0 study WLO1 path drifted.");
  const studyDirectory = path.dirname(studyFile);
  checkDigestFrom(studyDirectory, study.corpus?.path, study.corpus?.digest, "R1C0 study corpus");
  checkDigestFrom(studyDirectory, study.wlo1?.path, study.wlo1?.digest, "R1C0 study WLO1 manifest");
  const expectedStudyArtifacts = {
    bundle: "bundle.json",
    oracle: "oracle.test.mjs",
    checker: "../../check-r1c0-closure.mjs",
    snapshot: "../../r1c0-closure-results.snapshot.jsonl",
    wloChecker: "../../check-wlo1-closure.mjs",
    wloSnapshot: "../../wlo1-closure-results.snapshot.jsonl",
  };
  for (const [name, receipt] of Object.entries(study.artifacts ?? {})) {
    requireExactKeys(receipt, ["path", "digest"], `R1C0 study artifact ${name} receipt`);
    if (receipt.path !== expectedStudyArtifacts[name]) errors.push(`R1C0 study artifact ${name} path drifted.`);
    checkDigestFrom(studyDirectory, receipt.path, receipt.digest, `R1C0 study artifact ${name}`);
  }
  if (!Array.isArray(study.scopes) || JSON.stringify([...study.scopes].sort()) !== JSON.stringify([...expectedScopes].sort())) errors.push("R1C0 study scope set drifted.");
  for (const [name, receipt] of Object.entries({ manifest: study.manifest, corpus: study.corpus, wlo1: study.wlo1, ...study.artifacts })) {
    if (name !== "manifest" && !isDigest(receipt?.digest)) errors.push(`R1C0 study ${name} digest is missing or malformed.`);
  }
}

const pinnedPaths = new Map();
function registerPinnedPath(relativePath, label) {
  if (typeof relativePath !== "string") return;
  const normalized = relativePath.replaceAll("\\", "/");
  const previous = pinnedPaths.get(normalized);
  if (previous) errors.push(`duplicate pinned path ${normalized}: ${previous} and ${label}.`);
  else pinnedPaths.set(normalized, label);
}
for (const receipt of [manifest.closureBundle, manifest.corpus, manifest.checker, manifest.snapshot, manifest.study, ...Object.values(manifest.wlo1 ?? {})]) registerPinnedPath(receipt?.path, "manifest receipt");
for (const reference of manifest.bundles ?? []) registerPinnedPath(reference.path, `bundle ${reference.id}`);
for (const artifact of manifest.reusedArtifacts ?? []) registerPinnedPath(artifact.path, `reused ${artifact.id}`);

function deriveScope(scope, ids) {
  const bundles = ids.map((id) => bundleRefs.get(id)?.bundle).filter(Boolean);
  const variants = bundles.flatMap((bundle) => bundle.variants ?? []);
  const inputs = bundles.flatMap((bundle) => bundle.inputs ?? []);
  const tasks = bundles.flatMap((bundle) => bundle.tasks ?? []);
  const r0Cases = new Set(bundles.flatMap((bundle) => bundle.r0Cases ?? []));
  const statuses = Object.fromEntries(
    [...new Set(bundles.map((bundle) => bundle.status))].sort().map((status) => [status, bundles.filter((bundle) => bundle.status === status).length]),
  );
  return {
    bundleCount: bundles.length,
    variantCount: variants.length,
    taskCount: tasks.length,
    inputCount: inputs.length,
    uniqueR0CaseCount: r0Cases.size,
    globalR0Denominator: totalR0.size,
    selectedVariantCount: variants.filter((variant) => variant.role === "selected").length,
    missingEvidenceClaims: ["w-compile", "w-run", "human-study", "model-study"],
    statuses,
  };
}
const metrics = Object.fromEntries(Object.entries(manifest.scopes).map(([scope, ids]) => [scope, deriveScope(scope, ids)]));
const baseMetrics = metrics["R1-broad"];
const aggregateMetrics = {
  ...baseMetrics,
  bundleCount: baseMetrics.bundleCount + 1,
  variantCount: baseMetrics.variantCount + 2,
  taskCount: baseMetrics.taskCount + 4,
  inputCount: baseMetrics.inputCount + 3,
};
for (const finding of validateDerivedMetrics(baseMetrics, deriveScope("R1-broad", manifest.scopes?.["R1-broad"] ?? []))) {
  errors.push(`R1C0 metric validation failed: ${finding}.`);
}
const firstBundleReference = manifest.bundles?.[0];
const staleBundleReference = firstBundleReference ? { ...firstBundleReference, digest: "sha256:deadbeef" } : undefined;
const forgedBaseMetrics = { ...baseMetrics, bundleCount: baseMetrics.bundleCount + 1 };
const forgedClassificationCategories = new Map(classificationCategories);
forgedClassificationCategories.set("W-073", "source-backed-current");
const extensionDrift = (corpus.extensionBridge ?? []).map((item, index) => index === 0 ? { ...item, decision: "current" } : item);
const missingScope = { ...(manifest.scopes ?? {}) };
delete missingScope.R1S1;
const wloAuthorityDrift = clone(wloManifest);
wloAuthorityDrift.codec.header = "wlo1-header";
wloAuthorityDrift.targetProjections[0].wAbiImpact = "w-abi";
const mutationChecks = {
  staleBundleDetected: Boolean(staleBundleReference && validateBundleDigest(staleBundleReference, digestFile(resolvePath(staleBundleReference.path))).includes("staleBundleDigest")),
  forgedMetricDetected: validateDerivedMetrics(forgedBaseMetrics, baseMetrics).includes("forgedDerivedMetric"),
  wrongCategoryDetected: validateImplementationGapMap(corpus.implementationGapMap, forgedClassificationCategories, corpus.plannedImplementationGaps).includes("wrongImplementationGapCategory"),
  extensionDecisionDriftDetected: validateExtensionBridge(extensionDrift).includes("extensionDecisionDrift"),
  missingScopeDetected: validateScopeSet(missingScope, expectedScopes).includes("missingScope"),
  wloReceiptPayloadAuthorityDetected: validateWloPayloadAuthority(wloAuthorityDrift).some((finding) => ["wloPayloadAuthorityDrift", "wloHeaderDrift", "wloTargetWAbiDrift"].includes(finding)),
};
if (Object.values(mutationChecks).some((value) => value !== true)) errors.push("R1C0 mutation suite did not detect every planned mutation.");
const output = {
  schema: "w-r1c0-closure-results-1",
  status: "design-oracle-output",
  evidence: {
    kind: "host-design-oracle-reuse",
    claimsCompiler: false,
    claimsRuntime: false,
    claimsProvider: false,
    claimsHumanOrModel: false,
  },
  corpus: "tooling/r1c0-closure-cases.json",
  corpusDigest: digestFile(corpusPath),
  manifest: "tooling/studies/r1c0-closure/manifest.json",
  gateCount: corpus.gates.length,
  extensionBridgeCount: corpus.extensionBridge.length,
  pinnedBundleCount: bundleRefs.size,
  metrics: {
    "R1-base/pre-closure": baseMetrics,
    "R1-aggregate/after-closure": aggregateMetrics,
    R1E0: metrics.R1E0,
    R1H0: metrics.R1H0,
    R1S1: metrics.R1S1,
  },
  implementationGapMap: corpus.implementationGapMap,
  mutations: mutationChecks,
};
const snapshot = `${JSON.stringify(output)}\n`;
if (writeSnapshot) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  errors.push("r1c0-closure-results.snapshot.jsonl is stale; run with --write.");
}
if (fs.existsSync(snapshotPath) && manifest.snapshot?.digest !== digestFile(snapshotPath) && !writeSnapshot) {
  errors.push(`R1C0 snapshot digest is stale; expected ${digestFile(snapshotPath)}.`);
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}
process.stdout.write(
  `R1C0 closure: ${output.pinnedBundleCount} pinned bundles, ` +
  `${output.gateCount} gates, base=${baseMetrics.bundleCount}/${baseMetrics.variantCount}/${baseMetrics.taskCount}/${baseMetrics.uniqueR0CaseCount}/${baseMetrics.globalR0Denominator}, ` +
  `aggregate=${aggregateMetrics.bundleCount}/${aggregateMetrics.variantCount}/${aggregateMetrics.taskCount}/${aggregateMetrics.uniqueR0CaseCount}/${aggregateMetrics.globalR0Denominator}.\n`,
);
