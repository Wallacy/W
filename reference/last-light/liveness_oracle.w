// Pure oracle for E1 runtime closure, provider completion, reclamation and shutdown.

enum TaskClosurePhase {
  open
  closing
  childWaitDrain
  explicitCleanup
  typedDrop
  runtimeQuiescent
  committed
}

enum TaskClosureEvent {
  closeAdmission
  drainChildrenAndWaits
  runCleanup
  finishCleanup
  typedDrop
  drainRuntime
  commitOutcome
}

const fn nextTaskClosurePhase(
  state: TaskClosurePhase,
  on event: TaskClosureEvent,
): TaskClosurePhase? {
  return switch (state, event) {
    case (.open, .closeAdmission): .some(.closing)
    case (.closing, .drainChildrenAndWaits): .some(.childWaitDrain)
    case (.childWaitDrain, .runCleanup): .some(.explicitCleanup)
    case (.explicitCleanup, .finishCleanup): .some(.typedDrop)
    case (.typedDrop, .typedDrop): .some(.typedDrop)
    case (.typedDrop, .drainRuntime): .some(.runtimeQuiescent)
    case (.runtimeQuiescent, .commitOutcome): .some(.committed)
    case (_, _): .none
  }
}

enum RuntimeWaitPhase {
  registered
  submitted
  completing
  terminal
  drained
}

enum RuntimeWaitEvent {
  submit
  cancelBeforeSubmit
  providerStarted
  providerSuccess
  providerError
  providerCanceled
  drain
}

enum CompletionDisposition {
  selectedSuccess
  selectedError
  selectedCanceled
  lateDrained
  staleGeneration
}

enum ProviderOutcome {
  success
  error
  canceled
}

enum CancelDisposition {
  requested
  localDrained
  late
}

const fn nextRuntimeWaitPhase(
  state: RuntimeWaitPhase,
  on event: RuntimeWaitEvent,
): RuntimeWaitPhase? {
  return switch (state, event) {
    case (.registered, .submit): .some(.submitted)
    case (.submitted, .providerStarted): .some(.completing)
    case (.completing, .providerSuccess): .some(.terminal)
    case (.completing, .providerError): .some(.terminal)
    case (.completing, .providerCanceled): .some(.terminal)
    case (.terminal, .drain): .some(.drained)
    case (.registered, .cancelBeforeSubmit): .some(.drained)
    case (_, _): .none
  }
}

const fn completionDisposition(
  state: RuntimeWaitPhase,
  generationMatches: Bool,
  providerOutcome: ProviderOutcome,
): CompletionDisposition {
  if state == .terminal || state == .drained {
    return .lateDrained
  }

  if !generationMatches {
    return .staleGeneration
  }

  return switch providerOutcome {
    case .success: .selectedSuccess
    case .error: .selectedError
    case .canceled: .selectedCanceled
  }
}

enum CleanupMaskPhase {
  inactive
  active
  expired
  faulted
}

struct CleanupNodeState {
  let active: Bool
  let incomingCancellationRecorded: Bool
  let localCancellationAllowed: Bool
  let mask: CleanupMaskPhase
}

const fn cleanupNodeMayContinue(node: CleanupNodeState): Bool {
  return node.active
    && node.localCancellationAllowed
    && node.mask == .active
}

struct CommitGate {
  let outcomeCandidate: Bool
  let admissionClosed: Bool
  let childrenDrained: Bool
  let waitsDrained: Bool
  let cleanupComplete: Bool
  let typedDropsComplete: Bool
  let runtimeQuiescent: Bool
}

const fn canCommitOutcome(gate: CommitGate): Bool {
  return gate.outcomeCandidate
    && gate.admissionClosed
    && gate.childrenDrained
    && gate.waitsDrained
    && gate.cleanupComplete
    && gate.typedDropsComplete
    && gate.runtimeQuiescent
}

struct FrameReclaimGate {
  let closureQuiescent: Bool
  let outcomeMoved: Bool
  let zeroChildren: Bool
  let zeroRegistrations: Bool
  let zeroQueueTickets: Bool
  let zeroTimers: Bool
  let zeroWakers: Bool
  let zeroRuntimeRefs: Bool
}

enum FrameReclaimPhase {
  live
  retired
  reclaimable
  reclaimed
}

const fn canReclaimFrame(gate: FrameReclaimGate): Bool {
  return gate.closureQuiescent
    && gate.outcomeMoved
    && gate.zeroChildren
    && gate.zeroRegistrations
    && gate.zeroQueueTickets
    && gate.zeroTimers
    && gate.zeroWakers
    && gate.zeroRuntimeRefs
}

enum BoundaryShutdownPhase {
  ready
  admissionClosed
  cancellationRequested
  draining
  quiescent
  stopped
  terminating
  terminated
}

enum ShutdownAction {
  closeAdmission
  requestCancellation
  beginDrain
  markQuiescent
  stopGracefully
  expireGrace
  terminateBoundary
  rejectAdmission
}

