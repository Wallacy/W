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
  from state: TaskClosurePhase,
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
  from state: RuntimeWaitPhase,
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
  active: Bool
  incomingCancellationRecorded: Bool
  localCancellationAllowed: Bool
  mask: CleanupMaskPhase
}

const fn cleanupNodeMayContinue(node: CleanupNodeState): Bool {
  return node.active
    && node.localCancellationAllowed
    && node.mask == .active
}

struct CommitGate {
  outcomeCandidate: Bool
  admissionClosed: Bool
  childrenDrained: Bool
  waitsDrained: Bool
  cleanupComplete: Bool
  typedDropsComplete: Bool
  runtimeQuiescent: Bool
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
  closureQuiescent: Bool
  outcomeMoved: Bool
  zeroChildren: Bool
  zeroRegistrations: Bool
  zeroQueueTickets: Bool
  zeroTimers: Bool
  zeroWakers: Bool
  zeroRuntimeRefs: Bool
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
  from state: BoundaryShutdownPhase,
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
  let closed = nextTaskClosurePhase(.open, on: .closeAdmission)
  let drained = nextTaskClosurePhase(.closing, on: .drainChildrenAndWaits)
  let cleaned = nextTaskClosurePhase(.childWaitDrain, on: .runCleanup)
  let finished = nextTaskClosurePhase(.explicitCleanup, on: .finishCleanup)
  let dropped = nextTaskClosurePhase(.typedDrop, on: .typedDrop)
  let quiescent = nextTaskClosurePhase(.typedDrop, on: .drainRuntime)
  let committed = nextTaskClosurePhase(.runtimeQuiescent, on: .commitOutcome)

  expect closed == .some(.closing)
  expect drained == .some(.childWaitDrain)
  expect cleaned == .some(.explicitCleanup)
  expect finished == .some(.typedDrop)
  expect dropped == .some(.typedDrop)
  expect quiescent == .some(.runtimeQuiescent)
  expect committed == .some(.committed)
  expect nextTaskClosurePhase(.childWaitDrain, on: .commitOutcome) == none
}

test "provider completion is distinct from cancellation request" {
  let request = CancelDisposition.requested
  expect request == .requested
  expect nextRuntimeWaitPhase(.registered, on: .submit) == .some(.submitted)
  expect nextRuntimeWaitPhase(.registered, on: .cancelBeforeSubmit) == .some(.drained)
  expect nextRuntimeWaitPhase(.submitted, on: .providerStarted) == .some(.completing)
  expect nextRuntimeWaitPhase(.completing, on: .providerCanceled) == .some(.terminal)
  expect completionDisposition(
    .completing,
    generationMatches: true,
    providerOutcome: .canceled,
  ) == .selectedCanceled
  expect completionDisposition(
    .completing,
    generationMatches: false,
    providerOutcome: .canceled,
  ) == .staleGeneration
  expect completionDisposition(
    .terminal,
    generationMatches: true,
    providerOutcome: .success,
  ) == .lateDrained
  expect completionDisposition(
    .drained,
    generationMatches: false,
    providerOutcome: .error,
  ) == .lateDrained
}

test "cleanup mask records incoming cancel but expires on its boundary" {
  expect cleanupNodeMayContinue(CleanupNodeState(
    active: true,
    incomingCancellationRecorded: true,
    localCancellationAllowed: true,
    mask: .active,
  ))
  expect !cleanupNodeMayContinue(CleanupNodeState(
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
  expect canCommitOutcome(gate)

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
  expect canReclaimFrame(reclaim)
  expect !canReclaimFrame(FrameReclaimGate(
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
  let closed = nextBoundaryShutdownPhase(.ready, on: .closeAdmission)
  let requested = nextBoundaryShutdownPhase(.admissionClosed, on: .requestCancellation)
  let draining = nextBoundaryShutdownPhase(.cancellationRequested, on: .beginDrain)
  let quiescent = nextBoundaryShutdownPhase(.draining, on: .markQuiescent)
  let stopped = nextBoundaryShutdownPhase(.quiescent, on: .stopGracefully)
  let expiring = nextBoundaryShutdownPhase(.draining, on: .expireGrace)
  let terminated = nextBoundaryShutdownPhase(.terminating, on: .terminateBoundary)

  expect closed == .some(.admissionClosed)
  expect requested == .some(.cancellationRequested)
  expect draining == .some(.draining)
  expect quiescent == .some(.quiescent)
  expect stopped == .some(.stopped)
  expect expiring == .some(.terminating)
  expect terminated == .some(.terminated)
  expect nextBoundaryShutdownPhase(.ready, on: .stopGracefully) == none
}
