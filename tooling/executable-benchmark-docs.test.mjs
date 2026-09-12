import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import { ROOT, loadExecutableDocuments } from "./executable-benchmark-machine.mjs";
import { PROJECTION_PATH, formatBytes, formatNanoseconds, projectionPath, renderExecutableProjection, renderFromDisk } from "./executable-benchmark-docs.mjs";

test("generated projection is current, compact, and sourced only from the live catalog", async () => {
  const rendered = `${await renderFromDisk(ROOT)}\n`;
  assert.equal(fs.readFileSync(PROJECTION_PATH, "utf8"), rendered);
  assert.ok(rendered.split(/\r?\n/u).length <= 56);
  assert.match(rendered, /Best values/u);
  assert.match(rendered, /\| Workload \| Language \| Target \| Artifact \| Compile p50 \| Run p50 \| Run p95 \| Peak RSS \| CPU mean \|/u);
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
  const documents = loadExecutableDocuments();
  const copy = structuredClone(documents.catalog);
  assert.equal(renderExecutableProjection({ catalog: copy }), renderExecutableProjection({ catalog: documents.catalog }));
});
