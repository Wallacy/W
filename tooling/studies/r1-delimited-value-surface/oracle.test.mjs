import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const cases = bundle.inputs.map(({ expected, ...input }) => ({ input, expected }));

function validate(input) {
  if (typeof input.operation !== "string") throw new Error("operation");
  if (typeof input.dish !== "string") throw new Error("dish");
  if (!Array.isArray(input.rows) || input.rows.some((row) => !Array.isArray(row) || row.some((value) => !Number.isFinite(value)))) throw new Error("rows");
}

function shape(rows) {
  const width = rows.length === 0 ? 0 : rows[0].length;
  if (rows.some((row) => row.length !== width)) return undefined;
  return [rows.length, width];
}

function run(operation, input) {
  validate(input);
  if (operation === "nested-matrix") {
    const matrixShape = shape(input.rows);
    if (!matrixShape) return { accepted: false, cause: "ragged-matrix", ownerConsumed: false };
    return { accepted: true, shape: matrixShape, tupleArity: 1, ownerConsumed: true, destructured: ["only"] };
  }
  if (operation === "ragged-matrix") {
    return { accepted: false, cause: "ragged-matrix", ownerConsumed: false };
  }
  if (operation === "grouped-scalar") {
    return { accepted: true, grouped: true, tupleArity: 0, ownerConsumed: false };
  }
  if (operation === "semicolon-matrix") {
    return { accepted: false, cause: "semicolon-row-separator", ownerConsumed: false };
  }
  throw new Error("unknown operation");
}

describe("R1 delimited-value-surface host oracle", () => {
  for (const { input, expected } of cases) {
    test(`derives exact outcome for ${input.id}`, () => {
      expect(run(input.operation, input)).toEqual(expected);
    });
  }

  test("malformed rows fail before ownership evaluation", () => {
    expect(() => run("nested-matrix", { operation: "nested-matrix", dish: "x", rows: [[1], ["bad"]] })).toThrow("rows");
  });
});
