const SHARED_CONTEXTS = new Set(["binding", "stored-field"])
const SOURCE_KINDS = new Set(["temporary", "existing", "borrowed"])
const ALLOCATORS = new Set(["product.default", "lexical", "custom"])
const PROFILE_PROGRESS = new Set(["general", "bounded", "lockFree", "waitFree"])
const PROFILE_ADOPTION = new Set(["shared-control", "request", "process"])
const PROFILE_FAILURE = new Set(["infallible", "fallible"])
const ALLOCATOR_FAILURE = new Set(["infallible", "fallible", "normal-oom"])

// SHC0 reuses the repository diagnostic catalog. The remaining SHC0 labels
// are oracle-internal phases and must not become a second public family.
const DIAGNOSTICS = new Map([
  ["SHC0-MISSING-TRY", "W-EFFECT-0010"],
  ["SHC0-REDUNDANT-TRY", "W-EFFECT-0010"],
  ["SHC0-TRY-OUTSIDE-TYPE", "W-EFFECT-0010"],
  ["SHC0-ERROR-SET", "W-EFFECT-0010"],
  ["SHC0-PROVIDER-PROFILE-JOIN", "W-ALLOCATOR-0004"],
  ["SHC0-PROVIDER-FAILURE-MISMATCH", "W-ALLOCATOR-0004"],
  ["SHC0-PLAN-NOT-OPEN", "W-ALLOCATOR-0004"],
  ["SHC0-PLAN-OPEN-FAILURE", "W-ALLOCATOR-0007"],
  ["SHC0-PLAN-OPEN-CONTRADICTION", "W-ALLOCATOR-0004"],
  ["SHC0-LOCAL-ORIGIN-BOUNDARY", "W-ALLOCATOR-0003"],
  ["SHC0-SHARED-REHOME", "W-OWNERSHIP-0017"],
  ["SHC0-CROSS-DOMAIN-FACTS", "W-ALLOCATOR-0003"],
  ["SHC0-FFI-LOCAL-ORIGIN", "W-ALLOCATOR-0003"],
  ["SHC0-FFI-LEASE-FACTS", "W-OWNERSHIP-0017"],
  ["SHC0-BORROWED-PAYLOAD", "W-BORROW-0010"],
  ["SHC0-CONTEXT-PROMOTION", "W-OWNERSHIP-0013"],
])

function clone(value) {
  return JSON.parse(JSON.stringify(value))
}

function outcome(state, trace, status, code, operation, facts = {}) {
  const resultFacts = {
    sourceConsumed: state.sourceConsumed,
    strong: state.strong,
    weak: state.weak,
    blockAlive: state.controlBlockAlive,
    published: state.published,
    ...facts,
  }
  return {
    status,
    code,
    ...(DIAGNOSTICS.has(code) ? { diagnostic: DIAGNOSTICS.get(code) } : {}),
    operation,
    facts: resultFacts,
    state: clone(state),
    trace: clone(trace),
  }
}

function reject(state, trace, code, operation, facts = {}) {
  return outcome(state, trace, "rejected", code, operation, facts)
}

function errorOutcome(state, trace, code, operation, facts = {}) {
  return outcome(state, trace, "error", code, operation, facts)
}

function faultOutcome(state, trace, code, operation, facts = {}) {
  return outcome(state, trace, "fault", code, operation, facts)
}

function accepted(state, trace, facts = {}) {
  return {
    status: "accepted",
    facts: { ...facts },
    state: clone(state),
    trace: clone(trace),
  }
}

function descriptorProfileValid(profile) {
  return profile &&
    profile.payloadShareable === undefined &&
    profile.counterThreadSafe === undefined &&
    profile.allOriginsMobility === undefined &&
    typeof profile.providerDigest === "string" && /^sha256:[0-9a-f]{64}$/.test(profile.providerDigest) &&
    Number.isSafeInteger(profile.version) && profile.version > 0 &&
    PROFILE_FAILURE.has(profile.failure) &&
    ["provider", "backing"].includes(profile.deallocator) &&
    ["local", "crossDomain"].includes(profile.mobility) &&
    typeof profile.lifetime === "string" && profile.lifetime.length > 0 &&
    PROFILE_PROGRESS.has(profile.progress) &&
    PROFILE_ADOPTION.has(profile.adoptionFamily) &&
    profile.limits && Number.isSafeInteger(profile.limits.maxObjectBytes) && profile.limits.maxObjectBytes > 0 &&
    profile.recipeJoined === true
}

