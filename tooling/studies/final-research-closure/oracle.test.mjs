import { describe, expect, test } from "bun:test";
import corpus from "../../final-research-closure-cases.json" with { type: "json" };
import {
  loadState,
  runFRC0Case,
  validateCorpus,
} from "../../final-research-closure-machine.mjs";

describe("FRC0 final research closure host oracle", () => {
  test("derives exactly one current and one rejected route per gate", () => {
    const state = loadState();
    const validation = validateCorpus(corpus);
    expect(validation.errors).toEqual([]);
    expect(corpus.cases).toHaveLength(6);
    const results = corpus.cases.map((testCase) => runFRC0Case(testCase, { state }));
    expect(results.filter((result) => result.status === "accepted")).toHaveLength(3);
    expect(results.filter((result) => result.status === "rejected")).toHaveLength(3);
    for (const decision of corpus.decisions) {
      const routes = results.filter((result) => result.decision === decision);
      expect(routes.filter((result) => result.status === "accepted")).toHaveLength(1);
      expect(routes.filter((result) => result.status === "rejected")).toHaveLength(1);
    }
  });

  test("keeps implementation and human/model boundaries explicit", () => {
    const state = loadState();
    const current = corpus.cases.filter((testCase) => testCase.kind === "current-contract");
    for (const testCase of current) {
      const result = runFRC0Case(testCase, { state });
      expect(result.evidenceState).toBe("design-oracle-input");
      expect(result.hostOnly).toBe(true);
      expect(result.implementationClaimed).toBe(false);
      expect(result.humanResultsClaimed).toBe(false);
      expect(result.modelResultsClaimed).toBe(false);
    }
  });

  test("keeps the historical boundary and PFU0 closure explicit", () => {
    const state = loadState();
    const current = runFRC0Case(corpus.cases.find((testCase) => testCase.id === "FRC0-W-731-current"), { state });
    expect(current.status).toBe("accepted");
    expect(current.facts.historicalSnapshot).toEqual({
      first: "W-001",
      last: "W-1450",
      count: 1450,
      researchZero: true,
    });
    expect(current.facts.reopenedCategories).toEqual({
      "W-1451": "oracle-backed-current",
      "W-1452": "superseded",
      "W-1453": "superseded",
    });
    expect(current.facts.reopenedResearch).toBe(true);
    expect(current.facts.pfuSupersessionValid).toBe(true);
    expect(current.facts.globalResearch).toEqual([]);
    expect(current.facts.globalResearchExact).toBe(true);
    expect(current.facts.activeResearchGates).toEqual([]);
    expect(current.facts.historicalPostSnapshotResearchGates).toEqual(["W-1486", "W-1503"]);
    expect(current.facts.designOnlyClosures).toEqual({
      "W-1517": "oracle-backed-current",
      "W-1518": "oracle-backed-current",
    });
  });

  test("rejects copied-state mutations for all three gates", () => {
    const state = loadState();
    const adversarial = corpus.cases.filter((testCase) => testCase.kind === "rejected-route");
    for (const testCase of adversarial) {
      expect(runFRC0Case(testCase, { state }).status).toBe("rejected");
    }
  });
});
