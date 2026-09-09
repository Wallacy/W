import crypto from "node:crypto";
import { lstat, readFile, rmdir, rm } from "node:fs/promises";
import path from "node:path";
import {
  LOCAL_RESULTS_PATH,
  ROOT,
  updateExecutableBestMetrics,
  loadExecutableDocuments,
  validateExecutableBestMetrics,
  validateExecutableCatalog,
  validateExecutableResult,
} from "./executable-benchmark-machine.mjs";
import { renderExecutableProjection, renderFromDisk, writeAtomicFile } from "./executable-benchmark-docs.mjs";
import { runBenchmark } from "./executable-benchmark-runner.mjs";

const RESULTS_PATH = LOCAL_RESULTS_PATH;
const RUN_TARGETS = Object.freeze(["hello", "restaurant-branch"]);

function fail(message) {
  throw new Error(`benchmark: ${message}`);
}

function isContained(parent, candidate) {
  const relative = path.relative(path.resolve(parent), path.resolve(candidate));
  return relative === "" || (relative !== ".." && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative));
}

async function regularFile(filePath, label) {
  const stats = await lstat(filePath);
  if (!stats.isFile() || stats.isSymbolicLink()) fail(`${label} must be a regular non-link file`);
  return stats;
}

function nextValue(argv, index, name) {
  const value = argv[index + 1];
  if (value === undefined || value.startsWith("--")) fail(`${name} requires a value`);
  return value;
}

function integer(value, name, minimum, odd = false) {
  if (!/^[0-9]+$/u.test(value ?? "")) fail(`${name} must be a decimal integer`);
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || parsed < minimum) fail(`${name} is outside its allowed range`);
  if (odd && parsed % 2 === 0) fail(`${name} must be odd`);
  return parsed;
}

export function parseBenchmarkCliArguments(argv) {
  if (!Array.isArray(argv)) fail("arguments must be an array");
  const command = argv[0] ?? "help";
  if (command === "help" || command === "--help" || command === "-h") return { command: "help" };
  if (!["list", "run", "validate", "update", "check"].includes(command)) fail(`unknown command: ${command}`);
  if (command === "check") {
    if (argv.length !== 1) fail("check does not accept positional arguments or options");
    return { command };
  }
  if (command === "list") {
    if (argv.length !== 1) fail("list does not accept --target or other options");
    return { command };
  }
  if (command === "validate" || command === "update") {
    if (argv.length !== 2 || argv[1].startsWith("--")) fail(`${command} requires exactly one JSON path`);
    return { command, input: argv[1] };
  }
  const result = { command, target: "hello", language: "w", output: undefined, warmup: 1, samples: 9 };
  for (let index = 1; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--target") result.target = nextValue(argv, index++, "--target");
    else if (argument.startsWith("--target=")) result.target = argument.slice("--target=".length);
    else if (argument === "--language") result.language = nextValue(argv, index++, "--language");
    else if (argument.startsWith("--language=")) result.language = argument.slice("--language=".length);
    else if (argument === "--output") result.output = nextValue(argv, index++, "--output");
    else if (argument.startsWith("--output=")) result.output = argument.slice("--output=".length);
    else if (argument === "--warmup") result.warmup = integer(nextValue(argv, index++, "--warmup"), "--warmup", 1);
    else if (argument.startsWith("--warmup=")) result.warmup = integer(argument.slice("--warmup=".length), "--warmup", 1);
    else if (argument === "--samples") result.samples = integer(nextValue(argv, index++, "--samples"), "--samples", 9, true);
    else if (argument.startsWith("--samples=")) result.samples = integer(argument.slice("--samples=".length), "--samples", 9, true);
    else fail(`unknown run option: ${argument}`);
  }
  if (!RUN_TARGETS.includes(result.target)) fail(`unsupported target: ${result.target}`);
  if (!["w", "c", "rust"].includes(result.language)) fail(`unsupported language: ${result.language}`);
  if (result.samples < 9 || result.samples % 2 === 0) fail("--samples must be odd and at least nine");
  result.output ??= `benchmarks/results/${result.target}-${result.language}.local.json`;
  return result;
}

