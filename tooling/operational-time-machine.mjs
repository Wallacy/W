const i128Minimum = -(1n << 127n)
const i128Maximum = (1n << 127n) - 1n
const hostSuspendPolicies = new Set(["included", "excluded", "unspecified"])
const activeHostSuspendPolicies = new Set(["included", "excluded"])

function integer(value) {
  if (typeof value !== "string" || !/^-?(?:0|[1-9][0-9]*)$/.test(value)) return null
  try {
    return BigInt(value)
  } catch {
    return null
  }
}

function inI128(value) {
  return value >= i128Minimum && value <= i128Maximum
}

function duration(input) {
  if (input.operation === "construct") {
    const value = integer(input.nanoseconds)
    if (value === null) return { accepted: false, reason: "invalidInteger" }
    if (!inI128(value)) return { accepted: false, reason: "durationOutOfRange" }
    return {
      accepted: true,
      nanoseconds: value.toString(),
      exact: true,
      portable: true,
      serializable: true,
      physicalLayoutPublic: false,
    }
  }
  if (input.operation === "add") {
    const left = integer(input.left)
    const right = integer(input.right)
    if (left === null || right === null || !inI128(left) || !inI128(right)) {
      return { accepted: false, reason: "invalidDuration" }
    }
    const result = left + right
    if (!inI128(result)) return { accepted: false, reason: "checkedOverflow" }
    return { accepted: true, nanoseconds: result.toString(), checked: true }
  }
  if (input.operation === "unitLiteral") {
    const numerator = integer(input.nanosecondNumerator)
    const denominator = integer(input.denominator)
    if (numerator === null || denominator === null || denominator <= 0n) {
      return { accepted: false, reason: "invalidUnitRatio" }
    }
    if (numerator % denominator !== 0n) {
      return { accepted: false, reason: "inexactOperationalDuration" }
    }
    const result = numerator / denominator
    if (!inI128(result)) return { accepted: false, reason: "durationOutOfRange" }
    return { accepted: true, nanoseconds: result.toString(), exact: true }
  }
  return { accepted: false, reason: "unknownDurationOperation" }
}

