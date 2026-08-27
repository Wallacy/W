import fs from "node:fs";
import path from "node:path";
import {
  DESCRIPTOR_IDENTITIES,
  LANGUAGE_PROFILES,
  LIFECYCLE_SCENARIOS,
  LIFECYCLE_STAGES,
  MAX_BOUNDED_MUTATIONS,
  REQUIRED_CASES,
  SCHEMA_VERSION,
  SOURCE_FIXTURE,
  loadBmdDocuments,
  materializeCase,
  reduceCorpus,
  validateCorpus,
  validateManifest,
  validateProgram,
} from "./benchmark-driven-development-machine.mjs";

const root = path.resolve(import.meta.dir, "..");
const documents = loadBmdDocuments();
const errors = [];

function fail(message) {
  errors.push("BMD1: " + message);
}
function hasAll(values, expected) {
  return expected.every((value) => values.includes(value));
}

function checkErrors(label, values) {
  for (const value of values) fail(label + ": " + value);
}

checkErrors("program", validateProgram(documents.program, documents.corpus));
checkErrors("manifest", validateManifest(documents.manifest));
checkErrors("corpus", validateCorpus(documents.corpus));

const preciseBackendFlags = [
  "compilerLifecycleResultsAllowed",
  "languageResultsAllowed",
  "productRuntimeResultsAllowed",
];
for (const [name, backend] of [["program", documents.program.backend], ["manifest", documents.manifest.backend]]) {
  if (backend?.benchmarkRunnerAvailable !== true ||
      backend?.compilerLifecycleResultsAllowed !== true ||
      backend?.comparisonResultsAllowed !== true ||
      backend?.regressionResultsAllowed !== false ||
      preciseBackendFlags.slice(1).some((flag) => backend?.[flag] !== false)) {
    fail(name + " backend must enable compiler-lifecycle comparison results and disable regression, language and runtime results.");
  }
  if (Object.prototype.hasOwnProperty.call(backend ?? {}, "resultsAllowed")) {
    fail(name + " backend must not retain resultsAllowed.");
  }
}

if (documents.schema?.$id !== SCHEMA_VERSION) fail("schema id must be wbench/1.");
if (!documents.schema?.oneOf?.some((entry) => entry.$ref === "#/$defs/result")) {
  fail("WBench/1 root must expose kind result.");
}

