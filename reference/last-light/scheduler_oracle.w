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
