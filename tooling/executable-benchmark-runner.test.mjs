import assert from "node:assert/strict";
import test from "node:test";
import { mkdir, mkdtemp, readFile, rm, rmdir } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import {
  RESULTS_DIRECTORY,
  deriveSummary,
  parseBenchmarkArguments,
  publishRecord,
  resolveResultPath,
} from "./executable-benchmark-runner.mjs";

test("W benchmark arguments require an explicit output and fixed raw count", () => {
  assert.deepEqual(parseBenchmarkArguments([]), {
    target: "hello", output: undefined, warmup: 1, samples: 9, help: false,
  });
  assert.deepEqual(parseBenchmarkArguments(["--target", "hello", "--output", "benchmarks/results/hello-w.local.json", "--warmup", "2", "--samples", "11"]), {
    target: "hello", output: "benchmarks/results/hello-w.local.json", warmup: 2, samples: 11, help: false,
  });
  assert.throws(() => parseBenchmarkArguments(["--target", "rust"]), /unsupported/);
  assert.throws(() => parseBenchmarkArguments(["--samples", "10"]), /odd/);
  assert.throws(() => parseBenchmarkArguments(["--warmup", "0"]), /between 1/);
});

test("summary arithmetic means use integer floor and preserve zero CPU", () => {
  const raw = Array.from({ length: 9 }, (_, index) => ({
    wallNs: String(index + 1),
    cpuUserUs: "0",
    cpuSystemUs: "1",
    cpuTotalUs: "1",
    peakRssBytes: String(index + 100),
  }));
  const summary = deriveSummary(raw);
  assert.equal(summary.wallNs.median, "5");
  assert.equal(summary.wallNs.arithmeticMean, "5");
  assert.equal(summary.cpuUserUs.arithmeticMean, "0");
  assert.equal(summary.peakRssBytes.mad, "2");
});

test("publication is contained, atomic and refuses overwrite", async () => {
  await mkdir(RESULTS_DIRECTORY, { recursive: true });
  const directory = await mkdtemp(path.join(RESULTS_DIRECTORY, "w-executable-result-test-"));
  const output = path.join(directory, "record.json");
  try {
    const resolved = await resolveResultPath(output);
    const record = { kind: "executable-result", status: "recorded" };
    await publishRecord(resolved, record);
    assert.deepEqual(JSON.parse(await readFile(resolved, "utf8")), record);
    await assert.rejects(() => publishRecord(resolved, record));
    await assert.rejects(() => resolveResultPath(path.join(RESULTS_DIRECTORY, "..", "outside.json")), /contained/);
  } finally {
    await rm(directory, { recursive: true, force: true });
    await rmdir(RESULTS_DIRECTORY).catch((error) => {
      if (error?.code !== "ENOENT" && error?.code !== "ENOTEMPTY" && error?.code !== "EEXIST") throw error;
    });
  }
});
