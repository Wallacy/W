import crypto from "node:crypto";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { afterEach, describe, expect, test } from "bun:test";
import {
  buildStudyRegistry,
  checkStudyRegistry,
  refreshStudyDigests,
  repositoryRoot,
  serializeStudyMarkdown,
  serializeStudyRegistry,
  validateStudyRegistry,
  writeStudyRegistry,
} from "./study-registry.mjs";

const temporaryRoots = [];

function digest(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

function writeJson(file, value) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function makeRoot(studies) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "w-study-registry-"));
  temporaryRoots.push(root);
  fs.mkdirSync(path.join(root, "tooling", "tree-sitter-w"), { recursive: true });
  writeJson(path.join(root, "package.json"), { scripts: {} });
  writeJson(path.join(root, "tooling", "tree-sitter-w", "package.json"), { scripts: {} });
  for (const study of studies) {
    const directory = path.join(root, "tooling", "studies", study.directory);
    fs.mkdirSync(directory, { recursive: true });
    for (const [name, content] of Object.entries(study.files ?? {})) {
      const file = path.join(directory, name);
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, content, "utf8");
    }
    writeJson(path.join(directory, "study.json"), {
      $schema: "w-test-study-1",
      status: "design-oracle-input",
      id: study.id,
      ...(study.metadata ?? {}),
    });
    for (const [name, content] of Object.entries(study.metadataFiles ?? {})) {
      const file = path.join(directory, name);
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, content, "utf8");
    }
  }
  return root;
}

afterEach(() => {
  for (const root of temporaryRoots.splice(0)) fs.rmSync(root, { recursive: true, force: true });
});

