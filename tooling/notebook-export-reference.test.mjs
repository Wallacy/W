import { describe, expect, test } from "bun:test";
import { runNotebookExportFixtureProgram, runNotebookExportProgram, sourceDigest } from "./notebook-export-machine.mjs";

const lock = "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const context = "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const toolchain = "sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
const receipt = (source, id = "value", effectStatus = "known") => ({
  cellId: id,
  sourceDigest: sourceDigest(source),
  ordinal: 1,
  sessionId: "session-1",
  incarnation: 1,
  generationBefore: "opaque:g0",
  generationAfter: "opaque:g1",
  toolchainDigest: toolchain,
  lockDigest: lock,
  contextDigest: context,
  effectDigest: "sha256:ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
  outcome: "committed",
  bindingRecords: [{ id, version: 1, fingerprint: "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", creationIncarnation: 1, creationGenerationId: "opaque:g0" }],
  hardEdges: [],
  providerOutcomes: [{ provider: "std.presentation@1", status: "ok" }],
  effectOutcomes: [{ kind: "pure", status: effectStatus }],
  inputs: [{ kind: "literal", state: "resolved", secret: false }],
  resourceStates: [{ kind: "binding", state: "stable", serializable: true }],
});

function valid(source = "let value = 1", id = "value") {
  return [
    { op: "load", nbformat: 4, nbformat_minor: 5, cells: [{ id, cell_type: "code", source, outputs: [{ output_type: "execute_result", data: { "text/plain": ["old"] } }] }] },
    { op: "manifest", value: { version: 2, lock: { digest: lock }, context: { digest: context }, toolchainDigest: toolchain, target: "native", plan: { kind: "single-file", entry: id, defaultEntry: { cellId: id, bindingId: id, version: 1, moduleId: "root" }, roles: [{ cellId: id, role: "root" }], modules: [{ id: "root", role: "root", sourceCells: [id], digest: "auto" }], contentDigest: "auto" }, receipts: [receipt(source, id)] } },
  ];
}

describe("PYN3 notebook export host oracle", () => {
  test("exports a proof-backed single file without execution", () => {
    const result = runNotebookExportFixtureProgram([...valid(), { op: "export" }]);
    expect(result.status).toBe("accepted");
    expect(result.state.output.kind).toBe("single-file");
    expect(result.state.output.executed).toBe(false);
    expect(result.state.output.sourceDigest).toBe(result.state.output.modules.find((module) => module.role === "root").digest);
    expect(result.state.executions).toBe(0);
  });

  test("invalidated cells block export", () => {
    const result = runNotebookExportFixtureProgram([...valid(), { op: "invalidate", cellId: "value" }, { op: "export" }]);
    expect(result.error.code).toBe("W-EXPORT-0004");
  });

  test("unknown effects block export", () => {
    const operations = valid();
    operations[1].value.receipts[0].effectOutcomes[0].status = "unknown";
    const result = runNotebookExportFixtureProgram([...operations, { op: "export" }]);
    expect(result.error.code).toBe("W-EXPORT-0005");
  });

  test("cycles cannot be silently linearized", () => {
    const sourceA = "let a = 1";
    const sourceB = "let b = 2";
    const a = receipt(sourceA, "a");
    const b = receipt(sourceB, "b");
    b.ordinal = 2;
    b.generationBefore = "opaque:g1";
    b.generationAfter = "opaque:g2";
    b.hardEdges = [{ bindingId: "a", version: 1, kind: "compiledLookup", pyn2Kind: "compiledLookup", cellId: "a", incarnation: 1, generationId: "opaque:g0" }];
    a.hardEdges = [{ bindingId: "b", version: 1, kind: "compiledLookup", pyn2Kind: "compiledLookup", cellId: "b", incarnation: 1, generationId: "opaque:g0" }];
    const operations = [
      { op: "load", nbformat: 4, nbformat_minor: 5, cells: [{ id: "a", cell_type: "code", source: sourceA }, { id: "b", cell_type: "code", source: sourceB }] },
      { op: "manifest", value: { version: 2, lock: { digest: lock }, context: { digest: context }, toolchainDigest: toolchain, target: "native", plan: { kind: "single-file", entry: "a", modules: [], contentDigest: "auto" }, receipts: [a, b] } },
      { op: "export" },
    ];
    expect(runNotebookExportFixtureProgram(operations).error.code).toBe("W-EXPORT-0006");
  });

  test("replay is an explicit failure", () => {
    const result = runNotebookExportProgram([{ op: "load", nbformat: 4, nbformat_minor: 5, cells: [] }, { op: "replay" }]);
    expect(result.error.code).toBe("W-EXPORT-0007");
    expect(result.state.executions).toBe(0);
  });

  test("semantic machine rejects auto digests", () => {
    const result = runNotebookExportProgram([...valid(), { op: "export" }]);
    expect(result.error.code).toBe("W-EXPORT-0003");
  });
});
