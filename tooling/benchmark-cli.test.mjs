import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import { benchmarkUsage, consumeLocalResult, parseBenchmarkCliArguments, validateUpdateBoundary } from "./benchmark-cli.mjs";

test("benchmark facade exposes update and preserves bounded run arguments", () => {
  assert.deepEqual(parseBenchmarkCliArguments(["list"]), { command: "list" });
  assert.deepEqual(parseBenchmarkCliArguments(["check"]), { command: "check" });
  assert.deepEqual(parseBenchmarkCliArguments(["update", "benchmarks/results/local.json"]), { command: "update", input: "benchmarks/results/local.json" });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "hello", "--language", "rust", "--samples", "9"]), {
    command: "run", target: "hello", language: "rust", output: "benchmarks/results/hello-rust.local.json", warmup: 1, samples: 9,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "process-handler-lifecycle", "--language", "w"]), {
    command: "run", target: "process-handler-lifecycle", language: "w", output: "benchmarks/results/process-handler-lifecycle-w.local.json", warmup: 1, samples: 9,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "process-entry", "--language", "w"]), {
    command: "run", target: "process-entry", language: "w", output: "benchmarks/results/process-entry-w.local.json", warmup: 1, samples: 9,
  });
  assert.throws(() => parseBenchmarkCliArguments(["run", "--target", "process-entry0", "--language", "w"]), /unsupported target/);
  assert.throws(() => parseBenchmarkCliArguments(["record", "benchmarks/results/local.json"]), /unknown command/);
  assert.match(benchmarkUsage(), /<list\|run\|validate\|update\|check>/u);
  assert.match(benchmarkUsage(), /update <result\.json>/u);
  assert.match(benchmarkUsage(), /process-handler-lifecycle/u);
  assert.match(benchmarkUsage(), /process-entry/u);
  assert.doesNotMatch(benchmarkUsage(), /process-entry0/u);
});

test("successful update consumption removes only the local result and empty directory", async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), "w-benchmark-consume-test-"));
  const result = path.join(directory, "result.json");
  fs.writeFileSync(result, "{}\n");
  assert.equal(await consumeLocalResult(result, directory), true);
  assert.equal(fs.existsSync(result), false);
  assert.equal(fs.existsSync(directory), false);
});

test("update boundary rejects dirty and stale provenance before mutation", async () => {
  const result = { provenance: { commit: "0".repeat(40) } };
  await assert.rejects(validateUpdateBoundary(result, { gitState: { commit: "1".repeat(40), dirty: true } }), /clean Git worktree/);
  await assert.rejects(validateUpdateBoundary(result, { gitState: { commit: "1".repeat(40), dirty: false } }), /does not match current HEAD/);
});
