import assert from "node:assert/strict";
import crypto from "node:crypto";
import { mkdtemp, mkdir, readdir, readFile, rm, unlink, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import test from "node:test";
import {
  DEFAULT_SAMPLE_COUNT,
  DEFAULT_WARMUP_COUNT,
  COMPARISON_DEFAULT_PAIR_COUNT,
  ROOT,
  RUNNER_EVIDENCE_PATHS,
  TARGET_NAME,
  RunnerError,
  atomicPublishJson,
  buildSeed,
  calculateLatency,
  runComparisonBenchmark,
  validateComparisonOptions,
  validateCommitSha,
  cleanupDirectoryBestEffort,
  parseRunnerArgs,
  runnerEvidence,
  runBenchmark,
} from "./benchmark-driven-development-runner.mjs";
import { loadBmdDocuments, pairedScheduleForSeed, validateResult } from "./benchmark-driven-development-machine.mjs";

const digest = "sha256:" + "1".repeat(64);
const fakeToolchain = () => ({
  identity: "evidence-cmake-compiler;bun=" + process.versions.bun,
  digest,
  evidence: { bunVersion: process.versions.bun },
});

test("runner evidence is closed, ordered and composed from exact files", async () => {
  const evidence = await runnerEvidence();
  const expectedPaths = [
    "benchmarks/wbench-1.schema.json",
    "tooling/benchmark-driven-development-machine.mjs",
    "tooling/benchmark-driven-development-runner.mjs",
  ];
  assert.deepEqual([...RUNNER_EVIDENCE_PATHS], expectedPaths);
  assert.deepEqual(evidence.files.map((file) => file.path), expectedPaths);
  assert.deepEqual(evidence.files.map((file) => file.path), [...evidence.files.map((file) => file.path)].sort());
  for (const file of evidence.files) {
    const bytes = await readFile(path.join(ROOT, file.path));
    assert.equal(file.digest, "sha256:" + crypto.createHash("sha256").update(bytes).digest("hex"));
  }
  const recomputed = "sha256:" + crypto.createHash("sha256")
    .update(JSON.stringify({ schema: evidence.schema, files: evidence.files }))
    .digest("hex");
  assert.equal(evidence.digest, recomputed);
  assert.deepEqual(evidence, await runnerEvidence());
});

test("runner arguments are explicit, bounded and fail closed", () => {
  assert.deepEqual(parseRunnerArgs(["--output", "result.json"]), {
    help: false,
    outputPath: "result.json",
    warmupCount: DEFAULT_WARMUP_COUNT,
    sampleCount: DEFAULT_SAMPLE_COUNT,
  });
  assert.deepEqual(parseRunnerArgs(["--output=result.json", "--warmup=3", "--samples=11"]), {
    help: false,
    outputPath: "result.json",
    warmupCount: 3,
    sampleCount: 11,
  });
  for (const args of [
    [],
    ["--output", "result.json", "--samples", "8"],
    ["--output", "result.json", "--samples", "9", "--warmup", "0"],
    ["--output", "result.json", "--force"],
    ["--output", "result.json", "--unknown"],
  ]) {
    assert.throws(() => parseRunnerArgs(args), (error) =>
      error instanceof RunnerError && error.code === "usage");
  }
});

test("latency derives exact u64 median and MAD from raw values", () => {
  assert.deepEqual(calculateLatency([100n, 101n, 102n, 103n, 104n, 105n, 106n, 107n, 108n]), {
    unit: "ns",
    minimumNs: "100",
    medianNs: "104",
    maximumNs: "108",
    madNs: "2",
    derivedFromRawSamples: true,
  });
  assert.throws(() => calculateLatency([1n, 2n, 3n, 4n]), /odd raw sample count/u);
});

test("owned directory cleanup is bounded and never throws", async () => {
  let received;
  const cleaned = await cleanupDirectoryBestEffort("owned-build", async (directory, options) => {
    received = { directory, options };
    throw new Error("simulated cleanup failure");
  });
  assert.equal(cleaned, false);
  assert.deepEqual(received, {
    directory: "owned-build",
    options: { recursive: true, force: true, maxRetries: 3, retryDelay: 100 },
  });
});

test("publication is complete, atomic and refuses overwrite", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd1-runner-test-"));
  try {
    const target = path.join(directory, "result.json");
    await atomicPublishJson(target, { schema: "wbench/1", value: 1 });
    assert.deepEqual(JSON.parse(await readFile(target, "utf8")), {
      schema: "wbench/1",
      value: 1,
    });
    await assert.rejects(() => atomicPublishJson(target, { value: 2 }), /already exists/u);
    assert.deepEqual(JSON.parse(await readFile(target, "utf8")), {
      schema: "wbench/1",
      value: 1,
    });
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("publication remains successful when post-link sibling cleanup fails", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd1-runner-test-"));
  try {
    const target = path.join(directory, "result.json");
    let unlinkCalls = 0;
    await atomicPublishJson(target, { schema: "wbench/1", value: "complete" }, directory, {
      unlink: async (filePath) => {
        unlinkCalls += 1;
        if (unlinkCalls === 1) throw new Error("simulated sibling cleanup failure");
        return unlink(filePath);
      },
    });
    assert.equal(unlinkCalls, 1);
    assert.deepEqual(JSON.parse(await readFile(target, "utf8")), {
      schema: "wbench/1",
      value: "complete",
    });
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("toolchain evidence records Ninja and effective CMake flags, including empty values", async () => {
  const build = await mkdtemp(path.join(tmpdir(), "w-bmd1-runner-test-"));
  const seedDirectoriesBefore = new Set((await readdir(tmpdir()))
    .filter((entry) => entry.startsWith("w-bmd1-seed-")));
  let result;
  try {
    const cache = [
      "CMAKE_GENERATOR:INTERNAL=Ninja",
      "CMAKE_BUILD_TYPE:STRING=Release",
      "CMAKE_MAKE_PROGRAM:FILEPATH=/fake/ninja",
      "CMAKE_C_COMPILER:FILEPATH=/fake/cc",
      "CMAKE_C_COMPILER_VERSION:STRING=1.2.3",
      "CMAKE_SYSTEM_NAME:INTERNAL=TestOS",
      "CMAKE_C_FLAGS:STRING=",
      "CMAKE_C_FLAGS_RELEASE:STRING=-O3",
      "CMAKE_EXE_LINKER_FLAGS:STRING=",
      "CMAKE_EXE_LINKER_FLAGS_RELEASE:STRING=-Wl,--gc-sections",
    ].join("\n") + "\n";
    await writeFile(path.join(build, "CMakeCache.txt"), cache, "utf8");
    result = await buildSeed({
      executor: async (command, args) => {
        if (command === "cmake" && args[0] === "--version") {
          return { exitCode: 0, stdout: Buffer.from("cmake version 3.30.0\n"), stderr: Buffer.alloc(0) };
        }
        if (command === "/fake/cc") {
          return { exitCode: 0, stdout: Buffer.from("fake cc 1.2.3\n"), stderr: Buffer.alloc(0) };
        }
        if (command === "/fake/ninja") {
          return { exitCode: 0, stdout: Buffer.from("1.12.1\n"), stderr: Buffer.alloc(0) };
        }
        if (command === "cmake" && args[0] === "-S") {
          const buildIndex = args.indexOf("-B");
          const buildPath = args[buildIndex + 1];
          await writeFile(path.join(buildPath, "CMakeCache.txt"), cache, "utf8");
          await writeFile(path.join(buildPath,
            process.platform === "win32" ? TARGET_NAME + ".exe" : TARGET_NAME), "fake artifact", "utf8");
        }
        return { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0) };
      },
    });
    assert.equal(result.toolchain.evidence.ninjaVersion, "1.12.1");
    assert.equal(result.toolchain.evidence.cFlags, "");
    assert.equal(result.toolchain.evidence.exeLinkerFlags, "");
    assert.equal(result.toolchain.evidence.bunVersion, process.versions.bun);
    assert.match(result.toolchain.identity, /ninja=1\.12\.1/u);
    assert.ok(result.toolchain.identity.includes("bun=" + process.versions.bun));
    assert.match(result.toolchain.identity, /c-flags=;c-flags-release=-O3/u);
  } finally {
    if (result?.buildDirectory) {
      await rm(result.buildDirectory, { recursive: true, force: true });
    }
    await rm(build, { recursive: true, force: true });
    const seedDirectoriesAfter = new Set((await readdir(tmpdir()))
      .filter((entry) => entry.startsWith("w-bmd1-seed-")));
    assert.deepEqual(
      [...seedDirectoriesAfter].filter((entry) => !seedDirectoriesBefore.has(entry)),
      [],
      "toolchain evidence test must not leak a seed build directory",
    );
  }
});

test("fake executor proves oracle, fresh processes, validation and no partial output", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd1-runner-test-"));
  try {
    const target = path.join(directory, "result.json");
    const documents = loadBmdDocuments();
    const calls = [];
    let tick = 0n;
    const result = await runBenchmark({ outputPath: target }, {
      documents,
      build: async () => ({
        executable: "fake-driver",
        artifactDigest: digest,
        toolchain: fakeToolchain(),
      }),
      invoke: async (args) => {
        calls.push([...args]);
        return { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0) };
      },
      clock: () => ++tick,
      recipe: { schema: "test-recipe", files: ["source-backed"] },
      recipeDigest: digest,
      runnerDigest: digest,
      oracleDigest: digest,
    });
    assert.equal(calls.length, DEFAULT_WARMUP_COUNT + DEFAULT_SAMPLE_COUNT + 1);
    assert.equal(result.result.quality, "exploratory");
    assert.equal(result.result.claim, "measurement-only");
    assert.equal(result.result.comparison, null);
    assert.equal(result.result.samples.order, "single-series");
    assert.equal(result.result.oracle.complete, true);
    assert.equal(result.result.oracle.beforeSamples, true);
    assert.deepEqual(result.result.identity.command, documents.manifest.command);
    assert.equal(result.result.provenance.artifactDigest, digest);
    assert.equal(JSON.parse(await readFile(target, "utf8")).id, result.result.id);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("runner rejects missing Bun toolchain evidence before the oracle", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd1-runner-test-"));
  try {
    const target = path.join(directory, "result.json");
    const incompleteToolchain = fakeToolchain();
    delete incompleteToolchain.evidence.bunVersion;
    let invoked = false;
    await assert.rejects(() => runBenchmark({ outputPath: target }, {
      build: async () => ({ executable: "fake-driver", artifactDigest: digest, toolchain: incompleteToolchain }),
      invoke: async () => {
        invoked = true;
        return { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0) };
      },
      recipe: { schema: "test-recipe" },
      recipeDigest: digest,
      runnerDigest: digest,
      oracleDigest: digest,
    }), (error) => error instanceof RunnerError && error.code === "toolchain");
    assert.equal(invoked, false);
    await assert.rejects(() => readFile(target), { code: "ENOENT" });
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("existing output is rejected before build and oracle", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd1-runner-test-"));
  try {
    const target = path.join(directory, "result.json");
    await writeFile(target, "existing\n", "utf8");
    let buildCalled = false;
    await assert.rejects(() => runBenchmark({ outputPath: target }, {
      build: async () => { buildCalled = true; return {}; },
    }), /already exists/u);
    assert.equal(buildCalled, false);
    assert.equal(await readFile(target, "utf8"), "existing\n");
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("oracle failure leaves no result", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd1-runner-test-"));
  try {
    const target = path.join(directory, "result.json");
    await assert.rejects(() => runBenchmark({ outputPath: target }, {
      build: async () => ({ executable: "fake-driver", artifactDigest: digest, toolchain: fakeToolchain() }),
      invoke: async () => ({ exitCode: 1, stdout: Buffer.alloc(0), stderr: Buffer.from("oracle failed") }),
      recipe: { schema: "test-recipe" },
      recipeDigest: digest,
      runnerDigest: digest,
      oracleDigest: digest,
    }), /driver must exit 0/u);
    await assert.rejects(() => readFile(target), { code: "ENOENT" });
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("injected build directories remain caller-owned on runner failure", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd1-runner-test-"));
  const callerBuild = path.join(directory, "caller-build");
  try {
    await mkdir(callerBuild);
    const marker = path.join(callerBuild, "marker");
    await writeFile(marker, "caller-owned\n", "utf8");
    const target = path.join(directory, "result.json");
    await assert.rejects(() => runBenchmark({ outputPath: target }, {
      build: async () => ({
        buildDirectory: callerBuild,
        executable: "fake-driver",
        artifactDigest: digest,
        toolchain: fakeToolchain(),
      }),
      invoke: async () => ({ exitCode: 1, stdout: Buffer.alloc(0), stderr: Buffer.from("oracle failed") }),
      recipe: { schema: "test-recipe" },
      recipeDigest: digest,
      runnerDigest: digest,
      oracleDigest: digest,
    }), /driver must exit 0/u);
    assert.equal(await readFile(marker, "utf8"), "caller-owned\n");
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("comparison schedule is deterministic and balances first orientation", () => {
  const seed = "ab".repeat(32);
  const first = pairedScheduleForSeed(seed, COMPARISON_DEFAULT_PAIR_COUNT);
  const second = pairedScheduleForSeed(seed, COMPARISON_DEFAULT_PAIR_COUNT);
  assert.deepEqual(first, second);
  assert.equal(first.length, 9);
  const baselineFirst = first.filter((round) => round.first === "baseline").length;
  const candidateFirst = first.filter((round) => round.first === "candidate").length;
  assert.ok(Math.abs(baselineFirst - candidateFirst) <= 1);
  for (const round of first) assert.notEqual(round.first, round.second);
});

test("comparison options require two complete SHAs and never expose a seed option", () => {
  assert.equal(validateCommitSha("A".repeat(40)), "a".repeat(40));
  assert.throws(() => validateCommitSha("HEAD"), /complete 40-hex/u);
  assert.deepEqual(parseRunnerArgs([
    "--output", "result.json",
    "--baseline", "1".repeat(40),
    "--candidate", "2".repeat(40),
  ]), {
    help: false,
    outputPath: "result.json",
    warmupCount: DEFAULT_WARMUP_COUNT,
    sampleCount: DEFAULT_SAMPLE_COUNT,
    baseline: "1".repeat(40),
    candidate: "2".repeat(40),
  });
  for (const args of [
    ["--output", "result.json", "--baseline", "1".repeat(40)],
    ["--output", "result.json", "--candidate", "2".repeat(40)],
    ["--output", "result.json", "--baseline", "HEAD", "--candidate", "HEAD"],
    ["--output", "result.json", "--baseline", "1".repeat(40), "--candidate", "2".repeat(40), "--seed", "x"],
  ]) assert.throws(() => parseRunnerArgs(args), (error) => error instanceof RunnerError && error.code === "usage");
  assert.throws(() => validateComparisonOptions({ outputPath: "result.json", baseline: "1".repeat(40) }), /required for comparison/u);
});

test("fake comparison runner completes both oracles before paired samples", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd2-runner-test-"));
  const target = path.join(directory, "result.json");
  const documents = loadBmdDocuments();
  const roleDigest = "sha256:" + "2".repeat(64);
  const calls = [];
  const makeBuild = ({ commit, role }) => ({
    commit,
    closure: {
      digest: roleDigest,
      evidence: { schema: "wbench/1-compiler-seed-c-closure", root: "compiler/seed-c", files: [{ path: "compiler/seed-c/file", digest }] },
    },
    build: {
      executable: "fake-" + role,
      artifactDigest: "sha256:" + (role === "baseline" ? "3" : "4").repeat(64),
      toolchain: fakeToolchain(),
    },
  });
  let tick = 0n;
  try {
    const execution = await runComparisonBenchmark({
      outputPath: target,
      baseline: "1".repeat(40),
      candidate: "2".repeat(40),
      warmupCount: 2,
    }, {
      documents,
      randomBytes: () => Buffer.alloc(32, 7),
      buildRole: async ({ commit, role }) => makeBuild({ commit, role }),
      invokeRole: async (role) => {
        calls.push(role);
        return { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0) };
      },
      clock: () => ++tick,
    });
    assert.deepEqual(calls.slice(0, 2), ["baseline", "candidate"]);
    assert.equal(calls.length, 2 + 2 * (2 + COMPARISON_DEFAULT_PAIR_COUNT));
    assert.equal(execution.result.claim, "comparison-only");
    assert.equal(execution.result.comparison.calibration, true);
    assert.equal(execution.result.samples.raw.length, 18);
    assert.equal(execution.result.samples.warmup.length, 4);
    assert.deepEqual(execution.result.samples.warmup.map((sample) => sample.round), [1, 1, 2, 2]);
    assert.deepEqual(execution.result.samples.warmup.map((sample) => sample.position), ["first", "second", "first", "second"]);
    assert.deepEqual(validateResult(execution.result, documents.manifest), []);
    assert.equal(JSON.parse(await readFile(target, "utf8")).id, execution.result.id);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("baseline oracle failure publishes no result and starts no samples", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd2-runner-test-"));
  try {
    const target = path.join(directory, "result.json");
    let sampled = false;
    const makeBuild = ({ commit, role }) => ({
      commit,
      closure: { digest: "sha256:" + "2".repeat(64), evidence: { files: [{ path: "compiler/seed-c/file", digest }] } },
      build: { executable: role, artifactDigest: digest, toolchain: fakeToolchain() },
    });
    await assert.rejects(() => runComparisonBenchmark({ outputPath: target, baseline: "1".repeat(40), candidate: "2".repeat(40) }, {
      buildRole: async ({ commit, role }) => makeBuild({ commit, role }),
      invokeRole: async (role) => role === "baseline"
        ? { exitCode: 1, stdout: Buffer.alloc(0), stderr: Buffer.from("oracle failed") }
        : { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0) },
      randomBytes: () => Buffer.alloc(32, 9),
      clock: () => { sampled = true; return 1n; },
    }), /driver must exit 0/u);
    assert.equal(sampled, false);
    await assert.rejects(() => readFile(target), { code: "ENOENT" });
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("candidate oracle failure follows baseline oracle and precedes samples", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd2-runner-test-"));
  try {
    const target = path.join(directory, "result.json");
    const calls = [];
    let sampled = false;
    const makeBuild = ({ commit, role }) => ({
      commit,
      closure: { digest: "sha256:" + "2".repeat(64), evidence: { files: [{ path: "compiler/seed-c/file", digest }] } },
      build: { executable: role, artifactDigest: digest, toolchain: fakeToolchain() },
    });
    await assert.rejects(() => runComparisonBenchmark({ outputPath: target, baseline: "1".repeat(40), candidate: "2".repeat(40) }, {
      buildRole: async ({ commit, role }) => makeBuild({ commit, role }),
      invokeRole: async (role) => {
        calls.push(role);
        return role === "candidate"
          ? { exitCode: 1, stdout: Buffer.alloc(0), stderr: Buffer.from("candidate oracle failed") }
          : { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0) };
      },
      randomBytes: () => Buffer.alloc(32, 10),
      clock: () => { sampled = true; return 1n; },
    }), /driver must exit 0/u);
    assert.deepEqual(calls, ["baseline", "candidate"]);
    assert.equal(sampled, false);
    await assert.rejects(() => readFile(target), { code: "ENOENT" });
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("toolchain divergence rejects before either oracle or sample", async () => {
  const directory = await mkdtemp(path.join(tmpdir(), "w-bmd2-runner-test-"));
  try {
    const target = path.join(directory, "result.json");
    const invoked = [];
    let sampled = false;
    const makeBuild = ({ commit, role }) => ({
      commit,
      closure: { digest: "sha256:" + "2".repeat(64), evidence: { files: [{ path: "compiler/seed-c/file", digest }] } },
      build: {
        executable: role,
        artifactDigest: digest,
        toolchain: role === "candidate"
          ? { ...fakeToolchain(), digest: "sha256:" + "3".repeat(64) }
          : fakeToolchain(),
      },
    });
    await assert.rejects(() => runComparisonBenchmark({ outputPath: target, baseline: "1".repeat(40), candidate: "2".repeat(40) }, {
      buildRole: async ({ commit, role }) => makeBuild({ commit, role }),
      invokeRole: async (role) => {
        invoked.push(role);
        return { exitCode: 0, stdout: Buffer.alloc(0), stderr: Buffer.alloc(0) };
      },
      randomBytes: () => Buffer.alloc(32, 11),
      clock: () => { sampled = true; return 1n; },
    }), /toolchain digests diverge before samples/u);
    assert.deepEqual(invoked, []);
    assert.equal(sampled, false);
    await assert.rejects(() => readFile(target), { code: "ENOENT" });
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
