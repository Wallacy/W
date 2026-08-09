import assert from "node:assert/strict";
import test from "node:test";
import {
  runMemoryProgram,
  validateMemoryOperation,
} from "./hir-memory-machine.mjs";

const OWNER_STATE = Object.freeze({
  uninitialized: "uninitialized",
  owned: "owned",
  moved: "moved",
  dropped: "dropped",
});

const ADDRESS_STATE = Object.freeze({
  unstable: "unstable",
  stable: "stable",
  published: "published",
});

const BOUNDARY = Object.freeze({
  internal: "internal",
  wExact: "wExact",
  foreignC: "foreignC",
  wire: "wire",
  persisted: "persisted",
  capability: "capability",
});

const REPRESENTATION = Object.freeze({
  explicitTag: "explicitTag",
  provenNiche: "provenNiche",
  lowBit: "lowBit",
  highBit: "highBit",
  nativeCarrier: "nativeCarrier",
});

class HirMemoryError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function owner({ address = ADDRESS_STATE.unstable } = {}) {
  return {
    state: OWNER_STATE.owned,
    borrowCount: 0,
    address,
  };
}

function beginBorrow(value) {
  if (value.state !== OWNER_STATE.owned) {
    throw new HirMemoryError("borrowRequiresOwner");
  }

  const next = { ...value, borrowCount: value.borrowCount + 1 };
  return { owner: next, token: { active: true } };
}

function endBorrow(value, token) {
  if (!token.active || value.borrowCount === 0) {
    throw new HirMemoryError("invalidBorrowEnd");
  }

  token.active = false;
  return { ...value, borrowCount: value.borrowCount - 1 };
}

function moveOwner(value) {
  if (value.state !== OWNER_STATE.owned) {
    throw new HirMemoryError("moveRequiresOwner");
  }

  if (value.borrowCount !== 0) {
    throw new HirMemoryError("moveWithActiveBorrow");
  }

  return {
    source: { ...value, state: OWNER_STATE.moved },
    destination: { ...value },
  };
}

function dropOwner(value) {
  if (value.state !== OWNER_STATE.owned) {
    throw new HirMemoryError("dropRequiresOwner");
  }

  if (value.borrowCount !== 0) {
    throw new HirMemoryError("dropWithActiveBorrow");
  }

  return { ...value, state: OWNER_STATE.dropped };
}

function canSuspend(value) {
  return (
    value.borrowCount === 0 || value.address !== ADDRESS_STATE.unstable
  );
}

function movePinnedHandle(handle, destination) {
  if (handle.state !== OWNER_STATE.owned) {
    throw new HirMemoryError("moveRequiresOwner");
  }
  return {
    source: { ...handle, state: OWNER_STATE.moved },
    destination: { ...handle, name: destination },
  };
}

function acceptsRepresentation(boundary, representation) {
  if (representation === REPRESENTATION.explicitTag) {
    return true;
  }

  if (representation === REPRESENTATION.provenNiche) {
    return ![
      BOUNDARY.foreignC,
      BOUNDARY.wire,
      BOUNDARY.persisted,
    ].includes(boundary);
  }

  if (representation === REPRESENTATION.lowBit) {
    return boundary === BOUNDARY.internal;
  }

  if (representation === REPRESENTATION.nativeCarrier) {
    return [BOUNDARY.foreignC, BOUNDARY.capability].includes(boundary);
  }

  return false;
}

function verifyBoundary({ boundary, representation, owned, allocatorKnown }) {
  if (!acceptsRepresentation(boundary, representation)) {
    throw new HirMemoryError("nonCanonicalBoundaryRepresentation");
  }

  if (owned && !allocatorKnown) {
    throw new HirMemoryError("missingAllocatorOrigin");
  }

  return true;
}

function sameAbi(left, right) {
  return (
    left.target === right.target &&
    left.callingConvention === right.callingConvention &&
    left.representationPolicy === right.representationPolicy &&
    left.runtimeAbi === right.runtimeAbi
  );
}

