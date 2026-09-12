import crypto from "node:crypto";
import os from "node:os";
import {
  access,
  link,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  realpath,
  rm,
  stat,
  writeFile,
} from "node:fs/promises";
import path from "node:path";
import { createServer } from "node:net";
import {
  EXECUTABLE_ARTIFACT_TARGET_MINGW,
  EXECUTABLE_ARTIFACT_TARGET_MSVC,
  EXECUTABLE_LANGUAGES,
  EXECUTABLE_PLATFORM_TARGET,
  EXECUTABLE_RESULT_SCHEMA,
  EXECUTABLE_RUN_TARGETS,
  PROCESS_ENTRY0_CORRECTNESS_INPUTS,
  PROCESS_ENTRY0_EXECUTION_KIND,
  PROCESS_ENTRY0_FAULT_CASES,
  PROCESS_ENTRY0_RECIPE,
  PROCESS_ENTRY0_RECIPE_CLASS,
  PROCESS_ENTRY0_SUPPORT_ROLES,
  PROCESS_ENTRY0_TIMED_INPUT,
  PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID,
  PUBLIC_C_RECIPE,
  ROOT,
  executableEquivalenceKey,
  executableHostIdentity,
  exactOutputDigest,
  isProcessArgumentWorkload,
  loadExecutableDocuments,
  processArgumentOracleFor,
  validateExecutableCatalog,
  validateExecutableResult,
} from "./executable-benchmark-machine.mjs";
import {
  defaultCacheDirectory,
  validateManifest,
  validateMaterialized,
} from "./acquire-mlir0-windows.mjs";
import { captureVisualStudioEnvironment, findVisualStudio, findWindowsSdkKernel32 } from "./windows-build-support.mjs";
import { dialectArgs, dialectDisclosure, probeCDialect, probeCFlag } from "./c-dialect.mjs";
import {
  C_WHOLE_PROGRAM_FLAG,
  CLANG_C_TARGET,
  RUST_RELEASE_FLAGS,
  cReleaseFlags,
  clangReleaseFlags,
  W_LLC_FLAGS,
  W_LLD_LINK_FLAGS,
  W_MLIR_OPT_FLAGS,
} from "./executable-release-recipes.mjs";

export const RESULTS_DIRECTORY = path.resolve(ROOT, "benchmarks", "results");
const CATALOG_PATH = path.resolve(ROOT, "benchmarks", "executable-catalog.json");
const SEED_C_PATH = path.resolve(ROOT, "compiler", "seed-c");
const NATIVE_BENCHMARK_ABI = "w-native-benchmark/2";
const NATIVE_BENCHMARK_TARGET = "w_seed_native_benchmark_cli";
const NATIVE_BENCHMARK_CAPTURE_LIMIT = 65_536;
const RUNNER_SOURCE_PATHS = Object.freeze([
  path.resolve(import.meta.dir, "executable-benchmark-runner.mjs"),
  path.join(SEED_C_PATH, "include", "w_seed_native_benchmark.h"),
  path.join(SEED_C_PATH, "src", "w_seed_native_benchmark.c"),
  path.join(SEED_C_PATH, "cli", "native_benchmark.c"),
]);
const TOOLCHAIN_MANIFEST_PATH = path.resolve(ROOT, "tooling", "mlir0-windows-toolchain.json");
const DEFAULT_TARGET = "hello";
const RUN_TARGETS = EXECUTABLE_RUN_TARGETS;
const DEFAULT_WARMUP = 1;
const DEFAULT_COMPILE_SAMPLES = 9;
const DEFAULT_RUN_SAMPLES = 101;
export const EXECUTABLE_MAX_SAMPLES = 1001;
const RUN_DIRECTORY_PREFIX = "w-executable-run-";
const SAMPLE_DIRECTORY_PREFIX = "w-executable-sample-";
const PUBLIC_W_BUILD_RECIPE = "public-w-build-release";
const PUBLIC_W_BUILD_SCRIPT = path.resolve(ROOT, "tooling", "build-w-windows.mjs");
const PUBLIC_W_BUILD_DIRECTORY = path.resolve(ROOT, "build", "w-windows");
const PUBLIC_W_EXECUTABLE = path.join(PUBLIC_W_BUILD_DIRECTORY, "w.exe");
const PUBLIC_W_RECEIPT = path.join(PUBLIC_W_BUILD_DIRECTORY, "receipt.json");
const PROCESS_ENTRY0_GATE_TARGET = "w_seed_process_entry0_gate";
const PROCESS_ENTRY0_GATE = path.resolve(ROOT, "build", `${PROCESS_ENTRY0_GATE_TARGET}.exe`);

function benchmarkLeaseAddress(root = ROOT) {
  const identity = path.resolve(root).replaceAll("\\", "/");
  const digest = crypto.createHash("sha256").update(
    process.platform === "win32" ? identity.toLowerCase() : identity,
    "utf8",
  ).digest("hex").slice(0, 32);
  return process.platform === "win32"
    ? `\\\\.\\pipe\\w-executable-benchmark-${digest}`
    : `\0w-executable-benchmark-${digest}`;
}

export async function acquireExecutableBenchmarkLease(root = ROOT) {
  const server = createServer((socket) => socket.destroy());
  const address = benchmarkLeaseAddress(root);
  await new Promise((resolve, reject) => {
    const onError = (error) => {
      server.removeListener("listening", onListening);
      reject(new Error(error?.code === "EADDRINUSE"
        ? "another executable benchmark is already running for this checkout"
        : `executable benchmark lease failed: ${error?.code ?? error?.message ?? error}`));
    };
    const onListening = () => {
      server.removeListener("error", onError);
      resolve();
    };
    server.once("error", onError);
    server.once("listening", onListening);
    server.listen(address);
  });
  let released = false;
  return async () => {
    if (released) return;
    released = true;
    await new Promise((resolve, reject) =>
      server.close((error) => error ? reject(error) : resolve()));
  };
}
const PROCESS_ENTRY0_FAULT_ENV = "W_SEED_PROCESS_ENTRY0_FAULT";
const PROCESS_ENTRY0_HANDLER_SYMBOL = "w_seed_process_entry0_handler";
const PROCESS_ENTRY0_GENERATED_TRAP_EXIT_CODE = 0x1d;
const PROCESS_ENTRY0_HARNESS_FAILURE_EXIT_CODE = 11;
const PROCESS_ENTRY0_INCLUDE_DIRECTORY = path.resolve(ROOT, "compiler", "seed-c", "include");
const PROCESS_ENTRY0_SUPPORT_ROOT = path.resolve(ROOT, "compiler", "seed-c");
const PROCESS_ENTRY0_RUST_OBJECT_FLAGS = Object.freeze([
  "-C", "opt-level=3",
  "-C", "lto=fat",
  "-C", "codegen-units=1",
  "-C", "panic=abort",
  "-C", "debuginfo=0",
  "-C", "strip=symbols",
  "-C", "link-dead-code=no",
]);
const PROCESS_ENTRY0_GCC_ORIGIN_TARGET = EXECUTABLE_ARTIFACT_TARGET_MINGW;
const PRIVATE_C_COMPILER_NAMES = ["gcc", "cc"];
const RUST_TARGET = EXECUTABLE_ARTIFACT_TARGET_MSVC;
const PE_DOS_HEADER_SIZE = 0x40;
const PE_FILE_HEADER_SIZE = 20;
const PE_OPTIONAL_HEADER_MINIMUM_SIZE = 240;
const PE_OPTIONAL_HEADER_DATA_DIRECTORY_OFFSET = 108;
const PE_DATA_DIRECTORY_OFFSET = 112;
const PE_OPTIONAL_HEADER_SECTION_ALIGNMENT_OFFSET = 32;
const PE_OPTIONAL_HEADER_FILE_ALIGNMENT_OFFSET = 36;
const PE_OPTIONAL_HEADER_SIZE_OF_HEADERS_OFFSET = 60;
const PE_DATA_DIRECTORY_ENTRY_SIZE = 8;
const PE_DEBUG_DIRECTORY_INDEX = 6;
const PE_SECURITY_DIRECTORY_INDEX = 4;
const PE_REQUIRED_DIRECTORY_COUNT = PE_DEBUG_DIRECTORY_INDEX + 1;
const PE_MAX_DIRECTORY_COUNT = 16;
const PE_SECTION_HEADER_SIZE = 40;
const PE_SECTION_NAME_SIZE = 8;
const PE_DEBUG_DIRECTORY_ENTRY_SIZE = 28;
const PE_DEBUG_TYPE_CODEVIEW = 2;
const PE_DEBUG_TYPE_POGO = 13;
const PE_DEBUG_TYPE_REPRO = 16;
export const EXECUTABLE_CHILD_TIMEOUT_MS = 120_000;
export const EXECUTABLE_CHILD_KILL_SIGNAL = "SIGKILL";
const EXECUTABLE_TIMEOUT_STATUS = `Bun.spawnSync enforces a ${EXECUTABLE_CHILD_TIMEOUT_MS} ms per-direct-child timeout and sends ${EXECUTABLE_CHILD_KILL_SIGNAL}; descendant termination is not guaranteed without a Windows Job Object, and timed-out children abort the run without publishing a partial result`;
const textDecoder = new TextDecoder("utf-8", { fatal: true });

function fail(message) {
  throw new Error(`executable benchmark: ${message}`);
}

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function isContained(parent, candidate) {
  const relative = path.relative(path.resolve(parent), path.resolve(candidate));
  return relative === "" ||
    (relative !== ".." && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative));
}

function samePath(left, right) {
  const normalizedLeft = path.resolve(left);
  const normalizedRight = path.resolve(right);
  return process.platform === "win32"
    ? normalizedLeft.toLowerCase() === normalizedRight.toLowerCase()
    : normalizedLeft === normalizedRight;
}

async function assertNoReparseAncestors(candidate, stopAt) {
  let current = path.resolve(candidate);
  const stop = path.resolve(stopAt);
  if (!isContained(stop, current)) fail(`path escapes its containment root: ${current}`);
  let physicalStop;
  try {
    physicalStop = await realpath(stop);
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
  while (isContained(stop, current)) {
    try {
      const stats = await lstat(current);
      if (stats.isSymbolicLink()) fail(`path contains a symbolic link: ${current}`);
      if (physicalStop !== undefined) {
        const physical = await realpath(current);
        if (!isContained(physicalStop, physical) || !samePath(current, physical)) {
          fail(`path contains a reparse point or resolves outside its containment root: ${current}`);
        }
      }
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
    if (samePath(current, stop)) break;
    const parent = path.dirname(current);
    if (samePath(parent, current)) break;
    current = parent;
  }
}

async function regularFile(filePath, label) {
  const stats = await lstat(filePath);
  if (!stats.isFile() || stats.isSymbolicLink()) fail(`${label} must be a regular non-link file`);
  return stats;
}

async function assertSidecarFree(directory, artifact, label) {
  const produced = (await readdir(directory)).sort();
  if (produced.length !== 1 || produced[0] !== path.basename(artifact)) {
    fail(`${label} produced unexpected release sidecars: ${produced.join(", ")}`);
  }
}

function bufferValue(value) {
  if (value === undefined || value === null) return Buffer.alloc(0);
  return Buffer.isBuffer(value) ? value : Buffer.from(value);
}

function integerOption(value, name, minimum, maximum = Number.MAX_SAFE_INTEGER) {
  if (typeof value !== "string" || !/^[0-9]+$/u.test(value)) fail(`${name} must be a decimal integer`);
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || parsed < minimum || parsed > maximum) fail(`${name} must be between ${minimum} and ${maximum}`);
  return parsed;
}

export function parseBenchmarkArguments(argv) {
  if (!Array.isArray(argv)) fail("arguments must be an array");
  const result = {
    target: DEFAULT_TARGET,
    language: "w",
    output: undefined,
    warmup: DEFAULT_WARMUP,
    compileSamples: DEFAULT_COMPILE_SAMPLES,
    runSamples: DEFAULT_RUN_SAMPLES,
    help: false,
  };
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--help" || argument === "-h") {
      result.help = true;
    } else if (argument === "--target") {
      const value = argv[++index];
      if (value === undefined) fail("--target requires a value");
      result.target = value;
    } else if (argument.startsWith("--target=")) {
      result.target = argument.slice("--target=".length);
    } else if (argument === "--language") {
      const value = argv[++index];
      if (value === undefined) fail("--language requires a value");
      result.language = value;
    } else if (argument.startsWith("--language=")) {
      result.language = argument.slice("--language=".length);
    } else if (argument === "--output") {
      const value = argv[++index];
      if (value === undefined || value.length === 0) fail("--output requires a path");
      result.output = value;
    } else if (argument.startsWith("--output=")) {
      result.output = argument.slice("--output=".length);
      if (result.output.length === 0) fail("--output requires a path");
    } else if (argument === "--warmup") {
      result.warmup = integerOption(argv[++index], "--warmup", 1, EXECUTABLE_MAX_SAMPLES);
    } else if (argument.startsWith("--warmup=")) {
      result.warmup = integerOption(argument.slice("--warmup=".length), "--warmup", 1, EXECUTABLE_MAX_SAMPLES);
    } else if (argument === "--compile-samples") {
      result.compileSamples = integerOption(argv[++index], "--compile-samples", 9, EXECUTABLE_MAX_SAMPLES);
    } else if (argument.startsWith("--compile-samples=")) {
      result.compileSamples = integerOption(argument.slice("--compile-samples=".length), "--compile-samples", 9, EXECUTABLE_MAX_SAMPLES);
    } else if (argument === "--run-samples") {
      result.runSamples = integerOption(argv[++index], "--run-samples", 9, EXECUTABLE_MAX_SAMPLES);
    } else if (argument.startsWith("--run-samples=")) {
      result.runSamples = integerOption(argument.slice("--run-samples=".length), "--run-samples", 9, EXECUTABLE_MAX_SAMPLES);
    } else if (argument === "--samples") {
      result.compileSamples = result.runSamples = integerOption(argv[++index], "--samples", 9, EXECUTABLE_MAX_SAMPLES);
    } else if (argument.startsWith("--samples=")) {
      result.compileSamples = result.runSamples = integerOption(argument.slice("--samples=".length), "--samples", 9, EXECUTABLE_MAX_SAMPLES);
    } else {
      fail(`unknown option: ${argument}`);
    }
  }
  if (!RUN_TARGETS.includes(result.target)) fail(`unsupported benchmark target: ${result.target}`);
  if (!EXECUTABLE_LANGUAGES.includes(result.language)) fail(`unsupported language: ${result.language}`);
  if (result.compileSamples % 2 === 0 || result.runSamples % 2 === 0) fail("sample counts must be odd");
  return result;
}

