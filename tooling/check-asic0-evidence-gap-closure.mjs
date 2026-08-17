import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  CASE_KEYS,
  CURRENT_EVIDENCE,
  DECISIONS,
  EXPECT_KEYS,
  GAP_CATEGORY,
  MISSING_EVIDENCE,
  REUSED_STUDIES,
  SOURCE_KEYS,
  clone,
  corpusPath,
  deriveAsic0Case,
  digestFile,
  repositoryRoot,
  same,
  studyDirectory,
  validateCase,
  validateCorpus,
} from "./asic0-evidence-gap-closure-machine.mjs";

const manifestPath = path.join(studyDirectory, "manifest.json");
const bundlePath = path.join(studyDirectory, "bundle.json");
const snapshotPath = path.join(path.dirname(fileURLToPath(import.meta.url)), "asic0-evidence-gap-closure-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const bundle = JSON.parse(fs.readFileSync(bundlePath, "utf8"));
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));

const EXPECTED_ARTIFACTS = Object.freeze({
  corpus: "tooling/asic0-evidence-gap-closure-cases.json",
  machine: "tooling/asic0-evidence-gap-closure-machine.mjs",
  "root-checker": "tooling/check-asic0-evidence-gap-closure.mjs",
  "nested-checker": "tooling/tree-sitter-w/check-asic0.mjs",
  snapshot: "tooling/asic0-evidence-gap-closure-results.snapshot.jsonl",
  bundle: "tooling/studies/asic0-evidence-gap-closure/bundle.json",
  study: "tooling/studies/asic0-evidence-gap-closure/study.json",
  oracle: "tooling/studies/asic0-evidence-gap-closure/oracle.test.mjs",
  readme: "tooling/studies/asic0-evidence-gap-closure/README.md",
  index: "tooling/studies/asic0-evidence-gap-closure/INDEX.md",
  current: "tooling/studies/asic0-evidence-gap-closure/current.w",
  adversarial: "tooling/studies/asic0-evidence-gap-closure/adversarial.w",
  "ipc-study": "tooling/studies/ipc1-mapped-ipc/study.json",
  "ipc-corpus": "tooling/ipc1-mapped-ipc-cases.json",
  "ipc-machine": "tooling/ipc1-mapped-ipc-machine.mjs",
  "ipc-snapshot": "tooling/ipc1-mapped-ipc-results.snapshot.jsonl",
  "avf-study": "tooling/studies/avf0-availability-feature/study.json",
  "avf-corpus": "tooling/avf0-availability-feature-cases.json",
  "avf-machine": "tooling/avf0-availability-feature-machine.mjs",
  "avf-snapshot": "tooling/avf0-availability-feature-results.snapshot.jsonl",
  "sec-study": "tooling/studies/sec0-security-model/study.json",
  "sec-corpus": "tooling/sec0-security-model-cases.json",
  "sec-machine": "tooling/sec0-security-model-machine.mjs",
  "sec-snapshot": "tooling/sec0-security-model-results.snapshot.jsonl",
});

const MANIFEST_KEYS = ["$schema", "status", "id", "reuseOnly", "decisions", "artifacts", "evidence", "stopCondition"];

function error(errors, message) {
  errors.push(message);
}

function resolveArtifact(relativePath) {
  if (typeof relativePath !== "string" || relativePath.trim() === "") return null;
  const resolved = path.resolve(repositoryRoot, relativePath);
  const relative = path.relative(repositoryRoot, resolved);
  if (relative === "" || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) return null;
  return resolved;
}

function checkDigest(relativePath, expected, location, errors) {
  const file = resolveArtifact(relativePath);
  if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
    error(errors, `${location}.path escapes the repository or is missing.`);
    return null;
  }
  if (!/^sha256:[0-9a-f]{64}$/u.test(expected ?? "")) error(errors, `${location}.digest must be lowercase sha256.`);
  else if (digestFile(file) !== expected) error(errors, `${location}.digest is stale.`);
  return file;
}

function checkSymbol(file, symbol, location, errors) {
  if (!file || typeof symbol !== "string" || symbol === "") {
    error(errors, `${location}.symbol is required.`);
    return;
  }
  const count = fs.readFileSync(file, "utf8").split(symbol).length - 1;
  if (count !== 1) error(errors, `${location}.symbol must occur exactly once; got ${count}.`);
}

