import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import {
  BENCHMARK_DISPOSITIONS,
  DESCRIPTOR_IDENTITIES,
  LANGUAGE_PROFILES,
  LIFECYCLE_SCENARIOS,
  LIFECYCLE_STAGES,
  REQUIRED_CASES,
  SOURCE_FIXTURE,
  expectedMatrixBlockers,
  expectedMatrixStatus,
  loadBmdDocuments,
  materializeCase,
  reduceCase,
  reduceCorpus,
  validateCorpus,
  validateManifest,
  validateProgram,
  validateResult,
  validateScenario,
} from "./benchmark-driven-development-machine.mjs";

const root = path.resolve(import.meta.dir, "..");
const documents = loadBmdDocuments();
const clone = (value) => JSON.parse(JSON.stringify(value));

test("BMD1 documents pass the host validators", () => {
  assert.deepEqual(validateProgram(documents.program, documents.corpus), []);
  assert.deepEqual(validateManifest(documents.manifest), []);
  assert.deepEqual(validateCorpus(documents.corpus), []);
});

test("W-1487 profiles and lanes remain separate from compiler lifecycle", () => {
  assert.deepEqual(documents.program.profiles.map((profile) => profile.id), LANGUAGE_PROFILES);
  assert.equal(documents.program.profiles.find((profile) => profile.id === "idiomatic").primary, true);
  assert.equal(documents.program.profiles.find((profile) => profile.id === "idiomatic").regression, true);
  const compiler = documents.corpus.cases.find((item) => item.id === "BMD1-W-1488-current-matrix");
  const materialized = materializeCase(compiler);
  assert.deepEqual(materialized.errors, []);
  assert.equal(Object.hasOwn(materialized.value, "profiles"), false);
  assert.equal(materialized.value.languageProfiles.applicability, "not-applicable");
  assert.equal(documents.manifest.backend.benchmarkRunnerAvailable, true);
  assert.equal(documents.manifest.backend.compilerLifecycleResultsAllowed, true);
  assert.equal(documents.manifest.backend.languageResultsAllowed, false);
  assert.equal(documents.manifest.backend.productRuntimeResultsAllowed, false);
  assert.equal(documents.manifest.backend.runtimeAvailable, false);
});

test("the accepted corpus covers all dispositions", () => {
  const accepted = documents.corpus.cases.filter((item) => item.kind === "accepted" && item.fixture !== "comparison-result");
  const byDisposition = new Map(accepted.map((item) => {
    const value = materializeCase(item).value;
    return [value.benchmarkDisposition, { item, value }];
  }));
  assert.deepEqual([...byDisposition.keys()].sort(), [...BENCHMARK_DISPOSITIONS].sort());
  assert.equal(byDisposition.get("required").item.track, "language");
  assert.equal(byDisposition.get("compiler-lifecycle").item.track, "compiler-lifecycle");
  assert.equal(byDisposition.get("deferred").item.track, "language");
  assert.equal(byDisposition.get("not-applicable").item.track, "documentation");
});

test("the seed matrix has three scenarios, nine stages and one ready cell", () => {
  const matrix = documents.manifest.matrix;
  assert.equal(documents.program.tasks.find((task) => task.id === "seed-compiler-lifecycle").implementation, "partial");
  assert.deepEqual(matrix.scenarios, LIFECYCLE_SCENARIOS);
  assert.deepEqual(matrix.stages, LIFECYCLE_STAGES);
  assert.equal(matrix.points.length, 27);
  assert.equal(matrix.points.filter((point) => point.status === "ready").length, 1);
  for (const point of matrix.points) {
    assert.deepEqual(Object.keys(point).sort(), ["blockedBy", "scenario", "stage", "status"]);
    assert.equal(point.status, expectedMatrixStatus(point.scenario, point.stage));
    assert.deepEqual(point.blockedBy, expectedMatrixBlockers(point.scenario, point.stage));
  }
  assert.deepEqual(matrix.points.find((point) => point.status === "ready"), {
    scenario: "clean",
    stage: "check-end-to-end",
    status: "ready",
    blockedBy: [],
  });
  assert.deepEqual(matrix.points.find((point) => point.scenario === "no-op" && point.stage === "semantic").blockedBy, [
    "incremental-cache", "stage-instrumentation",
  ]);
  assert.deepEqual(matrix.points.find((point) => point.scenario === "edit" && point.stage === "hir").blockedBy, [
    "incremental-cache", "hir",
  ]);
  assert.equal(Object.hasOwn(documents.manifest, "lifecycle"), false);
  assert.equal(Object.hasOwn(documents.manifest, "phases"), false);
});

