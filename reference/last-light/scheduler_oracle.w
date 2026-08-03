// Pure oracle for logical scheduler replay and injected boundary outcomes.

enum SchedulerPacking {
  singleWorker
  twoWorkers
  fourWorkers
  remoteService
}

enum ReplayVerdict {
  sameLogicalExecution
  physicalPackingOnly
  reject
}

enum ReplayEvent {
  taskPublished
  taskSettled
  cleanupCompleted
  taskOutcomeCommitted
  serviceAdmitted
  commitStarted
  commitConfirmed
  commitFailed
  unknownOutcome
  ownerClosed
  workerAssigned
  threadMigrated
  queueSlotSelected
  transportHop
}

const fn contributesToLogicalTrace(event: ReplayEvent): Bool {
  return switch event {
    case .taskPublished: true
    case .taskSettled: true
    case .cleanupCompleted: true
    case .taskOutcomeCommitted: true
    case .serviceAdmitted: true
    case .commitStarted: true
    case .commitConfirmed: true
    case .commitFailed: true
    case .unknownOutcome: true
    case .ownerClosed: true
    case .workerAssigned: false
    case .threadMigrated: false
    case .queueSlotSelected: false
    case .transportHop: false
  }
}

struct ReplayComparison {
  scheduleIdSame: Bool
  decisionsSame: Bool
  logicalTraceSame: Bool
  outcomeSame: Bool
  ownersClosed: Bool
  packingSame: Bool
}

const fn compareReplay(comparison: ReplayComparison): ReplayVerdict {
  if !comparison.scheduleIdSame
    || !comparison.decisionsSame
    || !comparison.logicalTraceSame
    || !comparison.outcomeSame
    || !comparison.ownersClosed {
    return .reject
  }

  if !comparison.packingSame {
    return .physicalPackingOnly
  }

  return .sameLogicalExecution
}

enum InjectedDecision {
  cancelBeforePublish
  cancelAfterPublish
  bodyError
  commitFailure
  commitConfirmationLost
  panic
}

enum InjectedOutcome {
  notPublished
  taskCanceled
  applicationError
  commitFailed
  unknownOutcome
  faultBoundary
}

enum FaultBoundary {
  task
  serviceTurn
  storage
  transport
  allocator
  host
}

enum FaultPoint {
  beforePublish
  afterPublish
  beforeAwait
  afterAwait
  body
  duringCleanup
  beforeCommit
  afterCommitBeforeConfirm
}

enum FaultAction {
  requestCancel
  returnApplicationError
  panicBoundary
  failCommit
  loseCommitConfirmation
  disconnectTransport
  exhaustQuota
}

struct FaultSpec {
  caseId: u32
  boundary: FaultBoundary
  point: FaultPoint
  action: FaultAction
  occurrence: u32
}

const maximumFaultOccurrence: u32 = 1_000_000

const fn validFault(spec: FaultSpec): Bool {
  guard spec.caseId > 0 && spec.occurrence <= maximumFaultOccurrence else {
    return false
  }

  return switch (spec.action, spec.boundary, spec.point) {
    case (.requestCancel, .task, .beforePublish): true
    case (.requestCancel, .task, .afterPublish): true
    case (.requestCancel, .task, .beforeCommit): true
    case (.requestCancel, .serviceTurn, .beforeCommit): true
    case (.returnApplicationError, .task, .body): true
    case (.returnApplicationError, .serviceTurn, .body): true
    case (.panicBoundary, _, .body): true
    case (.panicBoundary, _, .duringCleanup): true
    case (.failCommit, .serviceTurn, .beforeCommit): true
    case (.failCommit, .storage, .beforeCommit): true
    case (.failCommit, .storage, .afterCommitBeforeConfirm): true
    case (.loseCommitConfirmation, .storage, .afterCommitBeforeConfirm): true
    case (.loseCommitConfirmation, .transport, .afterCommitBeforeConfirm): true
    case (.disconnectTransport, .transport, .beforeAwait): true
    case (.disconnectTransport, .transport, .afterAwait): true
    case (.disconnectTransport, .serviceTurn, .afterAwait): true
    case (.exhaustQuota, .task, .beforePublish): true
    case (.exhaustQuota, .task, .beforeAwait): true
    case (.exhaustQuota, .serviceTurn, .beforePublish): true
    case (.exhaustQuota, .allocator, .beforePublish): true
    case (.exhaustQuota, .host, .beforePublish): true
    case (_, _, _): false
  }
}

