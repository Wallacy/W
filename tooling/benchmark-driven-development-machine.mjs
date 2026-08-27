import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import {
  BYTE_SCAN_MANIFEST_ID,
  BYTE_SCAN_MAX_BYTES,
  BYTE_SCAN_OUTPUT_SHAPE,
  LANGUAGE_CATALOG_ID,
  LANGUAGE_CATALOG_VERSION,
  LANGUAGE_STRATA,
  LANGUAGE_UNIT_IDS,
  canonicalByteScanOutput,
  calculateByteScan,
  deterministicByteScanCases,
  expectedByteScanOutput,
  loadByteScanDocuments,
  validateByteScanManifest,
  validateLanguageCatalog,
} from "./byte-scan-view-machine.mjs";

export {
  BYTE_SCAN_MANIFEST_ID,
  BYTE_SCAN_MAX_BYTES,
  BYTE_SCAN_OUTPUT_SHAPE,
  LANGUAGE_CATALOG_ID,
  LANGUAGE_CATALOG_VERSION,
  LANGUAGE_STRATA,
  LANGUAGE_UNIT_IDS,
  canonicalByteScanOutput,
  calculateByteScan,
  deterministicByteScanCases,
  expectedByteScanOutput,
  validateByteScanManifest,
  validateLanguageCatalog,
};

export const ROOT = path.resolve(import.meta.dir, "..");
export const SCHEMA_VERSION = "wbench/1";
export const LANGUAGE_PROFILES = Object.freeze(["learner", "idiomatic", "frontier"]);
export const PROFILE_DISCLOSURES = Object.freeze([
  "unsafe",
  "ffi",
  "targetSpecialization",
  "manualLayout",
  "algorithm",
  "legibility",
]);
export const LANES = Object.freeze(["equivalent", "open"]);
export const BENCHMARK_DISPOSITIONS = Object.freeze([
  "required",
  "compiler-lifecycle",
  "deferred",
  "not-applicable",
]);
export const LIFECYCLE_SCENARIOS = Object.freeze(["clean", "no-op", "edit"]);
export const LIFECYCLE_STAGES = Object.freeze([
  "check-end-to-end",
  "source",
  "lex",
  "parse",
  "semantic",
  "hir",
  "lowering",
  "codegen",
  "link",
]);
export const REQUIRED_CASES = Object.freeze([
  "BMD1-W-1487-current",
  "BMD1-W-1488-current-matrix",
  "BMD1-W-1487-open-lane",
  "BMD1-W-1487-deferred",
  "BMD1-W-1487-not-applicable",
  "BMD1-W-1487-profile-missing",
  "BMD1-W-1487-profile-duplicate",
  "BMD1-W-1487-idiomatic-not-primary",
  "BMD1-W-1487-oracle-partial",
  "BMD1-W-1487-equivalent-semantic-difference",
  "BMD1-W-1487-learner-artificially-slow",
  "BMD1-W-1487-frontier-missing-disclosure",
  "BMD1-W-1488-baseline-provenance-missing",
  "BMD1-W-1488-compile-execution-mixed",
  "BMD1-W-1487-best-only",
  "BMD1-W-1487-output-constant-bypass",
  "BMD1-W-1488-claim-without-backend",
  "BMD1-W-1487-specialization-universal",
  "BMD1-W-1487-invalid-benchmark-disposition",
  "BMD1-W-1487-deferred-missing-blocker",
  "BMD1-W-1487-not-applicable-missing-reason",
  "BMD1-W-1488-flattened-axis",
  "BMD1-W-1488-no-op-without-cache",
  "BMD1-W-1488-edit-without-cache",
  "BMD1-W-1488-internal-stage-without-instrumentation",
  "BMD1-W-1488-runtime-mixed",
  "BMD1-W-1488-result-blocked-point",
  "BMD1-W-1488-result-language-without-backend",
  "BMD1-W-1488-result-regression-without-comparison",
  "BMD1-W-1488-result-partial-oracle",
  "BMD1-W-1488-result-tracked-timing",
  "BMD1-W-1488-result-output-overwrite",
  "BMD1-W-1488-result-oracle-false",
  "BMD1-W-1488-result-forged-metric",
  "BMD1-W-1488-result-even-samples",
  "BMD1-W-1488-result-leading-zero",
  "BMD1-W-1488-result-u64-overflow",
  "BMD1-W-1488-result-comparison-incomplete",
  "BMD1-W-1488-blocker-incomplete",
  "BMD2-W-1489-current-comparison",
  "BMD2-W-1489-different-closure-comparison",
  "BMD2-W-1489-schedule-forged",
  "BMD2-W-1489-sample-label-forged",
  "BMD2-W-1489-oracle-partial",
  "BMD2-W-1489-recipe-class-divergent",
  "BMD2-W-1489-toolchain-divergent",
  "BMD2-W-1489-workload-divergent",
  "BMD2-W-1489-metric-forged",
  "BMD2-W-1489-delta-forged",
  "BMD2-W-1489-calibration-forged",
  "BMD2-W-1489-counts-forged",
  "BMD2-W-1489-even-pairs",
  "BMD2-W-1489-zero-ns",
  "BMD2-W-1489-leading-zero",
  "BMD2-W-1489-u64-overflow",
  "BMD2-W-1489-regression-blocked",
  "BMD3-W-1490-current-catalog",
  "BMD3-W-1490-current-byte-scan-view",
  "BMD3-W-1490-catalog-not-21",
  "BMD3-W-1490-catalog-duplicate-id",
  "BMD3-W-1490-catalog-wrong-stratum",
  "BMD3-W-1490-profile-missing",
  "BMD3-W-1490-learner-artificial",
  "BMD3-W-1490-lane-fraud",
  "BMD3-W-1490-frontier-disclosure-missing",
  "BMD3-W-1490-constant-output",
  "BMD3-W-1490-output-precomputed",
  "BMD3-W-1490-input-incomplete",
  "BMD3-W-1490-boundary-incomplete",
  "BMD3-W-1490-oracle-incomplete",
  "BMD3-W-1490-claim-timing",
  "BMD3-W-1490-claim-backend",
  "BMD3-W-1490-baseline-partial",
  "BMD3-W-1490-baseline-forged",
  "BMD3-W-1490-temp-source-provenance",
]);
export const SOURCE_FIXTURE = Object.freeze({
  path: "reference/last-light/checker_bootstrap.w",
  symbol: "export fn canAcceptOrder(",
  digest: "sha256:06a1d29ebac7e2b80e0c37d0a7cfced8a5d1ce35fed3d945146a46f7fdca43bc",
});
export const SOURCE_ID = "last-light-checker-bootstrap-source";
export const EDIT_RECIPE = Object.freeze({
  kind: "whitespace-only",
  sourcePath: SOURCE_FIXTURE.path,
  targetSymbol: SOURCE_FIXTURE.symbol,
  occurrence: 1,
  match: "  return open\n",
  replacement: "    return open\n",
  semanticPreserving: true,
  applyTo: "temporary-copy",
  operation: "replace-once",
  expectedMatches: 1,
  temporaryCopy: true,
});
export const DESCRIPTOR_IDENTITIES = Object.freeze({
  graph: Object.freeze({
    id: "last-light-checker-bootstrap-graph",
    path: "benchmarks/seed-check-graph.json",
    digest: "sha256:ff78805c96c17e617106c27af1007ac3fc0e0327e2d87c1bfa36e3d5f23160f5",
  }),
  input: Object.freeze({
    id: "last-light-checker-bootstrap-input",
    path: "benchmarks/seed-check-input.json",
    digest: "sha256:c202dd03968c1a9a2999e81eb6bfbb633e8d53cb33e12e62adeb65e3fa4a2020",
  }),
});

export const RESULT_MIN_SAMPLES = 9;
export const RESULT_MIN_WARMUP = 1;
export const RESULT_CLOCK = "monotonic-wall-ns";
export const MEASUREMENT_ORDER = "single-series";
export const COMPARISON_ORDER = "randomized-interleaved";
export const PAIRED_COMPARISON_ORDER = "balanced-paired-interleaved-sha256-v1";
export const COMPARISON_CLAIM = "comparison-only";
export const REGRESSION_BLOCKER = "managed-regression-runner";
export const COMPARISON_SEED_BYTES = 32;
export const MAX_BOUNDED_MUTATIONS = 2;
export const MAX_U64 = (1n << 64n) - 1n;

const DIGEST_PATTERN = /^sha256:[0-9a-f]{64}$/u;
const PROFILE_SET = new Set(LANGUAGE_PROFILES);
const LANE_SET = new Set(LANES);
const DISPOSITION_SET = new Set(BENCHMARK_DISPOSITIONS);
const SCENARIO_SET = new Set(LIFECYCLE_SCENARIOS);
const STAGE_SET = new Set(LIFECYCLE_STAGES);
const INTERNAL_STAGES = new Set(["source", "lex", "parse", "semantic"]);
const COMPONENT_STAGES = new Set(["hir", "lowering", "codegen", "link"]);

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}
function push(errors, message) {
  errors.push(message);
}

function hasOwn(value, key) {
  return isObject(value) && Object.prototype.hasOwnProperty.call(value, key);
}

function requiredString(value, name, errors) {
  if (typeof value !== "string" || value.trim() === "") {
    push(errors, name + " must be a non-empty string.");
    return false;
  }
  return true;
}

function requiredBoolean(value, name, errors) {
  if (value !== true) {
    push(errors, name + " must be true.");
    return false;
  }
  return true;
}

function requiredDigest(value, name, errors) {
  if (!DIGEST_PATTERN.test(value ?? "")) {
    push(errors, name + " must be a lowercase sha256 digest.");
    return false;
  }
  return true;
}

function sameArray(actual, expected) {
  return Array.isArray(actual) && actual.length === expected.length &&
    actual.every((value, index) => value === expected[index]);
}

function checkExactKeys(value, name, expected, errors) {
  if (!isObject(value)) return false;
  if (!sameArray(Object.keys(value).sort(), [...expected].sort())) {
    push(errors, name + " must use a closed object shape.");
    return false;
  }
  return true;
}

function repositoryPath(relativePath) {
  if (typeof relativePath !== "string" || relativePath.trim() === "") return undefined;
  const absolutePath = path.resolve(ROOT, relativePath);
  const relative = path.relative(ROOT, absolutePath);
  if (relative === "" || relative === ".." ||
      relative.startsWith(".." + path.sep) || path.isAbsolute(relative)) return undefined;
  return absolutePath;
}

function fileDigest(relativePath) {
  const absolutePath = repositoryPath(relativePath);
  if (!absolutePath || !fs.existsSync(absolutePath) ||
      !fs.statSync(absolutePath).isFile()) return undefined;
  return "sha256:" + crypto.createHash("sha256")
    .update(fs.readFileSync(absolutePath)).digest("hex");
}

function checkSource(source, name, errors, requirePath = true) {
  if (!isObject(source)) {
    push(errors, name + " must be an object.");
    return;
  }
  if (requirePath) requiredString(source.path, name + ".path", errors);
  const hasSymbol = requiredString(source.symbol, name + ".symbol", errors);
  const hasDigest = requiredDigest(source.digest, name + ".digest", errors);
  if (source.path !== SOURCE_FIXTURE.path) {
    push(errors, name + ".path must use the Last Light benchmark fixture.");
  }
  if (hasSymbol && source.symbol !== SOURCE_FIXTURE.symbol) {
    push(errors, name + ".symbol must identify canAcceptOrder.");
  }
  if (hasDigest && source.digest !== SOURCE_FIXTURE.digest) {
    push(errors, name + ".digest is not the current fixture digest.");
  }
  const actual = fileDigest(source.path);
  if (!actual) {
    push(errors, name + ".path does not identify an existing source file.");
  } else {
    if (source.digest !== actual) push(errors, name + ".digest is stale.");
    if (hasSymbol &&
        !fs.readFileSync(repositoryPath(source.path), "utf8").includes(source.symbol)) {
      push(errors, name + ".symbol is not present in the source bytes.");
    }
  }
}

function checkCorpusSource(source, name, errors) {
  if (!isObject(source)) {
    push(errors, name + " must be an object.");
    return;
  }
  requiredString(source.path, name + ".path", errors);
  requiredString(source.symbol, name + ".symbol", errors);
  const hasDigest = requiredDigest(source.digest, name + ".digest", errors);
  const actual = fileDigest(source.path);
  if (!actual) {
    push(errors, name + ".path does not identify an existing source file.");
  } else {
    if (hasDigest && source.digest !== actual) push(errors, name + ".digest is stale.");
    if (typeof source.symbol === "string" &&
        !fs.readFileSync(repositoryPath(source.path), "utf8").includes(source.symbol)) {
      push(errors, name + ".symbol is not present in the source bytes.");
    }
  }
}

function checkIdentity(identity, name, errors, requirePath = false) {
  if (!isObject(identity)) {
    push(errors, name + " must be an object.");
    return false;
  }
  const validId = requiredString(identity.id, name + ".id", errors);
  const validDigest = requiredDigest(identity.digest, name + ".digest", errors);
  const validPath = requirePath
    ? requiredString(identity.path, name + ".path", errors)
    : true;
  return validId && validDigest && validPath;
}

function checkOracle(oracle, name, errors, expectedKind = "host-oracle") {
  if (!isObject(oracle)) {
    push(errors, name + " must be an object.");
    return;
  }
  if (expectedKind === "host-structural" && oracle.kind !== expectedKind) {
    push(errors, name + ".kind must be " + expectedKind + ".");
  }
  if (expectedKind === "host-oracle" && oracle.kind !== undefined &&
      oracle.kind !== expectedKind) {
    push(errors, name + ".kind must be " + expectedKind + " when present.");
  }
  if (expectedKind === "host-structural") {
    requiredBoolean(oracle.requiredBeforeSamples, name + ".requiredBeforeSamples", errors);
    if (oracle.runtime !== "unavailable") {
      push(errors, name + ".runtime must be unavailable.");
    }
    return;
  }
  requiredString(oracle.correctnessRecord, name + ".correctnessRecord", errors);
  requiredBoolean(oracle.complete, name + ".complete", errors);
  requiredBoolean(oracle.beforeSamples, name + ".beforeSamples", errors);
}

