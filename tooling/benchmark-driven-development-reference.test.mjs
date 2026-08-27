import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import {
  BENCHMARK_DISPOSITIONS,
  DESCRIPTOR_IDENTITIES,
  EDIT_RECIPE,
  LANGUAGE_PROFILES,
  LIFECYCLE_PHASES,
  REQUIRED_CASES,
  SOURCE_FIXTURE,
  loadBmdDocuments,
  reduceCase,
  reduceCorpus,
  validateCorpus,
  validateManifest,
  validateProgram,
  validateScenario,
} from "./benchmark-driven-development-machine.mjs";

const root = path.resolve(import.meta.dir, "..");
const documents = loadBmdDocuments();

test("BMD0 documents pass the host validators", () => {
  assert.deepEqual(validateProgram(documents.program, documents.corpus), []);
  assert.deepEqual(validateManifest(documents.manifest), []);
  assert.deepEqual(validateCorpus(documents.corpus), []);
});

test("program profiles and language scenario profiles have separate contracts", () => {
  assert.deepEqual(documents.program.profiles.map((profile) => profile.id), LANGUAGE_PROFILES);
  assert.equal(documents.program.profiles.find((profile) => profile.id === "idiomatic").primary, true);
  assert.equal(documents.program.profiles.find((profile) => profile.id === "idiomatic").regression, true);
  assert.equal(documents.program.profiles.find((profile) => profile.id === "learner").primary, false);
  assert.equal(documents.program.profiles.find((profile) => profile.id === "frontier").regression, false);

  const compiler = documents.corpus.cases.find((item) => item.id === "BMD0-W-1487-compiler-lifecycle");
  assert.equal(Object.hasOwn(compiler.input, "profiles"), false);
  assert.deepEqual(compiler.input.languageProfiles, {
    applicability: "not-applicable",
    reason: "Compiler lifecycle uses one source, graph and input identity instead of language source profiles.",
  });
  assert.equal(Object.hasOwn(documents.manifest, "profiles"), false);
  assert.equal(documents.manifest.benchmarkDisposition, "compiler-lifecycle");
  assert.equal(documents.manifest.languageProfiles.applicability, "not-applicable");
  assert.equal(documents.manifest.backend.benchmarkRunnerAvailable, false);
  assert.equal(documents.manifest.backend.frontendAvailable, true);
  assert.equal(documents.manifest.backend.nativeBackendAvailable, false);
  assert.equal(documents.manifest.backend.runtimeAvailable, false);
  assert.equal(documents.manifest.backend.resultsAllowed, false);
});

test("the accepted corpus covers all four dispositions with their tracks", () => {
  const accepted = documents.corpus.cases.filter((item) => item.kind === "accepted");
  const byDisposition = new Map(accepted.map((item) => [item.input.benchmarkDisposition, item]));
  assert.deepEqual([...byDisposition.keys()].sort(), [...BENCHMARK_DISPOSITIONS].sort());
  assert.equal(byDisposition.get("required").track, "language");
  assert.equal(byDisposition.get("compiler-lifecycle").track, "compiler-lifecycle");
  assert.equal(byDisposition.get("deferred").track, "language");
  assert.equal(byDisposition.get("deferred").input.blocker, "codegen");
  assert.equal(byDisposition.get("deferred").input.taskId, "core-language-units");
  assert.ok(byDisposition.get("deferred").input.stopCondition.length > 0);
  assert.equal(byDisposition.get("not-applicable").track, "documentation");
  assert.equal(byDisposition.get("not-applicable").input.digestOnly, true);
  assert.ok(byDisposition.get("not-applicable").input.reason.length > 0);
});

