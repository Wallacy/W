const LOCK_KINDS = new Set(["sync", "async"])
const ACCESS_MODES = new Set(["ref", "inout"])

export class ScopedLockModelError extends Error {
  constructor(code, status = "rejected") {
    super(code)
    this.name = "ScopedLockModelError"
    this.code = code
    this.status = status
  }
}

function fail(code, status = "rejected") {
  throw new ScopedLockModelError(code, status)
}

function requireName(value, code) {
  if (typeof value !== "string" || value.length === 0) fail(code)
}

function requireLock(state, lockId) {
  const lock = state.locks[lockId]
  if (!lock) fail("lockUnknown")
  if (lock.phase !== "open") fail("lockNotOpen")
  return lock
}

function requireHolder(lock, task) {
  if (!lock.holder || lock.holder.task !== task) fail("lockHolderMismatch")
  return lock.holder
}

function enqueue(lock, operation) {
  if (lock.holder?.task === operation.task || lock.queue.some((item) => item.task === operation.task)) {
    fail("W-LOCK-0004")
  }
  const ticket = lock.nextTicket
  lock.nextTicket += 1
  lock.queue.push({
    task: operation.task,
    ticket,
    access: operation.access,
    boundary: operation.boundary,
  })
  lock.trace.push(`enqueue:${ticket}:${operation.task}`)
  return ticket
}

function admit(lock, waiter, source) {
  lock.holder = {
    task: waiter.task,
    ticket: waiter.ticket,
    access: waiter.access,
    pendingCancellation: false,
  }
  lock.trace.push(`grant:${waiter.ticket}:${waiter.task}:${source}`)
  if (lock.lastUnlock !== null) {
    lock.happensBefore.push(`unlock:${lock.lastUnlock}->grant:${waiter.ticket}`)
  }
}

function validateRequest(lock, operation, immediate = false) {
  requireName(operation.task, "lockTaskMissing")
  if (!ACCESS_MODES.has(operation.access)) fail("lockAccessInvalid")
  if (operation.boundary !== lock.boundary) fail("W-LOCK-0005")
  if (lock.kind === "sync" && !immediate && operation.blockingAllowed !== true) {
    fail("W-LOCK-0003")
  }
}

function finishHolder(lock, operation) {
  const holder = requireHolder(lock, operation.task)
  if (!new Set(["success", "error"]).has(operation.outcome)) fail("lockOutcomeInvalid")
  lock.trace.push(`finish:${holder.ticket}:${operation.task}:${operation.outcome}`)
  lock.lastUnlock = holder.ticket
  lock.happensBefore.push(`body:${holder.ticket}->unlock:${holder.ticket}`)
  lock.outcomes.push({
    task: operation.task,
    ticket: holder.ticket,
    outcome: operation.outcome,
    cancellation: holder.pendingCancellation ? "after-unlock" : "none",
  })
  if (holder.pendingCancellation) lock.cancellations.push(`observed:${operation.task}`)
  lock.holder = null
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "create": {
      requireName(operation.lock, "lockNameMissing")
      requireName(operation.boundary, "lockBoundaryMissing")
      if (state.locks[operation.lock]) fail("lockAlreadyCreated")
      if (!LOCK_KINDS.has(operation.kind)) fail("W-SYNC-0001")
      if (
        operation.owned !== true ||
        operation.transferable !== true ||
        operation.lifetimeIndependent !== true
      ) {
        fail("lockPayloadContractMissing")
      }
      state.locks[operation.lock] = {
        kind: operation.kind,
        boundary: operation.boundary,
        value: operation.value,
        phase: "open",
        holder: null,
        queue: [],
        nextTicket: 0,
        lastUnlock: null,
        drops: 0,
        outcomes: [],
        cancellations: [],
        happensBefore: [],
        trace: [`create:${operation.kind}`],
      }
      return
    }

    case "request": {
      const lock = requireLock(state, operation.lock)
      validateRequest(lock, operation)
      const ticket = enqueue(lock, operation)
      state.receipts.push({ operation: "request", task: operation.task, ticket })
      return
    }

    case "grant": {
      const lock = requireLock(state, operation.lock)
      if (lock.holder) fail("lockAlreadyHeld")
      const waiter = lock.queue[0]
      if (!waiter || waiter.task !== operation.task) fail("W-LOCK-0009")
      lock.queue.shift()
      admit(lock, waiter, "queue")
      return
    }

    case "tryAcquire": {
      const lock = requireLock(state, operation.lock)
      validateRequest(lock, operation, true)
      if (lock.holder?.task === operation.task) fail("W-LOCK-0004")
      if (lock.holder || lock.queue.length > 0) {
        lock.trace.push(`try-busy:${operation.task}`)
        state.receipts.push({ operation: "try", task: operation.task, result: "busy" })
        return
      }
      const waiter = {
        task: operation.task,
        ticket: lock.nextTicket,
        access: operation.access,
        boundary: operation.boundary,
      }
      lock.nextTicket += 1
      admit(lock, waiter, "try")
      state.receipts.push({ operation: "try", task: operation.task, result: "acquired" })
      return
    }

    case "read": {
      const lock = requireLock(state, operation.lock)
      const holder = requireHolder(lock, operation.task)
      lock.trace.push(`read:${holder.ticket}:${String(lock.value)}`)
      state.reads.push(lock.value)
      return
    }

    case "write": {
      const lock = requireLock(state, operation.lock)
      const holder = requireHolder(lock, operation.task)
      if (holder.access !== "inout") fail("W-LOCK-0006")
      lock.value = operation.value
      lock.trace.push(`write:${holder.ticket}:${String(operation.value)}`)
      return
    }

    case "finish": {
      const lock = requireLock(state, operation.lock)
      finishHolder(lock, operation)
      return
    }

    case "cancelWait": {
      const lock = requireLock(state, operation.lock)
      if (lock.kind !== "async") fail("lockSyncWaitNotCancellable")
      const index = lock.queue.findIndex((item) => item.task === operation.task)
      if (index < 0) fail("lockWaiterMissing")
      const [waiter] = lock.queue.splice(index, 1)
      lock.cancellations.push(`removed:${operation.task}`)
      lock.trace.push(`cancel-wait:${waiter.ticket}:${operation.task}`)
      return
    }

    case "cancelHeld": {
      const lock = requireLock(state, operation.lock)
      if (lock.kind !== "async") fail("lockSyncHolderNotCancellable")
      const holder = requireHolder(lock, operation.task)
      holder.pendingCancellation = true
      lock.cancellations.push(`deferred:${operation.task}`)
      lock.trace.push(`cancel-held:${holder.ticket}:${operation.task}`)
      return
    }

    case "interruptHeld": {
      const lock = requireLock(state, operation.lock)
      const holder = requireHolder(lock, operation.task)
      if (!holder.pendingCancellation) fail("lockCancellationMissing")
      fail("W-LOCK-0010")
    }

    case "suspend":
      requireHolder(requireLock(state, operation.lock), operation.task)
      fail("W-LOCK-0002")

    case "escape":
      requireHolder(requireLock(state, operation.lock), operation.task)
      fail("W-LOCK-0001")

    case "copy":
      requireLock(state, operation.lock)
      fail("W-LOCK-0007")

    case "panic": {
      const lock = requireLock(state, operation.lock)
      const holder = requireHolder(lock, operation.task)
      lock.trace.push(`panic:${holder.ticket}:${operation.task}`)
      lock.phase = "faulted"
      lock.holder = null
      lock.queue = []
      state.failedBoundaries.push(lock.boundary)
      fail("lockBoundaryPanicked", "fault")
    }

    case "drop": {
      const lock = requireLock(state, operation.lock)
      if (lock.holder || lock.queue.length > 0) fail("W-LOCK-0008")
      if (lock.drops !== 0) fail("lockDropRepeated")
      lock.drops = 1
      lock.phase = "closed"
      lock.trace.push("drop")
      return
    }

    case "unsupported":
      if (!new Set(["ReadWriteLock", "Condition", "Once"]).has(operation.primitive)) {
        fail("lockUnsupportedEvidenceInvalid")
      }
      fail("W-SYNC-0001")

    case "select": {
      state.selections.push(selectSynchronization(operation.facts ?? {}))
      return
    }

    default:
      fail("lockOperationUnknown")
  }
}

