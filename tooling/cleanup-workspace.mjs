#!/usr/bin/env bun

import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import {
  lstat,
  readFile,
  readdir,
  realpath,
  rm,
} from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

export const DEFAULT_TEMP_MIN_AGE_MS = 24 * 60 * 60 * 1000;

export const WORKSPACE_OUTPUTS = Object.freeze([
  { relativePath: "build", marker: "cmake" },
  { relativePath: "reference/last-light/build", marker: "known-output" },
  { relativePath: "portal/dist", marker: "known-output" },
  { relativePath: "tooling/tree-sitter-w/build", marker: "known-output" },
  { relativePath: "tooling/vscode-w/dist", marker: "known-output" },
]);

export const TEMP_DIRECTORY_PREFIXES = Object.freeze([
  "w-acquisition-gate-",
  "w-bmd1-runner-test-",
  "w-bmd1-seed-",
  "w-bmd1-smoke-",
  "w-bmd2-runner-test-",
  "w-bmd2-smoke-",
  "w-bmd3-byte-scan-",
  "w-check-cli-",
  "w-check-cli-outside-",
  "w-check-suite-",
  "w-cheatsheet-",
  "w-cleanup-workspace-test-",
  "w-formatter-cases-",
  "w-freeze-classification-",
  "w-frontend-freeze-",
  "w-fz0-mutation-",
  "w-hir0-",
  "w-hlo0-",
  "w-hlo0-cases-",
  "w-hlo1-artifact-",
  "w-hlo1-seed-",
  "w-mlir0-artifact-",
  "w-mlir0-seed-",
  "w-operator-surface-",
  "w-own0-",
  "w-owner-guard-",
  "w-run0-gate-",
  "w-seed-check-driver-",
  "w-seed-constir-check-",
  "w-seed-diagnostic-",
  "w-seed-ephemeral-driver-",
  "w-seed-ephemeral-driver-asan-",
  "w-seed-ephemeral-graph-",
  "w-seed-ephemeral-provider-",
  "w-seed-foreign-",
  "w-seed-formatter-",
  "w-seed-frontend-check-",
  "w-seed-generic-validation-check-",
  "w-seed-lexer-",
  "w-seed-module-scan-",
  "w-seed-parser-",
  "w-seed-source-reader-",
  "w-semantic-cases-",
  "w-source-refs-",
  "w-study-registry-",
  "w-syn1-parse-",
  "w-wire-reference-",
  "wmeta-reader-",
]);

const ROOT_BUILD_PREFIX = "build-";
const RANDOM_SUFFIX = /^[A-Za-z0-9]{6}$/;
const BMD2_BUILD = /^w-bmd2-[0-9a-f]{8}-build-([A-Za-z0-9]{6})$/;
const BMD2_ARCHIVE = /^w-bmd2-(?:baseline|candidate)-archive-([A-Za-z0-9]{6})$/;
const PROTECTED_ENTRY_NAMES = new Set([".codex", ".git", "history", "node_modules", "sessions"]);
const PROTECTED_WORKSPACE_PATHS = Object.freeze([
  ".git",
  ".codex",
  "benchmarks/results",
  "history",
  "node_modules",
]);

const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

function pathKey(value) {
  const normalized = path.resolve(value);
  return process.platform === "win32" ? normalized.toLowerCase() : normalized;
}

function samePath(left, right) {
  return pathKey(left) === pathKey(right);
}

function isWithin(root, target, allowRoot = false) {
  const relative = path.relative(path.resolve(root), path.resolve(target));
  if (relative === "") return allowRoot;
  return relative !== ".." && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative);
}

function pathsIntersect(left, right) {
  return samePath(left, right) || isWithin(left, right) || isWithin(right, left);
}

function mountIntersectsCandidate(candidate, mountPoint) {
  return samePath(candidate, mountPoint) || isWithin(candidate, mountPoint);
}

function comparePaths(left, right) {
  const leftKey = pathKey(left.path);
  const rightKey = pathKey(right.path);
  return leftKey < rightKey ? -1 : leftKey > rightKey ? 1 : 0;
}

