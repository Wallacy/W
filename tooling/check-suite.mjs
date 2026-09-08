import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

export const repositoryRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "..",
);
export const suiteManifestPath = path.join(
  path.dirname(fileURLToPath(import.meta.url)),
  "check-suites.json",
);
export const commandRegistryPath = path.join(
  path.dirname(fileURLToPath(import.meta.url)),
  "command-registry.json",
);
export const CHECK_SUITE_SCHEMA = "w-check-suites-1";
export const COMMAND_REGISTRY_SCHEMA = "w-command-registry-1";

const NAME_PATTERN = /^[A-Za-z][A-Za-z0-9:_-]*$/u;

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function packageJsonPath(root, packagePath) {
  return path.join(root, packagePath, "package.json");
}

function isContained(root, candidate) {
  const relative = path.relative(root, candidate);
  return relative === "" ||
    (relative !== ".." &&
      !relative.startsWith(`..${path.sep}`) &&
      !path.isAbsolute(relative));
}

function readPackage(root, packagePath) {
  const file = packageJsonPath(root, packagePath);
  try {
    return JSON.parse(fs.readFileSync(file, "utf8"));
  } catch (error) {
    return { __error: `${file}: ${error.message}` };
  }
}

function readJson(file) {
  try {
    return { value: JSON.parse(fs.readFileSync(file, "utf8")), error: null };
  } catch (error) {
    return { value: null, error: `${file}: ${error.message}` };
  }
}

function validateName(value, label, errors) {
  if (typeof value !== "string" || !NAME_PATTERN.test(value)) {
    errors.push(`${label} must be a non-empty package or script name`);
  }
}

function exactKeys(value, keys) {
  return isObject(value) &&
    JSON.stringify(Object.keys(value).sort()) === JSON.stringify([...keys].sort());
}

function validateRelativeDirectory(value, label, root, errors) {
  if (typeof value !== "string" || value.length === 0 || path.isAbsolute(value)) {
    errors.push(`${label} must be a repository-relative directory`);
    return null;
  }
  const resolved = path.resolve(root, value);
  if (!isContained(root, resolved)) {
    errors.push(`${label} escapes the repository root`);
    return null;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isDirectory()) {
    errors.push(`${label} must name an existing directory`);
    return null;
  }
  return resolved;
}

function validateArgumentVector(value, label, errors) {
  if (!Array.isArray(value) || value.length === 0 || value.some((item) => typeof item !== "string")) {
    errors.push(`${label} must be a non-empty array of strings`);
    return false;
  }
  if (value.some((item) => item.includes("\u0000"))) {
    errors.push(`${label} must not contain NUL bytes`);
    return false;
  }
  return true;
}

/**
 * Validate the shell-free command registry used by root check leaves.
 * A command is an ordered list of Bun argv invocations or references to
 * another command. The registry never stores executable shell text.
 */
export function validateCommandRegistry({ registry, root = repositoryRoot } = {}) {
  const errors = [];
  if (!isObject(registry)) {
    return { errors: ["command registry must be an object"], commands: {} };
  }
  if (!exactKeys(registry, ["$schema", "version", "commands"])) {
    errors.push("command registry keys are invalid");
  }
  if (registry.$schema !== COMMAND_REGISTRY_SCHEMA) {
    errors.push(`command registry.$schema must be ${COMMAND_REGISTRY_SCHEMA}`);
  }
  if (registry.version !== 1) errors.push("command registry.version must be 1");
  if (!isObject(registry.commands)) {
    errors.push("command registry.commands must be an object");
    return { errors, commands: {} };
  }

  const commands = registry.commands;
  for (const [name, command] of Object.entries(commands)) {
    validateName(name, `command ${JSON.stringify(name)}`, errors);
    const label = `command ${JSON.stringify(name)}`;
    if (!isObject(command)) {
      errors.push(`${label} must be an object`);
      continue;
    }
    if (!exactKeys(command, ["description", "steps"])) {
      errors.push(`${label} keys are invalid`);
    }
    if (typeof command.description !== "string" || command.description.length === 0) {
      errors.push(`${label} must have a description`);
    }
    if (!Array.isArray(command.steps) || command.steps.length === 0) {
      errors.push(`${label}.steps must be a non-empty array`);
      continue;
    }
    command.steps.forEach((step, index) => {
      const stepLabel = `${label}.steps[${index}]`;
      if (!isObject(step)) {
        errors.push(`${stepLabel} must be an object`);
        return;
      }
      if (step.kind === "command") {
        if (!exactKeys(step, ["kind", "command"])) {
          errors.push(`${stepLabel} command reference keys are invalid`);
        }
        if (typeof step.command !== "string" || !Object.hasOwn(commands, step.command)) {
          errors.push(`${stepLabel} references an unknown command`);
        }
        return;
      }
      if (step.kind !== "bun" || !exactKeys(step, ["kind", "cwd", "args"])) {
        errors.push(`${stepLabel} must be a bun invocation or command reference`);
        return;
      }
      validateRelativeDirectory(step.cwd, `${stepLabel}.cwd`, root, errors);
      validateArgumentVector(step.args, `${stepLabel}.args`, errors);
    });
  }

  const visiting = new Set();
  const visited = new Set();
  function detectCycles(name, trail) {
    if (visiting.has(name)) {
      errors.push(`command cycle: ${[...trail, name].join(" -> ")}`);
      return;
    }
    if (visited.has(name) || !Object.hasOwn(commands, name)) return;
    visiting.add(name);
    const steps = Array.isArray(commands[name]?.steps) ? commands[name].steps : [];
    for (const step of steps) {
      if (isObject(step) && step.kind === "command" && typeof step.command === "string") {
        detectCycles(step.command, [...trail, name]);
      }
    }
    visiting.delete(name);
    visited.add(name);
  }
  for (const name of Object.keys(commands)) detectCycles(name, []);
  return { errors, commands };
}

