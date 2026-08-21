import { describe, expect, test } from "bun:test";
import {
  loadCorpus,
  mutationChecks,
  validateCorpus,
} from "../../pfu0-pre-freeze-usability-machine.mjs";

describe("PFU0 pre-freeze usability host oracle", () => {
  test("derives current controls, one accepted candidate, and rejected routes", () => {
    const corpus = loadCorpus();
    const checked = validateCorpus(corpus);
    expect(checked.errors).toEqual([]);
    expect(checked.results).toHaveLength(9);
    expect(checked.results.filter((result) => result.status === "accepted")).toHaveLength(4);
    expect(checked.results.filter((result) => result.status === "rejected")).toHaveLength(5);
    const candidates = new Map(checked.results.filter((result) => result.variant === "candidate").map((result) => [result.family, result]));
    expect(candidates.get("manifest")).toMatchObject({ status: "accepted", route: "current-control", promotion: false });
    expect(candidates.get("service")).toMatchObject({ status: "rejected", route: "rejected-route", promotion: false });
    expect(candidates.get("property")).toMatchObject({ status: "rejected", route: "rejected-route", promotion: false });
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