export function benchmarkUsage() {
  return [
    "usage: bun tooling/executable-benchmark-runner.mjs --output <new-json> [options]",
    "",
    "Options: --target <runnable-catalog-id> (default hello), --language w|c|rust (default w), --warmup <n> (default 1), --compile-samples <odd n> (default 9), --run-samples <odd n> (default 101). --samples sets both counts.",
    "The output must be a new JSON file under benchmarks/results.",
    "This is Windows x86_64 exploratory executable evidence. The runner selects the catalog source, recipe and exact-output oracle for each target. W uses the public w build Release source-to-PE candidate for public workloads; process-argument workloads validate all declared argument cases before timing and pin the declared timed vector; process-handler-lifecycle uses the private GCC/MinGW handler composite and remains contextual/non-ranking. Public C requires Clang with final C23, the MSVC ABI, and the DLL runtime; Rust uses rustc edition 2024.",
    `Timeout guard: ${EXECUTABLE_TIMEOUT_STATUS}.`,
  ].join("\n");
}

function sha256Bytes(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

async function sha256File(filePath) {
  return sha256Bytes(await readFile(filePath));
}

export async function benchmarkRunnerDigest() {
  const hash = crypto.createHash("sha256");
  for (const filePath of RUNNER_SOURCE_PATHS) {
    hash.update(path.relative(ROOT, filePath).replaceAll("\\", "/"));
    hash.update("\0");
    hash.update(await readFile(filePath));
    hash.update("\0");
  }
  return `sha256:${hash.digest("hex")}`;
}

function sha256Json(value) {
  return sha256Bytes(Buffer.from(JSON.stringify(value), "utf8"));
}

function commandResult(result) {
  if (!isObject(result)) fail("executor must return a result object");
  return {
    exitCode: result.exitCode === null || Number.isInteger(result.exitCode) ? result.exitCode : null,
    signalCode: result.signalCode ?? null,
    exitedDueToTimeout: result.exitedDueToTimeout === true,
    stdout: bufferValue(result.stdout),
    stderr: bufferValue(result.stderr),
    resourceUsage: result.resourceUsage,
  };
}

function stdinValue(value) {
  return typeof value === "string" ? Buffer.from(value, "utf8") : value;
}

export function defaultExecutor(command, args, options = {}) {
  if (!path.isAbsolute(command)) fail(`executor command must be absolute: ${command}`);
  if (!Array.isArray(args) || args.some((item) => typeof item !== "string")) fail("executor args must be strings");
  return commandResult(Bun.spawnSync({
    cmd: [command, ...args],
    cwd: options.cwd ?? ROOT,
    env: options.env,
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
    stdin: stdinValue(options.stdin),
    timeout: options.timeout ?? EXECUTABLE_CHILD_TIMEOUT_MS,
    killSignal: options.killSignal ?? EXECUTABLE_CHILD_KILL_SIGNAL,
  }));
}

function timeoutFailure(result, label, timeout) {
  if (!result.exitedDueToTimeout) return;
  const signal = result.signalCode ?? EXECUTABLE_CHILD_KILL_SIGNAL;
  fail(`${label} exceeded the ${timeout} ms child timeout and was terminated with ${signal}; no partial result was recorded`);
}

async function executeChild(executor, command, args, options, label) {
  const timeout = options?.timeout ?? EXECUTABLE_CHILD_TIMEOUT_MS;
  let result;
  try {
    result = commandResult(await executor(command, args, {
      ...options,
      timeout,
      killSignal: options?.killSignal ?? EXECUTABLE_CHILD_KILL_SIGNAL,
    }));
  } catch (error) {
    fail(`${label} child infrastructure error: ${error?.message ?? String(error)}`);
  }
  timeoutFailure(result, label, timeout);
  if (result.exitCode === null) fail(`${label} child did not return an exit code`);
  if (result.signalCode !== null) fail(`${label} child was terminated by ${result.signalCode}`);
  return result;
}

function counter(value, label) {
  try {
    const converted = typeof value === "bigint" ? value : BigInt(value);
    if (converted < 0n) throw new RangeError();
    return converted;
  } catch {
    fail(`${label} must be a non-negative integer counter`);
  }
}

function resourceUsage(usage, label) {
  if (!isObject(usage) || !isObject(usage.cpuTime)) fail(`${label} did not expose Bun resourceUsage CPU counters`);
  const user = counter(usage.cpuTime.user, `${label}.cpuTime.user`);
  const system = counter(usage.cpuTime.system, `${label}.cpuTime.system`);
  const rss = counter(usage.maxRSS, `${label}.maxRSS`);
  if (rss === 0n) fail(`${label}.maxRSS must be positive`);
  return { user, system, rss };
}

function sampleFrom(start, end, usages, label) {
  const wall = end - start;
  if (wall <= 0n) fail(`${label} monotonic wall sample must be positive`);
  let user = 0n;
  let system = 0n;
  let rss = 0n;
  for (const usage of usages) {
    user += usage.user;
    system += usage.system;
    if (usage.rss > rss) rss = usage.rss;
  }
  if (rss <= 0n) fail(`${label} peak RSS sample must be positive`);
  return {
    wallNs: wall.toString(10),
    cpuUserUs: user.toString(10),
    cpuSystemUs: system.toString(10),
    cpuTotalUs: (user + system).toString(10),
    peakRssBytes: rss.toString(10),
  };
}

async function timedStep(executor, command, args, cwd, label, options = {}) {
  const start = process.hrtime.bigint();
  const result = await executeChild(executor, command, args, {
    cwd,
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
    ...options,
  }, label);
  const end = process.hrtime.bigint();
  return {
    ...result,
    start,
    end,
    usage: resourceUsage(result.resourceUsage, label),
  };
}

function chainSample(steps, start, end, label) {
  return sampleFrom(start, end, steps.map((step) => step.usage), label);
}

export function deriveSummary(raw) {
  if (!Array.isArray(raw) || raw.length === 0) fail("summary requires raw samples");
  const summary = {};
  for (const field of ["wallNs", "cpuUserUs", "cpuSystemUs", "cpuTotalUs", "peakRssBytes"]) {
    const values = raw.map((sample) => BigInt(sample[field])).sort((left, right) => left < right ? -1 : left > right ? 1 : 0);
    const median = values[Math.floor(values.length / 2)];
    const mean = values.reduce((total, value) => total + value, 0n) / BigInt(values.length);
    const deviations = values.map((value) => value >= median ? value - median : median - value)
      .sort((left, right) => left < right ? -1 : left > right ? 1 : 0);
    summary[field] = {
      min: values[0].toString(10),
      median: median.toString(10),
      max: values[values.length - 1].toString(10),
      arithmeticMean: mean.toString(10),
      mad: deviations[Math.floor(deviations.length / 2)].toString(10),
    };
  }
  return summary;
}

function sampleSeries(warmup, raw, measurementKernel = "bun-direct/1") {
  return {
    warmup,
    raw,
    summary: deriveSummary(raw),
    cpuResolution: {
      unit: "microseconds",
      zeroAllowed: true,
      disclosure: measurementKernel === NATIVE_BENCHMARK_ABI
        ? "Windows Job Object CPU counters originate in 100 ns units and are normalized with floor rounding to catalog microseconds; zero-valued samples are preserved."
        : "Bun resourceUsage reports CPU counters in microseconds; zero-valued samples are preserved and do not imply nanosecond precision.",
    },
  };
}

function nativeCounter(value, label) {
  if (!Number.isSafeInteger(value) || value < 0) fail(`${label} must be a non-negative safe integer`);
  return BigInt(value);
}

export function nativeReceiptSamples(receipt, expectedCount, label = "native benchmark",
                                     expected = undefined) {
  const receiptFields = ["measurement", "oracle", "sampleCount", "samples",
    "schema", "status", "summary", "warmupCount"];
  if (!isObject(receipt) || receipt.schema !== NATIVE_BENCHMARK_ABI ||
      receipt.status !== "ok" || receipt.warmupCount !== 0 ||
      receipt.sampleCount !== expectedCount || receipt.oracle !== true ||
      !Array.isArray(receipt.samples) || receipt.samples.length !== expectedCount ||
      Object.keys(receipt).sort().join("\0") !== receiptFields.sort().join("\0")) {
    fail(`${label} receipt identity or sample count is invalid`);
  }
  return receipt.samples.map((sample, index) => {
    const name = `${label}.samples[${index}]`;
    if (!isObject(sample)) fail(`${name} must be an object`);
    const sampleFields = ["directProcessCpuNs", "directProcessKernelCpuNs",
      "directProcessUserCpuNs", "exitCode", "jobCpuNs", "jobKernelCpuNs",
      "jobUserCpuNs", "peakDirectWorkingSetBytes", "peakJobCommitBytes",
      "stderrBytes", "stdoutBytes", "wallNs"];
    if (Object.keys(sample).sort().join("\0") !== sampleFields.sort().join("\0")) {
      fail(`${name} fields are invalid`);
    }
    const wall = nativeCounter(sample.wallNs, `${name}.wallNs`);
    const directUser = nativeCounter(sample.directProcessUserCpuNs, `${name}.directProcessUserCpuNs`);
    const directKernel = nativeCounter(sample.directProcessKernelCpuNs, `${name}.directProcessKernelCpuNs`);
    const directTotal = nativeCounter(sample.directProcessCpuNs, `${name}.directProcessCpuNs`);
    const jobUser = nativeCounter(sample.jobUserCpuNs, `${name}.jobUserCpuNs`);
    const jobKernel = nativeCounter(sample.jobKernelCpuNs, `${name}.jobKernelCpuNs`);
    const jobTotal = nativeCounter(sample.jobCpuNs, `${name}.jobCpuNs`);
    const workingSet = nativeCounter(sample.peakDirectWorkingSetBytes, `${name}.peakDirectWorkingSetBytes`);
    const jobCommit = nativeCounter(sample.peakJobCommitBytes, `${name}.peakJobCommitBytes`);
    if (wall === 0n || workingSet === 0n || directTotal !== directUser + directKernel ||
        jobCommit === 0n || jobTotal !== jobUser + jobKernel || jobTotal < directTotal) {
      fail(`${name} contains an inconsistent native measurement`);
    }
    if (expected && (sample.exitCode !== expected.exitCode ||
        sample.stdoutBytes !== expected.stdoutBytes ||
        sample.stderrBytes !== expected.stderrBytes)) {
      fail(`${name} does not match the requested exact oracle receipt`);
    }
    const userUs = jobUser / 1000n;
    const systemUs = jobKernel / 1000n;
    return {
      wallNs: wall.toString(10),
      cpuUserUs: userUs.toString(10),
      cpuSystemUs: systemUs.toString(10),
      cpuTotalUs: (userUs + systemUs).toString(10),
      peakRssBytes: workingSet.toString(10),
    };
  });
}

async function prepareNativeBenchmark(executor, tempRoot) {
  const cmake = Bun.which("cmake");
  const ninja = Bun.which("ninja");
  const installedClang = process.env.ProgramFiles
    ? path.join(process.env.ProgramFiles, "LLVM", "bin", "clang.exe")
    : undefined;
  const clang = Bun.which("clang") ?? (installedClang && await regularFile(installedClang, "native benchmark Clang").then(() => installedClang, () => undefined));
  if (!cmake || !ninja || !clang) fail("native runtime measurement requires CMake, Ninja, and Clang");
  const buildDirectory = path.join(tempRoot, "native-benchmark");
  const environment = captureVisualStudioEnvironment(findVisualStudio().devCommand);
  const configured = await executeChild(executor, cmake, [
    "-S", SEED_C_PATH,
    "-B", buildDirectory,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DW_SEED_C_STANDARD=23",
    `-DCMAKE_MAKE_PROGRAM=${ninja}`,
    `-DCMAKE_C_COMPILER=${clang}`,
  ], { cwd: ROOT, env: environment }, "native benchmark configure");
  requireSuccess(configured, "native benchmark configure");
  const built = await executeChild(executor, cmake, [
    "--build", buildDirectory, "--target", NATIVE_BENCHMARK_TARGET,
    "--parallel", "2",
  ], { cwd: ROOT, env: environment }, "native benchmark build");
  requireSuccess(built, "native benchmark build");
  const executable = path.join(buildDirectory, "w_seed_native_benchmark.exe");
  await regularFile(executable, "native benchmark executable");
  return { abi: NATIVE_BENCHMARK_ABI, executable, digest: await sha256File(executable) };
}

async function measureNativeBatch(context, artifact, argumentsVector, oracle,
                                  count, label, environment) {
  const stdout = Buffer.from(oracle.stdout, "utf8");
  const stderr = Buffer.from(oracle.stderr, "utf8");
  if (stdout.length > NATIVE_BENCHMARK_CAPTURE_LIMIT || stderr.length > NATIVE_BENCHMARK_CAPTURE_LIMIT) {
    fail(`${label} oracle exceeds the native capture limit`);
  }
  const request = {
    executable: artifact,
    cwd: path.dirname(artifact),
    arguments: [...argumentsVector],
    warmup: 0,
    samples: count,
    timeoutMs: EXECUTABLE_CHILD_TIMEOUT_MS,
    expectedExitCode: oracle.exitCode,
    expectedStdoutHex: stdout.toString("hex"),
    expectedStderrHex: stderr.toString("hex"),
  };
  let receipt;
  if (typeof context.nativeBenchmark.measureBatch === "function") {
    receipt = await context.nativeBenchmark.measureBatch(request);
  } else {
    const args = [
      "--exe", request.executable,
      "--cwd", request.cwd,
      ...request.arguments.flatMap((argument) => ["--arg", argument]),
      "--warmup", "0",
      "--samples", String(request.samples),
      "--timeout-ms", String(request.timeoutMs),
      "--expect-exit", String(request.expectedExitCode),
      "--expect-stdout-hex", request.expectedStdoutHex,
      "--expect-stderr-hex", request.expectedStderrHex,
    ];
    const measured = await executeChild(context.executor, context.nativeBenchmark.executable,
      args, { cwd: request.cwd, env: environment, timeout: 900_000 }, label);
    requireSuccess(measured, label);
    if (measured.stderr.length !== 0) fail(`${label} wrote stderr`);
    try {
      receipt = JSON.parse(outputText(measured.stdout));
    } catch (error) {
      fail(`${label} did not emit one native receipt: ${error.message}`);
    }
  }
  return nativeReceiptSamples(receipt, count, label, {
    exitCode: oracle.exitCode,
    stdoutBytes: stdout.length,
    stderrBytes: stderr.length,
  });
}

async function nativeRuntimeSeries(context, artifact, argumentsVector, oracle,
                                   warmupCount, sampleCount, label, environment) {
  return {
    warmup: await measureNativeBatch(context, artifact, argumentsVector, oracle,
      warmupCount, `${label} warmup`, environment),
    raw: await measureNativeBatch(context, artifact, argumentsVector, oracle,
      sampleCount, `${label} raw`, environment),
  };
}

function outputText(bytes) {
  try { return textDecoder.decode(bytes); } catch { return "<non-UTF-8 output>"; }
}

function requireSuccess(step, label) {
  if (step.exitCode !== 0) {
    const detail = outputText(step.stderr) || outputText(step.stdout);
    fail(`${label} failed with exit ${step.exitCode}: ${detail.slice(-1200)}`);
  }
}

async function resolveWindowsToolchain() {
  if (process.platform !== "win32" || process.arch !== "x64") fail(`M3b measurements require Windows x86_64, got ${process.platform}/${process.arch}`);
  const manifestBytes = await readFile(TOOLCHAIN_MANIFEST_PATH);
  const manifest = JSON.parse(manifestBytes.toString("utf8"));
  const errors = validateManifest(manifest);
  if (errors.length > 0) fail(errors.join("; "));
  const cache = defaultCacheDirectory();
  const manifestDigest = sha256Bytes(manifestBytes);
  const materialized = await validateMaterialized(cache, manifest, manifestDigest.slice("sha256:".length));
  const tools = {};
  for (const name of manifest.tools.required) {
    const record = materialized.tools?.[name];
    if (!record?.relativePath) fail(`materialized tool record is missing ${name}`);
    const toolPath = path.resolve(cache, record.relativePath);
    if (!isContained(cache, toolPath)) fail(`materialized tool escapes its cache: ${name}`);
    await regularFile(toolPath, name);
    tools[name] = toolPath;
  }
  const sdk = await findWindowsSdkKernel32();
  await regularFile(sdk.path, "Windows SDK kernel32.lib");
  return { cache, manifest, manifestDigest, materialized, tools, sdk };
}

async function buildPublicW(executor) {
  if (process.platform !== "win32" || process.arch !== "x64") fail("public w build Release requires Windows x86_64");
  const build = await timedStep(executor, process.execPath, [PUBLIC_W_BUILD_SCRIPT, "--profile", "release"], ROOT, "public W Release build");
  requireSuccess(build, "public W Release build");
  await regularFile(PUBLIC_W_EXECUTABLE, "public W compiler");
  await regularFile(PUBLIC_W_RECEIPT, "public W build receipt");
  let receipt;
  try {
    receipt = JSON.parse(await readFile(PUBLIC_W_RECEIPT, "utf8"));
  } catch (error) {
    fail(`public W build receipt is not valid JSON: ${error.message}`);
  }
  if (receipt?.profile?.selected !== "release") fail("public W compiler must be built with the Release profile");
  const digest = await sha256File(PUBLIC_W_EXECUTABLE);
  if (receipt?.artifact?.sha256 !== digest.slice("sha256:".length)) fail("public W build receipt does not match w.exe");
  const outputEntries = (await readdir(PUBLIC_W_BUILD_DIRECTORY)).sort();
  if (JSON.stringify(outputEntries) !== JSON.stringify(["receipt.json", "w.exe"])) {
    fail(`public W build produced unexpected sidecars: ${outputEntries.join(", ")}`);
  }
  return {
    executable: PUBLIC_W_EXECUTABLE,
    digest,
    receiptDigest: await sha256File(PUBLIC_W_RECEIPT),
    compilerVersion: receipt.compiler?.version,
  };
}

function normalizePublicW(value) {
  if (!isObject(value) || typeof value.executable !== "string" || !path.isAbsolute(value.executable) ||
      typeof value.digest !== "string" || !/^sha256:[0-9a-f]{64}$/u.test(value.digest)) {
    fail("public W compiler must identify an absolute executable and its sha256 digest");
  }
  if (value.receiptDigest !== undefined && (typeof value.receiptDigest !== "string" || !/^sha256:[0-9a-f]{64}$/u.test(value.receiptDigest))) {
    fail("public W build receipt digest must be a sha256 digest");
  }
  if (value.compilerVersion !== undefined && (typeof value.compilerVersion !== "string" || value.compilerVersion.trim() === "")) {
    fail("public W compiler version must be a non-empty string");
  }
  return value;
}

function publicWToolchainIdentity(publicW) {
  const digestSlug = publicW.digest.slice("sha256:".length);
  return `w-public-build-release-${digestSlug}-${EXECUTABLE_ARTIFACT_TARGET_MSVC}`;
}

function identityToken(value, label) {
  const token = String(value ?? "").trim().toLowerCase().replace(/[^a-z0-9._-]+/gu, "-");
  if (!/^[a-z0-9][a-z0-9._-]*$/u.test(token)) fail(`${label} did not produce a safe toolchain identity`);
  return token;
}

function parseGccVersion(output) {
  const match = String(output).match(/(?:gcc|clang)[^\r\n]*?([0-9]+\.[0-9]+(?:\.[0-9]+)?)/iu);
  if (!match) fail("C compiler did not disclose a parseable version");
  return match[1];
}

function parseRustVersion(output) {
  const release = String(output).match(/^release:\s*([^\r\n]+)$/mu)?.[1]?.trim();
  const host = String(output).match(/^host:\s*([^\r\n]+)$/mu)?.[1]?.trim();
  if (!release || !host) fail("Rust compiler did not disclose release and host metadata");
  return { release, host };
}

function normalizeCDialect(dialect) {
  if (!dialect || !["-std=c23", "-std=c2x"].includes(dialect.flag)) fail("C toolchain dialect must be the probed -std=c23 or -std=c2x flag");
  return {
    flag: dialect.flag,
    name: dialect.flag === "-std=c23" ? "C23" : "c2x-preview",
    final: dialect.flag === "-std=c23",
  };
}

function normalizeCompilerCommandOverride(override, language) {
  if (typeof override === "string") return { command: override };
  if (!isObject(override) || Object.keys(override).length !== 1 || typeof override.command !== "string" || override.command.length === 0) {
    fail(`${language} compiler override must identify only a compiler command; metadata must come from compiler probes`);
  }
  return { command: override.command };
}

async function resolveCCompiler(executor, dependencies = {}, target = DEFAULT_TARGET) {
  const privateComposite = target === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID;
  const override = dependencies.testOnlyToolchains?.c;
  let candidates;
  if (override !== undefined) {
    candidates = [normalizeCompilerCommandOverride(override, "C")];
  } else if (privateComposite) {
    candidates = PRIVATE_C_COMPILER_NAMES.map((name) => Bun.which(name)).filter(Boolean);
  } else {
    const installed = process.env.ProgramFiles
      ? path.resolve(process.env.ProgramFiles, "LLVM", "bin", "clang.exe")
      : undefined;
    candidates = [Bun.which("clang"), installed].filter(Boolean);
  }
  const expectedTarget = privateComposite ? EXECUTABLE_ARTIFACT_TARGET_MINGW : CLANG_C_TARGET;
  const seen = new Set();
  for (const candidate of candidates) {
    const info = typeof candidate === "string" ? { command: candidate } : candidate;
    if (seen.has(info.command)) continue;
    seen.add(info.command);
    if (!(override !== undefined && dependencies.testOnly === true)) {
      try {
        await regularFile(info.command, privateComposite ? "GCC/MinGW compiler" : "Clang/MSVC compiler");
      } catch (error) {
        if (override !== undefined) throw error;
        continue;
      }
    }
    const targetProbe = await executeChild(executor, info.command, ["-dumpmachine"], { cwd: ROOT, stdout: "pipe", stderr: "pipe", windowsHide: true }, `${target} C target probe`);
    const targetTriple = targetProbe.exitCode === 0 ? outputText(targetProbe.stdout).trim() : "";
    if (targetTriple !== expectedTarget) continue;
    const dialectProbe = await probeCDialect(info.command, {
      executor: (command, args, options) => executeChild(executor, command, args, options, `${target} C dialect probe`),
    });
    const dialect = dialectProbe ? normalizeCDialect(dialectProbe) : undefined;
    if (!dialect || (!privateComposite && !dialect.final)) continue;
    const wholeProgram = privateComposite && await probeCFlag(info.command, C_WHOLE_PROGRAM_FLAG, {
        args: [dialect.flag],
        executor: (command, args, options) => executeChild(executor, command, args, options, `${target} C release flag probe`),
      });
    const versionProbe = await executeChild(executor, info.command, ["--version"], { cwd: ROOT, stdout: "pipe", stderr: "pipe", windowsHide: true }, `${target} C compiler probe`);
    requireSuccess(versionProbe, `${target} C compiler probe`);
    const version = parseGccVersion(outputText(Buffer.concat([versionProbe.stdout, versionProbe.stderr])));
    const compilerName = path.basename(info.command).replace(/\.exe$/iu, "").toLowerCase();
    if (!privateComposite && !compilerName.includes("clang")) continue;
    const family = privateComposite ? "gcc-mingw" : "clang-msvc";
    let environment;
    if (!privateComposite) {
      if (dependencies.testOnly === true) {
        environment = dependencies.testOnlyCEnvironment;
        if (!isObject(environment)) fail("test-only public C requires an explicit Visual Studio environment fixture");
      } else {
        environment = captureVisualStudioEnvironment(findVisualStudio().devCommand);
      }
    }
    const recipeKind = privateComposite && wholeProgram ? "whole-program" : "portable";
    const identity = `${identityToken(compilerName, "C compiler")}-${identityToken(version, "C compiler version")}-${identityToken(dialect.name, "C dialect")}-${identityToken(recipeKind, "C release recipe")}-${identityToken(expectedTarget, "C ABI")}`;
    console.error(`executable benchmark: C compiler=${identity}; standard=${dialectDisclosure(dialect)}; ABI=${expectedTarget}`);
    return {
      language: "c",
      command: info.command,
      target: expectedTarget,
      version,
      dialect,
      wholeProgram,
      family,
      environment,
      identity,
    };
  }
  if (privateComposite) fail(`C ${target} requires GCC/MinGW targeting ${EXECUTABLE_ARTIFACT_TARGET_MINGW} with -std=c23 or -std=c2x`);
  fail(`C ${target} requires Clang targeting ${CLANG_C_TARGET} with final -std=c23 support`);
}

async function resolveRustCompiler(executor, dependencies = {}, target = DEFAULT_TARGET) {
  const override = dependencies.testOnlyToolchains?.rust;
  const command = override ?? Bun.which("rustc");
  if (command === undefined) fail(`Rust ${target} requires rustc`);
  const info = normalizeCompilerCommandOverride(command, "Rust");
  const targetProbe = await executeChild(executor, info.command, ["--print", "target-libdir", `--target=${RUST_TARGET}`], { cwd: ROOT, stdout: "pipe", stderr: "pipe", windowsHide: true }, `${target} Rust target probe`);
  requireSuccess(targetProbe, `${target} Rust target probe`);
  const versionProbe = await executeChild(executor, info.command, ["--version", "--verbose"], { cwd: ROOT, stdout: "pipe", stderr: "pipe", windowsHide: true }, `${target} Rust compiler probe`);
  requireSuccess(versionProbe, `${target} Rust compiler probe`);
  const version = parseRustVersion(outputText(Buffer.concat([versionProbe.stdout, versionProbe.stderr])));
  if (version.host !== RUST_TARGET) {
    fail(`Rust compiler host must disclose ${RUST_TARGET}, got ${version.host}`);
  }
  const identity = `rustc-${identityToken(version.release, "Rust compiler version")}-edition-2024-${identityToken(RUST_TARGET, "Rust ABI")}`;
  console.error(`executable benchmark: Rust compiler=${identity}; edition=2024; ABI=${RUST_TARGET}`);
  return {
    language: "rust",
    command: info.command,
    target: RUST_TARGET,
    version: version.release,
    host: version.host,
    edition: "2024",
    identity,
  };
}

function environmentSnapshot() {
  const cpuModel = os.cpus()[0]?.model;
  if (typeof cpuModel !== "string" || cpuModel.trim().length === 0) fail("host CPU model is unavailable");
  const environment = {
    os: process.platform === "win32" ? "windows" : process.platform,
    kernel: process.platform === "win32" ? "windows-nt" : process.platform,
    cpuModel: cpuModel.trim().replace(/\s+/gu, " "),
    logicalCores: String(os.cpus().length),
    ramBytes: String(os.totalmem()),
  };
  if (!executableHostIdentity(environment)) fail("redacted environment is not accepted by the result contract");
  return environment;
}

function measurementPlatform(dependencies, publish) {
  const testPlatform = dependencies.testOnlyPlatform;
  const testToolchains = dependencies.testOnlyToolchains;
  const unsupportedMetadata = ["cCompiler", "rustCompiler", "languageToolchains", "toolchains"]
    .filter((key) => Object.prototype.hasOwnProperty.call(dependencies, key));
  if (unsupportedMetadata.length > 0) {
    fail(`compiler metadata overrides are unsupported (${unsupportedMetadata.join(", ")}); use testOnlyToolchains with command-only fixtures`);
  }
  if ((testPlatform !== undefined || testToolchains !== undefined) && (dependencies.testOnly !== true || publish)) {
    fail("test-only platform and toolchain fixtures require testOnly:true with publish:false");
  }
  if (testPlatform !== undefined && (!isObject(testPlatform) || Object.keys(testPlatform).some((key) => !["platform", "arch"].includes(key)) || typeof testPlatform.platform !== "string" || typeof testPlatform.arch !== "string")) {
    fail("testOnlyPlatform must identify only platform and arch");
  }
  if (testToolchains !== undefined && (!isObject(testToolchains) || Object.keys(testToolchains).some((key) => !["c", "rust"].includes(key)))) {
    fail("testOnlyToolchains may identify only C and Rust command fixtures");
  }
  const platform = testPlatform ?? { platform: process.platform, arch: process.arch };
  if (platform.platform !== "win32" || platform.arch !== "x64") {
    fail(`M3b measurements require Windows x86_64, got ${platform.platform}/${platform.arch}`);
  }
  return platform;
}

async function sourcePath(catalog, target, language) {
  const workload = catalog.workloads?.find((item) => item.id === target);
  if (!workload) fail(`catalog has no ${target} workload`);
  const source = workload?.sources?.find((item) => item.language === language);
  if (!source) fail(`catalog has no ${language} source for ${target}`);
  const filePath = path.resolve(ROOT, source.path);
  if (!isContained(ROOT, filePath)) fail(`${language} ${target} source escapes the repository`);
  await regularFile(filePath, `${language} ${target} source`);
  if (await sha256File(filePath) !== source.digest) fail(`${language} ${target} source digest is stale`);
  return { workload, source, filePath };
}

function isProcessHandlerLifecycle(target) {
  return target === PROCESS_HANDLER_LIFECYCLE_WORKLOAD_ID;
}

function processExecution(workload) {
  const execution = workload?.execution;
  if (!isObject(execution) || execution.kind !== PROCESS_ENTRY0_EXECUTION_KIND ||
      execution.recipeClass !== PROCESS_ENTRY0_RECIPE_CLASS ||
      JSON.stringify(execution.timedInput) !== JSON.stringify(PROCESS_ENTRY0_TIMED_INPUT) ||
      JSON.stringify(execution.correctnessInputs) !== JSON.stringify(PROCESS_ENTRY0_CORRECTNESS_INPUTS) ||
      JSON.stringify(execution.faultCases) !== JSON.stringify(PROCESS_ENTRY0_FAULT_CASES)) {
    fail("process-handler-lifecycle catalog execution descriptor is not the ratified private handler contract");
  }
  return execution;
}

function processArgumentOracle(workload) {
  const oracle = workload?.oracle;
  const expected = processArgumentOracleFor(workload?.id);
  if (!expected || !isObject(oracle) || oracle.kind !== expected.kind || oracle.status !== "source-backed" ||
      JSON.stringify(oracle.timedInput) !== JSON.stringify(expected.timedInput) ||
      !Array.isArray(oracle.cases) ||
      JSON.stringify(oracle.cases) !== JSON.stringify(expected.cases) ||
      JSON.stringify(oracle.cases.map((testCase) => testCase.arguments)) !== JSON.stringify(expected.correctnessInputs)) {
    const label = workload?.id ?? "process argument";
    fail(`${label} catalog oracle is not the ratified public argument/output contract`);
  }
  return oracle;
}

async function resolveProcessSupportSources(workload) {
  const execution = processExecution(workload);
  if (!Array.isArray(execution.supportSources) || execution.supportSources.length !== PROCESS_ENTRY0_SUPPORT_ROLES.length) {
    fail("process-handler-lifecycle support-source descriptor is incomplete");
  }
  const resolved = [];
  for (const [index, descriptor] of execution.supportSources.entries()) {
    const role = PROCESS_ENTRY0_SUPPORT_ROLES[index];
    if (!isObject(descriptor) || descriptor.role !== role || typeof descriptor.path !== "string") {
      fail(`process-handler-lifecycle support source ${role} is malformed`);
    }
    const filePath = path.resolve(ROOT, descriptor.path);
    if (!isContained(PROCESS_ENTRY0_SUPPORT_ROOT, filePath)) {
      fail(`process-handler-lifecycle support source ${role} escapes compiler/seed-c`);
    }
    const stats = await regularFile(filePath, `process-handler-lifecycle ${role}`);
    if (await sha256File(filePath) !== descriptor.digest) {
      fail(`process-handler-lifecycle ${role} source digest is stale`);
    }
    resolved.push({ role, descriptor, filePath, stats });
  }
  return resolved;
}

function processSupportSource(sources, role) {
  const source = sources.find((item) => item.role === role);
  if (!source) fail(`process-handler-lifecycle support source is missing ${role}`);
  return source;
}

function clearProcessFaultEnvironment(base = process.env) {
  const environment = { ...base };
  delete environment[PROCESS_ENTRY0_FAULT_ENV];
  return environment;
}

function processFaultEnvironment(fault) {
  const environment = clearProcessFaultEnvironment();
  environment[PROCESS_ENTRY0_FAULT_ENV] = fault;
  return environment;
}

async function prepareProcessGate(executor, dependencies = {}, publish = false) {
  if (dependencies.processGate !== undefined) {
    if (dependencies.testOnly !== true || publish) {
      fail("processGate fixtures require testOnly:true with publish:false");
    }
    if (typeof dependencies.processGate !== "string" || !path.isAbsolute(dependencies.processGate)) {
      fail("processGate must identify an absolute gate executable");
    }
    await regularFile(dependencies.processGate, "process-handler-lifecycle gate executable");
    return {
      executable: dependencies.processGate,
      digest: await sha256File(dependencies.processGate),
      buildProfile: "Release",
      prepared: "test-fixture",
    };
  }
  const cmake = Bun.which("cmake");
  const ninja = Bun.which("ninja");
  if (!cmake || !ninja) fail("process-handler-lifecycle requires CMake and Ninja for the existing build/");
  const buildNinja = path.resolve(ROOT, "build", "build.ninja");
  await regularFile(buildNinja, "existing process-handler-lifecycle build/build.ninja");
  const buildDescription = await readFile(buildNinja, "utf8");
  if (!buildDescription.includes(PROCESS_ENTRY0_GATE_TARGET)) {
    fail("existing build has no process-entry0 gate target for process-handler-lifecycle; regenerate build/ before benchmarking");
  }
  const buildCache = path.resolve(ROOT, "build", "CMakeCache.txt");
  await regularFile(buildCache, "existing process-handler-lifecycle build/CMakeCache.txt");
  const buildCacheText = await readFile(buildCache, "utf8");
  if (!/^CMAKE_BUILD_TYPE:STRING=Release$/mu.test(buildCacheText)) {
    fail("process-handler-lifecycle requires the existing single-config build/ CMAKE_BUILD_TYPE=Release");
  }
  const build = await timedStep(executor, cmake, [
    "--build", path.resolve(ROOT, "build"), "--target", PROCESS_ENTRY0_GATE_TARGET,
    "--", "-j", "2",
  ], ROOT, "process-handler-lifecycle Release gate build");
  requireSuccess(build, "process-handler-lifecycle Release gate build");
  await regularFile(PROCESS_ENTRY0_GATE, "process-handler-lifecycle gate executable");
  return {
    executable: PROCESS_ENTRY0_GATE,
    digest: await sha256File(PROCESS_ENTRY0_GATE),
    buildProfile: "Release",
    prepared: "existing-build",
  };
}

function verifyProcessHandlerArtifact(bytes, label = "process-entry0 MLIR") {
  const text = bytes.toString("utf8");
  if (!text.startsWith("// w-seed-mlir0-process-handler-1\n")) fail(`${label} has no process schema marker`);
  if (!text.includes(`llvm.target_triple = \"${RUST_TARGET}\"`)) fail(`${label} has the wrong target triple`);
  const signature = "llvm.func @w_seed_process_entry0_handler(%arguments: !llvm.ptr, %context: !llvm.ptr) -> i32";
  if (!text.includes(signature)) fail(`${label} has the wrong handler signature`);
  const contextCall = "llvm.call @w_seed_process_entry0_context_drop(%context)";
  const argumentsCall = "llvm.call @w_seed_process_entry0_arguments_drop(%arguments)";
  if (text.split(contextCall).length - 1 !== 1 || text.split(argumentsCall).length - 1 !== 1 ||
      text.indexOf(contextCall) >= text.indexOf(argumentsCall)) {
    fail(`${label} does not release context before arguments exactly once`);
  }
  if (!text.includes("llvm.intr.trap") || !text.includes("llvm.unreachable") ||
      !text.includes("llvm.return %zero : i32")) {
    fail(`${label} does not check both adapter statuses before returning`);
  }
  for (const forbidden of [
    "@main", "mainCRTStartup", "GetStdHandle", "WriteFile", "ExitProcess",
    "w_seed_process0", "root_finalize", "entry_invoke", "llvm.mlir.global",
  ]) {
    if (text.includes(forbidden)) fail(`${label} contains forbidden ${forbidden}`);
  }
  return text;
}

async function processSampleFiles(directory, paths, label) {
  const expected = new Set(paths.map((item) => path.basename(item)));
  const produced = (await readdir(directory)).sort();
  const expectedNames = [...expected].sort();
  if (JSON.stringify(produced) !== JSON.stringify(expectedNames)) {
    fail(`${label} produced unexpected sidecars or intermediates: ${produced.join(", ")}`);
  }
}

function processCCompileArgs(toolchain, sourcePath, objectPath) {
  return [
    ...dialectArgs(toolchain.dialect),
    ...cReleaseFlags({ wholeProgram: false }),
    "-I", PROCESS_ENTRY0_INCLUDE_DIRECTORY,
    "-c", sourcePath, "-o", objectPath,
  ];
}

function processLinkArgs(objectPaths, artifact) {
  return [
    ...cReleaseFlags({ wholeProgram: false }),
    "-static", "-static-libgcc", "-o", artifact, ...objectPaths,
  ];
}

async function compileProcessHandler(context, retain) {
  const sampleDirectory = await mkdtemp(path.join(context.tempRoot, SAMPLE_DIRECTORY_PREFIX));
  const artifact = path.join(sampleDirectory, `${context.source.workload.id}-${context.language}.exe`);
  const handlerObject = path.join(sampleDirectory, "process-entry0-handler.obj");
  const harnessObject = path.join(sampleDirectory, "process-entry0-harness.obj");
  const providerObject = path.join(sampleDirectory, "process-entry0-provider.obj");
  const handlerSteps = [];
  const compileSteps = [];
  try {
    if (context.language === "w") {
      const rawPath = path.join(sampleDirectory, "process-entry0.mlir");
      const verifiedPath = path.join(sampleDirectory, "process-entry0.verified.mlir");
      const llvmPath = path.join(sampleDirectory, "process-entry0.ll");
      const gate = await timedStep(context.executor, context.processGate.executable, [context.source.filePath], sampleDirectory, "W process handler gate", { env: clearProcessFaultEnvironment() });
      requireSuccess(gate, "W process handler gate");
      if (gate.stderr.length !== 0 || gate.stdout.length === 0) fail("W process handler gate did not emit exactly one MLIR artifact");
      verifyProcessHandlerArtifact(gate.stdout, "W process handler MLIR");
      await writeFile(rawPath, gate.stdout);
      handlerSteps.push(gate);

      const optimized = await timedStep(context.executor, context.tools["mlir-opt.exe"], [
        rawPath, "-o", verifiedPath, ...W_MLIR_OPT_FLAGS,
      ], sampleDirectory, "W process handler mlir-opt", { env: clearProcessFaultEnvironment() });
      requireSuccess(optimized, "W process handler mlir-opt");
      handlerSteps.push(optimized);

      const translated = await timedStep(context.executor, context.tools["mlir-translate.exe"], [
        "--mlir-to-llvmir", verifiedPath, "-o", llvmPath,
      ], sampleDirectory, "W process handler mlir-translate", { env: clearProcessFaultEnvironment() });
      requireSuccess(translated, "W process handler mlir-translate");
      handlerSteps.push(translated);

      const lowered = await timedStep(context.executor, context.tools["llc.exe"], [
        ...W_LLC_FLAGS, llvmPath, "-o", handlerObject,
      ], sampleDirectory, "W process handler llc", { env: clearProcessFaultEnvironment() });
      requireSuccess(lowered, "W process handler llc");
      handlerSteps.push(lowered);
      await regularFile(rawPath, "W process handler raw MLIR");
      await regularFile(verifiedPath, "W process handler verified MLIR");
      await regularFile(llvmPath, "W process handler LLVM IR");
    } else if (context.language === "c") {
      const handler = await timedStep(context.executor, context.processLinker.command,
        processCCompileArgs(context.processLinker, context.source.filePath, handlerObject),
        sampleDirectory, "C process handler", { env: clearProcessFaultEnvironment() });
      requireSuccess(handler, "C process handler");
      handlerSteps.push(handler);
    } else if (context.language === "rust") {
      const handler = await timedStep(context.executor, context.languageToolchain.command, [
        context.source.filePath,
        "--edition=2024",
        "--crate-type=lib",
        ...PROCESS_ENTRY0_RUST_OBJECT_FLAGS,
        `--target=${RUST_TARGET}`,
        "--emit=obj",
        "-o", handlerObject,
      ], sampleDirectory, "Rust process handler", { env: clearProcessFaultEnvironment() });
      requireSuccess(handler, "Rust process handler");
      handlerSteps.push(handler);
    } else {
      fail(`unsupported process-handler-lifecycle language: ${context.language}`);
    }

    const harness = processSupportSource(context.processSupportSources, "harness-c");
    const provider = processSupportSource(context.processSupportSources, "provider-c");
    const harnessStep = await timedStep(context.executor, context.processLinker.command,
      processCCompileArgs(context.processLinker, harness.filePath, harnessObject),
      sampleDirectory, "PROCESS0 harness", { env: clearProcessFaultEnvironment() });
    requireSuccess(harnessStep, "PROCESS0 harness");
    compileSteps.push(harnessStep);
    const providerStep = await timedStep(context.executor, context.processLinker.command,
      processCCompileArgs(context.processLinker, provider.filePath, providerObject),
      sampleDirectory, "PROCESS0 provider", { env: clearProcessFaultEnvironment() });
    requireSuccess(providerStep, "PROCESS0 provider");
    compileSteps.push(providerStep);
    const linkStep = await timedStep(context.executor, context.processLinker.command,
      processLinkArgs([handlerObject, harnessObject, providerObject], artifact),
      sampleDirectory, "PROCESS0 private handler link", { env: clearProcessFaultEnvironment() });
    requireSuccess(linkStep, "PROCESS0 private handler link");
    compileSteps.push(linkStep);
    const allSteps = [...handlerSteps, ...compileSteps];
    const first = allSteps[0];
    const last = allSteps[allSteps.length - 1];
    const sample = chainSample(allSteps, first.start, last.end, "process-entry0 composite compile");
    const artifactStats = await regularFile(artifact, "process-entry0 PE artifact");
    if (artifactStats.size <= 0) fail("process-entry0 PE artifact is empty");
    const intermediatePaths = [artifact, handlerObject, harnessObject, providerObject];
    if (context.language === "w") intermediatePaths.push(
      path.join(sampleDirectory, "process-entry0.mlir"),
      path.join(sampleDirectory, "process-entry0.verified.mlir"),
      path.join(sampleDirectory, "process-entry0.ll"),
    );
    await processSampleFiles(sampleDirectory, intermediatePaths, "process-entry0 composite compile");
    if (retain) return { sampleDirectory, artifact, sample };
    await rm(sampleDirectory, { recursive: true, force: true });
    return { sampleDirectory: undefined, artifact: undefined, sample };
  } catch (error) {
    await rm(sampleDirectory, { recursive: true, force: true });
    throw error;
  }
}

async function compileW(context, retain) {
  const sampleDirectory = await mkdtemp(path.join(context.tempRoot, SAMPLE_DIRECTORY_PREFIX));
  const artifact = path.join(sampleDirectory, `${context.source.workload.id}-${context.language}.exe`);
  try {
    const step = await timedStep(context.executor, context.publicW.executable, [
      "build",
      context.source.filePath,
      "--target",
      EXECUTABLE_ARTIFACT_TARGET_MSVC,
      "--output",
      artifact,
    ], sampleDirectory, "W public build");
    requireSuccess(step, "W public build");
    const stats = await regularFile(artifact, "W PE artifact");
    if (stats.size <= 0) fail("W PE artifact is empty");
    await assertSidecarFree(sampleDirectory, artifact, "W public build");
    const sample = chainSample([step], step.start, step.end, "W public build");
    if (retain) return { sampleDirectory, artifact, sample };
    await rm(sampleDirectory, { recursive: true, force: true });
    return { sampleDirectory: undefined, artifact: undefined, sample };
  } catch (error) {
    await rm(sampleDirectory, { recursive: true, force: true });
    throw error;
  }
}

async function compileC(context, retain) {
  const sampleDirectory = await mkdtemp(path.join(context.tempRoot, SAMPLE_DIRECTORY_PREFIX));
  const artifact = path.join(sampleDirectory, `${context.source.workload.id}-${context.language}.exe`);
  try {
    const start = process.hrtime.bigint();
    const step = await timedStep(context.executor, context.languageToolchain.command, [
      ...dialectArgs(context.languageToolchain.dialect),
      ...(context.languageToolchain.family === "clang-msvc"
        ? clangReleaseFlags()
        : cReleaseFlags(context.languageToolchain)),
      context.source.filePath,
      "-o", artifact,
    ], sampleDirectory, "C compiler", { env: context.languageToolchain.environment });
    requireSuccess(step, "C compiler");
    const stats = await regularFile(artifact, "C PE artifact");
    if (stats.size <= 0) fail("C PE artifact is empty");
    await assertSidecarFree(sampleDirectory, artifact, "C compiler");
    const end = process.hrtime.bigint();
    const sample = chainSample([step], start, end, "C compile");
    if (retain) return { sampleDirectory, artifact, sample };
    await rm(sampleDirectory, { recursive: true, force: true });
    return { sampleDirectory: undefined, artifact: undefined, sample };
  } catch (error) {
    await rm(sampleDirectory, { recursive: true, force: true });
    throw error;
  }
}

async function compileRust(context, retain) {
  const sampleDirectory = await mkdtemp(path.join(context.tempRoot, SAMPLE_DIRECTORY_PREFIX));
  const artifact = path.join(sampleDirectory, `${context.source.workload.id}-${context.language}.exe`);
  try {
    const start = process.hrtime.bigint();
    const step = await timedStep(context.executor, context.languageToolchain.command, [
      context.source.filePath,
      "--edition=2024",
      ...RUST_RELEASE_FLAGS,
      `--target=${RUST_TARGET}`,
      "-o", artifact,
    ], sampleDirectory, "Rust compiler");
    requireSuccess(step, "Rust compiler");
    const stats = await regularFile(artifact, "Rust PE artifact");
    if (stats.size <= 0) fail("Rust PE artifact is empty");
    await assertSidecarFree(sampleDirectory, artifact, "Rust compiler");
    const end = process.hrtime.bigint();
    const sample = chainSample([step], start, end, "Rust compile");
    if (retain) return { sampleDirectory, artifact, sample };
    await rm(sampleDirectory, { recursive: true, force: true });
    return { sampleDirectory: undefined, artifact: undefined, sample };
  } catch (error) {
    await rm(sampleDirectory, { recursive: true, force: true });
    throw error;
  }
}

async function compileSource(context, retain) {
  if (isProcessHandlerLifecycle(context.target)) return compileProcessHandler(context, retain);
  if (context.language === "w") return compileW(context, retain);
  if (context.language === "c") return compileC(context, retain);
  if (context.language === "rust") return compileRust(context, retain);
  fail(`unsupported language: ${context.language}`);
}

async function runArtifact(executor, artifact, language = "w", target = DEFAULT_TARGET, argumentsVector = []) {
  if (!Array.isArray(argumentsVector) || argumentsVector.some((item) => typeof item !== "string")) {
    fail(`${language} ${target} executable arguments must contain only strings`);
  }
  const step = await timedStep(executor, artifact, argumentsVector, path.dirname(artifact), `${language} ${target} run`);
  if (!isProcessArgumentWorkload(target)) requireSuccess(step, `${language} ${target} executable`);
  return {
    sample: sampleFrom(step.start, step.end, [step.usage], `${language} ${target} run`),
    stdout: step.stdout,
    stderr: step.stderr,
    exitCode: step.exitCode,
  };
}

async function runProcessArtifact(context, artifact, argumentsVector, label) {
  if (!Array.isArray(argumentsVector) || argumentsVector.some((item) => typeof item !== "string")) {
    fail(`${label} runtime vector must contain only strings`);
  }
  const step = await timedStep(context.executor, artifact, argumentsVector,
    path.dirname(artifact), label, { env: clearProcessFaultEnvironment() });
  if (step.exitCode !== 0) {
    const detail = outputText(step.stderr) || outputText(step.stdout);
    fail(`${label} returned exit ${step.exitCode}: ${detail.slice(-1200)}`);
  }
  if (step.stdout.length !== 0 || step.stderr.length !== 0) {
    fail(`${label} wrote process output`);
  }
  return {
    sample: sampleFrom(step.start, step.end, [step.usage], label),
    stdout: step.stdout,
    stderr: step.stderr,
    exitCode: step.exitCode,
  };
}

async function runProcessFault(context, artifact, argumentsVector, fault, expectedExitCode, label) {
  const result = await executeChild(context.executor, artifact, argumentsVector, {
    cwd: path.dirname(artifact),
    env: processFaultEnvironment(fault),
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
  }, label);
  if (result.exitCode !== expectedExitCode) {
    fail(`${label} returned ${String(result.exitCode)} instead of expected ${expectedExitCode}`);
  }
  if (result.stdout.length !== 0 || result.stderr.length !== 0) {
    fail(`${label} wrote process output`);
  }
  return result;
}

async function processCorrectness(context, compiled) {
  const vectors = context.processExecution.correctnessInputs;
  for (const [index, vector] of vectors.entries()) {
    await runProcessArtifact(context, compiled.artifact, vector,
      `process-handler-lifecycle correctness vector ${index === 0 ? "empty" : "timed"}`);
  }
  for (const fault of context.processExecution.faultCases) {
    const expected = fault === "missing" || fault === "noop-success"
      ? PROCESS_ENTRY0_HARNESS_FAILURE_EXIT_CODE
      : PROCESS_ENTRY0_GENERATED_TRAP_EXIT_CODE;
    await runProcessFault(context, compiled.artifact, [], fault, expected,
      `process-handler-lifecycle fault ${fault}`);
  }
}

export function assertOracle(execution, oracle, target, label) {
  if (!isObject(oracle) || !Number.isSafeInteger(oracle.exitCode) || typeof oracle.stdout !== "string" || typeof oracle.stderr !== "string") {
    fail(`${label} requires a source-backed executable oracle`);
  }
  if (execution.exitCode !== oracle.exitCode || !execution.stdout.equals(Buffer.from(oracle.stdout, "utf8")) || !execution.stderr.equals(Buffer.from(oracle.stderr, "utf8"))) {
    fail(`${label} output does not match the ${target} exact-output oracle`);
  }
}

function peRange(bytes, offset, length, label, language) {
  if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(length) || offset < 0 || length < 0 ||
      offset > bytes.length || length > bytes.length - offset) {
    fail(`${language} artifact has a truncated or invalid ${label}`);
  }
}

