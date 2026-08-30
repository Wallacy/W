import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { afterEach, describe, expect, test } from "bun:test";
import {
  CHECK_SUITE_SCHEMA,
  flattenCheckSuite,
  loadCheckSuites,
  parseCheckSuiteArguments,
  runCheckSuite,
  validateCheckSuites,
} from "./check-suite.mjs";

const temporaryRoots = [];

function writeJson(file, value) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function makeRoot({ scripts = { test: "node -e 0" }, manifest } = {}) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "w-check-suite-"));
  temporaryRoots.push(root);
  writeJson(path.join(root, "package.json"), { scripts });
  writeJson(path.join(root, "tooling", "tree-sitter-w", "package.json"), { scripts });
  writeJson(path.join(root, "tooling", "check-suites.json"), manifest);
  return root;
}

function baseManifest(overrides = {}) {
  return {
    $schema: CHECK_SUITE_SCHEMA,
    version: 1,
    packages: { root: ".", tree: "tooling/tree-sitter-w" },
    suites: {
      base: {
        description: "base suite",
        steps: [{ package: "root", script: "test" }],
      },
      wrapper: {
        description: "wrapper suite",
        steps: [{ suite: "base" }, { package: "tree", script: "test" }],
      },
    },
    ...overrides,
  };
}

afterEach(() => {
  for (const root of temporaryRoots.splice(0)) {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

describe("check-suite manifest", () => {
  test("validates the repository manifest and preserves expanded order", () => {
    const loaded = loadCheckSuites();
    expect(loaded.errors).toEqual([]);
    expect(Object.keys(loaded.suites)).toEqual([
      "root-check",
      "root-docs",
      "root-studies",
      "root-quick",
      "root-compiler",
      "tree-check",
      "tree-docs",
    ]);
    expect(flattenCheckSuite({ suites: loaded.suites, suiteName: "root-check" })).toHaveLength(108);
    expect(flattenCheckSuite({ suites: loaded.suites, suiteName: "root-docs" })).toHaveLength(79);
    expect(flattenCheckSuite({ suites: loaded.suites, suiteName: "root-studies" })).toHaveLength(33);
    expect(flattenCheckSuite({ suites: loaded.suites, suiteName: "root-quick" })).toHaveLength(21);
    expect(flattenCheckSuite({ suites: loaded.suites, suiteName: "root-compiler" })).toHaveLength(21);
    expect(flattenCheckSuite({ suites: loaded.suites, suiteName: "tree-check" })).toHaveLength(106);
    expect(flattenCheckSuite({ suites: loaded.suites, suiteName: "tree-docs" })).toHaveLength(76);
  });

  test("rejects unknown scripts, packages, suite references, and mixed step forms", () => {
    const manifest = baseManifest({
      suites: {
        bad: {
          description: "bad suite",
          steps: [
            { package: "missing", script: "test" },
            { package: "root", script: "missing" },
            { suite: "missing" },
            { suite: "base", package: "root" },
          ],
        },
      },
    });
    const result = validateCheckSuites({ manifest, root: makeRoot({ manifest }) });
    expect(result.errors.join("\n")).toContain("references an unknown package");
    expect(result.errors.join("\n")).toContain("references missing script");
    expect(result.errors.join("\n")).toContain("references an unknown suite");
    expect(result.errors.join("\n")).toContain("exactly package and script, or suite");
  });

  test("rejects path escape and package metadata without scripts", () => {
    const root = makeRoot({ manifest: baseManifest({ packages: { root: "..", tree: "tooling/tree-sitter-w" } }) });
    const result = validateCheckSuites({ manifest: baseManifest({ packages: { root: "..", tree: "tooling/tree-sitter-w" } }), root });
    expect(result.errors.join("\n")).toContain("escapes repository root");

    const noScriptsRoot = makeRoot({ manifest: baseManifest() });
    writeJson(path.join(noScriptsRoot, "package.json"), {});
    const noScripts = validateCheckSuites({ manifest: baseManifest(), root: noScriptsRoot });
    expect(noScripts.errors.join("\n")).toContain("must define scripts");
  });

  test("rejects cycles and flattening reports the cycle defensively", () => {
    const manifest = baseManifest({
      suites: {
        a: { description: "a", steps: [{ suite: "b" }] },
        b: { description: "b", steps: [{ suite: "a" }] },
      },
    });
    const result = validateCheckSuites({ manifest, root: makeRoot({ manifest }) });
    expect(result.errors.join("\n")).toContain("suite cycle");
    expect(() => flattenCheckSuite({ suites: manifest.suites, suiteName: "a" })).toThrow("suite cycle");
  });

  test("rejects duplicate leaves after nested suite expansion", () => {
    const manifest = baseManifest({
      suites: {
        ...baseManifest().suites,
        duplicate: {
          description: "duplicate leaf suite",
          steps: [{ suite: "base" }, { package: "root", script: "test" }],
        },
      },
    });
    const result = validateCheckSuites({ manifest, root: makeRoot({ manifest }) });
    expect(result.errors.join("\n")).toContain('suite "duplicate" expands to duplicate leaf "root/test"');
  });

  test("accepts exactly one CLI action and rejects ambiguous combinations", () => {
    expect(parseCheckSuiteArguments(["--check"])).toEqual({
      check: true,
      list: false,
      dryRun: false,
      suite: null,
    });
    expect(parseCheckSuiteArguments(["--list"]).list).toBe(true);
    expect(parseCheckSuiteArguments(["--suite", "base"]).suite).toBe("base");
    expect(parseCheckSuiteArguments(["--dry-run", "--suite", "base"])).toEqual({
      check: false,
      list: false,
      dryRun: true,
      suite: "base",
    });
    for (const argv of [
      [],
      ["--dry-run"],
      ["--check", "--list"],
      ["--check", "--suite", "base"],
      ["--list", "--dry-run"],
      ["--suite", "base", "--suite", "base"],
      ["--suite"],
      ["--suite", "--check"],
    ]) {
      expect(() => parseCheckSuiteArguments(argv)).toThrow();
    }
  });

  test("stops at the first failing child and preserves its exit code", () => {
    const manifest = baseManifest({
      suites: {
        failure: {
          description: "fail-fast suite",
          steps: [
            { package: "root", script: "first" },
            { package: "root", script: "fail" },
            { package: "root", script: "after" },
          ],
        },
      },
    });
    const root = makeRoot({
      manifest,
      scripts: {
        first: "bun first.mjs",
        fail: "bun fail.mjs",
        after: "bun after.mjs",
      },
    });
    for (const [name, source] of [
      ["first.mjs", "await Bun.write('marker.txt', (await Bun.file('marker.txt').text().catch(() => '')) + 'first\\n');"],
      ["fail.mjs", "await Bun.write('marker.txt', (await Bun.file('marker.txt').text().catch(() => '')) + 'fail\\n'); process.exit(7);"],
      ["after.mjs", "await Bun.write('marker.txt', (await Bun.file('marker.txt').text().catch(() => '')) + 'after\\n');"],
    ]) fs.writeFileSync(path.join(root, name), source, "utf8");
    const loaded = loadCheckSuites(root);
    const status = runCheckSuite({
      root,
      packageRecords: loaded.packages,
      suites: loaded.suites,
      suiteName: "failure",
      dryRun: false,
    });
    expect(status).toBe(7);
    expect(fs.readFileSync(path.join(root, "marker.txt"), "utf8")).toBe("first\nfail\n");
  });
});