test("the owner kernel moves and drops exactly once", () => {
  const first = owner();
  const moved = moveOwner(first);

  assert.equal(first.state, OWNER_STATE.owned);
  assert.equal(moved.source.state, OWNER_STATE.moved);
  assert.equal(moved.destination.state, OWNER_STATE.owned);

  const dropped = dropOwner(moved.destination);
  assert.equal(dropped.state, OWNER_STATE.dropped);
  assert.throws(() => dropOwner(dropped), { code: "dropRequiresOwner" });
  assert.throws(() => moveOwner(moved.source), { code: "moveRequiresOwner" });
});

test("an active borrow blocks move and drop until its token ends", () => {
  const borrowed = beginBorrow(owner());

  assert.throws(() => moveOwner(borrowed.owner), {
    code: "moveWithActiveBorrow",
  });
  assert.throws(() => dropOwner(borrowed.owner), {
    code: "dropWithActiveBorrow",
  });

  const released = endBorrow(borrowed.owner, borrowed.token);
  assert.equal(released.borrowCount, 0);
  assert.throws(() => endBorrow(released, borrowed.token), {
    code: "invalidBorrowEnd",
  });
  assert.equal(moveOwner(released).destination.state, OWNER_STATE.owned);
});

test("a pinned payload supports suspension while its handle moves independently", () => {
  const ordinary = beginBorrow(owner());
  assert.equal(canSuspend(ordinary.owner), false);

  const pinned = beginBorrow(owner({ address: ADDRESS_STATE.published }));
  assert.equal(canSuspend(pinned.owner), true);
  assert.throws(() => moveOwner(pinned.owner), {
    code: "moveWithActiveBorrow",
  });
  const handle = {
    state: OWNER_STATE.owned,
    name: "handle",
    payload: pinned.owner,
  };
  const movedHandle = movePinnedHandle(handle, "movedHandle");
  assert.equal(movedHandle.source.state, OWNER_STATE.moved);
  assert.equal(movedHandle.destination.payload.address, ADDRESS_STATE.published);
  assert.equal(movedHandle.destination.payload.borrowCount, 1);
});

test("boundary representation and allocator origin are explicit", () => {
  assert.equal(
    verifyBoundary({
      boundary: BOUNDARY.internal,
      representation: REPRESENTATION.lowBit,
      owned: false,
      allocatorKnown: false,
    }),
    true,
  );
  assert.equal(
    verifyBoundary({
      boundary: BOUNDARY.wExact,
      representation: REPRESENTATION.provenNiche,
      owned: true,
      allocatorKnown: true,
    }),
    true,
  );
  assert.throws(
    () =>
      verifyBoundary({
        boundary: BOUNDARY.foreignC,
        representation: REPRESENTATION.provenNiche,
        owned: false,
        allocatorKnown: false,
      }),
    { code: "nonCanonicalBoundaryRepresentation" },
  );
  assert.throws(
    () =>
      verifyBoundary({
        boundary: BOUNDARY.wire,
        representation: REPRESENTATION.explicitTag,
        owned: true,
        allocatorKnown: false,
      }),
    { code: "missingAllocatorOrigin" },
  );
});

test("ABI mismatch rejects the link before lowering", () => {
  const consumer = {
    target: "linux-x64",
    callingConvention: "w-v1",
    representationPolicy: "portable-v1",
    runtimeAbi: "core-v1",
  };

  assert.equal(sameAbi(consumer, { ...consumer }), true);
  assert.equal(
    sameAbi(consumer, { ...consumer, runtimeAbi: "core-v2" }),
    false,
  );
  assert.equal(
    sameAbi(consumer, { ...consumer, target: "windows-x64" }),
    false,
  );
});

test("M1 tracks disjoint places and conservative dynamic overlap", () => {
  const accepted = runMemoryProgram([
    { op: "initialize", binding: "kitchen" },
    {
      op: "beginBorrow",
      binding: "kitchen",
      token: "menu",
      mode: "shared",
      place: {
        root: "kitchen",
        projections: [{ kind: "field", name: "menu", container: "struct" }],
      },
    },
    {
      op: "beginBorrow",
      binding: "kitchen",
      token: "oven",
      mode: "exclusive",
      place: {
        root: "kitchen",
        projections: [{ kind: "field", name: "oven", container: "struct" }],
      },
    },
  ]);
  assert.equal(accepted.status, "accepted");

  const rejected = runMemoryProgram([
    { op: "initialize", binding: "items" },
    {
      op: "beginBorrow",
      binding: "items",
      token: "read",
      mode: "shared",
      place: { root: "items", projections: [{ kind: "index", dynamic: true }] },
    },
    {
      op: "beginBorrow",
      binding: "items",
      token: "write",
      mode: "exclusive",
      place: { root: "items", projections: [{ kind: "index", dynamic: true }] },
    },
  ]);
  assert.equal(rejected.code, "loanOverlap");
});

