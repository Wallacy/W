import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import { deriveSummary } from "./executable-benchmark-runner.mjs";
import {
  EXECUTABLE_ARTIFACT_TARGET_MINGW,
  EXECUTABLE_ARTIFACT_TARGET_LINUX,
  EXECUTABLE_ARTIFACT_TARGET_MSVC,
  EXECUTABLE_BEST_SCHEMA,
  EXECUTABLE_COMPARABILITY_AXES,
  EXECUTABLE_LANGUAGES,
  EXECUTABLE_RESULT_SCHEMA,
  EXECUTABLE_PLATFORM_TARGET,
  EXECUTABLE_PLATFORM_TARGET_LINUX,
  EXECUTABLE_PLATFORM_TARGET_LINUX_WSL,
  EXECUTABLE_STRUCTURE_CLASSES,
  PROCESS_ENTRY_CORRECTNESS_INPUTS,
  PROCESS_ENTRY_ORACLE_CASES,
  PROCESS_ENTRY_ORACLE_KIND,
  PROCESS_ENTRY_RECIPE_CLASS,
  PROCESS_ENTRY_TIMED_INPUT,
  PROCESS_ENTRY_WORKLOAD_ID,
  PROCESS_ENUM_PAYLOAD_CORRECTNESS_INPUTS,
  PROCESS_ENUM_PAYLOAD_ORACLE_CASES,
  PROCESS_ENUM_PAYLOAD_ORACLE_KIND,
  PROCESS_ENUM_PAYLOAD_RECIPE_CLASS,
  PROCESS_ENUM_PAYLOAD_TIMED_INPUT,
  PROCESS_ENUM_PAYLOAD_WORKLOAD_ID,
  PROCESS_ARGUMENTS_ORDERING_CORRECTNESS_INPUTS,
  PROCESS_ARGUMENTS_ORDERING_ORACLE_CASES,
  PROCESS_ARGUMENTS_ORDERING_ORACLE_KIND,
  PROCESS_ARGUMENTS_ORDERING_RECIPE_CLASS,
  PROCESS_ARGUMENTS_ORDERING_TIMED_INPUT,
  PROCESS_ARGUMENTS_ORDERING_WORKLOAD_ID,
  PROCESS_ENTRY0_CORRECTNESS_INPUTS,
  PROCESS_ENTRY0_EXECUTION_KIND,
  PROCESS_ENTRY0_FAULT_CASES,
  PROCESS_ENTRY0_RECIPE,
  PROCESS_ENTRY0_RECIPE_CLASS,
  PROCESS_ENTRY0_SUPPORT_ROLES,
  PROCESS_ENTRY0_TIMED_INPUT,
  PROCESS_HANDLER_LIFECYCLE_EXECUTION_STRUCTURE_CLASS,
  PROCESS_HANDLER_LIFECYCLE_STRUCTURE_CLASS,
  PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID,
  RESTAURANT_F64_STRICT_WORKLOAD_ID,
  RESTAURANT_UINT_ARITHMETIC_WORKLOAD_ID,
  RESTAURANT_UINT_BIT_NOT_WORKLOAD_ID,
  RESTAURANT_UINT_COMPOUND_WORKLOAD_ID,
  RESTAURANT_UINT_WRAPPING_ADD_WORKLOAD_ID,
  RESTAURANT_UINT_WRAPPING_MULTIPLY_WORKLOAD_ID,
  RESTAURANT_UINT_WRAPPING_NEGATE_WORKLOAD_ID,
  RESTAURANT_UINT_WRAPPING_POWER_WORKLOAD_ID,
  RESTAURANT_UINT_WRAPPING_SUBTRACT_WORKLOAD_ID,
  ROOT,
  deriveExecutableBestMetrics,
  executableEquivalenceKey,
  executableHostEvidenceForPlatform,
  executableHostIdentity,
  executableNativeHostForPlatform,
  executableSourceDigest,
  exactOutputDigest,
  loadExecutableDocuments,
  pruneExecutableBestMetrics,
  updateExecutableBestMetrics,
  validateExecutableBestMetric,
  validateExecutableBestMetrics,
  validateExecutableCatalog,
  validateExecutableResult,
} from "./executable-benchmark-machine.mjs";

const documents = loadExecutableDocuments();
const clone = (value) => structuredClone(value);
const digest = "sha256:1111111111111111111111111111111111111111111111111111111111111111";
const VALID_PE_LAYOUT = {
  fileAlignment: "512",
  sectionAlignment: "4096",
  sizeOfHeaders: "512",
  sections: [{ name: ".text", virtualSize: "384", rawSize: "512" }],
};
const VALID_ELF_LAYOUT = {
  class: "ELF64",
  data: "little-endian",
  machine: "x86-64",
  type: "pie",
  sections: [
    { name: ".text", sizeBytes: "384" },
    { name: ".rodata", sizeBytes: "128" },
  ],
};