export function loadCommandRegistry(root = repositoryRoot) {
  const file = path.join(root, "tooling", "command-registry.json");
  const loaded = readJson(file);
  if (loaded.error) throw new Error(`cannot load ${file}: ${loaded.error}`);
  const validation = validateCommandRegistry({ registry: loaded.value, root });
  if (validation.errors.length > 0) throw new Error(validation.errors.join("\n"));
  return { registry: loaded.value, commands: validation.commands };
}

/**
 * Validate a check-suite manifest without running any child process.
 *
 * The manifest is intentionally small. A leaf step names a package and one
 * command from that package. Root commands resolve through the shell-free
 * command registry; Tree-sitter leaves remain package-local because their
 * cwd-sensitive scripts are owned by that package. A suite step names another
 * suite. This keeps aggregate order in one inspectable projection without
 * requiring every root leaf to remain in package.json.
 */
export function validateCheckSuites({ manifest, registry = null, root = repositoryRoot } = {}) {
  const errors = [];
  if (!isObject(manifest)) {
    return { errors: ["manifest must be an object"], packages: {}, suites: {} };
  }
  if (manifest.$schema !== CHECK_SUITE_SCHEMA) {
    errors.push(`manifest.$schema must be ${CHECK_SUITE_SCHEMA}`);
  }
  if (manifest.version !== 1) errors.push("manifest.version must be 1");
  if (!isObject(manifest.packages)) {
    errors.push("manifest.packages must be an object");
  }
  if (!isObject(manifest.suites)) {
    errors.push("manifest.suites must be an object");
  }
  let commandRegistry = registry;
  if (commandRegistry === null) {
    const loaded = readJson(path.join(root, "tooling", "command-registry.json"));
    if (loaded.error) {
      errors.push(loaded.error);
      commandRegistry = {};
    } else {
      commandRegistry = loaded.value;
    }
  }
  const commandValidation = validateCommandRegistry({ registry: commandRegistry, root });
  errors.push(...commandValidation.errors);
  const commands = commandValidation.commands;
  const packageRecords = {};
  const suites = isObject(manifest.suites) ? manifest.suites : {};
  const packages = isObject(manifest.packages) ? manifest.packages : {};

  for (const [name, packagePath] of Object.entries(packages)) {
    validateName(name, `package ${JSON.stringify(name)}`, errors);
    if (typeof packagePath !== "string" || packagePath.length === 0 || path.isAbsolute(packagePath)) {
      errors.push(`package ${JSON.stringify(name)} path must be relative`);
      continue;
    }
    const resolved = path.resolve(root, packagePath);
    if (!isContained(root, resolved)) {
      errors.push(`package ${JSON.stringify(name)} path escapes repository root`);
      continue;
    }
    const metadata = readPackage(root, packagePath);
    if (metadata.__error) {
      errors.push(metadata.__error);
      continue;
    }
    if (name !== "root" && !isObject(metadata.scripts)) {
      errors.push(`package ${JSON.stringify(name)} must define scripts`);
    }
    packageRecords[name] = { path: packagePath, metadata };
  }

  for (const [name, suite] of Object.entries(suites)) {
    validateName(name, `suite ${JSON.stringify(name)}`, errors);
    if (!isObject(suite)) {
      errors.push(`suite ${JSON.stringify(name)} must be an object`);
      continue;
    }
    if (typeof suite.description !== "string" || suite.description.length === 0) {
      errors.push(`suite ${JSON.stringify(name)} must have a description`);
    }
    if (!Array.isArray(suite.steps) || suite.steps.length === 0) {
      errors.push(`suite ${JSON.stringify(name)} must have non-empty steps`);
      continue;
    }
    suite.steps.forEach((step, index) => {
      const label = `suite ${JSON.stringify(name)} step ${index + 1}`;
      if (!isObject(step)) {
        errors.push(`${label} must be an object`);
        return;
      }
      const keys = Object.keys(step);
      if (keys.length === 1 && Object.hasOwn(step, "suite")) {
        if (typeof step.suite !== "string" || !Object.hasOwn(suites, step.suite)) {
          errors.push(`${label} references an unknown suite`);
        }
        return;
      }
      if (keys.length !== 2 || !Object.hasOwn(step, "package") || !Object.hasOwn(step, "script")) {
        errors.push(`${label} must contain exactly package and script, or suite`);
        return;
      }
      if (typeof step.package !== "string" || !Object.hasOwn(packageRecords, step.package)) {
        errors.push(`${label} references an unknown package`);
        return;
      }
      if (typeof step.script !== "string" || !NAME_PATTERN.test(step.script)) {
        errors.push(`${label}.script must be a valid script name`);
        return;
      }
      if (step.package === "root") {
        if (!Object.hasOwn(commands, step.script)) {
          errors.push(`${label} references missing command ${JSON.stringify(step.script)}`);
        }
        return;
      }
      const scripts = isObject(packageRecords[step.package].metadata.scripts)
        ? packageRecords[step.package].metadata.scripts
        : {};
      if (!Object.hasOwn(scripts, step.script)) {
        errors.push(`${label} references missing script ${JSON.stringify(step.script)}`);
      }
    });
  }

  const visiting = new Set();
  const visited = new Set();
  function detectCycles(name, trail) {
    if (visiting.has(name)) {
      errors.push(`suite cycle: ${[...trail, name].join(" -> ")}`);
      return;
    }
    if (visited.has(name) || !Object.hasOwn(suites, name)) return;
    visiting.add(name);
    const steps = Array.isArray(suites[name]?.steps) ? suites[name].steps : [];
    for (const step of steps) {
      if (isObject(step) && Object.hasOwn(step, "suite")) {
        detectCycles(step.suite, [...trail, name]);
      }
    }
    visiting.delete(name);
    visited.add(name);
  }
  for (const name of Object.keys(suites)) detectCycles(name, []);

  // A suite is an ordered execution plan. Repeating a leaf in that plan is
  // almost always an accidental duplicate, even when it comes through a
  // nested suite. Reject it before any child process can run.
  function collectLeaves(name, visiting, leaves) {
    if (visiting.has(name) || !Object.hasOwn(suites, name)) return;
    visiting.add(name);
    const steps = Array.isArray(suites[name]?.steps) ? suites[name].steps : [];
    for (const step of steps) {
      if (isObject(step) && Object.hasOwn(step, "suite")) {
        if (typeof step.suite === "string") collectLeaves(step.suite, visiting, leaves);
      } else if (isObject(step) && typeof step.package === "string" && typeof step.script === "string") {
        leaves.push(`${step.package}/${step.script}`);
      }
    }
    visiting.delete(name);
  }
  for (const name of Object.keys(suites)) {
    const leaves = [];
    collectLeaves(name, new Set(), leaves);
    const seen = new Set();
    const reported = new Set();
    for (const leaf of leaves) {
      if (seen.has(leaf) && !reported.has(leaf)) {
        errors.push(`suite ${JSON.stringify(name)} expands to duplicate leaf ${JSON.stringify(leaf)}`);
        reported.add(leaf);
      }
      seen.add(leaf);
    }
  }

  return { errors, packages: packageRecords, commands, registry: commandRegistry, suites };
}

