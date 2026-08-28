import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
export const repositoryRoot = path.resolve(toolingDirectory, "..");
export const registryPath = path.join(toolingDirectory, "study-registry.json");
export const STUDY_REGISTRY_SCHEMA = "w-study-registry-2";

const METADATA_NAMES = new Set(["study.json", "bundle.json", "manifest.json", "task-ledger.json"]);
const ROOT_RELATIVE_PREFIXES = [
  "tooling/",
  "reference/",
  "benchmarks/",
  "compiler/",
  "std/",
  "portal/",
  "history/",
];
const PATH_STRING_KEYS = new Set([
  "bundle",
  "checker",
  "corpus",
  "data",
  "hostTests",
  "machine",
  "manifest",
  "nestedChecker",
  "oracle",
  "referenceTest",
  "snapshot",
  "studyOracle",
  "wlo1",
  "wloChecker",
  "wloSnapshot",
]);
const DEPENDENCY_KEYS = new Set([
  "buildsOn",
  "bundles",
  "dependsOn",
  "dependencies",
  "independentReducers",
  "reusedStudies",
  "reusedUnits",
  "studyRefs",
]);
const SCRIPT_FILE_PATTERN = /(?:^|\/|\\)(?:[^/\\\s]+\.(?:c|json|jsonl|mjs|md|ts|txt|w))(?:$|[?])/iu;

function slash(value) {
  return value.replaceAll(path.sep, "/").replaceAll("\\", "/");
}

function comparePath(left, right) {
  return Buffer.from(slash(left)).compare(Buffer.from(slash(right)));
}

function relativePath(root, file) {
  return slash(path.relative(root, file));
}

function contained(root, file) {
  const relative = path.relative(root, file);
  return relative !== "" && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative);
}

function realContained(root, file) {
  try {
    return contained(fs.realpathSync(root), fs.realpathSync(file));
  } catch {
    return false;
  }
}

function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function validDigest(value) {
  return typeof value === "string" && /^sha256:[0-9a-f]{64}$/u.test(value);
}

function canonical(value) {
  if (Array.isArray(value)) return `[${value.map(canonical).join(",")}]`;
  if (!value || typeof value !== "object") return JSON.stringify(value);
  return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonical(value[key])}`).join(",")}}`;
}

function pointer(parent, key) {
  if (parent === "") return String(key);
  return Array.isArray(key) ? `${parent}[${key[0]}]` : `${parent}.${key}`;
}

function metadataFile(name) {
  return METADATA_NAMES.has(name) || name.endsWith("-manifest.json");
}

function metadataKind(file) {
  const name = path.basename(file);
  if (name === "study.json") return "study";
  if (name === "bundle.json") return "bundle";
  if (name.includes("manifest")) return "manifest";
  return "ledger";
}

function walkFiles(directory) {
  const files = [];
  if (!fs.existsSync(directory) || !fs.statSync(directory).isDirectory()) return files;
  const visit = (current) => {
    for (const entry of fs.readdirSync(current, { withFileTypes: true }).sort((left, right) => comparePath(left.name, right.name))) {
      if (entry.name === "node_modules") continue;
      const file = path.join(current, entry.name);
      if (entry.isDirectory()) visit(file);
      else if (entry.isFile()) files.push(file);
    }
  };
  visit(directory);
  return files.sort(comparePath);
}

function looksRootRelative(value) {
  const normalized = slash(value);
  return ROOT_RELATIVE_PREFIXES.some((prefix) => normalized.startsWith(prefix)) ||
    normalized === "DESIGN.md" || normalized === "DESIGN-INDEX.md" || normalized === "RATIONALE.md";
}