function verifyState(state) {
  for (const lock of Object.values(state.locks)) {
    for (let index = 1; index < lock.queue.length; index += 1) {
      if (lock.queue[index - 1].ticket >= lock.queue[index].ticket) fail("W-LOCK-0009")
    }
    if (lock.phase === "closed" && (lock.holder || lock.queue.length > 0 || lock.drops !== 1)) {
      fail("lockClosedStateInvalid")
    }
  }
}

export function selectSynchronization(facts) {
  if (facts.durable === true || facts.distributed === true || facts.keyedIdentity === true) {
    return "service"
  }
  if (facts.transferOwnership === true) return "channel"
  if (facts.scalar === true && facts.singleLocation === true) return "atomic"
  if (facts.readHeavy === true && facts.replaceWholeVersion === true) return "snapshot-cell"
  if (
    facts.parallelReads === true &&
    facts.exclusiveWrite === true &&
    facts.staticDomain === true &&
    facts.closedAccessGraph === true
  ) {
    return "domain-barrier"
  }
  if (facts.shortCriticalSection === true && facts.taskContext === true) return "async-mutex"
  if (facts.shortCriticalSection === true && facts.synchronousContext === true) return "mutex"
  return "insufficient-facts"
}

function projectState(state) {
  const locks = {}
  for (const [id, lock] of Object.entries(state.locks)) {
    locks[id] = {
      kind: lock.kind,
      boundary: lock.boundary,
      value: lock.value,
      phase: lock.phase,
      holder: lock.holder?.task ?? null,
      queue: lock.queue.map((item) => item.task),
      drops: lock.drops,
      outcomes: lock.outcomes,
      cancellations: lock.cancellations,
      happensBefore: lock.happensBefore,
      trace: lock.trace,
    }
  }
  return {
    locks,
    receipts: state.receipts,
    reads: state.reads,
    selections: state.selections,
    failedBoundaries: state.failedBoundaries,
  }
}

export function runScopedLockOperations(operations) {
  const state = {
    locks: {},
    receipts: [],
    reads: [],
    selections: [],
    failedBoundaries: [],
  }
  let status = "accepted"
  let error = null
  let failedOperation = null

  for (const [index, operation] of operations.entries()) {
    try {
      applyOperation(state, operation)
      verifyState(state)
    } catch (cause) {
      if (!(cause instanceof ScopedLockModelError)) throw cause
      status = cause.status
      error = cause.code
      failedOperation = index
      break
    }
  }

  return {
    status,
    error,
    failedOperation,
    state: projectState(state),
  }
}
