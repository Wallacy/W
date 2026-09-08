import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import os from "node:os";
import path from "node:path";
import {
  EXECUTABLE_COMPARABILITY_AXES,
  EXECUTABLE_ARTIFACT_TARGET_MINGW,
  EXECUTABLE_ARTIFACT_TARGET_MSVC,
  EXECUTABLE_BEST_SCHEMA,
  EXECUTABLE_LANGUAGES,
  EXECUTABLE_METRICS,
  EXECUTABLE_PLATFORM_TARGET,
  EXECUTABLE_RESULT_SCHEMA,
  deriveExecutableBestKnown,
  executableEquivalenceKey,
  executableHostIdentity,
  exactOutputDigest,
  loadExecutableDocuments,
  loadExecutableHistoryResults,
  validateExecutableBestKnown,
  validateExecutableBestKnownFreshness,
  validateExecutableBestKnownIndex,
  validateExecutableCatalog,
  validateExecutableHistory,
  validateExecutableResult,
} from "./executable-benchmark-machine.mjs";

const documents = loadExecutableDocuments();
const historyResults = loadExecutableHistoryResults(documents.history).map(({ record }) => record);
const clone = (value) => structuredClone(value);
const digest = "sha256:1111111111111111111111111111111111111111111111111111111111111111";

test("executable catalog is source-backed and keeps planned work separate", () => {
  assert.deepEqual(validateExecutableCatalog(documents.catalog, documents), []);
  assert.equal(documents.schema.$id, "w-executable-benchmark/3");
  assert.deepEqual(documents.schema.oneOf.map((entry) => entry.$ref), [
    "#/$defs/catalog", "#/$defs/result", "#/$defs/bestKnown", "#/$defs/bestKnownIndex", "#/$defs/historyIndex",
  ]);
  for (const definition of ["catalog", "result", "bestKnown", "bestKnownIndex", "historyIndex", "historyReference", "sample", "sampleSeries"]) {
    assert.equal(documents.schema.$defs[definition].additionalProperties, false);
  }
  assert.deepEqual(documents.schema.$defs.sampleSeries.properties.raw.minItems, 9);
  assert.deepEqual(documents.schema.$defs.sample.required, [
    "wallNs", "cpuUserUs", "cpuSystemUs", "cpuTotalUs", "peakRssBytes",
  ]);
  assert.equal(documents.schema.$defs.sample.properties.wallNs.$ref, "#/$defs/positiveDecimal");
  assert.equal(documents.schema.$defs.sample.properties.peakRssBytes.$ref, "#/$defs/positiveDecimal");
  assert.equal(documents.schema.$defs.artifact.properties.sizeBytes.$ref, "#/$defs/positiveDecimal");
  assert.equal(documents.schema.$defs.protocol.properties.arithmeticMeanRounding.const, "floor-integer");
  assert.deepEqual(documents.catalog.metrics.map((item) => item.id), EXECUTABLE_METRICS.map((item) => item.id));
  assert.deepEqual(documents.catalog.comparabilityAxes, EXECUTABLE_COMPARABILITY_AXES);
  const hello = documents.catalog.workloads.find((item) => item.id === "hello");
  assert.equal(hello.status, "source-oracle-ready");
  assert.equal(hello.sourceReadiness, "source-and-oracle-ready");
  assert.equal(hello.benchmarkStatus, "not-performance-ready");
  assert.equal(hello.sources.find((item) => item.language === "w").recipe, "private-native0-mlir0-source-to-pe-candidate");
  assert.equal(hello.sources.find((item) => item.language === "w").comparability, "contextual-non-ranking-until-public-run");
  assert.equal(hello.sources.find((item) => item.language === "w").eligibility, "contextual-only-until-public-run");
  assert.deepEqual(hello.sources.map((item) => item.language), EXECUTABLE_LANGUAGES);
  assert.equal(hello.sources.find((item) => item.language === "c").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MINGW);
  assert.equal(hello.sources.find((item) => item.language === "c").comparability, "contextual-non-ranking-across-abi");
  assert.equal(hello.sources.find((item) => item.language === "rust").artifactTarget, EXECUTABLE_ARTIFACT_TARGET_MSVC);
  assert.equal(hello.oracle.stdout, "Hello, world!\n");
  for (const id of ["restaurant-branch", "restaurant-nested-branch", "bool-short-circuit", "restaurant-interpolation"]) {
    const workload = documents.catalog.workloads.find((item) => item.id === id);
    assert.equal(workload.status, "source-oracle-ready");
    assert.deepEqual(workload.blockedLanguages, ["c", "rust"]);
    assert.equal(workload.sources[0].language, "w");
  }
  assert.equal(documents.catalog.workloads.find((item) => item.id === "restaurant-composition").status, "planned");
  assert.equal(documents.catalog.bestKnownContract.status, "defined");
  assert.equal(documents.catalog.status, "catalog-ready");
  assert.deepEqual(validateExecutableBestKnownIndex(documents.bestKnown, documents.catalog, historyResults), []);
  assert.deepEqual(validateExecutableHistory(documents.history, documents.catalog), []);
});

