import { describe, expect, test } from "bun:test";

function bindingWord(tokens) {
  if (tokens.length === 0) return { error: "unexpectedEnd", wordCalls: 1, copyCount: 0 };
  const [text, foundLine] = [tokens[0], 7];
  return { text, line: foundLine, wordCalls: 1, copyCount: 0 };
}

function projectionWord(tokens) {
  if (tokens.length === 0) return { error: "unexpectedEnd", wordCalls: 1, copyCount: 0 };
  const result = [tokens[0], 7];
  const text = result[0];
  const foundLine = result[1];
  return { text, line: foundLine, wordCalls: 1, copyCount: 1 };
}

describe("R1 tuple-unpacking host oracle", () => {
  test("binding and projections preserve one successful word result", () => {
    const tokens = ["horizon", "cake"];
    const expected = { text: "horizon", line: 7, wordCalls: 1 };

    expect(bindingWord(tokens)).toEqual({ ...expected, copyCount: 0 });
    expect(projectionWord(tokens)).toEqual({ ...expected, copyCount: 1 });
  });

  test("empty input preserves one failure observation", () => {
    const expected = { error: "unexpectedEnd", wordCalls: 1, copyCount: 0 };

    expect(bindingWord([])).toEqual(expected);
    expect(projectionWord([])).toEqual(expected);
  });
});