/** Resolve root-prefixed paths from the repository root and every other path from metadata's directory. */
function resolveReference(root, sourceFile, value) {
  if (typeof value !== "string" || value.trim() === "") {
    return { status: "invalid-path", resolved: null, base: null };
  }
  const normalized = slash(value);
  if (path.isAbsolute(value) || /^[A-Za-z]:[\\/]/u.test(value)) {
    return { status: "invalid-path", resolved: null, base: null };
  }
  const useRoot = looksRootRelative(normalized);
  const base = useRoot ? root : path.dirname(sourceFile);
  const resolved = path.resolve(base, value);
  if (!contained(root, resolved)) {
    return { status: "invalid-path", resolved: null, base: useRoot ? "root" : "metadata-directory" };
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    return { status: "missing", resolved: null, base: useRoot ? "root" : "metadata-directory" };
  }
  if (!realContained(root, resolved)) {
    return { status: "invalid-path", resolved: null, base: useRoot ? "root" : "metadata-directory" };
  }
  return { status: "resolved", resolved, base: useRoot ? "root" : "metadata-directory" };
}

function addIssue(list, issue) {
  list.push(issue);
}

function addReference({ value, parent, sourceFile, root, source, studyIndex, metadataIndex, pointerValue, referenceRows, issues }) {
  const resolved = resolveReference(root, sourceFile, value);
  const pathStatus = resolved.status;
  const declaredDigest = Object.hasOwn(parent ?? {}, "digest") ? parent.digest : null;
  const record = {
    study: studyIndex,
    metadata: metadataIndex,
    pointer: pointerValue,
    path: typeof value === "string" ? slash(value) : value,
    resolved: resolved.resolved ? relativePath(root, resolved.resolved) : null,
    declaredDigest: declaredDigest ?? null,
    status: resolved.status,
  };
  if (declaredDigest !== null && !validDigest(declaredDigest)) {
    record.status = "stale";
    addIssue(issues.stale, {
      source,
      pointer: pointerValue,
      path: record.path,
      resolved: record.resolved,
      declaredDigest,
      actualDigest: null,
      reason: "invalid-digest",
    });
  }
  if (resolved.resolved && declaredDigest !== null && validDigest(declaredDigest)) {
    const actualDigest = digestFile(resolved.resolved);
    if (actualDigest !== declaredDigest) {
      record.status = "stale";
      record.actualDigest = actualDigest;
      addIssue(issues.stale, {
        source,
        pointer: pointerValue,
        path: record.path,
        resolved: record.resolved,
        declaredDigest,
        actualDigest,
        reason: "digest-mismatch",
      });
    }
  }
  if (pathStatus === "missing" || pathStatus === "invalid-path") {
    addIssue(issues.missing, {
      source,
      pointer: pointerValue,
      path: record.path,
      resolved: record.resolved,
      reason: pathStatus,
    });
  }
  referenceRows.push({ ...record, source, referenceOrder: referenceRows.length });
  return referenceRows.at(-1);
}

function collectReferences(value, context, parent = null, currentPointer = "") {
  if (Array.isArray(value)) {
    const seen = new Map();
    for (const [index, child] of value.entries()) {
      if (child && typeof child === "object" && !Array.isArray(child) && typeof child.path === "string") {
        const key = canonical(child);
        if (seen.has(key)) {
          context.issues.duplicates.push({
            source: context.source,
            pointer: pointer(currentPointer, index),
            previous: seen.get(key),
            reason: "duplicate-reference",
          });
        } else {
          seen.set(key, pointer(currentPointer, index));
        }
      }
      collectReferences(child, context, value, pointer(currentPointer, index));
    }
    return;
  }
  if (!value || typeof value !== "object") return;

  for (const [key, child] of Object.entries(value)) {
    const childPointer = pointer(currentPointer, key);
    if (key === "path") {
      const record = addReference({
        value: child,
        parent: value,
        sourceFile: context.sourceFile,
        root: context.root,
        source: context.source,
        studyIndex: context.studyIndex,
        metadataIndex: context.metadataIndex,
        pointerValue: childPointer,
        referenceRows: context.referenceRows,
        issues: context.issues,
      });
      context.referenceRecords.push(record);
    } else if (PATH_STRING_KEYS.has(key) && typeof child === "string" && SCRIPT_FILE_PATTERN.test(child)) {
      const record = addReference({
        value: child,
        parent: null,
        sourceFile: context.sourceFile,
        root: context.root,
        source: context.source,
        studyIndex: context.studyIndex,
        metadataIndex: context.metadataIndex,
        pointerValue: childPointer,
        referenceRows: context.referenceRows,
        issues: context.issues,
      });
      context.referenceRecords.push(record);
    }
    collectReferences(child, context, value, childPointer);
  }
}

