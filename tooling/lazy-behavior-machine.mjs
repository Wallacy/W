const LOWERINGS = new Set(["local", "isolated", "concurrent"])
const INITIALIZER_EFFECTS = new Set(["compute", "allocate"])

export class LazyBehaviorModelError extends Error {
  constructor(code, status = "rejected") {
    super(code)
    this.name = "LazyBehaviorModelError"
    this.code = code
    this.status = status
  }
}

function fail(code, status = "rejected") {
  throw new LazyBehaviorModelError(code, status)
}

function requireString(value, code) {
  if (typeof value !== "string" || value.length === 0) fail(code)
}

function requireDeclared(state) {
  if (state.phase === "undeclared") fail("lazyNotDeclared")
  if (state.phase === "closed") fail("lazyAlreadyClosed")
  if (state.boundary !== "open") fail("lazyBoundaryNotOpen")
}

function activeWaiters(state) {
  return Object.values(state.waiters).filter((waiter) => waiter.phase === "waiting")
}

function dropCaptures(state) {
  if (state.captureState === "dropped") fail("lazyCaptureDropRepeated")
  if (state.captureState === "absent") return
  state.captureState = "dropped"
  state.captureDrops += 1
  state.logicalTrace.push("drop:initializer-captures")
}

function dropValue(state) {
  if (state.value === null) fail("lazyValueMissingAtDrop")
  state.valueDrops += 1
  state.logicalTrace.push(`drop:value:${state.value}`)
  state.value = null
}

function requireAccessProof(state, operation) {
  if (state.lowering !== "concurrent") return
  if (
    operation.blockingDomain === true ||
    operation.exclusiveProof === true ||
    operation.initializedBeforePublication === true
  ) {
    return
  }
  fail("W-LAZY-0002")
}

function observeValue(state, actor, origin) {
  requireString(actor, "lazyActorMissing")
  if (state.phase !== "initialized" || state.value === null) {
    fail("lazyValueNotInitialized")
  }
  state.observations.push({ actor, value: state.value, origin })
  if (state.lowering === "concurrent") {
    state.happensBefore.push(`publish:${state.publication}->read:${actor}`)
    state.physicalTrace.push(`acquire-read:${actor}:${state.publication}`)
  } else {
    state.physicalTrace.push(`plain-read:${actor}`)
  }
}

function claimInitializer(state, actor) {
  state.phase = "initializing"
  state.winner = actor
  state.initializerRuns += 1
  state.captureState = "executing"
  state.logicalTrace.push(`winner:${actor}`)
  if (state.lowering === "concurrent") {
    state.physicalTrace.push(`atomic-claim:${actor}`)
  } else if (state.lowering === "isolated") {
    state.physicalTrace.push(`isolated-claim:${actor}`)
  } else {
    state.physicalTrace.push(`local-claim:${actor}`)
  }
}

function publishValue(state, actor, value, origin) {
  requireString(value, "lazyValueMissing")
  state.publication += 1
  state.value = value
  state.phase = "initialized"
  state.winner = null
  state.logicalTrace.push(`publish:${origin}:${state.publication}:${value}`)
  if (state.captureState !== "dropped") dropCaptures(state)
  if (state.lowering === "concurrent") {
    state.physicalTrace.push(`release-publish:${actor}:${state.publication}`)
  } else {
    state.physicalTrace.push(`store-publish:${actor}:${state.publication}`)
  }
}

function validateInitializer(operation) {
  const effects = operation.initializerEffects ?? []
  if (!Array.isArray(effects) || effects.some((effect) => !INITIALIZER_EFFECTS.has(effect))) {
    fail("W-LAZY-0001")
  }
  if (operation.knownCycle === true) fail("W-LAZY-0003")
}

