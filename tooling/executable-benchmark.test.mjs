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
  EXECUTABLE_SUITE_DEFAULT_PLATFORMS,
  EXECUTABLE_SUITE_RECEIPT_SCHEMA,
  EXECUTABLE_PLATFORM_TARGET,
  EXECUTABLE_PLATFORM_TARGET_LINUX,
  EXECUTABLE_PLATFORM_TARGET_LINUX_WSL,
  EXECUTABLE_RUN_TARGETS,
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
  FLOAT_BIT_REPRESENTATION_WORKLOAD_ID,
  FLOAT_STRICT_WORKLOAD_ID,
  INTEGER_PREFIX_WORKLOAD_ID,
  INTEGER_WRAPPING_WORKLOAD_ID,
  INTEGER_WIDENING_WORKLOAD_ID,
  NUMERIC_WIDENING_WORKLOAD_ID,
  INTEGER_TRUNCATING_BITS_WORKLOAD_ID,
  INTEGER_SATURATING_CONVERSION_WORKLOAD_ID,
  INTEGER_COMPARISON_WORKLOAD_ID,
  INTEGER_BITWISE_WORKLOAD_ID,
  INTEGER_SHIFT_SEMANTICS_WORKLOAD_ID,
  CHECKED_INTEGER_ARITHMETIC_WORKLOAD_ID,
  UINT_BITWISE_WORKLOAD_ID,
  FIXED_INTEGER_BIT_PRIMITIVES_WORKLOAD_ID,
  UINT_OVERFLOWING_FAMILY_WORKLOAD_ID,
  UINT_SATURATING_POLICY_WORKLOAD_ID,
  UINT_COMPOUND_WORKLOAD_ID,
  ROOT,
  deriveExecutableBestMetrics,
  executableEquivalenceKey,
  executableHostEvidenceForPlatform,
  executableHostIdentity,
  executableNativeHostForPlatform,
  executableSourceDigest,
  executableCatalogFileDigest,
  executableSuiteReceiptErrors,
  exactOutputDigest,
  loadExecutableDocuments,
  parseExecutableSourceExpectation,
  selectExecutableSuiteLanes,
  pruneExecutableBestMetrics,
  updateExecutableBestMetrics,
  validateExecutableBestMetric,
  validateExecutableBestMetrics,
  validateExecutableCatalog,
  validateExecutableSourceExpectation,
  validateExecutableResult,
} from "./executable-benchmark-machine.mjs";

const documents = loadExecutableDocuments();
const clone = (value) => structuredClone(value);
const digest = "sha256:1111111111111111111111111111111111111111111111111111111111111111";
const COMPACT_METRIC_SET = ["artifact-size", "compile-latency"];
const COMPLETE_METRIC_SET = ["artifact-size", "compile-latency", "cpu-time", "peak-working-set", "run-wall-p95", "run-wall-time"];
function assertCurrentMetricLanes(workload, entries, message) {
  assert.ok(entries.length > 0, `${message}: current suite must retain measured cells`);
  const lanes = Object.groupBy(entries, (entry) => `${entry.language}/${entry.platformTarget}`);
  for (const [laneId, laneEntries] of Object.entries(lanes)) {
    const metrics = [...new Set(laneEntries.map((entry) => entry.metric))].sort();
    assert.ok(
      JSON.stringify(metrics) === JSON.stringify(COMPACT_METRIC_SET) ||
      JSON.stringify(metrics) === JSON.stringify(COMPLETE_METRIC_SET),
      `${message}: ${laneId} must retain a complete current metric set`,
    );
    const [language, platformTarget] = laneId.split("/");
    const source = workload.sources.find((item) => item.language === language && item.platformTarget === platformTarget);
    assert.ok(source, `${message}: ${laneId} must still have a catalog source`);
    assert.ok(laneEntries.every((entry) => entry.provenance.sourceDigest === executableSourceDigest(source)),
      `${message}: ${laneId} must use current source evidence`);
  }
}
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
  assert.deepEqual(validateExecutableBestMetrics(documents.catalog.bestMetrics, documents.catalog), []);
  for (const workloadId of ["hello", "hello-platform-minimal"]) {
    assert.equal(documents.catalog.workloads.find((workload) => workload.id === workloadId).demoEvidence,
      "bounded-w-demo", `${workloadId} has successful public W suite evidence`);
  }
  assert.equal(documents.schema.$id, "w-executable-benchmark/7");
  assert.deepEqual(documents.schema.oneOf.map((entry) => entry.$ref), [
    "#/$defs/catalog", "#/$defs/result", "#/$defs/bestMetric", "#/$defs/bestMetrics", "#/$defs/executableSuiteCurrent",
  ]);
  assert.deepEqual(documents.schema.$defs.structureClass.enum, EXECUTABLE_STRUCTURE_CLASSES);
  assert.deepEqual(documents.schema.$defs.source.properties.comparability.enum,
    ["deferred-until-M3b", "promotable-after-equivalence", "contextual-non-ranking-private-composite", "same-physical-hardware-diagnostic-only"]);
  assert.deepEqual(documents.schema.$defs.source.properties.eligibility.enum,
    ["promotable-after-equivalence", "deferred-to-M3b", "exploratory-private-composite", "same-physical-hardware-diagnostic-only"]);
  for (const definition of ["catalog", "result", "bestMetric", "bestMetrics", "bestMetricProvenance", "sample", "sampleSeries", "processExecution", "processSupportSource", "sourceSupport", "elfLayout", "elfSection", "executableSuiteCurrent", "executableSuiteLane"]) {
    assert.equal(documents.schema.$defs[definition].additionalProperties, false);
  }
  assert.deepEqual(documents.catalog.comparabilityAxes, EXECUTABLE_COMPARABILITY_AXES);
  assert.ok(documents.catalog.comparabilityAxes.includes("runtime-closure"));
  assert.ok(documents.catalog.bestMetrics.entries.length > 0,
    "the catalog retains compact current best cells rather than immutable history");
  assert.ok(documents.catalog.workloads.every((workload) => typeof workload.family === "string"));
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
  const retainedMetrics = COMPACT_METRIC_SET;
  const completeRuntimeMetrics = COMPLETE_METRIC_SET;
  const declaredCells = new Set(documents.catalog.workloads.flatMap((workload) =>
    workload.sources.map((source) =>
      `${workload.id}/${source.language}/${source.platformTarget}`)));
  assert.ok(Object.keys(metricsByCell).every((cell) => declaredCells.has(cell)),
    "every live metric cell must still have a current workload source");
  for (const requiredCell of ["hello/c/windows-x64", "hello/rust/windows-x64",
    "hello/w/windows-x64", "bool-short-circuit/w/windows-x64",
    "process-entry/c/windows-x64", "process-entry/rust/windows-x64",
    "process-entry/w/windows-x64", "process-arguments-count/c/windows-x64",
    "process-arguments-count/rust/windows-x64", "process-arguments-count/w/windows-x64",
    "process-handler-lifecycle/c/windows-x64", "process-handler-lifecycle/rust/windows-x64",
    "process-handler-lifecycle/w/windows-x64"]) {
    assert.ok(metricsByCell[requiredCell], `${requiredCell} must retain live evidence`);
  }
  for (const workloadId of ["hello", "process-entry", "branch",
    "enum-switch", "while-post", "wmo"]) {
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
    .filter((entry) => entry.workloadId === "enum-switch")
    .every((entry) => entry.provenance.artifactCleanliness === "verified-clean"));
  assert.ok(documents.catalog.bestMetrics.entries.every((entry) => entry.value !== "0"));
  assert.ok(documents.catalog.bestMetrics.entries.every((entry) => entry.runtimeClosure.status === "unverified"));
  assert.ok(new Set(documents.catalog.bestMetrics.entries.map((entry) => entry.language)).size === 3);
  assert.ok(documents.catalog.bestMetrics.entries.some((entry) => entry.language === "rust" && entry.eligibility === "promotable-after-equivalence"));
});

test("suite lane selection is deterministic, excludes diagnostic/private rows, and retains six contextual Hello lanes", () => {
  const lanes = selectExecutableSuiteLanes(documents.catalog);
  assert.ok(lanes.length > 0);
  assert.equal(new Set(lanes.map((lane) => `${lane.workloadId}/${lane.language}/${lane.platformTarget}`)).size, lanes.length);
  assert.deepEqual(lanes.filter((lane) => lane.workloadId === "hello-platform-minimal"), [
    { workloadId: "hello-platform-minimal", language: "w", platformTarget: "windows-x64" },
    { workloadId: "hello-platform-minimal", language: "c", platformTarget: "windows-x64" },
    { workloadId: "hello-platform-minimal", language: "rust", platformTarget: "windows-x64" },
    { workloadId: "hello-platform-minimal", language: "w", platformTarget: "linux-wsl-x64" },
    { workloadId: "hello-platform-minimal", language: "c", platformTarget: "linux-wsl-x64" },
    { workloadId: "hello-platform-minimal", language: "rust", platformTarget: "linux-wsl-x64" },
  ]);
  assert.ok(lanes.every((lane) => lane.workloadId !== "process-handler-lifecycle" && lane.workloadId !== "composition"));
  assert.ok(lanes.filter((lane) => lane.platformTarget === "linux-wsl-x64")
    .every((lane) => lane.workloadId === "hello-platform-minimal"));
  assert.deepEqual(selectExecutableSuiteLanes(documents.catalog, { platforms: ["linux-wsl-x64"] }).map((lane) => lane.language), ["w", "c", "rust"]);
  assert.throws(() => selectExecutableSuiteLanes(documents.catalog, { platforms: ["linux-x64"] }), /supported runner platforms/u);
});

