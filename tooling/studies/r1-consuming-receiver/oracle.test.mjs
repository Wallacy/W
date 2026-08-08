import { describe, expect, test } from "bun:test";
import fs from "node:fs";

const bundle = JSON.parse(
  fs.readFileSync(new URL("bundle.json", import.meta.url), "utf8"),
);

function finishCommandStream(tail) {
  const owner = { available: true };
  owner.available = false;

  if (tail === "invalid") {
    return {
      kind: "failed",
      error: "invalidTail",
      ownerAvailable: owner.available,
    };
  }

  return {
    kind: "completed",
    bytes: new TextEncoder().encode(tail).length,
    ownerAvailable: owner.available,
  };
}

function explicitReceiver(tail) {
  return finishCommandStream(tail);
}

function implicitReceiver(tail) {
  return finishCommandStream(tail);
}

describe("R1 consuming-receiver host oracle", () => {
  for (const input of bundle.inputs) {
    test(`${input.id} preserves outcome and consumes the owner`, () => {
      expect(explicitReceiver(input.tail)).toEqual(input.expected);
      expect(implicitReceiver(input.tail)).toEqual(input.expected);
    });
  }
});
