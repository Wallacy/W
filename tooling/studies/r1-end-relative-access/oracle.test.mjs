import { describe, expect, test } from "bun:test";

function last(items) {
  return items.at(-1) ?? null;
}

function countMinusOne(items) {
  if (items.length === 0) return null;
  return items[items.length - 1] ?? null;
}

function negativeIndex(items) {
  if (items.length === 0) return null;
  return items.at(-1) ?? null;
}

describe("R1 end-relative-access host oracle", () => {
  test("all variants return the same nonempty String? outcome", () => {
    const menu = ["nebula broth", "horizon cake"];

    expect(last(menu)).toBe("horizon cake");
    expect(countMinusOne(menu)).toBe("horizon cake");
    expect(negativeIndex(menu)).toBe("horizon cake");
  });

  test("all variants preserve an empty optional outcome", () => {
    const empty = [];

    expect(last(empty)).toBeNull();
    expect(countMinusOne(empty)).toBeNull();
    expect(negativeIndex(empty)).toBeNull();
  });
});
