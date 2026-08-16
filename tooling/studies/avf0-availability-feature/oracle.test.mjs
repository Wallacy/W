import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { deriveAvf0, validateAvf0 } from "../../avf0-availability-feature-machine.mjs";
import { validateAvf0StudyManifest } from "../../avf0-availability-feature-manifest.mjs";

const studyDirectory = import.meta.dir;
const toolingDirectory = path.resolve(studyDirectory, "../..");
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "avf0-availability-feature-cases.json"), "utf8"));
const study = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const bundle = JSON.parse(fs.readFileSync(path.join(studyDirectory, "bundle.json"), "utf8"));

const results = deriveAvf0(corpus);
const result = (id) => results.find((entry) => entry.caseId === id);

describe("AVF0 study oracle", () => {
  test("keeps package, availability, runtime policy, and composition separate", () => {
    expect(validateAvf0(corpus).errors).toEqual([]);
    expect(validateAvf0StudyManifest(study, { studyDirectory, repositoryRoot })).toEqual([]);
    expect(study.routeMatrix.map(({ axis, disposition }) => [axis, disposition])).toEqual([
      ["package", "current"],
      ["availability", "research"],
      ["runtime", "composable"],
      ["composition", "composable"],
    ]);
    expect(bundle.variants.find((entry) => entry.id === "availability-binding")).toMatchObject({ role: "research-candidate", disposition: "research-candidate" });
  });

  test("does not let runtime policy become authority", () => {
    expect(result("AVF0-availability-capability-missing").code).toBe("availabilityCannotGrantCapability");
    expect(result("AVF0-availability-effect-missing").code).toBe("availabilityCannotGrantEffect");
    expect(result("AVF0-runtime-grant-capability").code).toBe("runtimeFeatureAuthorityRejected");
    expect(result("AVF0-runtime-load-module").code).toBe("runtimeFeatureAuthorityRejected");
    expect(result("AVF0-runtime-change-abi").code).toBe("runtimeFeatureAuthorityRejected");
    expect(result("AVF0-composition-flag-narrows").code).toBe("runtimeFeatureAuthorityRejected");
  });

  test("keeps typed fallback and explicit exposure", () => {
    expect(result("AVF0-runtime-stale-fallback")).toMatchObject({ status: "accepted", code: "featureStaleFallback", value: "control" });
    expect(result("AVF0-runtime-missing-fallback")).toMatchObject({ status: "accepted", code: "featureMissingFallback", value: "control" });
    expect(result("AVF0-runtime-implicit-exposure").code).toBe("implicitExposureEffectRejected");
    expect(result("AVF0-runtime-explicit-exposure")).toMatchObject({ status: "accepted", exposureRequired: true });
  });

  test("orders availability before runtime policy", () => {
    expect(result("AVF0-composition-availability-first").code).toBe("availabilityAndFeatureComposed");
    expect(result("AVF0-composition-provider-fallback").code).toBe("availabilityFallbackBeforeFeature");
    expect(result("AVF0-composition-feature-first").code).toBe("featureCannotNarrowAvailability");
  });
});
