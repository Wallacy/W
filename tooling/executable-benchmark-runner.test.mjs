import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import test from "node:test";
import { mkdir, mkdtemp, readFile, readdir, rm, rmdir, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import {
  RESULTS_DIRECTORY,
  defaultExecutor,
  deriveSummary,
  EXECUTABLE_CHILD_KILL_SIGNAL,
  EXECUTABLE_CHILD_TIMEOUT_MS,
  parseBenchmarkArguments,
  publishRecord,
  resolveResultPath,
  runBenchmark,
} from "./executable-benchmark-runner.mjs";
import {
  C_RELEASE_FLAGS,
  RUST_RELEASE_FLAGS,
  W_LLC_FLAGS,
  W_LLD_LINK_FLAGS,
  W_MLIR_OPT_FLAGS,
} from "./executable-release-recipes.mjs";

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
  assert.deepEqual(parseBenchmarkArguments(["--target", "restaurant-branch", "--language", "rust"]), {
    target: "restaurant-branch", language: "rust", output: undefined, warmup: 1, samples: 9, help: false,
  });
  assert.throws(() => parseBenchmarkArguments(["--target", "restaurant-composition"]), /unsupported/);
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
  assert.deepEqual(RUST_RELEASE_FLAGS, ["-C", "opt-level=3", "-C", "lto=fat", "-C", "codegen-units=1", "-C", "panic=abort", "-C", "debuginfo=0", "-C", "strip=symbols", "-C", "link-arg=/DEBUG:NONE"]);
  assert.equal(RUST_RELEASE_FLAGS.includes("incremental=off"), false, "rustc treats this as an output directory rather than disabling incremental compilation");
  assert.deepEqual(W_MLIR_OPT_FLAGS, ["--verify-each", "--canonicalize", "--cse"]);
  assert.ok(W_LLC_FLAGS.includes("-O3"));
  assert.ok(W_LLD_LINK_FLAGS.includes("/opt:ref"));
  assert.ok(W_LLD_LINK_FLAGS.includes("/opt:icf"));
  const all = [...C_RELEASE_FLAGS, ...RUST_RELEASE_FLAGS, ...W_LLC_FLAGS, ...W_LLD_LINK_FLAGS];
  assert.equal(all.some((flag) => /(?:^|=)(?:s|z)$|native/iu.test(flag)), false);
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

function fakeRunnerExecutor({ language, mismatch = false, target = "hello", timeoutMode = undefined, symbolSidecar = false }) {
  const compiler = path.resolve(`fake-${language === "c" ? "gcc" : "rustc"}.exe`);
  const calls = [];
  const sampleDirectories = new Set();
  const executor = async (command, args, options = {}) => {
    calls.push({ command, args: [...args], cwd: options.cwd, stdin: options.stdin, timeout: options.timeout, killSignal: options.killSignal });
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
      if (language === "rust" && symbolSidecar) {
        await writeFile(artifact.replace(/\.exe$/iu, ".pdb"), Buffer.from("debug symbols", "utf8"));
      }
      return { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0), resourceUsage: fakeResourceUsage() };
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

function fakeWRunnerExecutor(target) {
  const calls = [];
  const sampleDirectories = new Set();
  const gate = { executable: path.resolve("fake-w-seed-mlir0-gate.exe"), compilerVersion: "19.51.36256.0" };
  const toolNames = ["mlir-opt.exe", "mlir-translate.exe", "llc.exe", "lld-link.exe"];
  const windowsToolchain = {
    manifestDigest: TEST_DIGEST,
    materialized: {
      tools: Object.fromEntries(toolNames.map((name) => [name, {
        relativePath: `fake/${name}`,
        sizeBytes: "1",
        sha256: TEST_DIGEST,
        version: "23.1.0",
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
      return result(0, Buffer.from(target === "restaurant-branch" ? "Kitchen open\nAfter service\nKitchen closed\nAfter service\n" : "Hello, world!\n", "utf8"));
    }
    if (command === gate.executable) {
      if (args.includes("--target=unsupported")) return result(2);
      return result(0, Buffer.from("x86_64-pc-windows-msvc\n", "utf8"));
    }
    const outputIndex = args.indexOf("-o");
    if (outputIndex >= 0) {
      const output = args[outputIndex + 1];
      sampleDirectories.add(options.cwd);
      await writeFile(output, Buffer.from("generated\n", "utf8"));
      return result(0);
    }
    const linkOutput = args.find((argument) => argument.startsWith("/out:"));
    if (linkOutput !== undefined) {
      const output = linkOutput.slice("/out:".length);
      sampleDirectories.add(options.cwd);
      await writeFile(output, fakePeX64());
      return result(0);
    }
    throw new Error(`unexpected fake W executor invocation: ${command} ${args.join(" ")}`);
  };
  return { calls, sampleDirectories, executor, gate, windowsToolchain };
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
    assert.match(record.protocol.directProcessDisclosure, /per-direct-child timeout.*SIGKILL.*descendant termination.*Job Object/iu);
    assert.equal(fake.calls.some((call) => /w_seed|mlir|cmake|ninja/iu.test([call.command, ...call.args].join(" "))), false);
    if (language === "c") {
      assert.equal(fake.calls.filter((call) => call.args.includes("-dumpmachine")).length, 1);
      assert.equal(fake.calls.filter((call) => call.args.includes("-fsyntax-only")).length, 2, "C dialect probe must use the injected executor for both candidates");
      assert.ok(fake.calls.filter((call) => call.args.includes("-fsyntax-only")).every((call) => typeof call.stdin === "string" && call.stdin.includes("int main")));
      assert.equal(fake.calls.filter((call) => call.args.length === 1 && call.args[0] === "--version").length, 1);
      assert.ok(compileCalls.every((call) => call.args.includes("-std=c2x")));
      assert.ok(compileCalls.every((call) => C_RELEASE_FLAGS.every((flag) => call.args.includes(flag))));
      assert.match(record.identity.toolchain, /c2x-preview/u);
      assert.match(record.identity.toolchain, /x86_64-w64-mingw32/u);
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

test("restaurant-branch W uses the private route with a retained correctness artifact and separate run series", async () => {
  const fake = fakeWRunnerExecutor("restaurant-branch");
  const { record } = await runBenchmark({ target: "restaurant-branch", language: "w", warmup: 1, samples: 9, publish: false }, {
    executor: fake.executor,
    commit: TEST_COMMIT,
    environment: TEST_ENVIRONMENT,
    runnerDigest: TEST_DIGEST,
    catalogDigest: TEST_DIGEST,
    testOnly: true,
    testOnlyPlatform: { platform: "win32", arch: "x64" },
    windowsToolchain: fake.windowsToolchain,
    gate: fake.gate,
  });
  assert.equal(record.workloadId, "restaurant-branch");
  assert.equal(record.language, "w");
  assert.equal(record.identity.recipe, "private-native0-mlir0-source-to-pe-candidate");
  assert.equal(record.correctness.oracleId, "restaurant-branch:exact-output");
  assert.equal(record.compile.raw.length, 9);
  assert.equal(record.run.raw.length, 9);
  assert.equal(fake.calls.filter((call) => call.args.includes("--target=x86_64-pc-windows-msvc")).length, 12);
  assert.equal(fake.calls.filter((call) => call.args.length === 0).length, 11);
  assertNoFakeSampleDirectories(fake);
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
