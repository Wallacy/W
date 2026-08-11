import assert from "node:assert/strict"
import test from "node:test"
import { runOwnershipExecutionOperations } from "./ownership-execution-machine.mjs"

function owner(id, overrides = {}) {
  return {
    op: "createOwner",
    owner: id,
    value: id,
    transferable: true,
    shareable: false,
    duplicable: false,
    ...overrides,
  }
}

function takeTask(task, form = "asyncLet", strategy = "queued") {
  return {
    op: "stageChild",
    task,
    form,
    ...(form === "spawn" ? { domain: "compute" } : {}),
    strategy,
    captures: [{ mode: "take", owner: "course" }],
  }
}

test("direct call and await keep one current-task owner graph", () => {
  const result = runOwnershipExecutionOperations([
    owner("direct", { shareable: true }),
    owner("awaited", { shareable: false }),
    { op: "directAccess", owner: "direct", access: "read" },
    {
      op: "suspendParent",
      owner: "awaited",
      mode: "ref",
      referentStable: true,
      originSurvives: true,
    },
    { op: "resumeParent" },
    { op: "dropOwner", owner: "direct" },
    { op: "dropOwner", owner: "awaited" },
    { op: "closeScope" },
  ])
  assert.equal(result.status, "accepted")
  assert.deepEqual(result.state.tasks, {})
  assert.equal(result.state.scope, "closed")
})

test("await suspends only the parent while an authorized child reader can progress", () => {
  const prefix = [
    owner("course", { shareable: true }),
    {
      op: "stageChild",
      task: "reader",
      form: "spawn",
      domain: "compute",
      strategy: "queued",
      captures: [{ mode: "ref", owner: "course", referentStable: true, originSurvives: true }],
    },
    { op: "admitChild", task: "reader", accepted: true },
    { op: "startChild", task: "reader" },
    {
      op: "suspendParent",
      owner: "course",
      mode: "ref",
      referentStable: true,
      originSurvives: true,
    },
  ]
  const childProgress = runOwnershipExecutionOperations([
    ...prefix,
    { op: "childAccess", task: "reader", owner: "course", access: "read" },
  ])
  assert.equal(childProgress.status, "accepted")
  assert.equal(childProgress.state.parentSuspended, true)

  const parentProgress = runOwnershipExecutionOperations([
    ...prefix,
    { op: "directAccess", owner: "course", access: "read" },
  ])
  assert.equal(parentProgress.error, "compositionParentSuspended")
})

