import assert from "node:assert/strict";
import crypto from "node:crypto";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import { benchmarkUsage, consumeLocalResult, main, parseBenchmarkCliArguments, validateDiagnosticUpdateBoundary, validateUpdateBoundary } from "./benchmark-cli.mjs";
import { FLOAT_BIT_REPRESENTATION_WORKLOAD_ID } from "./executable-benchmark-machine.mjs";

test("benchmark facade exposes update and preserves bounded run arguments", () => {
  assert.deepEqual(parseBenchmarkCliArguments(["list"]), { command: "list" });
  assert.deepEqual(parseBenchmarkCliArguments(["check"]), { command: "check" });
  assert.deepEqual(parseBenchmarkCliArguments(["gpu0"]), { command: "gpu0", arguments: [] });
  assert.deepEqual(parseBenchmarkCliArguments(["gpu0", "run", "--samples", "21"]), {
    command: "gpu0", arguments: ["run", "--samples", "21"],
  });
  assert.deepEqual(parseBenchmarkCliArguments(["prune"]), { command: "prune" });
  assert.throws(() => parseBenchmarkCliArguments(["prune", "extra"]), /does not accept positional arguments or options/u);
  assert.deepEqual(parseBenchmarkCliArguments(["update", "benchmarks/results/w.json", "benchmarks/results/c.json"]), {
    command: "update", inputs: ["benchmarks/results/w.json", "benchmarks/results/c.json"],
  });
  assert.deepEqual(parseBenchmarkCliArguments(["diagnose", "benchmarks/results/w.json", "benchmarks/results/wsl.json"]), {
    command: "diagnose", inputs: ["benchmarks/results/w.json", "benchmarks/results/wsl.json"],
  });
  assert.throws(() => parseBenchmarkCliArguments(["diagnose"]), /one or more JSON paths/u);
  assert.throws(() => parseBenchmarkCliArguments(["diagnose", "same.json", "same.json"]), /must be unique/u);
  assert.throws(() => parseBenchmarkCliArguments(["update"]), /one or more JSON paths/u);
  assert.throws(() => parseBenchmarkCliArguments(["update", "same.json", "same.json"]), /must be unique/u);
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "hello", "--language", "rust", "--samples", "9"]), {
    command: "run", target: "hello", language: "rust", platform: "windows-x64", output: "benchmarks/results/hello-rust.local.json", warmup: 1, compileSamples: 9, runSamples: 9,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "hello", "--platform", "linux-wsl-x64"]), {
    command: "run", target: "hello", language: "w", platform: "linux-wsl-x64", output: "benchmarks/results/hello-w-linux-wsl-x64.local.json", warmup: 1, compileSamples: 9, runSamples: 101,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "hello-platform-minimal", "--language", "rust", "--platform", "linux-wsl-x64"]), {
    command: "run", target: "hello-platform-minimal", language: "rust", platform: "linux-wsl-x64", output: "benchmarks/results/hello-platform-minimal-rust-linux-wsl-x64.local.json", warmup: 1, compileSamples: 9, runSamples: 101,
  });
  assert.throws(() => parseBenchmarkCliArguments(["run", "--target", "hello", "--language", "c", "--platform", "linux-wsl-x64"]), /hello-platform-minimal/u);
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "process-handler-lifecycle", "--language", "w"]), {
    command: "run", target: "process-handler-lifecycle", language: "w", platform: "windows-x64", output: "benchmarks/results/process-handler-lifecycle-w.local.json", warmup: 1, compileSamples: 9, runSamples: 101,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "process-entry", "--language", "w"]), {
    command: "run", target: "process-entry", language: "w", platform: "windows-x64", output: "benchmarks/results/process-entry-w.local.json", warmup: 1, compileSamples: 9, runSamples: 101,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "process-arguments-ordering", "--language", "rust"]), {
    command: "run", target: "process-arguments-ordering", language: "rust", platform: "windows-x64", output: "benchmarks/results/process-arguments-ordering-rust.local.json", warmup: 1, compileSamples: 9, runSamples: 101,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", FLOAT_BIT_REPRESENTATION_WORKLOAD_ID, "--language", "rust"]), {
    command: "run", target: FLOAT_BIT_REPRESENTATION_WORKLOAD_ID, language: "rust", platform: "windows-x64", output: `benchmarks/results/${FLOAT_BIT_REPRESENTATION_WORKLOAD_ID}-rust.local.json`, warmup: 1, compileSamples: 9, runSamples: 101,
  });
  assert.throws(() => parseBenchmarkCliArguments(["run", "--target", "process-entry0", "--language", "w"]), /unsupported target/);
  assert.throws(() => parseBenchmarkCliArguments(["run", "--run-samples", "1003"]), /outside its allowed range/);
  assert.throws(() => parseBenchmarkCliArguments(["record", "benchmarks/results/local.json"]), /unknown command/);
  assert.match(benchmarkUsage(), /<list\|run\|gpu0\|validate\|update\|diagnose\|prune\|check>/u);
  assert.match(benchmarkUsage(), /gpu0 \[run\|check\]/u);
  assert.match(benchmarkUsage(), /update <result\.json>\.\.\./u);
  assert.match(benchmarkUsage(), /diagnose <result\.json>\.\.\./u);
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
  assert.equal(listing.workloads.some((workload) => workload.id === "composition"), false);
  assert.ok(listing.workloads.every((workload) => workload.benchmarkStatus !== "planned"));
  assert.ok(listing.workloads.filter((workload) =>
    workload.benchmarkStatus === "partial-exploratory-ready" && workload.languages.length === 1 && workload.languages[0] === "w",
  ).length > 0);
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

