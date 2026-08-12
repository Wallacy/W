// Operational time at the Restaurant at the End of the Universe.
//
// The clock is an explicit root-scoped capability. The provider remains
// missing; this source fixes call shape, ownership, and local-origin rules.
// HostSuspendPolicy describes HOST/SO suspension, not coroutine, task, or
// await suspension. The safe wrappers keep the provider boundary internal.

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
// In a native-process entry body, `process.clock()` equals `process.context.clock()`
// by identity, origin, authority, and lifetime. `process.deadline` equals
// `process.context.deadline` by value identity, origin, and lifetime. Deadline
// is not authority; its short projection keeps `authorityExpanded: false`.
// A provider profile with 60 ms active time, 50 ms HOST/SO suspension, and a
// 100 ms deadline reaches the deadline with `.included`, does not with
// `.excluded`, and remains unknown with `.unspecified`.
// let ambient = time.Clock()
// let global = time.Clock.current()
// let clock = process.clock() // nonthrowing when Context grants the capability
// let selected = try process.clock(hostSuspend: .included)
// let longForm = try process.context.clock(hostSuspend: .excluded)
// The active slot is HostSuspendPolicy<[.included, .excluded]>; `.unspecified`
// is a compile-time diagnostic. A valid but unsupported case fails before work.
// An unqualified `clock()` without Context is rejected by W-TIME-0002.
// serialize(clock.now())
// service.send(deadline)
// let wall = clock.wallNow()
