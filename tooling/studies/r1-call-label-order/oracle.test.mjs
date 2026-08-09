import { describe, expect, test } from "bun:test";

function fixedOrder(majorUnits, currency) {
  return { minorUnits: majorUnits * 100, currency };
}

function reordered(majorUnits, currency) {
  return { minorUnits: majorUnits * 100, currency };
}

describe("R1 call-label-order host oracle", () => {
  test("both label orders preserve the Money outcome", () => {
    const expected = { minorUnits: 4200, currency: "cr" };

    expect(fixedOrder(42, "cr")).toEqual(expected);
    expect(reordered(42, "cr")).toEqual(expected);
  });

  test("unordered policy reports collision while fixed shapes stay distinct", () => {
    const fixedShapes = [
      ["majorUnits:", "currency:"],
      ["majorUnits:"],
      ["currency:", "majorUnits:"],
    ];
    const unorderedKey = (shape) => [...shape].sort().join("|");
    const analyzeShapes = () => {
      const orderedDistinct = new Set(
        fixedShapes.map((shape) => shape.join("|")),
      ).size;
      const unorderedDistinct = new Set(fixedShapes.map(unorderedKey)).size;
      const collisionCount = fixedShapes.length - unorderedDistinct;

      return {
        orderedDistinct,
        unorderedDistinct,
        collisionCount,
        diagnostic: collisionCount > 0 ? "ambiguity-before-types" : null,
      };
    };

    expect(analyzeShapes()).toEqual({
      orderedDistinct: 3,
      unorderedDistinct: 2,
      collisionCount: 1,
      diagnostic: "ambiguity-before-types",
    });
  });
});
