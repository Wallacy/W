import time from std

// Current baseline. The enum and provider fact remain HOST/SO-only.
export fn observeSuspend(
  using clock: ref time.Clock,
  _ started: time.Instant,
  _ deadline: time.Deadline,
): time.SuspendAccounting {
  let policy = clock.suspendAccounting()
  let finished = clock.now()
  let _elapsed = clock.duration(from: started, to: finished)
  let _reached = clock.hasReached(deadline)
  return policy
}
