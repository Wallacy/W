import { afterEach, beforeEach, describe, expect, setDefaultTimeout, test } from "bun:test";
import {
  mkdir,
  mkdtemp,
  readFile,
  rm,
  stat,
  symlink,
  utimes,
  writeFile,
} from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import {
  DEFAULT_TEMP_MIN_AGE_MS,
  applyCleanupPlan,
  collectCleanupPlan,
  formatCleanupReport,
  getMountProof,
  parseLinuxMountInfo,
  parseCleanupArguments,
} from "./cleanup-workspace.mjs";

const hour = 60 * 60 * 1000;
const noMounts = Object.freeze({ supported: true, platform: "test", mountPoints: [] });
setDefaultTimeout(30_000);
let sandbox;
let workspace;
let temporary;

async function exists(target) {
  try {
    await stat(target);
    return true;
  } catch (error) {
    if (error?.code === "ENOENT") return false;
    throw error;
  }
}

async function makeFile(target, contents = "x") {
  await mkdir(path.dirname(target), { recursive: true });
  await writeFile(target, contents);
}

async function makeCMakeBuild(relativePath, contents = "artifact") {
  const directory = path.join(workspace, relativePath);
  await makeFile(path.join(directory, "CMakeCache.txt"), "CMAKE_HOME_DIRECTORY=/source\n");
  await makeFile(path.join(directory, "artifact.bin"), contents);
  return directory;
}

async function makeOldTemporary(name, ageHours = 25) {
  const directory = path.join(temporary, name);
  const file = path.join(directory, "artifact.bin");
  await makeFile(file, "temporary artifact");
  const time = new Date(Date.now() - ageHours * hour);
  await utimes(file, time, time);
  await utimes(directory, time, time);
  return directory;
}

function options(overrides = {}) {
  return {
    workspaceRoot: workspace,
    tempRoot: temporary,
    workspace: true,
    temp: false,
    trackedPaths: [],
    gitRoot: workspace,
    ...overrides,
  };
}

beforeEach(async () => {
  sandbox = await mkdtemp(path.join(os.tmpdir(), "w-cleanup-workspace-test-"));
  workspace = path.join(sandbox, "workspace");
  temporary = path.join(sandbox, "temporary");
  await mkdir(workspace);
  await mkdir(temporary);
});

afterEach(async () => {
  if (sandbox && path.dirname(path.resolve(sandbox)) === path.resolve(os.tmpdir()) &&
      path.basename(sandbox).startsWith("w-cleanup-workspace-test-")) {
    await rm(sandbox, { recursive: true, force: true });
  }
});

describe("cleanup argument contract", () => {
  test("defaults to a workspace dry-run", () => {
    expect(parseCleanupArguments([])).toEqual({
      apply: false,
      workspace: true,
      temp: false,
      legacyTemp: false,
      keep: [],
    });
  });

  test("requires an explicit legacy opt-in for the temporary scope", () => {
    expect(() => parseCleanupArguments(["--temp"])).toThrow("--legacy-temp");
    expect(() => parseCleanupArguments(["--legacy-temp"])).toThrow("only with --temp");
    expect(parseCleanupArguments([
      "--workspace", "--temp", "--legacy-temp", "--keep", "build-own0",
    ])).toMatchObject({
      apply: false,
      workspace: true,
      temp: true,
      legacyTemp: true,
      keep: ["build-own0"],
    });
    expect(parseCleanupArguments(["--workspace", "--apply"])).toMatchObject({
      apply: true,
      workspace: true,
      temp: false,
    });
    expect(parseCleanupArguments([
      "--temp", "--legacy-temp", "--temp-age-hours", "48",
    ])).toMatchObject({
      temp: true,
      tempMinAgeMs: 48 * hour,
    });
    expect(() => parseCleanupArguments([
      "--temp", "--legacy-temp", "--temp-age-hours", "23",
    ])).toThrow("at least 24");
    expect(() => parseCleanupArguments(["--apply", "--dry-run"])).toThrow("cannot be combined");
    expect(() => parseCleanupArguments(["--keep", "--apply"])).toThrow("requires a path");
    expect(() => parseCleanupArguments([
      "--temp", "--legacy-temp", "--apply",
    ])).toThrow("dry-run only");
  });
});