function toPosix(value) {
  return value.split(path.sep).join("/");
}

function matchesTempDirectoryName(name) {
  if (BMD2_BUILD.test(name) || BMD2_ARCHIVE.test(name)) return true;
  return TEMP_DIRECTORY_PREFIXES.some((prefix) => {
    if (!name.startsWith(prefix)) return false;
    return RANDOM_SUFFIX.test(name.slice(prefix.length));
  });
}

function validateRootPair(workspaceRoot, tempRoot) {
  if (samePath(workspaceRoot, tempRoot) || pathsIntersect(workspaceRoot, tempRoot)) {
    throw new Error("The workspace and temporary roots must be disjoint.");
  }
}

function readGitRoot(workspaceRoot) {
  const result = spawnSync("git", ["-C", workspaceRoot, "rev-parse", "--show-toplevel"], {
    encoding: "utf8",
    windowsHide: true,
  });
  if (result.error) throw new Error(`Cannot verify the Git root: ${result.error.message}`);
  if (result.status !== 0) throw new Error("The workspace is not a Git worktree root.");
  return path.resolve(result.stdout.trim());
}

function normalizeKeepPaths(values, workspaceRoot, tempRoot) {
  return values.map((value) => {
    if (typeof value !== "string" || value.length === 0) {
      throw new Error("--keep requires a non-empty path.");
    }
    const resolved = path.resolve(path.isAbsolute(value) ? value : path.join(workspaceRoot, value));
    if (!isWithin(workspaceRoot, resolved, true) && !isWithin(tempRoot, resolved, true)) {
      throw new Error(`Keep path is outside the cleanup roots: ${value}`);
    }
    return resolved;
  });
}

function hasProtectedWorkspaceIntersection(candidate, workspaceRoot) {
  return PROTECTED_WORKSPACE_PATHS.some((relativePath) =>
    pathsIntersect(candidate, path.join(workspaceRoot, relativePath)));
}

function hasProtectedNestedPath(relativePath) {
  const parts = relativePath.split(path.sep).filter(Boolean);
  if (parts.some((part) => PROTECTED_ENTRY_NAMES.has(part))) return true;
  return parts.some((part, index) => part === "benchmarks" && parts[index + 1] === "results");
}

async function pathExists(target) {
  try {
    await lstat(target);
    return true;
  } catch (error) {
    if (error?.code === "ENOENT") return false;
    throw error;
  }
}

async function hasCMakeMarker(target) {
  try {
    const marker = await lstat(path.join(target, "CMakeCache.txt"));
    return marker.isFile() && !marker.isSymbolicLink();
  } catch (error) {
    if (error?.code === "ENOENT") return false;
    throw error;
  }
}

async function workspaceTargets(workspaceRoot) {
  const targets = [];
  for (const rule of WORKSPACE_OUTPUTS) {
    const target = path.resolve(workspaceRoot, rule.relativePath);
    if (await pathExists(target)) targets.push({ path: target, scope: "workspace", rule });
  }

  const entries = await readdir(workspaceRoot, { withFileTypes: true });
  for (const entry of entries) {
    if (!entry.name.startsWith(ROOT_BUILD_PREFIX) || entry.name === ROOT_BUILD_PREFIX) continue;
    targets.push({
      path: path.join(workspaceRoot, entry.name),
      scope: "workspace",
      rule: { relativePath: entry.name, marker: "cmake" },
    });
  }
  return targets;
}

async function tempTargets(tempRoot) {
  const targets = [];
  const entries = await readdir(tempRoot, { withFileTypes: true });
  for (const entry of entries) {
    if (!matchesTempDirectoryName(entry.name)) continue;
    targets.push({ path: path.join(tempRoot, entry.name), scope: "temp", rule: null });
  }
  return targets;
}

