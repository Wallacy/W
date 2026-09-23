import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { lstat, readFile } from "node:fs/promises";
import path from "node:path";

const MAX_TOOL_OUTPUT_BYTES = 128 * 1024 * 1024;
const MAX_INPUT_FILE_BYTES = 128 * 1024 * 1024;
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

async function readRegularFile(inputPath, label, cwd) {
  if (typeof inputPath !== "string" || inputPath.length === 0) fail(`${label} path is required`);
  const absolutePath = path.resolve(cwd, inputPath);
  let stats;
  try {
    stats = await lstat(absolutePath);
  } catch (error) {
    fail(`${label} cannot be read: ${error.message}`);
  }
  if (!Number.isSafeInteger(stats.size) || stats.size < 0) fail(`${label} has an unsupported file size`);
  if (stats.size > MAX_INPUT_FILE_BYTES) fail(`${label} exceeds the 128 MiB input safety limit`);
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

function invokeTool(name, args, { findTool, runTool }) {
  const executable = findTool(name);
  if (!executable) fail(`${name} is required but was not found on PATH`);
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
  for (const [kind, heading] of [["regular", "Import"], ["delay", "DelayImport"]]) {
    for (const body of parseNamedToolBlocks(text, heading)) {
      const library = /^\s+Name:\s+([^\r\n]+)\s*$/mu.exec(body)?.[1]?.trim();
      if (!library) continue;
      const symbols = [...body.matchAll(/^\s+Symbol:\s+([^\r\n]+?)\s*(?:\([^\r\n]*\))?\s*$/gmu)]
        .map((match) => match[1].trim());
      imports.push({ library, kind, symbols });
    }
  }
  return imports;
}

function parseElfDependencies(record) {
  const libraries = record.NeededLibraries;
  if (Array.isArray(libraries) && libraries.every((value) => typeof value === "string")) {
    return { status: "observed", items: [...new Set(libraries)].sort() };
  }
  if (Array.isArray(record.DynamicSection)) {
    const needed = record.DynamicSection.filter((entry) => entry?.Type === "NEEDED" && typeof entry.Library === "string")
      .map((entry) => entry.Library);
    if (record.DynamicSection.every((entry) => entry?.Type !== "NEEDED" || typeof entry.Library === "string")) {
      return { status: "observed", items: [...new Set(needed)].sort() };
    }
  }
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
  let object;
  let positionalOnly = false;
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (!positionalOnly && argument === "--") {
      positionalOnly = true;
      continue;
    }
    const option = !positionalOnly && argument.startsWith("--") ? argument.split("=", 1)[0] : undefined;
    if (option === "--post-opt-ir" || option === "--object") {
      const inline = argument.startsWith(`${option}=`) ? argument.slice(option.length + 1) : undefined;
      const value = inline ?? argv[++index];
      if (typeof value !== "string" || value.length === 0 || value.startsWith("--")) fail(`${option} requires a path`);
      if (option === "--post-opt-ir") {
        if (postOptIr !== undefined) fail("--post-opt-ir may be specified only once");
        postOptIr = value;
      } else {
        if (object !== undefined) fail("--object may be specified only once");
        object = value;
      }
      continue;
    }
    if (option !== undefined) fail(`unknown option: ${argument}`);
    if (artifact !== undefined) fail("exactly one artifact path is required");
    artifact = argument;
  }
  if (!artifact) fail("an explicit artifact path is required");
  return { help: false, artifact, postOptIr, object };
}

export function artifactInspectionUsage() {
  return [
    "usage: bun tooling/artifact-inspection-receipt.mjs <artifact> [--post-opt-ir <file.ll>] [--object <file.o|file.obj>]",
    "",
    "Reads one PE or ELF artifact and emits a compact JSON evidence receipt.",
    "llvm-readobj, llvm-objdump, and (when --object is used) llvm-nm must be on PATH.",
    "Input files and each tool's combined output are capped at 128 MiB.",
    "Dependency closure and CRT-free status are always reported as unknown; inventories are not an allowlist proof.",
  ].join("\n");
}

export async function createArtifactInspectionReceipt({
  artifactPath,
  postOptIrPath,
  objectPath,
  cwd = process.cwd(),
  findTool = defaultFindTool,
  runTool = defaultRunTool,
} = {}) {
  const artifact = await readRegularFile(artifactPath, "artifact", cwd);
  const format = detectArtifactFormat(artifact.bytes);
  const readobjArgs = format === "ELF"
    ? ["--elf-output-style=JSON", "--pretty-print", "--sections", "--dynamic-table", "--needed-libs", artifact.path]
    : ["--sections", "--coff-imports", artifact.path];
  const readobj = invokeTool("llvm-readobj", readobjArgs, { findTool, runTool });
  const inventory = format === "ELF"
    ? inspectElf(readobj.stdout, artifact.size)
    : inspectCoff(readobj.stdout, artifact.size);
  const coveredSectionBytes = sectionCoverageFileBytes(inventory.sections);
  if (coveredSectionBytes > artifact.size) fail("section file ranges exceed artifact size");
  const disassembly = invokeTool("llvm-objdump", ["--disassemble", "--no-show-raw-insn", artifact.path], { findTool, runTool });

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

  let objectUndefinedSymbols = { status: "unknown", items: [], reason: "no object path was supplied" };
  let objectSnapshot;
  let llvmNmPath = null;
  if (objectPath !== undefined) {
    const object = await readRegularFile(objectPath, "object", cwd);
    const nm = invokeTool("llvm-nm", ["--undefined-only", "--format=posix", object.path], { findTool, runTool });
    objectSnapshot = object;
    llvmNmPath = nm.executable;
    objectUndefinedSymbols = {
      status: "observed",
      path: object.path,
      bytes: object.size,
      sha256: sha256(object.bytes),
      items: parseUndefinedSymbols(nm.stdout, nm.stderr),
    };
  }

  for (const [label, snapshot] of [["artifact", artifact], ["post-opt IR", postOptIrSnapshot], ["object", objectSnapshot]]) {
    if (!snapshot) continue;
    const after = await readRegularFile(snapshot.path, label, cwd);
    if (after.size !== snapshot.size || sha256(after.bytes) !== sha256(snapshot.bytes) || after.modifiedMs !== snapshot.modifiedMs) {
      fail(`${label} changed during inspection; no receipt was emitted`);
    }
  }

  return {
    schema: "w-artifact-inspection-receipt-1",
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
    objectUndefinedSymbols,
    checks: {
      postOptExternalClosure: { status: "unknown", reason: "the optional textual IR scan is partial and no required-symbol policy was supplied" },
      allObjectUndefinedSymbolClosure: { status: "unknown", reason: "at most one explicit object was inspected; no complete object manifest or allowlist was supplied" },
      finalDependencyClosure: { status: "unknown", reason: "observed imports/dependencies were not compared with a selected WRT, target SDK, provider, or hosted-capability allowlist" },
      crtFree: { status: "unknown", reason: "inventory alone cannot rule out statically linked runtime code or establish the product's runtime policy" },
    },
    tools: {
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
      objectPath: args.object,
    });
    process.stdout.write(`${JSON.stringify(receipt)}\n`);
    return 0;
  } catch (error) {
    process.stderr.write(`${error?.message ?? String(error)}\n`);
    return 1;
  }
}

if (import.meta.main) process.exitCode = await main();
