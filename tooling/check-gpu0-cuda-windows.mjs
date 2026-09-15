#!/usr/bin/env bun

import { spawnSync } from "node:child_process";
import crypto from "node:crypto";
import { mkdtemp, readFile, rm, stat, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { defaultCacheDirectory } from "./acquire-mlir0-windows.mjs";
import { CLANG_RELEASE_FLAGS } from "./executable-release-recipes.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const seed = path.join(root, "compiler", "seed-c");
const unitSuccessOutput = "GPU0 target-neutral logical witness: passed\r\n";
const successOutput = "GPU0 CUDA result: 42\r\n";
const benchmarkMode = process.argv[2] === "--benchmark";
const benchmarkWarmups = Number(process.argv[3] ?? "101");
const benchmarkSamples = Number(process.argv[4] ?? "1001");

if (process.argv.length > (benchmarkMode ? 5 : 2) ||
    (!benchmarkMode && process.argv.length !== 2) ||
    (benchmarkMode &&
      (!Number.isSafeInteger(benchmarkWarmups) || benchmarkWarmups < 1 || benchmarkWarmups > 10001 ||
       !Number.isSafeInteger(benchmarkSamples) || benchmarkSamples < 1 || benchmarkSamples > 10001 ||
       benchmarkSamples % 2 === 0))) {
  fail("usage: check-gpu0-cuda-windows.mjs [--benchmark <warmups> <odd-samples>]");
}

function fail(message) {
  throw new Error(message);
}

async function isFile(file) {
  try {
    return (await stat(file)).isFile();
  } catch (error) {
    if (error?.code === "ENOENT") return false;
    throw error;
  }
}

function run(executable, args, options = {}) {
  const result = spawnSync(executable, args, {
    cwd: options.cwd ?? root,
    encoding: "utf8",
    windowsHide: true,
    maxBuffer: 16 * 1024 * 1024,
  });
  if (result.error) fail(`${options.label ?? executable}: ${result.error.message}`);
  return {
    status: result.status,
    stdout: result.stdout ?? "",
    stderr: result.stderr ?? "",
  };
}

function requireSuccess(result, label) {
  if (result.status !== 0)
    fail(`${label} failed (${result.status}): ${result.stderr || result.stdout}`);
}

function skip(reason) {
  console.log(`${benchmarkMode ? "GPU0 CUDA benchmark" : "GPU0 CUDA integration"}: SKIP (${reason})`);
  process.exit(0);
}

function digest(bytes) {
  return `sha256:${crypto.createHash("sha256").update(bytes).digest("hex")}`;
}

function percentile(values, probability) {
  if (!Array.isArray(values) || values.length === 0) fail("timing samples are empty");
  const sorted = [...values].sort((left, right) => left - right);
  return sorted[Math.ceil(probability * sorted.length) - 1];
}

function summarizeTicks(samples, key, frequency) {
  const nanoseconds = samples.map((sample) => {
    const ticks = sample?.[key];
    if (!Number.isSafeInteger(ticks) || ticks < 0) fail(`invalid ${key} timing sample`);
    return Math.round((ticks * 1_000_000_000) / frequency);
  });
  return {
    p50: percentile(nanoseconds, 0.50),
    p95: percentile(nanoseconds, 0.95),
  };
}

function exactlyOne(text, pattern, label) {
  const matches = [...text.matchAll(pattern)];
  if (matches.length !== 1) fail(`${label} must occur exactly once, got ${matches.length}`);
  return matches[0];
}

function flattenKernelModule(text, chip) {
  const functionMatch = exactlyOne(
    text,
    /^\s*llvm\.func @w_gpu0_kernel\([^\n]*\) attributes \{gpu\.kernel, nvvm\.kernel\} \{/gmu,
    "lowered GPU0 kernel",
  );
  exactlyOne(text, /gpu\.module @w_gpu0_device/gmu, "lowered GPU0 module");
  const start = functionMatch.index;
  const declarationEnd = start + functionMatch[0].length;
  const bodyOpen = text.lastIndexOf("{", declarationEnd - 1);
  if (bodyOpen < start) fail("lowered GPU0 function body is missing");

  let depth = 1;
  let bodyClose = -1;
  for (let index = bodyOpen + 1; index < text.length; index += 1) {
    if (text[index] === "{") depth += 1;
    else if (text[index] === "}") {
      depth -= 1;
      if (depth === 0) {
        bodyClose = index;
        break;
      }
    }
  }
  if (bodyClose < 0) fail("lowered GPU0 function body is unbalanced");

  const prefix = text.slice(0, start).trim();
  const expectedPrefix = new RegExp(
    `^module\\s*\\{\\s*gpu\\.module @w_gpu0_device \\[#nvvm\\.target<chip = "${chip}">\\]\\s*\\{$`,
    "u",
  );
  if (!expectedPrefix.test(prefix))
    fail(`lowered GPU0 wrapper is unexpected: ${JSON.stringify(prefix)}`);
  if (!/^\}\s*\}\s*$/u.test(text.slice(bodyClose + 1).trim()))
    fail("lowered GPU0 module contains unexpected operations or symbols");

  const functionText = text.slice(start, bodyClose + 1)
    .split(/\r?\n/u)
    .map((line) => line.startsWith("    ") ? line.slice(4) : line)
    .join("\n");
  return `module {\n${functionText}\n}\n`;
}

