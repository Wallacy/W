import { describe, expect, test } from "bun:test";

function failFast(input) {
  const failures = [
    input.portFails ? { side: "port", tick: input.portDelay } : null,
    input.starboardFails ? { side: "starboard", tick: input.starboardDelay } : null,
  ]
    .filter(Boolean)
    .sort((left, right) => left.tick - right.tick || left.side.localeCompare(right.side));
  if (failures.length === 0) {
    return { outcome: "success", observedAt: Math.max(input.portDelay, input.starboardDelay) };
  }
  return { outcome: failures[0].side, observedAt: failures[0].tick };
}

function strictLexical(input) {
  if (input.portFails) return { outcome: "port", observedAt: input.portDelay };
  if (input.starboardFails) {
    return {
      outcome: "starboard",
      observedAt: Math.max(input.portDelay, input.starboardDelay),
    };
  }
  return { outcome: "success", observedAt: Math.max(input.portDelay, input.starboardDelay) };
}

describe("R1 fail-fast host oracle", () => {
  test("a later lexical child failure keeps the same error but changes observation", () => {
    const input = { portDelay: 8, portFails: false, starboardDelay: 2, starboardFails: true };
    expect(failFast(input)).toEqual({ outcome: "starboard", observedAt: 2 });
    expect(strictLexical(input)).toEqual({ outcome: "starboard", observedAt: 8 });
  });

  test("an earlier lexical child failure agrees in outcome and observation", () => {
    const input = { portDelay: 3, portFails: true, starboardDelay: 9, starboardFails: false };
    expect(failFast(input)).toEqual({ outcome: "port", observedAt: 3 });
    expect(strictLexical(input)).toEqual({ outcome: "port", observedAt: 3 });
  });
});
