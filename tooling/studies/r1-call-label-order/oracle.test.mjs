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

  test("label permutations share one call shape and collide as overloads", () => {
    const declarations = [
      ["majorUnits:", "currency:"],
      ["currency:", "majorUnits:"],
    ];
    const canonicalKey = (shape) => [...shape].sort().join("&");
    const analyzeShapes = () => {
      const canonicalShapes = declarations.map(canonicalKey);
      const distinctShapes = new Set(canonicalShapes).size;
      const collisionCount = declarations.length - distinctShapes;

      return {
        declarationCount: declarations.length,
        distinctShapes,
        collisionCount,
        diagnostic: collisionCount > 0 ? "same-label-set-order-collision" : null,
      };
    };

    expect(analyzeShapes()).toEqual({
      declarationCount: 2,
      distinctShapes: 1,
      collisionCount: 1,
      diagnostic: "same-label-set-order-collision",
    });
  });
});
