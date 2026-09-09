import assert from "node:assert/strict";
import crypto from "node:crypto";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import {
  ROOT,
  loadExecutableDocuments,
} from "./executable-benchmark-machine.mjs";
import {
  benchmarkUsage,
  consumeRecordedLocalResult,
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
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--language", "c"]), {
    command: "run",
    target: "hello",
    language: "c",
    output: "benchmarks/results/hello-c.local.json",
    warmup: 1,
    samples: 9,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--language", "rust"]), {
    command: "run",
    target: "hello",
    language: "rust",
    output: "benchmarks/results/hello-rust.local.json",
    warmup: 1,
    samples: 9,
  });
  assert.deepEqual(parseBenchmarkCliArguments(["run", "--target", "restaurant-branch", "--language", "c"]), {
    command: "run",
    target: "restaurant-branch",
    language: "c",
    output: "benchmarks/results/restaurant-branch-c.local.json",
    warmup: 1,
    samples: 9,
  });
  assert.throws(() => parseBenchmarkCliArguments(["run", "--samples", "10"]), /odd/);
  assert.match(benchmarkUsage(), /public w build Release/u);
  assert.match(benchmarkUsage(), /restaurant-branch/u);
});

test("successful history publication can consume its local result and empty directory", async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), "w-benchmark-consume-test-"));
  const result = path.join(directory, "result.json");
  fs.writeFileSync(result, "{}\n");
  assert.equal(await consumeRecordedLocalResult(result, directory), true);
  assert.equal(fs.existsSync(result), false);
  assert.equal(fs.existsSync(directory), false);
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

test("CLI record consumes its candidate only after isolated publication succeeds", async () => {
  const fixture = fs.mkdtempSync(path.join(os.tmpdir(), "w-benchmark-cli-record-test-"));
  const files = [
    "benchmarks/executable-benchmark.schema.json",
    "benchmarks/executable-catalog.json",
    "benchmarks/executable-best-known.json",
    "benchmarks/EXECUTABLES.md",
    "benchmarks/executable/hello.w",
    "benchmarks/executable/hello.c",
    "benchmarks/executable/hello.rs",
    "benchmarks/executable/restaurant_branch.c",
    "benchmarks/executable/restaurant_branch.rs",
    "compiler/seed-c/fixtures/restaurant-if.w",
    "compiler/seed-c/fixtures/restaurant-nested-if.w",
    "compiler/seed-c/fixtures/restaurant-bool-short-circuit.w",
    "compiler/seed-c/fixtures/restaurant-interpolation.w",
    "tooling/executable-benchmark-runner.mjs",
  ];
  try {
    for (const relative of files) {
      const source = path.join(ROOT, relative);
      const destination = path.join(fixture, relative);
      fs.mkdirSync(path.dirname(destination), { recursive: true });
      fs.copyFileSync(source, destination);
    }
    const sourceHistory = path.join(ROOT, "benchmarks", "history", "executables");
    const destinationHistory = path.join(fixture, "benchmarks", "history", "executables");
    fs.mkdirSync(destinationHistory, { recursive: true });
    for (const name of fs.readdirSync(sourceHistory)) {
      if (!name.endsWith(".json")) continue;
      fs.copyFileSync(path.join(sourceHistory, name), path.join(destinationHistory, name));
    }

    const fixtureDocuments = loadExecutableDocuments(fixture);
    const rustReference = fixtureDocuments.history.records.find((reference) => reference.id.startsWith("hello-rust-"));
    const rustRecord = JSON.parse(fs.readFileSync(path.join(destinationHistory, rustReference.path), "utf8"));
    rustRecord.id = "hello-rust-cli-fixture";
    rustRecord.artifact.cleanliness = {
      coffSymbols: { pointer: "0", count: "0" },
      codeView: { count: "0", sizeBytes: "0" },
      debugDirectory: { presence: "absent", sizeBytes: "0", entries: [] },
      certificateDirectory: { pointer: "0", sizeBytes: "0" },
      sectionData: "in-bounds",
      sidecars: { count: "0" },
      overlay: { sizeBytes: "0" },
    };
    rustRecord.provenance.observedAt = "2026-09-08T00:00:01.000Z";
    rustRecord.provenance.catalogDigest = "sha256:" + crypto.createHash("sha256")
      .update(fs.readFileSync(path.join(fixture, "benchmarks", "executable-catalog.json")))
      .digest("hex");
    rustRecord.provenance.runnerDigest = "sha256:" + crypto.createHash("sha256")
      .update(fs.readFileSync(path.join(fixture, "tooling", "executable-benchmark-runner.mjs")))
      .digest("hex");
    const resultsRoot = path.join(fixture, "benchmarks", "results");
    fs.mkdirSync(resultsRoot, { recursive: true });
    const candidate = path.join(resultsRoot, "candidate.json");
    fs.writeFileSync(candidate, JSON.stringify(rustRecord, null, 2) + "\n");

    assert.equal(await main(["record", candidate], {
      root: fixture,
      gitState: { commit: rustRecord.provenance.commit, dirty: false },
    }), 0);
    assert.equal(fs.existsSync(candidate), false, "successful record must consume the local candidate");
    assert.equal(fs.existsSync(resultsRoot), false, "successful record must remove an empty local results directory");
    const published = loadExecutableDocuments(fixture);
    assert.ok(published.history.records.some((reference) => reference.id === rustRecord.id));
  } finally {
    fs.rmSync(fixture, { recursive: true, force: true });
  }
});
