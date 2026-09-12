import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import { ROOT, loadExecutableDocuments } from "./executable-benchmark-machine.mjs";
import { PROJECTION_PATH, formatBytes, formatNanoseconds, projectionPath, renderExecutableProjection, renderFromDisk } from "./executable-benchmark-docs.mjs";

const documents = loadExecutableDocuments();

test("generated projection is current, compact, and sourced only from the live catalog", async () => {
  const rendered = `${await renderFromDisk(ROOT)}\n`;
  assert.equal(fs.readFileSync(PROJECTION_PATH, "utf8"), rendered);
  const measuredCells = new Set(documents.catalog.bestMetrics.entries.map(
    (entry) => `${entry.workloadId}\0${entry.language}`,
  )).size;
  const maximumCompactLines = documents.catalog.workloads.length + measuredCells + 18;
  assert.ok(rendered.split(/\r?\n/u).length <= maximumCompactLines);
  assert.match(rendered, /Best values/u);
  assert.match(rendered, /\| Workload \| Language \| Target \| Runtime \| Artifact \| Compile p50 \| Run p50 \| Run p95 \| Peak RSS \| CPU mean \|/u);
  assert.match(rendered, /\| hello \| c \| Windows x64 \/ MSVC \| MSVC CRT DLL \| 9216 B/u);
  assert.match(rendered, /\| hello \| w \| Windows x64 \/ MSVC \| CRT-free \| 2560 B/u);
  assert.match(rendered, /Artifact size counts only the PE file\. It excludes imported runtime DLLs\./u);
  assert.match(rendered, /\[w\]\(\.\/executable\/hello\.w\)/u);
  assert.match(rendered, /\[w\]\(\.\.\/compiler\/seed-c\/fixtures\/restaurant-if\.w\)/u);
  assert.match(rendered, /\x7c process-handler-lifecycle \x7c integration-linkage \x7c/u);
  assert.match(rendered, /public-end-to-end/u);
  assert.doesNotMatch(rendered, /recordId|equivalenceKey|sha256:|historical-unverified|verified-clean|Execution witness/iu);
});

test("projection formatting and links remain deterministic", () => {
  assert.equal(formatNanoseconds("1000000000"), "1 s");
  assert.equal(formatNanoseconds("1000000"), "1 ms");
  assert.equal(formatBytes("3715072"), "3715072 B (3.54 MiB)");
  assert.equal(formatBytes("2560"), "2560 B (2.5 KiB)");
  assert.equal(projectionPath("benchmarks/executable/hello.w"), "./executable/hello.w");
  assert.equal(projectionPath("compiler/seed-c/fixtures/restaurant-if.w"), "../compiler/seed-c/fixtures/restaurant-if.w");
  const copy = structuredClone(documents.catalog);
  assert.equal(renderExecutableProjection({ catalog: copy }), renderExecutableProjection({ catalog: documents.catalog }));
});