export function benchmarkUsage() {
  return [
    "usage: bun benchmark <list|run|validate|update|check>",
    "",
    "  list",
    "  run --target hello|restaurant-branch --language w|c|rust [--output benchmarks/results/<new>.json] [--warmup 1] [--samples 9]",
    "  validate <result.json>",
    "  update <result.json>    (lower-is-better live-catalog update; consumes a local result on success)",
    "  check",
    "",
    "Run measures one selected source with its catalog exact-output oracle. C probes -std=c23/-std=c2x for the MinGW ABI, and Rust uses rustc edition 2024 for the MSVC ABI. W uses the public w build Release source-to-PE candidate for workloads that declare that recipe; public-w-run targets require retained-artifact and separate compile-run support.",
  ].join("\n");
}

function safeJsonPath(input, label, allowedRoots, base = process.cwd()) {
  if (typeof input !== "string" || input.length === 0) fail(`${label} requires a path`);
  const candidate = path.resolve(base, input);
  if (path.extname(candidate).toLowerCase() !== ".json" || !allowedRoots.some((root) => isContained(root, candidate))) fail(`${label} must be a JSON file contained by the repository benchmark directories`);
  return candidate;
}

async function readResultInput(input, root = ROOT) {
  const rootPath = path.resolve(root);
  const candidate = safeJsonPath(input, "result path", [path.resolve(rootPath, RESULTS_PATH)], rootPath);
  await regularFile(candidate, "result path");
  let value;
  try { value = JSON.parse(await readFile(candidate, "utf8")); } catch { fail("result path must contain valid JSON"); }
  return { candidate, value };
}

export async function consumeLocalResult(candidate, resultsRoot) {
  const resolved = path.resolve(candidate);
  const root = path.resolve(resultsRoot);
  if (!isContained(root, resolved) || resolved === root) return false;
  await regularFile(resolved, "local result");
  await rm(resolved);
  try { await rmdir(root); } catch (error) {
    if (!["ENOENT", "ENOTEMPTY", "EEXIST"].includes(error?.code)) throw error;
  }
  return true;
}

export const consumeRecordedLocalResult = consumeLocalResult;

function digestBytes(bytes) {
  return crypto.createHash("sha256").update(bytes).digest("hex");
}

function outputText(value) {
  return Buffer.from(value ?? "").toString("utf8");
}

export async function currentGitState(root = ROOT) {
  const git = Bun.which("git");
  if (!git) fail("git is required for catalog update provenance");
  const head = Bun.spawnSync({ cmd: [git, "rev-parse", "HEAD"], cwd: root, stdout: "pipe", stderr: "pipe", windowsHide: true });
  if (head.exitCode !== 0) fail(`git HEAD lookup failed: ${outputText(head.stderr).trim()}`);
  const commit = outputText(head.stdout).trim();
  if (!/^[0-9a-f]{40}$/u.test(commit)) fail("git HEAD is not a full lowercase commit identity");
  const status = Bun.spawnSync({ cmd: [git, "status", "--porcelain", "--untracked-files=all"], cwd: root, stdout: "pipe", stderr: "pipe", windowsHide: true });
  if (status.exitCode !== 0) fail(`git status lookup failed: ${outputText(status.stderr).trim()}`);
  return { commit, dirty: outputText(status.stdout).length !== 0 };
}

async function fileDigest(filePath, label) {
  try { return `sha256:${digestBytes(await readFile(filePath))}`; } catch { fail(`${label} is not readable`); }
}

export async function validateUpdateBoundary(result, { root = ROOT, gitState } = {}) {
  const state = gitState ?? await currentGitState(root);
  if (state.dirty) fail("catalog update requires a clean Git worktree");
  if (result.provenance.commit !== state.commit) fail("update result provenance.commit does not match current HEAD");
  const expectedCatalogDigest = await fileDigest(path.resolve(root, "benchmarks/executable-catalog.json"), "catalog");
  if (result.provenance.catalogDigest !== expectedCatalogDigest) fail("update result provenance.catalogDigest is stale");
  const expectedRunnerDigest = await fileDigest(path.resolve(root, "tooling/executable-benchmark-runner.mjs"), "runner");
  if (result.provenance.runnerDigest !== expectedRunnerDigest) fail("update result provenance.runnerDigest is stale");
  return state;
}

export const validateRecordBoundary = validateUpdateBoundary;