function clock(input) {
  if (input.operation === "defaultAcquire") {
    if (input.root !== true) return { accepted: false, reason: "clockOutsideRoot" }
    if (!(input.capabilities ?? []).includes("clock")) {
      return { accepted: false, reason: "clockCapabilityMissing", providerCalled: false }
    }
    return {
      accepted: true,
      selection: "default",
      hostSuspendPolicy: input.providerPolicy ?? "unspecified",
      mayReportUnspecified: true,
      retainedOwner: true,
      rootBound: true,
      global: false,
    }
  }
  if (input.operation === "selectAcquire") {
    if (input.root !== true) return { accepted: false, reason: "clockOutsideRoot" }
    if (!(input.capabilities ?? []).includes("clock")) {
      return { accepted: false, reason: "clockCapabilityMissing", providerCalled: false }
    }
    if (!activeHostSuspendPolicies.has(input.hostSuspendPolicy)) {
      return { accepted: false, reason: "activeHostSuspendPolicySubsetRequired", requested: input.hostSuspendPolicy, providerCalled: false }
    }
    if (!(input.providerPolicies ?? activeHostSuspendPolicies).includes(input.hostSuspendPolicy)) {
      return { accepted: false, reason: "hostSuspendPolicyUnsupported", requested: input.hostSuspendPolicy, providerCalled: true }
    }
    return {
      accepted: true,
      selection: "active",
      hostSuspendPolicy: input.hostSuspendPolicy,
      retainedOwner: true,
      rootBound: true,
      global: false,
    }
  }
  if (input.operation === "project") {
    if (input.root !== true) return { accepted: false, reason: "clockOutsideRoot" }
    if (!(input.capabilities ?? []).includes("clock")) {
      return { accepted: false, reason: "clockCapabilityMissing", providerCalled: false }
    }
    return {
      accepted: true,
      requirement: "clock",
      retainedOwner: true,
      rootBound: true,
      global: false,
      wallClock: false,
    }
  }
  if (input.operation === "runtimeInternal") {
    return {
      accepted: true,
      schedulerMayRead: true,
      applicationAuthorityGranted: false,
    }
  }
  if (input.operation === "global") return { accepted: false, reason: "ambientClockForbidden" }
  if (input.operation === "wallNow") return { accepted: false, reason: "civilTimeCapabilityRequired" }
  if (input.operation === "samples") {
    if (input.factSource !== "provider") {
      return { accepted: false, reason: "providerClockFactsRequired" }
    }
    const resolution = integer(input.resolutionNanoseconds)
    if (resolution === null || resolution <= 0n || !inI128(resolution)) {
      return { accepted: false, reason: "invalidResolution" }
    }
    if (!hostSuspendPolicies.has(input.hostSuspendPolicy)) {
      return { accepted: false, reason: "invalidHostSuspendPolicy" }
    }
    const samples = (input.samples ?? []).map(integer)
    if (samples.length === 0 || samples.some((value) => value === null)) {
      return { accepted: false, reason: "invalidSamples" }
    }
    if (samples.some((value, index) => index > 0 && value < samples[index - 1])) {
      return { accepted: false, reason: "clockRegressed" }
    }
    return {
      accepted: true,
      sampleCount: samples.length,
      nondecreasing: true,
      resolutionNanoseconds: resolution.toString(),
      hostSuspendPolicy: input.hostSuspendPolicy,
      steadyFrequencyPromised: false,
      hardRealtimePromised: false,
      rawTicksPublic: false,
    }
  }
  if (input.operation === "deadlineHostSuspend") {
    if (input.factSource !== "provider") {
      return { accepted: false, reason: "providerClockFactsRequired" }
    }
    const active = integer(input.activeNanoseconds)
    const hostSuspend = integer(input.hostSuspendNanoseconds)
    const deadline = integer(input.deadlineNanoseconds)
    if (active === null || hostSuspend === null || deadline === null
      || active < 0n || hostSuspend < 0n || deadline < 0n) {
      return { accepted: false, reason: "invalidDeadlineSuspendFacts" }
    }
    if (!hostSuspendPolicies.has(input.hostSuspendPolicy)) {
      return { accepted: false, reason: "invalidHostSuspendPolicy" }
    }
    if (input.requiredHostSuspendPolicy !== undefined
      && input.requiredHostSuspendPolicy !== input.hostSuspendPolicy) {
      return {
        accepted: false,
        reason: "hostSuspendPolicyRequired",
        required: input.requiredHostSuspendPolicy,
        actual: input.hostSuspendPolicy,
      }
    }
    if (input.hostSuspendPolicy === "unspecified") {
      return {
        accepted: true,
        hostSuspendPolicy: "unspecified",
        deadlineReached: null,
        elapsedNanoseconds: null,
        inferenceAllowed: false,
        hostSuspended: hostSuspend > 0n,
      }
    }
    const elapsed = input.hostSuspendPolicy === "included" ? active + hostSuspend : active
    return {
      accepted: true,
      elapsedNanoseconds: elapsed.toString(),
      deadlineReached: elapsed >= deadline,
      hostSuspendPolicy: input.hostSuspendPolicy,
      hostSuspended: hostSuspend > 0n,
    }
  }
  return { accepted: false, reason: "unknownClockOperation" }
}

function sameRoot(input) {
  return typeof input.clockRoot === "string"
    && input.clockRoot !== ""
    && input.valueRoot === input.clockRoot
}

