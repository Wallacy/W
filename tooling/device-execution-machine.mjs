const MODES = new Set(["take", "copy", "ref", "inout"])
const NUMERIC_MODES = new Set(["strict", "reproducible", "fast"])

export class DeviceExecutionModelError extends Error {
  constructor(code) {
    super(code)
    this.name = "DeviceExecutionModelError"
    this.code = code
  }
}

function fail(code) {
  throw new DeviceExecutionModelError(code)
}

function requireString(value, code) {
  if (typeof value !== "string" || value.length === 0) fail(code)
}

function requireDigest(value, code) {
  if (!/^sha256:[0-9a-f]{64}$/.test(value ?? "")) fail(code)
}

function requirePositive(value, code) {
  if (!Number.isSafeInteger(value) || value <= 0) fail(code)
}

function requireScope(state, phases = ["ready"]) {
  if (!phases.includes(state.phase)) fail("W-DEVICE-0004")
}

function requireInvocation(state, id, phases = undefined) {
  const invocation = state.invocations[id]
  if (!invocation) fail("deviceInvocationMissing")
  if (phases && !phases.includes(invocation.phase)) {
    fail("deviceInvocationPhaseInvalid")
  }
  return invocation
}

function validateLimits(limits) {
  for (const key of [
    "maximumInFlight",
    "maximumCommandBytes",
    "maximumArgumentBytes",
    "maximumResultBytes",
    "maximumDependencyEdges",
    "maximumRetainedDeviceBytes",
    "maximumCompletionRecords",
    "maximumCleanupSteps",
  ]) {
    requirePositive(limits?.[key], "W-DEVICE-0008")
  }
}

function validateReceipt(state, invocation, receipt, phase) {
  if (receipt?.issuedBy !== "provider") fail("W-DEVICE-0005")
  if (receipt.generation !== state.providerGeneration) fail("W-DEVICE-0006")
  if (receipt.moduleIdentity !== state.moduleIdentity) fail("W-DEVICE-0001")
  if (receipt.artifactIdentity !== state.artifactIdentity) fail("W-DEVICE-0001")
  if (receipt.queueId !== state.queueId) fail("W-DEVICE-0005")
  if (receipt.invocation !== invocation.id) fail("W-DEVICE-0005")
  state.physicalTrace.push(`${phase}-receipt:${invocation.id}:${receipt.generation}`)
}

function validateArguments(state, args) {
  if (!Array.isArray(args)) fail("W-DEVICE-0003")
  for (const argument of args) {
    if (!MODES.has(argument.mode)) fail("W-DEVICE-0003")
    if (argument.hiddenTransfer === true) fail("W-DEVICE-0003")
    if (argument.kind === "tensor") {
      const resident = argument.deviceId === state.deviceId
      if (!resident && argument.hostShared !== true) fail("W-DEVICE-0003")
    }
    if (argument.mode === "ref" && argument.lifetimeStable !== true) {
      fail("W-DEVICE-0003")
    }
    if (argument.mode === "inout") {
      if (argument.exclusive !== true || argument.aliasFree !== true) {
        fail("W-DEVICE-0003")
      }
    }
    if (argument.mode === "take" && argument.owned !== true) fail("W-DEVICE-0003")
    if (argument.mode === "copy" && argument.copy === true && argument.copyable !== true) {
      fail("W-DEVICE-0003")
    }
  }
}

function budgetInvocation(state, operation) {
  const requested = {
    command: operation.commandBytes ?? 0,
    argument: operation.argumentBytes ?? 0,
    result: operation.resultBytes ?? 0,
    retained: operation.retainedDeviceBytes ?? 0,
  }
  for (const value of Object.values(requested)) {
    if (!Number.isSafeInteger(value) || value < 0) fail("W-DEVICE-0008")
  }
  const live = Object.values(state.invocations).filter((item) => item.phase !== "joined")
  if (live.length >= state.limits.maximumInFlight) {
    fail("W-DEVICE-0008")
  }
  if (requested.command > state.limits.maximumCommandBytes) fail("W-DEVICE-0008")
  if (requested.argument > state.limits.maximumArgumentBytes) fail("W-DEVICE-0008")
  if (requested.result > state.limits.maximumResultBytes) fail("W-DEVICE-0008")
  const retained = live.reduce((total, item) => total + item.budget.retained, 0)
  if (retained + requested.retained > state.limits.maximumRetainedDeviceBytes) {
    fail("W-DEVICE-0008")
  }
  if ((operation.dependencies?.length ?? 0) > state.limits.maximumDependencyEdges) {
    fail("W-DEVICE-0008")
  }
  return requested
}

