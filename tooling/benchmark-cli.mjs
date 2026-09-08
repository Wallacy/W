import crypto from "node:crypto";
import {
  lstat,
  link,
  mkdir,
  readFile,
  rename,
  rm,
  rmdir,
  writeFile,
} from "node:fs/promises";
import path from "node:path";
import {
  EXECUTABLE_HISTORY_INDEX_PATH,
  BEST_KNOWN_PATH,
  ROOT,
  RESULT_HISTORY_PATH,
  deriveExecutableBestKnown,
  loadExecutableHistoryResults,
  loadExecutableDocuments,
  validateExecutableBestKnownIndex,
  validateExecutableBestKnownFreshness,
  validateExecutableCatalog,
  validateExecutableHistory,
  validateExecutableResult,
} from "./executable-benchmark-machine.mjs";
import { renderExecutableProjection, renderFromDisk, writeAtomicFile } from "./executable-benchmark-docs.mjs";
import { runBenchmark } from "./executable-benchmark-runner.mjs";

const RESULTS_PATH = "benchmarks/results";
const HISTORY_ROOT = path.resolve(ROOT, RESULT_HISTORY_PATH);
const RUN_TARGETS = Object.freeze(["hello", "restaurant-branch"]);

function fail(message) {
  throw new Error(`benchmark: ${message}`);
}

function isContained(parent, candidate) {
  const relative = path.relative(path.resolve(parent), path.resolve(candidate));
  return relative === "" || (relative !== ".." && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative));
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
  if (!["list", "run", "validate", "record", "check"].includes(command)) fail(`unknown command: ${command}`);
  if (command === "check") {
    if (argv.length !== 1) fail("check does not accept positional arguments or options");
    return { command };
  }
  if (command === "list") {
    if (argv.length !== 1) fail("list does not accept --target or other options");
    return { command };
  }
  if (command === "validate" || command === "record") {
    if (argv.length !== 2 || argv[1].startsWith("--")) fail(`${command} requires exactly one JSON path`);
    return { command, input: argv[1] };
  }
  const result = {
    command,
    target: "hello",
    language: "w",
    output: undefined,
    warmup: 1,
    samples: 9,
  };
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
  if (!['w', 'c', 'rust'].includes(result.language)) fail(`unsupported language: ${result.language}`);
  if (result.samples < 9 || result.samples % 2 === 0) fail("--samples must be odd and at least nine");
  result.output ??= `benchmarks/results/${result.target}-${result.language}.local.json`;
  return result;
}

export function benchmarkUsage() {
  return [
    "usage: bun benchmark <list|run|validate|record|check>",
    "",
    "  list",
    "  run --target hello|restaurant-branch --language w|c|rust [--output benchmarks/results/<new>.json] [--warmup 1] [--samples 9]",
    "  validate <result.json>",
    "  record <result.json>    (content-addressed history publication; consumes a local result on success)",
    "  check",
    "",
    "Run measures one selected source with its catalog exact-output oracle. C probes -std=c23/-std=c2x for the MinGW ABI, and Rust uses rustc edition 2024 for the MSVC ABI. W uses the private Native0/MLIR0 source-to-PE candidate for workloads that declare that recipe; public-w-run targets require retained-artifact and separate compile-run support.",
  ].join("\n");
}

function safeJsonPath(input, label, allowedRoots) {
  if (typeof input !== "string" || input.length === 0) fail(`${label} requires a path`);
  const candidate = path.resolve(process.cwd(), input);
  if (path.extname(candidate).toLowerCase() !== ".json" || !allowedRoots.some((root) => isContained(root, candidate))) fail(`${label} must be a JSON file contained by the repository benchmark directories`);
  return candidate;
}

async function readResultInput(input, root = ROOT) {
  const rootPath = path.resolve(root);
  const candidate = safeJsonPath(input, "result path", [path.resolve(rootPath, RESULTS_PATH), path.resolve(rootPath, RESULT_HISTORY_PATH)]);
  await regularFile(candidate, "result path");
  let value;
  try { value = JSON.parse((await readFile(candidate, "utf8"))); } catch { fail("result path must contain valid JSON"); }
  return { candidate, value };
}

export async function consumeRecordedLocalResult(candidate, resultsRoot = path.resolve(ROOT, RESULTS_PATH)) {
  const resolved = path.resolve(candidate);
  const root = path.resolve(resultsRoot);
  if (!isContained(root, resolved)) return false;
  await regularFile(resolved, "recorded local result");
  await rm(resolved);
  try {
    await rmdir(root);
  } catch (error) {
    if (error?.code !== "ENOENT" && error?.code !== "ENOTEMPTY" && error?.code !== "EEXIST") throw error;
  }
  return true;
}

function digestBytes(bytes) {
  return crypto.createHash("sha256").update(bytes).digest("hex");
}

