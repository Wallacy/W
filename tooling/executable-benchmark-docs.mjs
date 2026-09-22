import { randomUUID } from "node:crypto";
import { lstat, mkdir, readFile, rename, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import {
  executableWorkloadHasRunner,
  EXECUTABLE_PLATFORM_TARGET_LINUX,
  EXECUTABLE_PLATFORM_TARGET_LINUX_WSL,
  EXECUTABLE_PLATFORM_TARGET_WINDOWS,
  HELLO_PLATFORM_MINIMAL_WORKLOAD_ID,
  EXECUTABLE_WORKLOAD_FAMILY_IDS,
  ROOT,
  loadExecutableDocuments,
  validateExecutableCatalog,
  validateExecutableBestMetrics,
} from "./executable-benchmark-machine.mjs";
import { platformMinimalRecipeExamples } from "./executable-release-recipes.mjs";

export const PROJECTION_PATH = path.resolve(ROOT, "benchmarks", "EXECUTABLES.md");

function slash(value) {
  return String(value).replaceAll("\\", "/");
}

function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

export function projectionPath(repositoryPath) {
  const normalized = slash(repositoryPath);
  return normalized.startsWith("benchmarks/") ? `./${normalized.slice("benchmarks/".length)}` : `../${normalized}`;
}

function isContained(parent, candidate) {
  const relative = path.relative(path.resolve(parent), path.resolve(candidate));
  return relative === "" || (relative !== ".." && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative));
}