test("suite receipt binds a full successful selection and rejects stale, filtered, or incomplete claims", () => {
  const platforms = [...EXECUTABLE_SUITE_DEFAULT_PLATFORMS];
  const lanes = selectExecutableSuiteLanes(documents.catalog, { platforms }).map((lane) => {
    const workload = documents.catalog.workloads.find((item) => item.id === lane.workloadId);
    const source = workload.sources.find((item) => item.language === lane.language && item.platformTarget === lane.platformTarget);
    return {
      ...lane,
      status: "passed",
      toolchain: `test-${lane.language}-${lane.platformTarget}`,
      recipe: source.recipe,
      toolchainDigest: digest,
    };
  });
  const receipt = {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_SUITE_RECEIPT_SCHEMA,
    kind: "executable-suite-current",
    status: "current",
    mode: "full",
    platforms,
    catalogDigest: executableCatalogFileDigest(ROOT),
    observedAt: "2026-09-22T12:00:00.000Z",
    durationMs: 1200,
    laneCounts: { total: lanes.length, passed: lanes.length, failed: 0, skipped: 0 },
    lanes,
  };
  assert.deepEqual(executableSuiteReceiptErrors(receipt, documents.catalog, { catalogDigest: receipt.catalogDigest }), []);
  const stale = clone(receipt);
  stale.catalogDigest = digest;
  assert.match(executableSuiteReceiptErrors(stale, documents.catalog, { catalogDigest: receipt.catalogDigest }).join("\n"), /stale/u);
  const incomplete = clone(receipt);
  incomplete.laneCounts.failed = 1;
  assert.match(executableSuiteReceiptErrors(incomplete, documents.catalog, { catalogDigest: receipt.catalogDigest }).join("\n"), /complete successful suite/u);
  const falseFull = clone(receipt);
  falseFull.platforms = ["windows-x64"];
  assert.match(executableSuiteReceiptErrors(falseFull, documents.catalog, { catalogDigest: receipt.catalogDigest }).join("\n"), /mode does not match/u);
});

test("runtime closure classifications are recipe-derived and artifact verification remains unclaimed", () => {
  const source = (workloadId, language, platformTarget = EXECUTABLE_PLATFORM_TARGET) =>
    documents.catalog.workloads.find((workload) => workload.id === workloadId)
      .sources.find((item) => item.language === language && item.platformTarget === platformTarget);
  assert.deepEqual(source("hello", "w").runtimeClosure, { class: "freestanding", status: "unverified" });
  assert.deepEqual(source("hello", "c").runtimeClosure, { class: "hosted-crt", status: "unverified" });
  assert.deepEqual(source("hello-platform-minimal", "c").runtimeClosure, { class: "freestanding", status: "unverified" });
  assert.deepEqual(source(PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID, "w").runtimeClosure, { class: "hosted-crt", status: "unverified" });
  assert.ok(documents.catalog.workloads.every((workload) =>
    workload.sources.every((item) => item.runtimeClosure.class !== "instrumentation")),
  "no instrumentation measurements are currently cataloged");

  const forged = validResult("rust");
  forged.identity.runtimeClosure = { class: "freestanding", status: "unverified" };
  assert.match(validateExecutableResult(forged, documents.catalog).join("\n"), /runtimeClosure must match the catalog/u);
});

test("source-local expected-output comments are opt-in and exact", () => {
  const optInPaths = new Set();
  for (const workload of documents.catalog.workloads) {
    for (const source of workload.sources) {
      const sourceText = readFileSync(`${ROOT}/${source.path}`, "utf8");
      const expectation = parseExecutableSourceExpectation(sourceText);
      if (expectation === undefined) continue;
      optInPaths.add(source.path);
      assert.deepEqual(expectation.errors, [], `${source.path} must use the compact expected-output form`);
      assert.deepEqual(
        validateExecutableSourceExpectation(sourceText, workload.oracle, `${workload.id}/${source.language}`),
        [],
        `${source.path} must match its catalog oracle`,
      );
    }
  }
  const requiredOptInPaths = [
    "benchmarks/executable/process_arguments_ordering.c",
    "benchmarks/executable/process_arguments_ordering.rs",
    "compiler/seed-c/fixtures/process-arguments-ordering.w",
    "benchmarks/executable/process_enum_payload.c",
    "benchmarks/executable/process_enum_payload.rs",
    "compiler/seed-c/fixtures/process-enum-payload.w",
    "benchmarks/executable/integer_wrapping.c",
    "benchmarks/executable/integer_wrapping.rs",
    "compiler/seed-c/fixtures/integer-wrapping.w",
    "benchmarks/executable/integer_prefix.c",
    "benchmarks/executable/integer_prefix.rs",
    "compiler/seed-c/fixtures/integer-prefix.w",
    "benchmarks/executable/integer_widening.c",
    "benchmarks/executable/integer_widening.rs",
    "compiler/seed-c/fixtures/integer-widening.w",
    "benchmarks/executable/numeric_widening.c",
    "benchmarks/executable/numeric_widening.rs",
    "compiler/seed-c/fixtures/numeric-widening.w",
    "benchmarks/executable/integer_truncating_bits.c",
    "benchmarks/executable/integer_truncating_bits.rs",
    "compiler/seed-c/fixtures/integer-truncating-bits.w",
    "benchmarks/executable/integer_bitwise.c",
    "benchmarks/executable/integer_bitwise.rs",
    "compiler/seed-c/fixtures/integer-bitwise.w",
    "benchmarks/executable/uint_bitwise.c",
    "benchmarks/executable/uint_bitwise.rs",
    "compiler/seed-c/fixtures/uint-bitwise.w",
    "benchmarks/executable/uint_overflowing_family.c",
    "benchmarks/executable/uint_overflowing_family.rs",
    "compiler/seed-c/fixtures/uint-overflowing-family.w",
    "benchmarks/executable/uint_saturating_policy.c",
    "benchmarks/executable/uint_saturating_policy.rs",
    "compiler/seed-c/fixtures/uint-saturating-policy.w",
    "benchmarks/executable/float_bit_representation.c",
    "benchmarks/executable/float_bit_representation.rs",
    "compiler/seed-c/fixtures/float-bit-representation.w",
  ];
  for (const sourcePath of requiredOptInPaths) {
    assert.ok(optInPaths.has(sourcePath), `${sourcePath} must retain its local oracle`);
  }
  assert.ok(optInPaths.size >= requiredOptInPaths.length);

  const legacySource = "// Existing benchmark source without a local oracle.\nentry {}\n";
  assert.equal(parseExecutableSourceExpectation(legacySource), undefined);
  assert.deepEqual(validateExecutableSourceExpectation(legacySource, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 1,
    stdout: "different\n",
    stderr: "",
  }), []);

  const compact = [
    "// Expected exit: 7",
    "// Expected stdout:",
    "// first",
    "//",
    "// Expected stderr:",
    "// warning",
    "",
    "entry {}",
  ].join("\n");
  assert.deepEqual(parseExecutableSourceExpectation(compact), {
    exitCode: 7,
    stdout: "first\n\n",
    stderr: "warning\n",
    errors: [],
  });
  assert.match(validateExecutableSourceExpectation(
    "// Expected stdout:\n// output\nentry {}\n",
    { kind: "exact-output", status: "source-backed", exitCode: 0, stdout: "output\n", stderr: "" },
  ).join("\n"), /Expected exit marker is required/u);
  assert.deepEqual(validateExecutableSourceExpectation(
    "// Expected exit: 0\n// Expected stdout:\n// output\nentry {}\n",
    { kind: "exact-output", status: "source-backed", exitCode: 0, stdout: "different\n", stderr: "" },
    "drifted source",
  ), ["drifted source: Expected stdout must match the catalog oracle exactly."]);

  const argumentCases = [
    { arguments: [], exitCode: 7, stdout: "missing\n", stderr: "" },
    { arguments: [""], exitCode: 0, stdout: "present\n", stderr: "" },
  ];
  const argumentCaseSource = [
    "// Expected output cases (argv => exit; stdout):",
    '// [] => 7; "missing\\n"',
    '// [""] => 0; "present\\n"',
    "entry {}",
  ].join("\n");
  assert.deepEqual(parseExecutableSourceExpectation(argumentCaseSource), {
    cases: argumentCases,
    errors: [],
  });
  assert.deepEqual(validateExecutableSourceExpectation(argumentCaseSource, {
    kind: "argument-dependent-output", status: "source-backed", cases: argumentCases,
  }), []);
  assert.match(validateExecutableSourceExpectation(argumentCaseSource, {
    kind: "argument-dependent-output", status: "source-backed", cases: [...argumentCases, argumentCases[0]],
  }).join("\n"), /case count must match/u);
});