test("catalog rejects stale, escaped, duplicate and partition-drifting records", () => {
  const stale = clone(documents.catalog);
  stale.workloads[0].sources[0].digest = digest;
  assert.match(validateExecutableCatalog(stale, documents).join("\n"), /digest is stale/);

  const escaped = clone(documents.catalog);
  escaped.workloads[0].sources[1].path = "../outside.c";
  assert.match(validateExecutableCatalog(escaped, documents).join("\n"), /escapes/);

  const duplicate = clone(documents.catalog);
  duplicate.workloads[1].blockedLanguages.push("rust");
  assert.match(validateExecutableCatalog(duplicate, documents).join("\n"), /duplicates/);

  const forgedPartition = clone(documents.catalog);
  forgedPartition.workloads[1].blockedLanguages = ["c"];
  assert.match(validateExecutableCatalog(forgedPartition, documents).join("\n"), /must account for language rust/);

  const drifted = clone(documents.catalog);
  drifted.metrics[0].id = "timing-percent";
  assert.match(validateExecutableCatalog(drifted, documents).join("\n"), /metric/);
});

test("history is content-addressed and rejects unindexed entries", () => {
  const invalidName = clone(documents.history);
  invalidName.records = [{ id: "forged", path: "forged.json", digest }];
  invalidName.status = "recorded";
  assert.match(validateExecutableHistory(invalidName, documents.catalog).join("\n"), /lowercase sha256 hex digest filename/);
  const mismatchedDigest = clone(documents.history);
  mismatchedDigest.records = [{ id: "forged", path: "0".repeat(64) + ".json", digest }];
  mismatchedDigest.status = "recorded";
  assert.match(validateExecutableHistory(mismatchedDigest, documents.catalog).join("\n"), /match its sha256 digest filename/);

  const temporaryRoot = fs.mkdtempSync(path.join(os.tmpdir(), "w-executable-history-test-"));
  const historyRoot = path.join(temporaryRoot, "benchmarks", "history", "executables");
  fs.mkdirSync(historyRoot, { recursive: true });
  fs.writeFileSync(path.join(historyRoot, "index.json"), JSON.stringify(documents.history, null, 2) + "\n");
  fs.writeFileSync(path.join(historyRoot, "README.md"), "history\n");
  fs.writeFileSync(path.join(historyRoot, "unexpected.json"), "{}\n");
  try {
    assert.match(validateExecutableHistory(documents.history, documents.catalog, temporaryRoot).join("\n"), /unindexed entry: unexpected\.json/);
  } finally {
    fs.rmSync(temporaryRoot, { recursive: true, force: true });
  }
});

function sample(index) {
  return {
    wallNs: String(index + 1),
    cpuUserUs: "1",
    cpuSystemUs: "0",
    cpuTotalUs: "1",
    peakRssBytes: String(100 + index),
  };
}

