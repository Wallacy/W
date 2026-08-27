import { describe, expect, test } from "bun:test";
import { deriveClosure, loadCorpus } from "../../design-research-closure-machine.mjs";

describe("DRC0 design research closure", () => {
  test("closes all four finite research gates without an implementation claim", () => {
    const result = deriveClosure(loadCorpus());
    expect(result.errors).toEqual([]);
    expect(result.researchGates).toEqual([]);
    expect(result.activeResearchGates).toEqual(["W-1486"]);
    expect(result.historicalResearchZeroThrough).toBe("W-1459");
    expect(Object.values(result.studyFacts).every((study) => study.valid)).toBe(true);
    expect(result.studyFacts.SYNC1.facts).toMatchObject({
      explicitNeverSuspendAccepted: true,
      explicitAwaitRejected: true,
      dynamicPathRejected: true,
      blocksThread: false,
      createsTask: false,
      suspendsTask: false,
      sameExecutionContext: true,
      runtimeFallback: false,
      asyncEntryPublishesMay: true,
      selectedDirectEntryNeverSuspend: true,
      indirectFacetAccepted: true,
      composedDirectEntryAccepted: true,
      transitiveFacetLossRejected: true,
      invalidSyncPoisonsCaller: true,
      syncSccEligible: true,
      syncSccTerminationUnproven: true,
      syncSccNotExecuted: true,
      erasedFacetRejected: true,
      facetRemovalSourceBreaking: true,
      semanticInterfaceKeyChanged: true,
    });
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