function trackedPathIntersects(candidate, workspaceRoot, trackedPaths) {
  const candidateRelative = toPosix(path.relative(workspaceRoot, candidate));
  return trackedPaths.some((trackedPath) =>
    trackedPath === candidateRelative || trackedPath.startsWith(`${candidateRelative}/`));
}

function fingerprintPart(stats, relativePath) {
  const type = stats.isDirectory() ? "d" : stats.isFile() ? "f" : "o";
  return [relativePath, type, stats.dev, stats.ino, stats.size, stats.mtimeMs].join("\0");
}

async function scanCandidate(candidate, root) {
  const rootStats = await lstat(candidate);
  if (!rootStats.isDirectory() || rootStats.isSymbolicLink()) {
    throw new Error("target is not a physical directory");
  }
  const physical = await realpath(candidate);
  if (!samePath(physical, candidate) || !isWithin(root, physical)) {
    throw new Error("target resolves outside its cleanup root");
  }

  const expectedDevice = rootStats.dev;
  const digest = createHash("sha256");
  let bytes = 0n;
  let newestMtimeMs = rootStats.mtimeMs;

  async function visit(directory, relativeDirectory) {
    const entries = await readdir(directory, { withFileTypes: true });
    entries.sort((left, right) => left.name < right.name ? -1 : left.name > right.name ? 1 : 0);
    for (const entry of entries) {
      const entryPath = path.join(directory, entry.name);
      const relativePath = path.join(relativeDirectory, entry.name);
      if (hasProtectedNestedPath(relativePath)) {
        throw new Error(`protected path exists: ${relativePath}`);
      }
      const stats = await lstat(entryPath);
      if (stats.isSymbolicLink()) throw new Error(`link or reparse point exists: ${relativePath}`);
      if (stats.dev !== expectedDevice) throw new Error(`mounted filesystem exists: ${relativePath}`);
      newestMtimeMs = Math.max(newestMtimeMs, stats.mtimeMs);
      digest.update(fingerprintPart(stats, toPosix(relativePath)));
      digest.update("\0");
      if (stats.isDirectory()) await visit(entryPath, relativePath);
      else if (stats.isFile()) bytes += BigInt(stats.size);
    }
  }

  digest.update(fingerprintPart(rootStats, "."));
  digest.update("\0");
  await visit(candidate, "");
  return {
    bytes,
    device: rootStats.dev,
    inode: rootStats.ino,
    newestMtimeMs,
    fingerprint: digest.digest("hex"),
  };
}

function decodeMountInfoPath(value) {
  if (/\\(?![0-7]{3})/.test(value)) {
    throw new Error("Linux mountinfo contains an invalid path escape.");
  }
  return value.replace(/\\([0-7]{3})/g, (_match, octal) =>
    String.fromCharCode(Number.parseInt(octal, 8)));
}

export function parseLinuxMountInfo(source) {
  if (typeof source !== "string") throw new Error("Linux mountinfo must be text.");
  const mountPoints = [];
  for (const line of source.split("\n")) {
    if (line === "") continue;
    const separator = line.indexOf(" - ");
    const fields = separator < 0 ? [] : line.slice(0, separator).split(" ");
    if (fields.length < 6) throw new Error("Linux mountinfo contains a malformed record.");
    const mountPoint = decodeMountInfoPath(fields[4]);
    if (!path.posix.isAbsolute(mountPoint) || mountPoint.includes("\0")) {
      throw new Error("Linux mountinfo contains an invalid mountpoint.");
    }
    mountPoints.push(path.posix.normalize(mountPoint));
  }
  return [...new Set(mountPoints)].sort();
}

export async function getMountProof({
  platform = process.platform,
  readMountInfo = () => readFile("/proc/self/mountinfo", "utf8"),
} = {}) {
  if (platform === "win32") {
    return { supported: true, platform, mountPoints: [] };
  }
  if (platform !== "linux") {
    return { supported: false, platform, reason: "platform has no supported mount proof" };
  }
  try {
    return {
      supported: true,
      platform,
      mountPoints: parseLinuxMountInfo(await readMountInfo()),
    };
  } catch (error) {
    return { supported: false, platform, reason: error.message };
  }
}

