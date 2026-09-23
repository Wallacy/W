import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import {
  createArtifactInspectionReceipt,
  parseArtifactInspectionArguments,
} from "./artifact-inspection-receipt.mjs";

function makePeFixture() {
  const bytes = Buffer.alloc(512);
  bytes.write("MZ", 0, "ascii");
  bytes.writeUInt32LE(64, 0x3c);
  bytes.write("PE\0\0", 64, "ascii");
  return bytes;
}

function makeElfFixture() {
  const bytes = Buffer.alloc(512);
  bytes.set([0x7f, 0x45, 0x4c, 0x46, 2, 1, 1], 0);
  return bytes;
}

function withTempDirectory(t) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), "w-artifact-inspection-test-"));
  t.after(() => fs.rmSync(directory, { recursive: true, force: true }));
  return directory;
}

function mockedTools(readobjOutput, extras = {}) {
  const calls = [];
  return {
    calls,
    findTool: (name) => `mock-tools/${name}.exe`,
    runTool: (executable, args) => {
      const tool = path.posix.basename(executable).replace(/\.exe$/iu, "");
      calls.push({ tool, args });
      if (tool === "llvm-readobj") {
        if (extras.readobjError) throw new Error(extras.readobjError);
        return { stdout: readobjOutput, stderr: "" };
      }
      if (tool === "llvm-objdump") return { stdout: extras.disassembly ?? "file: file format\n\nDisassembly of section .text:\n  1000:\tretq\n", stderr: "" };
      if (tool === "llvm-nm") return { stdout: extras.undefinedSymbols ?? "object.obj: external_fn U 0 0\n", stderr: "" };
      throw new Error(`unexpected tool ${tool}`);
    },
  };
}

const peReadobjOutput = `File: sample.exe
Format: COFF-x86-64
Arch: x86_64
Sections [
  Section {
    Number: 1
    Name: .text (2E 74 65 78 74 00 00 00)
    VirtualSize: 0x14
    RawDataSize: 16
    PointerToRawData: 0x100
  }
  Section {
    Number: 2
    Name: .rdata (2E 72 64 61 74 61 00 00)
    VirtualSize: 0x20
    RawDataSize: 32
    PointerToRawData: 0x110
  }
]
Import {
  Name: KERNEL32.dll
  Symbol: ExitProcess (123)
  Symbol: WriteFile (456)
}
DelayImport {
  Name: USER32.dll
  Import {
    Symbol: MessageBoxW (0)
  }
}
`;

test("argument parser requires one explicit artifact and accepts optional evidence paths", () => {
  assert.deepEqual(parseArtifactInspectionArguments(["sample.exe", "--post-opt-ir", "after.ll", "--object=obj.obj"]), {
    help: false,
    artifact: "sample.exe",
    postOptIr: "after.ll",
    object: "obj.obj",
  });
  assert.deepEqual(parseArtifactInspectionArguments(["--help"]), { help: true });
  assert.throws(() => parseArtifactInspectionArguments([]), /explicit artifact path/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "b.exe"]), /exactly one artifact/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "--object"]), /--object requires/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "--nope"]), /unknown option/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "--object", "a.obj", "--object", "b.obj"]), /only once/u);
});