function peUInt16(bytes, offset, label, language) {
  peRange(bytes, offset, 2, label, language);
  return bytes.readUInt16LE(offset);
}

function peUInt32(bytes, offset, label, language) {
  peRange(bytes, offset, 4, label, language);
  return bytes.readUInt32LE(offset);
}

function isPowerOfTwo(value) {
  return value > 0 && (value & (value - 1)) === 0;
}

function peSectionName(bytes, offset, index, language) {
  peRange(bytes, offset, PE_SECTION_NAME_SIZE, `PE section ${index} name`, language);
  let length = 0;
  while (length < PE_SECTION_NAME_SIZE && bytes[offset + length] !== 0) {
    const code = bytes[offset + length];
    if (code < 0x20 || code > 0x7e) {
      fail(`${language} artifact has an invalid PE section ${index} name`);
    }
    length += 1;
  }
  if (length === 0) fail(`${language} artifact has an empty PE section ${index} name`);
  return bytes.subarray(offset, offset + length).toString("ascii");
}

function rejectOverlappingRanges(ranges, label, language) {
  const ordered = [...ranges].sort((left, right) => left.start - right.start || left.index - right.index);
  for (let index = 1; index < ordered.length; index += 1) {
    if (ordered[index].start < ordered[index - 1].end) {
      fail(`${language} artifact has overlapping ${label} ranges for sections ${ordered[index - 1].index} and ${ordered[index].index}`);
    }
  }
}

