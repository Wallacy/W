import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const cases = bundle.inputs.map(({ expected, ...input }) => ({ input, expected }));

function validate(input) {
  if (typeof input.operation !== "string") throw new Error("operation");
  if (typeof input.accumulatedError !== "number" || typeof input.previousError !== "number") throw new Error("error");
  if (!Number.isInteger(input.status) || !Number.isInteger(input.arrival) || !Number.isInteger(input.gate)) throw new Error("integer");
}

function run(operation, input) {
  validate(input);
  if (operation === "selected-property" || operation === "mandatory-method") {
    return {
      kind: "value",
      idle: input.accumulatedError === 0 && input.previousError === 0,
      effects: false,
    };
  }
  if (operation === "fn-c" || operation === "named-language-slot") {
    return { kind: "value", probe: input.status };
  }
  if (operation === "closure-arrow" || operation === "anonymous-fn") {
    return { kind: "value", welcome: input.arrival + input.gate, capture: true };
  }
  if (operation === "effectful-property") {
    return { kind: "rejected", cause: "effectful-property" };
  }
  throw new Error("unknown operation");
}

describe("R1 callable-property-surface host oracle", () => {
  for (const { input, expected } of cases) {
    test(`derives exact outcome for ${input.id}`, () => {
      expect(run(input.operation, input)).toEqual(expected);
    });
  }

  test("malformed callable data fails before syntax comparison", () => {
    expect(() => run("selected-property", { operation: "selected-property", accumulatedError: 0, previousError: 0, status: 0, arrival: 1.5, gate: 2 })).toThrow("integer");
  });
});
