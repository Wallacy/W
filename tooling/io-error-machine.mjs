export const ioOperations = Object.freeze([
  "resolve",
  "open",
  "close",
  "read",
  "write",
  "flush",
  "sync",
  "metadata",
  "list",
  "create",
  "remove",
  "rename",
  "connect",
  "listen",
  "accept",
  "send",
  "receive",
  "shutdown",
  "register",
  "other",
])

export const ioErrorKinds = Object.freeze([
  "permissionDenied",
  "notFound",
  "alreadyExists",
  "notDirectory",
  "isDirectory",
  "directoryNotEmpty",
  "readOnly",
  "busy",
  "invalidInput",
  "invalidData",
  "unsupported",
  "resourceExhausted",
  "storageFull",
  "quotaExceeded",
  "timedOut",
  "connectionReset",
  "brokenPipe",
  "other",
])

export const conditionToKind = Object.freeze({
  accessDenied: "permissionDenied",
  missing: "notFound",
  exists: "alreadyExists",
  expectedDirectory: "notDirectory",
  expectedNonDirectory: "isDirectory",
  nonEmptyDirectory: "directoryNotEmpty",
  readOnly: "readOnly",
  busy: "busy",
  invalidInput: "invalidInput",
  invalidData: "invalidData",
  unsupported: "unsupported",
  resourceExhausted: "resourceExhausted",
  storageFull: "storageFull",
  quotaExceeded: "quotaExceeded",
  adapterTimedOut: "timedOut",
  connectionReset: "connectionReset",
  brokenPipe: "brokenPipe",
  unknown: "other",
})

const causeKeys = new Set(["factSource", "target", "nativeCode", "summary", "redacted"])
const causeTargets = new Set(["posix", "windows", "wasi", "other"])
const encoder = new TextEncoder()

function isNatural(value) {
  return Number.isSafeInteger(value) && value >= 0
}

function deriveCause(cause, maximumCauseBytes) {
  if (cause === null || cause === undefined) {
    return { accepted: true, causePresent: false, causeBytes: 0 }
  }
  if (!isNatural(maximumCauseBytes) || maximumCauseBytes < 1) {
    return { accepted: false, reason: "invalidCauseLimit" }
  }
  if (!cause || typeof cause !== "object" || Array.isArray(cause)) {
    return { accepted: false, reason: "invalidCause" }
  }
  if (Object.keys(cause).some((key) => !causeKeys.has(key))) {
    return { accepted: false, reason: "causeContainsForbiddenField" }
  }
  if (cause.factSource !== "provider") {
    return { accepted: false, reason: "providerCauseRequired" }
  }
  if (!causeTargets.has(cause.target)
    || typeof cause.nativeCode !== "string"
    || typeof cause.summary !== "string") {
    return { accepted: false, reason: "invalidCause" }
  }
  if (cause.redacted !== true) return { accepted: false, reason: "causeNotRedacted" }
  const causeBytes = encoder.encode(cause.target).length
    + encoder.encode(cause.nativeCode).length
    + encoder.encode(cause.summary).length
  if (causeBytes > maximumCauseBytes) {
    return { accepted: false, reason: "causeLimitExceeded", maximumCauseBytes }
  }
  return {
    accepted: true,
    causePresent: true,
    causeBytes,
    causeRedacted: true,
    nativeCodePublic: false,
    serializable: false,
    liveResources: 0,
  }
}

function deriveError(input) {
  if (input.factSource !== "provider") {
    return { accepted: false, reason: "providerErrorFactRequired" }
  }
  if (!ioOperations.includes(input.logicalOperation)) {
    return { accepted: false, reason: "unknownLogicalOperation" }
  }
  if (Object.hasOwn(input, "retryable")) {
    return { accepted: false, reason: "universalRetryClaim" }
  }
  const kind = conditionToKind[input.condition]
  if (!kind) return { accepted: false, reason: "unknownProviderCondition" }
  const cause = deriveCause(input.cause, input.maximumCauseBytes)
  if (!cause.accepted) return cause
  return {
    accepted: true,
    kind,
    operation: input.logicalOperation,
    helperOperationsExposed: false,
    duplicable: true,
    authorityOwned: false,
    ...cause,
  }
}

function deriveControl(input) {
  if (input.publishedIoError === true) {
    return { accepted: false, reason: "controlPublishedAsIoError" }
  }
  if (input.event === "wouldBlock") {
    if (input.asyncApi !== true) return { accepted: false, reason: "asyncApiRequired" }
    return { accepted: true, action: "suspend", publishedIoError: false }
  }
  if (input.event === "interruptedNoProgress") {
    return { accepted: true, action: "retryProvider", publishedIoError: false }
  }
  if (input.event === "endOfFile") {
    return { accepted: true, action: "readStepEnd", publishedIoError: false }
  }
  if (["taskCanceled", "deadlineExpired"].includes(input.event)) {
    return {
      accepted: true,
      action: "taskOutcome",
      taskOutcome: "canceled",
      publishedIoError: false,
    }
  }
  return { accepted: false, reason: "unknownControlEvent" }
}

function deriveDuplicate(input) {
  if (!ioErrorKinds.includes(input.kind) || !ioOperations.includes(input.operation)
    || !isNatural(input.copies) || input.copies < 1
    || typeof input.causePresent !== "boolean") {
    return { accepted: false, reason: "invalidDuplicateRequest" }
  }
  return {
    accepted: true,
    duplicateErrors: input.copies,
    duplicateCauseSnapshots: input.causePresent ? input.copies : 0,
    duplicateRequests: 0,
    duplicateHandles: 0,
    duplicateAuthorities: 0,
    kind: input.kind,
    operation: input.operation,
  }
}

function deriveRecovery(input) {
  if (Object.hasOwn(input, "retryable")) {
    return { accepted: false, reason: "universalRetryClaim" }
  }
  if (input.policy !== "lastLightRecipe"
    || !ioErrorKinds.includes(input.kind)
    || !ioOperations.includes(input.operation)
    || !isNatural(input.committedBytes)
    || typeof input.idempotent !== "boolean"
    || typeof input.deadlineRemaining !== "boolean") {
    return { accepted: false, reason: "invalidRecoveryFacts" }
  }
  if (input.committedBytes !== 0 || !input.idempotent || !input.deadlineRemaining) {
    return { accepted: true, decision: "requireCallerDecision" }
  }
  if (["open", "read"].includes(input.operation)
    && ["busy", "timedOut"].includes(input.kind)) {
    return { accepted: true, decision: "retryAfterBackoff" }
  }
  return { accepted: true, decision: "stop" }
}

export function deriveIoError(input) {
  if (!input || typeof input !== "object") return { accepted: false, reason: "invalidInput" }
  if (input.subject === "error") return deriveError(input)
  if (input.subject === "control") return deriveControl(input)
  if (input.subject === "duplicate") return deriveDuplicate(input)
  if (input.subject === "recovery") return deriveRecovery(input)
  return { accepted: false, reason: "unknownSubject" }
}