function peRvaRange(bytes, sections, rva, length, label, language) {
  if (!Number.isSafeInteger(rva) || !Number.isSafeInteger(length) || rva < 0 || length < 0) {
    fail(`${language} artifact has an invalid ${label} RVA range`);
  }
  for (const section of sections) {
    if (section.rawSize === 0 || rva < section.virtualAddress) continue;
    const sectionOffset = rva - section.virtualAddress;
    if (sectionOffset > section.rawSize || length > section.rawSize - sectionOffset) continue;
    const fileOffset = section.rawPointer + sectionOffset;
    peRange(bytes, fileOffset, length, label, language);
    return { offset: fileOffset, length };
  }
  fail(`${language} artifact has an invalid ${label} RVA range`);
}

function parsePeDebugDirectory(bytes, sections, debugRva, debugSize, language) {
  if (debugRva === 0 && debugSize === 0) {
    return { presence: "absent", sizeBytes: "0", entries: [] };
  }
  if (debugRva === 0 || debugSize === 0 || debugSize % PE_DEBUG_DIRECTORY_ENTRY_SIZE !== 0) {
    fail(`${language} artifact has an invalid PE debug data directory`);
  }
  const directory = peRvaRange(bytes, sections, debugRva, debugSize, "PE debug directory", language);
  const entries = [];
  let directoryType;
  for (let index = 0; index < debugSize / PE_DEBUG_DIRECTORY_ENTRY_SIZE; index += 1) {
    const entry = directory.offset + index * PE_DEBUG_DIRECTORY_ENTRY_SIZE;
    const characteristics = peUInt32(bytes, entry, `PE debug directory entry ${index}`, language);
    const majorVersion = peUInt16(bytes, entry + 8, `PE debug directory entry ${index}`, language);
    const minorVersion = peUInt16(bytes, entry + 10, `PE debug directory entry ${index}`, language);
    const typeCode = peUInt32(bytes, entry + 12, `PE debug directory entry ${index}`, language);
    const sizeOfData = peUInt32(bytes, entry + 16, `PE debug directory entry ${index}`, language);
    const addressOfRawData = peUInt32(bytes, entry + 20, `PE debug directory entry ${index}`, language);
    const pointerToRawData = peUInt32(bytes, entry + 24, `PE debug directory entry ${index}`, language);
    if (typeCode === PE_DEBUG_TYPE_CODEVIEW) fail(`${language} artifact contains CodeView debug data`);
    if (typeCode !== PE_DEBUG_TYPE_POGO && typeCode !== PE_DEBUG_TYPE_REPRO) {
      fail(`${language} artifact contains unsupported PE debug data type ${typeCode}`);
    }
    const entryType = typeCode === PE_DEBUG_TYPE_POGO ? "pogo" : "repro";
    if (directoryType !== undefined && directoryType !== entryType) {
      fail(`${language} artifact mixes PE debug metadata types`);
    }
    directoryType = entryType;
    if (characteristics !== 0 || majorVersion !== 0 || minorVersion !== 0) {
      fail(`${language} artifact has an invalid ${entryType.toUpperCase()} debug directory entry`);
    }
    if (typeCode === PE_DEBUG_TYPE_REPRO) {
      if (entries.length !== 0) fail(`${language} artifact contains duplicate REPRO debug markers`);
      if (sizeOfData !== 0 || pointerToRawData !== 0 || addressOfRawData !== 0) {
        fail(`${language} artifact has an invalid REPRO debug marker`);
      }
      entries.push({ type: "repro", typeCode: PE_DEBUG_TYPE_REPRO, sizeBytes: "0" });
      continue;
    }
    if (sizeOfData === 0 || pointerToRawData === 0 || addressOfRawData === 0) {
      fail(`${language} artifact has an invalid POGO debug payload`);
    }
    peRange(bytes, pointerToRawData, sizeOfData, `POGO debug payload ${index}`, language);
    const payload = peRvaRange(bytes, sections, addressOfRawData, sizeOfData, `POGO debug payload ${index}`, language);
    if (payload.offset !== pointerToRawData) fail(`${language} artifact has mismatched POGO debug payload pointers`);
    entries.push({ type: "pogo", typeCode: PE_DEBUG_TYPE_POGO, sizeBytes: String(sizeOfData) });
  }
  return { presence: `${directoryType}-only`, sizeBytes: String(debugSize), entries };
}

