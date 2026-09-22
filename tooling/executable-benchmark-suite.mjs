import { spawn } from "node:child_process";
import { randomUUID } from "node:crypto";
import { lstat, readFile, rm } from "node:fs/promises";
import path from "node:path";
import {
  EXECUTABLE_SUITE_DEFAULT_PLATFORMS,
  EXECUTABLE_SUITE_RECEIPT_SCHEMA,
  ROOT,
  executableCatalogFileDigest,
  executableSuiteReceiptErrors,
  loadExecutableDocuments,
  selectExecutableSuiteLanes,
} from "./executable-benchmark-machine.mjs";
import { renderFromDisk, writeAtomicFile } from "./executable-benchmark-docs.mjs";

export const SUITE_RECEIPT_PATH = path.resolve(ROOT, "benchmarks", "executable-suite-current.json");
const RESULTS_DIRECTORY = "benchmarks/results";
const BENCHMARK_CLI = "tooling/benchmark-cli.mjs";
const DOCS_CLI = "tooling/executable-benchmark-docs.mjs";
const OUTPUT_LIMIT = 32 * 1024;

function relative(root, absolutePath) {
  return path.relative(root, absolutePath).replaceAll(path.sep, "/");
}

function retainTail(chunks, next) {
  chunks.push(next);
  const length = chunks.reduce((sum, chunk) => sum + chunk.length, 0);
  while (chunks.length > 1 && length - chunks[0].length >= OUTPUT_LIMIT) chunks.shift();
  if (chunks.reduce((sum, chunk) => sum + chunk.length, 0) > OUTPUT_LIMIT) {
    const last = chunks.pop();
    chunks.length = 0;
    chunks.push(last.subarray(-OUTPUT_LIMIT));
  }
}

export function executeSuiteCommand({ args, cwd, command = process.execPath }) {
  return new Promise((resolve) => {
    const child = spawn(command, args, { cwd, windowsHide: true, stdio: ["ignore", "pipe", "pipe"] });
    const stdout = [];
    const stderr = [];
    child.stdout.on("data", (chunk) => retainTail(stdout, Buffer.from(chunk)));
    child.stderr.on("data", (chunk) => retainTail(stderr, Buffer.from(chunk)));
    child.on("error", (error) => resolve({ code: null, stdout: Buffer.concat(stdout).toString("utf8"), stderr: Buffer.concat(stderr).toString("utf8"), error }));
    child.on("close", (code) => resolve({ code, stdout: Buffer.concat(stdout).toString("utf8"), stderr: Buffer.concat(stderr).toString("utf8") }));
  });
}

async function assertCleanGitTree({ root, execute }) {
  const result = await execute({ command: "git", args: ["status", "--porcelain", "--untracked-files=all"], cwd: root, label: "clean-worktree preflight" });
  if (result.code !== 0) {
    throw new Error(`executable suite clean-worktree preflight failed (exit ${result.code ?? "spawn error"})\n${result.stderr || result.stdout || result.error?.message || "No diagnostic output."}`);
  }
  if (result.stdout.length !== 0) {
    throw new Error(`executable suite requires a clean Git worktree before measurements\n${result.stdout.trimEnd()}`);
  }
}

function commandFailure(label, result, lane, remaining) {
  const laneDescription = lane ? ` at ${lane.workloadId}/${lane.language}/${lane.platformTarget}` : "";
  const tail = [result.stdout && `stdout:\n${result.stdout.trimEnd()}`, result.stderr && `stderr:\n${result.stderr.trimEnd()}`, result.error?.message]
    .filter(Boolean).join("\n");
  const error = new Error(`executable suite failed${laneDescription} during ${label} (exit ${result.code ?? "spawn error"}); ${remaining} lane(s) were not attempted; no new success receipt was published.\n${tail || "No diagnostic output."}`);
  error.lane = lane;
  error.commandStatus = result.code;
  error.stdout = result.stdout;
  error.stderr = result.stderr;
  error.remaining = remaining;
  return error;
}

async function readOptional(filePath) {
  try { return await readFile(filePath); }
  catch (error) { if (error?.code === "ENOENT") return undefined; throw error; }
}

