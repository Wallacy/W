import assert from "node:assert/strict"
import test from "node:test"
import { runScopedLockOperations, selectSynchronization } from "./scoped-lock-machine.mjs"

const validBody = {
  neverSuspend: true,
  neverThrow: true,
  nonBlocking: true,
  resultIndependent: true,
}

function create(value = 0) {
  return {
    op: "declareShared",
    owner: "ledger",
    form: "declaration",
    context: "binding",
    explicitSharedType: true,
    source: "temporary",
    lifetimeIndependent: true,
    boundary: "restaurant",
    value,
  }
}

function request(task, form = "await", access = "ref") {
  return {
    op: "request",
    owner: "ledger",
    task,
    form,
    access,
    boundary: "restaurant",
    ...(form === "sync" ? { blockingAllowed: true } : {}),
    body: validBody,
  }
}

test("an explicit shared declaration is the allocation operation", () => {
  const result = runScopedLockOperations([create("menu")])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.owners.ledger.allocation, "product.default")
  assert.deepEqual(result.state.receipts, [
    { operation: "shared", owner: "ledger", allocation: "product.default" },
  ])
})

test("context does not promote an argument or return to shared", () => {
  const result = runScopedLockOperations([{ ...create(), context: "return" }])
  assert.equal(result.error, "W-OWNERSHIP-0013")
  assert.deepEqual(result.state.owners, {})
})

test("custom allocation share calls are retired while construction remains blocked", () => {
  const result = runScopedLockOperations([{
    op: "share",
    owner: "ledger",
    form: "share",
    explicitOperation: true,
    source: "binding",
    take: true,
    lifetimeIndependent: true,
    allocator: "request.arena",
    recoverable: true,
    boundary: "restaurant",
    value: "menu",
  }])
  assert.equal(result.error, "retiredSharedConstructionCall")
  assert.deepEqual(result.state.owners, {})
})

test("unlock publishes a release acquire edge", () => {
  const result = runScopedLockOperations([
    create(),
    request("writer", "sync", "inout"),
    { op: "grant", owner: "ledger", task: "writer" },
    { op: "write", owner: "ledger", task: "writer", value: 1 },
    { op: "finish", owner: "ledger", task: "writer", resultIndependent: true },
    request("reader"),
    { op: "grant", owner: "ledger", task: "reader" },
  ])
  assert.equal(result.state.owners.ledger.value, 1)
  assert.equal(result.state.owners.ledger.happensBefore.includes("unlock:0->grant:1"), true)
})

test("provider admission is not a language FIFO promise", () => {
  const result = runScopedLockOperations([
    create(),
    request("first"),
    request("second"),
    { op: "grant", owner: "ledger", task: "second" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.owners.ledger.holder, "second")
  assert.deepEqual(result.state.owners.ledger.waiters, ["first"])
})

test("try lock does not evaluate or consume its body while busy", () => {
  const result = runScopedLockOperations([
    create("owned"),
    {
      op: "tryAcquire",
      owner: "ledger",
      task: "reader",
      access: "ref",
      boundary: "restaurant",
      providerBusy: true,
      body: validBody,
    },
  ])
  assert.equal(result.state.owners.ledger.bodyEvaluations, 0)
  assert.equal(result.state.owners.ledger.value, "owned")
  assert.equal(result.state.receipts.at(-1).result, "busy")
})

test("await cancellation drains before grant and defers after grant", () => {
  const before = runScopedLockOperations([
    create(),
    request("cancelled"),
    { op: "cancelWait", owner: "ledger", task: "cancelled" },
  ])
  assert.deepEqual(before.state.owners.ledger.waiters, [])

  const after = runScopedLockOperations([
    create(),
    request("writer", "await", "inout"),
    { op: "grant", owner: "ledger", task: "writer" },
    { op: "cancelHeld", owner: "ledger", task: "writer" },
    { op: "finish", owner: "ledger", task: "writer", resultIndependent: true },
  ])
  assert.equal(after.state.owners.ledger.outcomes[0].cancellation, "after-unlock")
})

test("the body cannot suspend throw block or escape", () => {
  for (const [field, code] of [
    ["neverSuspend", "W-LOCK-0002"],
    ["neverThrow", "W-LOCK-0011"],
    ["nonBlocking", "W-LOCK-0012"],
    ["resultIndependent", "W-LOCK-0001"],
  ]) {
    const result = runScopedLockOperations([
      create(),
      {
        op: "tryAcquire",
        owner: "ledger",
        task: "reader",
        access: "ref",
        boundary: "restaurant",
        body: { ...validBody, [field]: false },
      },
    ])
    assert.equal(result.error, code)
  }
})

test("a panic fails the whole fault boundary", () => {
  const result = runScopedLockOperations([
    create(),
    {
      op: "tryAcquire",
      owner: "ledger",
      task: "writer",
      access: "inout",
      boundary: "restaurant",
      body: validBody,
    },
    { op: "panic", owner: "ledger", task: "writer" },
  ])
  assert.equal(result.status, "fault")
  assert.deepEqual(result.state.failedBoundaries, ["restaurant"])
})

test("selection prefers W ownership and execution primitives", () => {
  assert.equal(selectSynchronization({ uniqueOwner: true }), "owner")
  assert.equal(selectSynchronization({ scalar: true, singleLocation: true }), "atomic")
  assert.equal(selectSynchronization({ taskOwnedMutableState: true }), "serial-domain")
  assert.equal(
    selectSynchronization({ parallelReads: true, exclusiveTaskWrite: true, closedAccessGraph: true }),
    "domain-barrier",
  )
  assert.equal(selectSynchronization({ immutableVersions: true, readHeavy: true }), "snapshot-cell")
  assert.equal(
    selectSynchronization({ synchronousForeign: true, shortCriticalSection: true, sameBoundary: true }),
    "language-lock",
  )
  assert.equal(selectSynchronization({ readWriteLockRequested: true }), "rejected-read-write-lock")
})

test("ordinary overlapping access cannot bypass the language gate", () => {
  const result = runScopedLockOperations([create(), { op: "unguardedOverlap", owner: "ledger" }])
  assert.equal(result.error, "W-LOCK-0013")
})