test("M1 freezes and restores a reborrow parent", () => {
  const result = runMemoryProgram([
    { op: "initialize", binding: "value" },
    { op: "beginBorrow", binding: "value", token: "parent", mode: "exclusive" },
    {
      op: "reborrow",
      binding: "value",
      token: "child",
      parent: "parent",
      mode: "exclusive",
    },
    { op: "endBorrow", token: "child" },
    { op: "endBorrow", token: "parent" },
    { op: "drop", binding: "value" },
  ]);
  assert.equal(result.status, "accepted");
});

test("M1 carries origins through copies and rejects dependent escapes", () => {
  const returned = runMemoryProgram([
    {
      op: "initialize",
      binding: "document",
      edges: [
        { ownerSlot: "menu", origin: "menu", mode: "shared" },
        { ownerSlot: "oven", origin: "oven", mode: "shared" },
      ],
    },
    { op: "copy", from: "document", to: "copy" },
    {
      op: "escape",
      binding: "copy",
      target: "return",
      availableOrigins: ["menu", "oven"],
    },
  ]);
  assert.equal(returned.status, "accepted");

  const escaped = runMemoryProgram([
    {
      op: "initialize",
      binding: "document",
      edges: [{ ownerSlot: "menu", origin: "menu", mode: "shared" }],
    },
    { op: "escape", binding: "document", target: "channel" },
  ]);
  assert.equal(escaped.code, "dependentEscape");

  const joinedWhileBorrowed = runMemoryProgram([
    { op: "initialize", binding: "menu" },
    {
      op: "initialize",
      binding: "document",
      edges: [{ ownerBinding: "menu", origin: "menu", mode: "shared" }],
    },
    { op: "initialize", binding: "outer" },
    { op: "beginBorrow", binding: "document", token: "writer", mode: "exclusive" },
    { op: "joinDependencies", binding: "outer", from: "document" },
  ]);
  assert.equal(joinedWhileBorrowed.code, "loanOverlap");
});

test("M1 await ignores an independent aggregate address and still requires drain", () => {
  const independent = runMemoryProgram([
    { op: "initialize", binding: "frame" },
    {
      op: "await",
      binding: "frame",
      conflictFree: true,
      cleanupDrained: true,
      cancelDrained: true,
    },
  ]);
  assert.equal(independent.status, "accepted");

  const undrained = runMemoryProgram([
    { op: "initialize", binding: "frame" },
    {
      op: "await",
      binding: "frame",
      conflictFree: true,
      cleanupDrained: false,
      cancelDrained: true,
    },
  ]);
  assert.equal(undrained.code, "awaitCleanupNotDrained");
});

test("M1 keeps lifetime independence separate from boundary capabilities", () => {
  const missingServiceProofs = runMemoryProgram([
    {
      op: "initialize",
      binding: "staticInput",
      edges: [{ origin: "program", immortal: true }],
    },
    { op: "escape", binding: "staticInput", target: "service" },
  ]);
  assert.equal(missingServiceProofs.code, "dependentEscape");

  const serviceReady = runMemoryProgram([
    {
      op: "initialize",
      binding: "staticInput",
      edges: [{ origin: "program", immortal: true }],
    },
    {
      op: "escape",
      binding: "staticInput",
      target: "service",
      wireValue: true,
      transferable: true,
    },
  ]);
  assert.equal(serviceReady.status, "accepted");
});

test("M1 rejects a noncanonical range bound before execution", () => {
  assert.equal(
    validateMemoryOperation({
      op: "beginBorrow",
      binding: "values",
      token: "slice",
      mode: "shared",
      place: {
        root: "values",
        projections: [{ kind: "range", start: 0, end: 4 }],
      },
    }),
    false,
  );
  assert.equal(
    validateMemoryOperation({
      op: "beginBorrow",
      binding: "values",
      token: "slice",
      mode: "shared",
      place: {
        root: "values",
        projections: [{ kind: "range", dynamic: true, end: "limit" }],
      },
    }),
    false,
  );
  assert.equal(
    validateMemoryOperation({
      op: "accessDependency",
      binding: "document",
      edgeId: "title",
      origin: "menu",
    }),
    false,
  );
  assert.equal(
    validateMemoryOperation({ op: "endBorrow", token: "loan", loanId: "other" }),
    false,
  );
  assert.equal(
    validateMemoryOperation({ op: "joinDependencies", binding: "items", from: "items" }),
    false,
  );
});