export function validateBundle(value = bundle) {
  const errors = [];
  if (!value || typeof value !== "object" || Array.isArray(value)) return ["bundle must be an object."];
  if (value.$schema !== "w-substitution-study-bundle-1") error(errors, "bundle schema is invalid.");
  if (value.status !== "design-oracle-input") error(errors, "bundle status is invalid.");
  if (value.id !== "R1-asic0-evidence-gap-closure") error(errors, "bundle id is invalid.");
  if (value.entry !== "asic0Closure") error(errors, "bundle entry is invalid.");
  const sourceBase = checkDigest(path.join("tooling/studies/asic0-evidence-gap-closure", value.sourceBase?.path ?? ""), value.sourceBase?.digest, "bundle.sourceBase", errors);
  checkSymbol(sourceBase, value.sourceBase?.symbol, "bundle.sourceBase", errors);
  if (!Array.isArray(value.sourceRefs) || value.sourceRefs.length !== 3) error(errors, "bundle sourceRefs must contain three reused references.");
  const sourceKeys = new Set();
  for (const [index, ref] of (value.sourceRefs ?? []).entries()) {
    const file = checkDigest(path.join("tooling/studies/asic0-evidence-gap-closure", ref?.path ?? ""), ref?.digest, `bundle.sourceRefs[${index}]`, errors);
    checkSymbol(file, ref?.symbol, `bundle.sourceRefs[${index}]`, errors);
    const key = `${ref?.path}\0${ref?.symbol}`;
    if (sourceKeys.has(key)) error(errors, `bundle.sourceRefs[${index}] duplicates a source reference.`);
    sourceKeys.add(key);
  }
  const variants = value.variants ?? [];
  if (!Array.isArray(variants) || variants.length !== 2) error(errors, "bundle must contain current and adversarial variants.");
  const roles = variants.map((variant) => variant.role).sort();
  if (!same(roles, ["rejected-witness", "selected"])) error(errors, "bundle variant roles are invalid.");
  const variantIds = variants.map((variant) => variant.id).sort();
  if (!same(variantIds, ["adversarial", "current"])) error(errors, "bundle variant ids are invalid.");
  const variantDigests = new Set();
  for (const [index, variant] of variants.entries()) {
    if (!["current", "adversarial"].includes(variant?.id)) error(errors, `bundle.variants[${index}].id is invalid.`);
    if (variant?.language !== "w") error(errors, `bundle.variants[${index}] must be W.`);
    const file = checkDigest(path.join("tooling/studies/asic0-evidence-gap-closure", variant?.path ?? ""), variant?.digest, `bundle.variants[${index}]`, errors);
    if (variant?.digest) variantDigests.add(variant.digest);
    if (file && !fs.readFileSync(file, "utf8").includes(value.entry)) error(errors, `bundle.variants[${index}] does not contain ${value.entry}.`);
    if (!Array.isArray(variant?.changedConstructs) || variant.changedConstructs.length === 0) error(errors, `bundle.variants[${index}].changedConstructs is empty.`);
    if (variant?.parseEvidence?.status !== "tree-sitter-parse") error(errors, `bundle.variants[${index}] must record tree-sitter-parse.`);
  }
  if (variantDigests.size !== variants.length) error(errors, "bundle variants must use distinct source bytes.");
  if (!Array.isArray(value.inputs) || value.inputs.length !== 2) error(errors, "bundle inputs must contain current and adversarial inputs.");
  const inputIds = new Set();
  for (const input of value.inputs ?? []) {
    if (inputIds.has(input?.id)) error(errors, `bundle input ${input?.id} is duplicated.`);
    inputIds.add(input?.id);
    if (input?.expected === undefined) error(errors, `bundle input ${input?.id} expected is required.`);
  }
  const taskKinds = (value.tasks ?? []).map((task) => task.kind);
  if (!same(taskKinds, ["explain", "recall", "repair", "change"])) error(errors, "bundle tasks are invalid.");
  if (!Number.isInteger(value.tasks?.[1]?.minimumDelaySeconds) || value.tasks[1].minimumDelaySeconds < 1) error(errors, "bundle recall delay must be positive.");
  if (!same(value.presentationOrders, [["current", "adversarial"], ["adversarial", "current"]])) error(errors, "bundle presentation orders are invalid.");
  if (!same(value.blinding?.participantLabels, { current: "A", adversarial: "B" })) error(errors, "bundle blinding labels are invalid.");
  if (!same(value.blinding?.hide, ["id", "role", "path", "changedConstructs"])) error(errors, "bundle blinding fields are invalid.");
  const oracle = checkDigest(path.join("tooling/studies/asic0-evidence-gap-closure", value.oracle?.path ?? ""), value.oracle?.digest, "bundle.oracle", errors);
  if (oracle && !fs.readFileSync(oracle, "utf8").includes("ASIC0 reuse-only host oracle")) error(errors, "bundle.oracle content is invalid.");
  if (!Array.isArray(value.evidence?.current) || !Array.isArray(value.evidence?.missing)) error(errors, "bundle evidence boundary is invalid.");
  for (const required of ["tree-sitter-parse", "host-oracle", "reused-corpus", "mutation-checks"]) if (!value.evidence?.current?.includes(required)) error(errors, `bundle evidence.current is missing ${required}.`);
  for (const required of ["w-compile", "w-run", "compiler", "runtime", "provider", "human-study", "model-study"]) if (!value.evidence?.missing?.includes(required)) error(errors, `bundle evidence.missing is missing ${required}.`);
  const overlap = (value.evidence?.current ?? []).filter((entry) => value.evidence?.missing?.includes(entry));
  if (overlap.length > 0) error(errors, `bundle evidence overlaps: ${overlap.join(", ")}.`);
  return errors;
}