export function flattenCheckSuite({ suites, suiteName } = {}) {
  if (!isObject(suites) || typeof suiteName !== "string" || !Object.hasOwn(suites, suiteName)) {
    throw new Error(`unknown suite ${JSON.stringify(suiteName)}`);
  }
  const steps = [];
  const visiting = new Set();
  function visit(name) {
    if (visiting.has(name)) throw new Error(`suite cycle at ${name}`);
    visiting.add(name);
    for (const step of suites[name].steps) {
      if (Object.hasOwn(step, "suite")) visit(step.suite);
      else steps.push({ package: step.package, script: step.script });
    }
    visiting.delete(name);
  }
  visit(suiteName);
  return steps;
}

export function loadCheckSuites(root = repositoryRoot) {
  const manifestPath = path.join(root, "tooling", "check-suites.json");
  const loadedManifest = readJson(manifestPath);
  if (loadedManifest.error) throw new Error(`cannot load ${manifestPath}: ${loadedManifest.error}`);
  const manifest = loadedManifest.value;
  const loadedRegistry = loadCommandRegistry(root);
  const validation = validateCheckSuites({ manifest, registry: loadedRegistry.registry, root });
  if (validation.errors.length > 0) {
    throw new Error(validation.errors.join("\n"));
  }
  return { manifest, ...validation, registry: loadedRegistry.registry, commands: loadedRegistry.commands };
}

