import assert from "node:assert/strict"
import test from "node:test"
import { runServiceRecoveryOperations } from "./service-recovery-machine.mjs"

const limits = {
  maximumCallRecords: 24,
  maximumMailboxItems: 6,
  maximumMailboxBytes: 4096,
  maximumInFlight: 6,
  maximumStagedOutputBytes: 4096,
  maximumJournalRecords: 32,
  maximumJournalBytes: 16384,
  maximumDedupRecords: 16,
  maximumTombstones: 16,
  retryWindowTicks: 100,
  maximumRestarts: 2,
  restartWindowTicks: 10,
}

function boot(overrides = {}) {
  return {
    op: "boot",
    instanceIdentity: "restaurant/order:42",
    generation: "generation-1",
    operationVersion: "fulfillment-v1",
    schemaDigest: "schema-v1",
    provider: {
      id: "sqlite-workflow",
      resolved: true,
      atomicCommit: true,
      crashRecovery: true,
      checksummed: true,
      profile: "sqlite-wal",
      filesystem: "local",
      ...(overrides.provider ?? {}),
    },
    limits: { ...limits, ...(overrides.limits ?? {}) },
    ...Object.fromEntries(Object.entries(overrides).filter(([key]) => !["provider", "limits"].includes(key))),
  }
}

function stage(callId = "call-1", effectId = "effect-42", overrides = {}) {
  return {
    op: "stageCall",
    callId,
    effectId,
    sender: "maitre",
    operationId: "OrderCoordinatorApi.submit",
    interfaceDigest: "interface-v1",
    inputDigest: "order-42-v1",
    policy: "idempotent",
    bytes: 128,
    ownerStaged: true,
    persistFrame: false,
    persistLoan: false,
    transportIdentityUsed: false,
    ...overrides,
  }
}

function journalReceipt(kind, callId = "call-1", effectId = "effect-42", generation = "generation-1") {
  return {
    issuedBy: "journal-provider",
    provider: "sqlite-workflow",
    generation,
    callId,
    effectId,
    kind,
  }
}

function recover(newGeneration = "generation-2", transactionDecisions = {}) {
  return {
    op: "recoverInstance",
    newGeneration,
    evidence: {
      issuedBy: "journal-provider",
      provider: "sqlite-workflow",
      committedPrefixValid: true,
      checksumValid: true,
      sequenceContiguous: true,
      oldGenerationIsolated: true,
      operationVersion: "fulfillment-v1",
      schemaDigest: "schema-v1",
    },
    transactionDecisions,
  }
}

function runtimeDrainReceipt(callId, generation = "generation-1") {
  return { issuedBy: "runtime", callId, generation, drained: true }
}

function compactionReceipt(coversThroughSequence, retainedEffectIds = []) {
  return {
    issuedBy: "journal-provider",
    provider: "sqlite-workflow",
    generation: "generation-1",
    coversThroughSequence,
    checkpointDigest: `checkpoint-${coversThroughSequence}`,
    retainedEffectIds,
  }
}

function admitted(callId = "call-1", effectId = "effect-42", overrides = {}) {
  return [
    stage(callId, effectId, overrides),
    { op: "commitEnvelope", callId },
    { op: "admitCall", callId },
    { op: "startTurn", callId },
  ]
}

function inputCommitted(callId = "call-1", effectId = "effect-42") {
  return [
    { op: "prepareInput", callId, bytes: 96 },
    { op: "confirmInput", callId, receipt: journalReceipt("input", callId, effectId) },
  ]
}

function outcomeCommitted(callId = "call-1", effectId = "effect-42") {
  return [
    {
      op: "dispatchEffect",
      callId,
      idempotencyKey: effectId,
      externalMutation: true,
    },
    {
      op: "settleEffect",
      callId,
      outcome: "success",
      receipt: { issuedBy: "effect-provider", effectId },
    },
    { op: "stageOutput", callId, kind: "response", bytes: 256 },
    { op: "settleBody", callId, outcome: "success" },
    {
      op: "closeTurn",
      callId,
      persistFrame: false,
      persistLoan: false,
      receipt: {
        issuedBy: "runtime",
        generation: "generation-1",
        callId,
        closureCommitted: true,
      },
    },
    { op: "prepareOutcome", callId, outcome: "success", bytes: 64 },
    { op: "confirmOutcome", callId, receipt: journalReceipt("outcome", callId, effectId) },
  ]
}