function directValues(value, keys) {
  const result = [];
  if (!value || typeof value !== "object" || Array.isArray(value)) return result;
  for (const key of keys) {
    const child = value[key];
    if (typeof child === "string") result.push(child);
    else if (Array.isArray(child)) result.push(...child.filter((item) => typeof item === "string"));
  }
  return result;
}

function fileRecord(root, file, kind) {
  return { path: relativePath(root, file), kind, digest: digestFile(file) };
}

function loadJson(file) {
  try {
    return { value: JSON.parse(fs.readFileSync(file, "utf8")), error: null };
  } catch (error) {
    return { value: null, error: error instanceof Error ? error.message : "invalid JSON" };
  }
}

function studyId(entry) {
  return entry.id ?? entry.directory;
}

function dependencyReference(record) {
  const key = record.pointer.split(/[.\[\]]/u).filter(Boolean);
  return key.some((part) => DEPENDENCY_KEYS.has(part));
}

function findCycles(entries, edges) {
  const adjacency = new Map(entries.map((_, index) => [index, new Set()]));
  for (const edge of edges) adjacency.get(edge.from)?.add(edge.to);
  const cycles = [];
  const seenCycles = new Set();
  const visit = (node, stack, active) => {
    active.add(node);
    stack.push(node);
    for (const next of [...(adjacency.get(node) ?? [])].sort((left, right) => left - right)) {
      if (active.has(next)) {
        const start = stack.indexOf(next);
        const core = stack.slice(start);
        const rotations = core.map((_, index) => [...core.slice(index), ...core.slice(0, index)]);
        const normalizedCore = rotations.sort((left, right) => left.join("\0").localeCompare(right.join("\0")))[0];
        const key = normalizedCore.join("\0");
        if (!seenCycles.has(key)) {
          seenCycles.add(key);
          cycles.push([...normalizedCore, normalizedCore[0]].map((index) => studyId(entries[index])));
        }
      } else if (!stack.includes(next)) {
        visit(next, stack, active);
      }
    }
    stack.pop();
    active.delete(node);
  };
  for (let index = 0; index < entries.length; index += 1) visit(index, [], new Set());
  return cycles.sort((left, right) => JSON.stringify(left).localeCompare(JSON.stringify(right)));
}