const fn expectedInjectedOutcome(for decision: InjectedDecision): InjectedOutcome {
  return switch decision {
    case .cancelBeforePublish: .notPublished
    case .cancelAfterPublish: .taskCanceled
    case .bodyError: .applicationError
    case .commitFailure: .commitFailed
    case .commitConfirmationLost: .unknownOutcome
    case .panic: .faultBoundary
  }
}

test "replay compares logical facts, not worker placement" for compareReplay {
  let samePacking = ReplayComparison(
    scheduleIdSame: true,
    decisionsSame: true,
    logicalTraceSame: true,
    outcomeSame: true,
    ownersClosed: true,
    packingSame: true,
  )
  expect compareReplay(samePacking) == .sameLogicalExecution

  let differentPacking = ReplayComparison(
    scheduleIdSame: true,
    decisionsSame: true,
    logicalTraceSame: true,
    outcomeSame: true,
    ownersClosed: true,
    packingSame: false,
  )
  expect compareReplay(differentPacking) == .physicalPackingOnly

  let drift = ReplayComparison(
    scheduleIdSame: true,
    decisionsSame: true,
    logicalTraceSame: false,
    outcomeSame: true,
    ownersClosed: true,
    packingSame: true,
  )
  expect compareReplay(drift) == .reject

  let leak = ReplayComparison(
    scheduleIdSame: true,
    decisionsSame: true,
    logicalTraceSame: true,
    outcomeSame: true,
    ownersClosed: false,
    packingSame: false,
  )
  expect compareReplay(leak) == .reject
}

test "fault injection keeps cancellation, errors, and commit uncertainty distinct" for expectedInjectedOutcome {
  expect expectedInjectedOutcome(for: .cancelBeforePublish) == .notPublished
  expect expectedInjectedOutcome(for: .cancelAfterPublish) == .taskCanceled
  expect expectedInjectedOutcome(for: .bodyError) == .applicationError
  expect expectedInjectedOutcome(for: .commitFailure) == .commitFailed
  expect expectedInjectedOutcome(for: .commitConfirmationLost) == .unknownOutcome
  expect expectedInjectedOutcome(for: .panic) == .faultBoundary
}

test "worker and transport details stay in the physical sidecar" for contributesToLogicalTrace {
  expect contributesToLogicalTrace(.taskPublished)
  expect contributesToLogicalTrace(.commitConfirmed)
  expect contributesToLogicalTrace(.unknownOutcome)
  expect contributesToLogicalTrace(.ownerClosed)
  expect !contributesToLogicalTrace(.workerAssigned)
  expect !contributesToLogicalTrace(.threadMigrated)
  expect !contributesToLogicalTrace(.queueSlotSelected)
  expect !contributesToLogicalTrace(.transportHop)
}

test "fault cases are bounded and match their boundary" for validFault {
  expect validFault(FaultSpec(
    caseId: 1,
    boundary: .task,
    point: .afterPublish,
    action: .requestCancel,
    occurrence: 0,
  ))
  expect validFault(FaultSpec(
    caseId: 2,
    boundary: .storage,
    point: .afterCommitBeforeConfirm,
    action: .loseCommitConfirmation,
    occurrence: 3,
  ))
  expect !validFault(FaultSpec(
    caseId: 3,
    boundary: .task,
    point: .body,
    action: .failCommit,
    occurrence: 0,
  ))
  expect !validFault(FaultSpec(
    caseId: 0,
    boundary: .host,
    point: .beforePublish,
    action: .exhaustQuota,
    occurrence: 0,
  ))
  expect !validFault(FaultSpec(
    caseId: 4,
    boundary: .task,
    point: .beforePublish,
    action: .exhaustQuota,
    occurrence: 1_000_001,
  ))
}
