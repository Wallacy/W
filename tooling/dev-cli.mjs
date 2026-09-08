import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import {
  flattenCheckSuite,
  loadCheckSuites,
  runCheckSuite,
} from "./check-suite.mjs";
import {
  readGitState,
  validateOutputDirectory,
  validateReceipt,
} from "./build-w-windows.mjs";

export const repositoryRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "..",
);
export const catalogPath = path.join(repositoryRoot, "tooling", "dev-cli.json");
export const DEV_CLI_SCHEMA = "w-dev-cli-1";
export const CHECK_TARGETS = Object.freeze([
  "quick",
  "compiler",
  "docs",
  "studies",
  "all",
]);
export const DEMO_TARGETS = Object.freeze(["hello", "bool-short-circuit"]);

const HOST_BINARY_ERROR =
  "development w.exe is unavailable; run bun bootstrap --target host " +
  "after materializing the pinned Windows toolchain (no download is performed)";

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function hasExactKeys(value, keys) {
  return isObject(value) &&
    JSON.stringify(Object.keys(value).sort()) === JSON.stringify([...keys].sort());
}

function isContained(root, candidate) {
  const relative = path.relative(path.resolve(root), path.resolve(candidate));
  return relative === "" ||
    (relative !== ".." &&
      !relative.startsWith(`..${path.sep}`) &&
      !path.isAbsolute(relative));
}

function validateRelativePath(value, label, root, errors, extension = null) {
  if (typeof value !== "string" || value.length === 0 || path.isAbsolute(value)) {
    errors.push(`${label} must be a repository-relative path`);
    return null;
  }
  const resolved = path.resolve(root, value);
  if (!isContained(root, resolved)) {
    errors.push(`${label} escapes the repository root`);
    return null;
  }
  if (extension !== null && path.extname(value).toLowerCase() !== extension) {
    errors.push(`${label} must use ${extension}`);
  }
  return resolved;
}

export function validateCatalog({ catalog, root = repositoryRoot } = {}) {
  const errors = [];
  if (!isObject(catalog)) {
    return { errors: ["catalog must be an object"] };
  }
  if (!hasExactKeys(catalog, ["$schema", "version", "checks", "demos", "host"]))
    errors.push("catalog keys are invalid");
  if (catalog.$schema !== DEV_CLI_SCHEMA)
    errors.push(`catalog.$schema must be ${DEV_CLI_SCHEMA}`);
  if (catalog.version !== 1) errors.push("catalog.version must be 1");

  if (!isObject(catalog.checks)) {
    errors.push("catalog.checks must be an object");
  } else {
    for (const target of CHECK_TARGETS) {
      const record = catalog.checks[target];
      if (!isObject(record)) {
        errors.push(`catalog check ${JSON.stringify(target)} must be an object`);
        continue;
      }
      if (!hasExactKeys(record, ["suite", "description"]))
        errors.push(`catalog check ${JSON.stringify(target)} keys are invalid`);
      if (typeof record.suite !== "string" || record.suite.length === 0)
        errors.push(`catalog check ${JSON.stringify(target)} must name a suite`);
      if (typeof record.description !== "string" || record.description.length === 0)
        errors.push(`catalog check ${JSON.stringify(target)} must have a description`);
    }
    for (const target of Object.keys(catalog.checks)) {
      if (!CHECK_TARGETS.includes(target))
        errors.push(`catalog has unknown check target ${JSON.stringify(target)}`);
    }
  }

  if (!isObject(catalog.demos)) {
    errors.push("catalog.demos must be an object");
  } else {
    for (const target of DEMO_TARGETS) {
      const record = catalog.demos[target];
      if (!isObject(record)) {
        errors.push(`catalog demo ${JSON.stringify(target)} must be an object`);
        continue;
      }
      if (!hasExactKeys(record, ["fixture", "description"]))
        errors.push(`catalog demo ${JSON.stringify(target)} keys are invalid`);
      validateRelativePath(record.fixture, `catalog demo ${JSON.stringify(target)}.fixture`,
        root, errors, ".w");
      if (typeof record.description !== "string" || record.description.length === 0)
        errors.push(`catalog demo ${JSON.stringify(target)} must have a description`);
    }
    for (const target of Object.keys(catalog.demos)) {
      if (!DEMO_TARGETS.includes(target))
        errors.push(`catalog has unknown demo target ${JSON.stringify(target)}`);
    }
  }

  if (!isObject(catalog.host)) {
    errors.push("catalog.host must be an object");
  } else {
    if (!hasExactKeys(catalog.host, [
      "platform", "arch", "binary", "receipt", "recipe", "profile", "c11Recovery",
    ])) errors.push("catalog.host keys are invalid");
    if (catalog.host.platform !== "win32")
      errors.push("catalog.host.platform must be win32 for DEVCLI1");
    if (catalog.host.arch !== "x64")
      errors.push("catalog.host.arch must be x64 for DEVCLI1");
    for (const key of ["binary", "receipt", "recipe"]) {
      validateRelativePath(catalog.host[key], `catalog host ${key}`, root, errors);
    }
    if (catalog.host.binary !== "build/w-windows/w.exe")
      errors.push("catalog host binary must be build/w-windows/w.exe");
    if (catalog.host.receipt !== "build/w-windows/receipt.json")
      errors.push("catalog host receipt must be build/w-windows/receipt.json");
    if (catalog.host.recipe !== "tooling/build-w-windows.mjs")
      errors.push("catalog host recipe must be tooling/build-w-windows.mjs");
    if (catalog.host.profile !== "development")
      errors.push("catalog.host.profile must be development");
    if (catalog.host.c11Recovery !== true)
      errors.push("catalog.host.c11Recovery must be true for the current Windows recipe");
  }
  return { errors };
}