function faultInitializer(state, actor, reason, error) {
  if (state.phase !== "initializing" || state.winner !== actor) {
    fail("lazyFaultWithoutWinner")
  }
  dropCaptures(state)
  state.phase = "faulted"
  state.boundary = "faulted"
  for (const waiter of activeWaiters(state)) waiter.phase = "faulted"
  state.logicalTrace.push(`fault:${reason}:${actor}`)
  state.physicalTrace.push("wake-waiters-for-boundary-fault")
  fail(error, "fault")
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "declare": {
      if (state.phase !== "undeclared") fail("lazyAlreadyDeclared")
      if (!LOWERINGS.has(operation.lowering)) fail("lazyLoweringUnknown")
      validateInitializer(operation)
      if (operation.captureLifetimeValid === false) fail("W-LAZY-0005")
      if (
        operation.lowering === "concurrent" &&
        (operation.capturesTransferable !== true ||
          operation.capturesLifetimeIndependent !== true ||
          operation.valueShareable !== true)
      ) {
        fail("W-LAZY-0005")
      }
      state.phase = "uninitialized"
      state.lowering = operation.lowering
      state.owner = operation.owner ?? operation.lowering
      state.captureState = "stored"
      state.logicalTrace.push("declare:uninitialized")
      state.physicalTrace.push(`lowering:${operation.lowering}`)
      return
    }

    case "beginAccess": {
      requireDeclared(state)
      requireString(operation.actor, "lazyActorMissing")
      requireAccessProof(state, operation)
      if (state.phase === "uninitialized") {
        claimInitializer(state, operation.actor)
        return
      }
      if (state.phase === "initializing") {
        if (state.winner === operation.actor) {
          dropCaptures(state)
          state.phase = "faulted"
          state.boundary = "faulted"
          state.logicalTrace.push(`fault:reentry:${operation.actor}`)
          fail("W-LAZY-0003", "fault")
        }
        if (state.lowering !== "concurrent") fail("W-LAZY-0005")
        if (state.waiters[operation.actor]) fail("lazyWaiterDuplicate")
        state.waiters[operation.actor] = { phase: "waiting", cancellation: "none" }
        state.logicalTrace.push(`wait:${operation.actor}`)
        state.physicalTrace.push(`park:${operation.actor}`)
        return
      }
      if (state.phase === "initialized") {
        observeValue(state, operation.actor, "direct")
        return
      }
      fail("lazyAccessInvalidPhase")
    }

    case "publish": {
      requireDeclared(state)
      if (state.phase !== "initializing" || state.winner !== operation.actor) {
        fail("lazyPublisherNotWinner")
      }
      publishValue(state, operation.actor, operation.value, "initializer")
      return
    }

    case "resumeAccess": {
      requireDeclared(state)
      requireAccessProof(state, operation)
      const waiter = state.waiters[operation.actor]
      if (!waiter || waiter.phase !== "waiting") fail("lazyWaiterNotWaiting")
      if (state.phase !== "initialized") fail("lazyWaiterResumedBeforePublication")
      waiter.phase = "resumed"
      state.physicalTrace.push(`unpark:${operation.actor}`)
      observeValue(state, operation.actor, "waiter")
      return
    }

    case "read": {
      requireDeclared(state)
      requireAccessProof(state, operation)
      observeValue(state, operation.actor, "read")
      return
    }

    case "requestCancellation": {
      requireDeclared(state)
      requireString(operation.actor, "lazyActorMissing")
      const isWinner = state.phase === "initializing" && state.winner === operation.actor
      const waiter = state.waiters[operation.actor]
      if (!isWinner && (!waiter || waiter.phase !== "waiting")) {
        fail("lazyCancellationTargetInactive")
      }
      state.cancellations[operation.actor] = "pending"
      if (waiter) waiter.cancellation = "pending"
      state.logicalTrace.push(`cancel-pending:${operation.actor}`)
      return
    }

    case "observeCancellation": {
      requireDeclared(state)
      if (state.phase !== "initialized") fail("lazyCancellationBeforePublication")
      if (state.cancellations[operation.actor] !== "pending") {
        fail("lazyCancellationNotPending")
      }
      state.cancellations[operation.actor] = "observed"
      if (state.waiters[operation.actor]) {
        state.waiters[operation.actor].cancellation = "observed"
      }
      state.logicalTrace.push(`cancel-observed:${operation.actor}`)
      return
    }

    case "set": {
      requireDeclared(state)
      if (operation.exclusive !== true) fail("W-LAZY-0004")
      if (state.phase === "initializing" || activeWaiters(state).length > 0) {
        fail("W-LAZY-0006")
      }
      requireString(operation.value, "lazyValueMissing")
      if (state.phase === "uninitialized") {
        dropCaptures(state)
      } else if (state.phase === "initialized") {
        dropValue(state)
      } else {
        fail("lazySetInvalidPhase")
      }
      state.publication += 1
      state.value = operation.value
      state.phase = "initialized"
      state.logicalTrace.push(`publish:set:${state.publication}:${operation.value}`)
      state.physicalTrace.push(`exclusive-set:${state.publication}`)
      return
    }

    case "modify": {
      requireDeclared(state)
      if (operation.exclusive !== true) fail("W-LAZY-0004")
      if (state.phase === "initializing" || activeWaiters(state).length > 0) {
        fail("W-LAZY-0006")
      }
      requireString(operation.value, "lazyValueMissing")
      if (state.phase === "uninitialized") {
        requireString(operation.initializerValue, "lazyInitializerValueMissing")
        state.initializerRuns += 1
        state.captureState = "executing"
        publishValue(state, "exclusive-modify", operation.initializerValue, "initializer")
      }
      if (state.phase !== "initialized") fail("lazyModifyInvalidPhase")
      state.value = operation.value
      state.modifications += 1
      state.logicalTrace.push(`modify:${operation.value}`)
      return
    }

    case "panic": {
      requireDeclared(state)
      faultInitializer(
        state,
        operation.actor,
        "panic",
        "lazyInitializerPanic",
      )
      return
    }

    case "allocationFault": {
      requireDeclared(state)
      faultInitializer(
        state,
        operation.actor,
        "out-of-memory",
        "lazyInitializerOutOfMemory",
      )
      return
    }

    case "close": {
      requireDeclared(state)
      if (state.phase === "initializing" || activeWaiters(state).length > 0) {
        fail("W-LAZY-0006")
      }
      if (state.phase === "uninitialized") {
        dropCaptures(state)
      } else if (state.phase === "initialized") {
        dropValue(state)
      } else {
        fail("lazyCloseInvalidPhase")
      }
      state.phase = "closed"
      state.boundary = "closed"
      state.logicalTrace.push("close")
      state.physicalTrace.push("close-storage")
      return
    }

    default:
      fail(`lazyOperationUnknown:${operation.op}`)
  }
}