function checkBaseline(baseline, name, errors) {
  if (!isObject(baseline)) {
    push(errors, name + " must be an object.");
    return;
  }
  if (!Array.isArray(baseline.independent)) {
    push(errors, name + ".independent must be an array.");
  } else {
    const unique = new Set(baseline.independent);
    if (unique.size !== baseline.independent.length) {
      push(errors, name + ".independent must not contain duplicates.");
    }
    for (const baselineId of baseline.independent) {
      if (!["c-clang", "rust"].includes(baselineId)) {
        push(errors, name + ".independent contains an unknown baseline.");
      }
    }
    if (baseline.independent.length < 2 &&
        !requiredString(baseline.exceptionReason, name + ".exceptionReason", errors)) {
      push(errors, name + " needs two independent baselines or an exception reason.");
    }
    if (baseline.independent.length >= 2 && baseline.exceptionReason !== null) {
      push(errors, name + ".exceptionReason must be null when two baselines are present.");
    }
  }
  if (baseline.provenanceComplete !== true) {
    push(errors, name + ".provenanceComplete must be true.");
  }
}

function checkSamples(samples, name, errors, backendAvailable, expectedOrder = COMPARISON_ORDER) {
  if (!isObject(samples)) {
    push(errors, name + " must be an object.");
    return;
  }
  if (samples.order !== expectedOrder) {
    push(errors, name + ".order must be " + expectedOrder + ".");
  }
  if (!backendAvailable && samples.mode !== "not-started") {
    push(errors, name + ".mode must be not-started while the backend is unavailable.");
  }
}

function checkBackend(backendAvailable, claims, name, errors) {
  if (backendAvailable !== false) {
    push(errors, name + ".backendAvailable must be false for this protocol.");
  }
  if (!Array.isArray(claims)) {
    push(errors, name + ".claims must be an array.");
  } else if (backendAvailable === false && claims.length > 0) {
    push(errors, name + ".claims must be empty without a backend.");
  }
}

function checkDisposition(value, name, errors) {
  if (!DISPOSITION_SET.has(value)) {
    push(errors, name + " must be required, compiler-lifecycle, deferred or not-applicable.");
  }
}

function checkLanguageProfiles(profiles, name, errors) {
  if (!Array.isArray(profiles) || profiles.length !== LANGUAGE_PROFILES.length) {
    push(errors, name + " must contain exactly learner, idiomatic and frontier.");
    return new Map();
  }
  const byId = new Map();
  for (const [index, profile] of profiles.entries()) {
    const location = name + "[" + index + "]";
    if (!isObject(profile)) {
      push(errors, location + " must be an object.");
      continue;
    }
    if (!PROFILE_SET.has(profile.id)) {
      push(errors, location + ".id is not a required profile.");
    }
    if (byId.has(profile.id)) {
      push(errors, location + ".id duplicates " + profile.id + ".");
    }
    byId.set(profile.id, profile);
    requiredBoolean(profile.correct, location + ".correct", errors);
    requiredBoolean(profile.plausible, location + ".plausible", errors);
  }
  for (const profileId of LANGUAGE_PROFILES) {
    if (!byId.has(profileId)) push(errors, name + " is missing " + profileId + ".");
  }
  const learner = byId.get("learner");
  if (learner) {
    for (const field of ["sleep", "uselessWork", "worseFlags", "bypass"]) {
      if (learner[field] !== false) push(errors, name + ".learner." + field + " must be false.");
    }
  }
  const frontier = byId.get("frontier");
  if (frontier) {
    if (!isObject(frontier.disclosures)) {
      push(errors, name + ".frontier.disclosures must declare every frontier axis.");
    } else {
      for (const field of PROFILE_DISCLOSURES) {
        requiredString(frontier.disclosures[field],
          name + ".frontier.disclosures." + field, errors);
      }
      for (const field of Object.keys(frontier.disclosures)) {
        if (!PROFILE_DISCLOSURES.includes(field)) {
          push(errors, name + ".frontier.disclosures has an unknown axis " + field + ".");
        }
      }
      if (frontier.disclosures.targetSpecialization === "universal") {
        push(errors, name + ".frontier.disclosures.targetSpecialization must name a bounded target and fallback.");
      }
    }
  }
  return byId;
}

function checkEquivalence(input, name, errors) {
  for (const field of [
    "sameAlgorithm",
    "sameRepresentation",
    "sameValidation",
    "sameNumericContract",
    "sameInput",
  ]) {
    if (input[field] !== true) {
      push(errors, name + "." + field + " must be true in the equivalent lane.");
    }
  }
}

function checkOpenLane(input, name, errors) {
  for (const field of ["sameValidation", "sameNumericContract", "sameInput"]) {
    if (input[field] !== true) {
      push(errors, name + "." + field + " must be true in the open lane.");
    }
  }
  if (input.sameAlgorithm !== false && input.sameRepresentation !== false) {
    push(errors, name + " must record an algorithm or representation difference in the open lane.");
  }
}

function checkLanguageScenario(input, name, errors) {
  const disposition = input?.benchmarkDisposition;
  checkDisposition(disposition, name + ".benchmarkDisposition", errors);
  if (!(["required", "deferred"].includes(disposition))) {
    push(errors, name + ".language workload must use required or deferred disposition.");
  }
  checkLanguageProfiles(input?.profiles, name + ".profiles", errors);
  if (input?.primaryProfile !== "idiomatic") {
    push(errors, name + ".primaryProfile must be idiomatic.");
  }
  if (input?.regressionProfile !== "idiomatic") {
    push(errors, name + ".regressionProfile must be idiomatic.");
  }
  if (input?.lane === "equivalent") checkEquivalence(input, name, errors);
  else if (input?.lane === "open") checkOpenLane(input, name, errors);
  else push(errors, name + ".lane must be equivalent or open.");
  checkOracle(input?.oracle, name + ".oracle", errors);
  checkBackend(input?.backendAvailable, input?.claims, name, errors);
  checkBaseline(input?.baseline, name + ".baseline", errors);
  checkSamples(input?.samples, name + ".samples", errors,
    input?.backendAvailable, COMPARISON_ORDER);
  if (disposition === "deferred") {
    requiredString(input.blocker, name + ".blocker", errors);
    requiredString(input.taskId, name + ".taskId", errors);
    requiredString(input.stopCondition, name + ".stopCondition", errors);
  }
}

function checkProgramProfiles(profiles, name, errors) {
  if (!Array.isArray(profiles) || profiles.length !== LANGUAGE_PROFILES.length) {
    push(errors, name + " must contain exactly learner, idiomatic and frontier.");
    return new Map();
  }
  const byId = new Map();
  for (const [index, profile] of profiles.entries()) {
    const location = name + "[" + index + "]";
    if (!isObject(profile)) {
      push(errors, location + " must be an object.");
      continue;
    }
    if (!PROFILE_SET.has(profile.id)) {
      push(errors, location + ".id is not a required profile.");
    }
    if (byId.has(profile.id)) {
      push(errors, location + ".id duplicates " + profile.id + ".");
    }
    byId.set(profile.id, profile);
    if (profile.track !== "language") {
      push(errors, location + ".track must be language.");
    }
    requiredString(profile.description, location + ".description", errors);
    if (typeof profile.primary !== "boolean") {
      push(errors, location + ".primary must be boolean.");
    }
    if (typeof profile.regression !== "boolean") {
      push(errors, location + ".regression must be boolean.");
    }
  }
  for (const profileId of LANGUAGE_PROFILES) {
    if (!byId.has(profileId)) push(errors, name + " is missing " + profileId + ".");
  }
  const frontier = byId.get("frontier");
  if (!isObject(frontier?.disclosures)) {
    push(errors, name + ".frontier.disclosures must declare every frontier axis.");
  } else {
    for (const field of PROFILE_DISCLOSURES) {
      requiredString(frontier.disclosures[field],
        name + ".frontier.disclosures." + field, errors);
    }
    for (const field of Object.keys(frontier.disclosures)) {
      if (!PROFILE_DISCLOSURES.includes(field)) {
        push(errors, name + ".frontier.disclosures has an unknown axis " + field + ".");
      }
    }
    if (frontier.disclosures.targetSpecialization === "universal") {
      push(errors, name + ".frontier.disclosures.targetSpecialization must name a bounded target and fallback.");
    }
  }
  return byId;
}

export function expectedMatrixBlockers(scenario, stage) {
  if (scenario === "clean" && stage === "check-end-to-end") return [];
  const blockers = [];
  if (scenario === "no-op" || scenario === "edit") blockers.push("incremental-cache");
  if (INTERNAL_STAGES.has(stage)) blockers.push("stage-instrumentation");
  if (COMPONENT_STAGES.has(stage)) blockers.push(stage);
  return blockers;
}

export function expectedMatrixStatus(scenario, stage) {
  return scenario === "clean" && stage === "check-end-to-end" ? "ready" : "blocked";
}

function checkMatrix(matrix, name, identities, errors) {
  if (!isObject(matrix)) {
    push(errors, name + " must be an object. The compiler lifecycle must use a matrix.");
    return;
  }
  if (!sameArray(matrix.scenarios, [...LIFECYCLE_SCENARIOS])) {
    push(errors, name + ".scenarios must be clean, no-op and edit in order.");
  }
  if (!sameArray(matrix.stages, [...LIFECYCLE_STAGES])) {
    push(errors, name + ".stages must be check-end-to-end, source, lex, parse, semantic, hir, lowering, codegen and link in order.");
  }
  if (!Array.isArray(matrix.points) ||
      matrix.points.length !== LIFECYCLE_SCENARIOS.length * LIFECYCLE_STAGES.length) {
    push(errors, name + ".points must contain all 27 scenario-stage cells.");
    return;
  }
  const seen = new Set();
  for (const [index, point] of matrix.points.entries()) {
    const location = name + ".points[" + index + "]";
    if (!isObject(point)) {
      push(errors, location + " must be an object.");
      continue;
    }
    checkExactKeys(point, location, ["scenario", "stage", "status", "blockedBy"], errors);
    const scenario = point.scenario;
    const stage = point.stage;
    if (!SCENARIO_SET.has(scenario)) push(errors, location + ".scenario is invalid.");
    if (!STAGE_SET.has(stage)) push(errors, location + ".stage is invalid.");
    const key = String(scenario) + "\u0000" + String(stage);
    if (seen.has(key)) push(errors, location + " duplicates a scenario-stage cell.");
    seen.add(key);
    const expectedStatus = expectedMatrixStatus(scenario, stage);
    const expectedBlockers = expectedMatrixBlockers(scenario, stage);
    if (point.status !== expectedStatus) {
      push(errors, location + ".status must be " + expectedStatus + ".");
    }
    if (!sameArray(point.blockedBy ?? [], expectedBlockers)) {
      push(errors, location + ".blockedBy must be " +
        (expectedBlockers.length ? expectedBlockers.join(", ") : "empty") + ".");
    }
  }
  for (const scenario of LIFECYCLE_SCENARIOS) {
    for (const stage of LIFECYCLE_STAGES) {
      const key = scenario + "\u0000" + stage;
      if (!seen.has(key)) push(errors, name + " is missing " + scenario + " x " + stage + ".");
    }
  }
  for (const key of ["lifecycle", "phases", "phaseIdentities", "phaseMix"]) {
    if (hasOwn(matrix, key)) push(errors, name + " must not flatten lifecycle axes.");
  }
}

function checkCompilerLifecycle(input, name, errors) {
  if (input?.benchmarkDisposition !== "compiler-lifecycle") {
    push(errors, name + ".benchmarkDisposition must be compiler-lifecycle.");
  }
  if (hasOwn(input, "profiles")) {
    push(errors, name + ".profiles must not define language source profiles.");
  }
  if (input?.languageProfiles?.applicability !== "not-applicable" ||
      !requiredString(input?.languageProfiles?.reason,
        name + ".languageProfiles.reason", errors)) {
    push(errors, name + ".languageProfiles must mark language profiles not-applicable with a reason.");
  }
  checkIdentity(input?.source, name + ".source", errors);
  checkIdentity(input?.graph, name + ".graph", errors);
  checkIdentity(input?.inputIdentity, name + ".inputIdentity", errors);
  if (input?.source?.id !== SOURCE_ID ||
      input?.source?.digest !== SOURCE_FIXTURE.digest) {
    push(errors, name + ".source must use the current checker source identity.");
  }
  if (input?.graph?.id !== DESCRIPTOR_IDENTITIES.graph.id ||
      input?.graph?.digest !== DESCRIPTOR_IDENTITIES.graph.digest) {
    push(errors, name + ".graph must use the source-backed graph descriptor.");
  }
  if (input?.inputIdentity?.id !== DESCRIPTOR_IDENTITIES.input.id ||
      input?.inputIdentity?.digest !== DESCRIPTOR_IDENTITIES.input.digest) {
    push(errors, name + ".inputIdentity must use the source-backed invocation descriptor.");
  }
  const identities = {
    source: input.source,
    graph: input.graph,
    input: input.inputIdentity,
  };
  checkMatrix(input.matrix, name + ".matrix", identities, errors);
  for (const key of ["phases", "phaseIdentities", "phaseMix", "lifecycle"]) {
    if (hasOwn(input, key)) push(errors, name + "." + key + " must not flatten compiler lifecycle axes.");
  }
  checkOracle(input?.oracle, name + ".oracle", errors);
  checkBackend(input?.backendAvailable, input?.claims, name, errors);
  checkBaseline(input?.baseline, name + ".baseline", errors);
  if (input?.baseline?.primary !== "historical-w") {
    push(errors, name + ".baseline.primary must be historical-w.");
  }
  if (input?.baseline?.role !== "contextual-not-ranking") {
    push(errors, name + ".baseline.role must be contextual-not-ranking.");
  }
  if (input?.baseline?.recipe !== "equivalent") {
    push(errors, name + ".baseline.recipe must be equivalent.");
  }
  checkSamples(input?.samples, name + ".samples", errors,
    input?.backendAvailable, MEASUREMENT_ORDER);
}

function checkManifestDescriptor(descriptor, name, expectedKind, errors) {
  if (!isObject(descriptor)) {
    push(errors, name + " must be an object.");
    return;
  }
  if (descriptor.$schema !== "wbench/1-identity") {
    push(errors, name + ".$schema must be wbench/1-identity.");
  }
  if (descriptor.schema !== "wbench/1-identity") {
    push(errors, name + ".schema must be wbench/1-identity.");
  }
  if (descriptor.kind !== expectedKind) {
    push(errors, name + ".kind must be " + expectedKind + ".");
  }
  requiredString(descriptor.id, name + ".id", errors);
  checkSource(descriptor.source, name + ".source", errors);
  if (expectedKind === "source-graph") {
    if (!Array.isArray(descriptor.nodes) || descriptor.nodes.length !== 1) {
      push(errors, name + ".nodes must contain one source node.");
    }
    if (!Array.isArray(descriptor.edges) || descriptor.edges.length !== 0) {
      push(errors, name + ".edges must be empty for the seed graph.");
    }
    const node = descriptor.nodes?.[0];
    if (node?.id !== "checker_bootstrap" || node?.path !== SOURCE_FIXTURE.path ||
        node?.digest !== SOURCE_FIXTURE.digest) {
      push(errors, name + ".nodes[0] must identify checker_bootstrap with the current digest.");
    }
  }
  if (expectedKind === "invocation-edit-input") {
    const edit = descriptor.invocation?.edit;
    const recipe = descriptor.invocation?.recipe;
    if (!isObject(edit) || edit.semanticPreserving !== true ||
        edit.applyTo !== "temporary-copy") {
      push(errors, name + ".invocation.edit must be a semantic-preserving temporary-copy edit.");
    }
    if (!isObject(recipe) || recipe.operation !== EDIT_RECIPE.operation ||
        recipe.expectedMatches !== EDIT_RECIPE.expectedMatches ||
        recipe.semanticPreserving !== true || recipe.temporaryCopy !== true) {
      push(errors, name + ".invocation.recipe must be the bounded source-backed recipe.");
    }
  }
}