export function loadCatalog(root = repositoryRoot) {
  const file = path.join(root, "tooling", "dev-cli.json");
  let catalog;
  try {
    catalog = JSON.parse(fs.readFileSync(file, "utf8"));
  } catch (error) {
    throw new Error(`cannot load ${file}: ${error.message}`);
  }
  const validation = validateCatalog({ catalog, root });
  if (validation.errors.length > 0)
    throw new Error(validation.errors.join("\n"));
  return catalog;
}

function readOptionValue(argv, index, option) {
  const value = argv[index + 1];
  if (typeof value !== "string" || value.length === 0 || value.startsWith("--"))
    throw new Error(`${option} requires exactly one value`);
  return value;
}

function assertArray(argv, label) {
  if (!Array.isArray(argv)) throw new Error(`${label} must be an array`);
}

export function parseCheckArguments(argv) {
  assertArray(argv, "check arguments");
  const options = { target: "quick", list: false, dryRun: false, help: false };
  let targetCount = 0;
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--target") {
      targetCount += 1;
      options.target = readOptionValue(argv, index, "--target");
      index += 1;
      if (!CHECK_TARGETS.includes(options.target))
        throw new Error(`unknown check target: ${options.target}`);
    } else if (argument === "--list") {
      if (options.list) throw new Error("--list may be used only once");
      options.list = true;
    } else if (argument === "--dry-run") {
      if (options.dryRun) throw new Error("--dry-run may be used only once");
      options.dryRun = true;
    } else if (argument === "--help") {
      if (options.help) throw new Error("--help may be used only once");
      options.help = true;
    } else {
      throw new Error(`unknown check option: ${String(argument)}`);
    }
  }
  if (targetCount > 1) throw new Error("--target may be used only once");
  if (options.help && (options.list || options.dryRun || targetCount > 0))
    throw new Error("--help cannot be combined with another option");
  if (options.list && targetCount > 0)
    throw new Error("--list cannot be combined with --target");
  if (options.list && options.dryRun)
    throw new Error("--list cannot be combined with --dry-run");
  return options;
}