function jsonBytes(value) {
  return Buffer.from(`${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function outputText(value) {
  return Buffer.from(value ?? "").toString("utf8");
}

function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

function historyReferenceSort(left, right) {
  return compareText(String(left?.id ?? ""), String(right?.id ?? "")) || compareText(String(left?.path ?? ""), String(right?.path ?? ""));
}

export async function currentGitState(root = ROOT) {
  const git = Bun.which("git");
  if (!git) fail("git is required for history provenance");
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

export async function validateRecordBoundary(record, { root = ROOT, gitState } = {}) {
  const state = gitState ?? await currentGitState(root);
  if (state.dirty) fail("history publication requires a clean Git worktree");
  if (record.provenance.commit !== state.commit) fail("history record provenance.commit does not match current HEAD");
  const expectedCatalogDigest = await fileDigest(path.resolve(root, "benchmarks/executable-catalog.json"), "catalog");
  if (record.provenance.catalogDigest !== expectedCatalogDigest) fail("history record provenance.catalogDigest is stale");
  const expectedRunnerDigest = await fileDigest(path.resolve(root, "tooling/executable-benchmark-runner.mjs"), "runner");
  if (record.provenance.runnerDigest !== expectedRunnerDigest) fail("history record provenance.runnerDigest is stale");
  return state;
}

async function refreshProjection(root) {
  const rendered = `${await renderFromDisk(root)}\n`;
  const projection = path.resolve(root, "benchmarks", "EXECUTABLES.md");
  await writeAtomicFile(projection, rendered, path.resolve(root, "benchmarks"));
}

export async function publishHistoryRecord(record, { root = ROOT, gitState } = {}) {
  const documents = loadExecutableDocuments(root);
  const historyErrors = validateExecutableHistory(documents.history, documents.catalog, root);
  if (historyErrors.length > 0) fail(historyErrors.join("; "));
  const currentResults = loadExecutableHistoryResults(documents.history, root).map((item) => item.record);
  const catalogErrors = validateExecutableCatalog(documents.catalog, { ...documents, historyResults: currentResults }, root);
  if (catalogErrors.length > 0) fail(catalogErrors.join("; "));
  const resultErrors = validateExecutableResult(record, documents.catalog);
  if (resultErrors.length > 0) fail(resultErrors.join("; "));
  await validateRecordBoundary(record, { root, gitState });
  const historyRoot = path.resolve(root, RESULT_HISTORY_PATH);
  const historyIndex = path.resolve(root, EXECUTABLE_HISTORY_INDEX_PATH);
  const bestKnownPath = path.resolve(root, BEST_KNOWN_PATH);
  const projection = path.resolve(root, "benchmarks", "EXECUTABLES.md");
  const benchmarksRoot = path.resolve(root, "benchmarks");
  await mkdir(historyRoot, { recursive: true });
  await assertNoReparseAncestors(historyRoot, benchmarksRoot);
  const bytes = jsonBytes(record);
  const digestHex = digestBytes(bytes);
  const digest = `sha256:${digestHex}`;
  const recordName = `${digestHex}.json`;
  const recordPath = path.join(historyRoot, recordName);
  if (!isContained(historyRoot, recordPath)) fail("history record path escaped its directory");
  try {
    await lstat(recordPath);
    fail(`history record already exists: ${recordName}`);
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
  if (documents.history.records.some((reference) => reference.id === record.id)) fail(`history already contains result id: ${record.id}`);
  const temporaryRecord = path.join(historyRoot, `.record-${process.pid}-${crypto.randomUUID()}.tmp`);
  const temporaryIndex = path.join(historyRoot, `.index-${process.pid}-${crypto.randomUUID()}.tmp`);
  const temporaryBestKnown = path.join(path.dirname(bestKnownPath), `.${path.basename(bestKnownPath)}.${process.pid}-${crypto.randomUUID()}.tmp`);
  const temporaryProjection = path.join(path.dirname(projection), `.${path.basename(projection)}.${process.pid}-${crypto.randomUUID()}.tmp`);
  let linked = false;
  let indexPublished = false;
  try {
    await writeFile(temporaryRecord, bytes, { flag: "wx" });
    await assertNoReparseAncestors(temporaryRecord, historyRoot);
    await link(temporaryRecord, recordPath);
    linked = true;
    await rm(temporaryRecord, { force: true });
    const nextIndex = {
      ...documents.history,
      status: "recorded",
      records: [...documents.history.records, { id: record.id, path: recordName, digest }]
        .sort(historyReferenceSort),
    };
    const nextErrors = validateExecutableHistory(nextIndex, documents.catalog, root);
    if (nextErrors.length > 0) fail(nextErrors.join("; "));
    const nextResults = [...currentResults, record];
    const nextBestKnown = deriveExecutableBestKnown(documents.catalog, nextResults);
    const bestKnownErrors = validateExecutableBestKnownIndex(nextBestKnown, documents.catalog, nextResults);
    if (bestKnownErrors.length > 0) fail(bestKnownErrors.join("; "));
    await assertNoReparseAncestors(temporaryBestKnown, benchmarksRoot);
    await assertNoReparseAncestors(temporaryProjection, benchmarksRoot);
    await writeFile(temporaryIndex, `${JSON.stringify(nextIndex, null, 2)}\n`, { flag: "wx" });
    await assertNoReparseAncestors(temporaryIndex, historyRoot);
    await regularFile(historyIndex, "history index");
    await writeFile(temporaryBestKnown, `${JSON.stringify(nextBestKnown, null, 2)}\n`, { flag: "wx" });
    await writeFile(temporaryProjection, `${renderExecutableProjection({ catalog: documents.catalog, history: nextIndex, bestKnown: nextBestKnown, root })}\n`, { flag: "wx" });
    await assertNoReparseAncestors(temporaryIndex, historyRoot);
    await assertNoReparseAncestors(temporaryBestKnown, benchmarksRoot);
    await assertNoReparseAncestors(temporaryProjection, benchmarksRoot);
    try {
      await regularFile(bestKnownPath, "best-known index");
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
    try {
      await regularFile(projection, "generated projection");
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
    await rename(temporaryIndex, historyIndex);
    indexPublished = true;
    await rename(temporaryBestKnown, bestKnownPath);
    await rename(temporaryProjection, projection);
    return { path: path.relative(root, recordPath).replaceAll(path.sep, "/"), digest };
  } catch (error) {
    await rm(temporaryRecord, { force: true });
    await rm(temporaryIndex, { force: true });
    await rm(temporaryBestKnown, { force: true });
    await rm(temporaryProjection, { force: true });
    if (linked && !indexPublished) await rm(recordPath, { force: true });
    throw error;
  }
}

async function listCommand(root = ROOT) {
  const documents = loadExecutableDocuments(root);
  const errors = validateExecutableCatalog(documents.catalog, documents, root);
  if (errors.length > 0) fail(errors.join("; "));
  console.log(JSON.stringify({
    catalog: documents.catalog.id,
    status: documents.catalog.status,
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
  const historyErrors = validateExecutableHistory(documents.history, documents.catalog, root);
  if (historyErrors.length > 0) fail(historyErrors.join("; "));
  const historyResults = loadExecutableHistoryResults(documents.history, root);
  const resultRecords = historyResults.map((item) => item.record);
  const errors = [
    ...validateExecutableCatalog(documents.catalog, { ...documents, historyResults: resultRecords }, root),
    ...validateExecutableBestKnownIndex(documents.bestKnown, documents.catalog, resultRecords),
    ...validateExecutableBestKnownFreshness(documents.bestKnown, documents.catalog, resultRecords),
  ];
  if (errors.length > 0) fail(errors.join("; "));
  const rendered = `${await renderFromDisk(root)}\n`;
  const projection = path.resolve(root, "benchmarks", "EXECUTABLES.md");
  const current = await readFile(projection, "utf8").catch((error) => error?.code === "ENOENT" ? undefined : Promise.reject(error));
  if (current !== rendered) fail(`generated projection is stale: ${path.relative(root, projection).replaceAll(path.sep, "/")}`);
  console.log("benchmark catalog/history/projection: current");
}

async function runCommand(options, root = ROOT) {
  const output = path.resolve(root, options.output);
  await runBenchmark({ target: options.target, language: options.language, warmup: options.warmup, samples: options.samples, output });
}

export async function main(argv = process.argv.slice(2), dependencies = {}) {
  const options = parseBenchmarkCliArguments(argv);
  const root = dependencies.root ?? ROOT;
  if (options.command === "help") {
    console.log(benchmarkUsage());
    return 0;
  }
  if (options.command === "list") await listCommand(root);
  else if (options.command === "check") await checkCommand(root);
  else if (options.command === "run") await (dependencies.runBenchmark ?? runCommand)(options, root);
  else {
    const { candidate, value } = await readResultInput(options.input, root);
    const documents = loadExecutableDocuments(root);
    const errors = validateExecutableResult(value, documents.catalog);
    if (errors.length > 0) fail(errors.join("; "));
    if (options.command === "record") {
      const published = await (dependencies.publishHistoryRecord ?? publishHistoryRecord)(value, { root, gitState: dependencies.gitState });
      await (dependencies.consumeRecordedLocalResult ?? consumeRecordedLocalResult)(candidate, path.resolve(root, RESULTS_PATH));
      console.log(`recorded ${published.path} (${published.digest})`);
    } else {
      console.log(`valid executable result: ${options.input}`);
    }
  }
  return 0;
}

if (import.meta.main) {
  main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
  });
}
