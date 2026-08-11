// Operational time at the Restaurant at the End of the Universe.
//
// The clock is an explicit root-scoped capability. The provider remains
// missing; this source fixes call shape, ownership, and local-origin rules.

import time from std

export struct ObservationWindow {
  export elapsed: time.Duration
  export deadlineReached: Bool
}

export fn openObservationWindow(
  using clock: ref time.Clock,
  after delay: time.Duration<(0...)>,
): time.Deadline throws time.ClockError {
  return try clock.deadline(after: delay)
}

export fn inspectObservationWindow(
  using clock: ref time.Clock,
  from started: time.Instant,
  until deadline: time.Deadline,
): ObservationWindow {
  let finished = clock.now()
  return ObservationWindow(
    elapsed: clock.duration(from: started, to: finished),
    deadlineReached: clock.hasReached(deadline),
  )
}

export fn remainingObservationTime(
  using clock: ref time.Clock,
  until deadline: time.Deadline,
): time.Duration<(0...)> {
  return clock.remaining(until: deadline)
}

test "duration remains portable signed data" {
  let early = time.Duration(nanoseconds: -1)
  let exact = time.Duration(nanoseconds: 250_000_000)
  expect early.nanoseconds == -1
  expect exact.nanoseconds == 250_000_000
}

// Compile-fail assays:
// let ambient = time.Clock()
// serialize(clock.now())
// service.send(deadline)
// let wall = clock.wallNow()
