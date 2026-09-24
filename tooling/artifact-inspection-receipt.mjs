import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { lstat, readFile } from "node:fs/promises";
import path from "node:path";
import {
  MATERIALIZED_MANIFEST,
  validateManifest,
  validateMaterialized,
} from "./acquire-mlir0-windows.mjs";

const MAX_TOOL_OUTPUT_BYTES = 128 * 1024 * 1024;
const MAX_INPUT_FILE_BYTES = 128 * 1024 * 1024;
const MAX_ALLOWLIST_BYTES = 1024 * 1024;
const KEY_SECTION_NAMES = [".text", ".rodata", ".rdata"];

function fail(message) {
  throw new Error(`artifact-inspection-receipt: ${message}`);
}

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex");
}

function safeInteger(value, label) {
  const number = typeof value === "number" ? value : Number(value);
  if (!Number.isSafeInteger(number) || number < 0) fail(`${label} is not a safe non-negative integer`);
  return number;
}

function parseInteger(value, label) {
  const text = String(value ?? "").trim();
  if (!/^(?:0x[0-9a-f]+|[0-9]+)$/iu.test(text)) fail(`${label} has an invalid integer value`);
  return safeInteger(Number(text), label);
}

function decodedName(value) {
  if (typeof value === "string") return value;
  if (value && typeof value.Name === "string") return value.Name;
  return undefined;
}

function detectArtifactFormat(bytes) {
  if (bytes.length >= 2 && bytes[0] === 0x4d && bytes[1] === 0x5a) {
    if (bytes.length < 0x40) fail("artifact has a truncated DOS header");
    const peOffset = bytes.readUInt32LE(0x3c);
    if (peOffset > bytes.length - 24 || !bytes.subarray(peOffset, peOffset + 4).equals(Buffer.from("PE\0\0", "ascii"))) {
      fail("artifact has an invalid or truncated PE signature");
    }
    return "PE";
  }
  if (bytes.length >= 4 && bytes.subarray(0, 4).equals(Buffer.from([0x7f, 0x45, 0x4c, 0x46]))) {
    if (bytes.length < 16) fail("artifact has a truncated ELF identification header");
    const classValue = bytes[4];
    const dataValue = bytes[5];
    if (![1, 2].includes(classValue) || ![1, 2].includes(dataValue) || bytes[6] !== 1) {
      fail("artifact has an invalid ELF class, byte order, or version");
    }
    const minimumHeaderBytes = classValue === 2 ? 64 : 52;
    if (bytes.length < minimumHeaderBytes) fail("artifact has a truncated ELF file header");
    return "ELF";
  }
  fail("artifact is neither a recognized PE nor ELF file");
}

async function readRegularFile(inputPath, label, cwd, maxBytes = MAX_INPUT_FILE_BYTES) {
  if (typeof inputPath !== "string" || inputPath.length === 0) fail(`${label} path is required`);
  const absolutePath = path.resolve(cwd, inputPath);
  let stats;
  try {
    stats = await lstat(absolutePath);
  } catch (error) {
    fail(`${label} cannot be read: ${error.message}`);
  }
  if (!Number.isSafeInteger(stats.size) || stats.size < 0) fail(`${label} has an unsupported file size`);
  if (stats.size > maxBytes) fail(`${label} exceeds the ${maxBytes / (1024 * 1024)} MiB input safety limit`);
  if (!stats.isFile() || stats.isSymbolicLink()) fail(`${label} must be a regular non-link file`);
  let bytes;
  try {
    bytes = await readFile(absolutePath);
  } catch (error) {
    fail(`${label} cannot be read: ${error.message}`);
  }
  if (bytes.length !== stats.size) fail(`${label} changed while it was being read`);
  return { path: absolutePath, bytes, size: stats.size, modifiedMs: stats.mtimeMs };
}

function defaultFindTool(name) {
  return Bun.which(name) ?? Bun.which(`${name}.exe`) ?? undefined;
}

function isReparsePoint(stats) {
  return stats.isSymbolicLink() || (stats.mode & 0xf000) === 0xa000;
}

async function loadPinnedMaterialization(toolchainDirectory) {
  const manifestPath = path.join(import.meta.dir, "mlir0-windows-toolchain.json");
  let manifestBytes;
  try {
    manifestBytes = await readFile(manifestPath);
  } catch (error) {
    fail(`pinned toolchain manifest cannot be read: ${error.message}`);
  }
  let manifest;
  try {
    manifest = JSON.parse(manifestBytes.toString("utf8"));
  } catch (error) {
    fail(`pinned toolchain manifest is not valid JSON: ${error.message}`);
  }
  const manifestErrors = validateManifest(manifest);
  if (manifestErrors.length > 0) fail(manifestErrors.join("; "));

  await validateMaterialized(toolchainDirectory, manifest, sha256(manifestBytes));
  const materializedPath = path.join(toolchainDirectory, MATERIALIZED_MANIFEST);
  let materializedBytes;
  try {
    materializedBytes = await readFile(materializedPath);
  } catch (error) {
    fail(`validated materialized manifest cannot be read: ${error.message}`);
  }
  try {
    return JSON.parse(materializedBytes.toString("utf8"));
  } catch (error) {
    fail(`validated materialized manifest is not valid JSON: ${error.message}`);
  }
}