export function validatePeX64(bytes, language = "w") {
  if (!Buffer.isBuffer(bytes)) fail(`${language} artifact bytes must be a buffer`);
  peRange(bytes, 0, PE_DOS_HEADER_SIZE, "DOS header", language);
  if (bytes.readUInt16LE(0) !== 0x5a4d) fail(`${language} artifact is not a PE image`);

  const offset = peUInt32(bytes, 0x3c, "DOS header", language);
  if (offset < PE_DOS_HEADER_SIZE) fail(`${language} artifact has an invalid DOS header`);
  peRange(bytes, offset, 4 + PE_FILE_HEADER_SIZE, "PE/COFF header", language);
  if (bytes.readUInt32LE(offset) !== 0x00004550) fail(`${language} artifact is not a PE image`);

  const fileHeader = offset + 4;
  const machine = peUInt16(bytes, fileHeader, "COFF file header", language);
  if (machine !== 0x8664) fail(`${language} artifact is not PE x64`);
  const sectionCount = peUInt16(bytes, fileHeader + 2, "COFF file header", language);
  if (sectionCount === 0) fail(`${language} artifact has no PE sections`);
  const symbolTablePointer = peUInt32(bytes, fileHeader + 8, "COFF file header", language);
  const symbolCount = peUInt32(bytes, fileHeader + 12, "COFF file header", language);
  if (symbolTablePointer !== 0 || symbolCount !== 0) fail(`${language} artifact contains a COFF symbol table`);
  const optionalHeaderSize = peUInt16(bytes, fileHeader + 16, "COFF file header", language);
  const optionalHeader = fileHeader + PE_FILE_HEADER_SIZE;
  if (optionalHeaderSize < PE_OPTIONAL_HEADER_MINIMUM_SIZE) {
    fail(`${language} artifact optional header is too small to prove PE debug cleanliness`);
  }
  peRange(bytes, optionalHeader, optionalHeaderSize, "PE optional header", language);
  if (peUInt16(bytes, optionalHeader, "PE optional header", language) !== 0x20b) {
    fail(`${language} artifact is not PE32+`);
  }

  const directoryCount = peUInt32(
    bytes,
    optionalHeader + PE_OPTIONAL_HEADER_DATA_DIRECTORY_OFFSET,
    "PE optional header data-directory count",
    language,
  );
  if (directoryCount < PE_REQUIRED_DIRECTORY_COUNT || directoryCount > PE_MAX_DIRECTORY_COUNT) {
    fail(`${language} artifact has an invalid PE data-directory count`);
  }
  const directoryBytes = directoryCount * PE_DATA_DIRECTORY_ENTRY_SIZE;
  const directoryStart = optionalHeader + PE_DATA_DIRECTORY_OFFSET;
  peRange(bytes, directoryStart, directoryBytes, "PE data directories", language);
  if (directoryStart + directoryBytes > optionalHeader + optionalHeaderSize) {
    fail(`${language} artifact has truncated PE data directories`);
  }

  const debugDirectory = directoryStart + PE_DEBUG_DIRECTORY_INDEX * PE_DATA_DIRECTORY_ENTRY_SIZE;
  const debugRva = peUInt32(bytes, debugDirectory, "PE debug data directory", language);
  const debugSize = peUInt32(bytes, debugDirectory + 4, "PE debug data directory", language);

  const securityDirectory = directoryStart + PE_SECURITY_DIRECTORY_INDEX * PE_DATA_DIRECTORY_ENTRY_SIZE;
  const securityPointer = peUInt32(bytes, securityDirectory, "PE certificate data directory", language);
  const securitySize = peUInt32(bytes, securityDirectory + 4, "PE certificate data directory", language);
  if (securityPointer !== 0 || securitySize !== 0) {
    fail(`${language} artifact contains a PE certificate directory`);
  }

  const sectionAlignment = peUInt32(
    bytes,
    optionalHeader + PE_OPTIONAL_HEADER_SECTION_ALIGNMENT_OFFSET,
    "PE optional header section alignment",
    language,
  );
  const fileAlignment = peUInt32(
    bytes,
    optionalHeader + PE_OPTIONAL_HEADER_FILE_ALIGNMENT_OFFSET,
    "PE optional header file alignment",
    language,
  );
  const sizeOfHeaders = peUInt32(
    bytes,
    optionalHeader + PE_OPTIONAL_HEADER_SIZE_OF_HEADERS_OFFSET,
    "PE optional header",
    language,
  );
  if (!isPowerOfTwo(fileAlignment) || fileAlignment > 65_536) {
    fail(`${language} artifact has an invalid PE file alignment`);
  }
  if (!isPowerOfTwo(sectionAlignment) || sectionAlignment < fileAlignment) {
    fail(`${language} artifact has an invalid PE section alignment`);
  }
  if ((sectionAlignment >= 4096 && fileAlignment < 512) ||
      (sectionAlignment < 4096 && sectionAlignment !== fileAlignment)) {
    fail(`${language} artifact has an incompatible PE alignment pair`);
  }
  if (sizeOfHeaders === 0 || sizeOfHeaders > bytes.length) fail(`${language} artifact has invalid PE headers size`);
  if (sizeOfHeaders % fileAlignment !== 0) fail(`${language} artifact PE headers size is not file-aligned`);
  const sectionTable = optionalHeader + optionalHeaderSize;
  const sectionTableBytes = sectionCount * PE_SECTION_HEADER_SIZE;
  peRange(bytes, sectionTable, sectionTableBytes, "PE section table", language);
  const sectionTableEnd = sectionTable + sectionTableBytes;
  if (sizeOfHeaders < sectionTableEnd) fail(`${language} artifact has invalid PE headers size`);

  const sections = [];
  const rawRanges = [];
  const virtualRanges = [];
  let rawEnd = sizeOfHeaders;
  for (let index = 0; index < sectionCount; index += 1) {
    const section = sectionTable + index * PE_SECTION_HEADER_SIZE;
    const name = peSectionName(bytes, section, index, language);
    const virtualSize = peUInt32(bytes, section + 8, `PE section ${index} header`, language);
    const virtualAddress = peUInt32(bytes, section + 12, `PE section ${index} header`, language);
    const rawSize = peUInt32(bytes, section + 16, `PE section ${index} header`, language);
    const rawPointer = peUInt32(bytes, section + 20, `PE section ${index} header`, language);
    if (virtualAddress === 0 || virtualAddress % sectionAlignment !== 0) {
      fail(`${language} artifact section ${index} virtual address is not section-aligned`);
    }
    const virtualSpan = Math.max(virtualSize, rawSize);
    if (virtualAddress + virtualSpan > 0x1_0000_0000) {
      fail(`${language} artifact section ${index} virtual range overflows PE32+ address space`);
    }
    if (virtualSpan > 0) virtualRanges.push({ index, start: virtualAddress, end: virtualAddress + virtualSpan });
    sections.push({ name, virtualSize, virtualAddress, rawSize, rawPointer });
    if (rawSize === 0) {
      if (rawPointer !== 0) fail(`${language} artifact section ${index} has a raw pointer without raw data`);
      continue;
    }
    if (rawPointer < sizeOfHeaders) fail(`${language} artifact section ${index} overlaps PE headers`);
    if (sectionAlignment < 4096 && rawPointer !== virtualAddress) {
      fail(`${language} artifact section ${index} low-alignment raw pointer must equal its virtual address`);
    }
    if (rawPointer % fileAlignment !== 0 || rawSize % fileAlignment !== 0) {
      fail(`${language} artifact section ${index} raw range is not file-aligned`);
    }
    peRange(bytes, rawPointer, rawSize, `PE section ${index} raw data`, language);
    rawRanges.push({ index, start: rawPointer, end: rawPointer + rawSize });
    rawEnd = Math.max(rawEnd, rawPointer + rawSize);
  }
  rejectOverlappingRanges(rawRanges, "PE section raw", language);
  rejectOverlappingRanges(virtualRanges, "PE section virtual", language);
  if (bytes.length > rawEnd) fail(`${language} artifact contains overlay bytes`);

  return {
    cleanliness: {
      coffSymbols: { pointer: String(symbolTablePointer), count: String(symbolCount) },
      codeView: { count: "0", sizeBytes: "0" },
      debugDirectory: parsePeDebugDirectory(bytes, sections, debugRva, debugSize, language),
      certificateDirectory: { pointer: String(securityPointer), sizeBytes: String(securitySize) },
      sectionData: "in-bounds",
      overlay: { sizeBytes: "0" },
    },
    peLayout: {
      fileAlignment: String(fileAlignment),
      sectionAlignment: String(sectionAlignment),
      sizeOfHeaders: String(sizeOfHeaders),
      sections: sections.map(({ name, virtualSize, rawSize }) => ({
        name,
        virtualSize: String(virtualSize),
        rawSize: String(rawSize),
      })),
    },
  };
}