function allocationError(input) {
  return input === "success" || input === undefined ? null : input
}

function runtimeFailureStatus(axis, allocatorFailure) {
  return axis === "allocation" && allocatorFailure === "normal-oom" ? "fault" : "error"
}

function originMobility(origin) {
  return ["process", "crossDomain"].includes(origin) ? "crossDomain" : "local"
}

function hasCallerFact(operation) {
  return ["controlBlockMobility", "payloadMobility", "originMobility", "rehomed"].some((key) =>
    Object.prototype.hasOwnProperty.call(operation, key))
}

function analysisFactsValid(facts) {
  if (facts === undefined) return true
  return facts && typeof facts === "object" && !Array.isArray(facts) &&
    (facts.payloadShareable === undefined || typeof facts.payloadShareable === "boolean") &&
    (facts.counterThreadSafe === undefined || typeof facts.counterThreadSafe === "boolean") &&
    facts.originMobility === undefined && facts.allOriginsMobility === undefined &&
    facts.controlBlockMobility === undefined
}

function mapOrigins(originMap) {
  const origins = []
  const visit = (value) => {
    if (typeof value === "string") origins.push(value)
    else if (value && typeof value === "object") {
      for (const child of Object.values(value)) visit(child)
    }
  }
  visit(originMap)
  return origins
}

/**
 * SHC0 is independent from hir-memory-machine.mjs. It models declarative
 * shared construction, the control block, and its publication boundary.
 */
