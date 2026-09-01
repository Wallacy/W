const portableSignals = new Set(["interrupt", "terminate"])
const contextRequirements = new Map([
  ["stdin", "stdio"],
  ["stdout", "stdio"],
  ["stderr", "stdio"],
  ["filesystem", "filesystem"],
  ["network", "network"],
  ["clock", "clock"],
  ["signals", "signals"],
  ["services", "services"],
  ["deadline", null],
])

function isNatural(value) {
  return Number.isSafeInteger(value) && value >= 0
}

function sameNative(left, right) {
  if (!left || !right || left.kind !== right.kind) return false
  const field = left.kind === "unixBytes" ? "bytes" : "units"
  return Array.isArray(left[field])
    && Array.isArray(right[field])
    && left[field].length === right[field].length
    && left[field].every((value, index) => value === right[field][index])
}

function validNative(value) {
  if (!value || !["unixBytes", "windowsUnits"].includes(value.kind)) return false
  const field = value.kind === "unixBytes" ? "bytes" : "units"
  const maximum = value.kind === "unixBytes" ? 0xff : 0xffff
  return Array.isArray(value[field])
    && value[field].every((item) => isNatural(item) && item <= maximum)
}

function encodeNativeText(kind, value) {
  if (kind === "unixBytes") {
    return { kind, bytes: [...new TextEncoder().encode(value)] }
  }
  if (kind === "windowsUnits") {
    const units = []
    for (let index = 0; index < value.length; index += 1) units.push(value.charCodeAt(index))
    return { kind, units }
  }
  return null
}

function deriveArguments(input) {
  if (input.operation === "bindHandler") {
    if (input.root !== true || input.profile !== "native-process" || input.signatureRequests !== true) {
      return { accepted: false, reason: "explicitBindingUnavailable" }
    }
    return {
      accepted: true,
      ownerId: input.ownerId,
      repeatedOwnerId: input.ownerId,
      copies: 0,
      rootBound: true,
      readOnly: true,
      explicitParameter: true,
    }
  }

  const values = input.values ?? []
  if (!Array.isArray(values) || !values.every(validNative)) {
    return { accepted: false, reason: "invalidNativeArgument" }
  }
  if (input.operation === "get") {
    if (!isNatural(input.index)) return { accepted: false, reason: "invalidIndex" }
    const value = values[input.index]
    return {
      accepted: true,
      found: value !== undefined,
      value: value ?? null,
      borrowed: value !== undefined,
      copied: false,
    }
  }
  if (input.operation === "containsText") {
    if (typeof input.text !== "string") return { accepted: false, reason: "invalidText" }
    return {
      accepted: true,
      contains: values.some((value) => sameNative(value, encodeNativeText(value.kind, input.text))),
      lossyDecode: false,
      encodedToNative: true,
      normalized: false,
      localeLookup: false,
    }
  }
  if (input.operation === "containsNative") {
    if (!validNative(input.value)) return { accepted: false, reason: "invalidNativeArgument" }
    return {
      accepted: true,
      contains: values.some((value) => sameNative(value, input.value)),
      lossyDecode: false,
    }
  }
  if (input.operation === "escape") {
    return {
      accepted: false,
      reason: "rootLifetime",
      serialized: false,
      serviceCrossed: false,
    }
  }
  return { accepted: false, reason: "unknownArgumentsOperation" }
}

function deriveContext(input) {
  if (input.root !== true || input.profile !== "native-process") {
    return { accepted: false, reason: "contextUnavailable" }
  }
  if (input.operation === "crossBoundary") {
    return {
      accepted: false,
      reason: input.boundary ?? "rootLifetime",
      copied: false,
      authorityExpanded: false,
    }
  }
  if (input.operation === "clockAcquire") {
    if (!(input.capabilities ?? []).includes("clock")) {
      return { accepted: false, reason: "capabilityMissing", requirement: "clock", providerCalled: false }
    }
    if (input.selection === "active") {
      if (!["included", "excluded"].includes(input.hostSuspendPolicy)) {
        return { accepted: false, reason: "activeHostSuspendPolicySubsetRequired", requested: input.hostSuspendPolicy, providerCalled: false }
      }
      if (!(input.providerPolicies ?? ["included", "excluded"]).includes(input.hostSuspendPolicy)) {
        return { accepted: false, reason: "hostSuspendPolicyUnsupported", requested: input.hostSuspendPolicy, providerCalled: true }
      }
    }
    return {
      accepted: true,
      selection: input.selection ?? "default",
      hostSuspendPolicy: input.hostSuspendPolicy ?? input.providerPolicy ?? "unspecified",
      retainedOwner: true,
      rootBound: true,
      providerCalled: true,
    }
  }
  if (input.operation === "contextualProjection") {
    if (!(input.member === "clock" || input.member === "deadline")) {
      return { accepted: false, reason: "unknownMember" }
    }
    const requirement = input.member === "clock" ? "clock" : null
    if (requirement !== null && !(input.capabilities ?? []).includes(requirement)) {
      return {
        accepted: false,
        reason: "capabilityMissing",
        requirement,
        providerCalled: false,
      }
    }
    return {
      accepted: true,
      member: input.member,
      contextualProjection: input.member === "clock" ? "execution.clock()" : "execution.deadline",
      explicitProjection: input.member === "clock" ? "ctx.clock()" : "ctx.deadline",
      sameIdentity: true,
      sameOrigin: true,
      ...(input.member === "clock" ? { sameAuthority: true } : {}),
      rootBound: true,
      serializable: false,
      authorityExpanded: false,
    }
  }
  if (input.operation !== "project") return { accepted: false, reason: "unknownContextOperation" }
  if (!contextRequirements.has(input.member)) return { accepted: false, reason: "unknownMember" }
  const requirement = contextRequirements.get(input.member)
  if (requirement !== null && !(input.capabilities ?? []).includes(requirement)) {
    return {
      accepted: false,
      reason: "capabilityMissing",
      requirement,
      providerCalled: false,
    }
  }
  return {
    accepted: true,
    member: input.member,
    requirement,
    retainedOwner: input.member !== "deadline",
    rootBound: true,
    serializable: false,
    authorityExpanded: false,
  }
}

