import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import {
  PLATFORM_MINIMAL_C_RECIPE,
  PLATFORM_MINIMAL_RUST_RECIPE,
} from "./executable-release-recipes.mjs";

export const ROOT = path.resolve(import.meta.dir, "..");
export const EXECUTABLE_SCHEMA = "w-executable-benchmark/7";
export const EXECUTABLE_CATALOG_ID = "w-executable-benchmark-catalog";
export const EXECUTABLE_RESULT_SCHEMA = "w-executable-benchmark-result/7";
export const EXECUTABLE_BEST_SCHEMA = "w-executable-benchmark-best-metrics/2";
export const EXECUTABLE_SUITE_RECEIPT_SCHEMA = "w-executable-benchmark-suite/1";
export const EXECUTABLE_RUNTIME_CLOSURE_CLASSES = Object.freeze([
  "freestanding",
  "hosted-crt",
  "instrumentation",
]);
export const EXECUTABLE_RUNTIME_CLOSURE_UNVERIFIED = Object.freeze({ status: "unverified" });
export const EXECUTABLE_LANGUAGES = Object.freeze(["w", "c", "rust"]);
export const EXECUTABLE_STRUCTURE_CLASSES = Object.freeze([
  "public-end-to-end",
  "integration-linkage",
  "transient-internal",
]);
export const EXECUTABLE_WORKLOAD_IDS = Object.freeze([
  "hello",
  "hello-platform-minimal",
  "branch",
  "nested-branch",
  "bool-short-circuit",
  "interpolation",
  "scalar-if",
  "nested-scalar-if",
  "terminal-returns",
  "while-post",
  "repeat",
  "wmo",
  "async-join",
  "async-yield",
  "main-dispatch",
  "main-cardinality",
  "enum-switch",
  "enum-subset",
  "enum-payload",
  "enum-bool-payload",
  "comparison-composition",
  "integer-bitwise",
  "integer-shift-semantics",
  "power",
  "power-prefix",
  "compound",
  "float-strict",
  "float-bit-representation",
  "checked-integer-arithmetic",
  "integer-prefix",
  "integer-wrapping",
  "integer-widening",
  "numeric-widening",
  "integer-truncating-bits",
  "integer-saturating-conversion",
  "integer-comparison",
  "uint-overflowing-family",
  "uint-saturating-policy",
  "fixed-integer-bit-primitives",
  "uint-bitwise",
  "uint-compound",
  "unsigned",
  "linear",
  "mutation",
  "conditional-mutation",
  "bool-mutation",
  "branch-mutation-multi",
  "composition",
  "process-entry",
  "fixed-integer-runtime-arithmetic",
  "float-integer-rounding",
  "process-enum-payload",
  "process-arguments-count",
  "process-arguments-ordering",
  "local-module-graph",
  "process-handler-lifecycle",
]);
const WORKLOAD_FAMILY_ROWS = Object.freeze({
  hello: Object.freeze(["hello", "hello-platform-minimal"]),
  "control-flow": Object.freeze([
    "branch", "nested-branch", "bool-short-circuit",
    "interpolation", "scalar-if", "nested-scalar-if", "terminal-returns",
    "while-post", "repeat", "wmo",
  ]),
  async: Object.freeze(["async-join", "async-yield"]),
  composition: Object.freeze([
    "main-dispatch", "main-cardinality", "enum-switch",
    "enum-subset", "enum-payload", "enum-bool-payload",
    "comparison-composition", "composition",
  ]),
  "integer-semantics": Object.freeze([
    "integer-bitwise", "integer-shift-semantics", "power",
    "power-prefix", "compound", "checked-integer-arithmetic",
    "integer-prefix", "integer-wrapping", "integer-widening",
    "numeric-widening", "integer-truncating-bits",
    "integer-saturating-conversion", "integer-comparison",
    "uint-overflowing-family", "uint-saturating-policy",
    "fixed-integer-bit-primitives", "uint-bitwise", "uint-compound",
    "unsigned", "fixed-integer-runtime-arithmetic",
  ]),
  "floating-point": Object.freeze([
    "float-strict", "float-bit-representation", "float-integer-rounding",
  ]),
  mutation: Object.freeze([
    "linear", "mutation", "conditional-mutation",
    "bool-mutation", "branch-mutation-multi",
  ]),
  process: Object.freeze([
    "process-entry", "process-enum-payload", "process-arguments-count",
    "process-arguments-ordering", "process-handler-lifecycle",
  ]),
  modules: Object.freeze(["local-module-graph"]),
});
export const EXECUTABLE_WORKLOAD_FAMILY_IDS = Object.freeze(Object.keys(WORKLOAD_FAMILY_ROWS));
const EXECUTABLE_WORKLOAD_FAMILY = Object.freeze(Object.fromEntries(
  Object.entries(WORKLOAD_FAMILY_ROWS).flatMap(([family, ids]) => ids.map((id) => [id, family])),
));
export const EXECUTABLE_RUN_TARGETS = Object.freeze(
  EXECUTABLE_WORKLOAD_IDS.filter((id) => id !== "composition"),
);
export const EXECUTABLE_BENCHMARK_STATUSES = Object.freeze([
  "not-performance-ready",
  "contextual-measurement-ready",
  "deferred-to-M3b",
  "partial-exploratory-ready",
  "exploratory-ready",
  "planned",
]);

export const EXECUTABLE_SUITE_DEFAULT_PLATFORMS = Object.freeze([
  "windows-x64",
  "linux-wsl-x64",
]);
export const EXECUTABLE_SUITE_LANE_LANGUAGES = Object.freeze(["w", "c", "rust"]);
const EXECUTABLE_SUITE_MEASUREMENT_STATUSES = new Set([
  "exploratory-ready",
  "partial-exploratory-ready",
]);

export function executableWorkloadHasRunner(workload) {
  if (!workload || !EXECUTABLE_RUN_TARGETS.includes(workload.id) || !Array.isArray(workload.sources) || workload.sources.length === 0) return false;
  return workload.sources.some((source) => source?.language !== "w" ||
    source.recipe === "public-w-build-release" ||
    (workload.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && source.recipe === PROCESS_ENTRY0_RECIPE));
}

function suiteRecipeSupported(workload, source) {
  if (source.language === "w") return source.recipe === "public-w-build-release";
  if (source.language === "c") {
    return source.recipe === "clang-c23-msvc" ||
      (workload.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID && source.recipe === PLATFORM_MINIMAL_C_RECIPE);
  }
  if (source.language === "rust") {
    return source.recipe === "rustc-edition-2024" ||
      (workload.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID && source.recipe === PLATFORM_MINIMAL_RUST_RECIPE);
  }
  return false;
}

export function selectExecutableSuiteLanes(catalog, { platforms = EXECUTABLE_SUITE_DEFAULT_PLATFORMS } = {}) {
  if (!Array.isArray(catalog?.workloads)) throw new TypeError("executable suite selection requires catalog workloads");
  if (!Array.isArray(platforms) || platforms.length === 0 || new Set(platforms).size !== platforms.length ||
      platforms.some((platform) => !EXECUTABLE_SUITE_DEFAULT_PLATFORMS.includes(platform))) {
    throw new TypeError("suite platforms must be a non-empty, unique subset of the supported runner platforms");
  }
  const lanes = [];
  for (const workload of catalog.workloads) {
    if (workload.status !== "source-oracle-ready" ||
        workload.sourceReadiness !== "source-and-oracle-ready" ||
        !executableWorkloadHasRunner(workload)) continue;
    const contextual = workload.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID &&
      workload.benchmarkStatus === "contextual-measurement-ready";
    if (!contextual && !EXECUTABLE_SUITE_MEASUREMENT_STATUSES.has(workload.benchmarkStatus)) continue;
    if (workload.structureClass !== "public-end-to-end") continue;

    for (const source of workload.sources) {
      if (!platforms.includes(source.platformTarget) ||
          source.status !== "source-oracle-ready" ||
          !EXECUTABLE_SUITE_LANE_LANGUAGES.includes(source.language) ||
          !suiteRecipeSupported(workload, source)) continue;
      if (contextual) {
        // The six platform-minimal sources are intentionally contextual even
        // where ordinary eligibility is deferred or WSL is diagnostic-only.
        if (!["windows-x64", "linux-wsl-x64"].includes(source.platformTarget)) continue;
      } else if (source.platformTarget !== "windows-x64" ||
                 source.eligibility !== "promotable-after-equivalence" ||
                 source.comparability !== "promotable-after-equivalence") {
        continue;
      }
      lanes.push({ workloadId: workload.id, language: source.language, platformTarget: source.platformTarget });
    }
  }
  const platformOrder = new Map(EXECUTABLE_SUITE_DEFAULT_PLATFORMS.map((platform, index) => [platform, index]));
  const languageOrder = new Map(EXECUTABLE_SUITE_LANE_LANGUAGES.map((language, index) => [language, index]));
  return lanes.sort((left, right) =>
    compareText(left.workloadId, right.workloadId) ||
    platformOrder.get(left.platformTarget) - platformOrder.get(right.platformTarget) ||
    languageOrder.get(left.language) - languageOrder.get(right.language));
}

export function executableSuiteReceiptErrors(receipt, catalog, { catalogDigest } = {}) {
  const errors = [];
  const requiredKeys = ["$schema", "schema", "kind", "status", "mode", "platforms", "catalogDigest", "observedAt", "durationMs", "laneCounts", "lanes"];
  if (!receipt || typeof receipt !== "object" || Array.isArray(receipt)) return ["suite receipt must be an object"];
  const actualKeys = Object.keys(receipt).sort(compareText);
  if (JSON.stringify(actualKeys) !== JSON.stringify([...requiredKeys].sort(compareText))) errors.push("suite receipt has missing or unknown fields");
  if (receipt.$schema !== "./executable-benchmark.schema.json") errors.push("suite receipt.$schema is invalid");
  if (receipt.schema !== EXECUTABLE_SUITE_RECEIPT_SCHEMA || receipt.kind !== "executable-suite-current" || receipt.status !== "current") {
    errors.push("suite receipt identity or status is invalid");
  }
  if (receipt.mode !== "full" && receipt.mode !== "filtered") errors.push("suite receipt.mode must be full or filtered");
  if (!Array.isArray(receipt.platforms) || receipt.platforms.length === 0 ||
      new Set(receipt.platforms).size !== receipt.platforms.length ||
      receipt.platforms.some((platform) => !EXECUTABLE_SUITE_DEFAULT_PLATFORMS.includes(platform))) {
    errors.push("suite receipt.platforms must be a non-empty unique supported platform list");
  } else {
    const canonicalPlatforms = EXECUTABLE_SUITE_DEFAULT_PLATFORMS.filter((platform) => receipt.platforms.includes(platform));
    if (JSON.stringify(canonicalPlatforms) !== JSON.stringify(receipt.platforms)) errors.push("suite receipt.platforms must use canonical order");
    const expectedMode = JSON.stringify(receipt.platforms) === JSON.stringify(EXECUTABLE_SUITE_DEFAULT_PLATFORMS) ? "full" : "filtered";
    if (receipt.mode !== expectedMode) errors.push("suite receipt.mode does not match its platform scope");
  }
  if (typeof receipt.catalogDigest !== "string" || !DIGEST_PATTERN.test(receipt.catalogDigest)) errors.push("suite receipt.catalogDigest is invalid");
  if (catalogDigest !== undefined && receipt.catalogDigest !== catalogDigest) errors.push("suite receipt is stale for the current catalog");
  if (typeof receipt.observedAt !== "string" || !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/u.test(receipt.observedAt) ||
      !Number.isFinite(Date.parse(receipt.observedAt)) || new Date(receipt.observedAt).toISOString() !== receipt.observedAt) {
    errors.push("suite receipt.observedAt must be canonical ISO-8601 UTC");
  }
  if (!Number.isSafeInteger(receipt.durationMs) || receipt.durationMs < 0) errors.push("suite receipt.durationMs must be a non-negative safe integer");
  if (!receipt.laneCounts || typeof receipt.laneCounts !== "object" || Array.isArray(receipt.laneCounts) ||
      Object.keys(receipt.laneCounts).sort(compareText).join(",") !== "failed,passed,skipped,total" ||
      Object.values(receipt.laneCounts).some((count) => !Number.isSafeInteger(count) || count < 0)) {
    errors.push("suite receipt.laneCounts must contain non-negative total, passed, failed, and skipped counts");
  }
  if (!Array.isArray(receipt.lanes)) errors.push("suite receipt.lanes must be an array");
  if (Array.isArray(receipt.platforms) && Array.isArray(catalog?.workloads) && Array.isArray(receipt.lanes)) {
    let expectedLanes = [];
    try { expectedLanes = selectExecutableSuiteLanes(catalog, { platforms: receipt.platforms }); }
    catch (error) { errors.push(String(error?.message ?? error)); }
    const lanes = receipt.lanes.map((lane) => {
      if (!lane || typeof lane !== "object" || Array.isArray(lane) ||
          Object.keys(lane).sort(compareText).join(",") !== "language,platformTarget,recipe,status,toolchain,toolchainDigest,workloadId" ||
          lane.status !== "passed") errors.push("suite receipt lanes may contain only complete passed results");
      if (typeof lane?.toolchain !== "string" || lane.toolchain.trim() === "") errors.push("suite receipt lane.toolchain must retain the observed toolchain identity");
      if (typeof lane?.recipe !== "string" || lane.recipe.trim() === "") errors.push("suite receipt lane.recipe must retain the selected source recipe");
      if (typeof lane?.toolchainDigest !== "string" || !DIGEST_PATTERN.test(lane.toolchainDigest)) errors.push("suite receipt lane.toolchainDigest must retain the observed toolchain provenance digest");
      return { workloadId: lane?.workloadId, language: lane?.language, platformTarget: lane?.platformTarget };
    });
    if (JSON.stringify(lanes) !== JSON.stringify(expectedLanes)) errors.push("suite receipt.lanes do not match the deterministic catalog selection");
    if (receipt.laneCounts && typeof receipt.laneCounts === "object" && !Array.isArray(receipt.laneCounts)) {
      if (receipt.laneCounts.total !== expectedLanes.length || receipt.laneCounts.passed !== expectedLanes.length ||
          receipt.laneCounts.failed !== 0 || receipt.laneCounts.skipped !== 0) {
        errors.push("suite receipt.laneCounts must report a complete successful suite");
      }
    }
  }
  return errors;
}