test("source-backed lifecycle descriptors use current bytes and identities", () => {
  assert.deepEqual(documents.manifest.identity.source, SOURCE_FIXTURE);
  for (const axis of ["graph", "input"]) {
    assert.deepEqual(documents.manifest.identity[axis], DESCRIPTOR_IDENTITIES[axis]);
    const descriptor = JSON.parse(fs.readFileSync(path.resolve(root, DESCRIPTOR_IDENTITIES[axis].path), "utf8"));
    assert.equal(descriptor.id, DESCRIPTOR_IDENTITIES[axis].id);
    assert.equal(descriptor.source.path, SOURCE_FIXTURE.path);
    assert.equal(descriptor.source.symbol, SOURCE_FIXTURE.symbol);
    assert.equal(descriptor.source.digest, SOURCE_FIXTURE.digest);
  }
  assert.equal(documents.manifest.command.operation, "check");
  assert.equal(documents.manifest.command.arguments[0], SOURCE_FIXTURE.path);
  const inputDescriptor = JSON.parse(fs.readFileSync(path.resolve(root, DESCRIPTOR_IDENTITIES.input.path), "utf8"));
  assert.deepEqual(inputDescriptor.invocation.edit, {
    kind: EDIT_RECIPE.kind,
    sourcePath: EDIT_RECIPE.sourcePath,
    targetSymbol: EDIT_RECIPE.targetSymbol,
    occurrence: EDIT_RECIPE.occurrence,
    match: EDIT_RECIPE.match,
    replacement: EDIT_RECIPE.replacement,
    semanticPreserving: true,
    applyTo: "temporary-copy",
  });
  assert.deepEqual(inputDescriptor.invocation.recipe, {
    operation: EDIT_RECIPE.operation,
    expectedMatches: EDIT_RECIPE.expectedMatches,
    match: EDIT_RECIPE.match,
    replacement: EDIT_RECIPE.replacement,
    semanticPreserving: true,
    temporaryCopy: true,
  });
});

test("compiler lifecycle is one identity across ten ordered phases", () => {
  assert.deepEqual(documents.manifest.lifecycle.map((phase) => phase.id), LIFECYCLE_PHASES);
  assert.deepEqual(documents.manifest.lifecycle.slice(0, 4).map((phase) => phase.status), ["ready", "ready", "ready", "ready"]);
  assert.deepEqual(documents.manifest.lifecycle.slice(4).map((phase) => phase.status), ["blocked", "blocked", "blocked", "blocked", "blocked", "blocked"]);
  assert.deepEqual(documents.manifest.lifecycle.find((phase) => phase.id === "hir").blockedBy, ["hir"]);
  assert.deepEqual(documents.manifest.lifecycle.find((phase) => phase.id === "lowering").blockedBy, ["lowering"]);
  assert.deepEqual(documents.manifest.lifecycle.find((phase) => phase.id === "codegen").blockedBy, ["codegen"]);
  assert.deepEqual(documents.manifest.lifecycle.find((phase) => phase.id === "link").blockedBy, ["codegen"]);
  assert.deepEqual(documents.manifest.lifecycle.find((phase) => phase.id === "startup").blockedBy, ["runtime"]);
  assert.deepEqual(documents.manifest.lifecycle.find((phase) => phase.id === "execution").blockedBy, ["runtime", "provider"]);
  for (const phase of documents.manifest.lifecycle) {
    assert.equal(phase.source.id, "last-light-checker-bootstrap-source");
    assert.equal(phase.graph.id, DESCRIPTOR_IDENTITIES.graph.id);
    assert.equal(phase.input.id, DESCRIPTOR_IDENTITIES.input.id);
  }
});

test("equivalent and open lanes preserve their different comparison contracts", () => {
  const equivalent = documents.corpus.cases.find((item) => item.id === "BMD0-W-1487-current").input;
  for (const field of ["sameAlgorithm", "sameRepresentation", "sameValidation", "sameNumericContract", "sameInput"]) assert.equal(equivalent[field], true);
  const open = documents.corpus.cases.find((item) => item.id === "BMD0-W-1487-open-lane").input;
  assert.equal(open.sameAlgorithm, false);
  assert.equal(open.sameRepresentation, false);
  assert.equal(open.sameValidation, true);
  assert.equal(open.sameNumericContract, true);
  assert.equal(open.sameInput, true);
  assert.deepEqual(documents.manifest.baselinePolicy, {
    primary: "historical-w",
    independent: ["c-clang", "rust"],
    role: "contextual-not-ranking",
    exceptionReason: null,
    recipe: "equivalent",
  });
});

