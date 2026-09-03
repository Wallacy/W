import time from std

// Current form. The passive property reports the provider fact.
export enum HostSuspendPolicy: Copy & Equatable {
  included
  excluded
  unspecified
}

export fn observeSuspend(
  using clock: ref time.Clock,
  _ started: time.Instant,
  _ deadline: time.Deadline,
): HostSuspendPolicy {
  let policy = clock.hostSuspendPolicy
  let finished = clock.now()
  let _elapsed = clock.duration(from: started, to: finished)
  let _reached = clock.hasReached(deadline)
  return policy
}
