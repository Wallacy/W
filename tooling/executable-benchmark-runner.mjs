import crypto from "node:crypto";
import os from "node:os";
import {
  access,
  link,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  rm,
  stat,
  writeFile,
} from "node:fs/promises";
import path from "node:path";
import {
  EXECUTABLE_ARTIFACT_TARGET_MSVC,
  EXECUTABLE_PLATFORM_TARGET,
  EXECUTABLE_RESULT_SCHEMA,
  ROOT,
  executableEquivalenceKey,
  executableHostIdentity,
  exactOutputDigest,
  loadExecutableDocuments,
  validateExecutableCatalog,
  validateExecutableResult,
} from "./executable-benchmark-machine.mjs";
import {
  MATERIALIZED_MANIFEST,
  defaultCacheDirectory,
  validateManifest,
  validateMaterialized,
} from "./acquire-mlir0-windows.mjs";
import { findVisualStudio, findWindowsSdkKernel32, runWithVisualStudio } from "./windows-build-support.mjs";
import { parseMsvcCompilerVersion } from "./build-w-windows.mjs";

export const RESULTS_DIRECTORY = path.resolve(ROOT, "benchmarks", "results");
const CATALOG_PATH = path.resolve(ROOT, "benchmarks", "executable-catalog.json");
const TOOLCHAIN_MANIFEST_PATH = path.resolve(ROOT, "tooling", "mlir0-windows-toolchain.json");
const SEED_DIRECTORY = path.resolve(ROOT, "compiler", "seed-c");
const DEFAULT_TARGET = "hello";
const DEFAULT_WARMUP = 1;
const DEFAULT_SAMPLES = 9;
const MAX_SAMPLES = 1001;
const RUN_DIRECTORY_PREFIX = "w-executable-run-";
const GATE_DIRECTORY_PREFIX = "w-executable-gate-";
const SAMPLE_DIRECTORY_PREFIX = "w-executable-sample-";
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

async function assertNoReparseAncestors(candidate, stopAt) {
  let current = path.resolve(candidate);
  const stop = path.resolve(stopAt);
  while (isContained(stop, current)) {
    try {
      const stats = await lstat(current);
      if (stats.isSymbolicLink()) fail(`path contains a symbolic link: ${current}`);
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
    if (current === stop) break;
    const parent = path.dirname(current);
    if (parent === current) break;
    current = parent;
  }
}

async function regularFile(filePath, label) {
  const stats = await lstat(filePath);
  if (!stats.isFile() || stats.isSymbolicLink()) fail(`${label} must be a regular non-link file`);
  return stats;
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
    output: undefined,
    warmup: DEFAULT_WARMUP,
    samples: DEFAULT_SAMPLES,
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
    } else if (argument === "--output") {
      const value = argv[++index];
      if (value === undefined || value.length === 0) fail("--output requires a path");
      result.output = value;
    } else if (argument.startsWith("--output=")) {
      result.output = argument.slice("--output=".length);
      if (result.output.length === 0) fail("--output requires a path");
    } else if (argument === "--warmup") {
      result.warmup = integerOption(argv[++index], "--warmup", 1, MAX_SAMPLES);
    } else if (argument.startsWith("--warmup=")) {
      result.warmup = integerOption(argument.slice("--warmup=".length), "--warmup", 1, MAX_SAMPLES);
    } else if (argument === "--samples") {
      result.samples = integerOption(argv[++index], "--samples", 9, MAX_SAMPLES);
    } else if (argument.startsWith("--samples=")) {
      result.samples = integerOption(argument.slice("--samples=".length), "--samples", 9, MAX_SAMPLES);
    } else {
      fail(`unknown option: ${argument}`);
    }
  }
  if (result.target !== DEFAULT_TARGET) fail(`unsupported benchmark target: ${result.target}`);
  if (result.samples % 2 === 0) fail("--samples must be odd");
  return result;
}

export function benchmarkUsage() {
  return [
    "usage: bun tooling/executable-benchmark-runner.mjs --output <new-json> [options]",
    "",
    "Options: --target hello (default), --warmup <n> (default 1), --samples <odd n> (default 9).",
    "The output must be a new JSON file under benchmarks/results.",
    "This is Windows x86_64 exploratory Native0/MLIR0 source-to-PE candidate evidence only.",
  ].join("\n");
}