async function assertNoReparseAncestors(candidate, stopAt) {
  let current = path.resolve(candidate);
  const stop = path.resolve(stopAt);
  while (isContained(stop, current)) {
    try {
      const stats = await lstat(current);
      if (stats.isSymbolicLink()) throw new Error(`generated path contains a symbolic link: ${current}`);
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
    if (current === stop) break;
    const parent = path.dirname(current);
    if (parent === current) break;
    current = parent;
  }
}

export async function writeAtomicFile(filePath, contents, stopAt = path.dirname(filePath)) {
  const parent = path.dirname(filePath);
  await mkdir(parent, { recursive: true });
  await assertNoReparseAncestors(filePath, stopAt);
  try {
    const stats = await lstat(filePath);
    if (!stats.isFile() || stats.isSymbolicLink()) throw new Error(`generated target must be a regular non-link file: ${filePath}`);
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
  const temporary = path.join(parent, `.${path.basename(filePath)}.${process.pid}-${randomUUID()}.tmp`);
  try {
    await writeFile(temporary, contents, { flag: "wx" });
    await assertNoReparseAncestors(temporary, stopAt);
    await rename(temporary, filePath);
  } catch (error) {
    await rm(temporary, { force: true });
    throw error;
  }
}

function jsonPathLink(relativePath, label = relativePath) {
  return `[${label}](${slash(relativePath)})`;
}

export function formatNanoseconds(value) {
  const ns = BigInt(value);
  if (ns >= 1_000_000_000n) {
    const fraction = String(ns % 1_000_000_000n).padStart(9, "0").replace(/0+$/u, "");
    return fraction.length === 0 ? `${ns / 1_000_000_000n} s` : `${ns / 1_000_000_000n}.${fraction} s`;
  }
  if (ns >= 1_000_000n) {
    const fraction = String(ns % 1_000_000n).padStart(6, "0").replace(/0+$/u, "");
    return fraction.length === 0 ? `${ns / 1_000_000n} ms` : `${ns / 1_000_000n}.${fraction} ms`;
  }
  if (ns >= 1_000n) {
    const fraction = String(ns % 1_000n).padStart(3, "0").replace(/0+$/u, "");
    return fraction.length === 0 ? `${ns / 1_000n} µs` : `${ns / 1_000n}.${fraction} µs`;
  }
  return `${ns} ns`;
}

function formatMicroseconds(value) {
  const us = BigInt(value);
  if (us >= 1_000_000n) {
    const fraction = String(us % 1_000_000n).padStart(6, "0").replace(/0+$/u, "");
    return fraction.length === 0 ? `${us / 1_000_000n} s` : `${us / 1_000_000n}.${fraction} s`;
  }
  if (us >= 1_000n) {
    const fraction = String(us % 1_000n).padStart(3, "0").replace(/0+$/u, "");
    return fraction.length === 0 ? `${us / 1_000n} ms` : `${us / 1_000n}.${fraction} ms`;
  }
  return `${us} µs`;
}

export function formatBytes(value) {
  const bytes = BigInt(value);
  if (bytes >= 1_048_576n) return `${bytes} B (${(Number(bytes) / 1_048_576).toFixed(2)} MiB)`;
  if (bytes >= 1_024n) return `${bytes} B (${(Number(bytes) / 1_024).toFixed(1)} KiB)`;
  return `${bytes} B`;
}

function formatValue(entry) {
  if (["compile-latency", "run-wall-time", "run-wall-p95"].includes(entry.metric)) return formatNanoseconds(entry.value);
  if (entry.metric === "cpu-time") return formatMicroseconds(entry.value);
  return formatBytes(entry.value);
}

function sourceLinks(workload) {
  const sources = workload.sources?.filter((source, index, all) =>
    all.findIndex((candidate) => candidate.language === source.language &&
      candidate.path === source.path) === index);
  return sources?.length
    ? sources.map((source) => jsonPathLink(projectionPath(source.path), source.language)).join(", ")
    : "—";
}

function bestSort(left, right) {
  return compareText(String(left?.workloadId ?? ""), String(right?.workloadId ?? "")) ||
    compareText(String(left?.language ?? ""), String(right?.language ?? "")) ||
    compareText(String(left?.platformTarget ?? ""), String(right?.platformTarget ?? "")) ||
    compareText(String(left?.artifactTarget ?? ""), String(right?.artifactTarget ?? "")) ||
    compareText(String(left?.toolchain ?? ""), String(right?.toolchain ?? "")) ||
    compareText(String(left?.host ?? ""), String(right?.host ?? "")) ||
    compareText(String(left?.recipe ?? ""), String(right?.recipe ?? "")) ||
    compareText(String(left?.metric ?? ""), String(right?.metric ?? "")) ||
    compareText(String(left?.id ?? ""), String(right?.id ?? ""));
}

function hasLayout(entry) {
  return entry?.peLayout !== undefined || entry?.elfLayout !== undefined;
}

function bestProjectionEntry(left, right) {
  if (!left) return right;
  if (!right) return left;
  const leftValue = BigInt(left.value);
  const rightValue = BigInt(right.value);
  if (leftValue !== rightValue) return leftValue < rightValue ? left : right;
  if (hasLayout(left) !== hasLayout(right)) return hasLayout(left) ? left : right;
  return bestSort(left, right) <= 0 ? left : right;
}

function categoryRows(entries) {
  const groups = new Map();
  for (const entry of entries) {
    // Keep the projection compact by collapsing only within one platform
    // lane. The machine catalog remains category-partitioned by toolchain and
    // recipe; each displayed metric is independently selected below.
    const hostPartition = entry.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL
      ? `\u0000${entry.host ?? ""}`
      : "";
    const key = `${entry.workloadId}\u0000${entry.language}\u0000${entry.platformTarget}${hostPartition}`;
    const group = groups.get(key) ?? { entry, metrics: new Map() };
    group.entry = bestSort(group.entry, entry) <= 0 ? group.entry : entry;
    group.metrics.set(entry.metric, bestProjectionEntry(group.metrics.get(entry.metric), entry));
    groups.set(key, group);
  }
  return [...groups.values()].sort((left, right) => bestSort(left.entry, right.entry));
}

function metricCell(group, metric) {
  const entry = group.metrics.get(metric);
  return entry ? formatValue(entry) : "—";
}

function uniqueSection(layout, name) {
  const matches = Array.isArray(layout?.sections) ? layout.sections.filter((section) => section?.name === name) : [];
  return matches.length === 1 ? matches[0] : undefined;
}

function artifactCell(group) {
  const entry = group.metrics.get("artifact-size");
  if (!entry) return "—";
  return formatValue(entry);
}

function sectionVirtualSizeCell(group, name) {
  const entry = group.metrics.get("artifact-size");
  const section = uniqueSection(entry?.peLayout, name);
  return section?.virtualSize ?? "—";
}

function sectionSizeCell(group, name) {
  const entry = group.metrics.get("artifact-size");
  const section = uniqueSection(entry?.elfLayout, name);
  return section?.sizeBytes ?? "—";
}

function tableHeader(platformTarget) {
  const sectionLabels = platformTarget === EXECUTABLE_PLATFORM_TARGET_WINDOWS
    ? [".text B", ".rdata B"]
    : ["ELF .text B", "ELF .rodata B"];
  return [
    `| Workload | Language | Target | Runtime | Artifact | ${sectionLabels[0]} | ${sectionLabels[1]} | Compile p50 | Run p50 | Run p95 | Peak RSS | CPU mean |`,
    "| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
  ];
}

function projectionWorkload(workload) {
  return workload?.status === "source-oracle-ready" &&
    workload.sourceReadiness === "source-and-oracle-ready" &&
    executableWorkloadHasRunner(workload);
}

function targetLabel(entry) {
  if (entry.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) return "Linux x64 / WSL2";
  if (entry.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX) return "Linux x64 / GNU";
  if (entry.artifactTarget.endsWith("windows-msvc")) return "Windows x64 / MSVC";
  if (entry.artifactTarget.endsWith("w64-mingw32")) return "Windows x64 / MinGW";
  return entry.artifactTarget;
}

function runtimeLabel(entry) {
  const runtimeClass = entry.runtimeClosure?.class;
  const label = runtimeClass === "freestanding" ? "Freestanding"
    : runtimeClass === "hosted-crt" ? "Hosted CRT"
      : runtimeClass === "instrumentation" ? "Instrumentation runtime"
        : "Runtime class unknown";
  return `${label} (${entry.runtimeClosure?.status ?? "unverified"})`;
}

const FAMILY_LABELS = Object.freeze({
  hello: "Hello and platform entry",
  "control-flow": "Control flow and interpolation",
  async: "Asynchronous scheduling",
  composition: "Composition, enums, and dispatch",
  "integer-semantics": "Integer semantics",
  "floating-point": "Floating-point semantics",
  mutation: "Mutation and linear values",
  process: "Process entry and arguments",
  modules: "Module graph",
});

export function renderExecutableProjection({ catalog, root = ROOT } = {}) {
  if (!catalog?.bestMetrics) throw new TypeError("catalog with bestMetrics is required");
  const entries = [...catalog.bestMetrics.entries].sort(bestSort);
  const rows = categoryRows(entries);
  const lines = [
    "<!-- generated by tooling/executable-benchmark-docs.mjs; do not edit -->",
    "# Executable benchmarks",
    "",
    "Current portable-release values. Lower is better; `—` means no published measurement.",
    "",
    "## Workloads",
  ];
  const projectedWorkloads = catalog.workloads.filter(projectionWorkload);
  for (const family of EXECUTABLE_WORKLOAD_FAMILY_IDS) {
    const familyWorkloads = projectedWorkloads.filter((workload) => workload.family === family);
    if (familyWorkloads.length === 0) continue;
    lines.push(
      "",
      `### ${FAMILY_LABELS[family] ?? family}`,
      "",
      "| Workload | Class | Sources | Oracle | Benchmark |",
      "| --- | --- | --- | --- | --- |",
    );
    for (const workload of familyWorkloads) {
      lines.push(`| ${workload.id} | ${workload.structureClass} | ${sourceLinks(workload)} | ${workload.oracle.status} | ${workload.benchmarkStatus} |`);
    }
  }
  const platformMinimalHello = catalog.workloads.find((workload) => workload.id === HELLO_PLATFORM_MINIMAL_WORKLOAD_ID);
  if (platformMinimalHello) {
    lines.push(
      "",
      "## Platform-minimal Hello correctness comparison",
      "",
      "This lane preserves the exact `Hello, world!\\n` / exit `0` oracle while C23 and Rust 2024 recipes request no CRT or standard library, write through the OS boundary, and exit directly. Windows recipes link Kernel32; the ELF correctness checker rejects `PT_INTERP` or `DT_NEEDED`, but neither exact dependency list is persisted in current results. The W row reuses the public W Hello build. This is platform-minimal correctness/context evidence, not an idiomatic C/Rust baseline or a language ranking.",
      "",
      "Reproducible release commands (the runner resolves the installed compiler paths; `<source>` and `<artifact>` are per-sample temporary paths):",
      "",
    );
    for (const example of platformMinimalRecipeExamples()) {
      lines.push(`- ${example.language}, ${example.platform}: \`${example.command}\``);
    }
    lines.push(
      "",
      "The commands request C23 `-O3`/full LTO, freestanding/no-builtin/no-stack-protector/no-unwind-table code generation, dead-section elimination and stripping; Rust uses edition 2024 `no_std`/`no_main`, `-C opt-level=3`, fat LTO, one codegen unit, aborting panics and stripped symbols (disabling unwind tables on Linux, where supported by the target ABI). Windows requests the Kernel32 OS boundary and no CRT; Linux requests `_start`, static linking, section GC, and no build ID. These recipe flags do not upgrade the unverified runtime-closure status of existing records. WSL rows remain same-physical-hardware diagnostics only.",
      "",
    );
  }
  lines.push("", "## Best values");
  for (const [platformTarget, label] of [
    [EXECUTABLE_PLATFORM_TARGET_WINDOWS, "Windows x64"],
    [EXECUTABLE_PLATFORM_TARGET_LINUX, "Linux x64"],
    [EXECUTABLE_PLATFORM_TARGET_LINUX_WSL, "Linux x64 via WSL2"],
  ]) {
    const platformRows = rows.filter((group) => group.entry.platformTarget === platformTarget);
    lines.push("", `### ${label}`);
    lines.push("", ...tableHeader(platformTarget));
    if (platformRows.length === 0) {
      lines.push("", platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL
        ? "No Linux x64 via WSL2 same-physical-hardware diagnostic measurements are published."
        : `No native ${label} measurements are published.`);
      continue;
    }
    for (const group of platformRows) {
      const entry = group.entry;
      const sections = platformTarget === EXECUTABLE_PLATFORM_TARGET_WINDOWS
        ? [sectionVirtualSizeCell(group, ".text"), sectionVirtualSizeCell(group, ".rdata")]
        : [sectionSizeCell(group, ".text"), sectionSizeCell(group, ".rodata")];
      lines.push(`| ${entry.workloadId} | ${entry.language} | ${targetLabel(entry)} | ${runtimeLabel(entry)} | ${artifactCell(group)} | ${sections[0]} | ${sections[1]} | ${metricCell(group, "compile-latency")} | ${metricCell(group, "run-wall-time")} | ${metricCell(group, "run-wall-p95")} | ${metricCell(group, "peak-working-set")} | ${metricCell(group, "cpu-time")} |`);
    }
  }
  lines.push(
    "",
    "Runtime labels are recipe-derived classes, not artifact dependency receipts. Every current row remains `unverified` for runtime closure because results do not record PE imports, ELF `DT_NEEDED` entries, or exact runtime provider/version. Current recipes select freestanding W and platform-minimal C/Rust paths, hosted-CRT public C/Rust paths, and a contextual hosted-CRT MinGW private composite. Do not compare rows across runtime-closure classes or treat an intended freestanding link as proof of emitted dependency closure.",
    "Artifact size counts only the emitted executable file. On Windows it excludes imported runtime DLLs. The private process-handler composite remains a Windows GCC/MinGW contextual lane. Native Linux records, when published, are kept in their own Linux x64 / GNU lane.",
    "Run p50/p95 measure one complete target-process invocation (launch, execution, and wait) per sample. Production samples use one native target-environment helper batch for each warmup/raw series: WSL initializes once, stages the ELF on WSL-native /tmp, and excludes wsl.exe startup and DrvFS access from every sample. The target process itself is intentionally fresh per sample, so these are product invocation costs, not in-process body-throughput numbers. A future persistent body lane must use a distinct protocol and never be merged with these cells.",
    "Do not compare Windows milliseconds with Linux/WSL microseconds as W-body speed. The Windows lane includes process creation, security, Job Object, scheduler, and accounting work; the WSL lane times the Linux executable from a Linux-native helper inside an already-running distribution with CLOCK_MONOTONIC and wait4. Compare regressions only within the same platform and runner lane.",
    "Each projection row is compact: every displayed metric chooses the lower value across pinned categories on that same platform, so cells may come from distinct toolchain/recipe categories. The machine catalog retains those category and provenance identities; no value is selected across platform sections. WSL rows remain host-partitioned and are never pooled across hosts.",
    "Linux x64 via WSL2 is Linux-target evidence on a Windows host, not native Linux support. It is accepted for same-host regression and same-physical-hardware diagnostics only; WSL values are not rankable across hosts. WSL provenance records the host mode, comparison purpose, and rankability explicitly.",
    "The Windows `.text B` and `.rdata B` columns are the unique PE sections' validated VirtualSize; VirtualSize includes padding and zero-fill and is not a useful-instruction count. Linux and WSL use explicitly labeled `ELF .text B` and `ELF .rodata B` columns containing the unique named ELF sections' `sh_size`; they are not byte-equivalent PE `.rdata` measurements. `—` means absent, ambiguous, or not measured. Linux ELF metadata is kept separate from PE metadata. Only source-backed workloads with a materialized source and runner-supported recipe appear here; planned/backlog entries remain in the catalog. CPU is the arithmetic mean of 101 fresh target-process counters; an all-zero estimate is omitted.",
    `Machine contract and provenance: ${jsonPathLink(projectionPath("benchmarks/executable-catalog.json"), "executable-catalog.json")}. Manual commands: [README](./README.md#manual-reproduction).`,
  );
  return lines.join("\n");
}

export async function renderFromDisk(root = ROOT) {
  const documents = loadExecutableDocuments(root);
  const errors = [
    ...validateExecutableCatalog(documents.catalog, documents, root),
    ...validateExecutableBestMetrics(documents.catalog.bestMetrics, documents.catalog),
  ];
  if (errors.length > 0) throw new Error(errors.join("\n"));
  return renderExecutableProjection({ catalog: documents.catalog, root });
}

export async function main(argv = process.argv.slice(2)) {
  if (argv.length !== 1 || !["--write", "--check"].includes(argv[0])) {
    console.error("usage: bun tooling/executable-benchmark-docs.mjs --write|--check");
    return 2;
  }
  const rendered = `${await renderFromDisk()}\n`;
  const current = await readFile(PROJECTION_PATH, "utf8").catch((error) => error?.code === "ENOENT" ? undefined : Promise.reject(error));
  if (argv[0] === "--check") {
    if (current !== rendered) {
      console.error(`executable benchmark projection is stale: ${PROJECTION_PATH}`);
      return 1;
    }
    console.log("executable benchmark projection: current");
    return 0;
  }
  await writeAtomicFile(PROJECTION_PATH, rendered, path.resolve(ROOT, "benchmarks"));
  console.log(`executable benchmark projection: wrote ${PROJECTION_PATH}`);
  return 0;
}

if (import.meta.main) process.exit(await main());