export function parseCheckSuiteArguments(argv) {
  const options = { check: false, list: false, dryRun: false, suite: null };
  let checkCount = 0;
  let listCount = 0;
  let dryRunCount = 0;
  let suiteCount = 0;
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--check") {
      checkCount += 1;
      options.check = true;
    } else if (argument === "--list") {
      listCount += 1;
      options.list = true;
    } else if (argument === "--dry-run") {
      dryRunCount += 1;
      options.dryRun = true;
    }
    else if (argument === "--suite") {
      suiteCount += 1;
      index += 1;
      const suite = argv[index] ?? null;
      if (typeof suite !== "string" || suite.length === 0 || suite.startsWith("--")) {
        throw new Error("--suite requires a suite name");
      }
      options.suite = suite;
    } else throw new Error(`unknown argument ${argument}`);
  }
  if (checkCount > 1 || listCount > 1 || dryRunCount > 1 || suiteCount > 1) {
    throw new Error("each command-line option may be used only once");
  }
  const hasSuite = suiteCount === 1;
  if (!options.check && !options.list && !hasSuite) {
    throw new Error("choose exactly one action: --check, --list, --suite <name>, or --dry-run --suite <name>");
  }
  if (options.check && (options.list || options.dryRun || hasSuite)) {
    throw new Error("--check cannot be combined with another action");
  }
  if (options.list && (options.check || options.dryRun || hasSuite)) {
    throw new Error("--list cannot be combined with another action");
  }
  if (options.dryRun && !hasSuite) {
    throw new Error("--dry-run requires --suite <name>");
  }
  return options;
}

export function flattenCommand({ commands, commandName } = {}) {
  if (!isObject(commands) || typeof commandName !== "string" || !Object.hasOwn(commands, commandName)) {
    throw new Error(`unknown command ${JSON.stringify(commandName)}`);
  }
  const steps = [];
  const visiting = new Set();
  function visit(name) {
    if (visiting.has(name)) throw new Error(`command cycle at ${name}`);
    const command = commands[name];
    if (!isObject(command) || !Array.isArray(command.steps)) {
      throw new Error(`command ${JSON.stringify(name)} is malformed`);
    }
    visiting.add(name);
    for (const step of command.steps) {
      if (step.kind === "command") visit(step.command);
      else steps.push({ command: name, cwd: step.cwd, args: step.args });
    }
    visiting.delete(name);
  }
  visit(commandName);
  return steps;
}

function childStatus(result) {
  if (result?.error) return null;
  if (Number.isInteger(result?.status)) return result.status;
  if (Number.isInteger(result?.exitCode)) return result.exitCode;
  return 1;
}

function runInvocation({ root, invocation, spawn = spawnSync }) {
  let result;
  try {
    result = spawn(process.execPath, invocation.args, {
      cwd: path.resolve(root, invocation.cwd),
      stdio: "inherit",
      windowsHide: true,
      shell: false,
    });
  } catch (error) {
    process.stderr.write(`command-runner: child process failed: ${error.message}\n`);
    return 1;
  }
  if (result?.error) {
    process.stderr.write(`command-runner: child process failed: ${result.error.message}\n`);
    return 1;
  }
  return childStatus(result);
}