export function validateManifest(value = manifest) {
  const errors = [];
  if (!value || typeof value !== "object" || Array.isArray(value)) return ["manifest must be an object."];
  if (!same(Object.keys(value).sort(), [...MANIFEST_KEYS].sort())) error(errors, "manifest keys are invalid.");
  if (value.$schema !== "w-asic0-evidence-gap-closure-manifest-1") error(errors, "manifest schema is invalid.");
  if (value.status !== "design-oracle-input" || value.id !== "ASIC0" || value.reuseOnly !== true) error(errors, "manifest status/id/reuseOnly is invalid.");
  if (!same(value.decisions, DECISIONS)) error(errors, "manifest decisions are invalid.");
  const artifacts = value.artifacts;
  if (!Array.isArray(artifacts) || artifacts.length !== Object.keys(EXPECTED_ARTIFACTS).length) error(errors, "manifest artifact count is invalid.");
  const ids = new Set();
  const roles = new Set();
  const paths = new Set();
  const byRole = new Map();
  for (const [index, artifact] of (artifacts ?? []).entries()) {
    const location = `manifest.artifacts[${index}]`;
    if (!artifact || !same(Object.keys(artifact).sort(), ["digest", "id", "path", "role"])) {
      error(errors, `${location} keys are invalid.`);
      continue;
    }
    if (ids.has(artifact.id)) error(errors, `${location} duplicate id.`);
    if (roles.has(artifact.role)) error(errors, `${location} duplicate role.`);
    if (paths.has(artifact.path)) error(errors, `${location} duplicate path.`);
    ids.add(artifact.id); roles.add(artifact.role); paths.add(artifact.path); byRole.set(artifact.role, artifact);
    if (EXPECTED_ARTIFACTS[artifact.role] !== artifact.path) error(errors, `${location} role/path is invalid.`);
    checkDigest(artifact.path, artifact.digest, location, errors);
  }
  for (const [role, expectedPath] of Object.entries(EXPECTED_ARTIFACTS)) {
    const artifact = byRole.get(role);
    if (!artifact) error(errors, `manifest is missing role ${role}.`);
    else if (artifact.path !== expectedPath) error(errors, `manifest role ${role} path is invalid.`);
  }
  const bundleArtifact = byRole.get("bundle");
  if (bundleArtifact && digestFile(bundlePath) !== bundleArtifact.digest) error(errors, "manifest bundle digest is stale.");
  const corpusArtifact = byRole.get("corpus");
  if (corpusArtifact && digestFile(corpusPath) !== corpusArtifact.digest) error(errors, "manifest corpus digest is stale.");
  if (!exactEvidence(value.evidence)) error(errors, "manifest evidence boundary is invalid.");
  if (typeof value.stopCondition !== "string" || value.stopCondition.trim() === "") error(errors, "manifest stopCondition is required.");
  errors.push(...validateBundle(bundle));
  return errors;
}

function exactEvidence(evidence) {
  return evidence && typeof evidence === "object" && same(evidence.current, CURRENT_EVIDENCE) && same(evidence.missing, MISSING_EVIDENCE) && evidence.hostOnly === true;
}

