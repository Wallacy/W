import assert from "node:assert/strict";
import test from "node:test";

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

test("a suspended borrow needs stable storage, while pinning preserves its address", () => {
  const ordinary = beginBorrow(owner());
  assert.equal(canSuspend(ordinary.owner), false);

  const pinned = beginBorrow(owner({ address: ADDRESS_STATE.published }));
  assert.equal(canSuspend(pinned.owner), true);

  assert.throws(() => moveOwner(pinned.owner), {
    code: "moveWithActiveBorrow",
  });
  const released = endBorrow(pinned.owner, pinned.token);
  const movedHandle = moveOwner(released);
  assert.equal(movedHandle.destination.address, ADDRESS_STATE.published);
  assert.equal(movedHandle.source.address, ADDRESS_STATE.published);
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