async function correctnessBuild(context) {
  const compiled = await compileSource(context, true);
  try {
    const bytes = await readFile(compiled.artifact);
    const validatedPe = validatePeX64(bytes, context.language);
    const artifactCleanliness = validatedPe.cleanliness;
    const artifactPeLayout = validatedPe.peLayout;
    artifactCleanliness.sidecars = { count: "0" };
    if (isProcessHandlerLifecycle(context.target)) {
      await processCorrectness(context, compiled);
      return {
        compiled,
        artifactDigest: sha256Bytes(bytes),
        artifactSizeBytes: String(bytes.length),
        artifactCleanliness,
        artifactPeLayout,
      };
    }
    const oracle = isProcessArgumentWorkload(context.target)
      ? processArgumentOracle(context.source.workload)
      : context.source.workload.oracle;
    if (isProcessArgumentWorkload(context.target)) {
      for (const [index, testCase] of oracle.cases.entries()) {
        const execution = await runArtifact(context.executor, compiled.artifact, context.language, context.target, testCase.arguments);
        assertOracle(execution, testCase, context.target, `${context.language} ${context.target} correctness case ${index}`);
      }
    } else {
      const execution = await runArtifact(context.executor, compiled.artifact, context.language, context.target);
      assertOracle(execution, oracle, context.target, `${context.language} ${context.target} correctness`);
    }
    return {
      compiled,
      artifactDigest: sha256Bytes(bytes),
      artifactSizeBytes: String(bytes.length),
      artifactCleanliness,
      artifactPeLayout,
    };
  } catch (error) {
    await rm(compiled.sampleDirectory, { recursive: true, force: true });
    throw error;
  }
}

function protocol(context, workload = undefined) {
  const { language } = context;
  const compileScope = language === "w"
    ? "W compile wall-clock spans the complete direct w.exe build interval, including its compiler descendants; direct-process CPU/RSS counters cover w.exe only, are non-comparable to C/Rust until process-tree accounting exists, and child process-tree counters are unavailable."
    : `${language} compile measures the direct compiler process only; compiler descendants are not aggregated.`;
  const processEntry = isProcessArgumentWorkload(workload?.id);
  const runScope = context.nativeBenchmark
    ? "Production run CPU covers the complete contained Job tree and peak working set covers the root target process."
    : "Test-only run observations cover the direct target process; descendants are not aggregated.";
  const argumentContract = processEntry ? processArgumentOracleFor(workload.id) : undefined;
  const argumentCaseLabels = argumentContract?.cases?.map((testCase) => {
    if (testCase.arguments.length === 0) return "no-argument";
    if (testCase.arguments.length === 1 && testCase.arguments[0] === "") return "empty-argument";
    if (testCase.arguments.length === 1 && testCase.arguments[0] === "payload") return "payload";
    return `${testCase.arguments.length}-argument`;
  }) ?? [];
  const argumentCases = argumentCaseLabels.length > 1
    ? `${argumentCaseLabels.slice(0, -1).join(", ")} and ${argumentCaseLabels.at(-1)}`
    : argumentCaseLabels[0];
  const timedVector = argumentContract?.timedInput ?? [];
  const timedVectorText = `[${timedVector.join(", ")}]`;
  return {
    warmupMinimum: 1,
    rawMinimum: 9,
    rawParity: "odd",
    arithmeticMeanRounding: "floor-integer",
    stopRule: "fixed-count",
    wallClock: "monotonic-nanoseconds",
    processIsolation: "fresh-process-per-sample",
    order: "compile-series-then-run-series",
    measurementKernel: context.nativeBenchmark?.abi ?? "bun-direct-test/1",
    resourceScope: processEntry
      ? `${compileScope} ${runScope} Correctness executes the ${argumentCases} cases before timing; runtime samples use the pinned ${timedVectorText} vector.`
      : `${compileScope} ${runScope}`,
    knownNoiseControls: processEntry
      ? ["warmup-discarded", "fresh-process-per-sample", "fixed-variant-order", "pinned-runtime-vector", "correctness-before-timing"]
      : ["warmup-discarded", "fresh-process-per-sample", "fixed-variant-order"],
    unknownNoiseControls: ["host-scheduler", "filesystem-cache", "thermal-state"],
    directProcessDisclosure: context.nativeBenchmark
      ? "Runtime wall time uses Windows QPC; CPU uses aggregate Job Object user/kernel accounting normalized to floor microseconds; peak working set is the root process, while Job peak commit remains a distinct receipt fact and is not mislabeled as RSS. Compile samples remain Bun direct-process observations."
      : `Bun direct-process CPU and RSS counters cover spawned processes only; process-tree CPU/RSS are not aggregated. ${EXECUTABLE_TIMEOUT_STATUS}.`,
  };
}