function checkDescriptorFile(identity, name, expectedKind, errors) {
  if (!isObject(identity)) {
    push(errors, name + " must be an object.");
    return;
  }
  const absolutePath = repositoryPath(identity.path);
  requiredString(identity.path, name + ".path", errors);
  checkIdentity(identity, name, errors);
  if (!absolutePath || !fs.existsSync(absolutePath) ||
      !fs.statSync(absolutePath).isFile()) {
    push(errors, name + ".path does not exist.");
    return;
  }
  const actualDigest = fileDigest(identity.path);
  if (actualDigest !== identity.digest) push(errors, name + ".digest is stale.");
  try {
    const descriptor = JSON.parse(fs.readFileSync(absolutePath, "utf8"));
    if (descriptor.id !== identity.id) {
      push(errors, name + ".path descriptor id must match its identity.");
    }
    checkManifestDescriptor(descriptor, name, expectedKind, errors);
  } catch (error) {
    push(errors, name + ".path is not valid JSON: " + error.message);
  }
}

function containsForbiddenInputKey(value, location, errors) {
  if (Array.isArray(value)) {
    value.forEach((item, index) =>
      containsForbiddenInputKey(item, location + "[" + index + "]", errors));
    return;
  }
  if (!isObject(value)) return;
  for (const [key, item] of Object.entries(value)) {
    if (["expected", "expectedResult", "result"].includes(key)) {
      push(errors, location + "." + key + " must not echo an expected result.");
    }
    containsForbiddenInputKey(item, location + "." + key, errors);
  }
}

function parseU64(value, name, errors) {
  if (typeof value !== "string" || !/^(?:0|[1-9]\d*)$/u.test(value)) {
    push(errors, name + " must be a decimal u64 string.");
    return undefined;
  }
  try {
    const parsed = BigInt(value);
    if (parsed > MAX_U64) {
      push(errors, name + " exceeds u64.");
      return undefined;
    }
    return parsed;
  } catch {
    push(errors, name + " must be a decimal u64 string.");
    return undefined;
  }
}

const COMPARISON_SEED_PATTERN = /^[0-9a-f]{64}$/u;
const COMPARISON_COMMIT_PATTERN = /^[0-9a-f]{40}$/u;

function hashHex(value) {
  return crypto.createHash("sha256").update(value, "utf8").digest("hex");
}

