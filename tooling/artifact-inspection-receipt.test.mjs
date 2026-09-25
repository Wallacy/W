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
      const tool = path.basename(executable).replace(/\.exe$/iu, "");
      calls.push({ tool, args });
      if (tool === "llvm-readobj") {
        if (extras.readobjError) throw new Error(extras.readobjError);
        return { stdout: readobjOutput, stderr: "" };
      }
      if (tool === "llvm-objdump") {
        if (args.includes("--reloc")) {
          const objectName = path.basename(args.at(-1));
          return {
            stdout: extras.relocationsByObject?.[objectName] ??
              `${objectName}: file format elf64-x86-64\n`,
            stderr: "",
          };
        }
        return { stdout: extras.disassembly ?? "file: file format\n\nDisassembly of section .text:\n  1000:\tretq\n", stderr: "" };
      }
      if (tool === "llvm-nm") {
        const objectName = path.basename(args.at(-1));
        const error = extras.undefinedSymbolsErrorByObject?.[objectName];
        if (error) throw new Error(error);
        return {
          stdout: extras.nmOutputByObject?.[objectName] ??
            extras.undefinedSymbolsByObject?.[objectName] ??
            extras.undefinedSymbols ?? "object.obj: external_fn U 0 0\n",
          stderr: "",
        };
      }
      throw new Error(`unexpected tool ${tool}`);
    },
  };
}

function elfObjectBoundaries(overrides = {}) {
  return {
    schema: "w-artifact-inspection-allowlists-2",
    objectUndefinedSymbols: ["main", "write"],
    objectSymbolLinkage: [
      {
        object: "output.o",
        routeRoots: ["main"],
        forbiddenGlobalWPrivateHelpers: ["w_seed_copy"],
      },
      {
        object: "wrt0.o",
        routeRoots: ["_start", "write"],
        forbiddenGlobalWPrivateHelpers: [],
      },
    ],
    objectRelocations: [
      {
        object: "output.o",
        allowedCrossObjectRelocations: [
          { targetObject: "wrt0.o", symbol: "write", type: "R_X86_64_PLT32" },
        ],
      },
      {
        object: "wrt0.o",
        allowedCrossObjectRelocations: [
          { targetObject: "output.o", symbol: "main", type: "R_X86_64_PLT32" },
        ],
      },
    ],
    ...overrides,
  };
}

function validElfObjectEvidence() {
  return {
    nmOutputByObject: {
      "output.o": "main T 0 10\nw_seed_copy t 10 4\nwrite U 0 0\n",
      "wrt0.o": "_start T 0 10\nmain U 0 0\nwrite T 10 8\n",
    },
    relocationsByObject: {
      "output.o": "output.o: file format elf64-x86-64\nRELOCATION RECORDS FOR [.text]:\nOFFSET TYPE VALUE\n0000000000000002 R_X86_64_PLT32 w_seed_copy-0x4\n0000000000000007 R_X86_64_PLT32 write-0x4\n",
      "wrt0.o": "wrt0.o: file format elf64-x86-64\nRELOCATION RECORDS FOR [.text]:\nOFFSET TYPE VALUE\n0000000000000001 R_X86_64_PLT32 main-0x4\n",
    },
  };
}

async function inspectElfObjectPair(t, evidence, allowlists = elfObjectBoundaries()) {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "product.exe");
  const productObject = path.join(directory, "output.o");
  const wrt0Object = path.join(directory, "wrt0.o");
  fs.writeFileSync(artifactPath, makePeFixture());
  fs.writeFileSync(productObject, Buffer.from("product object fixture"));
  fs.writeFileSync(wrt0Object, Buffer.from("WRT0 object fixture"));
  const tools = mockedTools(peReadobjOutput, evidence);
  return createArtifactInspectionReceipt({
    artifactPath,
    objectPaths: [productObject, wrt0Object],
    allowlists,
    findTool: tools.findTool,
    runTool: tools.runTool,
  });
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

