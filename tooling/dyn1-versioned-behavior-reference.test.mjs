import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { fileURLToPath } from "node:url";
import { canonicalDigest, deriveCompatibilityMapDigest, evaluateDyn1Case, normaliseFacts, validateDyn1 } from "./dyn1-versioned-behavior-machine.mjs";
import { digestFile, validateDyn1StudyManifest } from "./dyn1-versioned-behavior-manifest.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "dyn1-versioned-behavior-cases.json");
const studyDirectory = path.join(toolingDirectory, "studies", "dyn1-versioned-behavior");

function readCorpus() {
  return JSON.parse(fs.readFileSync(corpusPath, "utf8"));
}

function readStudy() {
  return JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
}

function byId(corpus, id) {
  return corpus.cases.find((testCase) => testCase.id === id);
}

describe("DYN1 versioned behavior host oracle", () => {
  test("validates the corpus and manifest as evidence-only", () => {
    const corpus = readCorpus();
    expect(validateDyn1(corpus, { root: repositoryRoot }).errors).toEqual([]);
    expect(validateDyn1StudyManifest(readStudy(), { studyDirectory, repositoryRoot })).toEqual([]);
    expect(corpus.cases.length).toBeGreaterThanOrEqual(45);
    expect(readStudy().languageDesign).toBe("partial");
  });

  test("derives REPL snapshot, invalidation, export, and stale generation behavior", () => {
    const corpus = readCorpus();
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-repl-snapshot"), { corpus })).toMatchObject({ status: "committed", route: "composable", generation: "g2" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-repl-invalidation"), { corpus }).staleRejections).toEqual(["oldCompletion"]);
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-repl-export-import"), { corpus })).toMatchObject({ status: "committed", imported: true });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-import-live-state-rejected"), { corpus }).code).toBe("import-live-state");
  });

  test("keeps preparation failures, rollback, degraded drain, and unknown crash separate", () => {
    const corpus = readCorpus();
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-repl-invalid-preserves-old"), { corpus })).toMatchObject({ status: "rejected", generation: "g1" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-provider-rollback-receipt"), { corpus })).toMatchObject({ status: "rolled-back", generation: "g1" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-post-switch-drain-degraded"), { corpus })).toMatchObject({ status: "degraded", generation: "g2" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-crash-after-publication"), { corpus })).toMatchObject({ status: "committed", code: "post-publication-crash", generation: "g2" });
  });

  test("compares independent local and split reducers", () => {
    const corpus = readCorpus();
    const paired = evaluateDyn1Case(byId(corpus, "DYN1-B-paired-local-split"), { corpus });
    expect(paired).toMatchObject({ status: "committed", mode: "paired" });
    expect(paired.projection.local).toEqual(paired.projection.split);
    expect(paired.physicalTrace.local).not.toEqual(paired.physicalTrace.split);
    const mutation = structuredClone(byId(corpus, "DYN1-B-paired-local-split"));
    mutation.projections.split = ["@commit", { op: "oldCompletion", generation: "g1" }];
    expect(evaluateDyn1Case(mutation, { corpus }).code).toBe("projection-divergence");
  });

  test("audits identity, capability, effect, FFI, and isolation boundaries", () => {
    const corpus = readCorpus();
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-interface-exact-drift"), { corpus }).code).toBe("schema-exact-mismatch");
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-wabi-mismatch"), { corpus }).code).toBe("wabi-mismatch");
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-hidden-capability"), { corpus }).code).toBe("capability-grant-mismatch");
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-effect-undeclared"), { corpus }).code).toBe("effect-capability-mismatch");
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-unload-live-callback"), { corpus }).code).toBe("unload-live-callback");
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-native-exact-mapping-retained"), { corpus })).toMatchObject({ status: "committed", ffiRelease: "native-release-mapping-pinned" });
    const targetB = byId(corpus, "DYN1-B-target-specific-abi");
    const projectedB = normaliseFacts(corpus, targetB);
    expect(projectedB.interface.wAbiKey).toBe(corpus.defaults.targets.B.wAbiReceipt.wAbiKey);
    expect(projectedB.interface.old.wAbiKey).toBe(corpus.defaults.targets.B.wAbiReceipt.wAbiKey);
    expect(projectedB.interface.candidate.wAbiKey).toBe(corpus.defaults.targets.B.wAbiReceipt.wAbiKey);
    expect(projectedB.interface.wAbiReceipt.wAbiKey).toBe(corpus.defaults.targets.B.wAbiReceipt.wAbiKey);
  });

  test("keeps the persistent generation reference narrow and rejects route D", () => {
    const corpus = readCorpus();
    expect(evaluateDyn1Case(byId(corpus, "DYN1-C-persistent-generation-reference"), { corpus })).toMatchObject({ status: "historical-candidate", route: "historical-candidate", migration: true });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-C-persistent-reference-write-rejected"), { corpus })).toMatchObject({ status: "rejected", route: "historical-candidate", code: "migration-live-state" });
    for (const testCase of corpus.cases.filter((item) => item.axis === "D" && item.family === "rejected")) {
      expect(evaluateDyn1Case(testCase, { corpus })).toMatchObject({ status: "intentionally-rejected", route: "intentionally-rejected" });
    }
    expect(evaluateDyn1Case(byId(corpus, "DYN1-D-forged-mechanism-invalid"), { corpus })).toMatchObject({ status: "invalid-assay", route: "invalid-assay", code: "mechanism-family-mismatch" });
  });

  test("rejects fallback identities and partial fact mutations before reduction", () => {
    const corpus = readCorpus();
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-repl-snapshot"))).toMatchObject({ status: "invalid-assay", code: "missing-corpus-defaults" });
    const missing = structuredClone(corpus);
    delete missing.defaults.interface.sourceMapKey;
    const validation = validateDyn1(missing, { root: repositoryRoot });
    expect(validation.errors.some((error) => error.includes("defaults.interface.sourceMapKey"))).toBe(true);
    const partial = structuredClone(byId(corpus, "DYN1-A-schema-add-optional"));
    partial.facts.schema = { policy: "compatible" };
    expect(validateDyn1({ ...corpus, cases: [partial, ...corpus.cases.slice(1)] }, { root: repositoryRoot }).errors.some((error) => error.includes("complete mutation"))).toBe(true);
  });

  test("derives structured schema, WAbi, runtime, capability, and inspector decisions", () => {
    const corpus = readCorpus();
    expect(corpus.defaults.interface.wAbiReceipt).toHaveProperty("wAbiKey");
    expect(corpus.defaults.schema).not.toHaveProperty("change");
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-schema-exact-mismatch"), { corpus })).toMatchObject({ status: "rejected", code: "schema-exact-mismatch" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-schema-add-optional"), { corpus })).toMatchObject({ status: "committed", interfaceResult: "compatible" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-wabi-mismatch"), { corpus })).toMatchObject({ status: "rejected", code: "wabi-mismatch" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-runtime-closure-new-generation"), { corpus })).toMatchObject({ status: "committed", generation: "g2" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-committed-inspector"), { corpus })).toMatchObject({ status: "committed", inspected: true });
    const duplicateAuthority = structuredClone(byId(corpus, "DYN1-B-runtime-closure-new-generation"));
    duplicateAuthority.facts.interface.schemaDigest = "sha256:" + "a".repeat(64);
    expect(evaluateDyn1Case(duplicateAuthority, { corpus }).code).toBe("interface-authority-duplicate");
  });

  test("keeps terminal failure, rollback, cleanup, callback, and crash boundaries strict", () => {
    const corpus = readCorpus();
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-phase-duplicate-switch-rejected"), { corpus })).toMatchObject({ status: "rejected", code: "phase-order" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-rollback-forged-receipt"), { corpus })).toMatchObject({ status: "rejected", code: "rollback-receipt-invalid" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-rollback-after-switch-rejected"), { corpus })).toMatchObject({ status: "rejected", code: "rollback-after-publication" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-crash-before-publication"), { corpus })).toMatchObject({ status: "fault-boundary", generation: "g1" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-crash-before-publication-unknown-effect"), { corpus })).toMatchObject({ status: "unknown-effect", generation: "g1" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-late-callback-rejected"), { corpus })).toMatchObject({ status: "rejected", code: "late-callback" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-unload-after-drain"), { corpus })).toMatchObject({ status: "committed", ffiRelease: "isolated-stop-after-drain" });
  });

  test("uses recursive canonical receipts and a replay-only import protocol", () => {
    const corpus = readCorpus();
    expect(canonicalDigest({ receipt: { id: "r1", digest: "sha256:a" } })).not.toBe(canonicalDigest({ receipt: { id: "r2", digest: "sha256:a" } }));
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-repl-export-import"), { corpus })).toMatchObject({ imported: true });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-import-parse-missing"), { corpus })).toMatchObject({ status: "rejected", code: "import-parse-missing" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-A-export-forged-receipt"), { corpus })).toMatchObject({ status: "rejected", code: "export-receipt-forged" });
  });

  test("keeps historical candidate provenance explicit, target projections distinct, and selection atomic", () => {
    const corpus = readCorpus();
    expect(corpus.defaults.targets.A.registryDigest).not.toBe(corpus.defaults.targets.B.registryDigest);
    expect(corpus.defaults.targets.A.physicalArtifactDigest).not.toBe(corpus.defaults.targets.B.physicalArtifactDigest);
    expect(corpus.defaults.targets.A.wAbiReceipt.wAbiKey).not.toBe(corpus.defaults.targets.B.wAbiReceipt.wAbiKey);
    const paired = evaluateDyn1Case(byId(corpus, "DYN1-B-target-pair-equivalence"), { corpus });
    expect(paired).toMatchObject({ status: "committed", mode: "paired" });
    expect(paired.projection.local.ownerGraph).toEqual(paired.projection.split.ownerGraph);
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-concurrent-selection"), { corpus })).toMatchObject({ selection: "g2" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-B-selection-duplicate-rejected"), { corpus })).toMatchObject({ status: "rejected", code: "concurrent-selection" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-C-persistent-generation-reference"), { corpus })).toMatchObject({ status: "historical-candidate", route: "historical-candidate" });
    expect(evaluateDyn1Case(byId(corpus, "DYN1-C-persistent-reference-write-rejected"), { corpus })).toMatchObject({ status: "rejected", route: "historical-candidate", code: "migration-live-state" });
    const forgedSelection = structuredClone(byId(corpus, "DYN1-B-concurrent-selection"));
    forgedSelection.events[1].selectionReceipt.receiptDigest = "sha256:" + "f".repeat(64);
    expect(evaluateDyn1Case(forgedSelection, { corpus }).code).toBe("concurrent-selection");
  });

  test("rejects a route-D attempt outside the closed registry and a non-D attempt", () => {
    const corpus = readCorpus();
    const forged = structuredClone(byId(corpus, "DYN1-D-eval-rejected"));
    forged.events[0].mechanism.id = "not-registered";
    expect(evaluateDyn1Case(forged, { corpus })).toMatchObject({ status: "invalid-assay", code: "unknown-mechanism" });
    const nonD = { id: "non-d-attempt", axis: "A", events: [{ op: "attempt", mechanism: { id: "eval", family: "dynamic-evaluation" }, invariant: "no-arbitrary-code" }] };
    expect(evaluateDyn1Case(nonD, { corpus })).toMatchObject({ status: "rejected", code: "attempt-route-mismatch" });
  });

  test("does not let expected output select the reducer result", () => {
    const corpus = readCorpus();
    const original = byId(corpus, "DYN1-A-repl-snapshot");
    const mutated = structuredClone(original);
    mutated.expect.status = "rejected";
    expect(evaluateDyn1Case(mutated, { corpus }).status).toBe("committed");
    expect(digestFile(corpusPath)).toMatch(/^sha256:[0-9a-f]{64}$/u);
  });

  test("closes nested facts and receipt authorities", () => {
    const corpus = readCorpus();
    const nested = structuredClone(corpus);
    nested.defaults.schema.old.fields[0].forged = true;
    expect(validateDyn1(nested, { root: repositoryRoot }).errors.some((error) => error.includes("schema.old.fields[0].forged"))).toBe(true);
    const effect = structuredClone(byId(corpus, "DYN1-B-local-plugin-generation"));
    delete effect.events[1].providerReceipt;
    expect(evaluateDyn1Case(effect, { corpus }).code).toBe("effect-capability-mismatch");
    const d = structuredClone(byId(corpus, "DYN1-D-eval-rejected"));
    d.events[0].invariant = "wrong-invariant";
    expect(evaluateDyn1Case(d, { corpus }).code).toBe("mechanism-invariant-mismatch");
    const staleCompatible = structuredClone(byId(corpus, "DYN1-A-schema-add-optional"));
    staleCompatible.facts.interface.semanticInterfaceKey = staleCompatible.facts.interface.old.semanticInterfaceKey;
    staleCompatible.facts.interface.candidate.semanticInterfaceKey = staleCompatible.facts.interface.old.semanticInterfaceKey;
    staleCompatible.facts.interface.serviceIRKey = staleCompatible.facts.interface.old.serviceIRKey;
    staleCompatible.facts.interface.candidate.serviceIRKey = staleCompatible.facts.interface.old.serviceIRKey;
    expect(evaluateDyn1Case(staleCompatible, { corpus }).code).toBe("compatible-identity-stale");
    const forgedService = structuredClone(byId(corpus, "DYN1-B-interface-compatible-drift"));
    forgedService.facts.interface.serviceIRReceipt.candidateServiceIRKey = forgedService.facts.interface.old.serviceIRKey;
    expect(evaluateDyn1Case(forgedService, { corpus }).code).toBe("serviceir-receipt-mismatch");
    const compatible = byId(corpus, "DYN1-B-interface-compatible-drift");
    expect(compatible.facts.interface.serviceIRReceipt.compatibilityMapDigest).toBe(deriveCompatibilityMapDigest(compatible.facts.schema.old, compatible.facts.schema.candidate));
    const forgedMap = structuredClone(compatible);
    forgedMap.facts.interface.serviceIRReceipt.compatibilityMapDigest = `sha256:${"0".repeat(64)}`;
    expect(evaluateDyn1Case(forgedMap, { corpus }).code).toBe("serviceir-receipt-mismatch");
    const missingMap = structuredClone(compatible);
    missingMap.facts.interface.serviceIRReceipt.compatibilityMapDigest = null;
    expect(evaluateDyn1Case(missingMap, { corpus }).code).toBe("serviceir-receipt-mismatch");
    expect(corpus.defaults.interface.serviceIRReceipt.compatibilityMapDigest).toBeNull();
  });

  test("host-only reducer mutations prove local and split operational independence", () => {
    const corpus = readCorpus();
    const paired = byId(corpus, "DYN1-B-paired-local-split");
    const commitPair = structuredClone(paired);
    const selection = byId(corpus, "DYN1-B-concurrent-selection");
    const selectionPair = structuredClone(paired);
    selectionPair.projections = { local: selection.events, split: selection.events };
    const crash = byId(corpus, "DYN1-A-crash-after-publication");
    const crashPair = structuredClone(paired);
    crashPair.projections = { local: crash.events, split: crash.events };
    for (const mutation of ["local-switch", "split-switch", "local-cleanup", "split-cleanup", "local-callback", "split-callback"]) {
      expect(evaluateDyn1Case(commitPair, { corpus, mutate: mutation }).code).toBe("projection-divergence");
    }
    for (const mutation of ["local-selection", "split-selection"]) {
      expect(evaluateDyn1Case(selectionPair, { corpus, mutate: mutation }).code).toBe("projection-divergence");
    }
    for (const mutation of ["local-crash", "split-crash"]) {
      expect(evaluateDyn1Case(crashPair, { corpus, mutate: mutation }).code).toBe("projection-divergence");
    }
  });

  test("rejects forged manifest roles, digests, and official hosts", () => {
    const study = readStudy();
    const role = structuredClone(study);
    role.variants[4].role = "bogus";
    expect(validateDyn1StudyManifest(role, { studyDirectory, repositoryRoot }).some((error) => error.includes("role is invalid"))).toBe(true);
    const digest = structuredClone(study);
    digest.variants[0].digest = `sha256:${"0".repeat(64)}`;
    expect(validateDyn1StudyManifest(digest, { studyDirectory, repositoryRoot }).some((error) => error.includes("digest is stale"))).toBe(true);
    const host = structuredClone(study);
    host.officialRefs[0].url = "https://example.com/forged";
    expect(validateDyn1StudyManifest(host, { studyDirectory, repositoryRoot }).some((error) => error.includes("allowlisted official HTTPS host"))).toBe(true);
    const cap = structuredClone(study);
    cap.composition.capabilityMatrix.classification = "research";
    expect(validateDyn1StudyManifest(cap, { studyDirectory, repositoryRoot }).some((error) => error.includes("CAP0 DYN0-versioned-change composable gate"))).toBe(true);
  });
});
