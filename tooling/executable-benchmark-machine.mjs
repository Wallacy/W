import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export const ROOT = path.resolve(import.meta.dir, "..");
export const EXECUTABLE_SCHEMA = "w-executable-benchmark/6";
export const EXECUTABLE_CATALOG_ID = "w-executable-benchmark-catalog";
export const EXECUTABLE_RESULT_SCHEMA = "w-executable-benchmark-result/5";
export const EXECUTABLE_BEST_SCHEMA = "w-executable-benchmark-best-metrics/1";
export const EXECUTABLE_LANGUAGES = Object.freeze(["w", "c", "rust"]);
export const EXECUTABLE_STRUCTURE_CLASSES = Object.freeze([
  "public-end-to-end",
  "integration-linkage",
  "transient-internal",
]);
export const EXECUTABLE_WORKLOAD_IDS = Object.freeze([
  "hello",
  "restaurant-branch",
  "restaurant-nested-branch",
  "bool-short-circuit",
  "restaurant-interpolation",
  "restaurant-scalar-if",
  "restaurant-nested-scalar-if",
  "restaurant-while",
  "restaurant-wmo",
  "restaurant-enum-switch",
  "restaurant-enum-payload",
  "restaurant-comparisons",
  "restaurant-comparison-composition",
  "restaurant-linear",
  "restaurant-runtime-divrem",
  "restaurant-unary-negate",
  "restaurant-unary-interpolation",
  "restaurant-mutation",
  "restaurant-conditional-mutation",
  "restaurant-bool-mutation",
  "restaurant-branch-mutation",
  "restaurant-branch-mutation-multi",
  "restaurant-composition",
  "process-entry",
  "process-handler-lifecycle",
]);
export const EXECUTABLE_RUN_TARGETS = Object.freeze(
  EXECUTABLE_WORKLOAD_IDS.filter((id) => id !== "restaurant-composition"),
);
const PUBLIC_WINDOWS_RUN_GATE = "tooling/check-w-run-windows.mjs";
const PUBLIC_WINDOWS_RUN_VARIANTS = Object.freeze({
  "compiler/seed-c/fixtures/hlo0-hello.w": "hello",
});
export const PROCESS_ENTRY_WORKLOAD_ID = "process-entry";
export const PROCESS_ENTRY_ORACLE_KIND = "argument-dependent-output";
export const PROCESS_ENTRY_RECIPE_CLASS = "process-entry-release";
export const PUBLIC_C_RECIPE = "clang-c23-msvc";
export const PRIVATE_C_RECIPE = "gcc-c23-or-c2x";
export const PROCESS_ENTRY_TIMED_INPUT = Object.freeze(["payload"]);
export const PROCESS_ENTRY_CORRECTNESS_INPUTS = Object.freeze([
  Object.freeze([]),
  Object.freeze([""]),
  PROCESS_ENTRY_TIMED_INPUT,
]);
export const PROCESS_ENTRY_ORACLE_CASES = Object.freeze([
  Object.freeze({ arguments: PROCESS_ENTRY_CORRECTNESS_INPUTS[0], exitCode: 2, stdout: "missing\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ENTRY_CORRECTNESS_INPUTS[1], exitCode: 0, stdout: "received\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ENTRY_CORRECTNESS_INPUTS[2], exitCode: 0, stdout: "received\n", stderr: "" }),
]);
export const PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID = "process-handler-lifecycle";
export const PROCESS_HANDLER_LIFECYCLE_STRUCTURE_CLASS = "integration-linkage";
export const PROCESS_HANDLER_LIFECYCLE_EXECUTION_STRUCTURE_CLASS = "transient-internal";
export const PROCESS_ENTRY0_EXECUTION_KIND = "private-process-handler";
export const PROCESS_ENTRY0_RECIPE = "private-process-handler";
export const PROCESS_ENTRY0_RECIPE_CLASS = "process-entry0-private-handler";
export const PROCESS_ENTRY0_TIMED_INPUT = Object.freeze(["alpha", "payload"]);
export const PROCESS_ENTRY0_CORRECTNESS_INPUTS = Object.freeze([
  Object.freeze([]),
  PROCESS_ENTRY0_TIMED_INPUT,
]);
export const PROCESS_ENTRY0_FAULT_CASES = Object.freeze([
  "missing",
  "noop-success",
  "stale-generation",
  "reversed-arguments",
  "wrong-context",
  "wrong-arguments",
]);
export const PROCESS_ENTRY0_SUPPORT_ROLES = Object.freeze([
  "harness-c",
  "provider-c",
  "provider-header",
]);
export const EXECUTABLE_METRICS = Object.freeze([
  { id: "compile-latency", unit: "nanoseconds", kind: "duration" },
  { id: "run-wall-time", unit: "nanoseconds", kind: "duration" },
  { id: "run-wall-p95", unit: "nanoseconds", kind: "duration" },
  { id: "cpu-user-time", unit: "microseconds", kind: "duration" },
  { id: "cpu-system-time", unit: "microseconds", kind: "duration" },
  { id: "cpu-time", unit: "microseconds", kind: "duration" },
  { id: "peak-working-set", unit: "bytes", kind: "size" },
  { id: "artifact-size", unit: "bytes", kind: "size" },
  { id: "exit-code", unit: "integer", kind: "exit" },
  { id: "stdout", unit: "utf8-bytes", kind: "output" },
  { id: "stderr", unit: "utf8-bytes", kind: "output" },
]);
export const EXECUTABLE_COMPARABILITY_AXES = Object.freeze([
  "source-semantics",
  "platform-target",
  "artifact-target",
  "abi",
  "profile",
  "toolchain",
  "host",
  "recipe",
  "provenance",
  "equivalence-key",
]);
export const SAMPLE_FIELDS = Object.freeze([
  "wallNs",
  "cpuUserUs",
  "cpuSystemUs",
  "cpuTotalUs",
  "peakRssBytes",
]);
export const SAMPLE_SUMMARY_FIELDS = Object.freeze([
  "min", "median", "max", "arithmeticMean", "mad",
]);
export const SAMPLE_COUNT_MINIMUM = 9;
export const SAMPLE_COUNT_WARMUP_MINIMUM = 1;
export const MEASUREMENT_PROFILES = Object.freeze(["release", "size-experimental"]);
export const OPTIMIZABLE_METRICS = Object.freeze([
  "compile-latency",
  "run-wall-time",
  "run-wall-p95",
  "cpu-time",
  "peak-working-set",
  "artifact-size",
]);
export const BEST_METRIC_ORDER = Object.freeze([...OPTIMIZABLE_METRICS]);
export const LOCAL_RESULTS_PATH = "benchmarks/results";
export const BEST_METRICS_STATUS = "current";
export const BEST_CATEGORY_AXES = Object.freeze([
  "workloadId",
  "language",
  "equivalenceKey",
  "platformTarget",
  "artifactTarget",
  "abi",
  "profile",
  "host",
  "recipeClass",
  "comparability",
  "eligibility",
]);
export const BEST_METRIC_PROVENANCE_FIELDS = Object.freeze([
  "recordId",
  "commit",
  "observedAt",
  "sourceDigest",
  "artifactDigest",
  "recipeDigest",
  "toolchainDigest",
  "runnerDigest",
  "catalogDigest",
  "artifactCleanliness",
]);
export const EXECUTABLE_PLATFORM_TARGET = "windows-x64";
export const EXECUTABLE_ARTIFACT_TARGET_MSVC = "x86_64-pc-windows-msvc";
export const EXECUTABLE_ARTIFACT_TARGET_MINGW = "x86_64-w64-mingw32";
export const ENVIRONMENT_FIELDS = Object.freeze(["os", "kernel", "cpuModel", "logicalCores", "ramBytes"]);
const ARTIFACT_CLEANLINESS_FIELDS = Object.freeze([
  "coffSymbols",
  "codeView",
  "debugDirectory",
  "certificateDirectory",
  "sectionData",
  "sidecars",
  "overlay",
]);
export const PROTOCOL_FIELDS = Object.freeze([
  "warmupMinimum", "rawMinimum", "rawParity", "arithmeticMeanRounding", "stopRule", "wallClock",
  "processIsolation", "order", "resourceScope", "knownNoiseControls",
  "unknownNoiseControls", "directProcessDisclosure", "measurementKernel",
]);
export const CATALOG_STATUS = "catalog-ready";
export const BEST_METRICS_CONTRACT_STATUS = "defined";

const SOURCE_ELIGIBILITY = Object.freeze({
  wPublicBuild: Object.freeze({
    comparability: "promotable-after-equivalence",
    eligibility: "promotable-after-equivalence",
  }),
  processHandler: Object.freeze({
    comparability: "contextual-non-ranking-private-composite",
    eligibility: "exploratory-private-composite",
  }),
  wDeferred: Object.freeze({
    comparability: "deferred-until-M3b",
    eligibility: "deferred-to-M3b",
  }),
  cPublic: Object.freeze({
    comparability: "promotable-after-equivalence",
    eligibility: "promotable-after-equivalence",
  }),
  cPrivate: Object.freeze({
    comparability: "contextual-non-ranking-private-composite",
    eligibility: "exploratory-private-composite",
  }),
  rust: Object.freeze({
    comparability: "promotable-after-equivalence",
    eligibility: "promotable-after-equivalence",
  }),
});

const SOURCE_RECIPES = Object.freeze({
  w: Object.freeze(["public-w-build-release", "public-w-run", PROCESS_ENTRY0_RECIPE]),
  c: Object.freeze([PUBLIC_C_RECIPE, PRIVATE_C_RECIPE]),
  rust: Object.freeze(["rustc-edition-2024"]),
});

const LEGACY_W_RESULT_RECIPE = "private-native0-mlir0-source-to-pe-candidate";
const LEGACY_W_RESULT_ELIGIBILITY = "contextual-only-until-public-run";