test("argument parser requires one explicit artifact and accepts repeatable object and allowlist paths", () => {
  assert.deepEqual(parseArtifactInspectionArguments(["sample.exe", "--post-opt-ir", "after.ll", "--object=obj.obj"]), {
    help: false,
    artifact: "sample.exe",
    postOptIr: "after.ll",
    objectPaths: ["obj.obj"],
  });
  assert.deepEqual(parseArtifactInspectionArguments(["sample.exe", "--toolchain-dir", "cache", "--object", "x.obj", "--object=y.obj", "--allowlists", "allowed.json"]), {
    help: false,
    artifact: "sample.exe",
    postOptIr: undefined,
    objectPaths: ["x.obj", "y.obj"],
    allowlistsPath: "allowed.json",
    toolchainDirectory: "cache",
  });
  assert.deepEqual(parseArtifactInspectionArguments(["sample.exe", "--toolchain-dir=cache"]), {
    help: false,
    artifact: "sample.exe",
    postOptIr: undefined,
    objectPaths: [],
    toolchainDirectory: "cache",
  });
  assert.deepEqual(parseArtifactInspectionArguments(["--help"]), { help: true });
  assert.throws(() => parseArtifactInspectionArguments([]), /explicit artifact path/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "b.exe"]), /exactly one artifact/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "--object"]), /--object requires/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "--nope"]), /unknown option/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "--toolchain-dir"]), /--toolchain-dir requires/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "--toolchain-dir", "one", "--toolchain-dir", "two"]), /--toolchain-dir may be specified only once/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "--allowlists"]), /--allowlists requires/u);
  assert.throws(() => parseArtifactInspectionArguments(["a.exe", "--allowlists", "one.json", "--allowlists", "two.json"]), /--allowlists may be specified only once/u);
});

test("PE receipt inventories section bytes, imports, IR declarations, and supplied object's undefined symbols", async (t) => {
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

  assert.equal(receipt.schema, "w-artifact-inspection-receipt-3");
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
  assert.equal(receipt.objectUndefinedSymbols.scope, "caller-supplied-object-paths");
  assert.deepEqual(receipt.objectUndefinedSymbols.items.map((item) => item.undefinedSymbols), [["external_fn"]]);
  assert.equal(receipt.checks.postOptExternalClosure.status, "unknown");
  assert.equal(receipt.checks.suppliedObjectUndefinedSymbolClosure.status, "unknown");
  assert.equal(receipt.checks.finalImportClosure.status, "unknown");
  assert.equal(receipt.checks.finalDependencyClosure.status, "unknown");
  assert.equal(receipt.checks.requestedClosureValidation.status, "not-requested");
  assert.equal(receipt.checks.crtFree.status, "unknown");
  assert.deepEqual(tools.calls.map((call) => call.tool), ["llvm-readobj", "llvm-objdump", "llvm-nm"]);
});

test("every supplied object is inspected and an unexpected later-object symbol rejects its closure", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const firstObjectPath = path.join(directory, "first.obj");
  const secondObjectPath = path.join(directory, "second.obj");
  fs.writeFileSync(artifactPath, makePeFixture());
  fs.writeFileSync(firstObjectPath, "first object");
  fs.writeFileSync(secondObjectPath, "second object");
  const tools = mockedTools(peReadobjOutput, {
    undefinedSymbolsByObject: {
      "first.obj": "allowed_fn U 0 0\n",
      "second.obj": "surprise_fn U 0 0\n",
    },
  });

  const receipt = await createArtifactInspectionReceipt({
    artifactPath,
    objectPaths: [firstObjectPath, secondObjectPath],
    allowlists: {
      schema: "w-artifact-inspection-allowlists-2",
      objectUndefinedSymbols: ["allowed_fn"],
    },
    findTool: tools.findTool,
    runTool: tools.runTool,
  });

  assert.equal(receipt.objectUndefinedSymbols.status, "observed");
  assert.deepEqual(receipt.objectUndefinedSymbols.items.map((item) => path.basename(item.path)), ["first.obj", "second.obj"]);
  assert.deepEqual(receipt.objectUndefinedSymbols.items.map((item) => item.undefinedSymbols), [["allowed_fn"], ["surprise_fn"]]);
  assert.equal(receipt.checks.suppliedObjectUndefinedSymbolClosure.status, "rejected");
  assert.deepEqual(receipt.checks.suppliedObjectUndefinedSymbolClosure.unexpectedReferences, ["surprise_fn"]);
  assert.equal(tools.calls.filter((call) => call.tool === "llvm-nm").length, 2);
});