describe("workspace cleanup", () => {
  test("selects only closed build outputs and keeps other ignored data", async () => {
    const build = await makeCMakeBuild("build");
    const sourceBuild = path.join(workspace, "build-source");
    await makeFile(path.join(sourceBuild, "main.w"), "package {}\n");
    const preserved = [
      "src/main.w",
      ".cache/cache.bin",
      "node_modules/package/index.js",
      "benchmarks/results/run.json",
      "generated/output.bin",
      "logs/check.log",
      "dumps/core.dump",
    ];
    for (const relativePath of preserved) await makeFile(path.join(workspace, relativePath));

    const plan = await collectCleanupPlan(options());
    expect(plan.candidates.map((candidate) => candidate.path)).toEqual([build]);
    expect(plan.refused).toEqual([
      expect.objectContaining({ path: sourceBuild, reason: "target has no CMakeCache.txt output marker" }),
    ]);

    const report = await applyCleanupPlan(plan, { mountProof: noMounts });
    expect(report.refused).toEqual([]);
    expect(await exists(build)).toBe(false);
    expect(await exists(sourceBuild)).toBe(true);
    for (const relativePath of preserved) {
      expect(await exists(path.join(workspace, relativePath))).toBe(true);
    }
  });

  test("refuses a candidate that contains a tracked file", async () => {
    const build = await makeCMakeBuild("build-own0");
    const plan = await collectCleanupPlan(options({
      trackedPaths: ["build-own0/CMakeCache.txt"],
    }));
    expect(plan.candidates).toEqual([]);
    expect(plan.refused).toEqual([
      expect.objectContaining({ path: build, reason: "target contains a tracked path" }),
    ]);
    expect(await exists(build)).toBe(true);
  });

  test("refuses links and reparse points inside a candidate", async () => {
    const build = await makeCMakeBuild("build-run0");
    const external = path.join(sandbox, "external");
    await mkdir(external);
    await symlink(external, path.join(build, "escape"), process.platform === "win32" ? "junction" : "dir");

    const plan = await collectCleanupPlan(options());
    expect(plan.candidates).toEqual([]);
    expect(plan.refused).toEqual([
      expect.objectContaining({ path: build }),
    ]);
    expect(plan.refused[0].reason).toMatch(/link or reparse point/);
    expect(await exists(external)).toBe(true);
  });

  test("refuses a build-prefixed file and a linked build target", async () => {
    const regularFile = path.join(workspace, "build-file");
    await writeFile(regularFile, "not a directory");
    const external = path.join(sandbox, "external-cmake");
    await makeFile(path.join(external, "CMakeCache.txt"), "external\n");
    const linked = path.join(workspace, "build-linked");
    await symlink(external, linked, process.platform === "win32" ? "junction" : "dir");

    const plan = await collectCleanupPlan(options());
    expect(plan.candidates).toEqual([]);
    expect(plan.refused.map((item) => item.path)).toEqual([regularFile, linked].sort());
    expect(plan.refused.find((item) => item.path === regularFile)?.reason).toMatch(/physical directory/);
    expect(plan.refused.find((item) => item.path === linked)?.reason).toMatch(/physical directory/);
    expect(await exists(external)).toBe(true);
  });

  test("uses keep only to subtract an allowlisted candidate", async () => {
    const first = await makeCMakeBuild("build");
    const retained = await makeCMakeBuild("build-own0");
    const unlisted = path.join(workspace, "src");
    await makeFile(path.join(unlisted, "main.w"));
    const invalid = path.join(workspace, "build-source");
    await makeFile(path.join(invalid, "main.w"));

    const plan = await collectCleanupPlan(options({
      keep: [path.join("build-own0", "active.ninja"), "build-source"],
    }));
    expect(plan.candidates.map((candidate) => candidate.path)).toEqual([first]);
    expect(plan.retained).toEqual([
      expect.objectContaining({ path: retained }),
    ]);
    expect(plan.candidates.some((candidate) => candidate.path === unlisted)).toBe(false);
    expect(plan.refused).toEqual([
      expect.objectContaining({ path: invalid, reason: "target has no CMakeCache.txt output marker" }),
    ]);
  });

  test("refuses a candidate with protected nested data", async () => {
    const build = await makeCMakeBuild("build");
    await makeFile(path.join(build, "node_modules", "package", "index.js"));
    const plan = await collectCleanupPlan(options());
    expect(plan.candidates).toEqual([]);
    expect(plan.refused[0]).toMatchObject({ path: build });
    expect(plan.refused[0].reason).toMatch(/protected path/);
  });

  test("requires the verified workspace to be the Git root", async () => {
    await expect(collectCleanupPlan(options({ gitRoot: sandbox }))).rejects.toThrow("Git worktree root");
  });
});

