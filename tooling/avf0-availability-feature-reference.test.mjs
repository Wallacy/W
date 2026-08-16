import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import {
  deriveAvf0,
  deriveAvf0Case,
  reduceAvailability,
  reducePackageFeature,
  reduceRuntimeFeature,
  validateAvf0,
} from "./avf0-availability-feature-machine.mjs";

const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "avf0-availability-feature-cases.json"), "utf8"));
const result = (id) => deriveAvf0(corpus).find((entry) => entry.caseId === id);

describe("AVF0 availability and feature host oracle", () => {
  test("derives all four axes independently from expected output", () => {
    const validation = validateAvf0(corpus);
    expect(validation.errors).toEqual([]);
    expect(validation.results).toHaveLength(38);
    expect(new Set(validation.results.map((entry) => entry.axis))).toEqual(new Set(["package", "availability", "runtime", "composition"]));
  });

  test("package features remain additive graph selection", () => {
    expect(result("AVF0-package-additive")).toMatchObject({ status: "accepted", route: "current" });
    expect(result("AVF0-package-union-idempotent").graphSelection).toEqual(["moduleSet:checkout-candidate"]);
    expect(result("AVF0-package-removal").code).toBe("featureMustBeAdditive");
    expect(result("AVF0-package-source-conditional").code).toBe("sourceFeatureConditionalRejected");
  });

  test("availability can bind a symbol but cannot create authority", () => {
    expect(result("AVF0-availability-provider-bind")).toMatchObject({ status: "accepted", route: "research", boundSymbol: "CameraFrame.acquire" });
    expect(result("AVF0-availability-provider-fallback").boundSymbol).toBeNull();
    expect(result("AVF0-availability-capability-missing").code).toBe("availabilityCannotGrantCapability");
    expect(result("AVF0-availability-raw-version").code).toBe("availabilityEvidenceNotAuthoritative");
  });

  test("declaration lifecycle remains distinct from runtime feature policy", () => {
    expect(result("AVF0-availability-deprecated").warning).toBe("deprecated");
    expect(result("AVF0-availability-obsoleted").code).toBe("symbolObsoleted");
    expect(result("AVF0-availability-renamed").details.renamed).toBe("CameraFrame.capture");
  });

  test("runtime flags are typed snapshots with explicit fallback", () => {
    expect(result("AVF0-runtime-typed")).toMatchObject({ status: "accepted", route: "composable", value: "control", freshness: "fresh" });
    expect(result("AVF0-runtime-stale-fallback")).toMatchObject({ value: "control", freshness: "stale" });
    expect(result("AVF0-runtime-missing-fallback")).toMatchObject({ value: "control", freshness: "missing" });
    expect(result("AVF0-runtime-value-mismatch").code).toBe("featureValueTypeMismatch");
  });

  test("configuration changes preserve schema identity", () => {
    const base = structuredClone(corpus.fixtures.runtime);
    const first = reduceRuntimeFeature(base);
    base.snapshot.configurationDigest = "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    base.snapshot.value = "candidate";
    const second = reduceRuntimeFeature(base);
    expect(second.featureSchemaKey).toBe(first.featureSchemaKey);
    expect(second.configurationDigest).not.toBe(first.configurationDigest);
    base.key.values.push("experiment");
    expect(reduceRuntimeFeature(base).featureSchemaKey).not.toBe(first.featureSchemaKey);
  });

  test("rollout is deterministic and evaluation has no hidden exposure effect", () => {
    const input = structuredClone(corpus.fixtures.runtime);
    input.snapshot.rules = [{ priority: 1, conditions: [], value: "candidate", rollout: { field: "userId", basisPoints: 5000 } }];
    expect(reduceRuntimeFeature(input).value).toBe(reduceRuntimeFeature(input).value);
    expect(result("AVF0-runtime-implicit-exposure").code).toBe("implicitExposureEffectRejected");
    expect(result("AVF0-runtime-explicit-exposure").exposureRequired).toBe(true);
  });

  test("runtime policy cannot load code, change ABI, or narrow availability", () => {
    for (const id of ["AVF0-runtime-grant-capability", "AVF0-runtime-load-module", "AVF0-runtime-change-abi", "AVF0-composition-flag-narrows"]) {
      expect(result(id).code).toBe("runtimeFeatureAuthorityRejected");
    }
    expect(result("AVF0-composition-feature-first").code).toBe("featureCannotNarrowAvailability");
  });

  test("closed schemas and caller echo cannot forge a decision", () => {
    const packageInput = structuredClone(corpus.fixtures.package);
    packageInput.status = "accepted";
    expect(deriveAvf0Case({ id: "AVF0-mutation-echo", axis: "package", input: packageInput }).code).toBe("callerResultEcho");
    const availability = structuredClone(corpus.fixtures.availability);
    availability.declaration.priority = 1;
    expect(() => reduceAvailability(availability)).toThrow("availabilityDeclarationInvalid");
    const packageUnknown = structuredClone(corpus.fixtures.package);
    packageUnknown.mode = "dynamic";
    expect(() => reducePackageFeature(packageUnknown)).toThrow("packageFeatureInputInvalid");
  });
});
