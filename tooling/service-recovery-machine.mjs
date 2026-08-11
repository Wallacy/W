const EFFECT_POLICIES = new Set([
  "repeatable",
  "idempotent",
  "transactional",
  "atMostOnce",
])

const OUTPUT_KINDS = new Set([
  "response",
  "outgoingCall",
  "stream",
  "capability",
  "supervisedWork",
])

export class ServiceRecoveryModelError extends Error {
  constructor(code) {
    super(code)
    this.name = "ServiceRecoveryModelError"
    this.code = code
  }
}

function fail(code) {
  throw new ServiceRecoveryModelError(code)
}

function requireString(value, code) {
  if (typeof value !== "string" || value.length === 0) fail(code)
}

function requirePositive(value, code) {
  if (!Number.isSafeInteger(value) || value <= 0) fail(code)
}

function requireNonNegative(value, code) {
  if (!Number.isSafeInteger(value) || value < 0) fail(code)
}

function validateLimits(limits) {
  for (const key of [
    "maximumCallRecords",
    "maximumMailboxItems",
    "maximumMailboxBytes",
    "maximumInFlight",
    "maximumStagedOutputBytes",
    "maximumJournalRecords",
    "maximumJournalBytes",
    "maximumDedupRecords",
    "maximumTombstones",
    "retryWindowTicks",
    "maximumRestarts",
    "restartWindowTicks",
  ]) {
    requirePositive(limits?.[key], "W-RECOVERY-0008")
  }
}

function initialState() {
  return {
    phase: "absent",
    admissionOpen: false,
    instanceIdentity: null,
    generation: null,
    operationVersion: null,
    schemaDigest: null,
    provider: null,
    limits: null,
    clock: 0,
    nextTicket: 1,
    calls: {},
    effects: {},
    mailbox: [],
    activeTurn: null,
    reserved: { items: 0, bytes: 0, work: 0, stagedOutputBytes: 0 },
    journal: {
      records: [],
      pending: null,
      bytes: 0,
      nextSequence: 1,
      predecessor: "journal-root",
      anchorSequence: 0,
      anchorRecordId: "journal-root",
      checkpointDigest: null,
      compactedRecords: 0,
    },
    delivered: [],
    disconnectedCapabilities: 0,
    restartTicks: [],
    quarantined: [],
    suppressedCompletions: [],
    logicalTrace: [],
    physicalTrace: [],
  }
}

function requireCall(state, id, phases = undefined) {
  const call = state.calls[id]
  if (!call) fail("serviceCallMissing")
  if (phases && !phases.includes(call.phase)) fail("serviceCallPhaseInvalid")
  return call
}

function requireReady(state) {
  if (state.phase !== "ready" || !state.admissionOpen) fail("W-RECOVERY-0002")
}

function sameEffectIdentity(effect, call) {
  return effect.serviceIdentity === call.serviceIdentity
    && effect.operationId === call.operationId
    && effect.inputDigest === call.inputDigest
    && effect.interfaceDigest === call.interfaceDigest
    && effect.policy === call.policy
}

function validateProviderReceipt(state, call, receipt, kind) {
  if (receipt?.issuedBy !== "journal-provider") fail("W-RECOVERY-0001")
  if (receipt.provider !== state.provider.id) fail("W-RECOVERY-0001")
  if (receipt.generation !== state.generation) fail("W-RECOVERY-0007")
  if (receipt.callId !== call.id || receipt.effectId !== call.effectId) {
    fail("W-RECOVERY-0001")
  }
  if (receipt.kind !== kind) fail("W-RECOVERY-0001")
  state.physicalTrace.push(`provider:${kind}:${call.id}:${state.generation}`)
}

function validateRuntimeClosureReceipt(state, call, receipt) {
  if (receipt?.issuedBy !== "runtime") fail("W-RECOVERY-0009")
  if (receipt.generation !== state.generation || receipt.callId !== call.id) {
    fail("W-RECOVERY-0009")
  }
  if (receipt.closureCommitted !== true) fail("W-RECOVERY-0009")
}

function reserveCall(state, call) {
  const nextItems = state.reserved.items + 1
  const nextBytes = state.reserved.bytes + call.bytes
  const nextWork = state.reserved.work + 1
  if (nextItems > state.limits.maximumMailboxItems) fail("W-RECOVERY-0002")
  if (nextBytes > state.limits.maximumMailboxBytes) fail("W-RECOVERY-0002")
  if (nextWork > state.limits.maximumInFlight) fail("W-RECOVERY-0002")
  state.reserved.items = nextItems
  state.reserved.bytes = nextBytes
  state.reserved.work = nextWork
}

