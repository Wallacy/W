import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import {
  createPkg1State,
  applyPkg1Operation,
  deriveOwnerDigest,
  deriveDocumentState,
  digestRecord,
  derivePkg1,
  deriveProviderDurabilityReceiptDigest,
  reducePosixEvents,
  reduceWindowsEvents,
} from "./pkg1-project-transaction-machine.mjs";
import { parseManifestDocument } from "./w-manifest-data.mjs";

const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "pkg1-project-transaction-cases.json"), "utf8"));
const workspaceFixture = corpus.fixtures.workspace.operations[0];
const baseDigest = "sha256:0bc1bf029fca3ee18af1b92e92c256f313ba7a77ea8230fb5165ad31fb5a1022";

function seeded() {
  const state = createPkg1State();
  applyPkg1Operation(state, workspaceFixture);
  return state;
}

const events = (...kinds) => kinds.map((kind) => ({ kind }));
const posixSuccess = () => events("temp-created", "temp-written", "temp-data-flushed", "compare-verified", "rename-committed", "target-reopened", "content-verified", "parent-directory-flushed");
const windowsSuccess = () => events("temp-created", "temp-written", "temp-data-flushed", "compare-verified", "replace-file-committed", "target-reopened", "content-verified");

describe("PKG1 project transaction host oracle", () => {
  test("structured manifest canonicalization is recursive and rejects duplicate fields", () => {
    const left = parseManifestDocument('workspace { schema: "w.workspace/1" members: [".", "tools"] nested: { beta: 2 alpha: 1 } resolution: {} deployments: [] }');
    const right = parseManifestDocument('workspace { deployments: [] nested: { alpha: 1 beta: 2 } members: [".", "tools"] schema: "w.workspace/1" resolution: {} }');
    expect(deriveOwnerDigest(left)).toBe(deriveOwnerDigest(right));
    const reorderedMembers = structuredClone(right);
    reorderedMembers.members.reverse();
    expect(deriveOwnerDigest(reorderedMembers)).not.toBe(deriveOwnerDigest(right));
    expect(() => parseManifestDocument('workspace { schema: "w.workspace/1" schema: "duplicate" }')).toThrow("manifestDuplicateField");
  });

  test("owner basis excludes resolution and deployments", () => {
    const document = structuredClone(workspaceFixture.document);
    const first = deriveOwnerDigest(document);
    document.resolution.extra = "refresh";
    document.deployments[0].plans = [{ id: "changed" }];
    expect(deriveOwnerDigest(document)).toBe(first);
    document.members = [".", "packages/menu-compiler", "packages/new"];
    expect(deriveOwnerDigest(document)).not.toBe(first);
  });

  test("resolution and deployment identities remain separate", () => {
    const state = seeded();
    expect(state.documentDigest).toBe(baseDigest);
    const before = deriveDocumentState(state.document);
    const resolution = applyPkg1Operation(state, {
      op: "transaction",
      command: "resolve",
      resolution: { contexts: [{ name: "product", root: "last-light-native", use: "product", target: "x86_64-unknown-linux-gnu", targetVariants: ["refresh"], nodes: ["sha256:1111111111111111111111111111111111111111111111111111111111111111"], rootEdges: [{ alias: "chart", id: "sha256:1111111111111111111111111111111111111111111111111111111111111111" }] }] },
      expectedDocumentDigest: before.documentDigest,
      platform: "posix",
      providerEvents: posixSuccess(),
    });
    expect(resolution.changed).toEqual({ owner: false, resolution: true, deployment: false });
    const afterResolution = deriveDocumentState(state.document);
    const deployment = applyPkg1Operation(state, {
      op: "transaction",
      command: "deployment",
      deployment: { name: "local", plans: [{ id: "deployment-only" }] },
      expectedDocumentDigest: afterResolution.documentDigest,
      platform: "posix",
      providerEvents: posixSuccess(),
    });
    expect(deployment.changed.owner).toBe(false);
    expect(deployment.changed.resolution).toBe(false);
    expect(deployment.changed.deployment).toBe(true);
  });

  test("stale write leaves the previous bytes", () => {
    const state = seeded();
    expect(() => applyPkg1Operation(state, { op: "transaction", command: "resolve", resolution: {}, expectedDocumentDigest: "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" })).toThrow("staleWrite");
    expect(state.documentDigest).toBe(baseDigest);
    expect(state.temporary).toBe(0);
  });

  test("solve failure and dry-run do not replace", () => {
    const state = seeded();
    expect(() => applyPkg1Operation(state, { op: "transaction", command: "update", ownerPatch: { dependencies: [] }, solve: "fail", expectedDocumentDigest: baseDigest })).toThrow("resolutionFailed");
    const dry = applyPkg1Operation(state, { op: "transaction", command: "resolve", resolution: { contexts: [{ name: "product", root: "last-light-native", use: "product", target: "x86_64-unknown-linux-gnu", targetVariants: ["dry"], nodes: ["sha256:1111111111111111111111111111111111111111111111111111111111111111"], rootEdges: [{ alias: "chart", id: "sha256:1111111111111111111111111111111111111111111111111111111111111111" }] }] }, dryRun: true, expectedDocumentDigest: baseDigest });
    expect(dry.code).toBe("dryRun");
    expect(state.documentDigest).toBe(baseDigest);
  });

  test("rejects forged caller facts and preserves explicit durability evidence", () => {
    const state = seeded();
    expect(() => applyPkg1Operation(state, { op: "transaction", command: "resolve", resolution: {}, expectedDocumentDigest: baseDigest, status: "accepted" })).toThrow("callerEchoRejected");
    expect(() => applyPkg1Operation(state, { op: "transaction", command: "resolve", resolution: {}, expectedDocumentDigest: baseDigest, flushData: true })).toThrow("legacyProviderClaimRejected");
    const result = applyPkg1Operation(state, { op: "transaction", command: "resolve", resolution: {}, expectedDocumentDigest: baseDigest, platform: "posix", providerEvents: posixSuccess() });
    expect(result.atomicVisible).toBe(true);
    expect(result.crashDurable).toBe(false);
    expect(result.durabilityEvidence).toBe("evidence-missing");
    const receiptDigest = deriveProviderDurabilityReceiptDigest("posix", {
      oldDocumentDigest: result.documentDigest,
      newDocumentDigest: result.documentDigest,
    });
    const receiptEvents = [...posixSuccess(), { kind: "provider-durability-receipt", schema: "w.provider-durability-receipt/1", durable: true, digest: receiptDigest }];
    const receipt = applyPkg1Operation(state, {
      op: "transaction",
      command: "resolve",
      resolution: {},
      expectedDocumentDigest: result.documentDigest,
      platform: "posix",
      providerEvents: receiptEvents,
    });
    expect(receipt.crashDurable).toBe(true);
    expect(receipt.durabilityEvidence).toBe("provider-receipt");
    const forged = structuredClone(receiptEvents);
    forged.at(-1).digest = digestRecord("provider", "forged");
    expect(() => applyPkg1Operation(state, { op: "transaction", command: "resolve", resolution: {}, expectedDocumentDigest: receipt.documentDigest, platform: "posix", providerEvents: forged })).toThrow("forgedDurabilityReceipt");
  });

  test("POSIX and Windows reducers are independent but agree on logical publication", () => {
    const context = { oldDocumentDigest: baseDigest, newDocumentDigest: "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" };
    expect(reducePosixEvents(posixSuccess(), context)).toMatchObject({ atomicVisible: true, published: "new", fault: null });
    expect(reduceWindowsEvents(windowsSuccess(), context)).toMatchObject({ atomicVisible: true, published: "new", fault: null });
    expect(() => reduceWindowsEvents(posixSuccess(), context)).toThrow("replaceFileFailed");
    expect(() => reducePosixEvents(windowsSuccess(), context)).toThrow("renameCommitMissing");
    expect(() => reducePosixEvents([...posixSuccess(), { kind: "unexpected" }], context)).toThrow("posixEventOrderInvalid");
    expect(() => reduceWindowsEvents([{ kind: "temp-created", claimed: true }], context)).toThrow("providerEventFieldInvalid");
  });

  test("all PKG1 corpus cases derive without caller expected facts", () => {
    const results = derivePkg1(corpus);
    expect(results).toHaveLength(25);
    expect(results.find((result) => result.caseId === "PKG1-crash-after-replace").status).toBe("faulted");
    expect(results.find((result) => result.caseId === "PKG1-reducer-divergence").code).toBe("reducerDivergence");
  });
});