test("M1 rejects widening and sibling reborrows and restores parent access", () => {
  const widening = runMemoryProgram([
    { op: "initialize", binding: "kitchen" },
    {
      op: "beginBorrow",
      binding: "kitchen",
      token: "parent",
      mode: "exclusive",
      place: { root: "kitchen", projections: [{ kind: "field", name: "west", container: "struct" }] },
    },
    {
      op: "reborrow",
      binding: "kitchen",
      token: "child",
      parent: "parent",
      mode: "exclusive",
      place: "kitchen",
    },
  ]);
  assert.equal(widening.code, "reborrowPlaceMismatch");

  const restored = runMemoryProgram([
    { op: "initialize", binding: "kitchen" },
    { op: "beginBorrow", binding: "kitchen", token: "parent", mode: "exclusive", place: "kitchen" },
    {
      op: "reborrow",
      binding: "kitchen",
      token: "child",
      parent: "parent",
      mode: "exclusive",
      place: { root: "kitchen", projections: [{ kind: "field", name: "west", container: "struct" }] },
    },
    { op: "accessLoan", token: "parent", access: "write" },
  ]);
  assert.equal(restored.code, "frozenReborrowParent");
  const afterChild = runMemoryProgram([
    { op: "initialize", binding: "kitchen" },
    { op: "beginBorrow", binding: "kitchen", token: "parent", mode: "exclusive", place: "kitchen" },
    {
      op: "reborrow",
      binding: "kitchen",
      token: "child",
      parent: "parent",
      mode: "exclusive",
      place: { root: "kitchen", projections: [{ kind: "field", name: "west", container: "struct" }] },
    },
    { op: "endBorrow", token: "child" },
    { op: "accessLoan", token: "parent", access: "write" },
  ]);
  assert.equal(afterChild.status, "accepted");
});

test("M1 keeps a reborrow parent frozen for every duplicated child", () => {
  const prefix = [
    { op: "initialize", binding: "kitchen" },
    { op: "beginBorrow", binding: "kitchen", token: "parent", mode: "exclusive" },
    {
      op: "reborrow",
      binding: "kitchen",
      token: "child",
      parent: "parent",
      mode: "shared",
      place: { root: "kitchen", projections: [{ kind: "field", name: "west", container: "struct" }] },
    },
    { op: "duplicateLoan", from: "child", token: "copy" },
    { op: "endBorrow", token: "child" },
  ];
  const frozen = runMemoryProgram([
    ...prefix,
    { op: "accessLoan", token: "parent", access: "write" },
  ]);
  assert.equal(frozen.code, "frozenReborrowParent");

  const released = runMemoryProgram([
    ...prefix,
    { op: "endBorrow", token: "copy" },
    { op: "accessLoan", token: "parent", access: "write" },
  ]);
  assert.equal(released.status, "accepted");
});

test("M1 keeps dependency edges individual and checks referents at await", () => {
  const blocked = runMemoryProgram([
    { op: "initialize", binding: "owner" },
    {
      op: "initialize",
      binding: "dependent",
      edges: [
        { ownerBinding: "owner", origin: "owner", mode: "shared" },
        { ownerBinding: "owner", origin: "owner", mode: "shared" },
      ],
    },
    { op: "move", from: "owner", to: "out" },
  ]);
  assert.equal(blocked.code, "ownerMoveWithDependency");

  const unstableReferent = runMemoryProgram([
    { op: "initialize", binding: "owner" },
    {
      op: "initialize",
      binding: "dependent",
      address: "stable",
      edges: [{ ownerBinding: "owner", origin: "owner", mode: "shared" }],
    },
    {
      op: "await",
      binding: "dependent",
      conflictFree: true,
      cleanupDrained: true,
      cancelDrained: true,
    },
  ]);
  assert.equal(unstableReferent.code, "unstableReferentSuspension");
});

