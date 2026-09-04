import { describe, expect, test } from "bun:test";
import corpus from "../../final-research-closure-cases.json" with { type: "json" };
import {
  loadState,
  runFRC0Case,
  sortResearchStateArtifacts,
  validateResearchStateInventory,
  validateCorpus,
} from "../../final-research-closure-machine.mjs";

describe("FRC0 final research closure host oracle", () => {
  test("orders artifact paths by UTF-8 bytes", () => {
    const bmp = "tooling/\uE000";
    const astral = "tooling/\u{10000}";
    expect(sortResearchStateArtifacts([astral, bmp])).toEqual([bmp, astral]);
  });

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
    expect(current.facts.researchStateInventory).toMatchObject({
      active: [],
      status: "authoritative-maintained-surface",
      normalized: true,
      familyCount: 15,
      normalizationPendingCount: 0,
    });
    expect(current.facts.researchStateInventory).toMatchObject({
      categoryCounts: {
        historical: 8,
        rejected: 1,
        "current-design-evidence-gap": 5,
        "future-reopen-candidate": 1,
      },
    });
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

  test("rejects a reintroduced normalization migration state", () => {
    const state = loadState();
    state.researchStateInventory.status = "normalization-in-progress";
    state.researchStateInventory.families.find((family) => family.id === "cyc1").normalizationPending = true;
    const current = corpus.cases.find((testCase) =>
      testCase.kind === "current-contract" && testCase.decisions?.[0] === "W-731" && testCase.gate === "freeze-research-close");
    expect(runFRC0Case(current, { state }).status).toBe("rejected");
  });

  test("rejects a merged or duplicated BRX2/BRX3 inventory family", () => {
    const inventory = loadState().researchStateInventory;
    const merged = structuredClone(inventory);
    merged.families.find((family) => family.id === "brx2").id = "brx2-brx3";
    expect(validateResearchStateInventory(merged).some((error) =>
      error.includes("unknown") || error.includes("exhaustive"))).toBe(true);

    const duplicate = structuredClone(inventory);
    duplicate.families.find((family) => family.id === "brx3").id = "brx2";
    expect(validateResearchStateInventory(duplicate).some((error) =>
      error.includes("unique") || error.includes("exhaustive"))).toBe(true);
  });

  test("rejects a merged or duplicated ATOM1/ATOM2 inventory family", () => {
    const inventory = loadState().researchStateInventory;
    const merged = structuredClone(inventory);
    merged.families.find((family) => family.id === "atom1").id = "atom1-atom2";
    expect(validateResearchStateInventory(merged).some((error) =>
      error.includes("unknown") || error.includes("exhaustive"))).toBe(true);

    const duplicate = structuredClone(inventory);
    duplicate.families.find((family) => family.id === "atom2").id = "atom1";
    expect(validateResearchStateInventory(duplicate).some((error) =>
      error.includes("unique") || error.includes("exhaustive"))).toBe(true);
  });
});
