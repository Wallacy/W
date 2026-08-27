import assert from "node:assert/strict";
import { mkdtemp, readFile, readdir, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import {
  ROOT,
  RUNNER_PATH,
} from "./benchmark-driven-development-runner.mjs";
import {
  loadBmdDocuments,
  validateResult,
} from "./benchmark-driven-development-machine.mjs";

function temporaryComparisonNames(entries) {
  return new Set(entries.filter((entry) => entry.startsWith("w-bmd2-")));
}

const headExecution = Bun.spawnSync({
  cmd: ["git", "rev-parse", "HEAD"],
  cwd: ROOT,
  stdout: "pipe",
  stderr: "pipe",
});
assert.equal(headExecution.exitCode, 0, headExecution.stderr.toString("utf8"));
const head = headExecution.stdout.toString("utf8").trim();
assert.match(head, /^[0-9a-f]{40}$/u);

const outputDirectory = await mkdtemp(path.join(tmpdir(), "w-bmd2-smoke-"));
const outputPath = path.join(outputDirectory, "comparison.json");
const beforeBuild = temporaryComparisonNames(await readdir(tmpdir()));
try {
  const execution = Bun.spawnSync({
    cmd: [
      process.execPath,
      RUNNER_PATH,
      "--output", outputPath,
      "--baseline", head,
      "--candidate", head,
    ],
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
  assert.equal(result.claim, "comparison-only");
  assert.equal(result.verdict, "not-evaluated");
  assert.equal(result.comparison.calibration, true);
  assert.equal(result.comparison.baseline.commit, head);
  assert.equal(result.comparison.candidate.commit, head);
  assert.equal(result.samples.raw.length, 18);
  assert.equal(result.samples.warmup.length, 2);
  assert.equal(result.samples.stopRule.pairs, 9);
  assert.equal(result.samples.order, "balanced-paired-interleaved-sha256-v1");
  console.log("BMD2 runner smoke: independent HEAD x HEAD Release builds, paired oracles, 1 warmup pair, 9 raw pairs, comparison-only calibration and empty CLI output passed");
} finally {
  await rm(outputDirectory, { recursive: true, force: true });
  const afterBuild = temporaryComparisonNames(await readdir(tmpdir()));
  for (const name of afterBuild) {
    if (!beforeBuild.has(name)) throw new Error("temporary comparison directory was not removed: " + name);
  }
}
