import time from std

// Research alternative. This name is not the current std.time contract.
export enum HostSuspendPolicy: Copy & Equatable {
  counted
  paused
  unknown
}

export fn observeSuspend(
  using clock: ref time.Clock,
  started: time.Instant,
  deadline: time.Deadline,
): HostSuspendPolicy {
  let policy = clock.hostSuspendPolicy()
  let finished = clock.now()
  let _elapsed = clock.duration(from: started, to: finished)
  let _reached = clock.hasReached(deadline)
  return policy
}
