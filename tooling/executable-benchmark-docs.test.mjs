import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import { EXECUTABLE_SUITE_DEFAULT_PLATFORMS, EXECUTABLE_SUITE_RECEIPT_SCHEMA, ROOT, executableCatalogFileDigest, loadExecutableDocuments, selectExecutableSuiteLanes } from "./executable-benchmark-machine.mjs";
import { PROJECTION_PATH, currentSuiteReceipt, formatBytes, formatNanoseconds, projectionPath, renderExecutableProjection, renderFromDisk } from "./executable-benchmark-docs.mjs";

const documents = loadExecutableDocuments();

test("generated projection is current, compact, and sourced only from the live catalog", async () => {
  const rendered = `${await renderFromDisk(ROOT)}\n`;
  const suiteReceipt = await currentSuiteReceipt(documents.catalog, ROOT);
  assert.equal(fs.readFileSync(PROJECTION_PATH, "utf8"), rendered);
  const measuredPlatformCells = new Set(documents.catalog.bestMetrics.entries.map(
    (entry) => `${entry.workloadId}\0${entry.language}\0${entry.platformTarget}`,
  )).size;
  const maximumCompactLines = documents.catalog.workloads.length + measuredPlatformCells + 300;
  assert.ok(rendered.split(/\r?\n/u).length <= maximumCompactLines);
  if (suiteReceipt) {
    assert.match(rendered, /## Latest executable suite\n\n\*\*(?:Full suite|Filtered: [^*]+)\*\* · \d+\/\d+ lanes passed/u);
  } else {
    assert.match(rendered, /## Latest executable suite\n\nNo successful suite receipt matches this catalog yet\./u);
  }
  assert.match(rendered, /### Integer semantics[\s\S]*\[integer-wrapping \(W\)\]\([^)]*\)[^\n]*Not measured/u,
    "one compact per-family table includes linked examples and current measurement status");
  assert.match(rendered, /\| \[integer-shift-semantics \(W\)\]\(\.\/executable\/integer_shift_semantics\.w\), \[integer-shift-semantics \(C\)\]\(\.\/executable\/integer_shift_semantics\.c\), \[integer-shift-semantics \(Rust\)\]\(\.\/executable\/integer_shift_semantics\.rs\) \| Not measured \|/u,
    "an unmeasured workload appears once with language links and em dashes");
  for (const [workloadId, language] of [
    ["hello-platform-minimal", "w"], ["hello-platform-minimal", "c"], ["hello-platform-minimal", "rust"],
    ["process-arguments-ordering", "w"], ["process-enum-payload", "w"],
  ]) {
    const hasMetrics = documents.catalog.bestMetrics.entries.some((entry) =>
      entry.workloadId === workloadId && entry.language === language);
    const displayLanguage = language === "w" ? "W" : language === "rust" ? "Rust" : "C";
    const row = new RegExp(`^\\| \\[${workloadId} \\(${displayLanguage}\\)\\]`, "mu");
    assert.equal(row.test(rendered), hasMetrics,
      `${workloadId}/${language} projection rows must track current live measurements`);
  }
  assert.match(rendered, /\| Example \| System \/ lane \| Language \| Binary \| Compile p50 \| Execution p50 \| Execution p95 \| CPU mean \| Peak memory \|/u);
  assert.match(rendered, /\| \[hello \(C\)\].*Windows · CRT \| C \|/u);
  assert.match(rendered, /\| \[hello \(W\)\].*Windows · no CRT \(recipe-derived\) \| W \|/u);
  for (const language of ["w", "c", "rust"]) {
    const displayLanguage = language === "w" ? "W" : language === "rust" ? "Rust" : "C";
    const row = new RegExp(`^\\| \\[hello-platform-minimal-pie \\(${displayLanguage}\\)\\].*WSL · no CRT \\(recipe-derived\\) · diagnostic · PIE \\| ${displayLanguage} \\| [0-9]`, "mu");
    assert.match(rendered, row, `the current PIE ${language} measurements must be projected`);
  }
  assert.match(rendered, /^\| \[hello-platform-minimal \(W\)\].*WSL · no CRT \(recipe-derived\) · diagnostic · non-PIE \| W \|/mu,
    "the new W measurement is clearly labeled as non-PIE");
  assert.match(rendered, /^\| \[hello-platform-minimal \(C\)\].*WSL · no CRT \(recipe-derived\) · diagnostic · non-PIE \| C \|/mu,
    "existing freestanding C measurements are clearly labeled as non-PIE");
  assert.equal(documents.catalog.bestMetrics.entries.filter((entry) => entry.workloadId === "hello-platform-minimal-pie").length, 18,
    "all three measured PIE lanes publish complete current benchmark cells");
  assert.match(rendered, /Toolchain identity and recipe remain lane-specific/u);
  assert.match(rendered, /does not imply one suite-wide compiler version/u);
  assert.match(rendered, /short toolchain\/recipe-digest marker keeps their metric rows distinct/u);
  assert.match(rendered, /emitted imports and exact dependency closure are not receipted/u);
  const partialWOnly = documents.catalog.workloads.filter((workload) =>
    workload.benchmarkStatus === "partial-exploratory-ready" &&
    new Set(workload.sources.map((source) => source.language)).size === 1 &&
    workload.sources.every((source) => source.language === "w"),
  );
  assert.ok(partialWOnly.length > 0);
  assert.ok(partialWOnly.every((workload) => rendered.includes(`${workload.id} (W)`)));
  assert.doesNotMatch(rendered, /\[composition \(/u, "planned workloads stay out of the projection");
  assert.match(rendered, /fresh-process invocations/u);
  assert.match(rendered, /sampling policy, and recipe details: \[benchmark README\]/u);
  assert.match(rendered, /\[hello \(W\)\]\(\.\/executable\/hello\.w\)/u);
  assert.match(rendered, /\[branch \(W\)\]\(\.\.\/compiler\/seed-c\/fixtures\/if\.w\)/u);
  assert.match(rendered, /\[process-handler-lifecycle \(W\)\]/u);
  assert.doesNotMatch(rendered, /structureClass|source-backed|not-performance-ready|benchmarkStatus/u);
  assert.doesNotMatch(rendered, /recordId|equivalenceKey|sha256:|historical-unverified|verified-clean|Execution witness/iu);
});

test("projection formatting and links remain deterministic", () => {
  assert.equal(formatNanoseconds("1000000000"), "1 s");
  assert.equal(formatNanoseconds("1000000"), "1 ms");
  assert.equal(formatBytes("3715072"), "3715072 B (3.54 MiB)");
  assert.equal(formatBytes("2560"), "2560 B (2.5 KiB)");
  assert.equal(projectionPath("benchmarks/executable/hello.w"), "./executable/hello.w");
  assert.equal(projectionPath("compiler/seed-c/fixtures/if.w"), "../compiler/seed-c/fixtures/if.w");
  const copy = structuredClone(documents.catalog);
  assert.equal(renderExecutableProjection({ catalog: copy }), renderExecutableProjection({ catalog: documents.catalog }));

  const withSections = structuredClone(documents.catalog);
  const helloCArtifact = withSections.bestMetrics.entries.find((entry) =>
    entry.workloadId === "hello" && entry.language === "c" && entry.metric === "artifact-size");
  delete helloCArtifact.peLayout;
  assert.match(renderExecutableProjection({ catalog: withSections }), /\| \[hello \(C\)\].*Windows · CRT \| C \| [^|]+ \|/u);
});

test("projection publishes only a current matching suite receipt and summarizes it without provenance noise", async () => {
  const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), "w-suite-docs-test-"));
  try {
    const benchmarks = path.join(tempRoot, "benchmarks");
    fs.mkdirSync(benchmarks, { recursive: true });
    fs.writeFileSync(path.join(benchmarks, "executable-catalog.json"), `${JSON.stringify(documents.catalog, null, 2)}\n`);
    const suiteReceiptPath = path.join(benchmarks, "executable-suite-current.json");
    assert.equal(await currentSuiteReceipt(documents.catalog, tempRoot), undefined, "no file means no historical or invented run summary");
    const lanes = selectExecutableSuiteLanes(documents.catalog).map((lane) => {
      const workload = documents.catalog.workloads.find((item) => item.id === lane.workloadId);
      const source = workload.sources.find((item) => item.language === lane.language && item.platformTarget === lane.platformTarget);
      return { ...lane, status: "passed", toolchain: `compiler-${lane.language}`, recipe: source.recipe, toolchainDigest: `sha256:${"a".repeat(64)}` };
    });
    const receipt = {
      $schema: "./executable-benchmark.schema.json",
      schema: EXECUTABLE_SUITE_RECEIPT_SCHEMA,
      kind: "executable-suite-current",
      status: "current",
      mode: "full",
      platforms: [...EXECUTABLE_SUITE_DEFAULT_PLATFORMS],
      catalogDigest: executableCatalogFileDigest(tempRoot),
      observedAt: "2026-09-22T12:00:00.000Z",
      durationMs: 65_000,
      laneCounts: { total: lanes.length, passed: lanes.length, failed: 0, skipped: 0 },
      lanes,
    };
    fs.writeFileSync(suiteReceiptPath, `${JSON.stringify(receipt, null, 2)}\n`);
    assert.deepEqual(await currentSuiteReceipt(documents.catalog, tempRoot), receipt);
    const rendered = renderExecutableProjection({ catalog: documents.catalog, root: tempRoot, suiteReceipt: receipt });
    assert.ok(rendered.includes(`**Full suite** · ${lanes.length}/${lanes.length} lanes passed · 0 failed · 0 skipped · 1m 5s.`));
    assert.ok(renderExecutableProjection({ catalog: documents.catalog, root: tempRoot,
      suiteReceipt: { ...receipt, durationMs: 65_600 } }).includes("1m 6s."));
    assert.doesNotMatch(rendered, /sha256:|toolchainDigest|compiler-w/u);

    const stale = { ...receipt, catalogDigest: `sha256:${"b".repeat(64)}` };
    fs.writeFileSync(suiteReceiptPath, `${JSON.stringify(stale, null, 2)}\n`);
    assert.equal(await currentSuiteReceipt(documents.catalog, tempRoot), undefined, "stale receipt is hidden rather than attributed to the changed catalog");
  } finally {
    fs.rmSync(tempRoot, { recursive: true, force: true });
  }
});

test("projection collapses same-build categories per platform without pooling platform lanes", () => {
  const compact = structuredClone(documents.catalog);
  const runtime = compact.bestMetrics.entries.find((entry) =>
    entry.workloadId === "hello" && entry.language === "rust" && entry.platformTarget === "windows-x64" && entry.metric === "run-wall-time");
  assert.ok(runtime);
  const lowerWindows = structuredClone(runtime);
  lowerWindows.id = "synthetic-lower-windows-runtime";
  lowerWindows.categoryId = "category-" + "a".repeat(64);
  lowerWindows.value = "1";
  compact.bestMetrics.entries.push(lowerWindows);

  const linux = structuredClone(runtime);
  linux.id = "synthetic-linux-runtime";
  linux.categoryId = "category-" + "b".repeat(64);
  linux.platformTarget = "linux-x64";
  linux.artifactTarget = "x86_64-unknown-linux-gnu";
  linux.abi = linux.artifactTarget;
  linux.toolchain = "rustc-alternate-linux";
  linux.value = "2";
  compact.bestMetrics.entries.push(linux);

  const rendered = renderExecutableProjection({ catalog: compact });
  const lines = rendered.split(/\r?\n/u);
  const windowsRows = lines.filter((line) => line.includes("hello (Rust)") && line.includes("Windows · CRT"));
  const linuxRows = lines.filter((line) => line.includes("hello (Rust)") && line.includes("Linux"));
  assert.equal(windowsRows.length, 1, "Windows categories must collapse to one human row");
  assert.match(windowsRows[0], /\| 1 ns \|/u, "the lower Windows cell must win");
  assert.equal(linuxRows.length, 1, "Linux remains an independent human row");
  assert.match(linuxRows[0], /\| 2 ns \|/u, "Linux values must not pool with Windows");

  const wsl = structuredClone(runtime);
  wsl.id = "synthetic-wsl-runtime";
  wsl.categoryId = "category-" + "c".repeat(64);
  wsl.platformTarget = "linux-wsl-x64";
  wsl.artifactTarget = "x86_64-unknown-linux-gnu";
  wsl.abi = wsl.artifactTarget;
  wsl.toolchain = "rustc-alternate-wsl";
  wsl.host = "linux-wsl2-microsoft-standard-wsl2-host-a";
  wsl.value = "3";
  const otherWsl = structuredClone(wsl);
  otherWsl.id = "synthetic-wsl-runtime-other-host";
  otherWsl.categoryId = "category-" + "d".repeat(64);
  otherWsl.toolchain = "rustc-alternate-wsl-other";
  otherWsl.host = "linux-wsl2-microsoft-standard-wsl2-host-b";
  otherWsl.value = "4";
  compact.bestMetrics.entries.push(wsl, otherWsl);
  const wslRendered = renderExecutableProjection({ catalog: compact });
  const wslRows = wslRendered.split(/\r?\n/u).filter((line) => line.includes("hello (Rust)") && line.includes("WSL · CRT · diagnostic"));
  assert.equal(wslRows.length, 2, "WSL categories remain partitioned by host");
  assert.match(wslRows[0], /\| 3 ns \|/u, "WSL host rows must retain their own value");
  assert.match(wslRows[1], /\| 4 ns \|/u, "a second WSL host must not be pooled into the first");
});

test("projection keeps toolchain and recipe lanes intact instead of mixing their per-metric bests", () => {
  const catalog = structuredClone(documents.catalog);
  const baseEntries = catalog.bestMetrics.entries.filter((entry) =>
    entry.workloadId === "hello" && entry.language === "w" &&
    entry.platformTarget === "linux-wsl-x64" && ["artifact-size", "run-wall-time"].includes(entry.metric));
  const baseArtifact = baseEntries.find((entry) => entry.metric === "artifact-size");
  const baseRuntime = baseEntries.find((entry) => entry.metric === "run-wall-time");
  assert.ok(baseArtifact);
  assert.ok(baseRuntime);
  assert.equal(baseArtifact.value, "1712");
  assert.equal(baseRuntime.value, "182931");

  // Synthetic lane A has the smaller runtime but a larger artifact.
  baseArtifact.value = "2096";
  delete baseArtifact.elfLayout;

  // Synthetic lane B carries the smaller artifact but slower execution, with
  // distinct compiler/recipe digests representing a different hardening build.
  const alternate = [baseArtifact, baseRuntime].map((entry) => {
    const cloned = structuredClone(entry);
    cloned.id = `synthetic-alternate-${entry.metric}`;
    cloned.categoryId = `category-${"a".repeat(64)}`;
    cloned.toolchain = "w-public-build-release-alternate";
    cloned.recipe = "public-w-build-release-hardened";
    cloned.recipeClass = "hello-release-hardened";
    cloned.provenance = {
      ...cloned.provenance,
      toolchainDigest: `sha256:${"b".repeat(64)}`,
      recipeDigest: `sha256:${"c".repeat(64)}`,
    };
    if (cloned.metric === "artifact-size") cloned.value = "1712";
    if (cloned.metric === "run-wall-time") cloned.value = "240000";
    return cloned;
  });
  catalog.bestMetrics.entries.push(...alternate);

  const rows = renderExecutableProjection({ catalog }).split(/\r?\n/u)
    .filter((line) => line.includes("hello (W)") && line.includes("WSL · no CRT (recipe-derived) · diagnostic"));
  assert.equal(rows.length, 2, "distinct build identities must produce separate WSL rows");
  const largerArtifact = rows.find((line) => line.includes("2096 B"));
  const smallerArtifact = rows.find((line) => line.includes("1712 B"));
  assert.ok(largerArtifact);
  assert.ok(smallerArtifact);
  assert.match(largerArtifact, /\| 182\.931 µs \|/u, "the faster runtime stays with its own 2,096-byte artifact");
  assert.match(smallerArtifact, /\| 240 µs \|/u, "the slower runtime stays with its own 1,712-byte artifact");
  assert.match(largerArtifact, /build public-w-build-release/u);
  assert.match(smallerArtifact, /build public-w-build-release-hardened/u);
  assert.doesNotMatch(smallerArtifact, /182\.931 µs/u, "the human row must not form a hybrid best-of-builds result");
});

test("projection never collapses runtime or comparability lanes within a platform", () => {
  const catalog = structuredClone(documents.catalog);
  const base = catalog.bestMetrics.entries.find((entry) =>
    entry.workloadId === "hello" && entry.language === "rust" &&
    entry.platformTarget === "windows-x64" && entry.metric === "run-wall-time");
  assert.ok(base);
  const freestanding = structuredClone(base);
  freestanding.id = "synthetic-freestanding-runtime";
  freestanding.categoryId = "category-" + "e".repeat(64);
  freestanding.runtimeClosure = { class: "freestanding", status: "unverified" };
  freestanding.value = "2";
  const verifiedFreestanding = structuredClone(freestanding);
  verifiedFreestanding.id = "synthetic-verified-freestanding-runtime";
  verifiedFreestanding.categoryId = "category-" + "d".repeat(64);
  verifiedFreestanding.runtimeClosure = { class: "freestanding", status: "verified" };
  verifiedFreestanding.value = "4";
  const contextual = structuredClone(base);
  contextual.id = "synthetic-contextual-eligibility";
  contextual.categoryId = "category-" + "f".repeat(64);
  contextual.comparability = "contextual-non-ranking-private-composite";
  contextual.eligibility = "exploratory-private-composite";
  contextual.value = "3";
  catalog.bestMetrics.entries.push(freestanding, verifiedFreestanding, contextual);

  const rows = renderExecutableProjection({ catalog }).split(/\r?\n/u)
    .filter((line) => line.includes("hello (Rust)") && line.includes("Windows"));
  assert.equal(rows.length, 4, "distinct runtime and comparability lanes remain separate rows");
  assert.ok(rows.some((line) => line.includes("Windows · CRT")));
  assert.ok(rows.some((line) => line.includes("Windows · no CRT (recipe-derived)")));
  assert.ok(rows.some((line) => line.includes("Windows · no CRT |")), "verified freestanding closure retains its unqualified label");
  assert.ok(rows.some((line) => line.includes("Windows · CRT · private")));
});