const DIGEST_PATTERN = /^sha256:[0-9a-f]{64}$/u;
const DECIMAL_PATTERN = /^(?:0|[1-9][0-9]*)$/u;
const SAFE_SLUG_PATTERN = /^[a-z0-9][a-z0-9._-]*$/u;
const CPU_MODEL_PATTERN = /^[a-z0-9][a-z0-9 ._()+,:#\[\]{}=@-]*$/iu;
const CPU_MODEL_MAX_LENGTH = 128;

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function isContained(parent, candidate) {
  const relative = path.relative(path.resolve(parent), path.resolve(candidate));
  return relative === "" ||
    (relative !== ".." && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative));
}

function push(errors, message) {
  errors.push(message);
}

function requiredString(value, name, errors) {
  if (typeof value !== "string" || value.trim() === "") {
    push(errors, name + " must be a non-empty string.");
    return false;
  }
  return true;
}

function exactKeys(value, name, expected, errors) {
  if (!isObject(value)) {
    push(errors, name + " must be an object.");
    return false;
  }
  const actual = Object.keys(value).sort();
  const keys = [...expected].sort();
  if (actual.length !== keys.length || actual.some((key, index) => key !== keys[index])) {
    push(errors, name + " must use a closed object shape.");
    return false;
  }
  return true;
}

function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

function sortedById(items) {
  return [...items].sort((left, right) => compareText(String(left?.id ?? ""), String(right?.id ?? "")) || compareText(String(left?.path ?? ""), String(right?.path ?? "")));
}

function isCanonicalOrder(items, sorter) {
  const sorted = sorter(items);
  return items.length === sorted.length && items.every((item, index) => item === sorted[index]);
}

function stringArray(value, name, errors, minimum = 0) {
  if (!Array.isArray(value) || value.length < minimum) {
    push(errors, name + " must be an array of strings.");
    return false;
  }
  const seen = new Set();
  for (const [index, item] of value.entries()) {
    if (requiredString(item, name + "[" + index + "]", errors) && seen.has(item)) {
      push(errors, name + " must not contain duplicates.");
    }
    seen.add(item);
  }
  return true;
}

function digest(value, name, errors) {
  if (typeof value !== "string" || !DIGEST_PATTERN.test(value)) {
    push(errors, name + " must be a lowercase sha256 digest.");
    return false;
  }
  return true;
}

function decimal(value, name, errors) {
  if (typeof value !== "string" || !DECIMAL_PATTERN.test(value)) {
    push(errors, name + " must be a canonical decimal string.");
    return false;
  }
  try {
    if (BigInt(value) > ((1n << 64n) - 1n)) throw new RangeError();
  } catch {
    push(errors, name + " must fit in u64.");
    return false;
  }
  return true;
}

function containedFile(root, relativePath, name, errors) {
  if (typeof relativePath !== "string" || relativePath.length === 0 || path.isAbsolute(relativePath)) {
    push(errors, name + ".path must be repository-relative.");
    return undefined;
  }
  const rootPath = path.resolve(root);
  const lexical = path.resolve(rootPath, relativePath);
  const relative = path.relative(rootPath, lexical);
  if (relative === ".." || relative.startsWith(".." + path.sep) || path.isAbsolute(relative)) {
    push(errors, name + ".path escapes the repository root.");
    return undefined;
  }
  let physical;
  try {
    physical = fs.realpathSync(lexical);
  } catch {
    push(errors, name + ".path must identify an existing file.");
    return undefined;
  }
  const physicalRelative = path.relative(rootPath, physical);
  if (physicalRelative === ".." || physicalRelative.startsWith(".." + path.sep) || path.isAbsolute(physicalRelative)) {
    push(errors, name + ".path resolves outside the repository root.");
    return undefined;
  }
  try {
    if (!fs.statSync(physical).isFile()) throw new Error();
  } catch {
    push(errors, name + ".path must identify a regular file.");
    return undefined;
  }
  return physical;
}

function fileDigest(physical) {
  return "sha256:" + crypto.createHash("sha256").update(fs.readFileSync(physical)).digest("hex");
}

function checkProcessInputVector(value, name, errors) {
  if (!Array.isArray(value)) {
    push(errors, name + " must be an array of exact UTF-8 argument strings.");
    return false;
  }
  for (const [index, item] of value.entries()) {
    if (typeof item !== "string") push(errors, name + "[" + index + "] must be a string.");
  }
  return true;
}

function checkProcessSupportSource(source, name, root, errors) {
  if (!exactKeys(source, name, ["role", "path", "digest"], errors)) return;
  requiredString(source.role, name + ".role", errors);
  if (!PROCESS_ENTRY0_SUPPORT_ROLES.includes(source.role)) push(errors, name + ".role is not a supported PROCESS0 support source role.");
  if (source.role === "provider-header" && source.path !== "compiler/seed-c/include/w_seed_process0.h") {
    push(errors, name + ".path must identify the provider header consumed by the fixed include directory.");
  }
  const physical = containedFile(root, source.path, name, errors);
  if (digest(source.digest, name + ".digest", errors) && physical && source.digest !== fileDigest(physical)) {
    push(errors, name + ".digest is stale.");
  }
}

function checkProcessExecution(execution, name, root, errors) {
  const keys = ["structureClass", "kind", "recipeClass", "timedInput", "correctnessInputs", "faultCases", "supportSources"];
  if (!exactKeys(execution, name, keys, errors)) return;
  if (execution.structureClass !== PROCESS_HANDLER_LIFECYCLE_EXECUTION_STRUCTURE_CLASS) {
    push(errors, name + ".structureClass must identify the transient/internal execution descriptor.");
  }
  if (execution.kind !== PROCESS_ENTRY0_EXECUTION_KIND) push(errors, name + ".kind must identify the private process handler execution.");
  if (execution.recipeClass !== PROCESS_ENTRY0_RECIPE_CLASS) push(errors, name + ".recipeClass must identify the private handler implementation class.");
  if (checkProcessInputVector(execution.timedInput, name + ".timedInput", errors) &&
      JSON.stringify(execution.timedInput) !== JSON.stringify(PROCESS_ENTRY0_TIMED_INPUT)) {
    push(errors, name + ".timedInput must remain the selected [alpha, payload] vector.");
  }
  if (!Array.isArray(execution.correctnessInputs) || execution.correctnessInputs.length !== PROCESS_ENTRY0_CORRECTNESS_INPUTS.length) {
    push(errors, name + ".correctnessInputs must contain exactly the empty and [alpha, payload] vectors.");
  } else {
    for (const [index, vector] of execution.correctnessInputs.entries()) checkProcessInputVector(vector, name + ".correctnessInputs[" + index + "]", errors);
    if (JSON.stringify(execution.correctnessInputs) !== JSON.stringify(PROCESS_ENTRY0_CORRECTNESS_INPUTS)) {
      push(errors, name + ".correctnessInputs must remain ordered as empty then [alpha, payload].");
    }
  }
  if (JSON.stringify(execution.timedInput) !== JSON.stringify(execution.correctnessInputs?.[1])) {
    push(errors, name + ".timedInput must be one of the successful correctness vectors.");
  }
  if (JSON.stringify(execution.faultCases) !== JSON.stringify(PROCESS_ENTRY0_FAULT_CASES)) {
    push(errors, name + ".faultCases must preserve the fixed PROCESS0 fault witness set and order.");
  }
  if (!Array.isArray(execution.supportSources) || execution.supportSources.length !== PROCESS_ENTRY0_SUPPORT_ROLES.length) {
    push(errors, name + ".supportSources must contain the harness, provider and provider-header sources.");
  } else {
    const roles = new Set();
    for (const [index, source] of execution.supportSources.entries()) {
      const sourceName = name + ".supportSources[" + index + "]";
      checkProcessSupportSource(source, sourceName, root, errors);
      if (roles.has(source?.role)) push(errors, sourceName + ".role must be unique.");
      roles.add(source?.role);
      if (source?.role !== PROCESS_ENTRY0_SUPPORT_ROLES[index]) push(errors, sourceName + ".role is not in the canonical support-source order.");
    }
  }
}

function checkSource(source, location, workload, root, errors) {
  const keys = ["language", "path", "digest", "entry", "status", "profile", "quality", "recipe", "recipeClass", "platformTarget", "artifactTarget", "comparability", "eligibility"];
  if (!exactKeys(source, location, keys, errors)) return;
  if (!EXECUTABLE_LANGUAGES.includes(source.language)) push(errors, location + ".language is invalid.");
  requiredString(source.entry, location + ".entry", errors);
  requiredString(source.recipe, location + ".recipe", errors);
  if (SOURCE_RECIPES[source.language] !== undefined && !SOURCE_RECIPES[source.language].includes(source.recipe)) {
    push(errors, location + ".recipe is not a supported recipe for " + source.language + ".");
  }
  requiredString(source.recipeClass, location + ".recipeClass", errors);
  if (source.recipe === PROCESS_ENTRY0_RECIPE && workload?.id !== PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID) push(errors, location + ".recipe is private to process-handler-lifecycle.");
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && source.language === "w" && source.recipe !== PROCESS_ENTRY0_RECIPE) push(errors, location + ".recipe must use the private process handler route.");
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && source.language === "c" && source.recipe !== PRIVATE_C_RECIPE) push(errors, location + ".recipe must use the private GCC/MinGW process handler route.");
  if (workload?.id !== PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && source.language === "c" && source.recipe !== PUBLIC_C_RECIPE) push(errors, location + ".recipe must use the public Clang/MSVC process route.");
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && source.recipeClass !== PROCESS_ENTRY0_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the private process handler class.");
  if (workload?.id === PROCESS_ENTRY_WORKLOAD_ID && source.recipeClass !== PROCESS_ENTRY_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the public process-entry release class.");
  if (source.status !== "source-oracle-ready") push(errors, location + ".status must be source-oracle-ready for a materialized source.");
  if (!MEASUREMENT_PROFILES.includes(source.profile) || source.profile !== "release") push(errors, location + ".profile must be release for M3a sources.");
  if (source.quality !== "correctness-gate") push(errors, location + ".quality must identify correctness as a gate.");
  if (source.platformTarget !== EXECUTABLE_PLATFORM_TARGET) push(errors, location + ".platformTarget must be " + EXECUTABLE_PLATFORM_TARGET + ".");
  const expectedArtifactTarget = artifactTargetFor(workload, source.language);
  if (source.artifactTarget !== expectedArtifactTarget) push(errors, location + ".artifactTarget must be " + expectedArtifactTarget + ".");
  const expectedPolicy = sourcePolicy(workload, source.language, source.recipe);
  if (source.comparability !== expectedPolicy.comparability) push(errors, location + ".comparability does not match the language ABI and benchmark readiness.");
  if (source.eligibility !== expectedPolicy.eligibility) push(errors, location + ".eligibility does not match the source comparability policy.");
  const expectedExtension = { w: ".w", c: ".c", rust: ".rs" }[source.language];
  const physical = containedFile(root, source.path, location, errors);
  if (physical && path.extname(physical).toLowerCase() !== expectedExtension) push(errors, location + ".path extension does not match language.");
  if (physical && digest(source.digest, location + ".digest", errors) && source.digest !== fileDigest(physical)) push(errors, location + ".digest is stale.");
  if (workload.status !== "source-oracle-ready") push(errors, location + " cannot be present on a non-ready workload.");
}