function splitLines(bytes, maximumBytes) {
  const ranges = []
  let start = 0
  for (let index = 0; index < bytes.length; index += 1) {
    if (bytes[index] !== 0x0a) continue
    const end = index > start && bytes[index - 1] === 0x0d ? index - 1 : index
    ranges.push([start, end])
    start = index + 1
  }
  if (start < bytes.length) ranges.push([start, bytes.length])

  const decoder = new TextDecoder("utf-8", { fatal: true })
  const lines = []
  for (const [first, last] of ranges) {
    const line = bytes.slice(first, last)
    if (line.length > maximumBytes) {
      return { accepted: false, reason: "lineTooLong", maximumBytes, observedBytes: line.length }
    }
    try {
      lines.push(decoder.decode(Uint8Array.from(line)))
    } catch {
      return { accepted: false, reason: "invalidUtf8", publishedLines: lines.length }
    }
  }
  return { accepted: true, lines, delimiterRemoved: true, strictUtf8: true }
}

function deriveInput(input) {
  if (input.operation === "lines") {
    if (!isNatural(input.maximumBytes) || input.maximumBytes < 1) {
      return { accepted: false, reason: "invalidLimit" }
    }
    if (!Array.isArray(input.bytes)
      || !input.bytes.every((value) => isNatural(value) && value <= 0xff)) {
      return { accepted: false, reason: "invalidBytes" }
    }
    if ((input.activeReaders ?? 1) !== 1) {
      return { accepted: false, reason: "cursorBusy", activeReaders: input.activeReaders }
    }
    return {
      ...splitLines(input.bytes, input.maximumBytes),
      oneCursor: true,
      maximumConcurrentReads: 1,
    }
  }
  if (input.operation === "cancel") {
    const events = input.events ?? []
    const completion = events.indexOf("providerCompletion")
    const release = events.indexOf("releaseBuffer")
    if (completion < 0 || release < 0 || completion > release) {
      return { accepted: false, reason: "releaseBeforeDrain" }
    }
    return {
      accepted: true,
      cancellationRequested: events.includes("cancel"),
      providerDrained: true,
      bufferReleasedAfterDrain: true,
    }
  }
  return { accepted: false, reason: "unknownInputOperation" }
}

function payloadBytes(call) {
  if (typeof call.text === "string") return [...new TextEncoder().encode(call.text)]
  if (Array.isArray(call.bytes)
    && call.bytes.every((value) => isNatural(value) && value <= 0xff)) return [...call.bytes]
  return null
}

function deriveOutput(input) {
  if (input.operation !== "write") return { accepted: false, reason: "unknownOutputOperation" }
  const calls = input.calls ?? []
  const tickets = new Set()
  const normalized = []
  for (const call of calls) {
    if (!isNatural(call.ticket) || tickets.has(call.ticket)) {
      return { accepted: false, reason: "invalidTicket", publishedBytes: 0 }
    }
    const bytes = payloadBytes(call)
    if (bytes === null) return { accepted: false, reason: "invalidPayload", publishedBytes: 0 }
    tickets.add(call.ticket)
    normalized.push({ ...call, bytes })
  }
  normalized.sort((left, right) => left.ticket - right.ticket)
  const output = []
  const committedByTicket = []
  for (const call of normalized) {
    const failureAfterBytes = call.failureAfterBytes
    if (failureAfterBytes !== undefined) {
      if (!isNatural(failureAfterBytes) || failureAfterBytes > call.bytes.length) {
        return { accepted: false, reason: "invalidProgress", publishedBytes: output.length }
      }
      output.push(...call.bytes.slice(0, failureAfterBytes))
      committedByTicket.push({ ticket: call.ticket, committed: failureAfterBytes })
      return {
        accepted: false,
        reason: "io",
        output,
        publishedBytes: output.length,
        committedByTicket,
        noInterleaving: true,
      }
    }
    output.push(...call.bytes)
    committedByTicket.push({ ticket: call.ticket, committed: call.bytes.length })
  }
  return {
    accepted: true,
    ticketOrder: normalized.map((call) => call.ticket),
    output,
    committedByTicket,
    noInterleaving: true,
    newlineAdded: false,
  }
}