test("a normal turn commits closure before publishing its outcome", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    ...admitted(),
    ...inputCommitted(),
    ...outcomeCommitted(),
    { op: "deliverOutcome", callId: "call-1", connectionOpen: true },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.calls["call-1"].phase, "delivered")
  assert.equal(result.state.calls["call-1"].frameResolution, "runtimeClosure")
  assert.equal(result.state.calls["call-1"].cleanupCount, 1)
  assert.equal(result.state.effects["effect-42"].outcome, "success")
})

test("FIFO is per sender rather than global", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    stage("call-a", "effect-a", { sender: "a", inputDigest: "a" }),
    { op: "commitEnvelope", callId: "call-a" },
    { op: "admitCall", callId: "call-a" },
    stage("call-b", "effect-b", { sender: "b", inputDigest: "b" }),
    { op: "commitEnvelope", callId: "call-b" },
    { op: "admitCall", callId: "call-b" },
    { op: "startTurn", callId: "call-b" },
  ])
  assert.equal(result.state.activeTurn, "call-b")
  assert.deepEqual(result.state.mailbox, ["call-a"])
})

test("a duplicate active call attaches and receives the same terminal outcome", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    stage(),
    { op: "commitEnvelope", callId: "call-1" },
    { op: "admitCall", callId: "call-1" },
    stage("call-2"),
    { op: "commitEnvelope", callId: "call-2" },
    { op: "admitCall", callId: "call-2" },
    { op: "startTurn", callId: "call-1" },
    ...inputCommitted(),
    ...outcomeCommitted(),
  ])
  assert.equal(result.state.calls["call-2"].phase, "outcomeCommitted")
  assert.equal(result.state.calls["call-2"].durableOutcome, "success")
  assert.equal(result.state.calls["call-2"].replayed, true)
})

test("an attached duplicate closes when its uncommitted primary is lost", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    stage(),
    { op: "commitEnvelope", callId: "call-1" },
    { op: "admitCall", callId: "call-1" },
    stage("call-2"),
    { op: "commitEnvelope", callId: "call-2" },
    { op: "admitCall", callId: "call-2" },
    { op: "crashProcess" },
    recover(),
  ])
  assert.equal(result.state.calls["call-1"].phase, "boundaryFailed")
  assert.equal(result.state.calls["call-2"].phase, "boundaryFailed")
  assert.equal(result.state.calls["call-2"].cleanupCount, 1)
})

test("an uncommitted input record disappears after a crash", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    ...admitted(),
    { op: "prepareInput", callId: "call-1", bytes: 96 },
    { op: "crashProcess" },
    recover(),
  ])
  assert.equal(result.state.calls["call-1"].callerOutcome, "unavailable")
  assert.deepEqual(result.state.journal.records, [])
  assert.equal(result.state.journal.pending, null)
})

test("a committed input without dispatch replays the turn", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    ...admitted(),
    ...inputCommitted(),
    { op: "crashProcess" },
    recover(),
  ])
  assert.equal(result.state.calls["call-1"].phase, "queued")
  assert.equal(result.state.calls["call-1"].attempt, 2)
  assert.equal(result.state.calls["call-1"].recoveryAction, "replayTurn")
})

test("idempotent and at-most-once effects diverge after uncertain dispatch", () => {
  const idempotent = runServiceRecoveryOperations([
    boot(),
    ...admitted(),
    ...inputCommitted(),
    { op: "dispatchEffect", callId: "call-1", idempotencyKey: "effect-42" },
    { op: "crashProcess" },
    recover(),
  ])
  const atMostOnce = runServiceRecoveryOperations([
    boot(),
    ...admitted("call-1", "effect-42", { policy: "atMostOnce" }),
    ...inputCommitted(),
    { op: "dispatchEffect", callId: "call-1" },
    { op: "crashProcess" },
    recover(),
  ])
  assert.equal(idempotent.state.calls["call-1"].phase, "queued")
  assert.equal(atMostOnce.state.calls["call-1"].durableOutcome, "unknownOutcome")
})

test("a transactional effect uses the journal authority decision", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    ...admitted("call-1", "effect-42", { policy: "transactional" }),
    ...inputCommitted(),
    {
      op: "dispatchEffect",
      callId: "call-1",
      commitProvider: "sqlite-workflow",
      transactionId: "tx-42",
    },
    { op: "crashProcess" },
    recover("generation-2", {
      "call-1": {
        issuedBy: "journal-provider",
        provider: "sqlite-workflow",
        effectId: "effect-42",
        decision: "committed",
      },
    }),
  ])
  assert.equal(result.state.calls["call-1"].durableOutcome, "success")
  assert.match(result.state.journal.records.at(-1), /recoveryOutcome/)
})

