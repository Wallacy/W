import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { afterEach, describe, expect, test } from "bun:test";
import {
  CHECK_TARGETS,
  DEMO_TARGETS,
  bootstrapCommand,
  checkPlan,
  formatCheckList,
  formatDemoList,
  invokeChild,
  loadCatalog,
  parseBootstrapArguments,
  parseCheckArguments,
  parseDemoArguments,
  parseDevRunArguments,
  resolveContainedFile,
  resolveCheckTarget,
  resolveExplicitSource,
  runDevRun,
  validateHostSourceIdentity,
  validateCatalog,
} from "./dev-cli.mjs";
import { loadCheckSuites } from "./check-suite.mjs";

const temporaryRoots = [];

function makeRoot() {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "w-dev-cli-"));
  temporaryRoots.push(root);
  return root;
}

function writeFile(root, relativePath, contents = "") {
  const file = path.join(root, relativePath);
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, contents, "utf8");
  return file;
}

function fakeCatalog(binary = "build/w.exe") {
  return {
    host: {
      platform: "win32",
      arch: "x64",
      binary,
      receipt: "build/receipt.json",
      recipe: "tooling/build.mjs",
      profile: "development",
      c11Recovery: true,
    },
  };
}

afterEach(() => {
  for (const root of temporaryRoots.splice(0))
    fs.rmSync(root, { recursive: true, force: true });
});

describe("DEVCLI1 catalog and parsing", () => {
  test("validates the checked-in catalog and keeps targets bounded", () => {
    const catalog = loadCatalog();
    expect(validateCatalog({ catalog }).errors).toEqual([]);
    expect(Object.keys(catalog.checks)).toEqual(CHECK_TARGETS);
    expect(Object.keys(catalog.demos)).toEqual(DEMO_TARGETS);
    expect(catalog.host.binary).toBe("build/w-windows/w.exe");
    const drifted = structuredClone(catalog);
    drifted.host.extra = true;
    expect(validateCatalog({ catalog: drifted }).errors.join("\n")).toContain(
      "catalog.host keys are invalid",
    );
  });

  test("rejects catalog paths that escape the repository", () => {
    const catalog = loadCatalog();
    const invalid = structuredClone(catalog);
    invalid.demos.hello.fixture = "../outside.w";
    const result = validateCatalog({ catalog: invalid });
    expect(result.errors.join("\n")).toContain("escapes the repository root");
  });

  test("parses public defaults, targets, list, and dry-run without execution", () => {
    expect(parseCheckArguments([])).toEqual({
      target: "quick", list: false, listAll: false, dryRun: false, help: false,
    });
    expect(parseCheckArguments(["--target", "compiler", "--dry-run"])).toEqual({
      target: "compiler", list: false, listAll: false, dryRun: true, help: false,
    });
    expect(parseCheckArguments(["--list"])).toEqual({
      target: "quick", list: true, listAll: false, dryRun: false, help: false,
    });
    expect(parseCheckArguments(["--list-all"])).toEqual({
      target: "quick", list: false, listAll: true, dryRun: false, help: false,
    });
    expect(() => parseCheckArguments(["--list", "--target", "all"])).toThrow();
    expect(() => parseCheckArguments(["--list", "--dry-run"])).toThrow();
    expect(() => parseCheckArguments(["--list", "--list-all"])).toThrow();
    expect(parseCheckArguments(["--target", "hlo0"])).toEqual({
      target: "hlo0", list: false, listAll: false, dryRun: false, help: false,
    });
    expect(() => parseCheckArguments(["--target", "not a target"])).toThrow();

    expect(parseDemoArguments([])).toEqual({ target: "hello", list: false, help: false });
    expect(parseDemoArguments(["--target", "bool-short-circuit"])).toEqual({
      target: "bool-short-circuit", list: false, help: false,
    });
    expect(parseDemoArguments(["--list"])).toEqual({ target: "hello", list: true, help: false });
    expect(() => parseDemoArguments(["--list", "--target", "hello"])).toThrow();

    expect(parseBootstrapArguments(["--target", "host"])).toEqual({
      target: "host", help: false,
    });
    expect(() => parseBootstrapArguments([])).toThrow();
    expect(() => parseBootstrapArguments(["--target", "linux"])).toThrow();
  });

  test("parses an explicit dev source and forwards only arguments after --", () => {
    expect(parseDevRunArguments(["run", "examples/demo.w", "--", "one", "--two"]))
      .toEqual({ help: false, source: "examples/demo.w", args: ["one", "--two"] });
    expect(parseDevRunArguments(["--help"])).toEqual({ help: true, source: null, args: [] });
    expect(() => parseDevRunArguments(["run"])).toThrow();
    expect(() => parseDevRunArguments(["run", "a.w", "b.w"])).toThrow();
    expect(() => parseDevRunArguments(["run", "../outside.w"])).not.toThrow();
  });
});

