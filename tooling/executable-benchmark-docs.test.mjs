import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import {
  ROOT,
  loadExecutableDocuments,
} from "./executable-benchmark-machine.mjs";
import {
  PROJECTION_PATH,
  formatBytes,
  formatNanoseconds,
  projectionPath,
  renderExecutableProjection,
  renderFromDisk,
} from "./executable-benchmark-docs.mjs";

test("generated executable projection is current and uses projection-relative links", async () => {
  const rendered = `${await renderFromDisk(ROOT)}\n`;
  assert.equal(fs.readFileSync(PROJECTION_PATH, "utf8"), rendered);
  assert.match(rendered, /\]\(\.\/executable\/hello\.w\)/u);
  assert.match(rendered, /\[history record\]\(\.\/history\/executables\/[0-9a-f]{64}\.json\)/u);
  assert.match(rendered, /\]\(\.\.\/compiler\/seed-c\/fixtures\/restaurant-if\.w\)/u);
  assert.match(rendered, /runner accepts W, C and Rust Hello sources/u);
  assert.match(rendered, /probes `-std=c23` and then `-std=c2x`/u);
  assert.match(rendered, /private Native0\/MLIR0 gate/u);
  assert.match(rendered, /Windows MLIR\/LLVM\/LLD chain/u);
  assert.match(rendered, /contextual-non-ranking/u);
});

test("projection formatting and root-relative record lookup are deterministic", () => {
  assert.equal(formatNanoseconds("1000000000"), "1 s");
  assert.equal(formatNanoseconds("1000000"), "1 ms");
  assert.equal(formatBytes("3715072"), "3715072 B (3.543 MiB)");
  assert.equal(formatBytes("2560"), "2560 B (2.5 KiB)");
  assert.equal(projectionPath("benchmarks/executable/hello.w"), "./executable/hello.w");
  assert.equal(projectionPath("compiler/seed-c/fixtures/restaurant-if.w"), "../compiler/seed-c/fixtures/restaurant-if.w");

  const documents = loadExecutableDocuments();
  const temporaryRoot = fs.mkdtempSync(path.join(os.tmpdir(), "w-executable-projection-test-"));
  const recordPath = path.join(temporaryRoot, "benchmarks", "history", "executables");
  fs.mkdirSync(recordPath, { recursive: true });
  fs.writeFileSync(path.join(recordPath, "record.json"), JSON.stringify({
    workloadId: "hello",
    language: "w",
    compile: { summary: { wallNs: { median: "1000000" }, cpuTotalUs: { median: "500" }, peakRssBytes: { median: "8192" } } },
    run: { summary: { wallNs: { median: "2000000" }, cpuTotalUs: { median: "0" }, peakRssBytes: { median: "4096" } } },
    artifact: { sizeBytes: "2048" },
    provenance: { commit: "1".repeat(40) },
    identity: { toolchain: "test-toolchain" },
  }));
  try {
    const rendered = renderExecutableProjection({
      ...documents,
      history: { records: [{ id: "record", path: "record.json", digest: "sha256:" + "0".repeat(64) }] },
      root: temporaryRoot,
    });
    assert.match(rendered, /compile median 1 ms \(CPU .*?, RSS .*?\); run median 2 ms \(CPU 0 ns, RSS 4096 B \(4 KiB\)\); artifact 2048 B \(2 KiB\)/u);
  } finally {
    fs.rmSync(temporaryRoot, { recursive: true, force: true });
  }
});
