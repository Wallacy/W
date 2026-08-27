import crypto from "node:crypto";
import {
  lstat,
  link,
  mkdtemp,
  open,
  readFile,
  readdir,
  rm,
  stat,
  unlink,
} from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  DESCRIPTOR_IDENTITIES,
  MEASUREMENT_ORDER,
  RESULT_CLOCK,
  SCHEMA_VERSION,
  SOURCE_FIXTURE,
  loadBmdDocuments,
  validateManifest,
  validateResult,
} from "./benchmark-driven-development-machine.mjs";

export const ROOT = path.resolve(import.meta.dir, "..");
export const RUNNER_PATH = fileURLToPath(import.meta.url);
export const SEED_DIRECTORY = path.join(ROOT, "compiler", "seed-c");
export const MANIFEST_PATH = path.join(ROOT, "benchmarks", "seed-check-lifecycle.manifest.json");
export const TARGET_NAME = "w_seed_check_driver";
export const BUILD_GENERATOR = "Ninja";
export const BUILD_TYPE = "Release";
export const DEFAULT_WARMUP_COUNT = 1;
export const DEFAULT_SAMPLE_COUNT = 9;
export const MAX_COUNT = 100_001;
export const RUNNER_EVIDENCE_PATHS = Object.freeze([
  "benchmarks/wbench-1.schema.json",
  "tooling/benchmark-driven-development-machine.mjs",
  "tooling/benchmark-driven-development-runner.mjs",
]);

const CLEANUP_MAX_RETRIES = 3;
const CLEANUP_RETRY_DELAY_MS = 100;

const DIGEST_PATTERN = /^sha256:[0-9a-f]{64}$/u;
const USAGE = "usage: bun tooling/benchmark-driven-development-runner.mjs --output <path> [--warmup <count>] [--samples <odd-count>]";

export class RunnerError extends Error {
  constructor(code, message) {
    super(message);
    this.name = "RunnerError";
    this.code = code;
  }
}

function asBuffer(value) {
  if (Buffer.isBuffer(value)) return value;
  if (value instanceof Uint8Array) return Buffer.from(value);
  if (typeof value === "string") return Buffer.from(value, "utf8");
  return Buffer.alloc(0);
}

function digestBytes(value) {
  return "sha256:" + crypto.createHash("sha256").update(asBuffer(value)).digest("hex");
}

function canonicalJson(value) {
  return JSON.stringify(value);
}

function digestJson(value) {
  return digestBytes(canonicalJson(value));
}

async function digestFile(filePath) {
  return digestBytes(await readFile(filePath));
}

function normalizeNewlines(value) {
  return value.replace(/\r\n/gu, "\n");
}

function commandResult(value) {
  return {
    exitCode: Number.isInteger(value?.exitCode) ? value.exitCode : 1,
    stdout: asBuffer(value?.stdout),
    stderr: asBuffer(value?.stderr),
  };
}

export function defaultExecutor(command, args, cwd = ROOT) {
  const execution = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  });
  return commandResult(execution);
}

function outputExcerpt(result) {
  const stderr = result.stderr.toString("utf8").trim();
  const stdout = result.stdout.toString("utf8").trim();
  const detail = stderr || stdout;
  return detail === "" ? "" : " " + detail.slice(0, 2000);
}

async function runChecked(executor, command, args, cwd, phase) {
  const result = commandResult(await executor(command, args, cwd));
  if (result.exitCode !== 0) {
    throw new RunnerError(phase, command + " failed with exit " + result.exitCode + "." + outputExcerpt(result));
  }
  return result;
}

function parsePositiveInteger(value, name) {
  if (typeof value !== "string" || !/^[0-9]+$/u.test(value)) {
    throw new RunnerError("usage", name + " must be a decimal integer.");
  }
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || parsed < 1 || parsed > MAX_COUNT) {
    throw new RunnerError("usage", name + " must be between 1 and " + MAX_COUNT + ".");
  }
  return parsed;
}