describe("study registry integrity", () => {
  test("enumerates all current studies with compact global indexes", () => {
    const result = buildStudyRegistry({ root: repositoryRoot });
    const { registry } = result;
    const studiesDirectory = path.join(repositoryRoot, "tooling", "studies");
    const directories = fs.readdirSync(studiesDirectory, { withFileTypes: true })
      .filter((entry) => entry.isDirectory())
      .map((entry) => `tooling/studies/${entry.name}`)
      .sort();
    expect(registry.studies.map((study) => study.directory)).toEqual(directories);
    expect(registry.counts.studyDirectories).toBe(registry.studies.length);
    expect(registry.counts.metadataFiles).toBe(registry.metadata.length);
    expect(registry.counts.fixtureFiles).toBe(registry.fixtures.length);
    expect(registry.counts.references).toBe(registry.references.length);
    expect(registry.counts.digestReferences + registry.counts.pathOnlyReferences).toBe(registry.counts.references);
    expect(registry.counts.digestReferences).toBe(registry.references.filter((record) => record.declaredDigest !== null).length);
    expect(registry.counts.pathOnlyReferences).toBe(registry.references.filter((record) => record.declaredDigest === null).length);
    expect(registry.counts.dependencyEdges).toBe(registry.graph.edges.length);
    expect(registry.counts.entrypoints).toBe(registry.entrypoints.length);
    expect(registry.counts.scripts).toBe(registry.scripts.length);
    expect(registry.counts.roots).toBe(registry.graph.roots.length);
    expect(registry.counts.leaves).toBe(registry.graph.leaves.length);
    expect(registry.counts.cycles).toBe(registry.graph.cycles.length);
    expect(registry.counts.missing).toBe(registry.missing.length);
    expect(registry.counts.stale).toBe(registry.stale.length);
    expect(registry.counts.duplicates).toBe(registry.duplicates.length);
    expect(registry.counts.invalidJson).toBe(registry.invalidJson.length);
    expect(registry.benchmarkDisposition).toBe("not-applicable");
    expect(registry).not.toHaveProperty("inputs");
    expect(registry.integrity).toEqual({
      valid: true,
      counts: { missing: 0, stale: 0, duplicates: 0, invalidJson: 0, cycles: 0 },
    });
    expect(registry.studies.every((study) => Object.hasOwn(study, "title"))).toBe(true);
    expect(registry.metadata.every((record) => typeof record.digest === "string")).toBe(true);
    expect(registry.fixtures.every((record) => typeof record === "string")).toBe(true);
    expect(registry.references.every((record) => Number.isInteger(record.study) && Number.isInteger(record.metadata))).toBe(true);
    expect(registry.studies.every((study) => study.references.every(Number.isInteger))).toBe(true);
    expect(registry.studies.every((study) => study.entrypoints.every(Number.isInteger))).toBe(true);
    expect(registry.studies.every((study) => study.dependencies.inbound.every(Number.isInteger) && study.dependencies.outbound.every(Number.isInteger))).toBe(true);
    expect(registry.entrypoints.every((entrypoint) => Array.isArray(entrypoint.studies) && Array.isArray(entrypoint.appliesTo))).toBe(true);
    expect(serializeStudyRegistry(registry)).not.toMatch(/"cases"\s*:/u);
    expect(serializeStudyRegistry(registry)).not.toMatch(/\r\n/u);
    expect(Buffer.byteLength(serializeStudyRegistry(registry), "utf8")).toBeLessThan(750 * 1024);
    const markdown = serializeStudyMarkdown(registry);
    expect(markdown).toContain("# Estudos do W\n");
    expect(markdown).toContain("| ID | Função / estado | Caminho | Gate principal | Entrypoint principal |");
    expect(markdown.split("\n").find((line) => line.startsWith("| `CAP0` |"))).toContain("`bun run check:study-bundles`");
    expect(markdown).toContain("| **Total** | **");
    expect(markdown).toContain("Status: `design-oracle-input`");
    expect(markdown).not.toMatch(/\r\n/u);
  });

  test("reports a missing path", () => {
    const root = makeRoot([{
      directory: "missing",
      id: "MISSING",
      metadata: { references: [{ path: "does-not-exist.txt" }] },
    }]);
    const result = buildStudyRegistry({ root });
    expect(result.issues.missing.some((issue) => issue.reason === "missing")).toBe(true);
  });

  test("reports a stale digest", () => {
    const root = makeRoot([{
      directory: "stale",
      id: "STALE",
      files: { "fixture.txt": "current bytes\n" },
      metadata: { references: [{ path: "fixture.txt", digest: `sha256:${"0".repeat(64)}` }] },
    }]);
    const result = buildStudyRegistry({ root });
    expect(result.issues.stale.some((issue) => issue.reason === "digest-mismatch")).toBe(true);
  });

  test("refreshes only resolved stale digest references", () => {
    const content = "current bytes\n";
    const root = makeRoot([{
      directory: "refresh",
      id: "REFRESH",
      files: { "fixture.txt": content },
      metadata: { references: [{ path: "fixture.txt", digest: `sha256:${"0".repeat(64)}` }] },
    }]);
    const refreshed = refreshStudyDigests({ root });
    expect(refreshed).toEqual({ written: true, updatedFiles: 1, updatedReferences: 1, errors: [] });
    const metadata = JSON.parse(fs.readFileSync(path.join(root, "tooling", "studies", "refresh", "study.json"), "utf8"));
    expect(metadata.references[0].digest).toBe(digest(content));
    expect(buildStudyRegistry({ root }).issues.stale).toHaveLength(0);
  });

  test("rejects an invalid digest", () => {
    const root = makeRoot([{
      directory: "invalid-digest",
      id: "INVALID-DIGEST",
      files: { "fixture.txt": "current bytes\n" },
      metadata: { references: [{ path: "fixture.txt", digest: "sha256:not-a-digest" }] },
    }]);
    const result = buildStudyRegistry({ root });
    expect(result.issues.stale.some((issue) => issue.reason === "invalid-digest")).toBe(true);
    expect(result.registry.integrity.valid).toBe(false);
  });

  test("reports both a missing path and an invalid digest", () => {
    const root = makeRoot([{
      directory: "missing-invalid-digest",
      id: "MISSING-INVALID-DIGEST",
      metadata: { references: [{ path: "does-not-exist.txt", digest: "sha256:not-a-digest" }] },
    }]);
    const result = buildStudyRegistry({ root });
    expect(result.issues.missing.some((issue) => issue.reason === "missing")).toBe(true);
    expect(result.issues.stale.some((issue) => issue.reason === "invalid-digest")).toBe(true);
  });

  test("rejects traversal outside the repository and uses one unambiguous base", () => {
    const root = makeRoot([{
      directory: "paths",
      id: "PATHS",
      files: { "local.txt": "local\n" },
      metadata: { references: [{ path: "../../../../outside.txt" }, { path: "local.txt" }] },
    }]);
    const result = buildStudyRegistry({ root });
    expect(result.issues.missing.some((issue) => issue.reason === "invalid-path")).toBe(true);
    const local = result.registry.references.find((reference) => reference.path === "local.txt");
    expect(local?.resolved).toBe("tooling/studies/paths/local.txt");
  });

  test("reports a duplicate reference in one metadata array", () => {
    const content = "fixture bytes\n";
    const root = makeRoot([{
      directory: "duplicate",
      id: "DUPLICATE",
      files: { "fixture.txt": content },
      metadata: {
        references: [
          { path: "fixture.txt", digest: digest(content) },
          { path: "fixture.txt", digest: digest(content) },
        ],
      },
    }]);
    const result = buildStudyRegistry({ root });
    expect(result.issues.duplicates.some((issue) => issue.reason === "duplicate-reference")).toBe(true);
  });

  test("reports a duplicate primary study ID", () => {
    const root = makeRoot([
      { directory: "first", id: "DUPLICATE-ID" },
      { directory: "second", id: "DUPLICATE-ID" },
    ]);
    const result = buildStudyRegistry({ root });
    expect(result.issues.duplicates.some((issue) => issue.reason === "duplicate-study-id")).toBe(true);
  });

  test("reports invalid metadata JSON", () => {
    const root = makeRoot([{
      directory: "invalid-json",
      id: "INVALID-JSON",
      metadataFiles: { "bundle.json": "{ invalid\n" },
    }]);
    const result = buildStudyRegistry({ root });
    expect(result.issues.invalidJson.some((issue) => issue.reason === "invalid-json")).toBe(true);
    expect(result.registry.integrity.valid).toBe(false);
  });

  test("enumerates a fixture below a study src directory", () => {
    const root = makeRoot([{
      directory: "source-tree",
      id: "SOURCE-TREE",
      files: { "src/fixture.w": "fn sourceTree() {}\n" },
    }]);
    const result = buildStudyRegistry({ root });
    expect(result.registry.fixtures).toContain("tooling/studies/source-tree/src/fixture.w");
    expect(result.registry.studies[0].fixtures).toHaveLength(1);
  });

  test("reports a dependency cycle", () => {
    const root = makeRoot([
      { directory: "alpha", id: "ALPHA", metadata: { buildsOn: [{ path: "../beta/study.json" }] } },
      { directory: "beta", id: "BETA", metadata: { buildsOn: [{ path: "../alpha/study.json" }] } },
    ]);
    const result = buildStudyRegistry({ root });
    expect(result.registry.graph.cycles).toEqual([["ALPHA", "BETA", "ALPHA"]]);
  });

  test("fails write before changing the output when integrity is invalid", () => {
    const root = makeRoot([{
      directory: "write-failure",
      id: "WRITE-FAILURE",
      files: { "fixture.txt": "current bytes\n" },
      metadata: { references: [{ path: "fixture.txt", digest: `sha256:${"0".repeat(64)}` }] },
    }]);
    const file = path.join(root, "tooling", "study-registry.json");
    const markdownFile = path.join(root, "STUDIES.md");
    fs.writeFileSync(file, "sentinel\n", "utf8");
    fs.writeFileSync(markdownFile, "sentinel markdown\n", "utf8");
    const result = writeStudyRegistry({ root, file, markdownFile });
    expect(result.written).toBe(false);
    expect(result.errors.length).toBeGreaterThan(0);
    expect(fs.readFileSync(file, "utf8")).toBe("sentinel\n");
    expect(fs.readFileSync(markdownFile, "utf8")).toBe("sentinel markdown\n");
  });

  test("writes and checks JSON and Markdown projections as one deterministic pair", () => {
    const root = makeRoot([{ directory: "valid", id: "VALID", metadata: { title: "Valid study" } }]);
    const file = path.join(root, "tooling", "study-registry.json");
    const markdownFile = path.join(root, "STUDIES.md");
    const first = writeStudyRegistry({ root, file, markdownFile });
    expect(first.written).toBe(true);
    const jsonBytes = fs.readFileSync(file, "utf8");
    const markdownBytes = fs.readFileSync(markdownFile, "utf8");
    expect(checkStudyRegistry({ root, file, markdownFile }).errors).toEqual([]);
    const second = writeStudyRegistry({ root, file, markdownFile });
    expect(second.written).toBe(true);
    expect(fs.readFileSync(file, "utf8")).toBe(jsonBytes);
    expect(fs.readFileSync(markdownFile, "utf8")).toBe(markdownBytes);
  });

  test("restores both sentinels when the second target installation fails", () => {
    const root = makeRoot([{ directory: "valid", id: "VALID", metadata: { title: "Valid study" } }]);
    const file = path.join(root, "tooling", "study-registry.json");
    const markdownFile = path.join(root, "STUDIES.md");
    fs.writeFileSync(file, "json sentinel\n", "utf8");
    fs.writeFileSync(markdownFile, "markdown sentinel\n", "utf8");
    const targets = new Set([path.resolve(file), path.resolve(markdownFile)]);
    let installationCount = 0;
    let failureInjected = false;
    const fileSystem = {
      ...fs,
      renameSync(source, target, ...options) {
        if (targets.has(path.resolve(target)) && path.basename(source).endsWith(".tmp")) {
          installationCount += 1;
          if (installationCount === 2 && !failureInjected) {
            failureInjected = true;
            throw new Error("injected second installation failure");
          }
        }
        return fs.renameSync(source, target, ...options);
      },
    };
    const result = writeStudyRegistry({ root, file, markdownFile, fileSystem });
    expect(result.written).toBe(false);
    expect(failureInjected).toBe(true);
    expect(fs.readFileSync(file, "utf8")).toBe("json sentinel\n");
    expect(fs.readFileSync(markdownFile, "utf8")).toBe("markdown sentinel\n");
    expect(fs.readdirSync(root).filter((name) => /^(?:\.study-registry\.json|\.STUDIES\.md)\..+\.tmp$/u.test(name))).toEqual([]);
  });

  test("keeps committed outputs when backup cleanup fails", () => {
    const root = makeRoot([{ directory: "valid", id: "VALID", metadata: { title: "Valid study" } }]);
    const file = path.join(root, "tooling", "study-registry.json");
    const markdownFile = path.join(root, "STUDIES.md");
    fs.writeFileSync(file, "old json\n", "utf8");
    fs.writeFileSync(markdownFile, "old markdown\n", "utf8");
    let cleanupFailures = 0;
    const fileSystem = {
      ...fs,
      rmSync(target, ...options) {
        if (path.basename(target).startsWith(".study-registry.json.") && cleanupFailures === 0) {
          cleanupFailures += 1;
          throw new Error("injected backup cleanup failure");
        }
        return fs.rmSync(target, ...options);
      },
    };
    const result = writeStudyRegistry({ root, file, markdownFile, fileSystem });
    const expected = buildStudyRegistry({ root }).registry;
    expect(result.written).toBe(true);
    expect(cleanupFailures).toBe(1);
    expect(fs.readFileSync(file, "utf8")).toBe(serializeStudyRegistry(expected));
    expect(fs.readFileSync(markdownFile, "utf8")).toBe(serializeStudyMarkdown(expected));
    expect(fs.readdirSync(root).filter((name) => /^(?:\.study-registry\.json|\.STUDIES\.md)\..+\.tmp$/u.test(name))).toEqual([]);
  });

  test("rejects a stale human projection through the shared API", () => {
    const root = makeRoot([{ directory: "valid", id: "VALID" }]);
    const file = path.join(root, "tooling", "study-registry.json");
    const markdownFile = path.join(root, "STUDIES.md");
    const generated = writeStudyRegistry({ root, file, markdownFile });
    expect(generated.written).toBe(true);
    fs.writeFileSync(markdownFile, "stale\n", "utf8");
    const result = checkStudyRegistry({ root, file, markdownFile });
    expect(result.errors).toContain("STUDIES.md is stale. Run bun tooling/study-registry.mjs --write.");
  });

  test("rejects a registry projection that is stale through the shared API", () => {
    const root = makeRoot([{ directory: "valid", id: "VALID" }]);
    const registry = buildStudyRegistry({ root }).registry;
    registry.counts.studyDirectories += 1;
    const file = path.join(root, "tooling", "study-registry.json");
    fs.writeFileSync(file, serializeStudyRegistry(registry), "utf8");
    const result = checkStudyRegistry({ root, file });
    expect(result.errors.some((error) => error.includes("study-registry.json is stale"))).toBe(true);
    expect(validateStudyRegistry(registry, { root }).errors.some((error) => error.includes("study-registry.json is stale"))).toBe(true);
  });
});
