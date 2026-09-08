import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import test from "node:test";
import { mkdir, mkdtemp, readFile, readdir, rm, rmdir, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import {
  RESULTS_DIRECTORY,
  deriveSummary,
  parseBenchmarkArguments,
  publishRecord,
  resolveResultPath,
  runBenchmark,
} from "./executable-benchmark-runner.mjs";

test("benchmark arguments select a language and keep the fixed raw count", () => {
  assert.deepEqual(parseBenchmarkArguments([]), {
    target: "hello", language: "w", output: undefined, warmup: 1, samples: 9, help: false,
  });
  assert.deepEqual(parseBenchmarkArguments(["--target", "hello", "--language", "c", "--output", "benchmarks/results/hello-c.local.json", "--warmup", "2", "--samples", "11"]), {
    target: "hello", language: "c", output: "benchmarks/results/hello-c.local.json", warmup: 2, samples: 11, help: false,
  });
  assert.deepEqual(parseBenchmarkArguments(["--language=rust"]), {
    target: "hello", language: "rust", output: undefined, warmup: 1, samples: 9, help: false,
  });
  assert.throws(() => parseBenchmarkArguments(["--language", "swift"]), /unsupported/);
  assert.throws(() => parseBenchmarkArguments(["--samples", "10"]), /odd/);
  assert.throws(() => parseBenchmarkArguments(["--warmup", "0"]), /between 1/);
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

function fakePeX64() {
  const bytes = Buffer.alloc(0x200);
  bytes[0] = 0x4d;
  bytes[1] = 0x5a;
  bytes.writeUInt32LE(0x80, 0x3c);
  bytes.writeUInt32LE(0x00004550, 0x80);
  bytes.writeUInt16LE(0x8664, 0x84);
  bytes.writeUInt16LE(0x20b, 0x98);
  return bytes;
}

function fakeResourceUsage() {
  return { cpuTime: { user: 1, system: 1 }, maxRSS: 4096 };
}

function fakeRunnerExecutor({ language, mismatch = false }) {
  const compiler = path.resolve(`fake-${language === "c" ? "gcc" : "rustc"}.exe`);
  const calls = [];
  const sampleDirectories = new Set();
  const executor = async (command, args, options = {}) => {
    calls.push({ command, args: [...args], cwd: options.cwd, stdin: options.stdin });
    if (args.length === 1 && args[0] === "-dumpmachine") {
      return { exitCode: 0, stdout: "x86_64-w64-mingw32", stderr: Buffer.alloc(0) };
    }
    if (language === "c" && args.includes("-fsyntax-only")) {
      return {
        exitCode: args.includes("-std=c23") ? 1 : 0,
        stdout: Buffer.alloc(0),
        stderr: Buffer.from(args.includes("-std=c23") ? "C23 preview unavailable\n" : "", "utf8"),
      };
    }
    if (language === "c" && args.length === 1 && args[0] === "--version") {
      return { exitCode: 0, stdout: "gcc (GCC) 13.2.0\n", stderr: Buffer.alloc(0) };
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
      await writeFile(artifact, fakePeX64());
      return { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0), resourceUsage: fakeResourceUsage() };
    }
    if (args.length === 0) {
      return {
        exitCode: 0,
        stdout: mismatch ? "Not the oracle\n" : "Hello, world!\n",
        stderr: Buffer.alloc(0),
        resourceUsage: fakeResourceUsage(),
      };
    }
    throw new Error(`unexpected fake executor invocation: ${command} ${args.join(" ")}`);
  };
  return { compiler, calls, sampleDirectories, executor };
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
  };
}

function assertNoFakeSampleDirectories(fake) {
  for (const directory of fake.sampleDirectories) assert.equal(existsSync(directory), false, `temporary sample remains: ${directory}`);
}

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
    assert.equal(record.artifactTarget, language === "c" ? "x86_64-w64-mingw32" : "x86_64-pc-windows-msvc");
    assert.match(record.protocol.resourceScope, /direct compiler process only/u);
    assert.match(record.protocol.directProcessDisclosure, /deferred:.*timeout option/iu);
    assert.equal(fake.calls.some((call) => /w_seed|mlir|cmake|ninja/iu.test([call.command, ...call.args].join(" "))), false);
    if (language === "c") {
      assert.equal(fake.calls.filter((call) => call.args.includes("-dumpmachine")).length, 1);
      assert.equal(fake.calls.filter((call) => call.args.includes("-fsyntax-only")).length, 2, "C dialect probe must use the injected executor for both candidates");
      assert.ok(fake.calls.filter((call) => call.args.includes("-fsyntax-only")).every((call) => typeof call.stdin === "string" && call.stdin.includes("int main")));
      assert.equal(fake.calls.filter((call) => call.args.length === 1 && call.args[0] === "--version").length, 1);
      assert.ok(compileCalls.every((call) => call.args.includes("-std=c2x")));
      assert.ok(compileCalls.every((call) => call.args.includes("-O2")));
      assert.match(record.identity.toolchain, /c2x-preview/u);
      assert.match(record.identity.toolchain, /x86_64-w64-mingw32/u);
      assert.equal(record.provenance.toolchainDigest.length, 71);
    } else {
      assert.equal(fake.calls.filter((call) => call.args.includes("target-libdir")).length, 1);
      assert.equal(fake.calls.filter((call) => call.args.includes("--verbose")).length, 1);
      assert.ok(compileCalls.every((call) => call.args.includes("--edition=2024")));
      assert.ok(compileCalls.every((call) => call.args.includes("-C") && call.args.includes("opt-level=2") && call.args.includes("debuginfo=0") && call.args.includes("incremental=off")));
      assert.ok(compileCalls.every((call) => call.args.includes("--target=x86_64-pc-windows-msvc")));
      assert.match(record.identity.toolchain, /edition-2024/u);
      assert.match(record.identity.toolchain, /x86_64-pc-windows-msvc/u);
    }
    assertNoFakeSampleDirectories(fake);
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
      /does not match the Hello oracle/u,
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