function verifyState(state) {
  if (state.captureDrops > 1) fail("lazyCaptureDropRepeated")
  if (state.phase === "uninitialized" && state.captureState !== "stored") {
    fail("lazyUninitializedCaptureInvalid")
  }
  if (state.phase === "initializing" && state.captureState !== "executing") {
    fail("lazyInitializingCaptureInvalid")
  }
  if (state.phase === "initialized") {
    if (state.value === null || state.captureState !== "dropped") {
      fail("lazyInitializedStateInvalid")
    }
  }
  if (state.phase === "closed" && (state.value !== null || state.captureState !== "dropped")) {
    fail("lazyClosedStateInvalid")
  }
  if (state.initializerRuns > 1) fail("lazyInitializerRepeated")
}

function projectState(state) {
  return {
    phase: state.phase,
    boundary: state.boundary,
    value: state.value,
    initializerRuns: state.initializerRuns,
    publication: state.publication,
    observations: state.observations,
    waiterPhases: Object.fromEntries(
      Object.entries(state.waiters).map(([actor, waiter]) => [actor, waiter.phase]),
    ),
    cancellations: state.cancellations,
    captureState: state.captureState,
    captureDrops: state.captureDrops,
    valueDrops: state.valueDrops,
    modifications: state.modifications,
    happensBefore: state.happensBefore,
    logicalTrace: state.logicalTrace,
  }
}

export function runLazyBehaviorOperations(operations) {
  const state = {
    phase: "undeclared",
    boundary: "open",
    lowering: null,
    owner: null,
    value: null,
    initializerRuns: 0,
    publication: 0,
    observations: [],
    waiters: {},
    cancellations: {},
    winner: null,
    captureState: "absent",
    captureDrops: 0,
    valueDrops: 0,
    modifications: 0,
    happensBefore: [],
    logicalTrace: [],
    physicalTrace: [],
  }

  let status = "accepted"
  let error = null
  for (const operation of operations) {
    try {
      applyOperation(state, operation)
      verifyState(state)
    } catch (caught) {
      if (!(caught instanceof LazyBehaviorModelError)) throw caught
      status = caught.status
      error = caught.code
      break
    }
  }

  return {
    status,
    error,
    state: projectState(state),
    physical: {
      lowering: state.lowering,
      owner: state.owner,
      trace: state.physicalTrace,
    },
  }
}
