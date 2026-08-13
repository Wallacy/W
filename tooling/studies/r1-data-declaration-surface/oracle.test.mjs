import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const cases = bundle.inputs.map(({ expected, ...input }) => ({ input, expected }));

function validate(input) {
  if (typeof input.operation !== "string") throw new Error("operation");
  if (typeof input.code !== "string") throw new Error("code");
  if (!Number.isSafeInteger(input.price) || input.price < 0) throw new Error("price");
  if (!Number.isSafeInteger(input.reserved) || input.reserved < 0) throw new Error("reserved");
}

function run(operation, input) {
  validate(input);
  if (operation === "transparent-and-encapsulated" || operation === "field-by-field-exports") {
    return {
      accepted: true,
      item: { code: input.code, price: input.price },
      storageVisible: false,
      released: 0,
    };
  }
  if (operation === "public-storage") {
    return { accepted: false, cause: "public-storage" };
  }
  throw new Error("unknown operation");
}

describe("R1 data-declaration-surface host oracle", () => {
  for (const { input, expected } of cases) {
    test(`derives exact outcome for ${input.id}`, () => {
      expect(run(input.operation, input)).toEqual(expected);
    });
  }

  test("malformed declaration data fails before a result", () => {
    expect(() => run("transparent-and-encapsulated", {
      operation: "transparent-and-encapsulated",
      code: "nebula",
      price: -1,
      reserved: 0,
    })).toThrow("price");
  });
});