function sha256Bytes(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

async function sha256File(filePath) {
  return sha256Bytes(await readFile(filePath));
}

function sha256Json(value) {
  return sha256Bytes(Buffer.from(JSON.stringify(value), "utf8"));
}

function commandResult(result) {
  if (!isObject(result)) fail("executor must return a result object");
  return {
    exitCode: Number.isInteger(result.exitCode) ? result.exitCode : 3,
    stdout: bufferValue(result.stdout),
    stderr: bufferValue(result.stderr),
    resourceUsage: result.resourceUsage,
  };
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
  }));
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

async function timedStep(executor, command, args, cwd, label) {
  const start = process.hrtime.bigint();
  const result = commandResult(await executor(command, args, { cwd, stdout: "pipe", stderr: "pipe", windowsHide: true }));
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

function sampleSeries(warmup, raw) {
  return {
    warmup,
    raw,
    summary: deriveSummary(raw),
    cpuResolution: {
      unit: "microseconds",
      zeroAllowed: true,
      disclosure: "Bun resourceUsage reports CPU counters in microseconds; zero-valued samples are preserved and do not imply nanosecond precision.",
    },
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

async function buildPrivateGate() {
  if (process.platform !== "win32" || process.arch !== "x64") fail("private Native0 gate requires Windows x86_64");
  const vs = findVisualStudio();
  const cmake = Bun.which("cmake");
  const ninja = Bun.which("ninja");
  if (!cmake || !ninja) fail("private gate requires cmake and ninja");
  const buildDirectory = await mkdtemp(path.join(os.tmpdir(), GATE_DIRECTORY_PREFIX));
  console.error(`executable benchmark: private gate temp=${buildDirectory}`);
  try {
    const invoke = (command, args, label) => {
      const result = commandResult(runWithVisualStudio(vs.devCommand, command, args, { cwd: ROOT }));
      requireSuccess(result, label);
    };
    invoke(cmake, [
      "-S", SEED_DIRECTORY,
      "-B", buildDirectory,
      "-G", "Ninja",
      `-DCMAKE_MAKE_PROGRAM=${ninja}`,
      "-DCMAKE_BUILD_TYPE=Release",
      "-DCMAKE_C_FLAGS_RELEASE=/O2",
      "-DW_SEED_C_STANDARD=11",
      `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${buildDirectory}`,
    ], "private gate configure");
    invoke(cmake, ["--build", buildDirectory, "--target", "w_seed_mlir0_gate", "--", "-j", "2"], "private gate build");
    const executable = path.join(buildDirectory, "w_seed_mlir0_gate.exe");
    await regularFile(executable, "private gate executable");
    const compilerProbe = runWithVisualStudio(vs.devCommand, "cl.exe", ["/Bv"], { cwd: ROOT });
    const compilerOutput = Buffer.concat([bufferValue(compilerProbe.stdout), bufferValue(compilerProbe.stderr)]).toString("latin1");
    const compilerVersion = parseMsvcCompilerVersion(compilerOutput);
    return { buildDirectory, executable, compilerVersion };
  } catch (error) {
    await rm(buildDirectory, { recursive: true, force: true });
    throw error;
  }
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

async function sourcePath(catalog) {
  const workload = catalog.workloads?.find((item) => item.id === DEFAULT_TARGET);
  const source = workload?.sources?.find((item) => item.language === "w");
  if (!source) fail("catalog has no W Hello source");
  const filePath = path.resolve(ROOT, source.path);
  if (!isContained(ROOT, filePath)) fail("W Hello source escapes the repository");
  await regularFile(filePath, "W Hello source");
  if (await sha256File(filePath) !== source.digest) fail("W Hello source digest is stale");
  return { workload, source, filePath };
}

async function compileW(context, retain) {
  const sampleDirectory = await mkdtemp(path.join(context.tempRoot, SAMPLE_DIRECTORY_PREFIX));
  const input = path.join(sampleDirectory, "input.mlir");
  const verified = path.join(sampleDirectory, "verified.mlir");
  const llvm = path.join(sampleDirectory, "output.ll");
  const object = path.join(sampleDirectory, "output.obj");
  const artifact = path.join(sampleDirectory, "hello.exe");
  try {
    const start = process.hrtime.bigint();
    const steps = [];
    const gate = await timedStep(context.executor, context.gate.executable, [context.source.filePath, "--target=x86_64-pc-windows-msvc"], sampleDirectory, "W Native0 gate");
    requireSuccess(gate, "W Native0 gate");
    await writeFile(input, gate.stdout);
    steps.push(gate);
    const opt = await timedStep(context.executor, context.tools["mlir-opt.exe"], [input, "-o", verified, "--verify-each"], sampleDirectory, "mlir-opt");
    requireSuccess(opt, "mlir-opt");
    steps.push(opt);
    const translate = await timedStep(context.executor, context.tools["mlir-translate.exe"], ["--mlir-to-llvmir", verified, "-o", llvm], sampleDirectory, "mlir-translate");
    requireSuccess(translate, "mlir-translate");
    steps.push(translate);
    const llc = await timedStep(context.executor, context.tools["llc.exe"], ["-filetype=obj", "-mtriple=x86_64-pc-windows-msvc", llvm, "-o", object], sampleDirectory, "llc");
    requireSuccess(llc, "llc");
    steps.push(llc);
    const linkStep = await timedStep(context.executor, context.tools["lld-link.exe"], [
      "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/machine:x64",
      `/out:${artifact}`, object, context.windowsToolchain.sdk.path,
    ], sampleDirectory, "lld-link");
    requireSuccess(linkStep, "lld-link");
    steps.push(linkStep);
    const stats = await regularFile(artifact, "W PE artifact");
    if (stats.size <= 0) fail("W PE artifact is empty");
    const end = process.hrtime.bigint();
    const sample = chainSample(steps, start, end, "W compile");
    if (retain) return { sampleDirectory, artifact, sample };
    await rm(sampleDirectory, { recursive: true, force: true });
    return { sampleDirectory: undefined, artifact: undefined, sample };
  } catch (error) {
    await rm(sampleDirectory, { recursive: true, force: true });
    throw error;
  }
}

async function runArtifact(executor, artifact) {
  const step = await timedStep(executor, artifact, [], path.dirname(artifact), "W run");
  requireSuccess(step, "W executable");
  return {
    sample: sampleFrom(step.start, step.end, [step.usage], "W run"),
    stdout: step.stdout,
    stderr: step.stderr,
    exitCode: step.exitCode,
  };
}

function validatePeX64(bytes) {
  if (bytes.length < 0x40 || bytes[0] !== 0x4d || bytes[1] !== 0x5a) fail("W artifact is not a PE image");
  const offset = bytes.readUInt32LE(0x3c);
  if (offset + 26 > bytes.length || bytes.readUInt32LE(offset) !== 0x00004550 || bytes.readUInt16LE(offset + 4) !== 0x8664 || bytes.readUInt16LE(offset + 24) !== 0x20b) fail("W artifact is not PE x64 MSVC-compatible");
}

async function correctnessBuild(context) {
  const compiled = await compileW(context, true);
  try {
    const bytes = await readFile(compiled.artifact);
    validatePeX64(bytes);
    const execution = await runArtifact(context.executor, compiled.artifact);
    const oracle = context.source.workload.oracle;
    if (execution.exitCode !== oracle.exitCode || !execution.stdout.equals(Buffer.from(oracle.stdout, "utf8")) || !execution.stderr.equals(Buffer.from(oracle.stderr, "utf8"))) fail("W correctness output does not match the Hello oracle");
    return { compiled, artifactDigest: sha256Bytes(bytes), artifactSizeBytes: String(bytes.length) };
  } catch (error) {
    await rm(compiled.sampleDirectory, { recursive: true, force: true });
    throw error;
  }
}

async function checkGateArguments(context) {
  const positive = await timedStep(context.executor, context.gate.executable, [context.source.filePath, "--target=x86_64-pc-windows-msvc"], ROOT, "W gate target contract");
  requireSuccess(positive, "W gate target contract");
  if (!positive.stdout.toString("utf8").includes("x86_64-pc-windows-msvc")) fail("W gate target contract did not emit a Windows-target artifact");
  const invalid = await timedStep(context.executor, context.gate.executable, [context.source.filePath, "--target=unsupported"], ROOT, "W gate invalid target contract");
  if (invalid.exitCode !== 2 || invalid.stdout.length !== 0) fail("W gate invalid target contract must return exit 2 with empty stdout");
}

function protocol() {
  return {
    warmupMinimum: 1,
    rawMinimum: 9,
    rawParity: "odd",
    arithmeticMeanRounding: "floor-integer",
    stopRule: "fixed-count",
    wallClock: "monotonic-nanoseconds",
    processIsolation: "fresh-process-per-sample",
    order: "deterministic-interleaved",
    resourceScope: "W compile aggregates the five direct children (Native0 gate, mlir-opt, mlir-translate, llc and lld-link); run is the direct target process only; descendants are not aggregated.",
    knownNoiseControls: ["warmup-discarded", "fresh-process-per-sample", "fixed-variant-order"],
    unknownNoiseControls: ["host-scheduler", "filesystem-cache", "thermal-state"],
    directProcessDisclosure: "Bun direct-process CPU and RSS counters cover spawned processes only; process-tree CPU/RSS are not aggregated.",
  };
}

function makeResult(context, correctness, compileWarmup, compileRaw, runWarmup, runRaw, observedAt) {
  const { workload, source } = context.source;
  const recipe = {
    command: "w-seed-mlir0-gate",
    target: EXECUTABLE_ARTIFACT_TARGET_MSVC,
    args: ["<source>", "--target=x86_64-pc-windows-msvc", "|", "mlir-opt", "|", "mlir-translate", "|", "llc", "|", "lld-link"],
    flags: ["--verify-each", "-filetype=obj", "-mtriple=x86_64-pc-windows-msvc", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/machine:x64"],
    cmakeBuildType: "Release",
    cStandard: "11-recovery",
  };
  const recipeDigest = sha256Json(recipe);
  const result = {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_RESULT_SCHEMA,
    kind: "executable-result",
    id: `hello-w-${context.commit.slice(0, 12)}`,
    status: "recorded",
    workloadId: DEFAULT_TARGET,
    language: "w",
    platformTarget: EXECUTABLE_PLATFORM_TARGET,
    artifactTarget: EXECUTABLE_ARTIFACT_TARGET_MSVC,
    profile: "release",
    quality: "exploratory",
    claim: "measurement-only",
    verdict: "not-evaluated",
    equivalenceKey: executableEquivalenceKey(context.catalog, DEFAULT_TARGET, EXECUTABLE_PLATFORM_TARGET, "release", source.recipeClass),
    identity: {
      sourceDigest: source.digest,
      platformTarget: EXECUTABLE_PLATFORM_TARGET,
      artifactTarget: EXECUTABLE_ARTIFACT_TARGET_MSVC,
      profile: "release",
      toolchain: `msvc-${context.gate.compilerVersion}-mlir-23.1.0`,
      host: executableHostIdentity(context.environment),
      recipe: source.recipe,
      recipeClass: source.recipeClass,
      recipeDigest,
      eligibility: source.eligibility,
    },
    correctness: {
      oracleId: "hello:exact-output",
      exitCode: workload.oracle.exitCode,
      stdoutDigest: exactOutputDigest(workload.oracle.stdout),
      stderrDigest: exactOutputDigest(workload.oracle.stderr),
    },
    artifact: {
      digest: correctness.artifactDigest,
      sizeBytes: correctness.artifactSizeBytes,
    },
    protocol: protocol(),
    environment: context.environment,
    compile: sampleSeries(compileWarmup, compileRaw),
    run: sampleSeries(runWarmup, runRaw),
    provenance: {
      sourceDigest: source.digest,
      artifactDigest: correctness.artifactDigest,
      recipeDigest,
      toolchainDigest: sha256Json({
        manifestDigest: context.windowsToolchain.manifestDigest,
        materializedTools: Object.fromEntries(Object.entries(context.windowsToolchain.materialized.tools)
          .filter(([name]) => context.tools[name] !== undefined)
          .map(([name, record]) => [name, { relativePath: record.relativePath, sizeBytes: record.sizeBytes, sha256: record.sha256, version: record.version }])),
        gateCompiler: context.gate.compilerVersion,
      }),
      runnerDigest: context.runnerDigest,
      catalogDigest: context.catalogDigest,
      commit: context.commit,
      observedAt,
    },
  };
  const errors = validateExecutableResult(result, context.catalog);
  if (errors.length > 0) fail(`W result failed validation: ${errors.join("; ")}`);
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
  const parent = path.dirname(outputPath);
  await mkdir(parent, { recursive: true });
  await assertNoReparseAncestors(outputPath, RESULTS_DIRECTORY);
  const temporary = path.join(parent, `.${path.basename(outputPath)}.${process.pid}.tmp`);
  try {
    await writeFile(temporary, `${JSON.stringify(record, null, 2)}\n`, { flag: "wx" });
    await link(temporary, outputPath);
    await rm(temporary, { force: true });
  } catch (error) {
    await rm(temporary, { force: true });
    throw error;
  }
}

async function cleanupOwned(directory) {
  if (directory === undefined) return;
  const base = path.basename(directory);
  if (![RUN_DIRECTORY_PREFIX, GATE_DIRECTORY_PREFIX, SAMPLE_DIRECTORY_PREFIX].some((prefix) => base.startsWith(prefix))) fail(`refusing to clean unowned directory: ${directory}`);
  await rm(directory, { recursive: true, force: true });
  try {
    await access(directory);
    fail(`owned temporary directory remains: ${directory}`);
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
}

export async function runBenchmark(options = {}, dependencies = {}) {
  const target = options.target ?? DEFAULT_TARGET;
  const warmup = options.warmup ?? DEFAULT_WARMUP;
  const samples = options.samples ?? DEFAULT_SAMPLES;
  if (target !== DEFAULT_TARGET) fail(`unsupported benchmark target: ${target}`);
  if (!Number.isSafeInteger(warmup) || warmup < 1) fail("warmup must be at least one");
  if (!Number.isSafeInteger(samples) || samples < 9 || samples % 2 === 0) fail("samples must be odd and at least nine");
  const publish = options.publish !== false;
  const outputPath = publish ? await resolveResultPath(options.output) : undefined;
  const executor = dependencies.executor ?? defaultExecutor;
  const documents = dependencies.documents ?? loadExecutableDocuments();
  const catalog = dependencies.catalog ?? documents.catalog;
  const catalogErrors = validateExecutableCatalog(catalog, documents);
  if (catalogErrors.length > 0) fail(`catalog validation failed: ${catalogErrors.join("; ")}`);
  const runnerDigest = dependencies.runnerDigest ?? await sha256File(path.resolve(import.meta.dir, "executable-benchmark-runner.mjs"));
  const catalogDigest = dependencies.catalogDigest ?? await sha256File(CATALOG_PATH);
  const commit = dependencies.commit ?? await currentCommit(executor);
  if (!/^[0-9a-f]{40}$/u.test(commit)) fail("commit provenance must be a full lowercase identity");
  const environment = dependencies.environment ?? environmentSnapshot();
  const windowsToolchain = dependencies.windowsToolchain ?? await resolveWindowsToolchain();
  let gate;
  let tempRoot;
  const retained = [];
  try {
    gate = dependencies.gate ?? await buildPrivateGate();
    tempRoot = await mkdtemp(path.join(os.tmpdir(), RUN_DIRECTORY_PREFIX));
    console.error(`executable benchmark: measurement temp=${tempRoot}`);
    const source = await sourcePath(catalog);
    const context = {
      executor,
      catalog,
      source,
      windowsToolchain,
      tools: windowsToolchain.tools,
      gate,
      environment,
      commit,
      runnerDigest,
      catalogDigest,
      tempRoot,
    };
    await checkGateArguments(context);
    const correctness = await correctnessBuild(context);
    retained.push(correctness.compiled.sampleDirectory);
    const compileWarmup = [];
    const compileRaw = [];
    for (let round = 0; round < warmup; round += 1) compileWarmup.push((await compileW(context, false)).sample);
    for (let round = 0; round < samples; round += 1) compileRaw.push((await compileW(context, false)).sample);
    const runWarmup = [];
    const runRaw = [];
    const oracle = source.workload.oracle;
    for (let round = 0; round < warmup; round += 1) {
      const execution = await runArtifact(executor, correctness.compiled.artifact);
      if (execution.exitCode !== oracle.exitCode || !execution.stdout.equals(Buffer.from(oracle.stdout, "utf8")) || !execution.stderr.equals(Buffer.from(oracle.stderr, "utf8"))) fail("W warmup output differs from the Hello oracle");
      runWarmup.push(execution.sample);
    }
    for (let round = 0; round < samples; round += 1) {
      const execution = await runArtifact(executor, correctness.compiled.artifact);
      if (execution.exitCode !== oracle.exitCode || !execution.stdout.equals(Buffer.from(oracle.stdout, "utf8")) || !execution.stderr.equals(Buffer.from(oracle.stderr, "utf8"))) fail("W raw output differs from the Hello oracle");
      runRaw.push(execution.sample);
    }
    const record = makeResult(context, correctness, compileWarmup, compileRaw, runWarmup, runRaw, new Date().toISOString());
    if (publish) {
      await publishRecord(outputPath, record);
      console.error(`executable benchmark: published exploratory W record=${outputPath} (non-benchmark timing evidence)`);
    }
    return { record, outputPath };
  } finally {
    for (const directory of retained) await cleanupOwned(directory);
    await cleanupOwned(tempRoot);
    if (gate !== undefined && dependencies.gate === undefined && gate.buildDirectory !== undefined) await cleanupOwned(gate.buildDirectory);
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