function processProtocol(context) {
  const compileScope = "Private process-handler-lifecycle compile wall-clock spans the complete selected handler pipeline (W source gate through Native0/HIR16/MLIR, MLIR optimization, LLVM translation, object lowering, or the direct C/Rust handler object) plus fresh shared PROCESS0 harness/provider compilation and the final GCC PE link. Direct child CPU counters are summed and peak RSS is the maximum across these explicit pipeline steps; descendants of any child are not aggregated.";
  return {
    warmupMinimum: 1,
    rawMinimum: 9,
    rawParity: "odd",
    arithmeticMeanRounding: "floor-integer",
    stopRule: "fixed-count",
    wallClock: "monotonic-nanoseconds",
    processIsolation: "fresh-process-per-sample",
    order: "compile-series-then-run-series",
    measurementKernel: context.nativeBenchmark?.abi ?? "bun-direct-test/1",
    resourceScope: `${compileScope} Runtime samples execute the final private handler PE directly with the pinned [alpha, payload] vector; the empty vector is correctness-only. Fault witnesses are correctness-only and are not timed.`,
    knownNoiseControls: [
      "warmup-discarded",
      "fresh-process-per-sample",
      "fixed-variant-order",
      "pinned-runtime-vector",
      "fault-environment-cleared-for-timed-runs",
    ],
    unknownNoiseControls: ["host-scheduler", "filesystem-cache", "thermal-state"],
    directProcessDisclosure: context.nativeBenchmark
      ? `Runtime wall time uses Windows QPC; CPU aggregates the contained Job Object, peak working set describes the root PE, and Job peak commit remains a distinct receipt fact rather than RSS. Compile samples remain Bun direct-child observations. The final artifact target is ${EXECUTABLE_ARTIFACT_TARGET_MINGW}; W and Rust handler objects originate from ${RUST_TARGET}, while the private composite uses a GCC MinGW C ABI link and is contextual/non-ranking, not a production MSVC CRT claim.`
      : `Bun direct-process CPU and RSS counters cover each spawned compiler, lowering tool, linker and runtime process only; process-tree CPU/RSS are not aggregated. The final artifact target is ${EXECUTABLE_ARTIFACT_TARGET_MINGW}; W and Rust handler objects originate from ${RUST_TARGET}, while the private composite uses a GCC MinGW C ABI link and is contextual/non-ranking, not a production MSVC CRT claim. ${EXECUTABLE_TIMEOUT_STATUS}`,
  };
}

function recipeFor(context) {
  if (isProcessHandlerLifecycle(context.target)) {
    const sharedFlags = [
      ...dialectArgs(context.processLinker.dialect),
      ...cReleaseFlags({ wholeProgram: false }),
    ];
    const sourceRecipe = context.source.source.recipe;
    const handlerOriginTarget = context.language === "w"
      ? RUST_TARGET
      : context.language === "rust" ? RUST_TARGET : PROCESS_ENTRY0_GCC_ORIGIN_TARGET;
    const handler = context.language === "w"
      ? {
        kind: "native0-mlir0-handler-object",
        gateTarget: PROCESS_ENTRY0_GATE_TARGET,
        gateProfile: "Release",
        mlirOptFlags: [...W_MLIR_OPT_FLAGS],
        mlirTranslateFlags: ["--mlir-to-llvmir"],
        llcFlags: [...W_LLC_FLAGS],
        originTarget: handlerOriginTarget,
      }
      : context.language === "rust"
        ? {
          kind: "rustc-handler-object",
          flags: ["--edition=2024", "--crate-type=lib", ...PROCESS_ENTRY0_RUST_OBJECT_FLAGS, `--target=${RUST_TARGET}`, "--emit=obj"],
          originTarget: handlerOriginTarget,
        }
        : {
          kind: "gcc-handler-object",
          flags: [...sharedFlags],
          originTarget: handlerOriginTarget,
        };
    return {
      id: sourceRecipe,
      kind: PROCESS_ENTRY0_EXECUTION_KIND,
      recipeClass: PROCESS_ENTRY0_RECIPE_CLASS,
      handlerSymbol: PROCESS_ENTRY0_HANDLER_SYMBOL,
      handler,
      supportSources: context.processSupportSources.map(({ role, descriptor }) => ({
        role, path: descriptor.path, digest: descriptor.digest,
      })),
      sharedHarnessProvider: {
        compiler: path.basename(context.processLinker.command).replace(/\.exe$/iu, ""),
        target: PROCESS_ENTRY0_GCC_ORIGIN_TARGET,
        flags: sharedFlags,
        linkFlags: [...cReleaseFlags({ wholeProgram: false }), "-static", "-static-libgcc"],
        handlerCallAbi: "opaque-ptr-int32-status",
        dropOrder: ["context", "arguments"],
      },
      finalArtifactTarget: EXECUTABLE_ARTIFACT_TARGET_MINGW,
      runtimeVectors: {
        timed: [...context.processExecution.timedInput],
        correctness: context.processExecution.correctnessInputs.map((vector) => [...vector]),
      },
      faultCases: [...context.processExecution.faultCases],
    };
  }
  if (context.language === "w") {
    return {
      command: "w.exe",
      subcommand: "build",
      target: EXECUTABLE_ARTIFACT_TARGET_MSVC,
      args: ["build", "<source>", "--target", EXECUTABLE_ARTIFACT_TARGET_MSVC, "--output", "<artifact>"],
      flags: [...W_MLIR_OPT_FLAGS, "--mlir-to-llvmir", ...W_LLC_FLAGS, ...W_LLD_LINK_FLAGS],
      cmakeBuildType: "Release",
      profile: "release",
      artifactAbi: EXECUTABLE_ARTIFACT_TARGET_MSVC,
    };
  }
  if (context.language === "c") {
    const flags = context.languageToolchain.family === "clang-msvc"
      ? clangReleaseFlags()
      : cReleaseFlags(context.languageToolchain);
    return {
      command: path.basename(context.languageToolchain.command).replace(/\.exe$/iu, ""),
      target: context.languageToolchain.target,
      args: [context.languageToolchain.dialect.flag, ...flags, "<source>", "-o", "<artifact>"],
      flags: [context.languageToolchain.dialect.flag, ...flags],
      cStandard: dialectDisclosure(context.languageToolchain.dialect),
      artifactAbi: context.languageToolchain.target,
    };
  }
  if (context.language === "rust") {
    return {
      command: "rustc",
      target: RUST_TARGET,
      args: ["<source>", "--edition=2024", ...RUST_RELEASE_FLAGS, `--target=${RUST_TARGET}`, "-o", "<artifact>"],
      flags: ["--edition=2024", ...RUST_RELEASE_FLAGS, `--target=${RUST_TARGET}`],
      edition: "2024",
      artifactAbi: RUST_TARGET,
    };
  }
  fail(`unsupported language: ${context.language}`);
}

function toolchainProvenance(context) {
  if (isProcessHandlerLifecycle(context.target)) {
    const supportSources = context.processSupportSources.map(({ role, descriptor, filePath, stats }) => ({
      role, path: descriptor.path, digest: descriptor.digest,
      observedDigest: descriptor.digest,
      sizeBytes: String(stats.size),
      file: path.basename(filePath),
    }));
    const process = {
      executionKind: PROCESS_ENTRY0_EXECUTION_KIND,
      finalArtifactTarget: EXECUTABLE_ARTIFACT_TARGET_MINGW,
      handlerSymbol: PROCESS_ENTRY0_HANDLER_SYMBOL,
      sourceDigest: context.source.source.digest,
      supportSources,
      finalLink: {
        driver: path.basename(context.processLinker.command).replace(/\.exe$/iu, ""),
        version: context.processLinker.version,
        target: context.processLinker.target,
        flags: [...cReleaseFlags({ wholeProgram: false }), "-static", "-static-libgcc"],
      },
    };
    if (context.language === "w") {
      process.handlerOrigin = {
        compiler: "w_seed_process_entry0_gate",
        gateDigest: context.processGate.digest,
        gateProfile: context.processGate.buildProfile,
        target: RUST_TARGET,
        manifestDigest: context.windowsToolchain.manifestDigest,
        materializedTools: Object.fromEntries(Object.entries(context.windowsToolchain.materialized.tools)
          .filter(([name]) => context.tools[name] !== undefined)
          .map(([name, record]) => [name, { relativePath: record.relativePath, sizeBytes: record.sizeBytes, sha256: record.sha256, version: record.version }])),
      };
    } else if (context.language === "rust") {
      process.handlerOrigin = {
        compiler: context.languageToolchain.command,
        compilerVersion: context.languageToolchain.version,
        host: context.languageToolchain.host,
        target: RUST_TARGET,
        flags: [...PROCESS_ENTRY0_RUST_OBJECT_FLAGS],
      };
    } else {
      process.handlerOrigin = {
        compiler: context.processLinker.command,
        compilerVersion: context.processLinker.version,
        target: PROCESS_ENTRY0_GCC_ORIGIN_TARGET,
        flags: [...dialectArgs(context.processLinker.dialect), ...cReleaseFlags({ wholeProgram: false })],
      };
    }
    return process;
  }
  if (context.language === "w") {
    return {
      compiler: "w.exe",
      compilerProfile: "release",
      compilerTarget: EXECUTABLE_ARTIFACT_TARGET_MSVC,
      wExecutableDigest: context.publicW.digest,
      wExecutableReceiptDigest: context.publicW.receiptDigest,
      manifestDigest: context.windowsToolchain.manifestDigest,
      materializedTools: Object.fromEntries(Object.entries(context.windowsToolchain.materialized.tools)
        .filter(([name]) => context.tools[name] !== undefined)
        .map(([name, record]) => [name, { relativePath: record.relativePath, sizeBytes: record.sizeBytes, sha256: record.sha256, version: record.version }])),
      compilerVersion: context.publicW.compilerVersion,
    };
  }
  return {
    compiler: context.languageToolchain.command,
    compilerVersion: context.languageToolchain.version,
    standard: context.language === "c" ? {
      flag: context.languageToolchain.dialect.flag,
      name: context.languageToolchain.dialect.name,
      final: context.languageToolchain.dialect.final,
      disclosure: dialectDisclosure(context.languageToolchain.dialect),
    } : undefined,
    edition: context.language === "rust" ? context.languageToolchain.edition : undefined,
    host: context.language === "rust" ? context.languageToolchain.host : undefined,
    target: context.languageToolchain.target,
    artifactAbi: context.source.source.artifactTarget,
  };
}

function makeResult(context, correctness, compileWarmup, compileRaw, runWarmup, runRaw, observedAt) {
  const { workload, source } = context.source;
  const recipe = recipeFor(context);
  const recipeDigest = sha256Json(recipe);
  const toolchain = context.languageToolchain;
  const artifactTarget = isProcessHandlerLifecycle(context.target)
    ? EXECUTABLE_ARTIFACT_TARGET_MINGW
    : source.artifactTarget;
  const result = {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_RESULT_SCHEMA,
    kind: "executable-result",
    id: `${context.target}-${context.language}-${context.commit.slice(0, 12)}`,
    status: "recorded",
    workloadId: context.target,
    language: context.language,
    platformTarget: EXECUTABLE_PLATFORM_TARGET,
    artifactTarget,
    profile: "release",
    quality: "exploratory",
    claim: "measurement-only",
    verdict: "not-evaluated",
    equivalenceKey: executableEquivalenceKey(context.catalog, context.target, EXECUTABLE_PLATFORM_TARGET, "release", source.recipeClass),
    identity: {
      sourceDigest: source.digest,
      platformTarget: EXECUTABLE_PLATFORM_TARGET,
      artifactTarget,
      profile: "release",
      toolchain: toolchain.identity,
      host: executableHostIdentity(context.environment),
      recipe: source.recipe,
      recipeClass: source.recipeClass,
      recipeDigest,
      eligibility: source.eligibility,
    },
    correctness: isProcessArgumentWorkload(context.target)
      ? {
        oracleId: `${context.target}:${processArgumentOracleFor(context.target).kind}`,
        cases: workload.oracle.cases.map((testCase) => ({
          arguments: [...testCase.arguments],
          exitCode: testCase.exitCode,
          stdoutDigest: exactOutputDigest(testCase.stdout),
          stderrDigest: exactOutputDigest(testCase.stderr),
        })),
      }
      : {
        oracleId: `${context.target}:exact-output`,
        exitCode: workload.oracle.exitCode,
        stdoutDigest: exactOutputDigest(workload.oracle.stdout),
        stderrDigest: exactOutputDigest(workload.oracle.stderr),
      },
    artifact: {
      digest: correctness.artifactDigest,
      sizeBytes: correctness.artifactSizeBytes,
      cleanliness: correctness.artifactCleanliness,
      peLayout: correctness.artifactPeLayout,
    },
    protocol: isProcessHandlerLifecycle(context.target) ? processProtocol(context) : protocol(context, workload),
    environment: context.environment,
    compile: sampleSeries(compileWarmup, compileRaw),
    run: sampleSeries(runWarmup, runRaw, context.nativeBenchmark?.abi),
    provenance: {
      sourceDigest: source.digest,
      artifactDigest: correctness.artifactDigest,
      recipeDigest,
      toolchainDigest: sha256Json(toolchainProvenance(context)),
      runnerDigest: context.runnerDigest,
      catalogDigest: context.catalogDigest,
      commit: context.commit,
      observedAt,
    },
  };
  const errors = validateExecutableResult(result, context.catalog);
  if (errors.length > 0) fail(`${context.language} result failed validation: ${errors.join("; ")}`);
  return result;
}

async function currentCommit(executor) {
  const git = Bun.which("git");
  if (!git) fail("git is required for commit provenance");
  const result = await timedStep(executor, git, ["rev-parse", "HEAD"], ROOT, "Git HEAD provenance");
  requireSuccess(result, "Git HEAD provenance");
  const value = outputText(result.stdout).trim();
  if (!/^[0-9a-f]{40}$/u.test(value)) fail("Git HEAD provenance must be a full lowercase commit identity");
  return value;
}

