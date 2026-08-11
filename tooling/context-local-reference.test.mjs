import assert from "node:assert/strict"
import test from "node:test"
import { runContextLocalOperations } from "./context-local-machine.mjs"

function taskKey() {
  return {
    op: "declareTaskKey",
    key: "order",
    symbol: "Trace.requestId",
    type: "OrderId?",
    context: "associatedConst",
    defaultConst: true,
    shareable: true,
    default: "none",
  }
}

function threadKey() {
  return {
    op: "declareThreadKey",
    key: "samples",
    symbol: "NativeCounters.samples",
    type: "u64",
    context: "associatedConst",
    initialConst: true,
    copy: true,
    hasDrop: false,
    nativeTls: true,
    initial: 0,
  }
}

test("a child captures the binding at creation", () => {
  const result = runContextLocalOperations([
    taskKey(),
    { op: "beginTaskScope", task: "root", scope: "outer", key: "order", value: "one", ownership: "move" },
    { op: "spawnChild", parent: "root", child: "cook", scope: "outer", form: "spawn" },
    { op: "beginTaskScope", task: "root", scope: "inner", key: "order", value: "two", ownership: "copy", copyable: true },
    { op: "getTaskLocal", task: "root", key: "order" },
    { op: "getTaskLocal", task: "cook", key: "order" },
  ])
  assert.equal(result.status, "accepted")
  assert.deepEqual(result.state.taskReads.map((read) => read.value), ["two", "one"])
  assert.notEqual(result.state.taskReads[0].bindingId, result.state.taskReads[1].bindingId)
})

test("scope pop waits for structured child drain", () => {
  const blocked = runContextLocalOperations([
    taskKey(),
    { op: "beginTaskScope", task: "root", scope: "outer", key: "order", value: "one", ownership: "move" },
    { op: "spawnChild", parent: "root", child: "cook", scope: "outer", form: "asyncLet" },
    { op: "closeTaskScope", task: "root", scope: "outer", operationSettled: true, dependenciesClosed: true, outcome: "success" },
  ])
  assert.equal(blocked.error, "W-CONTEXT-0006")

  const drained = runContextLocalOperations([
    taskKey(),
    { op: "beginTaskScope", task: "root", scope: "outer", key: "order", value: "one", ownership: "move" },
    { op: "spawnChild", parent: "root", child: "cook", scope: "outer", form: "asyncLet" },
    { op: "settleChild", child: "cook" },
    { op: "closeTaskScope", task: "root", scope: "outer", operationSettled: true, dependenciesClosed: true, outcome: "success" },
  ])
  assert.equal(drained.status, "accepted")
  assert.deepEqual(drained.state.activeBindings, [])
})

test("service boundaries receive the default instead of ambient authority", () => {
  const result = runContextLocalOperations([
    taskKey(),
    { op: "beginTaskScope", task: "root", scope: "outer", key: "order", value: "one", ownership: "move" },
    { op: "crossBoundary", parent: "root", target: "service", kind: "service" },
    { op: "getTaskLocal", task: "service", key: "order" },
  ])
  assert.equal(result.state.taskReads[0].value, "none")
  assert.equal(result.state.taskReads[0].bindingId, null)
})

test("a migrated task observes physical thread slots", () => {
  const result = runContextLocalOperations([
    threadKey(),
    { op: "placeTaskOnThread", task: "root", thread: "left" },
    { op: "writeThreadLocal", task: "root", key: "samples", action: "increment" },
    { op: "placeTaskOnThread", task: "root", thread: "right" },
    { op: "readThreadLocal", task: "root", key: "samples" },
    { op: "placeTaskOnThread", task: "root", thread: "left" },
    { op: "readThreadLocal", task: "root", key: "samples" },
  ])
  assert.deepEqual(result.state.threadReads.map((read) => read.value), [0, 1])
  assert.deepEqual(result.state.threadReads.map((read) => read.thread), ["right", "left"])
})

test("TLS mutation is not transactional", () => {
  const result = runContextLocalOperations([
    threadKey(),
    { op: "placeTaskOnThread", task: "root", thread: "native" },
    { op: "writeThreadLocal", task: "root", key: "samples", action: "increment", throws: true },
    { op: "readThreadLocal", task: "root", key: "samples" },
  ])
  assert.equal(result.state.threadWrites[0].outcome, "error")
  assert.equal(result.state.threadReads[0].value, 1)
})

test("TLS rejects suspension and cleanup obligations", () => {
  const suspension = runContextLocalOperations([
    threadKey(),
    { op: "placeTaskOnThread", task: "root", thread: "native" },
    { op: "writeThreadLocal", task: "root", key: "samples", action: "increment", maySuspend: true },
  ])
  assert.equal(suspension.error, "W-TLS-0003")

  const cleanup = runContextLocalOperations([{
    ...threadKey(),
    type: "OwnedSession",
    hasDrop: true,
  }])
  assert.equal(cleanup.error, "W-TLS-0001")
})