function releaseCallReservation(state, call) {
  if (!call.reserved) return
  state.reserved.items -= 1
  state.reserved.bytes -= call.bytes
  state.reserved.work -= 1
  call.reserved = false
}

function releaseStagedOutput(state, call) {
  if (!call.output || call.output.released) return
  state.reserved.stagedOutputBytes -= call.output.bytes
  call.output.released = true
}

function cleanupEnvelope(call, resolution) {
  if (call.envelopeCleanupCount !== 0) fail("serviceEnvelopeCleanupRepeated")
  call.envelopeCleanupCount = 1
  call.owner = "released"
  call.frameResolution = resolution
}

function prepareJournalRecord(state, call, kind, bytes, payload = {}) {
  if (state.journal.pending) fail("W-RECOVERY-0001")
  requirePositive(bytes, "W-RECOVERY-0008")
  if (state.journal.records.length + 1 > state.limits.maximumJournalRecords) {
    fail("W-RECOVERY-0008")
  }
  if (state.journal.bytes + bytes > state.limits.maximumJournalBytes) {
    fail("W-RECOVERY-0008")
  }
  const sequence = state.journal.nextSequence
  state.journal.pending = {
    sequence,
    predecessor: state.journal.predecessor,
    recordId: `record:${sequence}:${kind}:${call.effectId}`,
    kind,
    callId: call.id,
    effectId: call.effectId,
    bytes,
    payload: structuredClone(payload),
  }
  state.journal.pending.checksum = recordChecksum(state.journal.pending)
  state.logicalTrace.push(`journal-prepare:${kind}:${call.id}`)
}

function recordChecksum(record) {
  return [
    record.sequence,
    record.predecessor,
    record.recordId,
    record.kind,
    record.callId,
    record.effectId,
    record.bytes,
    JSON.stringify(record.payload),
  ].join("|")
}

function verifyJournalRecords(state) {
  let priorSequence = state.journal.anchorSequence
  let priorRecordId = state.journal.anchorRecordId
  for (const record of state.journal.records) {
    if (record.checksum !== recordChecksum(record)) fail("W-RECOVERY-0006")
    if (record.sequence !== priorSequence + 1) {
      fail("W-RECOVERY-0006")
    }
    if (record.predecessor !== priorRecordId) {
      fail("W-RECOVERY-0006")
    }
    priorSequence = record.sequence
    priorRecordId = record.recordId
  }
  if (state.journal.nextSequence !== priorSequence + 1) {
    fail("W-RECOVERY-0006")
  }
}

function commitPendingRecord(state, call, receipt, kind) {
  const record = state.journal.pending
  if (!record || record.kind !== kind || record.callId !== call.id) {
    fail("W-RECOVERY-0001")
  }
  validateProviderReceipt(state, call, receipt, kind)
  state.journal.records.push(record)
  state.journal.bytes += record.bytes
  state.journal.nextSequence += 1
  state.journal.predecessor = record.recordId
  state.journal.pending = null
  state.logicalTrace.push(`journal-commit:${kind}:${call.id}`)
  return record
}

function appendRecoveryOutcome(state, call, outcome) {
  const bytes = 32
  if (state.journal.records.length + 1 > state.limits.maximumJournalRecords) {
    fail("W-RECOVERY-0008")
  }
  if (state.journal.bytes + bytes > state.limits.maximumJournalBytes) {
    fail("W-RECOVERY-0008")
  }
  const sequence = state.journal.nextSequence
  const record = {
    sequence,
    predecessor: state.journal.predecessor,
    recordId: `record:${sequence}:recoveryOutcome:${call.effectId}`,
    kind: "recoveryOutcome",
    callId: call.id,
    effectId: call.effectId,
    bytes,
    payload: { outcome },
  }
  record.checksum = recordChecksum(record)
  state.journal.records.push(record)
  state.journal.bytes += bytes
  state.journal.nextSequence += 1
  state.journal.predecessor = record.recordId
}

function terminalize(state, call, outcome, replayed = false) {
  if (call.envelopeCleanupCount === 0) {
    cleanupEnvelope(call, call.frameResolution ?? "runtimeClosure")
  }
  call.phase = "outcomeCommitted"
  call.durableOutcome = outcome
  call.replayed = replayed
  call.owner = "journal"
  call.retainedUntil = state.clock + state.limits.retryWindowTicks
  const effect = state.effects[call.effectId]
  effect.state = "terminal"
  effect.outcome = outcome
  effect.retainedUntil = call.retainedUntil
  effect.currentCallId = call.id
  if (state.activeTurn === call.id) state.activeTurn = null
  releaseCallReservation(state, call)
  if (outcome === "unknownOutcome") releaseStagedOutput(state, call)
  for (const attached of Object.values(state.calls)) {
    if (attached.phase !== "attached" || attached.attachedTo !== call.id) continue
    cleanupEnvelope(attached, "runtimeClosure")
    attached.phase = "outcomeCommitted"
    attached.durableOutcome = outcome
    attached.owner = "journal"
    attached.replayed = true
    attached.retainedUntil = call.retainedUntil
  }
}