test("PE receipt inventories section bytes, imports, IR declarations, and one object's undefined symbols", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const irPath = path.join(directory, "after.ll");
  const objectPath = path.join(directory, "sample.obj");
  fs.writeFileSync(artifactPath, makePeFixture());
  fs.writeFileSync(irPath, "declare i32 @puts(ptr)\n@runtime_state = external global ptr\ndefine i32 @main() { ret i32 0 }\n");
  fs.writeFileSync(objectPath, Buffer.from("object fixture"));
  const tools = mockedTools(peReadobjOutput);

  const receipt = await createArtifactInspectionReceipt({
    artifactPath,
    postOptIrPath: irPath,
    objectPath,
    findTool: tools.findTool,
    runTool: tools.runTool,
  });

  assert.equal(receipt.schema, "w-artifact-inspection-receipt-1");
  assert.equal(receipt.artifact.format, "PE");
  assert.equal(receipt.artifact.bytes, 512);
  assert.equal(receipt.sectionInventory.status, "observed");
  assert.deepEqual(receipt.sectionInventory.keySectionFileBytes, { ".text": 16, ".rodata": null, ".rdata": 32 });
  assert.equal(receipt.fileByteAccounting.sectionCoverageFileBytes, 48);
  assert.equal(receipt.fileByteAccounting.outsideDeclaredSectionsFileBytes, 464);
  assert.match(receipt.fileByteAccounting.outsideDeclaredSectionsNote, /not automatically linker overhead/u);
  assert.deepEqual(receipt.imports.items.map(({ library, kind, symbols }) => ({ library, kind, symbols })), [
    { library: "KERNEL32.dll", kind: "regular", symbols: ["ExitProcess", "WriteFile"] },
    { library: "USER32.dll", kind: "delay", symbols: ["MessageBoxW"] },
  ]);
  assert.deepEqual(receipt.dependencies.items, ["KERNEL32.dll", "USER32.dll"]);
  assert.equal(receipt.postOptIr.externals.coverage, "partial");
  assert.deepEqual(receipt.postOptIr.externals.externalFunctions, ["puts"]);
  assert.deepEqual(receipt.postOptIr.externals.externalGlobals, ["runtime_state"]);
  assert.deepEqual(receipt.objectUndefinedSymbols.items, ["external_fn"]);
  assert.equal(receipt.checks.finalDependencyClosure.status, "unknown");
  assert.equal(receipt.checks.crtFree.status, "unknown");
  assert.deepEqual(tools.calls.map((call) => call.tool), ["llvm-readobj", "llvm-objdump", "llvm-nm"]);
});

test("ELF receipt handles NOBITS and reports dependencies without claiming import or CRT closure", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.elf");
  fs.writeFileSync(artifactPath, makeElfFixture());
  const output = JSON.stringify([{
    FileSummary: { Format: "elf64-x86-64" },
    Sections: [
      { Section: { Name: { Name: ".text" }, Type: { Name: "SHT_PROGBITS" }, Offset: 64, Size: 20 } },
      { Section: { Name: { Name: ".rodata" }, Type: { Name: "SHT_PROGBITS" }, Offset: 84, Size: 8 } },
      { Section: { Name: { Name: ".bss" }, Type: { Name: "SHT_NOBITS" }, Offset: 92, Size: 128 } },
    ],
    DynamicSection: [{ Type: "NEEDED", Library: "libc.so.6" }],
    NeededLibraries: ["libc.so.6"],
  }]);
  const tools = mockedTools(output);

  const receipt = await createArtifactInspectionReceipt({
    artifactPath,
    findTool: tools.findTool,
    runTool: tools.runTool,
  });

  assert.equal(receipt.artifact.format, "ELF");
  assert.deepEqual(receipt.sectionInventory.keySectionFileBytes, { ".text": 20, ".rodata": 8, ".rdata": null });
  assert.equal(receipt.sectionInventory.items[2].declaredBytes, 128);
  assert.equal(receipt.sectionInventory.items[2].fileBytes, 0);
  assert.equal(receipt.fileByteAccounting.sectionCoverageFileBytes, 28);
  assert.equal(receipt.fileByteAccounting.outsideDeclaredSectionsFileBytes, 484);
  assert.deepEqual(receipt.dependencies, { status: "observed", items: ["libc.so.6"] });
  assert.equal(receipt.imports.status, "unknown");
  assert.equal(receipt.postOptIr.status, "unknown");
  assert.equal(receipt.objectUndefinedSymbols.status, "unknown");
  assert.equal(receipt.checks.crtFree.status, "unknown");
  assert.ok(tools.calls[0].args.includes("--elf-output-style=JSON"));
});

