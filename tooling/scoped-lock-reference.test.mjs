import assert from "node:assert/strict"
import test from "node:test"
import { runScopedLockOperations, selectSynchronization } from "./scoped-lock-machine.mjs"

function create(kind = "async", value = 0) {
  return {
    op: "create",
    lock: "ledger",
    kind,
    boundary: "restaurant",
    value,
    owned: true,
    transferable: true,
    lifetimeIndependent: true,
    shareable: false,
  }
}

function request(task, access = "ref", blockingAllowed = undefined) {
  return {
    op: "request",
    lock: "ledger",
    task,
    access,
    boundary: "restaurant",
    ...(blockingAllowed === undefined ? {} : { blockingAllowed }),
  }
}

test("a protected payload need not be shareable", () => {
  const result = runScopedLockOperations([
    create("sync", "owner-only"),
    { op: "tryAcquire", lock: "ledger", task: "reader", access: "ref", boundary: "restaurant" },
    { op: "read", lock: "ledger", task: "reader" },
    { op: "finish", lock: "ledger", task: "reader", outcome: "success" },
  ])
  assert.equal(result.status, "accepted")
  assert.deepEqual(result.state.reads, ["owner-only"])
})

test("async admission is FIFO and creates unlock to grant edges", () => {
  const result = runScopedLockOperations([
    create(),
    request("first", "inout"),
    request("second", "inout"),
    { op: "grant", lock: "ledger", task: "first" },
    { op: "write", lock: "ledger", task: "first", value: 1 },
    { op: "finish", lock: "ledger", task: "first", outcome: "success" },
    { op: "grant", lock: "ledger", task: "second" },
    { op: "write", lock: "ledger", task: "second", value: 2 },
    { op: "finish", lock: "ledger", task: "second", outcome: "success" },
  ])
  const lock = result.state.locks.ledger
  assert.equal(lock.value, 2)
  assert.equal(lock.happensBefore.includes("unlock:0->grant:1"), true)
})

test("cancellation before grant removes a waiter", () => {
  const result = runScopedLockOperations([
    create(),
    request("holder"),
    { op: "grant", lock: "ledger", task: "holder" },
    request("cancelled"),
    request("next"),
    { op: "cancelWait", lock: "ledger", task: "cancelled" },
    { op: "finish", lock: "ledger", task: "holder", outcome: "success" },
    { op: "grant", lock: "ledger", task: "next" },
  ])
  assert.equal(result.state.locks.ledger.holder, "next")
  assert.deepEqual(result.state.locks.ledger.cancellations, ["removed:cancelled"])
})

test("cancellation after grant is observed after unlock", () => {
  const result = runScopedLockOperations([
    create(),
    request("writer", "inout"),
    { op: "grant", lock: "ledger", task: "writer" },
    { op: "cancelHeld", lock: "ledger", task: "writer" },
    { op: "write", lock: "ledger", task: "writer", value: 1 },
    { op: "finish", lock: "ledger", task: "writer", outcome: "success" },
  ])
  assert.equal(result.state.locks.ledger.value, 1)
  assert.equal(result.state.locks.ledger.outcomes[0].cancellation, "after-unlock")
})

test("try acquisition does not bypass a queued ticket", () => {
  const result = runScopedLockOperations([
    create(),
    request("first"),
    { op: "tryAcquire", lock: "ledger", task: "late", access: "ref", boundary: "restaurant" },
  ])
  assert.deepEqual(result.state.receipts.at(-1), {
    operation: "try",
    task: "late",
    result: "busy",
  })
})

test("panic fails the owning fault boundary", () => {
  const result = runScopedLockOperations([
    create("sync"),
    request("writer", "inout", true),
    { op: "grant", lock: "ledger", task: "writer" },
    { op: "panic", lock: "ledger", task: "writer" },
  ])
  assert.equal(result.status, "fault")
  assert.equal(result.state.locks.ledger.phase, "faulted")
  assert.deepEqual(result.state.failedBoundaries, ["restaurant"])
})