function invocationProjection(invocation) {
  return {
    phase: invocation.phase,
    outcome: invocation.outcome,
    cancelRequested: invocation.cancelRequested,
    cleanupCount: invocation.cleanupCount,
  }
}

function verifyState(state) {
  for (const invocation of Object.values(state.invocations)) {
    if (["outcomeCommitted", "joined"].includes(invocation.phase)) {
      if (invocation.drained !== true || invocation.cleanupCount !== 1) {
        fail("deviceOutcomeBeforeCleanup")
      }
    }
    if (invocation.cleanupCount > 1) fail("deviceCleanupRepeated")
  }
  if (state.phase === "closed") {
    const active = Object.values(state.invocations).some((item) => item.phase !== "joined")
    if (active || state.quarantined.length > 0) fail("deviceCloseBeforeDrain")
  }
}

function initialState() {
  return {
    phase: "uninitialized",
    admissionOpen: false,
    moduleIdentity: null,
    artifactClass: null,
    artifactIdentity: null,
    artifactInstances: [],
    providerAbiDigest: null,
    deviceTarget: null,
    queueId: null,
    deviceId: null,
    provider: null,
    providerGeneration: null,
    numericMode: null,
    limits: null,
    invocations: {},
    happensBefore: [],
    logicalTrace: [],
    physicalTrace: [],
    quarantined: [],
    suppressedCompletions: [],
    comparisons: [],
  }
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "open": {
      requireScope(state, ["uninitialized"])
      requireString(operation.moduleIdentity, "W-DEVICE-0001")
      if (operation.artifactClass !== "closed") fail("W-DEVICE-0001")
      requireString(operation.artifactIdentity, "W-DEVICE-0001")
      if (operation.artifactModuleIdentity !== operation.moduleIdentity) fail("W-DEVICE-0001")
      requireDigest(operation.artifactProviderAbiDigest, "W-DEVICE-0001")
      requireDigest(operation.providerAbiDigest, "W-DEVICE-0002")
      if (operation.artifactProviderAbiDigest !== operation.providerAbiDigest) {
        fail("W-DEVICE-0001")
      }
      requireString(operation.artifactTarget, "W-DEVICE-0001")
      requireString(operation.deviceTarget, "W-DEVICE-0002")
      if (operation.artifactTarget !== operation.deviceTarget) fail("W-DEVICE-0001")
      if (
        !Array.isArray(operation.artifactInstances) ||
        operation.artifactInstances.length === 0 ||
        new Set(operation.artifactInstances).size !== operation.artifactInstances.length
      ) {
        fail("W-DEVICE-0001")
      }
      for (const instance of operation.artifactInstances) requireString(instance, "W-DEVICE-0001")
      requireString(operation.queueId, "W-DEVICE-0002")
      requireString(operation.deviceId, "W-DEVICE-0002")
      requireString(operation.provider, "W-DEVICE-0002")
      requireString(operation.providerGeneration, "W-DEVICE-0006")
      if (operation.providerResolved !== true) fail("W-DEVICE-0002")
      if (operation.queueDeviceId !== operation.deviceId) fail("W-DEVICE-0002")
      if (!NUMERIC_MODES.has(operation.numericMode)) fail("W-DEVICE-0007")
      validateLimits(operation.limits)
      state.phase = "ready"
      state.admissionOpen = true
      state.moduleIdentity = operation.moduleIdentity
      state.artifactClass = operation.artifactClass
      state.artifactIdentity = operation.artifactIdentity
      state.artifactInstances = [...operation.artifactInstances]
      state.providerAbiDigest = operation.providerAbiDigest
      state.deviceTarget = operation.deviceTarget
      state.queueId = operation.queueId
      state.deviceId = operation.deviceId
      state.provider = operation.provider
      state.providerGeneration = operation.providerGeneration
      state.numericMode = operation.numericMode
      state.limits = structuredClone(operation.limits)
      state.logicalTrace.push("scope:ready")
      return
    }

    case "stage": {
      requireScope(state)
      if (!state.admissionOpen) fail("W-DEVICE-0004")
      requireString(operation.id, "deviceInvocationMissing")
      if (state.invocations[operation.id]) fail("deviceInvocationDuplicate")
      if (operation.moduleIdentity !== state.moduleIdentity) fail("W-DEVICE-0001")
      if (operation.queueId !== state.queueId) fail("W-DEVICE-0002")
      if (!state.artifactInstances.includes(operation.kernelInstanceIdentity)) {
        fail("W-DEVICE-0001")
      }
      validateArguments(state, operation.arguments)
      const budget = budgetInvocation(state, operation)
      state.invocations[operation.id] = {
        id: operation.id,
        phase: "staged",
        dependencies: [...(operation.dependencies ?? [])],
        budget,
        cancelRequested: false,
        drained: false,
        outcome: null,
        error: null,
        cleanupCount: 0,
        resultPublished: false,
      }
      state.logicalTrace.push(`stage:${operation.id}`)
      return
    }

    case "submit": {
      const invocation = requireInvocation(state, operation.id, ["staged"])
      if (operation.callerReady === true) fail("W-DEVICE-0005")
      validateReceipt(state, invocation, operation.receipt, "submit")
      invocation.phase = "submitted"
      state.logicalTrace.push(`submit:${operation.id}`)
      return
    }

    case "start": {
      const invocation = requireInvocation(state, operation.id, ["submitted"])
      for (const dependency of invocation.dependencies) {
        const parent = requireInvocation(state, dependency)
        if (!new Set([
          "bodySettled",
          "providerDrained",
          "cleanup",
          "outcomeCommitted",
          "joined",
        ]).has(parent.phase)) {
          fail("W-DEVICE-0005")
        }
        state.happensBefore.push(`device:${dependency}->device:${operation.id}`)
      }
      invocation.phase = "deviceRunning"
      state.logicalTrace.push(`start:${operation.id}`)
      return
    }

    case "cancel": {
      const invocation = requireInvocation(state, operation.id)
      if (invocation.phase === "staged") {
        invocation.cancelRequested = true
        invocation.drained = true
        invocation.cleanupCount = 1
        invocation.outcome = "cancelled"
        invocation.phase = "joined"
        state.logicalTrace.push(`cancel-before-submit:${operation.id}`)
        return
      }
      if (["submitted", "deviceRunning"].includes(invocation.phase)) {
        invocation.cancelRequested = true
        state.logicalTrace.push(`cancel-after-submit:${operation.id}`)
        return
      }
      if (["bodySettled", "providerDrained", "cleanup", "outcomeCommitted", "joined"].includes(invocation.phase)) {
        state.logicalTrace.push(`cancel-late:${operation.id}`)
        return
      }
      fail("deviceCancellationPhaseInvalid")
    }

    case "complete": {
      const invocation = requireInvocation(state, operation.id, ["submitted", "deviceRunning"])
      validateReceipt(state, invocation, operation.receipt, "completion")
      const pendingCompletionRecords = Object.values(state.invocations).filter((item) =>
        new Set(["bodySettled", "providerDrained", "cleanup", "outcomeCommitted"]).has(item.phase)
      ).length
      if (pendingCompletionRecords >= state.limits.maximumCompletionRecords) {
        fail("W-DEVICE-0008")
      }
      if (!new Set(["success", "error", "deviceLost"]).has(operation.result)) {
        fail("W-DEVICE-0006")
      }
      invocation.phase = "bodySettled"
      invocation.error = operation.result === "error" ? "asynchronousFailure" : null
      invocation.resultPublished = false
      if (operation.result === "deviceLost") {
        invocation.error = "deviceLost"
        state.phase = "faulted"
        state.admissionOpen = false
        state.quarantined.push(operation.id)
      }
      state.happensBefore.push(`device:${operation.id}->host:${operation.id}`)
      state.logicalTrace.push(`complete:${operation.id}:${operation.result}`)
      return
    }

    case "drain": {
      const invocation = requireInvocation(state, operation.id, ["bodySettled"])
      if (operation.providerDrained !== true) fail("W-DEVICE-0006")
      invocation.phase = "providerDrained"
      invocation.drained = true
      state.quarantined = state.quarantined.filter((id) => id !== operation.id)
      state.logicalTrace.push(`drain:${operation.id}`)
      return
    }

    case "cleanup": {
      const invocation = requireInvocation(state, operation.id, ["providerDrained"])
      invocation.phase = "cleanup"
      invocation.cleanupCount += 1
      if (operation.releasedLoans !== true || operation.releasedStaging !== true) {
        fail("W-DEVICE-0003")
      }
      const steps = operation.steps ?? 1
      if (!Number.isSafeInteger(steps) || steps <= 0 || steps > state.limits.maximumCleanupSteps) {
        fail("W-DEVICE-0008")
      }
      state.logicalTrace.push(`cleanup:${operation.id}`)
      return
    }

    case "commit": {
      const invocation = requireInvocation(state, operation.id, ["cleanup"])
      invocation.outcome = invocation.error
        ? "error"
        : invocation.cancelRequested
          ? "cancelled"
          : "success"
      invocation.resultPublished = invocation.outcome === "success"
      invocation.phase = "outcomeCommitted"
      state.logicalTrace.push(`commit:${operation.id}:${invocation.outcome}`)
      return
    }

    case "join": {
      const invocation = requireInvocation(state, operation.id, ["outcomeCommitted"])
      invocation.phase = "joined"
      state.logicalTrace.push(`join:${operation.id}`)
      return
    }

    case "hostRead": {
      const invocation = requireInvocation(state, operation.id, ["joined"])
      if (invocation.outcome !== "success") fail("W-DEVICE-0003")
      if (operation.explicitTransfer !== true || operation.hostAddressable !== true) {
        fail("W-DEVICE-0003")
      }
      state.logicalTrace.push(`host-read:${operation.id}`)
      return
    }

    case "crossQueue": {
      requireString(operation.from, "W-DEVICE-0005")
      requireString(operation.to, "W-DEVICE-0005")
      if (
        operation.receipt?.issuedBy !== "provider" ||
        operation.receipt.generation !== state.providerGeneration ||
        operation.receipt.from !== operation.from ||
        operation.receipt.to !== operation.to
      ) {
        fail("W-DEVICE-0005")
      }
      state.happensBefore.push(`queue:${operation.from}->queue:${operation.to}`)
      return
    }

    case "lateCompletion": {
      if (operation.generation === state.providerGeneration) fail("W-DEVICE-0006")
      if (!new Set(["faulted", "closing", "closed"]).has(state.phase)) {
        fail("W-DEVICE-0006")
      }
      state.suppressedCompletions.push(operation.id)
      state.physicalTrace.push(`suppress-stale:${operation.id}:${operation.generation}`)
      return
    }

    case "compare": {
      if (!NUMERIC_MODES.has(operation.mode)) fail("W-DEVICE-0007")
      if (
        operation.sameModule !== true ||
        operation.sameLayout !== true ||
        operation.sameEffects !== true ||
        operation.memoryPlanProved !== true ||
        operation.hiddenTransfer === true
      ) {
        fail("W-DEVICE-0007")
      }
      let equivalent = false
      if (operation.mode === "strict") {
        equivalent = operation.expected === operation.actual && operation.sameFailurePoint === true
      } else if (operation.mode === "reproducible") {
        equivalent =
          operation.algorithmVersion === operation.expectedAlgorithmVersion &&
          operation.valueDigest === operation.expectedValueDigest
      } else {
        equivalent =
          Number.isFinite(operation.tolerance) &&
          operation.tolerance >= 0 &&
          Math.abs(operation.expected - operation.actual) <= operation.tolerance
      }
      if (!equivalent) fail("W-DEVICE-0007")
      state.comparisons.push(operation.mode)
      return
    }

    case "closeAdmission": {
      requireScope(state, ["ready", "faulted"])
      state.admissionOpen = false
      state.phase = "closing"
      state.logicalTrace.push("scope:closing")
      return
    }

    case "close": {
      requireScope(state, ["closing"])
      if (state.quarantined.length > 0) fail("W-DEVICE-0006")
      const active = Object.values(state.invocations).some((item) => item.phase !== "joined")
      if (active) fail("W-DEVICE-0004")
      state.phase = "closed"
      state.logicalTrace.push("scope:closed")
      return
    }

    case "dropScope":
      if (state.phase !== "closed") fail("W-DEVICE-0004")
      return

    default:
      fail("deviceOperationUnknown")
  }
}