test("M1 requires exact interface keys and every dependent result mapping", () => {
  const baseAbi = {
    target: "linux-x64",
    callingConvention: "w-v1",
    representationPolicy: "portable-v1",
    runtimeAbi: "core-v1",
  };
  const asymmetricKey = runMemoryProgram([
    {
      op: "verifyAbi",
      consumer: baseAbi,
      provider: { ...baseAbi, semanticInterfaceKey: "if-v1" },
    },
  ]);
  assert.equal(asymmetricKey.code, "interfaceLockMismatch");

  const missingResult = runMemoryProgram([
    {
      op: "verifyInterface",
      body: true,
      resultSlots: [
        { slot: "result.title", borrowed: true },
        { slot: "result.body", dependent: true },
      ],
      inferredMapping: { "result.title": ["parameter:0"] },
    },
  ]);
  assert.equal(missingResult.code, "interfaceOriginUnknown");
});

test("M1 binds ProofFacts to exact place prefixes", () => {
  const indexPlace = (root, value) => ({
    root,
    projections: [{ kind: "index", value }],
  });
  const correct = runMemoryProgram([
    { op: "initialize", binding: "rows" },
    { op: "beginBorrow", binding: "rows", token: "left", mode: "shared", place: indexPlace("rows", "i") },
    {
      op: "write",
      binding: "rows",
      place: indexPlace("rows", "j"),
      proofFacts: [{ kind: "disjoint", left: "rows/index:i", right: "rows/index:j" }],
    },
  ]);
  assert.equal(correct.status, "accepted");

  const wrongPlace = runMemoryProgram([
    { op: "initialize", binding: "rows" },
    { op: "beginBorrow", binding: "rows", token: "left", mode: "shared", place: indexPlace("rows", "i") },
    {
      op: "write",
      binding: "rows",
      place: indexPlace("rows", "j"),
      proofFacts: [{ kind: "disjoint", left: "other/index:i", right: "other/index:j" }],
    },
  ]);
  assert.equal(wrongPlace.code, "loanOverlap");
});

test("M1 treats stored dependency edges as access capabilities", () => {
  const sharedRead = runMemoryProgram([
    { op: "initialize", binding: "owner" },
    {
      op: "initialize",
      binding: "document",
      edges: [{ id: "title", ownerBinding: "owner", origin: "owner", mode: "shared" }],
    },
    { op: "accessDependency", binding: "document", edgeId: "title", access: "read" },
  ]);
  assert.equal(sharedRead.status, "accepted");

  const sharedWrite = runMemoryProgram([
    { op: "initialize", binding: "owner" },
    {
      op: "initialize",
      binding: "document",
      edges: [{ id: "title", ownerBinding: "owner", origin: "owner", mode: "shared" }],
    },
    { op: "accessDependency", binding: "document", edgeId: "title", access: "write" },
  ]);
  assert.equal(sharedWrite.code, "dependencyAccessModeMismatch");

  const exclusiveWrite = runMemoryProgram([
    { op: "initialize", binding: "owner" },
    {
      op: "initialize",
      binding: "controller",
      edges: [{ id: "temperature", ownerBinding: "owner", origin: "owner", mode: "exclusive" }],
    },
    { op: "accessDependency", binding: "controller", edgeId: "temperature", access: "write" },
  ]);
  assert.equal(exclusiveWrite.status, "accepted");
});

test("M1 requires unique dependency identity and commits edge creation atomically", () => {
  const ambiguous = runMemoryProgram([
    { op: "initialize", binding: "owner" },
    {
      op: "initialize",
      binding: "refs",
      edges: [
        { id: "item:0", ownerBinding: "owner", origin: "owner", mode: "shared" },
        { id: "item:1", ownerBinding: "owner", origin: "owner", mode: "shared" },
      ],
    },
    { op: "accessDependency", binding: "refs", origin: "owner", access: "read" },
  ]);
  assert.equal(ambiguous.code, "dependencyEdgeAmbiguous");

  const duplicate = runMemoryProgram([
    { op: "initialize", binding: "owner" },
    {
      op: "initialize",
      binding: "refs",
      edges: [
        { id: "item", ownerBinding: "owner", origin: "owner", mode: "shared" },
        { id: "item", ownerBinding: "owner", origin: "owner", mode: "shared" },
      ],
    },
  ]);
  assert.equal(duplicate.code, "dependencyEdgeIdAlreadyActive");
  assert.equal(duplicate.state.bindings.refs, undefined);
  assert.equal(duplicate.state.nextPayload, 1);
});

