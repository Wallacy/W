import assert from "node:assert/strict"
import test from "node:test"
import {
  conditionToKind,
  deriveIoError,
  ioErrorKinds,
  ioOperations,
} from "./io-error-machine.mjs"

test("portable taxonomies are closed and unknown provider conditions map explicitly", () => {
  assert.equal(ioErrorKinds.length, 18)
  assert.equal(ioOperations.length, 20)
  assert.equal(conditionToKind.unknown, "other")
  assert.equal(ioErrorKinds.includes("wouldBlock"), false)
  assert.equal(ioErrorKinds.includes("canceled"), false)
})

test("logical W operation is independent from provider helper operations", () => {
  const result = deriveIoError({
    subject: "error",
    factSource: "provider",
    condition: "missing",
    logicalOperation: "read",
    helperOperations: ["poll", "pread"],
  })
  assert.equal(result.accepted, true)
  assert.equal(result.kind, "notFound")
  assert.equal(result.operation, "read")
  assert.equal(result.helperOperationsExposed, false)
})

test("native causes are bounded redacted snapshots without live authority", () => {
  const accepted = deriveIoError({
    subject: "error",
    factSource: "provider",
    condition: "accessDenied",
    logicalOperation: "open",
    maximumCauseBytes: 64,
    cause: {
      factSource: "provider",
      target: "posix",
      nativeCode: "EACCES",
      summary: "access denied",
      redacted: true,
    },
  })
  const leaked = deriveIoError({
    subject: "error",
    factSource: "provider",
    condition: "accessDenied",
    logicalOperation: "open",
    maximumCauseBytes: 64,
    cause: {
      factSource: "provider",
      target: "posix",
      nativeCode: "EACCES",
      summary: "access denied",
      redacted: true,
      path: "/secret/menu",
    },
  })
  assert.equal(accepted.causeRedacted, true)
  assert.equal(accepted.nativeCodePublic, false)
  assert.equal(accepted.serializable, false)
  assert.equal(accepted.liveResources, 0)
  assert.equal(leaked.reason, "causeContainsForbiddenField")
})

test("control outcomes never become portable I/O failures", () => {
  const wouldBlock = deriveIoError({
    subject: "control",
    event: "wouldBlock",
    asyncApi: true,
  })
  const eof = deriveIoError({ subject: "control", event: "endOfFile" })
  const canceled = deriveIoError({ subject: "control", event: "taskCanceled" })
  assert.deepEqual(wouldBlock, { accepted: true, action: "suspend", publishedIoError: false })
  assert.equal(eof.action, "readStepEnd")
  assert.equal(canceled.taskOutcome, "canceled")
  assert.equal(canceled.publishedIoError, false)
})

test("duplicating an error copies snapshots but no request, handle, or authority", () => {
  const result = deriveIoError({
    subject: "duplicate",
    kind: "storageFull",
    operation: "write",
    copies: 2,
    causePresent: true,
  })
  assert.equal(result.duplicateErrors, 2)
  assert.equal(result.duplicateCauseSnapshots, 2)
  assert.equal(result.duplicateRequests, 0)
  assert.equal(result.duplicateHandles, 0)
  assert.equal(result.duplicateAuthorities, 0)
})

test("recipe recovery depends on progress, idempotence, and remaining deadline", () => {
  const retry = deriveIoError({
    subject: "recovery",
    policy: "lastLightRecipe",
    kind: "timedOut",
    operation: "read",
    committedBytes: 0,
    idempotent: true,
    deadlineRemaining: true,
  })
  const afterProgress = deriveIoError({
    subject: "recovery",
    policy: "lastLightRecipe",
    kind: "timedOut",
    operation: "read",
    committedBytes: 1,
    idempotent: true,
    deadlineRemaining: true,
  })
  const forged = deriveIoError({
    subject: "recovery",
    policy: "lastLightRecipe",
    kind: "timedOut",
    operation: "read",
    committedBytes: 0,
    idempotent: true,
    deadlineRemaining: true,
    retryable: true,
  })
  assert.equal(retry.decision, "retryAfterBackoff")
  assert.equal(afterProgress.decision, "requireCallerDecision")
  assert.equal(forged.reason, "universalRetryClaim")
})

test("callers cannot forge provider facts", () => {
  const result = deriveIoError({
    subject: "error",
    factSource: "caller",
    condition: "missing",
    logicalOperation: "open",
  })
  assert.equal(result.accepted, false)
  assert.equal(result.reason, "providerErrorFactRequired")
})