export function parseDemoArguments(argv) {
  assertArray(argv, "demo arguments");
  const options = { target: "hello", list: false, help: false };
  let targetCount = 0;
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--target") {
      targetCount += 1;
      options.target = readOptionValue(argv, index, "--target");
      index += 1;
      if (!DEMO_TARGETS.includes(options.target))
        throw new Error(`unknown demo target: ${options.target}`);
    } else if (argument === "--list") {
      if (options.list) throw new Error("--list may be used only once");
      options.list = true;
    } else if (argument === "--help") {
      if (options.help) throw new Error("--help may be used only once");
      options.help = true;
    } else {
      throw new Error(`unknown demo option: ${String(argument)}`);
    }
  }
  if (targetCount > 1) throw new Error("--target may be used only once");
  if (options.help && (options.list || targetCount > 0))
    throw new Error("--help cannot be combined with another option");
  if (options.list && targetCount > 0)
    throw new Error("--list cannot be combined with --target");
  return options;
}

export function parseBootstrapArguments(argv) {
  assertArray(argv, "bootstrap arguments");
  const options = { target: null, help: false };
  let targetCount = 0;
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--target") {
      targetCount += 1;
      options.target = readOptionValue(argv, index, "--target");
      index += 1;
      if (options.target !== "host")
        throw new Error(`unknown bootstrap target: ${options.target}`);
    } else if (argument === "--help") {
      if (options.help) throw new Error("--help may be used only once");
      options.help = true;
    } else {
      throw new Error(`unknown bootstrap option: ${String(argument)}`);
    }
  }
  if (targetCount > 1) throw new Error("--target may be used only once");
  if (!options.help && options.target === null)
    throw new Error("bootstrap requires --target host");
  if (options.help && (targetCount > 0 || options.target !== null))
    throw new Error("--help cannot be combined with another option");
  return options;
}

export function parseDevRunArguments(argv) {
  assertArray(argv, "dev arguments");
  if (argv.length === 0 || argv[0] === "--help")
    return { help: true, source: null, args: [] };
  if (argv[0] !== "run")
    throw new Error("dev currently supports only `run <explicit .w path> [-- args]`");
  const separator = argv.indexOf("--");
  const commandArgs = separator < 0 ? argv.slice(1) : argv.slice(1, separator);
  if (commandArgs.length !== 1 || commandArgs[0].startsWith("--"))
    throw new Error("dev run requires exactly one explicit .w path before --");
  return {
    help: false,
    source: commandArgs[0],
    args: separator < 0 ? [] : argv.slice(separator + 1),
  };
}

export function formatCheckList({ catalog, suites } = {}) {
  return CHECK_TARGETS.map((target) => {
    const record = catalog.checks[target];
    const count = checkPlan({ catalog, suites, target }).steps.length;
    return `${target}\t${record.suite}\t${count}\t${record.description}`;
  }).join("\n") + "\n";
}

export function formatDemoList(catalog) {
  return DEMO_TARGETS.map((target) => {
    const record = catalog.demos[target];
    return `${target}\t${record.fixture}\t${record.description}`;
  }).join("\n") + "\n";
}

export function checkPlan({ catalog, suites, target = "quick" } = {}) {
  if (!CHECK_TARGETS.includes(target))
    throw new Error(`unknown check target: ${target}`);
  const record = catalog?.checks?.[target];
  if (!isObject(record) || typeof record.suite !== "string")
    throw new Error(`check target ${JSON.stringify(target)} is not configured`);
  return {
    target,
    suite: record.suite,
    steps: flattenCheckSuite({ suites, suiteName: record.suite }),
  };
}

export function resolveContainedFile(root, value, label, extension = null) {
  if (typeof value !== "string" || value.length === 0)
    throw new Error(`${label} is required`);
  const lexical = path.resolve(root, value);
  if (!isContained(root, lexical)) throw new Error(`${label} escapes the repository root`);
  let physical;
  try {
    physical = fs.realpathSync(lexical);
  } catch (error) {
    throw new Error(`${label} is unavailable: ${error.message}`);
  }
  if (!isContained(root, physical)) throw new Error(`${label} resolves outside the repository root`);
  if (extension !== null && path.extname(physical).toLowerCase() !== extension)
    throw new Error(`${label} must use ${extension}`);
  let stats;
  try {
    stats = fs.statSync(physical);
  } catch (error) {
    throw new Error(`${label} is unavailable: ${error.message}`);
  }
  if (!stats.isFile()) throw new Error(`${label} is not a regular file`);
  return physical;
}