if (process.platform !== "win32") skip("native Windows evidence only");

const provider = path.join(process.env.SystemRoot ?? "C:\\Windows", "System32", "nvcuda.dll");
if (!(await isFile(provider))) skip("nvcuda.dll is unavailable");

const smi = run("nvidia-smi", ["--query-gpu=compute_cap,name,driver_version", "--format=csv,noheader,nounits"], {
  label: "nvidia-smi",
});
if (smi.status !== 0) skip("nvidia-smi or a CUDA device is unavailable");
const firstGpu = smi.stdout.trim().split(/\r?\n/u)[0] ?? "";
const gpuMatch = /^(\d+)\.(\d+),\s*(.+),\s*([^,]+)$/u.exec(firstGpu);
if (gpuMatch === null) fail(`unexpected nvidia-smi output: ${JSON.stringify(firstGpu)}`);
const chip = `sm_${gpuMatch[1]}${gpuMatch[2]}`;
const gpuName = gpuMatch[3].trim();
const driverVersion = gpuMatch[4].trim();

const toolchain = defaultCacheDirectory();
const receiptPath = path.join(toolchain, "w-mlir0-windows-materialized.json");
if (!(await isFile(receiptPath))) skip("pinned MLIR 23.1.1 cache is not materialized");
const receipt = JSON.parse(await readFile(receiptPath, "utf8"));
if (receipt?.$schema !== "w-seed-mlir0-windows-materialized-1" ||
    receipt?.version !== 1 ||
    receipt?.target?.triple !== "x86_64-pc-windows-msvc" ||
    path.resolve(receipt?.destination ?? "") !== path.resolve(toolchain))
  fail("pinned MLIR materialization receipt is invalid");
const toolPath = (name) => {
  const record = receipt?.tools?.[name];
  if (record?.version !== "23.1.1" || typeof record.relativePath !== "string")
    skip(`${name} is not verified as 23.1.1`);
  const resolved = path.resolve(toolchain, record.relativePath);
  const relative = path.relative(toolchain, resolved);
  if (relative === "" || relative === ".." || relative.startsWith(`..${path.sep}`) ||
      path.isAbsolute(relative))
    fail(`${name} escapes the pinned toolchain root`);
  return resolved;
};
const mlirOpt = toolPath("mlir-opt.exe");
const mlirTranslate = toolPath("mlir-translate.exe");
if (!(await isFile(mlirOpt)) || !(await isFile(mlirTranslate)))
  skip("pinned MLIR tools are missing");