function classifyKeep(candidate, keepPaths) {
  return keepPaths.find((keepPath) => pathsIntersect(candidate, keepPath));
}

export async function listTrackedPaths(workspaceRoot) {
  const result = spawnSync("git", ["-C", workspaceRoot, "ls-files", "-z", "--full-name"], {
    encoding: null,
    windowsHide: true,
  });
  if (result.error) throw new Error(`Cannot read tracked paths: ${result.error.message}`);
  if (result.status !== 0) throw new Error("Cannot read tracked paths from Git.");
  return result.stdout.toString("utf8").split("\0").filter(Boolean);
}

export async function collectCleanupPlan({
  workspaceRoot = repositoryRoot,
  tempRoot = os.tmpdir(),
  workspace = true,
  temp = false,
  keep = [],
  nowMs = Date.now(),
  tempMinAgeMs = DEFAULT_TEMP_MIN_AGE_MS,
  trackedPaths,
  gitRoot,
} = {}) {
  const workspacePhysical = await realpath(path.resolve(workspaceRoot));
  const tempPhysical = await realpath(path.resolve(tempRoot));
  validateRootPair(workspacePhysical, tempPhysical);
  const verifiedGitRoot = gitRoot === undefined ? readGitRoot(workspacePhysical) : path.resolve(gitRoot);
  if (!samePath(verifiedGitRoot, workspacePhysical)) {
    throw new Error("The workspace must be the verified Git worktree root.");
  }
  if (!Number.isFinite(nowMs) || !Number.isFinite(tempMinAgeMs) ||
      tempMinAgeMs < DEFAULT_TEMP_MIN_AGE_MS) {
    throw new Error("The temporary age threshold must be at least 24 hours.");
  }
  const keepPaths = normalizeKeepPaths(keep, workspacePhysical, tempPhysical);
  const tracked = trackedPaths ?? await listTrackedPaths(workspacePhysical);
  const selected = [];
  if (workspace) selected.push(...await workspaceTargets(workspacePhysical));
  if (temp) selected.push(...await tempTargets(tempPhysical));

  const plan = {
    workspaceRoot: workspacePhysical,
    tempRoot: tempPhysical,
    tempMinAgeMs,
    nowMs,
    scopes: { workspace: Boolean(workspace), temp: Boolean(temp) },
    candidates: [],
    retained: [],
    refused: [],
    bytes: 0n,
  };

  const seen = new Set();
  for (const target of selected.sort(comparePaths)) {
    const key = pathKey(target.path);
    if (seen.has(key)) continue;
    seen.add(key);
    const root = target.scope === "workspace" ? workspacePhysical : tempPhysical;
    if (!isWithin(root, target.path) ||
        (target.scope === "workspace" && hasProtectedWorkspaceIntersection(target.path, workspacePhysical))) {
      plan.refused.push({ ...target, reason: "target is outside the closed cleanup allowlist" });
      continue;
    }
    if (target.scope === "workspace" && trackedPathIntersects(target.path, workspacePhysical, tracked)) {
      plan.refused.push({ ...target, reason: "target contains a tracked path" });
      continue;
    }
    try {
      const scan = await scanCandidate(target.path, root);
      if (target.rule?.marker === "cmake" && !await hasCMakeMarker(target.path)) {
        plan.refused.push({ ...target, reason: "target has no CMakeCache.txt output marker" });
        continue;
      }
      if (target.scope === "temp" && nowMs - scan.newestMtimeMs < tempMinAgeMs) {
        const hours = Math.floor(tempMinAgeMs / (60 * 60 * 1000));
        plan.retained.push({ ...target, reason: `temporary directory is newer than ${hours} hours` });
        continue;
      }
      const keptBy = classifyKeep(target.path, keepPaths);
      if (keptBy) {
        plan.retained.push({ ...target, reason: `kept by ${keptBy}` });
        continue;
      }
      const candidate = { ...target, ...scan };
      plan.candidates.push(candidate);
      plan.bytes += scan.bytes;
    } catch (error) {
      plan.refused.push({ ...target, reason: error.message });
    }
  }
  return plan;
}

