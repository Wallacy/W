#!/usr/bin/env bun

import { spawnSync } from "node:child_process";
import { mkdtemp, readFile, rm, stat, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { defaultCacheDirectory } from "./acquire-mlir0-windows.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const seed = path.join(root, "compiler", "seed-c");
const unitSuccessOutput = "GPU0 target-neutral logical witness: passed\r\n";
const successOutput = "GPU0 CUDA result: 42\r\n";

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
  console.log(`GPU0 CUDA integration: SKIP (${reason})`);
  process.exit(0);
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

const smi = run("nvidia-smi", ["--query-gpu=compute_cap,name", "--format=csv,noheader,nounits"], {
  label: "nvidia-smi",
});
if (smi.status !== 0) skip("nvidia-smi or a CUDA device is unavailable");
const firstGpu = smi.stdout.trim().split(/\r?\n/u)[0] ?? "";
const gpuMatch = /^(\d+)\.(\d+),\s*(.+)$/u.exec(firstGpu);
if (gpuMatch === null) fail(`unexpected nvidia-smi output: ${JSON.stringify(firstGpu)}`);
const chip = `sm_${gpuMatch[1]}${gpuMatch[2]}`;
const gpuName = gpuMatch[3].trim();

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
    "-Wsign-conversion", "-Wshadow", "-Werror", "-O2",
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
    ...strict, ...include, ...commonSources,
    path.join(seed, "tests", "test_gpu0.c"), "-o", unit,
  ], { label: "compile GPU0 unit" });
  requireSuccess(unitCompile, "compile GPU0 unit");
  const unitRun = run(unit, [], { label: "run GPU0 unit" });
  requireSuccess(unitRun, "run GPU0 unit");
  if (unitRun.stdout !== unitSuccessOutput || unitRun.stderr !== "")
    fail(`unexpected GPU0 unit output: ${JSON.stringify(unitRun)}`);
  const emitterCompile = run(clang, [
    ...strict, ...include,
    ...commonSources,
    path.join(seed, "tests", "gpu0_emit.c"),
    "-o", emitter,
  ], { label: "compile GPU0 emitter" });
  requireSuccess(emitterCompile, "compile GPU0 emitter");
  const adapterCompile = run(clang, [
    ...strict, path.join(seed, "tests", "gpu0_cuda_windows.c"), "-o", adapter,
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

  const executed = run(adapter, [provider, ptx, "w_gpu0_kernel"], {
    label: "execute GPU0 CUDA kernel",
  });
  requireSuccess(executed, "execute GPU0 CUDA kernel");
  if (executed.stdout !== successOutput || executed.stderr !== "")
    fail(`unexpected GPU0 CUDA output: ${JSON.stringify(executed)}`);

  const missing = run(adapter, [path.join(directory, "missing-provider.dll"), ptx, "w_gpu0_kernel"], {
    label: "missing GPU0 provider",
  });
  if (missing.status !== 2 || missing.stdout !== "" || missing.stderr.length === 0)
    fail("missing-provider adversarial did not fail closed");
  const wrongKernel = run(adapter, [provider, ptx, "missing_kernel"], {
    label: "missing GPU0 kernel",
  });
  if (wrongKernel.status !== 2 || wrongKernel.stdout !== "" || wrongKernel.stderr.length === 0)
    fail("missing-kernel adversarial did not fail closed");

  console.log(
    `GPU0 CUDA integration: PASS (${gpuName}; ${chip}; result 42; ` +
    `mixed MLIR 23.1.1 + Clang ${clangVersionMatch[1]}; experimental, ` +
    "not homogeneous pinned production support and not source-backed W)",
  );
} finally {
  await rm(directory, { recursive: true, force: true });
}
