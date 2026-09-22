#!/usr/bin/env bun

import { existsSync } from "node:fs";
import { copyFile, mkdir, mkdtemp, rm } from "node:fs/promises";
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
    const visualStudio = findVisualStudio();
    return {
      clang: path.resolve(clang),
      env: captureVisualStudioEnvironment(visualStudio.devCommand),
    };
  }
  const clang = Bun.which("clang");
  if (!clang) fail("Clang is required");
  return { clang: path.resolve(clang), env: process.env };
}

function assertReceipt(value, { warmupCount = 1, sampleCount = 3 } = {}) {
  if (value?.schema !== "w-native-benchmark/2" || value.status !== "ok" ||
      value.warmupCount !== warmupCount || value.sampleCount !== sampleCount ||
      value.oracle !== true || !Array.isArray(value.samples) ||
      value.samples.length !== sampleCount)
    fail("CLI receipt identity or sample counts are invalid");
  if (typeof value.measurement !== "string" ||
      !/Windows QPC.*cold target-process.*no steady body lane/u.test(value.measurement))
    fail("CLI receipt does not disclose Windows cold-process-only measurement");
  for (const metric of ["wallNs", "directProcessCpuNs", "jobCpuNs"]) {
    const summary = value.summary?.[metric];
    if (!summary || !Number.isSafeInteger(summary.min) ||
        !Number.isSafeInteger(summary.median) || !Number.isSafeInteger(summary.p95) ||
        !Number.isSafeInteger(summary.mean) || summary.min < 0 ||
        summary.median < summary.min || summary.p95 < summary.median)
      fail(`CLI summary ${metric} is invalid`);
  }
  for (const [index, sample] of value.samples.entries()) {
    const fields = ["wallNs", "directProcessUserCpuNs", "directProcessKernelCpuNs",
      "directProcessCpuNs", "jobUserCpuNs", "jobKernelCpuNs", "jobCpuNs",
      "peakDirectWorkingSetBytes", "peakJobCommitBytes", "exitCode",
      "stdoutBytes", "stderrBytes"];
    if (fields.some((field) => !Number.isSafeInteger(sample?.[field]) || sample[field] < 0) ||
        sample.wallNs === 0 || sample.peakDirectWorkingSetBytes === 0 ||
        sample.directProcessCpuNs !== sample.directProcessUserCpuNs + sample.directProcessKernelCpuNs ||
        sample.jobCpuNs !== sample.jobUserCpuNs + sample.jobKernelCpuNs)
      fail(`CLI sample ${index} is invalid`);
  }
  if (!Number.isSafeInteger(value.summary?.peakDirectWorkingSetBytes) ||
      value.summary.peakDirectWorkingSetBytes <= 0 ||
      !Number.isSafeInteger(value.summary?.peakJobCommitBytes) ||
      value.summary.peakJobCommitBytes <= 0)
    fail("CLI memory summary is invalid");
}