function scanMatches(candidate, scan) {
  return candidate.device === scan.device && candidate.inode === scan.inode &&
    candidate.bytes === scan.bytes && candidate.newestMtimeMs === scan.newestMtimeMs &&
    candidate.fingerprint === scan.fingerprint;
}

export async function applyCleanupPlan(plan, { mountProof } = {}) {
  const report = { removed: [], refused: [], error: null };

  if (plan.scopes?.temp || plan.candidates.some((candidate) => candidate.scope === "temp")) {
    report.error = "legacy temporary cleanup is dry-run only";
    for (const candidate of plan.candidates) {
      report.refused.push({ ...candidate, reason: "legacy temporary cleanup is dry-run only" });
    }
    return report;
  }

  const proof = mountProof ?? await getMountProof();
  if (!proof?.supported) {
    report.error = `mount absence cannot be proved: ${proof?.reason ?? "unsupported platform"}`;
    for (const candidate of plan.candidates) {
      report.refused.push({
        ...candidate,
        reason: report.error,
      });
    }
    return report;
  }
  if (!Array.isArray(proof.mountPoints) || proof.mountPoints.some((item) =>
    typeof item !== "string" || !path.isAbsolute(item))) {
    report.error = "mount proof is malformed";
    for (const candidate of plan.candidates) {
      report.refused.push({ ...candidate, reason: report.error });
    }
    return report;
  }

  const batchFailures = new Map();
  for (const candidate of plan.candidates) {
    const root = candidate.scope === "workspace" ? plan.workspaceRoot : plan.tempRoot;
    try {
      if (!isWithin(root, candidate.path)) throw new Error("target left its cleanup root");
      const mountPoint = proof.mountPoints.find((item) =>
        mountIntersectsCandidate(candidate.path, item));
      if (mountPoint !== undefined) throw new Error(`mountpoint exists: ${mountPoint}`);
      const current = await scanCandidate(candidate.path, root);
      if (!scanMatches(candidate, current)) throw new Error("target changed after the dry-run scan");
    } catch (error) {
      batchFailures.set(pathKey(candidate.path), error.message);
    }
  }
  if (batchFailures.size !== 0) {
    for (const candidate of plan.candidates) {
      report.refused.push({
        ...candidate,
        reason: batchFailures.get(pathKey(candidate.path)) ??
          "batch preflight failed before removal",
      });
    }
    return report;
  }

  for (let index = 0; index < plan.candidates.length; index += 1) {
    const candidate = plan.candidates[index];
    const root = candidate.scope === "workspace" ? plan.workspaceRoot : plan.tempRoot;
    try {
      if (!isWithin(root, candidate.path)) throw new Error("target left its cleanup root");
      const current = await scanCandidate(candidate.path, root);
      if (!scanMatches(candidate, current)) throw new Error("target changed after batch preflight");
      await rm(candidate.path, { recursive: true, force: false, maxRetries: 0 });
      report.removed.push(candidate);
    } catch (error) {
      report.refused.push({ ...candidate, reason: error.message });
      for (const remaining of plan.candidates.slice(index + 1)) {
        report.refused.push({ ...remaining, reason: "batch stopped after a removal failure" });
      }
      break;
    }
  }
  return report;
}

