import { randomUUID } from "node:crypto";
import { lstat, mkdir, readFile, rename, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import {
  executableWorkloadHasRunner,
  EXECUTABLE_PLATFORM_TARGET_LINUX,
  EXECUTABLE_PLATFORM_TARGET_LINUX_WSL,
  EXECUTABLE_WORKLOAD_FAMILY_IDS,
  ROOT,
  executableCatalogFileDigest,
  executableSuiteReceiptErrors,
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

function unmeasuredExampleLinks(workload) {
  const sources = workload.sources?.filter((source, index, all) =>
    all.findIndex((candidate) => candidate.language === source.language) === index);
  return sources?.length
    ? sources.map((source) => jsonPathLink(
      projectionPath(source.path),
      `${workload.id} (${source.language === "w" ? "W" : source.language === "c" ? "C" : "Rust"})`,
    )).join(", ")
    : workload.id;
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
    // Collapse recipe/toolchain categories only after the platform, artifact,
    // runtime-closure, and comparison identities match. The machine catalog
    // remains category-partitioned; each displayed metric is selected below.
    const hostPartition = entry.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL
      ? `\u0000${entry.host ?? ""}`
      : "";
    const key = [
      entry.workloadId,
      entry.language,
      entry.platformTarget,
      entry.artifactTarget,
      entry.abi,
      entry.runtimeClosure?.class ?? "",
      entry.runtimeClosure?.status ?? "unverified",
      entry.comparability,
      entry.eligibility,
    ].join("\u0000") + hostPartition;
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

function artifactCell(group) {
  const entry = group.metrics.get("artifact-size");
  if (!entry) return "—";
  return formatValue(entry);
}

function measurementTableHeader() {
  return [
    "| Example | System / lane | Language | Binary | Compile p50 | Execution p50 | Execution p95 | CPU mean | Peak memory |",
    "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |",
  ];
}

function projectionWorkload(workload) {
  return workload?.status === "source-oracle-ready" &&
    workload.sourceReadiness === "source-and-oracle-ready" &&
    executableWorkloadHasRunner(workload);
}

function targetLabel(entry) {
  if (entry.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) return "WSL";
  if (entry.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX) return "Linux";
  return "Windows";
}

function runtimeLabel(entry) {
  const runtimeClass = entry.runtimeClosure?.class;
  return runtimeClass === "freestanding" ? "no CRT"
    : runtimeClass === "hosted-crt" ? "CRT"
      : runtimeClass === "instrumentation" ? "instrumented"
        : "Unknown";
}

function measurementLaneLabel(entry) {
  const pieces = [targetLabel(entry), runtimeLabel(entry)];
  if (entry.eligibility === "deferred-to-M3b") pieces.push("contextual");
  else if (entry.eligibility === "exploratory-private-composite") pieces.push("private");
  if (entry.platformTarget === EXECUTABLE_PLATFORM_TARGET_LINUX_WSL) pieces.push("diagnostic");
  return pieces.join(" · ");
}

function measurementRow(group, workload) {
  const entry = group.entry;
  const source = workload?.sources.find((item) => item.language === entry.language && item.platformTarget === entry.platformTarget);
  const label = `${entry.workloadId} (${entry.language === "w" ? "W" : entry.language === "c" ? "C" : "Rust"})`;
  const example = source ? jsonPathLink(projectionPath(source.path), label) : label;
  const lane = measurementLaneLabel(entry);
  return `| ${example} | ${lane} | ${entry.language === "w" ? "W" : entry.language === "c" ? "C" : "Rust"} | ${artifactCell(group)} | ${metricCell(group, "compile-latency")} | ${metricCell(group, "run-wall-time")} | ${metricCell(group, "run-wall-p95")} | ${metricCell(group, "cpu-time")} | ${metricCell(group, "peak-working-set")} |`;
}

function formatSuiteDuration(milliseconds) {
  const wholeSeconds = Math.floor(milliseconds / 1000);
  const hours = Math.floor(wholeSeconds / 3600);
  const minutes = Math.floor((wholeSeconds % 3600) / 60);
  const seconds = wholeSeconds % 60;
  if (hours > 0) return `${hours}h ${String(minutes).padStart(2, "0")}m`;
  if (minutes > 0) return `${minutes}m ${seconds}s`;
  return `${seconds}s`;
}

export async function currentSuiteReceipt(catalog, root = ROOT) {
  const receiptPath = path.resolve(root, "benchmarks", "executable-suite-current.json");
  const bytes = await readFile(receiptPath, "utf8").catch((error) => error?.code === "ENOENT" ? undefined : Promise.reject(error));
  if (bytes === undefined) return undefined;
  let receipt;
  try { receipt = JSON.parse(bytes); }
  catch (error) { throw new Error(`invalid latest executable suite receipt: ${error?.message ?? error}`); }
  const currentDigest = executableCatalogFileDigest(root);
  if (receipt?.catalogDigest !== currentDigest) return undefined;
  const errors = executableSuiteReceiptErrors(receipt, catalog, { catalogDigest: currentDigest });
  if (errors.length > 0) throw new Error(`invalid latest executable suite receipt: ${errors.join("; ")}`);
  return receipt;
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

export function renderExecutableProjection({ catalog, root = ROOT, suiteReceipt } = {}) {
  if (!catalog?.bestMetrics) throw new TypeError("catalog with bestMetrics is required");
  const entries = [...catalog.bestMetrics.entries].sort(bestSort);
  const rows = categoryRows(entries);
  const lines = [
    "<!-- generated by tooling/executable-benchmark-docs.mjs; do not edit -->",
    "# Executable benchmarks",
    "",
    "Current best recorded values. Lower is better; `—` means no published measurement.",
    "",
    "A ready status means only that the listed witness is runnable, not that the design is complete. The machine catalog keeps exercised scope, measurement blockers, and blocked languages in separate fields.",
    "",
    "## Latest executable suite",
    "",
    suiteReceipt
      ? `**${suiteReceipt.mode === "full" ? "Full suite" : `Filtered: ${suiteReceipt.platforms.join(", ")}`}** · ${suiteReceipt.laneCounts.passed}/${suiteReceipt.laneCounts.total} lanes passed · ${suiteReceipt.laneCounts.failed} failed · ${suiteReceipt.laneCounts.skipped} skipped · ${formatSuiteDuration(suiteReceipt.durationMs)}.`
      : "No successful suite receipt matches this catalog yet.",
    "",
    "The timer covers lane runs, result validation, catalog update, and catalog/documentation checks; it excludes writing the receipt and refreshing this projection. Filtered runs are not full-suite results.",
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
      ...measurementTableHeader(),
    );
    const familyIds = new Set(familyWorkloads.map((workload) => workload.id));
    const familyRows = rows.filter((group) => familyIds.has(group.entry.workloadId));
    if (familyRows.length === 0) {
      for (const workload of familyWorkloads) {
        lines.push(`| ${unmeasuredExampleLinks(workload)} | Not measured | — | — | — | — | — | — | — |`);
      }
    } else {
      for (const workload of familyWorkloads) {
        const workloadRows = familyRows.filter((group) => group.entry.workloadId === workload.id);
        if (workloadRows.length === 0) {
          lines.push(`| ${unmeasuredExampleLinks(workload)} | Not measured | — | — | — | — | — | — | — |`);
          continue;
        }
        for (const group of workloadRows) lines.push(measurementRow(group, workload));
      }
    }
  }
  lines.push(
    "",
    "## Reading the measurements",
    "",
    "Runtime labels describe recipe intent; emitted dependency closure remains unverified. No row is a language ranking: cross-language equivalence is not established. Executable size excludes imported libraries. Toolchain identity and recipe remain lane-specific; the report does not imply one suite-wide compiler version.",
    "Run p50/p95 measure complete fresh-process invocations. Compare only like workload, language, platform, and runtime lane. WSL rows are same-host diagnostics, not native-Linux support or cross-host rankings; Windows and WSL values are never pooled.",
    `Machine catalog: ${jsonPathLink(projectionPath("benchmarks/executable-catalog.json"), "executable-catalog.json")}. Commands, sampling policy, and recipe details: [benchmark README](./README.md#manual-reproduction).`,
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
  const suiteReceipt = await currentSuiteReceipt(documents.catalog, root);
  return renderExecutableProjection({ catalog: documents.catalog, root, suiteReceipt });
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