test("per-object linkage and relocation boundaries prove local helpers and exact route roots", async (t) => {
  const receipt = await inspectElfObjectPair(t, validElfObjectEvidence());

  assert.equal(receipt.schema, "w-artifact-inspection-receipt-3");
  assert.equal(receipt.tools.resolution, "caller-supplied-explicit-paths");
  assert.deepEqual(receipt.objectUndefinedSymbols.items.map((item) => item.object), [
    "output.o", "wrt0.o",
  ]);
  assert.deepEqual(receipt.objectUndefinedSymbols.items[0].definitions, [
    { symbol: "main", type: "T", linkage: "external" },
    { symbol: "w_seed_copy", type: "t", linkage: "local" },
  ]);
  assert.deepEqual(receipt.objectUndefinedSymbols.items[0].relocations.map(
    ({ symbol, type }) => ({ symbol, type })), [
    { symbol: "w_seed_copy", type: "R_X86_64_PLT32" },
    { symbol: "write", type: "R_X86_64_PLT32" },
  ]);
  assert.equal(receipt.checks.objectSymbolLinkage.status, "passed");
  assert.deepEqual(receipt.checks.objectSymbolLinkage.items[0].routeRoots, ["main"]);
  assert.deepEqual(receipt.checks.objectSymbolLinkage.items[0].localWPrivateHelpers, ["w_seed_copy"]);
  assert.equal(receipt.checks.objectRelocations.status, "passed");
  assert.deepEqual(receipt.checks.objectRelocations.items.map((item) =>
    item.observedCrossObjectRelocations), [[
    { targetObject: "wrt0.o", symbol: "write", type: "R_X86_64_PLT32" },
  ], [
    { targetObject: "output.o", symbol: "main", type: "R_X86_64_PLT32" },
  ]]);
  assert.equal(receipt.checks.requestedClosureValidation.status, "passed");
});

test("an externalized W-private helper and a lost route root reject object linkage", async (t) => {
  const externalized = validElfObjectEvidence();
  externalized.nmOutputByObject["output.o"] =
    "main T 0 10\nw_seed_copy T 10 4\nwrite U 0 0\n";
  const externalizedReceipt = await inspectElfObjectPair(t, externalized);
  assert.equal(externalizedReceipt.checks.objectSymbolLinkage.status, "rejected");
  assert.deepEqual(
    externalizedReceipt.checks.objectSymbolLinkage.items[0].forbiddenGlobalWPrivateHelpers,
    ["w_seed_copy"],
  );

  const rootLost = validElfObjectEvidence();
  rootLost.nmOutputByObject["output.o"] = "w_seed_copy t 10 4\nwrite U 0 0\n";
  const rootLostReceipt = await inspectElfObjectPair(t, rootLost);
  assert.equal(rootLostReceipt.checks.objectSymbolLinkage.status, "rejected");
  assert.deepEqual(rootLostReceipt.checks.objectSymbolLinkage.items[0].missingRouteRoots, ["main"]);
  assert.equal(rootLostReceipt.checks.objectRelocations.status, "rejected");
});

test("an unexpected external definition rejects object linkage", async (t) => {
  const evidence = validElfObjectEvidence();
  evidence.nmOutputByObject["output.o"] += "surprise_fn T 20 4\n";
  const receipt = await inspectElfObjectPair(t, evidence);
  assert.equal(receipt.checks.objectSymbolLinkage.status, "rejected");
  assert.deepEqual(
    receipt.checks.objectSymbolLinkage.items[0].unexpectedGlobalDefinitions,
    ["surprise_fn"],
  );
});