export function parseCleanupArguments(argv) {
  const options = {
    apply: false,
    workspace: false,
    temp: false,
    legacyTemp: false,
    keep: [],
  };
  let scopeSeen = false;
  let mode;
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--apply" || argument === "--dry-run") {
      const nextMode = argument === "--apply" ? "apply" : "dry-run";
      if (mode !== undefined && mode !== nextMode) {
        throw new Error("--apply and --dry-run cannot be combined.");
      }
      mode = nextMode;
      options.apply = nextMode === "apply";
    } else if (argument === "--workspace") {
      options.workspace = true;
      scopeSeen = true;
    } else if (argument === "--temp") {
      options.temp = true;
      scopeSeen = true;
    } else if (argument === "--legacy-temp") {
      options.legacyTemp = true;
    } else if (argument === "--temp-age-hours") {
      index += 1;
      const hours = Number(argv[index]);
      if (!Number.isSafeInteger(hours) || hours < 24 ||
          !Number.isSafeInteger(hours * 60 * 60 * 1000)) {
        throw new Error("--temp-age-hours requires an integer of at least 24.");
      }
      options.tempMinAgeMs = hours * 60 * 60 * 1000;
    } else if (argument === "--keep") {
      index += 1;
      if (index >= argv.length || argv[index].startsWith("--")) {
        throw new Error("--keep requires a path.");
      }
      options.keep.push(argv[index]);
    } else if (argument === "--help" || argument === "-h") options.help = true;
    else throw new Error(`Unknown argument: ${argument}`);
  }
  if (!scopeSeen) {
    options.workspace = true;
    options.temp = false;
  }
  if (options.temp && !options.legacyTemp) {
    throw new Error("--temp requires the explicit --legacy-temp opt-in.");
  }
  if (options.legacyTemp && !options.temp) {
    throw new Error("--legacy-temp is valid only with --temp.");
  }
  if (options.tempMinAgeMs !== undefined && !options.temp) {
    throw new Error("--temp-age-hours is valid only with --temp.");
  }
  if (options.apply && options.temp) {
    throw new Error("Legacy temporary cleanup is dry-run only.");
  }
  return options;
}

function formatBytes(bytes) {
  return `${bytes.toString()} bytes`;
}

function printUsage() {
  console.log("Usage: bun tooling/cleanup-workspace.mjs [--workspace] [--temp --legacy-temp] [--temp-age-hours N] [--keep <path>]... [--apply]");
  console.log("The command is a dry-run unless --apply is present. Legacy temporary candidates must be at least 24 hours old.");
}

export function formatCleanupReport(plan, applied) {
  const lines = [
    `cleanup mode=${applied ? "apply" : "dry-run"} candidates=${plan.candidates.length} bytes=${plan.bytes}`,
  ];
  for (const candidate of plan.candidates) {
    lines.push(`${applied ? "planned" : "would-remove"} ${formatBytes(candidate.bytes)} ${candidate.path}`);
  }
  for (const retained of plan.retained) lines.push(`retained ${retained.path} (${retained.reason})`);
  for (const refused of plan.refused) lines.push(`refused ${refused.path} (${refused.reason})`);
  return lines;
}

export async function main(argv = process.argv.slice(2)) {
  const options = parseCleanupArguments(argv);
  if (options.help) {
    printUsage();
    return 0;
  }
  const plan = await collectCleanupPlan(options);
  for (const line of formatCleanupReport(plan, options.apply)) console.log(line);
  if (!options.apply) {
    console.log(`removed=0 removed-bytes=0 retained=${plan.retained.length} refused=${plan.refused.length}`);
    return 0;
  }
  const result = await applyCleanupPlan(plan);
  const removedBytes = result.removed.reduce((sum, item) => sum + item.bytes, 0n);
  for (const removed of result.removed) {
    console.log(`removed ${formatBytes(removed.bytes)} ${removed.path}`);
  }
  for (const refused of result.refused) {
    console.log(`refused ${refused.path} (${refused.reason})`);
  }
  if (result.error !== null) console.log(`refused batch (${result.error})`);
  console.log(`removed=${result.removed.length} removed-bytes=${removedBytes} retained=${plan.retained.length} refused=${plan.refused.length + result.refused.length}`);
  return plan.refused.length === 0 && result.refused.length === 0 && result.error === null ? 0 : 1;
}

if (import.meta.main) {
  try {
    process.exitCode = await main();
  } catch (error) {
    console.error(`cleanup error: ${error.message}`);
    process.exitCode = 2;
  }
}