test("M1 keeps borrow origins separate from allocator origins", () => {
  const dependentShare = runMemoryProgram([
    {
      op: "defineAllocator",
      allocator: "process",
      lifetime: "product",
      mobility: "crossDomain",
      adoptionFamily: "general",
      limit: 256,
    },
    { op: "initialize", binding: "menu" },
    {
      op: "initialize",
      binding: "borrowed",
      edges: [{ ownerBinding: "menu", origin: "menu", mode: "shared" }],
    },
    { op: "share", from: "borrowed", to: "root", using: "process" },
  ]);
  assert.equal(dependentShare.code, "shareRequiresLifetimeIndependent");

  const localStorage = runMemoryProgram([
    {
      op: "defineAllocator",
      allocator: "scratch",
      lifetime: "scoped",
      mobility: "local",
      adoptionFamily: "arena",
      limit: 64,
    },
    { op: "initialize", binding: "snapshot", using: "scratch", bytes: 32 },
    {
      op: "escape",
      binding: "snapshot",
      target: "structuredChild",
      transferable: true,
      joinPrecedesOrigins: true,
    },
  ]);
  assert.equal(localStorage.code, "allocationOriginNotTransferable");
});

test("M1 rehome rewrites storage provenance before parallel transfer", () => {
  const result = runMemoryProgram([
    {
      op: "defineAllocator",
      allocator: "scratch",
      lifetime: "scoped",
      mobility: "local",
      adoptionFamily: "arena",
      limit: 64,
    },
    {
      op: "defineAllocator",
      allocator: "process",
      lifetime: "product",
      mobility: "crossDomain",
      adoptionFamily: "general",
      limit: 256,
    },
    { op: "initialize", binding: "snapshot", using: "scratch", bytes: 32 },
    { op: "rehome", from: "snapshot", to: "portable", using: "process", bytes: 32 },
    { op: "closeAllocator", allocator: "scratch" },
    {
      op: "escape",
      binding: "portable",
      target: "structuredChild",
      transferable: true,
      joinPrecedesOrigins: true,
    },
  ]);
  assert.equal(result.status, "accepted");
  assert.deepEqual(result.state.payloads.p0.storageOrigins, ["process"]);
  assert.equal(result.state.allocators.scratch.state, "closed");
});

test("M1 keeps weak storage until the last weak handle and drops payload once", () => {
  const result = runMemoryProgram([
    {
      op: "defineAllocator",
      allocator: "process",
      lifetime: "product",
      mobility: "crossDomain",
      adoptionFamily: "general",
      limit: 256,
    },
    { op: "initialize", binding: "menu", using: "process", bytes: 32 },
    {
      op: "share",
      from: "menu",
      to: "root",
      using: "process",
      bytes: 16,
      threadSafe: true,
    },
    { op: "makeWeak", from: "root", to: "observer" },
    { op: "drop", binding: "root" },
    { op: "upgradeWeak", from: "observer", to: "expired", result: "afterRelease" },
    { op: "drop", binding: "observer" },
  ]);
  assert.equal(result.status, "accepted");
  assert.equal(result.state.outcomes.afterRelease, "none");
  assert.equal(result.state.controlBlocks.c0.deinitCount, 1);
  assert.equal(result.state.controlBlocks.c0.blockAlive, false);
  assert.equal(result.state.payloads.p0.dropCount, 1);
});

test("M1 consuming allocation failure drops the source before propagation", () => {
  const result = runMemoryProgram([
    { op: "initialize", binding: "bellState" },
    {
      op: "pin",
      from: "bellState",
      to: "pinned",
      outcome: "allocationError",
      result: "pin",
    },
  ]);
  assert.equal(result.status, "accepted");
  assert.equal(result.state.bindings.bellState.state, "dropped");
  assert.equal(result.state.payloads.p0.dropCount, 1);
  assert.equal(result.state.outcomes.pin, "allocationError");
});