test("catalog stores compact live best cells and no immutable history", () => {
  assert.deepEqual(validateExecutableCatalog(documents.catalog, documents), []);
  assert.equal(documents.schema.$id, "w-executable-benchmark/6");
  assert.deepEqual(documents.schema.oneOf.map((entry) => entry.$ref), [
    "#/$defs/catalog", "#/$defs/result", "#/$defs/bestMetric", "#/$defs/bestMetrics",
  ]);
  assert.deepEqual(documents.schema.$defs.structureClass.enum, EXECUTABLE_STRUCTURE_CLASSES);
  assert.deepEqual(documents.schema.$defs.source.properties.comparability.enum,
    ["deferred-until-M3b", "promotable-after-equivalence", "contextual-non-ranking-private-composite", "same-physical-hardware-diagnostic-only"]);
  assert.deepEqual(documents.schema.$defs.source.properties.eligibility.enum,
    ["promotable-after-equivalence", "deferred-to-M3b", "exploratory-private-composite", "same-physical-hardware-diagnostic-only"]);
  for (const definition of ["catalog", "result", "bestMetric", "bestMetrics", "bestMetricProvenance", "sample", "sampleSeries", "processExecution", "processSupportSource", "sourceSupport", "elfLayout", "elfSection"]) {
    assert.equal(documents.schema.$defs[definition].additionalProperties, false);
  }
  assert.deepEqual(documents.catalog.comparabilityAxes, EXECUTABLE_COMPARABILITY_AXES);
  assert.deepEqual(documents.catalog.platformLanes.map((lane) => lane.id), [
    "windows-x64", "linux-x64", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL,
  ]);
  assert.deepEqual(documents.catalog.platformLanes.find((lane) => lane.id === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL), {
    id: EXECUTABLE_PLATFORM_TARGET_LINUX_WSL,
    hostMode: "wsl2",
    artifactTargets: [EXECUTABLE_ARTIFACT_TARGET_LINUX],
    regression: true,
    rankability: "same-host-only",
    crossPlatformDiagnostics: "same-physical-hardware-only",
    description: "Linux x64 executable evidence through WSL2; not native Linux and not rankable across hosts.",
  });
  assert.equal(documents.catalog.resultContract.recordsPath, "benchmarks/results");
  assert.equal(documents.catalog.bestMetricsContract.schema, EXECUTABLE_BEST_SCHEMA);
  const metricsByCell = Object.fromEntries(
    Object.entries(Object.groupBy(
      documents.catalog.bestMetrics.entries,
      (entry) => `${entry.workloadId}/${entry.language}/${entry.platformTarget}`,
    )).map(([cell, entries]) => [cell, entries.map((entry) => entry.metric).sort()]),
  );
  const retainedMetrics = ["artifact-size", "compile-latency"];
  const completeRuntimeMetrics = [...retainedMetrics, "cpu-time", "peak-working-set",
    "run-wall-p95", "run-wall-time"].sort();
  const declaredCells = new Set(documents.catalog.workloads.flatMap((workload) =>
    workload.sources.map((source) =>
      `${workload.id}/${source.language}/${source.platformTarget}`)));
  assert.ok(Object.keys(metricsByCell).every((cell) => declaredCells.has(cell)),
    "every live metric cell must still have a current workload source");
  for (const requiredCell of ["hello/c/windows-x64", "hello/rust/windows-x64",
    "hello/w/windows-x64", "restaurant-enum-switch/c/windows-x64",
    "restaurant-enum-switch/rust/windows-x64",
    "restaurant-enum-switch/w/windows-x64", "restaurant-wmo/c/windows-x64",
    "restaurant-wmo/rust/windows-x64", "restaurant-wmo/w/windows-x64",
    "restaurant-main-dispatch/w/linux-wsl-x64"]) {
    assert.ok(metricsByCell[requiredCell], `${requiredCell} must retain live evidence`);
  }
  for (const workloadId of ["hello", "process-entry", "restaurant-branch",
    "restaurant-enum-switch", "restaurant-while", "restaurant-wmo"]) {
    const workload = documents.catalog.workloads.find((item) => item.id === workloadId);
    assert.equal(workload.benchmarkStatus, "exploratory-ready");
    assert.deepEqual(workload.blockers, []);
    assert.ok(workload.sources.filter((source) =>
      source.platformTarget === EXECUTABLE_PLATFORM_TARGET).every((source) =>
      source.comparability === "promotable-after-equivalence" &&
      source.eligibility === "promotable-after-equivalence"));
  }
  const helloWsl = documents.catalog.workloads.find((item) => item.id === "hello")
    .sources.find((source) => source.language === "w" &&
      source.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL);
  assert.equal(helloWsl.comparability, "same-physical-hardware-diagnostic-only");
  assert.equal(helloWsl.eligibility, "same-physical-hardware-diagnostic-only");
  for (const [cell, metrics] of Object.entries(metricsByCell)) {
    assert.ok(
      JSON.stringify(metrics) === JSON.stringify(retainedMetrics) ||
      JSON.stringify(metrics) === JSON.stringify(completeRuntimeMetrics),
      `${cell} must retain compile/artifact facts alone or one complete platform runtime set`,
    );
  }
  assert.ok(documents.catalog.bestMetrics.entries.every((entry) =>
    ["historical-unverified", "verified-clean"].includes(entry.provenance.artifactCleanliness)));
  assert.ok(documents.catalog.bestMetrics.entries
    .filter((entry) => entry.workloadId === "restaurant-enum-switch")
    .every((entry) => entry.provenance.artifactCleanliness === "verified-clean"));
  assert.ok(documents.catalog.bestMetrics.entries.every((entry) => entry.value !== "0"));
  assert.ok(new Set(documents.catalog.bestMetrics.entries.map((entry) => entry.language)).size === 3);
  assert.ok(documents.catalog.bestMetrics.entries.some((entry) => entry.language === "rust" && entry.eligibility === "promotable-after-equivalence"));
});

test("every public Windows runnable fixture has an executable benchmark owner", () => {
  const missing = clone(documents.catalog);
  const workload = missing.workloads.find((item) => item.id === "restaurant-enum-switch");
  workload.sources = workload.sources.filter((source) => source.language !== "w");
  workload.blockedLanguages.push("w");
  assert.match(validateExecutableCatalog(missing, { ...documents, catalog: missing }).join("\n"),
    /restaurant-enum\.w has no executable benchmark owner/u);
});

test("local module graph identity covers the complete source set", () => {
  const workload = documents.catalog.workloads.find((item) => item.id === "local-module-graph");
  assert.ok(workload);
  assert.equal(workload.benchmarkStatus, "partial-exploratory-ready");
  assert.deepEqual(workload.blockedLanguages, ["c", "rust"]);
  const source = workload.sources[0];
  assert.equal(source.language, "w");
  assert.equal(source.supportSources.length, 1);
  assert.notEqual(executableSourceDigest(source), source.digest);

  const changed = clone(source);
  changed.supportSources[0].digest = digest;
  assert.notEqual(executableSourceDigest(changed), executableSourceDigest(source));
  const stale = clone(documents.catalog);
  stale.workloads.find((item) => item.id === "local-module-graph")
    .sources[0].supportSources[0].digest = digest;
  assert.match(validateExecutableCatalog(stale, { ...documents, catalog: stale }).join("\n"),
    /supportSources\[0\]\.digest is stale/u);
});

test("process-handler-lifecycle catalog pins the private composite execution witness", () => {
  const workload = documents.catalog.workloads.find((item) => item.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID);
  assert.ok(workload);
  assert.deepEqual(EXECUTABLE_STRUCTURE_CLASSES, ["public-end-to-end", "integration-linkage", "transient-internal"]);
  assert.equal(workload.structureClass, PROCESS_HANDLER_LIFECYCLE_STRUCTURE_CLASS);
  assert.equal(workload.execution.structureClass, PROCESS_HANDLER_LIFECYCLE_EXECUTION_STRUCTURE_CLASS);
  assert.equal(workload.execution.kind, PROCESS_ENTRY0_EXECUTION_KIND);
  assert.equal(workload.execution.recipeClass, PROCESS_ENTRY0_RECIPE_CLASS);
  assert.deepEqual(workload.execution.timedInput, PROCESS_ENTRY0_TIMED_INPUT);
  assert.deepEqual(workload.execution.correctnessInputs, PROCESS_ENTRY0_CORRECTNESS_INPUTS);
  assert.deepEqual(workload.execution.faultCases, PROCESS_ENTRY0_FAULT_CASES);
  assert.ok(documents.schema.$defs.workload.properties.benchmarkStatus.enum.includes("exploratory-ready"));
  const exploratory = clone(documents.catalog);
  exploratory.workloads.find((item) => item.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID).benchmarkStatus = "exploratory-ready";
  exploratory.workloads.find((item) => item.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID).blockers = [];
  assert.deepEqual(validateExecutableCatalog(exploratory, exploratory), []);
  exploratory.workloads.find((item) => item.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID).blockers = ["private-handler-benchmark-runner"];
  assert.match(validateExecutableCatalog(exploratory, exploratory).join("\n"), /active measurement blockers/u);
  assert.deepEqual(workload.sources.map((source) => source.language), EXECUTABLE_LANGUAGES);
  assert.ok(workload.sources.every((source) => source.recipeClass === PROCESS_ENTRY0_RECIPE_CLASS));
  assert.ok(workload.sources.every((source) => source.artifactTarget === EXECUTABLE_ARTIFACT_TARGET_MINGW));
  assert.ok(workload.sources.every((source) => source.recipe !== PROCESS_ENTRY0_RECIPE || source.language === "w"));
  const changedInput = clone(documents.catalog);
  changedInput.workloads.find((item) => item.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID).execution.timedInput[0] = "beta";
  assert.notEqual(
    executableEquivalenceKey(documents.catalog, PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID, EXECUTABLE_PLATFORM_TARGET, "release", PROCESS_ENTRY0_RECIPE_CLASS),
    executableEquivalenceKey(changedInput, PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID, EXECUTABLE_PLATFORM_TARGET, "release", PROCESS_ENTRY0_RECIPE_CLASS),
  );
  const staleSupport = clone(documents.catalog);
  staleSupport.workloads.find((item) => item.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID).execution.supportSources[0].digest = digest;
  assert.match(validateExecutableCatalog(staleSupport, staleSupport).join("\n"), /supportSources\[0\]\.digest is stale/);
  const wrongHeader = clone(documents.catalog);
  const supportSources = wrongHeader.workloads.find((item) => item.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID).execution.supportSources;
  supportSources[2].path = supportSources[1].path;
  supportSources[2].digest = supportSources[1].digest;
  assert.match(validateExecutableCatalog(wrongHeader, wrongHeader).join("\n"), /provider header consumed/u);
  const privateRecipe = clone(documents.catalog);
  privateRecipe.workloads.find((item) => item.id === "hello").sources[0].recipe = PROCESS_ENTRY0_RECIPE;
  assert.match(validateExecutableCatalog(privateRecipe, privateRecipe).join("\n"), /private to process-handler-lifecycle/);
  assert.deepEqual(PROCESS_ENTRY0_SUPPORT_ROLES, ["harness-c", "provider-c", "provider-header"]);
});

