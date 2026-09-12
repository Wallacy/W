import { randomUUID } from "node:crypto";
import { lstat, mkdir, readFile, rename, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import {
  ROOT,
  loadExecutableDocuments,
  validateExecutableCatalog,
  validateExecutableBestMetrics,
} from "./executable-benchmark-machine.mjs";

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
  return workload.sources?.length
    ? workload.sources.map((source) => jsonPathLink(projectionPath(source.path), source.language)).join(", ")
    : "—";
}

function bestSort(left, right) {
  return compareText(String(left?.workloadId ?? ""), String(right?.workloadId ?? "")) ||
    compareText(String(left?.language ?? ""), String(right?.language ?? "")) ||
    compareText(String(left?.metric ?? ""), String(right?.metric ?? "")) ||
    compareText(String(left?.id ?? ""), String(right?.id ?? ""));
}

function categoryRows(entries) {
  const groups = new Map();
  for (const entry of entries) {
    const key = `${entry.categoryId}\u0000${entry.workloadId}\u0000${entry.language}`;
    const group = groups.get(key) ?? { entry, metrics: new Map() };
    group.metrics.set(entry.metric, entry);
    groups.set(key, group);
  }
  return [...groups.values()].sort((left, right) => bestSort(left.entry, right.entry));
}

function metricCell(group, metric) {
  const entry = group.metrics.get(metric);
  return entry ? formatValue(entry) : "—";
}

function targetLabel(entry) {
  if (entry.artifactTarget.endsWith("windows-msvc")) return "Windows x64 / MSVC";
  if (entry.artifactTarget.endsWith("w64-mingw32")) return "Windows x64 / MinGW";
  return entry.artifactTarget;
}

function runtimeLabel(entry) {
  if (entry.artifactTarget.endsWith("w64-mingw32")) return "MinGW runtime";
  if (entry.language === "w" && entry.artifactTarget.endsWith("windows-msvc")) return "CRT-free";
  if (entry.language === "c" && entry.artifactTarget.endsWith("windows-msvc")) return "MSVC CRT DLL";
  if (entry.language === "rust" && entry.artifactTarget.endsWith("windows-msvc")) return "Rust std + MSVC CRT DLL";
  return "see recipe";
}

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
    "",
    "| Workload | Class | Sources | Oracle | Benchmark |",
    "| --- | --- | --- | --- | --- |",
  ];
  for (const workload of catalog.workloads) {
    lines.push(`| ${workload.id} | ${workload.structureClass} | ${sourceLinks(workload)} | ${workload.oracle.status} | ${workload.benchmarkStatus} |`);
  }
  lines.push("", "## Best values", "", "| Workload | Language | Target | Runtime | Artifact | Compile p50 | Run p50 | Run p95 | Peak RSS | CPU mean |", "| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |");
  for (const group of rows) {
    const entry = group.entry;
    lines.push(`| ${entry.workloadId} | ${entry.language} | ${targetLabel(entry)} | ${runtimeLabel(entry)} | ${metricCell(group, "artifact-size")} | ${metricCell(group, "compile-latency")} | ${metricCell(group, "run-wall-time")} | ${metricCell(group, "run-wall-p95")} | ${metricCell(group, "peak-working-set")} | ${metricCell(group, "cpu-time")} |`);
  }
  lines.push(
    "",
    "Artifact size counts only the PE file. It excludes imported runtime DLLs. Public W is CRT-free; public C and Rust import the MSVC runtime. The private process-handler composite remains a GCC/MinGW contextual lane. CPU is the arithmetic mean of 101 fresh-process counters; an all-zero estimate is omitted.",
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
