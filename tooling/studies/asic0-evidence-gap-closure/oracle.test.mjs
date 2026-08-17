import { describe, expect, test } from "bun:test";
import corpus from "../../asic0-evidence-gap-closure-cases.json" with { type: "json" };
import {
  DECISIONS,
  GAP_CATEGORY,
  deriveAsic0Case,
  validateCorpus,
} from "../../asic0-evidence-gap-closure-machine.mjs";
import { mutationChecks } from "../../check-asic0-evidence-gap-closure.mjs";

const results = corpus.cases.map(deriveAsic0Case);

describe("ASIC0 reuse-only host oracle", () => {
  test("links ten cases to five original decisions with current and adversarial routes", () => {
    expect(validateCorpus(corpus).errors).toEqual([]);
    expect(corpus.cases).toHaveLength(10);
    expect(DECISIONS).toHaveLength(5);
    for (const decision of DECISIONS) {
      const cases = corpus.cases.filter((item) => item.decisions.includes(decision));
      expect(cases.some((item) => item.kind === "current-contract")).toBe(true);
      expect(cases.some((item) => item.kind === "rejected-route")).toBe(true);
    }
  });

  test("keeps current and adversarial projections separate", () => {
    expect(results.filter((result) => result.contractRoute === "current")).toHaveLength(5);
    expect(results.filter((result) => result.contractRoute === "rejected")).toHaveLength(5);
    expect(results.every((result) => result.gapCategory === GAP_CATEGORY)).toBe(true);
    expect(results.filter((result) => result.status === "accepted")).toHaveLength(5);
    expect(results.filter((result) => result.status === "rejected")).toHaveLength(5);
  });

  test("reuses source cases without copying payload", () => {
    for (const item of corpus.cases) {
      expect(Object.keys(item.source).sort()).toEqual(["caseId", "corpus", "machine", "study"]);
      expect(item.source).not.toHaveProperty("input");
      expect(item.source).not.toHaveProperty("operations");
    }
  });

  test("detects all real helper mutations", () => {
    const checks = mutationChecks();
    expect(Object.keys(checks)).toHaveLength(13);
    expect(Object.values(checks).every(Boolean)).toBe(true);
  });
});
