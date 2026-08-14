import { describe, expect, test } from "bun:test";
import {
  assertNoRuntimeLifetimeMetadata,
  evaluateBorrowRelationCase,
} from "./brx2-borrow-relations-machine.mjs";

describe("BRX2 host reference", () => {
  test("maps a body-derived branch union and preserves edge occurrences", () => {
    const result = evaluateBorrowRelationCase({
      id: "reference-branch-union",
      declaration: {
        kind: "free",
        body: true,
        inputs: [
          { slot: "primary", mode: "ref" },
          { slot: "fallback", mode: "ref" },
        ],
        results: [{ slot: "result", mode: "view" }],
        bodyTrace: [
          { operation: "union", output: "selected", inputs: ["primary", "fallback"] },
          { operation: "return", result: "result", source: "selected" },
        ],
      },
    });
    expect(result.decision).toBe("accepted");
    expect(result.mapping.baseline).toEqual({ result: ["fallback", "primary"] });
    expect(result.mapping.baselineEdges).toHaveLength(2);
    expect(result.mapping.baselineOriginSets).toEqual(["fallback", "primary"]);
  });

  test("deduplicates OriginSet but keeps overlapping edge occurrences", () => {
    const result = evaluateBorrowRelationCase({
      id: "reference-overlap",
      declaration: {
        kind: "free",
        body: true,
        inputs: [{ slot: "primary", mode: "ref" }],
        results: [{ slot: "result", mode: "view" }],
        bodyTrace: [
          { operation: "union", output: "selected", inputs: ["primary", "primary"] },
          { operation: "return", result: "result", source: "selected" },
        ],
      },
    });
    expect(result.mapping.baseline).toEqual({ result: ["primary"] });
    expect(result.mapping.baselineEdges).toHaveLength(2);
    expect(result.mapping.baselineOriginSets).toEqual(["primary"]);
  });

  test("requires exact result slots, input modes, and owner authority", () => {
    const base = {
      kind: "free",
      body: false,
      inputs: [
        { slot: "primary", mode: "ref" },
        { slot: "fallback", mode: "inout" },
      ],
      results: [{ slot: "result", mode: "view" }],
      relationContract: {
        owner: "interface",
        sealed: true,
        pairs: [{ result: "result", sources: ["primary"] }],
      },
    };
    const assay = {
      kind: "independent-assay",
      problemTrace: [{ operation: "return", result: "result", source: "primary" }],
    };
    const good = evaluateBorrowRelationCase({ id: "reference-good", declaration: base, assay });
    expect(good.mapping.relationExact).toBe(true);
    expect(good.abi.baselineWAbiKey).toBe(good.abi.candidateWAbiKey);
    expect(good.abi.wAbiChanged).toBe(false);
    expect(good.abi.relationMetadataExcluded).toBe(true);
    const mode = evaluateBorrowRelationCase({
      id: "reference-mode",
      declaration: {
        ...base,
        relationContract: {
          ...base.relationContract,
          pairs: [{ result: "result", sources: ["fallback"], mode: "shared" }],
        },
      },
      assay,
    });
    expect(mode.decision).toBe("rejected");
    expect(mode.diagnostics.map((item) => item.code)).toContain("relationEdgeModeInvalid");
    const witness = evaluateBorrowRelationCase({
      id: "reference-witness",
      declaration: { ...base, relationContract: { ...base.relationContract, owner: "witness" } },
      assay,
    });
    expect(witness.decision).toBe("rejected");
    expect(witness.diagnostics.map((item) => item.code)).toContain("relationWitnessOnly");
    expect(assertNoRuntimeLifetimeMetadata(good)).toBe(true);
  });

  test("keeps no runtime carrier and rejects result recursion", () => {
    const result = evaluateBorrowRelationCase({
      id: "reference-recursion",
      declaration: {
        kind: "free",
        body: false,
        inputs: [{ slot: "primary", mode: "ref" }],
        results: [{ slot: "result", mode: "view" }],
        relationContract: {
          owner: "requirement",
          sealed: true,
          pairs: [{ result: "result", sources: ["result"] }],
        },
      },
    });
    expect(result.decision).toBe("rejected");
    expect(result.diagnostics.map((item) => item.code)).toContain("relationResultRecursion");
    expect(assertNoRuntimeLifetimeMetadata(result)).toBe(true);
  });

  test("rejects a top-level runtime lifetime table", () => {
    const result = evaluateBorrowRelationCase({
      id: "reference-runtime-table",
      runtimeLifetimeMetadata: [{ owner: "primary", result: "result" }],
      declaration: {
        kind: "free",
        body: true,
        inputs: [{ slot: "primary", mode: "ref" }],
        results: [{ slot: "result", mode: "view" }],
        bodyTrace: [{ operation: "return", result: "result", source: "primary" }],
      },
    });
    expect(result.decision).toBe("rejected");
    expect(result.diagnostics.map((item) => item.code)).toContain("runtimeLifetimeMetadataRejected");
    expect(assertNoRuntimeLifetimeMetadata(result)).toBe(true);
  });

  test("derives a changed candidate WAbi for an attempted runtime field", () => {
    const result = evaluateBorrowRelationCase({
      id: "reference-wabi-mutation",
      declaration: {
        kind: "free",
        body: true,
        inputs: [{ slot: "primary", mode: "ref" }],
        results: [{ slot: "result", mode: "view" }],
        bodyTrace: [{ operation: "return", result: "result", source: "primary" }],
      },
      interface: { runtimeFields: ["lifetime"] },
    });
    expect(result.abi.wAbiChanged).toBe(true);
    expect(result.abi.baselineWAbiKey).not.toBe(result.abi.candidateWAbiKey);
    expect(result.decision).toBe("rejected");
  });

  test("rejects legacy result and verification booleans", () => {
    const legacyResult = evaluateBorrowRelationCase({
      id: "reference-legacy-result-flag",
      declaration: {
        kind: "free",
        body: false,
        inputs: [{ slot: "value", mode: "value" }],
        results: [{ slot: "result", mode: "value" }],
        resultIndependent: true,
      },
    });
    expect(legacyResult.decision).toBe("rejected");
    expect(legacyResult.diagnostics.map((item) => item.code)).toContain("legacyResultDependencyFlagRejected");
    const legacyStatic = evaluateBorrowRelationCase({
      id: "reference-legacy-static-flag",
      declaration: {
        kind: "free",
        body: false,
        inputs: [{ slot: "value", mode: "value" }],
        results: [{ slot: "result", mode: "value" }],
        resultStatic: true,
      },
    });
    expect(legacyStatic.decision).toBe("rejected");
    expect(legacyStatic.diagnostics.map((item) => item.code)).toContain("legacyResultDependencyFlagRejected");
    const legacyVerification = evaluateBorrowRelationCase({
      id: "reference-legacy-verification-flag",
      declaration: {
        kind: "free",
        body: false,
        inputs: [{ slot: "primary", mode: "ref" }],
        results: [{ slot: "result", mode: "view" }],
      },
      artifacts: { verified: true },
    });
    expect(legacyVerification.decision).toBe("rejected");
    expect(legacyVerification.diagnostics.map((item) => item.code)).toContain("legacyVerificationFlagRejected");
  });

  test("derives route from declarations instead of a forged expected flag", () => {
    const result = evaluateBorrowRelationCase({
      id: "reference-forged-route",
      route: "current",
      decision: "accepted",
      declaration: {
        kind: "free",
        body: false,
        inputs: [{ slot: "primary", mode: "ref" }, { slot: "fallback", mode: "ref" }],
        results: [{ slot: "result", mode: "view" }],
      },
      assay: {
        kind: "independent-assay",
        problemTrace: [{ operation: "return", result: "result", source: "primary" }],
      },
    });
    expect(result.route).toBe("research");
    expect(result.decision).toBe("research-blocker");
  });
});