function checkOracleCase(testCase, location, errors) {
  if (!exactKeys(testCase, location, ["arguments", "exitCode", "stdout", "stderr"], errors)) return;
  if (!checkProcessInputVector(testCase.arguments, location + ".arguments", errors)) return;
  if (!Number.isSafeInteger(testCase.exitCode) || testCase.exitCode < 0) push(errors, location + ".exitCode must be a non-negative safe integer.");
  if (typeof testCase.stdout !== "string" || typeof testCase.stderr !== "string") push(errors, location + " output must be strings.");
}

function checkProcessEntryOracle(oracle, location, workloadStatus, errors) {
  if (!exactKeys(oracle, location, ["kind", "status", "timedInput", "cases"], errors)) return;
  if (oracle.kind !== PROCESS_ENTRY_ORACLE_KIND) push(errors, location + ".kind must identify argument-dependent process output.");
  if (oracle.status !== "source-backed") push(errors, location + ".status must be source-backed for the materialized process-entry witness.");
  if (checkProcessInputVector(oracle.timedInput, location + ".timedInput", errors) &&
      JSON.stringify(oracle.timedInput) !== JSON.stringify(PROCESS_ENTRY_TIMED_INPUT)) {
    push(errors, location + ".timedInput must remain the selected [payload] vector.");
  }
  if (!Array.isArray(oracle.cases) || oracle.cases.length !== PROCESS_ENTRY_ORACLE_CASES.length) {
    push(errors, location + ".cases must contain the no-argument, empty-argument and payload cases.");
  } else {
    for (const [index, testCase] of oracle.cases.entries()) checkOracleCase(testCase, location + ".cases[" + index + "]", errors);
    if (JSON.stringify(oracle.cases) !== JSON.stringify(PROCESS_ENTRY_ORACLE_CASES)) {
      push(errors, location + ".cases must preserve the fixed process-entry input/output contract.");
    }
  }
  if (workloadStatus === "source-oracle-ready" && oracle.status !== "source-backed") push(errors, location + " must be source-backed for a ready workload.");
}

function checkOracle(oracle, location, workloadStatus, errors, workloadId) {
  if (workloadId === PROCESS_ENTRY_WORKLOAD_ID) {
    checkProcessEntryOracle(oracle, location, workloadStatus, errors);
    return;
  }
  if (!exactKeys(oracle, location, ["kind", "status", "exitCode", "stdout", "stderr"], errors)) return;
  if (oracle.kind !== "exact-output") push(errors, location + ".kind must be exact-output.");
  if (!["declared", "source-backed"].includes(oracle.status)) push(errors, location + ".status is invalid.");
  if (oracle.status === "source-backed") {
    if (!Number.isSafeInteger(oracle.exitCode) || oracle.exitCode < 0) push(errors, location + ".exitCode must be a non-negative safe integer.");
    if (typeof oracle.stdout !== "string" || typeof oracle.stderr !== "string") push(errors, location + " source-backed output must be strings.");
  } else if (oracle.exitCode !== null || oracle.stdout !== null || oracle.stderr !== null) {
    push(errors, location + " declared oracle must not claim an output.");
  }
  if (workloadStatus === "source-oracle-ready" && oracle.status !== "source-backed") push(errors, location + " must be source-backed for a ready workload.");
}

function checkContract(contract, name, errors) {
  const keys = ["schema", "status", "recordsPath", "requiredIdentity", "provenance", "sampling", "protocolFields", "environmentFields", "artifact", "metrics", "storage"];
  if (!exactKeys(contract, name, keys, errors)) return;
  if (contract.schema !== EXECUTABLE_RESULT_SCHEMA || contract.status !== "contract-only") push(errors, name + " must remain a contract-only local result schema.");
  if (contract.recordsPath !== LOCAL_RESULTS_PATH) push(errors, name + ".recordsPath must identify the ignored local result directory.");
  stringArray(contract.requiredIdentity, name + ".requiredIdentity", errors, 1);
  stringArray(contract.provenance, name + ".provenance", errors, 1);
  if (exactKeys(contract.sampling, name + ".sampling", ["warmupMinimum", "rawMinimum", "rawParity", "sampleFields", "cpuUnits", "zeroCpuDisclosure"], errors)) {
    if (contract.sampling.warmupMinimum !== SAMPLE_COUNT_WARMUP_MINIMUM || contract.sampling.rawMinimum !== SAMPLE_COUNT_MINIMUM || contract.sampling.rawParity !== "odd") push(errors, name + ".sampling must require at least one warmup and an odd raw count of at least nine.");
    if (JSON.stringify(contract.sampling.sampleFields) !== JSON.stringify(SAMPLE_FIELDS)) push(errors, name + ".sampling.sampleFields are incomplete or reordered.");
    if (contract.sampling.cpuUnits !== "microseconds") push(errors, name + ".sampling.cpuUnits must be microseconds.");
    requiredString(contract.sampling.zeroCpuDisclosure, name + ".sampling.zeroCpuDisclosure", errors);
  }
  if (JSON.stringify(contract.protocolFields) !== JSON.stringify(PROTOCOL_FIELDS)) push(errors, name + ".protocolFields must freeze the per-result measurement protocol.");
  if (JSON.stringify(contract.environmentFields) !== JSON.stringify(ENVIRONMENT_FIELDS)) push(errors, name + ".environmentFields must freeze the redacted environment fields.");
  requiredString(contract.artifact, name + ".artifact", errors);
  stringArray(contract.metrics, name + ".metrics", errors, EXECUTABLE_METRICS.length);
  if (JSON.stringify(contract.metrics) !== JSON.stringify(EXECUTABLE_METRICS.map((item) => item.id))) push(errors, name + ".metrics must match the declared vocabulary.");
  requiredString(contract.storage, name + ".storage", errors);
}

function checkBestMetricsContract(contract, name, errors) {
  const keys = ["schema", "status", "storage", "optimizableMetrics", "forbiddenMetrics", "categoryAxes", "provenanceFields", "rule"];
  if (!exactKeys(contract, name, keys, errors)) return;
  if (contract.schema !== EXECUTABLE_BEST_SCHEMA || contract.status !== BEST_METRICS_CONTRACT_STATUS) push(errors, name + " must declare the live best-metrics contract.");
  requiredString(contract.storage, name + ".storage", errors);
  if (JSON.stringify(contract.optimizableMetrics) !== JSON.stringify(OPTIMIZABLE_METRICS)) push(errors, name + ".optimizableMetrics must be the closed promotable set.");
  if (JSON.stringify(contract.forbiddenMetrics) !== JSON.stringify(["exit-code", "stdout", "stderr"])) push(errors, name + ".forbiddenMetrics must exclude correctness outputs.");
  if (JSON.stringify(contract.categoryAxes) !== JSON.stringify(BEST_CATEGORY_AXES)) push(errors, name + ".categoryAxes must preserve every non-crossable category axis.");
  if (JSON.stringify(contract.provenanceFields) !== JSON.stringify(BEST_METRIC_PROVENANCE_FIELDS)) push(errors, name + ".provenanceFields must preserve exact per-metric provenance.");
  requiredString(contract.rule, name + ".rule", errors);
}