test("process-entry catalog pins the public argument-dependent contract", () => {
  const workload = documents.catalog.workloads.find((item) => item.id === PROCESS_ENTRY_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.benchmarkStatus, "exploratory-ready");
  assert.equal(workload.oracle.kind, PROCESS_ENTRY_ORACLE_KIND);
  assert.deepEqual(workload.oracle.timedInput, PROCESS_ENTRY_TIMED_INPUT);
  assert.deepEqual(workload.oracle.cases, PROCESS_ENTRY_ORACLE_CASES);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.arguments), PROCESS_ENTRY_CORRECTNESS_INPUTS);
  assert.deepEqual(workload.sources.map((source) => source.language), EXECUTABLE_LANGUAGES);
  assert.ok(workload.sources.every((source) => source.recipeClass === PROCESS_ENTRY_RECIPE_CLASS));
  assert.equal(workload.sources.find((source) => source.language === "w").entry, "run");
  assert.equal(workload.sources.find((source) => source.language === "c").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MSVC);
  assert.equal(workload.sources.find((source) => source.language === "rust").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MSVC);
  const liveMetrics = documents.catalog.bestMetrics.entries.filter(
    (entry) => entry.workloadId === PROCESS_ENTRY_WORKLOAD_ID,
  );
  const clangMetrics = liveMetrics.filter((entry) => entry.language === "c");
  assert.ok(clangMetrics.length === 0 || clangMetrics.length === 6,
    "a public Clang recipe has either no current cells or one complete high-sample set");
  assert.ok(clangMetrics.every((entry) =>
    entry.toolchain.startsWith("clang-22.1.8-c23-portable-") &&
    entry.artifactTarget === EXECUTABLE_ARTIFACT_TARGET_MSVC));
  assert.ok(liveMetrics.every((entry) =>
    entry.provenance.artifactCleanliness === "verified-clean"));
});

test("process-enum-payload catalog pins the promoted tagged-union contract", () => {
  const workload = documents.catalog.workloads.find((item) => item.id === PROCESS_ENUM_PAYLOAD_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "exploratory-ready");
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, []);
  assert.equal(workload.oracle.kind, PROCESS_ENUM_PAYLOAD_ORACLE_KIND);
  assert.deepEqual(workload.oracle.timedInput, PROCESS_ENUM_PAYLOAD_TIMED_INPUT);
  assert.deepEqual(workload.oracle.cases, PROCESS_ENUM_PAYLOAD_ORACLE_CASES);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.arguments), PROCESS_ENUM_PAYLOAD_CORRECTNESS_INPUTS);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.exitCode), [7, 0, 0]);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.stdout), [
    "enum-missing true\n",
    "enum-received false\n",
    "enum-received false\n",
  ]);
  assert.ok(workload.sources.every((source) => source.recipeClass === PROCESS_ENUM_PAYLOAD_RECIPE_CLASS));
  assert.equal(workload.sources.find((source) => source.language === "w").entry, "dispatch");
  assert.equal(workload.sources.find((source) => source.language === "c").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MSVC);
  assert.equal(workload.sources.find((source) => source.language === "rust").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MSVC);
  const liveMetrics = documents.catalog.bestMetrics.entries.filter((entry) => entry.workloadId === PROCESS_ENUM_PAYLOAD_WORKLOAD_ID);
  assert.deepEqual(Object.fromEntries(
    ["c", "rust", "w"].map((language) => [language, liveMetrics.filter((entry) => entry.language === language).length]),
  ), { c: 6, rust: 6, w: 6 });
});

