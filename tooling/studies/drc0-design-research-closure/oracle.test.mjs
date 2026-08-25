import { describe, expect, test } from "bun:test";
import { deriveClosure, loadCorpus } from "../../design-research-closure-machine.mjs";

describe("DRC0 design research closure", () => {
  test("closes all four finite research gates without an implementation claim", () => {
    const result = deriveClosure(loadCorpus());
    expect(result.errors).toEqual([]);
    expect(result.researchGates).toEqual([]);
    expect(Object.values(result.studyFacts).every((study) => study.valid)).toBe(true);
  });

  test("rejects a missing evidence boundary and caller-owned result", () => {
    const missing = structuredClone(loadCorpus());
    missing.cases[0].missingEvidence = [];
    expect(deriveClosure(missing).errors.some((error) => error.includes("missing evidence"))).toBe(true);
    const forged = structuredClone(loadCorpus());
    forged.cases[1].result = "accepted";
    expect(deriveClosure(forged).errors.some((error) => error.includes("caller-owned"))).toBe(true);
  });
});
