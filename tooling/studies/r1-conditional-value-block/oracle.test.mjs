import { describe, expect, test } from "bun:test";

function chooseStage(ready, effects) {
  if (ready) {
    effects.push(1);
    return "serving";
  }
  effects.push(2);
  return "preparing";
}

function unitStatement(ready, effects) {
  if (ready) effects.push("unit-effect");
  return undefined;
}

function valueBlock(items) {
  const valueOf = (item) => item.value ?? item.effect;
  const unterminated = items.filter(
    (item) => item.kind === "expression" && !item.terminated,
  );
  if (unterminated.length > 1 || (unterminated.length === 1 && items.at(-1) !== unterminated[0])) {
    return { accepted: false, reason: "unterminated-expression-before-boundary" };
  }
  const tail = items.at(-1);
  if (tail?.kind === "expression" && !tail.terminated) {
    return {
      accepted: true,
      value: tail.value,
      discarded: items
        .slice(0, -1)
        .map(valueOf)
        .filter((value) => value !== undefined),
    };
  }
  return {
    accepted: true,
    value: undefined,
    discarded: items
      .map(valueOf)
      .filter((value) => value !== undefined),
  };
}

function validateConditional({ hasElse, branchTypes, context }) {
  if (context === "unit" && !hasElse) return { accepted: true, type: "Unit" };
  if (!hasElse) return { accepted: false, reason: "missing-else" };
  if (new Set(branchTypes).size !== 1) return { accepted: false, reason: "branch-join" };
  return { accepted: true, type: branchTypes[0] };
}

function validateFunctionBody(items, declaredType) {
  const tail = items.at(-1);
  if (tail?.kind === "return") {
    return { accepted: true, mode: "explicit-return" };
  }
  if (declaredType !== "Unit") {
    return {
      accepted: false,
      reason: tail?.kind === "expression" && !tail.terminated
        ? "implicit-function-tail"
        : "explicit-return-required",
    };
  }
  return { accepted: true, mode: "unit" };
}

describe("R1 conditional value-block host oracle", () => {
  test("only the selected branch effect runs", () => {
    const trueEffects = [];
    const falseEffects = [];
    expect(chooseStage(true, trueEffects)).toBe("serving");
    expect(trueEffects).toEqual([1]);
    expect(chooseStage(false, falseEffects)).toBe("preparing");
    expect(falseEffects).toEqual([2]);
  });

  test("a statement if without else produces Unit", () => {
    const effects = [];
    expect(unitStatement(false, effects)).toBeUndefined();
    expect(effects).toEqual([]);
    expect(unitStatement(true, effects)).toBeUndefined();
    expect(effects).toEqual(["unit-effect"]);
    expect(validateConditional({ hasElse: false, branchTypes: [], context: "unit" })).toEqual({
      accepted: true,
      type: "Unit",
    });
  });

  test("non-Unit conditionals require else and a single branch type", () => {
    expect(validateConditional({ hasElse: false, branchTypes: ["Stage"], context: "value" })).toEqual({
      accepted: false,
      reason: "missing-else",
    });
    expect(validateConditional({ hasElse: true, branchTypes: ["Stage", "Unit"], context: "value" })).toEqual({
      accepted: false,
      reason: "branch-join",
    });
    expect(validateFunctionBody([{ kind: "return", value: "Stage" }], "Stage").mode).toBe("explicit-return");
    expect(validateFunctionBody([{ kind: "expression", value: "Stage", terminated: false }], "Stage")).toEqual({
      accepted: false,
      reason: "implicit-function-tail",
    });
    expect(validateFunctionBody([{ kind: "expression", value: "Stage", terminated: true }], "Stage")).toEqual({
      accepted: false,
      reason: "explicit-return-required",
    });
  });

  test("only the last unterminated expression yields and semicolons discard", () => {
    const selected = valueBlock([
      { kind: "statement", effect: "trace", terminated: true },
      { kind: "expression", value: "accepted", terminated: false },
    ]);
    expect(selected.value).toBe("accepted");
    expect(selected.discarded).toEqual(["trace"]);

    const discarded = valueBlock([
      { kind: "expression", value: "trace", terminated: true },
    ]);
    expect(discarded.value).toBeUndefined();
    expect(discarded.discarded).toEqual(["trace"]);

    expect(valueBlock([
      { kind: "expression", value: "trace", terminated: false },
      { kind: "expression", value: "accepted", terminated: false },
    ])).toEqual({ accepted: false, reason: "unterminated-expression-before-boundary" });
  });
});
