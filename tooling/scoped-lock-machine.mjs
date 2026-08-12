const ACCESS_MODES = new Set(["ref", "inout"])
const WAITING_FORMS = new Set(["sync", "await"])

class LanguageLockModelError extends Error {
  constructor(code, status = "rejected") {
    super(code)
    this.code = code
    this.status = status
  }
}

function fail(code, status = "rejected") {
  throw new LanguageLockModelError(code, status)
}

function requireText(value, code) {
  if (typeof value !== "string" || value.length === 0) fail(code)
  return value
}

function requireOwner(state, name) {
  const owner = state.owners[name]
  if (!owner) fail("sharedOwnerMissing")
  return owner
}

function requireOpen(owner) {
  if (owner.phase !== "open") fail("sharedOwnerNotOpen")
}

function activeTask(owner, task) {
  return owner.holder?.task === task || owner.waiters.some((waiter) => waiter.task === task)
}

function validateSharedConstruction(operation) {
  if (operation.lifetimeIndependent !== true) fail("W-BORROW-0010")
  if (operation.source === "binding" && operation.take !== true) fail("W-OWNERSHIP-0010")
  if (operation.source !== "binding" && operation.source !== "temporary") {
    fail("sharedSourceInvalid")
  }
}

function makeOwner(operation, allocation) {
  return {
    allocation,
    boundary: operation.boundary,
    value: operation.value,
    phase: "open",
    holder: null,
    waiters: [],
    nextWait: 0,
    lastUnlock: null,
    drops: 0,
    bodyEvaluations: 0,
    cancellations: [],
    happensBefore: [],
    outcomes: [],
    trace: [`create:${operation.form}`],
  }
}

function validateTarget(owner, operation) {
  requireOpen(owner)
  requireText(operation.task, "lockTaskMissing")
  if (!ACCESS_MODES.has(operation.access)) fail("lockAccessInvalid")
  if (operation.boundary !== owner.boundary) fail("W-LOCK-0005")
  if (activeTask(owner, operation.task)) fail("W-LOCK-0004")
}

function validateBody(operation) {
  const body = operation.body ?? {}
  if (body.neverSuspend !== true) fail("W-LOCK-0002")
  if (body.neverThrow !== true) fail("W-LOCK-0011")
  if (body.nonBlocking !== true) fail("W-LOCK-0012")
  if (body.resultIndependent !== true) fail("W-LOCK-0001")
}

function grant(owner, waiter, source) {
  if (owner.holder) fail("lockAlreadyHeld")
  owner.holder = {
    task: waiter.task,
    access: waiter.access,
    form: waiter.form,
    wait: waiter.wait,
    pendingCancellation: false,
  }
  owner.trace.push(`grant:${waiter.wait}:${waiter.task}:${source}`)
  if (owner.lastUnlock !== null) {
    owner.happensBefore.push(`unlock:${owner.lastUnlock}->grant:${waiter.wait}`)
  }
}

function requireHolder(owner, task) {
  if (!owner.holder || owner.holder.task !== task) fail("lockHolderMissing")
  return owner.holder
}