function assertErrorReceipt(value, expectedError, label) {
  if (value?.schema !== "w-native-benchmark/2" || value.status !== "error" ||
      value.error !== expectedError || Object.hasOwn(value, "samples") ||
      Object.hasOwn(value, "summary"))
    fail(`${label} emitted an invalid or partial error receipt`);
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

    const suffix = process.platform === "win32" ? ".exe" : "";
    const wBuild = path.join(target, "w-cli-host");
    const wCompiler = process.platform === "win32" ? "cl" : (Bun.which("cc") ?? clang);
    const wEnvironment = env;
    requireSuccess(run(cmake, [
      "-S", seed,
      "-B", wBuild,
      "-G", "Ninja",
      "-DCMAKE_BUILD_TYPE=Release",
      "-DW_SEED_C_STANDARD=23",
      `-DCMAKE_MAKE_PROGRAM=${ninja}`,
      `-DCMAKE_C_COMPILER=${wCompiler}`,
    ], { env: wEnvironment }), "native w CLI configure");
    requireSuccess(run(cmake, [
      "--build", wBuild, "--target", "w", "--parallel", "2",
    ], { env: wEnvironment }), "native w CLI build");
    const w = path.join(wBuild, `w${suffix}`);
    if (process.platform === "win32") {
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

      const command = [
        "bench", "process",
        "--exe", child,
        "--arg", "--native-benchmark-test-child",
        "--warmup", "1",
        "--samples", "3",
        "--timeout-ms", "10000",
        "--expect-exit", "23",
        "--expect-stdout-hex", stdoutHex,
        "--expect-stderr-hex", stderrHex,
      ];
      const native = run(w, command, { env });
      requireSuccess(native, "w bench process exact oracle");
      if (native.stderr.length !== 0) fail("w bench process wrote stderr");
      let nativeReceipt;
      try {
        nativeReceipt = JSON.parse(native.stdout.toString("utf8"));
      } catch (error) {
        fail(`w bench process did not emit one JSON receipt: ${error.message}`);
      }
      assertReceipt(nativeReceipt);

      const help = run(w, ["bench", "process", "--help"], { env });
      if (help.exitCode !== 0 || help.stderr.length !== 0 ||
          !help.stdout.toString("utf8").includes("Windows-native cold process measurement only") ||
          !help.stdout.toString("utf8").includes("does not compile or execute .w source"))
        fail("w bench process help did not state its Windows-native measurement boundary");

      const unicodeDirectory = path.join(target, "bench-path-東京-Ω");
      await mkdir(unicodeDirectory);
      const unicodeChild = path.join(unicodeDirectory, `native-child${suffix}`);
      await copyFile(child, unicodeChild);
      const unicodePathRun = run(w, [
        "bench", "process",
        "--exe", unicodeChild,
        "--cwd", unicodeDirectory,
        "--arg", "--native-benchmark-test-child",
        "--warmup", "0",
        "--samples", "1",
        "--timeout-ms", "10000",
        "--expect-exit", "23",
        "--expect-stdout-hex", stdoutHex,
        "--expect-stderr-hex", stderrHex,
      ], { env });
      requireSuccess(unicodePathRun, "w bench process Unicode executable/cwd paths");
      if (unicodePathRun.stderr.length !== 0)
        fail("w bench process Unicode-path run wrote stderr");
      try {
        assertReceipt(JSON.parse(unicodePathRun.stdout.toString("utf8")), {
          warmupCount: 0,
          sampleCount: 1,
        });
      } catch (error) {
        fail(`w bench process Unicode-path receipt is invalid: ${error.message}`);
      }

      const invalid = run(w, [
        "bench", "process", "--exe", child,
        "--warmup", "0", "--samples", "0",
        "--expect-exit", "23",
        "--expect-stdout-hex", stdoutHex,
        "--expect-stderr-hex", stderrHex,
      ], { env });
      if (invalid.exitCode !== 2 || invalid.stderr.length !== 0)
        fail("w bench process accepted a malformed sample count or wrote stderr");
      let invalidReceipt;
      try {
        invalidReceipt = JSON.parse(invalid.stdout.toString("utf8"));
      } catch (error) {
        fail(`w bench process malformed-input output was not one JSON error: ${error.message}`);
      }
      assertErrorReceipt(invalidReceipt, "invalid-argument",
        "w bench process malformed input");

      const missingOracle = run(w, [
        "bench", "process", "--exe", child,
        "--warmup", "0", "--samples", "1",
      ], { env });
      if (missingOracle.exitCode !== 2 || missingOracle.stderr.length !== 0)
        fail("w bench process did not require a complete exit/stdout/stderr oracle");
      let missingOracleReceipt;
      try {
        missingOracleReceipt = JSON.parse(missingOracle.stdout.toString("utf8"));
      } catch (error) {
        fail(`w bench process missing-oracle output was not one JSON error: ${error.message}`);
      }
      assertErrorReceipt(missingOracleReceipt, "invalid-argument",
        "w bench process missing-oracle path");

      const mismatch = run(w, [
        "bench", "process", "--exe", child,
        "--arg", "--native-benchmark-test-child",
        "--warmup", "0", "--samples", "1",
        "--timeout-ms", "10000",
        "--expect-exit", "23",
        "--expect-stdout-hex", Buffer.from("wrong output").toString("hex"),
        "--expect-stderr-hex", stderrHex,
      ], { env });
      if (mismatch.exitCode !== 1 || mismatch.stderr.length !== 0)
        fail("w bench process did not fail closed on an oracle mismatch");
      let mismatchReceipt;
      try {
        mismatchReceipt = JSON.parse(mismatch.stdout.toString("utf8"));
      } catch (error) {
        fail(`w bench process oracle-failure output was not one JSON error: ${error.message}`);
      }
      assertErrorReceipt(mismatchReceipt, "oracle-mismatch",
        "w bench process oracle failure");
    } else {
      const unsupported = run(w, ["bench", "process"], { env });
      if (unsupported.exitCode !== 2 || unsupported.stdout.length !== 0 ||
          !unsupported.stderr.toString("utf8").includes("Windows-only"))
        fail("non-Windows w bench process did not report the unsupported backend honestly");
    }
    console.log(`native benchmark: ok (C23 ${path.basename(clang)}; bounded Windows process-tree kernel${process.platform === "win32" ? "; w bench receipt/oracle/Unicode checks passed" : "; non-Windows w bench unsupported path verified"})`);
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
