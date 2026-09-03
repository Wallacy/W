import assert from "node:assert/strict"
import test from "node:test"
import { runLazyBehaviorOperations } from "./lazy-behavior-machine.mjs"

function declare(lowering) {
  return {
    op: "declare",
    lowering,
    initializerEffects: ["compute", "allocate"],
    capturesOwned: true,
    ...(lowering === "concurrent"
      ? {
          capturesTransferable: true,
          capturesLifetimeIndependent: true,
          valueShareable: true,
        }
      : {}),
  }
}

function access(actor, lowering) {
  return {
    op: "beginAccess",
    actor,
    ...(lowering === "concurrent" ? { blockingDomain: true } : {}),
  }
}

test("all lowerings preserve one logical initializer result", () => {
  const results = ["local", "isolated", "concurrent"].map((lowering) =>
    runLazyBehaviorOperations([
      declare(lowering),
      access("winner", lowering),
      { op: "publish", actor: "winner", value: "prices-v1" },
      {
        op: "read",
        actor: "reader",
        ...(lowering === "concurrent" ? { blockingDomain: true } : {}),
      },
      { op: "close" },
    ]),
  )
  for (const result of results) {
    assert.equal(result.status, "accepted")
    assert.equal(result.state.initializerRuns, 1)
    assert.equal(result.state.captureDrops, 1)
    assert.equal(result.state.valueDrops, 1)
    assert.deepEqual(
      result.state.observations.map((observation) => observation.value),
      ["prices-v1"],
    )
  }
  assert.equal(new Set(results.map((result) => result.physical.trace.join("|"))).size, 3)
})

test("one concurrent winner publishes to all waiters", () => {
  const result = runLazyBehaviorOperations([
    declare("concurrent"),
    access("winner", "concurrent"),
    access("left", "concurrent"),
    access("right", "concurrent"),
    { op: "publish", actor: "winner", value: "prices-v1" },
    { op: "resumeAccess", actor: "left", blockingDomain: true },
    { op: "resumeAccess", actor: "right", blockingDomain: true },
  ])
  assert.equal(result.state.initializerRuns, 1)
  assert.deepEqual(result.state.waiterPhases, { left: "resumed", right: "resumed" })
  assert.deepEqual(result.state.happensBefore, [
    "publish:1->read:left",
    "publish:1->read:right",
  ])
})

test("a non-blocking domain needs a proof", () => {
  const rejected = runLazyBehaviorOperations([
    declare("concurrent"),
    { op: "beginAccess", actor: "reader" },
  ])
  assert.equal(rejected.error, "W-LAZY-0002")

  const accepted = runLazyBehaviorOperations([
    declare("concurrent"),
    { op: "set", exclusive: true, value: "prices-v1" },
    { op: "read", actor: "reader", initializedBeforePublication: true },
  ])
  assert.equal(accepted.status, "accepted")
})

test("operational initializer effects require a named API", () => {
  for (const effect of ["suspend", "throws", "io", "network", "service", "device", "blocking", "authority"]) {
    const result = runLazyBehaviorOperations([
      {
        ...declare("local"),
        initializerEffects: [effect],
      },
    ])
    assert.equal(result.error, "W-LAZY-0001")
  }
})

test("cancellation is observed only after publication", () => {
  const result = runLazyBehaviorOperations([
    declare("concurrent"),
    access("winner", "concurrent"),
    access("waiter", "concurrent"),
    { op: "requestCancellation", actor: "waiter" },
    { op: "publish", actor: "winner", value: "prices-v1" },
    { op: "resumeAccess", actor: "waiter", blockingDomain: true },
    { op: "observeCancellation", actor: "waiter" },
  ])
  assert.equal(result.status, "accepted")
  assert.deepEqual(result.state.cancellations, { waiter: "observed" })
})

test("exclusive assignment supersedes the initializer", () => {
  const result = runLazyBehaviorOperations([
    declare("local"),
    { op: "set", exclusive: true, value: "manual-v1" },
    { op: "read", actor: "main" },
  ])
  assert.equal(result.state.initializerRuns, 0)
  assert.equal(result.state.captureDrops, 1)
  assert.equal(result.state.observations[0].value, "manual-v1")
})

test("get mut ref initializes before exposing a mutable borrow", () => {
  const result = runLazyBehaviorOperations([
    declare("local"),
    {
      op: "getMutRef",
      exclusive: true,
      initializerValue: "prices-v1",
      value: "prices-v2",
    },
  ])
  assert.equal(result.state.initializerRuns, 1)
  assert.equal(result.state.mutableBorrowCount, 1)
  assert.equal(result.state.value, "prices-v2")
})

test("drop selects captures or value from the logical state", () => {
  const unused = runLazyBehaviorOperations([declare("local"), { op: "close" }])
  assert.equal(unused.state.captureDrops, 1)
  assert.equal(unused.state.valueDrops, 0)

  const initialized = runLazyBehaviorOperations([
    declare("local"),
    access("main", "local"),
    { op: "publish", actor: "main", value: "prices-v1" },
    { op: "close" },
  ])
  assert.equal(initialized.state.captureDrops, 1)
  assert.equal(initialized.state.valueDrops, 1)
})

test("dynamic reentry faults instead of deadlocking", () => {
  const result = runLazyBehaviorOperations([
    declare("concurrent"),
    access("winner", "concurrent"),
    access("winner", "concurrent"),
  ])
  assert.equal(result.status, "fault")
  assert.equal(result.error, "W-LAZY-0003")
  assert.equal(result.state.boundary, "faulted")
})

test("initializer panic does not publish or double-drop", () => {
  const result = runLazyBehaviorOperations([
    declare("local"),
    access("winner", "local"),
    { op: "panic", actor: "winner" },
  ])
  assert.equal(result.status, "fault")
  assert.equal(result.state.publication, 0)
  assert.equal(result.state.captureDrops, 1)
  assert.equal(result.state.valueDrops, 0)
})

test("initializer OOM follows the fault boundary without publication", () => {
  const result = runLazyBehaviorOperations([
    declare("local"),
    access("winner", "local"),
    { op: "allocationFault", actor: "winner" },
  ])
  assert.equal(result.status, "fault")
  assert.equal(result.error, "lazyInitializerOutOfMemory")
  assert.equal(result.state.publication, 0)
  assert.equal(result.state.captureDrops, 1)
})

test("concurrent lowering rejects invalid mobility facts", () => {
  const result = runLazyBehaviorOperations([
    {
      ...declare("concurrent"),
      capturesLifetimeIndependent: false,
    },
  ])
  assert.equal(result.status, "rejected")
  assert.equal(result.error, "W-LAZY-0005")
})
