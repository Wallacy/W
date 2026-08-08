import { describe, expect, test } from "bun:test";
import fs from "node:fs";

const bundle = JSON.parse(
  fs.readFileSync(new URL("bundle.json", import.meta.url), "utf8"),
);

const transitions = new Map([
  ["accepted", new Set(["reserving", "cancelled"])],
  ["reserving", new Set(["preparing", "cancelled"])],
  ["preparing", new Set(["serving", "cancelled"])],
  ["serving", new Set(["completed", "cancelled"])],
  ["completed", new Set()],
  ["cancelled", new Set()],
]);

function analyzePath(stages) {
  if (stages.length === 0) {
    return { valid: false, firstInvalidIndex: 0, terminal: null };
  }

  for (let index = 1; index < stages.length; index += 1) {
    if (!transitions.get(stages[index - 1])?.has(stages[index])) {
      return {
        valid: false,
        firstInvalidIndex: index,
        terminal: stages.at(-1),
      };
    }
  }

  return { valid: true, firstInvalidIndex: null, terminal: stages.at(-1) };
}

function sequentialContract(stages) {
  return analyzePath(stages);
}

function fusedContract(stages) {
  return analyzePath(stages);
}

describe("R1 sequential-contract host oracle", () => {
  test("both notations preserve a valid stage path", () => {
    const input = bundle.inputs.find(({ id }) => id === "standard-service");

    expect(sequentialContract(input.stages)).toEqual(input.expected);
    expect(fusedContract(input.stages)).toEqual(input.expected);
  });

  test("both notations expose the same skipped transition", () => {
    const input = bundle.inputs.find(({ id }) => id === "skipped-reservation");

    expect(sequentialContract(input.stages)).toEqual(input.expected);
    expect(fusedContract(input.stages)).toEqual(input.expected);
  });
});