function projectState(state) {
  return {
    phase: state.phase,
    admissionOpen: state.admissionOpen,
    moduleIdentity: state.moduleIdentity,
    artifactClass: state.artifactClass,
    artifactIdentity: state.artifactIdentity,
    artifactInstances: [...state.artifactInstances],
    providerAbiDigest: state.providerAbiDigest,
    deviceTarget: state.deviceTarget,
    queueId: state.queueId,
    deviceId: state.deviceId,
    providerGeneration: state.providerGeneration,
    invocations: Object.fromEntries(
      Object.entries(state.invocations).map(([id, item]) => [id, invocationProjection(item)]),
    ),
    happensBefore: [...state.happensBefore],
    logicalTrace: [...state.logicalTrace],
    quarantined: [...state.quarantined],
    suppressedCompletions: [...state.suppressedCompletions],
    comparisons: [...state.comparisons],
  }
}

export function runDeviceExecutionOperations(operations) {
  const state = initialState()
  let status = "accepted"
  let error = null

  try {
    for (const operation of operations) {
      applyOperation(state, operation)
      verifyState(state)
    }
  } catch (caught) {
    if (!(caught instanceof DeviceExecutionModelError)) throw caught
    status = "rejected"
    error = caught.code
  }

  return {
    status,
    error,
    state: projectState(state),
    physical: { trace: [...state.physicalTrace] },
  }
}
