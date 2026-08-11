import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const cases = bundle.inputs.map(({ id: _id, expected, ...input }) => ({ input, expected }));

function scalarCount(value) {
  return Array.from(value).length;
}

function validateTitle(value) {
  const count = scalarCount(value);
  if (count < 1 || count > 40) {
    return { status: "rejected", error: "outsideBounds", scalarCount: count };
  }
  return { status: "accepted", value, scalarCount: count };
}

function runVariant(form, input) {
  if (!new Set(["contextual", "explicit", "runtime-check"]).has(form)) {
    throw new Error("unknown form");
  }
  return validateTitle(input.value);
}

function staticLiteralAdmission(form, value) {
  const result = validateTitle(value);
  if (form === "runtime-check") {
    return {
      declarationAccepted: true,
      refinementProved: false,
      functionOutcome: result.status,
    };
  }
  return {
    declarationAccepted: result.status === "accepted",
    refinementProved: result.status === "accepted",
    functionOutcome: result.status,
  };
}

function subjectResolution(form, outerValueType) {
  if (form === "contextual") {
    return { binding: "implicit-candidate", lexicalLookup: false, outerValueType };
  }
  if (form === "explicit") {
    return { binding: "contextual:value", lexicalLookup: false, outerValueType };
  }
  return { binding: "parameter:input", lexicalLookup: true, outerValueType };
}

describe("R1 refinement subject host oracle", () => {
  test("all variants preserve accepted and rejected application outcomes", () => {
    for (const { input, expected } of cases) {
      for (const form of ["contextual", "explicit", "runtime-check"]) {
        expect(runVariant(form, input)).toEqual(expected);
      }
    }
  });

  test("only the refined types reject an invalid literal at its declaration", () => {
    const invalid = "12345678901234567890123456789012345678901";
    expect(staticLiteralAdmission("contextual", invalid)).toEqual({
      declarationAccepted: false,
      refinementProved: false,
      functionOutcome: "rejected",
    });
    expect(staticLiteralAdmission("explicit", invalid)).toEqual(
      staticLiteralAdmission("contextual", invalid),
    );
    expect(staticLiteralAdmission("runtime-check", invalid)).toEqual({
      declarationAccepted: true,
      refinementProved: false,
      functionOutcome: "rejected",
    });
  });

  test("explicit value binds the candidate before lexical lookup", () => {
    expect(subjectResolution("contextual", "Bool")).toEqual({
      binding: "implicit-candidate",
      lexicalLookup: false,
      outerValueType: "Bool",
    });
    expect(subjectResolution("explicit", "Bool")).toEqual({
      binding: "contextual:value",
      lexicalLookup: false,
      outerValueType: "Bool",
    });
    expect(subjectResolution("runtime-check", "Bool").lexicalLookup).toBe(true);
  });
});
