import assert from "node:assert/strict";
import test from "node:test";
import {
  EXECUTABLE_ARTIFACT_TARGET_MINGW,
  EXECUTABLE_ARTIFACT_TARGET_MSVC,
  EXECUTABLE_BEST_SCHEMA,
  EXECUTABLE_COMPARABILITY_AXES,
  EXECUTABLE_LANGUAGES,
  EXECUTABLE_RESULT_SCHEMA,
  EXECUTABLE_PLATFORM_TARGET,
  EXECUTABLE_STRUCTURE_CLASSES,
  PROCESS_ENTRY_CORRECTNESS_INPUTS,
  PROCESS_ENTRY_ORACLE_CASES,
  PROCESS_ENTRY_ORACLE_KIND,
  PROCESS_ENTRY_RECIPE_CLASS,
  PROCESS_ENTRY_TIMED_INPUT,
  PROCESS_ENTRY_WORKLOAD_ID,
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
  deriveExecutableBestMetrics,
  executableEquivalenceKey,
  executableHostIdentity,
  exactOutputDigest,
  loadExecutableDocuments,
  updateExecutableBestMetrics,
  validateExecutableBestMetric,
  validateExecutableBestMetrics,
  validateExecutableCatalog,
  validateExecutableResult,
} from "./executable-benchmark-machine.mjs";

const documents = loadExecutableDocuments();
const clone = (value) => structuredClone(value);
const digest = "sha256:1111111111111111111111111111111111111111111111111111111111111111";

test("catalog stores compact live best cells and no immutable history", () => {
  assert.deepEqual(validateExecutableCatalog(documents.catalog, documents), []);
  assert.equal(documents.schema.$id, "w-executable-benchmark/4");
  assert.deepEqual(documents.schema.oneOf.map((entry) => entry.$ref), [
    "#/$defs/catalog", "#/$defs/result", "#/$defs/bestMetric", "#/$defs/bestMetrics",
  ]);
  assert.deepEqual(documents.schema.$defs.structureClass.enum, EXECUTABLE_STRUCTURE_CLASSES);
  for (const definition of ["catalog", "result", "bestMetric", "bestMetrics", "bestMetricProvenance", "sample", "sampleSeries", "processExecution", "processSupportSource"]) {
    assert.equal(documents.schema.$defs[definition].additionalProperties, false);
  }
  assert.deepEqual(documents.catalog.comparabilityAxes, EXECUTABLE_COMPARABILITY_AXES);
  assert.equal(documents.catalog.resultContract.recordsPath, "benchmarks/results");
  assert.equal(documents.catalog.bestMetricsContract.schema, EXECUTABLE_BEST_SCHEMA);
  const metricsByCell = Object.fromEntries(
    Object.entries(Object.groupBy(
      documents.catalog.bestMetrics.entries,
      (entry) => `${entry.workloadId}/${entry.language}`,
    )).map(([cell, entries]) => [cell, entries.map((entry) => entry.metric).sort()]),
  );
  const commonMetrics = ["artifact-size", "compile-latency", "peak-working-set", "run-wall-time"];
  assert.deepEqual(metricsByCell, {
    "hello/c": commonMetrics,
    "hello/rust": commonMetrics,
    "hello/w": commonMetrics,
    "process-entry/c": commonMetrics,
    "process-entry/rust": commonMetrics,
    "process-entry/w": commonMetrics,
    "process-handler-lifecycle/c": commonMetrics,
    "process-handler-lifecycle/rust": commonMetrics,
    "process-handler-lifecycle/w": [...commonMetrics, "cpu-time"].sort(),
    "restaurant-branch/c": commonMetrics,
    "restaurant-branch/rust": commonMetrics,
    "restaurant-branch/w": commonMetrics,
    "restaurant-enum-switch/c": commonMetrics,
    "restaurant-enum-switch/rust": commonMetrics,
    "restaurant-enum-switch/w": commonMetrics,
  });
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
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.oracle.kind, PROCESS_ENTRY_ORACLE_KIND);
  assert.deepEqual(workload.oracle.timedInput, PROCESS_ENTRY_TIMED_INPUT);
  assert.deepEqual(workload.oracle.cases, PROCESS_ENTRY_ORACLE_CASES);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.arguments), PROCESS_ENTRY_CORRECTNESS_INPUTS);
  assert.deepEqual(workload.sources.map((source) => source.language), EXECUTABLE_LANGUAGES);
  assert.ok(workload.sources.every((source) => source.recipeClass === PROCESS_ENTRY_RECIPE_CLASS));
  assert.equal(workload.sources.find((source) => source.language === "w").entry, "run");
  assert.equal(workload.sources.find((source) => source.language === "c").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MINGW);
  assert.equal(workload.sources.find((source) => source.language === "rust").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MSVC);
  const liveMetrics = documents.catalog.bestMetrics.entries.filter(
    (entry) => entry.workloadId === PROCESS_ENTRY_WORKLOAD_ID,
  );
  assert.equal(liveMetrics.length, EXECUTABLE_LANGUAGES.length * 4);
  assert.ok(liveMetrics.every((entry) =>
    entry.provenance.artifactCleanliness === "verified-clean"));
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
    artifact: { digest, sizeBytes: "123", cleanliness: { coffSymbols: { pointer: "0", count: "0" }, codeView: { count: "0", sizeBytes: "0" }, debugDirectory: { presence: "absent", sizeBytes: "0", entries: [] }, certificateDirectory: { pointer: "0", sizeBytes: "0" }, sectionData: "in-bounds", sidecars: { count: "0" }, overlay: { sizeBytes: "0" } } },
    protocol: { warmupMinimum: 1, rawMinimum: 9, rawParity: "odd", arithmeticMeanRounding: "floor-integer", stopRule: "fixed-count", wallClock: "monotonic-nanoseconds", processIsolation: "fresh-process-per-sample", order: "deterministic-interleaved", resourceScope: "direct child process only; descendants are not aggregated", knownNoiseControls: ["warmup-discarded", "fresh-process-per-sample"], unknownNoiseControls: ["host-scheduler", "filesystem-cache"], directProcessDisclosure: "Bun direct-process counters cover the spawned process only; process-tree CPU/RSS are not aggregated." },
    environment, compile: sampleSeries(), run: sampleSeries(),
    provenance: { sourceDigest: source.digest, artifactDigest: digest, recipeDigest: digest, toolchainDigest: digest, runnerDigest: digest, catalogDigest: digest, commit: "1".repeat(40), observedAt: "2026-09-08T00:00:00.000Z" },
  };
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
  const badSource = clone(result);
  badSource.identity.artifactTarget = EXECUTABLE_PLATFORM_TARGET;
  assert.match(validateExecutableResult(badSource, documents.catalog).join("\n"), /artifact target/);
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
