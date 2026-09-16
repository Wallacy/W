#!/usr/bin/env bun

import { spawnSync } from "node:child_process";
import crypto from "node:crypto";
import { readFile, rename, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const catalogPath = path.join(root, "benchmarks", "gpu0-device-linkage-catalog.json");
const documentPath = path.join(root, "benchmarks", "GPU0.md");

function fail(message) {
  throw new Error(`gpu0 benchmark: ${message}`);
}

function digest(bytes) {
  return `sha256:${crypto.createHash("sha256").update(bytes).digest("hex")}`;
}

function exactKeys(value, expected) {
  return value !== null && typeof value === "object" && !Array.isArray(value) &&
    JSON.stringify(Object.keys(value).sort()) === JSON.stringify([...expected].sort());
}

function uniqueKinds(values, expected, label) {
  if (!Array.isArray(values) || values.length !== expected.length) fail(`${label} count is invalid`);
  const actual = values.map((value) => value?.kind ?? value?.id);
  if (new Set(actual).size !== expected.length ||
      JSON.stringify([...actual].sort()) !== JSON.stringify([...expected].sort()))
    fail(`${label} identities are invalid`);
}

export function validateGpu0Catalog(value) {
  if (!exactKeys(value, ["$schema", "version", "status", "identity", "correctness", "protocol", "artifacts", "metrics", "boundaries"])) fail("catalog keys are invalid");
  if (value.$schema !== "w-gpu0-device-linkage-catalog-1" || value.version !== 1 ||
      value.status !== "experimental-not-source-backed-w") fail("catalog header is invalid");
  if (!exactKeys(value.identity, ["platform", "provider", "device", "computeCapability", "target", "driverVersion", "mlirVersion", "clangVersion", "recipe", "recipeDigest", "adapterSourceDigest", "gpu0CoreDigest"]) ||
      value.identity.platform !== "windows-x64" || value.identity.provider !== "cuda-driver" ||
      typeof value.identity.device !== "string" || value.identity.device.length === 0 ||
      !/^[0-9]+\.[0-9]+$/u.test(value.identity.computeCapability) ||
      !/^sm_[0-9]+$/u.test(value.identity.target) ||
      value.identity.target !== `sm_${value.identity.computeCapability.replace(".", "")}` ||
      typeof value.identity.driverVersion !== "string" || value.identity.driverVersion.length === 0 ||
      value.identity.mlirVersion !== "23.1.1" || !/^[0-9]+\.[0-9]+\.[0-9]+$/u.test(value.identity.clangVersion) ||
      value.identity.recipe !== "tooling/check-gpu0-cuda-windows.mjs" ||
      !/^sha256:[0-9a-f]{64}$/u.test(value.identity.recipeDigest) ||
      !/^sha256:[0-9a-f]{64}$/u.test(value.identity.adapterSourceDigest) ||
      !/^sha256:[0-9a-f]{64}$/u.test(value.identity.gpu0CoreDigest)) fail("catalog identity is invalid");
  if (!exactKeys(value.correctness, ["expected", "observed"]) ||
      value.correctness.expected !== 42 || value.correctness.observed !== 42) fail("correctness oracle is invalid");
  if (!exactKeys(value.protocol, ["clock", "warmups", "samples", "aggregation", "contextModuleAllocationOutsideTiming"]) ||
      value.protocol.clock !== "QueryPerformanceCounter" ||
      !Number.isSafeInteger(value.protocol.warmups) || value.protocol.warmups < 1 || value.protocol.warmups > 10001 ||
      !Number.isSafeInteger(value.protocol.samples) || value.protocol.samples < 1 || value.protocol.samples > 10001 ||
      value.protocol.samples % 2 === 0 || value.protocol.aggregation !== "nearest-rank" ||
      value.protocol.contextModuleAllocationOutsideTiming !== true)
    fail("benchmark protocol is invalid");
  uniqueKinds(value.artifacts, ["host-adapter", "device-mlir", "device-ptx"], "artifact");
  for (const artifact of value.artifacts) {
    if (!exactKeys(artifact, ["kind", "sizeBytes", "digest"]) ||
        !Number.isSafeInteger(artifact.sizeBytes) || artifact.sizeBytes < 1 ||
        !/^sha256:[0-9a-f]{64}$/u.test(artifact.digest)) fail("artifact record is invalid");
  }
  uniqueKinds(value.metrics, ["h2d", "dispatch-sync", "d2h", "end-to-end"], "metric");
  for (const metric of value.metrics) {
    if (!exactKeys(metric, ["id", "unit", "p50", "p95"]) || metric.unit !== "ns" ||
        !Number.isSafeInteger(metric.p50) || metric.p50 < 0 ||
        !Number.isSafeInteger(metric.p95) || metric.p95 < metric.p50) fail("metric record is invalid");
  }
  if (!exactKeys(value.boundaries, ["sourceBackedW", "wRuntimeOrProvider", "homogeneousPinnedToolchain", "productRanking"]) ||
      Object.values(value.boundaries).some((entry) => entry !== false)) fail("evidence boundaries are invalid");
  return value;
}

function formatNanoseconds(value) {
  if (value < 1000) return `${value} ns`;
  if (value < 1_000_000) return `${(value / 1000).toFixed(1)} us`;
  return `${(value / 1_000_000).toFixed(2)} ms`;
}

export function renderGpu0Catalog(value) {
  validateGpu0Catalog(value);
  const artifact = Object.fromEntries(value.artifacts.map((entry) => [entry.kind, entry]));
  const metric = Object.fromEntries(value.metrics.map((entry) => [entry.id, entry]));
  return `# GPU0 device-linkage diagnostic\n\n` +
    `Experimental compiler/linkage evidence on ${value.identity.device} (${value.identity.target}, driver ${value.identity.driverVersion}). ` +
    `Its device request is derived from the W fixture through ACCREQ0 and the normal correctness path crosses private ACCPROV0; it is not yet a complete W executable, a W runtime/provider, homogeneous toolchain support, or a product ranking.\n\n` +
    `| Observation | Value |\n| --- | ---: |\n` +
    `| Correct result | ${value.correctness.observed} |\n` +
    `| H2D p50 / p95 | ${formatNanoseconds(metric.h2d.p50)} / ${formatNanoseconds(metric.h2d.p95)} |\n` +
    `| Dispatch + synchronize p50 / p95 | ${formatNanoseconds(metric["dispatch-sync"].p50)} / ${formatNanoseconds(metric["dispatch-sync"].p95)} |\n` +
    `| D2H p50 / p95 | ${formatNanoseconds(metric.d2h.p50)} / ${formatNanoseconds(metric.d2h.p95)} |\n` +
    `| End-to-end p50 / p95 | ${formatNanoseconds(metric["end-to-end"].p50)} / ${formatNanoseconds(metric["end-to-end"].p95)} |\n` +
    `| Host adapter | ${artifact["host-adapter"].sizeBytes} B |\n` +
    `| Device MLIR | ${artifact["device-mlir"].sizeBytes} B |\n` +
    `| PTX | ${artifact["device-ptx"].sizeBytes} B |\n` +
    `| Protocol | ${value.protocol.warmups} warmups, ${value.protocol.samples} in-process samples |\n` +
    `| Toolchain | MLIR ${value.identity.mlirVersion} + Clang ${value.identity.clangVersion} |\n\n` +
    `Reproduce correctness with \`bun check --target gpu0\` and refresh this snapshot with \`bun benchmark gpu0\`. ` +
    `The recipe compiles the C23 adapter with Clang \`-O3 -flto=full /OPT:REF /OPT:ICF\` and lowers the device artifact to \`${value.identity.target}\` PTX with \`-O3\`; all produced artifacts are temporary.`;
}

async function atomicWrite(file, contents) {
  const temporary = `${file}.tmp-${process.pid}`;
  let committed = false;
  try {
    await writeFile(temporary, contents, { encoding: "utf8", flag: "wx" });
    await rename(temporary, file);
    committed = true;
  } finally {
    if (!committed) await rm(temporary, { force: true });
  }
}

export async function checkGpu0Catalog() {
  const value = validateGpu0Catalog(JSON.parse(await readFile(catalogPath, "utf8")));
  const currentDigests = {
    recipeDigest: digest(await readFile(path.join(root, "tooling", "check-gpu0-cuda-windows.mjs"))),
    adapterSourceDigest: digest(await readFile(path.join(root, "compiler", "seed-c", "tests", "gpu0_cuda_windows.c"))),
    gpu0CoreDigest: digest(await readFile(path.join(root, "compiler", "seed-c", "src", "w_seed_gpu0.c"))),
  };
  for (const [key, digest] of Object.entries(currentDigests)) {
    if (value.identity[key] !== digest) fail(`catalog ${key} is stale`);
  }
  const expected = `${renderGpu0Catalog(value)}\n`;
  const actual = await readFile(documentPath, "utf8");
  if (actual !== expected) fail("benchmarks/GPU0.md is stale");
  return value;
}

async function runAndWrite(warmups, samples) {
  const result = spawnSync(process.execPath, [
    path.join(root, "tooling", "check-gpu0-cuda-windows.mjs"),
    "--benchmark", String(warmups), String(samples),
  ], { cwd: root, encoding: "utf8", windowsHide: true, maxBuffer: 16 * 1024 * 1024 });
  if (result.error) fail(result.error.message);
  if (result.status !== 0) fail(result.stderr || result.stdout || `runner exited ${result.status}`);
  if (result.stdout.startsWith("GPU0 CUDA benchmark: SKIP")) {
    console.log(result.stdout.trim());
    return null;
  }
  if (result.stderr !== "") fail(`runner wrote stderr: ${result.stderr}`);
  const value = validateGpu0Catalog(JSON.parse(result.stdout));
  await atomicWrite(catalogPath, `${JSON.stringify(value, null, 2)}\n`);
  await atomicWrite(documentPath, `${renderGpu0Catalog(value)}\n`);
  console.log(`GPU0 benchmark: wrote ${path.relative(root, catalogPath)} and ${path.relative(root, documentPath)}`);
  return value;
}

export async function main(argv = process.argv.slice(2)) {
  const command = argv[0] ?? "check";
  if (command === "check" && argv.length === 1) {
    await checkGpu0Catalog();
    console.log("GPU0 benchmark catalog/projection: current");
    return;
  }
  if (command === "docs" && argv.length === 1) {
    const value = validateGpu0Catalog(JSON.parse(await readFile(catalogPath, "utf8")));
    await atomicWrite(documentPath, `${renderGpu0Catalog(value)}\n`);
    console.log("GPU0 benchmark projection: wrote benchmarks/GPU0.md");
    return;
  }
  if (command === "run") {
    let warmups = 101;
    let samples = 1001;
    for (let index = 1; index < argv.length; index += 1) {
      if (argv[index] === "--warmups") warmups = Number(argv[++index]);
      else if (argv[index] === "--samples") samples = Number(argv[++index]);
      else fail(`unknown option: ${argv[index]}`);
    }
    if (!Number.isSafeInteger(warmups) || warmups < 1 || warmups > 10001 ||
        !Number.isSafeInteger(samples) || samples < 1 || samples > 10001 || samples % 2 === 0)
      fail("warmups must be 1..10001 and samples must be odd in 1..10001");
    await runAndWrite(warmups, samples);
    return;
  }
  fail("usage: gpu0-device-linkage.mjs <check|docs|run> [--warmups N] [--samples odd-N]");
}

if (import.meta.main) await main();
