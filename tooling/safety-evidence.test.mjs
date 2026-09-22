import { describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { validateSafetyEvidence } from "./check-safety-evidence.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const source = JSON.parse(
  fs.readFileSync(path.join(root, "tooling", "safety-evidence.json"), "utf8"),
);

function mutated(change) {
  const value = structuredClone(source);
  change(value);
  return value;
}

describe("safety evidence promotion barrier", () => {
  test("accepts the current bounded catalog", () => {
    expect(validateSafetyEvidence(source).summary).toEqual({
      blocked: 7,
      bounded: 1,
      general: 0,
    });
  });

  test("rejects a current gate without a durable record", () => {
    const value = mutated((catalog) => {
      delete catalog.areas[1].evidence.source;
    });
    expect(() => validateSafetyEvidence(value)).toThrow();
  });

  test("rejects a record that omits a referenced gate", () => {
    const value = mutated((catalog) => {
      catalog.records["numeric-runtime-i8-process-v1"].gates = [
        "verifiedHir", "lowering", "nativeProduct", "negative",
      ];
    });
    expect(() => validateSafetyEvidence(value)).toThrow();
  });

  test("rejects unknown and unused record inventory", () => {
    const value = mutated((catalog) => {
      catalog.policy.recordIds.push("unused-record-v1");
      catalog.records["unused-record-v1"] = structuredClone(
        catalog.records["numeric-runtime-i8-process-v1"],
      );
    });
    expect(() => validateSafetyEvidence(value)).toThrow();
  });

  test("rejects stale source evidence", () => {
    const value = mutated((catalog) => {
      catalog.records["numeric-runtime-i8-process-v1"].freshness.digests[0].digest =
        `sha256:${"0".repeat(64)}`;
    });
    expect(() => validateSafetyEvidence(value)).toThrow();
  });

  test("rejects promotion beyond the applicable gate states", () => {
    const value = mutated((catalog) => {
      catalog.areas[1].promotion = "general";
    });
    expect(() => validateSafetyEvidence(value)).toThrow();
  });
});