export async function publishLiveCatalog(result, { root = ROOT, gitState } = {}) {
  const documents = loadExecutableDocuments(root);
  const catalogErrors = validateExecutableCatalog(documents.catalog, documents, root);
  if (catalogErrors.length > 0) fail(catalogErrors.join("; "));
  const resultErrors = validateExecutableResult(result, documents.catalog);
  if (resultErrors.length > 0) fail(resultErrors.join("; "));
  await validateUpdateBoundary(result, { root, gitState });
  const updated = updateExecutableBestMetrics(documents.catalog, result);
  const nextErrors = validateExecutableCatalog(updated.catalog, documents, root);
  if (nextErrors.length > 0) fail(nextErrors.join("; "));
  if (!updated.changed) return updated;
  const benchmarksRoot = path.resolve(root, "benchmarks");
  const catalogPath = path.resolve(benchmarksRoot, "executable-catalog.json");
  const projectionPath = path.resolve(benchmarksRoot, "EXECUTABLES.md");
  const catalogBytes = `${JSON.stringify(updated.catalog, null, 2)}\n`;
  const projectionBytes = `${renderExecutableProjection({ catalog: updated.catalog, root })}\n`;
  await writeAtomicFile(catalogPath, catalogBytes, benchmarksRoot);
  await writeAtomicFile(projectionPath, projectionBytes, benchmarksRoot);
  return updated;
}

async function listCommand(root = ROOT) {
  const documents = loadExecutableDocuments(root);
  const errors = validateExecutableCatalog(documents.catalog, documents, root);
  if (errors.length > 0) fail(errors.join("; "));
  console.log(JSON.stringify({
    catalog: documents.catalog.id,
    status: documents.catalog.status,
    bestMetrics: documents.catalog.bestMetrics.entries.length,
    workloads: documents.catalog.workloads.map((workload) => ({
      id: workload.id,
      sourceReadiness: workload.sourceReadiness,
      benchmarkStatus: workload.benchmarkStatus,
      languages: workload.sources.map((source) => source.language),
    })),
  }, null, 2));
}

async function checkCommand(root = ROOT) {
  const documents = loadExecutableDocuments(root);
  const errors = [
    ...validateExecutableCatalog(documents.catalog, documents, root),
    ...validateExecutableBestMetrics(documents.catalog.bestMetrics, documents.catalog),
  ];
  if (errors.length > 0) fail(errors.join("; "));
  const rendered = `${await renderFromDisk(root)}\n`;
  const projection = path.resolve(root, "benchmarks", "EXECUTABLES.md");
  const current = await readFile(projection, "utf8").catch((error) => error?.code === "ENOENT" ? undefined : Promise.reject(error));
  if (current !== rendered) fail(`generated projection is stale: ${path.relative(root, projection).replaceAll(path.sep, "/")}`);
  console.log("benchmark catalog/live-best/projection: current");
}

async function runCommand(options, root = ROOT) {
  const output = path.resolve(root, options.output);
  await runBenchmark({ target: options.target, language: options.language, warmup: options.warmup, samples: options.samples, output });
}

export async function main(argv = process.argv.slice(2), dependencies = {}) {
  const options = parseBenchmarkCliArguments(argv);
  const root = dependencies.root ?? ROOT;
  if (options.command === "help") console.log(benchmarkUsage());
  else if (options.command === "list") await listCommand(root);
  else if (options.command === "check") await checkCommand(root);
  else if (options.command === "run") await (dependencies.runBenchmark ?? runCommand)(options, root);
  else {
    const { candidate, value } = await readResultInput(options.input, root);
    const documents = loadExecutableDocuments(root);
    const errors = validateExecutableResult(value, documents.catalog);
    if (errors.length > 0) fail(errors.join("; "));
    if (options.command === "update") {
      const published = await (dependencies.publishLiveCatalog ?? publishLiveCatalog)(value, { root, gitState: dependencies.gitState });
      await (dependencies.consumeLocalResult ?? consumeLocalResult)(candidate, path.resolve(root, RESULTS_PATH));
      console.log(published.changed ? `updated live catalog (${published.updatedMetrics.join(", ")})` : "valid result is a non-improving no-op; consumed local result");
    } else console.log(`valid executable result: ${options.input}`);
  }
  return 0;
}

if (import.meta.main) {
  main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
  });
}