const fn nextBoundaryShutdownPhase(
  state: BoundaryShutdownPhase,
  on action: ShutdownAction,
): BoundaryShutdownPhase? {
  return switch (state, action) {
    case (.ready, .closeAdmission): .some(.admissionClosed)
    case (.admissionClosed, .requestCancellation): .some(.cancellationRequested)
    case (.cancellationRequested, .beginDrain): .some(.draining)
    case (.draining, .markQuiescent): .some(.quiescent)
    case (.quiescent, .stopGracefully): .some(.stopped)
    case (.draining, .expireGrace): .some(.terminating)
    case (.terminating, .terminateBoundary): .some(.terminated)
    case (.ready, .rejectAdmission): .some(.ready)
    case (.admissionClosed, .rejectAdmission): .some(.admissionClosed)
    case (.cancellationRequested, .rejectAdmission): .some(.cancellationRequested)
    case (.draining, .rejectAdmission): .some(.draining)
    case (.quiescent, .rejectAdmission): .some(.quiescent)
    case (.stopped, .rejectAdmission): .some(.stopped)
    case (.terminating, .rejectAdmission): .some(.terminating)
    case (.terminated, .rejectAdmission): .some(.terminated)
    case (_, _): .none
  }
}

test "closure publishes only after cleanup, drops and runtime drain" {
  let closed = nextTaskClosurePhase(state: .open, on: .closeAdmission)
  let drained = nextTaskClosurePhase(state: .closing, on: .drainChildrenAndWaits)
  let cleaned = nextTaskClosurePhase(state: .childWaitDrain, on: .runCleanup)
  let finished = nextTaskClosurePhase(state: .explicitCleanup, on: .finishCleanup)
  let dropped = nextTaskClosurePhase(state: .typedDrop, on: .typedDrop)
  let quiescent = nextTaskClosurePhase(state: .typedDrop, on: .drainRuntime)
  let committed = nextTaskClosurePhase(state: .runtimeQuiescent, on: .commitOutcome)

  expect closed == .some(.closing)
  expect drained == .some(.childWaitDrain)
  expect cleaned == .some(.explicitCleanup)
  expect finished == .some(.typedDrop)
  expect dropped == .some(.typedDrop)
  expect quiescent == .some(.runtimeQuiescent)
  expect committed == .some(.committed)
  expect nextTaskClosurePhase(state: .childWaitDrain, on: .commitOutcome) == none
}

test "provider completion is distinct from cancellation request" {
  let request = CancelDisposition.requested
  expect request == .requested
  expect nextRuntimeWaitPhase(state: .registered, on: .submit) == .some(.submitted)
  expect nextRuntimeWaitPhase(state: .registered, on: .cancelBeforeSubmit) == .some(.drained)
  expect nextRuntimeWaitPhase(state: .submitted, on: .providerStarted) == .some(.completing)
  expect nextRuntimeWaitPhase(state: .completing, on: .providerCanceled) == .some(.terminal)
  expect completionDisposition(
    state: .completing,
    generationMatches: true,
    providerOutcome: .canceled,
  ) == .selectedCanceled
  expect completionDisposition(
    state: .completing,
    generationMatches: false,
    providerOutcome: .canceled,
  ) == .staleGeneration
  expect completionDisposition(
    state: .terminal,
    generationMatches: true,
    providerOutcome: .success,
  ) == .lateDrained
  expect completionDisposition(
    state: .drained,
    generationMatches: false,
    providerOutcome: .error,
  ) == .lateDrained
}

test "cleanup mask records incoming cancel but expires on its boundary" {
  expect cleanupNodeMayContinue(node: CleanupNodeState(
    active: true,
    incomingCancellationRecorded: true,
    localCancellationAllowed: true,
    mask: .active,
  ))
  expect !cleanupNodeMayContinue(node: CleanupNodeState(
    active: true,
    incomingCancellationRecorded: true,
    localCancellationAllowed: true,
    mask: .expired,
  ))
}

test "outcome and frame gates are independent of normal join" {
  let gate = CommitGate(
    outcomeCandidate: true,
    admissionClosed: true,
    childrenDrained: true,
    waitsDrained: true,
    cleanupComplete: true,
    typedDropsComplete: true,
    runtimeQuiescent: true,
  )
  expect canCommitOutcome(gate: gate)

  let reclaim = FrameReclaimGate(
    closureQuiescent: true,
    outcomeMoved: true,
    zeroChildren: true,
    zeroRegistrations: true,
    zeroQueueTickets: true,
    zeroTimers: true,
    zeroWakers: true,
    zeroRuntimeRefs: true,
  )
  expect canReclaimFrame(gate: reclaim)
  expect !canReclaimFrame(gate: FrameReclaimGate(
    closureQuiescent: true,
    outcomeMoved: true,
    zeroChildren: true,
    zeroRegistrations: false,
    zeroQueueTickets: true,
    zeroTimers: true,
    zeroWakers: true,
    zeroRuntimeRefs: true,
  ))
}

test "shutdown escalation keeps forced termination distinct" {
  let closed = nextBoundaryShutdownPhase(state: .ready, on: .closeAdmission)
  let requested = nextBoundaryShutdownPhase(state: .admissionClosed, on: .requestCancellation)
  let draining = nextBoundaryShutdownPhase(state: .cancellationRequested, on: .beginDrain)
  let quiescent = nextBoundaryShutdownPhase(state: .draining, on: .markQuiescent)
  let stopped = nextBoundaryShutdownPhase(state: .quiescent, on: .stopGracefully)
  let expiring = nextBoundaryShutdownPhase(state: .draining, on: .expireGrace)
  let terminated = nextBoundaryShutdownPhase(state: .terminating, on: .terminateBoundary)

  expect closed == .some(.admissionClosed)
  expect requested == .some(.cancellationRequested)
  expect draining == .some(.draining)
  expect quiescent == .some(.quiescent)
  expect stopped == .some(.stopped)
  expect expiring == .some(.terminating)
  expect terminated == .some(.terminated)
  expect nextBoundaryShutdownPhase(state: .ready, on: .stopGracefully) == none
}