export function runCommand({
  root = repositoryRoot,
  commandRecords,
  commandName,
  dryRun = false,
  spawn = spawnSync,
  log = true,
  prefix = "command-runner",
  forwardArgs = [],
} = {}) {
  let invocations;
  try {
    invocations = flattenCommand({ commands: commandRecords, commandName });
  } catch (error) {
    process.stderr.write(`${prefix}: ${error.message}\n`);
    return 2;
  }
  if (!Array.isArray(forwardArgs) || forwardArgs.some((argument) => typeof argument !== "string")) {
    process.stderr.write(`${prefix}: forwarded arguments must be an array of strings\n`);
    return 2;
  }
  if (forwardArgs.length > 0) {
    const last = invocations.at(-1);
    last.args = [...last.args, ...forwardArgs];
  }
  for (const [index, invocation] of invocations.entries()) {
    const label = `${prefix}: ${commandName} ${index + 1}/${invocations.length}`;
    if (dryRun) {
      process.stdout.write(`${label} bun ${invocation.args.join(" ")} (dry-run)\n`);
      continue;
    }
    if (log) process.stderr.write(`${label} start\n`);
    const startedAt = process.hrtime.bigint();
    const status = runInvocation({ root, invocation, spawn });
    const durationMs = Number(process.hrtime.bigint() - startedAt) / 1_000_000;
    if (status !== 0) {
      if (log) process.stderr.write(`${label} failed after ${durationMs.toFixed(1)} ms: exit ${status}\n`);
      return status;
    }
    if (log) process.stderr.write(`${label} ok after ${durationMs.toFixed(1)} ms\n`);
  }
  return 0;
}

export function runCheckSuite({ root, packageRecords, commandRecords = {}, suites, suiteName, dryRun, spawn = spawnSync }) {
  const steps = flattenCheckSuite({ suites, suiteName });
  for (const [index, step] of steps.entries()) {
    const packageRecord = packageRecords[step.package];
    const cwd = path.resolve(root, packageRecord.path);
    const label = `${index + 1}/${steps.length} ${step.package}/${step.script}`;
    if (dryRun) {
      process.stdout.write(`check-suite: ${label} (dry-run)\n`);
      continue;
    }
    process.stderr.write(`check-suite: ${label} start\n`);
    const startedAt = process.hrtime.bigint();
    const status = step.package === "root"
      ? runCommand({
        root,
        commandRecords,
        commandName: step.script,
        spawn,
        log: false,
        prefix: "check-suite",
      })
      : childStatus(spawn(process.execPath, ["run", step.script], {
        cwd,
        stdio: "inherit",
        windowsHide: true,
        shell: false,
      }));
    const durationMs = Number(process.hrtime.bigint() - startedAt) / 1_000_000;
    const duration = `${durationMs.toFixed(1)} ms`;
    if (status !== 0) {
      process.stderr.write(`check-suite: ${label} failed after ${duration}: exit ${status}\n`);
      return status;
    }
    process.stderr.write(`check-suite: ${label} ok after ${duration}\n`);
  }
  return 0;
}

export function main(argv = process.argv.slice(2)) {
  let options;
  try {
    options = parseCheckSuiteArguments(argv);
  } catch (error) {
    process.stderr.write(`check-suite: ${error.message}\n`);
    return 2;
  }
  let loaded;
  try {
    loaded = loadCheckSuites();
  } catch (error) {
    process.stderr.write(`check-suite: ${error.message}\n`);
    return 2;
  }
  const suiteNames = Object.keys(loaded.suites);
  if (options.check) {
    process.stdout.write(
      `check-suites: ok (${suiteNames.length} suites, ` +
      `${suiteNames.reduce((total, name) => total + flattenCheckSuite({ suites: loaded.suites, suiteName: name }).length, 0)} ` +
      `expanded steps)\n`,
    );
    return 0;
  }
  if (options.list) {
    for (const name of suiteNames) {
      const count = flattenCheckSuite({ suites: loaded.suites, suiteName: name }).length;
      process.stdout.write(`${name}\t${count}\t${loaded.suites[name].description}\n`);
    }
    return 0;
  }
  if (options.suite === null) {
    process.stderr.write("check-suite: use --check, --list, or --suite <name>\n");
    return 2;
  }
  if (!Object.hasOwn(loaded.suites, options.suite)) {
    process.stderr.write(`check-suite: unknown suite ${JSON.stringify(options.suite)}\n`);
    return 2;
  }
  return runCheckSuite({
    root: repositoryRoot,
    packageRecords: loaded.packages,
    commandRecords: loaded.commands,
    suites: loaded.suites,
    suiteName: options.suite,
    dryRun: options.dryRun,
  });
}

if (import.meta.main) process.exit(main());