export function runSharedControl(input) {
  const state = {
    schema: "w-shared-control-state-shc0",
    binding: "root",
    sourceConsumed: false,
    payloadInitialized: false,
    payloadAlive: false,
    controlBlockAlive: false,
    published: false,
    strong: 0,
    weak: 0,
    payloadDeinitCount: 0,
    controlBlockDeinitCount: 0,
    payloadOrigin: null,
    payloadMobility: null,
    payloadShareable: false,
    controlBlockOrigin: null,
    controlBlockAllocatorContract: null,
    controlBlockInstance: null,
    controlBlockMobility: null,
    controlBlockDeallocator: null,
    controlBlockLifetime: null,
    controlBlockAdoptionFamily: null,
    controlBlockBulkReleaseOwner: null,
    counterThreadSafe: false,
    allOriginsCrossDomain: false,
    allocator: null,
    allocatorScope: null,
    deallocator: null,
    providerProfile: null,
    analysisFacts: null,
    initializerThrows: false,
    allocatorFailure: null,
    initializerError: null,
    allocationError: null,
    errorEdges: [],
    errorSet: [],
    planOpen: false,
    allocationOriginMap: null,
    coallocationPromised: false,
    rehomed: false,
    ffiLease: false,
    ffiPinned: false,
    ffiUnregistered: false,
    ffiInFlightDrained: false,
  }
  const trace = []
  if (!input || typeof input !== "object" || !Array.isArray(input.operations) || input.operations.length === 0) {
    return reject(state, trace, "SHC0-INVALID-INPUT", 0)
  }

  for (const [index, operation] of input.operations.entries()) {
    if (!operation || typeof operation !== "object" || typeof operation.op !== "string") {
      return reject(state, trace, "SHC0-INVALID-OPERATION", index)
    }

    if (operation.op === "openPlan") {
      if (state.planOpen || state.sourceConsumed || state.published) return reject(state, trace, "SHC0-DUPLICATE-PLAN-OPEN", index)
      if (operation.allocator !== "custom") return reject(state, trace, "SHC0-PLAN-ALLOCATOR", index)
      if (!descriptorProfileValid(operation.providerProfile)) {
        return reject(state, trace, "SHC0-PROVIDER-PROFILE-JOIN", index)
      }
      if (!["success", "failure"].includes(operation.outcome)) {
        return reject(state, trace, "SHC0-PLAN-OPEN-OUTCOME", index)
      }
      if (operation.outcome === "failure") {
        if (operation.providerProfile.failure === "infallible") {
          trace.push({ operation: "allocator-plan-open", outcome: "fault", bodyEntered: false, bindingCreated: false, published: false })
          return faultOutcome(state, trace, "SHC0-PLAN-OPEN-CONTRADICTION", index, { published: false })
        }
        trace.push({ operation: "allocator-plan-open", outcome: "failure", bodyEntered: false, bindingCreated: false, published: false })
        return errorOutcome(state, trace, "SHC0-PLAN-OPEN-FAILURE", index, {
          bodyEntered: false,
          bindingCreated: false,
          sourceConsumed: false,
          payloadDropCount: 0,
          partialControlBlockDropCount: 0,
          failureKind: "AllocationError",
          published: false,
        })
      }
      state.planOpen = true
      state.allocator = "custom"
      state.providerProfile = clone(operation.providerProfile)
      state.allocatorScope = operation.allocatorScope ?? null
      trace.push({ operation: "allocator-plan-open", outcome: "success", profileJoined: true })
      continue
    }

    if (operation.op === "rehomePayload") {
      if (state.published || state.sourceConsumed) return reject(state, trace, "SHC0-SHARED-REHOME", index)
      if (operation.sourceUnique !== true) return reject(state, trace, "SHC0-REHOME-REQUIRES-UNIQUE", index)
      if (operation.destinationMobility !== "crossDomain") return reject(state, trace, "SHC0-REHOME-MOBILITY", index)
      state.rehomed = true
      state.payloadOrigin = operation.destinationOrigin ?? "process"
      state.payloadMobility = "crossDomain"
      // Rehome changes origin and mobility. It does not prove payload sharing.
      trace.push({ operation: "rehome-payload", source: "unique", destination: state.payloadOrigin, mobility: "crossDomain" })
      continue
    }

    if (operation.op === "construct") {
      if (state.sourceConsumed || state.published) return reject(state, trace, "SHC0-DUPLICATE-CONSTRUCTION", index)
      const context = operation.context
      const source = operation.source
      const allocator = operation.allocator ?? "product.default"
      if (!SHARED_CONTEXTS.has(context)) return reject(state, trace, "SHC0-CONTEXT-PROMOTION", index)
      if (operation.typeSpelling === "try shared T" || operation.tryOnType === true) {
        return reject(state, trace, "SHC0-TRY-OUTSIDE-TYPE", index)
      }
      if (operation.typeSpelling !== "shared T") return reject(state, trace, "SHC0-SHARED-TYPE-SPELLING", index)
      if (!SOURCE_KINDS.has(source)) return reject(state, trace, "SHC0-SOURCE-KIND", index)
      if (source === "borrowed") return reject(state, trace, "SHC0-BORROWED-PAYLOAD", index)
      if (source === "existing" && operation.take !== true) return reject(state, trace, "SHC0-MISSING-TAKE", index)
      if (operation.lifetimeIndependent !== true) return reject(state, trace, "SHC0-LIFETIME-DEPENDENT", index)
      if (!ALLOCATORS.has(allocator)) return reject(state, trace, "SHC0-ALLOCATOR-SOURCE", index)
      if (operation.nestedFieldAllocator === "propagated") return reject(state, trace, "SHC0-ALLOCATOR-PROPAGATION", index)
      if (hasCallerFact(operation)) return reject(state, trace, "SHC0-CALLER-FACT", index)
      if (!analysisFactsValid(operation.analysisFacts)) return reject(state, trace, "SHC0-CALLER-FACT", index)
      if (allocator === "custom") {
        if (operation.planOpen !== undefined || operation.providerProfile !== undefined) {
          return reject(state, trace, "SHC0-PROVIDER-PROFILE-JOIN", index)
        }
        if (!state.planOpen || !state.providerProfile) return reject(state, trace, "SHC0-PLAN-NOT-OPEN", index)
      }

      const profile = allocator === "custom" ? state.providerProfile : null
      const initializerThrows = operation.initializerThrows === true
      const allocatorFailure = operation.failure ?? operation.allocatorFailure ??
        (profile?.failure ?? (allocator === "product.default" ? "normal-oom" : "infallible"))
      if (!ALLOCATOR_FAILURE.has(allocatorFailure)) return reject(state, trace, "SHC0-ALLOCATOR-FAILURE", index)
      if (profile && operation.failure !== undefined && profile.failure !== operation.failure) {
        return reject(state, trace, "SHC0-PROVIDER-FAILURE-MISMATCH", index)
      }
      const syntaxTry = operation.try === true || operation.syntaxTry === true
      // The default OOM path is a normal runtime failure. A source `try` is
      // required only for a catchable allocator site or a throwing initializer.
      const tryRequired = initializerThrows || allocatorFailure === "fallible"
      if (tryRequired && !syntaxTry) return reject(state, trace, "SHC0-MISSING-TRY", index)
      if (!tryRequired && syntaxTry) return reject(state, trace, "SHC0-REDUNDANT-TRY", index)

      const initializerError = initializerThrows ? operation.initializerError ?? "InitializerError" : null
      const allocationErrorEdge = allocatorFailure === "fallible" ? operation.allocationError ?? "AllocationError" : null
      const errorEdges = [...new Set([initializerError, allocationErrorEdge].filter((edge) => edge !== null))]
      if (errorEdges.some((edge) => typeof edge !== "string" || edge.trim() === "")) {
        return reject(state, trace, "SHC0-ERROR-SET", index, { errorEdges, errorSet: operation.errorSet ?? [] })
      }
      if (operation.errorSet !== undefined) {
        const errorSet = operation.errorSet
        const uniqueErrorSet = Array.isArray(errorSet) && new Set(errorSet).size === errorSet.length
        const exactErrorSet = uniqueErrorSet && errorSet.length === errorEdges.length &&
          errorEdges.every((edge) => errorSet.includes(edge))
        if (!exactErrorSet) {
          return reject(state, trace, "SHC0-ERROR-SET", index, { errorEdges, errorSet })
        }
      } else if (errorEdges.length > 1) {
        return reject(state, trace, "SHC0-ERROR-SET", index, { errorEdges, errorSet: [] })
      }

      const payloadOrigin = operation.payloadOrigin ?? operation.originMap?.["$storage"] ?? state.payloadOrigin ?? (allocator === "lexical" ? "lexical" : allocator)
      const controlOrigin = operation.controlBlockOrigin ?? operation.originMap?.["$controlBlock"] ?? payloadOrigin
      const originMap = operation.originMap ?? {}
      if (originMap["$controlBlock"] !== controlOrigin || originMap["$storage"] !== payloadOrigin) {
        // A valid provider join must always lower both hidden paths. This is
        // an invariant failure, not a source diagnostic.
        return faultOutcome(state, trace, "SHC0-CONTROL-BLOCK-ORIGIN-INVARIANT", index)
      }
      const origins = mapOrigins(originMap)
      const payloadMobility = originMobility(payloadOrigin)
      const controlMobility = originMobility(controlOrigin)
      const allOriginsCrossDomain = origins.length > 0 && origins.every((origin) => originMobility(origin) === "crossDomain")
      if (profile && profile.mobility !== controlMobility) {
        return reject(state, trace, "SHC0-PROVIDER-FAILURE-MISMATCH", index, { controlBlockMobility: controlMobility })
      }
      const analysisFacts = operation.analysisFacts ?? {}
      const payloadShareable = analysisFacts.payloadShareable === true
      const counterThreadSafe = analysisFacts.counterThreadSafe === true
      const boundary = operation.crossDomain === true
      if (boundary && source === "existing" && state.rehomed !== true) {
        return reject(state, trace, "SHC0-REHOME-BEFORE-SHARED", index)
      }
      if (boundary && (!payloadShareable || !counterThreadSafe || !allOriginsCrossDomain || controlMobility !== "crossDomain")) {
        return reject(state, trace, "SHC0-CROSS-DOMAIN-FACTS", index, {
          payloadShareable,
          counterThreadSafe,
          allOriginsCrossDomain,
        })
      }

      state.sourceConsumed = true
      state.payloadOrigin = payloadOrigin
      state.payloadMobility = payloadMobility
      state.payloadShareable = payloadShareable
      state.controlBlockOrigin = controlOrigin
      state.controlBlockMobility = controlMobility
      state.counterThreadSafe = counterThreadSafe
      state.allOriginsCrossDomain = allOriginsCrossDomain
      state.analysisFacts = clone(analysisFacts)
      state.initializerThrows = initializerThrows
      state.allocatorFailure = allocatorFailure
      state.initializerError = initializerError
      state.allocationError = allocationErrorEdge
      state.errorEdges = errorEdges
      state.errorSet = Array.isArray(operation.errorSet) ? [...operation.errorSet] : []
      state.allocator = allocator
      state.allocatorScope = operation.allocatorScope ?? state.allocatorScope ?? null
      state.deallocator = profile?.deallocator ?? (allocator === "product.default" ? "product" : "lexical")
      state.controlBlockDeallocator = state.deallocator
      state.controlBlockLifetime = profile?.lifetime ?? (allocator === "lexical" ? "allocator-scope" : "product-default")
      state.controlBlockAdoptionFamily = profile?.adoptionFamily ?? "shared-control"
      state.controlBlockBulkReleaseOwner = profile?.bulkReleaseOwner ?? null
      state.controlBlockAllocatorContract = profile?.providerDigest ?? allocator
      state.controlBlockInstance = state.allocatorScope ?? allocator
      state.allocationOriginMap = {
        "$storage": payloadOrigin,
        "$controlBlock": controlOrigin,
        reachable: clone(originMap.reachable ?? originMap["$reachable"] ?? {}),
        controlBlock: {
          origin: controlOrigin,
          allocatorContract: state.controlBlockAllocatorContract,
          instance: state.controlBlockInstance,
          deallocator: state.controlBlockDeallocator,
          mobility: state.controlBlockMobility,
          lifetime: state.controlBlockLifetime,
          adoptionFamily: state.controlBlockAdoptionFamily,
          bulkReleaseOwner: state.controlBlockBulkReleaseOwner,
        },
      }
      trace.push({ operation: "consume-source", source, take: source === "existing" ? true : null })
      trace.push({
        operation: "prove-construction-facts",
        lifetimeIndependent: true,
        origins,
        controlBlockMobility: controlMobility,
        payloadShareable,
        counterThreadSafe,
        initializerThrows,
        allocatorFailure,
        initializerError: state.initializerError,
        allocationError: state.allocationError,
        try: syntaxTry,
      })
      // The optimizer may coallocate or reorder physical reserves. Only the
      // publish boundary and exactly-once cleanup are observable.
      trace.push({ operation: "stage-payload-and-control", physicalOrder: "unspecified" })
      const payloadReserveFailure = allocationError(operation.payloadReserve ?? operation.injectAllocationError)
      if (payloadReserveFailure) {
        trace.push({ operation: "cleanup", payload: 0, partialControlBlock: 0, exactlyOnce: true })
        const status = runtimeFailureStatus("allocation", allocatorFailure)
        const finish = status === "fault" ? faultOutcome : errorOutcome
        return finish(state, trace, "SHC0-RESERVE-FAILURE", index, {
          sourceConsumed: true,
          cleanup: { payloadDropCount: 0, partialControlBlockDropCount: 0 },
          failure: payloadReserveFailure,
          failureKind: status === "fault" ? "normal-oom" : "AllocationError",
          errorEdges,
          published: false,
        })
      }
      trace.push({ operation: "initialize-staged-values" })
      const payloadFailure = allocationError(operation.payloadInit ?? operation.injectInitializerError)
      if (payloadFailure) {
        trace.push({ operation: "cleanup", payload: 1, partialControlBlock: 0, exactlyOnce: true })
        state.payloadDeinitCount = 1
        return errorOutcome(state, trace, "SHC0-PAYLOAD-INIT-FAILURE", index, {
          sourceConsumed: true,
          cleanup: { payloadDropCount: 1, partialControlBlockDropCount: 0 },
          failure: payloadFailure,
          failureKind: "InitializerError",
          errorEdges,
          published: false,
        })
      }
      state.payloadInitialized = true
      const controlReserveFailure = allocationError(operation.controlReserve)
      if (controlReserveFailure) {
        trace.push({ operation: "cleanup", payload: 1, partialControlBlock: 0, exactlyOnce: true })
        state.payloadDeinitCount = 1
        const status = runtimeFailureStatus("allocation", allocatorFailure)
        const finish = status === "fault" ? faultOutcome : errorOutcome
        return finish(state, trace, "SHC0-CONTROL-RESERVE-FAILURE", index, {
          sourceConsumed: true,
          cleanup: { payloadDropCount: 1, partialControlBlockDropCount: 0 },
          failure: controlReserveFailure,
          failureKind: status === "fault" ? "normal-oom" : "AllocationError",
          errorEdges,
          published: false,
        })
      }
      const controlInitFailure = allocationError(operation.controlInit)
      if (controlInitFailure) {
        trace.push({ operation: "cleanup", payload: 1, partialControlBlock: 1, exactlyOnce: true })
        state.payloadDeinitCount = 1
        state.controlBlockDeinitCount = 1
        const status = runtimeFailureStatus("allocation", allocatorFailure)
        const finish = status === "fault" ? faultOutcome : errorOutcome
        return finish(state, trace, "SHC0-CONTROL-INIT-FAILURE", index, {
          sourceConsumed: true,
          cleanup: { payloadDropCount: 1, partialControlBlockDropCount: 1 },
          failure: controlInitFailure,
          failureKind: status === "fault" ? "normal-oom" : "AllocationError",
          errorEdges,
          published: false,
        })
      }
      trace.push({ operation: "initialize-counts", strong: 1, weak: 0 })
      state.payloadAlive = true
      state.controlBlockAlive = true
      state.published = true
      state.strong = 1
      trace.push({ operation: "publish-atomic", published: true })
      continue
    }

    if (operation.op === "callPromotion" || operation.op === "returnPromotion") {
      return reject(state, trace, "SHC0-CONTEXT-PROMOTION", index)
    }
    if (operation.op === "nestedField") {
      if (operation.inheritAllocator === true) return reject(state, trace, "SHC0-ALLOCATOR-PROPAGATION", index)
      trace.push({ operation: "nested-field-origin-independent", allocator: operation.allocator ?? "field-default" })
      continue
    }
    if (operation.op === "coallocate") {
      state.coallocationPromised = operation.promised === true
      if (state.coallocationPromised) return reject(state, trace, "SHC0-COALLOCATION-PROMISE", index)
      trace.push({ operation: "coallocation", promised: false, observableLayout: false })
      continue
    }
    if (operation.op === "makeWeak") {
      if (!state.published || !state.controlBlockAlive || state.strong === 0) return reject(state, trace, "SHC0-NO-PUBLISHED-OWNER", index)
      state.weak += 1
      trace.push({ operation: "retain-weak", strong: state.strong, weak: state.weak })
      continue
    }
    if (operation.op === "weakRead") {
      if (!state.controlBlockAlive) return reject(state, trace, "SHC0-WEAK-BLOCK-DEAD", index)
      const live = state.payloadAlive
      trace.push({ operation: "weak-read", result: live ? "some" : "none", resurrected: false })
      continue
    }
    if (operation.op === "weakAcquire") {
      if (!state.controlBlockAlive) return reject(state, trace, "SHC0-WEAK-BLOCK-DEAD", index)
      if (!state.payloadAlive || state.strong === 0) {
        trace.push({ operation: "weak-acquire", result: "none", resurrected: false, strong: state.strong })
        continue
      }
      state.strong += 1
      trace.push({ operation: "weak-acquire", result: "some", owner: true, strong: state.strong })
      continue
    }
    if (operation.op === "strongDrop") {
      if (!state.published || state.strong === 0) return reject(state, trace, "SHC0-STRONG-UNDERFLOW", index)
      state.strong -= 1
      if (state.strong === 0 && state.payloadAlive) {
        state.payloadAlive = false
        state.payloadDeinitCount += 1
        trace.push({ operation: "strong-zero", payloadDeinitCount: state.payloadDeinitCount })
      }
      if (state.strong === 0 && state.weak === 0 && state.controlBlockAlive) {
        state.controlBlockAlive = false
        state.controlBlockDeinitCount += 1
        trace.push({ operation: "strong-zero-reclaim-block", controlBlockDeinitCount: state.controlBlockDeinitCount })
      }
      continue
    }
    if (operation.op === "weakDrop") {
      if (state.weak === 0) return reject(state, trace, "SHC0-WEAK-UNDERFLOW", index)
      state.weak -= 1
      if (state.weak === 0 && state.strong === 0 && state.controlBlockAlive) {
        state.controlBlockAlive = false
        state.controlBlockDeinitCount += 1
        trace.push({ operation: "weak-zero-reclaim-block", controlBlockDeinitCount: state.controlBlockDeinitCount })
      } else if (state.weak === 0) {
        trace.push({ operation: "weak-zero-retain-block", strong: state.strong })
      }
      continue
    }
    if (operation.op === "boundary") {
      if (!state.published) return reject(state, trace, "SHC0-NO-PUBLISHED-OWNER", index)
      if (hasCallerFact(operation)) return reject(state, trace, "SHC0-CALLER-FACT", index)
      if (state.controlBlockMobility !== "crossDomain" || state.payloadMobility !== "crossDomain" ||
          !state.payloadShareable || !state.counterThreadSafe || !state.allOriginsCrossDomain) {
        return reject(state, trace, "SHC0-LOCAL-ORIGIN-BOUNDARY", index, {
          payloadShareable: state.payloadShareable,
          counterThreadSafe: state.counterThreadSafe,
          allOriginsCrossDomain: state.allOriginsCrossDomain,
        })
      }
      trace.push({ operation: "cross-domain-boundary", accepted: true, derived: true })
      continue
    }
    if (operation.op === "rehomeShared") {
      return reject(state, trace, "SHC0-SHARED-REHOME", index)
    }
    if (operation.op === "allocatorClose") {
      return reject(state, trace, "SHC0-OUTER-LEASE-OWNERSHIP", index, { owner: "ASC0" })
    }
    if (operation.op === "ffiPersistent") {
      if (!state.published) return reject(state, trace, "SHC0-NO-PUBLISHED-OWNER", index)
      if (hasCallerFact(operation)) return reject(state, trace, "SHC0-CALLER-FACT", index)
      if (state.controlBlockMobility !== "crossDomain" || state.payloadMobility !== "crossDomain" ||
          !state.payloadShareable || !state.counterThreadSafe || !state.allOriginsCrossDomain) {
        return reject(state, trace, "SHC0-FFI-LOCAL-ORIGIN", index)
      }
      if (operation.pinned !== true || operation.lease !== true || operation.destroy !== true ||
          operation.unpin !== true || operation.reclaim !== true || operation.unregister !== true || operation.inFlightDrained !== true ||
          operation.order !== "unregister-before-drain-before-destroy") {
        return reject(state, trace, "SHC0-FFI-LEASE-FACTS", index)
      }
      state.ffiPinned = true
      state.ffiLease = true
      state.ffiUnregistered = true
      state.ffiInFlightDrained = true
      trace.push({
        operation: "ffi-persistent",
        pinned: true,
        lease: true,
        unregister: true,
        inFlightDrained: true,
        destroy: true,
        unpin: true,
        reclaim: true,
        order: "unregister-before-drain-before-destroy",
      })
      continue
    }
    if (operation.op === "cycle") {
      if (operation.closedStrong === true) return reject(state, trace, "SHC0-CLOSED-STRONG-CYCLE", index)
      if (operation.weakBreak !== true) return reject(state, trace, "SHC0-CYCLE-BREAK-REQUIRED", index)
      trace.push({ operation: "cycle", strongScc: false, weakBreak: true })
      continue
    }
    return reject(state, trace, "SHC0-UNKNOWN-OPERATION", index)
  }

  return accepted(state, trace, {
    published: state.published,
    strong: state.strong,
    weak: state.weak,
    payloadAlive: state.payloadAlive,
    blockAlive: state.controlBlockAlive,
    payloadDeinitCount: state.payloadDeinitCount,
    controlBlockDeinitCount: state.controlBlockDeinitCount,
    allocationOriginMap: state.allocationOriginMap,
    coallocationPromised: state.coallocationPromised,
    payloadShareable: state.payloadShareable,
    counterThreadSafe: state.counterThreadSafe,
    initializerThrows: state.initializerThrows,
    allocatorFailure: state.allocatorFailure,
    initializerError: state.initializerError,
    allocationError: state.allocationError,
    errorEdges: state.errorEdges,
    errorSet: state.errorSet,
    allOriginsCrossDomain: state.allOriginsCrossDomain,
    ffiPinned: state.ffiPinned,
    ffiLease: state.ffiLease,
    ffiUnregistered: state.ffiUnregistered,
    ffiInFlightDrained: state.ffiInFlightDrained,
  })
}