export function executableCatalogFileDigest(root = ROOT) {
  const bytes = fs.readFileSync(path.resolve(root, "benchmarks", "executable-catalog.json"));
  return "sha256:" + crypto.createHash("sha256").update(bytes).digest("hex");
}
const PUBLIC_WINDOWS_RUN_GATE = "tooling/check-w-run-windows.mjs";
const PUBLIC_WINDOWS_RUN_VARIANTS = Object.freeze({
  "compiler/seed-c/fixtures/hlo0-hello.w": "hello",
  "compiler/seed-c/fixtures/process-enum-payload.w": "process-enum-payload",
  "compiler/seed-c/fixtures/process-arguments-count.w": "process-arguments-count",
  "compiler/seed-c/fixtures/process-arguments-ordering.w": "process-arguments-ordering",
  "compiler/seed-c/fixtures/process-integer-exact-success.w": "fixed-integer-runtime-arithmetic",
  "compiler/seed-c/fixtures/process-integer-exact-error.w": "fixed-integer-runtime-arithmetic",
  "compiler/seed-c/fixtures/process-float-rounding-success.w": "float-integer-rounding",
  "compiler/seed-c/fixtures/process-float-rounding-error.w": "float-integer-rounding",
  "compiler/seed-c/fixtures/terminal-returns.w": "terminal-returns",
  "compiler/seed-c/fixtures/repeat.w": "repeat",
  "compiler/seed-c/fixtures/local-graph/app.w": "local-module-graph",
  "compiler/seed-c/fixtures/uint-wrapping-add.w": "integer-wrapping",
  "compiler/seed-c/fixtures/uint-wrapping-subtract.w": "integer-wrapping",
  "compiler/seed-c/fixtures/uint-wrapping-multiply.w": "integer-wrapping",
  "compiler/seed-c/fixtures/uint-wrapping-negate.w": "integer-wrapping",
  "compiler/seed-c/fixtures/uint-wrapping-power.w": "integer-wrapping",
  "compiler/seed-c/fixtures/uint-wrapping-shift-left.w": "integer-wrapping",
  "compiler/seed-c/fixtures/integer-widening.w": "integer-widening",
  "compiler/seed-c/fixtures/numeric-widening.w": "numeric-widening",
  "compiler/seed-c/fixtures/integer-truncating-bits.w": "integer-truncating-bits",
  "compiler/seed-c/fixtures/integer-saturating-conversion.w": "integer-saturating-conversion",
  "compiler/seed-c/fixtures/integer-comparison.w": "integer-comparison",
  "compiler/seed-c/fixtures/integer-bitwise.w": "integer-bitwise",
  "compiler/seed-c/fixtures/shifts.w": "integer-shift-semantics",
  "compiler/seed-c/fixtures/fixed-integer-shift-policies.w": "integer-shift-semantics",
  "compiler/seed-c/fixtures/fixed-integer-bit-primitives.w": "fixed-integer-bit-primitives",
  "compiler/seed-c/fixtures/uint-rotated-left.w": "uint-bitwise",
  "compiler/seed-c/fixtures/uint-rotated-right.w": "uint-bitwise",
  "compiler/seed-c/fixtures/uint-bit-not.w": "uint-bitwise",
  "compiler/seed-c/fixtures/uint-count-ones.w": "uint-bitwise",
  "compiler/seed-c/fixtures/uint-count-zeros.w": "uint-bitwise",
  "compiler/seed-c/fixtures/uint-leading-zeros.w": "uint-bitwise",
  "compiler/seed-c/fixtures/uint-trailing-zeros.w": "uint-bitwise",
  "compiler/seed-c/fixtures/uint-reversed-bits.w": "uint-bitwise",
  "compiler/seed-c/fixtures/uint-reversed-bytes.w": "uint-bitwise",
  "compiler/seed-c/fixtures/uint-overflowing-add.w": "uint-overflowing-family",
  "compiler/seed-c/fixtures/uint-overflowing-power.w": "uint-overflowing-family",
  "compiler/seed-c/fixtures/uint-saturating-add.w": "uint-saturating-policy",
  "compiler/seed-c/fixtures/uint-saturating-subtract.w": "uint-saturating-policy",
  "compiler/seed-c/fixtures/uint-saturating-multiply.w": "uint-saturating-policy",
  "compiler/seed-c/fixtures/unary-negate.w": "integer-prefix",
  "compiler/seed-c/fixtures/unary-interpolation.w": "integer-prefix",
  "compiler/seed-c/fixtures/while.w": "while-post",
  "compiler/seed-c/fixtures/while-multi.w": "while-post",
  "compiler/seed-c/fixtures/comparisons.w": "comparison-composition",
  "compiler/seed-c/fixtures/branch-mutation.w": "branch-mutation-multi",
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
export const PROCESS_ENUM_PAYLOAD_WORKLOAD_ID = "process-enum-payload";
export const PROCESS_ENUM_PAYLOAD_ORACLE_KIND = PROCESS_ENTRY_ORACLE_KIND;
export const PROCESS_ENUM_PAYLOAD_RECIPE_CLASS = "process-enum-payload-release";
export const PROCESS_ENUM_PAYLOAD_TIMED_INPUT = Object.freeze(["alpha", "beta", "gamma"]);
export const PROCESS_ENUM_PAYLOAD_CORRECTNESS_INPUTS = Object.freeze([
  Object.freeze([]),
  Object.freeze([""]),
  Object.freeze(["alpha", "beta"]),
  PROCESS_ENUM_PAYLOAD_TIMED_INPUT,
]);
export const PROCESS_ENUM_PAYLOAD_ORACLE_CASES = Object.freeze([
  Object.freeze({ arguments: PROCESS_ENUM_PAYLOAD_CORRECTNESS_INPUTS[0], exitCode: 7, stdout: "arguments-missing count=0 amount=17 over-limit=false\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ENUM_PAYLOAD_CORRECTNESS_INPUTS[1], exitCode: 0, stdout: "arguments-present count=1 amount=17 over-limit=false\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ENUM_PAYLOAD_CORRECTNESS_INPUTS[2], exitCode: 0, stdout: "arguments-present count=2 amount=17 over-limit=false\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ENUM_PAYLOAD_CORRECTNESS_INPUTS[3], exitCode: 0, stdout: "arguments-present count=3 amount=17 over-limit=true\n", stderr: "" }),
]);
export const PROCESS_ARGUMENTS_COUNT_WORKLOAD_ID = "process-arguments-count";
export const PROCESS_ARGUMENTS_COUNT_ORACLE_KIND = PROCESS_ENTRY_ORACLE_KIND;
export const PROCESS_ARGUMENTS_COUNT_RECIPE_CLASS = "process-arguments-count-release";
export const PROCESS_ARGUMENTS_COUNT_TIMED_INPUT = Object.freeze(["alpha", "beta"]);
export const PROCESS_ARGUMENTS_COUNT_CORRECTNESS_INPUTS = Object.freeze([
  Object.freeze([]),
  Object.freeze([""]),
  PROCESS_ARGUMENTS_COUNT_TIMED_INPUT,
]);
export const PROCESS_ARGUMENTS_COUNT_ORACLE_CASES = Object.freeze([
  Object.freeze({ arguments: PROCESS_ARGUMENTS_COUNT_CORRECTNESS_INPUTS[0], exitCode: 0, stdout: "Argument count 0\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ARGUMENTS_COUNT_CORRECTNESS_INPUTS[1], exitCode: 0, stdout: "Argument count 1\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ARGUMENTS_COUNT_CORRECTNESS_INPUTS[2], exitCode: 0, stdout: "Exactly two arguments\n", stderr: "" }),
]);
export const PROCESS_ARGUMENTS_ORDERING_WORKLOAD_ID = "process-arguments-ordering";
export const PROCESS_ARGUMENTS_ORDERING_ORACLE_KIND = PROCESS_ENTRY_ORACLE_KIND;
export const PROCESS_ARGUMENTS_ORDERING_RECIPE_CLASS = "process-arguments-ordering-release";
export const HELLO_PLATFORM_MINIMAL_WORKLOAD_ID = "hello-platform-minimal";
export const HELLO_PLATFORM_MINIMAL_RECIPE_CLASS = "hello-platform-minimal";
export const PROCESS_ARGUMENTS_ORDERING_TIMED_INPUT = Object.freeze(["alpha", "beta", "gamma"]);
export const PROCESS_ARGUMENTS_ORDERING_CORRECTNESS_INPUTS = Object.freeze([
  Object.freeze([]),
  Object.freeze([""]),
  Object.freeze(["alpha", "beta"]),
  PROCESS_ARGUMENTS_ORDERING_TIMED_INPUT,
]);
export const PROCESS_ARGUMENTS_ORDERING_ORACLE_CASES = Object.freeze([
  Object.freeze({ arguments: PROCESS_ARGUMENTS_ORDERING_CORRECTNESS_INPUTS[0], exitCode: 0, stdout: "Argument mode compact: count=0\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ARGUMENTS_ORDERING_CORRECTNESS_INPUTS[1], exitCode: 0, stdout: "Argument mode compact: count=1\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ARGUMENTS_ORDERING_CORRECTNESS_INPUTS[2], exitCode: 0, stdout: "Argument mode extended: count=2\n", stderr: "" }),
  Object.freeze({ arguments: PROCESS_ARGUMENTS_ORDERING_CORRECTNESS_INPUTS[3], exitCode: 0, stdout: "Argument mode extended: count=3\n", stderr: "" }),
]);
export const FIXED_INTEGER_RUNTIME_ARITHMETIC_WORKLOAD_ID =
  "fixed-integer-runtime-arithmetic";
export const FIXED_INTEGER_RUNTIME_ARITHMETIC_ORACLE_KIND =
  PROCESS_ENTRY_ORACLE_KIND;
export const FIXED_INTEGER_RUNTIME_ARITHMETIC_RECIPE_CLASS =
  "fixed-integer-runtime-arithmetic-release";
export const FLOAT_INTEGER_ROUNDING_WORKLOAD_ID = "float-integer-rounding";
export const FLOAT_INTEGER_ROUNDING_RECIPE_CLASS = "float-integer-rounding-release";
export const FIXED_INTEGER_RUNTIME_ARITHMETIC_TIMED_INPUT = Object.freeze([]);
export const FIXED_INTEGER_RUNTIME_ARITHMETIC_CORRECTNESS_INPUTS = Object.freeze([
  FIXED_INTEGER_RUNTIME_ARITHMETIC_TIMED_INPUT,
  Object.freeze(Array.from({ length: 127 }, () => "x")),
  Object.freeze(Array.from({ length: 128 }, () => "x")),
]);
export const FIXED_INTEGER_RUNTIME_ARITHMETIC_ORACLE_CASES = Object.freeze([
  Object.freeze({ arguments: FIXED_INTEGER_RUNTIME_ARITHMETIC_CORRECTNESS_INPUTS[0], exitCode: 0, stdout: "Arithmetic 0/4/1\n", stderr: "" }),
  Object.freeze({ arguments: FIXED_INTEGER_RUNTIME_ARITHMETIC_CORRECTNESS_INPUTS[1], exitCode: 2, stdout: "", stderr: "" }),
  Object.freeze({ arguments: FIXED_INTEGER_RUNTIME_ARITHMETIC_CORRECTNESS_INPUTS[2], exitCode: 1, stdout: "", stderr: "" }),
]);
const PROCESS_ARGUMENT_ORACLE_CONTRACTS = Object.freeze({
  [PROCESS_ENTRY_WORKLOAD_ID]: Object.freeze({
    kind: PROCESS_ENTRY_ORACLE_KIND,
    timedInput: PROCESS_ENTRY_TIMED_INPUT,
    correctnessInputs: PROCESS_ENTRY_CORRECTNESS_INPUTS,
    cases: PROCESS_ENTRY_ORACLE_CASES,
  }),
  [PROCESS_ENUM_PAYLOAD_WORKLOAD_ID]: Object.freeze({
    kind: PROCESS_ENUM_PAYLOAD_ORACLE_KIND,
    timedInput: PROCESS_ENUM_PAYLOAD_TIMED_INPUT,
    correctnessInputs: PROCESS_ENUM_PAYLOAD_CORRECTNESS_INPUTS,
    cases: PROCESS_ENUM_PAYLOAD_ORACLE_CASES,
  }),
  [PROCESS_ARGUMENTS_COUNT_WORKLOAD_ID]: Object.freeze({
    kind: PROCESS_ARGUMENTS_COUNT_ORACLE_KIND,
    timedInput: PROCESS_ARGUMENTS_COUNT_TIMED_INPUT,
    correctnessInputs: PROCESS_ARGUMENTS_COUNT_CORRECTNESS_INPUTS,
    cases: PROCESS_ARGUMENTS_COUNT_ORACLE_CASES,
  }),
  [PROCESS_ARGUMENTS_ORDERING_WORKLOAD_ID]: Object.freeze({
    kind: PROCESS_ARGUMENTS_ORDERING_ORACLE_KIND,
    timedInput: PROCESS_ARGUMENTS_ORDERING_TIMED_INPUT,
    correctnessInputs: PROCESS_ARGUMENTS_ORDERING_CORRECTNESS_INPUTS,
    cases: PROCESS_ARGUMENTS_ORDERING_ORACLE_CASES,
  }),
  [FIXED_INTEGER_RUNTIME_ARITHMETIC_WORKLOAD_ID]: Object.freeze({
    kind: FIXED_INTEGER_RUNTIME_ARITHMETIC_ORACLE_KIND,
    timedInput: FIXED_INTEGER_RUNTIME_ARITHMETIC_TIMED_INPUT,
    correctnessInputs: FIXED_INTEGER_RUNTIME_ARITHMETIC_CORRECTNESS_INPUTS,
    cases: FIXED_INTEGER_RUNTIME_ARITHMETIC_ORACLE_CASES,
  }),
});
export const PROCESS_ARGUMENT_WORKLOAD_IDS = Object.freeze([
  PROCESS_ENTRY_WORKLOAD_ID,
  PROCESS_ENUM_PAYLOAD_WORKLOAD_ID,
  PROCESS_ARGUMENTS_COUNT_WORKLOAD_ID,
  PROCESS_ARGUMENTS_ORDERING_WORKLOAD_ID,
  FIXED_INTEGER_RUNTIME_ARITHMETIC_WORKLOAD_ID,
]);
export const FLOAT_STRICT_WORKLOAD_ID = "float-strict";
export const CHECKED_INTEGER_ARITHMETIC_WORKLOAD_ID = "checked-integer-arithmetic";
export const INTEGER_PREFIX_WORKLOAD_ID = "integer-prefix";
export const INTEGER_WRAPPING_WORKLOAD_ID = "integer-wrapping";
export const INTEGER_WIDENING_WORKLOAD_ID = "integer-widening";
export const NUMERIC_WIDENING_WORKLOAD_ID = "numeric-widening";
export const INTEGER_TRUNCATING_BITS_WORKLOAD_ID = "integer-truncating-bits";
export const INTEGER_SATURATING_CONVERSION_WORKLOAD_ID = "integer-saturating-conversion";
export const INTEGER_COMPARISON_WORKLOAD_ID = "integer-comparison";
export const INTEGER_BITWISE_WORKLOAD_ID = "integer-bitwise";
export const INTEGER_SHIFT_SEMANTICS_WORKLOAD_ID = "integer-shift-semantics";
export const UINT_OVERFLOWING_FAMILY_WORKLOAD_ID = "uint-overflowing-family";
export const UINT_SATURATING_POLICY_WORKLOAD_ID = "uint-saturating-policy";
export const UINT_BITWISE_WORKLOAD_ID = "uint-bitwise";
export const FIXED_INTEGER_BIT_PRIMITIVES_WORKLOAD_ID = "fixed-integer-bit-primitives";
export const UINT_COMPOUND_WORKLOAD_ID = "uint-compound";
export function isProcessArgumentWorkload(workloadId) {
  return PROCESS_ARGUMENT_WORKLOAD_IDS.includes(workloadId);
}
export function processArgumentOracleFor(workloadId) {
  return PROCESS_ARGUMENT_ORACLE_CONTRACTS[workloadId];
}
export const PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID = "process-handler-lifecycle";
export const FLOAT_BIT_REPRESENTATION_WORKLOAD_ID = "float-bit-representation";
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
  "runtime-closure",
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
const BEST_METRIC_BENCHMARK_STATUSES = Object.freeze([
  "contextual-measurement-ready",
  "partial-exploratory-ready",
  "exploratory-ready",
]);
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
  "toolchain",
  "host",
  "recipe",
  "recipeClass",
  "runtimeClosure",
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
export const EXECUTABLE_PLATFORM_TARGET_WINDOWS = "windows-x64";
export const EXECUTABLE_PLATFORM_TARGET_LINUX = "linux-x64";
export const EXECUTABLE_PLATFORM_TARGET_LINUX_WSL = "linux-wsl-x64";
export const EXECUTABLE_PLATFORM_TARGETS = Object.freeze([
  EXECUTABLE_PLATFORM_TARGET_WINDOWS,
  EXECUTABLE_PLATFORM_TARGET_LINUX,
  EXECUTABLE_PLATFORM_TARGET_LINUX_WSL,
]);
export const EXECUTABLE_ARTIFACT_TARGET_MSVC = "x86_64-pc-windows-msvc";
export const EXECUTABLE_ARTIFACT_TARGET_MINGW = "x86_64-w64-mingw32";
export const EXECUTABLE_ARTIFACT_TARGET_LINUX = "x86_64-unknown-linux-gnu";
export const EXECUTABLE_ARTIFACT_TARGETS = Object.freeze([
  EXECUTABLE_ARTIFACT_TARGET_MSVC,
  EXECUTABLE_ARTIFACT_TARGET_MINGW,
  EXECUTABLE_ARTIFACT_TARGET_LINUX,
]);
export const EXECUTABLE_PLATFORM_ARTIFACT_TARGETS = Object.freeze({
  [EXECUTABLE_PLATFORM_TARGET_WINDOWS]: Object.freeze([
    EXECUTABLE_ARTIFACT_TARGET_MSVC,
    EXECUTABLE_ARTIFACT_TARGET_MINGW,
  ]),
  [EXECUTABLE_PLATFORM_TARGET_LINUX]: Object.freeze([
    EXECUTABLE_ARTIFACT_TARGET_LINUX,
  ]),
  [EXECUTABLE_PLATFORM_TARGET_LINUX_WSL]: Object.freeze([
    EXECUTABLE_ARTIFACT_TARGET_LINUX,
  ]),
});
export const EXECUTABLE_PLATFORM_LANE_POLICIES = Object.freeze({
  [EXECUTABLE_PLATFORM_TARGET_WINDOWS]: Object.freeze({
    id: EXECUTABLE_PLATFORM_TARGET_WINDOWS,
    hostMode: "native",
    artifactTargets: Object.freeze([...EXECUTABLE_PLATFORM_ARTIFACT_TARGETS[EXECUTABLE_PLATFORM_TARGET_WINDOWS]]),
    regression: true,
    rankability: "host-partitioned",
    crossPlatformDiagnostics: "none",
    description: "Native Windows x64 executable evidence.",
  }),
  [EXECUTABLE_PLATFORM_TARGET_LINUX]: Object.freeze({
    id: EXECUTABLE_PLATFORM_TARGET_LINUX,
    hostMode: "native",
    artifactTargets: Object.freeze([...EXECUTABLE_PLATFORM_ARTIFACT_TARGETS[EXECUTABLE_PLATFORM_TARGET_LINUX]]),
    regression: true,
    rankability: "host-partitioned",
    crossPlatformDiagnostics: "none",
    description: "Native Linux x64 executable evidence.",
  }),
  [EXECUTABLE_PLATFORM_TARGET_LINUX_WSL]: Object.freeze({
    id: EXECUTABLE_PLATFORM_TARGET_LINUX_WSL,
    hostMode: "wsl2",
    artifactTargets: Object.freeze([...EXECUTABLE_PLATFORM_ARTIFACT_TARGETS[EXECUTABLE_PLATFORM_TARGET_LINUX_WSL]]),
    regression: true,
    rankability: "same-host-only",
    crossPlatformDiagnostics: "same-physical-hardware-only",
    description: "Linux x64 executable evidence through WSL2; not native Linux and not rankable across hosts.",
  }),
});
export const EXECUTABLE_WSL_COMPARISON_PURPOSE = "same-physical-hardware-diagnostic-only";
export const EXECUTABLE_WSL_RANKABILITY = "same-host-only";
export const EXECUTABLE_WSL_HOST_MODE = "wsl2";
// Compatibility alias for the original Windows-only public surface.
export const EXECUTABLE_PLATFORM_TARGET = EXECUTABLE_PLATFORM_TARGET_WINDOWS;
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
const PE_LAYOUT_FIELDS = Object.freeze([
  "fileAlignment",
  "sectionAlignment",
  "sizeOfHeaders",
  "sections",
]);
const PE_SECTION_FIELDS = Object.freeze(["name", "virtualSize", "rawSize"]);
const PE_SECTION_NAME_PATTERN = /^[\x20-\x7e]{1,8}$/u;
const ELF_LAYOUT_FIELDS = Object.freeze(["class", "data", "machine", "type"]);
const ELF_SECTION_FIELDS = Object.freeze(["name", "sizeBytes"]);
const ELF_SECTION_NAME_PATTERN = /^[\x20-\x7e]{1,255}$/u;
export const PROTOCOL_FIELDS = Object.freeze([
  "warmupMinimum", "rawMinimum", "rawParity", "arithmeticMeanRounding", "stopRule", "wallClock",
  "processIsolation", "runtimeScope", "order", "resourceScope", "knownNoiseControls",
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
  correctnessOnly: Object.freeze({
    comparability: "deferred-until-M3b",
    eligibility: "deferred-to-M3b",
  }),
  strictF64: Object.freeze({
    comparability: "deferred-until-M3b",
    eligibility: "deferred-to-M3b",
  }),
  platformMinimal: Object.freeze({
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
  wslDiagnostic: Object.freeze({
    comparability: "same-physical-hardware-diagnostic-only",
    eligibility: "same-physical-hardware-diagnostic-only",
  }),
  rust: Object.freeze({
    comparability: "promotable-after-equivalence",
    eligibility: "promotable-after-equivalence",
  }),
});

const SOURCE_RECIPES = Object.freeze({
  w: Object.freeze(["public-w-build-release", "public-w-run", PROCESS_ENTRY0_RECIPE]),
  c: Object.freeze([PUBLIC_C_RECIPE, PRIVATE_C_RECIPE, PLATFORM_MINIMAL_C_RECIPE]),
  rust: Object.freeze(["rustc-edition-2024", PLATFORM_MINIMAL_RUST_RECIPE]),
});

export function executableWorkloadFamily(workloadId) {
  return EXECUTABLE_WORKLOAD_FAMILY[workloadId];
}

export function executableRuntimeClosure(workload, language, recipe) {
  let runtimeClass;
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID) {
    // The private MinGW composite statically links its hosted C provider/runtime.
    runtimeClass = "hosted-crt";
  } else if (language === "w" || recipe === PLATFORM_MINIMAL_C_RECIPE || recipe === PLATFORM_MINIMAL_RUST_RECIPE) {
    // Public W links without a target CRT; the isolated C/Rust recipes use
    // explicit no-CRT/no-libc entry points. Artifact imports are not receipted.
    runtimeClass = "freestanding";
  } else if (language === "c" || language === "rust") {
    // The public MSVC recipes select/use the hosted CRT defaults.
    runtimeClass = "hosted-crt";
  }
  if (!runtimeClass) return undefined;
  return { class: runtimeClass, ...EXECUTABLE_RUNTIME_CLOSURE_UNVERIFIED };
}

function checkRuntimeClosure(value, name, errors) {
  if (!exactKeys(value, name, ["class", "status"], errors)) return;
  if (!EXECUTABLE_RUNTIME_CLOSURE_CLASSES.includes(value.class)) {
    push(errors, name + ".class must identify freestanding, hosted-crt or instrumentation closure.");
  }
  if (value.status !== "unverified") {
    push(errors, name + ".status must remain unverified until a runtime dependency receipt exists.");
  }
}

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

const SOURCE_EXPECTED_EXIT_PATTERN = /^\s*\/\/ Expected exit: ([0-9]+)$/u;
const SOURCE_EXPECTED_STREAM_PATTERN = /^\s*\/\/ Expected (stdout|stderr):$/u;
const SOURCE_EXPECTED_MARKER_PATTERN = /^\s*\/\/ Expected (?:exit|stdout|stderr)\b/u;
const SOURCE_EXPECTED_CASES_HEADER_PATTERN = /^\s*\/\/ Expected output cases \(argv => exit; stdout\):$/u;
const SOURCE_EXPECTED_CASE_ROW_PATTERN = /^\s*\/\/\s*(\[[^\]]*\])\s*=>\s*([0-9]+);\s*("(?:[^"\\]|\\.)*")\s*$/u;
const SOURCE_COMMENT_PATTERN = /^\s*\/\/(?: ?(.*))?$/u;

function sourceExpectedStreamText(lines) {
  return lines.length === 0 ? "" : lines.join("\n") + "\n";
}

/**
 * Parse the optional source-local executable oracle comments.
 *
 * A source opts in by containing one of the reserved `// Expected ...`
 * markers. Each output comment line contributes one literal output line and
 * therefore a trailing newline. Sources without a marker return undefined;
 * their oracle remains validated through the catalog without claiming
 * source-comment coverage.
 */
export function parseExecutableSourceExpectation(sourceText) {
  if (typeof sourceText !== "string") throw new TypeError("sourceText must be a string");
  const lines = sourceText.split(/\r?\n/u);
  const caseHeaders = lines.flatMap((line, index) => SOURCE_EXPECTED_CASES_HEADER_PATTERN.test(line) ? [index] : []);
  if (caseHeaders.length > 0) {
    const errors = [];
    if (caseHeaders.length !== 1) errors.push("Expected output cases header must appear at most once.");
    if (lines.some((line) => SOURCE_EXPECTED_MARKER_PATTERN.test(line))) {
      errors.push("Argument-dependent output cases cannot be mixed with single-case Expected exit/stdout/stderr markers.");
    }
    const cases = [];
    for (let index = caseHeaders[0] + 1; index < lines.length; index += 1) {
      const line = lines[index];
      if (line.trim() === "") break;
      if (!SOURCE_COMMENT_PATTERN.test(line)) break;
      const match = line.match(SOURCE_EXPECTED_CASE_ROW_PATTERN);
      if (!match) {
        errors.push("Expected output case rows must use `// [argv] => N; \"stdout\\n\"` with JSON strings.");
        continue;
      }
      try {
        const arguments_ = JSON.parse(match[1]);
        const exitCode = Number(match[2]);
        const stdout = JSON.parse(match[3]);
        if (!Array.isArray(arguments_) || arguments_.some((argument) => typeof argument !== "string")) {
          errors.push("Expected output case argv must be a JSON array of strings.");
          continue;
        }
        if (!Number.isSafeInteger(exitCode) || exitCode < 0 || exitCode > 255) {
          errors.push("Expected output case exit must be a safe byte-sized non-negative integer.");
          continue;
        }
        if (typeof stdout !== "string") {
          errors.push("Expected output case stdout must be a JSON string.");
          continue;
        }
        cases.push({ arguments: arguments_, exitCode, stdout, stderr: "" });
      } catch {
        errors.push("Expected output case argv and stdout must be valid JSON.");
      }
    }
    if (cases.length === 0) errors.push("Expected output cases must contain at least one case row.");
    return { cases, errors };
  }
  const outputLines = { stdout: [], stderr: [] };
  const declaredStreams = new Set();
  const errors = [];
  let exitCode;
  let stream;
  let marked = false;

  for (const line of lines) {
    const exitMatch = line.match(SOURCE_EXPECTED_EXIT_PATTERN);
    if (exitMatch) {
      marked = true;
      if (exitCode !== undefined) {
        errors.push("Expected exit must be declared at most once.");
      } else {
        const parsed = Number(exitMatch[1]);
        if (!Number.isSafeInteger(parsed)) {
          errors.push("Expected exit must be a non-negative safe integer.");
        } else {
          exitCode = parsed;
        }
      }
      stream = undefined;
      continue;
    }

    const streamMatch = line.match(SOURCE_EXPECTED_STREAM_PATTERN);
    if (streamMatch) {
      marked = true;
      const nextStream = streamMatch[1];
      if (declaredStreams.has(nextStream)) {
        errors.push(`Expected ${nextStream} must be declared at most once.`);
      }
      declaredStreams.add(nextStream);
      outputLines[nextStream] = [];
      stream = nextStream;
      continue;
    }

    if (SOURCE_EXPECTED_MARKER_PATTERN.test(line)) {
      marked = true;
      errors.push("Expected-output markers must use `// Expected exit: N`, `// Expected stdout:`, or `// Expected stderr:`.");
      stream = undefined;
      continue;
    }

    if (stream !== undefined) {
      const commentMatch = line.match(SOURCE_COMMENT_PATTERN);
      if (commentMatch) {
        outputLines[stream].push(commentMatch[1] ?? "");
        continue;
      }
      stream = undefined;
    }
  }

  if (!marked) return undefined;
  if (exitCode === undefined) errors.push("Expected exit marker is required when source-local output comments are present.");
  return {
    exitCode,
    stdout: sourceExpectedStreamText(outputLines.stdout),
    stderr: sourceExpectedStreamText(outputLines.stderr),
    errors,
  };
}

/**
 * Validate one opt-in source-local oracle against a source-backed catalog
 * oracle. Returns no errors for legacy sources without an opt-in marker.
 */
export function validateExecutableSourceExpectation(sourceText, oracle, location = "executable source") {
  const expectation = parseExecutableSourceExpectation(sourceText);
  if (expectation === undefined) return [];
  const errors = expectation.errors.map((error) => `${location}: ${error}`);
  if (expectation.errors.length > 0) return errors;
  if (expectation.cases !== undefined) {
    if (oracle?.kind !== "argument-dependent-output" || oracle?.status !== "source-backed") {
      errors.push(`${location}: source-local output cases require a source-backed argument-dependent catalog oracle.`);
      return errors;
    }
    if (!Array.isArray(oracle.cases) || expectation.cases.length !== oracle.cases.length) {
      errors.push(`${location}: Expected output case count must match the catalog oracle exactly.`);
      return errors;
    }
    for (const [index, expected] of expectation.cases.entries()) {
      const actual = oracle.cases[index];
      for (const field of ["arguments", "exitCode", "stdout", "stderr"]) {
        if (JSON.stringify(expected[field]) !== JSON.stringify(actual?.[field])) {
          errors.push(`${location}: Expected output case ${index + 1} ${field} must match the catalog oracle exactly.`);
        }
      }
    }
    return errors;
  }
  if (oracle?.kind !== "exact-output" || oracle?.status !== "source-backed") {
    errors.push(`${location}: source-local expected-output comments require a source-backed exact-output catalog oracle.`);
    return errors;
  }
  if (expectation.exitCode !== oracle.exitCode) {
    errors.push(`${location}: Expected exit must match the catalog oracle exactly.`);
  }
  if (expectation.stdout !== oracle.stdout) {
    errors.push(`${location}: Expected stdout must match the catalog oracle exactly.`);
  }
  if (expectation.stderr !== oracle.stderr) {
    errors.push(`${location}: Expected stderr must match the catalog oracle exactly.`);
  }
  return errors;
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

export function executableSourceDigest(source) {
  if (!Array.isArray(source?.supportSources) || source.supportSources.length === 0)
    return source?.digest;
  return "sha256:" + crypto.createHash("sha256").update(JSON.stringify({
    schema: "w-executable-source-set/1",
    root: { path: source.path, digest: source.digest },
    supportSources: source.supportSources,
  })).digest("hex");
}

function checkSourceSupport(source, name, root, errors) {
  if (!exactKeys(source, name, ["role", "path", "digest"], errors)) return;
  if (!requiredString(source.role, name + ".role", errors) ||
      !/^[a-z0-9][a-z0-9-]*$/u.test(source.role))
    push(errors, name + ".role must be a canonical lowercase role.");
  const physical = containedFile(root, source.path, name, errors);
  if (physical && digest(source.digest, name + ".digest", errors) &&
      source.digest !== fileDigest(physical)) push(errors, name + ".digest is stale.");
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
  const keys = ["language", "path", "digest", "entry", "status", "profile", "quality", "recipe", "recipeClass", "runtimeClosure", "platformTarget", "artifactTarget", "comparability", "eligibility"];
  if (source?.supportSources !== undefined) keys.push("supportSources");
  if (!exactKeys(source, location, keys, errors)) return;
  if (!EXECUTABLE_LANGUAGES.includes(source.language)) push(errors, location + ".language is invalid.");
  requiredString(source.entry, location + ".entry", errors);
  requiredString(source.recipe, location + ".recipe", errors);
  if (SOURCE_RECIPES[source.language] !== undefined && !SOURCE_RECIPES[source.language].includes(source.recipe)) {
    push(errors, location + ".recipe is not a supported recipe for " + source.language + ".");
  }
  requiredString(source.recipeClass, location + ".recipeClass", errors);
  checkRuntimeClosure(source.runtimeClosure, location + ".runtimeClosure", errors);
  const expectedRuntimeClosure = executableRuntimeClosure(workload, source.language, source.recipe);
  if (JSON.stringify(source.runtimeClosure) !== JSON.stringify(expectedRuntimeClosure)) {
    push(errors, location + ".runtimeClosure must match the runtime class selected by the declared recipe; exact imports remain unverified.");
  }
  if (source.recipe === PROCESS_ENTRY0_RECIPE && workload?.id !== PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID) push(errors, location + ".recipe is private to process-handler-lifecycle.");
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && source.language === "w" && source.recipe !== PROCESS_ENTRY0_RECIPE) push(errors, location + ".recipe must use the private process handler route.");
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && source.language === "c" && source.recipe !== PRIVATE_C_RECIPE) push(errors, location + ".recipe must use the private GCC/MinGW process handler route.");
  if (workload?.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID && source.language === "c" && source.recipe !== PLATFORM_MINIMAL_C_RECIPE) push(errors, location + ".recipe must use the platform-minimal Clang/C23 route.");
  if (workload?.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID && source.language === "rust" && source.recipe !== PLATFORM_MINIMAL_RUST_RECIPE) push(errors, location + ".recipe must use the platform-minimal Rust 2024 no_std route.");
  if (workload?.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID && source.language === "w" && source.recipe !== "public-w-build-release") push(errors, location + ".recipe must use the public W Release route.");
  if (workload?.id !== PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && workload?.id !== HELLO_PLATFORM_MINIMAL_WORKLOAD_ID && source.language === "c" && source.recipe !== PUBLIC_C_RECIPE) push(errors, location + ".recipe must use the public Clang/MSVC process route.");
  if (workload?.id !== HELLO_PLATFORM_MINIMAL_WORKLOAD_ID && source.language === "rust" && source.recipe !== "rustc-edition-2024") push(errors, location + ".recipe must use the Rust 2024 standard-library route.");
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && source.platformTarget !== EXECUTABLE_PLATFORM_TARGET_WINDOWS) push(errors, location + ".platformTarget must remain Windows x64 for the private composite process handler route.");
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && source.recipeClass !== PROCESS_ENTRY0_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the private process handler class.");
  if (workload?.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID && source.recipeClass !== HELLO_PLATFORM_MINIMAL_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the platform-minimal Hello correctness comparison.");
  if (workload?.id === PROCESS_ENTRY_WORKLOAD_ID && source.recipeClass !== PROCESS_ENTRY_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the public process-entry release class.");
  if (workload?.id === PROCESS_ENUM_PAYLOAD_WORKLOAD_ID && source.recipeClass !== PROCESS_ENUM_PAYLOAD_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the public process-enum-payload release class.");
  if (workload?.id === PROCESS_ARGUMENTS_COUNT_WORKLOAD_ID && source.recipeClass !== PROCESS_ARGUMENTS_COUNT_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the public process-arguments-count release class.");
  if (workload?.id === PROCESS_ARGUMENTS_ORDERING_WORKLOAD_ID && source.recipeClass !== PROCESS_ARGUMENTS_ORDERING_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the public process-arguments-ordering release class.");
  if (workload?.id === FIXED_INTEGER_RUNTIME_ARITHMETIC_WORKLOAD_ID && source.recipeClass !== FIXED_INTEGER_RUNTIME_ARITHMETIC_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the fixed-integer runtime arithmetic release class.");
  if (workload?.id === FLOAT_INTEGER_ROUNDING_WORKLOAD_ID && source.recipeClass !== FLOAT_INTEGER_ROUNDING_RECIPE_CLASS) push(errors, location + ".recipeClass must identify the float-integer rounding release class.");
  if (source.status !== "source-oracle-ready") push(errors, location + ".status must be source-oracle-ready for a materialized source.");
  if (!MEASUREMENT_PROFILES.includes(source.profile) || source.profile !== "release") push(errors, location + ".profile must be release for M3a sources.");
  if (source.quality !== "correctness-gate") push(errors, location + ".quality must identify correctness as a gate.");
  if (!EXECUTABLE_PLATFORM_TARGETS.includes(source.platformTarget)) {
    push(errors, location + ".platformTarget must identify one of the closed executable platform lanes.");
  }
  const expectedArtifactTarget = artifactTargetFor(workload, source.language, source.platformTarget);
  if (expectedArtifactTarget === undefined || source.artifactTarget !== expectedArtifactTarget) {
    push(errors, location + ".artifactTarget must match the closed platform/artifact mapping.");
  }
  const expectedPolicy = sourcePolicy(workload, source.language, source.recipe, source.platformTarget);
  if (source.comparability !== expectedPolicy.comparability) push(errors, location + ".comparability does not match the language ABI and benchmark readiness.");
  if (source.eligibility !== expectedPolicy.eligibility) push(errors, location + ".eligibility does not match the source comparability policy.");
  const expectedExtension = { w: ".w", c: ".c", rust: ".rs" }[source.language];
  const physical = containedFile(root, source.path, location, errors);
  if (physical && path.extname(physical).toLowerCase() !== expectedExtension) push(errors, location + ".path extension does not match language.");
  if (physical) {
    if (digest(source.digest, location + ".digest", errors) && source.digest !== fileDigest(physical)) push(errors, location + ".digest is stale.");
    errors.push(...validateExecutableSourceExpectation(
      fs.readFileSync(physical, "utf8"),
      workload?.oracle,
      location,
    ));
  }
  if (source.supportSources !== undefined) {
    if (!Array.isArray(source.supportSources) || source.supportSources.length === 0) {
      push(errors, location + ".supportSources must be a non-empty array.");
    } else {
      const roles = new Set();
      const paths = new Set([source.path]);
      for (const [index, support] of source.supportSources.entries()) {
        const supportLocation = `${location}.supportSources[${index}]`;
        checkSourceSupport(support, supportLocation, root, errors);
        if (roles.has(support?.role)) push(errors, supportLocation + ".role must be unique.");
        if (paths.has(support?.path)) push(errors, supportLocation + ".path must be unique in the source set.");
        roles.add(support?.role);
        paths.add(support?.path);
      }
    }
  }
  if (workload.status !== "source-oracle-ready") push(errors, location + " cannot be present on a non-ready workload.");
}

function checkOracleCase(testCase, location, errors) {
  if (!exactKeys(testCase, location, ["arguments", "exitCode", "stdout", "stderr"], errors)) return;
  if (!checkProcessInputVector(testCase.arguments, location + ".arguments", errors)) return;
  if (!Number.isSafeInteger(testCase.exitCode) || testCase.exitCode < 0) push(errors, location + ".exitCode must be a non-negative safe integer.");
  if (typeof testCase.stdout !== "string" || typeof testCase.stderr !== "string") push(errors, location + " output must be strings.");
}

function checkProcessArgumentOracle(oracle, location, workloadStatus, errors, workloadId) {
  const expected = processArgumentOracleFor(workloadId);
  if (!expected) {
    push(errors, location + ".kind must identify a supported argument-dependent process workload.");
    return;
  }
  if (!exactKeys(oracle, location, ["kind", "status", "timedInput", "cases"], errors)) return;
  if (oracle.kind !== expected.kind) push(errors, location + ".kind must identify argument-dependent process output.");
  const witnessName = workloadId;
  const timedInputLabel = `[${expected.timedInput.join(", ")}]`;
  if (oracle.status !== "source-backed") push(errors, location + `.status must be source-backed for the materialized ${witnessName} witness.`);
  if (checkProcessInputVector(oracle.timedInput, location + ".timedInput", errors) &&
      JSON.stringify(oracle.timedInput) !== JSON.stringify(expected.timedInput)) {
    push(errors, location + `.timedInput must remain the selected ${timedInputLabel} vector.`);
  }
  if (!Array.isArray(oracle.cases) || oracle.cases.length !== expected.cases.length) {
    push(errors, location + ".cases must contain the declared argument/output cases.");
  } else {
    for (const [index, testCase] of oracle.cases.entries()) checkOracleCase(testCase, location + ".cases[" + index + "]", errors);
    if (JSON.stringify(oracle.cases) !== JSON.stringify(expected.cases)) {
      push(errors, location + `.cases must preserve the fixed ${witnessName} input/output contract.`);
    }
  }
  if (workloadStatus === "source-oracle-ready" && oracle.status !== "source-backed") push(errors, location + " must be source-backed for a ready workload.");
}

function checkOracle(oracle, location, workloadStatus, errors, workloadId) {
  if (isProcessArgumentWorkload(workloadId)) {
    checkProcessArgumentOracle(oracle, location, workloadStatus, errors, workloadId);
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
  const keys = ["schema", "status", "recordsPath", "requiredIdentity", "provenance", "sampling", "protocolFields", "environmentFields", "artifact", "metrics", "storage", "runtimeMeasurement"];
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
  requiredString(contract.runtimeMeasurement, name + ".runtimeMeasurement", errors);
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

function checkPlatformLanes(lanes, name, errors) {
  if (!Array.isArray(lanes) || lanes.length !== EXECUTABLE_PLATFORM_TARGETS.length) {
    push(errors, name + " must contain the closed executable platform lane set.");
    return;
  }
  const seen = new Set();
  const fields = ["id", "hostMode", "artifactTargets", "regression", "rankability", "crossPlatformDiagnostics", "description"];
  for (const [index, lane] of lanes.entries()) {
    const location = `${name}[${index}]`;
    if (!exactKeys(lane, location, fields, errors)) continue;
    if (seen.has(lane.id)) push(errors, location + ".id must be unique.");
    seen.add(lane.id);
    const expectedId = EXECUTABLE_PLATFORM_TARGETS[index];
    const policy = EXECUTABLE_PLATFORM_LANE_POLICIES[lane.id];
    if (lane.id !== expectedId || !policy) push(errors, location + ".id is not in the stable platform lane order.");
    if (policy) {
      if (lane.hostMode !== policy.hostMode ||
          JSON.stringify(lane.artifactTargets) !== JSON.stringify(policy.artifactTargets) ||
          lane.regression !== policy.regression ||
          lane.rankability !== policy.rankability ||
          lane.crossPlatformDiagnostics !== policy.crossPlatformDiagnostics ||
          lane.description !== policy.description) {
        push(errors, location + " does not match the closed platform lane policy.");
      }
    }
    if (!EXECUTABLE_PLATFORM_TARGETS.includes(lane.id)) push(errors, location + ".id is invalid.");
    if (!Array.isArray(lane.artifactTargets) || lane.artifactTargets.length === 0) {
      push(errors, location + ".artifactTargets must be a non-empty array.");
    } else {
      const targets = new Set();
      for (const target of lane.artifactTargets) {
        if (!EXECUTABLE_ARTIFACT_TARGETS.includes(target)) push(errors, location + ".artifactTargets contains an unknown artifact target.");
        if (targets.has(target)) push(errors, location + ".artifactTargets must not contain duplicates.");
        targets.add(target);
      }
    }
    if (lane.regression !== true) push(errors, location + ".regression must be true for every maintained platform lane.");
    requiredString(lane.description, location + ".description", errors);
  }
  for (const id of EXECUTABLE_PLATFORM_TARGETS) if (!seen.has(id)) push(errors, name + " is missing platform lane " + id + ".");
}

export function validateExecutableCatalog(catalog, documents = undefined, root = ROOT, options = {}) {
  const errors = [];
  const keys = ["$schema", "schema", "kind", "id", "status", "metrics", "comparabilityAxes", "platformLanes", "workloads", "resultContract", "bestMetricsContract", "bestMetrics"];
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
  checkPlatformLanes(catalog.platformLanes, "executable catalog.platformLanes", errors);
  if (!Array.isArray(catalog.workloads) || catalog.workloads.length !== EXECUTABLE_WORKLOAD_IDS.length) {
    push(errors, "executable catalog.workloads must contain the closed workload set.");
  }
  const workloadIds = new Set();
  const workloads = Array.isArray(catalog.workloads) ? catalog.workloads : [];
  for (const [index, workload] of workloads.entries()) {
    const location = "executable catalog.workloads[" + index + "]";
    const workloadKeys = ["id", "family", "structureClass", "status", "sourceReadiness", "demoEvidence", "benchmarkStatus", "lane", "scope", "oracle", "sources", "blockedLanguages", "blockers"];
    if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID) workloadKeys.push("execution");
    if (!exactKeys(workload, location, workloadKeys, errors)) continue;
    if (workloadIds.has(workload.id)) push(errors, location + ".id must be unique.");
    workloadIds.add(workload.id);
    if (workload.id !== EXECUTABLE_WORKLOAD_IDS[index]) push(errors, location + ".id is not in the stable catalog order.");
    if (!EXECUTABLE_WORKLOAD_FAMILY_IDS.includes(workload.family) || workload.family !== executableWorkloadFamily(workload.id)) {
      push(errors, location + ".family must be the stable semantic family for this workload; individual workload evidence remains separate.");
    }
    if (!requiredString(workload.id, location + ".id", errors) || !requiredString(workload.scope, location + ".scope", errors)) continue;
    if (!EXECUTABLE_STRUCTURE_CLASSES.includes(workload.structureClass)) push(errors, location + ".structureClass is invalid.");
    const expectedStructureClass = workload.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID
      ? PROCESS_HANDLER_LIFECYCLE_STRUCTURE_CLASS
      : "public-end-to-end";
    if (workload.structureClass !== expectedStructureClass) push(errors, location + ".structureClass must be " + expectedStructureClass + ".");
    if (!["source-oracle-ready", "planned", "blocked"].includes(workload.status)) push(errors, location + ".status is invalid.");
    if (!["source-and-oracle-ready", "not-materialized"].includes(workload.sourceReadiness)) push(errors, location + ".sourceReadiness is invalid.");
    if (!["bounded-w-demo", "not-run"].includes(workload.demoEvidence)) push(errors, location + ".demoEvidence is invalid.");
    if (!EXECUTABLE_BENCHMARK_STATUSES.includes(workload.benchmarkStatus)) push(errors, location + ".benchmarkStatus is invalid.");
    if (workload.benchmarkStatus === "contextual-measurement-ready" && workload.id !== HELLO_PLATFORM_MINIMAL_WORKLOAD_ID) {
      push(errors, location + ".contextual-measurement-ready is reserved for the platform-minimal Hello correctness lane.");
    }
    if (workload.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID && workload.benchmarkStatus !== "contextual-measurement-ready") {
      push(errors, location + ".benchmarkStatus must preserve platform-minimal Hello as contextual, non-ranking measurement evidence.");
    }
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
    const partialExploratoryReady = workload.benchmarkStatus === "partial-exploratory-ready";
    stringArray(workload.blockers, location + ".blockers", errors, exploratoryReady ? 0 : 1);
    if (exploratoryReady && (workload.blockers?.length !== 0 || workload.blockedLanguages?.length !== 0)) {
      push(errors, location + ".benchmarkStatus cannot be exploratory-ready with active measurement blockers.");
    }
    const sourceLanguages = new Set();
    const sourceKeys = new Set();
    const sources = Array.isArray(workload.sources) ? workload.sources : [];
    for (const [sourceIndex, source] of sources.entries()) {
      const sourceLocation = location + ".sources[" + sourceIndex + "]";
      checkSource(source, sourceLocation, workload, root, errors);
      const sourceKey = `${source?.platformTarget ?? ""}\u0000${source?.language ?? ""}`;
      if (sourceKeys.has(sourceKey)) push(errors, sourceLocation + ".language/platformTarget pair must be unique per workload.");
      sourceKeys.add(sourceKey);
      sourceLanguages.add(source?.language);
      if (Array.isArray(workload.blockedLanguages) && workload.blockedLanguages.includes(source?.language)) push(errors, sourceLocation + ".language cannot be both materialized and blocked.");
    }
    const blockedLanguages = Array.isArray(workload.blockedLanguages) ? workload.blockedLanguages : [];
    const partition = new Set([...sourceLanguages, ...blockedLanguages]);
    for (const language of EXECUTABLE_LANGUAGES) if (!partition.has(language)) push(errors, location + " must account for language " + language + " as a source or explicit blocker.");
    if (partialExploratoryReady && (sources.length === 0 || blockedLanguages.length === 0 || workload.blockers?.length === 0)) {
      push(errors, location + ".benchmarkStatus partial-exploratory-ready requires materialized sources plus explicit blocked languages and blockers.");
    }
    if (partialExploratoryReady && !executableWorkloadHasRunner(workload)) {
      push(errors, location + ".benchmarkStatus partial-exploratory-ready requires a runner-supported source recipe.");
    }
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
    const nestedFixturePattern = /resolve\(\s*seedDirectory\s*,\s*"fixtures"\s*,\s*"([^"]+)"\s*,\s*"([^"]+\.w)"\s*\)/gu;
    for (const match of gateSource.matchAll(nestedFixturePattern))
      publicFixtures.add(`compiler/seed-c/fixtures/${match[1]}/${match[2]}`);
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
  errors.push(...validateExecutableBestMetrics(catalog.bestMetrics, catalog, options).map((error) => "best metrics: " + error));
  return errors;
}

function workloadFor(catalog, id) {
  return Array.isArray(catalog?.workloads) ? catalog.workloads.find((item) => item?.id === id) : undefined;
}

function sourceFor(workload, language, platformTarget = undefined) {
  if (!Array.isArray(workload?.sources)) return undefined;
  const matches = workload.sources.filter((item) => item?.language === language);
  if (platformTarget !== undefined) return matches.find((item) => item.platformTarget === platformTarget);
  // Existing catalog consumers have an implicit Windows lane. Keep that
  // default while allowing a workload to carry one source per platform lane.
  return matches.find((item) => item.platformTarget === EXECUTABLE_PLATFORM_TARGET_WINDOWS) ??
    (matches.length === 1 ? matches[0] : undefined);
}

export function executableArtifactTargetFor(workload, language, platformTarget = EXECUTABLE_PLATFORM_TARGET_WINDOWS) {
  if (!EXECUTABLE_PLATFORM_TARGETS.includes(platformTarget)) return undefined;
  if (platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX || platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) return EXECUTABLE_ARTIFACT_TARGET_LINUX;
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID) return EXECUTABLE_ARTIFACT_TARGET_MINGW;
  return EXECUTABLE_ARTIFACT_TARGET_MSVC;
}

function artifactTargetFor(workload, language, platformTarget = EXECUTABLE_PLATFORM_TARGET_WINDOWS) {
  return executableArtifactTargetFor(workload, language, platformTarget);
}

function isWslOrCompositeEnvironment(environment) {
  const values = [environment?.os, environment?.kernel].map((value) => String(value ?? "").toLowerCase());
  return values.some((value) => /(?:wsl|microsoft|lxss|interop|composite)/u.test(value));
}

function isLinuxEnvironment(environment) {
  const osName = String(environment?.os ?? "").toLowerCase();
  return osName === "linux" || osName.startsWith("linux-");
}

function isWslEnvironment(environment) {
  if (!isLinuxEnvironment(environment)) return false;
  const values = [environment?.os, environment?.kernel].map((value) => String(value ?? "").toLowerCase());
  return values.some((value) => /(?:wsl|microsoft|lxss)/u.test(value)) &&
    !values.some((value) => /interop|composite/u.test(value));
}

export function executableNativeHostForPlatform(environment, platformTarget) {
  if (!isObject(environment) || !EXECUTABLE_PLATFORM_TARGETS.includes(platformTarget)) return false;
  if (isWslOrCompositeEnvironment(environment)) return false;
  const osName = String(environment.os ?? "").toLowerCase();
  if (platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX) return isLinuxEnvironment(environment);
  return osName === "windows" || osName.startsWith("windows-");
}

export function executableHostEvidenceForPlatform(environment, platformTarget) {
  if (!isObject(environment) || !EXECUTABLE_PLATFORM_TARGETS.includes(platformTarget)) return false;
  if (platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) return isWslEnvironment(environment);
  return executableNativeHostForPlatform(environment, platformTarget);
}

function checkPlatformHostEvidence(environment, platformTarget, name, errors) {
  if (!EXECUTABLE_PLATFORM_TARGETS.includes(platformTarget)) return;
  if (!executableHostEvidenceForPlatform(environment, platformTarget)) {
    const label = platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL
      ? "Linux through WSL2"
      : platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX ? "native Linux" : "native Windows";
    push(errors, `${name} must provide ${label} host evidence; WSL is only valid in the explicit WSL lane and composite hosts are not native platform evidence.`);
  }
}

function executableHostSlugSupportsPlatform(host, platformTarget) {
  const value = String(host ?? "").toLowerCase();
  if (!EXECUTABLE_PLATFORM_TARGETS.includes(platformTarget) || /(?:interop|composite)/u.test(value)) return false;
  if (platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) {
    return (value === "linux" || value.startsWith("linux-")) && /(?:wsl|microsoft|lxss)/u.test(value);
  }
  if (/(?:wsl|microsoft|lxss)/u.test(value)) return false;
  if (platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX) return value === "linux" || value.startsWith("linux-");
  return value === "windows" || value.startsWith("windows-");
}

function sourcePolicy(workload, language, recipe, platformTarget = EXECUTABLE_PLATFORM_TARGET_WINDOWS) {
  if (platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) return SOURCE_ELIGIBILITY.wslDiagnostic;
  if (workload?.id === "terminal-returns") return SOURCE_ELIGIBILITY.correctnessOnly;
  if (workload?.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID) return SOURCE_ELIGIBILITY.platformMinimal;
  if (workload?.id === FLOAT_STRICT_WORKLOAD_ID ||
      workload?.id === FLOAT_BIT_REPRESENTATION_WORKLOAD_ID ||
      workload?.id === CHECKED_INTEGER_ARITHMETIC_WORKLOAD_ID ||
      workload?.id === INTEGER_PREFIX_WORKLOAD_ID ||
      workload?.id === INTEGER_WRAPPING_WORKLOAD_ID ||
      workload?.id === INTEGER_WIDENING_WORKLOAD_ID ||
      workload?.id === NUMERIC_WIDENING_WORKLOAD_ID ||
      workload?.id === INTEGER_TRUNCATING_BITS_WORKLOAD_ID ||
      workload?.id === INTEGER_SATURATING_CONVERSION_WORKLOAD_ID ||
      workload?.id === INTEGER_COMPARISON_WORKLOAD_ID ||
      workload?.id === INTEGER_BITWISE_WORKLOAD_ID ||
      workload?.id === INTEGER_SHIFT_SEMANTICS_WORKLOAD_ID ||
      workload?.id === UINT_BITWISE_WORKLOAD_ID ||
      workload?.id === FIXED_INTEGER_BIT_PRIMITIVES_WORKLOAD_ID ||
      workload?.id === UINT_COMPOUND_WORKLOAD_ID ||
      workload?.id === UINT_OVERFLOWING_FAMILY_WORKLOAD_ID ||
      workload?.id === UINT_SATURATING_POLICY_WORKLOAD_ID ||
      workload?.id === FLOAT_INTEGER_ROUNDING_WORKLOAD_ID) return SOURCE_ELIGIBILITY.strictF64;
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
  if (!workload || !EXECUTABLE_PLATFORM_TARGETS.includes(platformTarget) || typeof recipeClass !== "string") return undefined;
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
  if (!["native-target-process-batch", "direct-host-process"].includes(protocol.runtimeScope)) {
    push(errors, name + ".runtimeScope must distinguish the native target-process batch from the test-only host fallback.");
  }
  if (!["deterministic-interleaved", "compile-series-then-run-series"].includes(protocol.order)) {
    push(errors, name + ".order must declare a supported deterministic measurement order.");
  }
  requiredString(protocol.resourceScope, name + ".resourceScope", errors);
  if (!["w-native-benchmark/2", "w-linux-native-benchmark/1", "bun-direct-test/1"].includes(protocol.measurementKernel)) {
    push(errors, name + ".measurementKernel must identify the native production kernel or the bounded test adapter.");
  }
  stringArray(protocol.knownNoiseControls, name + ".knownNoiseControls", errors, 1);
  stringArray(protocol.unknownNoiseControls, name + ".unknownNoiseControls", errors, 1);
  const disclosure = String(protocol.directProcessDisclosure ?? "");
  const legacyDisclosure = /Bun.*direct.*process|direct.*process.*Bun/iu.test(disclosure) &&
    /process[- ]tree.*aggregat|aggregat.*process[- ]tree/iu.test(disclosure);
  const nativeDisclosure = /QPC/iu.test(disclosure) && /Job Object/iu.test(disclosure) &&
    /working set/iu.test(disclosure) && /commit/iu.test(disclosure);
  const linuxDisclosure = /CLOCK_MONOTONIC/iu.test(disclosure) && /wait4/iu.test(disclosure) &&
    /root-process/iu.test(disclosure) && /descendants.*not aggregated/iu.test(disclosure) &&
    /process group/iu.test(disclosure) && /best-effort/iu.test(disclosure);
  if (!requiredString(protocol.directProcessDisclosure, name + ".directProcessDisclosure", errors) ||
      (protocol.measurementKernel === "w-native-benchmark/2"
        ? !nativeDisclosure
        : protocol.measurementKernel === "w-linux-native-benchmark/1"
          ? !linuxDisclosure
          : !legacyDisclosure)) {
    push(errors, name + ".directProcessDisclosure must match the selected measurement kernel and distinguish process-tree CPU, root working set, and Job commit.");
  }
  if (protocol.runtimeScope === "native-target-process-batch" && protocol.measurementKernel === "bun-direct-test/1") {
    push(errors, name + ".runtimeScope native-target-process-batch requires a native measurement kernel.");
  }
  if (protocol.runtimeScope === "direct-host-process" && protocol.measurementKernel !== "bun-direct-test/1") {
    push(errors, name + ".runtimeScope direct-host-process is reserved for the test-only Bun adapter.");
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

function checkPlatformEvidence(value, name, platformTarget, errors) {
  if (platformTarget !== EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) return;
  const fields = ["hostMode", "comparisonPurpose", "rankability"];
  if (!exactKeys(value, name, fields, errors)) return;
  if (value.hostMode !== EXECUTABLE_WSL_HOST_MODE) push(errors, name + ".hostMode must identify WSL2 execution.");
  if (value.comparisonPurpose !== EXECUTABLE_WSL_COMPARISON_PURPOSE) push(errors, name + ".comparisonPurpose must identify same-physical-hardware diagnostics only.");
  if (value.rankability !== EXECUTABLE_WSL_RANKABILITY) push(errors, name + ".rankability must remain same-host-only.");
}

function boundedPeUInt32(value, name, errors, positive = false) {
  const valid = decimal(value, name, errors);
  if (!valid) return undefined;
  if (positive && value === "0") {
    push(errors, name + " must be positive.");
    return undefined;
  }
  try {
    const parsed = BigInt(value);
    if (parsed > 0xffff_ffffn) {
      push(errors, name + " must fit in a PE uint32.");
      return undefined;
    }
    return parsed;
  } catch {
    return undefined;
  }
}

function isPowerOfTwo(value) {
  return value > 0n && (value & (value - 1n)) === 0n;
}

function checkPeLayout(layout, name, errors, artifactSize = undefined) {
  if (!exactKeys(layout, name, PE_LAYOUT_FIELDS, errors)) return;
  const fileAlignment = boundedPeUInt32(layout.fileAlignment, `${name}.fileAlignment`, errors, true);
  const sectionAlignment = boundedPeUInt32(layout.sectionAlignment, `${name}.sectionAlignment`, errors, true);
  const sizeOfHeaders = boundedPeUInt32(layout.sizeOfHeaders, `${name}.sizeOfHeaders`, errors, true);
  if (fileAlignment !== undefined && (!isPowerOfTwo(fileAlignment) || fileAlignment > 65_536n)) {
    push(errors, `${name}.fileAlignment must be a power-of-two PE file alignment no greater than 65536.`);
  }
  if (sectionAlignment !== undefined && (!isPowerOfTwo(sectionAlignment) || sectionAlignment < (fileAlignment ?? 0n))) {
    push(errors, `${name}.sectionAlignment must be a power-of-two alignment at least as large as fileAlignment.`);
  }
  if (fileAlignment !== undefined && sectionAlignment !== undefined &&
      ((sectionAlignment >= 4096n && fileAlignment < 512n) ||
       (sectionAlignment < 4096n && sectionAlignment !== fileAlignment))) {
    push(errors, `${name} has an incompatible PE alignment pair.`);
  }
  if (fileAlignment !== undefined && sizeOfHeaders !== undefined && sizeOfHeaders % fileAlignment !== 0n) {
    push(errors, `${name}.sizeOfHeaders must be file-aligned.`);
  }
  if (!Array.isArray(layout.sections) || layout.sections.length === 0 || layout.sections.length > 65_535) {
    push(errors, `${name}.sections must contain between one and 65535 PE sections.`);
    return;
  }
  let rawTotal = 0n;
  for (const [index, section] of layout.sections.entries()) {
    const sectionName = `${name}.sections[${index}]`;
    if (!exactKeys(section, sectionName, PE_SECTION_FIELDS, errors)) continue;
    if (typeof section.name !== "string" || !PE_SECTION_NAME_PATTERN.test(section.name)) {
      push(errors, `${sectionName}.name must be one to eight printable ASCII characters.`);
    }
    boundedPeUInt32(section.virtualSize, `${sectionName}.virtualSize`, errors);
    const rawSize = boundedPeUInt32(section.rawSize, `${sectionName}.rawSize`, errors);
    if (fileAlignment !== undefined && rawSize !== undefined && rawSize % fileAlignment !== 0n) {
      push(errors, `${sectionName}.rawSize must be file-aligned.`);
    }
    if (rawSize !== undefined) rawTotal += rawSize;
  }
  const parsedArtifactSize = typeof artifactSize === "string" && /^[1-9][0-9]*$/u.test(artifactSize)
    ? BigInt(artifactSize)
    : undefined;
  if (sizeOfHeaders !== undefined && parsedArtifactSize !== undefined && sizeOfHeaders + rawTotal > parsedArtifactSize) {
    push(errors, `${name} headers plus section raw sizes must fit within the artifact.`);
  }
}

function checkElfLayout(layout, name, errors) {
  const fields = Object.prototype.hasOwnProperty.call(layout ?? {}, "sections")
    ? [...ELF_LAYOUT_FIELDS, "sections"]
    : ELF_LAYOUT_FIELDS;
  if (!exactKeys(layout, name, fields, errors)) return;
  if (layout.class !== "ELF64") push(errors, `${name}.class must identify an ELF64 artifact.`);
  if (layout.data !== "little-endian") push(errors, `${name}.data must identify little-endian ELF.`);
  if (layout.machine !== "x86-64") push(errors, `${name}.machine must identify x86-64 ELF.`);
  if (!['executable', 'pie'].includes(layout.type)) push(errors, `${name}.type must identify an executable or PIE ELF.`);
  if (layout.sections === undefined) return;
  if (!Array.isArray(layout.sections) || layout.sections.length > 65_535) {
    push(errors, `${name}.sections must contain at most 65535 named ELF sections.`);
    return;
  }
  for (const [index, section] of layout.sections.entries()) {
    const sectionName = `${name}.sections[${index}]`;
    if (!exactKeys(section, sectionName, ELF_SECTION_FIELDS, errors)) continue;
    if (typeof section.name !== "string" || !ELF_SECTION_NAME_PATTERN.test(section.name)) {
      push(errors, `${sectionName}.name must be one to 255 printable ASCII characters.`);
    }
    decimal(section.sizeBytes, `${sectionName}.sizeBytes`, errors);
  }
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

function checkProcessArgumentCorrectness(correctness, workload, name, errors) {
  if (!exactKeys(correctness, name, ["oracleId", "cases"], errors)) return;
  requiredString(correctness.oracleId, name + ".oracleId", errors);
  const expectedOracleKind = processArgumentOracleFor(workload?.id)?.kind;
  if (workload && correctness.oracleId !== `${workload.id}:${expectedOracleKind}`) {
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
  const source = sourceFor(workload, result.language, result.platformTarget);
  if (!source) push(errors, "executable result language must identify a materialized source.");
  if (!EXECUTABLE_LANGUAGES.includes(result.language)) push(errors, "executable result.language is invalid.");
  const expectedPolicy = sourcePolicy(workload, result.language, source?.recipe, result.platformTarget);
  if (source && (source.comparability !== expectedPolicy.comparability || source.eligibility !== expectedPolicy.eligibility)) {
    push(errors, "executable result source comparability and eligibility must match the exact catalog policy.");
  }
  if (!EXECUTABLE_PLATFORM_TARGETS.includes(result.platformTarget) || (source && result.platformTarget !== source.platformTarget)) {
    push(errors, "executable result.platformTarget must identify a closed platform lane and match the source identity.");
  }
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && result.platformTarget !== EXECUTABLE_PLATFORM_TARGET_WINDOWS) {
    push(errors, "executable result.platformTarget must remain Windows x64 for the private composite process handler route.");
  }
  if (source && result.artifactTarget !== source.artifactTarget) push(errors, "executable result.artifactTarget must match the source ABI target.");
  const expectedResultArtifactTarget = artifactTargetFor(workload, result.language, result.platformTarget);
  if (expectedResultArtifactTarget === undefined || result.artifactTarget !== expectedResultArtifactTarget) push(errors, "executable result artifact target must match the workload platform/artifact mapping.");
  if (result.profile !== "release") push(errors, "executable result.profile must be release in M3a.");
  if (result.quality !== "exploratory") push(errors, "executable result.quality must be exploratory for measurement evidence.");
  if (result.claim !== "measurement-only") push(errors, "executable result.claim must be measurement-only.");
  if (result.verdict !== "not-evaluated") push(errors, "executable result.verdict must be not-evaluated until managed regression exists.");
  const expectedKey = source ? executableEquivalenceKey(catalog, result.workloadId, result.platformTarget, result.profile, source.recipeClass) : undefined;
  if (!digest(result.equivalenceKey, "executable result.equivalenceKey", errors) || result.equivalenceKey !== expectedKey) push(errors, "executable result.equivalenceKey must be recomputed from workload semantics, platform target, profile and recipe class.");
  if (exactKeys(result.identity, "executable result.identity", ["sourceDigest", "platformTarget", "artifactTarget", "profile", "toolchain", "host", "recipe", "recipeClass", "runtimeClosure", "recipeDigest", "eligibility"], errors)) {
    digest(result.identity.sourceDigest, "executable result.identity.sourceDigest", errors);
    if (!EXECUTABLE_PLATFORM_TARGETS.includes(result.identity.platformTarget) || result.identity.platformTarget !== result.platformTarget) push(errors, "executable result.identity.platformTarget must match the shared platform target.");
    if (result.identity.artifactTarget !== result.artifactTarget) push(errors, "executable result.identity.artifactTarget must match the exact artifact target.");
    if (result.identity.profile !== result.profile) push(errors, "executable result identity profile must match the result.");
    checkSafeIdentityString(result.identity.toolchain, "executable result.identity.toolchain", errors);
    checkSafeIdentityString(result.identity.host, "executable result.identity.host", errors);
    requiredString(result.identity.recipe, "executable result.identity.recipe", errors);
    requiredString(result.identity.recipeClass, "executable result.identity.recipeClass", errors);
    checkRuntimeClosure(result.identity.runtimeClosure, "executable result.identity.runtimeClosure", errors);
    digest(result.identity.recipeDigest, "executable result.identity.recipeDigest", errors);
    requiredString(result.identity.eligibility, "executable result.identity.eligibility", errors);
    if (source && result.identity.sourceDigest !== executableSourceDigest(source)) push(errors, "executable result.identity.sourceDigest must match the catalog source set.");
    if (source && JSON.stringify(result.identity.runtimeClosure) !== JSON.stringify(source.runtimeClosure)) {
      push(errors, "executable result.identity.runtimeClosure must match the catalog recipe class and remain unverified until dependency imports are receipted.");
    }
    const historicalWRecipe = options.allowHistoricalWRecipe === true && result.language === "w" && source?.recipe === "public-w-build-release" && result.identity.recipe === LEGACY_W_RESULT_RECIPE && result.identity.eligibility === LEGACY_W_RESULT_ELIGIBILITY;
    const identityMismatch = historicalWRecipe
      ? result.identity.recipeClass !== source?.recipeClass || result.identity.platformTarget !== source?.platformTarget || result.identity.artifactTarget !== source?.artifactTarget
      : result.identity.recipe !== source?.recipe || result.identity.recipeClass !== source?.recipeClass || result.identity.eligibility !== source?.eligibility || result.identity.platformTarget !== source?.platformTarget || result.identity.artifactTarget !== source?.artifactTarget;
    if (source && identityMismatch) push(errors, "executable result identity must match catalog target, recipe, recipe class and eligibility.");
  }
  if (isProcessArgumentWorkload(workload?.id)) {
    checkProcessArgumentCorrectness(result.correctness, workload, "executable result.correctness", errors);
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
  const hasPeLayout = isObject(result.artifact) && Object.prototype.hasOwnProperty.call(result.artifact, "peLayout");
  const hasElfLayout = isObject(result.artifact) && Object.prototype.hasOwnProperty.call(result.artifact, "elfLayout");
  if (hasPeLayout && hasElfLayout) push(errors, "executable result.artifact must not mix PE and ELF layout evidence.");
  if (hasPeLayout && result.platformTarget !== EXECUTABLE_PLATFORM_TARGET_WINDOWS) push(errors, "executable result.artifact.peLayout is only valid for the Windows PE lane.");
  if (hasElfLayout && ![EXECUTABLE_PLATFORM_TARGET_LINUX, EXECUTABLE_PLATFORM_TARGET_LINUX_WSL].includes(result.platformTarget)) push(errors, "executable result.artifact.elfLayout is only valid for a Linux ELF lane.");
  if (hasPeLayout && !hasArtifactCleanliness) {
    push(errors, "executable result.artifact.peLayout requires validated artifact.cleanliness.");
  }
  if (hasElfLayout && !hasArtifactCleanliness) {
    push(errors, "executable result.artifact.elfLayout requires validated artifact.cleanliness.");
  }
  if (hasPeLayout) artifactFields.push("peLayout");
  if (hasElfLayout) artifactFields.push("elfLayout");
  if (exactKeys(result.artifact, "executable result.artifact", artifactFields, errors)) {
    digest(result.artifact.digest, "executable result.artifact.digest", errors);
    positiveDecimal(result.artifact.sizeBytes, "executable result.artifact.sizeBytes", errors);
    if (artifactFields.includes("cleanliness")) checkArtifactCleanliness(result.artifact.cleanliness, "executable result.artifact.cleanliness", errors);
    if (artifactFields.includes("peLayout")) checkPeLayout(result.artifact.peLayout, "executable result.artifact.peLayout", errors, result.artifact.sizeBytes);
    if (artifactFields.includes("elfLayout")) checkElfLayout(result.artifact.elfLayout, "executable result.artifact.elfLayout", errors);
  }
  checkProtocol(result.protocol, "executable result.protocol", errors);
  checkEnvironment(result.environment, "executable result.environment", errors);
  checkPlatformHostEvidence(result.environment, result.platformTarget, "executable result.environment", errors);
  const expectedHost = executableHostIdentity(result.environment);
  if (expectedHost && result.identity?.host !== expectedHost) push(errors, "executable result.identity.host must be derived from the redacted environment.");
  checkSampleSeries(result.compile, "executable result.compile", errors);
  checkSampleSeries(result.run, "executable result.run", errors);
  const provenanceKeys = ["sourceDigest", "artifactDigest", "recipeDigest", "toolchainDigest", "runnerDigest", "catalogDigest", "commit", "observedAt"];
  if (result.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) provenanceKeys.push("platformEvidence");
  if (exactKeys(result.provenance, "executable result.provenance", provenanceKeys, errors)) {
    for (const field of ["sourceDigest", "artifactDigest", "recipeDigest", "toolchainDigest", "runnerDigest", "catalogDigest"]) digest(result.provenance[field], "executable result.provenance." + field, errors);
    if (typeof result.provenance.commit !== "string" || !/^[0-9a-f]{40}$/u.test(result.provenance.commit)) push(errors, "executable result.provenance.commit must be the full lowercase Git commit identity.");
    checkObservedAt(result.provenance.observedAt, "executable result.provenance.observedAt", errors);
    if (result.provenance.sourceDigest !== result.identity?.sourceDigest || result.provenance.artifactDigest !== result.artifact?.digest || result.provenance.recipeDigest !== result.identity?.recipeDigest) push(errors, "executable result provenance must repeat source, artifact and recipe identity exactly.");
    if (result.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) checkPlatformEvidence(result.provenance.platformEvidence, "executable result.provenance.platformEvidence", result.platformTarget, errors);
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

function workloadAllowsBestMetrics(workload) {
  return BEST_METRIC_BENCHMARK_STATUSES.includes(workload?.benchmarkStatus);
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

export function executableBestMetricId(category, metric) {
  return "best-" + crypto.createHash("sha256").update(`${category}\u0000${metric}`, "utf8").digest("hex");
}

function compareResultForTie(left, right) {
  return compareText(String(left?.id ?? ""), String(right?.id ?? "")) ||
    compareText(String(left?.artifact?.digest ?? ""), String(right?.artifact?.digest ?? "")) ||
    compareText(String(left?.provenance?.commit ?? ""), String(right?.provenance?.commit ?? ""));
}

function hasPeLayout(value) {
  return isObject(value?.artifact?.peLayout) || isObject(value?.peLayout);
}

function hasArtifactLayout(value) {
  return hasPeLayout(value) || isObject(value?.artifact?.elfLayout) || isObject(value?.elfLayout);
}

function sameArtifactEvidence(left, right) {
  return left?.equivalenceKey === right?.equivalenceKey &&
    (left?.provenance?.sourceDigest ?? left?.identity?.sourceDigest) ===
      (right?.provenance?.sourceDigest ?? right?.identity?.sourceDigest) &&
    (left?.provenance?.recipeDigest ?? left?.identity?.recipeDigest) ===
      (right?.provenance?.recipeDigest ?? right?.identity?.recipeDigest);
}

function chooseBestRecord(values, metric) {
  const minimum = values.reduce((best, item) => item.value < best ? item.value : best, values[0].value);
  const tied = values.filter((item) => item.value === minimum).map((item) => item.record).sort(compareResultForTie);
  let primary = tied[0];
  if (metric === "artifact-size" && !hasArtifactLayout(primary)) {
    const withLayout = tied.filter((candidate) => hasArtifactLayout(candidate) && sameArtifactEvidence(primary, candidate));
    if (withLayout.length > 0) primary = withLayout[0];
  }
  return primary;
}

function entryFromResult(record, catalog, metric) {
  const workload = workloadFor(catalog, record.workloadId);
  const source = sourceFor(workload, record.language, record.platformTarget);
  const category = {
    workloadId: record.workloadId,
    language: record.language,
    equivalenceKey: record.equivalenceKey,
    platformTarget: record.platformTarget,
    artifactTarget: record.artifactTarget,
    abi: record.artifactTarget,
    profile: record.profile,
    toolchain: record.identity.toolchain,
    host: record.identity.host,
    recipe: record.identity.recipe,
    recipeClass: record.identity.recipeClass,
    runtimeClosure: record.identity.runtimeClosure,
    comparability: source?.comparability,
    eligibility: source?.eligibility,
  };
  const categoryKey = executableCategoryKey(category);
  const entry = {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_BEST_SCHEMA,
    kind: "executable-best-metric",
    id: executableBestMetricId(categoryKey, metric),
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
    runtimeClosure: structuredClone(record.identity.runtimeClosure),
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
  if (metric === "artifact-size" && record.artifact?.peLayout !== undefined) {
    entry.peLayout = structuredClone(record.artifact.peLayout);
  }
  if (metric === "artifact-size" && record.artifact?.elfLayout !== undefined) {
    entry.elfLayout = structuredClone(record.artifact.elfLayout);
  }
  if (record.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) {
    entry.provenance.platformEvidence = structuredClone(record.provenance.platformEvidence);
  }
  return entry;
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
    if (!workloadAllowsBestMetrics(workload)) continue;
    const source = sourceFor(workload, record.language, record.platformTarget);
    if (!source) continue;
    const category = {
      workloadId: record.workloadId,
      language: record.language,
      equivalenceKey: record.equivalenceKey,
      platformTarget: record.platformTarget,
      artifactTarget: record.artifactTarget,
      abi: record.artifactTarget,
      profile: record.profile,
      toolchain: record.identity.toolchain,
      host: record.identity.host,
      recipe: record.identity.recipe,
      recipeClass: record.identity.recipeClass,
      runtimeClosure: record.identity.runtimeClosure,
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
      const primary = chooseBestRecord(values, metric);
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
    compareText(String(left?.toolchain ?? ""), String(right?.toolchain ?? "")) ||
    compareText(String(left?.host ?? ""), String(right?.host ?? "")) ||
    compareText(String(left?.recipe ?? ""), String(right?.recipe ?? "")) ||
    compareText(String(left?.recipeClass ?? ""), String(right?.recipeClass ?? "")) ||
    compareText(JSON.stringify(left?.runtimeClosure ?? null), JSON.stringify(right?.runtimeClosure ?? null)) ||
    compareText(String(left?.metric ?? ""), String(right?.metric ?? "")) ||
    compareText(String(left?.id ?? ""), String(right?.id ?? ""));
}

export function validateExecutableBestMetric(record, catalog = loadExecutableDocuments().catalog, options = {}) {
  const errors = [];
  const keys = ["$schema", "schema", "kind", "id", "status", "categoryId", "workloadId", "language", "equivalenceKey", "platformTarget", "artifactTarget", "abi", "profile", "host", "recipeClass", "runtimeClosure", "comparability", "eligibility", "metric", "unit", "statistic", "value", "toolchain", "recipe", "provenance"];
  if (isObject(record) && Object.prototype.hasOwnProperty.call(record, "peLayout")) keys.push("peLayout");
  if (isObject(record) && Object.prototype.hasOwnProperty.call(record, "elfLayout")) keys.push("elfLayout");
  if (!exactKeys(record, "executable best-metric record", keys, errors)) return errors;
  if (record.$schema !== "./executable-benchmark.schema.json" || record.schema !== EXECUTABLE_BEST_SCHEMA || record.kind !== "executable-best-metric" || record.status !== "current") push(errors, "executable best-metric identity or status is invalid.");
  requiredString(record.id, "executable best metric.id", errors);
  const workload = workloadFor(catalog, record.workloadId);
  const source = sourceFor(workload, record.language, record.platformTarget);
  if (!workload || workload.status !== "source-oracle-ready") push(errors, "executable best metric must identify a ready workload.");
  if (workload && !workloadAllowsBestMetrics(workload)) {
    push(errors, "executable best metric must identify a workload eligible for live best metrics.");
  }
  if (!OPTIMIZABLE_METRICS.includes(record.metric)) push(errors, "executable best metric must be optimizable, never exit-code/stdout/stderr.");
  if (!EXECUTABLE_LANGUAGES.includes(record.language) || !source) push(errors, "executable best metric.language must identify a materialized source.");
  if (!EXECUTABLE_PLATFORM_TARGETS.includes(record.platformTarget)) push(errors, "executable best metric.platformTarget must identify a closed platform lane.");
  if (workload?.id === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID && record.platformTarget !== EXECUTABLE_PLATFORM_TARGET_WINDOWS) {
    push(errors, "executable best metric.platformTarget must remain Windows x64 for the private composite process handler route.");
  }
  if (!executableHostSlugSupportsPlatform(record.host, record.platformTarget)) push(errors, "executable best metric.host must identify a native host for its platform lane; WSL and composite hosts are not rankable.");
  if (source && record.artifactTarget !== source.artifactTarget) push(errors, "executable best metric.artifactTarget must match the source ABI target.");
  if (record.abi !== record.artifactTarget) push(errors, "executable best metric.abi must equal the artifact target.");
  if (record.artifactTarget !== artifactTargetFor(workload, record.language, record.platformTarget)) push(errors, "executable best metric artifact target must match the workload platform/artifact mapping.");
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
  checkRuntimeClosure(record.runtimeClosure, "executable best metric.runtimeClosure", errors);
  const staleSourceDigest = source &&
    record.provenance?.sourceDigest !== executableSourceDigest(source);
  if (source && record.recipeClass !== source.recipeClass) push(errors, "executable best metric.recipeClass must match the catalog source.");
  if (source) {
    if (JSON.stringify(record.runtimeClosure) !== JSON.stringify(source.runtimeClosure)) push(errors, "executable best metric.runtimeClosure must match the catalog source and remain unverified until a dependency receipt exists.");
    const expectedKey = executableEquivalenceKey(catalog, record.workloadId, record.platformTarget, record.profile, source.recipeClass);
    if (record.equivalenceKey !== expectedKey &&
        !(staleSourceDigest && options.allowStaleSourceDigest === true)) {
      push(errors, "executable best metric.equivalenceKey must be recomputed from the catalog.");
    }
    if (record.comparability !== source.comparability || record.eligibility !== source.eligibility) push(errors, "executable best metric policy must match the catalog source.");
  }
  positiveDecimal(record.value, "executable best metric.value", errors);
  const expectedCategoryId = executableCategoryId(record);
  if (record.categoryId !== expectedCategoryId) push(errors, "executable best metric.categoryId must be derived from its category identity.");
  const expectedId = executableBestMetricId(executableCategoryKey(record), record.metric);
  if (record.id !== expectedId) push(errors, "executable best metric.id must be derived from its category identity and metric.");
  const provenanceKeys = [...BEST_METRIC_PROVENANCE_FIELDS];
  if (record.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) provenanceKeys.push("platformEvidence");
  if (exactKeys(record.provenance, "executable best metric.provenance", provenanceKeys, errors)) {
    requiredString(record.provenance.recordId, "executable best metric.provenance.recordId", errors);
    if (typeof record.provenance.commit !== "string" || !/^[0-9a-f]{40}$/u.test(record.provenance.commit)) push(errors, "executable best metric.provenance.commit must be a full lowercase Git commit identity.");
    checkObservedAt(record.provenance.observedAt, "executable best metric.provenance.observedAt", errors);
    for (const field of ["sourceDigest", "artifactDigest", "recipeDigest", "toolchainDigest", "runnerDigest", "catalogDigest"]) digest(record.provenance[field], "executable best metric.provenance." + field, errors);
    if (!['historical-unverified', 'verified-clean'].includes(record.provenance.artifactCleanliness)) push(errors, "executable best metric.provenance.artifactCleanliness must be historical-unverified or verified-clean.");
    // A benchmark run/update may explicitly tolerate stale cells long enough
    // to replace them. Catalog checks remain strict so stale history is never
    // presented as a current measurement for edited source.
    if (staleSourceDigest && options.allowStaleSourceDigest !== true) {
      push(errors, "executable best metric.provenance.sourceDigest must match the catalog source.");
    }
    if (record.provenance.recipeDigest === undefined) push(errors, "executable best metric provenance must include recipeDigest.");
    if (record.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) checkPlatformEvidence(record.provenance.platformEvidence, "executable best metric.provenance.platformEvidence", record.platformTarget, errors);
  }
  if (Object.prototype.hasOwnProperty.call(record, "peLayout")) {
    if (record.platformTarget !== EXECUTABLE_PLATFORM_TARGET_WINDOWS) push(errors, "executable best metric.peLayout is only valid for the Windows PE lane.");
    if (record.metric !== "artifact-size") push(errors, "executable best metric.peLayout is only valid for the artifact-size cell.");
    if (record.provenance?.artifactCleanliness !== "verified-clean") push(errors, "executable best metric.peLayout requires verified-clean artifact provenance.");
    checkPeLayout(record.peLayout, "executable best metric.peLayout", errors, record.value);
  }
  if (Object.prototype.hasOwnProperty.call(record, "elfLayout")) {
    if (![EXECUTABLE_PLATFORM_TARGET_LINUX, EXECUTABLE_PLATFORM_TARGET_LINUX_WSL].includes(record.platformTarget)) push(errors, "executable best metric.elfLayout is only valid for a Linux ELF lane.");
    if (record.metric !== "artifact-size") push(errors, "executable best metric.elfLayout is only valid for the artifact-size cell.");
    if (record.provenance?.artifactCleanliness !== "verified-clean") push(errors, "executable best metric.elfLayout requires verified-clean artifact provenance.");
    checkElfLayout(record.elfLayout, "executable best metric.elfLayout", errors);
  }
  return errors;
}

export function validateExecutableBestMetrics(index, catalog = loadExecutableDocuments().catalog, options = {}) {
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
    for (const [number, record] of index.entries.entries()) errors.push(...validateExecutableBestMetric(record, catalog, options).map((error) => "entries[" + number + "]: " + error));
  }
  return errors;
}

export function pruneExecutableBestMetrics(catalog) {
  const entries = [...(catalog.bestMetrics?.entries ?? [])];
  const retained = [];
  const removedMetrics = [];
  for (const entry of entries) {
    const workload = workloadFor(catalog, entry?.workloadId);
    const source = sourceFor(workload, entry?.language, entry?.platformTarget);
    if (source && (!workloadAllowsBestMetrics(workload) ||
        entry?.provenance?.sourceDigest !== executableSourceDigest(source) ||
        JSON.stringify(entry?.runtimeClosure) !== JSON.stringify(source.runtimeClosure))) removedMetrics.push(entry.metric);
    else retained.push(entry);
  }
  const changed = retained.length !== entries.length;
  const nextCatalog = changed
    ? { ...catalog, bestMetrics: { ...catalog.bestMetrics, status: retained.length === 0 ? "empty" : BEST_METRICS_STATUS, entries: retained } }
    : catalog;
  return {
    catalog: nextCatalog,
    changed,
    removedCount: entries.length - retained.length,
    removedMetrics: [...new Set(removedMetrics)].sort(compareText),
  };
}

export function updateExecutableBestMetrics(catalog, result) {
  const errors = validateExecutableResult(result, catalog);
  if (errors.length > 0) throw new Error(errors.join("; "));
  const candidate = deriveExecutableBestMetrics(catalog, [result]);
  const pruned = pruneExecutableBestMetrics(catalog);
  const entries = [...(catalog.bestMetrics?.entries ?? [])];
  const candidateLane = candidate.entries[0];
  const liveLaneAxes = ["workloadId", "language", "equivalenceKey", "platformTarget",
    "artifactTarget", "abi", "profile", "host", "recipeClass", "runtimeClosure",
    "comparability", "eligibility"];
  const replacedBuildMetrics = [];
  const currentRunnerEntries = pruned.catalog.bestMetrics.entries.filter((entry) => {
    const sameLane = candidateLane !== undefined && liveLaneAxes.every((axis) => {
      const left = entry?.[axis];
      const right = candidateLane?.[axis];
      return isObject(left) || isObject(right)
        ? JSON.stringify(left) === JSON.stringify(right)
        : left === right;
    });
    const obsoleteBuild = sameLane && (
      entry?.toolchain !== candidateLane?.toolchain ||
      entry?.recipe !== candidateLane?.recipe ||
      entry?.provenance?.recipeDigest !== result.provenance.recipeDigest ||
      entry?.provenance?.toolchainDigest !== result.provenance.toolchainDigest ||
      entry?.provenance?.runnerDigest !== result.provenance.runnerDigest
    );
    if (obsoleteBuild) replacedBuildMetrics.push(entry.metric);
    return !obsoleteBuild;
  });
  const byCell = new Map(currentRunnerEntries.map((entry) => [`${entry.categoryId}\u0000${entry.metric}`, entry]));
  const updatedMetrics = [...pruned.removedMetrics, ...replacedBuildMetrics];
  for (const entry of candidate.entries) {
    const key = `${entry.categoryId}\u0000${entry.metric}`;
    const previous = byCell.get(key);
    const improves = !previous || BigInt(entry.value) < BigInt(previous.value);
    const fillsArtifactLayout = previous && entry.metric === "artifact-size" &&
      BigInt(entry.value) === BigInt(previous.value) &&
      !hasArtifactLayout(previous) && hasArtifactLayout(entry) && sameArtifactEvidence(previous, entry);
    if (improves || fillsArtifactLayout) {
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
