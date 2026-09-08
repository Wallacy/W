import assert from "node:assert/strict";
import test from "node:test";
import {
  benchmarkUsage,
  main,
  parseBenchmarkCliArguments,
  validateRecordBoundary,
} from "./benchmark-cli.mjs";

test("benchmark facade parses bounded commands without shell syntax", () => {
  assert.deepEqual(parseBenchmarkCliArguments(["list"]), { command: "list" });
  assert.deepEqual(parseBenchmarkCliArguments(["check"]), { command: "check" });
  assert.deepEqual(parseBenchmarkCliArguments(["validate", "benchmarks/results/local.json"]), {
    command: "validate",
    input: "benchmarks/results/local.json",
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "hello", "--language", "w", "--samples", "9"]), {
    command: "run",
    target: "hello",
    language: "w",
    output: "benchmarks/results/hello-w.local.json",
    warmup: 1,
    samples: 9,
  });
  assert.throws(() => parseBenchmarkCliArguments(["list", "--target", "hello"]), /does not accept/);
  assert.throws(() => parseBenchmarkCliArguments(["run", "--language", "c"]), /unsupported language/);
  assert.throws(() => parseBenchmarkCliArguments(["run", "--samples", "10"]), /odd/);
  assert.match(benchmarkUsage(), /private Native0\/MLIR0/u);
});

test("benchmark run forwards structured options through an injected runner", async () => {
  let observed;
  const exit = await main(["run", "--target=hello", "--language=w", "--output", "benchmarks/results/injected.json", "--warmup", "2", "--samples", "11"], {
    runBenchmark: async (options) => { observed = options; },
  });
  assert.equal(exit, 0);
  assert.deepEqual(observed, {
    command: "run",
    target: "hello",
    language: "w",
    output: "benchmarks/results/injected.json",
    warmup: 2,
    samples: 11,
  });
});

test("history boundary rejects dirty and stale provenance before mutation", async () => {
  const record = { provenance: { commit: "0".repeat(40) } };
  await assert.rejects(
    validateRecordBoundary(record, { gitState: { commit: "1".repeat(40), dirty: true } }),
    /clean Git worktree/,
  );
  await assert.rejects(
    validateRecordBoundary(record, { gitState: { commit: "1".repeat(40), dirty: false } }),
    /does not match current HEAD/,
  );
});