async function resolveMaterializedTools(toolchainDirectory, cwd, loadMaterialization) {
  const absoluteDirectory = path.resolve(cwd, toolchainDirectory);
  let materialized;
  try {
    materialized = await loadMaterialization(absoluteDirectory);
  } catch (error) {
    if (String(error?.message ?? "").startsWith("artifact-inspection-receipt:")) throw error;
    fail(`--toolchain-dir is not a valid pinned materialization: ${error?.message ?? String(error)}`);
  }
  const archiveEntries = materialized?.archiveEntries;
  if (!Array.isArray(archiveEntries)) fail("validated materialized toolchain has no archive inventory");

  const selected = new Map();
  for (const name of ["llvm-readobj", "llvm-objdump", "llvm-nm"]) {
    const basename = `${name}.exe`;
    const matches = archiveEntries.filter((entry) => entry?.type === "file" &&
      typeof entry.path === "string" && path.posix.basename(entry.path) === basename);
    if (matches.length > 1) fail(`tool basename is ambiguous for ${basename}`);
    if (matches.length === 0) continue;
    const relativePath = matches[0].path;
    if (relativePath.includes("\\") || relativePath.includes("\0") ||
        relativePath.startsWith("/") || /^[A-Za-z]:/u.test(relativePath) ||
        relativePath.split("/").some((part) => part.length === 0 || part === "." || part === "..")) {
      fail(`materialized tool path is invalid for ${basename}`);
    }
    const executable = path.resolve(absoluteDirectory, ...relativePath.split("/"));
    const relativeToDirectory = path.relative(absoluteDirectory, executable);
    if (relativeToDirectory === ".." || relativeToDirectory.startsWith(`..${path.sep}`) || path.isAbsolute(relativeToDirectory))
      fail(`materialized tool path escapes --toolchain-dir: ${basename}`);
    let stats;
    try {
      stats = await lstat(executable);
    } catch (error) {
      fail(`materialized tool is unavailable for ${basename}: ${error.message}`);
    }
    if (!stats.isFile() || isReparsePoint(stats)) fail(`materialized tool is not a regular file: ${basename}`);
    selected.set(name, executable);
  }
  return { directory: absoluteDirectory, selected };
}

function defaultRunTool(executable, args) {
  const result = spawnSync(executable, args, {
    encoding: "utf8",
    maxBuffer: MAX_TOOL_OUTPUT_BYTES,
    windowsHide: true,
  });
  if (result.error) fail(`${path.basename(executable)} could not be run: ${result.error.message}`);
  if (result.status !== 0) {
    const detail = String(result.stderr ?? "").trim().slice(0, 1000);
    fail(`${path.basename(executable)} exited with ${result.status}${detail ? `: ${detail}` : ""}`);
  }
  return { stdout: String(result.stdout ?? ""), stderr: String(result.stderr ?? "") };
}

function invokeTool(name, args, { findTool, runTool, resolution }) {
  const executable = findTool(name);
  if (!executable) {
    const source = resolution === "PATH" ? "PATH" : "the validated --toolchain-dir";
    fail(`${name} is required but was not found in ${source}`);
  }
  try {
    const result = runTool(executable, args);
    if (!result || typeof result.stdout !== "string" || typeof result.stderr !== "string") {
      fail(`${name} runner returned an invalid result`);
    }
    const outputBytes = Buffer.byteLength(result.stdout, "utf8") + Buffer.byteLength(result.stderr, "utf8");
    if (outputBytes > MAX_TOOL_OUTPUT_BYTES) fail(`${name} output exceeded the safety limit`);
    return { ...result, executable };
  } catch (error) {
    if (String(error?.message ?? "").startsWith("artifact-inspection-receipt:")) throw error;
    fail(`${name} could not be run: ${error?.message ?? String(error)}`);
  }
}

