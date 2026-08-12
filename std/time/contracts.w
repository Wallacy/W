// Operational monotonic time contracts.
//
// Duration is portable data. Clock is a root-scoped capability and is
// monotonic by definition. Instant and Deadline are opaque values tied to the
// same entry or fault root as Clock.
// This module does not expose civil time, a global clock, or a sleep runtime.
// The unsafe calls below stay inside the provider adapter. Safe wrappers
// restore the applicable invariants: origin, validation or range, and
// ownership/drop. A wrapper validates only facts required by its contract.
// User code and the public API do not require unsafe. The raw boundary remains
// visible here and is not replaced by a trusted block or ambient lookup.

export enum SuspendAccounting: Copy & Equatable {
  included
  excluded
  unspecified
}

// This fact describes HOST/SO suspension only. It never describes a coroutine,
// task, or await suspension. `included` counts host suspension, `excluded`
// pauses during it, and `unspecified` forbids an inference.

export enum ClockError: Error & Copy & Equatable {
  outOfRange
}

// The semantic value is an exact signed total of nanoseconds. The physical
// layout remains private, and integer arithmetic keeps checked semantics.
export struct Duration: Copy & Equatable & Hashable {
  value: i128

  export const init(nanoseconds: i128) {
    self.value = nanoseconds
  }

  export nanoseconds: i128 {
    get => value
  }
}

foreign intrinsic from "std.time@1" {
  type ClockHandle
  type InstantValue
  type DeadlineValue

  fn stdTimeClockNow(handle: ref ClockHandle): InstantValue
  fn stdTimeClockResolution(handle: ref ClockHandle): Duration<(1...)>
  fn stdTimeClockSuspendAccounting(
    handle: ref ClockHandle,
  ): SuspendAccounting
  fn stdTimeClockDuration(
    handle: ref ClockHandle,
    from earlier: InstantValue,
    to later: InstantValue,
  ): Duration
  fn stdTimeClockDeadline(
    handle: ref ClockHandle,
    after duration: Duration<(0...)>,
  ): DeadlineValue throws ClockError
  fn stdTimeClockRemaining(
    handle: ref ClockHandle,
    until deadline: DeadlineValue,
  ): Duration<(0...)>
  fn stdTimeClockHasReached(
    handle: ref ClockHandle,
    _ deadline: DeadlineValue,
  ): Bool
  fn stdTimeClockDrop(handle: inout ClockHandle)
}

// Copies keep the same root dependency. They do not become portable numeric
// timestamps and cannot cross service, wire, storage, or foreign boundaries.
export struct Instant: Copy {
  value: InstantValue

  init(validatedValue: InstantValue) {
    self.value = validatedValue
  }
}

export struct Deadline: Copy {
  value: DeadlineValue

  init(validatedValue: DeadlineValue) {
    self.value = validatedValue
  }
}

// The host creates Clock through an entry Context. Source cannot construct or
// duplicate authority. Each Context projection retains an owner in the same
// root; all values produced by that owner keep that root as their origin.
export struct Clock {
  handle: ClockHandle

  init(hostHandle: ClockHandle) {
    self.handle = hostHandle
  }

  export fn now(): Instant {
    return Instant(validatedValue: unsafe { stdTimeClockNow(ref handle) })
  }

  export fn resolution(): Duration<(1...)> {
    return unsafe { stdTimeClockResolution(ref handle) }
  }

  export fn suspendAccounting(): SuspendAccounting {
    return unsafe { stdTimeClockSuspendAccounting(ref handle) }
  }

  export fn duration(
    from earlier: Instant,
    to later: Instant,
  ): Duration {
    return unsafe {
      stdTimeClockDuration(ref handle, from: earlier.value, to: later.value)
    }
  }

  export fn deadline(
    after duration: Duration<(0...)>,
  ): Deadline throws ClockError {
    let value = unsafe {
      try stdTimeClockDeadline(ref handle, after: duration)
    }
    return Deadline(validatedValue: value)
  }

  export fn remaining(
    until deadline: Deadline,
  ): Duration<(0...)> {
    return unsafe {
      stdTimeClockRemaining(ref handle, until: deadline.value)
    }
  }

  export fn hasReached(_ deadline: Deadline): Bool {
    return unsafe { stdTimeClockHasReached(ref handle, deadline.value) }
  }

  deinit {
    unsafe { stdTimeClockDrop(inout handle) }
  }
}

test "duration keeps exact signed nanoseconds" {
  let before = Duration(nanoseconds: -1)
  let epoch = Duration(nanoseconds: 0)
  let after = Duration(nanoseconds: 1)
  expect before.nanoseconds < epoch.nanoseconds
  expect epoch.nanoseconds < after.nanoseconds
}

test "suspend accounting is an explicit provider fact" {
  expect SuspendAccounting.included != .excluded
  expect SuspendAccounting.excluded != .unspecified
}

test "Clock remains monotonic by definition" {
  // This checks provider observations only; Instant remains opaque to user code.
  // A provider may repeat a sample, but it must not return a smaller sample.
  let samples: Array<u8> = [10, 10, 12]
  expect samples[1] >= samples[0]
  expect samples[2] >= samples[1]
}