function queueRecoveredCall(state, call, action) {
  reserveCall(state, call)
  call.reserved = true
  call.phase = "queued"
  call.generation = state.generation
  call.recoveryAction = action
  call.attempt += 1
  call.ticket = state.nextTicket
  state.nextTicket += 1
  state.mailbox.push(call.id)
  const effect = state.effects[call.effectId]
  effect.state = "active"
  effect.currentCallId = call.id
  state.logicalTrace.push(`recovery-queue:${action}:${call.id}`)
}

function checkEffectAdmission(state, call) {
  const effect = state.effects[call.effectId]
  if (!effect) return "new"
  if (!sameEffectIdentity(effect, call)) fail("W-RECOVERY-0003")
  if (effect.state === "tombstone" || state.clock > effect.retainedUntil) {
    fail("W-RECOVERY-0003")
  }
  if (effect.state === "terminal") return "replay"
  return "attach"
}

function verifyState(state) {
  if (state.activeTurn) {
    const call = requireCall(state, state.activeTurn)
    if (["outcomeCommitted", "delivered", "boundaryFailed"].includes(call.phase)) {
      fail("serviceActiveTurnTerminal")
    }
  }
  if (state.reserved.items < 0 || state.reserved.bytes < 0 || state.reserved.work < 0) {
    fail("serviceReservationUnderflow")
  }
  if (state.reserved.stagedOutputBytes < 0) fail("serviceOutputReservationUnderflow")
  for (const call of Object.values(state.calls)) {
    if (["outcomeCommitted", "delivered"].includes(call.phase)) {
      if (call.frameResolution !== "runtimeClosure" && call.frameResolution !== "faultBoundary") {
        fail("serviceOutcomeBeforeClosure")
      }
      if (call.envelopeCleanupCount !== 1) fail("serviceEnvelopeCleanupMissing")
    }
    if (call.phase === "boundaryFailed" && call.envelopeCleanupCount !== 1) {
      fail("serviceEnvelopeCleanupMissing")
    }
    if (call.envelopeCleanupCount > 1) fail("serviceEnvelopeCleanupRepeated")
  }
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "boot": {
      if (!new Set(["absent", "stopped"]).has(state.phase)) fail("W-RECOVERY-0006")
      requireString(operation.instanceIdentity, "W-RECOVERY-0001")
      requireString(operation.generation, "W-RECOVERY-0007")
      requireString(operation.operationVersion, "W-RECOVERY-0006")
      requireString(operation.schemaDigest, "W-RECOVERY-0006")
      requireString(operation.provider?.id, "W-RECOVERY-0001")
      if (
        operation.provider.resolved !== true
        || operation.provider.atomicCommit !== true
        || operation.provider.crashRecovery !== true
        || operation.provider.checksummed !== true
      ) {
        fail("W-RECOVERY-0001")
      }
      if (operation.provider.profile === "sqlite-wal" && operation.provider.filesystem !== "local") {
        fail("W-RECOVERY-0001")
      }
      validateLimits(operation.limits)
      state.phase = "ready"
      state.admissionOpen = true
      state.instanceIdentity = operation.instanceIdentity
      state.generation = operation.generation
      state.operationVersion = operation.operationVersion
      state.schemaDigest = operation.schemaDigest
      state.provider = structuredClone(operation.provider)
      state.limits = structuredClone(operation.limits)
      state.logicalTrace.push(`boot:${state.generation}`)
      return
    }

    case "stageCall": {
      requireReady(state)
      if (Object.keys(state.calls).length >= state.limits.maximumCallRecords) {
        fail("W-RECOVERY-0008")
      }
      requireString(operation.callId, "serviceCallMissing")
      requireString(operation.effectId, "W-RECOVERY-0003")
      requireString(operation.sender, "W-RECOVERY-0002")
      requireString(operation.operationId, "W-RECOVERY-0003")
      requireString(operation.interfaceDigest, "W-RECOVERY-0003")
      requireString(operation.inputDigest, "W-RECOVERY-0003")
      requireNonNegative(operation.bytes, "W-RECOVERY-0008")
      if (!EFFECT_POLICIES.has(operation.policy)) fail("W-RECOVERY-0005")
      if (state.calls[operation.callId]) fail("serviceCallDuplicate")
      if (operation.ownerStaged !== true) fail("W-RECOVERY-0009")
      if (operation.persistFrame === true || operation.persistLoan === true) {
        fail("W-RECOVERY-0009")
      }
      if (operation.transportIdentityUsed === true) fail("W-RECOVERY-0003")
      state.calls[operation.callId] = {
        id: operation.callId,
        effectId: operation.effectId,
        sender: operation.sender,
        serviceIdentity: state.instanceIdentity,
        operationId: operation.operationId,
        interfaceDigest: operation.interfaceDigest,
        inputDigest: operation.inputDigest,
        policy: operation.policy,
        bytes: operation.bytes,
        phase: "staged",
        owner: "callerStaging",
        generation: state.generation,
        ticket: null,
        attempt: 1,
        reserved: false,
        attachedTo: null,
        inputCommitted: false,
        effectState: "notStarted",
        effectOutcome: null,
        bodyOutcome: null,
        frameResolution: null,
        envelopeCleanupCount: 0,
        output: null,
        durableOutcome: null,
        callerOutcome: null,
        replayed: false,
        recoveryAction: null,
        retainedUntil: null,
      }
      state.logicalTrace.push(`stage:${operation.callId}`)
      return
    }

    case "commitEnvelope": {
      const call = requireCall(state, operation.callId, ["staged"])
      call.phase = "envelopeCommitted"
      call.owner = "envelope"
      state.logicalTrace.push(`envelope:${call.id}`)
      return
    }

    case "admitCall": {
      requireReady(state)
      const call = requireCall(state, operation.callId, ["envelopeCommitted"])
      const admission = checkEffectAdmission(state, call)
      if (admission === "replay") {
        const effect = state.effects[call.effectId]
        call.frameResolution = "runtimeClosure"
        cleanupEnvelope(call, "runtimeClosure")
        call.phase = "outcomeCommitted"
        call.durableOutcome = effect.outcome
        call.owner = "journal"
        call.replayed = true
        call.retainedUntil = effect.retainedUntil
        state.logicalTrace.push(`dedup-replay:${call.id}`)
        return
      }
      if (admission === "attach") {
        const effect = state.effects[call.effectId]
        call.phase = "attached"
        call.owner = "envelope"
        call.attachedTo = effect.currentCallId
        state.logicalTrace.push(`dedup-attach:${call.id}:${call.attachedTo}`)
        return
      }
      if (Object.keys(state.effects).length >= state.limits.maximumDedupRecords) {
        fail("W-RECOVERY-0008")
      }
      reserveCall(state, call)
      call.reserved = true
      call.phase = "queued"
      call.owner = "instance"
      call.ticket = state.nextTicket
      state.nextTicket += 1
      state.mailbox.push(call.id)
      state.effects[call.effectId] = {
        serviceIdentity: call.serviceIdentity,
        operationId: call.operationId,
        inputDigest: call.inputDigest,
        interfaceDigest: call.interfaceDigest,
        policy: call.policy,
        state: "provisional",
        currentCallId: call.id,
        outcome: null,
        retainedUntil: Number.POSITIVE_INFINITY,
      }
      state.logicalTrace.push(`admit:${call.id}:${call.ticket}`)
      return
    }

    case "startTurn": {
      if (state.phase !== "ready") fail("W-RECOVERY-0004")
      if (state.activeTurn) fail("W-RECOVERY-0004")
      const call = requireCall(state, operation.callId, ["queued"])
      const earlier = state.mailbox
        .map((id) => state.calls[id])
        .some((item) => item.sender === call.sender && item.ticket < call.ticket)
      if (earlier) fail("W-RECOVERY-0002")
      state.mailbox = state.mailbox.filter((id) => id !== call.id)
      state.activeTurn = call.id
      call.phase = "running"
      state.effects[call.effectId].state = "active"
      state.logicalTrace.push(`turn:${call.id}`)
      return
    }

    case "prepareInput": {
      const call = requireCall(state, operation.callId, ["running"])
      prepareJournalRecord(state, call, "input", operation.bytes, {
        inputDigest: call.inputDigest,
        interfaceDigest: call.interfaceDigest,
        operationVersion: state.operationVersion,
        schemaDigest: state.schemaDigest,
      })
      call.phase = "inputPreparing"
      return
    }

    case "confirmInput": {
      const call = requireCall(state, operation.callId, ["inputPreparing"])
      commitPendingRecord(state, call, operation.receipt, "input")
      call.inputCommitted = true
      call.phase = "inputCommitted"
      call.owner = "journalAndTurn"
      state.effects[call.effectId].state = "active"
      return
    }

    case "dispatchEffect": {
      const call = requireCall(state, operation.callId, ["inputCommitted"])
      if (call.policy === "repeatable" && operation.externalMutation === true) {
        fail("W-RECOVERY-0005")
      }
      if (
        call.policy === "idempotent"
        && operation.idempotencyKey !== call.effectId
        && operation.stableDomainKey !== true
      ) {
        fail("W-RECOVERY-0005")
      }
      if (
        call.policy === "transactional"
        && (operation.commitProvider !== state.provider.id || typeof operation.transactionId !== "string")
      ) {
        fail("W-RECOVERY-0005")
      }
      call.effectState = "dispatched"
      call.phase = "effectDispatched"
      state.physicalTrace.push(`effect-dispatch:${call.effectId}:${call.attempt}`)
      return
    }

    case "settleEffect": {
      const call = requireCall(state, operation.callId, ["effectDispatched"])
      if (!new Set(["success", "applicationError", "uncertain"]).has(operation.outcome)) {
        fail("W-RECOVERY-0005")
      }
      if (operation.outcome === "uncertain") {
        call.effectState = "uncertain"
        call.effectOutcome = null
      } else {
        if (operation.receipt?.issuedBy !== "effect-provider") fail("W-RECOVERY-0005")
        if (operation.receipt.effectId !== call.effectId) fail("W-RECOVERY-0005")
        call.effectState = "confirmed"
        call.effectOutcome = operation.outcome
      }
      call.phase = "effectSettled"
      return
    }

    case "resolveTransaction": {
      const call = requireCall(state, operation.callId, ["effectSettled", "recoveryPending"])
      if (call.policy !== "transactional") fail("W-RECOVERY-0005")
      if (operation.receipt?.issuedBy !== "journal-provider") fail("W-RECOVERY-0005")
      if (operation.receipt.provider !== state.provider.id) fail("W-RECOVERY-0005")
      if (!new Set(["committed", "aborted", "unknown"]).has(operation.decision)) {
        fail("W-RECOVERY-0005")
      }
      call.transactionDecision = operation.decision
      if (operation.decision === "committed") {
        call.effectState = "confirmed"
        call.effectOutcome = "success"
      } else if (operation.decision === "aborted") {
        call.effectState = "notStarted"
        call.effectOutcome = null
      } else {
        call.effectState = "uncertain"
      }
      return
    }

    case "stageOutput": {
      const call = requireCall(state, operation.callId, [
        "inputCommitted",
        "effectSettled",
      ])
      if (!OUTPUT_KINDS.has(operation.kind)) fail("W-RECOVERY-0004")
      requireNonNegative(operation.bytes, "W-RECOVERY-0008")
      if (call.output) fail("serviceOutputDuplicate")
      if (
        state.reserved.stagedOutputBytes + operation.bytes
        > state.limits.maximumStagedOutputBytes
      ) {
        fail("W-RECOVERY-0008")
      }
      state.reserved.stagedOutputBytes += operation.bytes
      call.output = {
        kind: operation.kind,
        bytes: operation.bytes,
        frontier: state.journal.records.at(-1)?.sequence ?? 0,
        released: false,
      }
      state.logicalTrace.push(`output-stage:${call.id}:${call.output.frontier}`)
      return
    }

    case "settleBody": {
      const call = requireCall(state, operation.callId, [
        "inputCommitted",
        "effectSettled",
      ])
      if (!new Set(["success", "applicationError"]).has(operation.outcome)) {
        fail("W-RECOVERY-0004")
      }
      call.bodyOutcome = operation.outcome
      call.phase = "bodySettled"
      return
    }

    case "closeTurn": {
      const call = requireCall(state, operation.callId, ["bodySettled"])
      if (operation.persistFrame === true || operation.persistLoan === true) {
        fail("W-RECOVERY-0009")
      }
      validateRuntimeClosureReceipt(state, call, operation.receipt)
      call.frameResolution = "runtimeClosure"
      call.phase = "closureCommitted"
      return
    }

    case "prepareOutcome": {
      const call = requireCall(state, operation.callId, ["closureCommitted"])
      let outcome = operation.outcome ?? call.bodyOutcome
      if (call.effectState === "uncertain") {
        if (call.policy === "atMostOnce") outcome = "unknownOutcome"
        else if (call.policy === "transactional" && call.transactionDecision === "unknown") {
          outcome = "unknownOutcome"
        } else {
          fail("W-RECOVERY-0005")
        }
      }
      if (!new Set(["success", "applicationError", "unknownOutcome"]).has(outcome)) {
        fail("W-RECOVERY-0004")
      }
      prepareJournalRecord(state, call, "outcome", operation.bytes, { outcome })
      call.pendingOutcome = outcome
      call.phase = "outcomePreparing"
      return
    }

    case "confirmOutcome": {
      const call = requireCall(state, operation.callId, ["outcomePreparing"])
      commitPendingRecord(state, call, operation.receipt, "outcome")
      terminalize(state, call, call.pendingOutcome)
      call.pendingOutcome = null
      state.logicalTrace.push(`outcome:${call.id}:${call.durableOutcome}`)
      return
    }

    case "deliverOutcome": {
      const call = requireCall(state, operation.callId, ["outcomeCommitted"])
      if (operation.connectionOpen !== true) fail("W-RECOVERY-0006")
      call.phase = "delivered"
      call.callerOutcome = call.durableOutcome
      releaseStagedOutput(state, call)
      state.delivered.push(call.id)
      state.logicalTrace.push(`deliver:${call.id}`)
      return
    }

    case "disconnect": {
      const call = requireCall(state, operation.callId)
      requireNonNegative(operation.capabilityCount ?? 0, "W-RECOVERY-0008")
      if (operation.persistentCapabilityClaim === true && operation.persistenceProtocol !== true) {
        fail("W-RECOVERY-0006")
      }
      state.disconnectedCapabilities += operation.capabilityCount ?? 0
      if (call.durableOutcome && call.phase !== "delivered") {
        call.callerOutcome = "unknownOutcome"
      } else if (!call.durableOutcome) {
        call.callerOutcome = "unknownOutcome"
      }
      releaseStagedOutput(state, call)
      state.logicalTrace.push(`disconnect:${call.id}`)
      return
    }

    case "crashProcess": {
      if (!new Set(["ready", "draining"]).has(state.phase)) fail("W-RECOVERY-0006")
      const oldGeneration = state.generation
      if (state.journal.pending) {
        state.logicalTrace.push(`journal-discard:${state.journal.pending.kind}`)
        state.journal.pending = null
      }
      for (const call of Object.values(state.calls)) {
        if (call.phase === "outcomeCommitted") {
          call.callerOutcome = "unknownOutcome"
          releaseStagedOutput(state, call)
          continue
        }
        if (call.phase === "delivered" || call.phase === "attached") continue
        if (!call.inputCommitted) {
          releaseCallReservation(state, call)
          releaseStagedOutput(state, call)
          cleanupEnvelope(call, "faultBoundary")
          call.phase = "boundaryFailed"
          call.callerOutcome = "unavailable"
          const effect = state.effects[call.effectId]
          if (effect && effect.currentCallId === call.id) {
            delete state.effects[call.effectId]
          }
          continue
        }
        cleanupEnvelope(call, "faultBoundary")
        call.phase = "recoveryPending"
        if (call.effectState === "notStarted") call.recoveryAction = "replayTurn"
        else if (call.policy === "repeatable" || call.policy === "idempotent") {
          call.recoveryAction = "retryAttempt"
        } else if (call.policy === "transactional") {
          call.recoveryAction = "resolveTransaction"
        } else {
          call.recoveryAction = "returnUnknownOutcome"
        }
        if (["dispatched", "confirmed", "uncertain"].includes(call.effectState)) {
          state.quarantined.push(`${call.id}@${oldGeneration}`)
        }
        releaseCallReservation(state, call)
        releaseStagedOutput(state, call)
      }
      for (const call of Object.values(state.calls)) {
        if (call.phase !== "attached") continue
        const primary = state.calls[call.attachedTo]
        if (primary?.phase !== "boundaryFailed") continue
        cleanupEnvelope(call, "faultBoundary")
        call.phase = "boundaryFailed"
        call.callerOutcome = "unavailable"
      }
      state.mailbox = []
      state.activeTurn = null
      state.reserved.items = 0
      state.reserved.bytes = 0
      state.reserved.work = 0
      state.phase = "crashed"
      state.admissionOpen = false
      state.oldGeneration = oldGeneration
      state.logicalTrace.push(`crash:${oldGeneration}`)
      return
    }

    case "recoverInstance": {
      if (state.phase !== "crashed") fail("W-RECOVERY-0006")
      requireString(operation.newGeneration, "W-RECOVERY-0007")
      if (operation.newGeneration === state.oldGeneration) fail("W-RECOVERY-0007")
      const evidence = operation.evidence
      if (
        evidence?.issuedBy !== "journal-provider"
        || evidence.provider !== state.provider.id
        || evidence.committedPrefixValid !== true
        || evidence.checksumValid !== true
        || evidence.sequenceContiguous !== true
        || evidence.oldGenerationIsolated !== true
      ) {
        fail("W-RECOVERY-0006")
      }
      verifyJournalRecords(state)
      if (
        evidence.operationVersion !== state.operationVersion
        || evidence.schemaDigest !== state.schemaDigest
      ) {
        fail("W-RECOVERY-0006")
      }
      state.restartTicks = state.restartTicks.filter(
        (tick) => tick > state.clock - state.limits.restartWindowTicks,
      )
      if (state.restartTicks.length >= state.limits.maximumRestarts) {
        fail("W-RECOVERY-0008")
      }
      state.restartTicks.push(state.clock)
      state.generation = operation.newGeneration
      state.phase = "ready"
      state.admissionOpen = true
      for (const call of Object.values(state.calls)) {
        if (call.phase !== "recoveryPending") continue
        if (call.recoveryAction === "replayTurn" || call.recoveryAction === "retryAttempt") {
          call.effectState = "notStarted"
          call.effectOutcome = null
          queueRecoveredCall(state, call, call.recoveryAction)
          continue
        }
        if (call.recoveryAction === "resolveTransaction") {
          const decisionReceipt = operation.transactionDecisions?.[call.id]
          if (
            decisionReceipt?.issuedBy !== "journal-provider"
            || decisionReceipt.provider !== state.provider.id
            || decisionReceipt.effectId !== call.effectId
          ) {
            fail("W-RECOVERY-0005")
          }
          const decision = decisionReceipt.decision
          if (!new Set(["committed", "aborted", "unknown"]).has(decision)) {
            fail("W-RECOVERY-0005")
          }
          if (decision === "aborted") {
            call.effectState = "notStarted"
            call.effectOutcome = null
            queueRecoveredCall(state, call, "resolveAborted")
          } else {
            const outcome = decision === "committed" ? "success" : "unknownOutcome"
            appendRecoveryOutcome(state, call, outcome)
            terminalize(state, call, outcome)
          }
          continue
        }
        appendRecoveryOutcome(state, call, "unknownOutcome")
        terminalize(state, call, "unknownOutcome")
      }
      state.logicalTrace.push(`recover:${state.generation}`)
      return
    }

    case "corruptJournal": {
      if (state.phase !== "crashed") fail("W-RECOVERY-0006")
      const record = state.journal.records[operation.index]
      if (!record) fail("W-RECOVERY-0006")
      record.bytes += 1
      state.physicalTrace.push(`journal-corrupt:${record.sequence}`)
      return
    }

    case "lateCompletion": {
      requireString(operation.callId, "serviceCallMissing")
      requireString(operation.generation, "W-RECOVERY-0007")
      if (operation.generation === state.generation) fail("W-RECOVERY-0007")
      const quarantineKey = `${operation.callId}@${operation.generation}`
      if (!state.quarantined.includes(quarantineKey)) fail("W-RECOVERY-0007")
      if (
        operation.receipt?.issuedBy !== "runtime"
        || operation.receipt.callId !== operation.callId
        || operation.receipt.generation !== operation.generation
        || operation.receipt.drained !== true
      ) {
        fail("W-RECOVERY-0007")
      }
      if (operation.mutatesState === true) fail("W-RECOVERY-0007")
      state.suppressedCompletions.push(quarantineKey)
      state.quarantined = state.quarantined.filter((key) => key !== quarantineKey)
      return
    }

    case "advanceTime": {
      requirePositive(operation.ticks, "W-RECOVERY-0008")
      state.clock += operation.ticks
      return
    }

    case "compactJournal": {
      if (state.journal.pending) fail("W-RECOVERY-0006")
      const retained = new Set(operation.retainEffectIds ?? [])
      const lastSequence = state.journal.records.at(-1)?.sequence ?? state.journal.anchorSequence
      const retainedIds = [...retained].sort()
      if (
        operation.receipt?.issuedBy !== "journal-provider"
        || operation.receipt.provider !== state.provider.id
        || operation.receipt.generation !== state.generation
        || operation.receipt.coversThroughSequence !== lastSequence
        || !Array.isArray(operation.receipt.retainedEffectIds)
        || JSON.stringify([...operation.receipt.retainedEffectIds].sort()) !== JSON.stringify(retainedIds)
      ) {
        fail("W-RECOVERY-0006")
      }
      requireString(operation.receipt.checkpointDigest, "W-RECOVERY-0006")
      const required = Object.entries(state.effects)
        .filter(([, effect]) => effect.state !== "terminal" || state.clock <= effect.retainedUntil)
        .map(([effectId]) => effectId)
      for (const effectId of required) {
        if (!retained.has(effectId)) fail("W-RECOVERY-0008")
      }
      requireNonNegative(operation.targetRecords, "W-RECOVERY-0008")
      if (operation.targetRecords > state.journal.records.length) fail("W-RECOVERY-0008")
      const priorRecords = state.journal.records
      const removed = state.journal.records.length - operation.targetRecords
      state.journal.compactedRecords += removed
      state.journal.records = operation.targetRecords === 0
        ? []
        : priorRecords.slice(-operation.targetRecords)
      if (state.journal.records.length === 0) {
        state.journal.anchorSequence = lastSequence
        state.journal.anchorRecordId = state.journal.predecessor
      } else {
        state.journal.anchorSequence = state.journal.records[0].sequence - 1
        state.journal.anchorRecordId = state.journal.records[0].predecessor
      }
      state.journal.checkpointDigest = operation.receipt.checkpointDigest
      state.journal.bytes = state.journal.records.reduce((total, record) => total + record.bytes, 0)
      const expiring = Object.values(state.effects)
        .filter((effect) => effect.state === "terminal" && state.clock > effect.retainedUntil)
      const existingTombstones = Object.values(state.effects)
        .filter((effect) => effect.state === "tombstone").length
      if (existingTombstones + expiring.length > state.limits.maximumTombstones) {
        fail("W-RECOVERY-0008")
      }
      for (const effect of expiring) {
        effect.state = "tombstone"
        effect.outcome = null
      }
      state.logicalTrace.push(`compact:${removed}`)
      return
    }

    case "beginDrain": {
      if (state.phase !== "ready") fail("W-RECOVERY-0008")
      state.phase = "draining"
      state.admissionOpen = false
      state.logicalTrace.push("drain")
      return
    }

    case "shutdown": {
      if (state.phase !== "draining") fail("W-RECOVERY-0008")
      const pendingRecovery = Object.values(state.calls)
        .some((call) => call.phase === "recoveryPending")
      if (
        state.mailbox.length > 0
        || state.activeTurn
        || state.journal.pending
        || state.quarantined.length > 0
        || state.reserved.stagedOutputBytes > 0
        || pendingRecovery
      ) {
        fail("W-RECOVERY-0008")
      }
      state.phase = "stopped"
      state.logicalTrace.push("stopped")
      return
    }

    case "verify": {
      if (operation.phase !== undefined && state.phase !== operation.phase) {
        fail("serviceVerificationFailed")
      }
      if (operation.activeTurn !== undefined && state.activeTurn !== operation.activeTurn) {
        fail("serviceVerificationFailed")
      }
      return
    }

    default:
      fail("serviceRecoveryOperationUnknown")
  }
}