function parseCoffSections(text, artifactBytes) {
  if (!/^Format:\s+COFF-/mu.test(text) || !/^Sections \[/mu.test(text)) fail("llvm-readobj did not return COFF section data");
  const sections = [];
  const blocks = text.matchAll(/^  Section \{\r?\n([\s\S]*?)^  \}/gmu);
  for (const match of blocks) {
    const body = match[1];
    const name = /^\s+Name:\s+(.+?)(?:\s+\([^\r\n]*\))?\s*$/mu.exec(body)?.[1]?.trim();
    if (name === undefined) fail("llvm-readobj returned a COFF section without a name");
    const rawText = /^\s+RawDataSize:\s*(0x[0-9a-f]+|[0-9]+)\s*$/imu.exec(body)?.[1];
    const offsetText = /^\s+PointerToRawData:\s*(0x[0-9a-f]+|[0-9]+)\s*$/imu.exec(body)?.[1];
    const virtualText = /^\s+VirtualSize:\s*(0x[0-9a-f]+|[0-9]+)\s*$/imu.exec(body)?.[1];
    if (rawText === undefined || offsetText === undefined) fail(`llvm-readobj omitted raw size or file offset for COFF section ${name}`);
    const rawBytes = parseInteger(rawText, `${name} raw size`);
    const fileOffset = parseInteger(offsetText, `${name} file offset`);
    if (fileOffset > artifactBytes || rawBytes > artifactBytes - fileOffset) fail(`COFF section ${name} exceeds artifact bounds`);
    sections.push({
      name,
      type: "COFF",
      fileOffset,
      declaredBytes: virtualText === undefined ? null : parseInteger(virtualText, `${name} virtual size`),
      rawBytes,
      fileBytes: rawBytes,
    });
  }
  if (sections.length === 0) fail("llvm-readobj returned no parseable COFF sections");
  return sections;
}

function parseElfJson(text, artifactBytes) {
  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch {
    fail("llvm-readobj returned invalid ELF JSON");
  }
  if (Array.isArray(parsed) && parsed.length !== 1) fail("llvm-readobj returned an ambiguous ELF file-record count");
  const record = Array.isArray(parsed) ? parsed[0] : parsed;
  if (!record || typeof record !== "object" || !/^elf(?:32|64)-/iu.test(record.FileSummary?.Format ?? "")) {
    fail("llvm-readobj did not return ELF section data");
  }
  if (!Array.isArray(record.Sections)) fail("llvm-readobj JSON omitted the ELF sections array");
  const sections = record.Sections.map((entry) => {
    const section = entry?.Section;
    if (!section || typeof section !== "object") fail("llvm-readobj returned a malformed ELF section");
    const name = decodedName(section.Name);
    const type = decodedName(section.Type);
    if (typeof name !== "string" || typeof type !== "string") fail("llvm-readobj returned an ELF section without a name or type");
    const declaredBytes = safeInteger(section.Size, `${name} declared size`);
    const fileOffset = safeInteger(section.Offset, `${name} file offset`);
    const fileBytes = type === "SHT_NOBITS" ? 0 : declaredBytes;
    if (fileOffset > artifactBytes || fileBytes > artifactBytes - fileOffset) fail(`ELF section ${name} exceeds artifact bounds`);
    return { name, type, fileOffset, declaredBytes, rawBytes: fileBytes, fileBytes };
  });
  if (sections.length === 0) fail("llvm-readobj returned no ELF sections");
  return { record, sections };
}

function parseNamedToolBlocks(text, heading) {
  const matcher = new RegExp(`^${heading} \\{\\r?\\n([\\s\\S]*?)^\\}`, "gmu");
  return [...text.matchAll(matcher)].map((match) => match[1]);
}

function parseCoffImports(text) {
  const imports = [];
  const seen = new Set();
  for (const [kind, heading] of [["regular", "Import"], ["delay", "DelayImport"]]) {
    const starts = [...text.matchAll(new RegExp(`^${heading} \\{`, "gmu"))].length;
    const blocks = parseNamedToolBlocks(text, heading);
    if (starts !== blocks.length) fail(`llvm-readobj returned a malformed ${heading} block`);
    for (const body of blocks) {
      const library = /^\s+Name:\s+([^\r\n]+)\s*$/mu.exec(body)?.[1]?.trim();
      if (!library) fail(`llvm-readobj returned a ${heading} block without a library name`);
      const key = `${library.toLowerCase()}\0${kind}`;
      if (seen.has(key)) fail(`llvm-readobj returned an ambiguous duplicate ${heading} block for ${library}`);
      seen.add(key);
      const symbols = [...body.matchAll(/^\s+Symbol:\s+([^\r\n]+?)\s*(?:\([^\r\n]*\))?\s*$/gmu)]
        .map((match) => match[1].trim());
      if (symbols.length === 0) fail(`llvm-readobj returned an ${heading} block without import symbols for ${library}`);
      if (new Set(symbols).size !== symbols.length) fail(`llvm-readobj returned duplicate import symbols for ${library}`);
      imports.push({ library, kind, symbols });
    }
  }
  return imports;
}

function parseElfDependencies(record) {
  const libraries = record.NeededLibraries;
  if (libraries !== undefined && libraries !== null && (!Array.isArray(libraries) || !libraries.every((value) => typeof value === "string"))) {
    fail("llvm-readobj returned a malformed NeededLibraries inventory");
  }
  const neededLibraries = Array.isArray(libraries) && libraries.every((value) => typeof value === "string")
    ? [...new Set(libraries)].sort()
    : undefined;
  if (record.DynamicSection !== undefined && record.DynamicSection !== null && !Array.isArray(record.DynamicSection)) {
    fail("llvm-readobj returned a malformed ELF dynamic-section inventory");
  }
  if (Array.isArray(record.DynamicSection)) {
    if (record.DynamicSection.some((entry) => !entry || typeof entry !== "object" || typeof entry.Type !== "string")) {
      fail("llvm-readobj returned a malformed ELF dynamic-section entry");
    }
    if (record.DynamicSection.some((entry) => entry?.Type === "NEEDED" && typeof entry.Library !== "string")) {
      fail("llvm-readobj returned a DT_NEEDED entry without a library name");
    }
    const dynamicLibraries = [...new Set(record.DynamicSection
      .filter((entry) => entry?.Type === "NEEDED")
      .map((entry) => entry.Library))].sort();
    if (neededLibraries !== undefined && JSON.stringify(neededLibraries) !== JSON.stringify(dynamicLibraries)) {
      fail("llvm-readobj returned conflicting DT_NEEDED inventories");
    }
    return { status: "observed", items: dynamicLibraries };
  }
  if (neededLibraries !== undefined) return { status: "observed", items: neededLibraries };
  return { status: "unknown", items: [], reason: "llvm-readobj did not provide a complete DT_NEEDED inventory" };
}

function inspectCoff(text, artifactBytes) {
  const sections = parseCoffSections(text, artifactBytes);
  const imports = parseCoffImports(text);
  return {
    sections,
    imports: { status: "observed", items: imports },
    dependencies: { status: "observed", items: [...new Set(imports.map((item) => item.library))].sort() },
  };
}

function inspectElf(text, artifactBytes) {
  const { record, sections } = parseElfJson(text, artifactBytes);
  return {
    sections,
    imports: {
      status: "unknown",
      items: [],
      reason: "dynamic symbol imports were not enumerated; only shared-library dependencies were queried",
    },
    dependencies: parseElfDependencies(record),
  };
}

function namedSectionBytes(sections) {
  return Object.fromEntries(KEY_SECTION_NAMES.map((name) => {
    const matching = sections.filter((section) => section.name === name);
    return [name, matching.length === 0 ? null : matching.reduce((sum, section) => sum + section.fileBytes, 0)];
  }));
}

function sectionCoverageFileBytes(sections) {
  const intervals = sections
    .filter((section) => section.fileBytes > 0)
    .map((section) => ({ start: section.fileOffset, end: section.fileOffset + section.fileBytes }))
    .sort((left, right) => left.start - right.start || left.end - right.end);
  let coveredBytes = 0;
  let currentStart;
  let currentEnd;
  for (const interval of intervals) {
    if (currentStart === undefined) {
      currentStart = interval.start;
      currentEnd = interval.end;
    } else if (interval.start <= currentEnd) {
      currentEnd = Math.max(currentEnd, interval.end);
    } else {
      coveredBytes += currentEnd - currentStart;
      currentStart = interval.start;
      currentEnd = interval.end;
    }
  }
  if (currentStart !== undefined) coveredBytes += currentEnd - currentStart;
  return coveredBytes;
}

function parseUndefinedSymbols(output, errorOutput) {
  const symbols = [];
  for (const line of output.split(/\r?\n/u)) {
    const trimmed = line.trim();
    if (!trimmed || /^\S+:\s*$/u.test(trimmed)) continue;
    const match = /^(?:\S+:\s+)?(\S+)\s+U(?:\s|$)/u.exec(trimmed);
    if (match) symbols.push(match[1]);
    else fail(`llvm-nm returned unrecognized POSIX output: ${trimmed.slice(0, 160)}`);
  }
  if (symbols.length === 0 && /\bno symbols\b/iu.test(errorOutput)) return [];
  return [...new Set(symbols)].sort();
}

function parsePostOptIR(bytes) {
  const text = bytes.toString("utf8");
  const functions = [];
  const globals = [];
  const identifier = String.raw`(?:"((?:[^"\\]|\\.)+)"|([-A-Za-z$._0-9]+))`;
  const functionPattern = new RegExp(String.raw`^\s*declare\b[^@\r\n]*@${identifier}\s*\(`, "gmu");
  const globalPattern = new RegExp(String.raw`^\s*@${identifier}\s*=\s*external\b`, "gmu");
  for (const match of text.matchAll(functionPattern)) functions.push(match[1] ?? match[2]);
  for (const match of text.matchAll(globalPattern)) globals.push(match[1] ?? match[2]);
  return {
    parser: "textual-declarations-v1",
    coverage: "partial",
    externalFunctions: [...new Set(functions)].sort(),
    externalGlobals: [...new Set(globals)].sort(),
  };
}

function validateSymbolList(value, label) {
  if (!Array.isArray(value)) fail(`${label} must be an explicit array`);
  const seen = new Set();
  for (const symbol of value) {
    if (typeof symbol !== "string" || symbol.trim().length === 0 || symbol.includes("\0")) {
      fail(`${label} must contain only non-empty symbol strings`);
    }
    if (seen.has(symbol)) fail(`${label} contains a duplicate symbol: ${symbol}`);
    seen.add(symbol);
  }
  return [...seen].sort();
}

function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

function normalizeAllowlists(value) {
  if (value === undefined) return undefined;
  if (!value || typeof value !== "object" || Array.isArray(value)) fail("allowlists must be an object");
  if (value.schema !== "w-artifact-inspection-allowlists-1") fail("allowlists.schema must be w-artifact-inspection-allowlists-1");
  const allowedKeys = new Set([
    "schema", "postOptIrExternals", "objectUndefinedSymbols", "finalImports", "finalDependencies",
  ]);
  for (const key of Object.keys(value)) if (!allowedKeys.has(key)) fail(`unknown allowlist boundary: ${key}`);

  const normalized = { schema: value.schema };
  if (Object.hasOwn(value, "postOptIrExternals")) {
    const entry = value.postOptIrExternals;
    if (!entry || typeof entry !== "object" || Array.isArray(entry) ||
        Object.keys(entry).some((key) => !["functions", "globals"].includes(key))) {
      fail("postOptIrExternals must contain only functions and globals arrays");
    }
    normalized.postOptIrExternals = {
      functions: validateSymbolList(entry.functions, "postOptIrExternals.functions"),
      globals: validateSymbolList(entry.globals, "postOptIrExternals.globals"),
    };
  }
  if (Object.hasOwn(value, "objectUndefinedSymbols")) {
    normalized.objectUndefinedSymbols = validateSymbolList(value.objectUndefinedSymbols, "objectUndefinedSymbols");
  }
  if (Object.hasOwn(value, "finalImports")) {
    if (!Array.isArray(value.finalImports)) fail("finalImports must be an explicit array");
    const seen = new Set();
    normalized.finalImports = value.finalImports.map((item, index) => {
      if (!item || typeof item !== "object" || Array.isArray(item) ||
          Object.keys(item).some((key) => !["library", "kind", "symbols"].includes(key))) {
        fail(`finalImports[${index}] must contain only library, kind, and symbols`);
      }
      const { library, kind } = item;
      if (typeof library !== "string" || library.trim().length === 0 || library.includes("\0")) {
        fail(`finalImports[${index}].library must be a non-empty string`);
      }
      if (!["regular", "delay"].includes(kind)) fail(`finalImports[${index}].kind must be regular or delay`);
      const key = `${library.toLowerCase()}\0${kind}`;
      if (seen.has(key)) fail(`finalImports contains a duplicate library/kind entry: ${library}/${kind}`);
      seen.add(key);
      return { library, kind, symbols: validateSymbolList(item.symbols, `finalImports[${index}].symbols`) };
    }).sort((left, right) => compareText(left.library, right.library) || compareText(left.kind, right.kind));
  }
  if (Object.hasOwn(value, "finalDependencies")) {
    normalized.finalDependencies = validateSymbolList(value.finalDependencies, "finalDependencies");
  }
  if (Object.keys(normalized).length === 1) fail("allowlists must declare at least one closure boundary");
  return normalized;
}

function closureCheck({ allowlist, evidenceStatus, references, label, reason, coverage = "complete" }) {
  if (allowlist === undefined) {
    return { status: "unknown", reason: `no explicit ${label} allowlist was supplied` };
  }
  if (evidenceStatus !== "observed") {
    return { status: "unknown", reason: reason ?? `${label} evidence was not observed` };
  }
  const allowed = new Set(allowlist);
  const unexpectedReferences = [...new Set(references)].filter((reference) => !allowed.has(reference)).sort();
  if (unexpectedReferences.length > 0) {
    return {
      basis: "exact-comparison-with-caller-supplied-allowlist",
      status: "rejected",
      allowedReferenceCount: allowed.size,
      observedReferenceCount: new Set(references).size,
      unexpectedReferences,
      ...(coverage === "complete" ? {} : { reason: `${label} evidence has ${coverage} parser coverage` }),
    };
  }
  if (coverage !== "complete") {
    return {
      basis: "exact-comparison-with-caller-supplied-allowlist",
      status: "unknown",
      allowedReferenceCount: allowed.size,
      observedReferenceCount: new Set(references).size,
      unexpectedReferences,
      reason: `${label} evidence has ${coverage} parser coverage; closure was not proven`,
    };
  }
  return {
    basis: "exact-comparison-with-caller-supplied-allowlist",
    status: "passed",
    allowedReferenceCount: allowed.size,
    observedReferenceCount: new Set(references).size,
    unexpectedReferences: [],
  };
}

function finalImportClosureCheck(allowlist, imports) {
  if (allowlist === undefined) return { status: "unknown", reason: "no explicit final-import allowlist was supplied" };
  if (imports.status !== "observed") {
    return { status: "unknown", reason: imports.reason ?? "final import inventory was not observed" };
  }
  const allowedByLibrary = new Map(allowlist.map((entry) => [
    `${entry.library.toLowerCase()}\0${entry.kind}`,
    new Set(entry.symbols),
  ]));
  const unexpectedReferences = [];
  for (const entry of imports.items) {
    const symbols = allowedByLibrary.get(`${entry.library.toLowerCase()}\0${entry.kind}`);
    if (!symbols) {
      unexpectedReferences.push(`${entry.library} (${entry.kind})`);
      continue;
    }
    for (const symbol of entry.symbols) {
      if (!symbols.has(symbol)) unexpectedReferences.push(`${entry.library}!${symbol} (${entry.kind})`);
    }
  }
  return {
    basis: "exact-comparison-with-caller-supplied-allowlist",
    status: unexpectedReferences.length === 0 ? "passed" : "rejected",
    allowedLibraryKindCount: allowedByLibrary.size,
    observedLibraryKindCount: imports.items.length,
    unexpectedReferences: [...new Set(unexpectedReferences)].sort(),
  };
}

function allowlistReceipt(allowlists, source, bytes) {
  if (allowlists === undefined) return { status: "unknown", reason: "no closure allowlists were supplied" };
  const serialized = JSON.stringify(allowlists);
  return {
    status: "observed",
    source,
    authority: "caller-supplied; provider identity and target/ABI authorization are not authenticated",
    sha256: sha256(bytes ?? Buffer.from(serialized, "utf8")),
    schema: allowlists.schema,
    boundaries: Object.keys(allowlists).filter((key) => key !== "schema").sort(),
    policy: allowlists,
  };
}

function disassemblySummary(text) {
  const instructionLines = text.split(/\r?\n/u).filter((line) => /^\s*[0-9a-f]+:\s+\S/iu.test(line)).length;
  const bytes = Buffer.from(text, "utf8");
  return { status: "observed", bytes: bytes.length, sha256: sha256(bytes), instructionLines };
}

export function parseArtifactInspectionArguments(argv) {
  if (!Array.isArray(argv)) fail("arguments must be an array");
  if (argv.length === 1 && ["--help", "-h"].includes(argv[0])) return { help: true };
  let artifact;
  let postOptIr;
  const objects = [];
  let toolchainDirectory;
  let allowlistsPath;
  let positionalOnly = false;
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (!positionalOnly && argument === "--") {
      positionalOnly = true;
      continue;
    }
    const option = !positionalOnly && argument.startsWith("--") ? argument.split("=", 1)[0] : undefined;
    if (option === "--post-opt-ir" || option === "--object" || option === "--toolchain-dir" || option === "--allowlists") {
      const inline = argument.startsWith(`${option}=`) ? argument.slice(option.length + 1) : undefined;
      const value = inline ?? argv[++index];
      if (typeof value !== "string" || value.length === 0 || value.startsWith("--")) fail(`${option} requires a path`);
      if (option === "--post-opt-ir") {
        if (postOptIr !== undefined) fail("--post-opt-ir may be specified only once");
        postOptIr = value;
      } else if (option === "--object") {
        objects.push(value);
      } else if (option === "--toolchain-dir") {
        if (toolchainDirectory !== undefined) fail("--toolchain-dir may be specified only once");
        toolchainDirectory = value;
      } else {
        if (allowlistsPath !== undefined) fail("--allowlists may be specified only once");
        allowlistsPath = value;
      }
      continue;
    }
    if (option !== undefined) fail(`unknown option: ${argument}`);
    if (artifact !== undefined) fail("exactly one artifact path is required");
    artifact = argument;
  }
  if (!artifact) fail("an explicit artifact path is required");
  return {
    help: false,
    artifact,
    postOptIr,
    objectPaths: objects,
    ...(allowlistsPath === undefined ? {} : { allowlistsPath }),
    ...(toolchainDirectory === undefined ? {} : { toolchainDirectory }),
  };
}

