// Public value contracts for runtime-owned supervised work.

export enum WorkRight {
  observe
  cancel
  signal
}

export enum WorkState {
  queued
  running
  waiting
  succeeded
  failed
  canceled
  boundaryFailed
}

export struct WorkSnapshot<Progress> {
  let id: WorkId
  let revision: u64
  let attempt: u32
  let state: WorkState
  let progress: Progress?
  let cancellation: Cancellation?
  let suspension: WorkSuspension?
}

export enum WorkOutcome<Output, Failure: Error> {
  success(Output)
  error(Failure)
  canceled(Cancellation)
  boundary(WorkBoundaryFailure)
}

export enum WorkBoundaryFailure {
  fault
  generationLost
  restartLimit
  operationUnavailable
  durability
  unknownOutcome(EffectId)
  historyMismatch(WorkflowPointId)
  historyLimit
}

export enum WorkCancelResult<Progress> {
  requested(WorkSnapshot<Progress>)
  alreadyRequested(WorkSnapshot<Progress>)
  terminal(WorkSnapshot<Progress>)
}

export enum WorkLookupError: Error {
  unknown
  outcomeExpired(WorkId)
}

export enum SupervisorFailure: Error {
  unavailable
  unauthorized
  incompatibleSchema
  unknownOutcome(EffectId)
}

export enum WorkStartError<Key, Input>: Error {
  duplicate(key: Key, current: WorkId, rejected: Input)
  full(Input)
  draining(Input)
  unavailable(Input)
  unauthorized(Input)
  incompatibleSchema(Input)
}

test "terminal work remains distinct from cancellation request" {
  let terminal: WorkState = .succeeded
  let canceled: WorkState = .canceled
  expect terminal != canceled
}

test "durable waiting is not active execution" {
  let waiting: WorkState = .waiting
  let running: WorkState = .running
  expect waiting != running
}
