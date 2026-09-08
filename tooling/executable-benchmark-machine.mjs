import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export const ROOT = path.resolve(import.meta.dir, "..");
export const EXECUTABLE_SCHEMA = "w-executable-benchmark/3";
export const EXECUTABLE_CATALOG_ID = "w-executable-benchmark-catalog";
export const EXECUTABLE_RESULT_SCHEMA = "w-executable-benchmark-result/3";
export const EXECUTABLE_BEST_SCHEMA = "w-executable-benchmark-best/3";
export const EXECUTABLE_LANGUAGES = Object.freeze(["w", "c", "rust"]);
export const EXECUTABLE_WORKLOAD_IDS = Object.freeze([
  "hello",
  "restaurant-branch",
  "restaurant-nested-branch",
  "bool-short-circuit",
  "restaurant-interpolation",
  "restaurant-composition",
]);
export const EXECUTABLE_METRICS = Object.freeze([
  { id: "compile-latency", unit: "nanoseconds", kind: "duration" },
  { id: "run-wall-time", unit: "nanoseconds", kind: "duration" },
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
  "cpu-time",
  "peak-working-set",
  "artifact-size",
]);
export const BEST_KNOWN_METRIC_ORDER = Object.freeze([...OPTIMIZABLE_METRICS]);
export const RESULT_HISTORY_PATH = "benchmarks/history/executables";
export const EXECUTABLE_HISTORY_SCHEMA = "w-executable-benchmark-history/1";
export const EXECUTABLE_HISTORY_INDEX_PATH = "benchmarks/history/executables/index.json";
export const BEST_KNOWN_PATH = "benchmarks/executable-best-known.json";
export const EXECUTABLE_PLATFORM_TARGET = "windows-x64";
export const EXECUTABLE_ARTIFACT_TARGET_MSVC = "x86_64-pc-windows-msvc";
export const EXECUTABLE_ARTIFACT_TARGET_MINGW = "x86_64-w64-mingw32";
export const ENVIRONMENT_FIELDS = Object.freeze(["os", "kernel", "cpuModel", "logicalCores", "ramBytes"]);
export const PROTOCOL_FIELDS = Object.freeze([
  "warmupMinimum", "rawMinimum", "rawParity", "arithmeticMeanRounding", "stopRule", "wallClock",
  "processIsolation", "order", "resourceScope", "knownNoiseControls",
  "unknownNoiseControls", "directProcessDisclosure",
]);
export const CATALOG_STATUS = "catalog-ready";
export const BEST_KNOWN_CONTRACT_STATUS = "defined";

const SOURCE_ELIGIBILITY = Object.freeze({
  wPrivate: Object.freeze({
    comparability: "contextual-non-ranking-until-public-run",
    eligibility: "contextual-only-until-public-run",
  }),
  wDeferred: Object.freeze({
    comparability: "deferred-until-M3b",
    eligibility: "deferred-to-M3b",
  }),
  c: Object.freeze({
    comparability: "contextual-non-ranking-across-abi",
    eligibility: "correctness-only-until-c23",
  }),
  rust: Object.freeze({
    comparability: "promotable-after-equivalence",
    eligibility: "promotable-after-equivalence",
  }),
});

const SOURCE_RECIPES = Object.freeze({
  w: Object.freeze(["private-native0-mlir0-source-to-pe-candidate", "public-w-run"]),
  c: Object.freeze(["gcc-c23-or-c2x"]),
  rust: Object.freeze(["rustc-edition-2024"]),
});

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
  if (source.status !== "source-oracle-ready") push(errors, location + ".status must be source-oracle-ready for a materialized source.");
  if (!MEASUREMENT_PROFILES.includes(source.profile) || source.profile !== "release") push(errors, location + ".profile must be release for M3a sources.");
  if (source.quality !== "correctness-gate") push(errors, location + ".quality must identify correctness as a gate.");
  if (source.platformTarget !== EXECUTABLE_PLATFORM_TARGET) push(errors, location + ".platformTarget must be " + EXECUTABLE_PLATFORM_TARGET + ".");
  const expectedArtifactTarget = source.language === "c" ? EXECUTABLE_ARTIFACT_TARGET_MINGW : EXECUTABLE_ARTIFACT_TARGET_MSVC;
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

