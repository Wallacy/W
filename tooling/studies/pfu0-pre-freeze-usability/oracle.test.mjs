import { describe, expect, test } from "bun:test";
import {
  loadCorpus,
  mutationChecks,
  validateCorpus,
} from "../../pfu0-pre-freeze-usability-machine.mjs";

describe("PFU0 pre-freeze usability host oracle", () => {
  test("derives three current controls, three candidates, and three rejected routes", () => {
    const corpus = loadCorpus();
    const checked = validateCorpus(corpus);
    expect(checked.errors).toEqual([]);
    expect(checked.results).toHaveLength(9);
    expect(checked.results.filter((result) => result.status === "accepted")).toHaveLength(6);
    expect(checked.results.filter((result) => result.status === "rejected")).toHaveLength(3);
    expect(checked.results.filter((result) => result.variant === "candidate").every((result) => result.route === "candidate-research" && result.promotion === false)).toBe(true);
  });

  test("keeps source, implementation, and human/model boundaries explicit", () => {
    const checked = validateCorpus(loadCorpus());
    for (const result of checked.results) {
      expect(result.evidenceState).toBe("design-oracle-input");
      expect(result.hostOnly).toBe(true);
      expect(result.implementationClaimed).toBe(false);
      expect(result.humanResultsClaimed).toBe(false);
      expect(result.modelResultsClaimed).toBe(false);
    }
  });

  test("rejects hidden package, transport, oldValue, and observer mutations", () => {
    expect(Object.values(mutationChecks()).every(Boolean)).toBe(true);
  });
});