test("bad helper and unexpected cross-object relocations reject the relocation boundary", async (t) => {
  const helperReference = validElfObjectEvidence();
  helperReference.relocationsByObject["wrt0.o"] =
    "wrt0.o: file format elf64-x86-64\nRELOCATION RECORDS FOR [.text]:\nOFFSET TYPE VALUE\n0000000000000001 R_X86_64_PLT32 main-0x4\n0000000000000005 R_X86_64_PLT32 w_seed_copy-0x4\n";
  const helperReceipt = await inspectElfObjectPair(t, helperReference);
  assert.equal(helperReceipt.checks.objectRelocations.status, "rejected");
  assert.deepEqual(
    helperReceipt.checks.objectRelocations.items[1].forbiddenGlobalWPrivateHelperRelocations.map(
      (relocation) => relocation.symbol),
    ["w_seed_copy"],
  );

  const unexpectedCrossObject = validElfObjectEvidence();
  unexpectedCrossObject.relocationsByObject["output.o"] =
    "output.o: file format elf64-x86-64\nRELOCATION RECORDS FOR [.text]:\nOFFSET TYPE VALUE\n0000000000000002 R_X86_64_PLT32 w_seed_copy-0x4\n0000000000000007 R_X86_64_PLT32 write-0x4\n000000000000000b R_X86_64_PLT32 _start-0x4\n";
  unexpectedCrossObject.nmOutputByObject["output.o"] += "_start U 0 0\n";
  const crossReceipt = await inspectElfObjectPair(t, unexpectedCrossObject);
  assert.equal(crossReceipt.checks.objectRelocations.status, "rejected");
  assert.deepEqual(
    crossReceipt.checks.objectRelocations.items[0].unexpectedCrossObjectRelocations,
    [{ targetObject: "wrt0.o", symbol: "_start", type: "R_X86_64_PLT32" }],
  );
});

test("object linkage policy fails closed for missing evidence, mismatched objects, and malformed tool data", async (t) => {
  const noObjectsDirectory = withTempDirectory(t);
  const artifactPath = path.join(noObjectsDirectory, "product.exe");
  fs.writeFileSync(artifactPath, makePeFixture());
  const tools = mockedTools(peReadobjOutput);
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    allowlists: elfObjectBoundaries(),
    findTool: tools.findTool,
    runTool: tools.runTool,
  }), /require at least one supplied object/u);

  const mismatch = elfObjectBoundaries({
    objectSymbolLinkage: [
      { object: "other.o", routeRoots: ["main"], forbiddenGlobalWPrivateHelpers: [] },
      elfObjectBoundaries().objectSymbolLinkage[1],
    ],
  });
  const mismatchReceipt = await inspectElfObjectPair(t, validElfObjectEvidence(), mismatch);
  assert.equal(mismatchReceipt.checks.objectSymbolLinkage.status, "rejected");
  assert.deepEqual(mismatchReceipt.checks.objectSymbolLinkage.missingObjects, ["other.o"]);
  assert.deepEqual(mismatchReceipt.checks.objectSymbolLinkage.unexpectedObjects, ["output.o"]);

  const malformedNm = validElfObjectEvidence();
  malformedNm.nmOutputByObject["output.o"] = "not POSIX nm data";
  await assert.rejects(inspectElfObjectPair(t, malformedNm), /unrecognized POSIX output/u);
  const malformedRelocation = validElfObjectEvidence();
  malformedRelocation.relocationsByObject["output.o"] =
    "output.o: file format elf64-x86-64\nRELOCATION RECORDS FOR [.text]:\nOFFSET TYPE VALUE\nnot-a-relocation\n";
  await assert.rejects(inspectElfObjectPair(t, malformedRelocation), /malformed relocation data/u);
});