export function resolveExplicitSource(
  value,
  { cwd = process.cwd(), root = repositoryRoot, label = "dev run source" } = {},
) {
  if (typeof value !== "string" || value.length === 0)
    throw new Error(`${label} is required`);
  const candidates = path.isAbsolute(value)
    ? [path.resolve(value)]
    : [
      path.resolve(cwd, value),
      ...(path.resolve(cwd) === path.resolve(root) ? [] : [path.resolve(root, value)]),
    ];
  let lastError = null;
  for (const lexical of candidates) {
    if (path.extname(lexical).toLowerCase() !== ".w") continue;
    let physical;
    try {
      physical = fs.realpathSync(lexical);
      const stats = fs.statSync(physical);
      if (!stats.isFile()) throw new Error("is not a regular file");
      if (path.extname(physical).toLowerCase() !== ".w")
        throw new Error("resolved file must use .w");
      return physical;
    } catch (error) {
      lastError = error;
    }
  }
  if (lastError !== null)
    throw new Error(`${label} is unavailable: ${lastError.message}`);
  throw new Error(`${label} must use .w`);
}

export function hostBinaryPath({ root = repositoryRoot, catalog = loadCatalog(root) } = {}) {
  return path.resolve(root, catalog.host.binary);
}

export function bootstrapCommand({ catalog = loadCatalog() } = {}) {
  const args = ["run", catalog.host.recipe, "--profile", catalog.host.profile];
  if (catalog.host.c11Recovery) args.push("--c11-recovery");
  return { command: process.execPath, args };
}

function childStatus(result) {
  if (result?.error) return null;
  if (Number.isInteger(result?.status)) return result.status;
  if (Number.isInteger(result?.exitCode)) return result.exitCode;
  return 1;
}

export function invokeChild(command, args, {
  root = repositoryRoot,
  spawn = spawnSync,
  stdio = "inherit",
} = {}) {
  let result;
  try {
    result = spawn(command, args, {
      cwd: root,
      stdio,
      windowsHide: true,
      shell: false,
    });
  } catch (error) {
    process.stderr.write(`dev-cli: child process failed: ${error.message}\n`);
    return 1;
  }
  if (result?.error) {
    process.stderr.write(`dev-cli: child process failed: ${result.error.message}\n`);
    return 1;
  }
  return childStatus(result);
}

function hostSupported(catalog, platform = process.platform, arch = process.arch) {
  return platform === catalog.host.platform && arch === catalog.host.arch;
}

export function validateHostSourceIdentity({
  receipt,
  source,
  warn = (message) => process.stderr.write(`dev-cli: ${message}\n`),
} = {}) {
  if (receipt?.source?.head !== source?.head) {
    throw new Error(
      `host receipt HEAD ${receipt?.source?.head ?? "<missing>"} does not match current HEAD ${source?.head ?? "<missing>"}`,
    );
  }
  if (receipt?.source?.dirty !== source?.dirty)
    throw new Error(
      `host receipt dirty flag ${String(receipt?.source?.dirty)} does not match current source dirty flag ${String(source?.dirty)}`,
    );
  if (source?.dirty === true) {
    warn(
      "host build has a dirty source identity; artifact bytes and receipt are checked, " +
      "but this is non-strong identity evidence (not a clean-tree verification)",
    );
  }
  return { strong: source?.dirty === false };
}

export async function validateHostBuild({
  root = repositoryRoot,
  catalog = loadCatalog(root),
  validateBuild = validateOutputDirectory,
  warn = (message) => process.stderr.write(`dev-cli: ${message}\n`),
} = {}) {
  const binary = resolveContainedFile(root, catalog.host.binary, "development w.exe");
  const outputDirectory = path.dirname(binary);
  if (path.resolve(root, catalog.host.receipt) !== path.join(outputDirectory, "receipt.json"))
    throw new Error("host receipt path is not the canonical output receipt");
  const receipt = await validateBuild(outputDirectory);
  const receiptErrors = validateReceipt(receipt);
  if (receiptErrors.length > 0)
    throw new Error(`host receipt is invalid: ${receiptErrors.join("; ")}`);
  if (receipt.profile?.selected !== "development")
    throw new Error(`host build profile is ${JSON.stringify(receipt.profile?.selected)}; development is required`);
  const source = readGitState(root);
  const identity = validateHostSourceIdentity({ receipt, source, warn });
  return { binary, outputDirectory, receipt, source, identity };
}

