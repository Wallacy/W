import { describe, expect, test } from "bun:test";
import {
  assertNoRuntimeLifetimeMetadata,
  evaluateBorrowCase,
} from "./borrow-expressivity-machine.mjs";

function bodyCase(overrides = {}) {
  return {
    id: "host",
    ...overrides,
    declaration: {
      kind: "free",
      body: true,
      inputs: [{ slot: "source", mode: "ref" }],
      results: [{ slot: "result", mode: "view" }],
      bodyTrace: [{ operation: "return", result: "result", source: "source" }],
      ...overrides.declaration,
    },
  };
}

describe("BRX0 borrow expressivity host oracle", () => {
  test("a callable creates a fresh loan that lasts through the borrowed result", () => {
    const result = evaluateBorrowCase(bodyCase({
      invocation: { kind: "callable", parameter: "source", calls: 3 },
    }));
    expect(result.invocation.status).toBe("accepted");
    expect(result.invocation.freshLoans).toHaveLength(3);
    expect(result.invocation.invocationEdges).toHaveLength(3);
    expect(result.invocation.persistentEdges).toHaveLength(0);
    expect(new Set(result.invocation.invocationEdges.map((edge) => edge.ownerSlot)).size).toBe(3);
    expect(result.invocation.resultEdgeLifetime).toBe("last-result-use");
    expect(result.invocation.freshLoans.every((loan) =>
      loan.createdAt === "invocation" && loan.scope === "result-use" &&
        loan.lifetime === "result-use" && loan.end === "last-result-use",
    )).toBe(true);
    expect(result.invocation.invocationEdges.every((edge) =>
      edge.lifetime === "result-use" && edge.end === "last-result-use",
    )).toBe(true);
  });

  test("a lending cursor blocks next only while a reused view is live", () => {
    const declaration = {
      kind: "instance",
      body: true,
      inputs: [{ slot: "receiver", mode: "mut ref" }],
      results: [{ slot: "result", mode: "view" }],
      bodyTrace: [{ operation: "return", result: "result", source: "receiver" }],
    };
    expect(evaluateBorrowCase({ id: "live", declaration, invocation: {
      kind: "stream-next", viewLive: true, reusesStorage: true,
    }}).invocation.code).toBe("W-BORROW-0006");
    expect(evaluateBorrowCase({ id: "ended", declaration, invocation: {
      kind: "stream-next", viewLive: false, reusesStorage: true,
    }}).invocation.status).toBe("accepted");
  });

  test("bodyless free requirements reject ambiguous origins and retain relation research", () => {
    const declaration = {
      kind: "free",
      body: false,
      inputs: [{ slot: "primary", mode: "ref" }, { slot: "fallback", mode: "ref" }],
      results: [{ slot: "result", mode: "view" }],
      problemTrace: [{ operation: "return", result: "result", source: "primary" }],
      relationSchema: { pairs: [{ result: "result", sources: ["primary"] }] },
    };
    const result = evaluateBorrowCase({ id: "select", declaration });
    expect(result.decision).toBe("research-blocker");
    expect(result.mapping.baseline).toEqual({});
    expect(result.mapping.baselineError.code).toBe("W-BORROW-0011");
    expect(result.mapping.relational.result).toEqual(["primary"]);
    expect(result.forms.A2_freeAllInputs).toBe("rejects-ambiguous-inputs");
    expect(result.forms.B1_relationalSchema).toBe("candidate-closes");
  });

  test("a bodyless declaration with one compatible input derives that origin", () => {
    const result = evaluateBorrowCase({
      id: "unique",
      declaration: {
        kind: "protocol",
        body: false,
        inputs: [{ slot: "primary", mode: "ref" }],
        results: [{ slot: "result", mode: "view" }],
      },
    });
    expect(result.decision).toBe("accepted");
    expect(result.mapping.baseline).toEqual({ result: ["primary"] });
    expect(result.mapping.baselineError).toBeNull();
  });

  test("every bodyless static, free, and protocol result rejects two compatible inputs", () => {
    for (const kind of ["static", "free", "protocol"]) {
      const result = evaluateBorrowCase({
        id: `ambiguous-${kind}`,
        declaration: {
          kind,
          body: false,
          inputs: [{ slot: "left", mode: "ref" }, { slot: "right", mode: "view" }],
          results: [{ slot: "result", mode: "view" }],
        },
      });
      expect(result.decision).toBe("rejected");
      expect(result.mapping.baselineError.code).toBe("W-BORROW-0011");
      expect(result.mapping.baselineError.facts.compatibleInputs).toEqual(["left", "right"]);
    }
  });

  test("bodyless receiver, init, and independent/static result rules stay explicit", () => {
    for (const kind of ["instance", "member"]) {
      const result = evaluateBorrowCase({
        id: `receiver-${kind}`,
        declaration: {
          kind,
          body: false,
          inputs: [
            { slot: "receiver", mode: "mut ref" },
            { slot: "other", mode: "ref" },
          ],
          results: [{ slot: "result", mode: "view" }],
        },
      });
      expect(result.decision).toBe("accepted");
      expect(result.mapping.baseline).toEqual({ result: ["receiver"] });
      expect(result.mapping.baselineError).toBeNull();
    }

    const init = evaluateBorrowCase({
      id: "init-borrowed-result",
      declaration: {
        kind: "init",
        body: false,
        inputs: [{ slot: "source", mode: "ref" }],
        results: [{ slot: "result", mode: "view" }],
      },
    });
    expect(init.decision).toBe("rejected");
    expect(init.mapping.baselineError.code).toBe("initBorrowResultUnsupported");

    for (const resultMode of ["value", "static"]) {
      const result = evaluateBorrowCase({
        id: `zero-${resultMode}`,
        declaration: {
          kind: "free",
          body: false,
          inputs: [],
          results: [{ slot: "result", mode: resultMode }],
        },
      });
      expect(result.decision).toBe("accepted");
      expect(result.mapping.baseline).toEqual({});
      expect(result.mapping.baselineError).toBeNull();
    }

    const zeroBorrowed = evaluateBorrowCase({
      id: "zero-borrowed-result",
      declaration: {
        kind: "free",
        body: false,
        inputs: [],
        results: [{ slot: "result", mode: "view" }],
      },
    });
    expect(zeroBorrowed.decision).toBe("rejected");
    expect(zeroBorrowed.mapping.baselineError.code).toBe("interfaceOriginUnknown");
  });

  test("an owned nominal result remains an explicit API alternative", () => {
    const result = evaluateBorrowCase({
      id: "owned",
      declaration: {
        kind: "free",
        body: false,
        inputs: [{ slot: "primary", mode: "ref" }, { slot: "fallback", mode: "ref" }],
        results: [{ slot: "result", mode: "value" }],
        behavior: { returnShape: "nominal-owned" },
      },
    });
    expect(result.decision).toBe("accepted");
    expect(result.forms.B2_returnAggregate).toBe("owned-nominal-alternative");
  });

  test("map, filter, and chain compose OriginSet transitively", () => {
    const constructed = evaluateBorrowCase({
      id: "construct",
      declaration: {
        kind: "free",
        body: true,
        inputs: [{ slot: "source", mode: "take" }],
        results: [{ slot: "result", mode: "value" }],
        bodyTrace: [],
      },
    });
    expect(constructed.mapping.baseline).toEqual({});
    expect(constructed.mapping.baselineEdges).toHaveLength(0);

    const result = evaluateBorrowCase({
      id: "next",
      declaration: {
        kind: "instance",
        body: true,
        inputs: [{ slot: "receiver", mode: "mut ref", origin: "receiver.storage" }],
        results: [{ slot: "result", mode: "view" }],
        bodyTrace: [
          { operation: "adapter", adapter: "map", input: "receiver", output: "mapped" },
          { operation: "adapter", adapter: "filter", input: "mapped", output: "filtered" },
          { operation: "return", result: "result", source: "filtered" },
        ],
      },
    });
    expect(result.mapping.baseline.result).toEqual(["receiver"]);
    expect(result.mapping.baselineOriginSets.result).toEqual(["receiver"]);
    expect(result.mapping.baselineEdges).toHaveLength(1);
    expect(result.mapping.baselineEdges[0].origin).toBe("receiver.storage");
    expect(assertNoRuntimeLifetimeMetadata(result)).toBe(true);
  });

  test("a separate union preserves individual edges while OriginSet deduplicates", () => {
    const result = evaluateBorrowCase(bodyCase({
      declaration: {
        bodyTrace: [
          { operation: "adapter", adapter: "map", input: "source", output: "mapped" },
          { operation: "union", inputs: ["mapped", "source"], output: "joined" },
          { operation: "return", result: "result", source: "joined" },
        ],
      },
    }));
    expect(result.mapping.baseline.result).toEqual(["source"]);
    expect(result.mapping.baselineOriginSets.result).toEqual(["source"]);
    expect(result.mapping.baselineEdges).toHaveLength(2);
  });

  test("await, closure storage, and dynamic boundary preserve existing diagnostics", () => {
    const declaration = bodyCase().declaration;
    expect(evaluateBorrowCase({ id: "await", declaration, invocation: {
      await: true, ownerStable: false, storageStable: true,
    }}).invocation.code).toBe("W-BORROW-0007");
    expect(evaluateBorrowCase({ id: "closure", declaration, invocation: {
      closureStorage: { storage: "heap", escape: true },
    }}).invocation.code).toBe("W-BORROW-0003");
    expect(evaluateBorrowCase({ id: "channel", declaration, invocation: {
      boundary: "channel",
    }}).invocation.code).toBe("W-BORROW-0003");
  });

  test("any fn erasure retains mapping and does not add runtime lifetime metadata", () => {
    const result = evaluateBorrowCase(bodyCase({
      invocation: { erasure: "any-fn", closureStorage: { storage: "inline", escape: false } },
    }));
    expect(result.invocation.status).toBe("accepted");
    expect(result.invocation.erasure.mapping.result).toEqual(["source"]);
    expect(result.invocation.erasure.mappingComponentDigest).toMatch(/^sha256:[0-9a-f]{64}$/u);
    expect(assertNoRuntimeLifetimeMetadata(result)).toBe(true);
  });

  test("relation and interface artifacts reject missing, duplicate, forged, stale, and divergent mappings", () => {
    const declaration = {
      kind: "free",
      body: false,
      inputs: [{ slot: "primary", mode: "ref" }, { slot: "fallback", mode: "ref" }],
      results: [{ slot: "result", mode: "view" }],
      problemTrace: [{ operation: "return", result: "result", source: "primary" }],
    };
    const artifact = (extra) => evaluateBorrowCase({
      id: "artifact",
      declaration,
      artifacts: { implementationMapping: { result: ["primary"] }, ...extra },
    }).artifacts;
    expect(artifact({ mappingPairs: [] }).diagnostics[0].code).toBe("researchRelationMissing");
    expect(artifact({ mappingPairs: [{ result: "result", sources: ["primary", "primary"] }] }).diagnostics[0].code).toBe("researchRelationDuplicate");
    expect(artifact({ mappingPairs: [{ result: "result", sources: ["ghost"] }] }).diagnostics[0].code).toBe("researchRelationForged");
    expect(artifact({ witnessMapping: { result: ["fallback"] } }).diagnostics[0].code).toBe("interfaceWitnessMismatch");
    expect(artifact({ mappingComponentDigest: "sha256:0000000000000000000000000000000000000000000000000000000000000000" }).diagnostics[0].code).toBe("interfaceLockMismatch");
  });
});