test("platform-minimal Hello stays a separate correctness-only comparison across supported x64 targets", () => {
  const workload = documents.catalog.workloads.find((item) => item.id === "hello-platform-minimal");
  assert.ok(workload);
  assert.equal(workload.benchmarkStatus, "contextual-measurement-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.lane, "equivalent");
  assert.match(workload.scope, /platform-minimal Hello correctness comparison/u);
  assert.match(workload.scope, /not an idiomatic C\/Rust baseline or language ranking/u);
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.oracle, {
    kind: "exact-output", status: "source-backed", exitCode: 0,
    stdout: "Hello, world!\n", stderr: "",
  });
  assert.deepEqual(workload.sources.map((source) => `${source.language}/${source.platformTarget}`).sort(), [
    "c/linux-wsl-x64", "c/windows-x64", "rust/linux-wsl-x64", "rust/windows-x64",
    "w/linux-wsl-x64", "w/windows-x64",
  ]);
  assert.ok(workload.sources.every((source) => source.recipeClass === "hello-platform-minimal"));
  const contextualMetrics = documents.catalog.bestMetrics.entries.filter((entry) => entry.workloadId === workload.id);
  assert.equal(contextualMetrics.length, 36, "all six contextual Hello lanes have complete metric sets");
  assert.ok(contextualMetrics.every((entry) => {
    const source = workload.sources.find((item) => item.language === entry.language && item.platformTarget === entry.platformTarget);
    return source && entry.eligibility === source.eligibility && entry.comparability === source.comparability &&
      entry.eligibility !== "promotable-after-equivalence";
  }), "any contextual measurements must retain their lane's non-ranking classification");
  assert.ok(workload.sources.filter((source) => source.language === "w").every((source) =>
    source.recipe === "public-w-build-release"));
  assert.ok(workload.sources.filter((source) => source.language === "c").every((source) =>
    source.recipe === "clang-c23-freestanding"));
  assert.ok(workload.sources.filter((source) => source.language === "rust").every((source) =>
    source.recipe === "rustc-edition-2024-no-std"));
});

test("float rounding catalog scope covers only the success witness, not its separate failure gate", () => {
  const workload = documents.catalog.workloads.find((item) => item.id === "float-integer-rounding");
  assert.ok(workload);
  assert.match(workload.scope, /successful constant nearest-even conversion/u);
  assert.match(workload.scope, /process-float-rounding-error\.w gate/u);
  assert.match(workload.scope, /only the success result/u);
  assert.deepEqual(workload.sources.map((source) => source.path), [
    "compiler/seed-c/fixtures/process-float-rounding-success.w",
    "compiler/seed-c/fixtures/process-float-rounding-success.w",
  ]);
  assert.equal(documents.catalog.bestMetrics.entries.some((entry) => entry.workloadId === workload.id), false);
});

test("local module graph source digest is current and stale live cells stay pruned", () => {
  const workload = documents.catalog.workloads.find((item) => item.id === "local-module-graph");
  assert.ok(workload);
  const source = workload.sources.find((item) => item.path === "compiler/seed-c/fixtures/local-graph/app.w");
  assert.equal(source.digest, exactOutputDigest(readFileSync(`${ROOT}/${source.path}`, "utf8")));
  assertCurrentMetricLanes(workload,
    documents.catalog.bestMetrics.entries.filter((entry) => entry.workloadId === workload.id),
    "local module graph");
});

test("every public Windows runnable fixture has an executable benchmark owner", () => {
  const missing = clone(documents.catalog);
  const workload = missing.workloads.find((item) => item.id === "enum-switch");
  workload.sources = workload.sources.filter((source) => source.language !== "w");
  workload.blockedLanguages.push("w");
  assert.match(validateExecutableCatalog(missing, { ...documents, catalog: missing }).join("\n"),
    /enum\.w has no executable benchmark owner/u);
});

test("runtime fixed-integer arithmetic remains correctness-only despite equivalent references", () => {
  const workload = documents.catalog.workloads.find((item) => item.id === "fixed-integer-runtime-arithmetic");
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.oracle.kind, "argument-dependent-output");
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.exitCode), [0, 2, 1]);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.stdout),
    ["Arithmetic 0/4/1\n", "", ""]);
  assert.deepEqual(workload.sources.map((source) => source.language), ["w", "c", "rust"]);
  assert.deepEqual(workload.blockedLanguages, []);
  assert.ok(workload.blockers.includes("steady-runtime-family-workload"));
  assert.ok(EXECUTABLE_RUN_TARGETS.includes(workload.id));
  assert.equal(documents.catalog.bestMetrics.entries.some(
    (entry) => entry.workloadId === workload.id), false);
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
  assertCurrentMetricLanes(workload, liveMetrics, "process-entry");
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
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.exitCode), [7, 0, 0, 0]);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.stdout), [
    "arguments-missing count=0 amount=17 over-limit=false\n",
    "arguments-present count=1 amount=17 over-limit=false\n",
    "arguments-present count=2 amount=17 over-limit=false\n",
    "arguments-present count=3 amount=17 over-limit=true\n",
  ]);
  assert.ok(workload.sources.every((source) => source.recipeClass === PROCESS_ENUM_PAYLOAD_RECIPE_CLASS));
  assert.equal(workload.sources.find((source) => source.language === "w").entry, "dispatch");
  assert.equal(workload.sources.find((source) => source.language === "c").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MSVC);
  assert.equal(workload.sources.find((source) => source.language === "rust").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MSVC);
  const liveMetrics = documents.catalog.bestMetrics.entries.filter((entry) => entry.workloadId === PROCESS_ENUM_PAYLOAD_WORKLOAD_ID);
  assertCurrentMetricLanes(workload, liveMetrics, "process-enum-payload");
});

