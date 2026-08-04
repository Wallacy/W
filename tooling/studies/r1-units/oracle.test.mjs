import { describe, expect, test } from "bun:test";

function angleUnits(mass, height) {
  const gravity = 9.80665;
  return {
    energy: mass * gravity * height,
    fallTime: Math.sqrt((2 * height) / gravity),
  };
}

function squareUnits(mass, height) {
  const gravity = 9.80665;
  return {
    energy: mass * gravity * height,
    fallTime: Math.sqrt((2 * height) / gravity),
  };
}

describe("R1 unit-delimiter host oracle", () => {
  test("both notations preserve the fixed physical result", () => {
    const angle = angleUnits(2, 5);
    const square = squareUnits(2, 5);
    expect(angle.energy).toBeCloseTo(98.0665, 10);
    expect(angle.fallTime).toBeCloseTo(1.0098099885512761, 12);
    expect(square).toEqual(angle);
  });

  test("the notation does not change scaling", () => {
    expect(angleUnits(1250, 0.25)).toEqual(squareUnits(1250, 0.25));
  });
});
