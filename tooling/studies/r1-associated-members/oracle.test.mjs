import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const courses = ["nebulaBroth", "photonSouffle", "quietSalad", "horizonCake"];

function selectCourse(form, ordinal) {
  if (!new Set(["direct", "protocol-requirement"]).has(form)) {
    throw new Error("unknown form");
  }
  return ordinal < courses.length
    ? { status: "selected", course: courses[ordinal] }
    : { status: "absent" };
}

function associatedPlan(form) {
  if (form === "direct") {
    return {
      lookup: "Course.member",
      protocolRequirement: false,
      witness: false,
      runtimeMetatype: false,
    };
  }
  return {
    lookup: "T.member",
    protocolRequirement: true,
    witness: true,
    runtimeMetatype: false,
  };
}

function mutableStatePlan(form) {
  if (form === "static-var") {
    return {
      status: "rejected",
      error: "mutable-type-storage",
      published: false,
      hiddenInitialization: false,
    };
  }
  if (form === "entry-owner") {
    return {
      status: "accepted",
      owner: "entry.catalog",
      published: true,
      hiddenInitialization: false,
    };
  }
  throw new Error("unknown mutable state form");
}

describe("R1 associated-member host oracle", () => {
  test("direct and protocol forms preserve every application outcome", () => {
    for (const { ordinal, expected } of bundle.inputs) {
      expect(selectCourse("direct", ordinal)).toEqual(expected);
      expect(selectCourse("protocol-requirement", ordinal)).toEqual(expected);
    }
  });

  test("a protocol adds a generic requirement without runtime metatype state", () => {
    expect(associatedPlan("direct")).toEqual({
      lookup: "Course.member",
      protocolRequirement: false,
      witness: false,
      runtimeMetatype: false,
    });
    expect(associatedPlan("protocol-requirement")).toEqual({
      lookup: "T.member",
      protocolRequirement: true,
      witness: true,
      runtimeMetatype: false,
    });
  });

  test("mutable type storage is rejected before publication", () => {
    expect(mutableStatePlan("static-var")).toEqual({
      status: "rejected",
      error: "mutable-type-storage",
      published: false,
      hiddenInitialization: false,
    });
    expect(mutableStatePlan("entry-owner")).toEqual({
      status: "accepted",
      owner: "entry.catalog",
      published: true,
      hiddenInitialization: false,
    });
  });
});