function relation(input) {
  if (!sameRoot(input)) {
    return { accepted: false, reason: "clockOriginMismatch", providerCalled: false }
  }
  if (input.operation === "duration") {
    const earlier = integer(input.earlierNanoseconds)
    const later = integer(input.laterNanoseconds)
    if (earlier === null || later === null) return { accepted: false, reason: "invalidInstant" }
    const result = later - earlier
    if (!inI128(result)) return { accepted: false, reason: "durationOutOfRange" }
    return {
      accepted: true,
      nanoseconds: result.toString(),
      signed: true,
      rawInstantPublic: false,
    }
  }
  if (input.operation === "deadline") {
    const now = integer(input.nowNanoseconds)
    const delay = integer(input.delayNanoseconds)
    const maximum = integer(input.maximumNanoseconds)
    if (now === null || delay === null || maximum === null) {
      return { accepted: false, reason: "invalidDeadlineInput" }
    }
    if (delay < 0n) return { accepted: false, reason: "negativeDeadlineDuration" }
    const target = now + delay
    if (target > maximum || !inI128(target)) return { accepted: false, reason: "deadlineOutOfRange" }
    return {
      accepted: true,
      deadlineCreated: true,
      immediatelyReached: delay === 0n,
      rawDeadlinePublic: false,
    }
  }
  if (input.operation === "remaining") {
    const now = integer(input.nowNanoseconds)
    const deadline = integer(input.deadlineNanoseconds)
    if (now === null || deadline === null) return { accepted: false, reason: "invalidDeadlineInput" }
    const remaining = deadline > now ? deadline - now : 0n
    return { accepted: true, nanoseconds: remaining.toString(), clampedAtZero: true }
  }
  return { accepted: false, reason: "unknownRelationOperation" }
}

function boundary(input) {
  if (input.value === "duration") {
    return {
      accepted: true,
      boundary: input.boundary,
      portable: true,
      authorityTransferred: false,
    }
  }
  if (["clock", "instant", "deadline"].includes(input.value)) {
    return {
      accepted: false,
      reason: "rootLocalTimeValue",
      boundary: input.boundary,
      authorityTransferred: false,
    }
  }
  return { accepted: false, reason: "unknownTimeValue" }
}

function timer(input) {
  const deadline = integer(input.deadlineNanoseconds)
  const fired = integer(input.firedNanoseconds)
  if (deadline === null || fired === null) return { accepted: false, reason: "invalidTimerFacts" }
  if (fired < deadline) return { accepted: false, reason: "earlyExpiration" }
  if (input.cancellationRequested !== true) {
    return { accepted: false, reason: "expirationWithoutCancellation" }
  }
  if (input.cleanupDrained !== true) return { accepted: false, reason: "cleanupNotDrained" }
  return {
    accepted: true,
    expired: true,
    resumedLateNanoseconds: (fired - deadline).toString(),
    cancellationRequested: true,
    cleanupDrained: true,
    threadKilled: false,
    rollbackClaimed: false,
  }
}

function virtualClock(input) {
  if (input.provider !== "virtual" || input.factSource !== "testFixture") {
    return { accepted: false, reason: "invalidVirtualClockProvider" }
  }
  const advances = (input.advances ?? []).map(integer)
  if (advances.length === 0 || advances.some((value) => value === null || value < 0n)) {
    return { accepted: false, reason: "invalidVirtualAdvance" }
  }
  let now = 0n
  const samples = ["0"]
  for (const advance of advances) {
    now += advance
    samples.push(now.toString())
  }
  return { accepted: true, deterministic: true, samples, ambientReads: 0 }
}

export function deriveOperationalTime(input) {
  if (!input || typeof input !== "object") return { accepted: false, reason: "invalidInput" }
  if (input.subject === "duration") return duration(input)
  if (input.subject === "clock") return clock(input)
  if (input.subject === "relation") return relation(input)
  if (input.subject === "boundary") return boundary(input)
  if (input.subject === "timer") return timer(input)
  if (input.subject === "virtual") return virtualClock(input)
  return { accepted: false, reason: "unknownSubject" }
}