test("pre-2 allowlist schema is rejected without a compatibility path and PATH fallback is disabled for linkage claims", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  fs.writeFileSync(artifactPath, makePeFixture());
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    allowlists: { schema: "w-artifact-inspection-allowlists-1", finalDependencies: [] },
    findTool: mockedTools(peReadobjOutput).findTool,
    runTool: mockedTools(peReadobjOutput).runTool,
  }), /schema must be w-artifact-inspection-allowlists-2/u);
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    objectPaths: [path.join(directory, "output.o")],
    allowlists: elfObjectBoundaries(),
  }), /PATH fallback is disabled/u);
});

test("explicit PE import and dependency allowlists are checked as separate closures", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const allowlistsPath = path.join(directory, "allowed.json");
  fs.writeFileSync(artifactPath, makePeFixture());
  const allowlistBytes = Buffer.from(JSON.stringify({
    schema: "w-artifact-inspection-allowlists-2",
    finalImports: [
      { library: "KERNEL32.dll", kind: "regular", symbols: ["ExitProcess", "WriteFile"] },
      { library: "USER32.dll", kind: "delay", symbols: ["MessageBoxW"] },
    ],
    finalDependencies: ["KERNEL32.dll", "USER32.dll"],
  }));
  fs.writeFileSync(allowlistsPath, allowlistBytes);
  const tools = mockedTools(peReadobjOutput);

  const receipt = await createArtifactInspectionReceipt({
    artifactPath,
    allowlistsPath,
    findTool: tools.findTool,
    runTool: tools.runTool,
  });

  assert.equal(receipt.allowlists.status, "observed");
  assert.equal(receipt.allowlists.source, allowlistsPath);
  assert.equal(receipt.allowlists.sha256.length, 64);
  assert.deepEqual(receipt.allowlists.boundaries, ["finalDependencies", "finalImports"]);
  assert.deepEqual(receipt.allowlists.policy.finalDependencies, ["KERNEL32.dll", "USER32.dll"]);
  assert.equal(receipt.checks.finalImportClosure.status, "passed");
  assert.equal(receipt.checks.finalDependencyClosure.status, "passed");
  assert.equal(receipt.checks.requestedClosureValidation.status, "passed");
});

test("unexpected final imports and dependencies are both reported against their own allowlists", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  fs.writeFileSync(artifactPath, makePeFixture());
  const tools = mockedTools(peReadobjOutput);

  const receipt = await createArtifactInspectionReceipt({
    artifactPath,
    allowlists: {
      schema: "w-artifact-inspection-allowlists-2",
      finalImports: [{ library: "KERNEL32.dll", kind: "regular", symbols: ["ExitProcess"] }],
      finalDependencies: ["KERNEL32.dll"],
    },
    findTool: tools.findTool,
    runTool: tools.runTool,
  });

  assert.equal(receipt.checks.finalImportClosure.status, "rejected");
  assert.deepEqual(receipt.checks.finalImportClosure.unexpectedReferences, [
    "KERNEL32.dll!WriteFile (regular)", "USER32.dll (delay)",
  ]);
  assert.equal(receipt.checks.finalDependencyClosure.status, "rejected");
  assert.deepEqual(receipt.checks.finalDependencyClosure.unexpectedReferences, ["USER32.dll"]);
  assert.equal(receipt.checks.requestedClosureValidation.status, "incomplete");
});

test("post-opt IR allowlists report definite violations but never claim closure from the partial scanner", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const irPath = path.join(directory, "after.ll");
  fs.writeFileSync(artifactPath, makePeFixture());
  fs.writeFileSync(irPath, "declare i32 @puts(ptr)\n@runtime_state = external global ptr\n");
  const tools = mockedTools(peReadobjOutput);
  const matching = await createArtifactInspectionReceipt({
    artifactPath,
    postOptIrPath: irPath,
    allowlists: {
      schema: "w-artifact-inspection-allowlists-2",
      postOptIrExternals: { functions: ["puts"], globals: ["runtime_state"] },
    },
    findTool: tools.findTool,
    runTool: tools.runTool,
  });
  assert.equal(matching.checks.postOptExternalClosure.status, "unknown");
  assert.deepEqual(matching.checks.postOptExternalClosure.unexpectedReferences, []);
  assert.match(matching.checks.postOptExternalClosure.reason, /partial parser coverage/u);

  const violating = await createArtifactInspectionReceipt({
    artifactPath,
    postOptIrPath: irPath,
    allowlists: {
      schema: "w-artifact-inspection-allowlists-2",
      postOptIrExternals: { functions: [], globals: [] },
    },
    findTool: tools.findTool,
    runTool: tools.runTool,
  });
  assert.equal(violating.checks.postOptExternalClosure.status, "rejected");
  assert.deepEqual(violating.checks.postOptExternalClosure.unexpectedReferences, [
    "function:puts", "global:runtime_state",
  ]);
});

