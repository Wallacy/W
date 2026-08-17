import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { fileURLToPath } from "node:url";
import { evaluateAtom2Case, validateAtom2 } from "../../atom2-atomic-contract-machine.mjs";
import { validateAtom2StudyManifest } from "../../atom2-atomic-contract-manifest.mjs";

const toolingDirectory = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const studyDirectory = path.resolve(path.dirname(fileURLToPath(import.meta.url)));
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "atom2-atomic-contract-cases.json"), "utf8"));
const manifest = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const byId = new Map(corpus.cases.map((testCase) => [testCase.id, testCase]));

describe("ATOM2 atomic contract host oracle", () => {
  test("validates the four axes and closes active Research", () => {
    const checked = validateAtom2(corpus, { root: path.resolve(toolingDirectory, "..") });
    expect(checked.errors).toEqual([]);
    expect(new Set(checked.results.map((item) => item.axis))).toEqual(new Set(["A", "B", "C", "D"]));
    expect(checked.results.some((item) => /research/u.test(item.status))).toBe(false);
  });

  test("promotes only the closed value-only carrier and preserves target fallback", () => {
    expect(evaluateAtom2Case(byId.get("A-canonical-sign-epoch")).status).toBe("promoted-value-record");
    expect(evaluateAtom2Case(byId.get("A-canonical-sign-epoch")).carrierWidth).toBe(8);
    const mixed = evaluateAtom2Case(byId.get("A-canonical-mixed-bool-signed-enum"));
    expect(mixed.status).toBe("promoted-value-record");
    expect(mixed.carrierWidth).toBe(2);
    expect(mixed.canonicalBitDirection).toBe("least-significant-first");
    expect(mixed.highBitsZero).toBe(true);
    expect(evaluateAtom2Case(byId.get("A-canonical-fallback")).status).toBe("fallback-declared");
    expect(evaluateAtom2Case(byId.get("A-canonical-fallback")).blocksThread).toBe(true);
    expect(evaluateAtom2Case(byId.get("A-canonical-lockfree-missing")).code).toBe("target-lockfree-unavailable");
    for (const id of ["A-canonical-pointer", "A-canonical-nested", "A-canonical-custom", "A-canonical-enum-payload", "A-canonical-uint-size", "A-canonical-float"]) expect(evaluateAtom2Case(byId.get(id)).status).toBe("rejected");
    expect(evaluateAtom2Case(byId.get("A-canonical-order")).code).toBe("compare-exchange-order-pair");
    expect(evaluateAtom2Case(byId.get("A-canonical-order-release-acquire")).code).toBe("compare-exchange-order-pair");
  });

  test("canonical bits and fallback boundaries reject invalid constructions", () => {
    expect(evaluateAtom2Case(byId.get("A-canonical-field-direction")).code).toBe("canonical-bit-direction-invalid");
    expect(evaluateAtom2Case(byId.get("A-canonical-field-order")).code).toBe("canonical-field-order-invalid");
    expect(evaluateAtom2Case(byId.get("A-canonical-high-bits")).code).toBe("canonical-high-bits-nonzero");
    expect(evaluateAtom2Case(byId.get("A-canonical-enum-code-invalid")).code).toBe("enum-code-invalid");
    expect(evaluateAtom2Case(byId.get("A-canonical-fallback-allocating")).code).toBe("fallback-allocation-forbidden");
    expect(evaluateAtom2Case(byId.get("A-canonical-fallback-cooperative")).code).toBe("fallback-context-incompatible");
    expect(evaluateAtom2Case(byId.get("A-canonical-fallback-signal")).code).toBe("fallback-context-incompatible");
    expect(evaluateAtom2Case(byId.get("A-canonical-fallback-freestanding")).code).toBe("fallback-context-incompatible");
    expect(evaluateAtom2Case(byId.get("A-canonical-fallback-parking-ambiguous")).code).toBe("parking-blocking-ambiguous");
    expect(evaluateAtom2Case(byId.get("A-canonical-cancel")).code).toBe("atomic-cancellation-point-forbidden");
  });

  test("keeps handle lifetime separate and forbids generation wrap", () => {
    expect(evaluateAtom2Case(byId.get("B-valid-handle")).dereferenceCount).toBe(1);
    expect(evaluateAtom2Case(byId.get("B-stale-handle")).dereferenceCount).toBe(0);
    expect(evaluateAtom2Case(byId.get("B-generation-exhaustion")).code).toBe("generation-exhaustion-retired");
    expect(evaluateAtom2Case(byId.get("B-generation-wrap")).code).toBe("generation-wrap-forbidden");
    expect(evaluateAtom2Case(byId.get("B-tagged-pointer")).code).toBe("tagged-pointer-rejected");
  });

  test("permits only explicit unsafe reclamation receipts", () => {
    expect(evaluateAtom2Case(byId.get("C-adapter-complete")).status).toBe("unsafe-adapter-permitted");
    expect(evaluateAtom2Case(byId.get("C-adapter-ffi")).status).toBe("unsafe-adapter-permitted");
    expect(evaluateAtom2Case(byId.get("C-adapter-missing-registration")).code).toBe("registration-required");
    expect(evaluateAtom2Case(byId.get("C-adapter-before-unlink")).code).toBe("retire-before-unlink");
    expect(evaluateAtom2Case(byId.get("C-adapter-before-quiescence")).code).toBe("reclaim-before-quiescence");
    expect(evaluateAtom2Case(byId.get("C-adapter-drop-double")).code).toBe("drop-double-or-before-quiescence");
    expect(evaluateAtom2Case(byId.get("C-adapter-shutdown")).code).toBe("shutdown-nonquiescent");
    expect(evaluateAtom2Case(byId.get("C-adapter-ffi-drain")).code).toBe("foreign-drain-sequence-incomplete");
    expect(evaluateAtom2Case(byId.get("C-universal-reclamation")).code).toBe("universal-reclamation-rejected");
  });

  test("rejected pointer and RCU forms cannot enter the contract", () => {
    expect(evaluateAtom2Case(byId.get("D-raw-pointer")).status).toBe("rejected");
    expect(evaluateAtom2Case(byId.get("D-universal-rcu")).status).toBe("rejected");
  });

  test("manifest mutations cannot forge evidence", () => {
    const mutated = structuredClone(manifest);
    mutated.sourceRefs[0].digest = "sha256:stale";
    expect(validateAtom2StudyManifest(mutated, { studyDirectory }).some((error) => error.includes("stale"))).toBe(true);
    const missing = structuredClone(manifest);
    missing.sourceRefs[0].symbol = "NotInLastLight";
    expect(validateAtom2StudyManifest(missing, { studyDirectory }).some((error) => error.includes("exactly once"))).toBe(true);
  });
});