const resultDefinition = documents.schema?.$defs?.result;
if (!resultDefinition) {
  fail("WBench/1 must define a result kind.");
} else {
  const required = resultDefinition.required ?? [];
  const fields = [
    "$schema", "schema", "kind", "id", "status", "quality", "claim",
    "workload", "identity", "comparison", "oracle", "samples", "environment",
    "provenance", "metrics", "summary", "semanticDeviations", "disclosures",
  ];
  if (!hasAll(required, fields)) fail("result schema must require the complete result record.");
  if (resultDefinition.properties?.kind?.const !== "result") fail("result schema kind must be result.");
  if (resultDefinition.properties?.quality?.const !== "exploratory" ||
      resultDefinition.properties?.claim?.const !== "measurement-only" ||
      resultDefinition.properties?.comparison?.const !== null) {
    fail("BMD1 result schema must remain measurement-only; BMD2 comparison uses its separate closed result kind.");
  }
  const workload = resultDefinition.properties?.workload;
  if (JSON.stringify(workload?.required ?? []) !== JSON.stringify([
    "manifestDigest", "track", "lane", "scenario", "stage", "subject", "profile",
  ])) fail("result workload must identify manifest, track, lane, scenario, stage, subject and profile.");
  if (workload?.properties?.track?.const !== "compiler-lifecycle") {
    fail("result schema must prohibit language and runtime result tracks.");
  }
  if (workload?.properties?.profile?.const !== null) {
    fail("compiler lifecycle result profile must be null.");
  }
  const identity = resultDefinition.properties?.identity;
  if (!identity || identity.$ref !== "#/$defs/resultIdentity") {
    fail("result identity must bind source, graph, input and manifest command.");
  }
  if (resultDefinition.properties?.samples?.properties?.order?.const !== "single-series") {
    fail("BMD1 result schema must use single-series ordering; BMD2 declares its paired order separately.");
  }
  const oracleRequired = resultDefinition.properties?.oracle?.required ?? [];
  if (!hasAll(oracleRequired, ["validationDigest", "complete", "beforeSamples"])) {
    fail("result oracle must bind validation digest before samples.");
  }
  const sampleRequired = resultDefinition.properties?.samples?.required ?? [];
  if (!hasAll(sampleRequired, ["raw", "warmup", "stopRule", "clock", "order"])) {
    fail("result samples must include raw, warmup, fixed-count stop rule, clock and order.");
  }
  const environmentRequired = resultDefinition.properties?.environment?.required ?? [];
  if (!hasAll(environmentRequired, ["hardware", "kernel", "target", "provider", "toolchain", "flags", "noiseControls"])) {
    fail("result environment must contain hardware, kernel, target, provider, toolchain, flags and noise controls.");
  }
  const provenanceRequired = resultDefinition.properties?.provenance?.required ?? [];
  if (!hasAll(provenanceRequired, ["sourceDigest", "artifactDigest", "inputDigest", "recipeDigest", "runnerDigest", "toolchainDigest"])) {
    fail("result provenance must bind source, artifact, input, recipe, runner and toolchain digests.");
  }
  if (resultDefinition.properties?.summary?.properties?.derivedFromRawSamples?.const !== true) {
    fail("result summary must declare derivation from raw samples.");
  }
  const metricDefinition = resultDefinition.properties?.metrics;
  if (metricDefinition?.additionalProperties !== false ||
      JSON.stringify(metricDefinition?.required ?? []) !== JSON.stringify(["latency"])) {
    fail("result metrics must be a closed latency object.");
  }
  const latencyDefinition = metricDefinition?.properties?.latency;
  if (latencyDefinition?.additionalProperties !== false ||
      !hasAll(latencyDefinition?.required ?? [], ["unit", "minimumNs", "medianNs", "maximumNs", "madNs", "derivedFromRawSamples"])) {
    fail("latency metric must use the closed derived shape.");
  }
  const summaryDefinition = resultDefinition.properties?.summary;
  if (summaryDefinition?.additionalProperties !== false ||
      !hasAll(summaryDefinition?.required ?? [], ["sampleCount", "warmupCount", "derivedFromRawSamples"])) {
    fail("result summary must use the closed derived shape.");
  }
  const sampleDefinition = resultDefinition.properties?.samples?.properties?.raw?.items;
  if (sampleDefinition?.$ref !== "#/$defs/resultSample") {
    fail("result samples must use the closed resultSample definition.");
  }
  const u64Definition = documents.schema?.$defs?.u64;
  if (typeof u64Definition?.pattern !== "string" ||
      !new RegExp(u64Definition.pattern).test("18446744073709551615") ||
      new RegExp(u64Definition.pattern).test("18446744073709551616") ||
      new RegExp(u64Definition.pattern).test("01")) {
    fail("u64 schema must reject non-canonical and overflowing values.");
  }
  for (const field of ["flags", "semanticDeviations", "disclosures"]) {
    const definition = resultDefinition.properties?.environment?.properties?.[field] ??
      resultDefinition.properties?.[field];
    if (definition?.uniqueItems !== true || definition?.items?.minLength !== 1) {
      fail("result " + field + " must be a unique set of non-empty strings.");
    }
  }
  const noise = resultDefinition.properties?.environment?.properties?.noiseControls?.properties;
  for (const field of ["known", "unknown"]) {
    if (noise?.[field]?.uniqueItems !== true || noise?.[field]?.items?.minLength !== 1) {
      fail("result noiseControls." + field + " must be a unique set of non-empty strings.");
    }
  }
}

const comparisonDefinition = documents.schema?.$defs?.comparisonResult;
if (!comparisonDefinition || !documents.schema?.oneOf?.some((entry) => entry.$ref === "#/$defs/comparisonResult")) {
  fail("WBench/1 root must expose the comparison result kind.");
} else {
  if (comparisonDefinition.additionalProperties !== false ||
      comparisonDefinition.properties?.claim?.const !== "comparison-only" ||
      comparisonDefinition.properties?.verdict?.const !== "not-evaluated") {
    fail("comparison result schema must be closed and comparison-only/not-evaluated.");
  }
  if (comparisonDefinition.properties?.comparison?.$ref !== "#/$defs/comparison") {
    fail("comparison result schema must bind the closed comparison object.");
  }
  if (comparisonDefinition.properties?.samples?.$ref !== "#/$defs/comparisonSamples") {
    fail("comparison result schema must bind paired samples.");
  }
}

const taskDefinition = documents.schema?.$defs?.task;
const seedTaskRule = taskDefinition?.allOf?.find((rule) =>
  rule?.if?.properties?.id?.const === "seed-compiler-lifecycle");