test("the corpus has independent negative coverage and no expected-result echo", () => {
  assert.equal(documents.corpus.cases.length, 21);
  assert.equal(documents.corpus.cases.filter((item) => item.kind === "accepted").length, 5);
  assert.equal(documents.corpus.cases.filter((item) => item.kind === "rejected").length, 16);
  assert.deepEqual(REQUIRED_CASES.every((id) => documents.corpus.cases.some((item) => item.id === id)), true);
  for (const id of REQUIRED_CASES.filter((value) => value.includes("missing-blocker") || value.includes("missing-reason"))) {
    const item = documents.corpus.cases.find((candidate) => candidate.id === id);
    assert.equal(item.kind, "rejected");
    assert.equal(reduceCase(item).classification, "rejected");
  }
  for (const item of documents.corpus.cases) {
    assert.equal(Object.hasOwn(item.input ?? {}, "expected"), false);
    assert.equal(Object.hasOwn(item.input ?? {}, "expectedResult"), false);
    assert.equal(Object.hasOwn(item.input ?? {}, "result"), false);
  }
  const reductions = reduceCorpus(documents.corpus);
  assert.equal(reductions.filter((item) => item.classification === "accepted").length, 5);
  assert.equal(reductions.filter((item) => item.classification === "rejected").length, 16);
});

test("WBench/1 defines a future result without creating one", () => {
  const result = documents.schema.$defs.result;
  assert.equal(result.properties.kind.const, "result");
  assert.deepEqual(result.required, [
    "$schema",
    "schema",
    "kind",
    "id",
    "status",
    "workload",
    "oracle",
    "samples",
    "environment",
    "provenance",
    "metrics",
    "summary",
    "semanticDeviations",
    "disclosures",
  ]);
  assert.deepEqual(result.properties.oracle.required, ["validationDigest", "complete", "beforeSamples"]);
  assert.deepEqual(result.properties.samples.required, ["raw", "warmup", "stopRule", "order"]);
  assert.deepEqual(result.properties.environment.required, [
    "hardware",
    "kernel",
    "toolchain",
    "flags",
    "target",
    "provider",
  ]);
  assert.deepEqual(result.properties.provenance.required, ["sourceDigest", "artifactDigest", "inputDigest", "recipeDigest", "runnerDigest", "toolchainDigest"]);
  assert.deepEqual(result.properties.workload.required, ["manifestDigest", "track", "lane", "phase", "baseline", "variant"]);
  assert.deepEqual(result.properties.workload.properties.profile.enum, ["learner", "idiomatic", "frontier", null]);
  assert.equal(result.properties.summary.properties.derivedFromRawSamples.const, true);
  assert.equal(fs.existsSync(path.join(root, "benchmarks", "results")), false);
});

test("disposition fields are required and compiler lifecycle rejects source profiles", () => {
  const deferred = documents.corpus.cases.find((item) => item.id === "BMD0-W-1487-deferred");
  const missingBlocker = structuredClone(deferred.input);
  delete missingBlocker.blocker;
  assert.ok(validateScenario({ ...missingBlocker, track: "language", lane: "equivalent" }).some((error) => error.includes("blocker")));
  const notApplicable = documents.corpus.cases.find((item) => item.id === "BMD0-W-1487-not-applicable");
  const missingReason = structuredClone(notApplicable.input);
  delete missingReason.reason;
  assert.ok(validateScenario({ ...missingReason, track: "documentation", lane: "open" }).some((error) => error.includes("reason")));
  const compiler = documents.corpus.cases.find((item) => item.id === "BMD0-W-1487-compiler-lifecycle");
  assert.ok(validateScenario({ ...compiler.input, profiles: documents.program.profiles, track: compiler.track, lane: compiler.lane }).some((error) => error.includes("profiles")));
});
