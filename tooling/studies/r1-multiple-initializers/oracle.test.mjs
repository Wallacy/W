import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

function minorUnits(input) {
  const value = BigInt(input.value);
  return input.unit === "major" ? value * 100n : value;
}

function construct(form, input) {
  const value = minorUnits(input).toString();
  if (form === "multiple") {
    return {
      status: "accepted",
      value,
      route: input.unit === "major" ? "majorUnits:" : "minorUnits:",
      modeValue: false,
      factory: false,
      throws: false,
    };
  }
  if (form === "single-mode") {
    return {
      status: "accepted",
      value,
      route: "value:",
      modeValue: true,
      factory: false,
      throws: false,
    };
  }
  if (form === "factories") {
    return {
      status: "accepted",
      value,
      route: input.unit === "major" ? "fromMajorUnits" : "minorUnits:",
      modeValue: false,
      factory: input.unit === "major",
      throws: false,
    };
  }
  throw new Error("unknown form");
}

function initializerShape(parameters) {
  const shapes = parameters.map(({ external }) =>
    `${external === null ? "$positional" : `${external}:`},currency:`
  );
  return {
    shapes,
    distinct: new Set(shapes).size === shapes.length,
    selectedBeforeTypes: true,
  };
}

describe("R1 multiple-initializer host oracle", () => {
  test("all variants preserve exact minor-unit outcomes", () => {
    for (const input of bundle.inputs) {
      for (const form of ["multiple", "single-mode", "factories"]) {
        expect(construct(form, input)).toMatchObject({
          status: "accepted",
          value: input.expected.minorUnits,
          throws: false,
        });
      }
    }
  });

  test("external labels keep the two initializer shapes disjoint", () => {
    expect(initializerShape([
      { external: "minorUnits", internal: "value", type: "i128" },
      { external: "majorUnits", internal: "value", type: "i64" },
    ])).toEqual({
      shapes: ["minorUnits:,currency:", "majorUnits:,currency:"],
      distinct: true,
      selectedBeforeTypes: true,
    });
    expect(initializerShape([
      { external: null, internal: "minorUnits", type: "i128" },
      { external: null, internal: "majorUnits", type: "i64" },
    ]).distinct).toBe(false);
  });

  test("the single mode and factory alternatives expose their extra mechanism", () => {
    const major = bundle.inputs.find((input) => input.id === "major-units");
    expect(construct("multiple", major)).toMatchObject({
      route: "majorUnits:",
      modeValue: false,
      factory: false,
    });
    expect(construct("single-mode", major)).toMatchObject({
      route: "value:",
      modeValue: true,
      factory: false,
    });
    expect(construct("factories", major)).toMatchObject({
      route: "fromMajorUnits",
      modeValue: false,
      factory: true,
    });
  });

  test("every i64 major-unit input remains total after widening to i128", () => {
    const maximum = bundle.inputs.find((input) => input.id === "maximum-major-units");
    expect(construct("multiple", maximum)).toMatchObject({
      status: "accepted",
      value: "922337203685477580700",
      throws: false,
    });
  });
});