function sampleSeries() {
  const raw = Array.from({ length: 9 }, (_, index) => sample(index));
  const stats = (min, median, max, arithmeticMean, mad) => ({
    min: String(min), median: String(median), max: String(max), arithmeticMean: String(arithmeticMean), mad: String(mad),
  });
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
  const hello = documents.catalog.workloads.find((item) => item.id === "hello");
  const source = hello.sources.find((item) => item.language === language);
  const recipeDigest = digest;
  const artifactTarget = language === "c" ? EXECUTABLE_ARTIFACT_TARGET_MINGW : EXECUTABLE_ARTIFACT_TARGET_MSVC;
  const environment = {
    os: "windows-11",
    kernel: "windows-class",
    cpuModel: "x86_64-class",
    logicalCores: "16",
    ramBytes: "34359738368",
  };
  return {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_RESULT_SCHEMA,
    kind: "executable-result",
    id: "hello-" + language + "-example",
    status: "recorded",
    workloadId: "hello",
    language,
    platformTarget: EXECUTABLE_PLATFORM_TARGET,
    artifactTarget,
    profile: "release",
    quality: "exploratory",
    claim: "measurement-only",
    verdict: "not-evaluated",
    equivalenceKey: executableEquivalenceKey(documents.catalog, "hello", EXECUTABLE_PLATFORM_TARGET, "release", source.recipeClass),
    identity: {
      sourceDigest: source.digest,
      platformTarget: EXECUTABLE_PLATFORM_TARGET,
      artifactTarget,
      profile: "release",
      toolchain: language === "rust" ? "rustc-1.94" : "gcc-13.2",
      host: executableHostIdentity(environment),
      recipe: source.recipe,
      recipeClass: source.recipeClass,
      recipeDigest,
      eligibility: source.eligibility,
    },
    correctness: {
      oracleId: "hello:exact-output",
      exitCode: 0,
      stdoutDigest: exactOutputDigest("Hello, world!\n"),
      stderrDigest: exactOutputDigest(""),
    },
    artifact: {
      digest,
      sizeBytes: "123",
    },
    protocol: {
      warmupMinimum: 1,
      rawMinimum: 9,
      rawParity: "odd",
      arithmeticMeanRounding: "floor-integer",
      stopRule: "fixed-count",
      wallClock: "monotonic-nanoseconds",
      processIsolation: "fresh-process-per-sample",
      order: "deterministic-interleaved",
      resourceScope: "direct child process only; descendants are not aggregated",
      knownNoiseControls: ["warmup-discarded", "fresh-process-per-sample"],
      unknownNoiseControls: ["host-scheduler", "filesystem-cache"],
      directProcessDisclosure: "Bun direct-process counters cover the spawned process only; process-tree CPU/RSS are not aggregated.",
    },
    environment,
    compile: sampleSeries(),
    run: sampleSeries(),
    provenance: {
      sourceDigest: source.digest,
      artifactDigest: digest,
      recipeDigest,
      toolchainDigest: digest,
      runnerDigest: digest,
      catalogDigest: digest,
      commit: "1111111111111111111111111111111111111111",
      observedAt: "2026-09-08T00:00:00.000Z",
    },
  };
}

function validBest(result, metric = "run-wall-time") {
  return {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_BEST_SCHEMA,
    kind: "executable-best-known",
    id: result.workloadId + "-" + result.language + "-" + metric,
    status: "derived",
    workloadId: result.workloadId,
    metric,
    language: result.language,
    platformTarget: result.platformTarget,
    artifactTarget: result.artifactTarget,
    profile: result.profile,
    statistic: metric === "artifact-size" ? "single-artifact" : "median",
    equivalenceKey: result.equivalenceKey,
    toolchain: result.identity.toolchain,
    host: result.identity.host,
    recipe: result.identity.recipe,
    recipeClass: result.identity.recipeClass,
    recipeDigest: result.identity.recipeDigest,
    value: metric === "artifact-size" ? result.artifact.sizeBytes : "5",
    derivedFrom: [result.id],
  };
}

function zeroRunCpu(result) {
  for (const sample of [...result.run.warmup, ...result.run.raw]) {
    sample.cpuUserUs = "0";
    sample.cpuSystemUs = "0";
    sample.cpuTotalUs = "0";
  }
  for (const field of ["cpuUserUs", "cpuSystemUs", "cpuTotalUs"]) {
    result.run.summary[field] = { min: "0", median: "0", max: "0", arithmeticMean: "0", mad: "0" };
  }
  return result;
}