function validateSignalSet(signals, maximumSignals) {
  if (!Array.isArray(signals) || signals.length === 0) return "emptySet"
  if (!isNatural(maximumSignals) || maximumSignals < 1 || signals.length > maximumSignals) {
    return "limitExceeded"
  }
  const seen = new Set()
  for (const signal of signals) {
    if (!portableSignals.has(signal)) return "unsupported"
    if (seen.has(signal)) return "duplicate"
    seen.add(signal)
  }
  return null
}

function deriveSignals(input) {
  if (input.rawHandler === true || input.detachedHandler === true) {
    return { accepted: false, reason: input.rawHandler ? "rawHandler" : "detachedHandler" }
  }
  const reason = validateSignalSet(input.signals, input.maximumSignals ?? 2)
  if (reason) return { accepted: false, reason, registrationPublished: false }

  let generation = 1
  let admission = "open"
  const callbacks = []
  const acceptedGenerations = []
  const trace = [{ event: "registered", generation }]
  for (const event of input.events ?? []) {
    if (event.op === "emit") {
      if (!portableSignals.has(event.signal) || !input.signals.includes(event.signal)) {
        return { accepted: false, reason: "unregisteredSignal", trace }
      }
      if (admission === "open") {
        callbacks.push({ generation, signal: event.signal })
        acceptedGenerations.push(generation)
        trace.push({ event: "accepted", generation, signal: event.signal })
      } else trace.push({ event: "suppressed", signal: event.signal })
    } else if (event.op === "replace") {
      if (admission !== "open") return { accepted: false, reason: "closed", trace }
      generation += 1
      trace.push({ event: "replaced", generation })
    } else if (event.op === "complete") {
      const callback = callbacks.shift()
      if (!callback) return { accepted: false, reason: "noAcceptedCallback", trace }
      trace.push({ event: "completed", generation: callback.generation })
    } else if (event.op === "cancel" || event.op === "drop") {
      admission = "closed"
      trace.push({ event: "admissionClosed", by: event.op })
    } else if (event.op === "drain") {
      if (event.completeAll === true) {
        while (callbacks.length > 0) {
          const callback = callbacks.shift()
          trace.push({ event: "completed", generation: callback.generation })
        }
      }
      trace.push({ event: callbacks.length === 0 ? "drained" : "drainPending" })
    } else return { accepted: false, reason: "unknownSignalEvent", trace }
  }

  return {
    accepted: true,
    generation,
    admission,
    acceptedGenerations,
    pendingCallbacks: callbacks.length,
    maximumConcurrentHandlers: 1,
    structured: true,
    rawHandler: false,
    trace,
  }
}

function deriveServices(input) {
  if (input.operation !== "drain") return { accepted: false, reason: "unknownServicesOperation" }
  if (!isNatural(input.roots) || !isNatural(input.completedRoots) || input.completedRoots > input.roots) {
    return { accepted: false, reason: "invalidRootCount" }
  }
  if (!isNatural(input.deadlineTicks)) return { accepted: false, reason: "invalidDeadline" }
  const stopped = input.completedRoots === input.roots
  return {
    accepted: true,
    admission: "closed",
    state: stopped ? "stopped" : "draining",
    pendingRoots: input.roots - input.completedRoots,
    deadlineExpired: !stopped && input.deadlineTicks === 0,
    rollback: false,
    processExit: false,
    hostDecisionRequired: !stopped && input.deadlineTicks === 0,
  }
}

function deriveExit(input) {
  if (input.outcome !== "return") {
    if (!["typedError", "panic", "fatalSignal", "forcedBoundary"].includes(input.outcome)) {
      return { accepted: false, reason: "unknownOutcome" }
    }
    return {
      accepted: true,
      outcome: input.outcome,
      exitCodePublished: false,
      distinctFromNormalReturn: true,
    }
  }
  if (!isNatural(input.code) || input.code > 255) {
    return { accepted: false, reason: "nonPortableExitCode" }
  }
  return {
    accepted: true,
    outcome: "return",
    kind: input.code === 0 ? "success" : "failure",
    code: input.code,
    truncated: false,
  }
}

export function deriveProcessRoot(input) {
  if (input.subject === "arguments") return deriveArguments(input)
  if (input.subject === "context") return deriveContext(input)
  if (input.subject === "input") return deriveInput(input)
  if (input.subject === "output") return deriveOutput(input)
  if (input.subject === "signals") return deriveSignals(input)
  if (input.subject === "services") return deriveServices(input)
  if (input.subject === "exit") return deriveExit(input)
  return { accepted: false, reason: "unknownSubject" }
}