test("requested closure boundaries stay non-passing when their evidence is missing or unsupported", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.elf");
  fs.writeFileSync(artifactPath, makeElfFixture());
  const elfOutput = JSON.stringify([{
    FileSummary: { Format: "elf64-x86-64" },
    Sections: [{ Section: { Name: { Name: ".text" }, Type: { Name: "SHT_PROGBITS" }, Offset: 64, Size: 20 } }],
    DynamicSection: [{ Type: "NEEDED", Library: "libc.so.6" }],
    NeededLibraries: ["libc.so.6"],
  }]);
  const tools = mockedTools(elfOutput);

  const receipt = await createArtifactInspectionReceipt({
    artifactPath,
    allowlists: {
      schema: "w-artifact-inspection-allowlists-2",
      postOptIrExternals: { functions: [], globals: [] },
      objectUndefinedSymbols: [],
      finalImports: [],
      finalDependencies: ["libc.so.6"],
    },
    findTool: tools.findTool,
    runTool: tools.runTool,
  });

  assert.equal(receipt.checks.postOptExternalClosure.status, "unknown");
  assert.match(receipt.checks.postOptExternalClosure.reason, /was not supplied/u);
  assert.equal(receipt.checks.suppliedObjectUndefinedSymbolClosure.status, "unknown");
  assert.match(receipt.checks.suppliedObjectUndefinedSymbolClosure.reason, /no object files/u);
  assert.equal(receipt.checks.finalImportClosure.status, "unknown");
  assert.match(receipt.checks.finalImportClosure.reason, /not enumerated/u);
  assert.equal(receipt.checks.finalDependencyClosure.status, "passed");
  assert.equal(receipt.checks.requestedClosureValidation.status, "incomplete");
  assert.deepEqual(receipt.checks.requestedClosureValidation.nonPassingBoundaries, [
    "finalImports", "objectUndefinedSymbols", "postOptIrExternals",
  ]);
});

test("malformed allowlists, duplicate object paths, and conflicting ELF dependency inventories fail closed", async (t) => {
  const directory = withTempDirectory(t);
  const pePath = path.join(directory, "sample.exe");
  const objectPath = path.join(directory, "sample.obj");
  const elfPath = path.join(directory, "sample.elf");
  fs.writeFileSync(pePath, makePeFixture());
  fs.writeFileSync(objectPath, "object fixture");
  fs.writeFileSync(elfPath, makeElfFixture());
  const tools = mockedTools(peReadobjOutput);

  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath: pePath,
    allowlists: { schema: "w-artifact-inspection-allowlists-2", objectUndefinedSymbols: ["same", "same"] },
    findTool: tools.findTool,
    runTool: tools.runTool,
  }), /duplicate symbol/u);
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath: pePath,
    objectPaths: [objectPath, objectPath],
    findTool: tools.findTool,
    runTool: tools.runTool,
  }), /supplied more than once/u);
  const incompleteImportTools = mockedTools(`${peReadobjOutput}Import {\n`);
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath: pePath,
    findTool: incompleteImportTools.findTool,
    runTool: incompleteImportTools.runTool,
  }), /malformed Import block/u);

  const conflictingElfOutput = JSON.stringify([{
    FileSummary: { Format: "elf64-x86-64" },
    Sections: [{ Section: { Name: { Name: ".text" }, Type: { Name: "SHT_PROGBITS" }, Offset: 64, Size: 20 } }],
    DynamicSection: [{ Type: "NEEDED", Library: "libc.so.6" }],
    NeededLibraries: ["libpthread.so.0"],
  }]);
  const conflictingTools = mockedTools(conflictingElfOutput);
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath: elfPath,
    findTool: conflictingTools.findTool,
    runTool: conflictingTools.runTool,
  }), /conflicting DT_NEEDED inventories/u);
  const ambiguousElfTools = mockedTools(JSON.stringify([
    { FileSummary: { Format: "elf64-x86-64" }, Sections: [] },
    { FileSummary: { Format: "elf64-x86-64" }, Sections: [] },
  ]));
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath: elfPath,
    findTool: ambiguousElfTools.findTool,
    runTool: ambiguousElfTools.runTool,
  }), /ambiguous ELF file-record count/u);
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