export function validateExecutableCatalog(catalog, documents = undefined, root = ROOT) {
  const errors = [];
  const keys = ["$schema", "schema", "kind", "id", "status", "metrics", "comparabilityAxes", "workloads", "resultContract", "bestMetricsContract", "bestMetrics"];
  if (!exactKeys(catalog, "executable catalog", keys, errors)) return errors;
  if (catalog.$schema !== "./executable-benchmark.schema.json" ||
      catalog.schema !== EXECUTABLE_SCHEMA ||
      catalog.kind !== "executable-benchmark-catalog" ||
      catalog.id !== EXECUTABLE_CATALOG_ID ||
      catalog.status !== CATALOG_STATUS) {
    push(errors, "executable catalog identity or status is invalid.");
  }
  if (!Array.isArray(catalog.metrics) || catalog.metrics.length !== EXECUTABLE_METRICS.length) {
    push(errors, "executable catalog.metrics must match the closed metric vocabulary.");
  } else {
    for (const [index, metric] of catalog.metrics.entries()) {
      const expected = EXECUTABLE_METRICS[index];
      if (!exactKeys(metric, "executable catalog.metrics[" + index + "]", ["id", "unit", "kind", "status"], errors)) continue;
      if (metric.id !== expected.id || metric.unit !== expected.unit || metric.kind !== expected.kind || metric.status !== "declared") {
        push(errors, "executable catalog.metrics[" + index + "] does not match the declared vocabulary.");
      }
    }
  }
  if (JSON.stringify(catalog.comparabilityAxes) !== JSON.stringify(EXECUTABLE_COMPARABILITY_AXES)) {
    push(errors, "executable catalog.comparabilityAxes must declare every identity and provenance axis in order.");
  }
  if (!Array.isArray(catalog.workloads) || catalog.workloads.length !== EXECUTABLE_WORKLOAD_IDS.length) {
    push(errors, "executable catalog.workloads must contain the closed workload set.");
  }
  const workloadIds = new Set();
  const workloads = Array.isArray(catalog.workloads) ? catalog.workloads : [];
  for (const [index, workload] of workloads.entries()) {
    const location = "executable catalog.workloads[" + index + "]";
    const workloadKeys = ["id", "structureClass", "status", "sourceReadiness", "demoEvidence", "benchmarkStatus", "lane", "scope", "oracle", "sources", "blockedLanguages", "blockers"];
    if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID) workloadKeys.push("execution");
    if (!exactKeys(workload, location, workloadKeys, errors)) continue;
    if (workloadIds.has(workload.id)) push(errors, location + ".id must be unique.");
    workloadIds.add(workload.id);
    if (workload.id !== EXECUTABLE_WORKLOAD_IDS[index]) push(errors, location + ".id is not in the stable catalog order.");
    if (!requiredString(workload.id, location + ".id", errors) || !requiredString(workload.scope, location + ".scope", errors)) continue;
    if (!EXECUTABLE_STRUCTURE_CLASSES.includes(workload.structureClass)) push(errors, location + ".structureClass is invalid.");
    const expectedStructureClass = workload.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID
      ? PROCESS_HANDLER_LIFECYCLE_STRUCTURE_CLASS
      : "public-end-to-end";
    if (workload.structureClass !== expectedStructureClass) push(errors, location + ".structureClass must be " + expectedStructureClass + ".");
    if (!["source-oracle-ready", "planned", "blocked"].includes(workload.status)) push(errors, location + ".status is invalid.");
    if (!["source-and-oracle-ready", "not-materialized"].includes(workload.sourceReadiness)) push(errors, location + ".sourceReadiness is invalid.");
    if (!["bounded-w-demo", "not-run"].includes(workload.demoEvidence)) push(errors, location + ".demoEvidence is invalid.");
    if (!["not-performance-ready", "deferred-to-M3b", "exploratory-ready", "planned"].includes(workload.benchmarkStatus)) push(errors, location + ".benchmarkStatus is invalid.");
    if (workload.status === "source-oracle-ready" && workload.sourceReadiness !== "source-and-oracle-ready") push(errors, location + ".sourceReadiness must identify a source-backed oracle.");
    if (workload.status !== "source-oracle-ready" && workload.sourceReadiness !== "not-materialized") push(errors, location + ".sourceReadiness must remain not-materialized.");
    if (workload.status === "source-oracle-ready" && workload.benchmarkStatus === "planned") push(errors, location + ".benchmarkStatus must not be planned for a source-backed witness.");
    if (workload.status !== "source-oracle-ready" && workload.benchmarkStatus !== "planned") push(errors, location + ".benchmarkStatus must remain planned without a source-backed witness.");
    if (workload.lane !== "equivalent") push(errors, location + ".lane must be equivalent.");
    if (workload.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID) checkProcessExecution(workload.execution, location + ".execution", root, errors);
    checkOracle(workload.oracle, location + ".oracle", workload.status, errors, workload.id);
    if (!Array.isArray(workload.sources)) push(errors, location + ".sources must be an array.");
    if (!Array.isArray(workload.blockedLanguages)) {
      push(errors, location + ".blockedLanguages must be an array.");
    } else {
      const blocked = new Set();
      for (const [blockedIndex, language] of workload.blockedLanguages.entries()) {
        if (!EXECUTABLE_LANGUAGES.includes(language)) push(errors, location + ".blockedLanguages[" + blockedIndex + "] is invalid.");
        if (blocked.has(language)) push(errors, location + ".blockedLanguages must not contain duplicates.");
        blocked.add(language);
      }
    }
    const exploratoryReady = workload.benchmarkStatus === "exploratory-ready";
    stringArray(workload.blockers, location + ".blockers", errors, exploratoryReady ? 0 : 1);
    if (exploratoryReady && (workload.blockers?.length !== 0 || workload.blockedLanguages?.length !== 0)) {
      push(errors, location + ".benchmarkStatus cannot be exploratory-ready with active measurement blockers.");
    }
    const sourceLanguages = new Set();
    const sources = Array.isArray(workload.sources) ? workload.sources : [];
    for (const [sourceIndex, source] of sources.entries()) {
      const sourceLocation = location + ".sources[" + sourceIndex + "]";
      checkSource(source, sourceLocation, workload, root, errors);
      if (sourceLanguages.has(source?.language)) push(errors, sourceLocation + ".language must be unique per workload.");
      sourceLanguages.add(source?.language);
      if (Array.isArray(workload.blockedLanguages) && workload.blockedLanguages.includes(source?.language)) push(errors, sourceLocation + ".language cannot be both materialized and blocked.");
    }
    const blockedLanguages = Array.isArray(workload.blockedLanguages) ? workload.blockedLanguages : [];
    const partition = new Set([...sourceLanguages, ...blockedLanguages]);
    for (const language of EXECUTABLE_LANGUAGES) if (!partition.has(language)) push(errors, location + " must account for language " + language + " as a source or explicit blocker.");
    if (workload.status === "source-oracle-ready" && !sourceLanguages.has("w")) push(errors, location + " must have a source-backed W witness.");
    if (workload.demoEvidence === "bounded-w-demo" && !sourceLanguages.has("w")) push(errors, location + ".demoEvidence requires a W source witness.");
    if (workload.status !== "source-oracle-ready" && sources.length > 0) push(errors, location + " cannot materialize sources before its oracle is ready.");
  }
  for (const id of EXECUTABLE_WORKLOAD_IDS) if (!workloadIds.has(id)) push(errors, "executable catalog is missing workload " + id + ".");
  try {
    const gateSource = fs.readFileSync(path.resolve(root, PUBLIC_WINDOWS_RUN_GATE), "utf8");
    const publicFixtures = new Set();
    const fixturePattern = /resolve\(\s*seedDirectory\s*,\s*"fixtures"\s*,\s*"([^"]+\.w)"\s*\)/gu;
    for (const match of gateSource.matchAll(fixturePattern))
      publicFixtures.add(`compiler/seed-c/fixtures/${match[1]}`);
    const catalogSources = new Map();
    for (const workload of workloads)
      for (const descriptor of workload.sources ?? [])
        if (descriptor.language === "w") catalogSources.set(descriptor.path, workload.id);
    for (const fixture of publicFixtures) {
      if (catalogSources.has(fixture)) continue;
      const owner = PUBLIC_WINDOWS_RUN_VARIANTS[fixture];
      if (owner === undefined || !workloadIds.has(owner))
        push(errors, `public Windows runnable fixture ${fixture} has no executable benchmark owner.`);
    }
    for (const [fixture, owner] of Object.entries(PUBLIC_WINDOWS_RUN_VARIANTS)) {
      if (!publicFixtures.has(fixture))
        push(errors, `public Windows benchmark variant ${fixture} is stale.`);
      if (!workloadIds.has(owner))
        push(errors, `public Windows benchmark variant ${fixture} has unknown owner ${owner}.`);
    }
  } catch (error) {
    push(errors, `${PUBLIC_WINDOWS_RUN_GATE} cannot be checked for executable benchmark coverage: ${error?.message ?? error}`);
  }
  checkContract(catalog.resultContract, "executable catalog.resultContract", errors);
  checkBestMetricsContract(catalog.bestMetricsContract, "executable catalog.bestMetricsContract", errors);
  errors.push(...validateExecutableBestMetrics(catalog.bestMetrics, catalog).map((error) => "best metrics: " + error));
  return errors;
}

function workloadFor(catalog, id) {
  return Array.isArray(catalog?.workloads) ? catalog.workloads.find((item) => item?.id === id) : undefined;
}

function sourceFor(workload, language) {
  return Array.isArray(workload?.sources) ? workload.sources.find((item) => item.language === language) : undefined;
}

function artifactTargetFor(workload, language) {
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID) return EXECUTABLE_ARTIFACT_TARGET_MINGW;
  return EXECUTABLE_ARTIFACT_TARGET_MSVC;
}

function sourcePolicy(workload, language, recipe) {
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID) return SOURCE_ELIGIBILITY.processHandler;
  if (language === "c") return SOURCE_ELIGIBILITY.cPublic;
  if (language === "rust") return SOURCE_ELIGIBILITY.rust;
  return recipe === "public-w-build-release"
    ? SOURCE_ELIGIBILITY.wPublicBuild
    : SOURCE_ELIGIBILITY.wDeferred;
}

function canonicalEquivalencePayload(workload, platformTarget, profile, recipeClass) {
  const payload = {
    schema: "w-executable-equivalence/1",
    workloadId: workload.id,
    lane: workload.lane,
    scope: workload.scope,
    oracle: workload.oracle,
    platformTarget,
    profile,
    recipeClass,
  };
  if (workload.execution !== undefined) payload.execution = workload.execution;
  return payload;
}

export function executableEquivalenceKey(catalog, workloadId, platformTarget, profile, recipeClass) {
  const workload = workloadFor(catalog, workloadId);
  if (!workload || typeof recipeClass !== "string") return undefined;
  return "sha256:" + crypto.createHash("sha256")
    .update(JSON.stringify(canonicalEquivalencePayload(workload, platformTarget, profile, recipeClass)))
    .digest("hex");
}

function checkSafeIdentityString(value, name, errors) {
  if (!requiredString(value, name, errors)) return;
  if (!SAFE_SLUG_PATTERN.test(value)) push(errors, name + " must be a host/toolchain class, not a hostname or user identifier.");
}

function checkObservedAt(value, name, errors) {
  if (!requiredString(value, name, errors)) return;
  if (!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/u.test(value)) {
    push(errors, name + " must be canonical ISO-8601 UTC.");
    return;
  }
  const parsed = Date.parse(value);
  if (!Number.isFinite(parsed) || new Date(parsed).toISOString() !== value) push(errors, name + " must be a valid canonical ISO-8601 UTC instant.");
}