export function validateRunnerOptions(options) {
  if (!options || typeof options !== "object") {
    throw new RunnerError("usage", "runner options are required.");
  }
  if (typeof options.outputPath !== "string" || options.outputPath.trim() === "") {
    throw new RunnerError("usage", "--output is required and must be explicit.");
  }
  const warmupCount = options.warmupCount ?? DEFAULT_WARMUP_COUNT;
  const sampleCount = options.sampleCount ?? DEFAULT_SAMPLE_COUNT;
  if (!Number.isSafeInteger(warmupCount) || warmupCount < 1 || warmupCount > MAX_COUNT) {
    throw new RunnerError("usage", "warmup count must be between 1 and " + MAX_COUNT + ".");
  }
  if (!Number.isSafeInteger(sampleCount) || sampleCount < DEFAULT_SAMPLE_COUNT ||
      sampleCount > MAX_COUNT || sampleCount % 2 === 0) {
    throw new RunnerError("usage", "sample count must be an odd integer between 9 and " + MAX_COUNT + ".");
  }
  return { outputPath: options.outputPath, warmupCount, sampleCount };
}

function optionValue(argument, name) {
  const prefix = name + "=";
  return argument.startsWith(prefix) ? argument.slice(prefix.length) : undefined;
}

export function parseRunnerArgs(argv) {
  if (!Array.isArray(argv)) throw new RunnerError("usage", USAGE);
  const values = {
    outputPath: undefined,
    warmupCount: DEFAULT_WARMUP_COUNT,
    sampleCount: DEFAULT_SAMPLE_COUNT,
  };
  const seen = new Set();
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--help" || argument === "-h") {
      if (argv.length !== 1) throw new RunnerError("usage", "--help cannot be combined with runner options.");
      return { help: true };
    }
    let name;
    let value;
    if (argument === "--output" || argument === "--warmup" || argument === "--samples") {
      name = argument;
      if (index + 1 >= argv.length) throw new RunnerError("usage", name + " requires a value.");
      value = argv[++index];
    } else if (typeof argument === "string" && argument.startsWith("--output=")) {
      name = "--output";
      value = optionValue(argument, name);
    } else if (typeof argument === "string" && argument.startsWith("--warmup=")) {
      name = "--warmup";
      value = optionValue(argument, name);
    } else if (typeof argument === "string" && argument.startsWith("--samples=")) {
      name = "--samples";
      value = optionValue(argument, name);
    } else if (argument === "--force" || argument === "--overwrite") {
      throw new RunnerError("usage", argument + " is not supported; output is fail-if-exists.");
    } else {
      throw new RunnerError("usage", "unknown option or positional argument: " + String(argument) + ".");
    }
    if (seen.has(name)) throw new RunnerError("usage", name + " must be specified once.");
    seen.add(name);
    if (name === "--output") values.outputPath = value;
    if (name === "--warmup") values.warmupCount = parsePositiveInteger(value, "warmup count");
    if (name === "--samples") values.sampleCount = parsePositiveInteger(value, "sample count");
  }
  return { help: false, ...validateRunnerOptions(values) };
}

function isSymlinkOrReparse(statValue) {
  return statValue.isSymbolicLink();
}

async function existingStat(filePath) {
  try {
    return await lstat(filePath);
  } catch (error) {
    if (error?.code === "ENOENT") return undefined;
    throw error;
  }
}

async function assertNoSymlinkAncestors(directory) {
  const parsed = path.parse(directory);
  let current = parsed.root;
  const relative = path.relative(parsed.root, directory);
  for (const segment of relative.split(path.sep).filter(Boolean)) {
    current = path.join(current, segment);
    const statValue = await existingStat(current);
    if (!statValue) throw new RunnerError("output", "output parent does not exist: " + directory + ".");
    if (isSymlinkOrReparse(statValue)) {
      throw new RunnerError("output", "output parent cannot contain a symlink or reparse point: " + current + ".");
    }
    if (!statValue.isDirectory()) throw new RunnerError("output", "output parent is not a directory: " + current + ".");
  }
}

export async function assertOutputTargetAvailable(outputPath, cwd = process.cwd()) {
  const target = path.resolve(cwd, outputPath);
  await assertNoSymlinkAncestors(path.dirname(target));
  const statValue = await existingStat(target);
  if (statValue) {
    if (isSymlinkOrReparse(statValue)) {
      throw new RunnerError("output", "output target is a symlink or reparse point: " + target + ".");
    }
    throw new RunnerError("output", "output target already exists: " + target + ".");
  }
  return target;
}

