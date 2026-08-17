import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, test } from "bun:test";
import { buildGen2Snapshot, evaluateGen2Case, validateGen2Corpus } from "./gen2-stream-yield-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "gen2-stream-yield-cases.json"), "utf8"));

describe("GEN2 stream expression host oracle", () => {
  test("the source-backed corpus validates and promotes only the narrow form", () => {
    const checked = validateGen2Corpus(corpus);
    expect(checked.errors).toEqual([]);
    expect(checked.decision.status).toBe("promote-narrow-form");
    expect(checked.results).toHaveLength(20);
    expect(checked.decision.ergonomicWins).toBeGreaterThanOrEqual(3);
    expect(checked.decision.negativeCases).toBeGreaterThanOrEqual(8);
  });

  test("both reducers preserve semantic traces for every positive and negative case", () => {
    for (const caseData of corpus.cases) {
      const result = evaluateGen2Case(caseData);
      expect(result.current.loweringsEquivalent).toBe(true);
      expect(result.yield.loweringsEquivalent).toBe(true);
      expect(result.pass).toBe(true);
    }
  });

  test("human-first metrics count decisions, not LOC", () => {
    const reductions = corpus.cases.filter((caseData) => caseData.class === "positive").map((caseData) => evaluateGen2Case(caseData));
    expect(reductions.filter((result) => result.ergonomic === "yield-reduces-ceremony").length).toBeGreaterThanOrEqual(3);
    expect(reductions.every((result) => Number.isInteger(result.current.metrics.humanFirstScore))).toBe(true);
    expect(reductions.every((result) => Number.isInteger(result.yield.metrics.bytes))).toBe(true);
  });

  test("negative contract gates reject the broad generator surface", () => {
    const expectedReasons = new Set([
      "yield-view-borrow", "yield-dialogue", "yield-hidden-capacity", "yield-return-value",
      "yield-untyped-failure", "yield-reentrant-next", "yield-in-defer", "yield-inout-capture",
      "yield-implicit-capture",
      "yield-ffi-resume", "yield-public-frame", "yield-missing-ownership", "yield-copy-nonduplicable",
    ]);
    const negatives = corpus.cases.filter((caseData) => caseData.class === "negative").map(evaluateGen2Case);
    expect(new Set(negatives.map((result) => result.yield.reason))).toEqual(expectedReasons);
  });

  test("snapshot is deterministic and semantic-only", () => {
    const snapshot = buildGen2Snapshot(corpus).text;
    expect(snapshot).toContain('"kind":"gen2-stream-yield"');
    expect(snapshot).toContain('"status":"promote-narrow-form"');
    expect(snapshot).not.toContain('"resumeToken"');
    expect(snapshot).toBe(buildGen2Snapshot(corpus).text);
  });

  test("mutation of ownership or expected rejection cannot pass", () => {
    const ownedMutation = structuredClone(corpus.cases[0]);
    ownedMutation.operations[2].owned = false;
    expect(evaluateGen2Case(ownedMutation).pass).toBe(false);
    const reasonMutation = structuredClone(corpus.cases.find((caseData) => caseData.id === "GEN2-view-yield-rejected"));
    reasonMutation.expected.reason = "yield-dialogue";
    expect(evaluateGen2Case(reasonMutation).pass).toBe(false);
  });
});
