import { describe, expect, test } from "bun:test";

function repeatCount(value, trace = []) {
  let remaining = value;
  let digits = 0;
  do {
    digits += 1;
    trace.push("body");
    remaining = Math.floor(remaining / 10);
    trace.push("predicate");
  } while (remaining > 0);
  return digits;
}

function repeatContinue(value, trace = []) {
  let remaining = value;
  let digits = 0;
  do {
    digits += 1;
    trace.push("body");
    remaining = Math.floor(remaining / 10);
    if (remaining > 0) {
      trace.push("continue");
      trace.push("predicate");
      continue;
    }
    trace.push("predicate");
  } while (remaining > 0);
  return digits;
}

function whileTrueContinue(value, trace = []) {
  let remaining = value;
  let digits = 0;
  while (true) {
    digits += 1;
    remaining = Math.floor(remaining / 10);
    trace.push("guard");
    if (remaining > 0) continue;
    trace.push("break");
    break;
  }
  return digits;
}

function repeatBreak(value, limit, trace = []) {
  let remaining = value;
  let digits = 0;
  do {
    digits += 1;
    trace.push("body");
    if (digits === limit) {
      trace.push("break");
      break;
    }
    remaining = Math.floor(remaining / 10);
    trace.push("predicate");
  } while (remaining > 0);
  return digits;
}

function repeatBreakWithCleanup(value, limit, trace = []) {
  try {
    return repeatBreak(value, limit, trace);
  } finally {
    trace.push("cleanup");
  }
}

function whileTrueCount(value, trace = []) {
  let remaining = value;
  let digits = 0;
  while (true) {
    digits += 1;
    trace.push("body");
    remaining = Math.floor(remaining / 10);
    trace.push("guard");
    if (remaining === 0) break;
  }
  return digits;
}

function whileTrueBreak(value, limit, trace = []) {
  let remaining = value;
  let digits = 0;
  while (true) {
    digits += 1;
    trace.push("body");
    if (digits === limit) {
      trace.push("break");
      break;
    }
    remaining = Math.floor(remaining / 10);
    trace.push("guard");
    if (remaining === 0) break;
  }
  return digits;
}

describe("R1 post-test-loop host oracle", () => {
  test("zero, nine, and multidigit values run the body at least once", () => {
    for (const [index, value] of [0, 9, 42_424].entries()) {
      const repeatTrace = [];
      const alternativeTrace = [];
      expect(repeatCount(value, repeatTrace)).toBe(whileTrueCount(value, alternativeTrace));
      expect(repeatTrace.filter((event) => event === "body").length).toBe([1, 1, 5][index]);
      expect(repeatTrace.filter((event) => event === "predicate").length).toBe(
        repeatTrace.filter((event) => event === "body").length,
      );
      expect(alternativeTrace.filter((event) => event === "body").length).toBe(
        repeatTrace.filter((event) => event === "body").length,
      );
      expect(alternativeTrace.filter((event) => event === "guard").length).toBe(
        repeatTrace.filter((event) => event === "predicate").length,
      );
    }
  });

  test("continue reaches the trailing predicate and alternative guard", () => {
    const repeatContinueCounts = [];
    const alternativeGuardCounts = [];
    for (const value of [0, 9, 42_424]) {
      const repeatTrace = [];
      const alternativeTrace = [];
      expect(repeatContinue(value, repeatTrace)).toBe(whileTrueContinue(value, alternativeTrace));
      repeatContinueCounts.push(repeatTrace.filter((event) => event === "continue").length);
      alternativeGuardCounts.push(alternativeTrace.filter((event) => event === "guard").length);
      expect(repeatTrace.filter((event) => event === "continue").length).toBe(
        alternativeTrace.filter((event) => event === "guard").length - 1,
      );
    }
    expect(repeatContinueCounts).toEqual([0, 0, 4]);
    expect(alternativeGuardCounts).toEqual([1, 1, 5]);
  });

  test("break exits without re-evaluating the predicate", () => {
    const repeatTrace = [];
    const alternativeTrace = [];
    expect(repeatBreak(42_424, 3, repeatTrace)).toBe(whileTrueBreak(42_424, 3, alternativeTrace));
    expect(repeatTrace.at(-1)).toBe("break");
    expect(repeatTrace.filter((event) => event === "predicate").length).toBe(2);
    expect(alternativeTrace.at(-1)).toBe("break");
    expect(alternativeTrace.filter((event) => event === "guard").length).toBe(2);
  });

  test("lexical cleanup still runs when break exits the loop", () => {
    const trace = [];
    expect(repeatBreakWithCleanup(42_424, 2, trace)).toBe(2);
    expect(trace.at(-1)).toBe("cleanup");
  });

  test("predicate and body counts stay equal on normal completion", () => {
    for (const value of [0, 9, 42_424]) {
      const trace = [];
      expect(repeatCount(value, trace)).toBe(trace.filter((event) => event === "body").length);
      expect(trace.filter((event) => event === "body").length).toBe(
        trace.filter((event) => event === "predicate").length,
      );
    }
  });
});
