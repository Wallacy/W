import { describe, expect, test } from "bun:test";
import {
  loadCorpus,
  mutationChecks,
  researchZero,
  validateCorpus,
} from "../../aeg0-app-essentials-gate-machine.mjs";

describe("AEG0 App Essentials Gate host oracle", () => {
  test("derives six current routes and at least seven rejected routes", () => {
    const checked = validateCorpus(loadCorpus());
    expect(checked.errors).toEqual([]);
    expect(checked.results.filter((result) => result.status === "accepted")).toHaveLength(6);
    expect(checked.results.filter((result) => result.status === "rejected").length).toBeGreaterThanOrEqual(7);
    expect(checked.results.find((result) => result.caseId === "AEG0-W-1458-current")?.status).toBe("accepted");
    expect(checked.results.find((result) => result.caseId === "AEG0-W-1458-expiry-current")?.status).toBe("accepted");
  });

  test("keeps implementation and the historical W-1459 study boundary explicit", () => {
    const checked = validateCorpus(loadCorpus());
    for (const result of checked.results) {
      expect(result.evidenceState).toBe("design-oracle-input");
      expect(result.hostOnly).toBe(true);
      expect(result.implementationClaimed).toBe(false);
      expect(result.humanResultsClaimed).toBe(false);
      expect(result.modelResultsClaimed).toBe(false);
    }
    expect(researchZero()).toBe(true);
  });

  test("rejects ambient, fallback, plaintext, inference, and stale mutations", () => {
    expect(Object.values(mutationChecks()).every(Boolean)).toBe(true);
  });
});