export function artifactInspectionUsage() {
  return [
    "usage: bun tooling/artifact-inspection-receipt.mjs <artifact> [--post-opt-ir <file.ll>] [--object <file.o|file.obj> ...] [--allowlists <file.json>] [--toolchain-dir <materialized-toolchain>]",
    "",
    "Reads one PE or ELF artifact and emits a compact JSON evidence receipt.",
    "Repeated --object options inspect every supplied object with llvm-nm.",
    "By default llvm-readobj, llvm-objdump, and (when --object is used) llvm-nm are resolved on PATH.",
    "--toolchain-dir validates an existing pinned Windows materialization and resolves exact tool basenames from its archive inventory without PATH fallback.",
    "--allowlists supplies explicit per-boundary symbol/import/dependency allowlists using schema w-artifact-inspection-allowlists-1.",
    "Input files and each tool's combined output are capped at 128 MiB.",
    "CRT-free status remains unknown; a partial IR scan or unsupported import inventory cannot prove closure.",
  ].join("\n");
}

export async function createArtifactInspectionReceipt({
  artifactPath,
  postOptIrPath,
  objectPath,
  objectPaths,
  toolchainDir,
  allowlists: allowlistsInput,
  allowlistsPath,
  cwd = process.cwd(),
  findTool = defaultFindTool,
  runTool = defaultRunTool,
  loadMaterialization = loadPinnedMaterialization,
} = {}) {
  if (objectPath !== undefined && objectPaths !== undefined) fail("use either objectPath or objectPaths, not both");
  const requestedObjectPaths = objectPaths === undefined ? (objectPath === undefined ? [] : [objectPath]) : objectPaths;
  if (!Array.isArray(requestedObjectPaths)) fail("objectPaths must be an array");
  if (requestedObjectPaths.some((item) => typeof item !== "string" || item.length === 0)) fail("objectPaths must contain non-empty paths");
  if (allowlistsInput !== undefined && allowlistsPath !== undefined) fail("use either inline allowlists or allowlistsPath, not both");

  let allowlistSnapshot;
  let suppliedAllowlists = allowlistsInput;
  let allowlistSource = "caller-inline";
  if (allowlistsPath !== undefined) {
    allowlistSnapshot = await readRegularFile(allowlistsPath, "allowlists", cwd, MAX_ALLOWLIST_BYTES);
    try {
      suppliedAllowlists = JSON.parse(allowlistSnapshot.bytes.toString("utf8"));
    } catch (error) {
      fail(`allowlists file is not valid JSON: ${error.message}`);
    }
    allowlistSource = allowlistSnapshot.path;
  }
  const allowlists = normalizeAllowlists(suppliedAllowlists);

  const artifact = await readRegularFile(artifactPath, "artifact", cwd);
  const format = detectArtifactFormat(artifact.bytes);
  const toolchain = toolchainDir === undefined
    ? { directory: null, selected: null }
    : await resolveMaterializedTools(toolchainDir, cwd, loadMaterialization);
  const toolResolution = toolchain.selected === null
    ? findTool
    : (name) => toolchain.selected.get(name);
  const toolFindContext = {
    findTool: toolResolution,
    runTool,
    resolution: toolchain.selected === null ? "PATH" : "validated-materialized-toolchain",
  };
  const readobjArgs = format === "ELF"
    ? ["--elf-output-style=JSON", "--pretty-print", "--sections", "--dynamic-table", "--needed-libs", artifact.path]
    : ["--sections", "--coff-imports", artifact.path];
  const readobj = invokeTool("llvm-readobj", readobjArgs, toolFindContext);
  const inventory = format === "ELF"
    ? inspectElf(readobj.stdout, artifact.size)
    : inspectCoff(readobj.stdout, artifact.size);
  const coveredSectionBytes = sectionCoverageFileBytes(inventory.sections);
  if (coveredSectionBytes > artifact.size) fail("section file ranges exceed artifact size");
  const disassembly = invokeTool("llvm-objdump", ["--disassemble", "--no-show-raw-insn", artifact.path], toolFindContext);

  let postOptIr;
  let postOptIrSnapshot;
  if (postOptIrPath !== undefined) {
    const ll = await readRegularFile(postOptIrPath, "post-opt IR", cwd);
    if (path.extname(ll.path).toLowerCase() !== ".ll") fail("post-opt IR path must use the .ll extension");
    postOptIrSnapshot = ll;
    postOptIr = {
      path: ll.path,
      bytes: ll.size,
      sha256: sha256(ll.bytes),
      externals: parsePostOptIR(ll.bytes),
    };
  }

  const objectSnapshots = [];
  const objectRecords = [];
  let llvmNmPath = null;
  const resolvedObjectPaths = new Set();
  for (const objectPathValue of requestedObjectPaths) {
    const object = await readRegularFile(objectPathValue, "object", cwd);
    const pathKey = process.platform === "win32" ? object.path.toLowerCase() : object.path;
    if (resolvedObjectPaths.has(pathKey)) fail(`object path was supplied more than once: ${object.path}`);
    resolvedObjectPaths.add(pathKey);
    const nm = invokeTool("llvm-nm", ["--undefined-only", "--format=posix", object.path], toolFindContext);
    objectSnapshots.push(object);
    llvmNmPath = nm.executable;
    objectRecords.push({
      path: object.path,
      bytes: object.size,
      sha256: sha256(object.bytes),
      undefinedSymbols: parseUndefinedSymbols(nm.stdout, nm.stderr),
    });
  }

  const checkedSnapshots = [
    { label: "artifact", snapshot: artifact },
    { label: "post-opt IR", snapshot: postOptIrSnapshot },
    { label: "allowlists", snapshot: allowlistSnapshot, maxBytes: MAX_ALLOWLIST_BYTES },
    ...objectSnapshots.map((snapshot) => ({ label: "object", snapshot })),
  ];
  for (const { label, snapshot, maxBytes } of checkedSnapshots) {
    if (!snapshot) continue;
    const after = await readRegularFile(snapshot.path, label, cwd, maxBytes);
    if (after.size !== snapshot.size || sha256(after.bytes) !== sha256(snapshot.bytes) || after.modifiedMs !== snapshot.modifiedMs) {
      fail(`${label} changed during inspection; no receipt was emitted`);
    }
  }

  const postOptFunctions = postOptIr?.externals.externalFunctions ?? [];
  const postOptGlobals = postOptIr?.externals.externalGlobals ?? [];
  const objectSymbols = objectRecords.flatMap((record) => record.undefinedSymbols);
  const postOptAllowlist = allowlists?.postOptIrExternals;
  const postOptExternalClosure = closureCheck({
    allowlist: postOptAllowlist === undefined ? undefined : [
      ...postOptAllowlist.functions.map((symbol) => `function:${symbol}`),
      ...postOptAllowlist.globals.map((symbol) => `global:${symbol}`),
    ],
    evidenceStatus: postOptIr === undefined ? "unknown" : "observed",
    references: [
      ...postOptFunctions.map((symbol) => `function:${symbol}`),
      ...postOptGlobals.map((symbol) => `global:${symbol}`),
    ],
    label: "post-opt IR external",
    reason: "a post-opt .ll file was not supplied",
    coverage: postOptIr?.externals.coverage ?? "unknown",
  });
  const objectUndefinedSymbolClosure = closureCheck({
    allowlist: allowlists?.objectUndefinedSymbols,
    evidenceStatus: objectRecords.length === 0 ? "unknown" : "observed",
    references: objectSymbols,
    label: "object undefined-symbol",
    reason: "no object files were supplied",
  });
  const finalDependencyClosure = closureCheck({
    allowlist: allowlists?.finalDependencies,
    evidenceStatus: inventory.dependencies.status,
    references: inventory.dependencies.items,
    label: "final dependency",
    reason: inventory.dependencies.reason ?? "final dependency inventory was not observed",
  });
  const finalImports = finalImportClosureCheck(allowlists?.finalImports, inventory.imports);
  const allowlistInfo = allowlistReceipt(allowlists, allowlistSource, allowlistSnapshot?.bytes);
  const checks = {
    postOptExternalClosure,
    suppliedObjectUndefinedSymbolClosure: objectUndefinedSymbolClosure,
    finalImportClosure: finalImports,
    finalDependencyClosure,
    crtFree: { status: "unknown", reason: "inventory alone cannot rule out statically linked runtime code or establish the product's runtime policy" },
  };
  const boundaryChecks = {
    postOptIrExternals: postOptExternalClosure,
    objectUndefinedSymbols: objectUndefinedSymbolClosure,
    finalImports,
    finalDependencies: finalDependencyClosure,
  };
  const requestedBoundaries = Object.keys(allowlists ?? {}).filter((key) => key !== "schema");
  const nonPassingBoundaries = requestedBoundaries.filter((key) => boundaryChecks[key].status !== "passed");
  checks.requestedClosureValidation = {
    status: requestedBoundaries.length === 0 ? "not-requested" : nonPassingBoundaries.length === 0 ? "passed" : "incomplete",
    scope: "requested caller-allowlist comparisons only",
    requestedBoundaries: requestedBoundaries.sort(),
    nonPassingBoundaries: nonPassingBoundaries.sort(),
    ...(nonPassingBoundaries.length === 0 ? {} : { reason: "every requested closure boundary must pass; unknown or rejected evidence is non-passing" }),
  };

  return {
    schema: "w-artifact-inspection-receipt-2",
    allowlists: allowlistInfo,
    artifact: { path: artifact.path, format, bytes: artifact.size, sha256: sha256(artifact.bytes) },
    sectionInventory: { status: "observed", items: inventory.sections, keySectionFileBytes: namedSectionBytes(inventory.sections) },
    fileByteAccounting: {
      sectionCoverageFileBytes: coveredSectionBytes,
      outsideDeclaredSectionsFileBytes: artifact.size - coveredSectionBytes,
      outsideDeclaredSectionsNote: "Bytes outside declared section file ranges; not automatically linker overhead.",
    },
    imports: inventory.imports,
    dependencies: inventory.dependencies,
    disassembly: disassemblySummary(disassembly.stdout),
    postOptIr: postOptIr ?? { status: "unknown", reason: "no post-opt .ll path was supplied" },
    objectUndefinedSymbols: objectRecords.length === 0
      ? { status: "unknown", scope: "caller-supplied-object-paths", items: [], reason: "no object paths were supplied" }
      : { status: "observed", scope: "caller-supplied-object-paths", items: objectRecords },
    checks,
    tools: {
      resolution: toolchain.selected === null ? "PATH" : "validated-materialized-toolchain",
      toolchainDirectory: toolchain.directory,
      llvmReadobj: readobj.executable,
      llvmObjdump: disassembly.executable,
      llvmNm: llvmNmPath,
    },
  };
}

export async function main(argv = process.argv.slice(2)) {
  try {
    const args = parseArtifactInspectionArguments(argv);
    if (args.help) {
      process.stdout.write(`${artifactInspectionUsage()}\n`);
      return 0;
    }
    const receipt = await createArtifactInspectionReceipt({
      artifactPath: args.artifact,
      postOptIrPath: args.postOptIr,
      objectPaths: args.objectPaths,
      allowlistsPath: args.allowlistsPath,
      toolchainDir: args.toolchainDirectory,
    });
    process.stdout.write(`${JSON.stringify(receipt)}\n`);
    if (receipt.checks.requestedClosureValidation.status === "incomplete") return 2;
    return 0;
  } catch (error) {
    process.stderr.write(`${error?.message ?? String(error)}\n`);
    return 1;
  }
}

if (import.meta.main) process.exitCode = await main();
