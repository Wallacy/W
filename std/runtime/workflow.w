// Public value contracts for step-based supervised workflows.

export enum StepEffect {
  repeatable
  idempotent
  transactional
  atMostOnce
}

export enum StepBackoff {
  none
  fixed(Duration<(0...)>)
  linear(
    initial: Duration<(0...)>,
    increment: Duration<(0...)>,
    maximum: Duration<(0...)>,
  )
  exponential(
    initial: Duration<(0...)>,
    factor: u16<(2...)>,
    maximum: Duration<(0...)>,
  )
}

export struct StepRetry<Failure: Error> {
  export maximumAttempts: u16<(1...)>
  export backoff: StepBackoff
  export attemptTimeout: Duration<(0...)>?
  export retryWhen: fn(ref Failure): Bool
}

export enum WorkSuspension {
  retry(point: WorkflowPointId, attempt: u32, remaining: Duration<(0...)>)
  sleep(point: WorkflowPointId, remaining: Duration<(0...)>)
  event(
    point: WorkflowPointId,
    binding: WorkEventTypeId,
    remaining: Duration<(0...)>?,
  )
}

export struct WorkEventBinding<Payload> {
  export name: String
  export version: u32<(1...)>
}

export enum WaitOutcome<Payload> {
  event(Payload)
  timeout
}

export enum WorkEventSendResult {
  accepted(revision: u64)
  duplicate(revision: u64)
}

export enum WorkEventSendError<Payload>: Error {
  full(Payload)
  terminal(Payload)
  unavailable(Payload)
  unauthorized(Payload)
  incompatibleSchema(Payload)
  unknownOutcome(EventId)
}

test "at-most-once effects are distinct from safe repetition" {
  let atMostOnce: StepEffect = .atMostOnce
  let repeatable: StepEffect = .repeatable
  expect atMostOnce != repeatable
}

test "event timeout is data rather than an application error" {
  let timeout: WaitOutcome<u8> = .timeout
  let event: WaitOutcome<u8> = .event(42)
  expect timeout != event
}