export async function runBootstrap({
  root = repositoryRoot,
  catalog = loadCatalog(root),
  spawn = spawnSync,
  validateBuild = validateHostBuild,
  platform = process.platform,
  arch = process.arch,
} = {}) {
  if (!hostSupported(catalog, platform, arch)) {
    process.stderr.write(
      `dev-cli: bootstrap --target host currently supports native ${catalog.host.platform}/${catalog.host.arch} only; ` +
      "Linux is explicitly unsupported in DEVCLI1\n",
    );
    return 2;
  }
  const startedAt = process.hrtime.bigint();
  const { command, args } = bootstrapCommand({ catalog });
  const status = invokeChild(command, args, { root, spawn });
  const elapsedMs = Number(process.hrtime.bigint() - startedAt) / 1_000_000;
  process.stderr.write(
    `dev-cli: bootstrap ${status === 0 ? "ok" : "failed"} after ${elapsedMs.toFixed(1)} ms ` +
    "(DX metadata; non-benchmark)\n",
  );
  if (status !== 0) return status;
  let build;
  try {
    build = await validateBuild({ root, catalog });
  } catch (error) {
    process.stderr.write(`dev-cli: bootstrap output rejected: ${error.message}\n`);
    return 1;
  }
  process.stderr.write(`dev-cli: development binary ready at ${build.binary}\n`);
  process.stderr.write(
    `dev-cli: receipt/artifact checks passed for ${build.receipt.profile.selected} ` +
    `(non-benchmark DX validation)\n`,
  );
  return 0;
}

export async function runDemo({
  root = repositoryRoot,
  catalog = loadCatalog(root),
  target = "hello",
  spawn = spawnSync,
  validateBuild = validateHostBuild,
  platform = process.platform,
  arch = process.arch,
} = {}) {
  if (!DEMO_TARGETS.includes(target))
    throw new Error(`unknown demo target: ${target}`);
  if (!hostSupported(catalog, platform, arch)) {
    process.stderr.write(
      `dev-cli: demo target ${target} requires native ${catalog.host.platform}/${catalog.host.arch}; ` +
      "Linux is explicitly unsupported in DEVCLI1\n",
    );
    return 2;
  }
  let build;
  try {
    build = await validateBuild({ root, catalog });
  } catch (error) {
    throw new Error(`${HOST_BINARY_ERROR}: ${error.message}`);
  }
  const binary = build.binary;
  const fixture = resolveContainedFile(root, catalog.demos[target].fixture,
    `demo fixture ${target}`, ".w");
  const startedAt = process.hrtime.bigint();
  const status = invokeChild(binary, ["run", fixture], { root, spawn });
  const elapsedMs = Number(process.hrtime.bigint() - startedAt) / 1_000_000;
  process.stderr.write(
    `dev-cli: demo ${target} ${status === 0 ? "ok" : "failed"} after ${elapsedMs.toFixed(1)} ms ` +
    "(DX metadata; non-benchmark)\n",
  );
  return status;
}