function commandTargets(command) {
  if (typeof command !== "string") return [];
  const tokens = command.match(/(?:"[^"]+"|'[^']+'|[^\s]+)/gu) ?? [];
  return tokens
    .map((token) => token.replace(/^["']|["']$/gu, "").replace(/[;,]$/u, ""))
    .filter((token) => SCRIPT_FILE_PATTERN.test(token) || token.includes("studies/") || token.includes("studies\\"))
    .sort(comparePath);
}

function scriptRelated(entry, name, command) {
  const value = `${name} ${command}`.toLowerCase();
  const directory = entry.directory.toLowerCase();
  const shortDirectory = directory.replace(/^tooling\/studies\//u, "");
  const id = (entry.id ?? "").toLowerCase();
  if (value.includes(directory) || value.includes(`studies/${shortDirectory}`) || value.includes(`studies\\${shortDirectory}`)) return "direct-study-path";
  if (value.includes(shortDirectory) || (id && value.includes(id))) return "study-id-or-name";
  if (value.includes("studies") && (value.includes("*") || value.includes("check:studies") || value.includes("parse:studies"))) return "study-glob";
  if (name.toLowerCase().includes("study") && value.includes("study")) return "study-tooling";
  return null;
}

function collectEntrypoints(root, entries, issues) {
  const packages = [
    { scope: "root", file: path.join(root, "package.json") },
    { scope: "tree-sitter", file: path.join(root, "tooling", "tree-sitter-w", "package.json") },
  ];
  const rows = [];
  const scriptMap = new Map();
  const addScript = (file, kind) => {
    const scriptPath = relativePath(root, file);
    if (!scriptMap.has(scriptPath)) scriptMap.set(scriptPath, fileRecord(root, file, kind));
    return scriptPath;
  };
  for (const packageInfo of packages) {
    if (!fs.existsSync(packageInfo.file) || !fs.statSync(packageInfo.file).isFile()) {
      addIssue(issues.missing, { source: relativePath(root, packageInfo.file), pointer: "scripts", path: relativePath(root, packageInfo.file), reason: "missing-script-package" });
      continue;
    }
    const packagePath = addScript(packageInfo.file, "script-package");
    const loaded = loadJson(packageInfo.file);
    if (loaded.error) {
      addIssue(issues.invalidJson, { source: packagePath, pointer: "", path: packagePath, reason: "invalid-json", message: loaded.error });
      continue;
    }
    for (const [name, command] of Object.entries(loaded.value.scripts ?? {}).sort(([left], [right]) => left.localeCompare(right))) {
      const targets = commandTargets(command);
      const applicable = entries
        .map((entry, studyIndex) => ({ studyIndex, reason: scriptRelated(entry, name, command) }))
        .filter(({ reason }) => reason);
      if (applicable.length === 0) continue;
      const targetPaths = [];
      const patterns = [];
      for (const target of targets) {
        if (target.includes("*") || target.includes("?")) {
          patterns.push(target);
          continue;
        }
        const resolved = resolveReference(root, packageInfo.file, target);
        if (resolved.resolved) {
          const targetPath = addScript(resolved.resolved, "script-target");
          targetPaths.push(targetPath);
        } else {
          addIssue(issues.missing, {
            source: packagePath,
            pointer: `scripts.${name}`,
            path: target,
            resolved: null,
            reason: resolved.status,
          });
        }
      }
      rows.push({
        scope: packageInfo.scope,
        scriptPath: packagePath,
        name,
        command,
        targetPaths,
        patterns,
        studies: [...new Set(applicable.map(({ studyIndex }) => studyIndex))].sort((left, right) => left - right),
        appliesTo: applicable
          .map(({ studyIndex, reason }) => ({ study: studyIndex, reason }))
          .sort((left, right) => left.study - right.study || left.reason.localeCompare(right.reason)),
      });
    }
  }
  const scripts = [...scriptMap.values()].sort((left, right) => comparePath(left.path, right.path));
  const scriptIndices = new Map(scripts.map((record, index) => [record.path, index]));
  const entrypoints = rows
    .sort((left, right) => `${left.scope}\0${left.name}`.localeCompare(`${right.scope}\0${right.name}`))
    .map((row, index) => {
      for (const studyIndex of row.studies) entries[studyIndex].entrypoints.push(index);
      return {
        scope: row.scope,
        script: scriptIndices.get(row.scriptPath),
        name: row.name,
        command: row.command,
        targets: row.targetPaths.map((target) => scriptIndices.get(target)).filter((target) => target !== undefined),
        patterns: row.patterns,
        studies: row.studies,
        appliesTo: row.appliesTo,
      };
    });
  return { scripts, entrypoints };
}

function issueState(issues) {
  return {
    missing: issues.missing.sort((left, right) => JSON.stringify(left).localeCompare(JSON.stringify(right))),
    stale: issues.stale.sort((left, right) => JSON.stringify(left).localeCompare(JSON.stringify(right))),
    duplicates: issues.duplicates.sort((left, right) => JSON.stringify(left).localeCompare(JSON.stringify(right))),
    invalidJson: issues.invalidJson.sort((left, right) => JSON.stringify(left).localeCompare(JSON.stringify(right))),
  };
}

export function buildStudyRegistry({ root = repositoryRoot } = {}) {
  const resolvedRoot = path.resolve(root);
  const studiesDirectory = path.join(resolvedRoot, "tooling", "studies");
  const issues = { missing: [], stale: [], duplicates: [], invalidJson: [] };
  const referenceRows = [];
  const entries = [];
  const metadata = [];
  const fixtures = [];
  const primaryIds = new Map();
  const directoryEntries = fs.existsSync(studiesDirectory) && fs.statSync(studiesDirectory).isDirectory()
    ? fs.readdirSync(studiesDirectory, { withFileTypes: true }).filter((entry) => entry.isDirectory()).map((entry) => entry.name).sort(comparePath)
    : [];

  for (const directoryName of directoryEntries) {
    const studyIndex = entries.length;
    const studyDirectory = path.join(studiesDirectory, directoryName);
    const files = walkFiles(studyDirectory);
    const metadataPaths = files.filter((file) => metadataFile(path.basename(file)));
    const parsed = [];
    const entry = {
      directory: `tooling/studies/${directoryName}`,
      id: null,
      status: "metadata-missing",
      ids: [],
      gates: [],
      metadata: [],
      fixtures: [],
      references: [],
      entrypoints: [],
      dependencies: { inbound: [], outbound: [] },
    };

    for (const file of metadataPaths) {
      const descriptor = fileRecord(resolvedRoot, file, metadataKind(file));
      const loaded = loadJson(file);
      const metadataRecord = {
        ...descriptor,
        status: loaded.error ? "invalid-json" : loaded.value?.status ?? null,
        id: loaded.value?.id ?? null,
      };
      const metadataIndex = metadata.length;
      metadata.push(metadataRecord);
      entry.metadata.push(metadataIndex);
      if (loaded.error) {
        addIssue(issues.invalidJson, { source: descriptor.path, pointer: "", path: descriptor.path, reason: "invalid-json", message: loaded.error });
        continue;
      }
      parsed.push({ file, value: loaded.value, metadataIndex });
      if (typeof loaded.value?.id === "string") entry.ids.push(loaded.value.id);
      entry.ids.push(...directValues(loaded.value, ["decision", "decisions"]));
      entry.gates.push(...directValues(loaded.value, ["gate", "gates"]));
    }

    const primary = parsed.find(({ file }) => path.basename(file) === "study.json") ??
      parsed.find(({ file }) => path.basename(file) === "bundle.json") ??
      parsed.find(({ file }) => path.basename(file) === "manifest.json") ??
      parsed[0];
    entry.id = typeof primary?.value?.id === "string" ? primary.value.id : null;
    entry.status = typeof primary?.value?.status === "string" ? primary.value.status : "metadata-missing";
    if (entry.id === null) {
      addIssue(issues.missing, { source: entry.directory, pointer: "id", path: entry.directory, reason: "missing-study-id" });
    } else if (primaryIds.has(entry.id)) {
      addIssue(issues.duplicates, { source: entry.directory, pointer: "id", path: entry.id, previous: primaryIds.get(entry.id), reason: "duplicate-study-id" });
    } else {
      primaryIds.set(entry.id, entry.directory);
    }
    entry.ids = [...new Set(entry.ids.filter((value) => typeof value === "string"))].sort();
    entry.gates = [...new Set(entry.gates.filter((value) => typeof value === "string"))].sort();

    for (const file of files) {
      if (metadataFile(path.basename(file))) continue;
      const fixturePath = relativePath(resolvedRoot, file);
      const fixtureIndex = fixtures.length;
      fixtures.push(fixturePath);
      entry.fixtures.push(fixtureIndex);
    }

    for (const { file, metadataIndex, value } of parsed) {
      const context = {
        root: resolvedRoot,
        sourceFile: file,
        source: relativePath(resolvedRoot, file),
        studyDirectory,
        studyIndex,
        metadataIndex,
        referenceRows,
        referenceRecords: [],
        issues,
      };
      collectReferences(value, context);
      entry.references.push(...context.referenceRecords);
    }
    entries.push(entry);
  }

  referenceRows.sort((left, right) => `${left.source}\0${left.pointer}`.localeCompare(`${right.source}\0${right.pointer}`));
  const referenceIndices = new Map(referenceRows.map((record, index) => [record.referenceOrder, index]));
  const references = referenceRows.map(({ referenceOrder: order, source, ...record }) => ({ ...record, source }));
  for (const entry of entries) {
    entry.references = entry.references.map((record) => referenceIndices.get(record.referenceOrder)).sort((left, right) => left - right);
    entry.metadata.sort((left, right) => left - right);
    entry.fixtures.sort((left, right) => left - right);
  }
  const byDirectory = new Map(entries.map((entry, index) => [entry.directory, index]));
  const edges = [];
  const edgeKeys = new Set();
  for (const [referenceIndex, record] of references.entries()) {
    if (!dependencyReference(record)) continue;
    const sourceDirectory = entries[record.study]?.directory;
    const targetMatch = record.resolved?.match(/^tooling\/studies\/([^/]+)(?:\/|$)/u);
    const targetDirectory = targetMatch ? `tooling/studies/${targetMatch[1]}` : null;
    const from = sourceDirectory ? byDirectory.get(sourceDirectory) : undefined;
    const to = targetDirectory ? byDirectory.get(targetDirectory) : undefined;
    if (from === undefined || to === undefined) continue;
    const key = `${from}\0${to}\0${referenceIndex}`;
    if (edgeKeys.has(key)) continue;
    edgeKeys.add(key);
    edges.push({ from, to, reference: referenceIndex, pointer: record.pointer, status: record.status });
  }
  edges.sort((left, right) => left.from - right.from || left.to - right.to || left.reference - right.reference);
  for (const [edgeIndex, edge] of edges.entries()) {
    entries[edge.from].dependencies.outbound.push(edgeIndex);
    entries[edge.to].dependencies.inbound.push(edgeIndex);
  }
  const indegree = new Map(entries.map((_, index) => [index, 0]));
  const outdegree = new Map(entries.map((_, index) => [index, 0]));
  for (const edge of edges) {
    indegree.set(edge.to, (indegree.get(edge.to) ?? 0) + 1);
    outdegree.set(edge.from, (outdegree.get(edge.from) ?? 0) + 1);
  }
  const roots = entries.filter((_, index) => indegree.get(index) === 0).map(studyId).sort();
  const leaves = entries.filter((_, index) => outdegree.get(index) === 0).map(studyId).sort();
  const cycles = findCycles(entries, edges);
  const entrypointResult = collectEntrypoints(resolvedRoot, entries, issues);
  const allIssues = issueState(issues);
  const generator = path.join(resolvedRoot, "tooling", "study-registry.mjs");
  const source = {
    studiesDirectory: "tooling/studies",
    metadataNames: [...METADATA_NAMES].sort(),
    generator: fs.existsSync(generator) ? fileRecord(resolvedRoot, generator, "generator") : null,
    excludes: ["**/node_modules/**"],
  };
  const valid = allIssues.missing.length === 0 && allIssues.stale.length === 0 && allIssues.duplicates.length === 0 && allIssues.invalidJson.length === 0 && cycles.length === 0;
  const registry = {
    $schema: STUDY_REGISTRY_SCHEMA,
    status: "generated-projection",
    benchmarkDisposition: "not-applicable",
    benchmarkReason: "Registry maintenance and path integrity only.",
    source,
    counts: {
      studyDirectories: entries.length,
      metadataFiles: metadata.length,
      fixtureFiles: fixtures.length,
      references: references.length,
      digestReferences: references.filter((record) => record.declaredDigest !== null).length,
      pathOnlyReferences: references.filter((record) => record.declaredDigest === null).length,
      dependencyEdges: edges.length,
      entrypoints: entrypointResult.entrypoints.length,
      scripts: entrypointResult.scripts.length,
      roots: roots.length,
      leaves: leaves.length,
      missing: allIssues.missing.length,
      stale: allIssues.stale.length,
      duplicates: allIssues.duplicates.length,
      invalidJson: allIssues.invalidJson.length,
      cycles: cycles.length,
    },
    metadata,
    fixtures,
    scripts: entrypointResult.scripts,
    studies: entries,
    references,
    entrypoints: entrypointResult.entrypoints,
    graph: {
      edges,
      roots,
      leaves,
      rootDirectories: entries.filter((_, index) => indegree.get(index) === 0).map((entry) => entry.directory).sort(),
      leafDirectories: entries.filter((_, index) => outdegree.get(index) === 0).map((entry) => entry.directory).sort(),
      cycles,
    },
    missing: allIssues.missing,
    stale: allIssues.stale,
    duplicates: allIssues.duplicates,
    invalidJson: allIssues.invalidJson,
    integrity: {
      valid,
      counts: {
        missing: allIssues.missing.length,
        stale: allIssues.stale.length,
        duplicates: allIssues.duplicates.length,
        invalidJson: allIssues.invalidJson.length,
        cycles: cycles.length,
      },
    },
  };
  return { registry, issues: { ...allIssues, cycles }, errors: registryErrors(registry) };
}

function registryErrors(registry) {
  const errors = [];
  for (const issue of registry.missing) errors.push(`${issue.source}${issue.pointer ? `.${issue.pointer}` : ""} references a missing or invalid path.`);
  for (const issue of registry.stale) errors.push(`${issue.source}.${issue.pointer} digest is stale.`);
  for (const issue of registry.duplicates) errors.push(`${issue.source}.${issue.pointer} duplicates a registry reference or study ID.`);
  for (const issue of registry.invalidJson) errors.push(`${issue.source} is invalid JSON.`);
  for (const cycle of registry.graph.cycles) errors.push(`study dependency cycle: ${cycle.join(" -> ")}.`);
  return errors;
}

export function serializeStudyRegistry(registry) {
  return `${JSON.stringify(registry, null, 2)}\n`;
}

export function validateStudyRegistry(registry, { root = repositoryRoot, expected = null } = {}) {
  const expectedResult = expected ? { registry: expected, errors: [] } : buildStudyRegistry({ root });
  const errors = [...expectedResult.errors];
  if (serializeStudyRegistry(registry) !== serializeStudyRegistry(expectedResult.registry)) {
    errors.push("study-registry.json is stale. Run bun tooling/study-registry.mjs --write.");
  }
  return { errors, expected: expectedResult.registry };
}

export function checkStudyRegistry({ root = repositoryRoot, file = path.join(path.resolve(root), "tooling", "study-registry.json") } = {}) {
  const generated = buildStudyRegistry({ root });
  const errors = [...generated.errors];
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push("tooling/study-registry.json is missing. Run bun tooling/study-registry.mjs --write.");
    return { errors, registry: generated.registry };
  }
  const loaded = loadJson(file);
  if (loaded.error) {
    errors.push(`tooling/study-registry.json is invalid JSON: ${loaded.error}.`);
  } else {
    errors.push(...validateStudyRegistry(loaded.value, { root, expected: generated.registry }).errors.filter((error) => !errors.includes(error)));
  }
  return { errors, registry: generated.registry };
}

export function writeStudyRegistry({ root = repositoryRoot, file = path.join(path.resolve(root), "tooling", "study-registry.json") } = {}) {
  const generated = buildStudyRegistry({ root });
  if (generated.errors.length > 0) return { ...generated, written: false };
  fs.writeFileSync(file, serializeStudyRegistry(generated.registry), "utf8");
  return { ...generated, written: true };
}

function main(argv = process.argv.slice(2)) {
  const write = argv.includes("--write");
  const check = argv.includes("--check");
  if (write === check || argv.some((argument) => !["--write", "--check"].includes(argument))) {
    process.stderr.write("Usage: bun tooling/study-registry.mjs --write|--check\n");
    process.exitCode = 2;
    return;
  }
  if (write) {
    const result = writeStudyRegistry();
    if (result.errors.length > 0) {
      process.stderr.write(`${result.errors.join("\n")}\n`);
      process.exitCode = 1;
      return;
    }
    process.stdout.write(`Study registry written: ${result.registry.counts.studyDirectories} directories, ${result.registry.counts.references} references.\n`);
    return;
  }
  const result = checkStudyRegistry();
  if (result.errors.length > 0) {
    process.stderr.write(`${result.errors.join("\n")}\n`);
    process.exitCode = 1;
    return;
  }
  process.stdout.write(`Study registry valid: ${result.registry.counts.studyDirectories} directories, ${result.registry.counts.references} references.\n`);
}

if (import.meta.main) main();