function checkSample(sample, name, errors) {
  if (!exactKeys(sample, name, SAMPLE_FIELDS, errors)) return;
  positiveDecimal(sample.wallNs, name + ".wallNs", errors);
  decimal(sample.cpuUserUs, name + ".cpuUserUs", errors);
  decimal(sample.cpuSystemUs, name + ".cpuSystemUs", errors);
  decimal(sample.cpuTotalUs, name + ".cpuTotalUs", errors);
  positiveDecimal(sample.peakRssBytes, name + ".peakRssBytes", errors);
  if (DECIMAL_PATTERN.test(String(sample.cpuUserUs)) && DECIMAL_PATTERN.test(String(sample.cpuSystemUs)) && DECIMAL_PATTERN.test(String(sample.cpuTotalUs)) && BigInt(sample.cpuTotalUs) !== BigInt(sample.cpuUserUs) + BigInt(sample.cpuSystemUs)) push(errors, name + ".cpuTotalUs must equal cpuUserUs plus cpuSystemUs.");
}

function deriveSummary(raw) {
  const summary = {};
  for (const field of SAMPLE_FIELDS) {
    const values = raw.map((sample) => BigInt(sample[field])).sort((left, right) => left < right ? -1 : left > right ? 1 : 0);
    const median = values[Math.floor(values.length / 2)];
    const mean = values.reduce((total, value) => total + value, 0n) / BigInt(values.length);
    const deviations = values.map((value) => value >= median ? value - median : median - value).sort((left, right) => left < right ? -1 : left > right ? 1 : 0);
    summary[field] = {
      min: values[0].toString(10),
      median: median.toString(10),
      max: values[values.length - 1].toString(10),
      arithmeticMean: mean.toString(10),
      mad: deviations[Math.floor(deviations.length / 2)].toString(10),
    };
  }
  return summary;
}

function checkSummary(summary, raw, name, errors) {
  if (!exactKeys(summary, name, SAMPLE_FIELDS, errors)) return;
  for (const field of SAMPLE_FIELDS) {
    if (!exactKeys(summary[field], name + "." + field, SAMPLE_SUMMARY_FIELDS, errors)) continue;
    for (const statistic of SAMPLE_SUMMARY_FIELDS) decimal(summary[field][statistic], name + "." + field + "." + statistic, errors);
  }
  if (raw.length > 0) {
    try {
      if (JSON.stringify(summary) !== JSON.stringify(deriveSummary(raw))) push(errors, name + " must derive min/median/max/arithmeticMean/MAD from raw samples.");
    } catch {
      push(errors, name + " cannot be derived from malformed raw samples.");
    }
  }
}

function checkSampleSeries(series, name, errors) {
  if (!exactKeys(series, name, ["warmup", "raw", "summary", "cpuResolution"], errors)) return;
  if (!Array.isArray(series.warmup) || series.warmup.length < SAMPLE_COUNT_WARMUP_MINIMUM) push(errors, name + ".warmup must contain at least one sample.");
  if (!Array.isArray(series.raw) || series.raw.length < SAMPLE_COUNT_MINIMUM || series.raw.length % 2 === 0) push(errors, name + ".raw must contain an odd count of at least nine samples.");
  for (const [index, sample] of (Array.isArray(series.warmup) ? series.warmup : []).entries()) checkSample(sample, name + ".warmup[" + index + "]", errors);
  for (const [index, sample] of (Array.isArray(series.raw) ? series.raw : []).entries()) checkSample(sample, name + ".raw[" + index + "]", errors);
  if (!exactKeys(series.cpuResolution, name + ".cpuResolution", ["unit", "zeroAllowed", "disclosure"], errors)) return;
  if (series.cpuResolution.unit !== "microseconds" || series.cpuResolution.zeroAllowed !== true || !/zero.*microseconds|microseconds.*zero/iu.test(String(series.cpuResolution.disclosure))) push(errors, name + ".cpuResolution must disclose zero-valued microsecond samples without inferring nanosecond precision.");
  if (Array.isArray(series.raw) && series.raw.length >= SAMPLE_COUNT_MINIMUM && series.raw.length % 2 === 1) checkSummary(series.summary, series.raw, name + ".summary", errors);
}

function optimizableMetricField(metric, stage) {
  if (metric === "compile-latency" && stage === "compile") return ["compile", "wallNs"];
  if (metric === "run-wall-time" && stage === "run") return ["run", "wallNs"];
  if (metric === "cpu-time" && stage === "run") return ["run", "cpuTotalUs"];
  if (metric === "peak-working-set" && stage === "run") return ["run", "peakRssBytes"];
  if (metric === "artifact-size" && stage === "artifact") return ["artifact", "sizeBytes"];
  return undefined;
}

function positiveDecimal(value, name, errors) {
  if (!decimal(value, name, errors)) return;
  if (value === "0") push(errors, name + " must be positive.");
}

function checkProtocol(protocol, name, errors) {
  if (!exactKeys(protocol, name, PROTOCOL_FIELDS, errors)) return;
  if (protocol.warmupMinimum !== SAMPLE_COUNT_WARMUP_MINIMUM || protocol.rawMinimum !== SAMPLE_COUNT_MINIMUM || protocol.rawParity !== "odd") {
    push(errors, name + " must freeze at least one warmup and an odd raw count of at least nine.");
  }
  if (protocol.arithmeticMeanRounding !== "floor-integer") push(errors, name + ".arithmeticMeanRounding must be floor-integer.");
  if (protocol.stopRule !== "fixed-count") push(errors, name + ".stopRule must be fixed-count.");
  if (protocol.wallClock !== "monotonic-nanoseconds") push(errors, name + ".wallClock must be monotonic-nanoseconds.");
  if (protocol.processIsolation !== "fresh-process-per-sample") push(errors, name + ".processIsolation must require a fresh process per sample.");
  if (!["deterministic-interleaved", "compile-series-then-run-series"].includes(protocol.order)) {
    push(errors, name + ".order must declare a supported deterministic measurement order.");
  }
  requiredString(protocol.resourceScope, name + ".resourceScope", errors);
  if (!["w-native-benchmark/2", "bun-direct-test/1"].includes(protocol.measurementKernel)) {
    push(errors, name + ".measurementKernel must identify the native production kernel or the bounded test adapter.");
  }
  stringArray(protocol.knownNoiseControls, name + ".knownNoiseControls", errors, 1);
  stringArray(protocol.unknownNoiseControls, name + ".unknownNoiseControls", errors, 1);
  const disclosure = String(protocol.directProcessDisclosure ?? "");
  const legacyDisclosure = /Bun.*direct.*process|direct.*process.*Bun/iu.test(disclosure) &&
    /process[- ]tree.*aggregat|aggregat.*process[- ]tree/iu.test(disclosure);
  const nativeDisclosure = /QPC/iu.test(disclosure) && /Job Object/iu.test(disclosure) &&
    /working set/iu.test(disclosure) && /commit/iu.test(disclosure);
  if (!requiredString(protocol.directProcessDisclosure, name + ".directProcessDisclosure", errors) ||
      (protocol.measurementKernel === "w-native-benchmark/2" ? !nativeDisclosure : !legacyDisclosure)) {
    push(errors, name + ".directProcessDisclosure must match the selected measurement kernel and distinguish process-tree CPU, root working set, and Job commit.");
  }
}

function normalizeSlug(value) {
  if (typeof value !== "string") return undefined;
  const normalized = value.trim().toLowerCase();
  return normalized.length > 0 && normalized.length <= 64 && SAFE_SLUG_PATTERN.test(normalized) ? normalized : undefined;
}

function normalizeCpuModel(value) {
  if (typeof value !== "string") return undefined;
  const normalized = value.trim().replace(/\s+/gu, " ").toLowerCase();
  if (normalized.length === 0 || normalized.length > CPU_MODEL_MAX_LENGTH || !CPU_MODEL_PATTERN.test(normalized)) return undefined;
  if (/[\\/]/u.test(normalized) || /^[a-z]:/u.test(normalized) || normalized.includes("..")) return undefined;
  return normalized;
}

function isPositiveDecimal(value) {
  if (typeof value !== "string" || !DECIMAL_PATTERN.test(value) || value === "0") return false;
  try {
    return BigInt(value) <= ((1n << 64n) - 1n);
  } catch {
    return false;
  }
}

function normalizeEnvironment(environment) {
  if (!isObject(environment) || Object.keys(environment).sort().join("\u0000") !== [...ENVIRONMENT_FIELDS].sort().join("\u0000")) return undefined;
  const normalized = {
    os: normalizeSlug(environment.os),
    kernel: normalizeSlug(environment.kernel),
    cpuModel: normalizeCpuModel(environment.cpuModel),
    logicalCores: environment.logicalCores,
    ramBytes: environment.ramBytes,
  };
  if (!normalized.os || !normalized.kernel || !normalized.cpuModel || !isPositiveDecimal(normalized.logicalCores) || !isPositiveDecimal(normalized.ramBytes)) return undefined;
  return normalized;
}

export function executableHostIdentity(environment) {
  const normalized = normalizeEnvironment(environment);
  if (!normalized) return undefined;
  const fingerprint = crypto.createHash("sha256").update(JSON.stringify(normalized)).digest("hex").slice(0, 16);
  return normalized.os + "-" + normalized.kernel + "-" + fingerprint;
}

function checkEnvironment(environment, name, errors) {
  if (!exactKeys(environment, name, ENVIRONMENT_FIELDS, errors)) return;
  for (const field of ["os", "kernel"]) {
    if (!requiredString(environment[field], name + "." + field, errors)) continue;
    if (normalizeSlug(environment[field]) !== environment[field]) push(errors, name + "." + field + " must be a canonical redacted environment class without host/user/path identifiers.");
  }
  if (!requiredString(environment.cpuModel, name + ".cpuModel", errors) || !normalizeCpuModel(environment.cpuModel)) {
    push(errors, name + ".cpuModel must be a bounded readable redacted environment class without host/user/path identifiers.");
  }
  positiveDecimal(environment.logicalCores, name + ".logicalCores", errors);
  positiveDecimal(environment.ramBytes, name + ".ramBytes", errors);
}

