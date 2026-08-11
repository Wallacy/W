import assert from "node:assert/strict"
import test from "node:test"
import { runSnapshotCellOperations } from "./snapshot-cell-machine.mjs"

function create(strategy = "reference-counting") {
  return {
    op: "create",
    strategy,
    value: "v0",
    owned: true,
    transferable: true,
    shareable: true,
    lifetimeIndependent: true,
  }
}

function publish(value, allocation = undefined) {
  return {
    op: "publish",
    value,
    owned: true,
    transferable: true,
    shareable: true,
    lifetimeIndependent: true,
    ...(allocation ? { allocation } : {}),
  }
}

test("an old reader keeps one complete retired version", () => {
  const result = runSnapshotCellOperations([
    create("epoch"),
    { op: "beginRead", reader: "old" },
    publish("v1"),
    { op: "beginRead", reader: "new" },
    { op: "finishRead", reader: "new", outcome: "success" },
  ])
  assert.equal(result.status, "accepted")
  assert.deepEqual(result.state.observations.map((item) => item.value), ["v0", "v1"])
  assert.equal(result.state.versions[0].phase, "retired")
  assert.equal(result.state.versions[1].phase, "current")
})

test("the last reader reclaims a retired version once", () => {
  const result = runSnapshotCellOperations([
    create("hazard"),
    { op: "beginRead", reader: "left" },
    { op: "beginRead", reader: "right" },
    publish("v1"),
    { op: "finishRead", reader: "left", outcome: "success" },
    { op: "finishRead", reader: "right", outcome: "error" },
  ])
  assert.deepEqual(result.state.drops, [0])
  assert.equal(result.state.versions[0].dropCount, 1)
})

test("retained versions are bounded by active readers", () => {
  const result = runSnapshotCellOperations([
    create("epoch"),
    { op: "beginRead", reader: "old" },
    publish("v1"),
    { op: "beginRead", reader: "middle" },
    publish("v2"),
    { op: "finishRead", reader: "old", outcome: "success" },
    { op: "finishRead", reader: "middle", outcome: "success" },
  ])
  assert.deepEqual(result.state.drops, [0, 1])
  assert.deepEqual(
    Object.values(result.state.versions).map((version) => version.phase),
    ["reclaimed", "reclaimed", "current"],
  )
})

test("publication failure preserves the previous version", () => {
  const result = runSnapshotCellOperations([create(), publish("v1", "fault")])
  assert.equal(result.status, "fault")
  assert.equal(result.error, "snapshotPublicationAllocationFault")
  assert.equal(result.state.currentVersion, 0)
  assert.equal(result.state.currentValue, "v0")
  assert.deepEqual(result.state.publicationOrder, [])
})

test("publication order is total and creates read edges", () => {
  const result = runSnapshotCellOperations([
    create(),
    publish("v1"),
    publish("v2"),
    { op: "beginRead", reader: "reader" },
    { op: "finishRead", reader: "reader", outcome: "success" },
  ])
  assert.deepEqual(result.state.publicationOrder, [1, 2])
  assert.deepEqual(result.state.happensBefore, ["publish:2->read:reader"])
  assert.equal(result.state.currentValue, "v2")
})

test("all provider strategies preserve the same logical result", () => {
  const results = ["reference-counting", "epoch", "hazard", "lock"].map((strategy) => {
    const result = runSnapshotCellOperations([
      create(strategy),
      publish("v1"),
      { op: "beginRead", reader: "reader" },
      { op: "finishRead", reader: "reader", outcome: "success" },
      { op: "close" },
    ])
    return result
  })
  const projections = results.map((result) => result.state)
  assert.deepEqual(projections[0], projections[1])
  assert.deepEqual(projections[1], projections[2])
  assert.deepEqual(projections[2], projections[3])
  assert.equal(new Set(results.map((result) => result.physical.trace.join("|"))).size, 4)
  assert.equal(results[3].physical.trace.includes("lock:publish"), true)
  assert.equal(results[3].physical.trace.includes("unlock:publish"), true)
})

test("a dependency cannot escape or suspend", () => {
  const escape = runSnapshotCellOperations([
    create(),
    { op: "beginRead", reader: "reader" },
    { op: "escapeRead", reader: "reader" },
  ])
  assert.equal(escape.error, "W-SNAPSHOT-0001")
  const suspension = runSnapshotCellOperations([
    create(),
    { op: "beginRead", reader: "reader" },
    { op: "suspendRead", reader: "reader" },
  ])
  assert.equal(suspension.error, "W-SNAPSHOT-0002")
})
