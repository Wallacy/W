import assert from "node:assert/strict";
import test from "node:test";
import { loadExecutableDocuments, selectExecutableSuiteLanes } from "./executable-benchmark-machine.mjs";
import { parseExecutableSuiteArguments, runExecutableSuite } from "./executable-benchmark-suite.mjs";

const catalog = loadExecutableDocuments().catalog;

function fakeIdentity(lane) {
  const workload = catalog.workloads.find((entry) => entry.id === lane.workloadId);
  const source = workload.sources.find((entry) =>
    entry.language === lane.language && entry.platformTarget === lane.platformTarget);
  return {
    workloadId: lane.workloadId,
    language: lane.language,
    platformTarget: lane.platformTarget,
    identity: { toolchain: `fake-${lane.language}-${lane.platformTarget}`, recipe: source.recipe },
    provenance: { toolchainDigest: `sha256:${"a".repeat(64)}` },
  };
}

test("suite wrapper runs lanes serially with default sampling, validates all results, and publishes one full receipt", async () => {
  assert.deepEqual(parseExecutableSuiteArguments([]), {
    platforms: ["windows-x64", "linux-wsl-x64"], mode: "full",
  });
  assert.deepEqual(parseExecutableSuiteArguments(["--platform", "linux-wsl-x64"]), {
    platforms: ["linux-wsl-x64"], mode: "filtered",
  });
  assert.throws(() => parseExecutableSuiteArguments(["--platform", "linux-x64"]), /usage:/u);

  const calls = [];
  const receipts = [];
  let active = false;
  let ticks = 0;
  const receipt = await runExecutableSuite({
    catalog,
    id: () => "test-run",
    now: () => ticks++ === 0 ? 100 : 104_321,
    observedAt: () => "2026-09-22T12:00:00.000Z",
    ensureClean: async () => {},
    execute: async ({ args, label }) => {
      assert.equal(active, false, "suite commands must not overlap");
      active = true;
      calls.push({ args, label });
      await Promise.resolve();
      active = false;
      return { code: 0, stdout: "", stderr: "" };
    },
    readResult: async (_resultPath, lane) => fakeIdentity(lane),
    publishReceipt: async (value) => receipts.push(value),
  });

  const lanes = selectExecutableSuiteLanes(catalog);
  assert.ok(lanes.length > 0);
  assert.equal(new Set(lanes.map((lane) => `${lane.workloadId}/${lane.language}/${lane.platformTarget}`)).size, lanes.length,
    "suite selection is unique as the eligible catalog evolves");
  assert.equal(receipt.mode, "full");
  assert.deepEqual(receipt.laneCounts, { total: lanes.length, passed: lanes.length, failed: 0, skipped: 0 });
  assert.equal(receipt.durationMs, 104_221);
  assert.equal(receipts.length, 1);
  assert.equal(receipt.lanes.length, lanes.length);
  assert.ok(receipt.lanes.every((lane) => lane.toolchain.startsWith("fake-") && lane.recipe && lane.toolchainDigest));

  const runs = calls.filter((call) => call.args[1] === "run");
  const validations = calls.filter((call) => call.args[1] === "validate");
  const updates = calls.filter((call) => call.args[1] === "update");
  assert.equal(runs.length, lanes.length);
  assert.equal(validations.length, lanes.length);
  assert.equal(updates.length, 1);
  assert.deepEqual(calls.slice(0, lanes.length).map((call) => call.args[1]), Array(lanes.length).fill("run"));
  assert.deepEqual(calls.slice(lanes.length, lanes.length * 2).map((call) => call.args[1]), Array(lanes.length).fill("validate"));
  assert.equal(calls.at(-2).args[1], "check");
  assert.equal(calls.at(-1).args[0], "tooling/executable-benchmark-docs.mjs");
  assert.equal(calls.at(-1).args[1], "--check");
  assert.ok(runs.every((call) => !call.args.includes("--samples") && !call.args.includes("--warmup") && !call.args.includes("--compile-samples") && !call.args.includes("--run-samples")),
    "full suite keeps runner sampling defaults");
  assert.deepEqual(calls.find((call) => call.args[1] === "update").args.slice(2), runs.map((call) => call.args.at(-1)));
});

test("a failed WSL lane surfaces its status and output and does not update or publish a success receipt", async () => {
  const calls = [];
  const receipts = [];
  await assert.rejects(runExecutableSuite({
    catalog,
    platforms: ["linux-wsl-x64"],
    mode: "filtered",
    id: () => "failing-run",
    ensureClean: async () => {},
    execute: async ({ args }) => {
      calls.push(args);
      return { code: 127, stdout: "", stderr: "WSL distribution unavailable" };
    },
    publishReceipt: async (receipt) => receipts.push(receipt),
  }), (error) => {
    assert.match(error.message, /hello-platform-minimal\/w\/linux-wsl-x64/u);
    assert.match(error.message, /exit 127/u);
    assert.match(error.message, /WSL distribution unavailable/u);
    assert.match(error.message, /no new success receipt/u);
    assert.equal(error.remaining, 2);
    return true;
  });
  assert.equal(calls.length, 1, "failure is explicit and fail-fast; unattempted lanes are reported");
  assert.equal(calls.some((args) => args[1] === "update"), false);
  assert.equal(receipts.length, 0);
});
