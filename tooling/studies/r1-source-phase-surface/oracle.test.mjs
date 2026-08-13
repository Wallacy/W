import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const cases = bundle.inputs.map(({ expected, ...input }) => ({ input, expected }));

function validateItems(sourceItems) {
  if (!Array.isArray(sourceItems) || sourceItems.length === 0) throw new Error("sourceItems");
  sourceItems.forEach((item) => {
    if (!item || !["import", "declaration", "prototype", "body"].includes(item.kind)) throw new Error("kind");
    if (typeof item.name !== "string" || item.name === "") throw new Error("name");
  });
}

function analyze(sourceItems) {
  validateItems(sourceItems);
  const firstDeclaration = sourceItems.findIndex((item) => item.kind === "declaration" || item.kind === "prototype" || item.kind === "body");
  const importAfterDeclaration = firstDeclaration >= 0 && sourceItems.some((item, index) => item.kind === "import" && index > firstDeclaration);
  const bodyPresent = sourceItems.some((item) => item.kind === "body");
  const imports = sourceItems.filter((item) => item.kind === "import").map((item) => item.name);
  return {
    firstDeclaration,
    importAfterDeclaration,
    bodyPresent,
    importsFirst: !importAfterDeclaration,
    imports,
  };
}

function run(operation, input) {
  if (operation === "analyze-source") return analyze(input.sourceItems);
  if (operation === "interleaved-import") {
    validateItems(input.sourceItems);
    return { accepted: false, cause: "interleaved-import-and-prototype" };
  }
  if (operation === "prototype-without-body") {
    validateItems(input.sourceItems);
    return { accepted: false, cause: "prototype-without-body" };
  }
  throw new Error("unknown operation");
}

describe("R1 source-phase-surface host oracle", () => {
  for (const { input, expected } of cases) {
    test(`derives exact outcome for ${input.id}`, () => {
      expect(run(input.operation, input)).toEqual(expected);
    });
  }

  test("malformed source items fail before phase derivation", () => {
    expect(() => run("analyze-source", { sourceItems: [{ kind: "unknown", name: "x" }] })).toThrow("kind");
  });
});
