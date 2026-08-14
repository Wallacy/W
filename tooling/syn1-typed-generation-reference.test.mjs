import fs from "node:fs";
import path from "node:path";
import { describe, expect, setDefaultTimeout, test } from "bun:test";
import {
  deriveSyn1Case,
  digestValue,
  extractWSourceShape,
  PHASES,
  validateSyn1,
} from "./syn1-typed-generation-machine.mjs";

const root = path.resolve(import.meta.dir, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "syn1-typed-generation-cases.json"), "utf8"));
const byId = new Map(corpus.cases.map((testCase) => [testCase.id, testCase]));
const derive = (id, source = corpus) => deriveSyn1Case(source.cases.find((item) => item.id === id), source, { root });
const clone = (id) => structuredClone(byId.get(id));
setDefaultTimeout(120000);

describe("SYN1 schema-3 F2 generated-module oracle", () => {
  test("validates the 65-case problem-first corpus", () => {
    const checked = validateSyn1(corpus, { root });
    expect(checked.errors).toEqual([]);
    expect(checked.results).toHaveLength(65);
    expect(Object.fromEntries(["A", "B", "C", "D"].map((axis) => [axis, checked.results.filter((item) => item.axis === axis).length]))).toEqual({ A: 4, B: 1, C: 53, D: 7 });
  });

  test("scanner masks comments and strings and ignores nested declarations", () => {
    const shape = extractWSourceShape(`// export struct Commented {}\nexport const text = \"export struct Stringy {}\"\nexport struct Outer {\n  fn nested() {}\n}\n`);
    expect(shape.symbols.map(({ name, kind }) => ({ name, kind }))).toEqual([
      { name: "text", kind: "const" },
      { name: "Outer", kind: "struct" },
    ]);
    expect(shape.rejectedTopLevel).toEqual([]);
    expect(derive("comment-string-nested-shape-ignored").code).toBe("generated-module-reopened");
    expect(derive("multiline-generated-signature")).toMatchObject({ status: "accepted", code: "generated-module-reopened" });
  });

  test("runs the real parser for byte overrides", () => {
    expect(derive("valid-byte-override-parsed")).toMatchObject({ status: "accepted", observedTrace: ["declared-input", "tool-action", "staged-output", "parse"] });
    expect(derive("syntax-failure-preserves-action-result")).toMatchObject({ code: "generated-syntax", actionResultPublished: true, interfacePublished: false });
    expect(derive("utf8-failure-preserves-action-result")).toMatchObject({ code: "generated-utf8", actionResultPublished: true, interfacePublished: false });
  });

  test("treats module files as a canonical set and separates physical relocation", () => {
    const baseCase = clone("multi-file-generated-module");
    const base = deriveSyn1Case(baseCase, corpus, { root });
    const reordered = structuredClone(baseCase);
    reordered.input.output.moduleSet.reverse();
    const reorderedResult = deriveSyn1Case(reordered, corpus, { root });
    expect(reorderedResult.variantSetDigest).toBe(base.variantSetDigest);
    expect(reorderedResult.targetVariants[0].moduleArtifactIdentity).toBe(base.targetVariants[0].moduleArtifactIdentity);
    expect(derive("fixture-relocation-same-identity").targetVariants[0].moduleArtifactIdentity).toBe(derive("restaurant-final-menu-generated-module").targetVariants[0].moduleArtifactIdentity);
    expect(derive("cross-file-collision").code).toBe("declaration-collision");
  });

  test("allows many generated spans to one source span but requires unique generated coverage and UTF-8 boundaries", () => {
    expect(derive("many-generated-to-one-source")).toMatchObject({ status: "accepted", code: "generated-module-reopened" });
    expect(derive("exact-second-mapping-fix").targetVariants[0].sourceMap.fix.span).toEqual(byId.get("exact-second-mapping-fix").input.output.sourceMaps[0].mappings[1].sourceSpan);
    expect(derive("ambiguous-generated-map").code).toBe("source-map-invalid");
    expect(derive("unicode-byte-map-valid")).toMatchObject({ status: "accepted", sourceMapFixable: true });
    expect(derive("unicode-mid-byte-rejected").code).toBe("source-map-invalid");
  });

  test("uses durable target facts and derives WAbi independently from physical artifacts", () => {
    const neutralCase = clone("target-neutral-two-projections");
    const neutral = deriveSyn1Case(neutralCase, corpus, { root });
    expect(neutral.targetEquivalent).toBe(true);
    expect(new Set(neutral.targetVariants.map((item) => item.wAbiKey)).size).toBe(2);
    const specialized = derive("target-specific-variants");
    expect(specialized.targetVariants.every((item) => item.actionIdentity.productTargetFact.abiFacts && item.actionIdentity.productTargetFact.registryRevision)).toBe(true);
    const artifactRename = structuredClone(neutralCase);
    artifactRename.input.generator.targetMetadata.targets[0].physicalArtifact = "renamed-physical-product";
    const renamed = deriveSyn1Case(artifactRename, corpus, { root });
    expect(renamed.targetVariants.map((item) => item.wAbiKey)).toEqual(neutral.targetVariants.map((item) => item.wAbiKey));
    const targetOrder = structuredClone(neutralCase);
    targetOrder.input.generator.targetMetadata.targets.reverse();
    expect(deriveSyn1Case(targetOrder, corpus, { root }).variantSetDigest).toBe(neutral.variantSetDigest);
    expect(derive("forged-target-facts").code).toBe("invalid-target-metadata");
    expect(derive("forged-target-registry").code).toBe("invalid-target-metadata");
  });

  test("resolves interface baselines from accepted root cases", () => {
    for (const id of ["field-change-recompiles-consumer", "enum-change-recompiles-consumer", "effect-drift-interface", "ownership-drift-interface", "const-drift-interface", "conformance-drift-interface"]) expect(derive(id).interfaceChanged).toBe(true);
    expect(derive("docs-only-no-semantic-change").interfaceChanged).toBe(false);
    expect(derive("private-body-no-interface-change").interfaceChanged).toBe(false);
    for (const id of ["raw-baseline-rejected", "self-baseline-rejected", "missing-baseline-rejected"]) expect(derive(id).code).toBe("invalid-baseline-ref");
  });

  test("separates action-result publication from candidate interface publication", () => {
    expect(derive("failure-discards-staging")).toMatchObject({ status: "discarded", actionResultPublished: false, interfacePublished: false, compilerCachePublished: false });
    expect(derive("syntax-failure-preserves-action-result")).toMatchObject({ actionResultPublished: true, interfacePublished: false, compilerCachePublished: false });
    expect(derive("stale-receipt")).toMatchObject({ actionResultPublished: true, interfacePublished: false, compilerCachePublished: false });
    expect(derive("restaurant-final-menu-generated-module")).toMatchObject({ actionResultPublished: true, interfacePublished: true, compilerCachePublished: false, requiredCompilerPublication: true, requiredPhaseTrace: PHASES });
  });

  test("requires the exact current evidence set", () => {
    expect(derive("current-menu-data-artifact")).toMatchObject({ status: "accepted", route: "composable", actionResultPublished: false });
    const missing = structuredClone(corpus);
    missing.sourceRefs.pop();
    expect(validateSyn1(missing, { root }).errors.some((error) => error.includes("closed SYN1") || error.includes("missing last-light-final-menu"))).toBe(true);
    const renamed = structuredClone(corpus);
    renamed.sourceRefs[0].id = "renamed-ref";
    expect(validateSyn1(renamed, { root }).errors.some((error) => error.includes("exact SYN1") || error.includes("missing last-light-reflection"))).toBe(true);
    const extra = structuredClone(corpus);
    extra.sourceRefs.push(structuredClone(extra.sourceRefs[0]));
    expect(validateSyn1(extra, { root }).errors.some((error) => error.includes("closed SYN1"))).toBe(true);
  });

  test("excludes physical source paths from logical identities", () => {
    const base = derive("restaurant-final-menu-generated-module");
    const relocated = derive("source-relocation-same-identities");
    expect(relocated.actionRecipeKeys).toEqual(base.actionRecipeKeys);
    expect(relocated.targetVariants[0].moduleArtifactIdentity).toBe(base.targetVariants[0].moduleArtifactIdentity);
    expect(relocated.targetVariants[0].semanticInterfaceKey).toBe(base.targetVariants[0].semanticInterfaceKey);
    expect(relocated.targetVariants[0].diagnosticMapKey).toBe(base.targetVariants[0].diagnosticMapKey);
    expect(relocated.explainNavigation.provenanceDisplayPath).not.toBe(base.explainNavigation.provenanceDisplayPath);
  });

  test("binds action graph and closed output descriptor into action identity", () => {
    expect(derive("forged-graph-digest").code).toBe("invalid-action-recipe");
    expect(derive("missing-produces-edge").code).toBe("invalid-action-recipe");
    expect(derive("tool-output-cycle").code).toBe("invalid-action-recipe");
    expect(derive("self-import-rejected").code).toBe("dependency-mismatch");
    const baseCase = clone("restaurant-final-menu-generated-module");
    const base = deriveSyn1Case(baseCase, corpus, { root });
    const bounded = structuredClone(baseCase);
    bounded.input.generator.outputDescriptor.maximumModuleBytes += 1;
    const boundedResult = deriveSyn1Case(bounded, corpus, { root });
    expect(boundedResult.status).toBe("accepted");
    expect(boundedResult.actionRecipeKeys).not.toEqual(base.actionRecipeKeys);
    const invalid = structuredClone(baseCase);
    invalid.input.generator.outputDescriptor.binding = "undeclaredOutput";
    expect(deriveSyn1Case(invalid, corpus, { root }).code).toBe("invalid-output-descriptor");
    const consumerCycle = structuredClone(baseCase);
    const tool = consumerCycle.input.generator.toolArtifact;
    consumerCycle.input.generator.actionGraphReceipt.edges.push({ from: tool, to: "last_light.restaurant", kind: "imports" });
    const receipt = consumerCycle.input.generator.actionGraphReceipt;
    receipt.digest = digestValue({ nodes: [...receipt.nodes].sort((a, b) => `${a.kind}\0${a.identity}`.localeCompare(`${b.kind}\0${b.identity}`)), edges: [...receipt.edges].sort((a, b) => `${a.from}\0${a.to}\0${a.kind}`.localeCompare(`${b.from}\0${b.to}\0${b.kind}`)), schedule: receipt.schedule });
    expect(deriveSyn1Case(consumerCycle, corpus, { root }).code).toBe("invalid-action-recipe");
    expect(derive("forged-physical-input-handle").code).toBe("invalid-action-recipe");
    const graphCase = clone("restaurant-final-menu-generated-module");
    const graphDigest = (candidate) => {
      const receipt = candidate.input.generator.actionGraphReceipt;
      receipt.digest = digestValue({ nodes: [...receipt.nodes].sort((a, b) => `${a.kind}\0${a.identity}`.localeCompare(`${b.kind}\0${b.identity}`)), edges: [...receipt.edges].sort((a, b) => `${a.from}\0${a.to}\0${a.kind}`.localeCompare(`${b.from}\0${b.to}\0${b.kind}`)), schedule: receipt.schedule });
      return candidate;
    };
    const missingConsumer = structuredClone(graphCase);
    missingConsumer.input.generator.actionGraphReceipt.nodes = missingConsumer.input.generator.actionGraphReceipt.nodes.filter((node) => node.kind !== "consumer");
    missingConsumer.input.generator.actionGraphReceipt.edges = missingConsumer.input.generator.actionGraphReceipt.edges.filter((edge) => edge.from !== "last_light.restaurant");
    expect(deriveSyn1Case(graphDigest(missingConsumer), corpus, { root }).code).toBe("invalid-action-recipe");
    const extraDependency = structuredClone(graphCase);
    extraDependency.input.generator.actionGraphReceipt.nodes.push({ kind: "dependency", identity: "unused" });
    expect(deriveSyn1Case(graphDigest(extraDependency), corpus, { root }).code).toBe("invalid-action-recipe");
    const reverse = structuredClone(graphCase);
    reverse.input.generator.actionGraphReceipt.edges.push({ from: "std", to: reverse.input.generator.toolArtifact, kind: "imports" });
    expect(deriveSyn1Case(graphDigest(reverse), corpus, { root }).code).toBe("invalid-action-recipe");
    const unknown = structuredClone(graphCase);
    unknown.input.generator.actionGraphReceipt.nodes.push({ kind: "mystery", identity: "unknown" });
    expect(deriveSyn1Case(graphDigest(unknown), corpus, { root }).code).toBe("invalid-action-recipe");
  });

  test("publishes a Research explain/navigation record without claiming compiler or LSP evidence", () => {
    const result = derive("exact-second-mapping-fix");
    expect(result.explainNavigation).toMatchObject({
      logicalGeneratedModule: "last_light.generated.final_menu",
      outputBinding: "generatedMenuModule",
      compilerEvidence: "missing",
      generatedSourceAccess: "read-only-inspectable",
      navigation: "Research-requirement-not-implemented",
    });
    expect(result.explainNavigation.actionResultKey).toBe(result.actionResultKey);
    expect(result.explainNavigation.generatedSources.every((item) => item.logicalPath.endsWith(".w"))).toBe(true);
  });

  test("validates recipes before action failure and keeps route independent from status", () => {
    expect(derive("failure-discards-staging")).toMatchObject({ status: "discarded", route: "research-candidate", actionResultPublished: false });
    expect(derive("invalid-recipe-on-failure")).toMatchObject({ status: "rejected", route: "research-candidate", code: "invalid-action" });
    expect(derive("forged-dependency-on-failure")).toMatchObject({ status: "rejected", route: "research-candidate", code: "invalid-action-recipe" });
    expect(derive("proc-macro-rejected")).toMatchObject({ status: "rejected", route: "intentionally-rejected" });
  });

  test("rejects action failure after finish, event flags, and ambient authority", () => {
    const afterFinish = clone("failure-discards-staging");
    afterFinish.input.actionEvents = ["declared-input", "tool-start", "tool-stage", "tool-write", "tool-finish", "tool-error", "cleanup", "drain", "discard"].map((kind) => ({ kind }));
    expect(deriveSyn1Case(afterFinish, corpus, { root }).code).toBe("invalid-input");
    const flagged = clone("failure-discards-staging");
    flagged.input.actionEvents[1].outcome = "error";
    expect(deriveSyn1Case(flagged, corpus, { root }).code).toBe("invalid-input");
    expect(derive("authority-request")).toMatchObject({ status: "rejected", route: "research-candidate", code: "ambient-authority" });
  });
});