describe("DEVCLI1 plans and contained process boundaries", () => {
  test("lists manifest-backed suites and expands dry-run from the existing runner", () => {
    const catalog = loadCatalog();
    const loaded = loadCheckSuites();
    const list = formatCheckList({
      catalog,
      suites: loaded.suites,
      commands: loaded.commands,
    });
    expect(list).toContain("quick\troot-quick\t");
    expect(list).toContain("all\troot-check\t");
    expect(list).not.toContain("hlo0\tcheck:hlo0\t1\t");
    const completeList = formatCheckList({
      catalog,
      suites: loaded.suites,
      commands: loaded.commands,
      includeLeaves: true,
    });
    expect(completeList).toContain("hlo0\tcheck:hlo0\t1\t");
    expect(completeList).toContain("w-run\tcheck:w-run\t1\t");
    expect(formatDemoList(catalog)).toContain("bool-short-circuit\t");
    const plan = checkPlan({ catalog, suites: loaded.suites, target: "quick" });
    expect(plan.suite).toBe("root-quick");
    expect(plan.steps.length).toBeGreaterThan(0);
    expect(plan.steps[0]).toEqual({ package: "root", script: "check:suite-manifest" });
    expect(resolveCheckTarget({
      catalog,
      commands: loaded.commands,
      target: "docs",
    })).toEqual({ kind: "suite", target: "docs", suite: "root-docs" });
    expect(resolveCheckTarget({
      catalog,
      commands: loaded.commands,
      target: "hlo0",
    })).toEqual({ kind: "command", target: "hlo0", commandName: "check:hlo0" });
    expect(() => resolveCheckTarget({
      catalog,
      commands: loaded.commands,
      target: "missing-leaf",
    })).toThrow("unknown check target");
  });

  test("contains catalog files but accepts an explicit external source", () => {
    const root = makeRoot();
    const inside = writeFile(root, "source.w", "entry(main)\n");
    const outsideRoot = makeRoot();
    const outside = writeFile(outsideRoot, "outside.w", "entry(main)\n");
    expect(resolveContainedFile(root, "source.w", "source", ".w")).toBe(inside);
    expect(() => resolveContainedFile(root, outside, "source", ".w")).toThrow(/escapes/);
    expect(resolveExplicitSource(outside, { cwd: root, root })).toBe(outside);
    expect(() => resolveContainedFile(root, "missing.w", "source", ".w")).toThrow(/unavailable/);
    const link = path.join(root, "link.w");
    try {
      fs.symlinkSync(outside, link, "file");
      expect(() => resolveContainedFile(root, "link.w", "source", ".w")).toThrow(/outside/);
      expect(resolveExplicitSource("link.w", { cwd: root, root })).toBe(outside);
    } catch (error) {
      if (error?.code !== "EPERM" && error?.code !== "EACCES") throw error;
    }
  });

  test("keeps receipt HEAD and dirty identity exact", () => {
    const receipt = { source: { head: "abc", dirty: false } };
    expect(validateHostSourceIdentity({
      receipt,
      source: { head: "abc", dirty: false },
    })).toEqual({ strong: true });
    expect(() => validateHostSourceIdentity({
      receipt: { source: { head: "abc", dirty: true } },
      source: { head: "abc", dirty: false },
    })).toThrow(/dirty flag/);
    expect(() => validateHostSourceIdentity({
      receipt,
      source: { head: "abc", dirty: true },
    })).toThrow(/dirty flag/);
    const warnings = [];
    expect(validateHostSourceIdentity({
      receipt: { source: { head: "abc", dirty: true } },
      source: { head: "abc", dirty: true },
      warn: (message) => warnings.push(message),
    })).toEqual({ strong: false });
    expect(warnings[0]).toContain("non-strong");
  });

  test("builds the bootstrap argv without a shell or implicit download", () => {
    const command = bootstrapCommand({ catalog: loadCatalog() });
    expect(command.command).toBe(process.execPath);
    expect(command.args).toEqual([
      "run", "tooling/build-w-windows.mjs", "--profile", "development", "--c11-recovery",
    ]);
  });

  test("preserves child exit status and structured argv", () => {
    let observed;
    const status = invokeChild("w.exe", ["run", "sample.w", "--", "--arg"], {
      root: process.cwd(),
      spawn: (command, args, options) => {
        observed = { command, args, options };
        return { status: 17 };
      },
    });
    expect(status).toBe(17);
    expect(observed.command).toBe("w.exe");
    expect(observed.args).toEqual(["run", "sample.w", "--", "--arg"]);
    expect(observed.options.shell).toBe(false);
    expect(observed.options.stdio).toBe("inherit");
  });

  test("dev run validates a contained source and preserves forwarding and exit", async () => {
    const root = makeRoot();
    const source = writeFile(root, "examples/hello.w", "entry(main)\n");
    const binary = writeFile(root, "build/w.exe", "binary\n");
    const catalog = fakeCatalog();
    let observed;
    const status = await runDevRun({
      root,
      catalog,
      source: path.relative(root, source),
      args: ["first", "--second"],
      platform: "win32",
      arch: "x64",
      validateBuild: async () => ({ binary }),
      reportTiming: false,
      spawn: (command, args, options) => {
        observed = { command, args, options };
        return { status: 23 };
      },
    });
    expect(status).toBe(23);
    expect(observed.command).toBe(binary);
    expect(observed.args).toEqual([
      "run", source, "--", "first", "--second",
    ]);
    expect(observed.options.shell).toBe(false);
  });
});