function checkArtifactCleanliness(cleanliness, name, errors) {
  if (!exactKeys(cleanliness, name, ARTIFACT_CLEANLINESS_FIELDS, errors)) return;
  if (exactKeys(cleanliness.coffSymbols, `${name}.coffSymbols`, ["pointer", "count"], errors)) {
    if (cleanliness.coffSymbols.pointer !== "0" || cleanliness.coffSymbols.count !== "0") {
      push(errors, `${name}.coffSymbols must prove a zero pointer and zero count.`);
    }
  }
  if (exactKeys(cleanliness.codeView, `${name}.codeView`, ["count", "sizeBytes"], errors)) {
    if (cleanliness.codeView.count !== "0" || cleanliness.codeView.sizeBytes !== "0") {
      push(errors, `${name}.codeView must prove zero entries and zero bytes.`);
    }
  }
  const debug = cleanliness.debugDirectory;
  if (exactKeys(debug, `${name}.debugDirectory`, ["presence", "sizeBytes", "entries"], errors)) {
    if (debug.presence !== "absent" && debug.presence !== "pogo-only" && debug.presence !== "repro-only") {
      push(errors, `${name}.debugDirectory.presence must be absent, pogo-only or repro-only.`);
    }
    const expectedDirectorySize = Array.isArray(debug.entries) ? String(debug.entries.length * 28) : undefined;
    if (!Array.isArray(debug.entries)) {
      push(errors, `${name}.debugDirectory.entries must be an array.`);
    } else {
      for (const [index, entry] of debug.entries.entries()) {
        const entryName = `${name}.debugDirectory.entries[${index}]`;
        if (!exactKeys(entry, entryName, ["type", "typeCode", "sizeBytes"], errors)) continue;
        const pogo = entry.type === "pogo" && entry.typeCode === 13 && typeof entry.sizeBytes === "string" && /^[1-9][0-9]*$/u.test(entry.sizeBytes);
        const repro = entry.type === "repro" && entry.typeCode === 16 && entry.sizeBytes === "0";
        if (!pogo && !repro) push(errors, `${entryName} must be POGO type 13 with a positive payload or REPRO type 16 with no payload.`);
      }
    }
    if (debug.presence === "absent" && (debug.sizeBytes !== "0" || debug.entries?.length !== 0)) {
      push(errors, `${name}.debugDirectory absent form must have zero bytes and no entries.`);
    }
    if (debug.presence === "pogo-only" && (debug.entries?.length < 1 || debug.sizeBytes !== expectedDirectorySize)) {
      push(errors, `${name}.debugDirectory pogo-only form must contain only bounded 28-byte POGO entries.`);
    }
    if (debug.presence === "repro-only" && (debug.entries?.length !== 1 || debug.sizeBytes !== "28" || debug.entries.some((entry) => entry.type !== "repro"))) {
      push(errors, `${name}.debugDirectory repro-only form must contain exactly one payload-free 28-byte REPRO entry.`);
    }
    if (debug.presence === "pogo-only" && debug.entries?.some((entry) => entry.type !== "pogo")) {
      push(errors, `${name}.debugDirectory pogo-only form must not contain other entry types.`);
    }
  }
  for (const [field, keys] of [
    ["certificateDirectory", ["pointer", "sizeBytes"]],
    ["overlay", ["sizeBytes"]],
    ["sidecars", ["count"]],
  ]) {
    if (!exactKeys(cleanliness[field], `${name}.${field}`, keys, errors)) continue;
    if (keys.some((key) => cleanliness[field][key] !== "0")) {
      push(errors, `${name}.${field} must prove zero ${keys.join(" and ")}.`);
    }
  }
  if (cleanliness.sectionData !== "in-bounds") push(errors, `${name}.sectionData must be in-bounds.`);
}

function checkProcessEntryCorrectness(correctness, workload, name, errors) {
  if (!exactKeys(correctness, name, ["oracleId", "cases"], errors)) return;
  requiredString(correctness.oracleId, name + ".oracleId", errors);
  if (workload && correctness.oracleId !== `${workload.id}:${PROCESS_ENTRY_ORACLE_KIND}`) {
    push(errors, name + ".oracleId must identify the workload argument-dependent output oracle.");
  }
  const oracleCases = workload?.oracle?.cases;
  if (!Array.isArray(correctness.cases) || !Array.isArray(oracleCases) || correctness.cases.length !== oracleCases.length) {
    push(errors, name + ".cases must contain one digest record for every source-backed oracle case.");
    return;
  }
  for (const [index, testCase] of correctness.cases.entries()) {
    const caseName = `${name}.cases[${index}]`;
    if (!exactKeys(testCase, caseName, ["arguments", "exitCode", "stdoutDigest", "stderrDigest"], errors)) continue;
    checkProcessInputVector(testCase.arguments, caseName + ".arguments", errors);
    if (JSON.stringify(testCase.arguments) !== JSON.stringify(oracleCases[index].arguments)) push(errors, caseName + ".arguments must match the source-backed oracle case.");
    if (testCase.exitCode !== oracleCases[index].exitCode) push(errors, caseName + ".exitCode must match the source-backed oracle case.");
    digest(testCase.stdoutDigest, caseName + ".stdoutDigest", errors);
    digest(testCase.stderrDigest, caseName + ".stderrDigest", errors);
    if (testCase.stdoutDigest !== exactOutputDigest(oracleCases[index].stdout) || testCase.stderrDigest !== exactOutputDigest(oracleCases[index].stderr)) {
      push(errors, caseName + " output digests must match the source-backed oracle case.");
    }
  }
}

export function validateExecutableResult(result, catalog = loadExecutableDocuments().catalog, options = {}) {
  const errors = [];
  const keys = ["$schema", "schema", "kind", "id", "status", "workloadId", "language", "platformTarget", "artifactTarget", "profile", "quality", "claim", "verdict", "equivalenceKey", "identity", "correctness", "artifact", "protocol", "environment", "compile", "run", "provenance"];
  if (!exactKeys(result, "executable result", keys, errors)) return errors;
  if (result.$schema !== "./executable-benchmark.schema.json" || result.schema !== EXECUTABLE_RESULT_SCHEMA || result.kind !== "executable-result" || result.status !== "recorded") push(errors, "executable result identity or status is invalid.");
  requiredString(result.id, "executable result.id", errors);
  const workload = workloadFor(catalog, result.workloadId);
  if (!workload || workload.status !== "source-oracle-ready") push(errors, "executable result must identify a ready workload.");
  const source = sourceFor(workload, result.language);
  if (!source) push(errors, "executable result language must identify a materialized source.");
  if (!EXECUTABLE_LANGUAGES.includes(result.language)) push(errors, "executable result.language is invalid.");
  const expectedPolicy = sourcePolicy(workload, result.language, source?.recipe);
  if (source && (source.comparability !== expectedPolicy.comparability || source.eligibility !== expectedPolicy.eligibility)) {
    push(errors, "executable result source comparability and eligibility must match the exact catalog policy.");
  }
  if (result.platformTarget !== EXECUTABLE_PLATFORM_TARGET || (source && result.platformTarget !== source.platformTarget)) push(errors, "executable result.platformTarget must be " + EXECUTABLE_PLATFORM_TARGET + " and match the source identity.");
  if (source && result.artifactTarget !== source.artifactTarget) push(errors, "executable result.artifactTarget must match the source ABI target.");
  const expectedResultArtifactTarget = artifactTargetFor(workload, result.language);
  if (result.artifactTarget !== expectedResultArtifactTarget) push(errors, "executable result artifact target must match the workload ABI target.");
  if (result.profile !== "release") push(errors, "executable result.profile must be release in M3a.");
  if (result.quality !== "exploratory") push(errors, "executable result.quality must be exploratory for measurement evidence.");
  if (result.claim !== "measurement-only") push(errors, "executable result.claim must be measurement-only.");
  if (result.verdict !== "not-evaluated") push(errors, "executable result.verdict must be not-evaluated until managed regression exists.");
  const expectedKey = source ? executableEquivalenceKey(catalog, result.workloadId, result.platformTarget, result.profile, source.recipeClass) : undefined;
  if (!digest(result.equivalenceKey, "executable result.equivalenceKey", errors) || result.equivalenceKey !== expectedKey) push(errors, "executable result.equivalenceKey must be recomputed from workload semantics, platform target, profile and recipe class.");
  if (exactKeys(result.identity, "executable result.identity", ["sourceDigest", "platformTarget", "artifactTarget", "profile", "toolchain", "host", "recipe", "recipeClass", "recipeDigest", "eligibility"], errors)) {
    digest(result.identity.sourceDigest, "executable result.identity.sourceDigest", errors);
    if (result.identity.platformTarget !== EXECUTABLE_PLATFORM_TARGET || result.identity.platformTarget !== result.platformTarget) push(errors, "executable result.identity.platformTarget must match the shared platform target.");
    if (result.identity.artifactTarget !== result.artifactTarget) push(errors, "executable result.identity.artifactTarget must match the exact artifact target.");
    if (result.identity.profile !== result.profile) push(errors, "executable result identity profile must match the result.");
    checkSafeIdentityString(result.identity.toolchain, "executable result.identity.toolchain", errors);
    checkSafeIdentityString(result.identity.host, "executable result.identity.host", errors);
    requiredString(result.identity.recipe, "executable result.identity.recipe", errors);
    requiredString(result.identity.recipeClass, "executable result.identity.recipeClass", errors);
    digest(result.identity.recipeDigest, "executable result.identity.recipeDigest", errors);
    requiredString(result.identity.eligibility, "executable result.identity.eligibility", errors);
    if (source && result.identity.sourceDigest !== source.digest) push(errors, "executable result.identity.sourceDigest must match the catalog source.");
    const historicalWRecipe = options.allowHistoricalWRecipe === true && result.language === "w" && source?.recipe === "public-w-build-release" && result.identity.recipe === LEGACY_W_RESULT_RECIPE && result.identity.eligibility === LEGACY_W_RESULT_ELIGIBILITY;
    const identityMismatch = historicalWRecipe
      ? result.identity.recipeClass !== source?.recipeClass || result.identity.platformTarget !== source?.platformTarget || result.identity.artifactTarget !== source?.artifactTarget
      : result.identity.recipe !== source?.recipe || result.identity.recipeClass !== source?.recipeClass || result.identity.eligibility !== source?.eligibility || result.identity.platformTarget !== source?.platformTarget || result.identity.artifactTarget !== source?.artifactTarget;
    if (source && identityMismatch) push(errors, "executable result identity must match catalog target, recipe, recipe class and eligibility.");
  }
  if (workload?.id === PROCESS_ENTRY_WORKLOAD_ID) {
    checkProcessEntryCorrectness(result.correctness, workload, "executable result.correctness", errors);
  } else if (exactKeys(result.correctness, "executable result.correctness", ["oracleId", "exitCode", "stdoutDigest", "stderrDigest"], errors)) {
    requiredString(result.correctness.oracleId, "executable result.correctness.oracleId", errors);
    if (workload && result.correctness.oracleId !== `${workload.id}:exact-output`) push(errors, "executable result.correctness.oracleId must identify the workload exact-output oracle.");
    if (!Number.isSafeInteger(result.correctness.exitCode) || result.correctness.exitCode < 0) push(errors, "executable result.correctness.exitCode must be a non-negative safe integer.");
    digest(result.correctness.stdoutDigest, "executable result.correctness.stdoutDigest", errors);
    digest(result.correctness.stderrDigest, "executable result.correctness.stderrDigest", errors);
    if (workload?.oracle?.status === "source-backed" && (result.correctness.exitCode !== workload.oracle.exitCode || result.correctness.stdoutDigest !== exactOutputDigest(workload.oracle.stdout) || result.correctness.stderrDigest !== exactOutputDigest(workload.oracle.stderr))) push(errors, "executable result.correctness must match the exact-output oracle.");
  }
  const hasArtifactCleanliness = isObject(result.artifact) && Object.prototype.hasOwnProperty.call(result.artifact, "cleanliness");
  if (!hasArtifactCleanliness && options.allowHistoricalArtifactWithoutCleanliness !== true) {
    push(errors, "executable result.artifact.cleanliness is required for new runner-bound results.");
  }
  const artifactFields = hasArtifactCleanliness
    ? ["digest", "sizeBytes", "cleanliness"]
    : ["digest", "sizeBytes"];
  if (exactKeys(result.artifact, "executable result.artifact", artifactFields, errors)) {
    digest(result.artifact.digest, "executable result.artifact.digest", errors);
    positiveDecimal(result.artifact.sizeBytes, "executable result.artifact.sizeBytes", errors);
    if (artifactFields.includes("cleanliness")) checkArtifactCleanliness(result.artifact.cleanliness, "executable result.artifact.cleanliness", errors);
  }
  checkProtocol(result.protocol, "executable result.protocol", errors);
  checkEnvironment(result.environment, "executable result.environment", errors);
  const expectedHost = executableHostIdentity(result.environment);
  if (expectedHost && result.identity?.host !== expectedHost) push(errors, "executable result.identity.host must be derived from the redacted environment.");
  checkSampleSeries(result.compile, "executable result.compile", errors);
  checkSampleSeries(result.run, "executable result.run", errors);
  if (exactKeys(result.provenance, "executable result.provenance", ["sourceDigest", "artifactDigest", "recipeDigest", "toolchainDigest", "runnerDigest", "catalogDigest", "commit", "observedAt"], errors)) {
    for (const field of ["sourceDigest", "artifactDigest", "recipeDigest", "toolchainDigest", "runnerDigest", "catalogDigest"]) digest(result.provenance[field], "executable result.provenance." + field, errors);
    if (typeof result.provenance.commit !== "string" || !/^[0-9a-f]{40}$/u.test(result.provenance.commit)) push(errors, "executable result.provenance.commit must be the full lowercase Git commit identity.");
    checkObservedAt(result.provenance.observedAt, "executable result.provenance.observedAt", errors);
    if (result.provenance.sourceDigest !== result.identity?.sourceDigest || result.provenance.artifactDigest !== result.artifact?.digest || result.provenance.recipeDigest !== result.identity?.recipeDigest) push(errors, "executable result provenance must repeat source, artifact and recipe identity exactly.");
  }
  return errors;
}

