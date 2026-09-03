// Pure design oracle for W-1517. This reducer evaluates the source contract;
// it does not allocate host memory and it does not claim compiler conformance.

export const STORAGE_CLASSES = new Set([
  "ssa/elided",
  "inline",
  "nativeStack",
  "taskFrame",
  "static",
  "threadLocal",
  "fixedLease",
  "providerLease",
  "foreign",
]);

export const EXPLAIN_KINDS = new Set([
  "fact",
  "decision",
  "estimate",
  "measurement",
  "unknown",
]);

const PROFILE_DYNAMIC = new Set([".allow", ".forbid"]);
const PROFILE_STORAGE = new Set([".infer"]);
const ALLOCATION_POLICIES = new Set([".allow", ".forbidDynamic"]);
const RETURN_CLASSES = new Set(["inline", "callerDestination", "rehome"]);

export class W1517ContractError extends Error {
  constructor(code, detail = {}) {
    super(code);
    this.code = code;
    this.detail = detail;
  }
}

function reject(code, detail = {}) {
  return { status: "rejected", code, ...detail };
}

function accept(facts = {}) {
  return { status: "accepted", facts };
}

function positiveSafeInteger(value) {
  return Number.isSafeInteger(value) && value > 0;
}

function nonNegativeSafeInteger(value) {
  return Number.isSafeInteger(value) && value >= 0;
}

function hasOwn(value, key) {
  return Object.prototype.hasOwnProperty.call(value ?? {}, key);
}

function validateProfile(profile) {
  if (profile === undefined) return null;
  if (!profile || typeof profile !== "object" || Array.isArray(profile)) {
    return reject("invalidMemoryProfile");
  }
  if (!hasOwn(profile, "dynamicAllocation") || !hasOwn(profile, "automaticStorage")) {
    return reject("memoryProfileFieldRequired");
  }
  if (!PROFILE_DYNAMIC.has(profile.dynamicAllocation)) {
    return reject("invalidMemoryProfile");
  }
  const storage = profile.automaticStorage;
  if (PROFILE_STORAGE.has(storage)) return null;
  if (!storage || typeof storage !== "object" || storage.kind !== ".stack") {
    return reject("invalidMemoryProfile");
  }
  if (!positiveSafeInteger(storage.maximumFrame) || !positiveSafeInteger(storage.maximumCallPath)) {
    return reject("invalidMemoryProfile");
  }
  if (storage.suspendedTaskFrames === true) return reject("profileStackRejectsTaskFrame");
  return null;
}

function validateModule(module, profile) {
  if (module === undefined) return null;
  if (!module || typeof module !== "object" || Array.isArray(module)) {
    return reject("invalidModuleContract");
  }
  if (hasOwn(module, "functionAnnotation")) return reject("functionAnnotationNotCurrent");
  if (module.workspaceRelaxed === true || module.relaxesProfile === true) {
    return reject("modulePolicyRelaxation");
  }
  if (module.allocation !== undefined && !ALLOCATION_POLICIES.has(module.allocation)) {
    if (module.allocation === ".forbid") return reject("ambiguousAllocationPolicy");
    return reject("invalidModuleAllocationPolicy");
  }
  if (module.storage !== undefined) {
    const storage = module.storage;
    if (!storage || typeof storage !== "object" || storage.kind !== ".stack") {
      return reject("invalidModuleStoragePolicy");
    }
    if (!positiveSafeInteger(storage.maximumFrame)) return reject("invalidModuleStoragePolicy");
    if (storage.maximumCallPath !== undefined && !positiveSafeInteger(storage.maximumCallPath)) {
      return reject("invalidModuleStoragePolicy");
    }
    if (storage.suspendedTaskFrames === true) return reject("profileStackRejectsTaskFrame");
  }
  if (profile?.dynamicAllocation === ".forbid" && module.allocation === ".allow") {
    return reject("modulePolicyRelaxation");
  }
  if (profile?.automaticStorage?.kind === ".stack" && module.storage?.kind === ".infer") {
    return reject("modulePolicyRelaxation");
  }
  return null;
}