test("explicit toolchain directory uses only validated archive basenames and records selected paths", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const toolchainDir = path.join(directory, "toolchain");
  const binDirectory = path.join(toolchainDir, "bin");
  fs.mkdirSync(binDirectory, { recursive: true });
  fs.writeFileSync(artifactPath, makePeFixture());
  const objectPath = path.join(directory, "sample.obj");
  fs.writeFileSync(objectPath, "mock object");
  const toolPaths = Object.fromEntries(["llvm-readobj", "llvm-objdump", "llvm-nm"].map((name) => {
    const toolPath = path.join(binDirectory, `${name}.exe`);
    fs.writeFileSync(toolPath, "mock executable");
    return [name, toolPath];
  }));
  const tools = mockedTools(peReadobjOutput);
  const fallbackLookups = [];
  const validatedDirectories = [];

  const receipt = await createArtifactInspectionReceipt({
    artifactPath,
    objectPath,
    toolchainDir,
    findTool: (name) => {
      fallbackLookups.push(name);
      return `PATH/${name}.exe`;
    },
    runTool: tools.runTool,
    loadMaterialization: async (validatedDirectory) => {
      validatedDirectories.push(validatedDirectory);
      return {
        archiveEntries: Object.entries(toolPaths).map(([name]) => ({
          path: `bin/${name}.exe`,
          type: "file",
        })),
      };
    },
  });

  assert.deepEqual(validatedDirectories, [toolchainDir]);
  assert.deepEqual(fallbackLookups, []);
  assert.equal(receipt.tools.resolution, "validated-materialized-toolchain");
  assert.equal(receipt.tools.toolchainDirectory, toolchainDir);
  assert.equal(receipt.tools.llvmReadobj, toolPaths["llvm-readobj"]);
  assert.equal(receipt.tools.llvmObjdump, toolPaths["llvm-objdump"]);
  assert.equal(receipt.tools.llvmNm, toolPaths["llvm-nm"]);
});

test("explicit toolchain directory rejects a symlinked inspection tool", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const toolchainDir = path.join(directory, "toolchain");
  const binDirectory = path.join(toolchainDir, "bin");
  fs.mkdirSync(binDirectory, { recursive: true });
  fs.writeFileSync(artifactPath, makePeFixture());
  const targetPath = path.join(binDirectory, "llvm-readobj-target.exe");
  const linkPath = path.join(binDirectory, "llvm-readobj.exe");
  const objdumpPath = path.join(binDirectory, "llvm-objdump.exe");
  fs.writeFileSync(targetPath, "mock executable");
  fs.writeFileSync(objdumpPath, "mock executable");
  try {
    fs.symlinkSync(targetPath, linkPath, "file");
  } catch (error) {
    if (["EPERM", "EACCES", "ENOSYS"].includes(error?.code)) {
      t.skip("symbolic links are unavailable in this environment");
      return;
    }
    throw error;
  }

  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    toolchainDir,
    loadMaterialization: async () => ({
      archiveEntries: [
        { path: "bin/llvm-readobj.exe", type: "file" },
        { path: "bin/llvm-objdump.exe", type: "file" },
      ],
    }),
  }), /materialized tool is not a regular file: llvm-readobj\.exe/u);
});

