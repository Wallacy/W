import { randomUUID } from "node:crypto";
import { lstat, mkdir, readFile, rename, rm, writeFile } from "node:fs/promises";
import { readFileSync } from "node:fs";
import path from "node:path";
import {
  EXECUTABLE_HISTORY_INDEX_PATH,
  RESULT_HISTORY_PATH,
  ROOT,
  loadExecutableHistoryResults,
  loadExecutableDocuments,
  validateExecutableCatalog,
  validateExecutableBestKnownFreshness,
  validateExecutableBestKnownIndex,
  validateExecutableHistory,
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

function formatScaledInteger(value, divisor, unit) {
  const scaled = (value * 1_000n + divisor / 2n) / divisor;
  const whole = scaled / 1_000n;
  const fraction = String(scaled % 1_000n).padStart(3, "0").replace(/0+$/u, "");
  return fraction.length === 0 ? `${whole} ${unit}` : `${whole}.${fraction} ${unit}`;
}

export function formatBytes(value) {
  const bytes = BigInt(value);
  if (bytes >= 1_048_576n) return `${bytes} B (${formatScaledInteger(bytes, 1_048_576n, "MiB")})`;
  if (bytes >= 1_024n) return `${bytes} B (${formatScaledInteger(bytes, 1_024n, "KiB")})`;
  return `${bytes} B`;
}

function sourceLink(source) {
  return `${source.language}: ${jsonPathLink(projectionPath(source.path))}`;
}

function formatWorkloadIds(ids) {
  return ids.length === 0 ? "no catalog workloads" : ids.map((id) => `\`${id}\``).join(", ");
}

function languageBoundaryLines(catalog) {
  const cRustTargets = catalog.workloads
    .filter((workload) => workload.sources?.some((source) => source.language === "c" || source.language === "rust"))
    .map((workload) => workload.id);
  const publicWBuildTargets = catalog.workloads
    .filter((workload) => workload.sources?.some((source) => source.language === "w" && source.recipe === "public-w-build-release"))
    .map((workload) => workload.id);
  const publicWTargets = catalog.workloads
    .filter((workload) => workload.sources?.some((source) => source.language === "w" && source.recipe === "public-w-run"))
    .map((workload) => workload.id);
  return [
    "## Workload language boundary",
    "",
    "The runner selects each target workload, materialized source, recipe and source-backed exact-output oracle from the catalog before warmup and raw samples.",
    `C and Rust routes currently cover ${formatWorkloadIds(cRustTargets)} and preserve each workload's declared artifact ABI.`,
    `W uses the public \`w build\` Release driver for ${formatWorkloadIds(publicWBuildTargets)} with the externally materialized Windows MLIR/LLVM/LLD toolchain. Its compile CPU/RSS is non-comparable to C/Rust until process-tree accounting exists.`,
    `Routes for ${formatWorkloadIds(publicWTargets)} use catalog recipe \`public-w-run\`; the runner fails before compilation until retained-artifact and separate compile-run support exists.`,
    "Comparison recipes use performance-first release optimization and strip distributable symbols; they do not use size-only optimization levels or host-specific CPU tuning.",
    "C probes `-std=c23` and then `-std=c2x`, uses O3, LTO, function/data sections, section GC and stripped symbols, and records the `x86_64-w64-mingw32` MinGW ABI.",
    "Rust records its rustc release and uses edition 2024, O3, fat LTO, one codegen unit, panic abort, stripped symbols and `/DEBUG:NONE` to suppress the linker PDB sidecar with the `x86_64-pc-windows-msvc` ABI.",
    "The public W Release route canonicalizes and eliminates common subexpressions in MLIR, uses llc O3, lld dead-code/identical-code folding, links without the CRT, and verifies a sidecar-free artifact. Its compile wall interval includes compiler descendants; direct-process CPU/RSS covers only w.exe, is non-comparable to C/Rust until process-tree accounting exists, and does not aggregate child processes.",
    "All records remain exploratory, measurement-only and not-evaluated.",
  ];
}

function readRecordSync(root, reference) {
  try {
    return JSON.parse(readFileSync(path.resolve(root, RESULT_HISTORY_PATH, reference.path), "utf8"));
  } catch {
    return undefined;
  }
}

function recordedLines(workload, history, root) {
  const lines = [];
  const references = [...(history?.records ?? [])].sort((left, right) => compareText(String(left?.id ?? ""), String(right?.id ?? "")) || compareText(String(left?.path ?? ""), String(right?.path ?? "")));
  for (const reference of references) {
    const record = readRecordSync(root, reference);
    if (!record || record.workloadId !== workload.id) continue;
    const source = workload.sources?.find((item) => item.language === record.language);
    const comparability = source?.comparability ?? "unclassified";
    lines.push(`- ${record.language} — ${comparability}; compile median ${formatNanoseconds(record.compile.summary.wallNs.median)} (CPU ${formatMicroseconds(record.compile.summary.cpuTotalUs.median)}, RSS ${formatBytes(record.compile.summary.peakRssBytes.median)}); run median ${formatNanoseconds(record.run.summary.wallNs.median)} (CPU ${formatMicroseconds(record.run.summary.cpuTotalUs.median)}, RSS ${formatBytes(record.run.summary.peakRssBytes.median)}); artifact ${formatBytes(record.artifact.sizeBytes)}; commit ${record.provenance.commit.slice(0, 12)}; toolchain ${record.identity.toolchain}; ${jsonPathLink(projectionPath(`${RESULT_HISTORY_PATH}/${reference.path}`), "history record")}`);
  }
  return lines;
}

function formatBestKnownValue(record) {
  if (record.metric === "compile-latency" || record.metric === "run-wall-time") return formatNanoseconds(record.value);
  if (record.metric === "cpu-time") return formatMicroseconds(record.value);
  return formatBytes(record.value);
}

function bestKnownLines(workload, bestKnown, history) {
  const references = new Map((history?.records ?? []).map((reference) => [reference.id, reference]));
  return (bestKnown?.records ?? [])
    .filter((record) => record.workloadId === workload.id)
    .sort((left, right) => compareText(String(left.id), String(right.id)))
    .map((record) => {
      const links = (record.derivedFrom ?? []).map((id) => {
        const reference = references.get(id);
        return reference ? jsonPathLink(projectionPath(`${RESULT_HISTORY_PATH}/${reference.path}`), "history record") : id;
      }).join(", ");
      return `- ${record.language} — ${record.metric} ${formatBestKnownValue(record)} (${record.statistic}); toolchain ${record.toolchain}; host ${record.host}; derived from ${links}`;
    });
}

export function renderExecutableProjection({ catalog, history, bestKnown, root = ROOT } = {}) {
  if (!catalog || !history || !bestKnown) throw new TypeError("catalog, history and best-known documents are required");
  const lines = [
    "<!-- generated by tooling/executable-benchmark-docs.mjs; do not edit -->",
    "# Executable benchmark status",
    "",
    "This projection is generated from the executable catalog, immutable history index, and best-known index.",
    "It records evidence status, not a claim of general Windows support or performance.",
    "",
    `Best-known status: **${bestKnown.status}** (${bestKnown.records.length} promoted records).`,
    "",
    "| Workload | Source/oracle readiness | Benchmark status |",
    "| --- | --- | --- |",
  ];
  for (const workload of catalog.workloads) {
    const sources = workload.sources?.map(sourceLink).join("; ") || "no materialized source";
    lines.push(`| ${workload.id} | ${workload.sourceReadiness}; ${sources}; oracle ${workload.oracle.status} | ${workload.benchmarkStatus} |`);
  }
  lines.push("", "## Best-known validated records", "", "Only sources with `promotable-after-equivalence` eligibility are ranked; C MinGW and the public W build route remain contextual/non-ranking evidence until process-tree accounting exists.", "A zero-valued run CPU median remains recorded evidence but is excluded from promoted `cpu-time` rows because microsecond resolution cannot establish a positive measurement.");
  let bestRecorded = 0;
  for (const workload of catalog.workloads) {
    const evidence = bestKnownLines(workload, bestKnown, history);
    if (evidence.length === 0) continue;
    bestRecorded += evidence.length;
    lines.push("", `### ${workload.id}`, "", ...evidence);
  }
  if (bestRecorded === 0) lines.push("", "No promotable executable result is tracked yet.");
  lines.push("", "## Current recorded evidence", "");
  let recorded = 0;
  for (const workload of catalog.workloads) {
    const evidence = recordedLines(workload, history, root);
    if (evidence.length === 0) continue;
    recorded += evidence.length;
    lines.push(`### ${workload.id}`, "", ...evidence, "");
  }
  if (recorded === 0) {
    lines.push("No clean-HEAD executable result is tracked yet.", "", "The local W route is bounded candidate evidence only: public `w build` Release Windows source-to-PE for workloads that declare that recipe, contextual/non-ranking until process-tree accounting exists.", "", "The runner builds `build/w-windows/w.exe` once as a bootstrap outside sample directories and leaves it retained. A pre-existing bootstrap may be replaced during that Release build. Sample directories and target EXEs are removed after the run.", "", "Local outputs stay ignored under `benchmarks/results/`; the immutable index is", `${jsonPathLink(projectionPath(EXECUTABLE_HISTORY_INDEX_PATH), "benchmarks/history/executables/index.json")}.`, "");
  }
  lines.push(...languageBoundaryLines(catalog));
  return lines.join("\n");
}

export async function renderFromDisk(root = ROOT) {
  const documents = loadExecutableDocuments(root);
  const historyResults = loadExecutableHistoryResults(documents.history, root);
  const resultRecords = historyResults.map((item) => item.record);
  const errors = [
    ...validateExecutableHistory(documents.history, documents.catalog, root),
    ...validateExecutableCatalog(documents.catalog, { ...documents, historyResults: resultRecords }, root),
    ...validateExecutableBestKnownIndex(documents.bestKnown, documents.catalog, resultRecords),
    ...validateExecutableBestKnownFreshness(documents.bestKnown, documents.catalog, resultRecords),
  ];
  if (errors.length > 0) throw new Error(errors.join("\n"));
  return renderExecutableProjection({ ...documents, root });
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