function processSelectedToolchain(language, languageToolchain, processLinker, processGate) {
  if (language === "w") {
    return {
      language,
      identity: `process-entry0-w-${processGate.digest.slice("sha256:".length)}-gcc-${identityToken(processLinker.version, "GCC version")}-${identityToken(EXECUTABLE_ARTIFACT_TARGET_MINGW, "final artifact target")}`,
      version: processGate.buildProfile,
      target: EXECUTABLE_ARTIFACT_TARGET_MINGW,
      originTarget: RUST_TARGET,
    };
  }
  if (language === "c") {
    return {
      ...processLinker,
      identity: `process-entry0-c-${identityToken(path.basename(processLinker.command).replace(/\.exe$/iu, ""), "C private handler compiler")}-${identityToken(processLinker.version, "C private handler compiler version")}-${identityToken(processLinker.dialect.name, "C private handler dialect")}-portable-${identityToken(EXECUTABLE_ARTIFACT_TARGET_MINGW, "final artifact target")}`,
      target: EXECUTABLE_ARTIFACT_TARGET_MINGW,
      originTarget: EXECUTABLE_ARTIFACT_TARGET_MINGW,
      wholeProgram: false,
    };
  }
  return {
    ...languageToolchain,
    identity: `process-entry0-rust-${identityToken(languageToolchain.identity, "Rust private handler toolchain")}-${identityToken(processLinker.version, "GCC version")}-${identityToken(EXECUTABLE_ARTIFACT_TARGET_MINGW, "final artifact target")}`,
    target: EXECUTABLE_ARTIFACT_TARGET_MINGW,
    originTarget: RUST_TARGET,
  };
}

export async function resolveResultPath(output, cwd = process.cwd()) {
  if (typeof output !== "string" || output.length === 0) fail("--output is required");
  const candidate = path.resolve(cwd, output);
  if (!isContained(RESULTS_DIRECTORY, candidate) || path.extname(candidate).toLowerCase() !== ".json") fail("--output must be a new JSON file contained by benchmarks/results");
  await mkdir(RESULTS_DIRECTORY, { recursive: true });
  await assertNoReparseAncestors(candidate, RESULTS_DIRECTORY);
  try {
    await lstat(candidate);
    fail(`refusing to overwrite existing result: ${candidate}`);
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
  return candidate;
}

export async function publishRecord(outputPath, record) {
  if (typeof outputPath !== "string" || outputPath.length === 0) fail("--output is required");
  const candidate = path.resolve(outputPath);
  if (!isContained(RESULTS_DIRECTORY, candidate) || path.extname(candidate).toLowerCase() !== ".json") {
    fail("--output must be a new JSON file contained by benchmarks/results");
  }
  await assertNoReparseAncestors(candidate, RESULTS_DIRECTORY);
  const parent = path.dirname(candidate);
  await mkdir(parent, { recursive: true });
  await assertNoReparseAncestors(candidate, RESULTS_DIRECTORY);
  const temporary = path.join(parent, `.${path.basename(candidate)}.${process.pid}.tmp`);
  try {
    await writeFile(temporary, `${JSON.stringify(record, null, 2)}\n`, { flag: "wx" });
    await assertNoReparseAncestors(temporary, RESULTS_DIRECTORY);
    await link(temporary, candidate);
    await rm(temporary, { force: true });
  } catch (error) {
    await rm(temporary, { force: true });
    throw error;
  }
}

async function cleanupOwned(directory) {
  if (directory === undefined) return;
  const candidate = path.resolve(directory);
  const tempRoot = path.resolve(os.tmpdir());
  const base = path.basename(candidate);
  if (![RUN_DIRECTORY_PREFIX, SAMPLE_DIRECTORY_PREFIX].some((prefix) => base.startsWith(prefix))) fail(`refusing to clean unowned directory: ${directory}`);
  if (samePath(tempRoot, candidate) || !isContained(tempRoot, candidate)) fail(`owned temporary directory escapes the OS temp directory: ${directory}`);
  await assertNoReparseAncestors(candidate, tempRoot);
  await rm(candidate, { recursive: true, force: true });
  try {
    await access(candidate);
    fail(`owned temporary directory remains: ${candidate}`);
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
}

async function runBenchmarkUnlocked(options = {}, dependencies = {}) {
  const target = options.target ?? DEFAULT_TARGET;
  const language = options.language ?? "w";
  const warmup = options.warmup ?? DEFAULT_WARMUP;
  const compileSamples = options.compileSamples ?? options.samples ?? DEFAULT_COMPILE_SAMPLES;
  const runSamples = options.runSamples ?? options.samples ?? DEFAULT_RUN_SAMPLES;
  if (!RUN_TARGETS.includes(target)) fail(`unsupported benchmark target: ${target}`);
  if (!EXECUTABLE_LANGUAGES.includes(language)) fail(`unsupported language: ${language}`);
  if (!Number.isSafeInteger(warmup) || warmup < 1 || warmup > EXECUTABLE_MAX_SAMPLES) fail(`warmup must be between 1 and ${EXECUTABLE_MAX_SAMPLES}`);
  if (!Number.isSafeInteger(compileSamples) || compileSamples < 9 || compileSamples > EXECUTABLE_MAX_SAMPLES || compileSamples % 2 === 0) fail(`compileSamples must be odd and between 9 and ${EXECUTABLE_MAX_SAMPLES}`);
  if (!Number.isSafeInteger(runSamples) || runSamples < 9 || runSamples > EXECUTABLE_MAX_SAMPLES || runSamples % 2 === 0) fail(`runSamples must be odd and between 9 and ${EXECUTABLE_MAX_SAMPLES}`);
  const publish = options.publish !== false;
  if (dependencies.nativeBenchmark !== undefined &&
      (dependencies.testOnly !== true || publish)) {
    fail("native benchmark injection is test-only and cannot publish");
  }
  const processTarget = isProcessHandlerLifecycle(target);
  measurementPlatform(dependencies, publish);
  if (language === "w") {
    const legacyFallbacks = ["gate", "buildPrivateGate"]
      .filter((key) => Object.prototype.hasOwnProperty.call(dependencies, key));
    if (legacyFallbacks.length > 0) fail(`W private gate fallback dependencies are unsupported: ${legacyFallbacks.join(", ")}`);
  }
  const outputPath = publish ? await resolveResultPath(options.output) : undefined;
  const executor = dependencies.executor ?? defaultExecutor;
  const documents = dependencies.documents ?? loadExecutableDocuments();
  const catalog = dependencies.catalog ?? documents.catalog;
  const catalogErrors = validateExecutableCatalog(catalog, documents);
  if (catalogErrors.length > 0) fail(`catalog validation failed: ${catalogErrors.join("; ")}`);
  const source = await sourcePath(catalog, target, language);
  const processExecutionDescriptor = processTarget ? processExecution(source.workload) : undefined;
  const processSupportSources = processTarget ? await resolveProcessSupportSources(source.workload) : undefined;
  if (processTarget && source.source.recipeClass !== PROCESS_ENTRY0_RECIPE_CLASS) {
    fail("process-handler-lifecycle source must select the private handler recipe class");
  }
  if (processTarget && language === "w" && source.source.recipe !== PROCESS_ENTRY0_RECIPE) {
    fail("process-handler-lifecycle W source must select the private handler recipe");
  }
  if (language === "w" && !processTarget && source.source.recipe !== PUBLIC_W_BUILD_RECIPE) {
    fail(`${target} W cannot run: catalog recipe ${source.source.recipe} has no retained-artifact and separate compile-run benchmark support`);
  }
  const runnerDigest = dependencies.runnerDigest ?? await benchmarkRunnerDigest();
  const catalogDigest = dependencies.catalogDigest ?? await sha256File(CATALOG_PATH);
  const commit = dependencies.commit ?? await currentCommit(executor);
  if (!/^[0-9a-f]{40}$/u.test(commit)) fail("commit provenance must be a full lowercase identity");
  const environment = dependencies.environment ?? environmentSnapshot();
  const windowsToolchain = language === "w"
    ? dependencies.windowsToolchain ?? await resolveWindowsToolchain()
    : undefined;
  const publicW = language === "w"
    ? processTarget ? undefined : normalizePublicW(dependencies.publicW ?? (typeof dependencies.buildPublicW === "function"
      ? await dependencies.buildPublicW(executor)
      : await buildPublicW(executor)))
    : undefined;
  const languageToolchain = language === "c"
    ? await resolveCCompiler(executor, dependencies, target)
    : language === "rust"
      ? await resolveRustCompiler(executor, dependencies, target)
      : undefined;
  const processLinker = processTarget
    ? language === "c"
      ? { ...languageToolchain, wholeProgram: false }
      : await resolveCCompiler(executor, dependencies, target)
    : undefined;
  const processGate = processTarget && language === "w"
    ? await prepareProcessGate(executor, dependencies, publish)
    : undefined;
  let tempRoot;
  const retained = [];
  try {
    tempRoot = await mkdtemp(path.join(os.tmpdir(), RUN_DIRECTORY_PREFIX));
    console.error(`executable benchmark: measurement temp=${tempRoot}`);
    const selectedToolchain = processTarget
      ? processSelectedToolchain(language, languageToolchain, processLinker, processGate)
      : language === "w"
      ? {
        language: "w",
        identity: publicWToolchainIdentity(publicW),
        version: publicW.compilerVersion ?? "release",
        target: EXECUTABLE_ARTIFACT_TARGET_MSVC,
      }
      : languageToolchain;
    const nativeBenchmark = dependencies.nativeBenchmark ??
      (dependencies.testOnly === true ? undefined :
        await prepareNativeBenchmark(executor, tempRoot));
    const context = {
      executor,
      catalog,
      target,
      source,
      windowsToolchain,
      tools: windowsToolchain?.tools,
      publicW,
      language,
      languageToolchain: selectedToolchain,
      processExecution: processExecutionDescriptor,
      processSupportSources,
      processLinker,
      processGate,
      environment,
      commit,
      runnerDigest,
      catalogDigest,
      tempRoot,
      nativeBenchmark,
    };
    const correctness = await correctnessBuild(context);
    retained.push(correctness.compiled.sampleDirectory);
    const compileWarmup = [];
    const compileRaw = [];
    for (let round = 0; round < warmup; round += 1) compileWarmup.push((await compileSource(context, false)).sample);
    for (let round = 0; round < compileSamples; round += 1) compileRaw.push((await compileSource(context, false)).sample);
    const runWarmup = [];
    const runRaw = [];
    const oracle = isProcessArgumentWorkload(target)
      ? processArgumentOracle(source.workload)
      : source.workload.oracle;
    const timedOracleCase = isProcessArgumentWorkload(target)
      ? oracle.cases.find((testCase) => JSON.stringify(testCase.arguments) === JSON.stringify(oracle.timedInput))
      : undefined;
    const timedArguments = timedOracleCase?.arguments ?? [];
    const timedExpected = timedOracleCase ?? oracle;
    if (context.nativeBenchmark) {
      const runtimeEnvironment = processTarget ? clearProcessFaultEnvironment() : undefined;
      const nativeSeries = await nativeRuntimeSeries(
        context, correctness.compiled.artifact, processTarget
          ? context.processExecution.timedInput
          : timedArguments,
        processTarget ? { exitCode: 0, stdout: "", stderr: "" } : timedExpected,
        warmup, runSamples, `${language} ${target}`, runtimeEnvironment,
      );
      runWarmup.push(...nativeSeries.warmup);
      runRaw.push(...nativeSeries.raw);
    } else if (processTarget) {
      for (let round = 0; round < warmup; round += 1) {
        const execution = await runProcessArtifact(context, correctness.compiled.artifact,
          context.processExecution.timedInput, `process-handler-lifecycle timed warmup ${round + 1}`);
        runWarmup.push(execution.sample);
      }
      for (let round = 0; round < runSamples; round += 1) {
        const execution = await runProcessArtifact(context, correctness.compiled.artifact,
          context.processExecution.timedInput, `process-handler-lifecycle timed raw ${round + 1}`);
        runRaw.push(execution.sample);
      }
    } else {
      for (let round = 0; round < warmup; round += 1) {
        const execution = await runArtifact(executor, correctness.compiled.artifact, language, target, timedArguments);
        assertOracle(execution, timedExpected, target, `${language} ${target} warmup`);
        runWarmup.push(execution.sample);
      }
      for (let round = 0; round < runSamples; round += 1) {
        const execution = await runArtifact(executor, correctness.compiled.artifact, language, target, timedArguments);
        assertOracle(execution, timedExpected, target, `${language} ${target} raw`);
        runRaw.push(execution.sample);
      }
    }
    const record = makeResult(context, correctness, compileWarmup, compileRaw, runWarmup, runRaw, new Date().toISOString());
    if (publish) {
      await publishRecord(outputPath, record);
      const evidence = ["exploratory-ready", "partial-exploratory-ready"].includes(source.workload.benchmarkStatus)
        ? source.workload.benchmarkStatus === "partial-exploratory-ready"
          ? "exploratory benchmark evidence; cross-language baselines incomplete"
          : "exploratory benchmark evidence"
        : "non-benchmark timing evidence";
      console.error(`executable benchmark: published exploratory ${language} record=${outputPath} (${evidence})`);
    }
    return { record, outputPath };
  } finally {
    for (const directory of retained) await cleanupOwned(directory);
    await cleanupOwned(tempRoot);
  }
}

export async function runBenchmark(options = {}, dependencies = {}) {
  if (options.publish === false) return runBenchmarkUnlocked(options, dependencies);
  const release = await acquireExecutableBenchmarkLease();
  try {
    return await runBenchmarkUnlocked(options, dependencies);
  } finally {
    await release();
  }
}

export async function main(argv = process.argv.slice(2), dependencies = {}) {
  const options = parseBenchmarkArguments(argv);
  if (options.help) {
    console.log(benchmarkUsage());
    return 0;
  }
  await runBenchmark(options, dependencies);
  return 0;
}

if (import.meta.main) {
  main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
  });
}