async function removeIfPresent(filePath, unlinkFile = unlink) {
  try {
    await unlinkFile(filePath);
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
}

export async function cleanupDirectoryBestEffort(directory, removeDirectory = rm) {
  if (typeof directory !== "string" || directory.trim() === "") return false;
  try {
    await removeDirectory(directory, {
      recursive: true,
      force: true,
      maxRetries: CLEANUP_MAX_RETRIES,
      retryDelay: CLEANUP_RETRY_DELAY_MS,
    });
    return true;
  } catch {
    return false;
  }
}

export async function atomicPublishJson(outputPath, value, cwd = process.cwd(), fileSystem = {}) {
  const target = await assertOutputTargetAvailable(outputPath, cwd);
  const openFile = fileSystem.open ?? open;
  const linkFile = fileSystem.link ?? link;
  const unlinkFile = fileSystem.unlink ?? unlink;
  const text = typeof value === "string" ? value : canonicalJson(value) + "\n";
  const sibling = path.join(
    path.dirname(target),
    "." + path.basename(target) + ".bmd1-" + crypto.randomUUID() + ".tmp",
  );
  let handle;
  try {
    handle = await openFile(sibling, "wx", 0o600);
    await handle.writeFile(text, "utf8");
    await handle.sync();
    await handle.close();
    handle = undefined;
    await linkFile(sibling, target);
    // The hard link is the publication boundary. The sibling is only a
    // cleanup artifact after this point, so cleanup failure must not turn a
    // complete result into a reported failure.
    try {
      await removeIfPresent(sibling, unlinkFile);
    } catch {}
  } catch (error) {
    if (handle) {
      try { await handle.close(); } catch {}
    }
    try { await removeIfPresent(sibling, unlinkFile); } catch {}
    if (error instanceof RunnerError) throw error;
    throw new RunnerError("publish", "atomic fail-if-exists publication failed: " + error.message);
  }
  return target;
}

async function collectFileDigests(directory, baseDirectory = directory) {
  const entries = (await readdir(directory, { withFileTypes: true }))
    .sort((left, right) => left.name.localeCompare(right.name));
  const files = [];
  for (const entry of entries) {
    const absolute = path.join(directory, entry.name);
    if (entry.isSymbolicLink()) {
      throw new RunnerError("recipe", "recipe input cannot be a symlink: " + absolute + ".");
    }
    if (entry.isDirectory()) {
      files.push(...await collectFileDigests(absolute, baseDirectory));
    } else if (entry.isFile()) {
      files.push({
        path: path.relative(ROOT, absolute).split(path.sep).join("/"),
        digest: await digestFile(absolute),
      });
    }
  }
  return files;
}

async function recipeEvidence(manifest, sourceDigest, inputDigest) {
  const seedFiles = await collectFileDigests(SEED_DIRECTORY);
  const files = [
    ...seedFiles,
    { path: path.relative(ROOT, MANIFEST_PATH).split(path.sep).join("/"), digest: await digestFile(MANIFEST_PATH) },
    { path: DESCRIPTOR_IDENTITIES.graph.path, digest: await digestFile(path.resolve(ROOT, DESCRIPTOR_IDENTITIES.graph.path)) },
    { path: DESCRIPTOR_IDENTITIES.input.path, digest: inputDigest },
    { path: SOURCE_FIXTURE.path, digest: sourceDigest },
  ].sort((left, right) => left.path.localeCompare(right.path));
  return {
    schema: "wbench/1-runner-recipe",
    track: "compiler-lifecycle",
    source: SOURCE_FIXTURE,
    command: manifest.command,
    build: {
      sourceDirectory: "compiler/seed-c",
      generator: BUILD_GENERATOR,
      configuration: BUILD_TYPE,
      target: TARGET_NAME,
      configureArguments: ["-G", BUILD_GENERATOR, "-DCMAKE_BUILD_TYPE=" + BUILD_TYPE],
      buildArguments: ["--target", TARGET_NAME, "--", "-j", "2"],
    },
    measurement: {
      clock: RESULT_CLOCK,
      order: MEASUREMENT_ORDER,
      processPerSample: true,
    },
    files,
  };
}

export async function runnerEvidence() {
  const files = [];
  for (const relativePath of RUNNER_EVIDENCE_PATHS) {
    files.push({
      path: relativePath,
      digest: await digestFile(path.join(ROOT, relativePath)),
    });
  }
  const evidence = {
    schema: "wbench/1-runner-evidence",
    files,
  };
  return { ...evidence, digest: digestJson(evidence) };
}

function cacheValue(cache, key) {
  const pattern = new RegExp("^" + key.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&") + "(?::[^=]+)?=(.*)$", "mu");
  const match = cache.match(pattern);
  return match ? match[1].trim() : undefined;
}

async function toolchainEvidence(executor, buildDirectory) {
  const cache = await readFile(path.join(buildDirectory, "CMakeCache.txt"), "utf8");
  const cmake = await runChecked(executor, "cmake", ["--version"], ROOT, "toolchain");
  const compilerPath = cacheValue(cache, "CMAKE_C_COMPILER");
  const ninjaPath = cacheValue(cache, "CMAKE_MAKE_PROGRAM");
  if (!compilerPath || !ninjaPath) {
    throw new RunnerError("toolchain", "CMake did not record compiler and Ninja paths.");
  }
  const compiler = await runChecked(executor, compilerPath, ["--version"], ROOT, "toolchain");
  const ninja = await runChecked(executor, ninjaPath, ["--version"], ROOT, "toolchain");
  const bunVersion = typeof process.versions?.bun === "string"
    ? process.versions.bun.trim()
    : undefined;
  const recordedCompilerVersion = cacheValue(cache, "CMAKE_C_COMPILER_VERSION");
  const compilerVersion = !recordedCompilerVersion
    ? normalizeNewlines(compiler.stdout.toString("utf8")).split("\n")[0]?.trim() || "unknown"
    : recordedCompilerVersion;
  const evidence = {
    generator: cacheValue(cache, "CMAKE_GENERATOR") ?? "unknown",
    buildType: cacheValue(cache, "CMAKE_BUILD_TYPE") ?? "unknown",
    ninjaPath,
    ninjaVersion: normalizeNewlines(ninja.stdout.toString("utf8")).split("\n")[0]?.trim() || "unknown",
    compilerPath,
    compilerVersion,
    platformId: cacheValue(cache, "CMAKE_SYSTEM_NAME") || process.platform + "-" + process.arch,
    cmakeVersion: normalizeNewlines(cmake.stdout.toString("utf8")).split("\n")[0]?.trim() || "unknown",
    cFlags: cacheValue(cache, "CMAKE_C_FLAGS"),
    cFlagsRelease: cacheValue(cache, "CMAKE_C_FLAGS_RELEASE"),
    exeLinkerFlags: cacheValue(cache, "CMAKE_EXE_LINKER_FLAGS"),
    exeLinkerFlagsRelease: cacheValue(cache, "CMAKE_EXE_LINKER_FLAGS_RELEASE"),
    bunVersion,
  };
  if (typeof bunVersion !== "string" || bunVersion === "" || bunVersion === "unknown") {
    throw new RunnerError("toolchain", "Bun version evidence is incomplete.");
  }
  if (Object.values(evidence).some((value) => value === undefined || value === "unknown")) {
    throw new RunnerError("toolchain", "build evidence is incomplete.");
  }
  if (evidence.generator !== BUILD_GENERATOR || evidence.buildType !== BUILD_TYPE) {
    throw new RunnerError("toolchain", "build evidence is not Ninja Release.");
  }
  return {
    identity: "cmake=" + evidence.cmakeVersion + ";compiler=" + evidence.compilerPath +
      ";compiler-version=" + evidence.compilerVersion + ";generator=" + evidence.generator +
      ";build-type=" + evidence.buildType + ";ninja=" + evidence.ninjaVersion +
      ";bun=" + evidence.bunVersion +
      ";c-flags=" + evidence.cFlags + ";c-flags-release=" + evidence.cFlagsRelease +
      ";exe-linker-flags=" + evidence.exeLinkerFlags +
      ";exe-linker-flags-release=" + evidence.exeLinkerFlagsRelease,
    digest: digestJson(evidence),
    evidence,
  };
}

export async function buildSeed({ executor = defaultExecutor } = {}) {
  const buildDirectory = await mkdtemp(path.join(os.tmpdir(), "w-bmd1-seed-"));
  try {
    await runChecked(executor, "cmake", [
      "-S", SEED_DIRECTORY,
      "-B", buildDirectory,
      "-G", BUILD_GENERATOR,
      "-DCMAKE_BUILD_TYPE=" + BUILD_TYPE,
    ], ROOT, "build-configure");
    await runChecked(executor, "cmake", [
      "--build", buildDirectory,
      "--target", TARGET_NAME,
      "--", "-j", "2",
    ], ROOT, "build");
    const executable = path.join(buildDirectory,
      process.platform === "win32" ? TARGET_NAME + ".exe" : TARGET_NAME);
    const executableStat = await stat(executable);
    if (!executableStat.isFile()) throw new RunnerError("build", "built target is not a regular file.");
    const toolchain = await toolchainEvidence(executor, buildDirectory);
    return {
      buildDirectory,
      executable,
      artifactDigest: await digestFile(executable),
      toolchain,
    };
  } catch (error) {
    await cleanupDirectoryBestEffort(buildDirectory);
    if (error instanceof RunnerError) throw error;
    throw new RunnerError("build", error.message);
  }
}

function median(values) {
  const sorted = [...values].sort((left, right) => left < right ? -1 : left > right ? 1 : 0);
  return sorted[Math.floor(sorted.length / 2)];
}

export function calculateLatency(values) {
  const parsed = values.map((value) => typeof value === "bigint" ? value : BigInt(value));
  if (parsed.length < DEFAULT_SAMPLE_COUNT || parsed.length % 2 === 0) {
    throw new RunnerError("samples", "latency requires an odd raw sample count of at least 9.");
  }
  const center = median(parsed);
  const deviations = parsed.map((value) => value >= center ? value - center : center - value);
  return {
    unit: "ns",
    minimumNs: parsed.reduce((minimum, value) => value < minimum ? value : minimum).toString(),
    medianNs: center.toString(),
    maximumNs: parsed.reduce((maximum, value) => value > maximum ? value : maximum).toString(),
    madNs: median(deviations).toString(),
    derivedFromRawSamples: true,
  };
}

function environmentEvidence(toolchain) {
  const cpu = os.cpus()[0]?.model?.trim() || os.arch();
  const logicalCpuCount = os.cpus().length;
  const evidence = toolchain.evidence ?? {};
  if (typeof evidence.bunVersion !== "string" || evidence.bunVersion.trim() === "" ||
      evidence.bunVersion === "unknown") {
    throw new RunnerError("toolchain", "toolchain evidence must include a Bun version.");
  }
  return {
    hardware: cpu + "; logical-cpus=" + logicalCpuCount,
    kernel: os.platform() + " " + os.release(),
    target: process.platform + "-" + process.arch,
    provider: "host",
    toolchain: toolchain.identity,
    flags: [
      "CMAKE_BUILD_TYPE=" + (evidence.buildType ?? "unknown"),
      "generator=" + (evidence.generator ?? "unknown"),
      "ninja-version=" + (evidence.ninjaVersion ?? "unknown"),
      "bun-version=" + evidence.bunVersion,
      "CMAKE_C_FLAGS=" + (evidence.cFlags ?? "unknown"),
      "CMAKE_C_FLAGS_RELEASE=" + (evidence.cFlagsRelease ?? "unknown"),
      "CMAKE_EXE_LINKER_FLAGS=" + (evidence.exeLinkerFlags ?? "unknown"),
      "CMAKE_EXE_LINKER_FLAGS_RELEASE=" + (evidence.exeLinkerFlagsRelease ?? "unknown"),
    ],
    noiseControls: {
      known: [],
      unknown: [
        "aslr",
        "background-load",
        "cpu-affinity",
        "filesystem-cache-state",
        "frequency-scaling",
        "power-plan",
        "scheduler",
        "thermal-state",
      ],
    },
  };
}

function driverArguments(manifest) {
  const argumentsList = manifest.command?.arguments;
  if (!Array.isArray(argumentsList) || argumentsList.length === 0 ||
      !argumentsList.includes(SOURCE_FIXTURE.path) || !argumentsList.includes("--json")) {
    throw new RunnerError("manifest", "manifest command must identify checker_bootstrap.w and --json.");
  }
  return ["--json", SOURCE_FIXTURE.path];
}

async function invokeChecked(invoke, args, phase) {
  const result = commandResult(await invoke(args));
  if (result.exitCode !== 0 || result.stdout.length !== 0 || result.stderr.length !== 0) {
    throw new RunnerError(phase, "driver must exit 0 with empty stdout and stderr.");
  }
  return result;
}

async function timedInvocation(invoke, args, clock, phase) {
  const start = clock();
  if (typeof start !== "bigint") throw new RunnerError("clock", "clock must return monotonic bigint nanoseconds.");
  await invokeChecked(invoke, args, phase);
  const end = clock();
  if (typeof end !== "bigint" || end < start) throw new RunnerError("clock", "clock must be monotonic.");
  return end - start;
}

function resultId(payload) {
  return "bmd1-" + digestJson(payload).slice("sha256:".length);
}

export function makeResult({ manifest, artifactDigest, recipeDigest, runnerDigest, toolchainDigest,
  toolchainIdentity, toolchainEvidence: recordedToolchainEvidence, oracleDigest, warmup, raw,
  sourceDigest, graphDigest, inputDigest }) {
  const rawNs = raw.map((value) => value.toString());
  const warmupNs = warmup.map((value) => value.toString());
  const payload = {
    $schema: "./wbench-1.schema.json",
    schema: SCHEMA_VERSION,
    kind: "result",
    status: "recorded",
    quality: "exploratory",
    claim: "measurement-only",
    workload: {
      manifestDigest: digestFileFromManifest(manifest),
      track: "compiler-lifecycle",
      lane: manifest.lane,
      scenario: "clean",
      stage: "check-end-to-end",
      subject: "compiler/seed-c",
      profile: null,
    },
    identity: {
      source: manifest.identity.source,
      graph: manifest.identity.graph,
      input: manifest.identity.input,
      command: manifest.command,
    },
    comparison: null,
    oracle: {
      validationDigest: oracleDigest,
      complete: true,
      beforeSamples: true,
    },
    samples: {
      raw: rawNs.map((ns) => ({ ns })),
      warmup: warmupNs.map((ns) => ({ ns })),
      stopRule: { kind: "fixed-count", count: rawNs.length },
      clock: RESULT_CLOCK,
      order: MEASUREMENT_ORDER,
    },
    environment: environmentEvidence({ identity: toolchainIdentity, evidence: recordedToolchainEvidence }),
    provenance: {
      sourceDigest,
      artifactDigest,
      inputDigest,
      recipeDigest,
      runnerDigest,
      toolchainDigest,
    },
    metrics: { latency: calculateLatency(raw) },
    summary: {
      sampleCount: rawNs.length,
      warmupCount: warmupNs.length,
      derivedFromRawSamples: true,
    },
    semanticDeviations: [],
    disclosures: [
      "Each warmup and raw sample includes a new driver process and filesystem/OS cache state.",
      "environment.noiseControls.known is empty; every factor listed in environment.noiseControls.unknown remains uncontrolled.",
    ],
  };
  return { ...payload, id: resultId(payload) };
}

async function manifestDigest() {
  return digestFile(MANIFEST_PATH);
}

function digestFileFromManifest(manifest) {
  return manifest.__digest;
}

export async function runBenchmark(options, dependencies = {}) {
  const validated = validateRunnerOptions(options);
  const outputPath = await assertOutputTargetAvailable(validated.outputPath, dependencies.cwd ?? process.cwd());
  const executor = dependencies.executor ?? defaultExecutor;
  const documents = dependencies.documents ?? loadBmdDocuments();
  const manifest = dependencies.manifest ?? documents.manifest;
  const manifestErrors = dependencies.validateManifest
    ? dependencies.validateManifest(manifest)
    : validateManifest(manifest);
  if (manifestErrors.length > 0) throw new RunnerError("manifest", manifestErrors.join(" "));
  const manifestWithDigest = { ...manifest, __digest: await manifestDigest() };
  let build;
  let ownedBuildDirectory;
  try {
    if (dependencies.build) {
      // Injected builds belong to the caller. Never recursively remove a path
      // supplied by a test or embedding caller.
      build = await dependencies.build({ executor });
    } else {
      build = await buildSeed({ executor });
      ownedBuildDirectory = build.buildDirectory;
    }
    const sourceDigest = await digestFile(path.resolve(ROOT, SOURCE_FIXTURE.path));
    const graphDigest = await digestFile(path.resolve(ROOT, DESCRIPTOR_IDENTITIES.graph.path));
    const inputDigest = await digestFile(path.resolve(ROOT, DESCRIPTOR_IDENTITIES.input.path));
    if (!DIGEST_PATTERN.test(sourceDigest) || sourceDigest !== SOURCE_FIXTURE.digest ||
        !DIGEST_PATTERN.test(graphDigest) || graphDigest !== DESCRIPTOR_IDENTITIES.graph.digest ||
        !DIGEST_PATTERN.test(inputDigest) || inputDigest !== DESCRIPTOR_IDENTITIES.input.digest) {
      throw new RunnerError("provenance", "source, graph or input digest is not current.");
    }
    const recipe = dependencies.recipe ?? await recipeEvidence(manifest, sourceDigest, inputDigest);
    const recipeDigest = dependencies.recipeDigest ?? digestJson(recipe);
    const runnerEvidenceRecord = dependencies.runnerEvidence ?? await runnerEvidence();
    const runnerDigest = dependencies.runnerDigest ?? runnerEvidenceRecord.digest;
    const toolchain = build.toolchain ?? dependencies.toolchain;
    const bunVersion = toolchain?.evidence?.bunVersion;
    if (!toolchain?.identity || !DIGEST_PATTERN.test(toolchain.digest ?? "") ||
        !toolchain.evidence || typeof bunVersion !== "string" || bunVersion.trim() === "" ||
        bunVersion === "unknown" || !toolchain.identity.includes("bun=" + bunVersion)) {
      throw new RunnerError("toolchain", "toolchain evidence is required.");
    }
    const invoke = dependencies.invoke ?? ((args) => executor(build.executable, args, ROOT));
    const driverArgs = driverArguments(manifest);
    const oracleEvidence = {
      schema: "wbench/1-oracle",
      sourceDigest,
      graphDigest,
      inputDigest,
      command: manifest.command,
      driverArguments: driverArgs,
      exitCode: 0,
      stdoutDigest: digestBytes(Buffer.alloc(0)),
      stderrDigest: digestBytes(Buffer.alloc(0)),
      stdoutBytes: 0,
      stderrBytes: 0,
    };
    await invokeChecked(invoke, driverArgs, "oracle");
    const oracleDigest = dependencies.oracleDigest ?? digestJson(oracleEvidence);
    const clock = dependencies.clock ?? (() => process.hrtime.bigint());
    const warmup = [];
    for (let index = 0; index < validated.warmupCount; index += 1) {
      warmup.push(await timedInvocation(invoke, driverArgs, clock, "warmup"));
    }
    const raw = [];
    for (let index = 0; index < validated.sampleCount; index += 1) {
      raw.push(await timedInvocation(invoke, driverArgs, clock, "sample"));
    }
    const result = makeResult({
      manifest: manifestWithDigest,
      artifactDigest: build.artifactDigest,
      recipeDigest,
      runnerDigest,
      toolchainDigest: toolchain.digest,
      toolchainIdentity: toolchain.identity,
      toolchainEvidence: toolchain.evidence,
      oracleDigest,
      warmup,
      raw,
      sourceDigest,
      graphDigest,
      inputDigest,
    });
    const resultErrors = dependencies.validateResult
      ? dependencies.validateResult(result, manifest)
      : validateResult(result, manifest);
    if (resultErrors.length > 0) throw new RunnerError("result", resultErrors.join(" "));
    await atomicPublishJson(outputPath, result, dependencies.cwd ?? process.cwd());
    return { result, outputPath };
  } finally {
    if (ownedBuildDirectory) {
      await cleanupDirectoryBestEffort(ownedBuildDirectory);
    }
  }
}

export function usage() {
  return USAGE;
}

async function main() {
  try {
    const parsed = parseRunnerArgs(process.argv.slice(2));
    if (parsed.help) {
      process.stdout.write(USAGE + "\n");
      return;
    }
    await runBenchmark(parsed);
  } catch (error) {
    const message = error instanceof RunnerError ? error.message : error?.message || String(error);
    if (error?.code === "usage") process.stderr.write(USAGE + "\n");
    process.stderr.write("BMD1 runner: " + message + "\n");
    process.exitCode = 2;
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === RUNNER_PATH) await main();
