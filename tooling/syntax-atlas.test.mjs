import { describe, expect, test } from "bun:test";
import { buildManifest, checkManifest, deriveSnapshot, renderCoverage, VISIBLE_RULES_MUST_NOT_BE_INTERNAL, REQUIRED_VARIANT_IDS } from "./syntax-atlas.mjs";

const snapshot = deriveSnapshot();
const coverage = renderCoverage(snapshot);
const manifest = buildManifest(snapshot, coverage);

function errorsFor(mutator) {
  const candidate = structuredClone(manifest);
  mutator(candidate);
  return checkManifest(snapshot, candidate);
}

describe("syntax atlas coverage checker", () => {
  test("current atlas is internally green", () => {
    expect(checkManifest(snapshot, manifest)).toEqual([]);
  });

  test("generated header links normative operator and performance guidance", () => {
    expect(coverage).toContain("`language.w`, `execution.w`, `operators.w`, and `build.w`");
    expect(coverage).toContain("../../CHEATSHEET.md#operadores-bits-e-política-numérica");
    expect(coverage).toContain("../../CHEATSHEET.md#performance-e-custo");
  });

  test("rejects an unlisted marker", () => {
    const errors = errorsFor((candidate) => candidate.blocks.pop());
    expect(errors.some((error) => error.includes("missing or has unlisted markers"))).toBe(true);
  });

  test("rejects a stale snippet digest", () => {
    const errors = errorsFor((candidate) => { candidate.blocks[0].snippetDigest = "sha256:" + "0".repeat(64); });
    expect(errors.some((error) => error.includes("snippet digest is stale"))).toBe(true);
  });

  test("rejects a duplicate marker id", () => {
    const errors = errorsFor((candidate) => { candidate.blocks[1].id = candidate.blocks[0].id; });
    expect(errors.some((error) => error.includes("duplicate IDs"))).toBe(true);
  });

  test("rejects a stale syntax coverage digest", () => {
    const errors = errorsFor((candidate) => { candidate.generated.coverageDigest = "sha256:" + "f".repeat(64); });
    expect(errors.some((error) => error.includes("syntax coverage digest is stale"))).toBe(true);
  });

  test("rejects an incompatible root disposition", () => {
    const errors = errorsFor((candidate) => { candidate.rootForms[0].root = "package"; });
    expect(errors.some((error) => error.includes("root disposition inventory is stale"))).toBe(true);
  });

  test("rejects a role/evidence mutation", () => {
    const errors = errorsFor((candidate) => { candidate.blocks[0].evidenceStatus = "tree-sitter-parse-only"; });
    expect(errors.some((error) => error.includes("marker, grammar refs, source ref, order, or snippet digest is stale"))).toBe(true);
  });

  test("rejects the legacy implementationEvidence field", () => {
    const errors = errorsFor((candidate) => { candidate.blocks[0].implementationEvidence = "parse-only"; });
    expect(errors.some((error) => error.includes("legacy implementationEvidence field"))).toBe(true);
  });

  test("visible grammar subforms are never waived", () => {
    for (const name of VISIBLE_RULES_MUST_NOT_BE_INTERNAL) {
      const entry = snapshot.ruleEntries.find((rule) => rule.name === name);
      expect(entry?.surface).toBe("composed");
    }
    expect(snapshot.ruleEntries.some((rule) => rule.surface === "internal")).toBe(false);
  });

  test("rejects a missing required accepted variant", () => {
    const errors = errorsFor((candidate) => { candidate.variants = candidate.variants.filter((variant) => variant.id !== REQUIRED_VARIANT_IDS[0]); });
    expect(errors.some((error) => error.includes("accepted variant inventory is stale"))).toBe(true);
  });

  test("rejects a stale variant witness", () => {
    const errors = errorsFor((candidate) => { candidate.variants[0].witness = "not in source"; });
    expect(errors.some((error) => error.includes("accepted variant inventory is stale"))).toBe(true);
  });

  test("rejects a companion role mutation", () => {
    const errors = errorsFor((candidate) => { candidate.companions[0].designStatus = "current"; });
    expect(errors.some((error) => error.includes("companion status inventory is stale"))).toBe(true);
  });

  test("callable variants keep W label taxonomy", () => {
    const source = snapshot.blocks.find((block) => block.id === "callables-and-foreign").snippet;
    for (const witness of ["order: String", "named audit: String", "_ note: String", "to destination: String", "title: String =", "each tags: String..."]) {
      expect(source.includes(witness)).toBe(true);
    }
    expect(source.includes("fn optionalLabel")).toBe(false);
  });

  test("allocator variants keep contextual parameter and omitted call distinct", () => {
    const source = snapshot.blocks.find((block) => block.id === "allocator-and-bindings").snippet;
    expect(source.includes("allocator destination: ref Allocator")).toBe(true);
    expect(source.includes("stage(city)")).toBe(true);
    expect(source.includes("allocator .fixed<capacity: 128>")).toBe(true);
    expect(source.includes("allocator .root")).toBe(false);
    expect(source.includes("allocator .none")).toBe(false);
  });

  test("direct sync entry remains static and keeps its error edge", () => {
    const source = snapshot.blocks.find((block) => block.id === "execution-forms").snippet;
    const variant = snapshot.variants.find((entry) => entry.id === "execution-sync");
    expect(source).toContain('let direct = try sync fetch("north")');
    expect(source).toContain("async fn fetch(city: String): String throws String");
    expect(variant).toMatchObject({
      construction: "direct neverSuspend entry; requires explicit async fn and a declaration-wide static proof",
      evidenceStatus: "tree-sitter-parse-only-compiler-runtime-missing",
    });
    expect(snapshot.blocks.find((block) => block.id === "execution-forms").designRefs).toContain("12.2.2");
  });

  test("quick reference syntax cells come from source bytes", () => {
    for (const variant of snapshot.variants) {
      expect(snapshot.blocks.find((block) => block.id === variant.block).snippet.includes(variant.witness)).toBe(true);
      expect(coverage.includes(`| \`${variant.id}\` | \`${variant.witness}\` | \`${variant.block}\` |`)).toBe(true);
    }
  });

  test("syntax coverage contains each snippet byte sequence", () => {
    for (const block of snapshot.blocks) {
      const body = block.snippet.replace(/\n$/u, "");
      expect(coverage.includes("\n" + body + "\n```\n")).toBe(true);
    }
  });
});
