import { describe, expect, test } from "bun:test";
import { evaluateBorrowCase } from "../../borrow-expressivity-machine.mjs";

function selectDeclaration(extra = {}) {
  return {
    kind: "free",
    body: false,
    inputs: [{ slot: "primary", mode: "ref" }, { slot: "fallback", mode: "ref" }],
    results: [{ slot: "result", mode: "view" }],
    problemTrace: [{ operation: "return", result: "result", source: "primary" }],
    ...extra,
  };
}

describe("R1 BRX0 higher-order borrow expressivity", () => {
  test("the selected dependency-slot contract is exact for a member receiver", () => {
    const result = evaluateBorrowCase({
      id: "receiver",
      declaration: {
        kind: "instance",
        body: false,
        inputs: [{ slot: "receiver", mode: "ref" }, { slot: "fallback", mode: "ref" }],
        results: [{ slot: "result", mode: "view" }],
        problemTrace: [{ operation: "return", result: "result", source: "receiver" }],
      },
    });
    expect(result.decision).toBe("accepted");
    expect(result.forms.A1_memberReceiver).toBe("closes");
  });

  test("the free requirement remains a Research blocker under current all-inputs default", () => {
    const result = evaluateBorrowCase({ id: "free", declaration: selectDeclaration() });
    expect(result.decision).toBe("research-blocker");
    expect(result.mapping.baseline.result).toEqual(["fallback", "primary"]);
    expect(result.mapping.required.result).toEqual(["primary"]);
    expect(result.forms.A2_freeAllInputs).toBe("conservative-all-inputs");
  });

  test("the relation candidate closes the problem without changing the selected source form", () => {
    const result = evaluateBorrowCase({
      id: "relation",
      declaration: selectDeclaration({
        relationSchema: { pairs: [{ result: "result", sources: ["primary"] }] },
      }),
    });
    expect(result.mapping.relationalExact).toBe(true);
    expect(result.forms.B1_relationalSchema).toBe("candidate-closes");
    expect(result.decision).toBe("research-blocker");
  });

  test("the nominal aggregate is an API alternative, not a direct borrowed result", () => {
    const result = evaluateBorrowCase({
      id: "aggregate",
      declaration: selectDeclaration({ behavior: { returnShape: "sum" } }),
    });
    expect(result.forms.B2_returnAggregate).toBe("api-change");
  });
});
