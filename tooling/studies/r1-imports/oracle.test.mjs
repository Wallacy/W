import { describe, expect, test } from "bun:test";

function decode(input) {
  const normalized = input.trim().toLowerCase();
  return ["cli", "tui", "serve"].includes(normalized) ? normalized : "unknown";
}

function flattened(input) {
  return decode(input);
}

function qualified(input) {
  return decode(input);
}

describe("R1 import host oracle", () => {
  test("both import policies preserve command decoding", () => {
    for (const input of [" TUI ", "serve", "CLI", "orbit"]) {
      expect(flattened(input)).toBe(qualified(input));
    }
  });

  test("unknown input remains explicit", () => {
    expect(flattened("last-light")).toBe("unknown");
    expect(qualified("last-light")).toBe("unknown");
  });
});
