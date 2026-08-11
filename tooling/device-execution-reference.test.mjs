import assert from "node:assert/strict"
import test from "node:test"
import { runDeviceExecutionOperations } from "./device-execution-machine.mjs"

const limits = {
  maximumInFlight: 4,
  maximumCommandBytes: 1024,
  maximumArgumentBytes: 1024,
  maximumResultBytes: 2048,
  maximumDependencyEdges: 8,
  maximumRetainedDeviceBytes: 8192,
  maximumCompletionRecords: 8,
  maximumCleanupSteps: 128,
}

function open(overrides = {}) {
  return {
    op: "open",
    moduleIdentity: "module-v1",
    artifactClass: "closed",
    artifactIdentity: "artifact-v1",
    artifactModuleIdentity: "module-v1",
    artifactProviderAbiDigest:
      "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
    providerAbiDigest:
      "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
    artifactTarget: "gpu-target-v1",
    deviceTarget: "gpu-target-v1",
    artifactInstances: ["work-instance-v1"],
    queueId: "q0",
    queueDeviceId: "gpu0",
    deviceId: "gpu0",
    provider: "provider-a",
    providerGeneration: "g1",
    providerResolved: true,
    numericMode: "strict",
    limits,
    ...overrides,
  }
}

function stage(id = "work", overrides = {}) {
  return {
    op: "stage",
    id,
    moduleIdentity: "module-v1",
    queueId: "q0",
    kernelInstanceIdentity: "work-instance-v1",
    arguments: [
      { kind: "tensor", mode: "ref", deviceId: "gpu0", lifetimeStable: true },
    ],
    commandBytes: 32,
    argumentBytes: 32,
    resultBytes: 64,
    retainedDeviceBytes: 128,
    dependencies: [],
    ...overrides,
  }
}

function receipt(id = "work") {
  return {
    issuedBy: "provider",
    generation: "g1",
    moduleIdentity: "module-v1",
    artifactIdentity: "artifact-v1",
    queueId: "q0",
    invocation: id,
  }
}

function successfulLifecycle() {
  return [
    open(),
    stage(),
    { op: "submit", id: "work", receipt: receipt() },
    { op: "start", id: "work" },
    { op: "complete", id: "work", result: "success", receipt: receipt() },
    { op: "drain", id: "work", providerDrained: true },
    { op: "cleanup", id: "work", releasedLoans: true, releasedStaging: true },
    { op: "commit", id: "work" },
    { op: "join", id: "work" },
  ]
}

test("a provider completion publishes only after cleanup and commit", () => {
  const result = runDeviceExecutionOperations(successfulLifecycle())
  assert.equal(result.status, "accepted")
  assert.equal(result.state.artifactIdentity, "artifact-v1")
  assert.deepEqual(result.state.artifactInstances, ["work-instance-v1"])
  assert.equal(result.state.invocations.work.phase, "joined")
  assert.equal(result.state.invocations.work.outcome, "success")
  assert.equal(result.state.invocations.work.cleanupCount, 1)
  assert.deepEqual(result.state.happensBefore, ["device:work->host:work"])
})