const clang = "C:\\Program Files\\LLVM\\bin\\clang.exe";
if (!(await isFile(clang))) skip("system Clang with NVPTX is unavailable");
const clangVersion = run(clang, ["--version"], { label: "clang --version" });
requireSuccess(clangVersion, "clang --version");
const clangVersionMatch = /clang version (\d+\.\d+\.\d+)/u.exec(clangVersion.stdout);
if (clangVersionMatch === null) fail("system Clang version is not identifiable");
const clangTargets = run(clang, ["--print-targets"], { label: "clang --print-targets" });
requireSuccess(clangTargets, "clang --print-targets");
if (!/^\s*nvptx64\s+-/mu.test(clangTargets.stdout)) skip("system Clang has no NVPTX64 target");

const directory = await mkdtemp(path.join(os.tmpdir(), "w-gpu0-cuda-"));
try {
  const emitter = path.join(directory, "gpu0_emit.exe");
  const unit = path.join(directory, "gpu0_unit.exe");
  const adapter = path.join(directory, "gpu0_cuda_windows.exe");
  const host = path.join(directory, "host.mlir");
  const device = path.join(directory, "device.mlir");
  const hostChecked = path.join(directory, "host.checked.mlir");
  const deviceChecked = path.join(directory, "device.checked.mlir");
  const attached = path.join(directory, "device.attached.mlir");
  const lowered = path.join(directory, "device.lowered.mlir");
  const flat = path.join(directory, "device.flat.mlir");
  const flatChecked = path.join(directory, "device.flat.checked.mlir");
  const llvm = path.join(directory, "device.ll");
  const ptx = path.join(directory, "device.ptx");

  const strict = [
    "-std=c23", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion",
    "-Wsign-conversion", "-Wshadow", "-Werror",
  ];
  const include = ["-I", path.join(seed, "include")];
  const commonSources = [
    path.join(seed, "src", "w_seed_gpu0.c"),
    path.join(seed, "src", "w_seed_sha256.c"),
    path.join(seed, "src", "w_seed_source.c"),
    path.join(seed, "src", "w_seed_unicode.c"),
    path.join(seed, "src", "w_seed_unicode_data.c"),
  ];
  const unitCompile = run(clang, [
    ...strict, "-O2", ...include, ...commonSources,
    path.join(seed, "tests", "test_gpu0.c"), "-o", unit,
  ], { label: "compile GPU0 unit" });
  requireSuccess(unitCompile, "compile GPU0 unit");
  const unitRun = run(unit, [], { label: "run GPU0 unit" });
  requireSuccess(unitRun, "run GPU0 unit");
  if (unitRun.stdout !== unitSuccessOutput || unitRun.stderr !== "")
    fail(`unexpected GPU0 unit output: ${JSON.stringify(unitRun)}`);
  const emitterCompile = run(clang, [
    ...strict, "-O2", ...include,
    ...commonSources,
    path.join(seed, "tests", "gpu0_emit.c"),
    "-o", emitter,
  ], { label: "compile GPU0 emitter" });
  requireSuccess(emitterCompile, "compile GPU0 emitter");
  const adapterCompile = run(clang, [
    ...strict, ...CLANG_RELEASE_FLAGS,
    path.join(seed, "tests", "gpu0_cuda_windows.c"), "-o", adapter,
  ], { label: "compile GPU0 CUDA adapter" });
  requireSuccess(adapterCompile, "compile GPU0 CUDA adapter");

  const emitted = run(emitter, [host, device], { label: "GPU0 emitter" });
  requireSuccess(emitted, "GPU0 emitter");
  requireSuccess(run(mlirOpt, [host, "-o", hostChecked], { label: "parse host MLIR" }),
    "parse host MLIR");
  requireSuccess(run(mlirOpt, [device, "-o", deviceChecked], { label: "parse device MLIR" }),
    "parse device MLIR");
  requireSuccess(run(mlirOpt, [
    device, `--nvvm-attach-target=module=w_gpu0_device chip=${chip}`, "-o", attached,
  ], { label: "attach NVVM target" }), "attach NVVM target");
  const pipeline = "builtin.module(gpu.module(" +
    "convert-gpu-to-nvvm{use-bare-ptr-memref-call-conv}," +
    "convert-index-to-llvm,convert-arith-to-llvm)," +
    "finalize-memref-to-llvm," +
    "convert-nvvm-to-llvm," +
    "convert-func-to-llvm{use-bare-ptr-memref-call-conv})";
  requireSuccess(run(mlirOpt, [attached, `--pass-pipeline=${pipeline}`, "-o", lowered], {
    label: "lower GPU0 device MLIR",
  }), "lower GPU0 device MLIR");

  const flattened = flattenKernelModule(await readFile(lowered, "utf8"), chip);
  await writeFile(flat, flattened, { encoding: "utf8", flag: "wx" });
  requireSuccess(run(mlirOpt, [flat, "-o", flatChecked], { label: "parse flat LLVM dialect" }),
    "parse flat LLVM dialect");
  requireSuccess(run(mlirTranslate, [flatChecked, "--mlir-to-llvmir", "-o", llvm], {
    label: "translate GPU0 LLVM dialect",
  }), "translate GPU0 LLVM dialect");
  requireSuccess(run(clang, [
    "--target=nvptx64-nvidia-cuda", "-S", "-x", "ir", llvm,
    "-o", ptx, "-O3", `-march=${chip}`,
    "-Xclang", "-target-feature", "-Xclang", "+ptx71",
  ], { label: "emit GPU0 PTX" }), "emit GPU0 PTX");
  const ptxText = await readFile(ptx, "utf8");
  if (!ptxText.includes(`.target ${chip}`) ||
      !/\.visible\s+\.entry\s+w_gpu0_kernel\(/u.test(ptxText))
    fail("PTX does not expose the expected GPU0 kernel and target");

  const executed = run(adapter, [provider, ptx, "w_gpu0_kernel", "42"], {
    label: "execute GPU0 CUDA kernel",
  });
  requireSuccess(executed, "execute GPU0 CUDA kernel");
  if (executed.stdout !== successOutput || executed.stderr !== "")
    fail(`unexpected GPU0 CUDA output: ${JSON.stringify(executed)}`);

  const missing = run(adapter, [path.join(directory, "missing-provider.dll"), ptx, "w_gpu0_kernel", "42"], {
    label: "missing GPU0 provider",
  });
  if (missing.status !== 2 || missing.stdout !== "" || missing.stderr.length === 0)
    fail("missing-provider adversarial did not fail closed");
  const wrongKernel = run(adapter, [provider, ptx, "missing_kernel", "42"], {
    label: "missing GPU0 kernel",
  });
  if (wrongKernel.status !== 2 || wrongKernel.stdout !== "" || wrongKernel.stderr.length === 0)
    fail("missing-kernel adversarial did not fail closed");
  const invalidExpected = run(adapter, [provider, ptx, "w_gpu0_kernel", "not-i32"], {
    label: "invalid GPU0 expected result",
  });
  if (invalidExpected.status !== 2 || invalidExpected.stdout !== "" ||
      invalidExpected.stderr.length === 0)
    fail("invalid-expected-result adversarial did not fail closed");
  const wrongExpected = run(adapter, [provider, ptx, "w_gpu0_kernel", "41"], {
    label: "mismatched GPU0 expected result",
  });
  if (wrongExpected.status !== 2 || wrongExpected.stdout !== "" ||
      !wrongExpected.stderr.includes("expected 41, got 42"))
    fail("mismatched-expected-result adversarial did not fail closed");

  if (benchmarkMode) {
    const benchmark = run(adapter, [
      provider, ptx, "w_gpu0_kernel", "42", "--benchmark",
      String(benchmarkWarmups), String(benchmarkSamples),
    ], { label: "benchmark GPU0 CUDA kernel" });
    requireSuccess(benchmark, "benchmark GPU0 CUDA kernel");
    if (benchmark.stderr !== "") fail(`unexpected GPU0 benchmark stderr: ${JSON.stringify(benchmark.stderr)}`);
    let timings;
    try {
      timings = JSON.parse(benchmark.stdout);
    } catch {
      fail(`GPU0 benchmark output is not JSON: ${JSON.stringify(benchmark.stdout)}`);
    }
    if (timings?.schema !== "w-gpu0-cuda-timing-1" ||
        !Number.isSafeInteger(timings.frequency) || timings.frequency <= 0 ||
        timings.warmups !== benchmarkWarmups || timings.result !== 42 ||
        !Array.isArray(timings.samples) || timings.samples.length !== benchmarkSamples)
      fail("GPU0 benchmark output violates its timing contract");
    for (const [index, sample] of timings.samples.entries()) {
      if (sample === null || typeof sample !== "object" || Array.isArray(sample) ||
          JSON.stringify(Object.keys(sample).sort()) !==
            JSON.stringify(["d2h", "dispatchSync", "endToEnd", "h2d", "result"]) ||
          sample.result !== 42)
        fail(`GPU0 benchmark sample ${index} violates its correctness contract`);
    }
    const artifact = async (kind, file) => {
      const bytes = await readFile(file);
      return { kind, sizeBytes: bytes.length, digest: digest(bytes) };
    };
    const snapshot = {
      $schema: "w-gpu0-device-linkage-catalog-1",
      version: 1,
      status: "experimental-not-source-backed-w",
      identity: {
        platform: "windows-x64",
        provider: "cuda-driver",
        device: gpuName,
        computeCapability: `${gpuMatch[1]}.${gpuMatch[2]}`,
        target: chip,
        driverVersion,
        mlirVersion: "23.1.1",
        clangVersion: clangVersionMatch[1],
        recipe: "tooling/check-gpu0-cuda-windows.mjs",
        recipeDigest: digest(await readFile(path.join(root, "tooling", "check-gpu0-cuda-windows.mjs"))),
        adapterSourceDigest: digest(await readFile(path.join(seed, "tests", "gpu0_cuda_windows.c"))),
        gpu0CoreDigest: digest(await readFile(path.join(seed, "src", "w_seed_gpu0.c"))),
      },
      correctness: { expected: 42, observed: timings.result },
      protocol: {
        clock: "QueryPerformanceCounter",
        warmups: benchmarkWarmups,
        samples: benchmarkSamples,
        aggregation: "nearest-rank",
        contextModuleAllocationOutsideTiming: true,
      },
      artifacts: await Promise.all([
        artifact("host-adapter", adapter),
        artifact("host-mlir", host),
        artifact("device-mlir", device),
        artifact("device-ptx", ptx),
      ]),
      metrics: [
        { id: "h2d", unit: "ns", ...summarizeTicks(timings.samples, "h2d", timings.frequency) },
        { id: "dispatch-sync", unit: "ns", ...summarizeTicks(timings.samples, "dispatchSync", timings.frequency) },
        { id: "d2h", unit: "ns", ...summarizeTicks(timings.samples, "d2h", timings.frequency) },
        { id: "end-to-end", unit: "ns", ...summarizeTicks(timings.samples, "endToEnd", timings.frequency) },
      ],
      boundaries: {
        sourceBackedW: false,
        wRuntimeOrProvider: false,
        homogeneousPinnedToolchain: false,
        productRanking: false,
      },
    };
    console.log(JSON.stringify(snapshot));
  } else {
    console.log(
      `GPU0 CUDA integration: PASS (${gpuName}; ${chip}; result 42; ` +
      `mixed MLIR 23.1.1 + Clang ${clangVersionMatch[1]}; experimental, ` +
      "not homogeneous pinned production support and not source-backed W)",
    );
  }
} finally {
  await rm(directory, { recursive: true, force: true });
}