if (seedTaskRule?.then?.properties?.outputs?.contains?.const !== "source-backed paired compiler comparator" ||
    JSON.stringify(seedTaskRule?.then?.properties?.stopCondition?.allOf?.map((rule) => rule?.pattern).sort()) !==
      JSON.stringify(["comparison-only", "managed-regression-runner"].sort())) {
  fail("task schema must bind the seed compiler comparator and separate comparison-only from managed regression.");
}

const matrix = documents.manifest.matrix;
if (JSON.stringify(matrix?.scenarios) !== JSON.stringify(LIFECYCLE_SCENARIOS)) {
  fail("manifest matrix scenarios are not clean, no-op and edit.");
}
if (JSON.stringify(matrix?.stages) !== JSON.stringify(LIFECYCLE_STAGES)) {
  fail("manifest matrix stages are not the nine compiler stages.");
}
if (matrix?.points?.length !== 27) fail("manifest matrix must contain 27 points.");
if (matrix?.points?.filter((point) => point.status === "ready").length !== 1) {
  fail("manifest matrix must contain exactly one ready point.");
}
for (const point of matrix?.points ?? []) {
  if (JSON.stringify(Object.keys(point).sort()) !== JSON.stringify(["blockedBy", "scenario", "stage", "status"])) {
    fail("matrix points must contain no repeated root identities.");
  }
}
if (documents.manifest.identity?.source?.path !== SOURCE_FIXTURE.path ||
    documents.manifest.identity?.source?.digest !== SOURCE_FIXTURE.digest) {
  fail("seed manifest source must be the current checker_bootstrap fixture.");
}
for (const [axis, identity] of Object.entries(DESCRIPTOR_IDENTITIES)) {
  const actual = documents.manifest.identity?.[axis];
  if (JSON.stringify(actual) !== JSON.stringify(identity)) {
    fail("seed manifest " + axis + " descriptor identity is not source-backed.");
  }
}

const cases = documents.corpus.cases ?? [];
const reductions = reduceCorpus(documents.corpus);
const accepted = cases.filter((item) => item.kind === "accepted");
const rejected = cases.filter((item) => item.kind === "rejected");
if (accepted.length !== 7) fail("expected five BMD1 accepted cases and two BMD2 comparison cases.");
if (rejected.length < 1) fail("corpus must contain rejected adversarial cases.");
if (!hasAll(cases.map((item) => item.id), REQUIRED_CASES)) {
  fail("corpus is missing a required adversarial or disposition case.");
}
if (fs.readFileSync(path.join(root, "tooling", "benchmark-driven-development-cases.json"), "utf8").split(/\r?\n/u).length >= 500) {
  fail("corpus must use canonical fixtures and bounded mutations.");
}
for (const item of cases) {
  if (!Array.isArray(item.mutations) || item.mutations.length > MAX_BOUNDED_MUTATIONS || !materializeCase(item).value) {
    fail(item.id + " must use bounded canonical fixture mutations.");
  }
  for (const key of ["input", "result", "expected", "expectedResult"]) {
    if (Object.prototype.hasOwnProperty.call(item, key)) fail(item.id + " must not echo " + key + ".");
  }
}
for (const reduction of reductions) {
  const source = cases.find((item) => item.id === reduction.id);
  if (source?.kind === "accepted" && reduction.classification !== "accepted") {
    fail(reduction.id + " must be accepted by the host reducer.");
  }
  if (source?.kind === "rejected" && reduction.classification !== "rejected") {
    fail(reduction.id + " must be rejected by the host reducer.");
  }
}

const resultFixtures = fs.readdirSync(path.join(root, "benchmarks"), { withFileTypes: true })
  .filter((entry) => entry.isFile() && /(?:result|timing)/iu.test(entry.name) &&
    entry.name !== "wbench-1.schema.json");
if (resultFixtures.length > 0) {
  fail("benchmarks must not track result or timing fixtures: " +
    resultFixtures.map((entry) => entry.name).join(", ") + ".");
}
if (fs.existsSync(path.join(root, "benchmarks", "results"))) {
  fail("benchmarks/results must not exist in the source tree.");
}

if (errors.length > 0) {
  for (const error of errors) console.error(error);
  process.exitCode = 1;
} else {
  console.log("BMD1/BMD2 benchmark protocol: " + LANGUAGE_PROFILES.length +
    " language profiles, 2 lanes, " + LIFECYCLE_SCENARIOS.length +
    " scenarios, " + LIFECYCLE_STAGES.length + " stages, " +
    matrix.points.length + " matrix points, " + cases.length +
    " cases (" + accepted.length + " accepted, " + rejected.length +
    " rejected).");
}