function checkOracle(oracle, location, workloadStatus, errors) {
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
  const keys = ["schema", "status", "recordsPath", "requiredIdentity", "provenance", "sampling", "protocolFields", "environmentFields", "artifact", "metrics", "immutability"];
  if (!exactKeys(contract, name, keys, errors)) return;
  if (contract.schema !== EXECUTABLE_RESULT_SCHEMA || contract.status !== "contract-only") push(errors, name + " must remain a contract-only result schema.");
  if (contract.recordsPath !== RESULT_HISTORY_PATH) push(errors, name + ".recordsPath must identify the concrete immutable history path.");
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
  requiredString(contract.immutability, name + ".immutability", errors);
}

function checkBestKnownContract(contract, name, errors) {
  const keys = ["schema", "status", "path", "derivation", "optimizableMetrics", "forbiddenMetrics", "rule"];
  if (!exactKeys(contract, name, keys, errors)) return;
  if (contract.schema !== EXECUTABLE_BEST_SCHEMA || contract.status !== BEST_KNOWN_CONTRACT_STATUS) push(errors, name + " must declare the defined best-known contract.");
  if (contract.path !== BEST_KNOWN_PATH) push(errors, name + ".path must identify the concrete generated best-known index.");
  requiredString(contract.derivation, name + ".derivation", errors);
  if (JSON.stringify(contract.optimizableMetrics) !== JSON.stringify(OPTIMIZABLE_METRICS)) push(errors, name + ".optimizableMetrics must be the closed promotable set.");
  if (JSON.stringify(contract.forbiddenMetrics) !== JSON.stringify(["exit-code", "stdout", "stderr"])) push(errors, name + ".forbiddenMetrics must exclude correctness outputs.");
  requiredString(contract.rule, name + ".rule", errors);
}

export function validateExecutableCatalog(catalog, documents = undefined, root = ROOT) {
  const errors = [];
  const keys = ["$schema", "schema", "kind", "id", "status", "metrics", "comparabilityAxes", "workloads", "resultContract", "bestKnownContract"];
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
    if (!exactKeys(workload, location, ["id", "status", "sourceReadiness", "demoEvidence", "benchmarkStatus", "lane", "scope", "oracle", "sources", "blockedLanguages", "blockers"], errors)) continue;
    if (workloadIds.has(workload.id)) push(errors, location + ".id must be unique.");
    workloadIds.add(workload.id);
    if (workload.id !== EXECUTABLE_WORKLOAD_IDS[index]) push(errors, location + ".id is not in the stable catalog order.");
    if (!requiredString(workload.id, location + ".id", errors) || !requiredString(workload.scope, location + ".scope", errors)) continue;
    if (!["source-oracle-ready", "planned", "blocked"].includes(workload.status)) push(errors, location + ".status is invalid.");
    if (!["source-and-oracle-ready", "not-materialized"].includes(workload.sourceReadiness)) push(errors, location + ".sourceReadiness is invalid.");
    if (!["bounded-w-demo", "not-run"].includes(workload.demoEvidence)) push(errors, location + ".demoEvidence is invalid.");
    if (!["not-performance-ready", "deferred-to-M3b", "planned"].includes(workload.benchmarkStatus)) push(errors, location + ".benchmarkStatus is invalid.");
    if (workload.status === "source-oracle-ready" && workload.sourceReadiness !== "source-and-oracle-ready") push(errors, location + ".sourceReadiness must identify a source-backed oracle.");
    if (workload.status !== "source-oracle-ready" && workload.sourceReadiness !== "not-materialized") push(errors, location + ".sourceReadiness must remain not-materialized.");
    if (workload.status === "source-oracle-ready" && workload.benchmarkStatus === "planned") push(errors, location + ".benchmarkStatus must not be planned for a source-backed witness.");
    if (workload.status !== "source-oracle-ready" && workload.benchmarkStatus !== "planned") push(errors, location + ".benchmarkStatus must remain planned without a source-backed witness.");
    if (workload.lane !== "equivalent") push(errors, location + ".lane must be equivalent.");
    checkOracle(workload.oracle, location + ".oracle", workload.status, errors);
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
    stringArray(workload.blockers, location + ".blockers", errors, 1);
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
  checkContract(catalog.resultContract, "executable catalog.resultContract", errors);
  checkBestKnownContract(catalog.bestKnownContract, "executable catalog.bestKnownContract", errors);
  if (documents?.bestKnown && Object.prototype.hasOwnProperty.call(documents, "historyResults")) {
    errors.push(...validateExecutableBestKnownIndex(documents.bestKnown, catalog, documents.historyResults).map((error) => "best-known index: " + error));
  }
  return errors;
}

