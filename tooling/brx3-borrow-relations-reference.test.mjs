import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { evaluateBRX3Case, validateBRX3Corpus } from "./brx3-borrow-relations-machine.mjs";

const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "brx3-borrow-relations-cases.json"), "utf8"));

describe("BRX3 source clause reference", () => {
  test("uses requirement/interface authority and keeps the caller out", () => {
    const selected = evaluateBRX3Case(corpus.cases.find((item) => item.id === "BRX3-protocol-union"));
    expect(selected.status).toBe("accepted");
    expect(selected.relationAccepted).toBe(true);
    const caller = evaluateBRX3Case(corpus.cases.find((item) => item.id === "BRX3-caller-claim"));
    expect(caller.status).toBe("rejected");
    expect(caller.diagnosticCodes).toContain("callerRelationClaimRejected");
  });

  test("keeps missing and malformed clauses diagnostic", () => {
    const missing = evaluateBRX3Case(corpus.cases.find((item) => item.id === "BRX3-missing-clause"));
    expect(missing.diagnosticCodes).toEqual(["W-BORROW-0011"]);
    const duplicate = evaluateBRX3Case(corpus.cases.find((item) => item.id === "BRX3-duplicate-source"));
    expect(duplicate.diagnosticCodes).toContain("borrowSourceDuplicate");
  });

  test("corpus validation is deterministic", () => {
    const first = validateBRX3Corpus(corpus);
    const second = validateBRX3Corpus(corpus);
    expect(first.errors).toEqual([]);
    expect(first.metrics).toEqual(second.metrics);
  });
});
