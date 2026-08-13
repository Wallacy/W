import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const cases = bundle.inputs.map(({ expected, ...input }) => ({ input, expected }));

function validate(input) {
  if (typeof input.operation !== "string") throw new Error("operation");
}

function fixedFormatter(items) {
  if (!Array.isArray(items) || items.length === 0) throw new Error("items");
  return items.map((item) => {
    if (!item || typeof item.kind !== "string" || typeof item.value !== "string") throw new Error("item");
    if (item.kind === "import") return `import ${item.value}`;
    if (item.kind === "declaration") return item.value;
    throw new Error("kind");
  }).join("\n");
}

function deriveOrderLabel(items) {
  return items.map((item) => item.kind).join("-");
}

function run(operation, input) {
  validate(input);
  if (operation === "newline-return") {
    if (!Number.isSafeInteger(input.returnValue)) throw new Error("returnValue");
    return { value: input.returnValue };
  }
  if (operation === "forced-semicolon") {
    if (typeof input.traceValue !== "string" || typeof input.stage !== "string") throw new Error("discard");
    return { stage: input.stage, trace: input.traceValue === "" ? [] : [input.traceValue], semicolon: true };
  }
  if (operation === "discard") {
    if (typeof input.traceValue !== "string" || typeof input.stage !== "string") throw new Error("discard");
    return { stage: input.stage, trace: input.traceValue === "" ? [] : [input.traceValue], discarded: true };
  }
  if (operation === "formatter-policy") {
    return { canonical: fixedFormatter(input.items), orderLabel: deriveOrderLabel(input.items) };
  }
  if (operation === "automatic-semicolon-insertion") {
    return { accepted: false, cause: "automatic-semicolon-insertion" };
  }
  if (operation === "environmental-formatter-policy") {
    return { accepted: false, cause: "environmental-formatter-policy" };
  }
  throw new Error("unknown operation");
}

describe("R1 source-boundaries host oracle", () => {
  for (const { input, expected } of cases) {
    test(`derives exact outcome for ${input.id}`, () => {
      expect(run(input.operation, input)).toEqual(expected);
    });
  }

  test("malformed formatter input fails before canonicalization", () => {
    expect(() => run("formatter-policy", { operation: "formatter-policy", orderLabel: "bad", items: [] })).toThrow("items");
  });
});