async function restoreFile(filePath, previousBytes, benchmarksRoot) {
  if (previousBytes !== undefined) {
    await writeAtomicFile(filePath, previousBytes, benchmarksRoot);
    return;
  }
  try {
    const stats = await lstat(filePath);
    if (!stats.isFile() || stats.isSymbolicLink()) throw new Error(`refusing to remove non-regular generated output: ${filePath}`);
    await rm(filePath);
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
}

export async function publishSuiteReceiptAndProjection(receipt, { root = ROOT } = {}) {
  const benchmarksRoot = path.resolve(root, "benchmarks");
  const receiptPath = path.resolve(benchmarksRoot, "executable-suite-current.json");
  const projectionPath = path.resolve(benchmarksRoot, "EXECUTABLES.md");
  const previousReceipt = await readOptional(receiptPath);
  const previousProjection = await readOptional(projectionPath);
  try {
    await writeAtomicFile(receiptPath, `${JSON.stringify(receipt, null, 2)}\n`, benchmarksRoot);
    const markdown = `${await renderFromDisk(root)}\n`;
    await writeAtomicFile(projectionPath, markdown, benchmarksRoot);
    const projected = `${await renderFromDisk(root)}\n`;
    if (projected !== markdown || (await readFile(projectionPath, "utf8")) !== markdown) {
      throw new Error("generated executable benchmark projection did not verify after suite receipt publication");
    }
  } catch (error) {
    const restoreErrors = [];
    try { await restoreFile(receiptPath, previousReceipt, benchmarksRoot); } catch (restoreError) { restoreErrors.push(String(restoreError?.message ?? restoreError)); }
    try { await restoreFile(projectionPath, previousProjection, benchmarksRoot); } catch (restoreError) { restoreErrors.push(String(restoreError?.message ?? restoreError)); }
    if (restoreErrors.length > 0) throw new AggregateError([error, ...restoreErrors.map((message) => new Error(message))], "suite receipt publication failed and rollback was incomplete");
    throw error;
  }
}

export function parseExecutableSuiteArguments(argv) {
  const options = { platforms: [...EXECUTABLE_SUITE_DEFAULT_PLATFORMS], mode: "full" };
  if (!Array.isArray(argv)) throw new TypeError("suite arguments must be an array");
  if (argv.length === 0) return options;
  if (argv.length !== 2 || argv[0] !== "--platform" || !EXECUTABLE_SUITE_DEFAULT_PLATFORMS.includes(argv[1])) {
    throw new Error("usage: bun tooling/executable-benchmark-suite.mjs [--platform windows-x64|linux-wsl-x64]");
  }
  options.platforms = [argv[1]];
  options.mode = "filtered";
  return options;
}

export async function runExecutableSuite({
  root = ROOT,
  catalog = loadExecutableDocuments(root).catalog,
  platforms = EXECUTABLE_SUITE_DEFAULT_PLATFORMS,
  mode = JSON.stringify(platforms) === JSON.stringify(EXECUTABLE_SUITE_DEFAULT_PLATFORMS) ? "full" : "filtered",
  execute = executeSuiteCommand,
  ensureClean = assertCleanGitTree,
  now = () => Number(process.hrtime.bigint()) / 1_000_000,
  observedAt = () => new Date().toISOString(),
  id = randomUUID,
  readResult = async (relativePath) => JSON.parse(await readFile(path.resolve(root, relativePath), "utf8")),
  publishReceipt = (receipt) => publishSuiteReceiptAndProjection(receipt, { root }),
} = {}) {
  const selectedLanes = selectExecutableSuiteLanes(catalog, { platforms });
  const canonicalPlatforms = EXECUTABLE_SUITE_DEFAULT_PLATFORMS.filter((platform) => platforms.includes(platform));
  const expectedMode = JSON.stringify(canonicalPlatforms) === JSON.stringify(EXECUTABLE_SUITE_DEFAULT_PLATFORMS) ? "full" : "filtered";
  if (mode !== expectedMode) throw new Error(`suite mode ${mode} does not match selected platform scope (${expectedMode})`);
  const lanes = selectedLanes;
  if (lanes.length === 0) throw new Error("executable suite selected no runnable measurement lanes");
  await ensureClean({ root, execute });
  const started = now();
  const runId = id();
  const resultPaths = lanes.map((lane, index) => path.posix.join(
    RESULTS_DIRECTORY,
    `suite-${runId}-${String(index + 1).padStart(3, "0")}-${lane.workloadId}-${lane.language}-${lane.platformTarget}.local.json`,
  ));
  const invoke = async (args, label, lane = undefined, remaining = 0) => {
    const result = await execute({ command: process.execPath, args, cwd: root, label });
    if (result.code !== 0) throw commandFailure(label, result, lane, remaining);
  };

  for (let index = 0; index < lanes.length; index += 1) {
    const lane = lanes[index];
    // Omitting sampling options deliberately preserves the benchmark runner's
    // current full-tier warmup/sample defaults.
    await invoke([
      BENCHMARK_CLI, "run",
      "--target", lane.workloadId,
      "--language", lane.language,
      "--platform", lane.platformTarget,
      "--output", resultPaths[index],
    ], "run", lane, lanes.length - index - 1);
  }
  for (let index = 0; index < resultPaths.length; index += 1) {
    await invoke([BENCHMARK_CLI, "validate", resultPaths[index]], "result validation", lanes[index], lanes.length - index - 1);
  }
  const laneToolchains = [];
  for (let index = 0; index < resultPaths.length; index += 1) {
    const result = await readResult(resultPaths[index], lanes[index]);
    const lane = lanes[index];
    if (result?.workloadId !== lane.workloadId || result?.language !== lane.language ||
        result?.platformTarget !== lane.platformTarget ||
        typeof result?.identity?.toolchain !== "string" || typeof result?.identity?.recipe !== "string" ||
        typeof result?.provenance?.toolchainDigest !== "string") {
      throw new Error(`validated result ${resultPaths[index]} did not expose the expected lane and toolchain identity`);
    }
    laneToolchains.push({
      toolchain: result.identity.toolchain,
      recipe: result.identity.recipe,
      toolchainDigest: result.provenance.toolchainDigest,
    });
  }
  await invoke([BENCHMARK_CLI, "update", ...resultPaths], "atomic catalog update", undefined, 0);
  await invoke([BENCHMARK_CLI, "check"], "catalog and projection check");
  await invoke([DOCS_CLI, "--check"], "generated documentation check");

  const durationMs = Math.max(0, Math.floor(now() - started));
  const documents = loadExecutableDocuments(root);
  const currentDigest = executableCatalogFileDigest(root);
  const receipt = {
    $schema: "./executable-benchmark.schema.json",
    schema: EXECUTABLE_SUITE_RECEIPT_SCHEMA,
    kind: "executable-suite-current",
    status: "current",
    mode,
    platforms: canonicalPlatforms,
    catalogDigest: currentDigest,
    observedAt: observedAt(),
    durationMs,
    laneCounts: { total: lanes.length, passed: lanes.length, failed: 0, skipped: 0 },
    lanes: lanes.map((lane, index) => ({ ...lane, ...laneToolchains[index], status: "passed" })),
  };
  const receiptErrors = executableSuiteReceiptErrors(receipt, documents.catalog, { catalogDigest: currentDigest });
  if (receiptErrors.length > 0) throw new Error(`executable suite receipt validation failed: ${receiptErrors.join("; ")}`);
  await publishReceipt(receipt);
  return receipt;
}

export async function main(argv = process.argv.slice(2), dependencies = {}) {
  const options = parseExecutableSuiteArguments(argv);
  const receipt = await runExecutableSuite({ ...dependencies, ...options });
  console.log(`executable suite ${receipt.mode}: ${receipt.laneCounts.passed}/${receipt.laneCounts.total} lanes passed in ${receipt.durationMs} ms`);
  if (receipt.mode === "filtered") console.log(`scope: ${receipt.platforms.join(", ")} (not a full-suite receipt)`);
  console.log(`latest receipt: ${relative(dependencies.root ?? ROOT, dependencies.receiptPath ?? SUITE_RECEIPT_PATH)}`);
  return 0;
}

if (import.meta.main) {
  main().catch((error) => {
    console.error(error?.stack ?? error?.message ?? String(error));
    process.exitCode = 1;
  });
}
