import crypto from "node:crypto";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawnSync } from "node:child_process";

export const HEX_DIGEST = /^sha256:[0-9a-f]{64}$/;
export const PHASES = [
  "declared-input",
  "tool-action",
  "staged-output",
  "parse",
  "name",
  "type",
  "ownership",
  "effect",
  "constir",
  "generated-interface-diff",
  "freeze",
  "publish-module-interface",
  "consumer",
];
export const ACTION_EVENTS = ["declared-input", "tool-start", "tool-stage", "tool-write", "tool-finish"];
const FAILURE_EVENTS = new Map([["tool-error", "error"], ["tool-cancel", "cancel"], ["tool-quota", "quota"], ["tool-panic", "panic"]]);
const LEGACY_KEYS = new Set([
  "routeOverride", "statusFlag", "selectedResult", "oracleResult", "expectedResult", "chooseResult",
  "failure", "events", "sourceUnit", "inventory", "syntaxValid", "sourceUtf8Valid",
  "nondeterministicOrder", "publicEffects", "publicOwnership", "declarationCollision", "conflictingSymbol",
  "hiddenDependency", "explicitProductTargetInput", "targetEquivalent", "moduleCodeDigest", "importsGenerated",
  "importsConsumer", "ambientHostTarget", "ambientPath", "ambientEnv", "environment", "network", "clock",
  "time", "random", "undeclaredFs", "runAfter",
]);
const ASSERTION_KEYS = new Set([
  "status", "route", "code", "actionResultPublished", "interfacePublished", "compilerCachePublished", "requiredCompilerPublication", "stagingCleanupCount", "cleanupCount", "drainCount",
  "interfaceChanged", "sourceMapFixable", "targetEquivalent", "targetInterfaceChanged", "observedTrace", "requiredPhaseTrace",
  "discardReason", "variantSetDigest", "actionRecipeKeys",
]);
const D_INTENTS = new Set(["proc-macro", "annotation", "decorator", "metaclass", "eval", "ast-mutation", "textual-ast", "current-module-injection"]);
const READ_ONLY_MODE = "read-only";
const PARSE_CACHE = new Map();
const EXPECTED_SOURCE_REFS = new Map([
  ["last-light-reflection", ["reference/last-light/reflection.w", "export struct ReservationKey:", "closed Hashable and Reflectable synthesis"]],
  ["last-light-data", ["reference/last-light/data_formats.w", "export struct TabularTelemetryRow:", "closed data.Row schema synthesis"]],
  ["last-light-kernels", ["reference/last-light/ai_harness.w", "export const lastLightKernels", "finite compiler-owned kernel synthesis"]],
  ["last-light-menu-transform", ["reference/last-light/packages/menu-compiler/transform.w", "async fn transform(ctx: build.Context)", "hermetic typed transform"]],
  ["last-light-menu-compiler", ["reference/last-light/packages/menu-compiler/compiler.w", "export fn compileMenu(source: ref String)", "menu parser/compiler source"]],
  ["last-light-package", ["reference/last-light/build.w", "name: \"compile-final-menu\"", "build action and target separation"]],
  ["last-light-final-menu", ["reference/last-light/menus/final.menu", "ingredient horizon-fruit", "editable final.menu source and provenance root"]],
]);