test("a moved capture becomes a new binding only after cleanup and join", () => {
  const result = runOwnershipExecutionOperations([
    owner("course"),
    takeTask("child"),
    { op: "admitChild", task: "child", accepted: true },
    { op: "startChild", task: "child" },
    { op: "settleChild", task: "child", outcome: "success", returnOwners: ["course"] },
    { op: "finishCleanup", task: "child" },
    { op: "joinChild", task: "child", bindings: { course: "result" } },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.owners.course, undefined)
  assert.equal(result.state.owners.result.location, "parent")
  assert.deepEqual(result.state.happensBefore, ["stage:child->start:child", "commit:child->join:child"])
})

test("budget rejection drops a taken staging owner without starting the body", () => {
  const result = runOwnershipExecutionOperations([
    owner("course"),
    takeTask("child"),
    { op: "admitChild", task: "child", accepted: false },
    { op: "joinChild", task: "child" },
    { op: "closeScope" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.tasks.child.bodyStarted, false)
  assert.equal(result.state.tasks.child.outcome, "canceled")
  assert.deepEqual(result.state.drops.map((drop) => drop.owner), ["course"])
})

test("an inout child blocks the parent until join", () => {
  const prefix = [
    owner("ledger"),
    {
      op: "stageChild",
      task: "update",
      form: "asyncLet",
      strategy: "queued",
      captures: [{
        mode: "inout",
        owner: "ledger",
        referentStable: true,
        originSurvives: true,
      }],
    },
    { op: "admitChild", task: "update", accepted: true },
    { op: "startChild", task: "update" },
  ]
  const blocked = runOwnershipExecutionOperations([
    ...prefix,
    { op: "directAccess", owner: "ledger", access: "read" },
  ])
  assert.equal(blocked.error, "W-BORROW-0002")

  const joined = runOwnershipExecutionOperations([
    ...prefix,
    { op: "childAccess", task: "update", owner: "ledger", access: "write", value: 1 },
    { op: "settleChild", task: "update", outcome: "success" },
    { op: "finishCleanup", task: "update" },
    { op: "joinChild", task: "update" },
    { op: "directAccess", owner: "ledger", access: "read" },
  ])
  assert.equal(joined.status, "accepted")
  assert.equal(joined.state.owners.ledger.value, 1)
  assert.equal(joined.state.loans["loan-1"].phase, "released")
})

test("cancellation commits only after captured owners are cleaned", () => {
  const result = runOwnershipExecutionOperations([
    owner("course"),
    takeTask("child"),
    { op: "admitChild", task: "child", accepted: true },
    { op: "startChild", task: "child" },
    { op: "cancelChild", task: "child" },
    { op: "settleChild", task: "child", outcome: "canceled" },
    { op: "finishCleanup", task: "child" },
    { op: "joinChild", task: "child" },
  ])
  const dropIndex = result.state.events.indexOf("drop:course:cleanup:child")
  const commitIndex = result.state.events.indexOf("task:child:committed")
  const joinIndex = result.state.events.indexOf("task:child:joined")
  assert.equal(result.status, "accepted")
  assert.equal(dropIndex < commitIndex && commitIndex < joinIndex, true)
})

test("a shared ref permits reads but keeps drop reserved until join", () => {
  const prefix = [
    owner("course", { shareable: true }),
    {
      op: "stageChild",
      task: "reader",
      form: "spawn",
      domain: "compute",
      strategy: "queued",
      captures: [{ mode: "ref", owner: "course", referentStable: true, originSurvives: true }],
    },
    { op: "admitChild", task: "reader", accepted: true },
    { op: "startChild", task: "reader" },
  ]
  const readable = runOwnershipExecutionOperations([
    ...prefix,
    { op: "directAccess", owner: "course", access: "read" },
  ])
  assert.equal(readable.status, "accepted")

  const dropped = runOwnershipExecutionOperations([
    ...prefix,
    { op: "dropOwner", owner: "course" },
  ])
  assert.equal(dropped.error, "W-BORROW-0008")
})

test("inline and queued execution preserve the same logical projection", () => {
  const run = (strategy) => runOwnershipExecutionOperations([
    owner("course"),
    takeTask("child", "spawn", strategy),
    { op: "admitChild", task: "child", accepted: true },
    { op: "startChild", task: "child" },
    { op: "settleChild", task: "child", outcome: "success" },
    { op: "finishCleanup", task: "child" },
    { op: "joinChild", task: "child" },
    { op: "closeScope" },
  ])
  const inline = run("inline")
  const queued = run("queued")
  assert.deepEqual(inline.state, queued.state)
  assert.notDeepEqual(inline.physical, queued.physical)
})

test("join cannot observe an owner before cleanup commits", () => {
  const result = runOwnershipExecutionOperations([
    owner("course"),
    takeTask("child"),
    { op: "admitChild", task: "child", accepted: true },
    { op: "startChild", task: "child" },
    { op: "settleChild", task: "child", outcome: "success", returnOwners: ["course"] },
    { op: "joinChild", task: "child", bindings: { course: "result" } },
  ])
  assert.equal(result.status, "rejected")
  assert.equal(result.error, "compositionJoinBeforeCleanup")
})

test("evaluation failure cleans staging and never publishes a handle", () => {
  const result = runOwnershipExecutionOperations([
    owner("course"),
    takeTask("child"),
    { op: "failEvaluation", task: "child" },
    { op: "closeScope" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.tasks.child.phase, "evaluationFailed")
  assert.equal(result.state.tasks.child.handleConsumed, false)
  assert.deepEqual(result.state.drops.map((drop) => drop.owner), ["course"])
})

test("a scope cannot close over an orphan child", () => {
  const result = runOwnershipExecutionOperations([
    owner("course"),
    takeTask("child"),
    { op: "admitChild", task: "child", accepted: true },
    { op: "closeScope" },
  ])
  assert.equal(result.status, "rejected")
  assert.equal(result.error, "compositionScopeHasUnjoinedChild")
})

test("scope exit cancels, drains, joins, and drops cooperative children", () => {
  const result = runOwnershipExecutionOperations([
    owner("course"),
    takeTask("child"),
    { op: "admitChild", task: "child", accepted: true },
    { op: "startChild", task: "child" },
    { op: "scopeExit", cooperative: true },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.scope, "closed")
  assert.equal(result.state.tasks.child.phase, "joined")
  assert.equal(result.state.tasks.child.outcome, "canceled")
  assert.deepEqual(result.state.drops.map((drop) => drop.owner), ["course"])
})

test("scope exit preflight rejects without partially mutating earlier children", () => {
  const result = runOwnershipExecutionOperations([
    owner("staged"),
    owner("running"),
    {
      op: "stageChild",
      task: "stagedTask",
      form: "asyncLet",
      strategy: "queued",
      captures: [{ mode: "take", owner: "staged" }],
    },
    {
      op: "stageChild",
      task: "runningTask",
      form: "asyncLet",
      strategy: "queued",
      captures: [{ mode: "take", owner: "running" }],
    },
    { op: "admitChild", task: "runningTask", accepted: true },
    { op: "startChild", task: "runningTask" },
    { op: "scopeExit", cooperative: false },
  ])
  assert.equal(result.error, "compositionScopeExitNeedsProgress")
  assert.equal(result.state.tasks.stagedTask.phase, "staged")
  assert.equal(result.state.tasks.runningTask.phase, "active")
  assert.equal(result.state.tasks.runningTask.cancelRequested, false)
  assert.equal(result.state.owners.staged.location, "staging:stagedTask")
  assert.deepEqual(result.state.drops, [])
})

test("scope exit consumes an ignored successful result exactly once", () => {
  const result = runOwnershipExecutionOperations([
    owner("course"),
    takeTask("child"),
    { op: "admitChild", task: "child", accepted: true },
    { op: "startChild", task: "child" },
    { op: "settleChild", task: "child", outcome: "success", returnOwners: ["course"] },
    { op: "finishCleanup", task: "child" },
    { op: "scopeExit", cooperative: false },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.tasks.child.phase, "joined")
  assert.equal(result.state.owners.course.dropCount, 1)
  assert.deepEqual(result.state.drops, [{ owner: "course", reason: "scope-exit-result:child" }])
})
