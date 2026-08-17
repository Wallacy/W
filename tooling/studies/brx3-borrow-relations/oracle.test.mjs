import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { buildBRX3Snapshot, evaluateBRX3Case, validateBRX3Corpus } from "../../brx3-borrow-relations-machine.mjs";

const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "../../brx3-borrow-relations-cases.json"), "utf8"));

describe("BRX3 source-clause borrow relation oracle", () => {
  test("promotes only the explicit source clause contract", () => {
    const checked = validateBRX3Corpus(corpus);
    expect(checked.errors).toEqual([]);
    expect(checked.metrics.caseCount).toBe(32);
    expect(checked.metrics.accepted).toBeGreaterThanOrEqual(8);
    expect(checked.metrics.rejected).toBeGreaterThanOrEqual(8);
  });

  test("keeps declaration authority separate from invocation boundaries", () => {
    const stream = evaluateBRX3Case(corpus.cases.find((item) => item.id === "BRX3-stream-live"));
    expect(stream.status).toBe("accepted");
    expect(stream.invocationStatus).toBe("rejected");
    expect(stream.diagnosticCodes).toContain("W-BORROW-0006");
    const ffi = evaluateBRX3Case(corpus.cases.find((item) => item.id === "BRX3-foreign-boundary"));
    expect(ffi.status).toBe("accepted");
    expect(ffi.invocationStatus).toBe("rejected");
    expect(ffi.diagnosticCodes).toContain("W-BORROW-0003");
  });

  test("canonical lowering uses ordinals while syntax order remains observable", () => {
    const result = evaluateBRX3Case(corpus.cases.find((item) => item.id === "BRX3-source-order-canonical"));
    expect(result.canonicalRelation.pairs[0].sources).toEqual([0, 1]);
    expect(result.mapping.effective.result).toEqual(["fallback", "primary"]);
    expect(buildBRX3Snapshot(corpus).text).toBe(buildBRX3Snapshot(corpus).text);
  });

  test("mutation of a source slot cannot pass", () => {
    const mutated = structuredClone(corpus.cases.find((item) => item.id === "BRX3-protocol-union"));
    mutated.borrowClause.pairs[0].sources[0] = "ghost";
    const result = evaluateBRX3Case(mutated);
    expect(result.status).toBe("rejected");
    expect(result.diagnosticCodes).toContain("borrowSourceSlotUnknown");
  });
});
