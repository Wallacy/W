import { describe, expect, test } from "bun:test";

const rows = [
  [1, 2, 0, 31],
  [3, 4],
  [32, 5],
];
const expected = { bits: 0x0000_8864, finalizedRows: 1 };

function structuredControl(input) {
  let bits = 0;
  let finalizedRows = 0;

  assembleWord: {
    scanRows: for (const row of input) {
      for (const value of row) {
        if (value === 0) continue scanRows;
        if (value > 31) break assembleWord;
        bits = (bits << 5) | value;
      }

      finalizedRows += 1;
    }
  }

  return { bits, finalizedRows };
}

function flagControl(input) {
  let bits = 0;
  let finalizedRows = 0;
  let stopScan = false;

  for (const row of input) {
    let skipRow = false;

    for (const value of row) {
      if (value === 0) {
        skipRow = true;
        break;
      }

      if (value > 31) {
        stopScan = true;
        break;
      }

      bits = (bits << 5) | value;
    }

    if (stopScan) break;
    if (skipRow) continue;
    finalizedRows += 1;
  }

  return { bits, finalizedRows };
}

describe("R1 control-flow host oracle", () => {
  test("the structured and flag variants produce the fixed outcome", () => {
    expect(structuredControl(rows)).toEqual(expected);
    expect(flagControl(rows)).toEqual(expected);
  });

  test("zero skips row finalization and invalid input stops later rows", () => {
    const adversarial = [
      [7, 0, 9],
      [2],
      [32],
      [1],
    ];
    const adversarialExpected = { bits: 226, finalizedRows: 1 };

    expect(structuredControl(adversarial)).toEqual(adversarialExpected);
    expect(flagControl(adversarial)).toEqual(adversarialExpected);
  });
});