function projectedCall(call) {
  return {
    phase: call.phase,
    effectId: call.effectId,
    attempt: call.attempt,
    durableOutcome: call.durableOutcome,
    callerOutcome: call.callerOutcome,
    replayed: call.replayed,
    recoveryAction: call.recoveryAction,
    frameResolution: call.frameResolution,
    cleanupCount: call.envelopeCleanupCount,
  }
}

function projectState(state) {
  return {
    phase: state.phase,
    generation: state.generation,
    admissionOpen: state.admissionOpen,
    activeTurn: state.activeTurn,
    mailbox: [...state.mailbox],
    calls: Object.fromEntries(
      Object.entries(state.calls).map(([id, call]) => [id, projectedCall(call)]),
    ),
    effects: Object.fromEntries(
      Object.entries(state.effects).map(([id, effect]) => [id, {
        state: effect.state,
        outcome: effect.outcome,
        currentCallId: effect.currentCallId,
        retainedUntil: Number.isFinite(effect.retainedUntil) ? effect.retainedUntil : "active",
      }]),
    ),
    reserved: structuredClone(state.reserved),
    journal: {
      records: state.journal.records.map((record) => `${record.sequence}:${record.kind}:${record.effectId}`),
      pending: state.journal.pending?.kind ?? null,
      bytes: state.journal.bytes,
      anchorSequence: state.journal.anchorSequence,
      checkpointDigest: state.journal.checkpointDigest,
      compactedRecords: state.journal.compactedRecords,
    },
    delivered: [...state.delivered],
    disconnectedCapabilities: state.disconnectedCapabilities,
    restartTicks: [...state.restartTicks],
    quarantined: [...state.quarantined],
    suppressedCompletions: [...state.suppressedCompletions],
    logicalTrace: [...state.logicalTrace],
  }
}

export function runServiceRecoveryOperations(operations) {
  const state = initialState()
  try {
    for (const operation of operations) {
      applyOperation(state, operation)
      verifyState(state)
    }
    return {
      status: "accepted",
      error: null,
      state: projectState(state),
      physical: { trace: [...state.physicalTrace] },
    }
  } catch (error) {
    if (!(error instanceof ServiceRecoveryModelError)) throw error
    return {
      status: "rejected",
      error: error.code,
      state: projectState(state),
      physical: { trace: [...state.physicalTrace] },
    }
  }
}