test("seed compiler lifecycle task owns every W-1489 comparison case", () => {
  const task = documents.program.tasks.find((item) => item.id === "seed-compiler-lifecycle");
  const comparisonIds = REQUIRED_CASES.filter((id) => id.startsWith("BMD2-W-1489-"));
  assert.ok(task.outputs.includes("source-backed paired compiler comparator"));
  assert.equal(comparisonIds.every((id) => task.adversarialCases.includes(id)), true);
  assert.match(task.stopCondition, /comparison-only/u);
  assert.match(task.stopCondition, /regression remains blocked by managed-regression-runner/u);
});

test("root identities are source-backed once for the whole matrix", () => {
  assert.deepEqual(documents.manifest.identity.source, {
    path: SOURCE_FIXTURE.path,
    symbol: SOURCE_FIXTURE.symbol,
    digest: SOURCE_FIXTURE.digest,
  });
  assert.deepEqual(documents.manifest.identity.graph, DESCRIPTOR_IDENTITIES.graph);
  assert.deepEqual(documents.manifest.identity.input, DESCRIPTOR_IDENTITIES.input);
  for (const axis of ["graph", "input"]) {
    const descriptor = JSON.parse(fs.readFileSync(path.resolve(root, DESCRIPTOR_IDENTITIES[axis].path), "utf8"));
    assert.equal(descriptor.source.path, SOURCE_FIXTURE.path);
    assert.equal(descriptor.source.symbol, SOURCE_FIXTURE.symbol);
    assert.equal(descriptor.source.digest, SOURCE_FIXTURE.digest);
  }
});

test("WBench/1 result shape is closed and binds the ready matrix point", () => {
  const result = documents.schema.$defs.result;
  assert.deepEqual(result.required, [
    "$schema", "schema", "kind", "id", "status", "quality", "claim",
    "workload", "identity", "comparison", "oracle", "samples", "environment",
    "provenance", "metrics", "summary", "semanticDeviations", "disclosures",
  ]);
  assert.deepEqual(result.properties.workload.required, [
    "manifestDigest", "track", "lane", "scenario", "stage", "subject", "profile",
  ]);
  assert.equal(result.properties.workload.properties.track.const, "compiler-lifecycle");
  assert.equal(result.properties.workload.properties.profile.const, null);
  assert.equal(result.properties.quality.const, "exploratory");
  assert.equal(result.properties.claim.const, "measurement-only");
  assert.equal(result.properties.comparison.const, null);
  assert.deepEqual(result.properties.samples.required, ["raw", "warmup", "stopRule", "clock", "order"]);
  assert.equal(result.properties.samples.properties.clock.const, "monotonic-wall-ns");
  assert.equal(result.properties.samples.properties.order.const, "single-series");
  assert.deepEqual(result.properties.provenance.required, [
    "sourceDigest", "artifactDigest", "inputDigest", "recipeDigest", "runnerDigest", "toolchainDigest",
  ]);
  assert.deepEqual(result.properties.metrics.required, ["latency"]);
  assert.deepEqual(result.properties.summary.required, ["sampleCount", "warmupCount", "derivedFromRawSamples"]);
});

