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
  formatNanoseconds,
  projectionPath,
  renderExecutableProjection,
  renderFromDisk,
} from "./executable-benchmark-docs.mjs";

test("generated executable projection is current and uses projection-relative links", async () => {
  const rendered = `${await renderFromDisk(ROOT)}\n`;
  assert.equal(fs.readFileSync(PROJECTION_PATH, "utf8"), rendered);
  assert.match(rendered, /\]\(\.\/executable\/hello\.w\)/u);
  assert.match(rendered, /\]\(\.\/history\/executables\/index\.json\)/u);
  assert.match(rendered, /\]\(\.\.\/compiler\/seed-c\/fixtures\/restaurant-if\.w\)/u);
  assert.match(rendered, /private Native0\/MLIR0 Windows source-to-PE/u);
  assert.match(rendered, /contextual\/non-ranking/u);
});

test("projection formatting and root-relative record lookup are deterministic", () => {
  assert.equal(formatNanoseconds("1000000000"), "1 s");
  assert.equal(formatNanoseconds("1000000"), "1 ms");
  assert.equal(projectionPath("benchmarks/executable/hello.w"), "./executable/hello.w");
  assert.equal(projectionPath("compiler/seed-c/fixtures/restaurant-if.w"), "../compiler/seed-c/fixtures/restaurant-if.w");

  const documents = loadExecutableDocuments();
  const temporaryRoot = fs.mkdtempSync(path.join(os.tmpdir(), "w-executable-projection-test-"));
  const recordPath = path.join(temporaryRoot, "benchmarks", "history", "executables");
  fs.mkdirSync(recordPath, { recursive: true });
  fs.writeFileSync(path.join(recordPath, "record.json"), JSON.stringify({
    workloadId: "hello",
    language: "w",
    compile: { summary: { wallNs: { median: "1000000" } } },
    run: { summary: { wallNs: { median: "2000000" }, peakRssBytes: { median: "4096" } } },
    artifact: { sizeBytes: "2048" },
  }));
  try {
    const rendered = renderExecutableProjection({
      ...documents,
      history: { records: [{ id: "record", path: "record.json", digest: "sha256:" + "0".repeat(64) }] },
      root: temporaryRoot,
    });
    assert.match(rendered, /compile median 1 ms; run median 2 ms; peak RSS 4096 B \(4 KiB\); artifact 2048 B \(2 KiB\)/u);
  } finally {
    fs.rmSync(temporaryRoot, { recursive: true, force: true });
  }
});
