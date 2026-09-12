#!/usr/bin/env bun

import { existsSync } from "node:fs";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  captureVisualStudioEnvironment,
  findVisualStudio,
} from "./windows-build-support.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const seed = path.join(root, "compiler", "seed-c");
const timeout = 120_000;

function fail(message) {
  throw new Error(`native benchmark check: ${message}`);
}

function run(command, args, { cwd = root, env = process.env } = {}) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    env,
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
    timeout,
    killSignal: "SIGKILL",
  });
  return {
    ...result,
    stdout: Buffer.from(result.stdout),
    stderr: Buffer.from(result.stderr),
  };
}

function requireSuccess(result, label) {
  if (result.exitCode === 0) return;
  const detail = Buffer.concat([result.stdout, result.stderr]).toString("utf8").trim();
  fail(`${label} failed with exit ${String(result.exitCode)}${detail ? `: ${detail.slice(-2000)}` : ""}`);
}

function clangAndEnvironment() {
  if (process.platform === "win32") {
    const installed = process.env.ProgramFiles
      ? path.join(process.env.ProgramFiles, "LLVM", "bin", "clang.exe")
      : null;
    const clang = Bun.which("clang") ?? (installed && existsSync(installed) ? installed : null);
    if (!clang) fail("Clang is required on Windows");
    return {
      clang: path.resolve(clang),
      env: captureVisualStudioEnvironment(findVisualStudio().devCommand),
    };
  }
  const clang = Bun.which("clang");
  if (!clang) fail("Clang is required");
  return { clang: path.resolve(clang), env: process.env };
}

function assertReceipt(value) {
  if (value?.schema !== "w-native-benchmark/1" || value.status !== "ok" ||
      value.warmupCount !== 1 || value.sampleCount !== 3 || value.oracle !== true ||
      !Array.isArray(value.samples) || value.samples.length !== 3)
    fail("CLI receipt identity or sample counts are invalid");
  for (const metric of ["wallNs", "directProcessCpuNs", "jobCpuNs"]) {
    const summary = value.summary?.[metric];
    if (!summary || !Number.isSafeInteger(summary.min) ||
        !Number.isSafeInteger(summary.median) || !Number.isSafeInteger(summary.p95) ||
        !Number.isSafeInteger(summary.mean) || summary.min < 0 ||
        summary.median < summary.min || summary.p95 < summary.median)
      fail(`CLI summary ${metric} is invalid`);
  }
  if (!Number.isSafeInteger(value.summary?.peakDirectWorkingSetBytes) ||
      value.summary.peakDirectWorkingSetBytes <= 0 ||
      !Number.isSafeInteger(value.summary?.peakJobCommitBytes) ||
      value.summary.peakJobCommitBytes <= 0)
    fail("CLI memory summary is invalid");
}

export async function main() {
  const cmake = Bun.which("cmake");
  const ninja = Bun.which("ninja");
  const ctest = Bun.which("ctest");
  if (!cmake || !ninja || !ctest) fail("CMake, Ninja, and CTest are required");
  const { clang, env } = clangAndEnvironment();
  const target = await mkdtemp(path.join(tmpdir(), "w-native-benchmark-"));
  try {
    requireSuccess(run(cmake, [
      "-S", seed,
      "-B", target,
      "-G", "Ninja",
      "-DCMAKE_BUILD_TYPE=Release",
      "-DW_SEED_C_STANDARD=23",
      `-DCMAKE_MAKE_PROGRAM=${ninja}`,
      `-DCMAKE_C_COMPILER=${clang}`,
    ], { env }), "C23 configure");
    requireSuccess(run(cmake, [
      "--build", target,
      "--target", "w_seed_native_benchmark_cli", "w_seed_native_benchmark_tests",
      "--parallel", "2",
    ], { env }), "native benchmark build");
    requireSuccess(run(ctest, [
      "--test-dir", target,
      "-R", "^w_seed_native_benchmark_integration$",
      "--output-on-failure",
    ], { env }), "native benchmark integration test");

    if (process.platform === "win32") {
      const suffix = ".exe";
      const cli = path.join(target, `w_seed_native_benchmark${suffix}`);
      const child = path.join(target, `w_seed_native_benchmark_tests${suffix}`);
      const stdoutHex = Buffer.from("native-benchmark-child\r\n").toString("hex");
      const stderrHex = Buffer.from("native-benchmark-child-error\r\n").toString("hex");
      const receipt = run(cli, [
        "--exe", child,
        "--arg", "--native-benchmark-test-child",
        "--warmup", "1",
        "--samples", "3",
        "--timeout-ms", "10000",
        "--expect-exit", "23",
        "--expect-stdout-hex", stdoutHex,
        "--expect-stderr-hex", stderrHex,
      ], { env });
      requireSuccess(receipt, "native benchmark CLI oracle");
      if (receipt.stderr.length !== 0) fail("native benchmark CLI wrote stderr");
      let parsed;
      try {
        parsed = JSON.parse(receipt.stdout.toString("utf8"));
      } catch (error) {
        fail(`native benchmark CLI did not emit one JSON receipt: ${error.message}`);
      }
      assertReceipt(parsed);
    }
    console.log(`native benchmark: ok (C23 ${path.basename(clang)}; bounded Windows process-tree kernel${process.platform === "win32" ? "; receipt verified" : "; platform stub verified"})`);
  } finally {
    await rm(target, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
  });
}