test("cancel before submit launches no provider work", () => {
  const result = runDeviceExecutionOperations([
    open(),
    stage(),
    { op: "cancel", id: "work" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.invocations.work.outcome, "cancelled")
  assert.equal(result.physical.trace.length, 0)
})

test("cancel after submit still drains provider work", () => {
  const operations = successfulLifecycle()
  operations.splice(3, 0, { op: "cancel", id: "work" })
  const result = runDeviceExecutionOperations(operations)
  assert.equal(result.state.invocations.work.outcome, "cancelled")
  assert.equal(result.state.invocations.work.cleanupCount, 1)
})

test("queue order cannot replace an explicit dependency", () => {
  const result = runDeviceExecutionOperations([
    open(),
    stage("left"),
    { op: "submit", id: "left", receipt: receipt("left") },
    stage("right", { dependencies: ["left"] }),
    { op: "submit", id: "right", receipt: receipt("right") },
    { op: "start", id: "right" },
  ])
  assert.equal(result.status, "rejected")
  assert.equal(result.error, "W-DEVICE-0005")
})

test("a provider event can release a device dependency before host cleanup", () => {
  const result = runDeviceExecutionOperations([
    open(),
    stage("left"),
    { op: "submit", id: "left", receipt: receipt("left") },
    { op: "complete", id: "left", result: "success", receipt: receipt("left") },
    stage("right", { dependencies: ["left"] }),
    { op: "submit", id: "right", receipt: receipt("right") },
    { op: "start", id: "right" },
  ])
  assert.equal(result.status, "accepted")
  assert.deepEqual(result.state.happensBefore, [
    "device:left->host:left",
    "device:left->device:right",
  ])
})

test("cross-queue order requires a provider receipt", () => {
  const missing = runDeviceExecutionOperations([
    open(),
    { op: "crossQueue", from: "q0", to: "q1" },
  ])
  const issued = runDeviceExecutionOperations([
    open(),
    {
      op: "crossQueue",
      from: "q0",
      to: "q1",
      receipt: { issuedBy: "provider", generation: "g1", from: "q0", to: "q1" },
    },
  ])
  assert.equal(missing.error, "W-DEVICE-0005")
  assert.deepEqual(issued.state.happensBefore, ["queue:q0->queue:q1"])
})

test("device loss retains authority until drain", () => {
  const result = runDeviceExecutionOperations([
    open(),
    stage(),
    { op: "submit", id: "work", receipt: receipt() },
    { op: "complete", id: "work", result: "deviceLost", receipt: receipt() },
  ])
  assert.equal(result.state.phase, "faulted")
  assert.deepEqual(result.state.quarantined, ["work"])
  assert.equal(result.state.invocations.work.outcome, null)
})

test("stale provider completion is quarantined from logical state", () => {
  const result = runDeviceExecutionOperations([
    open(),
    stage(),
    { op: "submit", id: "work", receipt: receipt() },
    { op: "complete", id: "work", result: "deviceLost", receipt: receipt() },
    { op: "drain", id: "work", providerDrained: true },
    { op: "lateCompletion", id: "old", generation: "g0" },
  ])
  assert.deepEqual(result.state.suppressedCompletions, ["old"])
  assert.equal(result.state.invocations.work.phase, "providerDrained")
})

test("host read requires a completed explicit transfer", () => {
  const result = runDeviceExecutionOperations([
    ...successfulLifecycle(),
    { op: "hostRead", id: "work", explicitTransfer: false, hostAddressable: false },
  ])
  assert.equal(result.status, "rejected")
  assert.equal(result.error, "W-DEVICE-0003")
})

test("strict and fast equivalence remain different contracts", () => {
  const strict = runDeviceExecutionOperations([
    open(),
    {
      op: "compare",
      mode: "strict",
      sameModule: true,
      sameLayout: true,
      sameEffects: true,
      memoryPlanProved: true,
      hiddenTransfer: false,
      expected: 1,
      actual: 1.001,
      sameFailurePoint: true,
    },
  ])
  const fast = runDeviceExecutionOperations([
    open(),
    {
      op: "compare",
      mode: "fast",
      sameModule: true,
      sameLayout: true,
      sameEffects: true,
      memoryPlanProved: true,
      hiddenTransfer: false,
      expected: 1,
      actual: 1.001,
      tolerance: 0.01,
    },
  ])
  assert.equal(strict.error, "W-DEVICE-0007")
  assert.deepEqual(fast.state.comparisons, ["fast"])
})

test("caller assertions cannot forge provider receipts", () => {
  const result = runDeviceExecutionOperations([
    open(),
    stage(),
    { op: "submit", id: "work", callerReady: true, receipt: receipt() },
  ])
  assert.equal(result.error, "W-DEVICE-0005")
})

test("scope close refuses an invocation before join", () => {
  const result = runDeviceExecutionOperations([
    open(),
    stage(),
    { op: "closeAdmission" },
    { op: "close" },
  ])
  assert.equal(result.status, "rejected")
  assert.equal(result.error, "W-DEVICE-0004")
})

test("every per-invocation budget rejects before submission", () => {
  const variants = [
    { commandBytes: limits.maximumCommandBytes + 1 },
    { argumentBytes: limits.maximumArgumentBytes + 1 },
    { resultBytes: limits.maximumResultBytes + 1 },
    { retainedDeviceBytes: limits.maximumRetainedDeviceBytes + 1 },
    { dependencies: Array.from({ length: limits.maximumDependencyEdges + 1 }, (_, index) => `d${index}`) },
  ]
  for (const variant of variants) {
    const result = runDeviceExecutionOperations([open(), stage("work", variant)])
    assert.equal(result.error, "W-DEVICE-0008")
    assert.equal(result.state.invocations.work, undefined)
  }
})