function validateGeneralAllocator(input) {
  const value = input.generalAllocator;
  if (value === undefined) return null;
  if (value === ".none") {
    if (input.assumedPlacement !== undefined || input.assumedNoAllocation === true) {
      return reject("noneDoesNotProvePlacement");
    }
    return { generalAllocator: "absent", placement: "unknown" };
  }
  if (value === ".root") return reject("rootPlanNotCurrent");
  if (value === "no-allocation" || value === ".no-allocation") {
    return reject("genericNoAllocationSurface");
  }
  return null;
}

function validateBounded(plan) {
  if (!plan || !positiveSafeInteger(plan.budget)) return reject("invalidBoundedPlan");
  if (plan.backingCompatible !== true) return reject("boundedBackingIncompatible");
  if (!nonNegativeSafeInteger(plan.liveBytes) || !nonNegativeSafeInteger(plan.requestedBytes)) {
    return reject("invalidBoundedReceipt");
  }
  const next = plan.liveBytes + plan.requestedBytes;
  if (!Number.isSafeInteger(next)) return reject("boundedBudgetExceeded");
  if (next > plan.budget) return reject("boundedBudgetExceeded", { committedBytes: plan.liveBytes, requestedBytes: plan.requestedBytes });
  if (plan.deallocationBytes !== undefined &&
      (!nonNegativeSafeInteger(plan.deallocationBytes) || plan.deallocationBytes > next)) {
    return reject("invalidBoundedReceipt");
  }
  return {
    facts: {
      bounded: {
        budget: plan.budget,
        committedBytes: next,
        refundedBytes: plan.deallocationBytes ?? 0,
        closesNormally: plan.closesNormally !== false,
      },
    },
  };
}

function strictUnknown(input) {
  return input.recursion === "unknown" || input.indirectCalls === "unknown" ||
    input.generics === "unknown" || input.concurrency === "unknown";
}

function validateStack(plan, input, profile, module) {
  if (!plan || plan.kind !== ".stack") return reject("invalidStackPlan");
  if (!positiveSafeInteger(plan.capacity)) return reject("invalidStackPlan");
  if (plan.fallback === true || plan.fallbackRequested === true) return reject("stackFallbackForbidden");
  if (plan.physicalClass === "taskFrame" || input.physicalClass === "taskFrame" || input.suspended === true) {
    return reject("stackTaskFrameRejected");
  }
  if (input.detachedWork === true) return reject("stackDetachedWorkRejected");
  if (input.escapes === true || input.returnStorage === "calleeFrame") return reject("stackEscape");
  if (input.crossesSuspension === true) return reject("stackSuspensionRejected");
  if (input.abiWithoutSummary === true || input.ffiWithoutSummary === true) {
    return reject("stackSummaryRequired");
  }
  if (strictUnknown(input)) return reject("strictSummaryUnknown");
  if (input.concurrencyCount !== undefined) {
    if (!positiveSafeInteger(input.concurrencyCount)) return reject("invalidConcurrencyBound");
    if (!nonNegativeSafeInteger(input.frameBytes)) return reject("invalidStackReceipt");
    const total = input.frameBytes * input.concurrencyCount;
    if (!Number.isSafeInteger(total) || total > plan.capacity) {
      return reject("concurrencyBoundExceeded");
    }
  }
  if (!positiveSafeInteger(plan.alignment) || !hasOwn(plan, "target") || plan.target === "") {
    return reject("stackProofUnknown");
  }
  if (plan.guardProbing !== true || plan.callPathAdmission !== true) {
    return reject("stackProofUnknown");
  }
  if (input.frameBytes !== undefined && (!nonNegativeSafeInteger(input.frameBytes) || input.frameBytes > plan.capacity)) {
    return reject("stackCapacityExceeded");
  }
  if (profile?.automaticStorage?.kind === ".stack") {
    if (input.frameBytes !== undefined && input.frameBytes > profile.automaticStorage.maximumFrame) {
      return reject("profileFrameExceeded");
    }
    if (input.callPathBytes !== undefined && input.callPathBytes > profile.automaticStorage.maximumCallPath) {
      return reject("profileCallPathExceeded");
    }
  }
  if (module?.storage?.kind === ".stack" && input.frameBytes !== undefined &&
      input.frameBytes > module.storage.maximumFrame) {
    return reject("moduleFrameExceeded");
  }
  if (input.dynamicAdmission === true && input.tryHandlesAdmission !== true) {
    return reject("tryMustHandleAdmission");
  }
  return {
    facts: {
      stack: {
        capacity: plan.capacity,
        alignment: plan.alignment,
        target: plan.target,
        physicalClass: "nativeStack",
      },
    },
  };
}