test("the synchronization selector keeps distinct architectures distinct", () => {
  assert.equal(selectSynchronization({ scalar: true, singleLocation: true }), "atomic")
  assert.equal(
    selectSynchronization({
      parallelReads: true,
      exclusiveWrite: true,
      staticDomain: true,
      closedAccessGraph: true,
    }),
    "domain-barrier",
  )
  assert.equal(
    selectSynchronization({ readHeavy: true, replaceWholeVersion: true }),
    "snapshot-cell",
  )
  assert.equal(
    selectSynchronization({
      parallelReads: true,
      exclusiveWrite: true,
      synchronousContext: true,
    }),
    "read-write-lock",
  )
  assert.equal(selectSynchronization({ durable: true }), "service")
})

test("read write admission batches only the reader prefix before a writer", () => {
  const result = runScopedLockOperations([
    create("read-write"),
    request("reader-a", "ref", true),
    request("reader-b", "ref", true),
    request("writer", "inout", true),
    request("late-reader", "ref", true),
    { op: "grantPhase", lock: "ledger", tasks: ["reader-a", "reader-b"] },
    { op: "finish", lock: "ledger", task: "reader-b", outcome: "success" },
    { op: "finish", lock: "ledger", task: "reader-a", outcome: "success" },
    { op: "grantPhase", lock: "ledger", task: "writer" },
    { op: "write", lock: "ledger", task: "writer", value: 1 },
    { op: "finish", lock: "ledger", task: "writer", outcome: "success" },
    { op: "grantPhase", lock: "ledger", tasks: ["late-reader"] },
  ])
  const lock = result.state.locks.ledger
  assert.equal(result.status, "accepted")
  assert.equal(lock.holder, null)
  assert.deepEqual(lock.readers, ["late-reader"])
  assert.deepEqual(lock.queue, [])
  assert.deepEqual(lock.closedPhases, [
    { phase: 0, access: "read", tickets: [0, 1] },
    { phase: 1, access: "write", tickets: [2] },
  ])
})

test("a queued writer prevents a late try read from joining active readers", () => {
  const result = runScopedLockOperations([
    create("read-write"),
    { op: "tryAcquire", lock: "ledger", task: "reader", access: "ref", boundary: "restaurant" },
    request("writer", "inout", true),
    { op: "tryAcquire", lock: "ledger", task: "late", access: "ref", boundary: "restaurant" },
  ])
  assert.equal(result.status, "accepted")
  assert.deepEqual(
    result.state.receipts
      .filter((receipt) => receipt.operation === "try")
      .map((receipt) => receipt.result),
    ["acquired", "busy"],
  )
})

test("a blocking reader at the head joins the active reader phase", () => {
  const result = runScopedLockOperations([
    create("read-write", 5),
    { op: "tryAcquire", lock: "ledger", task: "first", access: "ref", boundary: "restaurant" },
    request("second", "ref", true),
    { op: "grantPhase", lock: "ledger", tasks: ["second"] },
  ])
  assert.equal(result.status, "accepted")
  assert.deepEqual(result.state.locks.ledger.readers, ["first", "second"])
})

test("scoped access rejects suspension, escape and reentry", () => {
  const base = [
    create(),
    { op: "tryAcquire", lock: "ledger", task: "reader", access: "ref", boundary: "restaurant" },
  ]
  assert.equal(
    runScopedLockOperations([...base, { op: "suspend", lock: "ledger", task: "reader" }]).error,
    "W-LOCK-0002",
  )
  assert.equal(
    runScopedLockOperations([...base, { op: "escape", lock: "ledger", task: "reader" }]).error,
    "W-LOCK-0001",
  )
  assert.equal(
    runScopedLockOperations([
      ...base,
      { op: "tryAcquire", lock: "ledger", task: "reader", access: "ref", boundary: "restaurant" },
    ]).error,
    "W-LOCK-0004",
  )
})