test("result preserves raw samples and derives every summary", () => {
  const result = validResult();
  assert.deepEqual(validateExecutableResult(result, documents.catalog), []);

  const tooShort = clone(result);
  tooShort.run.raw = tooShort.run.raw.slice(0, 8);
  assert.match(validateExecutableResult(tooShort, documents.catalog).join("\n"), /odd count of at least nine/);

  const even = clone(result);
  even.compile.raw.push(sample(10));
  assert.match(validateExecutableResult(even, documents.catalog).join("\n"), /odd count of at least nine/);

  const badSummary = clone(result);
  badSummary.run.summary.wallNs.arithmeticMean = "999";
  assert.match(validateExecutableResult(badSummary, documents.catalog).join("\n"), /derive min\/median\/max/);

  const badWall = clone(result);
  badWall.run.raw[0].wallNs = "0";
  assert.match(validateExecutableResult(badWall, documents.catalog).join("\n"), /wallNs must be positive/);

  const badPeakRss = clone(result);
  badPeakRss.run.raw[0].peakRssBytes = "0";
  assert.match(validateExecutableResult(badPeakRss, documents.catalog).join("\n"), /peakRssBytes must be positive/);

  const badArtifactSize = clone(result);
  badArtifactSize.artifact.sizeBytes = "0";
  assert.match(validateExecutableResult(badArtifactSize, documents.catalog).join("\n"), /sizeBytes must be positive/);

  const badMeanRounding = clone(result);
  badMeanRounding.protocol.arithmeticMeanRounding = "nearest-integer";
  assert.match(validateExecutableResult(badMeanRounding, documents.catalog).join("\n"), /arithmeticMeanRounding must be floor-integer/);

  const badCpu = clone(result);
  badCpu.run.cpuResolution.disclosure = "nanoseconds only";
  assert.match(validateExecutableResult(badCpu, documents.catalog).join("\n"), /zero-valued microsecond/);

  const badTimestamp = clone(result);
  badTimestamp.provenance.observedAt = "2026-09-08T00:00:00Z";
  assert.match(validateExecutableResult(badTimestamp, documents.catalog).join("\n"), /canonical ISO-8601 UTC/);

  const wrongTarget = clone(result);
  wrongTarget.artifactTarget = EXECUTABLE_PLATFORM_TARGET;
  assert.match(validateExecutableResult(wrongTarget, documents.catalog).join("\n"), /artifact target/);

  const wrongPlatform = clone(result);
  wrongPlatform.platformTarget = "linux-x64";
  assert.match(validateExecutableResult(wrongPlatform, documents.catalog).join("\n"), /platformTarget/);

  const falseCAbi = validResult("c");
  falseCAbi.artifactTarget = EXECUTABLE_ARTIFACT_TARGET_MSVC;
  falseCAbi.identity.artifactTarget = EXECUTABLE_ARTIFACT_TARGET_MSVC;
  assert.match(validateExecutableResult(falseCAbi, documents.catalog).join("\n"), /C executable results.*mingw32/);

  const wrongProfile = clone(result);
  wrongProfile.profile = "size-experimental";
  assert.match(validateExecutableResult(wrongProfile, documents.catalog).join("\n"), /profile must be release/);

  for (const [field, value, message] of [
    ["quality", "correctness-gate", /quality must be exploratory/],
    ["claim", "performance", /claim must be measurement-only/],
    ["verdict", "pass", /verdict must be not-evaluated/],
  ]) {
    const invalid = clone(result);
    invalid[field] = value;
    assert.match(validateExecutableResult(invalid, documents.catalog).join("\n"), message);
  }

  const missingProtocol = clone(result);
  missingProtocol.protocol = null;
  assert.match(validateExecutableResult(missingProtocol, documents.catalog).join("\n"), /protocol must be an object/);

  const badNoise = clone(result);
  badNoise.protocol.unknownNoiseControls = [];
  assert.match(validateExecutableResult(badNoise, documents.catalog).join("\n"), /unknownNoiseControls/);

  const badScope = clone(result);
  badScope.protocol.resourceScope = "";
  assert.match(validateExecutableResult(badScope, documents.catalog).join("\n"), /resourceScope/);

  const badDisclosure = clone(result);
  badDisclosure.protocol.directProcessDisclosure = "CPU is measured precisely.";
  assert.match(validateExecutableResult(badDisclosure, documents.catalog).join("\n"), /directProcessDisclosure/);

  const badEnvironment = clone(result);
  badEnvironment.environment.cpuModel = "C:\\Users\\Wallacy";
  assert.match(validateExecutableResult(badEnvironment, documents.catalog).join("\n"), /redacted environment class/);

  const readableCpu = clone(result);
  readableCpu.environment.cpuModel = "Intel(R) Core(TM) i7-12700K @ 3.60GHz";
  readableCpu.identity.host = executableHostIdentity(readableCpu.environment);
  assert.deepEqual(validateExecutableResult(readableCpu, documents.catalog), []);

  const forgedEnvironment = clone(result);
  forgedEnvironment.environment.logicalCores = "32";
  assert.match(validateExecutableResult(forgedEnvironment, documents.catalog).join("\n"), /host.*derived.*environment/);

  const unsafeHost = clone(result);
  unsafeHost.identity.host = "Wallacy-PC@home";
  assert.match(validateExecutableResult(unsafeHost, documents.catalog).join("\n"), /host.*class/);

  const forgedKey = clone(result);
  forgedKey.equivalenceKey = digest;
  assert.match(validateExecutableResult(forgedKey, documents.catalog).join("\n"), /recomputed/);
});

