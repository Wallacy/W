import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { fileURLToPath } from "node:url";
import { loadCapabilityMatrix, validateCapabilityMatrix } from "./capability-matrix-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryDirectory = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "capability-matrix-cases.json");

function readCorpus() {
  return JSON.parse(fs.readFileSync(corpusPath, "utf8"));
}

function check(corpus) {
  return validateCapabilityMatrix(corpus, { root: repositoryDirectory, checkSources: true }).errors;
}

describe("CAP0 capability matrix host oracle", () => {
  test("the current corpus validates and derives all eight routes", () => {
    const result = validateCapabilityMatrix(readCorpus(), { root: repositoryDirectory, checkSources: true });
    expect(result.errors).toEqual([]);
    expect(result.results.map((axis) => axis.derivedRoute)).toEqual([
      "research", "composable", "composable", "composable", "composable", "research", "current", "composable",
    ]);
  });

  test("a forged route is rejected because route is derived from structured coverage", () => {
    const corpus = readCorpus();
    corpus.axes.find((axis) => axis.id === "SRV0").route.classification = "research";
    expect(check(corpus).some((error) => error.includes("route.classification must derive as current"))).toBe(true);
  });

  test("changing coverage facts defeats a forged route and cannot be echoed by a route field", () => {
    const corpus = readCorpus();
    const dynamic = corpus.axes.find((axis) => axis.id === "DYN0");
    dynamic.coverage.subcapabilities[0].classification = "research";
    dynamic.route.classification = "composable";
    expect(check(corpus).some((error) => error.includes("route.classification must derive as research"))).toBe(true);
  });

  test("route-like legacy facts and mixed documentation targets are rejected", () => {
    const corpus = readCorpus();
    const borrowed = corpus.axes.find((axis) => axis.id === "BRX0");
    borrowed.decisionFacts = { current: true, composition: "full", gapKind: "none", invariantConflict: false };
    corpus.axes.find((axis) => axis.id === "CYC0").documentation.documentationTarget = borrowed.documentation.documentationTarget;
    corpus.axes.find((axis) => axis.id === "DYN0").documentation.documentationTarget = "guides/problems/problem-guide";
    const errors = check(corpus);
    expect(errors.some((error) => error.includes("decisionFacts is forbidden"))).toBe(true);
    expect(errors.some((error) => error.includes("documentationTarget duplicates"))).toBe(true);
    expect(errors.some((error) => error.includes("specific non-generic problem slug"))).toBe(true);
  });

  test("maturity and feature-copying fields are rejected", () => {
    const corpus = readCorpus();
    corpus.axes[0].maturity = "high";
    corpus.axes[1].foreignMechanisms.c[0].featureCopying = true;
    const errors = check(corpus);
    expect(errors.some((error) => error.includes("forbidden maturity"))).toBe(true);
    expect(errors.some((error) => error.includes("featureCopying"))).toBe(true);
  });

  test("missing, stale, and duplicate source refs are rejected", () => {
    const corpus = readCorpus();
    corpus.axes[0].lastLight.sourceRefs[0].path = "reference/last-light/missing.w";
    corpus.axes[1].lastLight.sourceRefs[0].symbol = "missingAtomicSymbol";
    corpus.axes[1].lastLight.sourceRefs[0].digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    corpus.axes[2].lastLight.sourceRefs.push({ ...corpus.axes[2].lastLight.sourceRefs[0] });
    const errors = check(corpus);
    expect(errors.some((error) => error.includes("references a missing file"))).toBe(true);
    expect(errors.some((error) => error.includes("symbol is absent"))).toBe(true);
    expect(errors.some((error) => error.includes("digest is stale"))).toBe(true);
    expect(errors.some((error) => error.includes("duplicates source reference"))).toBe(true);
  });

  test("ATOM1 study refs require durable paths, digests, claims, and no duplicates", () => {
    const corpus = readCorpus();
    const studyRefs = corpus.axes.find((axis) => axis.id === "ATOM0").nextStudyGate.studyRefs;
    studyRefs[0].digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    studyRefs[1].path = "tooling/studies/atom1-atomic-extensibility/missing.md";
    studyRefs.push({ ...studyRefs[2] });
    const errors = check(corpus);
    expect(errors.some((error) => error.includes("nextStudyGate.studyRefs[0].digest is stale"))).toBe(true);
    expect(errors.some((error) => error.includes("nextStudyGate.studyRefs[1].path references a missing file"))).toBe(true);
    expect(errors.some((error) => error.includes("duplicates study reference"))).toBe(true);
  });

  test("canonical and W teaching refs require a fresh digest and unique symbol", () => {
    const corpus = readCorpus();
    corpus.axes[0].lastLight.canonicalSource.digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    corpus.axes[1].documentation.wExample.sourceRefs[0].symbol = "HorizonTelemetryEpoch";
    const errors = check(corpus);
    expect(errors.some((error) => error.includes("canonicalSource.digest is stale"))).toBe(true);
    expect(errors.some((error) => error.includes("wExample.sourceRefs[0].symbol must occur exactly once"))).toBe(true);
  });

  test("subcapability coverage must reference real components or the blocking gate", () => {
    const corpus = readCorpus();
    const atom = corpus.axes.find((axis) => axis.id === "ATOM0");
    atom.coverage.subcapabilities[0].componentRefs[0] = "forged-component";
    const borrowed = corpus.axes.find((axis) => axis.id === "BRX0");
    borrowed.coverage.subcapabilities[0].gateId = "forged-gate";
    borrowed.coverage.subcapabilities[0].evidenceRefs = ["forged-symbol"];
    const errors = check(corpus);
    expect(errors.some((error) => error.includes("componentRefs must reference real"))).toBe(true);
    expect(errors.some((error) => error.includes("gateId must equal nextStudyGate.gateId"))).toBe(true);
    expect(errors.some((error) => error.includes("evidenceRefs must reference blocking Last Light"))).toBe(true);
  });

  test("capability level rationales cannot classify foreign extensions", () => {
    const corpus = readCorpus();
    corpus.axes.find((axis) => axis.id === "DYN0").capabilityLevels.languageDesign.rationale = "foreign mechanism is rejected";
    expect(check(corpus).some((error) => error.includes("rationale must not classify an extension or foreign mechanism"))).toBe(true);
  });

  test("preserve-strength invariants are mandatory for every axis", () => {
    const corpus = readCorpus();
    corpus.axes.find((axis) => axis.id === "GEN0").preserveStrengths = ["typed effects"];
    const errors = check(corpus);
    expect(errors.some((error) => error.includes("preserveStrengths must contain at least three"))).toBe(true);
    expect(errors.some((error) => error.includes("preserveStrengths must retain"))).toBe(true);
  });

  test("Research subcapabilities require an exact design gate", () => {
    const corpus = readCorpus();
    const atom = corpus.axes.find((axis) => axis.id === "ATOM0");
    delete atom.coverage.subcapabilities.find((subcapability) => subcapability.id === "ATOM0-new-primitive").gateId;
    const gen = corpus.axes.find((axis) => axis.id === "GEN0");
    gen.nextStudyGate.forSubcapability = "GEN0-incremental-production";
    const errors = check(corpus);
    expect(errors.some((error) => error.includes("Research subcapabilities must point to the design gate"))).toBe(true);
  });

  test("an evidence gate cannot pretend to be a design subcapability gate", () => {
    const corpus = readCorpus();
    const dynamic = corpus.axes.find((axis) => axis.id === "DYN0");
    dynamic.nextStudyGate.kind = "design";
    dynamic.nextStudyGate.forSubcapability = "DYN0-arbitrary-eval";
    expect(check(corpus).some((error) => error.includes("nextStudyGate.kind design has no Research subcapability focus"))).toBe(true);
  });

  test("documentation queue is mandatory for every axis", () => {
    const corpus = readCorpus();
    delete corpus.axes[7].documentation;
    expect(check(corpus).some((error) => error.includes("documentation is required"))).toBe(true);
  });

  test("current and composable routes require a source-backed W teaching example", () => {
    const corpus = readCorpus();
    delete corpus.axes.find((axis) => axis.id === "CYC0").documentation.wExample.sourceRefs;
    expect(check(corpus).some((error) => error.includes("source-backed for composable routes"))).toBe(true);
  });

  test("foreign examples require bounded original pseudocode and W examples stay source references", () => {
    const corpus = readCorpus();
    const borrowed = corpus.axes.find((axis) => axis.id === "BRX0");
    delete borrowed.documentation.foreignExamples.c.snippet;
    borrowed.documentation.foreignExamples.rust.snippet = Array.from({ length: 7 }, (_, index) => `line ${index + 1}`).join("\n");
    borrowed.documentation.foreignExamples.python.snippet = borrowed.documentation.foreignExamples.python.text;
    borrowed.documentation.wExample.snippet = "def drift():\n    pass";
    const errors = check(corpus);
    expect(errors.some((error) => error.includes("foreignExamples.c.snippet must be non-empty"))).toBe(true);
    expect(errors.some((error) => error.includes("foreignExamples.rust.snippet must contain at most 6 lines"))).toBe(true);
    expect(errors.some((error) => error.includes("foreignExamples.python.snippet must not duplicate prose text"))).toBe(true);
    expect(errors.some((error) => error.includes("wExample must not contain snippet"))).toBe(true);
  });

  test("each axis records a global simplification target", () => {
    const corpus = readCorpus();
    delete corpus.axes.find((axis) => axis.id === "IPC0").globalSimplification;
    expect(check(corpus).some((error) => error.includes("globalSimplification must be non-empty"))).toBe(true);
  });

  test("dynamic eval rejection is separate from the composable problem route", () => {
    const corpus = readCorpus();
    const dynamic = corpus.axes.find((axis) => axis.id === "DYN0");
    expect(dynamic.route.classification).toBe("composable");
    expect(dynamic.foreignMechanismDisposition.classification).toBe("intentionally-rejected");
    expect(dynamic.exactGap.kind).toBe("none");
  });
});
