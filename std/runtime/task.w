// Public value contracts for lexical tasks and structured cancellation.

import { Duration } from std.time

export enum CancellationReason {
  userRequest
  shutdown
  superseded
}

export enum TaskBudgetKind {
  liveTasks
  taskFrameBytes
  timers
  readyJobs
}

export enum TaskOutcome<Value, Failure: Error> {
  success(Value)
  error(Failure)
  canceled(Cancellation)
}

export enum TaskOrdering {
  input
  completion
}

// Task#firstSettled and pipeline task collect operations use this record. The
// index refers to the consumed input array. The outcome keeps application
// failure separate from structured cancellation.
export struct TaskSettlement<Value, Failure: Error> {
  export index: usize
  export outcome: TaskOutcome<Value, Failure>
}

export alias TaskTimeout = Duration<(0...)>

// TaskLocal is an immutable binding descriptor. The runtime stores bindings in
// task frames. Structured children inherit the current binding by dependency,
// not by copying the value.
foreign intrinsic from "std.runtime.task-local@1" {
  type TaskLocalIdentity

  const fn stdTaskLocalKey<Value>(
    default: Value<(.shareable)>,
  ): TaskLocalIdentity

  fn stdTaskLocalGet<Value>(
    identity: ref TaskLocalIdentity,
  ): ref Value

  fn stdTaskLocalWithValue<Value, Result, Failure: Error>(
    named identity: ref TaskLocalIdentity,
    named value: take Value<(.shareable)>,
    named operation: some fn(): Result throws Failure,
  ): Result throws Failure
}

export struct TaskLocal<Value> {
  identity: TaskLocalIdentity

  init(validatedIdentity: TaskLocalIdentity) {
    self.identity = validatedIdentity
  }

  export static const fn key(
    default: Value<(.shareable)>,
  ): TaskLocal<Value> {
    let identity = unsafe { stdTaskLocalKey(default) }
    return TaskLocal<Value>(validatedIdentity: identity)
  }

  export fn get(): ref Value {
    return unsafe { stdTaskLocalGet(identity) }
  }

  // W-1240 forwards suspension from operation. This wrapper does not create a
  // task. Error, cancellation, and panic cleanup drain children before pop.
  export fn withValue<Result, Failure: Error>(
    _ value: Value<(.shareable)>,
    named operation: some fn(): Result throws Failure,
  ): Result throws Failure {
    return unsafe {
      try stdTaskLocalWithValue(
        identity: identity,
        value: take value,
        operation: operation,
      )
    }
  }
}

struct TaskLocalTestTrace {
  const request = TaskLocal<u64?>.key(default: .none)
}

test "caller reasons do not contain runtime pressure" {
  let shutdown: CancellationReason = .shutdown
  let budget: TaskBudgetKind = .liveTasks

  expect shutdown == .shutdown
  expect budget == .liveTasks
}

test "task outcome keeps cancellation outside the application error" {
  let outcome: TaskOutcome<u8, Never> = .success(42)

  expect switch outcome {
    case .success(let value): value == 42
    case .error(_): false
    case .canceled(_): false
  }
}

test "task settlement preserves candidate position and outcome" {
  let settlement: TaskSettlement<u8, Never> = TaskSettlement(
    index: 1,
    outcome: .success(42),
  )

  expect settlement.index == 1
  expect switch settlement.outcome {
    case .success(let value): value == 42
    case .error(_): false
    case .canceled(_): false
  }
}

test "task ordering keeps input and completion policies distinct" {
  let stable: TaskOrdering = .input
  let scheduled: TaskOrdering = .completion

  expect stable == .input
  expect scheduled == .completion
}

test "task-local descriptors keep nominal identity" {
  expect TaskLocalTestTrace.request.get() == .none
}
