const activePlans = new Set(["fixed", "bounded", "custom"])

export function runAllocatorScope(input) {
  if (!input || typeof input !== "object") return { accepted: false, reason: "invalidInput" }
  const state = {
    scope: input.scope ?? "scratch",
    plan: input.plan ?? "fixed",
    capacity: input.capacity ?? null,
    origin: input.origin ?? "local",
    mobility: input.mobility ?? "local",
    children: input.children ?? 0,
    waits: input.waits ?? 0,
    loans: input.loans ?? 0,
    dependents: input.dependents ?? 0,
    customPlanValidated: false,
    customLeaseCreated: false,
    customLeaseOpen: false,
    customLeaseClosed: false,
    // These counters are transition outputs, never case inputs. A successful
    // open creates one lease; scope close performs one provider close and one
    // lease deinit. A second close is rejected below.
    customProviderCloseCount: 0,
    customDeinitCount: 0,
  }
  if (!activePlans.has(state.plan)) return { accepted: false, reason: "unknownPlan" }
  let lastResult = null
  for (const operation of input.operations ?? []) {
    if (operation.op === "planAdmission") {
      if (operation.admitted !== true) {
        return { accepted: false, reason: "planAdmissionFailed", bodyEntered: false, bindingCreated: false }
      }
      continue
    }
    if (operation.op === "fixedAdmission") {
      const provenInfallible = operation.reservation === "static"
        && operation.admission === "infallible"
        && operation.recursionClosed === true
      if (provenInfallible) continue
      if (operation.syntaxTry !== true) {
        return { accepted: false, reason: "fixedAdmissionRequiresTry", bodyEntered: false, bindingCreated: false }
      }
      if (operation.admitted !== true) {
        return { accepted: false, reason: "planAdmissionFailed", bodyEntered: false, bindingCreated: false }
      }
      continue
    }
    if (operation.op === "parameterSlots") {
      if (operation.first !== true || operation.count !== 1) {
        return { accepted: false, reason: "allocatorParameterFirstUnique" }
      }
      continue
    }
    if (operation.op === "foreignAbi") {
      if (operation.slotPublished !== true) {
        return { accepted: false, reason: "allocatorAbiOmission" }
      }
      continue
    }
    if (operation.op === "construct") {
      if (operation.explicitAllocator === "other") state.origin = "other"
      else if (operation.explicitAllocator === "none") state.origin = state.scope
      continue
    }
    if (operation.op === "call") {
      if (operation.transitiveAllocator === true) return { accepted: false, reason: "ambientPropagation" }
      continue
    }
    if (operation.op === "rehome") {
      if (operation.source !== undefined && operation.source !== state.origin) {
        return { accepted: false, reason: "invalidRehomeSource" }
      }
      state.origin = operation.destination ?? "process"
      state.mobility = operation.mobility ?? "crossDomain"
      continue
    }
    if (operation.op === "escape") {
      return { accepted: false, reason: "scopeEscape" }
    }
    if (operation.op === "boundary") {
      if (state.origin === state.scope || state.mobility !== "crossDomain") {
        return { accepted: false, reason: "localOriginBoundary" }
      }
      continue
    }
    if (operation.op === "shadow") {
      // Allocator names use the general lexical nominal binding rule. This
      // oracle does not close the language-wide same-name shadow decision.
      if (operation.name !== undefined) state.scope = operation.name
      continue
    }
    if (operation.op === "nested") {
      state.scope = operation.name ?? state.scope
      continue
    }
    if (operation.op === "await") {
      if (operation.stable !== true || (operation.ownerInTaskFrame !== undefined && operation.ownerInTaskFrame !== true)) {
        return { accepted: false, reason: "unstableOwnerAcrossAwait" }
      }
      continue
    }
    if (operation.op === "capacity") {
      if (state.plan !== "fixed") return { accepted: false, reason: "capacityOnlyFixed" }
      if (!Number.isSafeInteger(operation.capacity) || operation.capacity <= 0) {
        return { accepted: false, reason: "capacityOverflow" }
      }
      if (operation.supported !== true) return { accepted: false, reason: "fixedUnsupportedTarget" }
      continue
    }
    if (operation.op === "customContract") {
      // Descriptor validation is data-only. Opening the executable plan is a
      // separate compiler-invoked transition below.
      const digest = operation.providerDigest
      const validDigest = Array.isArray(digest)
        && digest.length === 32
        && digest.every((byte) => Number.isInteger(byte) && byte >= 0 && byte <= 255)
        && digest.some((byte) => byte !== 0)
      if (!validDigest) return { accepted: false, reason: "customProviderDigest" }
      if (!Number.isSafeInteger(operation.version) || operation.version <= 0) {
        return { accepted: false, reason: "customProviderVersion" }
      }
      if (!["infallible", "fallible"].includes(operation.failure)) {
        return { accepted: false, reason: "customProviderFailureMode" }
      }
      if (!["provider", "backing"].includes(operation.deallocator)) {
        return { accepted: false, reason: "customProviderDeallocator" }
      }
      if (!["local", "crossDomain"].includes(operation.mobility)) {
        return { accepted: false, reason: "customProviderMobility" }
      }
      if (operation.backingOutlivesLease !== true) {
        return { accepted: false, reason: "leaseBackingLifetime" }
      }
      state.mobility = operation.mobility
      state.customPlanValidated = true
      continue
    }
    if (operation.op === "open") {
      // A failed open admits neither body nor binding and creates no lease.
      if (state.plan !== "custom" || state.customPlanValidated !== true) {
        return { accepted: false, reason: "customDescriptorRequired" }
      }
      if (!["success", "failure"].includes(operation.outcome)) {
        return { accepted: false, reason: "customPlanOpenOutcome" }
      }
      if (operation.outcome !== "success") {
        return {
          accepted: false,
          reason: "planAdmissionFailed",
          bodyEntered: false,
          bindingCreated: false,
          leaseCreated: false,
          providerCloseCount: 0,
          deinitCount: 0,
        }
      }
      state.customLeaseCreated = true
      state.customLeaseOpen = true
      state.customLeaseClosed = false
      state.customProviderCloseCount = 0
      state.customDeinitCount = 0
      continue
    }
    if (operation.op === "close") {
      if (state.customLeaseOpen && state.customLeaseClosed) {
        return { accepted: false, reason: "customLeaseClosedTwice" }
      }
      const active = []
      if (state.children > 0) active.push("child")
      if (state.waits > 0) active.push("wait")
      if (state.loans > 0) active.push("loan")
      if (state.dependents > 0) active.push("dependent")
      if (active.length > 0) {
        return {
          accepted: false,
          reason: "undrainedClose",
          active,
          typedDropsBeforeReclaim: false,
          reclaimed: false,
        }
      }
      state.customLeaseClosed = state.customLeaseOpen
      if (state.customLeaseOpen) {
        state.customProviderCloseCount += 1
        state.customDeinitCount += 1
      }
      lastResult = {
        accepted: true,
        closed: true,
        typedDrops: operation.typedDrops ?? 0,
        typedDropsBeforeReclaim: true,
        reclaimed: true,
        ...(state.customLeaseOpen
          ? {
              leaseCreated: state.customLeaseCreated,
              leaseClosed: true,
              providerCloseCount: state.customProviderCloseCount,
              deinitCount: state.customDeinitCount,
              mobility: state.mobility,
              plan: state.plan,
            }
          : {}),
      }
      continue
    }
    return { accepted: false, reason: "unknownOperation" }
  }
  if (lastResult) return lastResult
  if (state.customLeaseCreated) {
    return {
      accepted: true,
      closed: false,
      origin: state.origin,
      mobility: state.mobility,
      plan: state.plan,
      leaseCreated: true,
      leaseClosed: state.customLeaseClosed,
      providerCloseCount: state.customProviderCloseCount,
      deinitCount: state.customDeinitCount,
    }
  }
  return { accepted: true, closed: false, origin: state.origin, mobility: state.mobility, plan: state.plan }
}