export function digestBytes(value) { return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`; }
export function digestFile(file) { return digestBytes(fs.readFileSync(file)); }
function canonicalize(value) {
  if (Array.isArray(value)) return `[${value.map(canonicalize).join(",")}]`;
  if (value && typeof value === "object") return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonicalize(value[key])}`).join(",")}}`;
  return JSON.stringify(value);
}
export function digestValue(value) { return digestBytes(canonicalize(value)); }
function hasOwn(value, key) { return value && typeof value === "object" && Object.prototype.hasOwnProperty.call(value, key); }
function requireString(value, location, errors) { if (typeof value !== "string" || value.trim() === "") { errors.push(`${location} must be a non-empty string.`); return false; } return true; }
function requireArray(value, location, errors) { if (!Array.isArray(value)) { errors.push(`${location} must be an array.`); return false; } return true; }
function pathInside(root, relative, location, errors) {
  if (!requireString(relative, location, errors)) return undefined;
  const file = path.resolve(root, relative);
  const rel = path.relative(root, file);
  if (!rel || rel.startsWith(`..${path.sep}`) || path.isAbsolute(rel)) { errors.push(`${location} must stay inside the repository.`); return undefined; }
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) { errors.push(`${location} references a missing file.`); return undefined; }
  return file;
}
function checkDigest(file, expected, location, errors) {
  if (!HEX_DIGEST.test(expected ?? "")) { errors.push(`${location} must use a real lowercase sha256 digest.`); return false; }
  const actual = digestFile(file);
  if (actual !== expected) errors.push(`${location} is stale; expected ${actual}.`);
  return actual === expected;
}
function walkLegacy(value, location, errors) {
  if (!value || typeof value !== "object") return;
  if (Array.isArray(value)) { value.forEach((item, index) => walkLegacy(item, `${location}[${index}]`, errors)); return; }
  for (const [key, child] of Object.entries(value)) { if (LEGACY_KEYS.has(key)) errors.push(`${location}.${key} is a legacy caller-result field.`); walkLegacy(child, `${location}.${key}`, errors); }
}
function walkDigestFields(value, location, errors) {
  if (!value || typeof value !== "object") return;
  if (Array.isArray(value)) { value.forEach((item, index) => walkDigestFields(item, `${location}[${index}]`, errors)); return; }
  for (const [key, child] of Object.entries(value)) {
    if ((key === "digest" || key.endsWith("Digest") || key === "toolArtifact" || key === "sourceRef") && typeof child === "string" && !HEX_DIGEST.test(child)) errors.push(`${location}.${key} must use a real lowercase sha256 digest.`);
    walkDigestFields(child, `${location}.${key}`, errors);
  }
}
function occurrenceCount(text, needle) { return needle ? text.split(needle).length - 1 : 0; }
function checkSourceRefs(corpus, root, errors) {
  if (!requireArray(corpus.sourceRefs, "sourceRefs", errors)) return;
  if (corpus.sourceRefs.length !== EXPECTED_SOURCE_REFS.size) errors.push("sourceRefs must equal the closed SYN1 Last Light evidence set.");
  const seenIds = new Set();
  const seenSymbols = new Set();
  for (const [index, reference] of corpus.sourceRefs.entries()) {
    const location = `sourceRefs[${index}]`;
    if (!requireString(reference?.id, `${location}.id`, errors) || seenIds.has(reference.id)) errors.push(`${location}.id must be unique.`); else seenIds.add(reference.id);
    const expected = EXPECTED_SOURCE_REFS.get(reference?.id);
    if (!expected || JSON.stringify([reference?.path, reference?.symbol, reference?.claim]) !== JSON.stringify(expected)) errors.push(`${location} must match the exact SYN1 Last Light evidence reference.`);
    const file = pathInside(root, reference?.path, `${location}.path`, errors);
    requireString(reference?.symbol, `${location}.symbol`, errors); requireString(reference?.claim, `${location}.claim`, errors);
    if (file) {
      checkDigest(file, reference.digest, `${location}.digest`, errors);
      const text = fs.readFileSync(file, "utf8"); const count = occurrenceCount(text, reference.symbol);
      if (count !== 1) errors.push(`${location}.symbol must identify one source occurrence; found ${count}.`);
      const key = `${reference.path}\0${reference.symbol}`; if (seenSymbols.has(key)) errors.push(`${location} duplicates a source reference.`); else seenSymbols.add(key);
    }
  }
  for (const id of EXPECTED_SOURCE_REFS.keys()) if (!seenIds.has(id)) errors.push(`sourceRefs is missing ${id}.`);
}
function checkOfficialRefs(corpus, errors) {
  if (!requireArray(corpus.officialRefs, "officialRefs", errors)) return;
  const allowed = ["open-std.org", "doc.rust-lang.org", "docs.python.org", "bazel.build"]; const seen = new Set();
  for (const [index, reference] of corpus.officialRefs.entries()) {
    const location = `officialRefs[${index}]`; requireString(reference?.url, `${location}.url`, errors); requireString(reference?.claim, `${location}.claim`, errors);
    try {
      const url = new URL(reference.url); const normalized = `${url.origin}${url.pathname}`;
      if (url.protocol !== "https:" || !allowed.some((domain) => url.hostname === domain || url.hostname.endsWith(`.${domain}`))) errors.push(`${location}.url must point to an allowed primary source.`);
      if (seen.has(normalized)) errors.push(`${location}.url duplicates an official document after fragment normalization.`); else seen.add(normalized);
    } catch { errors.push(`${location}.url must be an absolute HTTPS URL.`); }
  }
}
function sourceFacts(input, root, errors = []) {
  const source = input.source ?? {}; const file = pathInside(root, source.path, "source.path", errors); const bytes = file ? fs.readFileSync(file) : Buffer.from(String(source.text ?? ""), "utf8"); const digest = digestBytes(bytes);
  if (source.digest !== digest) errors.push(`source.digest is forged or stale; expected ${digest}.`);
  const typed = source.typedInput ?? {}; const typedDigest = digestValue(typed.descriptor ?? {});
  if (typed.digest !== typedDigest) errors.push(`source.typedInput.digest is forged; expected ${typedDigest}.`);
  requireString(typed.binding, "source.typedInput.binding", errors); requireString(typed.schema, "source.typedInput.schema", errors);
  if (typed.schema !== source.schema) errors.push("source.typedInput.schema must match source.schema.");
  return { source, file, bytes, digest, schema: source.schema ?? "final.menu", typedInput: { binding: typed.binding, schema: typed.schema, digest: typedDigest }, editablePath: source.editablePath ?? source.path };
}
function checkActionEvents(actionEvents, location, errors) {
  if (!requireArray(actionEvents, location, errors)) return { kind: "invalid", actionEvents: [] };
  for (const [index, event] of actionEvents.entries()) if (!event || typeof event !== "object" || Array.isArray(event) || Object.keys(event).length !== 1 || typeof event.kind !== "string") errors.push(`${location}[${index}] must be the strict event object {kind}.`);
  const names = actionEvents.map((event) => event?.kind); const failureIndex = names.findIndex((name) => FAILURE_EVENTS.has(name));
  if (failureIndex >= 0) {
    const prefix = names.slice(0, failureIndex); const suffix = names.slice(failureIndex + 1);
    if (prefix.includes("tool-finish") || prefix.length < 2 || prefix.join(",") !== ACTION_EVENTS.slice(0, -1).slice(0, prefix.length).join(",") || suffix.join(",") !== "cleanup,drain,discard") errors.push(`${location} failure must occur before tool-finish and end with cleanup, drain, discard.`);
    if (names.filter((name) => FAILURE_EVENTS.has(name)).length !== 1) errors.push(`${location} has duplicate failure events.`);
    return { kind: FAILURE_EVENTS.get(names[failureIndex]), actionEvents };
  }
  if (names.join(",") !== ACTION_EVENTS.join(",")) errors.push(`${location} must contain only the ordered tool action events.`);
  if (new Set(names).size !== names.length) errors.push(`${location} cannot repeat an action event.`);
  return { kind: "success", actionEvents };
}
export function parseWFile(root, relativePath) {
  const absolute = path.resolve(root, relativePath); const cacheKey = `${relativePath}\0${fs.existsSync(absolute) ? digestFile(absolute) : "missing"}`;
  if (PARSE_CACHE.has(cacheKey)) return PARSE_CACHE.get(cacheKey);
  const executable = process.platform === "win32" ? path.join(root, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter.cmd") : path.join(root, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter");
  // Windows exposes the package launcher as a `.cmd` shim. Direct spawn of
  // that shim can return status null/EINVAL, so use the platform shell only
  // for this repository-owned launcher; POSIX keeps the direct binary.
  const parsed = spawnSync(executable, ["parse", "--grammar-path", "tooling/tree-sitter-w", "--quiet", "--stat", relativePath], {
    cwd: root,
    encoding: "utf8",
    shell: process.platform === "win32",
  });
  const text = `${parsed.stdout ?? ""}\n${parsed.stderr ?? ""}`;
  const result = { ok: parsed.status === 0 && !/\b(?:ERROR|MISSING)\b/.test(text), output: text }; PARSE_CACHE.set(cacheKey, result); return result;
}
function parseWBytes(root, bytes) {
  const temporaryRoot = fs.mkdtempSync(path.join(os.tmpdir(), "w-syn1-parse-"));
  const file = path.join(temporaryRoot, "generated.w");
  try {
    fs.writeFileSync(file, bytes);
    const executable = process.platform === "win32" ? path.join(root, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter.cmd") : path.join(root, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter");
    const parsed = spawnSync(executable, ["parse", "--grammar-path", path.join(root, "tooling", "tree-sitter-w"), "--quiet", "--stat", file], {
      cwd: root,
      encoding: "utf8",
      shell: process.platform === "win32",
    });
    const output = `${parsed.stdout ?? ""}\n${parsed.stderr ?? ""}`;
    return { ok: parsed.status === 0 && !/\b(?:ERROR|MISSING)\b/.test(output), output };
  } finally {
    fs.rmSync(temporaryRoot, { recursive: true, force: true });
  }
}
function strictBytes(bytes) { try { return { ok: true, text: new TextDecoder("utf-8", { fatal: true }).decode(bytes) }; } catch { return { ok: false, text: "" }; } }
function maskWTrivia(text) {
  const chars = [...text]; let index = 0;
  const blank = (start, end) => { for (let cursor = start; cursor < end; cursor += 1) if (chars[cursor] !== "\n" && chars[cursor] !== "\r") chars[cursor] = " "; };
  while (index < text.length) {
    if (text.startsWith("//", index)) { const end = text.indexOf("\n", index); blank(index, end < 0 ? text.length : end); index = end < 0 ? text.length : end; continue; }
    if (text.startsWith("/*", index)) { const end = text.indexOf("*/", index + 2); const finish = end < 0 ? text.length : end + 2; blank(index, finish); index = finish; continue; }
    const rawTriple = text.startsWith('#"""', index); const triple = text.startsWith('"""', index); const raw = text.startsWith('#"', index); const quoted = text[index] === '"' || (text[index] === "b" && text[index + 1] === '"');
    const scalar = text[index] === "'" || (text[index] === "b" && text[index + 1] === "'");
    if (rawTriple || triple || raw || quoted || scalar) {
      const start = index; const marker = rawTriple ? '"""#' : triple ? '"""' : raw ? '"#' : scalar ? "'" : '"'; index += rawTriple ? 4 : triple ? 3 : (text[index] === "b" ? 2 : raw ? 2 : 1);
      while (index < text.length) { if (!rawTriple && !triple && !raw && text[index] === "\\") { index += 2; continue; } if (text.startsWith(marker, index)) { index += marker.length; break; } index += 1; }
      blank(start, index); continue;
    }
    index += 1;
  }
  return chars.join("");
}
function checkTextEnvelope(bytes, text, errors, location) {
  if (bytes.length >= 3 && bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) errors.push(`${location} must not contain a UTF-8 BOM.`);
  if (text.includes("\r")) errors.push(`${location} must use LF line endings.`);
  if (!text.endsWith("\n")) errors.push(`${location} must end with one newline.`);
  for (const [index, line] of text.split("\n").entries()) { if (/[ \t]+$/.test(line)) errors.push(`${location} line ${index + 1} has trailing whitespace.`); if (Buffer.byteLength(line, "utf8") > 120) errors.push(`${location} line ${index + 1} exceeds 120 bytes.`); }
}
export function extractWSourceShape(text) {
  const masked = maskWTrivia(text); const depthAt = new Array(masked.length); let depth = 0;
  for (let index = 0; index < masked.length; index += 1) { depthAt[index] = depth; if (masked[index] === "{") depth += 1; else if (masked[index] === "}") depth -= 1; }
  const imports = []; const symbols = []; const rejectedTopLevel = []; let lineOffset = 0;
  let pendingHeader = false;
  for (const rawLine of masked.split("\n")) {
    const indentation = /^[ \t]*/.exec(rawLine)[0]; const body = rawLine.slice(indentation.length); const start = lineOffset + indentation.length; lineOffset += rawLine.length + 1; if (!body.trim() || depthAt[start] !== 0) continue;
    const line = body.trim();
    if (pendingHeader) { if (line.includes("{")) pendingHeader = false; continue; }
    const imported = /^(?:export\s+)?import\s+(.+?)\s+from\s+([A-Za-z_][A-Za-z0-9_.]*)\s*;?$/.exec(line) ?? /^import\s+([A-Za-z_][A-Za-z0-9_.]*)\s*;?$/.exec(line);
    if (imported) { imports.push(imported.length === 3 ? { name: imported[1].trim(), moduleIdentity: imported[2] } : { name: "*", moduleIdentity: imported[1] }); continue; }
    const declaration = /^(export\s+)?(?:(?:static|const|unsafe|mut|take|async)\s+)*(struct|enum|fn|const|object|type)\s+([A-Za-z_][A-Za-z0-9_]*)/.exec(line);
    if (declaration) {
      const conformance = ["struct", "object", "type"].includes(declaration[2]) ? /:\s*([^\{]+)\{/.exec(line)?.[1]?.trim() : undefined;
      symbols.push({ name: declaration[3], kind: declaration[2], visibility: declaration[1] ? "public" : "private", sourceSpan: { start, end: start + body.length }, sourceConformances: conformance ? conformance.split("&").map((item) => item.trim()) : [] });
      if (["fn", "struct", "enum", "object"].includes(declaration[2]) && !line.includes("{")) pendingHeader = true;
      continue;
    }
    rejectedTopLevel.push({ offset: start, text: line });
  }
  return { imports, symbols, rejectedTopLevel };
}
function normalizeReceiptSymbol(symbol) {
  return { path: symbol.path, name: symbol.name, kind: symbol.kind, visibility: symbol.visibility, type: symbol.type ?? "unit", genericParams: symbol.genericParams ?? [], staticParams: symbol.staticParams ?? [], effects: symbol.effects ?? [], throws: symbol.throws ?? [], ownership: symbol.ownership ?? "value", origin: symbol.origin ?? "generated", allocation: symbol.allocation ?? "value", constValue: symbol.constValue ?? null, conformances: symbol.conformances ?? [], resourceFacts: symbol.resourceFacts ?? [] };
}
function validateFrontendReceipt(receipt, moduleSet, shapes, errors) {
  if (!receipt || receipt.status !== "implementation-evidence-gap" || receipt.parser !== "tree-sitter-w" || receipt.compilerEvidence !== "missing") { errors.push("frontendReceipt must identify Tree-sitter source-shape evidence with compiler evidence missing."); return undefined; }
  if (!Array.isArray(receipt.symbols)) { errors.push("frontendReceipt.symbols must be an array."); return undefined; }
  const logicalManifest = canonicalBy(moduleSet.map(({ logicalPath, digest, byteLength }) => ({ logicalPath, digest, byteLength })), (item) => item.logicalPath);
  const moduleSetDigest = digestValue(logicalManifest);
  if (receipt.moduleSetDigest !== moduleSetDigest) errors.push("frontendReceipt.moduleSetDigest is stale or forged.");
  const receiptPayload = { moduleSetDigest: receipt.moduleSetDigest, symbols: receipt.symbols };
  if (receipt.digest !== digestValue(receiptPayload)) errors.push("frontendReceipt.digest is stale or forged.");
  const expected = shapes.flatMap((shape) => shape.symbols.map((symbol) => ({ ...symbol, path: shape.path })));
  const actual = receipt.symbols.map(normalizeReceiptSymbol); const expectedKeys = expected.map((symbol) => `${symbol.path}\0${symbol.name}\0${symbol.kind}\0${symbol.visibility}`); const actualKeys = actual.map((symbol) => `${symbol.path}\0${symbol.name}\0${symbol.kind}\0${symbol.visibility}`);
  if (new Set(actual.map((symbol) => symbol.name)).size !== actual.length || new Set(expected.map((symbol) => symbol.name)).size !== expected.length) { errors.push("generated module contains a cross-file top-level name collision."); return undefined; }
  if (new Set(actualKeys).size !== actualKeys.length || new Set(expectedKeys).size !== expectedKeys.length) { errors.push("generated module contains duplicate declarations or receipt symbols."); return undefined; }
  if (expectedKeys.length !== actualKeys.length || expectedKeys.some((key) => !actualKeys.includes(key))) { errors.push("frontendReceipt symbols must exactly match parsed W source shape without missing or extra declarations."); return undefined; }
  for (const symbol of actual) {
    const shaped = expected.find((candidate) => candidate.path === symbol.path && candidate.name === symbol.name && candidate.kind === symbol.kind && candidate.visibility === symbol.visibility);
    if (JSON.stringify(symbol.conformances) !== JSON.stringify(shaped?.sourceConformances ?? [])) errors.push(`frontendReceipt conformance facts do not match source shape for ${symbol.path}:${symbol.name}.`);
    if (symbol.visibility === "public" && symbol.effects.some((effect) => !["pure", "read-only"].includes(effect))) errors.push(`frontendReceipt contains an unsupported public effect for ${symbol.name}.`);
    if (symbol.visibility === "public" && !["value", "borrowed", "owned"].includes(symbol.ownership)) errors.push(`frontendReceipt contains unsupported public ownership for ${symbol.name}.`);
  }
  return actual;
}
function moduleSet(root, entries, errors, override) {
  if (!Array.isArray(entries) || entries.length < 1) { errors.push("output.moduleSet must contain one or more W source files."); return undefined; }
  const seen = new Set(); const files = [];
  for (const [index, entry] of entries.entries()) {
    const location = `output.moduleSet[${index}]`; const file = pathInside(root, entry?.fixturePath, `${location}.fixturePath`, errors);
    if (!file || path.extname(file) !== ".w") { errors.push(`${location}.fixturePath must reference a .w host fixture.`); continue; }
    if (typeof entry.logicalPath !== "string" || entry.logicalPath.includes("\\") || entry.logicalPath.startsWith("./") || entry.logicalPath.startsWith("/") || path.posix.normalize(entry.logicalPath) !== entry.logicalPath || !entry.logicalPath.endsWith(".w")) errors.push(`${location}.logicalPath must use canonical module-relative POSIX form.`);
    if (seen.has(entry.logicalPath)) errors.push(`${location}.logicalPath duplicates a module source.`); else seen.add(entry.logicalPath);
    const bytes = index === 0 && override !== undefined ? Buffer.from(override, "base64") : fs.readFileSync(file); const digest = digestBytes(bytes);
    if (entry.digest !== digest) errors.push(`${location}.digest is stale or forged; expected ${digest}.`);
    if (entry.byteLength !== bytes.length) errors.push(`${location}.byteLength is stale or forged; expected ${bytes.length}.`);
    const utf8 = strictBytes(bytes); if (!utf8.ok) { errors.push(`${location} is not valid UTF-8.`); continue; }
    checkTextEnvelope(bytes, utf8.text, errors, location);
    const parse = override !== undefined ? parseWBytes(root, bytes) : parseWFile(root, entry.fixturePath);
    files.push({ logicalPath: entry.logicalPath, fixturePath: entry.fixturePath, file, bytes, text: utf8.text, digest, byteLength: bytes.length, parse });
  }
  if (files.some((item) => !item.parse.ok)) errors.push("generated module source must parse with the current Tree-sitter W grammar without recovery.");
  return canonicalBy(files, (item) => item.logicalPath);
}
function checkDependencies(input, shapes, errors) {
  const receipts = canonicalBy(input.generator.dependencyReceipts, (item) => item.moduleIdentity);
  const imports = shapes.flatMap((shape) => shape.imports).filter((item) => item.moduleIdentity !== "std"); const depById = new Map(receipts.map((dep) => [dep.moduleIdentity, dep]));
  const stdImported = shapes.flatMap((shape) => shape.imports).some((item) => item.moduleIdentity === "std"); if (stdImported && !depById.has("std")) errors.push("standard-library import lacks a dependency receipt.");
  const names = new Set(); for (const imported of imports) { if (names.has(imported.moduleIdentity) || !depById.has(imported.moduleIdentity)) errors.push("generated imports must map exactly to unique declared dependency receipts."); names.add(imported.moduleIdentity); }
  const bindings = new Set();
  for (const imported of shapes.flatMap((shape) => shape.imports).filter((item) => item.name !== "*")) {
    if (bindings.has(imported.name)) errors.push("generated imports contain a module-scope binding collision.");
    bindings.add(imported.name);
  }
  for (const dep of receipts) if (dep.moduleIdentity !== "std" && !imports.some((item) => item.moduleIdentity === dep.moduleIdentity)) errors.push("dependency receipts must not contain an unused module.");
  const outputIdentity = input.generator.outputDescriptor.logicalModuleIdentity; const consumers = new Set(input.generator.actionGraphReceipt.nodes.filter((node) => node.kind === "consumer").map((node) => node.identity));
  if (shapes.flatMap((shape) => shape.imports).some((item) => item.moduleIdentity === outputIdentity || consumers.has(item.moduleIdentity))) errors.push("generated source cannot import itself or an action-graph consumer.");
  return { receipts };
}
function checkSourceMaps(input, files, facts, diagnostic) {
  const maps = input.output.sourceMaps ?? []; const byPath = new Map(maps.map((map) => [map.logicalPath, map])); const normalized = []; let invalid = false; let stale = false; const allCovering = [];
  if (maps.length !== files.length || byPath.size !== maps.length || files.some((file) => !byPath.has(file.logicalPath))) invalid = true;
  const byteBoundaries = (bytes) => { const boundaries = new Set([0]); for (let index = 0; index < bytes.length;) { const byte = bytes[index]; index += byte < 0x80 ? 1 : byte < 0xe0 ? 2 : byte < 0xf0 ? 3 : 4; boundaries.add(index); } return boundaries; };
  const sourceBoundaries = byteBoundaries(facts.bytes);
  for (const file of files) {
    const mappings = byPath.get(file.logicalPath)?.mappings ?? []; const seen = new Set(); const generated = []; const generatedBoundaries = byteBoundaries(file.bytes);
    for (const mapping of mappings) {
      const gs = mapping.generated; const ss = mapping.sourceSpan;
      if (!Number.isInteger(gs?.start) || !Number.isInteger(gs?.end) || gs.start < 0 || gs.end <= gs.start || gs.end > file.bytes.length || !generatedBoundaries.has(gs.start) || !generatedBoundaries.has(gs.end) || !Number.isInteger(ss?.start) || !Number.isInteger(ss?.end) || ss.start < 0 || ss.end <= ss.start || ss.end > facts.bytes.length || !sourceBoundaries.has(ss.start) || !sourceBoundaries.has(ss.end)) invalid = true;
      if (mapping.sourceBinding !== facts.typedInput.binding || mapping.sourceId !== `input:${facts.typedInput.binding}` || mapping.sourceRef !== facts.digest || mapping.logicalPath !== file.logicalPath) stale = true;
      const logical = { logicalPath: mapping.logicalPath, generated: mapping.generated, sourceBinding: mapping.sourceBinding, sourceId: mapping.sourceId, sourceRef: mapping.sourceRef, sourceSpan: mapping.sourceSpan };
      const key = canonicalize(logical); if (seen.has(key)) invalid = true; seen.add(key); generated.push(gs); normalized.push(logical);
    }
    const overlap = (spans) => spans.some((left, i) => spans.some((right, j) => i < j && left && right && left.start < right.end && right.start < left.end)); if (overlap(generated)) invalid = true;
    if (diagnostic?.logicalPath === file.logicalPath) {
      if (!Number.isInteger(diagnostic.generatedSpan?.start) || !Number.isInteger(diagnostic.generatedSpan?.end) || diagnostic.generatedSpan.start < 0 || diagnostic.generatedSpan.end <= diagnostic.generatedSpan.start || diagnostic.generatedSpan.end > file.bytes.length || !generatedBoundaries.has(diagnostic.generatedSpan.start) || !generatedBoundaries.has(diagnostic.generatedSpan.end)) invalid = true;
      allCovering.push(...mappings.filter((mapping) => diagnostic.generatedSpan && mapping.generated.start <= diagnostic.generatedSpan.start && mapping.generated.end >= diagnostic.generatedSpan.end).map((mapping) => ({ ...mapping, logicalPath: file.logicalPath })));
    }
  }
  if (diagnostic?.logicalPath && !files.some((file) => file.logicalPath === diagnostic.logicalPath)) invalid = true;
  normalized.sort((left, right) => canonicalize(left).localeCompare(canonicalize(right)));
  const exact = !invalid && !stale && allCovering.length === 1 && diagnostic?.generatedOnly !== true;
  return { key: digestValue(normalized), diagnosticMapKey: digestValue(normalized), invalid, stale, generatedOnly: normalized.length === 0 || diagnostic?.generatedOnly === true, fixable: exact, fix: exact ? { sourceBinding: allCovering[0].sourceBinding, sourceId: allCovering[0].sourceId, displayPath: facts.editablePath, span: allCovering[0].sourceSpan, logicalPath: allCovering[0].logicalPath } : undefined };
}
function rejectResult(code, reason, extra = {}, route = "intentionally-rejected") { return { status: "rejected", route, code, reason, actionResultPublished: false, interfacePublished: false, compilerCachePublished: false, staging: "not-published", stagingCleanupCount: 0, cleanupCount: 0, drainCount: 0, ...extra }; }
function candidateReject(code, reason, extra = {}) { return rejectResult(code, reason, extra, "historical-candidate"); }
function discardResult(kind, actionTrace) { const cleanupCount = actionTrace.actionEvents.filter((event) => event.kind === "cleanup").length; const staged = actionTrace.actionEvents.some((event) => event.kind === "tool-stage"); return { status: "discarded", route: "historical-candidate", code: `staging-discarded-${kind}`, discardReason: kind, reason: "action failure discards staging before an action-result exists", actionResultPublished: false, interfacePublished: false, compilerCachePublished: false, staging: staged ? "discarded" : "not-staged", stagingCleanupCount: staged ? cleanupCount : 0, cleanupCount, drainCount: actionTrace.actionEvents.filter((event) => event.kind === "drain").length, observedTrace: staged ? ["declared-input", "tool-action", "staged-output"] : ["declared-input", "tool-action"] }; }
function canonicalBy(items, key) { return [...items].sort((left, right) => key(left).localeCompare(key(right))); }
function actionIdentity(input, facts, targetFact = undefined) {
  const generator = input.generator;
  const typedInputs = canonicalBy(generator.typedInputs, (item) => `${item.binding}\0${item.schema}\0${item.digest}`);
  const dependencies = canonicalBy(generator.dependencyReceipts, (item) => item.moduleIdentity);
  return {
    toolArtifact: generator.toolArtifact,
    entry: generator.entry,
    toolProfile: generator.toolProfile,
    hostContract: generator.hostContract,
    executionPlatform: generator.executionPlatform,
    typedInputs,
    dependencies,
    outputDescriptor: generator.outputDescriptor,
    actionGraphDigest: generator.actionGraphReceipt.digest,
    productTargetFact: targetFact ?? null,
    authorityRequests: generator.authorityRequests,
    quotas: generator.quotas,
    version: generator.version,
  };
}
function validateDependencyInputs(input, errors) {
  const generator = input.generator; const deps = generator.dependencies ?? []; const receipts = generator.dependencyReceipts ?? [];
  const canonicalReceipts = canonicalBy(receipts, (item) => item.moduleIdentity); const canonicalDeps = canonicalBy(deps, (item) => item.moduleIdentity);
  if (JSON.stringify(canonicalDeps) !== JSON.stringify(canonicalReceipts)) errors.push("generator dependencies and dependencyReceipts must be the same canonical set.");
  const ids = new Set();
  for (const receipt of canonicalReceipts) {
    if (!requireString(receipt?.moduleIdentity, "dependency receipt moduleIdentity", errors) || ids.has(receipt.moduleIdentity)) errors.push("dependency receipt moduleIdentity must be unique."); ids.add(receipt?.moduleIdentity);
    if (!HEX_DIGEST.test(receipt?.semanticInterfaceKey ?? "")) errors.push("dependency receipt semanticInterfaceKey must be a real lowercase sha256 digest.");
    if (receipt?.digest !== digestValue({ moduleIdentity: receipt?.moduleIdentity, semanticInterfaceKey: receipt?.semanticInterfaceKey })) errors.push("dependency receipt digest is stale or forged.");
  }
  const expectedTyped = canonicalBy([{ binding: input.source.typedInput.binding, schema: input.source.typedInput.schema, digest: input.source.typedInput.digest }, ...canonicalReceipts.map((dep) => ({ binding: `dependency:${dep.moduleIdentity}`, schema: "module", digest: dep.digest }))], (item) => item.binding);
  if (JSON.stringify(canonicalBy(generator.typedInputs ?? [], (item) => item.binding)) !== JSON.stringify(expectedTyped)) errors.push("typedInputs must exactly cover source and dependency receipts.");
  const expectedHandles = canonicalBy([{ binding: input.source.typedInput.binding, mode: READ_ONLY_MODE, digest: input.source.digest }, ...canonicalReceipts.map((dep) => ({ binding: `dependency:${dep.moduleIdentity}`, mode: READ_ONLY_MODE, digest: dep.digest }))], (item) => item.binding);
  const observedHandles = canonicalBy((generator.readOnlyInputHandles ?? []).map(({ binding, mode, digest }) => ({ binding, mode, digest })), (item) => item.binding);
  if (JSON.stringify(observedHandles) !== JSON.stringify(expectedHandles)) errors.push("readOnlyInputHandles must exactly cover source and dependencies by binding/digest.");
  for (const handle of generator.readOnlyInputHandles ?? []) {
    if (typeof handle.physicalPath !== "string" || !handle.physicalPath) { errors.push("readOnlyInputHandles require a physicalPath for authority validation, outside action identity."); continue; }
    if (handle.binding === input.source.typedInput.binding && handle.physicalPath !== input.source.path) errors.push("source input handle must resolve to input.source.path.");
    if (handle.binding.startsWith("dependency:")) {
      const moduleIdentity = handle.binding.slice("dependency:".length);
      if (handle.physicalPath !== `module://${moduleIdentity}`) errors.push("dependency input handle must use its canonical module receipt URI.");
    }
  }
  return canonicalReceipts;
}
function validateActionGraph(generator, errors) {
  const receipt = generator.actionGraphReceipt; if (!receipt || !Array.isArray(receipt.nodes) || !Array.isArray(receipt.edges)) { errors.push("actionGraphReceipt must contain nodes and edges."); return; }
  const nodes = canonicalBy(receipt.nodes, (item) => `${item.kind}\0${item.identity}`); const edges = canonicalBy(receipt.edges, (item) => `${item.from}\0${item.to}\0${item.kind}`);
  if (receipt.digest !== digestValue({ nodes, edges, schedule: receipt.schedule })) errors.push("actionGraphReceipt.digest is stale or forged.");
  if (receipt.schedule !== "before-interface-freeze") errors.push("action graph schedule must be before-interface-freeze.");
  const allowedNodeKinds = new Set(["tool", "dependency", "generated-output", "consumer"]); const nodeIds = new Set(nodes.map((node) => node.identity));
  if (nodes.some((node) => !allowedNodeKinds.has(node.kind)) || nodeIds.size !== nodes.length) errors.push("action graph nodes must use unique identities and closed node kinds.");
  const edgeKeys = new Set();
  for (const edge of edges) {
    const key = `${edge.from}\0${edge.to}\0${edge.kind}`;
    if (edgeKeys.has(key)) errors.push("action graph edges must be unique."); edgeKeys.add(key);
    if (!nodeIds.has(edge.from) || !nodeIds.has(edge.to) || !["imports", "produces"].includes(edge.kind)) errors.push("action graph edges must reference known nodes and closed edge kinds.");
  }
  const tool = nodes.find((item) => item.kind === "tool"); const output = nodes.find((item) => item.kind === "generated-output"); const consumers = new Set(nodes.filter((item) => item.kind === "consumer").map((item) => item.identity));
  if (!tool || !output || tool.identity !== generator.toolArtifact || output.identity !== generator.outputDescriptor.logicalModuleIdentity) errors.push("action graph must bind the tool artifact and logical generated output.");
  if (consumers.size < 1) errors.push("action graph requires at least one declared consumer.");
  const dependencyNodes = nodes.filter((node) => node.kind === "dependency").map((node) => node.identity);
  const receiptDependencies = new Set(generator.dependencyReceipts.map((receipt) => receipt.moduleIdentity));
  if (dependencyNodes.length !== receiptDependencies.size || dependencyNodes.some((identity) => !receiptDependencies.has(identity))) errors.push("action graph dependency nodes must equal dependency receipts.");
  const produces = edges.filter((edge) => edge.kind === "produces");
  if (produces.length !== 1 || produces[0].from !== tool?.identity || produces[0].to !== output?.identity) errors.push("action graph requires exactly one tool produces generated-output edge.");
  for (const dependency of dependencyNodes) if (edges.filter((edge) => edge.kind === "imports" && edge.from === tool?.identity && edge.to === dependency).length !== 1) errors.push("tool must import every declared dependency exactly once.");
  for (const consumer of consumers) if (edges.filter((edge) => edge.kind === "imports" && edge.from === consumer && edge.to === output?.identity).length !== 1) errors.push("every declared consumer must import the generated output exactly once.");
  if (edges.some((edge) => edge.kind === "imports" && !((edge.from === tool?.identity && dependencyNodes.includes(edge.to)) || (consumers.has(edge.from) && edge.to === output?.identity)))) errors.push("action graph contains an unsupported import direction.");
  if (edges.some((edge) => edge.kind === "produces" && (edge.from !== tool?.identity || edge.to !== output?.identity))) errors.push("only the tool can produce the generated output.");
  const dependencyDirection = edges.map((edge) => edge.kind === "produces" ? { from: edge.to, to: edge.from } : edge);
  const visiting = new Set(); const visited = new Set();
  const cyclic = (node) => { if (visiting.has(node)) return true; if (visited.has(node)) return false; visiting.add(node); for (const edge of dependencyDirection.filter((item) => item.from === node)) if (cyclic(edge.to)) return true; visiting.delete(node); visited.add(node); return false; };
  if (nodes.some((node) => cyclic(node.identity))) errors.push("action graph dependency direction must be acyclic.");
}
function validateAction(input, facts) {
  const generator = input.generator; if (!generator || typeof generator !== "object") return "invalid-action";
  if (!HEX_DIGEST.test(generator.toolArtifact ?? "") || typeof generator.toolArtifactContent !== "string" || digestBytes(Buffer.from(generator.toolArtifactContent, "utf8")) !== generator.toolArtifact) return "invalid-tool-artifact";
  if (typeof generator.entry !== "string" || !generator.entry || typeof generator.executionPlatform !== "string" || !generator.executionPlatform) return "invalid-action";
  if (!Array.isArray(generator.capabilities) || !Array.isArray(generator.authorityRequests) || !Array.isArray(generator.dependencies) || !Array.isArray(generator.dependencyReceipts) || !Array.isArray(generator.typedInputs) || !Array.isArray(generator.readOnlyInputHandles) || typeof generator.version !== "string" || !generator.version) return "invalid-action";
  const quotas = generator.quotas; if (!quotas || !Number.isInteger(quotas.maximumOutputBytes) || quotas.maximumOutputBytes <= 0 || !Number.isInteger(quotas.maximumInputBytes) || quotas.maximumInputBytes <= 0) return "invalid-quotas";
  if (generator.capabilities.length !== 0 || generator.authorityRequests.length !== 0) return "ambient-authority";
  if (generator.toolProfile !== "bootstrap.w0" || generator.hostContract !== "w.host/build-transform@1") return "invalid-tool-profile";
  const descriptor = generator.outputDescriptor;
  if (!descriptor || descriptor.binding !== "generatedMenuModule" || descriptor.schema !== "generated-w-module-set" || descriptor.version !== 1 || descriptor.sourceProfile !== "syn1-generated-declarations@1" || !Number.isInteger(descriptor.maximumModuleBytes) || descriptor.maximumModuleBytes <= 0 || typeof descriptor.logicalModuleIdentity !== "string" || !descriptor.logicalModuleIdentity || descriptor.edition !== "2026" || !Array.isArray(descriptor.features) || descriptor.features.length !== 0) return "invalid-output-descriptor";
  if (generator.actionKeyFields?.includes("outputDigest") || generator.outputDigestInActionKey === true) return "output-in-action-key";
  return undefined;
}
function targetMetadata(input, errors) {
  const metadata = input.generator.targetMetadata; if (!metadata || !["uniform", "targetSpecific"].includes(metadata.interface) || !Array.isArray(metadata.targets) || metadata.targets.length < 2) { errors.push("generator.targetMetadata must declare uniform or targetSpecific interface and at least two targets."); return undefined; }
  const ids = metadata.targets.map((target) => target.id); if (new Set(ids).size !== ids.length || ids.some((id) => typeof id !== "string" || !id)) errors.push("targetMetadata target ids must be unique.");
  if (metadata.targets.some((target) => typeof target.physicalArtifact !== "string" || !target.physicalArtifact)) errors.push("targetMetadata targets must name physical artifacts.");
  const root = path.resolve(import.meta.dir, ".."); const registryPath = path.join(root, "tooling", "studies", "syn1-typed-generation", "target-registry.json"); const registry = JSON.parse(fs.readFileSync(registryPath, "utf8")); const registryDigest = digestFile(registryPath);
  const receipt = input.generator.targetRegistryReceipt;
  if (!receipt || receipt.digest !== registryDigest || receipt.revision !== registry.revision || receipt.schema !== registry.$schema) errors.push("targetRegistryReceipt must bind the durable SYN1 target registry fixture.");
  const registryById = new Map(registry.targets.map((target) => [target.id, target]));
  for (const target of metadata.targets) {
    const expected = registryById.get(target.id); if (!expected) { errors.push("targetMetadata names an unknown target registry id."); continue; }
    if (target.targetIdentity !== expected.targetIdentity || JSON.stringify(target.semanticFacts) !== JSON.stringify(expected.semanticFacts) || JSON.stringify(target.abiFacts) !== JSON.stringify(expected.abiFacts)) errors.push(`targetMetadata facts are forged for ${target.id}.`);
  }
  if (metadata.interface === "uniform" && metadata.targets.some((target) => hasOwn(target, "productFacts"))) errors.push("uniform target metadata cannot contain product semantic facts.");
  if (metadata.interface === "targetSpecific" && metadata.targets.some((target) => !target.semanticFacts || typeof target.semanticFacts !== "object")) errors.push("targetSpecific metadata must declare registry-backed semantic facts for every target.");
  return { ...metadata, registryDigest, registryRevision: registry.revision, targets: canonicalBy(metadata.targets, (target) => target.id) };
}
function validateActionResultContainer(input, metadata, errors, root) {
  const output = input.output ?? {}; const variants = metadata.interface === "uniform" ? [{ target: null, moduleSet: output.moduleSet }] : output.targetVariants;
  if (!Array.isArray(variants) || variants.length < 1) { errors.push("tool success must stage a module-set container or target variants."); return undefined; }
  const canonicalVariants = [];
  for (const variant of variants) {
    if (!Array.isArray(variant?.moduleSet) || variant.moduleSet.length < 1) { errors.push("every staged variant must inventory at least one generated source artifact."); continue; }
    const entries = canonicalBy(variant.moduleSet, (entry) => entry.logicalPath ?? "");
    const logicalPaths = new Set(); let totalBytes = 0;
    for (const [entryIndex, entry] of entries.entries()) {
      if (typeof entry.logicalPath !== "string" || !entry.logicalPath.endsWith(".w") || entry.logicalPath.startsWith("/") || entry.logicalPath.includes("\\") || path.posix.normalize(entry.logicalPath) !== entry.logicalPath) errors.push("moduleSet logicalPath must be canonical and module-relative.");
      if (logicalPaths.has(entry.logicalPath)) errors.push("moduleSet logicalPath must be unique."); logicalPaths.add(entry.logicalPath);
      if (!HEX_DIGEST.test(entry.digest ?? "") || !Number.isInteger(entry.byteLength) || entry.byteLength < 1) errors.push("moduleSet entries require digest and positive byteLength."); else totalBytes += entry.byteLength;
      const file = pathInside(root, entry.fixturePath, "moduleSet.fixturePath", errors); if (file) { const bytes = entryIndex === 0 && input.output.byteOverrideBase64 !== undefined && metadata.interface === "uniform" ? Buffer.from(input.output.byteOverrideBase64, "base64") : fs.readFileSync(file); if (entry.digest !== digestBytes(bytes) || entry.byteLength !== bytes.length) errors.push("action result inventory digest/length does not match staged bytes."); }
    }
    if (totalBytes > input.generator.outputDescriptor.maximumModuleBytes || totalBytes > input.generator.quotas.maximumOutputBytes) errors.push("staged module set exceeds output descriptor or action quota.");
    canonicalVariants.push({ target: variant.target ?? null, moduleSet: entries.map(({ logicalPath, digest, byteLength }) => ({ logicalPath, digest, byteLength })) });
  }
  if (errors.length) return undefined;
  canonicalVariants.sort((left, right) => String(left.target).localeCompare(String(right.target)));
  const actionResultKey = digestValue({ outputDescriptor: input.generator.outputDescriptor, variants: canonicalVariants });
  return { actionResultKey, canonicalVariants };
}
function observedFrontendTrace() { return ["declared-input", "tool-action", "staged-output", "parse"]; }
function deriveVariant(input, facts, moduleEntries, root, targetFact, metadata, diagnostic) {
  const errors = []; const files = moduleSet(root, moduleEntries, errors, input.output.byteOverrideBase64); if (!files || errors.length) return { errors };
  if (files.reduce((total, file) => total + file.bytes.length, 0) > input.generator.quotas.maximumOutputBytes) return { errors: ["generated module output exceeds the declared output quota."] };
  const shapes = files.map((file) => ({ path: file.logicalPath, ...extractWSourceShape(file.text) }));
  if (shapes.some((shape) => shape.rejectedTopLevel.length > 0)) errors.push("generated module contains a top-level form outside the bounded declaration profile.");
  const dependencyResult = checkDependencies(input, shapes, errors); if (errors.length) return { errors };
  const receipt = input.output.frontendReceipt; const symbols = validateFrontendReceipt(receipt, moduleEntries, shapes, errors); if (!symbols) return { errors };
  const symbolKeys = symbols.map((symbol) => `${symbol.path}\0${symbol.name}`);
  if (JSON.stringify(symbolKeys) !== JSON.stringify([...symbolKeys].sort())) errors.push("frontendReceipt symbols must use deterministic canonical ordering.");
  const sourceMaps = checkSourceMaps(input, files, facts, diagnostic); if (sourceMaps.invalid) errors.push("source map spans are invalid, overlapping, duplicate, or out of bounds."); if (sourceMaps.stale) errors.push("source map provenance is stale or points to another module."); if (errors.length) return { errors, sourceMaps };
  const publicSymbols = symbols.filter((symbol) => symbol.visibility === "public").map(({ path: _path, ...symbol }) => symbol); const descriptor = input.generator.outputDescriptor;
  const semanticInterfaceKey = digestValue({ logicalModuleIdentity: descriptor.logicalModuleIdentity, edition: descriptor.edition, features: descriptor.features, sourceProfile: descriptor.sourceProfile, interfacePolicy: metadata.interface, targetSemanticFacts: targetFact?.semanticFacts ?? null, symbols: publicSymbols });
  const moduleManifest = files.map((file) => ({ logicalPath: file.logicalPath, digest: file.digest, byteLength: file.byteLength })); const moduleCodeDigest = digestValue(moduleManifest); const sourceDescriptor = { logicalModuleIdentity: descriptor.logicalModuleIdentity, edition: descriptor.edition, features: descriptor.features, sourceProfile: descriptor.sourceProfile }; const moduleArtifactIdentity = digestValue({ sourceDescriptor, files: moduleManifest }); const action = actionIdentity(input, facts, targetFact ? { targetIdentity: targetFact.targetIdentity, semanticFacts: targetFact.semanticFacts, abiFacts: targetFact.abiFacts, registryDigest: metadata.registryDigest, registryRevision: metadata.registryRevision } : null); const actionRecipeKey = digestValue(action);
  return { target: targetFact?.id ?? null, physicalArtifacts: metadata.targets.map((target) => ({ target: target.id, physicalArtifact: target.physicalArtifact })), logicalModuleIdentity: descriptor.logicalModuleIdentity, moduleSet: moduleManifest, moduleArtifactIdentity, moduleCodeDigest, semanticInterfaceKey, diagnosticMapKey: sourceMaps.diagnosticMapKey, sourceMap: sourceMaps, actionRecipeKey, actionIdentity: action, dependencies: dependencyResult.receipts, frontendEvidence: { parser: "tree-sitter-w", sourceShape: "validated", compiler: "missing", nameResolver: "missing", typeChecker: "missing", ownershipChecker: "missing", effectChecker: "missing", constIrNormalizer: "missing", targetCompilerProvider: "missing", receipt: "implementation-evidence-gap" } };
}
function variantErrorCode(errors) {
  const text = errors.join(" ");
  if (text.includes("not valid UTF-8")) return "generated-utf8";
  if (text.includes("parse with the current Tree-sitter")) return "generated-syntax";
  if (text.includes("source map")) return "source-map-invalid";
  if (text.includes("output quota")) return "output-limit";
  if (text.includes("unsupported public effect")) return "unsupported-public-effect";
  if (text.includes("unsupported public ownership")) return "unsupported-public-ownership";
  if (text.includes("top-level form outside")) return "source-profile-rejected";
  if (text.includes("cross-file top-level name collision")) return "declaration-collision";
  if (text.includes("duplicate declarations")) return "declaration-collision";
  if (text.includes("canonical ordering")) return "nondeterministic-order";
  if (text.includes("frontendReceipt")) return "frontend-receipt-mismatch";
  if (text.includes("dependency") || text.includes("imports")) return "dependency-mismatch";
  return "invalid-generated-module";
}
function failedObservedTrace(errors) {
  return ["generated-utf8"].includes(variantErrorCode(errors))
    ? ["declared-input", "tool-action", "staged-output"]
    : observedFrontendTrace();
}
function deriveWAbiKey(semanticInterfaceKey, target, metadata) { return digestValue({ semanticInterfaceKey, targetIdentity: target.targetIdentity, abiFacts: target.abiFacts, targetRegistryDigest: metadata.registryDigest, targetRegistryRevision: metadata.registryRevision }); }
function resolveBaseline(input, corpus, root, currentCaseId) {
  const reference = input.baselineRef; if (!reference) return undefined;
  if (hasOwn(input, "baseline")) return { error: "raw baseline keys are forbidden; use baselineRef." };
  if (reference.caseId === currentCaseId) return { error: "baselineRef cannot reference itself." };
  const baselineCase = corpus.cases.find((item) => item.id === reference.caseId); if (!baselineCase || baselineCase.input?.intent !== "generated-module" || baselineCase.input?.baselineRef) return { error: "baselineRef must resolve to an accepted, acyclic root generated case." };
  const result = deriveGeneratedModule(baselineCase.input, corpus, root, baselineCase.id, true); if (result.status !== "accepted") return { error: "baselineRef target case is not accepted." };
  const variant = result.targetVariants.find((item) => item.target === (reference.target ?? result.targetVariants[0].target)); if (!variant) return { error: "baselineRef target projection is missing." };
  return { semanticInterfaceKey: variant.semanticInterfaceKey };
}
function deriveGeneratedModule(input, corpus, root, currentCaseId = undefined, resolvingBaseline = false) {
  const errors = []; const facts = sourceFacts(input, root, errors); const actionTrace = checkActionEvents(input.actionEvents, "input.actionEvents", errors); if (errors.length) return candidateReject("invalid-input", errors.join(" "), { validationErrors: errors });
  const actionCode = validateAction(input, facts); if (actionCode) return candidateReject(actionCode, "action identity must be complete, typed, hermetic, and content-addressed.");
  const metadata = targetMetadata(input, errors); if (errors.length) return candidateReject("invalid-target-metadata", errors.join(" "));
  validateDependencyInputs(input, errors); validateActionGraph(input.generator, errors); if (errors.length) return candidateReject("invalid-action-recipe", errors.join(" "));
  if (facts.bytes.length > input.generator.quotas.maximumInputBytes) return candidateReject("input-limit", "typed input exceeds the declared input quota.");
  if (actionTrace.kind !== "success") return discardResult(actionTrace.kind, actionTrace);
  const output = input.output ?? {}; if (output.targetVariants && metadata.interface === "uniform") return candidateReject("target-variants-on-uniform", "uniform targets derive one logical module and cannot carry target variants."); if (!output.targetVariants && metadata.interface === "targetSpecific") return candidateReject("target-variants-missing", "targetSpecific interface requires one source module set per target.");
  const actionResult = validateActionResultContainer(input, metadata, errors, root); if (!actionResult) return candidateReject("invalid-action-result-container", errors.join(" "));
  const actionResultState = { actionResultPublished: true, actionResultKey: actionResult.actionResultKey, interfacePublished: false, compilerCachePublished: false };
  if (!output.provenance || output.provenance.inputBinding !== facts.typedInput.binding || output.provenance.sourceRef !== facts.digest || output.provenance.toolArtifact !== input.generator.toolArtifact || output.provenance.generatorVersion !== input.generator.version) return candidateReject("provenance-mismatch", "generated output provenance must bind the logical input, source digest, tool artifact, and generator version.", actionResultState);
  if (output.provenance.generatorPlanDigest !== digestValue(output.provenance.generatorPlan ?? {})) return candidateReject("provenance-digest", "generator plan digest is forged.", actionResultState);
  const diagnostics = input.diagnostic ?? {};
  if (hasOwn(input, "diagnostic") && (diagnostics.provisionalCode !== "D0-implementation-evidence-gap" || typeof diagnostics.causalRoot !== "string" || !diagnostics.causalRoot || !Array.isArray(diagnostics.labels) || diagnostics.labels.length < 1 || diagnostics.labels.some((label) => label?.role !== "primary" || typeof label?.message !== "string" || !label.message))) return candidateReject("diagnostic-shape", "historical candidate diagnostics require a provisional implementation-evidence-gap code, one causal root, and explicit primary labels.", actionResultState);
  const variants = [];
  if (metadata.interface === "uniform") {
    const variant = deriveVariant(input, facts, output.moduleSet, root, undefined, metadata, diagnostics); if (variant.errors?.length) return candidateReject(variantErrorCode(variant.errors), variant.errors.join(" "), { ...actionResultState, sourceMap: variant.sourceMaps, observedTrace: failedObservedTrace(variant.errors), requiredPhaseTrace: PHASES }); variants.push(...metadata.targets.map((target) => ({ ...variant, target: target.id, physicalArtifact: target.physicalArtifact, wAbiKey: deriveWAbiKey(variant.semanticInterfaceKey, target, metadata) })));
  } else {
    if (output.targetVariants.length !== metadata.targets.length || new Set(output.targetVariants.map((variant) => variant.target)).size !== output.targetVariants.length) return candidateReject("target-variant-inventory", "targetSpecific output must contain exactly one variant per declared target.", actionResultState);
    for (const target of metadata.targets) { const targetVariant = output.targetVariants.find((candidate) => candidate.target === target.id); if (!targetVariant) return candidateReject("target-variant-missing", "targetSpecific output must provide every declared target.", actionResultState); const localInput = { ...input, output: { ...targetVariant, frontendReceipt: targetVariant.frontendReceipt, sourceMaps: targetVariant.sourceMaps, provenance: output.provenance }, diagnostic: targetVariant.diagnostic ?? diagnostics }; const variant = deriveVariant(localInput, facts, targetVariant.moduleSet, root, target, metadata, localInput.diagnostic); if (variant.errors?.length) return candidateReject(variantErrorCode(variant.errors), variant.errors.join(" "), { ...actionResultState, target: target.id, observedTrace: failedObservedTrace(variant.errors), requiredPhaseTrace: PHASES }); variants.push({ ...variant, target: target.id, physicalArtifact: target.physicalArtifact, wAbiKey: deriveWAbiKey(variant.semanticInterfaceKey, target, metadata) }); }
  }
  const baseline = resolvingBaseline ? undefined : resolveBaseline(input, corpus, root, currentCaseId); if (baseline?.error) return candidateReject("invalid-baseline-ref", baseline.error, actionResultState);
  const interfaceKey = variants[0].semanticInterfaceKey; const interfaceChanged = baseline ? interfaceKey !== baseline.semanticInterfaceKey : false; const targetEquivalent = variants.every((variant) => variant.semanticInterfaceKey === variants[0].semanticInterfaceKey && variant.moduleCodeDigest === variants[0].moduleCodeDigest && variant.diagnosticMapKey === variants[0].diagnosticMapKey); const targetInterfaceChanged = new Set(variants.map((variant) => variant.semanticInterfaceKey)).size > 1; const canonicalVariants = canonicalBy(variants, (variant) => variant.target); const variantSetDigest = digestValue(canonicalVariants.map((variant) => ({ target: variant.target, moduleSet: variant.moduleSet, moduleArtifactIdentity: variant.moduleArtifactIdentity, moduleCodeDigest: variant.moduleCodeDigest, semanticInterfaceKey: variant.semanticInterfaceKey, diagnosticMapKey: variant.diagnosticMapKey, wAbiKey: variant.wAbiKey, actionRecipeKey: variant.actionRecipeKey })));
  const explainNavigation = { logicalGeneratedModule: input.generator.outputDescriptor.logicalModuleIdentity, outputBinding: input.generator.outputDescriptor.binding, actionResultKey: actionResultState.actionResultKey, actionRecipeKeys: canonicalVariants.map((variant) => variant.actionRecipeKey), toolArtifact: input.generator.toolArtifact, typedInputs: canonicalBy(input.generator.typedInputs, (item) => item.binding), generatedSources: canonicalVariants[0].moduleSet, provenanceDisplayPath: facts.editablePath, diagnosticMapKeys: canonicalVariants.map((variant) => variant.diagnosticMapKey), compilerEvidence: "missing", generatedSourceAccess: "read-only-inspectable", navigation: "implementation-evidence-gap-requirement-not-implemented" };
  return { status: "accepted", route: "historical-candidate", code: metadata.interface === "targetSpecific" ? "target-specialized-generated-module" : "generated-module-reopened", reason: "A validated action result stages real W source artifacts; semantic frontend receipts remain an implementation evidence gap.", ...actionResultState, interfacePublished: true, compilerCachePublished: false, requiredCompilerPublication: true, staging: "published-atomically", stagingCleanupCount: 0, cleanupCount: 0, drainCount: 0, observedTrace: observedFrontendTrace(), requiredPhaseTrace: PHASES, publishEvent: "publish-module-interface", consumerRecompiled: interfaceChanged, interfaceChanged, targetEquivalent, targetInterfaceChanged, variantSetDigest, actionRecipeKeys: canonicalVariants.map((variant) => variant.actionRecipeKey), targetVariants: canonicalVariants, frontendEvidence: canonicalVariants[0].frontendEvidence, documentationKey: digestValue(output.documentation ?? ""), sourceMapFixable: canonicalVariants.some((variant) => variant.sourceMap.fixable), explainNavigation };
}
const CURRENT_EVIDENCE = new Map([
  ["closed-compiler-synthesis", ["last-light-reflection", "last-light-data", "last-light-kernels"]],
  ["generic-protocol-composition", ["last-light-reflection", "last-light-data"]],
  ["manual-resource", ["last-light-final-menu", "last-light-menu-compiler"]],
  ["typed-data-artifact", ["last-light-final-menu", "last-light-menu-transform"]],
]);
function deriveCurrentCase(testCase, corpus, root) {
  const input = testCase.input ?? {}; const exact = CURRENT_EVIDENCE.get(input.intent); const refs = new Map((corpus.sourceRefs ?? []).map((reference) => [reference.id, reference]));
  if (input.evidenceStatus !== "current-evidence" || !exact || JSON.stringify(input.sourceRefIds) !== JSON.stringify(exact) || exact.some((id) => !refs.has(id))) return rejectResult("invalid-current-evidence", "current A/B routes require exact source-backed corpus references and current-evidence status.");
  for (const id of exact) { const reference = refs.get(id); const file = pathInside(root, reference.path, `current evidence ${id}`, []); if (!file || digestFile(file) !== reference.digest || fs.readFileSync(file, "utf8").split(reference.symbol).length - 1 !== 1) return rejectResult("invalid-current-evidence", "current evidence reference is missing, renamed, stale, or ambiguous."); }
  return { status: "accepted", route: "composable", code: input.intent, reason: input.intent === "typed-data-artifact" ? "final.menu current transform publishes typed data without declarations." : input.intent === "manual-resource" ? "manual declarations/runtime lookup remain the current baseline." : "closed compiler synthesis and generic/protocol composition remain the current declaration mechanisms.", actionResultPublished: false, interfacePublished: false, compilerCachePublished: false, requiredCompilerPublication: false, staging: "evidence-only", stagingCleanupCount: 0, cleanupCount: 0, drainCount: 0, sourceRefIds: input.sourceRefIds };
}
export function deriveSyn1Case(testCase, corpus, { root = path.resolve(import.meta.dir, "..") } = {}) {
  const input = testCase.input ?? {}; if (CURRENT_EVIDENCE.has(input.intent)) return deriveCurrentCase(testCase, corpus, root); if (input.intent === "typed-declaration-recipe") return rejectResult("recipe-duplicates-checker", "a closed declaration recipe duplicates the frontend and remains rejected."); if (D_INTENTS.has(input.intent)) return rejectResult("dynamic-source-mutation", "macro, eval, decorator, metaclass, textual AST, and current-module injection remain rejected."); if (input.intent === "generated-module") return deriveGeneratedModule(input, corpus, root, testCase.id); return rejectResult("unknown-intent", "the case does not name a supported route.");
}
function checkExpected(testCase, result, location, errors) {
  const expected = testCase.expected; if (!expected || typeof expected !== "object" || Array.isArray(expected)) { errors.push(`${location}.expected must be a complete assertion object.`); return; } if (!hasOwn(expected, "status") || !hasOwn(expected, "route") || !hasOwn(expected, "code")) errors.push(`${location}.expected must assert status, route, and code.`);
  for (const key of Object.keys(expected)) { if (!ASSERTION_KEYS.has(key)) errors.push(`${location}.expected.${key} is not an allowed derived assertion key.`); else if (JSON.stringify(result[key]) !== JSON.stringify(expected[key])) errors.push(`${location}.expected.${key} does not match the derived result.`); }
}
function checkCaseShape(testCase, index, corpus, root, errors) {
  const location = `cases[${index}]`; requireString(testCase?.id, `${location}.id`, errors); if (!/^[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase?.id ?? "")) errors.push(`${location}.id must use kebab-case.`); if (!["A", "B", "C", "D"].includes(testCase?.axis)) errors.push(`${location}.axis must be A, B, C, or D.`); if (!testCase?.input || typeof testCase.input !== "object" || Array.isArray(testCase.input)) errors.push(`${location}.input must be an object.`); walkLegacy(testCase?.input, `${location}.input`, errors); if (hasOwn(testCase?.input, "phases")) errors.push(`${location}.input.phases is a legacy caller-result field.`); walkDigestFields(testCase?.input, `${location}.input`, errors); if (testCase?.input?.intent === "generated-module") checkActionEvents(testCase.input.actionEvents, `${location}.input.actionEvents`, errors); const result = deriveSyn1Case(testCase, corpus, { root }); checkExpected(testCase, result, location, errors);
}
export function validateSyn1(corpus, { root }) {
  const errors = []; if (corpus?.$schema !== "w-syn1-typed-generation-3") errors.push("corpus must use schema w-syn1-typed-generation-3."); if (corpus?.status !== "design-oracle-input-syn1") errors.push("corpus must have status design-oracle-input-syn1."); if (corpus?.id !== "SYN1" || corpus?.gate !== "SYN0-R1") errors.push("corpus must identify SYN1 and gate SYN0-R1."); for (const key of Object.keys(corpus ?? {})) if (LEGACY_KEYS.has(key)) errors.push(`corpus.${key} is a legacy caller-result field.`); walkDigestFields(corpus, "corpus", errors); checkSourceRefs(corpus, root, errors); checkOfficialRefs(corpus, errors); if (!requireArray(corpus?.phases, "phases", errors) || JSON.stringify(corpus.phases) !== JSON.stringify(PHASES)) errors.push("phases must equal the SYN1 phase DAG including atomic publish."); if (!requireArray(corpus?.cases, "cases", errors)) return { errors, results: [] };
  const ids = new Set(); for (const [index, testCase] of corpus.cases.entries()) { checkCaseShape(testCase, index, corpus, root, errors); if (ids.has(testCase?.id)) errors.push(`cases[${index}].id duplicates a case.`); ids.add(testCase?.id); } if (corpus.cases.length < 20) errors.push("SYN1 requires at least 20 adversarial cases."); const axes = new Set(corpus.cases.map((testCase) => testCase.axis)); for (const axis of ["A", "B", "C", "D"]) if (!axes.has(axis)) errors.push(`SYN1 must cover axis ${axis}.`); return { errors, results: corpus.cases.map((testCase) => ({ caseId: testCase.id, axis: testCase.axis, ...deriveSyn1Case(testCase, corpus, { root }) })) };
}
export function deriveSyn1(corpus, { root = path.resolve(import.meta.dir, "..") } = {}) { return corpus.cases.map((testCase) => ({ caseId: testCase.id, axis: testCase.axis, ...deriveSyn1Case(testCase, corpus, { root }) })); }