describe("legacy temporary cleanup", () => {
  test("selects only an exact old mkdtemp prefix", async () => {
    const old = await makeOldTemporary("w-seed-parser-ABC123", 25);
    const fresh = await makeOldTemporary("w-owner-guard-DEF456", 1);
    const unknown = await makeOldTemporary("w-unrelated-XYZ789", 48);
    const prefixLookalike = await makeOldTemporary("w-seed-parser-notrandom-ABC123", 48);

    const plan = await collectCleanupPlan(options({
      workspace: false,
      temp: true,
      nowMs: Date.now(),
    }));
    expect(plan.candidates.map((candidate) => candidate.path)).toEqual([old]);
    expect(plan.retained).toEqual([
      expect.objectContaining({ path: fresh, reason: "temporary directory is newer than 24 hours" }),
    ]);
    expect(plan.candidates.some((candidate) => candidate.path === unknown)).toBe(false);
    expect(plan.candidates.some((candidate) => candidate.path === prefixLookalike)).toBe(false);
    expect(await exists(old)).toBe(true);
  });

  test("rejects an age threshold below 24 hours", async () => {
    await expect(collectCleanupPlan(options({
      workspace: false,
      temp: true,
      tempMinAgeMs: DEFAULT_TEMP_MIN_AGE_MS - 1,
    }))).rejects.toThrow("at least 24 hours");
  });

  test("refuses programmatic apply for a legacy temporary plan", async () => {
    const old = await makeOldTemporary("w-seed-parser-ABC123", 25);
    const plan = await collectCleanupPlan(options({ workspace: false, temp: true }));
    const report = await applyCleanupPlan(plan, { mountProof: noMounts });
    expect(report.removed).toEqual([]);
    expect(report.error).toBe("legacy temporary cleanup is dry-run only");
    expect(report.refused).toEqual([
      expect.objectContaining({ path: old, reason: "legacy temporary cleanup is dry-run only" }),
    ]);
    expect(await exists(old)).toBe(true);
  });
});

describe("mount proof", () => {
  test("decodes Linux mountinfo paths", () => {
    const source = [
      "31 23 0:26 / / rw,relatime - ext4 /dev/root rw",
      "32 31 0:27 / /tmp/w\\040bind\\134name rw,relatime - ext4 /dev/root rw",
      "",
    ].join("\n");
    expect(parseLinuxMountInfo(source)).toEqual(["/", "/tmp/w bind\\name"]);
    expect(() => parseLinuxMountInfo("malformed\n")).toThrow("malformed record");
  });

  test("rejects a synthetic same-device bind mount", async () => {
    const build = await makeCMakeBuild("build");
    const plan = await collectCleanupPlan(options());
    const bindMount = path.join(build, "same-device-bind");
    const report = await applyCleanupPlan(plan, {
      mountProof: { supported: true, platform: "linux", mountPoints: [bindMount] },
    });
    expect(report.removed).toEqual([]);
    expect(report.refused).toEqual([
      expect.objectContaining({ path: build, reason: `mountpoint exists: ${bindMount}` }),
    ]);
    expect(await exists(build)).toBe(true);
  });

  test("fails closed when the platform has no mount proof", async () => {
    const build = await makeCMakeBuild("build");
    const plan = await collectCleanupPlan(options());
    const proof = await getMountProof({ platform: "unsupported-test" });
    const report = await applyCleanupPlan(plan, { mountProof: proof });
    expect(report.removed).toEqual([]);
    expect(report.error).toMatch(/mount absence cannot be proved/);
    expect(report.refused[0]).toMatchObject({ path: build });
    expect(report.refused[0].reason).toMatch(/mount absence cannot be proved/);
    expect(await exists(build)).toBe(true);
  });
});

describe("transactional apply", () => {
  test("keeps dry-run output deterministic without removing data", async () => {
    const later = await makeCMakeBuild("build-zeta", "zeta");
    const earlier = await makeCMakeBuild("build-alpha", "alpha");
    const plan = await collectCleanupPlan(options());
    const first = formatCleanupReport(plan, false);
    const second = formatCleanupReport(plan, false);
    expect(first).toEqual(second);
    expect(plan.candidates.map((candidate) => candidate.path)).toEqual([earlier, later]);
    expect(await readFile(path.join(earlier, "artifact.bin"), "utf8")).toBe("alpha");
    expect(await readFile(path.join(later, "artifact.bin"), "utf8")).toBe("zeta");
  });

  test("refuses a candidate that changes after planning", async () => {
    const build = await makeCMakeBuild("build");
    const plan = await collectCleanupPlan(options());
    await writeFile(path.join(build, "artifact.bin"), "mutated after plan");

    const report = await applyCleanupPlan(plan, { mountProof: noMounts });
    expect(report.removed).toEqual([]);
    expect(report.refused).toEqual([
      expect.objectContaining({ path: build, reason: "target changed after the dry-run scan" }),
    ]);
    expect(await exists(build)).toBe(true);
  });

  test("preflights all candidates before the first removal", async () => {
    const stable = await makeCMakeBuild("build-alpha", "stable");
    const changed = await makeCMakeBuild("build-zeta", "before");
    const plan = await collectCleanupPlan(options());
    await writeFile(path.join(changed, "artifact.bin"), "changed after plan");

    const report = await applyCleanupPlan(plan, { mountProof: noMounts });
    expect(report.removed).toEqual([]);
    expect(report.refused).toEqual([
      expect.objectContaining({ path: stable, reason: "batch preflight failed before removal" }),
      expect.objectContaining({ path: changed, reason: "target changed after the dry-run scan" }),
    ]);
    expect(await exists(stable)).toBe(true);
    expect(await exists(changed)).toBe(true);
  });
});
