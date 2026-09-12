import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import test from "node:test";
import { mkdir, mkdtemp, readFile, readdir, rm, rmdir, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import {
  RESULTS_DIRECTORY,
  acquireExecutableBenchmarkLease,
  assertOracle,
  defaultExecutor,
  deriveSummary,
  EXECUTABLE_CHILD_KILL_SIGNAL,
  EXECUTABLE_CHILD_TIMEOUT_MS,
  nativeReceiptSamples,
  parseBenchmarkArguments,
  publishRecord,
  resolveResultPath,
  runBenchmark,
  validatePeX64,
} from "./executable-benchmark-runner.mjs";
import {
  C_RELEASE_FLAGS,
  C_WHOLE_PROGRAM_FLAG,
  CLANG_C_TARGET,
  CLANG_RELEASE_FLAGS,
  NATIVE_RECIPE_PROFILE,
  RUST_RELEASE_FLAGS,
  cReleaseFlags,
  W_LLC_FLAGS,
  W_LLD_LINK_FLAGS,
  W_MLIR_OPT_FLAGS,
} from "./executable-release-recipes.mjs";

test("benchmark arguments separate compile cost from high-resolution run sampling", () => {
  assert.deepEqual(parseBenchmarkArguments([]), {
    target: "hello", language: "w", output: undefined, warmup: 1, compileSamples: 9, runSamples: 101, help: false,
  });
  assert.deepEqual(parseBenchmarkArguments(["--target", "hello", "--language", "c", "--output", "benchmarks/results/hello-c.local.json", "--warmup", "2", "--samples", "11"]), {
    target: "hello", language: "c", output: "benchmarks/results/hello-c.local.json", warmup: 2, compileSamples: 11, runSamples: 11, help: false,
  });
  assert.deepEqual(parseBenchmarkArguments(["--language=rust"]), {
    target: "hello", language: "rust", output: undefined, warmup: 1, compileSamples: 9, runSamples: 101, help: false,
  });
  assert.throws(() => parseBenchmarkArguments(["--language", "swift"]), /unsupported/);
  assert.throws(() => parseBenchmarkArguments(["--samples", "10"]), /odd/);
  assert.throws(() => parseBenchmarkArguments(["--run-samples", "100"]), /odd/);
  assert.throws(() => parseBenchmarkArguments(["--run-samples", "1003"]), /between 9 and 1001/);
  assert.throws(() => parseBenchmarkArguments(["--warmup", "0"]), /between 1/);
  assert.deepEqual(parseBenchmarkArguments(["--target", "restaurant-branch", "--language", "rust"]), {
    target: "restaurant-branch", language: "rust", output: undefined, warmup: 1, compileSamples: 9, runSamples: 101, help: false,
  });
  assert.deepEqual(parseBenchmarkArguments(["--target", "process-handler-lifecycle", "--language", "c"]), {
    target: "process-handler-lifecycle", language: "c", output: undefined, warmup: 1, compileSamples: 9, runSamples: 101, help: false,
  });
  assert.deepEqual(parseBenchmarkArguments(["--target", "process-entry", "--language", "rust"]), {
    target: "process-entry", language: "rust", output: undefined, warmup: 1, compileSamples: 9, runSamples: 101, help: false,
  });
  assert.throws(() => parseBenchmarkArguments(["--target", "process-entry0", "--language", "c"]), /unsupported benchmark target/);
  assert.throws(() => parseBenchmarkArguments(["--target", "restaurant-composition"]), /unsupported/);
});

test("Clang MSVC release flags select the DLL runtime and COFF linker controls", () => {
  assert.ok(CLANG_RELEASE_FLAGS.includes("-fms-runtime-lib=dll"));
  assert.ok(CLANG_RELEASE_FLAGS.includes("-fuse-ld=lld"));
  assert.ok(CLANG_RELEASE_FLAGS.includes("-Wl,/OPT:REF"));
  assert.ok(CLANG_RELEASE_FLAGS.includes("-Wl,/OPT:ICF"));
  assert.ok(CLANG_RELEASE_FLAGS.includes("-Wl,/DEBUG:NONE"));
  assert.equal(CLANG_RELEASE_FLAGS.includes("-s"), false, "Clang MSVC ignores the GNU -s driver flag");
  assert.equal(CLANG_RELEASE_FLAGS.some((flag) => flag.includes("--gc-sections")), false,
    "Clang MSVC uses /OPT:REF instead of the ELF/MinGW --gc-sections spelling");
});

test("checkout lease rejects concurrent benchmark runs and is reusable after release", async () => {
  const root = await mkdtemp(path.join(os.tmpdir(), "w-benchmark-lease-test-"));
  try {
    const release = await acquireExecutableBenchmarkLease(root);
    await assert.rejects(() => acquireExecutableBenchmarkLease(root),
      /another executable benchmark is already running/u);
    await release();
    const releaseAgain = await acquireExecutableBenchmarkLease(root);
    await releaseAgain();
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("default executor enforces the Bun child timeout and preserves termination metadata", () => {
  const result = defaultExecutor(process.execPath, [
    "-e",
    "Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 1000)",
  ], { timeout: 10 });
  assert.equal(result.exitedDueToTimeout, true);
  assert.equal(result.signalCode, EXECUTABLE_CHILD_KILL_SIGNAL);
  assert.equal(result.exitCode, null);
  assert.ok(result.resourceUsage);
  assert.equal(EXECUTABLE_CHILD_TIMEOUT_MS, 120_000);
});

test("release recipes prioritize runtime and strip distributable symbols", () => {
  assert.deepEqual(C_RELEASE_FLAGS, ["-O3", "-flto", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-s"]);
  assert.equal(C_WHOLE_PROGRAM_FLAG, "-fwhole-program");
  assert.equal(CLANG_C_TARGET, "x86_64-pc-windows-msvc");
  assert.deepEqual(CLANG_RELEASE_FLAGS, ["-O3", "-flto=full", "-ffunction-sections", "-fdata-sections", "-fuse-ld=lld", "-fms-runtime-lib=dll", "-Wl,/Brepro", "-Wl,/OPT:REF", "-Wl,/OPT:ICF", "-Wl,/INCREMENTAL:NO", "-Wl,/DEBUG:NONE"]);
  assert.deepEqual(cReleaseFlags(), C_RELEASE_FLAGS);
  assert.deepEqual(cReleaseFlags({ wholeProgram: true }), [...C_RELEASE_FLAGS, C_WHOLE_PROGRAM_FLAG]);
  assert.equal(NATIVE_RECIPE_PROFILE, "release-native");
  assert.deepEqual(RUST_RELEASE_FLAGS, ["-C", "opt-level=3", "-C", "lto=fat", "-C", "codegen-units=1", "-C", "panic=abort", "-C", "debuginfo=0", "-C", "strip=symbols", "-C", "link-dead-code=no", "-C", "link-arg=/OPT:REF", "-C", "link-arg=/OPT:ICF", "-C", "link-arg=/INCREMENTAL:NO", "-C", "link-arg=/DEBUG:NONE"]);
  assert.equal(RUST_RELEASE_FLAGS.includes("incremental=off"), false, "rustc treats this as an output directory rather than disabling incremental compilation");
  assert.deepEqual(W_MLIR_OPT_FLAGS, ["--verify-each", "--canonicalize", "--cse"]);
  assert.ok(W_LLC_FLAGS.includes("-O3"));
  assert.ok(W_LLD_LINK_FLAGS.includes("/opt:ref"));
  assert.ok(W_LLD_LINK_FLAGS.includes("/opt:icf"));
  const all = [...C_RELEASE_FLAGS, ...CLANG_RELEASE_FLAGS, ...RUST_RELEASE_FLAGS, ...W_LLC_FLAGS, ...W_LLD_LINK_FLAGS];
  assert.equal(all.some((flag) => /(?:^|=)(?:s|z)$|native/iu.test(flag)), false);
});

test("native receipts map Job CPU and root working set without relabeling commit as RSS", () => {
  const receipt = {
    schema: "w-native-benchmark/2",
    status: "ok",
    warmupCount: 0,
    sampleCount: 1,
    oracle: true,
    samples: [{
      wallNs: 101,
      directProcessUserCpuNs: 1200,
      directProcessKernelCpuNs: 2300,
      directProcessCpuNs: 3500,
      jobUserCpuNs: 4500,
      jobKernelCpuNs: 6700,
      jobCpuNs: 11200,
      peakDirectWorkingSetBytes: 8192,
      peakJobCommitBytes: 16384,
      exitCode: 0,
      stdoutBytes: 0,
      stderrBytes: 0,
    }],
    summary: {},
    measurement: "bounded native test receipt",
  };
  assert.deepEqual(nativeReceiptSamples(receipt, 1), [{
    wallNs: "101",
    cpuUserUs: "4",
    cpuSystemUs: "6",
    cpuTotalUs: "10",
    peakRssBytes: "8192",
  }]);
  const inconsistent = structuredClone(receipt);
  inconsistent.samples[0].jobCpuNs = 11201;
  assert.throws(() => nativeReceiptSamples(inconsistent, 1), /inconsistent native measurement/u);
});

test("summary arithmetic means use integer floor and preserve zero CPU", () => {
  const raw = Array.from({ length: 9 }, (_, index) => ({
    wallNs: String(index + 1),
    cpuUserUs: "0",
    cpuSystemUs: "1",
    cpuTotalUs: "1",
    peakRssBytes: String(index + 100),
  }));
  const summary = deriveSummary(raw);
  assert.equal(summary.wallNs.median, "5");
  assert.equal(summary.wallNs.arithmeticMean, "5");
  assert.equal(summary.cpuUserUs.arithmeticMean, "0");
  assert.equal(summary.peakRssBytes.mad, "2");
});

const TEST_DIGEST = "sha256:" + "1".repeat(64);
const TEST_COMMIT = "a".repeat(40);
const TEST_ENVIRONMENT = {
  os: "windows",
  kernel: "windows-nt",
  cpuModel: "x86_64-test-cpu",
  logicalCores: "8",
  ramBytes: "17179869184",
};
const EXPECTED_PE_ARTIFACT_CLEANLINESS = {
  coffSymbols: { pointer: "0", count: "0" },
  codeView: { count: "0", sizeBytes: "0" },
  debugDirectory: { presence: "absent", sizeBytes: "0", entries: [] },
  certificateDirectory: { pointer: "0", sizeBytes: "0" },
  sectionData: "in-bounds",
  sidecars: { count: "0" },
  overlay: { sizeBytes: "0" },
};
const { sidecars: _sidecars, ...EXPECTED_PE_IMAGE_CLEANLINESS } = EXPECTED_PE_ARTIFACT_CLEANLINESS;
const EXPECTED_PE_LAYOUT = {
  fileAlignment: "512",
  sectionAlignment: "4096",
  sizeOfHeaders: "512",
  sections: [{ name: ".text0", virtualSize: "512", rawSize: "512" }],
};
const EXPECTED_PE_IMAGE = {
  cleanliness: EXPECTED_PE_IMAGE_CLEANLINESS,
  peLayout: EXPECTED_PE_LAYOUT,
};

function fakePeX64({
  symbolTablePointer = 0,
  symbolCount = 0,
  debugRva = 0,
  debugSize = 0,
  certificatePointer = 0,
  certificateSize = 0,
  overlay = Buffer.alloc(0),
  rawPointer = 0x200,
  rawSize = 0x200,
  virtualSize = rawSize,
  virtualAddress = 0x1000,
  sectionCount = 1,
  optionalHeaderSize = 0xf0,
  directoryCount = 16,
  sizeOfHeaders = 0x200,
  fileAlignment = 0x200,
  sectionAlignment = 0x1000,
  debugType,
  debugPayloadSize = 16,
  debugPayloadRva = 0x1040,
  debugPayloadPointer = 0x240,
} = {}) {
  const peOffset = 0x80;
  const fileHeader = peOffset + 4;
  const optionalHeader = fileHeader + 20;
  const sectionTable = optionalHeader + optionalHeaderSize;
  const sectionTableEnd = sectionTable + sectionCount * 40;
  const rawEnd = rawPointer + rawSize;
  const bytes = Buffer.alloc(Math.max(sizeOfHeaders, sectionTableEnd, rawEnd) + overlay.length);
  bytes[0] = 0x4d;
  bytes[1] = 0x5a;
  bytes.writeUInt32LE(peOffset, 0x3c);
  bytes.writeUInt32LE(0x00004550, peOffset);
  bytes.writeUInt16LE(0x8664, fileHeader);
  bytes.writeUInt16LE(sectionCount, fileHeader + 2);
  bytes.writeUInt32LE(symbolTablePointer, fileHeader + 8);
  bytes.writeUInt32LE(symbolCount, fileHeader + 12);
  bytes.writeUInt16LE(optionalHeaderSize, fileHeader + 16);
  bytes.writeUInt16LE(0x20b, optionalHeader);
  bytes.writeUInt32LE(sectionAlignment, optionalHeader + 32);
  bytes.writeUInt32LE(fileAlignment, optionalHeader + 36);
  bytes.writeUInt32LE(sizeOfHeaders, optionalHeader + 60);
  bytes.writeUInt32LE(directoryCount, optionalHeader + 108);
  bytes.writeUInt32LE(debugRva, optionalHeader + 112 + 6 * 8);
  bytes.writeUInt32LE(debugSize, optionalHeader + 112 + 6 * 8 + 4);
  bytes.writeUInt32LE(certificatePointer, optionalHeader + 112 + 4 * 8);
  bytes.writeUInt32LE(certificateSize, optionalHeader + 112 + 4 * 8 + 4);
  for (let index = 0; index < sectionCount; index += 1) {
    const section = sectionTable + index * 40;
    Buffer.from(`.text${index}\0`, "ascii").copy(bytes, section, 0, 8);
    if (index === 0) {
      bytes.writeUInt32LE(virtualSize, section + 8);
      bytes.writeUInt32LE(virtualAddress, section + 12);
      bytes.writeUInt32LE(rawSize, section + 16);
      bytes.writeUInt32LE(rawPointer, section + 20);
    }
  }
  if (debugType !== undefined) {
    const debugEntry = rawPointer;
    bytes.writeUInt32LE(0x1000, optionalHeader + 112 + 6 * 8);
    bytes.writeUInt32LE(28, optionalHeader + 112 + 6 * 8 + 4);
    bytes.writeUInt32LE(debugType, debugEntry + 12);
    bytes.writeUInt32LE(debugPayloadSize, debugEntry + 16);
    bytes.writeUInt32LE(debugPayloadRva, debugEntry + 20);
    bytes.writeUInt32LE(debugPayloadPointer, debugEntry + 24);
  }
  if (overlay.length > 0) Buffer.from(overlay).copy(bytes, rawEnd);
  return bytes;
}

function fakeResourceUsage() {
  return { cpuTime: { user: 1, system: 1 }, maxRSS: 4096 };
}

function fakeRunnerExecutor({ language, mismatch = false, target = "hello", timeoutMode = undefined, symbolSidecar = false, peOptions = {} }) {
  const privateC = language === "c" && target === "process-handler-lifecycle";
  const compiler = path.resolve(`fake-${language === "c" ? privateC ? "gcc" : "clang" : "rustc"}.exe`);
  const calls = [];
  const sampleDirectories = new Set();
  const executor = async (command, args, options = {}) => {
    calls.push({ command, args: [...args], cwd: options.cwd, stdin: options.stdin, env: options.env, timeout: options.timeout, killSignal: options.killSignal });
    const timedOut = () => ({
      exitCode: null,
      signalCode: "SIGKILL",
      exitedDueToTimeout: true,
      stdout: Buffer.alloc(0),
      stderr: Buffer.alloc(0),
      resourceUsage: fakeResourceUsage(),
    });
    if (timeoutMode === "probe" && args.length === 1 && args[0] === "-dumpmachine") return timedOut();
    if (args.length === 1 && args[0] === "-dumpmachine") {
      return { exitCode: 0, stdout: privateC ? "x86_64-w64-mingw32" : CLANG_C_TARGET, stderr: Buffer.alloc(0) };
    }
    if (language === "c" && args.includes("-fsyntax-only")) {
      const accepted = privateC ? args.includes("-std=c2x") : args.includes("-std=c23");
      return {
        exitCode: accepted ? 0 : 1,
        stdout: Buffer.alloc(0),
        stderr: Buffer.from(accepted ? "" : "dialect unavailable\n", "utf8"),
      };
    }
    if (language === "c" && args.length === 1 && args[0] === "--version") {
      return { exitCode: 0, stdout: privateC ? "gcc (GCC) 13.2.0\n" : "clang version 22.1.8\n", stderr: Buffer.alloc(0) };
    }
    if (language === "rust" && args.includes("--print") && args.includes("target-libdir")) {
      return { exitCode: 0, stdout: "C:\\Rust\\lib\\rustlib\\x86_64-pc-windows-msvc\\lib\n", stderr: Buffer.alloc(0) };
    }
    if (language === "rust" && args.includes("--version") && args.includes("--verbose")) {
      return {
        exitCode: 0,
        stdout: "rustc 1.94.0 (2026-02-01)\nrelease: 1.94.0\nhost: x86_64-pc-windows-msvc\n",
        stderr: Buffer.alloc(0),
      };
    }
    const outputIndex = args.indexOf("-o");
    if (outputIndex >= 0) {
      const artifact = args[outputIndex + 1];
      sampleDirectories.add(options.cwd);
      await writeFile(artifact, fakePeX64(peOptions));
      if (symbolSidecar) {
        await writeFile(artifact.replace(/\.exe$/iu, ".pdb"), Buffer.from("debug symbols", "utf8"));
      }
      return { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0), resourceUsage: fakeResourceUsage() };
    }
    if (target === "process-entry") {
      return {
        exitCode: args.length === 0 ? 2 : 0,
        stdout: Buffer.from(args.length === 0 ? "missing\n" : "received\n", "utf8"),
        stderr: Buffer.alloc(0),
        resourceUsage: fakeResourceUsage(),
      };
    }
    if (args.length === 0) {
      if (timeoutMode === "run") return timedOut();
      return {
        exitCode: 0,
        stdout: mismatch ? "Not the oracle\n" : target === "restaurant-branch" ? "Kitchen open\nAfter service\nKitchen closed\nAfter service\n" : "Hello, world!\n",
        stderr: Buffer.alloc(0),
        resourceUsage: fakeResourceUsage(),
      };
    }
    throw new Error(`unexpected fake executor invocation: ${command} ${args.join(" ")}`);
  };
  return { compiler, calls, sampleDirectories, executor };
}

function fakeWRunnerExecutor(target, { symbolSidecar = false, peOptions = {} } = {}) {
  const calls = [];
  const sampleDirectories = new Set();
  const publicW = {
    executable: path.resolve("fake-w.exe"),
    digest: TEST_DIGEST,
    receiptDigest: TEST_DIGEST,
    compilerVersion: "19.51.36256.0",
  };
  const toolNames = ["mlir-opt.exe", "mlir-translate.exe", "llc.exe", "lld-link.exe"];
  const windowsToolchain = {
    manifestDigest: TEST_DIGEST,
    materialized: {
      tools: Object.fromEntries(toolNames.map((name) => [name, {
        relativePath: `fake/${name}`,
        sizeBytes: "1",
        sha256: TEST_DIGEST,
        version: "23.1.1",
      }])),
    },
    tools: Object.fromEntries(toolNames.map((name) => [name, path.resolve(`fake-${name}`)])),
    sdk: { path: path.resolve("fake-kernel32.lib") },
  };
  const executor = async (command, args, options = {}) => {
    calls.push({ command, args: [...args], cwd: options.cwd, timeout: options.timeout, killSignal: options.killSignal });
    const result = (exitCode, stdout = Buffer.alloc(0), stderr = Buffer.alloc(0)) => ({
      exitCode, stdout, stderr, resourceUsage: fakeResourceUsage(),
    });
    if (args.length === 0) {
      return target === "process-entry"
        ? result(2, Buffer.from("missing\n", "utf8"))
        : result(0, Buffer.from(target === "restaurant-branch" ? "Kitchen open\nAfter service\nKitchen closed\nAfter service\n" : "Hello, world!\n", "utf8"));
    }
    if (command === publicW.executable && args[0] === "build") {
      const source = target === "restaurant-branch"
        ? "compiler/seed-c/fixtures/restaurant-if.w"
        : target === "process-entry" ? "compiler/seed-c/fixtures/process-input0.w" : "benchmarks/executable/hello.w";
      assert.deepEqual(args.slice(0, 6), ["build", path.resolve(source), "--target", "x86_64-pc-windows-msvc", "--output", args[5]]);
      const output = args[5];
      sampleDirectories.add(options.cwd);
      await writeFile(output, fakePeX64(peOptions));
      if (symbolSidecar) await writeFile(output.replace(/\.exe$/iu, ".pdb"), Buffer.from("debug symbols", "utf8"));
      return result(0);
    }
    if (target === "process-entry") {
      return result(args.length === 0 ? 2 : 0, Buffer.from(args.length === 0 ? "missing\n" : "received\n", "utf8"));
    }
    throw new Error(`unexpected fake W executor invocation: ${command} ${args.join(" ")}`);
  };
  return { calls, sampleDirectories, executor, publicW, windowsToolchain };
}

function fakeRunnerDependencies(language, fake) {
  return {
    executor: fake.executor,
    commit: TEST_COMMIT,
    environment: TEST_ENVIRONMENT,
    runnerDigest: TEST_DIGEST,
    catalogDigest: TEST_DIGEST,
    testOnly: true,
    testOnlyPlatform: { platform: "win32", arch: "x64" },
    testOnlyToolchains: { [language]: { command: fake.compiler } },
    ...(language === "c" ? { testOnlyCEnvironment: { PATH: "C:\\fake" } } : {}),
  };
}

function fakeProcessRunnerExecutor() {
  const fake = fakeRunnerExecutor({ language: "c", target: "process-handler-lifecycle" });
  const baseExecutor = fake.executor;
  fake.executor = async (command, args, options = {}) => {
    if (args.includes("-o")) return baseExecutor(command, args, options);
    if (command === fake.compiler) return baseExecutor(command, args, options);
    fake.calls.push({ command, args: [...args], cwd: options.cwd, stdin: options.stdin, env: options.env, timeout: options.timeout, killSignal: options.killSignal });
    const fault = options.env?.W_SEED_PROCESS_ENTRY0_FAULT;
    if (path.extname(command).toLowerCase() === ".exe") {
      const exitCode = fault === "missing" || fault === "noop-success" ? 11 : fault ? 29 : 0;
      return { exitCode, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0), resourceUsage: fakeResourceUsage() };
    }
    return baseExecutor(command, args, options);
  };
  return fake;
}

function assertNoFakeSampleDirectories(fake) {
  for (const directory of fake.sampleDirectories) assert.equal(existsSync(directory), false, `temporary sample remains: ${directory}`);
}

test("bounded PE verifier accepts clean PE32+ and rejects symbol, debug, certificate, overlay and malformed metadata", () => {
  for (const language of ["w", "c", "rust"]) {
    assert.deepEqual(validatePeX64(fakePeX64(), language), EXPECTED_PE_IMAGE);
    assert.deepEqual(validatePeX64(fakePeX64({ debugType: 13 }), language), {
      ...EXPECTED_PE_IMAGE,
      cleanliness: {
        ...EXPECTED_PE_IMAGE_CLEANLINESS,
        debugDirectory: {
          presence: "pogo-only",
          sizeBytes: "28",
          entries: [{ type: "pogo", typeCode: 13, sizeBytes: "16" }],
        },
      },
    });
    assert.deepEqual(validatePeX64(fakePeX64({ debugType: 16, debugPayloadSize: 0, debugPayloadRva: 0, debugPayloadPointer: 0 }), language), {
      ...EXPECTED_PE_IMAGE,
      cleanliness: {
        ...EXPECTED_PE_IMAGE_CLEANLINESS,
        debugDirectory: {
          presence: "repro-only",
          sizeBytes: "28",
          entries: [{ type: "repro", typeCode: 16, sizeBytes: "0" }],
        },
      },
    });
    const codeViewBytesInSection = fakePeX64();
    Buffer.from("RSDS", "ascii").copy(codeViewBytesInSection, 0x200);
    assert.doesNotThrow(() => validatePeX64(codeViewBytesInSection, language), `${language} section bytes are not a debug directory`);

    for (const [options, message] of [
      [{ symbolTablePointer: 0x200 }, /COFF symbol table/u],
      [{ symbolCount: 1 }, /COFF symbol table/u],
      [{ debugRva: 0x200 }, /PE debug data/u],
      [{ debugSize: 28 }, /PE debug data/u],
      [{ debugType: 2 }, /CodeView debug data/u],
      [{ debugType: 1 }, /unsupported PE debug data type 1/u],
      [{ debugType: 13, debugPayloadPointer: 0 }, /invalid POGO debug payload/u],
      [{ debugType: 13, debugPayloadRva: 0x2000 }, /invalid POGO debug payload 0 RVA range/u],
      [{ debugType: 16, debugPayloadSize: 1 }, /invalid REPRO debug marker/u],
      [{ debugType: 16, debugPayloadPointer: 0x240 }, /invalid REPRO debug marker/u],
      [{ certificatePointer: 0x400 }, /PE certificate directory/u],
      [{ certificateSize: 1 }, /PE certificate directory/u],
      [{ overlay: Buffer.from("RSDS synthetic CodeView overlay", "ascii") }, /overlay bytes/u],
      [{ directoryCount: 6 }, /invalid PE data-directory count/u],
      [{ directoryCount: 17 }, /invalid PE data-directory count/u],
      [{ sectionCount: 0 }, /no PE sections/u],
    ]) {
      assert.throws(() => validatePeX64(fakePeX64(options), language), message, `${language} must reject ${message}`);
    }

    const invalidOffset = fakePeX64();
    invalidOffset.writeUInt32LE(0xfffffff0, 0x3c);
    assert.throws(() => validatePeX64(invalidOffset, language), /truncated or invalid PE\/COFF header/u);

    const shortOptionalHeader = fakePeX64();
    shortOptionalHeader.writeUInt16LE(0xa0, 0x94);
    assert.throws(() => validatePeX64(shortOptionalHeader, language), /optional header is too small/u);

    const shortSectionTable = fakePeX64({ optionalHeaderSize: 0x180 }).subarray(0, 0x230);
    assert.throws(() => validatePeX64(shortSectionTable, language), /truncated or invalid PE section table/u);

    const outOfBoundsSection = fakePeX64();
    outOfBoundsSection.writeUInt32LE(0x1000, 0x198);
    assert.throws(() => validatePeX64(outOfBoundsSection, language), /truncated or invalid PE section 0 raw data/u);

    const distinctSectionSizes = validatePeX64(fakePeX64({ virtualSize: 0x180 }), language);
    assert.deepEqual(distinctSectionSizes.peLayout.sections[0], { name: ".text0", virtualSize: "384", rawSize: "512" });
    assert.equal(distinctSectionSizes.cleanliness.sectionData, "in-bounds");

    const lowAlignment = fakePeX64({ fileAlignment: 0x200, sectionAlignment: 0x200, virtualAddress: 0x200 });
    assert.doesNotThrow(() => validatePeX64(lowAlignment, language), `${language} must accept a valid low-alignment PE`);
    assert.deepEqual(validatePeX64(lowAlignment, language).peLayout, {
      ...EXPECTED_PE_LAYOUT,
      sectionAlignment: "512",
    });
    const lowerAlignment = fakePeX64({ fileAlignment: 0x100, sectionAlignment: 0x100, virtualAddress: 0x200 });
    assert.doesNotThrow(() => validatePeX64(lowerAlignment, language), `${language} must accept equal low alignments below 512`);
    assert.equal(validatePeX64(lowerAlignment, language).peLayout.fileAlignment, "256");

    const invalidAlignment = fakePeX64();
    invalidAlignment.writeUInt32LE(0x300, 0x98 + 36);
    assert.throws(() => validatePeX64(invalidAlignment, language), /invalid PE file alignment/u);

    const invalidSectionName = fakePeX64();
    invalidSectionName.writeUInt8(0x1f, 0x188);
    assert.throws(() => validatePeX64(invalidSectionName, language), /invalid PE section 0 name/u);

    const oversizedRaw = fakePeX64();
    oversizedRaw.writeUInt32LE(0x1000, 0x188 + 16);
    assert.throws(() => validatePeX64(oversizedRaw, language), /truncated or invalid PE section 0 raw data/u);

    const overlappingRaw = fakePeX64({ sectionCount: 2 });
    const secondSection = 0x188 + 40;
    Buffer.from(".rdata\0", "ascii").copy(overlappingRaw, secondSection, 0, 8);
    overlappingRaw.writeUInt32LE(0x200, secondSection + 8);
    overlappingRaw.writeUInt32LE(0x2000, secondSection + 12);
    overlappingRaw.writeUInt32LE(0x200, secondSection + 16);
    overlappingRaw.writeUInt32LE(0x200, secondSection + 20);
    assert.throws(() => validatePeX64(overlappingRaw, language), /overlapping PE section raw ranges/u);

    const overlappingVirtual = Buffer.concat([fakePeX64({ sectionCount: 2 }), Buffer.alloc(0x200)]);
    const secondVirtualSection = 0x188 + 40;
    Buffer.from(".rdata\0", "ascii").copy(overlappingVirtual, secondVirtualSection, 0, 8);
    overlappingVirtual.writeUInt32LE(0x200, secondVirtualSection + 8);
    overlappingVirtual.writeUInt32LE(0x1000, secondVirtualSection + 12);
    overlappingVirtual.writeUInt32LE(0x200, secondVirtualSection + 16);
    overlappingVirtual.writeUInt32LE(0x400, secondVirtualSection + 20);
    assert.throws(() => validatePeX64(overlappingVirtual, language), /overlapping PE section virtual ranges/u);
  }
});

test("assertOracle rejects altered exit, stdout and stderr", () => {
  const oracle = { exitCode: 0, stdout: "ok\n", stderr: "" };
  for (const execution of [
    { exitCode: 1, stdout: Buffer.from("ok\n"), stderr: Buffer.alloc(0) },
    { exitCode: 0, stdout: Buffer.from("not ok\n"), stderr: Buffer.alloc(0) },
    { exitCode: 0, stdout: Buffer.from("ok\n"), stderr: Buffer.from("diagnostic\n") },
  ]) {
    assert.throws(
      () => assertOracle(execution, oracle, "fixture", "fixture correctness"),
      /fixture correctness output does not match the fixture exact-output oracle/u,
    );
  }
});

async function ownedRunDirectories() {
  return new Set((await readdir(os.tmpdir())).filter((name) => name.startsWith("w-executable-run-")));
}

test("C and Rust dispatch compile directly with declared targets and skip W toolchains", async () => {
  for (const language of ["c", "rust"]) {
    const fake = fakeRunnerExecutor({ language });
    const { record } = await runBenchmark({ language, warmup: 1, samples: 9, publish: false }, fakeRunnerDependencies(language, fake));
    const compileCalls = fake.calls.filter((call) => call.args.includes("-o"));
    assert.equal(compileCalls.length, 11, `${language} must compile once for correctness, once for warmup and nine raw samples`);
    assert.equal(record.language, language);
    assert.equal(record.compile.warmup.length, 1);
    assert.equal(record.compile.raw.length, 9);
    assert.equal(record.run.warmup.length, 1);
    assert.equal(record.run.raw.length, 9);
    assert.equal(record.correctness.oracleId, "hello:exact-output");
    assert.deepEqual(record.artifact.cleanliness, EXPECTED_PE_ARTIFACT_CLEANLINESS);
    assert.deepEqual(record.artifact.peLayout, EXPECTED_PE_LAYOUT);
    assert.equal(record.artifactTarget, "x86_64-pc-windows-msvc");
    assert.match(record.protocol.resourceScope, /direct compiler process only/u);
    assert.match(record.protocol.directProcessDisclosure, /per-direct-child timeout.*SIGKILL.*descendant termination.*Job Object/iu);
    assert.equal(fake.calls.some((call) => /w_seed|mlir|cmake|ninja/iu.test([call.command, ...call.args].join(" "))), false);
    if (language === "c") {
      assert.equal(fake.calls.filter((call) => call.args.includes("-dumpmachine")).length, 1);
      assert.equal(fake.calls.filter((call) => call.args.includes("-fsyntax-only")).length, 1, "public C final-dialect probe must use the injected executor");
      assert.ok(fake.calls.filter((call) => call.args.includes("-fsyntax-only")).every((call) => typeof call.stdin === "string" && call.stdin.includes("int main")));
      assert.equal(fake.calls.some((call) => call.args.includes(C_WHOLE_PROGRAM_FLAG)), false);
      assert.equal(fake.calls.filter((call) => call.args.length === 1 && call.args[0] === "--version").length, 1);
      assert.ok(compileCalls.every((call) => call.args.includes("-std=c23")));
      assert.ok(compileCalls.every((call) => CLANG_RELEASE_FLAGS.every((flag) => call.args.includes(flag))));
      assert.ok(compileCalls.every((call) => call.env?.PATH === "C:\\fake"));
      assert.match(record.identity.toolchain, /clang-22\.1\.8-c23-portable/u);
      assert.match(record.identity.toolchain, /x86_64-pc-windows-msvc/u);
      assert.equal(record.provenance.toolchainDigest.length, 71);
    } else {
      assert.equal(fake.calls.filter((call) => call.args.includes("target-libdir")).length, 1);
      assert.equal(fake.calls.filter((call) => call.args.includes("--verbose")).length, 1);
      assert.ok(compileCalls.every((call) => call.args.includes("--edition=2024")));
      assert.ok(compileCalls.every((call) => RUST_RELEASE_FLAGS.every((flag) => call.args.includes(flag))));
      assert.ok(compileCalls.every((call) => call.args.includes("--target=x86_64-pc-windows-msvc")));
      assert.match(record.identity.toolchain, /edition-2024/u);
      assert.match(record.identity.toolchain, /x86_64-pc-windows-msvc/u);
    }
    assertNoFakeSampleDirectories(fake);
  }
});

test("process-entry checks every argument case before timing and pins the payload run", async () => {
  for (const language of ["c", "rust"]) {
    const fake = fakeRunnerExecutor({ language, target: "process-entry" });
    const { record } = await runBenchmark({ target: "process-entry", language, warmup: 1, samples: 9, publish: false }, fakeRunnerDependencies(language, fake));
    const runtimeCalls = fake.calls.filter((call) => path.extname(call.command).toLowerCase() === ".exe" && !call.args.includes("-o") && call.command !== fake.compiler);
    assert.deepEqual(runtimeCalls.slice(0, 3).map((call) => call.args), [[], [""], ["payload"]]);
    assert.equal(runtimeCalls.length, 13, `${language} must run three correctness cases and ten timed payload cases`);
    assert.ok(runtimeCalls.slice(3).every((call) => call.args.length === 1 && call.args[0] === "payload"));
    assert.equal(record.correctness.oracleId, "process-entry:argument-dependent-output");
    assert.deepEqual(record.correctness.cases.map((testCase) => testCase.arguments), [[], [""], ["payload"]]);
    assert.match(record.protocol.resourceScope, /no-argument, empty-argument and payload cases before timing/u);
    assertNoFakeSampleDirectories(fake);
  }

  const fake = fakeWRunnerExecutor("process-entry");
  const { record } = await runBenchmark({ target: "process-entry", language: "w", warmup: 1, samples: 9, publish: false }, {
    executor: fake.executor,
    commit: TEST_COMMIT,
    environment: TEST_ENVIRONMENT,
    runnerDigest: TEST_DIGEST,
    catalogDigest: TEST_DIGEST,
    testOnly: true,
    testOnlyPlatform: { platform: "win32", arch: "x64" },
    windowsToolchain: fake.windowsToolchain,
    buildPublicW: async () => fake.publicW,
  });
  const runtimeCalls = fake.calls.filter((call) => call.command !== fake.publicW.executable && path.extname(call.command).toLowerCase() === ".exe");
  assert.deepEqual(runtimeCalls.slice(0, 3).map((call) => call.args), [[], [""], ["payload"]]);
  assert.equal(runtimeCalls.length, 13, "W must run three correctness cases and ten timed payload cases");
  assert.ok(runtimeCalls.slice(3).every((call) => call.args.length === 1 && call.args[0] === "payload"));
  assert.equal(record.correctness.oracleId, "process-entry:argument-dependent-output");
  assertNoFakeSampleDirectories(fake);
});

test("process-handler-lifecycle keeps the pinned runtime vector and isolates fault trials", async () => {
  const fake = fakeProcessRunnerExecutor();
  const previousFault = process.env.W_SEED_PROCESS_ENTRY0_FAULT;
  process.env.W_SEED_PROCESS_ENTRY0_FAULT = "wrong-context";
  let record;
  try {
    ({ record } = await runBenchmark({ target: "process-handler-lifecycle", language: "c", warmup: 1, samples: 9, publish: false }, {
      ...fakeRunnerDependencies("c", fake),
    }));
  } finally {
    if (previousFault === undefined) delete process.env.W_SEED_PROCESS_ENTRY0_FAULT;
    else process.env.W_SEED_PROCESS_ENTRY0_FAULT = previousFault;
  }
  assert.equal(record.workloadId, "process-handler-lifecycle");
  assert.equal(record.artifactTarget, "x86_64-w64-mingw32");
  assert.equal(record.identity.recipeClass, "process-entry0-private-handler");
  assert.match(record.protocol.resourceScope, /pinned \[alpha, payload\].*Fault witnesses.*not timed/u);
  const runtimeCalls = fake.calls.filter((call) => path.extname(call.command).toLowerCase() === ".exe" && !call.args.includes("-o") && call.command !== fake.compiler);
  assert.equal(runtimeCalls.length, 18, "two correctness vectors, six faults and ten timed runs are required");
  const normalCalls = runtimeCalls.filter((call) => call.env?.W_SEED_PROCESS_ENTRY0_FAULT === undefined);
  assert.equal(normalCalls.length, 12, "normal correctness and timed runs must clear the fault selector");
  assert.ok(normalCalls.every((call) => call.env !== undefined), "normal runs must receive an explicit sanitized environment");
  assert.ok(normalCalls.some((call) => call.args.length === 2 && call.args[0] === "alpha" && call.args[1] === "payload"));
  assert.ok(runtimeCalls.filter((call) => call.env?.W_SEED_PROCESS_ENTRY0_FAULT !== undefined).every((call) =>
    ["missing", "noop-success", "stale-generation", "reversed-arguments", "wrong-context", "wrong-arguments"].includes(call.env.W_SEED_PROCESS_ENTRY0_FAULT)));
  assertNoFakeSampleDirectories(fake);
});

test("restaurant-branch C and Rust records use target-specific source, oracle and identity metadata", async () => {
  for (const language of ["c", "rust"]) {
    const fake = fakeRunnerExecutor({ language, target: "restaurant-branch" });
    const { record } = await runBenchmark({ target: "restaurant-branch", language, warmup: 1, samples: 9, publish: false }, fakeRunnerDependencies(language, fake));
    const compileCalls = fake.calls.filter((call) => call.args.includes("-o"));
    assert.equal(record.workloadId, "restaurant-branch");
    assert.equal(record.id, `restaurant-branch-${language}-${TEST_COMMIT.slice(0, 12)}`);
    assert.equal(record.correctness.oracleId, "restaurant-branch:exact-output");
    assert.ok(compileCalls.every((call) => call.args.some((argument) => argument.endsWith(`restaurant-branch-${language}.exe`))));
    assert.equal(record.identity.recipeClass, "restaurant-release");
    assert.match(record.identity.sourceDigest, /^sha256:[0-9a-f]{64}$/u);
    assert.equal(record.compile.raw.length, 9);
  }
});

test("Rust release measurement rejects an unexpected PDB sidecar", async () => {
  const fake = fakeRunnerExecutor({ language: "rust", symbolSidecar: true });
  await assert.rejects(
    () => runBenchmark({ language: "rust", warmup: 1, samples: 9, publish: false }, fakeRunnerDependencies("rust", fake)),
    /Rust compiler produced unexpected release sidecars: .*\.exe, .*\.pdb/iu,
  );
  assertNoFakeSampleDirectories(fake);
});

test("C release measurement rejects an unexpected release sidecar", async () => {
  const fake = fakeRunnerExecutor({ language: "c", symbolSidecar: true });
  await assert.rejects(
    () => runBenchmark({ language: "c", warmup: 1, samples: 9, publish: false }, fakeRunnerDependencies("c", fake)),
    /C compiler produced unexpected release sidecars: .*\.exe, .*\.pdb/iu,
  );
  assertNoFakeSampleDirectories(fake);
});

test("PE cleanliness is checked on the retained correctness artifact before execution and sizing", async () => {
  for (const language of ["c", "rust"]) {
    const fake = fakeRunnerExecutor({ language, peOptions: { debugRva: 0x200, debugSize: 28 } });
    await assert.rejects(
      () => runBenchmark({ language, warmup: 1, samples: 9, publish: false }, fakeRunnerDependencies(language, fake)),
      /invalid PE debug directory RVA range/u,
    );
    assert.equal(fake.calls.filter((call) => call.args.length === 0).length, 0, `${language} must reject before execution`);
    assert.equal(fake.calls.filter((call) => call.args.includes("-o")).length, 1, `${language} must reject before measured samples`);
    assertNoFakeSampleDirectories(fake);
  }

  const fake = fakeWRunnerExecutor("hello", { peOptions: { overlay: Buffer.from("RSDS synthetic CodeView overlay", "ascii") } });
  await assert.rejects(
    () => runBenchmark({ language: "w", warmup: 1, samples: 9, publish: false }, {
      executor: fake.executor,
      commit: TEST_COMMIT,
      environment: TEST_ENVIRONMENT,
      runnerDigest: TEST_DIGEST,
      catalogDigest: TEST_DIGEST,
      testOnly: true,
      testOnlyPlatform: { platform: "win32", arch: "x64" },
      windowsToolchain: fake.windowsToolchain,
      publicW: fake.publicW,
    }),
    /contains overlay bytes/u,
  );
  assert.equal(fake.calls.filter((call) => call.args.length === 0).length, 0, "W must reject before execution");
  assert.equal(fake.calls.filter((call) => call.args[0] === "build").length, 1, "W must reject before measured samples");
  assertNoFakeSampleDirectories(fake);
});

test("restaurant-branch W uses public w build with a retained correctness artifact and separate run series", async () => {
  const fake = fakeWRunnerExecutor("restaurant-branch");
  let publicBuilds = 0;
  const { record } = await runBenchmark({ target: "restaurant-branch", language: "w", warmup: 1, samples: 9, publish: false }, {
    executor: fake.executor,
    commit: TEST_COMMIT,
    environment: TEST_ENVIRONMENT,
    runnerDigest: TEST_DIGEST,
    catalogDigest: TEST_DIGEST,
    testOnly: true,
    testOnlyPlatform: { platform: "win32", arch: "x64" },
    windowsToolchain: fake.windowsToolchain,
    buildPublicW: async () => {
      publicBuilds += 1;
      return fake.publicW;
    },
  });
  assert.equal(record.workloadId, "restaurant-branch");
  assert.equal(record.language, "w");
  assert.equal(record.identity.recipe, "public-w-build-release");
  assert.equal(record.correctness.oracleId, "restaurant-branch:exact-output");
  assert.deepEqual(record.artifact.cleanliness, EXPECTED_PE_ARTIFACT_CLEANLINESS);
  assert.deepEqual(record.artifact.peLayout, EXPECTED_PE_LAYOUT);
  assert.equal(record.compile.raw.length, 9);
  assert.equal(record.run.raw.length, 9);
  assert.deepEqual(record.artifact.cleanliness, EXPECTED_PE_ARTIFACT_CLEANLINESS);
  assert.equal(publicBuilds, 1, "public w.exe must be built once outside the compile samples");
  const compileCalls = fake.calls.filter((call) => call.args[0] === "build");
  assert.equal(compileCalls.length, 11);
  assert.ok(compileCalls.every((call) => call.command === fake.publicW.executable && call.args[2] === "--target" && call.args[3] === "x86_64-pc-windows-msvc" && call.args[4] === "--output"));
  assert.equal(fake.calls.some((call) => /w_seed|mlir|cmake|ninja/iu.test([call.command, ...call.args].join(" "))), false);
  assert.equal(fake.calls.filter((call) => call.args.length === 0).length, 11);
  assert.match(record.protocol.resourceScope, /complete direct w\.exe build interval.*direct-process CPU\/RSS.*non-comparable to C\/Rust.*child process-tree/u);
  assertNoFakeSampleDirectories(fake);
});

test("W toolchain identity binds the complete public CLI digest", async () => {
  const first = fakeWRunnerExecutor("hello");
  const firstRun = await runBenchmark({ language: "w", warmup: 1, samples: 9, publish: false }, {
    executor: first.executor,
    commit: TEST_COMMIT,
    environment: TEST_ENVIRONMENT,
    runnerDigest: TEST_DIGEST,
    catalogDigest: TEST_DIGEST,
    testOnly: true,
    testOnlyPlatform: { platform: "win32", arch: "x64" },
    windowsToolchain: first.windowsToolchain,
    publicW: first.publicW,
  });
  const second = fakeWRunnerExecutor("hello");
  const changedDigest = "sha256:" + "2".repeat(64);
  const secondRun = await runBenchmark({ language: "w", warmup: 1, samples: 9, publish: false }, {
    executor: second.executor,
    commit: TEST_COMMIT,
    environment: TEST_ENVIRONMENT,
    runnerDigest: TEST_DIGEST,
    catalogDigest: TEST_DIGEST,
    testOnly: true,
    testOnlyPlatform: { platform: "win32", arch: "x64" },
    windowsToolchain: second.windowsToolchain,
    publicW: { ...second.publicW, digest: changedDigest },
  });
  assert.match(firstRun.record.identity.toolchain, new RegExp(TEST_DIGEST.slice("sha256:".length), "u"));
  assert.match(secondRun.record.identity.toolchain, new RegExp(changedDigest.slice("sha256:".length), "u"));
  assert.notEqual(firstRun.record.identity.toolchain, secondRun.record.identity.toolchain);
  assertNoFakeSampleDirectories(first);
  assertNoFakeSampleDirectories(second);
});

test("W public build rejects an unexpected release sidecar", async () => {
  const fake = fakeWRunnerExecutor("hello", { symbolSidecar: true });
  await assert.rejects(
    () => runBenchmark({ language: "w", warmup: 1, samples: 9, publish: false }, {
      executor: fake.executor,
      commit: TEST_COMMIT,
      environment: TEST_ENVIRONMENT,
      runnerDigest: TEST_DIGEST,
      catalogDigest: TEST_DIGEST,
      testOnly: true,
      testOnlyPlatform: { platform: "win32", arch: "x64" },
      windowsToolchain: fake.windowsToolchain,
      publicW: fake.publicW,
    }),
    /W public build produced unexpected release sidecars: .*\.exe, .*\.pdb/iu,
  );
  assertNoFakeSampleDirectories(fake);
});

test("W rejects a private gate fallback dependency", async () => {
  const fake = fakeWRunnerExecutor("hello");
  await assert.rejects(
    () => runBenchmark({ language: "w", warmup: 1, samples: 9, publish: false }, {
      executor: fake.executor,
      commit: TEST_COMMIT,
      environment: TEST_ENVIRONMENT,
      runnerDigest: TEST_DIGEST,
      catalogDigest: TEST_DIGEST,
      testOnly: true,
      testOnlyPlatform: { platform: "win32", arch: "x64" },
      windowsToolchain: fake.windowsToolchain,
      publicW: fake.publicW,
      gate: { executable: path.resolve("fake-w-seed-mlir0-gate.exe") },
    }),
    /private gate fallback dependencies are unsupported/u,
  );
  assert.equal(fake.calls.length, 0);
});

test("timed-out compiler probes abort before a measurement directory or result exists", async () => {
  const fake = fakeRunnerExecutor({ language: "c", timeoutMode: "probe" });
  await assert.rejects(
    () => runBenchmark({ target: "hello", language: "c", warmup: 1, samples: 9, publish: false }, fakeRunnerDependencies("c", fake)),
    /hello C target probe.*child timeout.*SIGKILL/iu,
  );
  assert.equal(fake.calls.length, 1);
  assert.equal(fake.calls[0].timeout, EXECUTABLE_CHILD_TIMEOUT_MS);
  assert.equal(fake.calls[0].killSignal, EXECUTABLE_CHILD_KILL_SIGNAL);
});

test("timed-out executable runs abort without publishing a partial result or leaking temporary files", async () => {
  const fake = fakeRunnerExecutor({ language: "c", timeoutMode: "run" });
  await mkdir(RESULTS_DIRECTORY, { recursive: true });
  const directory = await mkdtemp(path.join(RESULTS_DIRECTORY, "w-executable-timeout-test-"));
  const output = path.join(directory, "timeout.json");
  const before = await ownedRunDirectories();
  try {
    await assert.rejects(
      () => runBenchmark({ target: "hello", language: "c", warmup: 1, samples: 9, output, publish: false }, fakeRunnerDependencies("c", fake)),
      /c hello run.*child timeout.*SIGKILL/iu,
    );
    assert.equal(existsSync(output), false);
    assertNoFakeSampleDirectories(fake);
    assert.ok(fake.calls.some((call) => call.args.length === 0 && call.timeout === EXECUTABLE_CHILD_TIMEOUT_MS));
  } finally {
    const after = await ownedRunDirectories();
    await rm(directory, { recursive: true, force: true });
    await rmdir(RESULTS_DIRECTORY).catch((error) => {
      if (error?.code !== "ENOENT" && error?.code !== "ENOTEMPTY" && error?.code !== "EEXIST") throw error;
    });
    assert.deepEqual(after, before, "owned benchmark run directory must be removed");
  }
});

test("production platform identity cannot be replaced by an unscoped test fixture", async () => {
  const fake = fakeRunnerExecutor({ language: "c" });
  await assert.rejects(
    () => runBenchmark({ language: "c", warmup: 1, samples: 9, publish: false }, {
      ...fakeRunnerDependencies("c", fake),
      testOnlyPlatform: { platform: "linux", arch: "x64" },
    }),
    /Windows x86_64/u,
  );
  await assert.rejects(
    () => runBenchmark({ language: "c", warmup: 1, samples: 9, publish: false }, {
      ...fakeRunnerDependencies("c", fake),
      testOnly: false,
    }),
    /test-only platform and toolchain fixtures/u,
  );
});

test("compiler metadata cannot be injected in place of executor probes", async () => {
  const fake = fakeRunnerExecutor({ language: "c" });
  await assert.rejects(
    () => runBenchmark({ language: "c", warmup: 1, samples: 9, publish: false }, {
      ...fakeRunnerDependencies("c", fake),
      testOnlyToolchains: { c: { command: fake.compiler, target: "spoofed-target" } },
    }),
    /metadata must come from compiler probes/u,
  );
  await assert.rejects(
    () => runBenchmark({ language: "c", warmup: 1, samples: 9, publish: false }, {
      ...fakeRunnerDependencies("c", fake),
      languageToolchains: { c: { command: fake.compiler, dialect: { flag: "-std=c23" }, target: "spoofed-target" } },
    }),
    /metadata overrides are unsupported/u,
  );
});

test("oracle mismatch stops before samples and leaves no result or owned temporary", async () => {
  const fake = fakeRunnerExecutor({ language: "c", mismatch: true });
  await mkdir(RESULTS_DIRECTORY, { recursive: true });
  const directory = await mkdtemp(path.join(RESULTS_DIRECTORY, "w-executable-oracle-test-"));
  const output = path.join(directory, "mismatch.json");
  const before = await ownedRunDirectories();
  try {
    await assert.rejects(
      () => runBenchmark({ language: "c", warmup: 1, samples: 9, output, publish: false }, fakeRunnerDependencies("c", fake)),
      /does not match the hello exact-output oracle/u,
    );
    assert.equal(existsSync(output), false);
    assert.equal(fake.calls.filter((call) => call.args.includes("-o")).length, 1, "oracle must run before measured compile samples");
    assertNoFakeSampleDirectories(fake);
  } finally {
    const after = await ownedRunDirectories();
    await rm(directory, { recursive: true, force: true });
    await rmdir(RESULTS_DIRECTORY).catch((error) => {
      if (error?.code !== "ENOENT" && error?.code !== "ENOTEMPTY" && error?.code !== "EEXIST") throw error;
    });
    assert.deepEqual(after, before, "owned benchmark run directory must be removed");
  }
});

test("publication is contained, atomic and refuses overwrite", async () => {
  await mkdir(RESULTS_DIRECTORY, { recursive: true });
  const directory = await mkdtemp(path.join(RESULTS_DIRECTORY, "w-executable-result-test-"));
  const output = path.join(directory, "record.json");
  try {
    const resolved = await resolveResultPath(output);
    const record = { kind: "executable-result", status: "recorded" };
    await publishRecord(resolved, record);
    assert.deepEqual(JSON.parse(await readFile(resolved, "utf8")), record);
    await assert.rejects(() => publishRecord(resolved, record));
    await assert.rejects(() => resolveResultPath(path.join(RESULTS_DIRECTORY, "..", "outside.json")), /contained/);
    await assert.rejects(() => publishRecord(path.join(RESULTS_DIRECTORY, "..", "outside.json"), record), /contained/);
  } finally {
    await rm(directory, { recursive: true, force: true });
    await rmdir(RESULTS_DIRECTORY).catch((error) => {
      if (error?.code !== "ENOENT" && error?.code !== "ENOTEMPTY" && error?.code !== "EEXIST") throw error;
    });
  }
});