test("reply loss does not erase a committed outcome", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    ...admitted(),
    ...inputCommitted(),
    ...outcomeCommitted(),
    { op: "disconnect", callId: "call-1", capabilityCount: 1 },
    stage("call-2"),
    { op: "commitEnvelope", callId: "call-2" },
    { op: "admitCall", callId: "call-2" },
  ])
  assert.equal(result.state.calls["call-1"].callerOutcome, "unknownOutcome")
  assert.equal(result.state.calls["call-2"].durableOutcome, "success")
  assert.equal(result.state.disconnectedCapabilities, 1)
})

test("an old provider completion is suppressed by generation", () => {
  const prefix = [
    boot(),
    ...admitted("old", "effect-old", { inputDigest: "old" }),
    ...inputCommitted("old", "effect-old"),
    { op: "dispatchEffect", callId: "old", idempotencyKey: "effect-old" },
    { op: "crashProcess" },
    recover(),
  ]
  const withoutReceipt = runServiceRecoveryOperations([
    ...prefix,
    { op: "lateCompletion", callId: "old", generation: "generation-1", mutatesState: false },
  ])
  assert.equal(withoutReceipt.error, "W-RECOVERY-0007")

  const result = runServiceRecoveryOperations([
    ...prefix,
    {
      op: "lateCompletion",
      callId: "old",
      generation: "generation-1",
      mutatesState: false,
      receipt: runtimeDrainReceipt("old"),
    },
  ])
  assert.deepEqual(result.state.suppressedCompletions, ["old@generation-1"])
  assert.deepEqual(result.state.quarantined, [])
  assert.equal(result.state.generation, "generation-2")
})

test("journal compaction publishes a provider checkpoint anchor", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    ...admitted(),
    ...inputCommitted(),
    ...outcomeCommitted(),
    {
      op: "compactJournal",
      retainEffectIds: ["effect-42"],
      targetRecords: 1,
      receipt: compactionReceipt(2, ["effect-42"]),
    },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.journal.anchorSequence, 1)
  assert.equal(result.state.journal.checkpointDigest, "checkpoint-2")
})

test("compaction cannot discard a live dedup record", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    ...admitted(),
    ...inputCommitted(),
    {
      op: "compactJournal",
      retainEffectIds: [],
      targetRecords: 0,
      receipt: compactionReceipt(1),
    },
  ])
  assert.equal(result.status, "rejected")
  assert.equal(result.error, "W-RECOVERY-0008")
})

test("retry after the declared window is rejected", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    ...admitted(),
    ...inputCommitted(),
    ...outcomeCommitted(),
    { op: "advanceTime", ticks: 101 },
    {
      op: "compactJournal",
      retainEffectIds: [],
      targetRecords: 0,
      receipt: compactionReceipt(2),
    },
    stage("call-2"),
    { op: "commitEnvelope", callId: "call-2" },
    { op: "admitCall", callId: "call-2" },
  ])
  assert.equal(result.error, "W-RECOVERY-0003")
})

test("restart intensity is bounded within its window", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    { op: "crashProcess" },
    recover("generation-2"),
    { op: "crashProcess" },
    recover("generation-3"),
    { op: "crashProcess" },
    recover("generation-4"),
  ])
  assert.equal(result.error, "W-RECOVERY-0008")
  assert.equal(result.state.phase, "crashed")
})

test("shutdown waits for active turns", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    ...admitted(),
    { op: "beginDrain" },
    { op: "shutdown" },
  ])
  assert.equal(result.error, "W-RECOVERY-0008")
  assert.equal(result.state.phase, "draining")
})

test("a WAL profile cannot claim a network filesystem", () => {
  const result = runServiceRecoveryOperations([
    boot({ provider: { filesystem: "network" } }),
  ])
  assert.equal(result.error, "W-RECOVERY-0001")
})

test("recovery never persists a task frame or loan", () => {
  const result = runServiceRecoveryOperations([
    boot(),
    stage("call-1", "effect-42", { persistFrame: true }),
  ])
  assert.equal(result.error, "W-RECOVERY-0009")
})
