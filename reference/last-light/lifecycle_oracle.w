// Pure oracle for task lifetime, service turn, and commit transitions.

enum TaskLifecycle {
  reserved
  published
  bodySettled
  cleanup
  outcomeCommitted
  joined
}

enum TaskEvent {
  publish
  settleSuccess
  settleError
  settleCanceled
  requestCancel
  runCleanup
  commitOutcome
  join
}

const fn nextTaskState(
  state: TaskLifecycle,
  on event: TaskEvent,
): TaskLifecycle? {
  return switch (state, event) {
    case (.reserved, .publish): .some(.published)
    case (.published, .settleSuccess): .some(.bodySettled)
    case (.published, .settleError): .some(.bodySettled)
    case (.published, .settleCanceled): .some(.bodySettled)
    case (.published, .requestCancel): .some(.published)
    case (.bodySettled, .requestCancel): .some(.bodySettled)
    case (.bodySettled, .runCleanup): .some(.cleanup)
    case (.cleanup, .commitOutcome): .some(.outcomeCommitted)
    case (.outcomeCommitted, .join): .some(.joined)
    case (_, _): .none
  }
}

enum ServiceTurnState {
  admitted
  bodySettled
  committing
  committed
  commitFailed
  unknownOutcome
  drained
}

enum ServiceTurnEvent {
  settleBody
  beginCommit
  confirmCommit
  failCommit
  loseCommitConfirmation
  drain
}

const fn nextServiceTurnState(
  state: ServiceTurnState,
  on event: ServiceTurnEvent,
): ServiceTurnState? {
  return switch (state, event) {
    case (.admitted, .settleBody): .some(.bodySettled)
    case (.bodySettled, .beginCommit): .some(.committing)
    case (.committing, .confirmCommit): .some(.committed)
    case (.committing, .failCommit): .some(.commitFailed)
    case (.committing, .loseCommitConfirmation): .some(.unknownOutcome)
    case (.committed, .drain): .some(.drained)
    case (.commitFailed, .drain): .some(.drained)
    case (.unknownOutcome, .drain): .some(.drained)
    case (_, _): .none
  }
}

// W-1244: one provider closes one bounded frontier with one terminal decision.
enum CommitProviderState {
  collecting
  closing
  committed
  aborted
  unknown
}

enum CommitProviderEvent {
  registerDependency
  closeFrontier
  confirmCommit
  confirmAbort
  loseEvidence
}

const fn nextCommitProviderState(
  state: CommitProviderState,
  on event: CommitProviderEvent,
): CommitProviderState? {
  return switch (state, event) {
    case (.collecting, .registerDependency): .some(.collecting)
    case (.collecting, .closeFrontier): .some(.closing)
    case (.closing, .confirmCommit): .some(.committed)
    case (.closing, .confirmAbort): .some(.aborted)
    case (.closing, .loseEvidence): .some(.unknown)
    case (_, _): .none
  }
}

test "task outcome is published only after cleanup" for nextTaskState {
  let published = nextTaskState(state: .reserved, on: .publish)
  let settled = nextTaskState(state: .published, on: .settleSuccess)
  let cleaned = nextTaskState(state: .bodySettled, on: .runCleanup)
  let committed = nextTaskState(state: .cleanup, on: .commitOutcome)
  let joined = nextTaskState(state: .outcomeCommitted, on: .join)

  expect published == .some(.published)
  expect settled == .some(.bodySettled)
  expect cleaned == .some(.cleanup)
  expect committed == .some(.outcomeCommitted)
  expect joined == .some(.joined)
}

test "late cancellation cannot replace a settled task body" for nextTaskState {
  expect nextTaskState(state: .published, on: .requestCancel) == .some(.published)
  expect nextTaskState(state: .bodySettled, on: .requestCancel) == .some(.bodySettled)
  expect nextTaskState(state: .reserved, on: .runCleanup) == none
  expect nextTaskState(state: .cleanup, on: .join) == none
}

test "a service turn has one terminal commit observation" for nextServiceTurnState {
  let settled = nextServiceTurnState(state: .admitted, on: .settleBody)
  let committing = nextServiceTurnState(state: .bodySettled, on: .beginCommit)
  let committed = nextServiceTurnState(state: .committing, on: .confirmCommit)
  let drained = nextServiceTurnState(state: .committed, on: .drain)

  expect settled == .some(.bodySettled)
  expect committing == .some(.committing)
  expect committed == .some(.committed)
  expect drained == .some(.drained)
  expect nextServiceTurnState(state: .committing, on: .loseCommitConfirmation)
    == .some(.unknownOutcome)
  expect nextServiceTurnState(state: .committing, on: .failCommit)
    == .some(.commitFailed)
}

test "commit uncertainty is not an abort" for nextServiceTurnState {
  expect nextServiceTurnState(state: .unknownOutcome, on: .drain) == .some(.drained)
  expect nextServiceTurnState(state: .unknownOutcome, on: .confirmCommit) == none
  expect nextServiceTurnState(state: .commitFailed, on: .confirmCommit) == none
  expect nextServiceTurnState(state: .drained, on: .settleBody) == none
}

test "a commit provider publishes one stable terminal" for nextCommitProviderState {
  expect nextCommitProviderState(state: .collecting, on: .registerDependency)
    == .some(.collecting)
  expect nextCommitProviderState(state: .collecting, on: .closeFrontier)
    == .some(.closing)
  expect nextCommitProviderState(state: .closing, on: .confirmCommit)
    == .some(.committed)
  expect nextCommitProviderState(state: .closing, on: .confirmAbort)
    == .some(.aborted)
  expect nextCommitProviderState(state: .closing, on: .loseEvidence)
    == .some(.unknown)
  expect nextCommitProviderState(state: .committed, on: .confirmAbort) == none
  expect nextCommitProviderState(state: .closing, on: .registerDependency) == none
}
