import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import {
  assertNoRuntimeLifetimeMetadata,
  deriveRelationContract,
  evaluateBorrowRelationCase,
} from "../../brx2-borrow-relations-machine.mjs";
import { validateBRX2StudyManifest } from "../../brx2-borrow-relations-manifest.mjs";

const studyDirectory = import.meta.dir;
const root = path.resolve(studyDirectory, "../../..");
const manifest = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const fileDigest = (file) => "sha256:" + crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex");

const bodyless = (overrides = {}) => ({
  kind: "free",
  body: false,
  inputs: [
    { slot: "primary", mode: "ref" },
    { slot: "fallback", mode: "ref" },
  ],
  results: [{ slot: "result", mode: "view" }],
  ...overrides,
});

const primaryAssay = {
  kind: "independent-assay",
  problemTrace: [{ operation: "return", result: "result", source: "primary" }],
};

describe("BRX2 borrow-relation study oracle", () => {
  test("keeps the study witness manifest data-only and durable", () => {
    expect(manifest.$schema).toBe("w-brx2-borrow-relations-study-1");
    expect(manifest.status).toBe("design-oracle-input");
    expect(manifest.evidence.current).toContain("host-oracle");
    expect(manifest.evidence.missing).toContain("hir-verifier");
    for (const witness of manifest.witnesses) {
      expect(fileDigest(path.join(studyDirectory, witness.path))).toBe(witness.digest);
      expect(["alternative", "research-candidate", "rejected-witness"]).toContain(witness.role);
    }
    expect(validateBRX2StudyManifest(manifest, { studyDirectory, root })).toEqual([]);
  });

  test("rejects manifest mutations for paths, digests, identities, and roles", () => {
    const missing = structuredClone(manifest);
    missing.artifactRefs[1].path = "../../missing-corpus.json";
    expect(validateBRX2StudyManifest(missing, { studyDirectory, root }).some((error) => error.includes("path"))).toBe(true);
    const stale = structuredClone(manifest);
    stale.artifactRefs[0].digest = "sha256:" + "0".repeat(64);
    expect(validateBRX2StudyManifest(stale, { studyDirectory, root }).some((error) => error.includes("digest is stale"))).toBe(true);
    const duplicate = structuredClone(manifest);
    duplicate.artifactRefs.push(structuredClone(duplicate.artifactRefs[0]));
    expect(validateBRX2StudyManifest(duplicate, { studyDirectory, root }).some((error) => error.includes("artifactRefs must") || error.includes("duplicated"))).toBe(true);
    const role = structuredClone(manifest);
    role.witnesses[0].role = "caller-claim";
    expect(validateBRX2StudyManifest(role, { studyDirectory, root }).some((error) => error.includes("role is invalid"))).toBe(true);
    const symbol = structuredClone(manifest);
    symbol.sourceBase.symbol = "forgedSymbol";
    expect(validateBRX2StudyManifest(symbol, { studyDirectory, root }).some((error) => error.includes("symbol"))).toBe(true);
  });

  test("derives current receiver/body facts without a caller mapping", () => {
    const result = evaluateBorrowRelationCase({
      id: "oracle-receiver",
      declaration: {
        kind: "instance",
        body: false,
        inputs: [
          { slot: "receiver", mode: "ref" },
          { slot: "fallback", mode: "ref" },
        ],
        results: [{ slot: "result", mode: "view" }],
      },
    });
    expect(result.route).toBe("current");
    expect(result.mapping.baseline).toEqual({ result: ["receiver"] });
    expect(result.mapping.baselineEdges).toHaveLength(1);
    expect(result.mapping.baselineOriginSets).toEqual(["receiver"]);
    expect(result.diagnostics.map((item) => item.code)).toContain("relationOmitted");
    expect(assertNoRuntimeLifetimeMetadata(result)).toBe(true);
  });

  test("derives a sealed requirement relation and interface facts", () => {
    const declaration = bodyless({
      relationContract: {
        owner: "requirement",
        sealed: true,
        pairs: [{ result: "result", sources: ["primary"] }],
      },
      interface: {},
    });
    const result = evaluateBorrowRelationCase({ id: "oracle-relation", declaration, assay: primaryAssay });
    expect(result.route).toBe("research");
    expect(result.mapping.relationExact).toBe(true);
    expect(result.mapping.relation).toEqual({ result: ["primary"] });
    expect(result.mapping.relationEdges).toHaveLength(1);
    expect(result.mapping.relationOriginSets).toEqual(["primary"]);
    expect(result.interfaces.semanticInterfaceKey).toMatch(/^sha256:[0-9a-f]{64}$/u);
    expect(result.artifacts.relationDigest).toBe(result.mapping.relationDigest);
    expect(deriveRelationContract(declaration).owner).toBe("requirement");
  });

  test("rejects authority and interface drift mutations", () => {
    const declaration = bodyless({
      relationContract: {
        owner: "requirement",
        sealed: true,
        pairs: [{ result: "result", sources: ["primary"] }],
      },
    });
    const caller = evaluateBorrowRelationCase({
      id: "oracle-caller-claim",
      declaration,
      assay: primaryAssay,
      artifacts: { callerClaim: { result: ["fallback"] } },
    });
    expect(caller.decision).toBe("rejected");
    expect(caller.diagnostics.map((item) => item.code)).toContain("callerRelationClaimRejected");
    const stale = evaluateBorrowRelationCase({
      id: "oracle-stale-interface",
      declaration,
      assay: primaryAssay,
      interface: { relationDigest: "sha256:" + "0".repeat(64) },
    });
    expect(stale.decision).toBe("rejected");
    expect(stale.diagnostics.map((item) => item.code)).toContain("interfaceRelationDigestMismatch");
  });

  test("keeps invocation loans fresh and rejects live Stream reuse", () => {
    const callable = evaluateBorrowRelationCase({
      id: "oracle-callable",
      declaration: bodyless({ body: true, bodyTrace: [
        { operation: "return", result: "result", source: "primary" },
      ] }),
      invocation: { kind: "callable", parameter: "primary", calls: 3 },
    });
    expect(callable.invocation.freshLoans).toHaveLength(3);
    expect(callable.invocation.invocationEdges).toHaveLength(3);
    const stream = evaluateBorrowRelationCase({
      id: "oracle-stream",
      declaration: bodyless({ body: true, bodyTrace: [
        { operation: "return", result: "result", source: "primary" },
      ] }),
      invocation: { kind: "stream-next", viewLive: true, reusesStorage: true },
    });
    expect(stream.invocation.status).toBe("rejected");
    expect(stream.invocation.code).toBe("W-BORROW-0006");
  });

  test("uses an applicable relation for invocation and keeps conservative fallback on rejection", () => {
    const declaration = bodyless({
      relationContract: {
        owner: "requirement",
        sealed: true,
        pairs: [{ result: "result", sources: ["primary"] }],
      },
    });
    const applicable = evaluateBorrowRelationCase({
      id: "oracle-effective-relation",
      declaration,
      assay: primaryAssay,
      invocation: { kind: "callable", parameter: "primary", calls: 2, erasure: "any-fn" },
    });
    expect(applicable.mapping.relationApplicable).toBe(true);
    expect(applicable.invocation.mappingSource).toBe("relation");
    expect(applicable.invocation.effectiveMapping).toEqual({ result: ["primary"] });
    expect(applicable.invocation.erasure.mapping).toEqual({ result: ["primary"] });
    const rejected = evaluateBorrowRelationCase({
      id: "oracle-relation-rejected-conservative",
      declaration,
      assay: primaryAssay,
      artifacts: { relationDigest: "sha256:" + "0".repeat(64) },
      invocation: { kind: "callable", parameter: "primary", calls: 1 },
    });
    expect(rejected.mapping.relationApplicable).toBe(false);
    expect(rejected.invocation.mappingSource).toBe("baseline");
    expect(rejected.invocation.effectiveMapping).toEqual({ result: ["fallback", "primary"] });
  });

  test("treats problemTrace as an independent host assay, not compiler evidence", () => {
    const declaration = bodyless({
      relationContract: {
        owner: "requirement",
        sealed: true,
        pairs: [{ result: "result", sources: ["primary"] }],
      },
    });
    const missing = evaluateBorrowRelationCase({ id: "oracle-assay-omitted", declaration });
    expect(missing.mapping.relationExact).toBe(false);
    expect(missing.mapping.relationApplicable).toBe(false);
    const alternate = evaluateBorrowRelationCase({
      id: "oracle-assay-alternate",
      declaration,
      assay: {
        kind: "independent-assay",
        problemTrace: [{ operation: "return", result: "result", source: "fallback" }],
      },
    });
    const primary = evaluateBorrowRelationCase({ id: "oracle-assay-primary", declaration, assay: primaryAssay });
    expect(alternate.mapping.relationDigest).toBe(primary.mapping.relationDigest);
    expect(alternate.interfaces.semanticInterfaceKey).toBe(primary.interfaces.semanticInterfaceKey);
    const invalid = evaluateBorrowRelationCase({
      id: "oracle-assay-forged",
      declaration,
      assay: {
        kind: "independent-assay",
        problemTrace: [{ operation: "relation", result: "result", source: "primary" }],
      },
    });
    expect(invalid.decision).toBe("rejected");
    expect(invalid.diagnostics.map((item) => item.code)).toContain("problemTraceOperationInvalid");
    const expectedAsTrace = evaluateBorrowRelationCase({
      id: "oracle-assay-expected-forged",
      declaration,
      assay: {
        kind: "independent-assay",
        problemTrace: [{ operation: "expected", result: "result", source: "primary" }],
      },
    });
    expect(expectedAsTrace.diagnostics.map((item) => item.code)).toContain("problemTraceOperationInvalid");
    const declarationPlaced = evaluateBorrowRelationCase({
      id: "oracle-assay-declaration-placement",
      declaration: {
        ...declaration,
        problemTrace: primaryAssay.problemTrace,
        problemTraceKind: "independent-assay",
      },
    });
    expect(declarationPlaced.decision).toBe("rejected");
    expect(declarationPlaced.diagnostics.map((item) => item.code)).toContain("problemTracePlacementInvalid");
  });
});
