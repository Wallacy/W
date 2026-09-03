import fs from "node:fs";
import path from "node:path";
import { describe, expect, setDefaultTimeout, test } from "bun:test";
import { deriveSyn1Case, validateSyn1 } from "../../syn1-typed-generation-machine.mjs";
import { validateSyn1StudyManifest } from "../../syn1-typed-generation-manifest.mjs";

const studyDirectory = import.meta.dir;
const toolingDirectory = path.resolve(studyDirectory, "../..");
const root = path.resolve(toolingDirectory, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "syn1-typed-generation-cases.json"), "utf8"));
const manifest = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const byId = new Map(corpus.cases.map((item) => [item.id, item]));
const derive = (id) => deriveSyn1Case(byId.get(id), corpus, { root });
const errors = (candidate) => validateSyn1StudyManifest(candidate, { studyDirectory });
setDefaultTimeout(120000);

describe("SYN1 generated W module study oracle", () => {
  test("validates schema-3 corpus and exact study evidence", () => {
    expect(validateSyn1(corpus, { root }).errors).toEqual([]);
    expect(errors(manifest)).toEqual([]);
    expect(manifest.generatedArtifacts).toHaveLength(14);
    expect(manifest.evidence.current).toContain("target-registry-host");
    expect(manifest.evidence.missing).toContain("target-compiler-provider");
  });

  test("keeps route separate from status and publication evidence", () => {
    expect(derive("current-menu-data-artifact")).toMatchObject({ status: "accepted", route: "composable", actionResultPublished: false });
    expect(derive("restaurant-final-menu-generated-module")).toMatchObject({ status: "accepted", route: "historical-candidate", actionResultPublished: true, interfacePublished: true, compilerCachePublished: false });
    expect(derive("failure-discards-staging")).toMatchObject({ status: "discarded", route: "historical-candidate", actionResultPublished: false });
    expect(derive("proc-macro-rejected")).toMatchObject({ status: "rejected", route: "intentionally-rejected" });
  });

  test("rejects forged evidence roles, languages, refs, hosts, and oracle paths", () => {
    const role = structuredClone(manifest); role.variants[4].role = "fixture-current";
    expect(errors(role).some((error) => error.includes("exact fixture/candidate/rejected"))).toBe(true);
    const language = structuredClone(manifest); language.variants[4].language = "w-fixture";
    expect(errors(language).some((error) => error.includes("language must be"))).toBe(true);
    const changedRef = structuredClone(manifest); changedRef.sourceRefs[0].symbol = "forged";
    expect(errors(changedRef).some((error) => error.includes("normalized corpus source reference"))).toBe(true);
    const extraRef = structuredClone(manifest); extraRef.sourceRefs.push(structuredClone(extraRef.sourceRefs[0]));
    expect(errors(extraRef).some((error) => error.includes("equal the corpus"))).toBe(true);
    const host = structuredClone(manifest); host.officialRefs[0].url = "https://example.com/c23";
    expect(errors(host).some((error) => error.includes("allowlisted primary host") || error.includes("normalized corpus"))).toBe(true);
    const oracle = structuredClone(manifest); oracle.oracle.path = "oracle.test.mjs";
    expect(errors(oracle).some((error) => error.includes("exact independent SYN1 reference test"))).toBe(true);
  });

  test("rejects stale artifact and target-registry receipts", () => {
    const artifact = structuredClone(manifest); artifact.generatedArtifacts[0].digest = `sha256:${"0".repeat(64)}`;
    expect(errors(artifact).some((error) => error.includes("generatedArtifacts[0].digest is stale"))).toBe(true);
    const registry = structuredClone(manifest); registry.targetRegistry.digest = `sha256:${"0".repeat(64)}`;
    expect(errors(registry).some((error) => error.includes("targetRegistry.digest is stale"))).toBe(true);
  });
});