export async function runDevRun({
  root = repositoryRoot,
  catalog = loadCatalog(root),
  source,
  args = [],
  spawn = spawnSync,
  validateBuild = validateHostBuild,
  platform = process.platform,
  arch = process.arch,
  cwd = process.cwd(),
  reportTiming = true,
} = {}) {
  if (!hostSupported(catalog, platform, arch)) {
    process.stderr.write(
      `dev-cli: dev run currently supports native ${catalog.host.platform}/${catalog.host.arch} only; ` +
      "Linux is explicitly unsupported in DEVCLI1\n",
    );
    return 2;
  }
  const sourcePath = resolveExplicitSource(source, { cwd, root, label: "dev run source" });
  let build;
  try {
    build = await validateBuild({ root, catalog });
  } catch (error) {
    process.stderr.write(`dev-cli: existing host build is unusable: ${error.message}\n`);
    const bootstrapStatus = await runBootstrap({
      root, catalog, spawn, validateBuild, platform, arch,
    });
    if (bootstrapStatus !== 0) return bootstrapStatus;
    try {
      build = await validateBuild({ root, catalog });
    } catch (error) {
      process.stderr.write(`dev-cli: ${error.message}\n`);
      return 1;
    }
  }
  const binary = build.binary;
  if (!Array.isArray(args)) throw new Error("dev run forwarded arguments must be an array");
  if (args.some((argument) => typeof argument !== "string"))
    throw new Error("dev run forwarded arguments must be strings");
  const childArgs = ["run", sourcePath];
  if (args.length > 0) childArgs.push("--", ...args);
  const startedAt = process.hrtime.bigint();
  const status = invokeChild(binary, childArgs, { root, spawn });
  const elapsedMs = Number(process.hrtime.bigint() - startedAt) / 1_000_000;
  if (reportTiming) {
    process.stderr.write(
      `dev-cli: dev run ${status === 0 ? "ok" : "failed"} after ${elapsedMs.toFixed(1)} ms ` +
      "(DX metadata; non-benchmark)\n",
    );
  }
  return status;
}

const HELP = `usage:
  bun check [--target quick|compiler|docs|studies|all] [--list] [--dry-run]
  bun demo [--target hello|bool-short-circuit] [--list]
  bun bootstrap --target host
  bun dev run <explicit .w path> [-- args]

check defaults to quick and reads tooling/check-suites.json.
demo and dev run use the public w run route through the current MLIR native host build.
bootstrap is local-only and never downloads a toolchain; the current host is Windows x64.
`;

function printHelp(text = HELP) {
  process.stdout.write(text);
}

export async function main(argv = process.argv.slice(2)) {
  try {
    const command = argv[0];
    const rest = argv.slice(1);
    if (command === undefined || command === "--help" || command === "-h") {
      if (rest.length > 0) throw new Error("help cannot be combined with another command");
      printHelp();
      return 0;
    }
    if (command === "check") {
      const options = parseCheckArguments(rest);
      if (options.help) {
        printHelp(HELP.split("\n").slice(0, 2).join("\n") + "\n");
        return 0;
      }
      const catalog = loadCatalog();
      const loaded = loadCheckSuites();
      if (options.list) {
        process.stdout.write(formatCheckList({ catalog, suites: loaded.suites }));
        return 0;
      }
      const suite = catalog.checks[options.target].suite;
      return runCheckSuite({
        root: repositoryRoot,
        packageRecords: loaded.packages,
        suites: loaded.suites,
        suiteName: suite,
        dryRun: options.dryRun,
      });
    }
    if (command === "demo") {
      const options = parseDemoArguments(rest);
      if (options.help) {
        printHelp(HELP.split("\n").slice(0, 3).join("\n") + "\n");
        return 0;
      }
      const catalog = loadCatalog();
      if (options.list) {
        process.stdout.write(formatDemoList(catalog));
        return 0;
      }
      return await runDemo({ catalog, target: options.target });
    }
    if (command === "bootstrap") {
      const options = parseBootstrapArguments(rest);
      if (options.help) {
        printHelp(HELP.split("\n").slice(0, 4).join("\n") + "\n");
        return 0;
      }
      return await runBootstrap({ catalog: loadCatalog() });
    }
    if (command === "dev") {
      const options = parseDevRunArguments(rest);
      if (options.help) {
        printHelp(HELP.split("\n").slice(0, 5).join("\n") + "\n");
        return 0;
      }
      return await runDevRun({ catalog: loadCatalog(), source: options.source, args: options.args });
    }
    throw new Error(`unknown command: ${command}`);
  } catch (error) {
    process.stderr.write(`dev-cli: ${error.message}\n`);
    return 2;
  }
}

if (import.meta.main) process.exit(await main());