test("diagnostic boundary binds current provenance while explicitly permitting dirty worktrees", async () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "w-benchmark-diagnostic-boundary-"));
  try {
    fs.mkdirSync(path.join(root, "benchmarks"), { recursive: true });
    fs.mkdirSync(path.join(root, "tooling"), { recursive: true });
    const catalogPath = path.join(root, "benchmarks", "executable-catalog.json");
    const runnerPath = path.join(root, "tooling", "executable-benchmark-runner.mjs");
    fs.writeFileSync(catalogPath, "{}\n");
    fs.writeFileSync(runnerPath, "export {};\n");
    const fileDigest = (file) => `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
    const gitState = { commit: "1".repeat(40), dirty: true };
    const result = { provenance: { commit: gitState.commit, catalogDigest: fileDigest(catalogPath), runnerDigest: fileDigest(runnerPath), worktreeDirtyAtMeasurement: false } };
    assert.deepEqual(await validateDiagnosticUpdateBoundary(result, { root, gitState }), gitState);
    assert.equal(result.provenance.worktreeDirtyAtMeasurement, false,
      "a clean run retains its measurement-time state when publication happens in a dirty checkout");
    const dirtyMeasurement = { provenance: { ...result.provenance, worktreeDirtyAtMeasurement: true } };
    assert.deepEqual(await validateDiagnosticUpdateBoundary(dirtyMeasurement, {
      root,
      gitState: { ...gitState, dirty: false },
    }), { ...gitState, dirty: false });
    assert.equal(dirtyMeasurement.provenance.worktreeDirtyAtMeasurement, true,
      "cleaning the checkout before publication does not rewrite a dirty run receipt");
    await assert.rejects(validateDiagnosticUpdateBoundary({ provenance: { ...result.provenance, commit: "2".repeat(40) } }, { root, gitState }), /does not match current HEAD/u);
    await assert.rejects(validateDiagnosticUpdateBoundary({ provenance: { ...result.provenance, catalogDigest: `sha256:${"a".repeat(64)}` } }, { root, gitState }), /catalogDigest is stale/u);
    await assert.rejects(validateDiagnosticUpdateBoundary({ provenance: { ...result.provenance, runnerDigest: `sha256:${"b".repeat(64)}` } }, { root, gitState }), /runnerDigest is stale/u);
    await assert.rejects(validateUpdateBoundary(result, { root, gitState }), /clean Git worktree/u,
      "ordinary ranked publication remains clean-worktree-only");
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
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
        return { removedCount: 18, removedDiagnostics: 2 };
      },
    });
  } finally {
    console.log = originalLog;
  }
  assert.deepEqual(output, ["pruned 18 stale best-metric cells and 2 stale diagnostic rows"]);
});