test("process-enum-payload C and Rust variants retain independent runtime enum paths", () => {
  const c = readFileSync(`${ROOT}/benchmarks/executable/process_enum_payload.c`, "utf8");
  const rust = readFileSync(`${ROOT}/benchmarks/executable/process_enum_payload.rs`, "utf8");
  assert.match(c, /int main\(int argc, char \*\*argv\)/u);
  assert.match(c, /enum argument_state_kind/u);
  assert.match(c, /union \{/u);
  assert.match(c, /int64_t amount/u);
  assert.match(c, /static struct argument_state build_argument_state/u);
  assert.match(c, /switch \(state\.kind\)/u);
  assert.match(c, /argc\s*==\s*1/u);
  assert.match(c, /printf\("arguments-missing count=%d amount=%lld over-limit=%s\\n"/u);
  assert.match(c, /over_limit \? "true" : "false"/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.match(rust, /fn main\(\)/u);
  assert.match(rust, /enum ArgumentState/u);
  assert.match(rust, /fn build_argument_state/u);
  assert.match(rust, /match state/u);
  assert.match(rust, /let count = std::env::args_os\(\)\.count\(\)\.saturating_sub\(1\)/u);
  assert.match(rust, /write!\(stdout, "\{label\} count=\{count\} amount=\{amount\} over-limit=\{over_limit\}\\n"\)/u);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("strict f32/f64 references retain independent runtime operations", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === FLOAT_STRICT_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-float-compile-time-folded",
    "runtime-float-equivalence",
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
    `${ROOT}/benchmarks/executable/float_strict.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/float_strict.rs`, "utf8");
  assert.match(c, /volatile float/u);
  assert.match(c, /volatile double/u);
  assert.match(c, /f32_sum_left \+ f32_sum_right/u);
  assert.match(c, /f64_sum_left \+ f64_sum_right/u);
  assert.match(c, /difference_left - f(?:32|64)_difference_right/u);
  assert.match(c, /product_left \* f(?:32|64)_product_right/u);
  assert.match(c, /quotient_left \/ f(?:32|64)_quotient_right/u);
  assert.match(c, /nan != nan/u);
  assert.doesNotMatch(c, /fast-math/iu);
  assert.match(rust, /black_box/u);
  assert.match(rust, /1\.5_f32/u);
  assert.match(rust, /1\.5_f64/u);
  assert.match(rust, /nan != nan/u);
  assert.doesNotMatch(rust, /fast-math/iu);
});

test("f32/f64 bit-representation family is correctness-only and deferred", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === FLOAT_BIT_REPRESENTATION_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "not-run");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate f32.fromBits(u32)/toBits() and f64.fromBits(u64)/toBits() round trips for negative zero, positive infinity, and one quiet-NaN payload per width, printing exact unsigned decimal bit patterns. W's literal inputs and C23/Rust runtime inputs are correctness-only until equivalent runtime work is established.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Float bits f32 2147483648/2139095040/2143363909 f64 9223372036854775808/9218868437227405312/9221140253039434428\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-float-bit-representation-literal-folding",
    "runtime-float-bit-representation-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "float-bit-representation-release" &&
    source.quality === "correctness-gate"));
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
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === FLOAT_BIT_REPRESENTATION_WORKLOAD_ID),
  "float bit-representation correctness references must not acquire timing or ranking data");

  const c = readFileSync(
    `${ROOT}/benchmarks/executable/float_bit_representation.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/float_bit_representation.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/float-bit-representation.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: workload.oracle.exitCode,
      stdout: workload.oracle.stdout,
      stderr: "",
      errors: [],
    }, `${name} source must declare the exact local oracle`);
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle, `${name} source`), []);
  }
  for (const bits of [
    "0x80000000", "0x7f800000", "0x7fc12345",
    "0x8000000000000000", "0x7ff0000000000000", "0x7ff8123456789abc",
  ]) {
    assert.ok(w.includes(bits), `W fixture must retain ${bits}`);
    assert.ok(c.includes(bits), `C23 reference must retain ${bits}`);
    assert.ok(rust.includes(bits), `Rust reference must retain ${bits}`);
  }
  assert.match(w, /f32\.fromBits\([^\n]+\)/u);
  assert.match(w, /f64\.fromBits\([^\n]+\)/u);
  assert.match(w, /\.toBits\(\)/u);
  assert.match(c, /memcpy\(&value, &bits/u);
  assert.match(c, /memcpy\(&result, &value/u);
  assert.match(c, /volatile uint32_t/u);
  assert.match(c, /volatile uint64_t/u);
  assert.match(rust, /f32::from_bits\(black_box/u);
  assert.match(rust, /f64::from_bits\(black_box/u);
  assert.match(rust, /\.to_bits\(\)/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("checked fixed-width integer arithmetic is one correctness-only family", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === CHECKED_INTEGER_ARITHMETIC_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate successful fixed-input checked +, -, *, /, %, +=, -=, *=, /=, and %= across signed and unsigned i8/i16/i32/i64 and the current x86-64 Int/UInt aliases with exact output. Fault behavior is covered by the W run gates only; C23 and Rust are correctness references for the successful domain.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "i8 -9/-15/-36; divrem -4/0; compound -2\nu8 43/37/120; divrem 13/1; compound 2\ni16 -970/-1030/-30000; divrem -33/-10; compound -12\nu16 1030/970/30000; divrem 33/10; compound 8\ni32 -117000/-123000/-360000000; divrem -40/0; compound -2\nu32 100300/99700/30000000; divrem 333/100; compound 98\ni64 -600000/-1200000/-270000000000; divrem -3/0; compound -2\nu64 6000000000/4000000000/5000000000000000000; divrem 5/0; compound 999999998\nInt -4000000000/-6000000000/-5000000000000000000; divrem -5/0; compound -2\nUInt 9000000000/3000000000/18000000000000000000; divrem 2/0; compound 2999999998\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-fixed-input-checked-integer-arithmetic",
    "runtime-checked-integer-arithmetic-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) => [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) => source.recipeClass === "checked-integer-arithmetic-release"));
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
    `${ROOT}/benchmarks/executable/checked_integer_arithmetic.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/checked_integer_arithmetic.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/checked-integer-arithmetic.w`, "utf8");
  for (const source of [w, c, rust])
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: workload.oracle.exitCode,
      stdout: workload.oracle.stdout,
      stderr: workload.oracle.stderr,
      errors: [],
    });
  assert.match(c, /volatile TYPE runtime_left/u);
  assert.match(c, /runtime_left \+ runtime_right/u);
  assert.match(c, /runtime_left - runtime_right/u);
  assert.match(c, /runtime_left \* runtime_right/u);
  assert.match(c, /runtime_left \/ runtime_right/u);
  assert.match(c, /runtime_left % runtime_right/u);
  assert.match(c, /compound \+= runtime_right/u);
  assert.match(c, /compound -= \(TYPE\)2/u);
  assert.match(c, /compound \*= \(TYPE\)2/u);
  assert.match(c, /compound \/= \(TYPE\)2/u);
  assert.match(c, /compound %= runtime_right/u);
  assert.match(rust, /black_box/u);
  assert.match(rust, /type Int = i64/u);
  assert.match(rust, /type UInt = u64/u);
  assert.match(rust, /left \+= right/u);
  assert.match(rust, /left -= 2 as \$type/u);
  assert.match(rust, /left \*= 2 as \$type/u);
  assert.match(rust, /left \/ right/u);
  assert.match(rust, /left % right/u);
  assert.match(rust, /left \/= 2 as \$type/u);
  assert.match(rust, /left %= right/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi|checked_(?:add|div|rem))/iu);
});

test("UInt bit-primitives family catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === UINT_BITWISE_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.match(workload.scope, /representative fixed-input UInt bit-primitives family/u);
  assert.match(workload.scope, /without separate benchmark rows/u);
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout:
      "Not 18446744073709551615\nAnd 0\nOr 18446744073709551615\n" +
      "Xor 18446744073709551615\nOnes 32\nZeros 32\nLeading 56\n" +
      "Leading zero 64\nTrailing 12\nTrailing zero 64\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "uint-bitwise-release"));
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/uint-bitwise.w`, "utf8");
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/uint_bitwise.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/uint_bitwise.rs`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle,
      `${name} source`), []);
  }
  for (const helper of ["count_ones_u64", "count_leading_zeros_u64",
    "count_trailing_zeros_u64"]) {
    assert.match(c, new RegExp(helper));
  }
  for (const operation of [".count_ones()", ".count_zeros()",
    ".leading_zeros()", ".trailing_zeros()"]) {
    assert.ok(rust.includes(operation));
  }
  assert.match(c, /volatile uint64_t runtime_inputs/u);
  assert.match(rust, /black_box/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("fixed-integer bit-primitives family covers the portable signed/unsigned matrix", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === FIXED_INTEGER_BIT_PRIMITIVES_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.match(workload.scope, /all eight portable bit primitives/u);
  assert.match(workload.scope, /signed and unsigned i8\/i16\/i32\/i64/u);
  assert.match(workload.scope, /one public W witness per signed and unsigned i8\/i16\/i32\/i64 type and operation/u);
  assert.match(workload.scope, /C23\/Rust references and focused unit coverage/u);
  assert.match(workload.scope, /zero-input width counts/u);
  assert.match(workload.scope, /rotations reduced modulo width at 0\/width\/width\+1/u);
  assert.match(workload.scope, /two's-complement reversal of negative signed values/u);
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout:
      "i8 3/5/1/1 74/127 82 -92/41\n" +
      "u8 4/4/0/1 105 150 45/75\n" +
      "i16 5/11/3/2 11336/32767 13330 9320/2330\n" +
      "u16 8/8/0/0 54673 43913 4951/50389\n" +
      "i32 13/19/3/3 510274632/2147483647 2018915346 610839792/152709948\n" +
      "u32 20/12/0/0 4155757969 4023233417 324508639/3302352631\n" +
      "i64 30/34/7/1 8553414939923104896/9223372036854775807 7984226321029210881 163971058432973532/40992764608243383\n" +
      "u64 32/32/0/4 597899502893742975 1167088121787636990 18282773015276577825/9182379272246532360\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-fixed-integer-bit-primitives-compile-time-folded",
    "runtime-fixed-integer-bit-primitives-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "fixed-integer-bit-primitives-release" &&
    source.quality === "correctness-gate"));
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === FIXED_INTEGER_BIT_PRIMITIVES_WORKLOAD_ID),
  "the correctness-only family must not have timings or ranking cells");

  const sources = Object.fromEntries(workload.sources
    .filter((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET)
    .map((source) => [source.language,
      readFileSync(`${ROOT}/${source.path}`, "utf8")]));
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/fixed-integer-bit-primitives.w`, "utf8");
  assert.ok(Buffer.byteLength(w, "utf8") <= 4096,
    "the public fixture must fit the Native0 source-byte limit");
  for (const [name, source] of Object.entries(sources))
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle,
      `${name} source`), []);
  assert.deepEqual(validateExecutableSourceExpectation(w, workload.oracle,
    "W public-run source"), []);
  for (const type of ["i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64"]) {
    for (const operation of ["countOnes", "countZeros", "countLeadingZeros",
      "countTrailingZeros", "reversedBits", "reversedBytes", "rotatedLeft",
      "rotatedRight"])
      assert.match(w, new RegExp(`\\b${type}\\.${operation}\\(`));
  }
  for (const pattern of [
    /reversedBits\(~1_i8\)/u,
    /reversedBits\(~1_i16\)/u,
    /reversedBits\(~1_i32\)/u,
    /reversedBits\(~1_i64\)/u,
    /rotatedLeft\(0x52_i8, 9_u64\)/u,
    /rotatedRight\(0xfedcba9876543210_u64, 65_u64\)/u,
  ]) assert.match(w, pattern);
  assert.match(sources.c, /static volatile int64_t runtime_i64/u);
  assert.match(sources.c, /static volatile uint64_t rotations64/u);
  assert.match(sources.c, /signed_bits_value/u);
  assert.match(sources.c, /zero_leading != width/u);
  assert.match(sources.c, /left_width != input/u);
  assert.match(sources.rust, /use std::hint::black_box/u);
  assert.match(sources.rust, /signed_row_values!\(i64/u);
  assert.match(sources.rust, /unsigned_row_values!\(u64/u);
  assert.match(sources.rust, /assert_signed_edges!/u);
  assert.match(sources.rust, /assert_unsigned_edges!/u);
  for (const gatePath of [
    "tooling/check-w-run-windows.mjs",
    "tooling/check-w-run.mjs",
    "tooling/check-mlir0.mjs",
  ]) {
    const gate = readFileSync(`${ROOT}/${gatePath}`, "utf8");
    assert.match(gate, /fixed-integer-bit-primitives\.w/u,
      `${gatePath} must register the family witness`);
    assert.match(gate, /fixedIntegerBitPrimitivesOutput/u,
      `${gatePath} must check the exact family oracle`);
  }
  assert.doesNotMatch(sources.c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(sources.rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("integer shift semantics is one correctness-only checked and policy family", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === INTEGER_SHIFT_SEMANTICS_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(documents.catalog.workloads.some((item) =>
    ["shifts", "fixed-integer-shift-policies"].includes(item.id)), false,
  "the component families must not retain independent benchmark rows");
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "not-run",
    "the combined W witness has not been executed through the native runner");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.match(workload.scope, /checked ordinary arithmetic-right and non-overflowing left shifts/u);
  assert.match(workload.scope, /Int\/UInt aliases/u);
  assert.match(workload.scope, /masked left\/right and logical right policies/u);
  assert.match(workload.scope, /signed and unsigned i8\/u8 through i64\/u64/u);
  assert.match(workload.scope, /width and width\+1/u);
  assert.match(workload.scope, /logical counts at zero and width-1/u);
  assert.match(workload.scope, /bounded native route does not yet execute this combined W witness/u);
  assert.match(workload.scope, /C23 and Rust retain runtime inputs while individual W operations may fold literals/u);
  assert.match(workload.scope, /performance ranking is deferred until the W route executes equivalent runtime work/u);
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout:
      "i8 -16/-128\nu8 32/128\ni16 -4096/-32768\nu16 8192/32768\n" +
      "i32 -268435456/-2147483648\nu32 536870912/2147483648\n" +
      "i64 -1152921504606846976/-9223372036854775808\n" +
      "u64 2305843009213693952/9223372036854775808\n" +
      "Int -1152921504606846976/-9223372036854775808\n" +
      "UInt 2305843009213693952/9223372036854775808\n" +
      "i8 -128/0/-64/64\nu8 128/0/64/64\n" +
      "i16 -32768/0/-16384/16384\nu16 32768/0/16384/16384\n" +
      "i32 -2147483648/0/-1073741824/1073741824\n" +
      "u32 2147483648/0/1073741824/1073741824\n" +
      "i64 -9223372036854775808/0/-4611686018427387904/4611686018427387904\n" +
      "u64 9223372036854775808/0/4611686018427387904/4611686018427387904\n" +
      "u64 small-value 2/64/64\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-integer-shift-semantics-combined-native-route",
    "w-integer-shift-semantics-compile-time-folded",
    "runtime-integer-shift-semantics-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "integer-shift-semantics-release" &&
    source.quality === "correctness-gate"));
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
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === INTEGER_SHIFT_SEMANTICS_WORKLOAD_ID),
  "the family must have no timings or ranking cells");
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    ["shifts", "fixed-integer-shift-policies"].includes(entry.workloadId)),
  "the removed component rows must not retain timing or ranking cells");

  const sources = Object.fromEntries(workload.sources
    .filter((source) => source.platformTarget === EXECUTABLE_PLATFORM_TARGET)
    .map((source) => [source.language,
      readFileSync(`${ROOT}/${source.path}`, "utf8")]));
  const w = sources.w;
  assert.ok(Buffer.byteLength(w, "utf8") <= 4096,
    "the combined public witness must fit the Native0 source-byte limit");
  for (const [name, source] of Object.entries(sources)) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: 0,
      stdout: workload.oracle.stdout,
      stderr: "",
      errors: [],
    }, `${name} source must declare the exact local oracle`);
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle,
      `${name} source`), []);
  }
  for (const type of ["i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64"]) {
    for (const operation of ["maskedShiftLeft", "maskedShiftRight",
      "logicalShiftRight"])
      assert.match(w, new RegExp(`\\b${type}\\.${operation}\\(`));
  }
  for (const pattern of [
    /i8\.maskedShiftLeft\(policyI8, 8_u64\)/u,
    /i64\.maskedShiftLeft\(policyI64, 65_u64\)/u,
    /i64\.maskedShiftRight\(policyI64, 65_u64\)/u,
    /i64\.logicalShiftRight\(policyI64, 1_u64\)/u,
    /u64\.maskedShiftLeft\(1_u64, 65_u64\)/u,
    /u64\.maskedShiftRight\(128_u64, 65_u64\)/u,
    /u64\.logicalShiftRight\(128_u64, 1_u64\)/u,
  ]) assert.match(w, pattern);
  assert.match(sources.c, /static volatile uint64_t policy_inputs/u);
  assert.match(sources.c, /count & \(uint64_t\)\(width - 1U\)/u);
  assert.match(sources.c, /logical_max != UINT64_C\(1\)/u);
  assert.match(sources.c, /is_signed && shift != 0U/u);
  assert.match(sources.rust, /use std::hint::black_box/u);
  assert.match(sources.rust, /wrapping_shl\(black_box\(width \+ 1\)\)/u);
  assert.match(sources.rust, /wrapping_shr\(black_box\(width \+ 1\)\)/u);
  assert.match(sources.rust, /logical_max, 1 as \$signed/u);
  assert.match(sources.rust, /\(value as \$unsigned\) >> black_box\(1\)/u);
  for (const gatePath of [
    "tooling/build-w-windows.mjs",
    "tooling/check-w-run-windows.mjs",
    "tooling/check-w-run.mjs",
    "tooling/check-mlir0.mjs",
  ]) {
    const gate = readFileSync(`${ROOT}/${gatePath}`, "utf8");
    assert.match(gate, /fixed-integer-shift-policies\.w/u,
      `${gatePath} must register the family witness`);
    if (gatePath === "tooling/build-w-windows.mjs") {
      assert.match(gate, /id: "fixed-integer-shift-policies"/u,
        `${gatePath} must register the family smoke`);
      assert.match(gate, /expectedStdout:\s*"i8 -128\/0\/-64\/64/u,
        `${gatePath} must check the exact family oracle`);
    } else {
      assert.match(gate, /fixedIntegerShiftPoliciesOutput/u,
        `${gatePath} must check the exact family oracle`);
    }
  }
  assert.doesNotMatch(sources.c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(sources.rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("UInt overflowing-family catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === UINT_OVERFLOWING_FAMILY_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.match(workload.scope, /covering addition, subtraction, multiplication, negation, and exponentiation/u);
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Overflowing family add 0/true,11/false; subtract 41/false,18446744073709551615/true; multiply 42/false,18446744073709551614/true; negate 0/false,18446744073709551615/true; power 9223372036854775808/false,0/true,1/true,1/false\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-overflowing-family-compile-time-folded",
    "runtime-uint-overflowing-family-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "uint-overflowing-family-release"));
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
    `${ROOT}/benchmarks/executable/uint_overflowing_family.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/uint_overflowing_family.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/uint-overflowing-family.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle,
      `${name} source`), []);
  }
  assert.match(c, /overflowing_add_u64/u);
  assert.match(c, /volatile uint64_t runtime_subtract_left/u);
  assert.match(c, /volatile uint64_t runtime_multiply_left/u);
  assert.match(c, /overflowing_subtract_u64/u);
  assert.match(c, /overflowing_multiply_u64/u);
  assert.match(c, /overflowing_negate_u64/u);
  assert.match(c, /overflowing_power_u64/u);
  assert.match(c, /left < right/u);
  assert.match(c, /UINT64_MAX \/ left/u);
  assert.match(c, /value != UINT64_C\(0\)/u);
  assert.match(rust, /black_box\(42_u64\)/u);
  assert.match(rust, /black_box\(u64::MAX\)/u);
  assert.match(rust, /\.overflowing_sub\(/u);
  assert.match(rust, /\.overflowing_mul\(/u);
  assert.match(rust, /\.overflowing_neg\(\)/u);
  assert.match(rust, /\.overflowing_add\(/u);
  assert.match(rust, /overflowing_power/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("UInt saturating-policy catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === UINT_SATURATING_POLICY_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.match(workload.scope, /covering addition, subtraction, multiplication, negation, and exponentiation/u);
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Saturating policy add 18446744073709551615/11; subtract 0/10; multiply 18446744073709551615/42; negate 0/0; power 8/18446744073709551615/1\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-uint-saturating-policy-compile-time-folded",
    "runtime-uint-saturating-policy-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "uint-saturating-policy-release"));
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
    `${ROOT}/benchmarks/executable/uint_saturating_policy.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/uint_saturating_policy.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/uint-saturating-policy.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle,
      `${name} source`), []);
  }
  assert.match(c, /saturating_add_u64/u);
  assert.match(c, /saturating_subtract_u64/u);
  assert.match(c, /volatile uint64_t runtime_negate_zero/u);
  assert.match(c, /volatile uint64_t runtime_negate_maximum/u);
  assert.match(c, /saturating_negate_u64/u);
  assert.match(c, /saturating_power_u64/u);
  assert.match(c, /saturating_multiply_u64/u);
  assert.match(c, /exponent >>= 1/u);
  assert.match(c, /UINT64_MAX \/ left/u);
  assert.match(rust, /black_box\(0_u64\)/u);
  assert.match(rust, /black_box\(u64::MAX\)/u);
  assert.match(rust, /\.saturating_sub\(/u);
  assert.match(rust, /\.saturating_add\(/u);
  assert.match(rust, /\.saturating_mul\(/u);
  assert.match(rust, /exponent >>= 1/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("UInt compound catalog keeps correctness separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === UINT_COMPOUND_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate fixed-input full-width UInt compound assignment for all eleven operators on one mutable local, with exact unsigned decimal output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.equal(workload.oracle.stdout,
    "UInt compound 4611686018427387907/4611686018427387906/9223372036854775812/4611686018427387906/4611686018427387906/4611686018427387906/9223372036854775812/4611686018427387906/2/87/95\n");
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
    source.recipeClass === "uint-compound-release"));
  const c = readFileSync(
    `${ROOT}/benchmarks/executable/uint_compound.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/uint_compound.rs`, "utf8");
  for (const helper of ["checked_add_u64", "checked_subtract_u64",
    "checked_multiply_u64", "checked_divide_u64",
    "checked_remainder_u64", "checked_power_u64",
    "checked_shift_left_u64", "checked_shift_right_u64"]) {
    assert.match(c, new RegExp(helper));
  }
  for (const operation of [".checked_add(", ".checked_sub(",
    ".checked_mul(", ".checked_div(", ".checked_rem("]) {
    assert.ok(rust.includes(operation));
  }
  assert.match(rust, /checked_power_u64/u);
  assert.match(rust, /checked_shift_left_u64/u);
  assert.match(rust, /checked_shift_right_u64/u);
  assert.match(c, /value &= runtime_and/u);
  assert.match(c, /value \^= runtime_xor/u);
  assert.match(c, /value \|= runtime_or/u);
  assert.match(rust, /value &= black_box\(255_u64\)/u);
  assert.match(rust, /value \^= black_box\(85_u64\)/u);
  assert.match(rust, /value \|= black_box\(10_u64\)/u);
  assert.match(rust, /value >> \(64 - count\)/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("fixed-width integer prefix family is one correctness-only witness", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === INTEGER_PREFIX_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate fixed-input signed unary negation and bitwise complement over i8/i16/i32/i64 and Int plus unsigned complement over u8/u16/u32/u64 and UInt, alongside literal prefix negation. W may fold literal call arguments while C23 and Rust preserve runtime operands, so performance ranking is deferred until runtime-equivalent W work exists.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout:
      "i8 -7/-43\ni16 -7/-43\ni32 -7/-43\ni64 -7/-43\nInt -7/-43\n" +
      "u8 170\nu16 65450\nu32 4294967210\nu64 18446744073709551530\n" +
      "UInt 18446744073709551530\nliteral -7\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-integer-prefix-compile-time-folded",
    "runtime-integer-prefix-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "integer-prefix-release" &&
    source.quality === "correctness-gate"));
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
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === INTEGER_PREFIX_WORKLOAD_ID),
  "correctness-only integer prefixes must not acquire timing or ranking data");

  const c = readFileSync(
    `${ROOT}/benchmarks/executable/integer_prefix.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/integer_prefix.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/integer-prefix.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: workload.oracle.exitCode,
      stdout: workload.oracle.stdout,
      stderr: workload.oracle.stderr,
      errors: [],
    }, `${name} source must declare the exact local oracle`);
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle, `${name} source`), []);
  }
  assert.match(w, /fn negate_i8\(value: i8\): i8 \{ return -value \}/u);
  assert.match(w, /fn complement_u64\(value: u64\): u64 \{ return ~value \}/u);
  assert.match(w, /negate_i8\(value: 7_i8\)/u);
  assert.match(w, /complement_u64\(value: 85_u64\)/u);
  assert.match(c, /volatile int8_t signed8/u);
  assert.match(c, /volatile uint64_t uint_value/u);
  assert.match(c, /\(int8_t\)~bits8/u);
  assert.match(c, /\(uint64_t\)~uint_value/u);
  assert.match(rust, /black_box\(7_i8\)/u);
  assert.match(rust, /black_box\(0x55_u64\)/u);
  assert.match(rust, /!bits8/u);
  assert.match(rust, /!uint_value/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);

  for (const id of ["unary-negate", "unary-interpolation"]) {
    assert.ok(!documents.catalog.workloads.some((item) => item.id === id),
      `${id} must no longer be an independent benchmark row`);
    assert.ok(!documents.catalog.bestMetrics.entries.some((entry) => entry.workloadId === id),
      `${id} must not retain live best-metric cells`);
  }
  readFileSync(`${ROOT}/compiler/seed-c/fixtures/unary-negate.w`, "utf8");
  readFileSync(`${ROOT}/compiler/seed-c/fixtures/unary-interpolation.w`, "utf8");
  assert.ok(!documents.catalog.workloads.some((item) => item.id === "bitwise"),
    "signed-i64 binary bitwise no longer has an atom-level workload row");
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) => entry.workloadId === "bitwise"),
    "old signed-i64 fixture metrics must not be transferred to the wider family");
  assert.ok(documents.catalog.workloads.some((item) => item.id === INTEGER_BITWISE_WORKLOAD_ID));
  assert.ok(documents.catalog.workloads.some((item) => item.id === UINT_BITWISE_WORKLOAD_ID));
});

test("fixed-width integer wrapping policy catalog keeps one matrix separate from ranking", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === INTEGER_WRAPPING_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate fixed-input signed and unsigned i8/i16/i32/i64 plus Int/UInt wrapping-policy representatives for add, subtract, multiply, negate, power, and left shift with exact five-line decimal output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout:
      "i8/u8 -128/0\ni16/u16 32767/2\ni32/u32 -2/4294967295\n" +
      "i64/u64 -9223372036854775808/0\n" +
      "Int/UInt -9223372036854775808/18446744073709551615\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-integer-wrapping-compile-time-folded",
    "runtime-integer-wrapping-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "integer-wrapping-release"));
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
    `${ROOT}/benchmarks/executable/integer_wrapping.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/integer_wrapping.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/integer-wrapping.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: workload.oracle.exitCode,
      stdout: workload.oracle.stdout,
      stderr: workload.oracle.stderr,
      errors: [],
    }, `${name} source must declare the compact local oracle`);
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle, `${name} source`), []);
  }
  assert.match(w, /i16\.wrappingSubtract\(-32767_i16, 2_i16\)/u);
  assert.match(w, /let signed64Min: i64 = i64\.wrappingAdd\(9223372036854775807_i64, 1_i64\)/u);
  assert.match(w, /let signed64: i64 = i64\.wrappingNegate\(signed64Min\)/u);
  assert.doesNotMatch(w, /-32768_i16|9223372036854775808_i64/u);
  assert.match(c, /volatile uint64_t runtime_inputs/u);
  assert.match(c, /width_mask\(8\)/u);
  assert.match(c, /wrapping_power/u);
  assert.match(c, /PRINT_PAIR\("i8\/u8"/u);
  assert.match(c, /PRINT_PAIR\("Int\/UInt"/u);
  assert.match(rust, /black_box\(i8::MAX\)/u);
  assert.match(rust, /black_box\(-32767_i16\)/u);
  assert.match(rust, /black_box\(2_i16\)/u);
  assert.match(rust, /black_box\(i32::MAX\)/u);
  assert.match(rust, /let signed64_min = black_box\(i64::MAX\)\.wrapping_add\(black_box\(1_i64\)\)/u);
  assert.match(rust, /let signed64 = black_box\(signed64_min\)\.wrapping_neg\(\)/u);
  assert.match(c, /runtime_inputs\[6\] \+ runtime_inputs\[7\]/u);
  assert.match(c, /const uint64_t signed64 = UINT64_C\(0\) - signed64_min/u);
  assert.match(rust, /\.wrapping_add\(/u);
  assert.match(rust, /\.wrapping_sub\(/u);
  assert.match(rust, /\.wrapping_mul\(/u);
  assert.match(rust, /\.wrapping_neg\(\)/u);
  assert.match(rust, /\.wrapping_pow\(/u);
  assert.match(rust, /\.wrapping_shl\(/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("implicit integer widening catalog is one correctness-only family witness", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === INTEGER_WIDENING_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate representative fixed-width and x86-64 Int implicit integer widenings across return, call argument, local binding, and unsigned-to-signed cases with exact output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Widen -7/200/202/203\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-integer-widening-compile-time-folded",
    "runtime-integer-widening-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "integer-widening-release" &&
    source.quality === "correctness-gate"));
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
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === INTEGER_WIDENING_WORKLOAD_ID),
  "correctness-only widening must not acquire timing or ranking data");

  const c = readFileSync(
    `${ROOT}/benchmarks/executable/integer_widening.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/integer_widening.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/integer-widening.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: 0,
      stdout: workload.oracle.stdout,
      stderr: "",
      errors: [],
    }, `${name} source must declare the exact local oracle`);
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle, `${name} source`), []);
  }
  assert.match(w, /widenReturn\(value: -7_i8\)/u);
  assert.match(w, /accept\(value: 200_u8\)/u);
  assert.match(w, /let unsigned: u16 = 202_u8/u);
  assert.match(w, /let alias: Int = 203_u8/u);
  assert.match(c, /volatile (?:int8_t|uint8_t)/u);
  assert.match(rust, /black_box\(-7_i8\)/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("exact numeric widening catalog is one correctness-only family witness", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === NUMERIC_WIDENING_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Numeric widen ok\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-numeric-widening-compile-time-folded",
    "runtime-numeric-widening-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "numeric-widening-release" &&
    source.quality === "correctness-gate"));
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === NUMERIC_WIDENING_WORKLOAD_ID),
  "correctness-only numeric widening must not acquire timing or ranking data");

  const c = readFileSync(
    `${ROOT}/benchmarks/executable/numeric_widening.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/numeric_widening.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/numeric-widening.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: 0,
      stdout: workload.oracle.stdout,
      stderr: "",
      errors: [],
    }, `${name} source must declare the exact local oracle`);
    assert.deepEqual(
      validateExecutableSourceExpectation(source, workload.oracle, `${name} source`),
      []);
  }
  assert.match(w, /fn returnF32\(value: i16\): f32/u);
  assert.match(w, /acceptF64\(value: 4294967295_u32\)/u);
  assert.match(w, /let widenedFloat: f64 = 1\.5_f32/u);
  assert.match(w, /let explicitFloat = f64\(1\.25_f32\)/u);
  assert.match(w, /let mixedInteger = 2_i32 \+ 0\.5_f64/u);
  assert.match(w, /let mixedFloat = 1\.5_f32 \+ 2\.25_f64/u);
  assert.match(c, /volatile (?:int16_t|uint16_t|int32_t|uint32_t|float)/u);
  assert.match(rust, /black_box\(u32::MAX\)/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("explicit integer truncating-bits catalog is one correctness-only family witness", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === INTEGER_TRUNCATING_BITS_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate explicit integer truncating-bits conversions across signed and unsigned 8/16/64-bit values, including narrowing, widening, signedness changes, and x86-64 Int/UInt aliases, with exact decimal output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "Trunc 2/-7/-6/18446744073709551609/-1\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-integer-truncating-bits-compile-time-folded",
    "runtime-integer-truncating-bits-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "integer-truncating-bits-release" &&
    source.quality === "correctness-gate"));
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
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === INTEGER_TRUNCATING_BITS_WORKLOAD_ID),
  "truncating-bits correctness references must not acquire timing or ranking data");

  const c = readFileSync(
    `${ROOT}/benchmarks/executable/integer_truncating_bits.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/integer_truncating_bits.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/integer-truncating-bits.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: workload.oracle.exitCode,
      stdout: workload.oracle.stdout,
      stderr: "",
      errors: [],
    }, `${name} source must declare the exact local oracle`);
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle, `${name} source`), []);
  }
  assert.match(c, /volatile int16_t signed_wide_input/u);
  assert.match(c, /volatile uint8_t unsigned_narrow_input/u);
  assert.match(c, /signed_i8_from_bits/u);
  assert.match(c, /signed_i64_from_bits/u);
  assert.match(rust, /black_box\(258_i16\) as i8/u);
  assert.match(rust, /black_box\(250_u8\) as i8/u);
  assert.match(rust, /black_box\(-7_i64\) as u64/u);
  assert.match(rust, /black_box\(u64::MAX\) as i64/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("integer saturating-conversion catalog is a compact correctness-only four-quadrant witness", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === INTEGER_SATURATING_CONVERSION_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.match(workload.scope, /all four signedness quadrants/u);
  assert.match(workload.scope, /representative i16\/u16-to-i8\/u8 conversions/u);
  assert.match(workload.scope, /UInt-to-Int alias path/u);
  assert.match(workload.scope, /all 100 source\/destination pairs/u);
  assert.match(workload.scope, /scalar 8\/16\/32\/64-bit boundaries/u);
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout: "ss -128/7/127; us 7/127/127; su 0/200/255; uu 7/255/255; UInt->Int 9223372036854775807\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-integer-saturating-conversion-product-closure-unsupported",
    "runtime-integer-saturating-conversion-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "integer-saturating-conversion-release" &&
    source.quality === "correctness-gate"));
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
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === INTEGER_SATURATING_CONVERSION_WORKLOAD_ID),
  "saturating-conversion correctness references must not acquire timing or ranking data");

  const c = readFileSync(
    `${ROOT}/benchmarks/executable/integer_saturating_conversion.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/integer_saturating_conversion.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/integer-saturating-conversion.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: workload.oracle.exitCode,
      stdout: workload.oracle.stdout,
      stderr: workload.oracle.stderr,
      errors: [],
    }, `${name} source must declare the exact local oracle`);
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle,
      `${name} source`), []);
  }
  assert.match(w, /fn signedToSigned\(value: i16\): i8/u);
  assert.match(w, /fn unsignedToSigned\(value: u16\): i8/u);
  assert.match(w, /fn signedToUnsigned\(value: i16\): u8/u);
  assert.match(w, /fn unsignedToUnsigned\(value: u16\): u8/u);
  assert.match(w, /fn aliasToAlias\(value: UInt\): Int/u);
  assert.match(w, /signedToSigned\(value: -129_i16\)/u);
  assert.match(w, /unsignedToSigned\(value: 128_u16\)/u);
  assert.match(w, /signedToUnsigned\(value: -1_i16\)/u);
  assert.match(w, /unsignedToUnsigned\(value: 256_u16\)/u);
  assert.match(w, /aliasToAlias\(value: 18446744073709551615_u64\)/u);
  assert.match(c, /volatile int16_t signed16/u);
  assert.match(c, /volatile uint16_t unsigned16/u);
  assert.match(c, /volatile uint64_t unsigned64_maximum/u);
  assert.match(c, /saturate_signed_to_signed_i8/u);
  assert.match(c, /saturate_signed_to_unsigned_u8/u);
  assert.match(c, /saturate_unsigned_to_signed_i8/u);
  assert.match(c, /saturate_unsigned_to_unsigned_u8/u);
  assert.match(c, /saturate_uint_to_int/u);
  assert.match(rust, /black_box\(u64::MAX\)/u);
  assert.match(rust, /value\.clamp\(i8::MIN as i16, i8::MAX as i16\)/u);
  assert.match(rust, /value\.min\(i64::MAX as u64\)/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("integer binary bitwise catalog is one correctness-only signed/unsigned family witness", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === INTEGER_BITWISE_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.match(workload.scope, /binary bitwise AND, OR, and XOR/u);
  assert.match(workload.scope, /i8\/u8\/i16\/u16\/i32\/u32\/i64\/u64/u);
  assert.match(workload.scope, /Int\/UInt aliases/u);
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout:
      "i8 10/-81/-91\nu8 10/175/165\ni16 2570/-20561/-23131\n" +
      "u16 2570/44975/42405\ni32 168430090/-1347440721/-1515870811\n" +
      "u32 168430090/2947526575/2779096485\ni64 723401728380766730/" +
      "-5787213827046133841/-6510615555426900571\n" +
      "u64 723401728380766730/12659530246663417775/11936128518282651045\n" +
      "Int 723401728380766730/-5787213827046133841/-6510615555426900571\n" +
      "UInt 723401728380766730/12659530246663417775/11936128518282651045\n" +
      "Widened -13\nMixed 255\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-integer-bitwise-compile-time-folded",
    "runtime-integer-bitwise-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "integer-bitwise-release" &&
    source.quality === "correctness-gate"));
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
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === INTEGER_BITWISE_WORKLOAD_ID),
  "correctness-only bitwise evidence must not acquire timing or ranking data");

  const c = readFileSync(
    `${ROOT}/benchmarks/executable/integer_bitwise.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/integer_bitwise.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/integer-bitwise.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: 0,
      stdout: workload.oracle.stdout,
      stderr: "",
      errors: [],
    }, `${name} source must declare the exact local oracle`);
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle,
      `${name} source`), []);
  }
  for (const type of ["int8_t", "uint8_t", "int16_t", "uint16_t",
    "int32_t", "uint32_t", "int64_t", "uint64_t", "intptr_t", "uintptr_t"]) {
    assert.match(c, new RegExp(`static volatile ${type} \\w+_inputs`, "u"),
      `${type} must use volatile runtime operands in C23`);
  }
  assert.match(c, /static volatile int8_t widened_left/u);
  assert.match(c, /\(int32_t\)widened_left \| widened_right/u);
  assert.match(c, /\(int16_t\)mixed_left \| mixed_right/u);
  assert.match(rust, /i32::from\(widened_left\) \| widened_right/u);
  assert.match(rust, /i16::from\(mixed_left\) \| mixed_right/u);
  assert.match(w,
    /fn bitwise_widened\(left: i8, right: i32\): i32/u);
  assert.match(w,
    /fn bitwise_mixed\(left: u8, right: i16\): i16/u);
  for (const operation of ["a & b", "a | b", "a ^ b"]) {
    assert.ok(c.includes(operation), `C23 must perform ${operation}`);
  }
  assert.equal((rust.match(/black_box\(/gu) ?? []).length, 24,
    "Rust must black-box both operands for all ten types and exact widening");
  assert.match(rust, /i64_a \^ i64_b/u);
  assert.match(rust, /u64_a \| u64_b/u);
  assert.match(rust, /int_a & int_b/u);
  assert.match(rust, /uint_a \^ uint_b/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("integer comparison catalog is one correctness-only signed/unsigned family witness", () => {
  const workload = documents.catalog.workloads.find((item) =>
    item.id === INTEGER_COMPARISON_WORKLOAD_ID);
  assert.ok(workload);
  assert.equal(workload.structureClass, "public-end-to-end");
  assert.equal(workload.status, "source-oracle-ready");
  assert.equal(workload.sourceReadiness, "source-and-oracle-ready");
  assert.equal(workload.demoEvidence, "bounded-w-demo");
  assert.equal(workload.benchmarkStatus, "not-performance-ready");
  assert.equal(workload.scope,
    "Validate fixed-input signed and unsigned comparisons (==, !=, <, <=, >, >=) across i8/u8/i16/u16/i32/u32/i64/u64 and the current x86-64 Int/UInt aliases, plus a u8-to-i16 comparison after argument widening, with exact output. Performance ranking is deferred until W preserves equivalent runtime work.");
  assert.deepEqual(workload.oracle, {
    kind: "exact-output",
    status: "source-backed",
    exitCode: 0,
    stdout:
      "i8 false/true/true/true/false/false\nu8 false/true/false/false/true/true\n" +
      "i16 true/false/false/true/false/true\nu16 false/true/true/true/false/false\n" +
      "i32 false/true/true/true/false/false\nu32 false/true/false/false/true/true\n" +
      "i64 false/true/true/true/false/false\nu64 false/true/false/false/true/true\n" +
      "Int false/true/true/true/false/false\nUInt true/false/false/true/false/true\n" +
      "widen u8->i16 true\n",
    stderr: "",
  });
  assert.deepEqual(workload.blockedLanguages, []);
  assert.deepEqual(workload.blockers, [
    "w-integer-comparison-fixed-input",
    "runtime-integer-comparison-equivalence",
  ]);
  assert.deepEqual(workload.sources.map((source) =>
    [source.language, source.platformTarget]), [
    ["w", EXECUTABLE_PLATFORM_TARGET],
    ["w", EXECUTABLE_PLATFORM_TARGET_LINUX_WSL],
    ["c", EXECUTABLE_PLATFORM_TARGET],
    ["rust", EXECUTABLE_PLATFORM_TARGET],
  ]);
  assert.ok(workload.sources.every((source) =>
    source.recipeClass === "integer-comparison-release" &&
    source.quality === "correctness-gate"));
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
  assert.ok(!documents.catalog.bestMetrics.entries.some((entry) =>
    entry.workloadId === INTEGER_COMPARISON_WORKLOAD_ID),
  "correctness-only comparisons must not acquire timing or ranking data");

  const c = readFileSync(
    `${ROOT}/benchmarks/executable/integer_comparison.c`, "utf8");
  const rust = readFileSync(
    `${ROOT}/benchmarks/executable/integer_comparison.rs`, "utf8");
  const w = readFileSync(
    `${ROOT}/compiler/seed-c/fixtures/integer-comparison.w`, "utf8");
  for (const [name, source] of [["W", w], ["C23", c], ["Rust", rust]]) {
    assert.deepEqual(parseExecutableSourceExpectation(source), {
      exitCode: 0,
      stdout: workload.oracle.stdout,
      stderr: "",
      errors: [],
    }, `${name} source must declare the exact local oracle`);
    assert.deepEqual(validateExecutableSourceExpectation(source, workload.oracle, `${name} source`), []);
  }
  assert.match(w, /compareI8\(left: -1_i8, right: 1_i8\)/u);
  assert.match(w, /compareU32\(left: 3000000000_u32, right: 1_u32\)/u);
  assert.match(w, /compareInt\(left: -7, right: 12\)/u);
  assert.match(w, /compareWidened\(value: 200_u8\)/u);
  assert.match(c, /static volatile int8_t i8_input/u);
  assert.match(c, /report_i8\(i8_input\[0\], i8_input\[1\]\)/u);
  assert.match(rust, /black_box\(-1_i8\)/u);
  assert.match(rust, /report_widened\(black_box\(200_u8\)\.into\(\)\)/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.doesNotMatch(rust, /\b(?:extern|unsafe|ffi)\b/iu);
});

test("not-performance-ready strict float evidence cannot become live best metrics", () => {
  const result = validResult();
  const workload = documents.catalog.workloads.find((item) => item.id === FLOAT_STRICT_WORKLOAD_ID);
  const source = workload.sources.find((item) => item.language === "rust" && item.platformTarget === EXECUTABLE_PLATFORM_TARGET);
  result.id = `${FLOAT_STRICT_WORKLOAD_ID}-rust-example`;
  result.workloadId = FLOAT_STRICT_WORKLOAD_ID;
  result.identity.sourceDigest = source.digest;
  result.identity.recipe = source.recipe;
  result.identity.recipeClass = source.recipeClass;
  result.identity.eligibility = source.eligibility;
  result.equivalenceKey = executableEquivalenceKey(
    documents.catalog,
    FLOAT_STRICT_WORKLOAD_ID,
    EXECUTABLE_PLATFORM_TARGET,
    "release",
    source.recipeClass,
  );
  result.correctness.oracleId = `${FLOAT_STRICT_WORKLOAD_ID}:exact-output`;
  result.correctness.stdoutDigest = exactOutputDigest(workload.oracle.stdout);
  result.provenance.sourceDigest = source.digest;
  assert.deepEqual(validateExecutableResult(result, documents.catalog), []);

  const derived = deriveExecutableBestMetrics(documents.catalog, [result]);
  assert.equal(derived.entries.length, 0);

  const forbidden = clone(documents.catalog.bestMetrics.entries[0]);
  forbidden.workloadId = FLOAT_STRICT_WORKLOAD_ID;
  forbidden.provenance.sourceDigest = source.digest;
  assert.match(
    validateExecutableBestMetric(forbidden, documents.catalog).join("\n"),
    /eligible for live best metrics/u,
  );
});

test("process-arguments-ordering catalog pins the count-dependent argument-mode contract", () => {
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
    "Argument mode compact: count=0\n",
    "Argument mode compact: count=1\n",
    "Argument mode extended: count=2\n",
    "Argument mode extended: count=3\n",
  ]);
  assert.deepEqual(workload.oracle.cases.map((testCase) => testCase.arguments), [[], [""], ["alpha", "beta"], ["alpha", "beta", "gamma"]]);
  assert.ok(workload.sources.every((source) => source.recipeClass === PROCESS_ARGUMENTS_ORDERING_RECIPE_CLASS));
  assert.deepEqual(workload.sources.map((source) => source.language), EXECUTABLE_LANGUAGES);
  assert.equal(workload.sources.find((source) => source.language === "w").entry, "run");
  assert.equal(workload.sources.find((source) => source.language === "c").entry, "main");
  assert.equal(workload.sources.find((source) => source.language === "rust").entry, "main");
  assertCurrentMetricLanes(workload,
    documents.catalog.bestMetrics.entries.filter((entry) => entry.workloadId === PROCESS_ARGUMENTS_ORDERING_WORKLOAD_ID),
    "process-arguments-ordering");
});

test("process-arguments-ordering C and Rust variants retain independent count branches", () => {
  const c = readFileSync(`${ROOT}/benchmarks/executable/process_arguments_ordering.c`, "utf8");
  const rust = readFileSync(`${ROOT}/benchmarks/executable/process_arguments_ordering.rs`, "utf8");
  assert.match(c, /int main\(int argc, char \*\*argv\)/u);
  assert.match(c, /const int count = argc - 1/u);
  assert.match(c, /count < 2/u);
  assert.match(c, /Argument mode compact: count=%d\\n/u);
  assert.match(c, /Argument mode extended: count=%d\\n/u);
  assert.doesNotMatch(c, /\b(?:extern|ffi)\b/iu);
  assert.match(rust, /fn main\(\)/u);
  assert.match(rust, /args_os\(\)\.count\(\)\.saturating_sub\(1\)/u);
  assert.match(rust, /count < 2/u);
  assert.match(rust, /Argument mode compact: count=\{count\}/u);
  assert.match(rust, /Argument mode extended: count=\{count\}/u);
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
    identity: { sourceDigest: source.digest, platformTarget: EXECUTABLE_PLATFORM_TARGET, artifactTarget, profile: "release", toolchain: language === "rust" ? "rustc-1.94" : "gcc-13.2", host: executableHostIdentity(environment), recipe: source.recipe, recipeClass: source.recipeClass, runtimeClosure: structuredClone(source.runtimeClosure), recipeDigest: digest, eligibility: source.eligibility },
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

test("changed compiler bytes replace same-lane current cells even after a performance regression", () => {
  const baseline = validResult();
  const first = updateExecutableBestMetrics(documents.catalog, baseline);
  const current = clone(baseline);
  current.id = "hello-rust-same-version-new-compiler-slower";
  current.provenance.toolchainDigest = "sha256:" + "6".repeat(64);
  current.provenance.commit = "6".repeat(40);
  current.provenance.observedAt = "2026-09-15T12:00:00.000Z";
  for (const stage of [current.compile, current.run]) {
    for (const sample of [...stage.warmup, ...stage.raw]) sample.wallNs = String(BigInt(sample.wallNs) + 1000n);
    stage.summary = deriveSummary(stage.raw);
  }
  const expected = deriveExecutableBestMetrics(documents.catalog, [current]);
  const updated = updateExecutableBestMetrics(first.catalog, current);
  const lane = updated.catalog.bestMetrics.entries.filter((entry) =>
    entry.workloadId === current.workloadId && entry.language === current.language &&
    entry.platformTarget === current.platformTarget && entry.host === current.identity.host);
  assert.equal(lane.length, expected.entries.length);
  assert.ok(lane.every((entry) => entry.provenance.recordId === current.id));
  assert.ok(lane.every((entry) => entry.toolchain === current.identity.toolchain));
  for (const entry of expected.entries) {
    assert.equal(lane.find((item) => item.metric === entry.metric)?.value, entry.value);
  }
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
