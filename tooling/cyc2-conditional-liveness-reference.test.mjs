import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { evaluateCYC2Case, validateCYC2Corpus } from "./cyc2-conditional-liveness-machine.mjs";

const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "cyc2-conditional-liveness-cases.json"), "utf8"));

describe("CYC2 conditional liveness closure", () => {
  test("three existing compositions close the baseline", () => {
    const checked = validateCYC2Corpus(corpus);
    expect(checked.errors).toEqual([]);
    expect(checked.metrics.baselineCompositions).toBe(3);
    expect(checked.metrics.activeResearch).toBe(0);
  });

  test("weak-key, ephemeron, collector, and finalizer remain rejected", () => {
    for (const id of [
      "CYC2-REJECT-naive-weak-key",
      "CYC2-REJECT-ephemeron",
      "CYC2-REJECT-transparent-collector",
      "CYC2-REJECT-hidden-finalizer",
    ]) {
      const result = evaluateCYC2Case(corpus.cases.find((item) => item.id === id));
      expect(result.status).toBe("intentionally-rejected");
    }
  });

  test("future reopening needs every bounded criterion", () => {
    const blocked = evaluateCYC2Case(corpus.cases.find((item) => item.id === "CYC2-REOPEN-insufficient"));
    const qualified = evaluateCYC2Case(corpus.cases.find((item) => item.id === "CYC2-REOPEN-qualified"));
    expect(blocked.status).toBe("reopen-blocked");
    expect(qualified.status).toBe("future-reopen-candidate");
  });

  test("collector side effects cannot be enabled by mutation", () => {
    const mutated = structuredClone(corpus.cases.find((item) => item.id === "CYC2-POS-detached-value"));
    mutated.kind = "transparent-collector";
    mutated.composition = { collectorSideEffects: true, finalizer: true };
    expect(evaluateCYC2Case(mutated).status).toBe("intentionally-rejected");
  });
});
