import { describe, expect, test } from "bun:test";
import fs from "node:fs";

const study = JSON.parse(fs.readFileSync(new URL("./study.json", import.meta.url), "utf8"));
const cases = JSON.parse(fs.readFileSync(new URL("./cases.json", import.meta.url), "utf8"));

describe("MLS0 modern MLIR and science boundary", () => {
  test("pins the current stable release without rewriting old evidence", () => {
    expect(cases.toolchain).toMatchObject({
      stableVersion: "23.1.1",
      tag: "llvmorg-23.1.1",
      tagObject: "e7ce3600b55034ddf819638f395e3c475fad5be2",
      commit: "6dfe1677ab8dffbc6ec13d53a1e0215d75147689",
      selection: "exact-stable-pin",
      legacyPolicy: "retain-reproducible-evidence; revalidate-ideas; do-not-inherit-apis",
    });
    expect(JSON.stringify(cases.toolchain)).not.toMatch(/latest|nightly|main-as-contract/u);
  });

  test("keeps W semantics above replaceable MLIR adapters", () => {
    expect(cases.wOwnedLayers).toEqual([
      "w.core", "w.ownership", "w.effect", "w.task", "w.memory", "w.abi", "w.science", "w.accelerator",
    ]);
    expect(cases.dialectRoutes.every((route) => route.boundary !== "public-W-ABI")).toBe(true);
    expect(cases.runtimeBoundary.asyncDialectIsScheduler).toBe(false);
    expect(cases.runtimeBoundary.gpuDialectIsProvider).toBe(false);
    expect(cases.runtimeBoundary.referenceMlirAsyncRuntimeIsDefault).toBe(false);
  });

  test("retains semantic information until scheduling and late bufferization", () => {
    expect(cases.pipeline.indexOf("W-semantic-IR-and-verification")).toBeLessThan(cases.pipeline.indexOf("structured-domain-IR"));
    expect(cases.pipeline.indexOf("schedule-and-cost-selection")).toBeLessThan(cases.pipeline.indexOf("late-bufferization-and-data-movement"));
    expect(cases.loweringStrategies).toEqual([
      "small-static-inline-generated-code",
      "structured-MLIR-transforms",
      "large-or-dynamic-provider-call",
    ]);
  });

  test("covers the bounded science roadmap with explicit semantics and benchmarks", () => {
    expect(cases.algorithmFamilies.map((family) => family.id)).toEqual([
      "linear-algebra", "fft", "convolution", "reductions", "scans", "sparse", "stencils", "random", "sorting", "solvers",
    ]);
    expect(cases.algorithmFamilies.filter((family) => family.priority === "P0")).toHaveLength(7);
    expect(cases.algorithmFamilies.filter((family) => family.priority === "P1")).toHaveLength(3);
    for (const family of cases.algorithmFamilies) {
      expect(family.package.length).toBeGreaterThan(0);
      expect(family.operations.length).toBeGreaterThan(0);
      expect(family.preserve.length).toBeGreaterThanOrEqual(6);
      expect(family.primaryMlir.length).toBeGreaterThan(0);
      expect(family.benchmark.length).toBeGreaterThan(0);
    }
    expect(cases.algorithmFamilies.find((family) => family.id === "fft")?.preserve).toContain("factorization");
  });

  test("closes research without claiming implementation", () => {
    expect(study.status).toBe("complete-design-study");
    expect(study.evidenceBoundary.researchClosed).toBe(true);
    for (const field of ["languageSurfaceAdded", "dialectImplemented", "providerImplemented", "algorithmImplemented", "performanceMeasured", "proofSurfaceRatified", "proofLanguageImplemented"]) {
      expect(study.evidenceBoundary[field]).toBe(false);
    }
    expect(cases.promotionGate).toContain("compile-time-and-artifact-size-measurement");
  });

  test("keeps proof capability optional, erased, and independent from build profiles", () => {
    const proof = cases.proofLayerCandidate;
    expect(proof.status).toBe("orthogonal-candidate-research-gated");
    expect(proof.proofCapabilityGoal).toBe(true);
    expect(proof.dependentTypesInCore).toBe(false);
    expect(proof.runtimeAndAbiCostAllowed).toBe(false);
    expect(proof.modeAxis.separateFromBuildProfile).toBe(true);
    expect(proof.modeAxis.compositions).toContain("release+proof");
    expect(proof.modeAxis.proofFailure).toBe("compile-error-before-lowering");
    expect(proof.requiredProperties).toContain("proof-terms-erased-before-codegen");
    expect(proof.requiredProperties).toContain("byte-and-performance-equivalence-after-erasure");
    expect(proof.rejectedShortcuts).toContain("universal-dependent-types-in-main-type-checker");
    expect(proof.nextStudy).toBe("PVL0-proof-kernel-and-erasure");
  });
});