test("explicit toolchain directory rejects ambiguous and non-regular tool basenames", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const toolchainDir = path.join(directory, "toolchain");
  const firstBin = path.join(toolchainDir, "one");
  const secondBin = path.join(toolchainDir, "two");
  fs.mkdirSync(firstBin, { recursive: true });
  fs.mkdirSync(secondBin, { recursive: true });
  fs.writeFileSync(artifactPath, makePeFixture());
  for (const bin of [firstBin, secondBin]) {
    fs.writeFileSync(path.join(bin, "llvm-readobj.exe"), "mock executable");
    fs.writeFileSync(path.join(bin, "llvm-objdump.exe"), "mock executable");
  }
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    toolchainDir,
    loadMaterialization: async () => ({
      archiveEntries: [
        { path: "one/llvm-readobj.exe", type: "file" },
        { path: "two/llvm-readobj.exe", type: "file" },
        { path: "one/llvm-objdump.exe", type: "file" },
      ],
    }),
  }), /tool basename is ambiguous for llvm-readobj\.exe/u);

  const regular = path.join(firstBin, "llvm-readobj.exe");
  fs.rmSync(regular);
  fs.mkdirSync(regular);
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    toolchainDir,
    loadMaterialization: async () => ({
      archiveEntries: [
        { path: "one/llvm-readobj.exe", type: "file" },
        { path: "one/llvm-objdump.exe", type: "file" },
      ],
    }),
  }), /materialized tool is not a regular file: llvm-readobj\.exe/u);
});

test("explicit toolchain directory does not fall back to PATH for a missing optional tool", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "sample.exe");
  const toolchainDir = path.join(directory, "toolchain");
  const binDirectory = path.join(toolchainDir, "bin");
  fs.mkdirSync(binDirectory, { recursive: true });
  fs.writeFileSync(artifactPath, makePeFixture());
  for (const name of ["llvm-readobj", "llvm-objdump"])
    fs.writeFileSync(path.join(binDirectory, `${name}.exe`), "mock executable");
  const tools = mockedTools(peReadobjOutput);
  const fallbackLookups = [];
  const objectPath = path.join(directory, "sample.obj");
  fs.writeFileSync(objectPath, "mock object");

  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath,
    objectPath,
    toolchainDir,
    findTool: (name) => {
      fallbackLookups.push(name);
      return `PATH/${name}.exe`;
    },
    runTool: tools.runTool,
    loadMaterialization: async () => ({
      archiveEntries: [
        { path: "bin/llvm-readobj.exe", type: "file" },
        { path: "bin/llvm-objdump.exe", type: "file" },
      ],
    }),
  }), /llvm-nm is required but was not found in the validated --toolchain-dir/u);
  assert.deepEqual(fallbackLookups, []);
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

test("oversized artifact and allowlist inputs are rejected before they are read into memory", async (t) => {
  const directory = withTempDirectory(t);
  const artifactPath = path.join(directory, "oversized.exe");
  const artifactHandle = fs.openSync(artifactPath, "w");
  try {
    fs.ftruncateSync(artifactHandle, 128 * 1024 * 1024 + 1);
  } finally {
    fs.closeSync(artifactHandle);
  }
  await assert.rejects(createArtifactInspectionReceipt({ artifactPath }), /128 MiB input safety limit/u);

  const allowlistsPath = path.join(directory, "oversized.json");
  const allowlistHandle = fs.openSync(allowlistsPath, "w");
  try {
    fs.ftruncateSync(allowlistHandle, 1024 * 1024 + 1);
  } finally {
    fs.closeSync(allowlistHandle);
  }
  await assert.rejects(createArtifactInspectionReceipt({
    artifactPath: path.join(directory, "missing.exe"),
    allowlistsPath,
  }), /allowlists exceeds the 1 MiB input safety limit/u);
});