test("best-known records rank only validated optimizable measurements", () => {
  const result = validResult();
  assert.deepEqual(validateExecutableResult(result, documents.catalog), []);
  const best = validBest(result);
  assert.deepEqual(validateExecutableBestKnown(best, documents.catalog, [result]), []);
  const artifactBest = validBest(result, "artifact-size");
  assert.equal(artifactBest.statistic, "single-artifact");
  assert.deepEqual(validateExecutableBestKnown(artifactBest, documents.catalog, [result]), []);

  const badArtifactStatistic = clone(artifactBest);
  badArtifactStatistic.statistic = "median";
  assert.match(validateExecutableBestKnown(badArtifactStatistic, documents.catalog, [result]).join("\n"), /single-artifact/);

  const forbidden = clone(best);
  forbidden.metric = "stdout";
  assert.match(validateExecutableBestKnown(forbidden, documents.catalog, [result]).join("\n"), /optimizable/);

  const malformed = clone(best);
  malformed.value = "01";
  malformed.derivedFrom = [];
  const malformedErrors = validateExecutableBestKnown(malformed, documents.catalog, [result]).join("\n");
  assert.match(malformedErrors, /canonical decimal/);
  assert.match(malformedErrors, /array of strings/);

  const forgedKey = clone(best);
  forgedKey.equivalenceKey = digest;
  assert.match(validateExecutableBestKnown(forgedKey, documents.catalog, [result]).join("\n"), /recomputed/);

  const mixed = clone(best);
  mixed.toolchain = "other-tool";
  assert.match(validateExecutableBestKnown(mixed, documents.catalog, [result]).join("\n"), /mixes incomparable/);

  const mixedRecipe = clone(best);
  mixedRecipe.recipe = "different-recipe";
  assert.match(validateExecutableBestKnown(mixedRecipe, documents.catalog, [result]).join("\n"), /mixes incomparable/);

  const cResult = validResult("c");
  assert.deepEqual(validateExecutableResult(cResult, documents.catalog), []);
  const cBest = validBest(cResult);
  assert.match(validateExecutableBestKnown(cBest, documents.catalog, [cResult]).join("\n"), /ineligible/);

  assert.match(validateExecutableBestKnown(best, documents.catalog).join("\n"), /validated result records/);
  assert.deepEqual(validateExecutableBestKnownIndex(documents.bestKnown, documents.catalog, historyResults), []);

  const emptyIndex = { ...clone(documents.bestKnown), status: "not-established", records: [] };
  const establishedEmpty = clone(emptyIndex);
  establishedEmpty.status = "established";
  assert.match(validateExecutableBestKnownIndex(establishedEmpty, documents.catalog, [result]).join("\n"), /no records.*not-established/);

  const nonEmptyNotEstablished = clone(emptyIndex);
  nonEmptyNotEstablished.records = [best];
  assert.match(validateExecutableBestKnownIndex(nonEmptyNotEstablished, documents.catalog, [result]).join("\n"), /non-empty.*established/);

  const established = clone(emptyIndex);
  established.status = "established";
  established.records = [best];
  assert.deepEqual(validateExecutableBestKnownIndex(established, documents.catalog, [result]), []);
});

