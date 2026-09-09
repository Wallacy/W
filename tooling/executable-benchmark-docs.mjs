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
  if (entry.metric === "compile-latency" || entry.metric === "run-wall-time") return formatNanoseconds(entry.value);
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

function compactRecordId(entry) {
  const prefix = `${entry.workloadId}-${entry.language}-`;
  return entry.provenance.recordId.startsWith(prefix)
    ? entry.provenance.recordId.slice(prefix.length, prefix.length + 12)
    : entry.provenance.recordId.slice(0, 12);
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
  return entry ? `${formatValue(entry)} (${compactRecordId(entry)})` : "—";
}

export function renderExecutableProjection({ catalog, root = ROOT } = {}) {
  if (!catalog?.bestMetrics) throw new TypeError("catalog with bestMetrics is required");
  const entries = [...catalog.bestMetrics.entries].sort(bestSort);
  const rows = categoryRows(entries);
  const lines = [
    "<!-- generated by tooling/executable-benchmark-docs.mjs; do not edit -->",
    "# Executable benchmark status",
    "",
    "The catalog stores one lower-is-better value per comparable category and metric; local full-fidelity results are consumed after a successful update.",
    "Migrated cells are historical/unverified cleanliness, not current clean-run evidence. New `benchmark update` cells require the runner's verified PE-cleanliness result.",
    "",
    "## Workload readiness",
    "",
    "| Workload | Source/oracle | Benchmark lane |",
    "| --- | --- | --- |",
  ];
  for (const workload of catalog.workloads) {
    lines.push(`| ${workload.id} | ${workload.sourceReadiness}; oracle ${workload.oracle.status}; ${sourceLinks(workload)} | ${workload.benchmarkStatus} |`);
  }
  lines.push("", "## Best known cells", "", "Values include compact record-id prefixes; full per-metric provenance is in the machine catalog.", "", "| Workload | Language | Category | Artifact | Compile | Run | Peak RSS | CPU | Cleanliness |", "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |");
  for (const group of rows) {
    const entry = group.entry;
    const category = `${entry.artifactTarget}; ${entry.profile}; ${entry.host}; ${entry.recipeClass}; eq ${entry.equivalenceKey.slice(-8)}`;
    const cleanliness = [...new Set([...group.metrics.values()].map((item) => item.provenance.artifactCleanliness))].join(", ");
    lines.push(`| ${entry.workloadId} | ${entry.language} | ${category} | ${metricCell(group, "artifact-size")} | ${metricCell(group, "compile-latency")} | ${metricCell(group, "run-wall-time")} | ${metricCell(group, "peak-working-set")} | ${metricCell(group, "cpu-time")} | ${cleanliness} |`);
  }
  lines.push(
    "",
    "C MinGW and W rows are contextual and are not cross-ABI rankings; promotable Rust rows remain source-equivalence scoped.",
    "Category identity includes workload, source equivalence, platform, artifact target/ABI, profile, host, recipe class and readiness policy. Toolchain and recipe changes may improve the same cell.",
    `Machine source: ${jsonPathLink(projectionPath("benchmarks/executable-catalog.json"), "benchmarks/executable-catalog.json")}.`,
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
