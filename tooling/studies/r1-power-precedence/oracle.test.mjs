import { describe, expect, test } from "bun:test";

function mathematical() {
  return {
    negativeBase: -(2 ** 2),
    negativeExponent: 2 ** -3,
    associated: 2 ** (3 ** 2),
    xor: 5 ^ 3,
  };
}

function parenthesized() {
  return {
    negativeBase: -(2 ** 2),
    negativeExponent: 2 ** (-3),
    associated: 2 ** (3 ** 2),
    xor: 5 ^ 3,
  };
}

function explicitParentheses() {
  return {
    negativeBase: (-2) ** 2,
    leftAssociated: (2 ** 3) ** 2,
  };
}

function classifyUnitExpression(tokens) {
  const open = tokens.indexOf("<");
  const close = tokens.lastIndexOf(">");
  const body = tokens.slice(open + 1, close);
  return {
    owner: open >= 0 && close > open ? "unit" : "runtime",
    runtimePower: body.includes("**"),
    unitCaret: body.includes("^"),
    unitBody: body.join(""),
  };
}

describe("R1 power-precedence host oracle", () => {
  test("mathematical precedence handles negative base, exponent, and association", () => {
    expect(mathematical()).toEqual({ negativeBase: -4, negativeExponent: 0.125, associated: 512, xor: 6 });
    expect(parenthesized()).toEqual(mathematical());
    expect(explicitParentheses()).toEqual({ negativeBase: 4, leftAssociated: 64 });
  });

  test("caret remains XOR in runtime expressions", () => {
    expect(5 ^ 3).toBe(6);
  });

  test("unit grammar owns its caret separately from runtime power", () => {
    const classified = classifyUnitExpression(["9.81", "<", "m", "/", "s", "^", "2", ">"]);
    expect(classified.owner).toBe("unit");
    expect(classified.runtimePower).toBe(false);
    expect(classified.unitCaret).toBe(true);
    expect(classified.unitBody).toBe("m/s^2");
  });
});