test("machine materializes and recalculates a complete exploratory result", () => {
  const result = materializeCase({ fixture: "result", mutations: [] }).value;
  assert.deepEqual(validateResult(result, documents.manifest), []);
  assert.deepEqual(result.samples.raw.map((sample) => typeof sample.ns), Array(9).fill("string"));
  assert.deepEqual(result.metrics.latency, {
    unit: "ns",
    minimumNs: "100",
    medianNs: "104",
    maximumNs: "108",
    madNs: "2",
    derivedFromRawSamples: true,
  });
  assert.deepEqual(result.summary, { sampleCount: 9, warmupCount: 1, derivedFromRawSamples: true });
  const forged = clone(result);
  forged.metrics.latency.medianNs = "999";
  assert.ok(validateResult(forged, documents.manifest).some((error) => error.includes("does not match")));
  const comparison = clone(result);
  comparison.comparison = { baseline: {}, candidate: {}, noisePolicy: {} };
  assert.ok(validateResult(comparison, documents.manifest).some((error) => error.includes("interleaved-comparison-runner")));
  const invalidLists = clone(result);
  invalidLists.environment.flags = ["Release", "Release"];
  invalidLists.environment.noiseControls.known = ["scheduler"];
  invalidLists.environment.noiseControls.unknown = ["scheduler"];
  invalidLists.semanticDeviations = [""];
  invalidLists.disclosures = ["host", "host"];
  const listErrors = validateResult(invalidLists, documents.manifest);
  assert.ok(listErrors.some((error) => error.includes("must not contain duplicates")));
  assert.ok(listErrors.some((error) => error.includes("must not overlap")));
  assert.ok(listErrors.some((error) => error.includes("non-empty string")));
});

test("corpus uses bounded fixture mutations and rejects unknown mutations", () => {
  const source = fs.readFileSync(path.join(root, "tooling", "benchmark-driven-development-cases.json"), "utf8");
  assert.ok(source.split(/\r?\n/u).length < 500);
  for (const item of documents.corpus.cases) {
    assert.equal(Object.hasOwn(item, "input"), false);
    assert.equal(Object.hasOwn(item, "result"), false);
    assert.equal(Object.hasOwn(item, "expected"), false);
    assert.equal(Object.hasOwn(item, "expectedResult"), false);
    assert.equal(Array.isArray(item.mutations), true);
  }
  assert.deepEqual(materializeCase({ fixture: "compiler-lifecycle", mutations: ["unknown"] }).errors.length > 0, true);
});

test("the corpus has independent negative coverage", () => {
  assert.equal(REQUIRED_CASES.every((id) => documents.corpus.cases.some((item) => item.id === id)), true);
  const reductions = reduceCorpus(documents.corpus);
  assert.equal(reductions.length, documents.corpus.cases.length);
  assert.equal(reductions.filter((item) => item.classification === "accepted").length, 7);
  assert.equal(reductions.filter((item) => item.classification === "rejected").length, documents.corpus.cases.length - 7);
  for (const id of REQUIRED_CASES.filter((value) => value.includes("result-") || value.includes("blocker-incomplete"))) {
    assert.equal(reduceCase(documents.corpus.cases.find((item) => item.id === id)).classification, "rejected", id);
  }
});

test("BMD2 comparison fixture is closed, paired and calibration-aware", () => {
  const current = documents.corpus.cases.find((item) => item.id === "BMD2-W-1489-current-comparison");
  const result = materializeCase(current).value;
  assert.deepEqual(validateResult(result, documents.manifest), []);
  assert.equal(result.claim, "comparison-only");
  assert.equal(result.verdict, "not-evaluated");
  assert.equal(result.samples.raw.length, 18);
  assert.equal(result.samples.order, "balanced-paired-interleaved-sha256-v1");
  assert.equal(result.comparison.calibration, true);
  assert.equal(result.comparison.pairs.length, 9);
  const different = materializeCase(documents.corpus.cases.find((item) => item.id === "BMD2-W-1489-different-closure-comparison")).value;
  assert.deepEqual(validateResult(different, documents.manifest), []);
  assert.equal(different.comparison.calibration, false);
});

test("paired comparison samples reject round zero", () => {
  const current = documents.corpus.cases.find((item) => item.id === "BMD2-W-1489-current-comparison");
  const result = clone(materializeCase(current).value);
  result.samples.raw[0].round = 0;
  assert.equal(documents.schema.$defs.comparisonSample.properties.round.minimum, 1);
  assert.ok(validateResult(result, documents.manifest).some((error) => error.includes("positive rounds")));
});

test("required adversarial cases reject the matrix and result boundaries", () => {
  const ids = [
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
  ];
  for (const id of ids) {
    assert.equal(reduceCase(documents.corpus.cases.find((item) => item.id === id)).classification, "rejected", id);
  }
  const compiler = materializeCase({ fixture: "compiler-lifecycle", mutations: [] }).value;
  assert.ok(validateScenario({ ...compiler, phases: ["clean"], track: "compiler-lifecycle", lane: "equivalent" }).some((error) => error.includes("flatten")));
});