export function mutationChecks() {
  const checks = {};
  const staleDigest = clone(manifest);
  staleDigest.artifacts.find((artifact) => artifact.role === "machine").digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
  checks.staleSourceDigestRejected = validateManifest(staleDigest).some((entry) => entry.includes("digest"));

  const escapedPath = clone(manifest);
  escapedPath.artifacts.find((artifact) => artifact.role === "ipc-corpus").path = "../../outside.json";
  checks.sourcePathEscapeRejected = validateManifest(escapedPath).some((entry) => entry.includes("escapes") || entry.includes("role/path"));

  const routeDrift = clone(corpus);
  routeDrift.cases.find((item) => item.id === "ASIC0-W-1420-current").source.caseId = "AVF0-availability-provider-fallback";
  checks.sourceRouteDriftRejected = validateCorpus(routeDrift).errors.some((entry) => entry.includes("ASIC0-W-1420-current") && entry.includes("derived"));

  const expectedDrift = clone(corpus);
  expectedDrift.cases.find((item) => item.id === "ASIC0-W-1355-current").expect.code = "forged-code";
  checks.expectedAssertionDriftRejected = validateCorpus(expectedDrift).errors.some((entry) => entry.includes("ASIC0-W-1355-current") && entry.includes("code expected"));

  const gapDrift = clone(corpus);
  gapDrift.plannedImplementationGaps["W-1448"].category = "oracle-backed-current";
  checks.gapCategoryMapDriftRejected = validateCorpus(gapDrift).errors.some((entry) => entry.includes("W-1448") && entry.includes("planned implementation gap"));

  const forgedProvider = clone(corpus);
  forgedProvider.evidence.current.push("provider-ready");
  checks.forgedProviderReadyRejected = validateCorpus(forgedProvider).errors.some((entry) => entry.includes("must not claim readiness"));

  const duplicateDecision = clone(corpus);
  duplicateDecision.decisions[0] = duplicateDecision.decisions[1];
  checks.duplicateDecisionRejected = validateCorpus(duplicateDecision).errors.some((entry) => entry.includes("decisions"));

  const wrongOriginalDecision = clone(corpus);
  wrongOriginalDecision.cases.find((item) => item.id === "ASIC0-W-1355-current").decisions[0] = "W-1448";
  checks.wrongOriginalDecisionRejected = validateCorpus(wrongOriginalDecision).errors.some((entry) => entry.includes("original decision") || entry.includes("not reusable"));

  const duplicateCase = clone(corpus);
  duplicateCase.cases[1].id = duplicateCase.cases[0].id;
  checks.duplicateCaseRejected = validateCorpus(duplicateCase).errors.some((entry) => entry.includes("duplicate case id"));

  const missingEvidence = clone(corpus);
  missingEvidence.evidence.missing = missingEvidence.evidence.missing.filter((entry) => entry !== "provider");
  checks.evidenceMissingRejected = validateCorpus(missingEvidence).errors.some((entry) => entry.includes("evidence.missing"));

  const supplementalRoute = clone(corpus);
  supplementalRoute.cases.find((item) => item.id === "ASIC0-W-1355-current").evidenceCaseIds[0] = "IPC1-provider-durable-requirement-reject";
  checks.supplementalEvidenceRouteRejected = validateCorpus(supplementalRoute).errors.some((entry) => entry.includes("evidence case") && entry.includes("allowed closed route"));

  const roleDrift = clone(bundle);
  roleDrift.variants[0].role = "alternative";
  checks.bundleRoleDriftRejected = validateBundle(roleDrift).some((entry) => entry.includes("variant roles"));

  const schemaDrift = clone(bundle);
  schemaDrift.$schema = "w-asic0-evidence-gap-closure-bundle-1";
  checks.bundleSchemaDriftRejected = validateBundle(schemaDrift).some((entry) => entry.includes("bundle schema"));
  return checks;
}

function main() {
  const corpusValidation = validateCorpus(corpus);
  const errors = [...corpusValidation.errors, ...validateManifest(manifest)];
  const mutations = mutationChecks();
  for (const [name, passed] of Object.entries(mutations)) if (!passed) error(errors, `mutation ${name} did not detect drift.`);
  const output = {
    schema: "w-asic0-evidence-gap-closure-results-1",
    status: "design-oracle-output",
    corpus: "tooling/asic0-evidence-gap-closure-cases.json",
    corpusDigest: digestFile(corpusPath),
    metrics: {
      caseCount: corpusValidation.results.length,
      decisionCount: DECISIONS.length,
      currentCount: corpusValidation.results.filter((result) => result.contractRoute === "current").length,
      adversarialCount: corpusValidation.results.filter((result) => result.contractRoute === "rejected").length,
      gapCategory: GAP_CATEGORY,
      reusedStudies: ["IPC1", "AVF0", "SEC0"],
    },
    results: corpusValidation.results,
    integrityMutations: mutations,
  };
  const snapshot = `${JSON.stringify(output)}\n`;
  if (process.argv.includes("--write")) fs.writeFileSync(snapshotPath, snapshot, "utf8");
  else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) error(errors, "ASIC0 snapshot is stale; run with --write.");
  if (errors.length > 0) {
    process.stderr.write(`${errors.map((entry) => `- ${entry}`).join("\n")}\n`);
    process.exitCode = 1;
    return;
  }
  process.stdout.write(`ASIC0 evidence-gap closure: ${output.metrics.caseCount} cases, ${output.metrics.currentCount} current, ${output.metrics.adversarialCount} adversarial; mutations green.\n`);
}

export { main };

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
