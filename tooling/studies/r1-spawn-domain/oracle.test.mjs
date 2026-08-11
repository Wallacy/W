import { describe, expect, test } from "bun:test";
import fs from "node:fs";

const bundle = JSON.parse(
  fs.readFileSync(new URL("bundle.json", import.meta.url), "utf8"),
);

function evaluate(input) {
  const profile = input.profile;

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

describe("R1 spawn-domain host oracle", () => {
  for (const input of bundle.inputs) {
    test(`${input.id} preserves resolution and outcome`, () => {
      expect(positionalDomain(input)).toEqual(input.expected);
      expect(namedDomain(input)).toEqual(input.expected);
    });
  }
});