test("best-known derivation is deterministic, excludes zero CPU and separates provenance", () => {
  const zeroCpu = zeroRunCpu(validResult("rust"));
  zeroCpu.id = "hello-rust-zero-cpu";
  assert.deepEqual(validateExecutableResult(zeroCpu, documents.catalog), []);
  const cResult = validResult("c");
  const wResult = validResult("w");

  const derived = deriveExecutableBestKnown(documents.catalog, [wResult, cResult, zeroCpu]);
  assert.equal(derived.status, "established");
  assert.equal(derived.records.length, 4, "zero CPU is recorded but cannot create a CPU best-known record");
  assert.equal(derived.records.some((record) => record.metric === "cpu-time"), false);
  assert.ok(derived.records.every((record) => record.language === "rust"));
  assert.deepEqual(validateExecutableBestKnownFreshness(derived, documents.catalog, [wResult, cResult, zeroCpu]), []);

  const reordered = deriveExecutableBestKnown(documents.catalog, [zeroCpu, cResult, wResult]);
  assert.deepEqual(reordered, derived, "input order must not affect derived output");

  const stale = clone(derived);
  stale.records[0].value = stale.records[0].value === "1" ? "2" : "1";
  assert.match(validateExecutableBestKnownFreshness(stale, documents.catalog, [wResult, cResult, zeroCpu]).join("\n"), /stale/);

  const forbiddenCpu = validBest(zeroCpu, "cpu-time");
  assert.match(validateExecutableBestKnown(forbiddenCpu, documents.catalog, [zeroCpu]).join("\n"), /zero microsecond/);

  const provenanceVariant = clone(zeroCpu);
  provenanceVariant.id = "hello-rust-zero-cpu-other-runner";
  provenanceVariant.provenance.runnerDigest = "sha256:2222222222222222222222222222222222222222222222222222222222222222";
  const separated = deriveExecutableBestKnown(documents.catalog, [provenanceVariant, zeroCpu]);
  assert.equal(separated.records.length, 8, "different runner provenance must not share a best-known group");
  assert.ok(separated.records.every((record) => record.derivedFrom.length === 1));

  const duplicate = clone(zeroCpu);
  assert.throws(() => deriveExecutableBestKnown(documents.catalog, [zeroCpu, duplicate]), /duplicate id/);

  const tieLeft = validResult("rust");
  tieLeft.id = "hello-rust-tie-a";
  const tieRight = clone(tieLeft);
  tieRight.id = "hello-rust-tie-b";
  const tied = deriveExecutableBestKnown(documents.catalog, [tieRight, tieLeft]);
  assert.deepEqual(tied.records[0].derivedFrom, [tieLeft.id, tieRight.id]);

  const reversedIndex = clone(tied);
  reversedIndex.records.reverse();
  assert.match(validateExecutableBestKnownIndex(reversedIndex, documents.catalog, [tieLeft, tieRight]).join("\n"), /sorted by id/);
  const duplicateIndex = clone(tied);
  duplicateIndex.records[1].id = duplicateIndex.records[0].id;
  assert.match(validateExecutableBestKnownIndex(duplicateIndex, documents.catalog, [tieLeft, tieRight]).join("\n"), /ids must be unique/);
});
