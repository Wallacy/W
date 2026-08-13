import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const cases = bundle.inputs.map(({ expected, ...input }) => ({ input, expected }));

function validate(input) {
  if (typeof input.operation !== "string") throw new Error("operation");
  if (typeof input.name !== "string") throw new Error("name");
  if (!/^\d+\.\d+\.\d+$/.test(input.version)) throw new Error("version");
  if (input.edition !== "2026") throw new Error("edition");
}

function run(operation, input) {
  validate(input);
  if (operation === "invalid-name") {
    return { accepted: false, cause: "invalid-name" };
  }
  if (operation === "data-only-package") {
    return {
      accepted: true,
      manifest: { name: input.name, version: input.version, edition: input.edition },
      executableSource: false,
    };
  }
  if (operation === "inline-package-record") {
    return { accepted: false, cause: "inline-package-contract" };
  }
  throw new Error("unknown operation");
}

describe("R1 manifest-surface host oracle", () => {
  for (const { input, expected } of cases) {
    test(`derives exact outcome for ${input.id}`, () => {
      expect(run(input.operation, input)).toEqual(expected);
    });
  }

  test("malformed manifest data fails before an oracle result", () => {
    expect(() => run("data-only-package", { operation: "data-only-package", name: "x", version: "1", edition: "2026" })).toThrow("version");
  });
});