function resultMetricValue(record, metric) {
  if (metric === "compile-latency") return record.compile.summary.wallNs.median;
  if (metric === "run-wall-time") return record.run.summary.wallNs.median;
  if (metric === "run-wall-p95") {
    const values = record.run.raw.map((sample) => BigInt(sample.wallNs))
      .sort((left, right) => left < right ? -1 : left > right ? 1 : 0);
    const index = Number((BigInt(values.length) * 95n + 99n) / 100n - 1n);
    return values[index]?.toString(10);
  }
  if (metric === "cpu-time") return record.run.summary.cpuTotalUs.arithmeticMean;
  if (metric === "peak-working-set") return record.run.summary.peakRssBytes.median;
  if (metric === "artifact-size") return record.artifact.sizeBytes;
  return undefined;
}

function bestMetricStatistic(metric) {
  if (metric === "artifact-size") return "single-artifact";
  if (metric === "cpu-time") return "arithmeticMean";
  if (metric === "run-wall-p95") return "p95";
  return "median";
}

function bestMetricIsEligible(record, metric) {
  const value = resultMetricValue(record, metric);
  if (["cpu-time", "run-wall-p95"].includes(metric) && record.run.raw.length < 101) return false;
  // Averaging 101 fresh processes can recover a useful scheduler-accounted
  // CPU estimate from quantized per-process counters. An all-zero mean still
  // cannot establish a useful lower-is-better cell.
  return value !== undefined && (!metric.startsWith("cpu-") || value !== "0");
}

function categoryIdentity(value) {
  return Object.fromEntries(BEST_CATEGORY_AXES.map((axis) => [axis, value?.[axis]]));
}

export function executableCategoryKey(value) {
  return JSON.stringify(categoryIdentity(value));
}

export function executableCategoryId(value) {
  return "category-" + crypto.createHash("sha256").update(executableCategoryKey(value), "utf8").digest("hex");
}

function bestMetricId(category, metric) {
  return "best-" + crypto.createHash("sha256").update(`${category}\u0000${metric}`, "utf8").digest("hex");
}

function compareResultForTie(left, right) {
  return compareText(String(left?.id ?? ""), String(right?.id ?? "")) ||
    compareText(String(left?.artifact?.digest ?? ""), String(right?.artifact?.digest ?? "")) ||
    compareText(String(left?.provenance?.commit ?? ""), String(right?.provenance?.commit ?? ""));
}

function entryFromResult(record, catalog, metric) {
  const workload = workloadFor(catalog, record.workloadId);
  const source = sourceFor(workload, record.language);
  const category = {
    workloadId: record.workloadId,
    language: record.language,
    equivalenceKey: record.equivalenceKey,
    platformTarget: record.platformTarget,
    artifactTarget: record.artifactTarget,
    abi: record.artifactTarget,
    profile: record.profile,
    host: record.identity.host,
    recipeClass: record.identity.recipeClass,
    comparability: source?.comparability,
    eligibility: source?.eligibility,
  };
  const categoryKey = executableCategoryKey(category);
  return {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_BEST_SCHEMA,
    kind: "executable-best-metric",
    id: bestMetricId(categoryKey, metric),
    status: "current",
    categoryId: executableCategoryId(category),
    workloadId: record.workloadId,
    language: record.language,
    equivalenceKey: record.equivalenceKey,
    platformTarget: record.platformTarget,
    artifactTarget: record.artifactTarget,
    abi: record.artifactTarget,
    profile: record.profile,
    host: record.identity.host,
    recipeClass: record.identity.recipeClass,
    comparability: source?.comparability,
    eligibility: source?.eligibility,
    metric,
    unit: EXECUTABLE_METRICS.find((item) => item.id === metric)?.unit,
    statistic: bestMetricStatistic(metric),
    value: resultMetricValue(record, metric),
    toolchain: record.identity.toolchain,
    recipe: record.identity.recipe,
    provenance: {
      recordId: record.id,
      commit: record.provenance.commit,
      observedAt: record.provenance.observedAt,
      sourceDigest: record.provenance.sourceDigest,
      artifactDigest: record.provenance.artifactDigest,
      recipeDigest: record.provenance.recipeDigest,
      toolchainDigest: record.provenance.toolchainDigest,
      runnerDigest: record.provenance.runnerDigest,
      catalogDigest: record.provenance.catalogDigest,
      artifactCleanliness: record.artifact?.cleanliness ? "verified-clean" : "historical-unverified",
    },
  };
}

export function deriveExecutableBestMetrics(catalog, results) {
  if (!Array.isArray(results)) throw new TypeError("validated executable results are required");
  const groups = new Map();
  const ids = new Set();
  for (const item of results) {
    const record = item?.record ?? item;
    const errors = validateExecutableResult(record, catalog, {
      allowHistoricalWRecipe: true,
      allowHistoricalArtifactWithoutCleanliness: true,
    });
    if (errors.length > 0) throw new Error(errors.join("; "));
    if (ids.has(record.id)) throw new Error(`executable results contain duplicate id: ${record.id}`);
    ids.add(record.id);
    const workload = workloadFor(catalog, record.workloadId);
    const source = sourceFor(workload, record.language);
    if (!source) continue;
    const category = {
      workloadId: record.workloadId,
      language: record.language,
      equivalenceKey: record.equivalenceKey,
      platformTarget: record.platformTarget,
      artifactTarget: record.artifactTarget,
      abi: record.artifactTarget,
      profile: record.profile,
      host: record.identity.host,
      recipeClass: record.identity.recipeClass,
      comparability: source.comparability,
      eligibility: source.eligibility,
    };
    const key = executableCategoryKey(category);
    const group = groups.get(key) ?? { key, records: [] };
    group.records.push(record);
    groups.set(key, group);
  }

  const entries = [];
  for (const group of [...groups.values()].sort((left, right) => compareText(left.key, right.key))) {
    const ordered = [...group.records].sort(compareResultForTie);
    for (const metric of BEST_METRIC_ORDER) {
      const values = ordered
        .filter((record) => bestMetricIsEligible(record, metric))
        .map((record) => ({ record, value: BigInt(resultMetricValue(record, metric)) }));
      if (values.length === 0) continue;
      const minimum = values.reduce((best, item) => item.value < best ? item.value : best, values[0].value);
      const primary = values.filter((item) => item.value === minimum).map((item) => item.record).sort(compareResultForTie)[0];
      entries.push(entryFromResult(primary, catalog, metric));
    }
  }
  entries.sort(bestMetricSort);
  return {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_BEST_SCHEMA,
    kind: "executable-best-metrics",
    status: entries.length === 0 ? "empty" : BEST_METRICS_STATUS,
    entries,
  };
}

