const LOCK_KINDS = new Set(["sync", "async", "read-write"])
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
  if (lock.kind !== "read-write") {
    if (!lock.holder || lock.holder.task !== task) fail("lockHolderMismatch")
    return lock.holder
  }
  if (lock.writer?.task === task) return lock.writer
  const reader = lock.readers.find((item) => item.task === task)
  if (!reader) fail("lockHolderMismatch")
  return reader
}

function hasActiveHolder(lock) {
  if (lock.kind === "read-write") return lock.writer !== null || lock.readers.length > 0
  return lock.holder !== null
}

function taskIsActive(lock, task) {
  if (lock.kind === "read-write") {
    return lock.writer?.task === task || lock.readers.some((item) => item.task === task)
  }
  return lock.holder?.task === task
}

function enqueue(lock, operation) {
  if (taskIsActive(lock, operation.task) || lock.queue.some((item) => item.task === operation.task)) {
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
  if (lock.kind === "read-write") fail("lockAdmissionKindInvalid")
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

function beginReadWritePhase(lock, waiters, source) {
  if (hasActiveHolder(lock) || waiters.length === 0) fail("lockPhaseAdmissionInvalid")
  const phase = lock.nextPhase
  lock.nextPhase += 1
  const access = waiters[0].access
  if (!waiters.every((waiter) => waiter.access === access)) fail("lockPhaseAccessMixed")
  if (access === "inout" && waiters.length !== 1) fail("lockWriterPhaseWidthInvalid")

  for (const waiter of waiters) {
    const holder = {
      task: waiter.task,
      ticket: waiter.ticket,
      access: waiter.access,
      phase,
      pendingCancellation: false,
    }
    if (access === "ref") lock.readers.push(holder)
    else lock.writer = holder
    lock.trace.push(`grant:${waiter.ticket}:${waiter.task}:${source}:phase:${phase}`)
    if (lock.lastPhaseClose !== null) {
      lock.happensBefore.push(`phase-close:${lock.lastPhaseClose}->grant:${waiter.ticket}`)
    }
  }
}

function joinReadWritePhase(lock, waiters, source) {
  if (lock.writer || lock.readers.length === 0 || waiters.length === 0) {
    fail("lockPhaseJoinInvalid")
  }
  const phase = lock.readers[0].phase
  if (!waiters.every((waiter) => waiter.access === "ref")) fail("lockPhaseAccessMixed")
  for (const waiter of waiters) {
    lock.readers.push({
      task: waiter.task,
      ticket: waiter.ticket,
      access: "ref",
      phase,
      pendingCancellation: false,
    })
    lock.trace.push(`grant:${waiter.ticket}:${waiter.task}:${source}:phase:${phase}`)
  }
}

function validateRequest(lock, operation, immediate = false) {
  requireName(operation.task, "lockTaskMissing")
  if (!ACCESS_MODES.has(operation.access)) fail("lockAccessInvalid")
  if (operation.boundary !== lock.boundary) fail("W-LOCK-0005")
  if (
    new Set(["sync", "read-write"]).has(lock.kind) &&
    !immediate &&
    operation.blockingAllowed !== true
  ) {
    fail("W-LOCK-0003")
  }
}

function finishHolder(lock, operation) {
  const holder = requireHolder(lock, operation.task)
  if (!new Set(["success", "error"]).has(operation.outcome)) fail("lockOutcomeInvalid")
  lock.trace.push(`finish:${holder.ticket}:${operation.task}:${operation.outcome}`)
  lock.happensBefore.push(`body:${holder.ticket}->unlock:${holder.ticket}`)
  lock.outcomes.push({
    task: operation.task,
    ticket: holder.ticket,
    outcome: operation.outcome,
    cancellation: holder.pendingCancellation ? "after-unlock" : "none",
  })
  if (holder.pendingCancellation) lock.cancellations.push(`observed:${operation.task}`)
  if (lock.kind !== "read-write") {
    lock.lastUnlock = holder.ticket
    lock.holder = null
    return
  }

  if (holder.access === "inout") {
    lock.writer = null
    lock.lastPhaseClose = holder.phase
    lock.happensBefore.push(`unlock:${holder.ticket}->phase-close:${holder.phase}`)
    lock.closedPhases.push({ phase: holder.phase, access: "write", tickets: [holder.ticket] })
    return
  }

  lock.readers = lock.readers.filter((item) => item.task !== operation.task)
  lock.readerUnlocks.push({ phase: holder.phase, ticket: holder.ticket })
  if (lock.readers.some((item) => item.phase === holder.phase)) return
  const tickets = lock.readerUnlocks
    .filter((item) => item.phase === holder.phase)
    .map((item) => item.ticket)
    .sort((left, right) => left - right)
  lock.lastPhaseClose = holder.phase
  lock.happensBefore.push(`readers:${tickets.join(",")}->phase-close:${holder.phase}`)
  lock.closedPhases.push({ phase: holder.phase, access: "read", tickets })
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
        readers: [],
        writer: null,
        queue: [],
        nextTicket: 0,
        nextPhase: 0,
        lastUnlock: null,
        lastPhaseClose: null,
        readerUnlocks: [],
        closedPhases: [],
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
      if (lock.kind === "read-write") fail("lockGrantFormInvalid")
      if (lock.holder) fail("lockAlreadyHeld")
      const waiter = lock.queue[0]
      if (!waiter || waiter.task !== operation.task) fail("W-LOCK-0009")
      lock.queue.shift()
      admit(lock, waiter, "queue")
      return
    }

    case "grantPhase": {
      const lock = requireLock(state, operation.lock)
      if (lock.kind !== "read-write") fail("lockGrantFormInvalid")
      if (lock.writer) fail("lockAlreadyHeld")
      const head = lock.queue[0]
      if (!head) fail("lockWaiterMissing")

      if (head.access === "inout") {
        if (lock.readers.length > 0) fail("lockAlreadyHeld")
        if (operation.task !== head.task || operation.tasks !== undefined) fail("W-LOCK-0009")
        lock.queue.shift()
        beginReadWritePhase(lock, [head], "queue")
        return
      }

      const prefix = []
      while (lock.queue[0]?.access === "ref") prefix.push(lock.queue.shift())
      const expectedTasks = prefix.map((item) => item.task)
      if (JSON.stringify(operation.tasks) !== JSON.stringify(expectedTasks)) fail("W-LOCK-0009")
      if (lock.readers.length > 0) joinReadWritePhase(lock, prefix, "queue")
      else beginReadWritePhase(lock, prefix, "queue")
      return
    }

    case "tryAcquire": {
      const lock = requireLock(state, operation.lock)
      validateRequest(lock, operation, true)
      if (taskIsActive(lock, operation.task)) fail("W-LOCK-0004")
      const readCanJoin =
        lock.kind === "read-write" &&
        operation.access === "ref" &&
        lock.writer === null &&
        lock.queue.length === 0
      const writeCanEnter =
        lock.kind === "read-write" &&
        operation.access === "inout" &&
        !hasActiveHolder(lock) &&
        lock.queue.length === 0
      const ordinaryCanEnter =
        lock.kind !== "read-write" && lock.holder === null && lock.queue.length === 0
      if (!readCanJoin && !writeCanEnter && !ordinaryCanEnter) {
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
      if (lock.kind === "read-write") {
        if (readCanJoin && lock.readers.length > 0) {
          joinReadWritePhase(lock, [waiter], "try")
        } else {
          beginReadWritePhase(lock, [waiter], "try")
        }
      } else {
        admit(lock, waiter, "try")
      }
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
      lock.writer = null
      lock.readers = []
      lock.queue = []
      state.failedBoundaries.push(lock.boundary)
      fail("lockBoundaryPanicked", "fault")
    }

    case "drop": {
      const lock = requireLock(state, operation.lock)
      if (hasActiveHolder(lock) || lock.queue.length > 0) fail("W-LOCK-0008")
      if (lock.drops !== 0) fail("lockDropRepeated")
      lock.drops = 1
      lock.phase = "closed"
      lock.trace.push("drop")
      return
    }

    case "unsupported":
      if (!new Set(["Condition", "Once"]).has(operation.primitive)) {
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
    if (
      lock.phase === "closed" &&
      (hasActiveHolder(lock) || lock.queue.length > 0 || lock.drops !== 1)
    ) {
      fail("lockClosedStateInvalid")
    }
    if (lock.kind === "read-write" && lock.writer && lock.readers.length > 0) {
      fail("lockReadWriteOverlap")
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
  if (
    facts.parallelReads === true &&
    facts.exclusiveWrite === true &&
    facts.synchronousContext === true
  ) {
    return "read-write-lock"
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
      holder: lock.kind === "read-write" ? lock.writer?.task ?? null : lock.holder?.task ?? null,
      readers: lock.readers.map((item) => item.task),
      queue: lock.queue.map((item) => item.task),
      drops: lock.drops,
      outcomes: lock.outcomes,
      cancellations: lock.cancellations,
      happensBefore: lock.happensBefore,
      closedPhases: lock.closedPhases,
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
