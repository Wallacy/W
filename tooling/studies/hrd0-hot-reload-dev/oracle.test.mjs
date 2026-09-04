import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { fileURLToPath } from "node:url";
import { evaluateHotReloadCase, evaluateHotReloadMutation, reduceLocal, reduceSplit, validateHotReload } from "../../hot-reload-dev-machine.mjs";
import { validateHotReloadStudyManifest } from "../../hot-reload-dev-manifest.mjs";

const studyDirectory = path.dirname(fileURLToPath(import.meta.url));
const toolingDirectory = path.resolve(studyDirectory, "../..");
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "hot-reload-dev-cases.json"), "utf8"));
const study = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const byId = new Map(corpus.cases.map((testCase) => [testCase.id, testCase]));

describe("HRD0 development-only hot reload oracle", () => {
  test("keeps the problem-first study and evidence boundary closed", () => {
    expect(validateHotReload(corpus, { root: repositoryRoot }).errors).toEqual([]);
    expect(validateHotReloadStudyManifest(study, { studyDirectory, repositoryRoot, allowStaleSnapshot: true })).toEqual([]);
    expect(study.languageSurface).toBe("none");
    expect(study.profileSurface).toBe("absent");
    expect(study.decision.runner).toBe("dev-only-tooling");
    expect(study.evidence.missing).toContain("provider");
    expect(study.evidence.missing).toContain("w-compile");
  });

  test("derives current composition, historical-candidate, and rejection routes", () => {
    const result = (id) => evaluateHotReloadCase(byId.get(id), { corpus });
    expect(result("HRD0-A-normal-unit-reopen")).toMatchObject({ status: "committed", route: "current-composition", generation: "g2" });
    expect(result("HRD0-B-typed-service-generation")).toMatchObject({ status: "committed", route: "current-composition" });
    expect(result("HRD0-C-generated-module-reopen-historical-candidate")).toMatchObject({ status: "historical-candidate", route: "historical-candidate", generatedReopened: true });
    expect(result("HRD0-C-invocation-spelling-unresolved")).toMatchObject({ status: "historical-candidate", route: "historical-candidate", code: "invocation-not-selected" });
    expect(result("HRD0-D-production-reload-rejected")).toMatchObject({ status: "intentionally-rejected", route: "intentionally-rejected" });
    expect(result("HRD0-D-live-state-migration-rejected")).toMatchObject({ status: "intentionally-rejected", generation: "g1" });
  });

  test("preserves the publication frontier and stale-generation invariants", () => {
    expect(evaluateHotReloadCase(byId.get("HRD0-A-invalid-unit-preserves-old"), { corpus })).toMatchObject({ status: "rejected", generation: "g1", code: "prepublication-parseFailure" });
    expect(evaluateHotReloadCase(byId.get("HRD0-A-prepublication-rollback"), { corpus })).toMatchObject({ status: "rolled-back", generation: "g1", rollback: true });
    expect(evaluateHotReloadCase(byId.get("HRD0-A-postpublication-drain-degraded"), { corpus })).toMatchObject({ status: "degraded", generation: "g2", postSwitchDrainFailure: "deadline" });
    expect(evaluateHotReloadCase(byId.get("HRD0-A-stale-completion-rejected"), { corpus }).staleRejections).toEqual(["staleCompletion"]);
    expect(evaluateHotReloadCase(byId.get("HRD0-A-cancel-before-publication"), { corpus })).toMatchObject({ status: "rejected", generation: "g1", code: "cancelled-before-publication" });
    expect(evaluateHotReloadCase(byId.get("HRD0-D-crash-before-publication-unknown"), { corpus })).toMatchObject({ status: "unknown-effect", generation: "g1" });
  });

  test("uses independent local and split reducers", () => {
    const testCase = byId.get("HRD0-B-local-split-equivalence");
    const local = reduceLocal({ ...testCase, events: testCase.projections.local, projections: undefined }, corpus);
    const split = reduceSplit({ ...testCase, events: testCase.projections.split, projections: undefined }, corpus);
    expect(local.physicalTrace).not.toEqual(split.physicalTrace);
    expect(evaluateHotReloadCase(testCase, { corpus })).toMatchObject({ mode: "paired", status: "committed", route: "current-composition" });
    const mutation = structuredClone(testCase);
    mutation.projections.split = [...mutation.projections.split, { op: "staleMessage", generation: "g1" }];
    expect(evaluateHotReloadCase(mutation, { corpus })).toMatchObject({ mode: "paired", code: "projection-divergence", status: "rejected" });
  });

  test("mutations cannot forge outcomes or reopen rejected mechanisms", () => {
    const expectedMutation = structuredClone(byId.get("HRD0-A-normal-unit-reopen"));
    expectedMutation.expect.status = "intentionally-rejected";
    expect(evaluateHotReloadCase(expectedMutation, { corpus }).status).toBe("committed");
    const forbidden = structuredClone(corpus);
    forbidden.cases[0].facts = { authority: "ambient" };
    expect(validateHotReload(forbidden, { root: repositoryRoot }).errors.some((error) => error.includes("derived fact"))).toBe(true);
    const unknownEvent = structuredClone(corpus);
    unknownEvent.cases[0].events = ["prepare", "inventedReload"];
    expect(validateHotReload(unknownEvent, { root: repositoryRoot }).errors.some((error) => error.includes("unknown operation"))).toBe(true);
    const cleanupMutation = structuredClone(corpus);
    cleanupMutation.cases[0].events.at(-1).steps = ["release", "destroy"];
    expect(validateHotReload(cleanupMutation, { root: repositoryRoot }).errors.some((error) => error.includes("canonical cleanup order"))).toBe(true);
    for (const id of ["HRD0-D-active-frame-rejected", "HRD0-D-eval-rejected", "HRD0-D-dlclose-live-callback-rejected", "HRD0-C-generated-module-injection-rejected"]) {
      expect(evaluateHotReloadCase(byId.get(id), { corpus }).route).toBe("intentionally-rejected");
    }
  });

  test("cleanup keeps logical steps common and physical resources declared", () => {
    const mutationResults = corpus.adversarialMutations.map((mutation) => evaluateHotReloadMutation(mutation, { corpus }));
    expect(mutationResults.slice(0, 3).map((result) => result.code)).toEqual([
      "cleanup-unneeded-step",
      "cleanup-missing-step",
      "cleanup-order",
    ]);
    const typed = evaluateHotReloadCase(byId.get("HRD0-B-typed-service-generation"), { corpus });
    expect(typed.cleanupOrder).toContain("unpin");
    expect(typed.cleanupOrder).toContain("unmap");
    expect(typed.logicalCleanupOrder).not.toContain("unpin");
    expect(typed.logicalCleanupOrder).not.toContain("unmap");
    const normal = evaluateHotReloadCase(byId.get("HRD0-A-normal-unit-reopen"), { corpus });
    expect(normal.cleanupOrder).not.toContain("unpin");
    expect(normal.cleanupOrder).not.toContain("unmap");
  });

  test("local and split import one nominal contract and reject identity drift", () => {
    const pair = byId.get("HRD0-B-local-split-equivalence");
    expect(pair.facts.contract.localNominal).toBe("hot_reload_dev_contract::ReloadInput+ReloadResult");
    expect(pair.facts.contract.splitNominal).toBe(pair.facts.contract.localNominal);
    expect(pair.facts.contract.splitInterfaceDigest).toBe(pair.facts.contract.localInterfaceDigest);
    const mutationResults = corpus.adversarialMutations.slice(3).map((mutation) => evaluateHotReloadMutation(mutation, { corpus }));
    expect(mutationResults.map((result) => result.code)).toEqual(["nominal-contract-divergence", "interface-digest-divergence"]);
    const contract = fs.readFileSync(path.join(repositoryRoot, "reference/last-light/hot_reload_dev_contract.w"), "utf8");
    const local = fs.readFileSync(path.join(repositoryRoot, "reference/last-light/hot_reload_dev_local.w"), "utf8");
    const split = fs.readFileSync(path.join(repositoryRoot, "reference/last-light/hot_reload_dev_split.w"), "utf8");
    expect(contract).toContain("export struct ReloadResult");
    expect(local).toContain("from hot_reload_dev_contract");
    expect(split).toContain("from hot_reload_dev_contract");
    for (const duplicate of ["export enum DevRunnerEvent", "export enum DevRunnerOutcome", "export struct ReloadInput", "export struct ReloadResult"]) {
      expect(local).not.toContain(duplicate);
      expect(split).not.toContain(duplicate);
    }
  });

  test("manifest digests and cross-study links are authoritative", () => {
    const stale = structuredClone(study);
    stale.sourceRefs[0].digest = "sha256:stale";
    expect(validateHotReloadStudyManifest(stale, { studyDirectory, repositoryRoot, allowStaleSnapshot: true }).some((error) => error.includes("sourceRefs[0].digest"))).toBe(true);
    const missing = structuredClone(study);
    missing.crossStudies = missing.crossStudies.filter((item) => item.id !== "SYN1");
    expect(validateHotReloadStudyManifest(missing, { studyDirectory, repositoryRoot, allowStaleSnapshot: true }).some((error) => error.includes("SYN1"))).toBe(true);
    const forged = structuredClone(study);
    forged.evidence.current.push("provider-ready");
    expect(validateHotReloadStudyManifest(forged, { studyDirectory, repositoryRoot, allowStaleSnapshot: true }).some((error) => error.includes("must not claim compiler/runtime/provider"))).toBe(true);
  });
});