function bestMetricSort(left, right) {
  return compareText(String(left?.workloadId ?? ""), String(right?.workloadId ?? "")) ||
    compareText(String(left?.language ?? ""), String(right?.language ?? "")) ||
    compareText(String(left?.equivalenceKey ?? ""), String(right?.equivalenceKey ?? "")) ||
    compareText(String(left?.platformTarget ?? ""), String(right?.platformTarget ?? "")) ||
    compareText(String(left?.artifactTarget ?? ""), String(right?.artifactTarget ?? "")) ||
    compareText(String(left?.profile ?? ""), String(right?.profile ?? "")) ||
    compareText(String(left?.host ?? ""), String(right?.host ?? "")) ||
    compareText(String(left?.recipeClass ?? ""), String(right?.recipeClass ?? "")) ||
    compareText(String(left?.metric ?? ""), String(right?.metric ?? "")) ||
    compareText(String(left?.id ?? ""), String(right?.id ?? ""));
}

export function validateExecutableBestMetric(record, catalog = loadExecutableDocuments().catalog) {
  const errors = [];
  const keys = ["$schema", "schema", "kind", "id", "status", "categoryId", "workloadId", "language", "equivalenceKey", "platformTarget", "artifactTarget", "abi", "profile", "host", "recipeClass", "comparability", "eligibility", "metric", "unit", "statistic", "value", "toolchain", "recipe", "provenance"];
  if (!exactKeys(record, "executable best-metric record", keys, errors)) return errors;
  if (record.$schema !== "./executable-benchmark.schema.json" || record.schema !== EXECUTABLE_BEST_SCHEMA || record.kind !== "executable-best-metric" || record.status !== "current") push(errors, "executable best-metric identity or status is invalid.");
  requiredString(record.id, "executable best metric.id", errors);
  const workload = workloadFor(catalog, record.workloadId);
  const source = sourceFor(workload, record.language);
  if (!workload || workload.status !== "source-oracle-ready") push(errors, "executable best metric must identify a ready workload.");
  if (!OPTIMIZABLE_METRICS.includes(record.metric)) push(errors, "executable best metric must be optimizable, never exit-code/stdout/stderr.");
  if (!EXECUTABLE_LANGUAGES.includes(record.language) || !source) push(errors, "executable best metric.language must identify a materialized source.");
  if (record.platformTarget !== EXECUTABLE_PLATFORM_TARGET) push(errors, "executable best metric.platformTarget must be " + EXECUTABLE_PLATFORM_TARGET + ".");
  if (source && record.artifactTarget !== source.artifactTarget) push(errors, "executable best metric.artifactTarget must match the source ABI target.");
  if (record.abi !== record.artifactTarget) push(errors, "executable best metric.abi must equal the artifact target.");
  if (record.artifactTarget !== artifactTargetFor(workload, record.language)) push(errors, "executable best metric artifact target must match the workload ABI target.");
  if (record.profile !== "release") push(errors, "executable best metric.profile must be release.");
  const expectedUnit = EXECUTABLE_METRICS.find((item) => item.id === record.metric)?.unit;
  if (record.unit !== expectedUnit) push(errors, "executable best metric.unit must match the declared metric.");
  const expectedStatistic = bestMetricStatistic(record.metric);
  if (record.statistic !== expectedStatistic) push(errors, "executable best metric.statistic must be " + expectedStatistic + ".");
  digest(record.equivalenceKey, "executable best metric.equivalenceKey", errors);
  checkSafeIdentityString(record.toolchain, "executable best metric.toolchain", errors);
  checkSafeIdentityString(record.host, "executable best metric.host", errors);
  requiredString(record.recipe, "executable best metric.recipe", errors);
  requiredString(record.recipeClass, "executable best metric.recipeClass", errors);
  if (source && record.recipeClass !== source.recipeClass) push(errors, "executable best metric.recipeClass must match the catalog source.");
  if (source) {
    const expectedKey = executableEquivalenceKey(catalog, record.workloadId, record.platformTarget, record.profile, source.recipeClass);
    if (record.equivalenceKey !== expectedKey) push(errors, "executable best metric.equivalenceKey must be recomputed from the catalog.");
    if (record.comparability !== source.comparability || record.eligibility !== source.eligibility) push(errors, "executable best metric policy must match the catalog source.");
  }
  positiveDecimal(record.value, "executable best metric.value", errors);
  const expectedCategoryId = executableCategoryId(record);
  if (record.categoryId !== expectedCategoryId) push(errors, "executable best metric.categoryId must be derived from its category identity.");
  if (exactKeys(record.provenance, "executable best metric.provenance", BEST_METRIC_PROVENANCE_FIELDS, errors)) {
    requiredString(record.provenance.recordId, "executable best metric.provenance.recordId", errors);
    if (typeof record.provenance.commit !== "string" || !/^[0-9a-f]{40}$/u.test(record.provenance.commit)) push(errors, "executable best metric.provenance.commit must be a full lowercase Git commit identity.");
    checkObservedAt(record.provenance.observedAt, "executable best metric.provenance.observedAt", errors);
    for (const field of ["sourceDigest", "artifactDigest", "recipeDigest", "toolchainDigest", "runnerDigest", "catalogDigest"]) digest(record.provenance[field], "executable best metric.provenance." + field, errors);
    if (!['historical-unverified', 'verified-clean'].includes(record.provenance.artifactCleanliness)) push(errors, "executable best metric.provenance.artifactCleanliness must be historical-unverified or verified-clean.");
    if (source && record.provenance.sourceDigest !== source.digest) push(errors, "executable best metric.provenance.sourceDigest must match the catalog source.");
    if (record.provenance.recipeDigest === undefined) push(errors, "executable best metric provenance must include recipeDigest.");
  }
  return errors;
}

export function validateExecutableBestMetrics(index, catalog = loadExecutableDocuments().catalog) {
  const errors = [];
  if (!exactKeys(index, "executable best-metrics catalog", ["$schema", "schema", "kind", "status", "entries"], errors)) return errors;
  if (index.$schema !== "./executable-benchmark.schema.json" || index.schema !== EXECUTABLE_BEST_SCHEMA || index.kind !== "executable-best-metrics") {
    push(errors, "executable best-metrics identity is invalid.");
  }
  if (!["empty", BEST_METRICS_STATUS].includes(index.status)) push(errors, "executable best-metrics status must be empty or current.");
  if (!Array.isArray(index.entries)) {
    push(errors, "executable best-metrics entries must be an array.");
  } else {
    if (index.entries.length === 0 && index.status !== "empty") push(errors, "empty executable best-metrics catalog must be empty.");
    if (index.entries.length > 0 && index.status !== BEST_METRICS_STATUS) push(errors, "non-empty executable best-metrics catalog must be current.");
    const ids = new Set();
    const cells = new Set();
    for (const record of index.entries) {
      if (ids.has(record?.id)) push(errors, "executable best-metrics ids must be unique.");
      ids.add(record?.id);
      const cell = `${record?.categoryId ?? ""}\u0000${record?.metric ?? ""}`;
      if (cells.has(cell)) push(errors, "executable best-metrics must contain one cell per category and metric.");
      cells.add(cell);
    }
    if (!isCanonicalOrder(index.entries, (items) => [...items].sort(bestMetricSort))) {
      push(errors, "executable best-metrics entries must be sorted by category and metric.");
    }
    for (const [number, record] of index.entries.entries()) errors.push(...validateExecutableBestMetric(record, catalog).map((error) => "entries[" + number + "]: " + error));
  }
  return errors;
}

export function updateExecutableBestMetrics(catalog, result) {
  const errors = validateExecutableResult(result, catalog);
  if (errors.length > 0) throw new Error(errors.join("; "));
  const candidate = deriveExecutableBestMetrics(catalog, [result]);
  const entries = [...(catalog.bestMetrics?.entries ?? [])];
  const byCell = new Map(entries.map((entry) => [`${entry.categoryId}\u0000${entry.metric}`, entry]));
  const updatedMetrics = [];
  for (const entry of candidate.entries) {
    const key = `${entry.categoryId}\u0000${entry.metric}`;
    const previous = byCell.get(key);
    if (!previous || BigInt(entry.value) < BigInt(previous.value)) {
      byCell.set(key, entry);
      updatedMetrics.push(entry.metric);
    }
  }
  const nextEntries = [...byCell.values()].sort(bestMetricSort);
  const changed = JSON.stringify(nextEntries) !== JSON.stringify(entries);
  return {
    catalog: changed ? { ...catalog, bestMetrics: { ...catalog.bestMetrics, status: nextEntries.length === 0 ? "empty" : BEST_METRICS_STATUS, entries: nextEntries } } : catalog,
    changed,
    updatedMetrics: [...new Set(updatedMetrics)].sort(compareText),
  };
}

export function validateExecutableBestMetricsFreshness(index, catalog, results) {
  if (Array.isArray(results)) {
    try {
      const expected = deriveExecutableBestMetrics(catalog, results);
      if (JSON.stringify(index) !== JSON.stringify(expected)) return ["executable best-metrics catalog is stale; regenerate it from validated local results."];
    } catch (error) {
      return [String(error?.message ?? error)];
    }
  }
  return validateExecutableBestMetrics(index, catalog);
}

export function loadExecutableDocuments(root = ROOT) {
  const read = (relativePath) => JSON.parse(fs.readFileSync(path.resolve(root, relativePath), "utf8"));
  return {
    catalog: read("benchmarks/executable-catalog.json"),
    schema: read("benchmarks/executable-benchmark.schema.json"),
  };
}

export function exactOutputDigest(value) {
  return "sha256:" + crypto.createHash("sha256").update(Buffer.from(value, "utf8")).digest("hex");
}
