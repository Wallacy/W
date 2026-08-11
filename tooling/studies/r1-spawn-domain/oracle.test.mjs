import { describe, expect, test } from "bun:test";
import fs from "node:fs";

const bundle = JSON.parse(
  fs.readFileSync(new URL("bundle.json", import.meta.url), "utf8"),
);

function evaluate(input) {
  const profile = input.profile;

  if (profile.selectionOwner !== "product") {
    return {
      status: "rejected",
      reason: "profileSelectionOwner",
      domain: profile.domain,
      dispatch: "explicit",
      actualOwner: profile.selectionOwner,
    };
  }

  if (profile.capacity < 1) {
    return {
      status: "rejected",
      reason: "emptyCapacity",
      domain: profile.domain,
      dispatch: "explicit",
    };
  }

  const missingCapability = (profile.requiredCapabilities ?? [])
    .find((capability) => !profile.capabilities.includes(capability));
  if (missingCapability) {
    return {
      status: "rejected",
      reason: "missingCapability",
      domain: profile.domain,
      dispatch: "explicit",
      missingCapability,
    };
  }

  return {
    status: "accepted",
    domain: profile.domain,
    dispatch: "explicit",
    scheduling: profile.capabilities.includes("serial") ? "serial-fifo" : "domain-contract",
    parallelCapability: profile.capabilities.includes("parallel"),
    result: {
      port: input.left * input.left + 1,
      starboard: input.right * input.right + 1,
    },
  };
}

function positionalDomain(input) {
  return evaluate(input);
}

function namedDomain(input) {
  return evaluate(input);
}

function packUnits(profile, units) {
  if (profile.selectionOwner !== "product") {
    return { status: "rejected", reason: "profileSelectionOwner" };
  }
  return {
    status: "accepted",
    units: units.map((unit) => ({
      unit,
      executionProfile: profile.id,
      capacity: profile.capacity,
      capabilities: [...profile.capabilities],
    })),
  };
}

describe("R1 spawn-domain host oracle", () => {
  for (const input of bundle.inputs) {
    test(`${input.id} preserves resolution and outcome`, () => {
      expect(positionalDomain(input)).toEqual(input.expected);
      expect(namedDomain(input)).toEqual(input.expected);
    });
  }

  test("the product selection is preserved in every packed task unit", () => {
    const profile = bundle.inputs[0].profile;
    expect(packUnits(profile, ["restaurant", "observatory"])).toEqual({
      status: "accepted",
      units: [
        {
          unit: "restaurant",
          executionProfile: "native-bounded",
          capacity: 1,
          capabilities: ["concurrent", "parallel"],
        },
        {
          unit: "observatory",
          executionProfile: "native-bounded",
          capacity: 1,
          capabilities: ["concurrent", "parallel"],
        },
      ],
    });
    expect(packUnits(bundle.inputs[3].profile, ["restaurant"])).toEqual({
      status: "rejected",
      reason: "profileSelectionOwner",
    });
  });
});
