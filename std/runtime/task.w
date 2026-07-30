// Public value contracts for lexical tasks and structured cancellation.

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

export alias TaskTimeout = Duration<(0...)>

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
