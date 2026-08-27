import assert from "node:assert/strict";
import { mkdtemp, readFile, readdir, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import {
  ROOT,
  RUNNER_PATH,
  runnerEvidence,
} from "./benchmark-driven-development-runner.mjs";
import {
  DESCRIPTOR_IDENTITIES,
  SOURCE_FIXTURE,
  loadBmdDocuments,
  validateResult,
} from "./benchmark-driven-development-machine.mjs";

function tempBuildNames(entries) {
  return new Set(entries.filter((entry) => entry.startsWith("w-bmd1-seed-")));
}

const outputDirectory = await mkdtemp(path.join(tmpdir(), "w-bmd1-smoke-"));
const outputPath = path.join(outputDirectory, "result.json");
const beforeBuild = tempBuildNames(await readdir(tmpdir()));
try {
  const execution = Bun.spawnSync({
    cmd: [process.execPath, RUNNER_PATH, "--output", outputPath],
    cwd: ROOT,
    stdout: "pipe",
    stderr: "pipe",
  });
  assert.equal(execution.exitCode, 0, execution.stderr.toString("utf8"));
  assert.equal(execution.stdout.length, 0);
  assert.equal(execution.stderr.length, 0);
  const result = JSON.parse(await readFile(outputPath, "utf8"));
  const documents = loadBmdDocuments();
  assert.deepEqual(validateResult(result, documents.manifest), []);
  assert.equal(result.quality, "exploratory");
  assert.equal(result.claim, "measurement-only");
  assert.equal(result.comparison, null);
  assert.equal(result.samples.raw.length, 9);
  assert.equal(result.samples.warmup.length, 1);
  assert.equal(result.samples.stopRule.count, 9);
  assert.equal(result.samples.order, "single-series");
  assert.equal(result.oracle.complete, true);
  assert.equal(result.oracle.beforeSamples, true);
  assert.deepEqual(result.identity.source, documents.manifest.identity.source);
  assert.deepEqual(result.identity.graph, documents.manifest.identity.graph);
  assert.deepEqual(result.identity.input, documents.manifest.identity.input);
  assert.deepEqual(result.identity.command, documents.manifest.command);
  assert.equal(result.provenance.sourceDigest, SOURCE_FIXTURE.digest);
  assert.equal(result.provenance.inputDigest, DESCRIPTOR_IDENTITIES.input.digest);
  for (const field of ["artifactDigest", "recipeDigest", "runnerDigest", "toolchainDigest"]) {
    assert.match(result.provenance[field], /^sha256:[0-9a-f]{64}$/u);
  }
  assert.equal(result.provenance.runnerDigest, (await runnerEvidence()).digest);
  assert.ok(result.environment.toolchain.includes("bun=" + process.versions.bun));
  assert.ok(result.environment.flags.includes("bun-version=" + process.versions.bun));
  console.log("BMD1 runner smoke: Release build, oracle, 1 warmup, 9 fresh-process samples, result validation, and empty CLI output passed");
} finally {
  await rm(outputDirectory, { recursive: true, force: true });
  const afterBuild = tempBuildNames(await readdir(tmpdir()));
  for (const name of afterBuild) {
    if (!beforeBuild.has(name)) throw new Error("temporary build directory was not removed: " + name);
  }
}
