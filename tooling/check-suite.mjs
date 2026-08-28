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
export const CHECK_SUITE_SCHEMA = "w-check-suites-1";

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

function validateName(value, label, errors) {
  if (typeof value !== "string" || !NAME_PATTERN.test(value)) {
    errors.push(`${label} must be a non-empty package or script name`);
  }
}

/**
 * Validate a check-suite manifest without running any child process.
 *
 * The manifest is intentionally small. A leaf step names a package and one
 * script from that package. A suite step names another suite. This keeps the
 * package scripts as stable user-facing aliases while moving aggregate order
 * into one inspectable projection.
 */
export function validateCheckSuites({ manifest, root = repositoryRoot } = {}) {
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
    if (!isObject(metadata.scripts)) {
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

  return { errors, packages: packageRecords, suites };
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
  const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
  const validation = validateCheckSuites({ manifest, root });
  if (validation.errors.length > 0) {
    throw new Error(validation.errors.join("\n"));
  }
  return { manifest, ...validation };
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

export function runCheckSuite({ root, packageRecords, suites, suiteName, dryRun }) {
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
    const result = spawnSync(process.execPath, ["run", step.script], {
      cwd,
      stdio: "inherit",
      windowsHide: true,
    });
    const durationMs = Number(process.hrtime.bigint() - startedAt) / 1_000_000;
    const duration = `${durationMs.toFixed(1)} ms`;
    if (result.error || result.status !== 0) {
      const status = result.error ? result.error.message : `exit ${result.status ?? "unknown"}`;
      process.stderr.write(
        `check-suite: ${label} failed after ${duration}: ${status}\n`,
      );
      return result.status === null ? 1 : (result.status ?? 1);
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
    suites: loaded.suites,
    suiteName: options.suite,
    dryRun: options.dryRun,
  });
}

if (import.meta.main) process.exit(main());