function validateExplain(input) {
  if (!input.explainKinds) return null;
  if (!Array.isArray(input.explainKinds) || input.explainKinds.length === 0) {
    return reject("invalidExplainKinds");
  }
  for (const kind of input.explainKinds) {
    if (!EXPLAIN_KINDS.has(kind)) return reject("invalidExplainKind");
    if (kind === "unknown" && input.strict === true) return reject("explainUnknownInStrict");
  }
  return { explainKinds: [...input.explainKinds] };
}

function validateStorageFact(input) {
  if (input.storageClass === undefined) return null;
  if (!STORAGE_CLASSES.has(input.storageClass)) return reject("unknownStorageClass");
  if (input.storageClass === "nativeStack" && input.logicalAllocationEffect === true) {
    return reject("storageEffectAxesConflated");
  }
  if (input.storageClass === "taskFrame" && input.strictStack === true) {
    return reject("stackTaskFrameRejected");
  }
  if (input.spill === true && input.spillReceipt !== "stack") return reject("spillReceiptRequired");
  const mapping = {
    "nativeStack": ["memref.alloca", "llvm.alloca"],
    "providerLease": ["memref.alloc", "runtime"],
    "static": ["global"],
    "taskFrame": ["coroutine", "runtime"],
  };
  if (input.mlirLowering !== undefined) {
    const expected = mapping[input.storageClass];
    if (!expected || !expected.includes(input.mlirLowering)) return reject("mlirStorageClassMismatch");
  }
  return { storageClass: input.storageClass, ...(input.spill ? { spillReceipt: "stack" } : {}) };
}

function validateReturn(input) {
  if (input.returnStorage === undefined) return null;
  if (input.returnStorage === "calleeFrame") return reject("calleeFrameEscape");
  if (!RETURN_CLASSES.has(input.returnStorage)) return reject("invalidReturnStorage");
  return { returnStorage: input.returnStorage };
}

export function evaluateMemoryContract(input = {}) {
  if (!input || typeof input !== "object" || Array.isArray(input)) return reject("invalidInput");
  if (hasOwn(input, "functionAnnotation")) return reject("functionAnnotationNotCurrent");
  if (input.benchmark?.timing === true || input.benchmark?.result === true) {
    return reject("benchmarkClaimForbidden");
  }
  const profileError = validateProfile(input.profile);
  if (profileError) return profileError;
  const moduleError = validateModule(input.module, input.profile);
  if (moduleError) return moduleError;
  const general = validateGeneralAllocator(input);
  if (general?.status === "rejected") return general;
  const explain = validateExplain(input);
  if (explain?.status === "rejected") return explain;
  const storageFact = validateStorageFact(input);
  if (storageFact?.status === "rejected") return storageFact;
  const returned = validateReturn(input);
  if (returned?.status === "rejected") return returned;

  const facts = {
    ...(general ?? {}),
    ...(explain ?? {}),
    ...(storageFact ?? {}),
    ...(returned ?? {}),
  };
  if (input.allocationPolicy !== undefined) {
    if (input.allocationPolicy === ".forbid") return reject("ambiguousAllocationPolicy");
    if (!ALLOCATION_POLICIES.has(input.allocationPolicy)) return reject("invalidAllocationPolicy");
    facts.allocationPolicy = input.allocationPolicy;
  }
  if (input.allocatorPlan?.kind === ".bounded") {
    const bounded = validateBounded(input.allocatorPlan);
    if (bounded.status === "rejected") return bounded;
    Object.assign(facts, bounded.facts);
  } else if (input.allocatorPlan?.kind === ".stack") {
    const stack = validateStack(input.allocatorPlan, input, input.profile, input.module);
    if (stack.status === "rejected") return stack;
    Object.assign(facts, stack.facts);
  } else if (input.allocatorPlan !== undefined) {
    return reject("invalidAllocatorPlan");
  }
  if (input.functionSummary === "inferred") facts.functionSummary = "inferred";
  if (input.functionSummary === "provider-conformant") return reject("implementationClaimForbidden");
  return accept(facts);
}

export function runW1517Case(testCase) {
  return evaluateMemoryContract(testCase?.input ?? {});
}
