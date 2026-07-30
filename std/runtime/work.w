// Public value contracts for runtime-owned supervised work.

export enum WorkRight {
  observe
  cancel
}

export enum WorkState {
  queued
  running
  succeeded
  failed
  canceled
  boundaryFailed
}

export struct WorkSnapshot<Progress> {
  id: WorkId
  revision: u64
  attempt: u32
  state: WorkState
  progress: Progress?
  cancellation: Cancellation?
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