test("process-enum-payload C and Rust variants retain independent runtime enum paths", () => {
  const c = readFileSync(`${ROOT}/benchmarks/executable/process_enum_payload.c`, "utf8");
  const rust = readFileSync(`${ROOT}/benchmarks/executable/process_enum_payload.rs`, "utf8");
  assert.match(c, /int main\(int argc, char \*\*argv\)/u);
  assert.match(c, /enum admission_state_kind/u);
  assert.match(c, /union \{/u);
  assert.match(c, /int64_t amount/u);
  assert.match(c, /static struct admission_state build_admission/u);
  assert.match(c, /switch \(state\.kind\)/u);
  assert.match(c, /argc\s*==\s*1/u);
  assert.match(c, /printf\("enum-missing %s\\n"/u);
  assert.match(c, /repeated \? "true" : "false"/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.match(rust, /fn main\(\)/u);
  assert.match(rust, /enum AdmissionState/u);
  assert.match(rust, /fn build_admission/u);
  assert.match(rust, /match state/u);
  assert.match(rust, /let repeated = std::env::args_os\(\)\.nth\(1\)\.is_none\(\)/u);
  assert.match(rust, /write!\(stdout, "\{label\} \{repeated\}\\n"\)/u);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("strict f64 references retain independent runtime operations", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === RESTAURANT_F64_STRICT_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-f64-compile-time-folded",
    "runtime-f64-equivalence",
  ]);
  assert.deepEqual(new Set(workload.sources.map((source) => source.language)),
    new Set(EXECUTABLE_LANGUAGES));
  assert.deepEqual(workload.sources
    .filter((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET)
    .map((source) => [source.comparability, source.eligibility]), [
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
    ]);
  const wsl = workload.sources.find((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL);
  assert.deepEqual([wsl.comparability, wsl.eligibility], [
    "same-physical-hardware-diagnostic-only",
    "same-physical-hardware-diagnostic-only",
  ]);
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_f64_strict.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_f64_strict.rs`, "utf8");
  assert.match(c, /volatile double/u);
  assert.match(c, /sum_left \+ sum_right/u);
  assert.match(c, /difference_left - difference_right/u);
  assert.match(c, /product_left \* product_right/u);
  assert.match(c, /quotient_left \/ quotient_right/u);
  assert.match(c, /nan != nan/u);
  assert.doesNotMatch(c, /fast-math/iu);
  assert.match(rust, /black_box/u);
  assert.match(rust, /nan != nan/u);
  assert.doesNotMatch(rust, /fast-math/iu);
});

test("checked UInt arithmetic catalog pins fixed-input references and deferred equivalence", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === RESTAURANT_UINT_ARITHMETIC_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate successful fixed-input checked UInt arithmetic and comparisons with exact output. Fault behavior is outside scope because W traps while the C23 and Rust references exit 1.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "UInt 9223372036854775810/9223372036854775809/21; div 7; rem 2; cmp true/true/true/true/false/true/true\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-compile-time-folded",
    "runtime-uint-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) => [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) => source.recipeClass === "restaurant-uint-arithmetic-release"));
  assert.deepEqual(workload.sources
    .filter((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET)
    .map((source) => [source.comparability, source.eligibility]), [
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
    ]);
  const wsl = workload.sources.find((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL);
  assert.deepEqual([wsl.comparability, wsl.eligibility], [
    "same-physical-hardware-diagnostic-only",
    "same-physical-hardware-diagnostic-only",
  ]);
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_arithmetic.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_arithmetic.rs`, "utf8");
  assert.match(c, /volatile uint64_t/u);
  assert.match(c, /checked_add_u64/u);
  assert.match(c, /checked_divide_u64/u);
  assert.match(c, /checked_remainder_u64/u);
  assert.match(rust, /black_box/u);
  assert.match(rust, /checked_add_u64/u);
  assert.match(rust, /checked_divide_u64/u);
  assert.match(rust, /checked_remainder_u64/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("UInt bitwise complement catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === RESTAURANT_UINT_BIT_NOT_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.oracle.stdout,
    "UInt not 18446744073709551615\n");
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-bit-not-compile-time-folded",
    "runtime-uint-bit-not-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "restaurant-uint-bit-not-release"));
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_bit_not.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_bit_not.rs`, "utf8");
  assert.match(c, /volatile uint64_t/u);
  assert.match(c, /~runtime_zero/u);
  assert.match(rust, /black_box/u);
  assert.match(rust, /!black_box/u);
});

test("UInt compound catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === RESTAURANT_UINT_COMPOUND_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.oracle.stdout, "UInt compound 95\n");
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-compound-compile-time-folded",
    "runtime-uint-compound-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "restaurant-uint-compound-release"));
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_compound.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_compound.rs`, "utf8");
  assert.match(c, /value &= runtime_mask/u);
  assert.match(c, /value \^= runtime_toggle/u);
  assert.match(c, /value \|= runtime_set/u);
  assert.match(rust, /value &= black_box\(240_u64\)/u);
  assert.match(rust, /value \^= black_box\(170_u64\)/u);
  assert.match(rust, /value \|= black_box\(5_u64\)/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("UInt wrapping-add catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === RESTAURANT_UINT_WRAPPING_ADD_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate fixed-input full-width UInt wrapping addition at UINT64_MAX and exact unsigned decimal output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Wrapped 0\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-wrapping-add-compile-time-folded",
    "runtime-uint-wrapping-add-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "restaurant-uint-wrapping-add-release"));
  assert.deepEqual(workload.sources
    .filter((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET)
    .map((source) => [source.comparability, source.eligibility]), [
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
    ]);
  const wsl = workload.sources.find((source) =>
    source.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL);
  assert.deepEqual([wsl.comparability, wsl.eligibility], [
    "same-physical-hardware-diagnostic-only",
    "same-physical-hardware-diagnostic-only",
  ]);
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_add.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_add.rs`, "utf8");
  assert.match(c, /volatile uint64_t runtime_value/u);
  assert.match(c, /volatile uint64_t runtime_increment/u);
  assert.match(c, /wrapping_add_u64/u);
  assert.match(c, /left \+ right/u);
  assert.match(rust, /black_box\(u64::MAX\)/u);
  assert.match(rust, /black_box\(1_u64\)/u);
  assert.match(rust, /\.wrapping_add\(/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("UInt wrapping-subtract catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === RESTAURANT_UINT_WRAPPING_SUBTRACT_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate fixed-input full-width UInt wrapping subtraction of 1 from zero and exact unsigned decimal output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Wrapped 18446744073709551615\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-wrapping-subtract-compile-time-folded",
    "runtime-uint-wrapping-subtract-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "restaurant-uint-wrapping-subtract-release"));
  assert.deepEqual(workload.sources
    .filter((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET)
    .map((source) => [source.comparability, source.eligibility]), [
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
    ]);
  const wsl = workload.sources.find((source) =>
    source.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL);
  assert.deepEqual([wsl.comparability, wsl.eligibility], [
    "same-physical-hardware-diagnostic-only",
    "same-physical-hardware-diagnostic-only",
  ]);
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_subtract.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_subtract.rs`, "utf8");
  assert.match(c, /volatile uint64_t runtime_value/u);
  assert.match(c, /volatile uint64_t runtime_decrement/u);
  assert.match(c, /wrapping_subtract_u64/u);
  assert.match(c, /left - right/u);
  assert.match(rust, /black_box\(0_u64\)/u);
  assert.match(rust, /black_box\(1_u64\)/u);
  assert.match(rust, /\.wrapping_sub\(/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("UInt wrapping-multiply catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === RESTAURANT_UINT_WRAPPING_MULTIPLY_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate fixed-input full-width UInt wrapping multiplication at UINT64_MAX and 2 and exact unsigned decimal output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Wrapped 18446744073709551614\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-wrapping-multiply-compile-time-folded",
    "runtime-uint-wrapping-multiply-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "restaurant-uint-wrapping-multiply-release"));
  assert.deepEqual(workload.sources
    .filter((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET)
    .map((source) => [source.comparability, source.eligibility]), [
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
    ]);
  const wsl = workload.sources.find((source) =>
    source.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL);
  assert.deepEqual([wsl.comparability, wsl.eligibility], [
    "same-physical-hardware-diagnostic-only",
    "same-physical-hardware-diagnostic-only",
  ]);
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_multiply.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_multiply.rs`, "utf8");
  assert.match(c, /volatile uint64_t runtime_value/u);
  assert.match(c, /volatile uint64_t runtime_factor/u);
  assert.match(c, /wrapping_multiply_u64/u);
  assert.match(c, /left \* right/u);
  assert.match(rust, /black_box\(u64::MAX\)/u);
  assert.match(rust, /black_box\(2_u64\)/u);
  assert.match(rust, /\.wrapping_mul\(/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("UInt wrapping-negate catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === RESTAURANT_UINT_WRAPPING_NEGATE_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate fixed-input full-width UInt wrapping negation of 1 and exact unsigned decimal output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Wrapped 18446744073709551615\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-wrapping-negate-compile-time-folded",
    "runtime-uint-wrapping-negate-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "restaurant-uint-wrapping-negate-release"));
  assert.deepEqual(workload.sources
    .filter((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET)
    .map((source) => [source.comparability, source.eligibility]), [
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
    ]);
  const wsl = workload.sources.find((source) =>
    source.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL);
  assert.deepEqual([wsl.comparability, wsl.eligibility], [
    "same-physical-hardware-diagnostic-only",
    "same-physical-hardware-diagnostic-only",
  ]);
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_negate.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_negate.rs`, "utf8");
  assert.match(c, /volatile uint64_t runtime_value/u);
  assert.match(c, /UINT64_C\(1\)/u);
  assert.match(c, /wrapping_negate_u64/u);
  assert.match(c, /UINT64_C\(0\) - value/u);
  assert.match(rust, /black_box\(1_u64\)/u);
  assert.match(rust, /\.wrapping_neg\(\)/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("UInt wrapping-power catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === RESTAURANT_UINT_WRAPPING_POWER_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate fixed-input full-width UInt wrapping power of 3 to 40 and exact unsigned decimal output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Wrapped 12157665459056928801\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-wrapping-power-compile-time-folded",
    "runtime-uint-wrapping-power-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "restaurant-uint-wrapping-power-release"));
  assert.deepEqual(workload.sources
    .filter((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET)
    .map((source) => [source.comparability, source.eligibility]), [
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
      ["deferred-until-M3b", "deferred-to-M3b"],
    ]);
  const wsl = workload.sources.find((source) =>
    source.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL);
  assert.deepEqual([wsl.comparability, wsl.eligibility], [
    "same-physical-hardware-diagnostic-only",
    "same-physical-hardware-diagnostic-only",
  ]);
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_power.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/restaurant_uint_wrapping_power.rs`, "utf8");
  assert.match(c, /volatile uint64_t runtime_base/u);
  assert.match(c, /volatile uint64_t runtime_exponent/u);
  assert.match(c, /wrapping_power_u64/u);
  assert.match(c, /base \*= base/u);
  assert.match(c, /exponent >>= 1/u);
  assert.match(rust, /black_box\(3_u64\)/u);
  assert.match(rust, /black_box\(40_u64\)/u);
  assert.match(rust, /\.wrapping_mul\(/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("not-performance-ready strict f64 evidence cannot become live best metrics", () => {
  const result = validResult();
  const workload = documents.catalog.workloads.find((item) => item.id === RESTAURANT_F64_STRICT_WORKLOAD_ID);
  const source = workload.sources.find((item) => item.language === "rust" && item.platformTarget === EXECUTABLE_PLATFORM_TARGET);
  result.id = `${RESTAURANT_F64_STRICT_WORKLOAD_ID}-rust-example`;
  result.workloadId = RESTAURANT_F64_STRICT_WORKLOAD_ID;
  result.identity.sourceDigest = source.digest;
  result.identity.recipe = source.recipe;
  result.identity.recipeClass = source.recipeClass;
  result.identity.eligibility = source.eligibility;
  result.equivalenceKey = executableEquivalenceKey(
    documents.catalog,
    RESTAURANT_F64_STRICT_WORKLOAD_ID,
    EXECUTABLE_PLATFORM_TARGET,
    "release",
    source.recipeClass,
  );
  result.correctness.oracleId = `${RESTAURANT_F64_STRICT_WORKLOAD_ID}:exact-output`;
  result.correctness.stdoutDigest = exactOutputDigest(workload.oracle.stdout);
  result.provenance.sourceDigest = source.digest;
  assert.deepEqual(validateExecutableResult(result, documents.catalog), []);

  const derived = deriveExecutableBestMetrics(documents.catalog, [result]);
  assert.equal(derived.entries.length, 0);

  const forbidden = clone(documents.catalog.bestMetrics.entries[0]);
  forbidden.workloadId = RESTAURANT_F64_STRICT_WORKLOAD_ID;
  forbidden.provenance.sourceDigest = source.digest;
  assert.match(
    validateExecutableBestMetric(forbidden, documents.catalog).join("\n"),
    /eligible for live best metrics/u,
  );
});

test("process-arguments-ordering catalog pins the count-dependent seating contract", () => {
  const workload = documents.catalog.workloads.find((item) => item.id === PROCESS_ARGUMENTS_ORDERING_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "exploratory-ready");
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, []);
  assert.equal(workload.oracle.kind, PROCESS_ARGUMENTS_ORDERING_ORACLE_KIND);
  assert.deepEqual(workload.oracle.timedInput, PROCESS_ARGUMENTS_ORDERING_TIMED_INPUT);
  assert.deepEqual(workload.oracle.cases, PROCESS_ARGUMENTS_ORDERING_ORACLE_CASES);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.arguments), PROCESS_ARGUMENTS_ORDERING_CORRECTNESS_INPUTS);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.stdout), [
    "Kitchen seats 0 guests\n",
    "Kitchen seats 1 guests\n",
    "Banquet seats 2 guests\n",
  ]);
  assert.ok(workload.sources.every((source) => source.recipeClass === PROCESS_ARGUMENTS_ORDERING_RECIPE_CLASS));
  assert.deepEqual(workload.sources.map((source) => source.language), EXECUTABLE_LANGUAGES);
  assert.equal(workload.sources.find((source) => source.language === "w").entry, "run");
  assert.equal(workload.sources.find((source) => source.language === "c").entry, "main");
  assert.equal(workload.sources.find((source) => source.language === "rust").entry, "main");
});

test("process-arguments-ordering C and Rust variants retain independent count branches", () => {
  const c = readFileSync(`${ROOT}/benchmarks/executable/process_arguments_ordering.c`, "utf8");
  const rust = readFileSync(`${ROOT}/benchmarks/executable/process_arguments_ordering.rs`, "utf8");
  assert.match(c, /int main\(int argc, char \*\*argv\)/u);
  assert.match(c, /const int count = argc - 1/u);
  assert.match(c, /count < 2/u);
  assert.match(c, /Kitchen seats %d guests\\n/u);
  assert.match(c, /Banquet seats %d guests\\n/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.match(rust, /fn main\(\)/u);
  assert.match(rust, /args_os\(\)\.count\(\)\.saturating_sub\(1\)/u);
  assert.match(rust, /count < 2/u);
  assert.match(rust, /Kitchen seats \{count\} guests/u);
  assert.match(rust, /Banquet seats \{count\} guests/u);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("structure taxonomy rejects unknown and contradictory classes", () => {
  const invalidValue = clone(documents.catalog);
  invalidValue.workloads.find((item) => item.id === "hello").structureClass = "not-a-class";
  assert.match(validateExecutableCatalog(invalidValue, invalidValue).join("\n"), /structureClass is invalid/u);

  const wrongPublicClass = clone(documents.catalog);
  wrongPublicClass.workloads.find((item) => item.id === "hello").structureClass = "integration-linkage";
  assert.match(validateExecutableCatalog(wrongPublicClass, wrongPublicClass).join("\n"), /structureClass must be public-end-to-end/u);

  const wrongTransientClass = clone(documents.catalog);
  wrongTransientClass.workloads.find((item) => item.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID).execution.structureClass = "integration-linkage";
  assert.match(validateExecutableCatalog(wrongTransientClass, wrongTransientClass).join("\n"), /execution\.structureClass must identify the transient\/internal/u);
});

function sample(index) {
  return { wallNs: String(index + 1), cpuUserUs: "1", cpuSystemUs: "0", cpuTotalUs: "1", peakRssBytes: String(100 + index) };
}

function sampleSeries() {
  const raw = Array.from({ length: 9 }, (_, index) => sample(index));
  const stats = (min, median, max, arithmeticMean, mad) => ({ min: String(min), median: String(median), max: String(max), arithmeticMean: String(arithmeticMean), mad: String(mad) });
  return {
    warmup: [sample(99)],
    raw,
    summary: {
      wallNs: stats(1, 5, 9, 5, 2),
      cpuUserUs: stats(1, 1, 1, 1, 0),
      cpuSystemUs: stats(0, 0, 0, 0, 0),
      cpuTotalUs: stats(1, 1, 1, 1, 0),
      peakRssBytes: stats(100, 104, 108, 104, 2),
    },
    cpuResolution: {
      unit: "microseconds",
      zeroAllowed: true,
      disclosure: "Zero-valued samples are valid at microseconds resolution; they do not imply nanosecond precision.",
    },
  };
}

function validResult(language = "rust") {
  const workload = documents.catalog.workloads.find((item) => item.id === "hello");
  const source = workload.sources.find((item) => item.language === language);
  const artifactTarget = language === "c" ? EXECUTABLE_ARTIFACT_TARGET_MINGW : EXECUTABLE_ARTIFACT_TARGET_MSVC;
  const environment = { os: "windows-11", kernel: "windows-class", cpuModel: "x86_64-class", logicalCores: "16", ramBytes: "34359738368" };
  return {
    $schema: "./executable-benchmark.schema.json", schema: EXECUTABLE_RESULT_SCHEMA, kind: "executable-result", id: `hello-${language}-example`, status: "recorded",
    workloadId: "hello", language, platformTarget: EXECUTABLE_PLATFORM_TARGET, artifactTarget, profile: "release", quality: "exploratory", claim: "measurement-only", verdict: "not-evaluated",
    equivalenceKey: executableEquivalenceKey(documents.catalog, "hello", EXECUTABLE_PLATFORM_TARGET, "release", source.recipeClass),
    identity: { sourceDigest: source.digest, platformTarget: EXECUTABLE_PLATFORM_TARGET, artifactTarget, profile: "release", toolchain: language === "rust" ? "rustc-1.94" : "gcc-13.2", host: executableHostIdentity(environment), recipe: source.recipe, recipeClass: source.recipeClass, recipeDigest: digest, eligibility: source.eligibility },
    correctness: { oracleId: "hello:exact-output", exitCode: 0, stdoutDigest: exactOutputDigest("Hello, world!\n"), stderrDigest: exactOutputDigest("") },
    artifact: { digest, sizeBytes: "1024", cleanliness: { coffSymbols: { pointer: "0", count: "0" }, codeView: { count: "0", sizeBytes: "0" }, debugDirectory: { presence: "absent", sizeBytes: "0", entries: [] }, certificateDirectory: { pointer: "0", sizeBytes: "0" }, sectionData: "in-bounds", sidecars: { count: "0" }, overlay: { sizeBytes: "0" } } },
    protocol: { warmupMinimum: 1, rawMinimum: 9, rawParity: "odd", arithmeticMeanRounding: "floor-integer", stopRule: "fixed-count", wallClock: "monotonic-nanoseconds", processIsolation: "fresh-process-per-sample", runtimeScope: "direct-host-process", order: "deterministic-interleaved", resourceScope: "direct child process only; descendants are not aggregated", knownNoiseControls: ["warmup-discarded", "fresh-process-per-sample"], unknownNoiseControls: ["host-scheduler", "filesystem-cache"], directProcessDisclosure: "Bun direct-process counters cover the spawned process only; process-tree CPU/RSS are not aggregated.", measurementKernel: "bun-direct-test/1" },
    environment, compile: sampleSeries(), run: sampleSeries(),
    provenance: { sourceDigest: source.digest, artifactDigest: digest, recipeDigest: digest, toolchainDigest: digest, runnerDigest: digest, catalogDigest: digest, commit: "1".repeat(40), observedAt: "2026-09-08T00:00:00.000Z" },
  };
}

function linuxCatalogAndResult(language = "rust") {
  const catalog = clone(documents.catalog);
  const workload = catalog.workloads.find((item) => item.id === "hello");
  const source = clone(workload.sources.find((item) => item.language === language));
  source.platformTarget = EXECUTABLE_PLATFORM_TARGET_LINUX;
  source.artifactTarget = EXECUTABLE_ARTIFACT_TARGET_LINUX;
  workload.sources.push(source);
  const result = validResult(language);
  result.id = `hello-${language}-linux-example`;
  result.platformTarget = EXECUTABLE_PLATFORM_TARGET_LINUX;
  result.artifactTarget = EXECUTABLE_ARTIFACT_TARGET_LINUX;
  result.environment = { os: "linux", kernel: "linux-6.8", cpuModel: "x86_64-class", logicalCores: "16", ramBytes: "34359738368" };
  result.identity.sourceDigest = source.digest;
  result.identity.platformTarget = EXECUTABLE_PLATFORM_TARGET_LINUX;
  result.identity.artifactTarget = EXECUTABLE_ARTIFACT_TARGET_LINUX;
  result.identity.toolchain = "clang-18-linux";
  result.identity.host = executableHostIdentity(result.environment);
  result.equivalenceKey = executableEquivalenceKey(catalog, "hello", EXECUTABLE_PLATFORM_TARGET_LINUX, "release", source.recipeClass);
  result.provenance.sourceDigest = source.digest;
  return { catalog, result };
}

function wslCatalogAndResult(language = "rust", hostVariant = "microsoft-standard-wsl2") {
  const catalog = clone(documents.catalog);
  const workload = catalog.workloads.find((item) => item.id === "hello");
  const source = clone(workload.sources.find((item) => item.language === language));
  source.platformTarget = EXECUTABLE_PLATFORM_TARGET_LINUX_WSL;
  source.artifactTarget = EXECUTABLE_ARTIFACT_TARGET_LINUX;
  source.comparability = "same-physical-hardware-diagnostic-only";
  source.eligibility = "same-physical-hardware-diagnostic-only";
  workload.sources.push(source);
  const result = validResult(language);
  result.id = `hello-${language}-wsl-${hostVariant}`;
  result.platformTarget = EXECUTABLE_PLATFORM_TARGET_LINUX_WSL;
  result.artifactTarget = EXECUTABLE_ARTIFACT_TARGET_LINUX;
  result.environment = { os: "linux-wsl2", kernel: hostVariant, cpuModel: "x86_64-class", logicalCores: "16", ramBytes: "34359738368" };
  result.identity.sourceDigest = source.digest;
  result.identity.platformTarget = EXECUTABLE_PLATFORM_TARGET_LINUX_WSL;
  result.identity.artifactTarget = EXECUTABLE_ARTIFACT_TARGET_LINUX;
  result.identity.host = executableHostIdentity(result.environment);
  result.identity.eligibility = source.eligibility;
  result.equivalenceKey = executableEquivalenceKey(catalog, "hello", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL, "release", source.recipeClass);
  result.provenance.sourceDigest = source.digest;
  result.provenance.platformEvidence = {
    hostMode: "wsl2",
    comparisonPurpose: "same-physical-hardware-diagnostic-only",
    rankability: "same-host-only",
  };
  result.artifact.elfLayout = clone(VALID_ELF_LAYOUT);
  return { catalog, result };
}

function withPeLayout(result, layout = VALID_PE_LAYOUT) {
  result.artifact.peLayout = clone(layout);
  return result;
}

function zeroRunCpu(result) {
  for (const sample of [...result.run.warmup, ...result.run.raw]) Object.assign(sample, { cpuUserUs: "0", cpuSystemUs: "0", cpuTotalUs: "0" });
  for (const field of ["cpuUserUs", "cpuSystemUs", "cpuTotalUs"]) result.run.summary[field] = { min: "0", median: "0", max: "0", arithmeticMean: "0", mad: "0" };
  return result;
}

test("local result remains full-fidelity and rejects invalid measurements", () => {
  const result = validResult();
  assert.deepEqual(validateExecutableResult(result, documents.catalog), []);
  const badWall = clone(result);
  badWall.run.raw[0].wallNs = "0";
  assert.match(validateExecutableResult(badWall, documents.catalog).join("\n"), /wallNs must be positive/);
  const badCpuDisclosure = clone(result);
  badCpuDisclosure.run.cpuResolution.disclosure = "nanoseconds only";
  assert.match(validateExecutableResult(badCpuDisclosure, documents.catalog).join("\n"), /zero-valued microsecond/);
  const badRuntimeScope = clone(result);
  badRuntimeScope.protocol.runtimeScope = "cold-start";
  assert.match(validateExecutableResult(badRuntimeScope, documents.catalog).join("\n"), /runtimeScope/u);
  const badSource = clone(result);
  badSource.identity.artifactTarget = EXECUTABLE_PLATFORM_TARGET;
  assert.match(validateExecutableResult(badSource, documents.catalog).join("\n"), /artifact target/);
});

test("platform lanes stay closed, partitioned, and reject WSL masquerading as native Linux", () => {
  const { catalog, result } = linuxCatalogAndResult();
  assert.deepEqual(validateExecutableCatalog(catalog, { ...documents, catalog }), []);
  assert.equal(executableNativeHostForPlatform(result.environment, EXECUTABLE_PLATFORM_TARGET_LINUX), true);
  assert.deepEqual(validateExecutableResult(result, catalog), []);

  const windowsResult = validResult();
  windowsResult.id = "hello-rust-windows-baseline";
  const updated = updateExecutableBestMetrics(catalog, result);
  const windowsCells = updated.catalog.bestMetrics.entries.filter((entry) => entry.platformTarget === EXECUTABLE_PLATFORM_TARGET);
  const linuxCells = updated.catalog.bestMetrics.entries.filter((entry) => entry.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX);
  assert.ok(windowsCells.length > 0);
  assert.ok(linuxCells.length > 0);
  assert.ok(linuxCells.every((entry) => entry.artifactTarget === EXECUTABLE_ARTIFACT_TARGET_LINUX));
  assert.ok(updated.catalog.bestMetrics.entries.some((entry) => entry.platformTarget === EXECUTABLE_PLATFORM_TARGET && entry.workloadId === windowsResult.workloadId));

  const wsl = clone(result);
  wsl.environment.kernel = "microsoft-standard-wsl2";
  wsl.identity.host = executableHostIdentity(wsl.environment);
  assert.match(validateExecutableResult(wsl, catalog).join("\n"), /native Linux|WSL|composite/iu);
  assert.throws(() => deriveExecutableBestMetrics(catalog, [wsl]), /native Linux|WSL|composite/iu);

  const wslLane = wslCatalogAndResult();
  assert.equal(executableNativeHostForPlatform(wslLane.result.environment, EXECUTABLE_PLATFORM_TARGET_LINUX), false);
  assert.equal(executableHostEvidenceForPlatform(wslLane.result.environment, EXECUTABLE_PLATFORM_TARGET_LINUX_WSL), true);
  assert.deepEqual(validateExecutableCatalog(wslLane.catalog, { ...documents, catalog: wslLane.catalog }), []);
  assert.deepEqual(validateExecutableResult(wslLane.result, wslLane.catalog), []);
  const missingWslEvidence = clone(wslLane.result);
  delete missingWslEvidence.provenance.platformEvidence;
  assert.match(validateExecutableResult(missingWslEvidence, wslLane.catalog).join("\n"), /closed object shape|platformEvidence/u);
  const forgedWslEvidence = clone(wslLane.result);
  forgedWslEvidence.provenance.platformEvidence.rankability = "host-partitioned";
  assert.match(validateExecutableResult(forgedWslEvidence, wslLane.catalog).join("\n"), /same-host-only/u);
  const wslUpdated = updateExecutableBestMetrics(wslLane.catalog, wslLane.result);
  const wslCells = wslUpdated.catalog.bestMetrics.entries.filter((entry) => entry.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL);
  assert.ok(wslCells.length > 0);
  assert.ok(wslCells.every((entry) => entry.host.includes("wsl") || entry.host.includes("microsoft")));
  assert.ok(wslCells.every((entry) => entry.provenance.platformEvidence.comparisonPurpose === "same-physical-hardware-diagnostic-only"));
  assert.ok(wslCells.every((entry) => entry.platformTarget !== EXECUTABLE_PLATFORM_TARGET_LINUX));

  const otherWsl = wslCatalogAndResult("rust", "microsoft-standard-wsl2-other-host");
  otherWsl.result.id = "hello-rust-wsl-other-host";
  const separatelyPartitioned = deriveExecutableBestMetrics(wslLane.catalog, [wslLane.result, otherWsl.result]);
  assert.equal(new Set(separatelyPartitioned.entries.map((entry) => entry.categoryId)).size, 2);

  const toolchainVariant = clone(result);
  toolchainVariant.id = "hello-rust-linux-toolchain-variant";
  toolchainVariant.identity.toolchain = "gcc-13-linux";
  const partitioned = deriveExecutableBestMetrics(catalog, [result, toolchainVariant]);
  assert.equal(new Set(partitioned.entries.map((entry) => entry.categoryId)).size, 2);
});

test("ELF layout retains optional named section sizes and rejects forged metadata", () => {
  const { catalog, result } = wslCatalogAndResult();
  assert.deepEqual(validateExecutableResult(result, catalog), []);
  assert.deepEqual(result.artifact.elfLayout.sections, VALID_ELF_LAYOUT.sections);

  const malformedSize = clone(result);
  malformedSize.artifact.elfLayout.sections[0].sizeBytes = "01";
  assert.match(validateExecutableResult(malformedSize, catalog).join("\n"), /canonical decimal/u);

  const malformedName = clone(result);
  malformedName.artifact.elfLayout.sections[0].name = "";
  assert.match(validateExecutableResult(malformedName, catalog).join("\n"), /printable ASCII/u);

  const extraField = clone(result);
  extraField.artifact.elfLayout.sections[0].sh_size = "384";
  assert.match(validateExecutableResult(extraField, catalog).join("\n"), /closed object shape/u);

  const tooManySections = clone(result);
  tooManySections.artifact.elfLayout.sections = Array.from({ length: 65_536 }, () => ({ name: ".text", sizeBytes: "1" }));
  assert.match(validateExecutableResult(tooManySections, catalog).join("\n"), /at most 65535/u);
});

test("best derivation excludes zero CPU and preserves category/provenance", () => {
  const result = validResult();
  const zero = zeroRunCpu(clone(result));
  const derived = deriveExecutableBestMetrics(documents.catalog, [zero]);
  assert.deepEqual(validateExecutableBestMetrics(derived, documents.catalog), []);
  assert.equal(derived.entries.length, 4);
  assert.equal(derived.entries.some((entry) => entry.metric === "cpu-time"), false);
  assert.equal(derived.entries[0].provenance.recordId, result.id);
  const hostVariant = clone(result);
  hostVariant.id = "hello-rust-host-variant";
  hostVariant.environment.cpuModel = "x86_64-other-class";
  hostVariant.identity.host = executableHostIdentity(hostVariant.environment);
  const variant = deriveExecutableBestMetrics(documents.catalog, [result, hostVariant]);
  assert.equal(new Set(variant.entries.map((entry) => entry.categoryId)).size, 2);
});

test("101 fresh runs publish mean CPU and nearest-rank P95 wall time", () => {
  const result = validResult();
  result.run.raw = Array.from({ length: 101 }, (_, index) => sample(index));
  result.run.summary = deriveSummary(result.run.raw);
  const derived = deriveExecutableBestMetrics(documents.catalog, [result]);
  const cpu = derived.entries.find((entry) => entry.metric === "cpu-time");
  const p95 = derived.entries.find((entry) => entry.metric === "run-wall-p95");
  assert.equal(cpu?.statistic, "arithmeticMean");
  assert.equal(cpu?.value, "1");
  assert.equal(p95?.statistic, "p95");
  assert.equal(p95?.value, "96");
  assert.deepEqual(validateExecutableBestMetrics(derived, documents.catalog), []);
});

test("update replaces only lower cells and is idempotent for non-improving values", () => {
  const result = validResult();
  const first = updateExecutableBestMetrics(documents.catalog, result);
  assert.equal(first.changed, true);
  assert.ok(first.updatedMetrics.includes("compile-latency"));
  const again = updateExecutableBestMetrics(first.catalog, result);
  assert.equal(again.changed, false);
  const zero = clone(documents.catalog.bestMetrics.entries[0]);
  zero.value = "0";
  assert.match(validateExecutableBestMetric(zero, documents.catalog).join("\n"), /positive/);
});

test("update keeps only current runner evidence inside one live lane", () => {
  const firstResult = validResult();
  const first = updateExecutableBestMetrics(documents.catalog, firstResult);
  const nextResult = clone(firstResult);
  nextResult.id = "hello-rust-current-runner";
  nextResult.identity.toolchain = `${firstResult.identity.toolchain}-next`;
  nextResult.provenance.runnerDigest = "sha256:" + "4".repeat(64);
  nextResult.provenance.toolchainDigest = "sha256:" + "5".repeat(64);
  nextResult.provenance.commit = "4".repeat(40);
  nextResult.provenance.observedAt = "2026-09-14T12:00:00.000Z";
  for (const stage of [nextResult.compile, nextResult.run]) {
    for (const sample of [...stage.warmup, ...stage.raw]) sample.wallNs = String(BigInt(sample.wallNs) + 1000n);
    stage.summary = deriveSummary(stage.raw);
  }
  const updated = updateExecutableBestMetrics(first.catalog, nextResult);
  const lane = updated.catalog.bestMetrics.entries.filter((entry) =>
    entry.workloadId === nextResult.workloadId && entry.language === nextResult.language &&
    entry.host === nextResult.identity.host);
  assert.ok(updated.changed);
  assert.ok(lane.length > 0);
  assert.ok(lane.every((entry) => entry.provenance.runnerDigest === nextResult.provenance.runnerDigest));
  assert.ok(lane.every((entry) => entry.provenance.recordId === nextResult.id));
  assert.ok(updated.updatedMetrics.includes("run-wall-time"));
});

test("source refresh evicts stale cells without relabeling history", () => {
  const catalog = clone(documents.catalog);
  catalog.bestMetrics.entries = catalog.bestMetrics.entries.filter((entry) => entry.workloadId === "hello");
  const workload = catalog.workloads.find((item) => item.id === "hello");
  const source = workload.sources.find((item) => item.language === "rust");
  const staleEntries = catalog.bestMetrics.entries.filter(
    (entry) => entry.workloadId === "hello" && entry.language === "rust",
  );
  const preserved = catalog.bestMetrics.entries.find(
    (entry) => entry.workloadId === "hello" && entry.language === "c" && entry.metric === "artifact-size",
  );
  const refreshedDigest = "sha256:" + "2".repeat(64);
  source.digest = refreshedDigest;

  const result = validResult("rust");
  result.id = "hello-rust-source-refresh";
  result.identity.sourceDigest = refreshedDigest;
  result.provenance.sourceDigest = refreshedDigest;

  assert.match(
    validateExecutableBestMetrics(catalog.bestMetrics, catalog).join("\n"),
    /sourceDigest must match the catalog source/u,
  );
  assert.deepEqual(validateExecutableBestMetrics(
    catalog.bestMetrics, catalog, { allowStaleSourceDigest: true }), []);

  const update = updateExecutableBestMetrics(catalog, result);
  const refreshedEntries = update.catalog.bestMetrics.entries.filter(
    (entry) => entry.workloadId === "hello" && entry.language === "rust",
  );
  assert.ok(update.changed);
  assert.ok(update.updatedMetrics.includes("cpu-time"));
  assert.ok(update.updatedMetrics.includes("run-wall-p95"));
  assert.ok(refreshedEntries.length > 0);
  assert.ok(refreshedEntries.every((entry) => entry.provenance.recordId === result.id));
  assert.ok(refreshedEntries.every((entry) => entry.provenance.sourceDigest === refreshedDigest));
  assert.ok(staleEntries.every((stale) =>
    update.catalog.bestMetrics.entries.every((entry) => entry.provenance.recordId !== stale.provenance.recordId),
  ));
  assert.deepEqual(
    update.catalog.bestMetrics.entries.find(
      (entry) => entry.workloadId === preserved.workloadId && entry.language === preserved.language && entry.metric === preserved.metric,
    ),
    preserved,
  );
  assert.deepEqual(
    validateExecutableBestMetrics(update.catalog.bestMetrics, update.catalog),
    [],
  );
});

test("best-metric prune removes only source-stale cells and is idempotent", () => {
  const catalog = clone(documents.catalog);
  const source = catalog.workloads.find((item) => item.id === "hello").sources
    .find((item) => item.language === "rust");
  const stale = clone(catalog.bestMetrics.entries.find(
    (entry) => entry.workloadId === "hello" && entry.language === "rust" && entry.metric === "artifact-size",
  ));
  const preserved = clone(catalog.bestMetrics.entries.find(
    (entry) => entry.workloadId === "hello" && entry.language === "c" && entry.metric === "artifact-size",
  ));
  assert.ok(stale);
  assert.ok(preserved);
  source.digest = "sha256:" + "3".repeat(64);
  catalog.bestMetrics.entries = [stale, preserved];

  const first = pruneExecutableBestMetrics(catalog);
  assert.equal(first.removedCount, 1);
  assert.deepEqual(first.removedMetrics, ["artifact-size"]);
  assert.deepEqual(first.catalog.bestMetrics.entries, [preserved]);
  assert.equal(first.catalog.bestMetrics.status, "current");
  assert.deepEqual(validateExecutableBestMetrics(first.catalog.bestMetrics, first.catalog), []);

  const second = pruneExecutableBestMetrics(first.catalog);
  assert.equal(second.changed, false);
  assert.equal(second.removedCount, 0);
  assert.deepEqual(second.catalog, first.catalog);
});

test("artifact-size derivation preserves historical absence and enriches equal-size evidence atomically", () => {
  const historical = validResult();
  const historicalDerived = deriveExecutableBestMetrics(documents.catalog, [historical]);
  const historicalArtifact = historicalDerived.entries.find((entry) => entry.metric === "artifact-size");
  assert.equal(Object.hasOwn(historicalArtifact, "peLayout"), false, "historical metadata must not be invented");

  const emptyCatalog = clone(documents.catalog);
  emptyCatalog.bestMetrics = { ...emptyCatalog.bestMetrics, entries: [] };
  const incrementalHistorical = updateExecutableBestMetrics(emptyCatalog, historical);
  assert.deepEqual(incrementalHistorical.catalog.bestMetrics, historicalDerived, "incremental publication must match derivation");

  const enriched = withPeLayout(validResult());
  enriched.id = "hello-rust-enriched";
  enriched.artifact.digest = "sha256:2222222222222222222222222222222222222222222222222222222222222222";
  enriched.provenance.artifactDigest = enriched.artifact.digest;
  enriched.provenance.commit = "2".repeat(40);
  enriched.provenance.observedAt = "2026-09-09T00:00:00.000Z";
  const enrichedUpdate = updateExecutableBestMetrics(incrementalHistorical.catalog, enriched);
  assert.equal(enrichedUpdate.changed, true, "same-size validated layout should enrich the artifact cell");
  const enrichedArtifact = enrichedUpdate.catalog.bestMetrics.entries.find((entry) => entry.metric === "artifact-size");
  const expectedEnrichedArtifact = deriveExecutableBestMetrics(documents.catalog, [enriched]).entries
    .find((entry) => entry.metric === "artifact-size");
  assert.deepEqual(enrichedArtifact, expectedEnrichedArtifact, "an enrichment replaces the whole cell with fresh provenance");
  assert.deepEqual(enrichedArtifact.peLayout, VALID_PE_LAYOUT);
  assert.equal(enrichedArtifact.provenance.recordId, enriched.id);
  for (const order of [[historical, enriched], [enriched, historical]]) {
    const derived = deriveExecutableBestMetrics(documents.catalog, order).entries.find((entry) => entry.metric === "artifact-size");
    assert.deepEqual(derived, enrichedArtifact);
  }
  const oversizedSections = clone(enriched);
  oversizedSections.artifact.peLayout.sections[0].rawSize = "2048";
  assert.match(validateExecutableResult(oversizedSections, documents.catalog).join("\n"), /fit within the artifact/u);
  const unverifiedLayout = clone(enrichedArtifact);
  unverifiedLayout.provenance.artifactCleanliness = "historical-unverified";
  assert.match(validateExecutableBestMetric(unverifiedLayout, documents.catalog).join("\n"), /verified-clean artifact provenance/u);

  const repeated = updateExecutableBestMetrics(enrichedUpdate.catalog, enriched);
  assert.equal(repeated.changed, false, "an already-enriched equal-size tie is a no-op");
  assert.deepEqual(repeated.catalog.bestMetrics, enrichedUpdate.catalog.bestMetrics);

  const regression = clone(enriched);
  regression.id = "hello-rust-larger-artifact";
  regression.artifact.sizeBytes = "2048";
  regression.artifact.digest = "sha256:3333333333333333333333333333333333333333333333333333333333333333";
  regression.provenance.artifactDigest = regression.artifact.digest;
  const regressed = updateExecutableBestMetrics(enrichedUpdate.catalog, regression);
  assert.equal(regressed.changed, false, "a larger artifact must not promote any best cell");
  assert.deepEqual(regressed.catalog.bestMetrics, enrichedUpdate.catalog.bestMetrics);
});
