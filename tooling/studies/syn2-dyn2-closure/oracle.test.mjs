import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import {
  evaluateSyn2Dyn2Case,
  summarizeSyn2Dyn2,
  validateSyn2Dyn2,
} from "../../syn2-dyn2-closure-machine.mjs";

const toolingDirectory = path.resolve(import.meta.dir, "../..");
const repositoryDirectory = path.resolve(toolingDirectory, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "syn2-dyn2-closure-cases.json"), "utf8"));
const byId = new Map(corpus.cases.map((testCase) => [testCase.id, testCase]));

describe("SYN2/DYN2 closure host oracle", () => {
  test("validates the small reused corpus and derives both axes", () => {
    expect(validateSyn2Dyn2(corpus).errors).toEqual([]);
    expect(summarizeSyn2Dyn2(corpus)).toEqual({
      caseCount: 17,
      currentContractCount: 12,
      rejectedCount: 5,
      implementationBoundaryCount: 1,
      axes: { SYN2: 10, DYN2: 7 },
    });
  });

  test("keeps generated module publication identities and phases separate", () => {
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-C-module-set")).code).toBe("promoted-design-contract");
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-C-action-result-interface")).status).toBe("current-contract");
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-C-result-preserved-on-parse-fault")).status).toBe("current-contract");
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-C-pre-result-cancel")).status).toBe("current-contract");
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-C-target-receipts")).status).toBe("current-contract");
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-C-implementation-boundary")).implementationEvidenceGap).toBe(true);
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-C-implementation-boundary")).missingEvidence).toContain("target-compiler-provider");
  });

  test("derives strict events and manifest evidence without accepting C2 or injection", () => {
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-events-derived")).status).toBe("current-contract");
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-manifest-boundary")).status).toBe("current-contract");
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-reject-typed-recipe"))).toMatchObject({ status: "rejected", code: "typed-recipe-duplicates-frontend" });
    expect(evaluateSyn2Dyn2Case(byId.get("SYN2-reject-current-module-injection"))).toMatchObject({ status: "rejected", code: "current-module-injection-rejected" });
  });

  test("keeps DYN2 A/B/C narrow and rejects live-state and D mechanisms", () => {
    for (const id of ["DYN2-A-repl-snapshot", "DYN2-B-typed-service-local-split", "DYN2-C-generation-reference", "DYN2-switch-cleanup-fault"]) {
      expect(evaluateSyn2Dyn2Case(byId.get(id)).status).toBe("current-contract");
    }
    expect(evaluateSyn2Dyn2Case(byId.get("DYN2-C-generation-reference")).status).toBe("current-contract");
    for (const id of ["DYN2-reject-live-state-migration", "DYN2-D-reject-eval-frame", "DYN2-reject-native-unload-callback"]) {
      expect(evaluateSyn2Dyn2Case(byId.get(id)).status).toBe("rejected");
    }
  });

  test("caller-owned result fields cannot change a derived outcome", () => {
    const candidate = structuredClone(byId.get("DYN2-B-typed-service-local-split"));
    const derived = evaluateSyn2Dyn2Case(candidate);
    candidate.status = "rejected";
    candidate.expected = { route: "rejected" };
    candidate.observedStatus = "rejected";
    expect(evaluateSyn2Dyn2Case(candidate)).toEqual(derived);
    expect(validateSyn2Dyn2({ ...corpus, cases: [candidate, ...corpus.cases.slice(1)] }).errors.join("\n")).toContain("caller-owned");
  });

  test("mutation labels assert a fact-derived rejection and cannot select it", () => {
    const candidate = structuredClone(byId.get("SYN2-reject-typed-recipe"));
    expect(evaluateSyn2Dyn2Case(candidate)).toMatchObject({ status: "rejected", code: "typed-recipe-duplicates-frontend" });
    candidate.mutation.kind = "forged-outcome-selector";
    expect(evaluateSyn2Dyn2Case(candidate)).toMatchObject({ status: "rejected", code: "typed-recipe-duplicates-frontend" });
    expect(validateSyn2Dyn2({ ...corpus, cases: [candidate, ...corpus.cases.slice(1)] }).errors.join("\n")).toContain("expected assertion label");
  });

  test("reused study manifests are digest-pinned and no generated implementation is claimed", () => {
    const study = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "study.json"), "utf8"));
    expect(study.supersedes).toBeUndefined();
    expect(study.buildsOn.map((ref) => ref.id)).toEqual(["SYN1", "DYN1", "HRD0"]);
    for (const ref of study.buildsOn) {
      expect(/^sha256:[0-9a-f]{64}$/.test(ref.digest)).toBe(true);
      expect(fs.existsSync(path.resolve(import.meta.dir, ref.path))).toBe(true);
    }
    expect(Object.keys(study.artifacts).sort()).toEqual(["bundle", "checker", "corpus", "machine", "nestedChecker", "snapshot", "studyOracle"].sort());
    for (const ref of Object.values(study.artifacts)) {
      expect(/^sha256:[0-9a-f]{64}$/.test(ref.digest)).toBe(true);
      expect(fs.existsSync(path.resolve(import.meta.dir, ref.path))).toBe(true);
    }
    expect(study.independentReducers.map((ref) => ref.id)).toEqual(["DYN1", "HRD0"]);
    expect(study.independentReducers.every((ref) => ref.role === "validated-independent-reducer")).toBe(true);
    expect(study.sourceRefs.length).toBeGreaterThanOrEqual(6);
    expect(study.generationReference.fields).toEqual(["generationId", "artifactDigest", "recipeDigest", "semanticInterfaceKey", "schemaDigest", "targetReceipt", "resolveReceipt", "migrationReceipt"]);
    expect(study.generationReference.authority).toBe("none");
    expect(study.generationReference.convertToServiceRef).toBe(false);
    expect(study.status).toBe("design-oracle-input");
    expect(study.evidence.missing.length).toBeGreaterThan(3);
    expect(study.stopCondition).toContain("bounded case");
  });
});