function workloadFor(catalog, id) {
  return Array.isArray(catalog?.workloads) ? catalog.workloads.find((item) => item?.id === id) : undefined;
}

function sourceFor(workload, language) {
  return Array.isArray(workload?.sources) ? workload.sources.find((item) => item.language === language) : undefined;
}

function sourcePolicy(workload, language, recipe) {
  if (language === "c") return SOURCE_ELIGIBILITY.c;
  if (language === "rust") return SOURCE_ELIGIBILITY.rust;
  return recipe === "private-native0-mlir0-source-to-pe-candidate"
    ? SOURCE_ELIGIBILITY.wPrivate
    : SOURCE_ELIGIBILITY.wDeferred;
}

function canonicalEquivalencePayload(workload, platformTarget, profile, recipeClass) {
  return {
    schema: EXECUTABLE_SCHEMA,
    workloadId: workload.id,
    lane: workload.lane,
    scope: workload.scope,
    oracle: workload.oracle,
    platformTarget,
    profile,
    recipeClass,
  };
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
  stringArray(protocol.knownNoiseControls, name + ".knownNoiseControls", errors, 1);
  stringArray(protocol.unknownNoiseControls, name + ".unknownNoiseControls", errors, 1);
  if (!requiredString(protocol.directProcessDisclosure, name + ".directProcessDisclosure", errors) ||
      !/Bun.*direct.*process|direct.*process.*Bun/iu.test(protocol.directProcessDisclosure) ||
      !/process[- ]tree.*aggregat|aggregat.*process[- ]tree/iu.test(protocol.directProcessDisclosure)) {
    push(errors, name + ".directProcessDisclosure must state Bun direct-process limits and the process-tree aggregation gap.");
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

export function validateExecutableResult(result, catalog = loadExecutableDocuments().catalog) {
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
  if (result.language === "c" && result.artifactTarget !== EXECUTABLE_ARTIFACT_TARGET_MINGW) push(errors, "C executable results must use the x86_64-w64-mingw32 artifact target.");
  if (result.language !== "c" && result.artifactTarget !== EXECUTABLE_ARTIFACT_TARGET_MSVC) push(errors, "W and Rust executable results must use the x86_64-pc-windows-msvc artifact target.");
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
    if (source && (result.identity.recipe !== source.recipe || result.identity.recipeClass !== source.recipeClass || result.identity.eligibility !== source.eligibility || result.identity.platformTarget !== source.platformTarget || result.identity.artifactTarget !== source.artifactTarget)) push(errors, "executable result identity must match catalog target, recipe, recipe class and eligibility.");
  }
  if (exactKeys(result.correctness, "executable result.correctness", ["oracleId", "exitCode", "stdoutDigest", "stderrDigest"], errors)) {
    requiredString(result.correctness.oracleId, "executable result.correctness.oracleId", errors);
    if (workload && result.correctness.oracleId !== `${workload.id}:exact-output`) push(errors, "executable result.correctness.oracleId must identify the workload exact-output oracle.");
    if (!Number.isSafeInteger(result.correctness.exitCode) || result.correctness.exitCode < 0) push(errors, "executable result.correctness.exitCode must be a non-negative safe integer.");
    digest(result.correctness.stdoutDigest, "executable result.correctness.stdoutDigest", errors);
    digest(result.correctness.stderrDigest, "executable result.correctness.stderrDigest", errors);
    if (workload?.oracle?.status === "source-backed" && (result.correctness.exitCode !== workload.oracle.exitCode || result.correctness.stdoutDigest !== exactOutputDigest(workload.oracle.stdout) || result.correctness.stderrDigest !== exactOutputDigest(workload.oracle.stderr))) push(errors, "executable result.correctness must match the exact-output oracle.");
  }
  if (exactKeys(result.artifact, "executable result.artifact", ["digest", "sizeBytes"], errors)) {
    digest(result.artifact.digest, "executable result.artifact.digest", errors);
    positiveDecimal(result.artifact.sizeBytes, "executable result.artifact.sizeBytes", errors);
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

function resultValue(item) {
  return item?.record ?? item;
}

function historyReferenceSort(left, right) {
  return compareText(String(left?.id ?? ""), String(right?.id ?? "")) || compareText(String(left?.path ?? ""), String(right?.path ?? ""));
}

export function loadExecutableHistoryResults(history, root = ROOT) {
  if (!isObject(history) || !Array.isArray(history.records)) throw new TypeError("executable history records are required");
  const historyRoot = path.resolve(root, RESULT_HISTORY_PATH);
  return history.records.map((reference) => {
    const physical = path.resolve(historyRoot, reference.path);
    if (!isContained(historyRoot, physical)) throw new Error("executable history record path escapes its directory");
    const bytes = fs.readFileSync(physical);
    const actual = "sha256:" + crypto.createHash("sha256").update(bytes).digest("hex");
    if (actual !== reference.digest) throw new Error(`executable history record digest is stale: ${reference.path}`);
    let record;
    try {
      record = JSON.parse(bytes.toString("utf8"));
    } catch {
      throw new Error(`executable history record is not valid JSON: ${reference.path}`);
    }
    return { reference, record, bytes };
  });
}

const BEST_KNOWN_PROVENANCE_FIELDS = Object.freeze([
  "sourceDigest", "runnerDigest", "toolchainDigest", "catalogDigest",
]);

function bestKnownGroupIdentity(record) {
  return {
    workloadId: record.workloadId,
    language: record.language,
    platformTarget: record.platformTarget,
    artifactTarget: record.artifactTarget,
    profile: record.profile,
    equivalenceKey: record.equivalenceKey,
    toolchain: record.identity.toolchain,
    host: record.identity.host,
    recipe: record.identity.recipe,
    recipeClass: record.identity.recipeClass,
    recipeDigest: record.identity.recipeDigest,
    eligibility: record.identity.eligibility,
    provenance: Object.fromEntries(BEST_KNOWN_PROVENANCE_FIELDS.map((field) => [field, record.provenance[field]])),
    protocol: record.protocol,
  };
}

function bestKnownGroupKey(record) {
  return JSON.stringify(bestKnownGroupIdentity(record));
}

function bestKnownMetricValue(record, metric) {
  if (metric === "compile-latency") return record.compile.summary.wallNs.median;
  if (metric === "run-wall-time") return record.run.summary.wallNs.median;
  if (metric === "cpu-time") return record.run.summary.cpuTotalUs.median;
  if (metric === "peak-working-set") return record.run.summary.peakRssBytes.median;
  if (metric === "artifact-size") return record.artifact.sizeBytes;
  return undefined;
}

function bestKnownStatistic(metric) {
  return metric === "artifact-size" ? "single-artifact" : "median";
}

function bestKnownMetricIsEligible(record, metric) {
  // A zero run CPU median is a valid microsecond-resolution observation, but
  // it cannot establish a useful CPU best-known value.
  return metric !== "cpu-time" || record.run.summary.cpuTotalUs.median !== "0";
}

function bestKnownRecordId(groupKey, metric) {
  const digestHex = crypto.createHash("sha256").update(`${groupKey}\u0000${metric}`, "utf8").digest("hex");
  return `best-${digestHex}`;
}

function compareResultForTie(left, right) {
  return compareText(String(left?.id ?? ""), String(right?.id ?? "")) ||
    compareText(String(left?.artifact?.digest ?? ""), String(right?.artifact?.digest ?? "")) ||
    compareText(String(left?.provenance?.commit ?? ""), String(right?.provenance?.commit ?? ""));
}

export function deriveExecutableBestKnown(catalog, results) {
  if (!Array.isArray(results)) throw new TypeError("validated executable results are required");
  const groups = new Map();
  const ids = new Set();
  for (const item of results) {
    const record = resultValue(item);
    const errors = validateExecutableResult(record, catalog);
    if (errors.length > 0) throw new Error(errors.join("; "));
    if (ids.has(record.id)) throw new Error(`executable results contain duplicate id: ${record.id}`);
    ids.add(record.id);
    const workload = workloadFor(catalog, record.workloadId);
    const source = sourceFor(workload, record.language);
    if (source?.comparability !== "promotable-after-equivalence" || source?.eligibility !== "promotable-after-equivalence") continue;
    const key = bestKnownGroupKey(record);
    const group = groups.get(key) ?? { key, records: [] };
    group.records.push(record);
    groups.set(key, group);
  }

  const derived = [];
  for (const group of [...groups.values()].sort((left, right) => compareText(left.key, right.key))) {
    const ordered = [...group.records].sort(compareResultForTie);
    for (const metric of BEST_KNOWN_METRIC_ORDER) {
      const values = ordered
        .filter((record) => bestKnownMetricIsEligible(record, metric))
        .map((record) => ({ record, value: BigInt(bestKnownMetricValue(record, metric)) }));
      if (values.length === 0) continue;
      const minimum = values.reduce((best, item) => item.value < best ? item.value : best, values[0].value);
      const selected = values.filter((item) => item.value === minimum).map((item) => item.record).sort(compareResultForTie);
      const primary = selected[0];
      derived.push({
        $schema: "./executable-benchmark.schema.json",
        schema: EXECUTABLE_BEST_SCHEMA,
        kind: "executable-best-known",
        id: bestKnownRecordId(group.key, metric),
        status: "derived",
        workloadId: primary.workloadId,
        metric,
        language: primary.language,
        platformTarget: primary.platformTarget,
        artifactTarget: primary.artifactTarget,
        profile: primary.profile,
        statistic: bestKnownStatistic(metric),
        equivalenceKey: primary.equivalenceKey,
        toolchain: primary.identity.toolchain,
        host: primary.identity.host,
        recipe: primary.identity.recipe,
        recipeClass: primary.identity.recipeClass,
        recipeDigest: primary.identity.recipeDigest,
        value: minimum.toString(10),
        derivedFrom: selected.map((record) => record.id),
      });
    }
  }
  derived.sort((left, right) => compareText(left.id, right.id));
  return {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_BEST_SCHEMA,
    kind: "executable-best-known-index",
    status: derived.length === 0 ? "not-established" : "established",
    records: derived,
  };
}

export function validateExecutableBestKnown(record, catalog = loadExecutableDocuments().catalog, results = []) {
  const errors = [];
  const keys = ["$schema", "schema", "kind", "id", "status", "workloadId", "metric", "language", "platformTarget", "artifactTarget", "profile", "statistic", "equivalenceKey", "toolchain", "host", "recipe", "recipeClass", "recipeDigest", "value", "derivedFrom"];
  if (!exactKeys(record, "executable best-known record", keys, errors)) return errors;
  if (record.$schema !== "./executable-benchmark.schema.json" || record.schema !== EXECUTABLE_BEST_SCHEMA || record.kind !== "executable-best-known" || record.status !== "derived") push(errors, "executable best-known identity or status is invalid.");
  requiredString(record.id, "executable best-known record.id", errors);
  const workload = workloadFor(catalog, record.workloadId);
  const source = sourceFor(workload, record.language);
  if (!workload || workload.status !== "source-oracle-ready") push(errors, "executable best-known record must identify a ready workload.");
  if (!OPTIMIZABLE_METRICS.includes(record.metric)) push(errors, "executable best-known record.metric must be optimizable, never exit-code/stdout/stderr.");
  if (!EXECUTABLE_LANGUAGES.includes(record.language) || !source) push(errors, "executable best-known record.language must identify a materialized source.");
  if (record.platformTarget !== EXECUTABLE_PLATFORM_TARGET) push(errors, "executable best-known record.platformTarget must be " + EXECUTABLE_PLATFORM_TARGET + ".");
  if (source && record.artifactTarget !== source.artifactTarget) push(errors, "executable best-known record.artifactTarget must match the source ABI target.");
  if (record.language === "c" && record.artifactTarget !== EXECUTABLE_ARTIFACT_TARGET_MINGW) push(errors, "C best-known records must use the x86_64-w64-mingw32 artifact target.");
  if (record.language !== "c" && record.artifactTarget !== EXECUTABLE_ARTIFACT_TARGET_MSVC) push(errors, "W and Rust best-known records must use the x86_64-pc-windows-msvc artifact target.");
  if (record.profile !== "release") push(errors, "executable best-known record.profile must be release.");
  const expectedStatistic = record.metric === "artifact-size" ? "single-artifact" : "median";
  if (record.statistic !== expectedStatistic) push(errors, "executable best-known record.statistic must be " + expectedStatistic + ".");
  digest(record.equivalenceKey, "executable best-known record.equivalenceKey", errors);
  checkSafeIdentityString(record.toolchain, "executable best-known record.toolchain", errors);
  checkSafeIdentityString(record.host, "executable best-known record.host", errors);
  requiredString(record.recipe, "executable best-known record.recipe", errors);
  requiredString(record.recipeClass, "executable best-known record.recipeClass", errors);
  digest(record.recipeDigest, "executable best-known record.recipeDigest", errors);
  decimal(record.value, "executable best-known record.value", errors);
  if (record.metric === "cpu-time" && record.value === "0") {
    push(errors, "executable best-known cpu-time cannot promote a zero microsecond run median.");
  }
  stringArray(record.derivedFrom, "executable best-known record.derivedFrom", errors, 1);
  if (Array.isArray(record.derivedFrom) && !isCanonicalOrder(record.derivedFrom, (items) => [...items].sort(compareText))) {
    push(errors, "executable best-known record.derivedFrom must be sorted by result id.");
  }
  if (!Array.isArray(results) || results.length === 0) {
    push(errors, "executable best-known record must be derived from validated result records.");
  } else {
    const derivedFrom = Array.isArray(record.derivedFrom) ? record.derivedFrom : [];
    const normalizedResults = results.map(resultValue);
    const selected = normalizedResults.filter((item) => derivedFrom.includes(item?.id));
    if (selected.length !== derivedFrom.length) push(errors, "executable best-known record.derivedFrom must reference supplied results exactly.");
    if (selected.some((item) => validateExecutableResult(item, catalog).length > 0)) push(errors, "executable best-known record derives from an invalid result.");
    const expectedKey = source ? executableEquivalenceKey(catalog, record.workloadId, record.platformTarget, record.profile, source.recipeClass) : undefined;
    if (record.equivalenceKey !== expectedKey) push(errors, "executable best-known record.equivalenceKey must be recomputed from the catalog.");
    if (source?.comparability !== "promotable-after-equivalence" || source?.eligibility !== "promotable-after-equivalence") push(errors, "executable best-known record cannot promote an ineligible or non-comparable source variant.");
    const selectedGroup = selected[0] ? bestKnownGroupIdentity(selected[0]) : undefined;
    for (const item of selected) {
      if (item.workloadId !== record.workloadId || item.language !== record.language || item.platformTarget !== record.platformTarget || item.artifactTarget !== record.artifactTarget || item.profile !== record.profile || item.equivalenceKey !== record.equivalenceKey || item.identity?.toolchain !== record.toolchain || item.identity?.host !== record.host || item.identity?.recipe !== record.recipe || item.identity?.recipeClass !== record.recipeClass || item.identity?.recipeDigest !== record.recipeDigest || item.identity?.eligibility !== "promotable-after-equivalence" || JSON.stringify(bestKnownGroupIdentity(item)) !== JSON.stringify(selectedGroup)) push(errors, "executable best-known record mixes incomparable identity, artifact target, toolchain, host, recipe or provenance.");
    }
    const stage = record.metric === "compile-latency" ? "compile" : record.metric === "artifact-size" ? "artifact" : "run";
    const metricPath = optimizableMetricField(record.metric, stage);
    const values = selected.map((item) => metricPath?.[0] === "artifact" ? item.artifact?.sizeBytes : metricPath ? item[metricPath[0]]?.summary?.[metricPath[1]]?.median : undefined).filter((value) => typeof value === "string" && DECIMAL_PATTERN.test(value));
    if (values.length !== selected.length || values.length === 0) {
      push(errors, "executable best-known record metric values must be complete validated summaries.");
    } else {
      if (record.metric === "cpu-time" && values.some((value) => value === "0")) {
        push(errors, "executable best-known cpu-time cannot derive from a zero microsecond run median.");
      }
      const minimum = values.reduce((best, value) => BigInt(value) < BigInt(best) ? value : best);
      if (record.value !== minimum) push(errors, "executable best-known record.value must be the derived minimum summary.");
    }
  }
  return errors;
}

export function validateExecutableBestKnownIndex(index, catalog = loadExecutableDocuments().catalog, results = []) {
  const errors = [];
  if (!exactKeys(index, "executable best-known index", ["$schema", "schema", "kind", "status", "records"], errors)) return errors;
  if (index.$schema !== "./executable-benchmark.schema.json" || index.schema !== EXECUTABLE_BEST_SCHEMA || index.kind !== "executable-best-known-index") {
    push(errors, "executable best-known index identity is invalid.");
  }
  if (!["not-established", "established"].includes(index.status)) push(errors, "executable best-known index.status must be not-established or established.");
  if (!Array.isArray(index.records)) {
    push(errors, "executable best-known index.records must be an array.");
  } else {
    if (index.records.length === 0 && index.status !== "not-established") push(errors, "executable best-known index with no records must be not-established.");
    if (index.records.length > 0 && index.status !== "established") push(errors, "executable best-known index with non-empty records must be established.");
    const ids = new Set();
    for (const record of index.records) {
      if (ids.has(record?.id)) push(errors, "executable best-known index record ids must be unique.");
      ids.add(record?.id);
    }
    if (!isCanonicalOrder(index.records, (items) => [...items].sort((left, right) => compareText(String(left?.id ?? ""), String(right?.id ?? ""))))) {
      push(errors, "executable best-known index.records must be sorted by id.");
    }
    for (const [number, record] of index.records.entries()) errors.push(...validateExecutableBestKnown(record, catalog, results).map((error) => "records[" + number + "]: " + error));
  }
  return errors;
}

export function validateExecutableBestKnownFreshness(index, catalog, results) {
  try {
    const expected = deriveExecutableBestKnown(catalog, results);
    if (JSON.stringify(index) !== JSON.stringify(expected)) return ["executable best-known index is stale; regenerate it from immutable history."];
  } catch (error) {
    return [String(error?.message ?? error)];
  }
  return [];
}

export function validateExecutableHistory(index, catalog = loadExecutableDocuments().catalog, root = ROOT) {
  const errors = [];
  const keys = ["$schema", "schema", "kind", "status", "recordsPath", "records"];
  if (!exactKeys(index, "executable history index", keys, errors)) return errors;
  if (index.$schema !== "../../executable-benchmark.schema.json" || index.schema !== EXECUTABLE_HISTORY_SCHEMA || index.kind !== "executable-history-index") push(errors, "executable history index identity is invalid.");
  if (!["empty-awaiting-clean-head-record", "recorded"].includes(index.status)) push(errors, "executable history index.status is invalid.");
  if (index.recordsPath !== RESULT_HISTORY_PATH) push(errors, "executable history index.recordsPath must identify the immutable history directory.");
  if (!Array.isArray(index.records)) {
    push(errors, "executable history index.records must be an array.");
    return errors;
  }
  if (index.records.length === 0 && index.status !== "empty-awaiting-clean-head-record") push(errors, "empty executable history must await a clean-head record.");
  if (index.records.length > 0 && index.status !== "recorded") push(errors, "non-empty executable history must be recorded.");
  if (!isCanonicalOrder(index.records, (items) => [...items].sort(historyReferenceSort))) push(errors, "executable history index.records must be sorted by id and path.");
  const historyRoot = path.resolve(root, RESULT_HISTORY_PATH);
  const ids = new Set();
  const paths = new Set();
  for (const [number, reference] of index.records.entries()) {
    const location = "executable history index.records[" + number + "]";
    if (!exactKeys(reference, location, ["id", "path", "digest"], errors)) continue;
    requiredString(reference.id, location + ".id", errors);
    if (ids.has(reference.id)) push(errors, location + ".id must be unique.");
    ids.add(reference.id);
    if (typeof reference.path !== "string" || !/^[0-9a-f]{64}\.json$/u.test(reference.path)) {
      push(errors, location + ".path must be the lowercase sha256 hex digest filename.");
      continue;
    }
    if (digest(reference.digest, location + ".digest", errors) && reference.path !== reference.digest.slice("sha256:".length) + ".json") {
      push(errors, location + ".path must match its sha256 digest filename.");
    }
    const physical = path.resolve(historyRoot, reference.path);
    if (!isContained(historyRoot, physical)) {
      push(errors, location + ".path escapes immutable history.");
      continue;
    }
    if (paths.has(physical)) push(errors, location + ".path must be unique.");
    paths.add(physical);
    if (!digest(reference.digest, location + ".digest", errors)) continue;
    let bytes;
    try {
      const stats = fs.lstatSync(physical);
      if (!stats.isFile() || stats.isSymbolicLink()) throw new Error();
      bytes = fs.readFileSync(physical);
    } catch {
      push(errors, location + ".path must identify an existing regular history record.");
      continue;
    }
    if (reference.digest !== "sha256:" + crypto.createHash("sha256").update(bytes).digest("hex")) push(errors, location + ".digest is stale.");
    let record;
    try { record = JSON.parse(bytes.toString("utf8")); } catch { push(errors, location + ".path must contain valid JSON."); continue; }
    errors.push(...validateExecutableResult(record, catalog).map((error) => location + ": " + error));
    if (record?.id !== reference.id) push(errors, location + ".id must match the immutable result record.");
  }
  let entries;
  try {
    entries = fs.readdirSync(historyRoot, { withFileTypes: true });
  } catch {
    push(errors, "executable history directory must be readable.");
    return errors;
  }
  const indexedNames = new Set([...paths].map((physical) => path.basename(physical)));
  for (const entry of entries) {
    if (!entry.isFile() || entry.isSymbolicLink()) {
      push(errors, "executable history contains a non-regular or symbolic-link entry: " + entry.name + ".");
      continue;
    }
    if (entry.name === "index.json" || entry.name === "README.md") continue;
    if (!indexedNames.has(entry.name)) push(errors, "executable history contains an unindexed entry: " + entry.name + ".");
  }
  return errors;
}

export function loadExecutableDocuments(root = ROOT) {
  const read = (relativePath) => JSON.parse(fs.readFileSync(path.resolve(root, relativePath), "utf8"));
  return {
    catalog: read("benchmarks/executable-catalog.json"),
    schema: read("benchmarks/executable-benchmark.schema.json"),
    bestKnown: read(BEST_KNOWN_PATH),
    history: read(EXECUTABLE_HISTORY_INDEX_PATH),
  };
}

export function exactOutputDigest(value) {
  return "sha256:" + crypto.createHash("sha256").update(Buffer.from(value, "utf8")).digest("hex");
}
