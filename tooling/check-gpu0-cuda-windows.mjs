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
    env: options.env ?? process.env,
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

function escapeRegExp(text) {
  return text.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&");
}

function flattenKernelModule(text, chip, kernelSymbol) {
  const escapedKernel = escapeRegExp(kernelSymbol);
  const functionMatch = exactlyOne(
    text,
    new RegExp(
      `^\\s*llvm\\.func @${escapedKernel}\\([^\\n]*\\) attributes ` +
        "\\{gpu\\.kernel, nvvm\\.kernel\\} \\{",
      "gmu",
    ),
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

const clang = "C:/Program Files/LLVM/bin/clang.exe";
if (!(await isFile(clang))) skip("system Clang with NVPTX is unavailable");
const seedCompiler = Bun.which("gcc") ?? Bun.which("cc") ?? clang;
const clangVersion = run(clang, ["--version"], { label: "clang --version" });
requireSuccess(clangVersion, "clang --version");
const clangVersionMatch = /clang version (\d+\.\d+\.\d+)/u.exec(clangVersion.stdout);
if (clangVersionMatch === null) fail("system Clang version is not identifiable");
const clangTargets = run(clang, ["--print-targets"], { label: "clang --print-targets" });
requireSuccess(clangTargets, "clang --print-targets");
if (!/^\s*nvptx64\s+-/mu.test(clangTargets.stdout)) skip("system Clang has no NVPTX64 target");

const directory = await mkdtemp(path.join(os.tmpdir(), "w-gpu0-cuda-"));
try {
  const build = path.join(directory, "seed-build");
  const executableSuffix = process.platform === "win32" ? ".exe" : "";
  const unit = path.join(build, `w_seed_gpu0_tests${executableSuffix}`);
  const frontendUnit = path.join(build, `w_seed_frontend_tests${executableSuffix}`);
  const bindingUnit = path.join(
    build,
    `w_seed_accelerated_binding0_tests${executableSuffix}`,
  );
  const requestEmitter = path.join(
    build,
    `w_seed_accelerated_invocation0_tests${executableSuffix}`,
  );
  const adapter = path.join(directory, "gpu0_cuda_windows.exe");
  const device = path.join(directory, "device.mlir");
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
  const cmake = Bun.which("cmake");
  const ninja = Bun.which("ninja");
  if (!cmake || !ninja) skip("CMake or Ninja is unavailable");
  const buildEnvironment = { ...process.env, CC: seedCompiler };
  const configure = run(cmake, [
    "-S", seed, "-B", build, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release", "-DW_SEED_C_STANDARD=23",
    `-DCMAKE_C_COMPILER=${seedCompiler}`,
  ], { label: "configure integrated GPU0 seed build", env: buildEnvironment });
  requireSuccess(configure, "configure integrated GPU0 seed build");
  const seedBuild = run(cmake, [
    "--build", build, "--target", "w_seed_frontend_tests",
    "w_seed_accelerated_binding0_tests", "w_seed_gpu0_tests",
    "w_seed_accelerated_invocation0_tests", "--parallel", "2",
  ], { label: "build integrated GPU0 seed route", env: buildEnvironment });
  requireSuccess(seedBuild, "build integrated GPU0 seed route");
  const unitRun = run(unit, [], { label: "run GPU0 unit" });
  requireSuccess(unitRun, "run GPU0 unit");
  if (unitRun.stdout !== unitSuccessOutput || unitRun.stderr !== "")
    fail(`unexpected GPU0 unit output: ${JSON.stringify(unitRun)}`);
  const adapterCompile = run(clang, [
    ...strict, ...CLANG_RELEASE_FLAGS,
    path.join(seed, "tests", "gpu0_cuda_windows.c"), "-o", adapter,
  ], { label: "compile GPU0 CUDA adapter" });
  requireSuccess(adapterCompile, "compile GPU0 CUDA adapter");

  const fixture = path.join(seed, "fixtures", "accelerated-invocation0.w");
  const moduleFixture = path.join(seed, "fixtures", "gpu0-module.w");
  const frontendRun = run(frontendUnit, [moduleFixture], {
    label: "run source-derived GPU module unit",
  });
  requireSuccess(frontendRun, "run source-derived GPU module unit");
  if (frontendRun.stdout !== "" || frontendRun.stderr !== "")
    fail("source-derived GPU module unit produced output");
  const bindingRun = run(bindingUnit, [fixture], {
    label: "run accelerated binding unit",
  });
  requireSuccess(bindingRun, "run accelerated binding unit");
  const expectedBindingOutput =
    "ACCBIND0 verified static root binding: PASS\r\n" +
    "ACCBIND0 budgets/digests/teardown/negative barriers: PASS\r\n" +
    "ACCREQ0 provider-neutral request: PASS\r\n";
  if (bindingRun.stdout !== expectedBindingOutput || bindingRun.stderr !== "")
    fail(`accelerated binding unit output is unexpected: ${JSON.stringify(bindingRun)}`);
  const invocationRun = run(requestEmitter, [fixture], {
    label: "run accelerated invocation unit",
  });
  requireSuccess(invocationRun, "run accelerated invocation unit");
  const expectedInvocationOutput =
    "ACCINV0 source->frontend32->gpu-module-2->program: PASS\r\n" +
    "ACCINV0 identities/spans/digests/teardown/negative barriers: PASS\r\n" +
    "ACCREQ0 source-derived request/artifact/teardown: PASS\r\n";
  if (invocationRun.stdout !== expectedInvocationOutput || invocationRun.stderr !== "")
    fail(`accelerated invocation unit output is unexpected: ${JSON.stringify(invocationRun)}`);
  const emitted = run(requestEmitter, ["--emit-request", fixture, device], {
    label: "emit verified accelerated request",
  });
  requireSuccess(emitted, "emit verified accelerated request");
  if (emitted.stderr !== "") fail("accelerated request emitter produced stderr");
  let request;
  try {
    request = JSON.parse(emitted.stdout);
  } catch {
    fail(`accelerated request metadata is not JSON: ${JSON.stringify(emitted.stdout)}`);
  }
  const emittedDevice = await readFile(device);
  if (request?.schema !== "w-seed-accelerated-request0-2" ||
      typeof request.kernel !== "string" ||
      !/^[A-Za-z_][A-Za-z0-9_]*$/u.test(request.kernel) ||
      !Number.isSafeInteger(request.expected) ||
      request.expected < -2147483648 || request.expected > 2147483647 ||
      !Number.isSafeInteger(request.deviceArtifactBytes) ||
      request.deviceArtifactBytes <= 0 ||
      request.deviceArtifactBytes !== emittedDevice.length)
    fail("accelerated request metadata violates its gate contract");
  const kernelSymbol = request.kernel;
  const expectedResult = String(request.expected);
  const duplicateEmission = run(requestEmitter, ["--emit-request", fixture, device], {
    label: "reject duplicate accelerated request artifact",
  });
  if (duplicateEmission.status === 0 || duplicateEmission.stdout !== "" ||
      digest(await readFile(device)) !== digest(emittedDevice))
    fail("accelerated request emitter did not preserve an existing destination");
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

  const flattened = flattenKernelModule(
    await readFile(lowered, "utf8"), chip, kernelSymbol,
  );
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
      !new RegExp(`\\.visible\\s+\\.entry\\s+${escapeRegExp(kernelSymbol)}\\(`, "u")
        .test(ptxText))
    fail("PTX does not expose the expected GPU0 kernel and target");

  const executed = run(adapter, [provider, ptx, kernelSymbol, expectedResult], {
    label: "execute GPU0 CUDA kernel",
  });
  requireSuccess(executed, "execute GPU0 CUDA kernel");
  if (executed.stdout !== `GPU0 CUDA result: ${request.expected}\r\n` ||
      executed.stderr !== "")
    fail(`unexpected GPU0 CUDA output: ${JSON.stringify(executed)}`);

  const missing = run(adapter, [path.join(directory, "missing-provider.dll"), ptx, kernelSymbol, expectedResult], {
    label: "missing GPU0 provider",
  });
  if (missing.status !== 2 || missing.stdout !== "" || missing.stderr.length === 0)
    fail("missing-provider adversarial did not fail closed");
  const wrongKernel = run(adapter, [provider, ptx, "missing_kernel", "42"], {
    label: "missing GPU0 kernel",
  });
  if (wrongKernel.status !== 2 || wrongKernel.stdout !== "" || wrongKernel.stderr.length === 0)
    fail("missing-kernel adversarial did not fail closed");
  for (const invalid of ["not-i32", "01", "-0", "2147483648", "-2147483649"]) {
    const invalidExpected = run(adapter, [provider, ptx, kernelSymbol, invalid], {
      label: "invalid GPU0 expected result",
    });
    if (invalidExpected.status !== 2 || invalidExpected.stdout !== "" ||
        invalidExpected.stderr.length === 0)
      fail(`invalid-expected-result adversarial did not fail closed: ${invalid}`);
  }
  const mismatchedResult = request.expected === 41 ? 42 : 41;
  const wrongExpected = run(adapter, [provider, ptx, kernelSymbol, String(mismatchedResult)], {
    label: "mismatched GPU0 expected result",
  });
  if (wrongExpected.status !== 2 || wrongExpected.stdout !== "" ||
      !wrongExpected.stderr.includes(`expected ${mismatchedResult}, got ${request.expected}`))
    fail("mismatched-expected-result adversarial did not fail closed");

  if (benchmarkMode) {
    const benchmark = run(adapter, [
      provider, ptx, kernelSymbol, expectedResult, "--benchmark",
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
        timings.warmups !== benchmarkWarmups || timings.result !== request.expected ||
        !Array.isArray(timings.samples) || timings.samples.length !== benchmarkSamples)
      fail("GPU0 benchmark output violates its timing contract");
    for (const [index, sample] of timings.samples.entries()) {
      if (sample === null || typeof sample !== "object" || Array.isArray(sample) ||
          JSON.stringify(Object.keys(sample).sort()) !==
            JSON.stringify(["d2h", "dispatchSync", "endToEnd", "h2d", "result"]) ||
          sample.result !== request.expected)
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
      correctness: { expected: request.expected, observed: timings.result },
      protocol: {
        clock: "QueryPerformanceCounter",
        warmups: benchmarkWarmups,
        samples: benchmarkSamples,
        aggregation: "nearest-rank",
        contextModuleAllocationOutsideTiming: true,
      },
      artifacts: await Promise.all([
        artifact("host-adapter", adapter),
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
      `GPU0 CUDA integration: PASS (${gpuName}; ${chip}; result ${request.expected}; ` +
      `mixed MLIR 23.1.1 + Clang ${clangVersionMatch[1]}; experimental, ` +
      "source-derived ACCREQ0, not homogeneous pinned production support)",
    );
  }
} finally {
  await rm(directory, { recursive: true, force: true });
}