test("overlapping PE section file ranges are counted once", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "overlapping.exe");
  fs.writeFileSync(artifactPath, makePeFixture());
  const readobjOutput = `Format: COFF-x86-64
Sections [
  Section {
    Number: 1
    Name: .text (2E 74 65 78 74 00 00 00)
    VirtualSize: 0x10
    RawDataSize: 16
    PointerToRawData: 0x100
  }
  Section {
    Number: 2
    Name: .rdata (2E 72 64 61 74 61 00 00)
    VirtualSize: 0x20
    RawDataSize: 32
    PointerToRawData: 0x120
  }
  Section {
    Number: 3
    Name: .pdata (2E 70 64 61 74 61 00 00)
    VirtualSize: 0x18
    RawDataSize: 24
    PointerToRawData: 0x108
  }
]
`;
  const tools = mockedTools(readobjOutput);

  const receipt = await createArtifactInspectionReceipt({
    artifactPath,
    findTool: tools.findTool,
    runTool: tools.runTool,
  });

  assert.equal(receipt.sectionInventory.items.reduce((sum, section) => sum + section.fileBytes, 0), 72);
  assert.equal(receipt.fileByteAccounting.sectionCoverageFileBytes, 64);
  assert.equal(receipt.fileByteAccounting.outsideDeclaredSectionsFileBytes, 448);
});

test("invalid or unsupported binaries, absent tools, and malformed inspection output fail closed", async (t) => {
  const directory = withTempDirectory(t);
  const invalidPath = path.join(directory, "invalid.exe");
  const pePath = path.join(directory, "sample.exe");
  fs.writeFileSync(invalidPath, Buffer.from("MZ"));
  fs.writeFileSync(pePath, makePeFixture());

  await assert.rejects(createArtifactInspectionReceipt({ artifactPath: invalidPath, findTool: () => undefined }), /truncated DOS header/u);
  await assert.rejects(createArtifactInspectionReceipt({ artifactPath: path.join(directory, "missing"), findTool: () => undefined }), /cannot be read/u);
  await assert.rejects(createArtifactInspectionReceipt({ artifactPath: pePath, findTool: () => undefined }), /llvm-readobj is required/u);

  const failingTools = mockedTools("", { readobjError: "bad executable" });
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath: pePath,
    findTool: failingTools.findTool,
    runTool: failingTools.runTool,
  }), /llvm-readobj could not be run: bad executable/u);

  const malformedTools = mockedTools("Format: COFF-x86-64\nSections [\n]\n");
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath: pePath,
    findTool: malformedTools.findTool,
    runTool: malformedTools.runTool,
  }), /no parseable COFF sections/u);
});

test("an explicitly requested object fails clearly when llvm-nm is unavailable", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const objectPath = path.join(directory, "sample.obj");
  fs.writeFileSync(artifactPath, makePeFixture());
  fs.writeFileSync(objectPath, Buffer.from("object fixture"));
  const tools = mockedTools(peReadobjOutput);
  const findTool = (name) => name === "llvm-nm" ? undefined : tools.findTool(name);

  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    objectPath,
    findTool,
    runTool: tools.runTool,
  }), /llvm-nm is required but was not found/u);
});

test("artifact and optional files must be regular, and post-opt input must be .ll", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const wrongIrPath = path.join(directory, "after.txt");
  fs.writeFileSync(artifactPath, makePeFixture());
  fs.writeFileSync(wrongIrPath, "declare void @puts()\n");
  const tools = mockedTools(peReadobjOutput);
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    postOptIrPath: wrongIrPath,
    findTool: tools.findTool,
    runTool: tools.runTool,
  }), /must use the \.ll extension/u);
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    objectPath: directory,
    findTool: tools.findTool,
    runTool: tools.runTool,
  }), /object must be a regular non-link file/u);
});

test("oversized input is rejected before it is read into memory", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "oversized.exe");
  const handle = fs.openSync(artifactPath, "w");
  try {
    fs.ftruncateSync(handle, 128 * 1024 * 1024 + 1);
  } finally {
    fs.closeSync(handle);
  }
  await assert.rejects(createArtifactInspectionReceipt({ artifactPath }), /128 MiB input safety limit/u);
});
