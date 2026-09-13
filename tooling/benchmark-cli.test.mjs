import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import { benchmarkUsage, consumeLocalResult, main, parseBenchmarkCliArguments, validateUpdateBoundary } from "./benchmark-cli.mjs";

test("benchmark facade exposes update and preserves bounded run arguments", () => {
  assert.deepEqual(parseBenchmarkCliArguments(["list"]), { command: "list" });
  assert.deepEqual(parseBenchmarkCliArguments(["check"]), { command: "check" });
  assert.deepEqual(parseBenchmarkCliArguments(["prune"]), { command: "prune" });
  assert.throws(() => parseBenchmarkCliArguments(["prune", "extra"]), /does not accept positional arguments or options/u);
  assert.deepEqual(parseBenchmarkCliArguments(["update", "benchmarks/results/w.json", "benchmarks/results/c.json"]), {
    command: "update", inputs: ["benchmarks/results/w.json", "benchmarks/results/c.json"],
  });
  assert.throws(() => parseBenchmarkCliArguments(["update"]), /one or more JSON paths/u);
  assert.throws(() => parseBenchmarkCliArguments(["update", "same.json", "same.json"]), /must be unique/u);
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "hello", "--language", "rust", "--samples", "9"]), {
    command: "run", target: "hello", language: "rust", output: "benchmarks/results/hello-rust.local.json", warmup: 1, compileSamples: 9, runSamples: 9,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "process-handler-lifecycle", "--language", "w"]), {
    command: "run", target: "process-handler-lifecycle", language: "w", output: "benchmarks/results/process-handler-lifecycle-w.local.json", warmup: 1, compileSamples: 9, runSamples: 101,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "process-entry", "--language", "w"]), {
    command: "run", target: "process-entry", language: "w", output: "benchmarks/results/process-entry-w.local.json", warmup: 1, compileSamples: 9, runSamples: 101,
  });
  assert.throws(() => parseBenchmarkCliArguments(["run", "--target", "process-entry0", "--language", "w"]), /unsupported target/);
  assert.throws(() => parseBenchmarkCliArguments(["run", "--run-samples", "1003"]), /outside its allowed range/);
  assert.throws(() => parseBenchmarkCliArguments(["record", "benchmarks/results/local.json"]), /unknown command/);
  assert.match(benchmarkUsage(), /<list\|run\|validate\|update\|prune\|check>/u);
  assert.match(benchmarkUsage(), /update <result\.json>\.\.\./u);
  assert.match(benchmarkUsage(), /prune/u);
  assert.match(benchmarkUsage(), /process-handler-lifecycle/u);
  assert.match(benchmarkUsage(), /process-entry/u);
  assert.doesNotMatch(benchmarkUsage(), /process-entry0/u);
});

test("list exposes runner-backed workloads and omits the planned backlog", async () => {
  const output = [];
  const originalLog = console.log;
  console.log = (value) => output.push(value);
  try {
    await main(["list"]);
  } finally {
    console.log = originalLog;
  }
  assert.equal(output.length, 1);
  const listing = JSON.parse(output[0]);
  assert.equal(listing.workloads.some((workload) => workload.id === "restaurant-composition"), false);
  assert.ok(listing.workloads.every((workload) => workload.benchmarkStatus !== "planned"));
  assert.equal(listing.workloads.filter((workload) =>
    workload.benchmarkStatus === "partial-exploratory-ready" && workload.languages.length === 1 && workload.languages[0] === "w",
  ).length, 16);
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

test("prune command reports its finite removal count", async () => {
  const output = [];
  const originalLog = console.log;
  console.log = (value) => output.push(value);
  try {
    await main(["prune"], {
      root: "C:\\benchmark-prune-test",
      pruneLiveCatalog: async ({ root }) => {
        assert.equal(root, "C:\\benchmark-prune-test");
        return { removedCount: 18 };
      },
    });
  } finally {
    console.log = originalLog;
  }
  assert.deepEqual(output, ["pruned 18 stale best-metric cells"]);
});
