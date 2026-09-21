import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import { ROOT, loadExecutableDocuments } from "./executable-benchmark-machine.mjs";
import { PROJECTION_PATH, formatBytes, formatNanoseconds, projectionPath, renderExecutableProjection, renderFromDisk } from "./executable-benchmark-docs.mjs";

const documents = loadExecutableDocuments();

test("generated projection is current, compact, and sourced only from the live catalog", async () => {
  const rendered = `${await renderFromDisk(ROOT)}\n`;
  assert.equal(fs.readFileSync(PROJECTION_PATH, "utf8"), rendered);
  const measuredPlatformCells = new Set(documents.catalog.bestMetrics.entries.map(
    (entry) => `${entry.workloadId}\0${entry.language}\0${entry.platformTarget}`,
  )).size;
  const maximumCompactLines = documents.catalog.workloads.length + measuredPlatformCells + 50;
  assert.ok(rendered.split(/\r?\n/u).length <= maximumCompactLines);
  assert.match(rendered, /Best values/u);
  assert.match(rendered, /### Windows x64/u);
  assert.match(rendered, /### Linux x64/u);
  assert.match(rendered, /### Linux x64 via WSL2/u);
  assert.match(rendered, /\| restaurant-main-dispatch \| w \| Linux x64 \/ WSL2 \| CRT-free \| 2768 B/u);
  assert.match(rendered, /\| restaurant-main-cardinality \| public-end-to-end \|/u);
  assert.match(rendered, /\| restaurant-shifts \| public-end-to-end \| \[w\]\(\.\.\/compiler\/seed-c\/fixtures\/restaurant-shifts\.w\), \[c\]\(\.\/executable\/restaurant_shifts\.c\), \[rust\]\(\.\/executable\/restaurant_shifts\.rs\) \| source-backed \| not-performance-ready \|/u);
  assert.doesNotMatch(rendered, /\| restaurant-shifts \| w \|/u,
    "stale shift measurements must not remain in the generated best-value tables");
  assert.match(rendered, /WSL values are not rankable across hosts\./u);
  assert.match(rendered, /\| Workload \| Language \| Target \| Runtime \| Artifact \| \.text B \| \.rdata B \| Compile p50 \| Run p50 \| Run p95 \| Peak RSS \| CPU mean \|/u);
  assert.match(rendered, /### Linux x64 via WSL2[\s\S]*\| Workload \| Language \| Target \| Runtime \| Artifact \| ELF \.text B \| ELF \.rodata B \|/u);
  assert.match(rendered, /\| hello \| c \| Windows x64 \/ MSVC \| MSVC CRT DLL \| [0-9]+ B/u);
  assert.match(rendered, /\| hello \| w \| Windows x64 \/ MSVC \| CRT-free \| [0-9]+ B/u);
  assert.match(rendered, /## Platform-minimal Hello correctness comparison/u);
  assert.match(rendered, /not an idiomatic C\/Rust baseline or a language ranking/u);
  assert.match(rendered, /clang --target=x86_64-unknown-linux-gnu[\s\S]*-nostdlib[\s\S]*--strip-all/u);
  assert.match(rendered, /rustc <source> --edition=2024[\s\S]*--target=x86_64-unknown-linux-gnu/u);
  const partialWOnly = documents.catalog.workloads.filter((workload) =>
    workload.benchmarkStatus === "partial-exploratory-ready" &&
    new Set(workload.sources.map((source) => source.language)).size === 1 &&
    workload.sources.every((source) => source.language === "w"),
  );
  assert.ok(partialWOnly.length > 0);
  assert.ok(partialWOnly.every((workload) => rendered.includes(`| ${workload.id} |`)));
  assert.doesNotMatch(rendered, /restaurant-composition/u, "planned workloads stay out of the projection");
  assert.match(rendered, /Artifact size counts only the emitted executable file\. On Windows it excludes imported runtime DLLs\./u);
  assert.match(rendered, /Run p50\/p95 measure one complete target-process invocation \(launch, execution, and wait\) per sample\./u);
  assert.match(rendered, /WSL initializes once, stages the ELF on WSL-native \/tmp, and excludes wsl\.exe startup and DrvFS access/u);
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

  const withSections = structuredClone(documents.catalog);
  const helloCArtifact = withSections.bestMetrics.entries.find((entry) =>
    entry.workloadId === "hello" && entry.language === "c" && entry.metric === "artifact-size");
  delete helloCArtifact.peLayout;
  assert.match(renderExecutableProjection({ catalog: withSections }), /\| hello \| c \| Windows x64 \/ MSVC \| MSVC CRT DLL \| [0-9]+ B[^|]*\| — \| — \|/u);
  helloCArtifact.peLayout = {
    fileAlignment: "512",
    sectionAlignment: "4096",
    sizeOfHeaders: "512",
    sections: [
      { name: ".text", virtualSize: "111", rawSize: "512" },
      { name: ".rdata", virtualSize: "222", rawSize: "512" },
    ],
  };
  const sectionRendered = renderExecutableProjection({ catalog: withSections });
  assert.match(sectionRendered, /\| hello \| c \| Windows x64 \/ MSVC \| MSVC CRT DLL \| [0-9]+ B[^|]*\| 111 \| 222 \|/u);
  helloCArtifact.peLayout.sections.push({ name: ".text", virtualSize: "333", rawSize: "512" });
  const ambiguousRendered = renderExecutableProjection({ catalog: withSections });
  assert.match(ambiguousRendered, /\| hello \| c \| Windows x64 \/ MSVC \| MSVC CRT DLL \| [0-9]+ B[^|]*\| — \| 222 \|/u);

  const helloWslArtifact = withSections.bestMetrics.entries.find((entry) =>
    entry.workloadId === "hello" && entry.language === "w" && entry.platformTarget === "linux-wsl-x64" && entry.metric === "artifact-size");
  assert.ok(helloWslArtifact);
  helloWslArtifact.elfLayout = {
    class: "ELF64",
    data: "little-endian",
    machine: "x86-64",
    type: "pie",
    sections: [
      { name: ".text", sizeBytes: "77" },
      { name: ".rodata", sizeBytes: "88" },
    ],
  };
  const linuxSectionRendered = renderExecutableProjection({ catalog: withSections });
  assert.match(linuxSectionRendered, /\| hello \| w \| Linux x64 \/ WSL2 \| CRT-free \| [0-9]+ B[^|]*\| 77 \| 88 \|/u);
});

test("projection collapses categories per platform without pooling platform lanes", () => {
  const compact = structuredClone(documents.catalog);
  const runtime = compact.bestMetrics.entries.find((entry) =>
    entry.workloadId === "hello" && entry.language === "rust" && entry.platformTarget === "windows-x64" && entry.metric === "run-wall-time");
  assert.ok(runtime);
  const lowerWindows = structuredClone(runtime);
  lowerWindows.id = "synthetic-lower-windows-runtime";
  lowerWindows.categoryId = "category-" + "a".repeat(64);
  lowerWindows.toolchain = "rustc-alternate-windows";
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
  const windowsRows = lines.filter((line) => line.startsWith("| hello | rust | Windows x64 /"));
  const linuxRows = lines.filter((line) => line.startsWith("| hello | rust | Linux x64 / GNU"));
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
  const wslRows = wslRendered.split(/\r?\n/u).filter((line) => line.startsWith("| hello | rust | Linux x64 / WSL2"));
  assert.equal(wslRows.length, 2, "WSL categories remain partitioned by host");
  assert.match(wslRows[0], /\| 3 ns \|/u, "WSL host rows must retain their own value");
  assert.match(wslRows[1], /\| 4 ns \|/u, "a second WSL host must not be pooled into the first");
});
