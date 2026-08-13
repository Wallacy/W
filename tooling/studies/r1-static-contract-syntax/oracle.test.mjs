import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const cases = bundle.inputs.map(({ expected, ...input }) => ({ input, expected }));

function validate(input) {
  if (typeof input.operation !== "string") throw new Error("operation");
  if (!Number.isSafeInteger(input.rows) || input.rows < 0) throw new Error("rows");
  if (!Number.isSafeInteger(input.columns) || input.columns < 0) throw new Error("columns");
  if (typeof input.dish !== "string") throw new Error("dish");
}

function run(operation, input) {
  validate(input);
  if (operation === "attached-contract") {
    return { accepted: true, area: input.rows * input.columns, nested: true, localContract: true };
  }
  if (operation === "spaced-close") {
    return { accepted: true, area: input.rows * input.columns, nested: true, close: "spaced" };
  }
  if (operation === "manifest-comparison") {
    if (typeof input.manifestName !== "string" || typeof input.manifestVersion !== "string") throw new Error("manifest");
    return { accepted: true, manifest: input.manifestName, version: input.manifestVersion, localContract: true };
  }
  if (operation === "detached-envelope") {
    return { accepted: false, cause: "detached-envelope" };
  }
  if (operation === "angular-record") {
    return { accepted: false, cause: "angular-record" };
  }
  throw new Error("unknown operation");
}

describe("R1 static-contract-syntax host oracle", () => {
  for (const { input, expected } of cases) {
    test(`derives exact outcome for ${input.id}`, () => {
      expect(run(input.operation, input)).toEqual(expected);
    });
  }

  test("malformed shape data fails before contract evaluation", () => {
    expect(() => run("attached-contract", { operation: "attached-contract", rows: -1, columns: 1, dish: "x" })).toThrow("rows");
  });
});
