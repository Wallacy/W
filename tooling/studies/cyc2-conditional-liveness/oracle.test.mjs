import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { buildCYC2Snapshot, evaluateCYC2Case, validateCYC2Corpus } from "../../cyc2-conditional-liveness-machine.mjs";

const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "../../cyc2-conditional-liveness-cases.json"), "utf8"));

describe("CYC2 conditional liveness host oracle", () => {
  test("closes the baseline without an active Research route", () => {
    const checked = validateCYC2Corpus(corpus);
    expect(checked.errors).toEqual([]);
    expect(checked.metrics.baselineCompositions).toBe(3);
    expect(checked.metrics.activeResearch).toBe(0);
  });

  test("reopening is a future review, not a baseline primitive", () => {
    const result = evaluateCYC2Case(corpus.cases.find((item) => item.id === "CYC2-REOPEN-qualified"));
    expect(result.status).toBe("future-reopen-candidate");
    expect(result.code).toBe("future-ephemeron-review");
  });

  test("snapshot is deterministic", () => {
    expect(buildCYC2Snapshot(corpus).text).toBe(buildCYC2Snapshot(corpus).text);
  });
});
