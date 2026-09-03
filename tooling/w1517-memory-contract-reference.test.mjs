import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import { evaluateMemoryContract, runW1517Case } from "./w1517-memory-contract-machine.mjs";

const directory = new URL(".", import.meta.url);
const corpus = JSON.parse(fs.readFileSync(new URL("w1517-memory-contract-cases.json", directory), "utf8"));

test("all W-1517 corpus cases reduce to their declared design outcome", () => {
  for (const testCase of corpus.cases) {
    const actual = runW1517Case(testCase);
    assert.equal(actual.status, testCase.expected.status, testCase.id);
    if (actual.status === "rejected") assert.equal(actual.code, testCase.expected.code, testCase.id);
  }
});

test("strict native-stack proof rejects task-frame fallback and unknown recursion", () => {
  const base = {
    allocatorPlan: {
      kind: ".stack", capacity: 1024, alignment: 16, target: "x86_64",
      guardProbing: true, callPathAdmission: true,
    },
  };
  assert.equal(evaluateMemoryContract({ ...base, physicalClass: "taskFrame" }).code, "stackTaskFrameRejected");
  assert.equal(evaluateMemoryContract({ ...base, allocatorPlan: { ...base.allocatorPlan, fallback: true } }).code, "stackFallbackForbidden");
  assert.equal(evaluateMemoryContract({ ...base, recursion: "unknown" }).code, "strictSummaryUnknown");
});

test("profile and module contracts fail closed without provider implementation claims", () => {
  assert.equal(
    evaluateMemoryContract({ profile: { dynamicAllocation: ".allow" } }).code,
    "memoryProfileFieldRequired",
  );
  assert.equal(
    evaluateMemoryContract({ module: { functionAnnotation: { allocation: ".forbidDynamic" } } }).code,
    "functionAnnotationNotCurrent",
  );
  assert.equal(
    evaluateMemoryContract({ functionSummary: "provider-conformant" }).code,
    "implementationClaimForbidden",
  );
});