test("M1 erasure derives inline and spill storage from the product policy", () => {
  const allocators = [
    {
      op: "defineAllocator",
      allocator: "request",
      lifetime: "scoped",
      mobility: "local",
      adoptionFamily: "arena",
      limit: 64,
    },
    {
      op: "defineAllocator",
      allocator: "process",
      lifetime: "product",
      mobility: "crossDomain",
      adoptionFamily: "general",
      limit: 64,
    },
  ];

  const inline = runMemoryProgram([
    ...allocators,
    { op: "initialize", binding: "handler", using: "request", bytes: 8 },
    {
      op: "erase",
      from: "handler",
      to: "stored",
      payloadBytes: 16,
      payloadAlignment: 8,
      inlineBytes: 24,
      inlineAlignment: 8,
      spill: "allocator",
      using: "process",
      boxBytes: 32,
    },
  ]);
  assert.equal(inline.status, "accepted");
  assert.equal(inline.state.payloads.p0.erasure.storage, "inline");
  assert.equal(inline.state.payloads.p0.erasure.boxOrigin, null);
  assert.deepEqual(inline.state.payloads.p0.storageOrigins, ["request"]);
  assert.equal(inline.state.allocators.process.charged, 0);

  const spill = runMemoryProgram([
    ...allocators,
    { op: "initialize", binding: "handler", using: "request", bytes: 8 },
    {
      op: "erase",
      from: "handler",
      to: "stored",
      payloadBytes: 64,
      payloadAlignment: 8,
      inlineBytes: 24,
      inlineAlignment: 8,
      spill: "allocator",
      using: "process",
      boxBytes: 32,
    },
  ]);
  assert.equal(spill.status, "accepted");
  assert.equal(spill.state.payloads.p0.erasure.storage, "spill");
  assert.equal(spill.state.payloads.p0.erasure.boxOrigin, "process");
  assert.deepEqual(spill.state.payloads.p0.storageOrigins, ["process", "request"]);
  assert.equal(spill.state.allocators.process.charged, 32);
});

test("M1 erasure failure consumes once and forbidden spill preserves the owner", () => {
  const allocator = {
    op: "defineAllocator",
    allocator: "process",
    lifetime: "product",
    mobility: "crossDomain",
    adoptionFamily: "general",
    limit: 64,
  };
  const failed = runMemoryProgram([
    allocator,
    { op: "initialize", binding: "handler" },
    {
      op: "erase",
      from: "handler",
      to: "stored",
      payloadBytes: 64,
      payloadAlignment: 8,
      inlineBytes: 24,
      inlineAlignment: 8,
      spill: "allocator",
      using: "process",
      boxBytes: 32,
      outcome: "allocationError",
      result: "erase",
    },
  ]);
  assert.equal(failed.status, "accepted");
  assert.equal(failed.state.bindings.handler.state, "dropped");
  assert.equal(failed.state.bindings.stored, undefined);
  assert.equal(failed.state.payloads.p0.dropCount, 1);
  assert.equal(failed.state.outcomes.erase, "allocationError");

  const exhausted = runMemoryProgram([
    { ...allocator, limit: 16 },
    { op: "initialize", binding: "handler" },
    {
      op: "erase",
      from: "handler",
      to: "stored",
      payloadBytes: 64,
      payloadAlignment: 8,
      inlineBytes: 24,
      inlineAlignment: 8,
      spill: "allocator",
      using: "process",
      boxBytes: 32,
      result: "erase",
    },
  ]);
  assert.equal(exhausted.status, "accepted");
  assert.equal(exhausted.state.bindings.handler.state, "dropped");
  assert.equal(exhausted.state.payloads.p0.dropCount, 1);
  assert.equal(exhausted.state.outcomes.erase, "allocationError");

  const forbidden = runMemoryProgram([
    { op: "initialize", binding: "handler" },
    {
      op: "erase",
      from: "handler",
      to: "stored",
      payloadBytes: 64,
      payloadAlignment: 8,
      inlineBytes: 24,
      inlineAlignment: 8,
      spill: "forbid",
      boxBytes: 0,
    },
  ]);
  assert.equal(forbidden.code, "erasureSpillForbidden");
  assert.equal(forbidden.state.bindings.handler.state, "owned");
  assert.equal(forbidden.state.bindings.stored, undefined);
});