export function selectSynchronization(facts) {
  if (facts.durable || facts.distributed || facts.keyedIdentity) return "service"
  if (facts.transferOwnership || facts.mailbox || facts.closeProtocol) return "channel"
  if (facts.uniqueOwner || facts.taskLocal) return "owner"
  if (facts.scalar && facts.singleLocation) return "atomic"
  if (facts.immutableVersions && facts.readHeavy) return "snapshot-cell"
  if (facts.taskOwnedMutableState) return "serial-domain"
  if (facts.parallelReads && facts.exclusiveTaskWrite && facts.closedAccessGraph) {
    return "domain-barrier"
  }
  if (facts.synchronousForeign && facts.shortCriticalSection && facts.sameBoundary) {
    return "language-lock"
  }
  if (facts.kernelOrOsSpecialization && facts.unsafeAdapterContract) return "specialized-adapter"
  if (facts.readWriteLockRequested) return "rejected-read-write-lock"
  return "insufficient-facts"
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "declareShared": {
      requireText(operation.owner, "sharedOwnerNameMissing")
      requireText(operation.boundary, "sharedBoundaryMissing")
      if (state.owners[operation.owner]) fail("sharedOwnerAlreadyExists")
      if (!new Set(["binding", "storedField"]).has(operation.context)) {
        fail("W-OWNERSHIP-0013")
      }
      if (operation.explicitSharedType !== true) fail("W-OWNERSHIP-0013")
      validateSharedConstruction(operation)
      state.owners[operation.owner] = makeOwner(operation, "product.default")
      state.receipts.push({ operation: "shared", owner: operation.owner, allocation: "product.default" })
      return
    }

    case "share": {
      fail("retiredSharedConstructionCall")
    }

    case "request": {
      const owner = requireOwner(state, operation.owner)
      if (!WAITING_FORMS.has(operation.form)) fail("lockWaitingFormInvalid")
      validateTarget(owner, operation)
      validateBody(operation)
      if (operation.form === "sync" && operation.blockingAllowed !== true) fail("W-LOCK-0003")
      const waiter = {
        task: operation.task,
        access: operation.access,
        form: operation.form,
        wait: owner.nextWait,
      }
      owner.nextWait += 1
      owner.waiters.push(waiter)
      owner.trace.push(`request:${waiter.wait}:${waiter.task}:${waiter.form}`)
      state.receipts.push({ operation: "request", task: waiter.task, wait: waiter.wait })
      return
    }

    case "grant": {
      const owner = requireOwner(state, operation.owner)
      requireOpen(owner)
      if (owner.holder) fail("lockAlreadyHeld")
      const index = owner.waiters.findIndex((waiter) => waiter.task === operation.task)
      if (index < 0) fail("W-LOCK-0009")
      const [waiter] = owner.waiters.splice(index, 1)
      grant(owner, waiter, "provider")
      return
    }

    case "tryAcquire": {
      const owner = requireOwner(state, operation.owner)
      validateTarget(owner, operation)
      validateBody(operation)
      if (owner.holder || owner.waiters.length > 0 || operation.providerBusy === true) {
        owner.trace.push(`try-busy:${operation.task}`)
        state.receipts.push({ operation: "try", task: operation.task, result: "busy" })
        return
      }
      const waiter = {
        task: operation.task,
        access: operation.access,
        form: "try",
        wait: owner.nextWait,
      }
      owner.nextWait += 1
      grant(owner, waiter, "try")
      state.receipts.push({ operation: "try", task: operation.task, result: "acquired" })
      return
    }

    case "read": {
      const owner = requireOwner(state, operation.owner)
      requireHolder(owner, operation.task)
      owner.bodyEvaluations += 1
      state.reads.push(owner.value)
      owner.trace.push(`read:${operation.task}`)
      return
    }

    case "write": {
      const owner = requireOwner(state, operation.owner)
      const holder = requireHolder(owner, operation.task)
      if (holder.access !== "inout") fail("W-LOCK-0006")
      owner.bodyEvaluations += 1
      owner.value = operation.value
      owner.trace.push(`write:${operation.task}`)
      return
    }

    case "finish": {
      const owner = requireOwner(state, operation.owner)
      const holder = requireHolder(owner, operation.task)
      if (operation.resultIndependent !== true) fail("W-LOCK-0001")
      const cancellation = holder.pendingCancellation ? "after-unlock" : "none"
      owner.outcomes.push({ task: holder.task, outcome: "success", cancellation })
      owner.happensBefore.push(`body:${holder.wait}->unlock:${holder.wait}`)
      owner.lastUnlock = holder.wait
      owner.trace.push(`finish:${holder.wait}:${holder.task}`)
      owner.holder = null
      if (cancellation === "after-unlock") owner.cancellations.push(`observed:${holder.task}`)
      return
    }

    case "cancelWait": {
      const owner = requireOwner(state, operation.owner)
      const index = owner.waiters.findIndex((waiter) => waiter.task === operation.task)
      if (index < 0) fail("lockWaiterMissing")
      if (owner.waiters[index].form !== "await") fail("lockSyncWaitNotCancellable")
      owner.waiters.splice(index, 1)
      owner.cancellations.push(`removed:${operation.task}`)
      owner.trace.push(`cancel-wait:${operation.task}`)
      return
    }

    case "cancelHeld": {
      const owner = requireOwner(state, operation.owner)
      const holder = requireHolder(owner, operation.task)
      if (holder.form !== "await") fail("lockSyncHolderNotCancellable")
      holder.pendingCancellation = true
      owner.cancellations.push(`deferred:${operation.task}`)
      return
    }

    case "suspend":
      requireHolder(requireOwner(state, operation.owner), operation.task)
      fail("W-LOCK-0002")

    case "throw":
      requireHolder(requireOwner(state, operation.owner), operation.task)
      fail("W-LOCK-0011")

    case "block":
      requireHolder(requireOwner(state, operation.owner), operation.task)
      fail("W-LOCK-0012")

    case "escape":
      requireHolder(requireOwner(state, operation.owner), operation.task)
      fail("W-LOCK-0001")

    case "unguardedOverlap":
      requireOwner(state, operation.owner)
      fail("W-LOCK-0013")

    case "drop": {
      const owner = requireOwner(state, operation.owner)
      if (owner.holder || owner.waiters.length > 0) fail("W-LOCK-0008")
      if (owner.drops !== 0) fail("sharedDropRepeated")
      owner.drops = 1
      owner.phase = "closed"
      owner.trace.push("drop")
      return
    }

    case "panic": {
      const owner = requireOwner(state, operation.owner)
      requireHolder(owner, operation.task)
      owner.phase = "faulted"
      owner.holder = null
      owner.waiters = []
      state.failedBoundaries.push(owner.boundary)
      fail("lockBoundaryPanicked", "fault")
    }

    case "unsupported":
      fail("W-SYNC-0001")

    case "select":
      state.selections.push(selectSynchronization(operation.facts ?? {}))
      return

    default:
      fail("lockOperationUnknown")
  }
}

function projectState(state) {
  const owners = {}
  for (const [name, owner] of Object.entries(state.owners)) {
    owners[name] = {
      allocation: owner.allocation,
      boundary: owner.boundary,
      value: owner.value,
      phase: owner.phase,
      holder: owner.holder?.task ?? null,
      waiters: owner.waiters.map((waiter) => waiter.task),
      drops: owner.drops,
      bodyEvaluations: owner.bodyEvaluations,
      cancellations: owner.cancellations,
      happensBefore: owner.happensBefore,
      outcomes: owner.outcomes,
      trace: owner.trace,
    }
  }
  return {
    owners,
    receipts: state.receipts,
    reads: state.reads,
    selections: state.selections,
    failedBoundaries: state.failedBoundaries,
  }
}

export function runScopedLockOperations(operations) {
  const state = {
    owners: {},
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
    } catch (cause) {
      if (!(cause instanceof LanguageLockModelError)) throw cause
      status = cause.status
      error = cause.code
      failedOperation = index
      break
    }
  }

  return { status, error, failedOperation, state: projectState(state) }
}
