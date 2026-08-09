import { describe, expect, test } from "bun:test";

function explicitBroadcast(calibrated, means, targetShape) {
  if (
    targetShape[0] !== calibrated.length ||
    calibrated.some((row) => row.length !== targetShape[1]) ||
    targetShape[1] !== means.length
  ) {
    return { status: "shapeMismatch" };
  }

  return {
    status: "centered",
    values: calibrated.map((row) => row.map((value, index) => value - means[index])),
  };
}

function checkedImplicit(calibrated, means) {
  if (
    calibrated.length === 0 ||
    calibrated.some((row) => row.length !== means.length)
  ) {
    return { status: "shapeMismatch" };
  }

  return {
    status: "centered",
    values: calibrated.map((row) => row.map((value, index) => value - means[index])),
  };
}

describe("R1 tensor-broadcast host oracle", () => {
  test("explicit and checked implicit forms preserve a valid shape", () => {
    const calibrated = [
      [2, 4, 6, 8, 10, 12],
      [14, 16, 18, 20, 22, 24],
      [26, 28, 30, 32, 34, 36],
    ];
    const means = [1, 2, 3, 4, 5, 6];
    const expected = {
      status: "centered",
      values: [
        [1, 2, 3, 4, 5, 6],
        [13, 14, 15, 16, 17, 18],
        [25, 26, 27, 28, 29, 30],
      ],
    };

    expect(explicitBroadcast(calibrated, means, [3, 6])).toEqual(expected);
    expect(checkedImplicit(calibrated, means)).toEqual(expected);
  });

  test("mismatched shape and changed axis remain explicit failures", () => {
    const calibrated = [
      [2, 4, 6, 8, 10, 12],
      [14, 16, 18, 20, 22, 24],
      [26, 28, 30, 32, 34, 36],
    ];
    const mismatchedMeans = [1, 2, 3, 4, 5];

    expect(explicitBroadcast(calibrated, mismatchedMeans, [3, 6])).toEqual({
      status: "shapeMismatch",
    });
    expect(checkedImplicit(calibrated, mismatchedMeans)).toEqual({
      status: "shapeMismatch",
    });
    const changedAxisMeans = [1, 2, 3];
    expect(explicitBroadcast(calibrated, changedAxisMeans, [3, 6])).toEqual({
      status: "shapeMismatch",
    });
    expect(checkedImplicit(calibrated, changedAxisMeans)).toEqual({
      status: "shapeMismatch",
    });
  });
});
