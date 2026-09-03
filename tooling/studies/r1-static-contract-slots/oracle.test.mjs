import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const inputs = bundle.inputs.map(({ id: _id, expected: _expected, ...input }) => input);

function validate(input) {
  if (typeof input.enabled !== "boolean") throw new Error("enabled");
  if (typeof input.name !== "string" || input.name.length === 0) throw new Error("name");
  for (const field of ["timeoutMilliseconds", "tables", "courses", "bufferBytes"]) {
    if (!Number.isSafeInteger(input[field]) || input[field] < 0) throw new Error(field);
  }
}

function named(input) {
  validate(input);
  const slots = {
    enabled: input.enabled,
    name: input.name,
    timeoutMilliseconds: input.timeoutMilliseconds,
    tables: input.tables,
    courses: input.courses,
    bufferBytes: input.bufferBytes,
  };
  return { ...slots };
}

function positional(input) {
  validate(input);
  const slots = [
    input.enabled,
    input.name,
    input.timeoutMilliseconds,
    input.tables,
    input.courses,
    input.bufferBytes,
  ];
  return {
    enabled: slots[0],
    name: slots[1],
    timeoutMilliseconds: slots[2],
    tables: slots[3],
    courses: slots[4],
    bufferBytes: slots[5],
  };
}

function splitCall(input) {
  validate(input);
  const staticSlots = [input.enabled, input.tables, input.courses, input.bufferBytes];
  const runtimeArguments = { name: input.name, timeoutMilliseconds: input.timeoutMilliseconds };
  return {
    enabled: staticSlots[0],
    name: runtimeArguments.name,
    timeoutMilliseconds: runtimeArguments.timeoutMilliseconds,
    tables: staticSlots[1],
    courses: staticSlots[2],
    bufferBytes: staticSlots[3],
  };
}

function swappedSameTypeSlots(input, form) {
  validate(input);
  if (form === "named") {
    return { accepted: true, published: true, result: named(input) };
  }
  const result = form === "positional" ? positional(input) : splitCall(input);
  return {
    accepted: true,
    published: true,
    result: { ...result, tables: input.courses, courses: input.tables },
  };
}

function staticParity(form) {
  const typeKinds = ["Bool", "String", "Quantity", "usize", "usize", "usize"];
  if (form === "split-call") {
    return {
      typeKinds,
      callKinds: ["Bool", "usize", "usize", "usize"],
      parity: false,
      runtimeFields: ["name", "timeout"],
    };
  }
  return { typeKinds, callKinds: [...typeKinds], parity: true, runtimeFields: [] };
}

describe("R1 static contract slot host oracle", () => {
  test("all variants preserve both application outcomes", () => {
    for (const input of inputs) {
      const expected = { ...input };
      expect(named(input)).toEqual(expected);
      expect(positional(input)).toEqual(expected);
      expect(splitCall(input)).toEqual(expected);
    }
  });

  test("named labels reorder globally while positional slots stay ordered", () => {
    const input = inputs[0];
    expect(swappedSameTypeSlots(input, "named")).toEqual({
      accepted: true,
      published: true,
      result: input,
    });
    expect(swappedSameTypeSlots(input, "positional").result).toMatchObject({
      tables: 4,
      courses: 8,
    });
    expect(swappedSameTypeSlots(input, "split-call").result).toMatchObject({
      tables: 4,
      courses: 8,
    });
  });

  test("type application and generic call expose the same static categories", () => {
    expect(staticParity("named")).toEqual({
      typeKinds: ["Bool", "String", "Quantity", "usize", "usize", "usize"],
      callKinds: ["Bool", "String", "Quantity", "usize", "usize", "usize"],
      parity: true,
      runtimeFields: [],
    });
    expect(staticParity("positional").parity).toBe(true);
    expect(staticParity("split-call")).toMatchObject({
      parity: false,
      runtimeFields: ["name", "timeout"],
    });
  });
});