function compareStrings(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

function validateScheduleSeed(seed) {
  if (typeof seed !== "string" || !COMPARISON_SEED_PATTERN.test(seed)) {
    throw new TypeError("comparison schedule seed must be 64 lowercase hexadecimal characters.");
  }
}

function validatePairCount(pairCount) {
  if (!Number.isSafeInteger(pairCount) || pairCount < RESULT_MIN_SAMPLES || pairCount % 2 === 0) {
    throw new TypeError("comparison schedule pair count must be odd and at least 9.");
  }
}

/**
 * Build the versioned randomized balanced paired schedule.
 *
 * The seed is a runner-generated CSPRNG value. Hash-ranked orientation and
 * round shuffles make the schedule deterministic without exposing a seed CLI
 * option. Exactly ceil(N/2) rounds start with baseline.
 */
export function pairedScheduleForSeed(seed, pairCount) {
  validateScheduleSeed(seed);
  validatePairCount(pairCount);
  const orientationRanks = Array.from({ length: pairCount }, (_, index) => ({
    index,
    rank: hashHex(seed + "\u0000orientation\u0000" + index),
  })).sort((left, right) => compareStrings(left.rank, right.rank));
  const baselineFirst = new Set(
    orientationRanks.slice(0, Math.ceil(pairCount / 2)).map((entry) => entry.index),
  );
  const order = Array.from({ length: pairCount }, (_, index) => index);
  for (let index = order.length - 1; index > 0; index -= 1) {
    const rank = hashHex(seed + "\u0000round\u0000" + index);
    const choice = Number(BigInt("0x" + rank.slice(0, 16)) % BigInt(index + 1));
    [order[index], order[choice]] = [order[choice], order[index]];
  }
  return order.map((sourceIndex, index) => {
    const first = baselineFirst.has(sourceIndex) ? "baseline" : "candidate";
    return {
      round: index + 1,
      first,
      second: first === "baseline" ? "candidate" : "baseline",
    };
  });
}

export const createPairedSchedule = pairedScheduleForSeed;

function parsePositiveU64(value, name, errors) {
  const parsed = parseU64(value, name, errors);
  if (parsed === 0n) push(errors, name + " must be positive for paired comparison.");
  return parsed === 0n ? undefined : parsed;
}

function parseSignedDecimal(value, name, errors) {
  if (typeof value !== "string" || !/^(?:0|-?[1-9]\d*)$/u.test(value)) {
    push(errors, name + " must be a canonical signed decimal string.");
    return undefined;
  }
  try {
    return BigInt(value);
  } catch {
    push(errors, name + " must be a canonical signed decimal string.");
    return undefined;
  }
}

function roundedRatio(numerator, denominator) {
  if (denominator <= 0n) throw new RangeError("ratio denominator must be positive.");
  const negative = numerator < 0n;
  const absolute = negative ? -numerator : numerator;
  let quotient = absolute / denominator;
  const remainder = absolute % denominator;
  if (remainder * 2n >= denominator) quotient += 1n;
  return negative ? -quotient : quotient;
}

export function roundSignedRatio(numerator, denominator) {
  return roundedRatio(BigInt(numerator), BigInt(denominator));
}

function positiveLatency(values) {
  const parsed = values.map((value) => typeof value === "bigint" ? value : BigInt(value));
  if (parsed.length < RESULT_MIN_SAMPLES || parsed.length % 2 === 0 || parsed.some((value) => value <= 0n)) {
    throw new TypeError("paired latency requires an odd positive raw sample count of at least 9.");
  }
  const center = median(parsed);
  const deviations = parsed.map((value) => value >= center ? value - center : center - value);
  return {
    unit: "ns",
    minimumNs: parsed.reduce((minimum, value) => value < minimum ? value : minimum).toString(),
    medianNs: center.toString(),
    maximumNs: parsed.reduce((maximum, value) => value > maximum ? value : maximum).toString(),
    madNs: median(deviations).toString(),
    derivedFromRawSamples: true,
  };
}

function signedSummary(values, suffix) {
  if (values.length < RESULT_MIN_SAMPLES || values.length % 2 === 0) {
    throw new TypeError("paired derived values require an odd count of at least 9.");
  }
  const minimum = values.reduce((left, right) => left < right ? left : right);
  const maximum = values.reduce((left, right) => left > right ? left : right);
  return {
    ["minimum" + suffix]: minimum.toString(),
    ["median" + suffix]: median(values).toString(),
    ["maximum" + suffix]: maximum.toString(),
  };
}

export function calculatePairedComparison(rawSamples, pairCount = undefined) {
  if (!Array.isArray(rawSamples)) throw new TypeError("paired raw samples must be an array.");
  const grouped = new Map([["baseline", new Map()], ["candidate", new Map()]]);
  for (const sample of rawSamples) {
    if (!isObject(sample) || !["baseline", "candidate"].includes(sample.series)) {
      throw new TypeError("paired raw samples must identify baseline or candidate.");
    }
    const ns = BigInt(sample.ns);
    if (ns <= 0n) throw new TypeError("paired raw samples must use positive nanoseconds.");
    const round = Number(sample.round);
    if (!Number.isSafeInteger(round) || round < 1) throw new TypeError("paired raw samples must use positive rounds.");
    const values = grouped.get(sample.series);
    if (values.has(round)) throw new TypeError("paired raw samples must not duplicate a round.");
    values.set(round, ns);
  }
  const count = pairCount ?? grouped.get("baseline").size;
  validatePairCount(count);
  const baseline = grouped.get("baseline");
  const candidate = grouped.get("candidate");
  if (baseline.size !== count || candidate.size !== count ||
      [...baseline.keys()].some((round) => !candidate.has(round))) {
    throw new TypeError("paired raw samples must contain one baseline and candidate value per round.");
  }
  const pairs = Array.from({ length: count }, (_, index) => {
    const round = index + 1;
    const baselineNs = baseline.get(round);
    const candidateNs = candidate.get(round);
    const deltaNs = candidateNs - baselineNs;
    const relativePpm = roundedRatio(deltaNs * 1_000_000n, baselineNs);
    return {
      round,
      baselineNs: baselineNs.toString(),
      candidateNs: candidateNs.toString(),
      deltaNs: deltaNs.toString(),
      relativePpm: relativePpm.toString(),
    };
  });
  const deltas = pairs.map((pair) => BigInt(pair.deltaNs));
  const ppm = pairs.map((pair) => BigInt(pair.relativePpm));
  return {
    baseline: positiveLatency([...baseline.values()]),
    candidate: positiveLatency([...candidate.values()]),
    pairs,
    delta: signedSummary(deltas, "Ns"),
    relativePpm: signedSummary(ppm, "Ppm"),
    counts: {
      faster: deltas.filter((value) => value < 0n).length,
      tied: deltas.filter((value) => value === 0n).length,
      slower: deltas.filter((value) => value > 0n).length,
    },
  };
}

function checkStringSet(value, name, errors, minimum = 0) {
  if (!Array.isArray(value)) {
    push(errors, name + " must be an array of non-empty strings.");
    return new Set();
  }
  if (value.length < minimum) {
    push(errors, name + " must contain at least " + minimum + " item(s).");
  }
  const seen = new Set();
  for (const [index, item] of value.entries()) {
    if (!requiredString(item, name + "[" + index + "]", errors)) continue;
    if (seen.has(item)) push(errors, name + " must not contain duplicates.");
    seen.add(item);
  }
  return seen;
}

function checkResultSample(sample, name, errors) {
  if (!isObject(sample)) {
    push(errors, name + " must be an object.");
    return undefined;
  }
  checkExactKeys(sample, name, ["ns"], errors);
  return parseU64(sample.ns, name + ".ns", errors);
}

function median(values) {
  const sorted = [...values].sort((left, right) => left < right ? -1 : left > right ? 1 : 0);
  return sorted[Math.floor(sorted.length / 2)];
}

function expectedLatency(raw) {
  const center = median(raw);
  const deviations = raw.map((value) => value >= center ? value - center : center - value);
  return {
    minimumNs: raw.reduce((minimum, value) => value < minimum ? value : minimum),
    medianNs: center,
    maximumNs: raw.reduce((maximum, value) => value > maximum ? value : maximum),
    madNs: median(deviations),
  };
}

function checkResultComparison(comparison, result, errors) {
  if (comparison !== null) {
    push(errors, "result.comparison must be null; comparison is blocked by interleaved-comparison-runner.");
  }
  if (result.quality !== "exploratory") {
    push(errors, "result.quality must be exploratory; regression is blocked by interleaved-comparison-runner.");
  }
  if (result.claim !== "measurement-only") {
    push(errors, "result.claim must be measurement-only; regression is blocked by interleaved-comparison-runner.");
  }
}

function loadCurrentManifest() {
  try {
    return JSON.parse(fs.readFileSync(
      path.join(ROOT, "benchmarks", "seed-check-lifecycle.manifest.json"), "utf8"));
  } catch {
    return undefined;
  }
}

function checkResultWorkload(workload, name, manifest, errors) {
  if (!isObject(workload)) {
    push(errors, name + " must be an object.");
    return;
  }
  checkExactKeys(workload, name, ["manifestDigest", "track", "lane", "scenario", "stage", "subject", "profile"], errors);
  requiredDigest(workload.manifestDigest, name + ".manifestDigest", errors);
  if (workload.track !== "compiler-lifecycle") {
    push(errors, name + ".track must be compiler-lifecycle.");
  }
  if (!LANE_SET.has(workload.lane)) push(errors, name + ".lane must be equivalent or open.");
  if (!SCENARIO_SET.has(workload.scenario)) {
    push(errors, name + ".scenario must be clean, no-op or edit.");
  }
  if (!STAGE_SET.has(workload.stage)) {
    push(errors, name + ".stage must be a compiler lifecycle stage.");
  }
  requiredString(workload.subject, name + ".subject", errors);
  if (workload.profile !== null) {
    push(errors, name + ".profile must be null for compiler lifecycle.");
  }
  const expectedManifestDigest = fileDigest(
    "benchmarks/seed-check-lifecycle.manifest.json");
  if (expectedManifestDigest && workload.manifestDigest !== expectedManifestDigest) {
    push(errors, name + ".manifestDigest is stale.");
  }
  if (manifest) {
    if (workload.manifestDigest !== fileDigest(
      "benchmarks/seed-check-lifecycle.manifest.json")) {
      push(errors, name + ".manifestDigest does not identify the supplied manifest.");
    }
    if (workload.lane !== manifest.lane) {
      push(errors, name + ".lane must match the manifest.");
    }
    const point = manifest.matrix?.points?.find((candidate) =>
      candidate?.scenario === workload.scenario &&
      candidate?.stage === workload.stage);
    if (!point) {
      push(errors, name + " does not identify a manifest matrix point.");
    } else if (point.status !== "ready") {
      push(errors, name + " identifies a blocked matrix point.");
    }
  }
}

function checkResultIdentity(identity, name, manifest, errors) {
  if (!isObject(identity)) {
    push(errors, name + " must be an object.");
    return;
  }
  checkExactKeys(identity, name, ["source", "graph", "input", "command"], errors);
  checkSource(identity.source, name + ".source", errors);
  checkDescriptorFile(identity.graph, name + ".graph", "source-graph", errors);
  checkDescriptorFile(identity.input, name + ".input", "invocation-edit-input", errors);
  if (!isObject(identity.command)) {
    push(errors, name + ".command must be an object.");
  } else {
    checkExactKeys(identity.command, name + ".command", ["tool", "operation", "arguments"], errors);
    if (identity.command.tool !== "w" || identity.command.operation !== "check" ||
        !Array.isArray(identity.command.arguments) || identity.command.arguments.length === 0) {
      push(errors, name + ".command must invoke w check with arguments.");
    }
  }
  if (!manifest) return;
  if (JSON.stringify(identity.source) !== JSON.stringify(manifest.identity?.source)) {
    push(errors, name + ".source must match the manifest identity.");
  }
  if (JSON.stringify(identity.graph) !== JSON.stringify(manifest.identity?.graph)) {
    push(errors, name + ".graph must match the manifest identity.");
  }
  if (JSON.stringify(identity.input) !== JSON.stringify(manifest.identity?.input)) {
    push(errors, name + ".input must match the manifest identity.");
  }
  if (JSON.stringify(identity.command) !== JSON.stringify(manifest.command)) {
    push(errors, name + ".command must match the manifest command.");
  }
}

function requiredCommit(value, name, errors) {
  if (typeof value !== "string" || !COMPARISON_COMMIT_PATTERN.test(value)) {
    push(errors, name + " must be a complete 40-hex Git commit SHA.");
    return false;
  }
  return true;
}

function checkComparisonRole(role, name, errors) {
  if (!isObject(role)) {
    push(errors, name + " must be an object.");
    return;
  }
  checkExactKeys(role, name, [
    "commit", "closureDigest", "artifactDigest", "recipeDigest",
    "recipeClassDigest", "toolchainDigest",
  ], errors);
  requiredCommit(role.commit, name + ".commit", errors);
  for (const field of [
    "closureDigest", "artifactDigest", "recipeDigest", "recipeClassDigest", "toolchainDigest",
  ]) requiredDigest(role[field], name + "." + field, errors);
}

function comparisonWorkloadDigest(result) {
  const payload = {
    manifestDigest: result.workload?.manifestDigest,
    track: result.workload?.track,
    lane: result.workload?.lane,
    scenario: result.workload?.scenario,
    stage: result.workload?.stage,
    subject: result.workload?.subject,
    profile: result.workload?.profile,
    source: result.identity?.source,
    graph: result.identity?.graph,
    input: result.identity?.input,
    command: result.identity?.command,
  };
  return "sha256:" + crypto.createHash("sha256").update(JSON.stringify(payload)).digest("hex");
}

function checkPositiveResultSample(sample, name, errors) {
  if (!isObject(sample)) {
    push(errors, name + " must be an object.");
    return undefined;
  }
  checkExactKeys(sample, name, ["round", "series", "position", "ns"], errors);
  if (!Number.isInteger(sample.round) || sample.round < 0) {
    push(errors, name + ".round must be a non-negative integer.");
  }
  if (!["baseline", "candidate"].includes(sample.series)) {
    push(errors, name + ".series must be baseline or candidate.");
  }
  if (!["first", "second"].includes(sample.position)) {
    push(errors, name + ".position must be first or second.");
  }
  return parsePositiveU64(sample.ns, name + ".ns", errors);
}

function checkLatencyShape(metric, name, errors) {
  const fields = ["unit", "minimumNs", "medianNs", "maximumNs", "madNs", "derivedFromRawSamples"];
  if (!isObject(metric) || !sameArray(Object.keys(metric).sort(), [...fields].sort())) {
    push(errors, name + " must use the closed latency shape.");
    return;
  }
  if (metric.unit !== "ns") push(errors, name + ".unit must be ns.");
  if (metric.derivedFromRawSamples !== true) push(errors, name + " must derive only from raw samples.");
  for (const field of ["minimumNs", "medianNs", "maximumNs"]) {
    parsePositiveU64(metric[field], name + "." + field, errors);
  }
  parseU64(metric.madNs, name + ".madNs", errors);
}

function checkComparisonSignedSummary(summary, suffix, name, errors) {
  const fields = ["minimum" + suffix, "median" + suffix, "maximum" + suffix];
  if (!isObject(summary) || !sameArray(Object.keys(summary).sort(), [...fields].sort())) {
    push(errors, name + " must use the closed signed summary shape.");
    return;
  }
  for (const field of fields) parseSignedDecimal(summary[field], name + "." + field, errors);
}

function checkComparisonResult(result, manifest, errors) {
  checkExactKeys(result, "result", [
    "$schema", "schema", "kind", "id", "status", "quality", "claim",
    "workload", "identity", "comparison", "verdict", "oracle", "samples", "environment",
    "provenance", "metrics", "summary", "semanticDeviations", "disclosures",
  ], errors);
  if (result.$schema !== "./wbench-1.schema.json") push(errors, "result.$schema must identify wbench-1.schema.json.");
  if (result.schema !== SCHEMA_VERSION) push(errors, "result.schema must be wbench/1.");
  if (result.kind !== "result") push(errors, "result.kind must be result.");
  if (result.status !== "recorded") push(errors, "result.status must be recorded.");
  requiredString(result.id, "result.id", errors);
  if (result.quality !== "exploratory") push(errors, "comparison result quality must be exploratory.");
  if (result.claim === "regression" || result.quality === "regression-grade") {
    push(errors, "regression-grade/regression requires " + REGRESSION_BLOCKER + " with controlled provider, repetition/uncertainty policy and threshold.");
  }
  if (result.claim !== COMPARISON_CLAIM) push(errors, "comparison result claim must be comparison-only.");
  if (result.verdict !== "not-evaluated") push(errors, "comparison result verdict must be not-evaluated.");
  checkResultWorkload(result.workload, "result.workload", manifest, errors);
  checkResultIdentity(result.identity, "result.identity", manifest, errors);

  const comparison = result.comparison;
  if (!isObject(comparison)) {
    push(errors, "result.comparison must be a complete comparison object.");
  } else {
    checkExactKeys(comparison, "result.comparison", [
      "baseline", "candidate", "calibration", "verdict", "pairs", "delta",
      "relativePpm", "counts", "noisePolicy",
    ], errors);
    checkComparisonRole(comparison.baseline, "result.comparison.baseline", errors);
    checkComparisonRole(comparison.candidate, "result.comparison.candidate", errors);
    if (comparison.calibration !== (comparison.baseline?.closureDigest === comparison.candidate?.closureDigest)) {
      push(errors, "result.comparison.calibration must derive from compiler/seed-c closure digests.");
    }
    if (comparison.verdict !== "not-evaluated") push(errors, "result.comparison.verdict must be not-evaluated.");
    checkComparisonSignedSummary(comparison.delta, "Ns", "result.comparison.delta", errors);
    checkComparisonSignedSummary(comparison.relativePpm, "Ppm", "result.comparison.relativePpm", errors);
    if (!isObject(comparison.counts) ||
        !sameArray(Object.keys(comparison.counts).sort(), ["faster", "slower", "tied"].sort())) {
      push(errors, "result.comparison.counts must contain faster, tied and slower.");
    } else {
      for (const field of ["faster", "tied", "slower"]) {
        if (!Number.isSafeInteger(comparison.counts[field]) || comparison.counts[field] < 0) {
          push(errors, "result.comparison.counts." + field + " must be a non-negative integer.");
        }
      }
    }
    if (!isObject(comparison.noisePolicy) ||
        !checkExactKeys(comparison.noisePolicy, "result.comparison.noisePolicy", ["known", "unknown"], errors)) {
      push(errors, "result.comparison.noisePolicy must record known and unknown controls.");
    } else {
      checkStringSet(comparison.noisePolicy.known, "result.comparison.noisePolicy.known", errors);
      checkStringSet(comparison.noisePolicy.unknown, "result.comparison.noisePolicy.unknown", errors, 1);
    }
  }

  const oracle = result.oracle;
  if (!isObject(oracle) || !checkExactKeys(oracle, "result.oracle", ["baseline", "candidate", "complete", "beforeSamples"], errors)) {
    push(errors, "result.oracle must contain both role validations and completion flags.");
  } else {
    for (const role of ["baseline", "candidate"]) {
      const evidence = oracle[role];
      if (!isObject(evidence) || !checkExactKeys(evidence, "result.oracle." + role, ["validationDigest", "complete", "beforeSamples"], errors)) {
        push(errors, "result.oracle." + role + " must be closed.");
      } else {
        requiredDigest(evidence.validationDigest, "result.oracle." + role + ".validationDigest", errors);
        requiredBoolean(evidence.complete, "result.oracle." + role + ".complete", errors);
        requiredBoolean(evidence.beforeSamples, "result.oracle." + role + ".beforeSamples", errors);
      }
    }
    requiredBoolean(oracle.complete, "result.oracle.complete", errors);
    requiredBoolean(oracle.beforeSamples, "result.oracle.beforeSamples", errors);
  }

  const samples = result.samples;
  let rawValues = [];
  let pairCount;
  let computed;
  if (!isObject(samples) || !checkExactKeys(samples, "result.samples", ["raw", "warmup", "stopRule", "clock", "order", "schedule"], errors)) {
    push(errors, "result.samples must use the closed paired shape.");
  } else {
    if (!isObject(samples.stopRule) ||
        !checkExactKeys(samples.stopRule, "result.samples.stopRule", ["kind", "pairs"], errors) ||
        samples.stopRule.kind !== "fixed-pairs" || !Number.isSafeInteger(samples.stopRule.pairs) ||
        samples.stopRule.pairs < RESULT_MIN_SAMPLES || samples.stopRule.pairs % 2 === 0) {
      push(errors, "result.samples.stopRule must be fixed-pairs with odd pairs >= 9.");
    } else pairCount = samples.stopRule.pairs;
    if (samples.clock !== RESULT_CLOCK) push(errors, "result.samples.clock must be monotonic-wall-ns.");
    if (samples.order !== PAIRED_COMPARISON_ORDER) push(errors, "result.samples.order must be " + PAIRED_COMPARISON_ORDER + ".");
    const schedule = samples.schedule;
    if (!isObject(schedule) || !checkExactKeys(schedule, "result.samples.schedule", ["algorithm", "seed", "rounds"], errors)) {
      push(errors, "result.samples.schedule must identify the versioned seed and order.");
    } else {
      if (schedule.algorithm !== PAIRED_COMPARISON_ORDER) push(errors, "result.samples.schedule.algorithm must be " + PAIRED_COMPARISON_ORDER + ".");
      if (typeof schedule.seed !== "string" || !COMPARISON_SEED_PATTERN.test(schedule.seed)) {
        push(errors, "result.samples.schedule.seed must be 64 lowercase hexadecimal characters.");
      } else if (pairCount !== undefined) {
        try {
          const expectedSchedule = pairedScheduleForSeed(schedule.seed, pairCount);
          if (!Array.isArray(schedule.rounds) || JSON.stringify(schedule.rounds) !== JSON.stringify(expectedSchedule)) {
            push(errors, "result.samples.schedule.rounds do not match the seed-derived schedule.");
          }
        } catch (error) {
          push(errors, "result.samples.schedule is invalid: " + error.message);
        }
      }
    }
    if (!Array.isArray(samples.raw) || pairCount === undefined || samples.raw.length !== pairCount * 2) {
      push(errors, "result.samples.raw must contain exactly two samples per fixed pair.");
    } else {
      rawValues = samples.raw.map((sample, index) =>
        checkPositiveResultSample(sample, "result.samples.raw[" + index + "]", errors));
      const rounds = samples.schedule?.rounds;
      if (Array.isArray(rounds) && rounds.length === pairCount) {
        for (let index = 0; index < samples.raw.length; index += 1) {
          const sample = samples.raw[index];
          const roundIndex = Math.floor(index / 2);
          const expected = rounds[roundIndex];
          const expectedPosition = index % 2 === 0 ? "first" : "second";
          const expectedSeries = index % 2 === 0 ? expected?.first : expected?.second;
          if (sample.round !== roundIndex + 1 || sample.position !== expectedPosition || sample.series !== expectedSeries) {
            push(errors, "result.samples.raw order does not match the declared seed-derived schedule.");
            break;
          }
        }
      }
    }
    if (!Array.isArray(samples.warmup) || samples.warmup.length < 2 || samples.warmup.length % 2 !== 0) {
      push(errors, "result.samples.warmup must contain at least one complete pair.");
    } else {
      for (let index = 0; index < samples.warmup.length; index += 1) {
        const sample = samples.warmup[index];
        checkPositiveResultSample(sample, "result.samples.warmup[" + index + "]", errors);
        const expectedPosition = index % 2 === 0 ? "first" : "second";
        const expectedSeries = index % 2 === 0 ? samples.schedule?.rounds?.[0]?.first : samples.schedule?.rounds?.[0]?.second;
        const expectedRound = Math.floor(index / 2) + 1;
        if (sample?.round !== expectedRound || sample?.position !== expectedPosition || sample?.series !== expectedSeries) {
          push(errors, "result.samples.warmup must use contiguous rounds 1..warmupPairCount with the first schedule orientation.");
          break;
        }
      }
    }
    if (Array.isArray(samples.raw) && pairCount !== undefined && rawValues.every((value) => value !== undefined)) {
      try {
        computed = calculatePairedComparison(samples.raw, pairCount);
      } catch (error) {
        push(errors, "result.samples raw comparison is invalid: " + error.message);
      }
    }
  }

  const environment = result.environment;
  if (!isObject(environment) || !checkExactKeys(environment, "result.environment", ["hardware", "kernel", "target", "provider", "toolchain", "flags", "noiseControls"], errors)) {
    push(errors, "result.environment must use the closed environment shape.");
  } else {
    for (const field of ["hardware", "kernel", "target", "provider", "toolchain"]) requiredString(environment[field], "result.environment." + field, errors);
    checkStringSet(environment.flags, "result.environment.flags", errors, 1);
    if (!isObject(environment.noiseControls) || !checkExactKeys(environment.noiseControls, "result.environment.noiseControls", ["known", "unknown"], errors)) {
      push(errors, "result.environment.noiseControls must record known and unknown controls.");
    } else {
      const known = checkStringSet(environment.noiseControls.known, "result.environment.noiseControls.known", errors);
      const unknown = checkStringSet(environment.noiseControls.unknown, "result.environment.noiseControls.unknown", errors, 1);
      for (const value of known) if (unknown.has(value)) {
        push(errors, "result.environment.noiseControls.known and unknown must not overlap.");
        break;
      }
    }
  }

  const provenance = result.provenance;
  if (!isObject(provenance) || !checkExactKeys(provenance, "result.provenance", ["sourceDigest", "graphDigest", "inputDigest", "workloadDigest", "runnerDigest", "baseline", "candidate"], errors)) {
    push(errors, "result.provenance must record shared and per-role identities.");
  } else {
    for (const field of ["sourceDigest", "graphDigest", "inputDigest", "workloadDigest", "runnerDigest"]) requiredDigest(provenance[field], "result.provenance." + field, errors);
    if (provenance.sourceDigest !== SOURCE_FIXTURE.digest) push(errors, "result.provenance.sourceDigest must identify the current fixture.");
    if (provenance.graphDigest !== DESCRIPTOR_IDENTITIES.graph.digest) push(errors, "result.provenance.graphDigest must identify the current graph.");
    if (provenance.inputDigest !== DESCRIPTOR_IDENTITIES.input.digest) push(errors, "result.provenance.inputDigest must identify the current input.");
    if (provenance.workloadDigest !== comparisonWorkloadDigest(result)) push(errors, "result.provenance.workloadDigest must derive from the current workload identity.");
    checkComparisonRole(provenance.baseline, "result.provenance.baseline", errors);
    checkComparisonRole(provenance.candidate, "result.provenance.candidate", errors);
    if (comparison && JSON.stringify(provenance.baseline) !== JSON.stringify(comparison.baseline)) push(errors, "result.provenance.baseline must match comparison.baseline.");
    if (comparison && JSON.stringify(provenance.candidate) !== JSON.stringify(comparison.candidate)) push(errors, "result.provenance.candidate must match comparison.candidate.");
    if (provenance.baseline?.recipeClassDigest !== provenance.candidate?.recipeClassDigest) push(errors, "baseline and candidate recipe-class digests must match before samples.");
    if (provenance.baseline?.toolchainDigest !== provenance.candidate?.toolchainDigest) push(errors, "baseline and candidate toolchain digests must match before samples.");
  }

  if (!isObject(result.metrics) || !sameArray(Object.keys(result.metrics).sort(), ["baseline", "candidate"].sort())) {
    push(errors, "result.metrics must contain baseline and candidate latency metrics.");
  } else {
    checkLatencyShape(result.metrics.baseline, "result.metrics.baseline", errors);
    checkLatencyShape(result.metrics.candidate, "result.metrics.candidate", errors);
    if (computed) {
      if (JSON.stringify(result.metrics.baseline) !== JSON.stringify(computed.baseline)) push(errors, "result.metrics.baseline does not match raw samples.");
      if (JSON.stringify(result.metrics.candidate) !== JSON.stringify(computed.candidate)) push(errors, "result.metrics.candidate does not match raw samples.");
    }
  }
  if (!isObject(result.summary) || !checkExactKeys(result.summary, "result.summary", ["pairCount", "warmupPairCount", "derivedFromRawSamples"], errors)) {
    push(errors, "result.summary must use the closed paired shape.");
  } else {
    if (!Number.isSafeInteger(result.summary.pairCount) || result.summary.pairCount !== pairCount) push(errors, "result.summary.pairCount must equal raw pair count.");
    if (!Number.isSafeInteger(result.summary.warmupPairCount) || result.summary.warmupPairCount !== (Array.isArray(samples?.warmup) ? samples.warmup.length / 2 : -1)) push(errors, "result.summary.warmupPairCount must equal warmup pair count.");
    if (result.summary.derivedFromRawSamples !== true) push(errors, "result.summary must derive only from raw samples.");
  }
  if (computed && comparison) {
    for (const field of ["pairs", "delta", "relativePpm", "counts"]) {
      if (JSON.stringify(comparison[field]) !== JSON.stringify(computed[field])) push(errors, "result.comparison." + field + " does not match raw samples.");
    }
  }
  checkStringSet(result.semanticDeviations, "result.semanticDeviations", errors);
  checkStringSet(result.disclosures, "result.disclosures", errors);
  if (hasOwn(result, "timing") || hasOwn(result, "timings") || hasOwn(result, "expected") || hasOwn(result, "expectedResult") || hasOwn(result, "overwrite") || hasOwn(result, "force") || hasOwn(result, "outputPath")) {
    push(errors, "result must not contain tracked timing or expected output fields.");
  }
  return errors;
}

export function validateResult(result, manifest = loadCurrentManifest()) {
  const errors = [];
  if (!isObject(result)) return ["result must be an object."];
  if (result.claim === COMPARISON_CLAIM || result.verdict === "not-evaluated" ||
      (result.comparison !== null && result.claim !== "measurement-only")) {
    return checkComparisonResult(result, manifest, errors);
  }
  checkExactKeys(result, "result", [
    "$schema", "schema", "kind", "id", "status", "quality", "claim",
    "workload", "identity", "comparison", "oracle", "samples", "environment",
    "provenance", "metrics", "summary", "semanticDeviations", "disclosures",
  ], errors);
  if (result.$schema !== "./wbench-1.schema.json") {
    push(errors, "result.$schema must identify wbench-1.schema.json.");
  }
  if (result.schema !== SCHEMA_VERSION) push(errors, "result.schema must be wbench/1.");
  if (result.kind !== "result") push(errors, "result.kind must be result.");
  if (result.status !== "recorded") push(errors, "result.status must be recorded.");
  requiredString(result.id, "result.id", errors);
  if (!["exploratory", "regression-grade"].includes(result.quality)) {
    push(errors, "result.quality must be exploratory or regression-grade.");
  }
  if (!["measurement-only", "regression"].includes(result.claim)) {
    push(errors, "result.claim must be measurement-only or regression.");
  }
  checkResultWorkload(result.workload, "result.workload", manifest, errors);
  checkResultIdentity(result.identity, "result.identity", manifest, errors);
  if (!hasOwn(result, "comparison")) {
    push(errors, "result.comparison must be present and null for measurement-only.");
  }
  checkResultComparison(result.comparison, result, errors);

  const oracle = result.oracle;
  if (!isObject(oracle)) {
    push(errors, "result.oracle must be an object.");
  } else {
    checkExactKeys(oracle, "result.oracle", ["validationDigest", "complete", "beforeSamples"], errors);
    requiredDigest(oracle.validationDigest, "result.oracle.validationDigest", errors);
    requiredBoolean(oracle.complete, "result.oracle.complete", errors);
    requiredBoolean(oracle.beforeSamples, "result.oracle.beforeSamples", errors);
  }

  const samples = result.samples;
  let rawValues = [];
  let warmupValues = [];
  if (!isObject(samples)) {
    push(errors, "result.samples must be an object.");
  } else {
    checkExactKeys(samples, "result.samples", ["raw", "warmup", "stopRule", "clock", "order"], errors);
    if (!Array.isArray(samples.raw) || samples.raw.length < RESULT_MIN_SAMPLES) {
      push(errors, "result.samples.raw must contain at least " + RESULT_MIN_SAMPLES + " samples.");
    } else {
      rawValues = samples.raw.map((sample, index) =>
        checkResultSample(sample, "result.samples.raw[" + index + "]", errors));
      if (samples.raw.length % 2 === 0) {
        push(errors, "result.samples.raw count must be odd.");
      }
    }
    if (!Array.isArray(samples.warmup) || samples.warmup.length < RESULT_MIN_WARMUP) {
      push(errors, "result.samples.warmup must contain at least " + RESULT_MIN_WARMUP + " sample.");
    } else {
      warmupValues = samples.warmup.map((sample, index) =>
        checkResultSample(sample, "result.samples.warmup[" + index + "]", errors));
    }
    if (!isObject(samples.stopRule) ||
        !checkExactKeys(samples.stopRule, "result.samples.stopRule", ["kind", "count"], errors) ||
        samples.stopRule.kind !== "fixed-count" ||
        !Number.isInteger(samples.stopRule.count) ||
        samples.stopRule.count !== samples.raw?.length) {
      push(errors, "result.samples.stopRule must be fixed-count and equal raw sample count.");
    }
    if (samples.clock !== RESULT_CLOCK) {
      push(errors, "result.samples.clock must be monotonic-wall-ns.");
    }
    if (samples.order !== MEASUREMENT_ORDER) {
      push(errors, "result.samples.order must be single-series; interleaved comparison is blocked by interleaved-comparison-runner.");
    }
  }

  const environment = result.environment;
  if (!isObject(environment)) {
    push(errors, "result.environment must be an object.");
  } else {
    checkExactKeys(environment, "result.environment", ["hardware", "kernel", "target", "provider", "toolchain", "flags", "noiseControls"], errors);
    for (const field of ["hardware", "kernel", "target", "provider", "toolchain"]) {
      requiredString(environment[field], "result.environment." + field, errors);
    }
    checkStringSet(environment.flags, "result.environment.flags", errors, 1);
    if (!isObject(environment.noiseControls) ||
        !checkExactKeys(environment.noiseControls, "result.environment.noiseControls", ["known", "unknown"], errors)) {
      push(errors, "result.environment.noiseControls must record known and unknown controls.");
    } else {
      const known = checkStringSet(environment.noiseControls.known,
        "result.environment.noiseControls.known", errors);
      const unknown = checkStringSet(environment.noiseControls.unknown,
        "result.environment.noiseControls.unknown", errors);
      for (const value of known) {
        if (unknown.has(value)) {
          push(errors, "result.environment.noiseControls.known and unknown must not overlap.");
          break;
        }
      }
    }
  }

  const provenance = result.provenance;
  if (!isObject(provenance)) {
    push(errors, "result.provenance must be an object.");
  } else {
    checkExactKeys(provenance, "result.provenance", ["sourceDigest", "artifactDigest", "inputDigest", "recipeDigest", "runnerDigest", "toolchainDigest"], errors);
    for (const field of [
      "sourceDigest",
      "artifactDigest",
      "inputDigest",
      "recipeDigest",
      "runnerDigest",
      "toolchainDigest",
    ]) requiredDigest(provenance[field], "result.provenance." + field, errors);
    if (provenance.sourceDigest !== SOURCE_FIXTURE.digest) {
      push(errors, "result.provenance.sourceDigest must identify the current fixture.");
    }
    if (provenance.inputDigest !== DESCRIPTOR_IDENTITIES.input.digest) {
      push(errors, "result.provenance.inputDigest must identify the current input.");
    }
  }

  const rawIsValid = rawValues.length >= RESULT_MIN_SAMPLES &&
    rawValues.every((value) => value !== undefined) && rawValues.length % 2 === 1;
  if (!isObject(result.metrics) || Object.keys(result.metrics).length !== 1 ||
      !hasOwn(result.metrics, "latency")) {
    push(errors, "result.metrics must contain only the derived latency metric.");
  } else {
    const metric = result.metrics.latency;
    const metricFields = ["unit", "minimumNs", "medianNs", "maximumNs", "madNs", "derivedFromRawSamples"];
    if (!isObject(metric) || !sameArray(Object.keys(metric).sort(), [...metricFields].sort())) {
      push(errors, "result.metrics.latency must use the closed latency shape.");
    } else {
      if (metric.unit !== "ns") push(errors, "result.metrics.latency.unit must be ns.");
      if (metric.derivedFromRawSamples !== true) {
        push(errors, "result.metrics.latency must derive only from raw samples.");
      }
      const expected = rawIsValid ? expectedLatency(rawValues) : undefined;
      for (const field of ["minimumNs", "medianNs", "maximumNs", "madNs"]) {
        const actual = parseU64(metric[field], "result.metrics.latency." + field, errors);
        if (expected && actual !== expected[field]) {
          push(errors, "result.metrics.latency." + field + " does not match raw samples.");
        }
      }
    }
  }
  if (!isObject(result.summary) ||
      !sameArray(Object.keys(result.summary).sort(), ["derivedFromRawSamples", "sampleCount", "warmupCount"].sort())) {
    push(errors, "result.summary must use the closed derived shape.");
  } else {
    if (result.summary.derivedFromRawSamples !== true) {
      push(errors, "result.summary must derive only from raw samples.");
    }
    if (!Number.isInteger(result.summary.sampleCount) ||
        result.summary.sampleCount !== rawValues.length) {
      push(errors, "result.summary.sampleCount must equal raw sample count.");
    }
    if (!Number.isInteger(result.summary.warmupCount) ||
        result.summary.warmupCount !== warmupValues.length) {
      push(errors, "result.summary.warmupCount must equal warmup sample count.");
    }
  }
  checkStringSet(result.semanticDeviations, "result.semanticDeviations", errors);
  checkStringSet(result.disclosures, "result.disclosures", errors);
  if (hasOwn(result, "timing") || hasOwn(result, "timings") ||
      hasOwn(result, "expected") || hasOwn(result, "expectedResult") ||
      hasOwn(result, "overwrite") || hasOwn(result, "force") ||
      hasOwn(result, "outputPath")) {
    push(errors, "result must not contain tracked timing or expected output fields.");
  }
  return errors;
}

function checkTaskGraph(tasks, corpusIds, errors) {
  const expected = [
    "protocol",
    "seed-compiler-lifecycle",
    "language-catalog",
    "core-language-units",
    "computer-language-benchmarks-game",
    "restaurant-composition",
  ];
  if (!Array.isArray(tasks) || !sameArray(tasks.map((task) => task?.id), expected)) {
    push(errors, "program.tasks must use the six BMD3 task projections in order.");
    return;
  }
  const ids = new Set(expected);
  const comparisonCaseIds = [...corpusIds].filter((caseId) => caseId.startsWith("BMD2-W-1489-")).sort();
  const languageCaseIds = [...corpusIds].filter((caseId) => caseId.startsWith("BMD3-W-1490-")).sort();
  const seen = new Set();
  for (const [index, task] of tasks.entries()) {
    const location = "program.tasks[" + index + "]";
    if (!isObject(task)) {
      push(errors, location + " must be an object.");
      continue;
    }
    if (seen.has(task.id)) push(errors, location + ".id duplicates " + task.id + ".");
    seen.add(task.id);
    if (!Array.isArray(task.dependencies)) {
      push(errors, location + ".dependencies must be an array.");
    } else {
      for (const dependency of task.dependencies) {
        if (!ids.has(dependency)) {
          push(errors, location + ".dependencies contains an unknown task.");
        }
        if (dependency === task.id) {
          push(errors, location + ".dependencies must not self-reference.");
        }
        if (expected.indexOf(dependency) >= index) {
          push(errors, location + ".dependencies must point to an earlier task.");
        }
      }
    }
    if (!Array.isArray(task.outputs) || task.outputs.length === 0) {
      push(errors, location + ".outputs must not be empty.");
    }
    if (!Array.isArray(task.adversarialCases) || task.adversarialCases.length === 0) {
      push(errors, location + ".adversarialCases must not be empty.");
    } else {
      for (const caseId of task.adversarialCases) {
        if (!corpusIds.has(caseId)) {
          push(errors, location + ".adversarialCases references an unknown case " + caseId + ".");
        }
      }
    }
    requiredString(task.stopCondition, location + ".stopCondition", errors);
    if (task.id === "protocol" &&
        (task.status !== "completed" || task.implementation !== "complete")) {
      push(errors, "protocol task must be completed.");
    }
    if (task.id === "seed-compiler-lifecycle" &&
        (task.status !== "ready" || task.implementation !== "partial")) {
      push(errors, "seed compiler lifecycle must be ready with partial implementation.");
    }
    if (task.id === "seed-compiler-lifecycle") {
      if (!task.outputs?.includes("source-backed paired compiler comparator")) {
        push(errors, "seed compiler lifecycle outputs must include the source-backed paired compiler comparator.");
      }
      for (const caseId of comparisonCaseIds) {
        if (!task.adversarialCases?.includes(caseId)) {
          push(errors, "seed compiler lifecycle must cover comparison case " + caseId + ".");
        }
      }
      if (!task.stopCondition?.includes("comparison-only") ||
          !task.stopCondition?.includes("regression remains blocked") ||
          !task.stopCondition?.includes(REGRESSION_BLOCKER)) {
        push(errors, "seed compiler lifecycle stopCondition must separate comparison-only current results from managed regression.");
      }
    }
    if (task.id === "language-catalog") {
      if (task.status !== "ready" || task.implementation !== "partial") {
        push(errors, "language-catalog must be ready with partial implementation: catalog validation is ready while reserved units remain blocked.");
      }
      if (!task.outputs?.includes("versioned 21-unit language catalog") ||
          !task.outputs?.includes("source-backed byte-scan-view unit") ||
          !task.outputs?.includes("correctness smoke")) {
        push(errors, "language-catalog outputs must include the catalog, byte-scan source package and correctness smoke.");
      }
      for (const caseId of languageCaseIds) {
        if (!task.adversarialCases?.includes(caseId)) {
          push(errors, "language-catalog must cover BMD3 case " + caseId + ".");
        }
      }
      if (!task.stopCondition?.includes("catalog status ready means catalog validated") ||
          !task.stopCondition?.includes("language-benchmark-runner") ||
          !task.stopCondition?.includes("no W timing")) {
        push(errors, "language-catalog stopCondition must separate catalog readiness from W execution and timing.");
      }
    }
    if (["core-language-units", "computer-language-benchmarks-game", "restaurant-composition"].includes(task.id) &&
        (task.status !== "blocked" || task.implementation !== "blocked")) {
      push(errors, task.id + " must be blocked.");
    }
    if (task.id === "core-language-units" || task.id === "computer-language-benchmarks-game") {
      if (!Array.isArray(task.blockedBy) || !task.blockedBy.includes("codegen")) {
        push(errors, task.id + " must be blocked by codegen.");
      }
    }
    if (task.id === "restaurant-composition") {
      if (!Array.isArray(task.blockedBy) || !task.blockedBy.includes("runtime/provider")) {
        push(errors, "restaurant-composition must be blocked by runtime/provider.");
      }
    }
  }
}

export function validateScenario(input) {
  const errors = [];
  if (!isObject(input)) {
    push(errors, "scenario must be an object.");
    return errors;
  }
  if (input.track === "language") {
    checkLanguageScenario(input, "scenario", errors);
  } else if (input.track === "compiler-lifecycle") {
    checkCompilerLifecycle(input, "scenario", errors);
  } else if (input.track === "documentation") {
    if (input.benchmarkDisposition !== "not-applicable") {
      push(errors, "documentation benchmarkDisposition must be not-applicable.");
    }
    requiredString(input.reason, "scenario.reason", errors);
    if (input.digestOnly !== true) {
      push(errors, "documentation digestOnly must be true.");
    }
    if (hasOwn(input, "profiles")) {
      push(errors, "documentation must not define language source profiles.");
    }
  } else {
    push(errors, "scenario.track must be language, compiler-lifecycle or documentation.");
  }
  if (!LANE_SET.has(input.lane)) {
    push(errors, "scenario.lane must be equivalent or open.");
  }
  containsForbiddenInputKey(input, "scenario", errors);
  return errors;
}

export function validateProgram(program, corpus = undefined) {
  const errors = [];
  if (!isObject(program)) return ["program must be an object."];
  if (program.schema !== SCHEMA_VERSION) push(errors, "program.schema must be wbench/1.");
  if (program.kind !== "program") push(errors, "program.kind must be program.");
  if (program.id !== "bmd1") push(errors, "program.id must be bmd1.");
  if (program.status !== "ready") {
    push(errors, "program.status must be ready after M2 runner implementation.");
  }
  if (program.backend?.benchmarkRunnerAvailable !== true ||
      program.backend?.compilerLifecycleResultsAllowed !== true ||
      program.backend?.comparisonResultsAllowed !== true ||
      program.backend?.regressionResultsAllowed !== false ||
      program.backend?.languageResultsAllowed !== false ||
      program.backend?.productRuntimeResultsAllowed !== false) {
    push(errors, "program backend must enable compiler-lifecycle comparison results and disable regression, language and runtime results.");
  }
  if (hasOwn(program.backend, "resultsAllowed")) {
    push(errors, "program.backend.resultsAllowed is obsolete; use precise result-track flags.");
  }
  if (!requiredString(program.backend?.note, "program.backend.note", errors)) {}
  const profiles = checkProgramProfiles(program.profiles, "program.profiles", errors);
  if (profiles.get("idiomatic")?.primary !== true ||
      profiles.get("idiomatic")?.regression !== true) {
    push(errors, "idiomatic must be primary and regression.");
  }
  if (profiles.get("learner")?.primary !== false ||
      profiles.get("learner")?.regression !== false ||
      profiles.get("frontier")?.primary !== false ||
      profiles.get("frontier")?.regression !== false) {
    push(errors, "learner and frontier must not be primary or regression profiles.");
  }
  if (hasOwn(program, "results") || hasOwn(program, "timings")) {
    push(errors, "program must not contain runtime results or timings.");
  }
  if (!Array.isArray(program.lanes) ||
      !sameArray(program.lanes.map((lane) => lane?.id), [...LANES])) {
    push(errors, "program.lanes must define equivalent and open in order.");
  } else {
    const equivalent = program.lanes[0];
    const open = program.lanes[1];
    for (const field of [
      "same algorithm",
      "same representation",
      "same validation",
      "same numeric contract",
      "same input",
    ]) {
      if (!equivalent.requirements?.includes(field)) {
        push(errors, "equivalent lane must require " + field + ".");
      }
    }
    if (!open.requirements?.includes("record algorithm and representation changes")) {
      push(errors, "open lane must record algorithm and representation changes.");
    }
  }
  if (!sameArray(program.baselinePolicy?.defaultIndependent, ["c-clang", "rust"])) {
    push(errors, "program baseline default must be C/Clang and Rust.");
  }
  if (program.baselinePolicy?.gameRole !== "exploratory-never-authority") {
    push(errors, "Benchmarks Game must remain exploratory and never authority.");
  }
  if (!checkExactKeys(program.languageCatalog, "program.languageCatalog", [
    "path", "id", "version", "unitCount", "firstUnit",
  ], errors)) {
    // The closed-shape error is sufficient; field checks below retain useful diagnostics.
  }
  if (program.languageCatalog?.path !== "benchmarks/language-catalog.json" ||
      program.languageCatalog?.id !== LANGUAGE_CATALOG_ID ||
      program.languageCatalog?.version !== LANGUAGE_CATALOG_VERSION ||
      program.languageCatalog?.unitCount !== LANGUAGE_UNIT_IDS.length ||
      program.languageCatalog?.firstUnit !== LANGUAGE_UNIT_IDS[3]) {
    push(errors, "program.languageCatalog must identify the versioned 21-unit catalog and byte-scan-view as its first borrow/memory unit.");
  }
  const corpusIds = new Set(corpus?.cases?.map((item) => item?.id) ?? []);
  checkTaskGraph(program.tasks, corpusIds, errors);
  return errors;
}

export function validateManifest(manifest) {
  const errors = [];
  if (!isObject(manifest)) return ["manifest must be an object."];
  if (manifest.schema !== SCHEMA_VERSION) push(errors, "manifest.schema must be wbench/1.");
  if (manifest.kind !== "workload-manifest") push(errors, "manifest.kind must be workload-manifest.");
  if (manifest.id !== "bmd1-seed-check-lifecycle") {
    push(errors, "manifest.id must be bmd1-seed-check-lifecycle.");
  }
  if (manifest.status !== "ready") push(errors, "manifest.status must be ready.");
  if (manifest.track !== "compiler-lifecycle") {
    push(errors, "manifest.track must be compiler-lifecycle.");
  }
  if (!LANE_SET.has(manifest.lane)) push(errors, "manifest.lane must be equivalent or open.");
  if (manifest.benchmarkDisposition !== "compiler-lifecycle") {
    push(errors, "manifest.benchmarkDisposition must be compiler-lifecycle.");
  }
  if (manifest.backend?.benchmarkRunnerAvailable !== true ||
      manifest.backend?.frontendAvailable !== true ||
      manifest.backend?.nativeBackendAvailable !== false ||
      manifest.backend?.runtimeAvailable !== false ||
      manifest.backend?.compilerLifecycleResultsAllowed !== true ||
      manifest.backend?.comparisonResultsAllowed !== true ||
      manifest.backend?.regressionResultsAllowed !== false ||
      manifest.backend?.languageResultsAllowed !== false ||
      manifest.backend?.productRuntimeResultsAllowed !== false) {
    push(errors, "manifest backend must expose the seed frontend, comparison results and no regression, language or runtime results.");
  }
  if (hasOwn(manifest.backend, "resultsAllowed")) {
    push(errors, "manifest.backend.resultsAllowed is obsolete; use precise result-track flags.");
  }
  checkSource(manifest.identity?.source, "manifest.identity.source", errors);
  const graph = manifest.identity?.graph;
  const input = manifest.identity?.input;
  checkDescriptorFile(graph, "manifest.identity.graph", "source-graph", errors);
  checkDescriptorFile(input, "manifest.identity.input", "invocation-edit-input", errors);
  if (manifest.command?.tool !== "w" || manifest.command?.operation !== "check") {
    push(errors, "manifest.command must invoke w check.");
  }
  if (!Array.isArray(manifest.command?.arguments) ||
      manifest.command.arguments.length === 0) {
    push(errors, "manifest.command.arguments must not be empty.");
  } else if (manifest.command.arguments[0] !== SOURCE_FIXTURE.path) {
    push(errors, "manifest.command must check the source-backed fixture.");
  }
  if (manifest.languageProfiles?.applicability !== "not-applicable" ||
      !requiredString(manifest.languageProfiles?.reason,
        "manifest.languageProfiles.reason", errors)) {
    push(errors, "manifest.languageProfiles must mark compiler lifecycle as not-applicable with a reason.");
  }
  if (manifest.baselinePolicy?.primary !== "historical-w" ||
      manifest.baselinePolicy?.role !== "contextual-not-ranking") {
    push(errors, "manifest.baselinePolicy must use historical W as primary and C/Clang plus Rust as contextual-not-ranking.");
  }
  if (manifest.baselinePolicy?.recipe !== "equivalent") {
    push(errors, "manifest.baselinePolicy.recipe must be equivalent.");
  }
  if (!sameArray(manifest.baselinePolicy?.independent, ["c-clang", "rust"])) {
    push(errors, "manifest.baselinePolicy.independent must list C/Clang and Rust.");
  }
  if (manifest.baselinePolicy?.exceptionReason !== null &&
      !requiredString(manifest.baselinePolicy?.exceptionReason,
        "manifest.baselinePolicy.exceptionReason", errors)) {}
  const identities = {
    source: manifest.identity?.source && {
      id: SOURCE_ID,
      digest: manifest.identity.source.digest,
    },
    graph,
    input,
  };
  checkMatrix(manifest.matrix, "manifest.matrix", identities, errors);
  for (const key of ["lifecycle", "phases", "phaseIdentities", "phaseMix"]) {
    if (hasOwn(manifest, key)) {
      push(errors, "manifest." + key + " must not flatten compiler lifecycle axes.");
    }
  }
  checkOracle(manifest.oracle, "manifest.oracle", errors, "host-structural");
  const policy = manifest.measurementPolicy;
  if (!isObject(policy)) {
    push(errors, "manifest.measurementPolicy must be an object.");
  } else {
    requiredBoolean(policy.correctnessFirst,
      "manifest.measurementPolicy.correctnessFirst", errors);
    if (policy.rawSamples !== "required-after-oracle") {
      push(errors, "manifest.measurementPolicy.rawSamples must require the oracle first.");
    }
    if (policy.warmup !== "record-before-samples") {
      push(errors, "manifest.measurementPolicy.warmup must be recorded before samples.");
    }
    if (policy.stopRule !== "fixed-count") {
      push(errors, "manifest.measurementPolicy.stopRule must be fixed-count.");
    }
    if (policy.clock !== RESULT_CLOCK) {
      push(errors, "manifest.measurementPolicy.clock must be monotonic-wall-ns.");
    }
    if (policy.order !== MEASUREMENT_ORDER) {
      push(errors, "manifest.measurementPolicy.order must be single-series.");
    }
    if (policy.comparisonOrder !== COMPARISON_ORDER) {
      push(errors, "manifest.measurementPolicy.comparisonOrder must be randomized-interleaved.");
    }
    if (!Array.isArray(policy.environmentFields) ||
        !["hardware", "kernel", "toolchain", "flags", "target", "provider",
          "noise-controls-known", "noise-controls-unknown"]
          .every((field) => policy.environmentFields.includes(field))) {
      push(errors, "manifest.measurementPolicy.environmentFields must include environment and noise controls.");
    }
    if (!Array.isArray(policy.metrics) || !policy.metrics.includes("latency-ns")) {
      push(errors, "manifest.measurementPolicy.metrics must include latency-ns.");
    }
    if (policy.semanticDeviations !== "record-all") {
      push(errors, "manifest.measurementPolicy.semanticDeviations must record all deviations.");
    }
    if (policy.safetyDisclosures !== "record-all") {
      push(errors, "manifest.measurementPolicy.safetyDisclosures must record all disclosures.");
    }
  }
  const comparisonPolicy = manifest.comparisonPolicy;
  if (!isObject(comparisonPolicy) ||
      !sameArray(Object.keys(comparisonPolicy).sort(), ["algorithm", "nanoseconds", "order", "pairsMinimum", "regressionBlocker", "warmup"].sort())) {
    push(errors, "manifest.comparisonPolicy must define the closed paired comparison protocol.");
  } else {
    if (comparisonPolicy.order !== PAIRED_COMPARISON_ORDER || comparisonPolicy.algorithm !== PAIRED_COMPARISON_ORDER) {
      push(errors, "manifest.comparisonPolicy must use the versioned balanced paired order.");
    }
    if (comparisonPolicy.pairsMinimum !== RESULT_MIN_SAMPLES) push(errors, "manifest.comparisonPolicy.pairsMinimum must be 9.");
    if (comparisonPolicy.warmup !== "at-least-one-pair") push(errors, "manifest.comparisonPolicy.warmup must require one pair.");
    if (comparisonPolicy.nanoseconds !== "positive-u64") push(errors, "manifest.comparisonPolicy.nanoseconds must require positive u64.");
    if (comparisonPolicy.regressionBlocker !== REGRESSION_BLOCKER) push(errors, "manifest.comparisonPolicy.regressionBlocker must be managed-regression-runner.");
  }
  if (!Array.isArray(manifest.outputs) || manifest.outputs.length === 0) {
    push(errors, "manifest.outputs must not be empty.");
  }
  if (hasOwn(manifest, "results") || hasOwn(manifest, "timings")) {
    push(errors, "manifest must not contain runtime results or timings.");
  }
  return errors;
}

const CANONICAL_FIXTURES = new Set([
  "language-equivalent",
  "language-catalog",
  "byte-scan-view",
  "documentation",
  "compiler-lifecycle",
  "result",
  "comparison-result",
]);
const BOUNDED_MUTATIONS = new Set([
  "open-lane",
  "deferred",
  "profile-missing",
  "profile-duplicate",
  "idiomatic-not-primary",
  "oracle-partial",
  "oracle-false",
  "equivalent-semantic-difference",
  "learner-artificially-slow",
  "frontier-missing-disclosure",
  "baseline-provenance-missing",
  "compile-execution-mixed",
  "best-only",
  "output-constant-bypass",
  "claim-without-backend",
  "specialization-universal",
  "invalid-benchmark-disposition",
  "deferred-missing-blocker",
  "not-applicable-missing-reason",
  "flattened-axis",
  "no-op-without-cache",
  "edit-without-cache",
  "internal-stage-without-instrumentation",
  "runtime-mixed",
  "result-blocked-point",
  "result-language-without-backend",
  "result-regression-without-comparison",
  "result-partial-oracle",
  "result-tracked-timing",
  "result-output-overwrite",
  "result-forged-metric",
  "result-even-samples",
  "result-leading-zero",
  "result-u64-overflow",
  "result-comparison-incomplete",
  "blocker-incomplete",
  "comparison-different-closure",
  "comparison-schedule-forged",
  "comparison-sample-label-forged",
  "comparison-oracle-partial",
  "comparison-recipe-class-divergent",
  "comparison-toolchain-divergent",
  "comparison-workload-divergent",
  "comparison-metric-forged",
  "comparison-delta-forged",
  "comparison-calibration-forged",
  "comparison-counts-forged",
  "comparison-even-pairs",
  "comparison-zero-ns",
  "comparison-leading-zero",
  "comparison-u64-overflow",
  "comparison-regression-blocked",
  "catalog-not-21",
  "catalog-duplicate-id",
  "catalog-wrong-stratum",
  "byte-profile-missing",
  "byte-learner-artificial",
  "byte-lane-fraud",
  "byte-frontier-disclosure-missing",
  "byte-constant-output",
  "byte-output-precomputed",
  "byte-input-incomplete",
  "byte-boundary-incomplete",
  "byte-oracle-incomplete",
  "byte-claim-timing",
  "byte-claim-backend",
  "byte-baseline-partial",
  "byte-baseline-forged",
  "byte-temp-source-provenance",
]);

function cloneValue(value) {
  return JSON.parse(JSON.stringify(value));
}

function languageProfilesFixture() {
  return [
    { id: "learner", correct: true, plausible: true, sleep: false, uselessWork: false, worseFlags: false, bypass: false },
    { id: "idiomatic", correct: true, plausible: true, sleep: false, uselessWork: false, worseFlags: false, bypass: false },
    { id: "frontier", correct: true, plausible: true, disclosures: {
      unsafe: "none", ffi: "none", targetSpecialization: "none", manualLayout: "none", algorithm: "same", legibility: "none",
    } },
  ];
}

function languageFixture() {
  return {
    benchmarkDisposition: "required",
    profiles: languageProfilesFixture(),
    primaryProfile: "idiomatic",
    regressionProfile: "idiomatic",
    sameAlgorithm: true,
    sameRepresentation: true,
    sameValidation: true,
    sameNumericContract: true,
    sameInput: true,
    oracle: { correctnessRecord: "host-oracle", complete: true, beforeSamples: true },
    backendAvailable: false,
    claims: [],
    baseline: { independent: ["c-clang", "rust"], provenanceComplete: true, exceptionReason: null },
    samples: { mode: "not-started", order: COMPARISON_ORDER },
  };
}

function languageCatalogFixture() {
  return cloneValue(loadByteScanDocuments().catalog);
}

function byteScanViewFixture() {
  return cloneValue(loadByteScanDocuments().manifest);
}

function compilerFixture() {
  return {
    benchmarkDisposition: "compiler-lifecycle",
    languageProfiles: {
      applicability: "not-applicable",
      reason: "Compiler lifecycle uses one source, graph and input identity instead of language source profiles.",
    },
    source: { id: SOURCE_ID, digest: SOURCE_FIXTURE.digest },
    graph: { id: DESCRIPTOR_IDENTITIES.graph.id, digest: DESCRIPTOR_IDENTITIES.graph.digest },
    inputIdentity: { id: DESCRIPTOR_IDENTITIES.input.id, digest: DESCRIPTOR_IDENTITIES.input.digest },
    matrix: buildMatrix(),
    oracle: { correctnessRecord: "host-oracle", complete: true, beforeSamples: true },
    backendAvailable: false,
    claims: [],
    baseline: {
      primary: "historical-w",
      role: "contextual-not-ranking",
      independent: ["c-clang", "rust"],
      provenanceComplete: true,
      exceptionReason: null,
      recipe: "equivalent",
    },
    samples: { mode: "not-started", order: MEASUREMENT_ORDER },
  };
}

function resultFixture() {
  const values = ["100", "101", "102", "103", "104", "105", "106", "107", "108"];
  return {
    $schema: "./wbench-1.schema.json",
    schema: SCHEMA_VERSION,
    kind: "result",
    id: "bmd1-seed-check-exploratory",
    status: "recorded",
    quality: "exploratory",
    claim: "measurement-only",
    workload: {
      manifestDigest: fileDigest("benchmarks/seed-check-lifecycle.manifest.json") || "sha256:" + "0".repeat(64),
      track: "compiler-lifecycle",
      lane: "equivalent",
      scenario: "clean",
      stage: "check-end-to-end",
      subject: "compiler/seed-c",
      profile: null,
    },
    identity: {
      source: {
        path: SOURCE_FIXTURE.path,
        symbol: SOURCE_FIXTURE.symbol,
        digest: SOURCE_FIXTURE.digest,
      },
      graph: { ...DESCRIPTOR_IDENTITIES.graph },
      input: { ...DESCRIPTOR_IDENTITIES.input },
      command: {
        tool: "w",
        operation: "check",
        arguments: [SOURCE_FIXTURE.path, "--json"],
      },
    },
    comparison: null,
    oracle: {
      validationDigest: SOURCE_FIXTURE.digest,
      complete: true,
      beforeSamples: true,
    },
    samples: {
      raw: values.map((ns) => ({ ns })),
      warmup: [{ ns: "99" }],
      stopRule: { kind: "fixed-count", count: values.length },
      clock: RESULT_CLOCK,
      order: MEASUREMENT_ORDER,
    },
    environment: {
      hardware: "test-host",
      kernel: "test-kernel",
      target: "host",
      provider: "host",
      toolchain: "seed-c-release",
      flags: ["Release"],
      noiseControls: { known: [], unknown: ["scheduler"] },
    },
    provenance: {
      sourceDigest: SOURCE_FIXTURE.digest,
      artifactDigest: SOURCE_FIXTURE.digest,
      inputDigest: DESCRIPTOR_IDENTITIES.input.digest,
      recipeDigest: SOURCE_FIXTURE.digest,
      runnerDigest: SOURCE_FIXTURE.digest,
      toolchainDigest: SOURCE_FIXTURE.digest,
    },
    metrics: {
      latency: {
        unit: "ns",
        minimumNs: "100",
        medianNs: "104",
        maximumNs: "108",
        madNs: "2",
        derivedFromRawSamples: true,
      },
    },
    summary: { sampleCount: values.length, warmupCount: 1, derivedFromRawSamples: true },
    semanticDeviations: [],
    disclosures: [],
  };
}

function comparisonRoleFixture(commit = "1".repeat(40), closureDigest = SOURCE_FIXTURE.digest) {
  return {
    commit,
    closureDigest,
    artifactDigest: "sha256:" + "2".repeat(64),
    recipeDigest: "sha256:" + "3".repeat(64),
    recipeClassDigest: "sha256:" + "4".repeat(64),
    toolchainDigest: "sha256:" + "5".repeat(64),
  };
}

function comparisonResultFixture() {
  const pairCount = RESULT_MIN_SAMPLES;
  const seed = "0123456789abcdef".repeat(4);
  const schedule = pairedScheduleForSeed(seed, pairCount);
  const raw = [];
  for (const round of schedule) {
    const baselineNs = String(100 + round.round);
    const candidateNs = String(101 + round.round);
    raw.push({ round: round.round, series: round.first, position: "first", ns: round.first === "baseline" ? baselineNs : candidateNs });
    raw.push({ round: round.round, series: round.second, position: "second", ns: round.second === "baseline" ? baselineNs : candidateNs });
  }
  const computed = calculatePairedComparison(raw, pairCount);
  const role = comparisonRoleFixture();
  const workload = {
    manifestDigest: fileDigest("benchmarks/seed-check-lifecycle.manifest.json") || "sha256:" + "0".repeat(64),
    track: "compiler-lifecycle",
    lane: "equivalent",
    scenario: "clean",
    stage: "check-end-to-end",
    subject: "compiler/seed-c",
    profile: null,
  };
  const identity = {
    source: { path: SOURCE_FIXTURE.path, symbol: SOURCE_FIXTURE.symbol, digest: SOURCE_FIXTURE.digest },
    graph: { ...DESCRIPTOR_IDENTITIES.graph },
    input: { ...DESCRIPTOR_IDENTITIES.input },
    command: { tool: "w", operation: "check", arguments: [SOURCE_FIXTURE.path, "--json"] },
  };
  const value = {
    $schema: "./wbench-1.schema.json",
    schema: SCHEMA_VERSION,
    kind: "result",
    id: "bmd2-seed-check-comparison",
    status: "recorded",
    quality: "exploratory",
    claim: COMPARISON_CLAIM,
    verdict: "not-evaluated",
    workload,
    identity,
    comparison: {
      baseline: { ...role },
      candidate: { ...role },
      calibration: true,
      verdict: "not-evaluated",
      pairs: computed.pairs,
      delta: computed.delta,
      relativePpm: computed.relativePpm,
      counts: computed.counts,
      noisePolicy: { known: [], unknown: ["scheduler"] },
    },
    oracle: {
      baseline: { validationDigest: "sha256:" + "6".repeat(64), complete: true, beforeSamples: true },
      candidate: { validationDigest: "sha256:" + "7".repeat(64), complete: true, beforeSamples: true },
      complete: true,
      beforeSamples: true,
    },
    samples: {
      raw,
      warmup: [
        { round: 1, series: schedule[0].first, position: "first", ns: schedule[0].first === "baseline" ? "99" : "100" },
        { round: 1, series: schedule[0].second, position: "second", ns: schedule[0].second === "baseline" ? "99" : "100" },
      ],
      stopRule: { kind: "fixed-pairs", pairs: pairCount },
      clock: RESULT_CLOCK,
      order: PAIRED_COMPARISON_ORDER,
      schedule: { algorithm: PAIRED_COMPARISON_ORDER, seed, rounds: schedule },
    },
    environment: {
      hardware: "test-host",
      kernel: "test-kernel",
      target: "host",
      provider: "host",
      toolchain: "seed-c-release",
      flags: ["Release"],
      noiseControls: { known: [], unknown: ["scheduler"] },
    },
    provenance: {
      sourceDigest: SOURCE_FIXTURE.digest,
      graphDigest: DESCRIPTOR_IDENTITIES.graph.digest,
      inputDigest: DESCRIPTOR_IDENTITIES.input.digest,
      workloadDigest: "pending",
      runnerDigest: "sha256:" + "8".repeat(64),
      baseline: { ...role },
      candidate: { ...role },
    },
    metrics: { baseline: computed.baseline, candidate: computed.candidate },
    summary: { pairCount, warmupPairCount: 1, derivedFromRawSamples: true },
    semanticDeviations: [],
    disclosures: [
      "Each warmup and raw sample uses a new driver process.",
      "Noise controls remain unknown unless the environment records them as controlled.",
    ],
  };
  value.provenance.workloadDigest = comparisonWorkloadDigest(value);
  return value;
}

function buildMatrix() {
  const points = [];
  for (const scenario of LIFECYCLE_SCENARIOS) {
    for (const stage of LIFECYCLE_STAGES) {
      points.push({
        scenario,
        stage,
        status: expectedMatrixStatus(scenario, stage),
        blockedBy: expectedMatrixBlockers(scenario, stage),
      });
    }
  }
  return { scenarios: [...LIFECYCLE_SCENARIOS], stages: [...LIFECYCLE_STAGES], points };
}

function applyBoundedMutation(value, mutation, errors) {
  if (!BOUNDED_MUTATIONS.has(mutation)) {
    push(errors, "unknown or unbounded corpus mutation " + String(mutation) + ".");
    return;
  }
  const point = (scenario, stage) => value.matrix?.points?.find((candidate) =>
    candidate.scenario === scenario && candidate.stage === stage);
  switch (mutation) {
    case "open-lane":
      value.sameAlgorithm = false;
      value.sameRepresentation = false;
      value.profiles[2].disclosures.algorithm = "quicksort versus stable sort is recorded";
      break;
    case "deferred":
      value.benchmarkDisposition = "deferred";
      value.blocker = "codegen";
      value.taskId = "core-language-units";
      value.stopCondition = "Do not collect timing until codegen produces an equivalent artifact and the correctness oracle passes.";
      break;
    case "profile-missing": value.profiles = value.profiles.slice(0, 2); break;
    case "profile-duplicate": value.profiles[2].id = "idiomatic"; break;
    case "idiomatic-not-primary": value.primaryProfile = "learner"; value.regressionProfile = "learner"; break;
    case "oracle-partial": value.oracle.complete = false; value.oracle.beforeSamples = false; break;
    case "oracle-false": value.oracle.complete = false; break;
    case "equivalent-semantic-difference": value.sameValidation = false; break;
    case "learner-artificially-slow": value.profiles[0].sleep = true; break;
    case "frontier-missing-disclosure": delete value.profiles[2].disclosures.ffi; break;
    case "baseline-provenance-missing": value.baseline.independent = []; value.baseline.provenanceComplete = false; break;
    case "compile-execution-mixed": value.phaseMix = "compiler+runtime"; break;
    case "best-only": value.samples = { mode: "started", order: "best-only" }; break;
    case "output-constant-bypass": value.profiles[0].bypass = true; value.sameValidation = false; break;
    case "claim-without-backend": value.claims = ["W has the fastest execution"]; break;
    case "specialization-universal": value.profiles[2].disclosures.targetSpecialization = "universal"; break;
    case "invalid-benchmark-disposition": value.benchmarkDisposition = "maybe"; value.profiles = []; break;
    case "deferred-missing-blocker": value.benchmarkDisposition = "deferred"; delete value.blocker; value.taskId = "core-language-units"; value.stopCondition = "Wait for codegen and correctness before timing."; break;
    case "not-applicable-missing-reason": delete value.reason; break;
    case "flattened-axis": value.phases = [...LIFECYCLE_STAGES]; break;
    case "no-op-without-cache": { const target = point("no-op", "check-end-to-end"); if (target) target.blockedBy = []; break; }
    case "edit-without-cache": { const target = point("edit", "check-end-to-end"); if (target) target.blockedBy = []; break; }
    case "internal-stage-without-instrumentation": { const target = point("clean", "source"); if (target) target.blockedBy = []; break; }
    case "runtime-mixed": value.phaseMix = "check-end-to-end+execution"; break;
    case "result-blocked-point": value.workload.stage = "source"; break;
    case "result-language-without-backend": value.workload.track = "language"; value.workload.profile = "idiomatic"; break;
    case "result-regression-without-comparison": value.quality = "regression-grade"; value.claim = "regression"; break;
    case "result-partial-oracle": value.oracle.complete = false; value.oracle.beforeSamples = false; break;
    case "result-tracked-timing": value.timings = [{ ns: "100" }]; break;
    case "result-output-overwrite": value.overwrite = true; break;
    case "result-forged-metric": value.metrics.latency.medianNs = "999"; break;
    case "result-even-samples": value.samples.raw = value.samples.raw.slice(0, 8); value.samples.stopRule.count = 8; break;
    case "result-leading-zero": value.samples.raw[0].ns = "0100"; break;
    case "result-u64-overflow": value.samples.raw[0].ns = "18446744073709551616"; break;
    case "result-comparison-incomplete":
      value.quality = "regression-grade";
      value.claim = "regression";
      value.comparison = { baseline: { subject: value.workload.subject }, candidate: {}, noisePolicy: { complete: true, controlled: ["scheduler"], unknown: [] } };
      value.samples.order = COMPARISON_ORDER;
      break;
    case "blocker-incomplete": { const target = point("no-op", "semantic"); if (target) target.blockedBy = ["incremental-cache"]; break; }
    case "comparison-different-closure":
      value.comparison.candidate.closureDigest = "sha256:" + "9".repeat(64);
      value.provenance.candidate.closureDigest = value.comparison.candidate.closureDigest;
      value.comparison.calibration = false;
      break;
    case "comparison-schedule-forged":
      value.samples.schedule.rounds = [...value.samples.schedule.rounds].reverse();
      break;
    case "comparison-sample-label-forged":
      value.samples.raw[0].series = value.samples.raw[0].series === "baseline" ? "candidate" : "baseline";
      break;
    case "comparison-oracle-partial":
      value.oracle.candidate.complete = false;
      value.oracle.complete = false;
      break;
    case "comparison-recipe-class-divergent":
      value.comparison.candidate.recipeClassDigest = "sha256:" + "a".repeat(64);
      value.provenance.candidate.recipeClassDigest = value.comparison.candidate.recipeClassDigest;
      break;
    case "comparison-toolchain-divergent":
      value.comparison.candidate.toolchainDigest = "sha256:" + "b".repeat(64);
      value.provenance.candidate.toolchainDigest = value.comparison.candidate.toolchainDigest;
      break;
    case "comparison-workload-divergent":
      value.provenance.workloadDigest = "sha256:" + "c".repeat(64);
      break;
    case "comparison-metric-forged":
      value.metrics.baseline.medianNs = "999";
      break;
    case "comparison-delta-forged":
      value.comparison.delta.medianNs = "999";
      break;
    case "comparison-calibration-forged":
      value.comparison.calibration = false;
      break;
    case "comparison-counts-forged":
      value.comparison.counts.tied += 1;
      break;
    case "comparison-even-pairs":
      value.samples.stopRule.pairs = 10;
      break;
    case "comparison-zero-ns":
      value.samples.raw[0].ns = "0";
      break;
    case "comparison-leading-zero":
      value.samples.raw[0].ns = "0100";
      break;
    case "comparison-u64-overflow":
      value.samples.raw[0].ns = "18446744073709551616";
      break;
    case "comparison-regression-blocked":
      value.quality = "regression-grade";
      value.claim = "regression";
      break;
    case "catalog-not-21":
      value.units = value.units.slice(0, 20);
      break;
    case "catalog-duplicate-id":
      value.units[1].id = value.units[0].id;
      break;
    case "catalog-wrong-stratum":
      value.units.find((unit) => unit.id === "byte-scan-view").stratum = "scalar/control";
      break;
    case "byte-profile-missing":
      value.profiles = value.profiles.slice(0, 2);
      break;
    case "byte-learner-artificial":
      value.profiles[0].shape = "sleep before scanning to appear slower";
      break;
    case "byte-lane-fraud":
      value.profiles[0].sameInput = false;
      break;
    case "byte-frontier-disclosure-missing":
      delete value.profiles[2].disclosures.algorithm;
      break;
    case "byte-constant-output":
      value.profiles[0].shape = "return constant output without reading source";
      break;
    case "byte-output-precomputed":
      value.profiles[1].shape = "precomputed output bypass";
      break;
    case "byte-input-incomplete":
      value.inputs.classes = value.inputs.classes.slice(0, 7);
      break;
    case "byte-boundary-incomplete":
      value.inputs.classes.find((item) => item.id === "boundary").sizes = [0, 1, 15, 16, 17, 64, 65];
      break;
    case "byte-oracle-incomplete":
      value.oracle.complete = false;
      break;
    case "byte-claim-timing":
      value.backend.timingResultsAllowed = true;
      break;
    case "byte-claim-backend":
      value.backend.nativeBackendAvailable = true;
      break;
    case "byte-baseline-partial":
      value.baselines = value.baselines.slice(0, 1);
      break;
    case "byte-baseline-forged":
      value.baselines[0].output = "forged-output";
      break;
    case "byte-temp-source-provenance":
      value.profiles[0].path = "C:\\temp\\learner.w";
      break;
  }
}

export function materializeCase(item) {
  const errors = [];
  if (!isObject(item)) return { value: undefined, errors: ["case must be an object."] };
  if (!CANONICAL_FIXTURES.has(item.fixture)) {
    errors.push("case.fixture must identify a canonical fixture.");
    return { value: undefined, errors };
  }
  let value;
  if (item.fixture === "language-equivalent") value = languageFixture();
  else if (item.fixture === "language-catalog") value = languageCatalogFixture();
  else if (item.fixture === "byte-scan-view") value = byteScanViewFixture();
  else if (item.fixture === "documentation") value = {
    benchmarkDisposition: "not-applicable",
    reason: "A digest-only documentation change has no behavioral workload.",
    digestOnly: true,
  };
  else if (item.fixture === "compiler-lifecycle") value = compilerFixture();
  else if (item.fixture === "result") value = resultFixture();
  else value = comparisonResultFixture();
  if (!Array.isArray(item.mutations)) {
    errors.push("case.mutations must be a bounded array.");
  } else {
    if (item.mutations.length > MAX_BOUNDED_MUTATIONS) {
      errors.push("case.mutations must contain at most " + MAX_BOUNDED_MUTATIONS + " bounded mutations.");
    }
    for (const mutation of item.mutations) applyBoundedMutation(value, mutation, errors);
  }
  return { value, errors };
}

export function validateCorpus(corpus) {
  const errors = [];
  if (!isObject(corpus)) return ["corpus must be an object."];
  if (corpus.$schema !== "w-benchmark-driven-development-cases-1") {
    push(errors, "corpus schema is invalid.");
  }
  if (corpus.status !== "design-oracle-input") {
    push(errors, "corpus status must be design-oracle-input.");
  }
  if (corpus.id !== "BMD1") push(errors, "corpus id must be BMD1.");
  if (!Array.isArray(corpus.decisions) ||
      !corpus.decisions.includes("W-1487") ||
      !corpus.decisions.includes("W-1488")) {
    push(errors, "corpus decisions must cite W-1487 and W-1488.");
  }
  const hasBmd3Cases = corpus.cases?.some((item) => String(item?.id ?? "").startsWith("BMD3-W-1490-"));
  if (hasBmd3Cases && !corpus.decisions?.includes("W-1490")) {
    push(errors, "corpus decisions must cite W-1490 for language catalog and byte-scan cases.");
  }
  if (corpus.cases?.some((item) => item?.fixture === "comparison-result") &&
      !corpus.decisions?.includes("W-1489")) {
    push(errors, "corpus decisions must cite W-1489 for comparison cases.");
  }
  if (!Array.isArray(corpus.cases)) {
    push(errors, "corpus.cases must be an array.");
    return errors;
  }
  const ids = new Set();
  for (const [index, item] of corpus.cases.entries()) {
    const location = "corpus.cases[" + index + "]";
    if (!isObject(item)) {
      push(errors, location + " must be an object.");
      continue;
    }
    const caseKeys = ["id", "kind", "track", "lane", "decisions", "fixture", "mutations", "contract"];
    if (item.kind === "rejected") caseKeys.push("violation");
    checkExactKeys(item, location, caseKeys, errors);
    for (const forbidden of ["input", "result", "expected", "expectedResult"]) {
      if (hasOwn(item, forbidden)) push(errors, location + "." + forbidden + " must not echo a fixture or result.");
    }
    if (ids.has(item.id)) push(errors, location + ".id duplicates " + item.id + ".");
    ids.add(item.id);
    requiredString(item.id, location + ".id", errors);
    if (!["accepted", "rejected"].includes(item.kind)) {
      push(errors, location + ".kind must be accepted or rejected.");
    }
    if (!LANE_SET.has(item.lane)) push(errors, location + ".lane is invalid.");
    if (!Array.isArray(item.decisions) || !item.decisions.includes("W-1487")) {
      push(errors, location + ".decisions must cite W-1487.");
    }
    if (item.track === "compiler-lifecycle" &&
        (!Array.isArray(item.decisions) || !item.decisions.includes("W-1488"))) {
      push(errors, location + ".decisions must cite W-1488 for compiler lifecycle.");
    }
    if (item.fixture === "comparison-result" &&
        (!Array.isArray(item.decisions) || !item.decisions.includes("W-1489"))) {
      push(errors, location + ".decisions must cite W-1489 for comparison result.");
    }
    if (["language-catalog", "byte-scan-view"].includes(item.fixture)) {
      if (item.track !== "language") push(errors, location + ".track must be language for BMD3 fixtures.");
      if (item.lane !== "equivalent") push(errors, location + ".lane must be equivalent for BMD3 fixture identity.");
      if (!Array.isArray(item.decisions) || !item.decisions.includes("W-1490")) {
        push(errors, location + ".decisions must cite W-1490 for BMD3 fixtures.");
      }
    }
    if (item.kind === "rejected") requiredString(item.violation, location + ".violation", errors);
    if (!Array.isArray(item.contract) || item.contract.length === 0) {
      push(errors, location + ".contract must not be empty.");
    }
    const materialized = materializeCase(item);
    let caseErrors = [...materialized.errors];
    if (materialized.value !== undefined && caseErrors.length === 0) {
      if (item.fixture === "result" || item.fixture === "comparison-result") caseErrors = validateResult(materialized.value);
      else if (item.fixture === "language-catalog") caseErrors = validateLanguageCatalog(materialized.value);
      else if (item.fixture === "byte-scan-view") caseErrors = validateByteScanManifest(materialized.value, loadByteScanDocuments().catalog);
      else caseErrors = validateScenario({
        ...materialized.value,
        track: item.track,
        lane: item.lane,
      });
    }
    if (item.kind === "accepted" && caseErrors.length > 0) {
      push(errors, location + " accepted case is invalid: " + caseErrors.join(" "));
    }
    if (item.kind === "rejected" && caseErrors.length === 0) {
      push(errors, location + " rejected case has no observable violation.");
    }
  }
  for (const caseId of REQUIRED_CASES) {
    if (!ids.has(caseId)) push(errors, "corpus is missing required case " + caseId + ".");
  }
  return errors;
}

export function reduceCase(item) {
  const materialized = materializeCase(item);
  const errors = [...materialized.errors];
  if (materialized.value !== undefined && errors.length === 0) {
    const value = materialized.value;
    errors.push(...((item.fixture === "result" || item.fixture === "comparison-result")
      ? validateResult(value)
      : item.fixture === "language-catalog"
        ? validateLanguageCatalog(value)
        : item.fixture === "byte-scan-view"
          ? validateByteScanManifest(value, loadByteScanDocuments().catalog)
          : validateScenario({ ...value, track: item.track, lane: item.lane })));
  }
  return {
    id: item.id,
    classification: errors.length === 0 ? "accepted" : "rejected",
    errors,
  };
}

export function reduceCorpus(corpus) {
  return (corpus.cases ?? []).map(reduceCase);
}

export function loadBmdDocuments() {
  const read = (relativePath) =>
    JSON.parse(fs.readFileSync(path.resolve(ROOT, relativePath), "utf8"));
  return {
    schema: read("benchmarks/wbench-1.schema.json"),
    program: read("benchmarks/program.json"),
    manifest: read("benchmarks/seed-check-lifecycle.manifest.json"),
    corpus: read("tooling/benchmark-driven-development-cases.json"),
    languageCatalog: read("benchmarks/language-catalog.json"),
    byteScanManifest: read("benchmarks/byte-scan-view.manifest.json"),
  };
}
