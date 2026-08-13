import fs from "node:fs";
import path from "node:path";

const AXES = new Set(["A", "B", "C"]);
const CURRENT_A_ROUTES = new Set(["scalar-packing", "snapshot-cell", "lock-domain"]);
const VALUE_OPERATIONS = new Set(["load", "store", "exchange", "compareExchange"]);
const FIELD_KINDS = new Map([
  ["enum", { copy: true, lifetimeIndependent: true, dropFree: true, bits: (field) => Number.isSafeInteger(field.caseCount) && field.caseCount >= 1 && field.caseCount <= 0x100000000 ? Math.ceil(Math.log2(field.caseCount)) : undefined }],
  ["bool", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 1 }],
  ["u8", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 8 }],
  ["u16", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 16 }],
  ["u32", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 32 }],
  ["u64", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 64 }],
  ["u128", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 128 }],
  ["i8", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 8 }],
  ["i16", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 16 }],
  ["i32", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 32 }],
  ["i64", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 64 }],
  ["i128", { copy: true, lifetimeIndependent: true, dropFree: true, bits: 128 }],
  ["usize", { copy: true, lifetimeIndependent: true, dropFree: true }],
  ["isize", { copy: true, lifetimeIndependent: true, dropFree: true }],
  ["pointer", { copy: true, lifetimeIndependent: false, dropFree: true, pointer: true }],
  ["owner", { copy: false, lifetimeIndependent: false, dropFree: false, owner: true }],
  ["borrow", { copy: false, lifetimeIndependent: false, dropFree: true, borrow: true }],
  ["view", { copy: false, lifetimeIndependent: false, dropFree: true, view: true }],
  ["allocator-origin", { copy: false, lifetimeIndependent: false, dropFree: false, allocatorOrigin: true }],
  ["drop", { copy: false, lifetimeIndependent: false, dropFree: false, drop: true }],
]);
const ORDER_RANK = new Map([
  ["relaxed", 0],
  ["acquire", 1],
  ["release", 1],
  ["acquireRelease", 2],
  ["sequential", 3],
]);
const ADAPTER_FIELDS = [
  "domain",
  "participants",
  "registration",
  "retire",
  "retiredBound",
  "quiescence",
  "deleterContext",
  "shutdown",
  "memoryOrders",
  "targetProgress",
  "faultBehavior",
  "ffiDrain",
  "foreignBoundary",
  "events",
];
const EVENT_NAMES = new Set([
  "register",
  "access",
  "readerExit",
  "unlink",
  "retire",
  "quiescence",
  "reclaim",
  "drop",
  "participantDrain",
  "unregister",
  "ffiUnregister",
  "inFlightDrain",
  "destroy",
  "unpin",
  "shutdown",
]);
const VALID_ORDERS = new Set(["relaxed", "acquire", "release", "acquireRelease", "sequential"]);
const VALID_PROGRESS = new Set(["profile-fact", "lock-free", "wait-free", "blocking"]);
const VALID_FAULT_BEHAVIORS = new Set(["typed-fault", "fault-boundary", "abort"]);
const VALID_FOREIGN_BOUNDARIES = new Set(["none", "persistent-callback"]);
const VALID_PROVIDER_KINDS = new Set(["none", "striped-lock", "parking-table", "allocator-lock", "service"]);

function result(testCase, status, code, details = {}) {
  return { caseId: testCase.id, axis: testCase.axis, status, code, ...details };
}

function validCompareOrders(orders) {
  if (!orders || !ORDER_RANK.has(orders.success) || !ORDER_RANK.has(orders.failure)) return false;
  if (["release", "acquireRelease"].includes(orders.failure)) return false;
  if (orders.success === "release" && orders.failure !== "relaxed") return false;
  return ORDER_RANK.get(orders.failure) <= ORDER_RANK.get(orders.success);
}

function deriveLogicalFacts(record) {
  if (!Array.isArray(record?.fields) || record.fields.length === 0) {
    return { error: "atomic-value-fields-missing" };
  }
  const names = new Set();
  const facts = { copy: true, lifetimeIndependent: true, dropFree: true };
  if (record.encoding !== undefined && !["compiler-synthesized-canonical", "canonical"].includes(record.encoding)) return { error: record.encoding === "custom" || record.encoding === "noninjective" ? "custom-encoding-noninvertible" : "encoding-schema-invalid" };
  for (const field of record.fields) {
    if (!field || typeof field.name !== "string" || field.name.trim() === "") return { error: "field-schema-invalid" };
    if (names.has(field.name)) return { error: "duplicate-field-name" };
    names.add(field.name);
    if (field.kind === "record") return { error: "nested-record-measurement-missing" };
    if (!FIELD_KINDS.has(field.kind)) return { error: "unknown-field-kind" };
    if (["usize", "isize"].includes(field.kind)) return { error: "target-sized-field-unsupported" };
    const kindFacts = FIELD_KINDS.get(field.kind);
    if (field.kind === "enum" && (!Number.isSafeInteger(field.caseCount) || field.caseCount < 1 || field.caseCount > 0x100000000 || field.payload === true || field.payloadKind !== undefined)) return { error: "enum-descriptor-invalid" };
    facts.copy &&= kindFacts.copy;
    facts.lifetimeIndependent &&= kindFacts.lifetimeIndependent;
    facts.dropFree &&= kindFacts.dropFree;
    for (const key of ["pointer", "owner", "borrow", "view", "allocatorOrigin", "drop"]) {
      if (kindFacts[key]) facts[key] = true;
    }
  }
  const bits = record.fields.reduce((sum, field) => sum + (typeof kindFactsBits(field) === "number" ? kindFactsBits(field) : 0), 0);
  return { facts, canonicalBits: bits, callerFactsIgnored: record.facts !== undefined };
}

function kindFactsBits(field) {
  const descriptor = FIELD_KINDS.get(field.kind);
  if (!descriptor) return undefined;
  return typeof descriptor.bits === "function" ? descriptor.bits(field) : descriptor.bits;
}

function validateRecordLayout(record) {
  if (!record || typeof record !== "object") return "record-schema-invalid";
  if (record.atomicEligibilityLayout === "raw") return "raw-layout-coupling-rejected";
  return undefined;
}

function validateAbiLayout(record) {
  const fields = ["sizeBytes", "alignmentBytes", "extentCount", "paddingBits", "canonical", "fullyInitialized", "bitwiseEquality"];
  const present = fields.filter((field) => record[field] !== undefined).length;
  if (present === 0) return undefined;
  if (present !== fields.length) return "layout-receipt-schema";
  if (!Number.isSafeInteger(record.sizeBytes) || record.sizeBytes <= 0) return "invalid-size";
  if (!Number.isSafeInteger(record.alignmentBytes) || record.alignmentBytes <= 0) return "invalid-alignment";
  if ((record.alignmentBytes & (record.alignmentBytes - 1)) !== 0) return "alignment-not-power-of-two";
  if (record.extentCount !== 1) return "multiple-atomic-extents";
  if (!Number.isSafeInteger(record.paddingBits) || record.paddingBits < 0) return "invalid-padding";
  if (typeof record.canonical !== "boolean" || typeof record.fullyInitialized !== "boolean" || typeof record.bitwiseEquality !== "boolean") return "layout-facts-invalid";
  return undefined;
}

function validateTarget(target) {
  if (!target || typeof target !== "object") return { error: "target-profile-missing" };
  const native = target.nativeAtomicWidthsBytes;
  const lockFree = target.lockFreeWidthsBytes;
  if (![native, lockFree].every((value) => Array.isArray(value) && value.every((item) => Number.isSafeInteger(item) && item > 0))) {
    return { error: "target-width-schema" };
  }
  if (new Set(native).size !== native.length || new Set(lockFree).size !== lockFree.length) return { error: "target-width-duplicate" };
  if (lockFree.some((width) => !native.includes(width))) return { error: "lockfree-width-not-native" };
  const fallback = target.fallbackCapability;
  if (!fallback || typeof fallback !== "object" || !VALID_PROVIDER_KINDS.has(fallback.kind)) return { error: "fallback-capability-missing" };
  for (const key of ["blocking", "taskSafe", "parking", "allocation"]) {
    if (typeof fallback[key] !== "boolean") return { error: "fallback-capability-schema" };
  }
  return { native, lockFree, fallback };
}

function validateContext(context) {
  const value = context ?? {};
  const keys = ["blockingAllowed", "taskSafe", "cooperativeWorker", "freestanding", "signalOrInterrupt"];
  for (const key of keys) if (value[key] !== undefined && typeof value[key] !== "boolean") return { error: "context-facts-invalid" };
  return {
    blockingAllowed: value.blockingAllowed ?? true,
    taskSafe: value.taskSafe ?? true,
    cooperativeWorker: value.cooperativeWorker ?? false,
    freestanding: value.freestanding ?? false,
    signalOrInterrupt: value.signalOrInterrupt ?? false,
  };
}

function evaluatePacking(testCase) {
  const packing = testCase.packing;
  if (!packing || packing.carrier !== "u64" || packing.stateBits !== 2 || packing.generationBits !== 32 || packing.totalBits !== 34) {
    return result(testCase, "rejected", "packing-schema-invalid");
  }
  if (packing.encode !== "canonical" || packing.decode !== "canonical") return result(testCase, "rejected", "packing-not-canonical");
  if (packing.invalidState !== "reject") return result(testCase, "rejected", "invalid-discriminant-policy");
  if (packing.generationOverflow !== "reject") return result(testCase, "rejected", "generation-overflow-policy");
  if (!Array.isArray(packing.examples) || packing.examples.length === 0) return result(testCase, "rejected", "packing-roundtrip-missing");
  for (const example of packing.examples) {
    if (!Number.isSafeInteger(example?.stateCode) || example.stateCode < 0 || example.stateCode >= 3 || !Number.isSafeInteger(example?.generation) || example.generation < 0 || example.generation > 0xffffffff || typeof example.encoded !== "string") return result(testCase, "rejected", "packing-roundtrip-schema");
    let encoded;
    try { encoded = BigInt(example.encoded); } catch { return result(testCase, "rejected", "packing-roundtrip-schema"); }
    const expected = (BigInt(example.generation) << 2n) | BigInt(example.stateCode);
    if (encoded !== expected || Number(encoded & 3n) !== example.stateCode || Number(encoded >> 2n) !== example.generation) return result(testCase, "rejected", "packing-roundtrip-invalid");
  }
  return result(testCase, "current-composition", "canonical-scalar-packing", {
    route: "scalar-packing",
    fields: ["state", "generation"],
    roundTrips: packing.examples.length,
  });
}

function fallbackCompatible(provider, context) {
  if (!provider || provider.kind === "none") return false;
  if (!context.blockingAllowed && provider.blocking) return false;
  if (context.taskSafe && !provider.taskSafe) return false;
  if (context.signalOrInterrupt || context.freestanding) {
    if (provider.blocking || provider.allocation || !provider.taskSafe) return false;
  }
  if (context.cooperativeWorker && (!provider.taskSafe || !provider.parking || provider.blocking)) return false;
  return true;
}

function evaluateA(testCase) {
  if (testCase.route === "scalar-packing") return evaluatePacking(testCase);
  if (testCase.route === "snapshot-cell" || testCase.route === "lock-domain") {
    if (testCase.route === "lock-domain" && testCase.composition?.domain !== "spawn<.apology>") {
      return result(testCase, "rejected", "domain-composition-missing");
    }
    return result(testCase, "current-composition", "existing-route", { route: testCase.route, proof: "composes-existing-contracts" });
  }
  if (testCase.route !== "derived-record") return result(testCase, "rejected", "unknown-axis-a-route");
  const logical = deriveLogicalFacts(testCase.record);
  if (logical.error) return result(testCase, "rejected", logical.error);
  const layoutError = validateRecordLayout(testCase.record);
  if (layoutError) return result(testCase, "rejected", layoutError);
  const abiLayoutError = validateAbiLayout(testCase.record);
  if (abiLayoutError) return result(testCase, "rejected", abiLayoutError);
  const facts = logical.facts;
  if (!facts.copy || !facts.lifetimeIndependent || !facts.dropFree) {
    return result(testCase, "rejected", facts.drop ? "drop-required" : "atomic-value-lifetime-or-provenance");
  }
  if (facts.pointer || facts.owner || facts.borrow || facts.view || facts.allocatorOrigin) return result(testCase, "rejected", "atomic-value-lifetime-or-provenance");
  if (!Number.isSafeInteger(logical.canonicalBits) || logical.canonicalBits < 0 || logical.canonicalBits > 128) return result(testCase, "rejected", "canonical-carrier-too-wide");
  if (logical.canonicalBits === 0) return result(testCase, "rejected", "canonical-carrier-zero-width");
  const operations = testCase.operations ?? [];
  if (operations.some((operation) => !VALUE_OPERATIONS.has(operation))) return result(testCase, "rejected", "generic-arithmetic-not-derived");
  if (operations.includes("compareExchange") && !validCompareOrders(testCase.orders)) return result(testCase, "rejected", "compare-exchange-order-pair");
  const targetResult = validateTarget(testCase.target);
  if (targetResult.error) return result(testCase, "rejected", targetResult.error);
  const context = validateContext(testCase.context);
  if (context.error) return result(testCase, "rejected", context.error);
  if (testCase.interface) {
    if (testCase.interface.semanticInterfaceKey !== undefined && testCase.interface.candidateKey !== undefined && testCase.interface.semanticInterfaceKey !== testCase.interface.candidateKey) return result(testCase, "rejected", "semantic-interface-drift");
    if (testCase.interface.directFFI === true) return result(testCase, "rejected", "ffi-direct-carrier-forbidden");
    if (testCase.interface.crossesWAbi === true && testCase.interface.wAbiKey !== testCase.interface.candidateAbiKey) return result(testCase, "rejected", "w-abi-carrier-drift");
  }
  const { native, lockFree, fallback } = targetResult;
  const carrierBytes = Math.ceil(logical.canonicalBits / 8);
  const carrierWidth = [1, 2, 4, 8, 16].find((width) => width >= carrierBytes);
  if (!carrierWidth) return result(testCase, "rejected", "canonical-carrier-too-wide");
  const supported = native.includes(carrierWidth);
  const isLockFree = lockFree.includes(carrierWidth);
  if (testCase.lockFreeRequested === true && !isLockFree) return result(testCase, "rejected", "target-lockfree-unavailable", { target: testCase.target.id });
  if (supported) {
    if (!isLockFree) {
      if (!fallbackCompatible(fallback, context)) return result(testCase, "rejected", "fallback-context-incompatible", { target: testCase.target.id });
      return result(testCase, "fallback-declared", "native-carrier-not-lockfree-fallback", { target: testCase.target.id, carrierBytes: carrierWidth, provider: fallback.kind, progress: "profile-fact", canonicalBits: logical.canonicalBits, logicalFacts: facts, callerFactsIgnored: logical.callerFactsIgnored });
    }
    const providerDigestChanged = testCase.interface?.providerDigest !== undefined && testCase.interface?.candidateProviderDigest !== undefined && testCase.interface.providerDigest !== testCase.interface.candidateProviderDigest;
    return result(testCase, "candidate-research", "derived-value-record", {
      target: testCase.target.id,
      lockFree: isLockFree,
      progress: "profile-lockfree",
      canonicalBits: logical.canonicalBits,
      carrierBytes: carrierWidth,
      operations,
      logicalFacts: facts,
      callerFactsIgnored: logical.callerFactsIgnored,
      providerDigestChanged,
      artifactEvidence: providerDigestChanged ? "recipe-runtime-closure" : undefined,
    });
  }
  if (!fallbackCompatible(fallback, context)) return result(testCase, "rejected", "fallback-context-incompatible", { target: testCase.target.id });
  return result(testCase, "fallback-declared", "declared-target-fallback", {
    target: testCase.target.id,
    canonicalBits: logical.canonicalBits,
    carrierBytes: carrierWidth,
    progress: "profile-fact",
    provider: fallback.kind,
    logicalFacts: facts,
    callerFactsIgnored: logical.callerFactsIgnored,
  });
}

function packHandle(handle) {
  if (!handle || !Number.isSafeInteger(handle.slot) || !Number.isSafeInteger(handle.generation) || handle.slot < 0 || handle.slot > 0xffffffff || handle.generation < 0 || handle.generation > 0xffffffff) return undefined;
  return (BigInt(handle.generation) << 32n) | BigInt(handle.slot);
}

function unpackHandle(packed) {
  if (typeof packed !== "string" && typeof packed !== "number" && typeof packed !== "bigint") return undefined;
  let value;
  try {
    value = typeof packed === "string" ? BigInt(packed) : BigInt(packed);
  } catch {
    return undefined;
  }
  if (value < 0n || value > 0xffffffffffffffffn) return undefined;
  return {
    slot: Number(value & 0xffffffffn),
    generation: Number((value >> 32n) & 0xffffffffn),
  };
}

function evaluateB(testCase) {
  if (testCase.route === "integer-handle") {
    const handle = testCase.handle;
    const table = testCase.ownerTable;
    if (!handle || handle.slotBits !== 32 || !Number.isSafeInteger(handle.generationBits) || handle.generationBits < 1 || handle.generationBits > 32 || handle.packedCarrier !== "u64") return result(testCase, "rejected", "handle-representation");
    if (!table?.present || !table.ownsPayload || !table.generationCheck || !table.dereferenceAfterCheck || !table.returnsOptional || table.sourceShape !== true || table.payloadType !== "Menu" || !["Optional<Menu>", "Bool"].includes(table.resolveResult)) return result(testCase, "rejected", "owner-table-generation-check-missing");
    const slots = new Map();
    const seenGenerations = new Map();
    const generationLimit = 2 ** handle.generationBits;
    let generationWrapped = false;
    let staleRejected = false;
    let dereferenceCount = 0;
    for (const operation of testCase.operations ?? []) {
      const current = slots.get(operation.slot);
      if (operation.op === "pack") {
        const packed = packHandle(operation.handle);
        let expected;
        try { expected = operation.packed === undefined ? undefined : BigInt(operation.packed); } catch { expected = undefined; }
        if (packed === undefined || expected === undefined || packed !== expected) return result(testCase, "rejected", "handle-pack-noncanonical");
      } else if (operation.op === "unpack") {
        const unpacked = unpackHandle(operation.packed);
        if (!unpacked || !operation.handle || unpacked.slot !== operation.handle.slot || unpacked.generation !== operation.handle.generation) return result(testCase, "rejected", "handle-unpack-noncanonical");
      } else if (operation.op === "install" || operation.op === "reuse") {
        const seen = seenGenerations.get(operation.slot) ?? new Set();
        if (operation.op === "reuse" && seen.has(operation.generation)) generationWrapped = true;
        if (!Number.isSafeInteger(operation.slot) || operation.slot < 0 || operation.slot > 0xffffffff || !Number.isSafeInteger(operation.generation) || operation.generation < 0 || operation.generation >= generationLimit) return result(testCase, "rejected", "generation-out-of-range");
        slots.set(operation.slot, { generation: operation.generation, live: true });
        seen.add(operation.generation);
        seenGenerations.set(operation.slot, seen);
      } else if (operation.op === "resolve") {
        if (generationWrapped || !current || !current.live || current.generation !== operation.generation) {
          staleRejected = true;
          continue;
        }
        dereferenceCount += 1;
      } else return result(testCase, "rejected", "unknown-handle-operation");
    }
    if (generationWrapped) return result(testCase, "research-blocker", "generation-wrap-alias", { dereferenceCount: 0, route: "integer-handle-requires-wider-generation-or-retirement" });
    return result(testCase, "handle-current", staleRejected ? "stale-generation" : "generation-checked", {
      dereferenceCount,
      dereference: false,
      staleRejected,
      route: "integer-handle-owner-table",
    });
  }
  if (testCase.route === "tagged-pointer") {
    const pointer = testCase.pointer ?? {};
    if (!pointer.cas) return result(testCase, "rejected", "pointer-cas-missing");
    const adapter = testCase.reclamation ?? {};
    const schema = validateAdapterSchema(adapter);
    if (!pointer.provenance || !pointer.lifetime || !pointer.abaProof) return result(testCase, "research-blocker", "pointer-proof-missing", { unsafeCoreRequired: true, safeWrapperPromotion: "unproven", missing: ["provenance", "lifetime", "abaProof"].filter((key) => !pointer[key]) });
    if (!schema.ok) return result(testCase, "research-blocker", "atomic-pair-insufficient", { missing: schema.missing, route: "unsafe-adapter-or-domain-service" });
    const events = deriveReclamationEvents(adapter);
    if (!events.ok) return result(testCase, "rejected", events.code, events.details);
    return result(testCase, "adapter-research", "unsafe-schema-complete", { route: "specialized-unsafe-adapter", unsafeCoreRequired: true, safeWrapperPromotion: "unproven" });
  }
  return result(testCase, "rejected", "unknown-axis-b-route");
}

function validateAdapterSchema(adapter) {
  const missing = ADAPTER_FIELDS.filter((field) => {
    const value = adapter?.[field];
    if (field === "participants") return !Array.isArray(value) || value.length === 0;
    if (field === "retiredBound") return !Number.isSafeInteger(value) || value <= 0;
    if (field === "memoryOrders") return !Array.isArray(value) || value.length === 0;
    if (field === "events") return !Array.isArray(value) || value.length === 0;
    if (field === "ffiDrain") return typeof value !== "boolean";
    return value === undefined || value === null || value === false || value === "";
  });
  if (missing.length > 0) return { ok: false, missing, code: missing.includes("ffiDrain") ? "foreign-drain-missing" : "reclamation-schema-incomplete" };
  if (adapter.participants.some((participant) => typeof participant !== "string" || participant.trim() === "")) return { ok: false, code: "participant-schema-invalid" };
  if (new Set(adapter.participants).size !== adapter.participants.length) return { ok: false, code: "duplicate-participant" };
  if (typeof adapter.domain !== "string" || adapter.domain.trim() === "") return { ok: false, code: "domain-schema-invalid" };
  if (adapter.registration !== true) return { ok: false, code: "registration-schema-invalid" };
  if (adapter.retire !== "after-unlink") return { ok: false, code: "retire-policy-invalid" };
  if (!["domain-barrier", "epoch", "hazard"].includes(adapter.quiescence)) return { ok: false, code: "quiescence-policy-invalid" };
  if (adapter.shutdown !== "drain") return { ok: false, code: "shutdown-policy-invalid" };
  if (typeof adapter.deleterContext !== "string" || adapter.deleterContext.trim() === "") return { ok: false, code: "deleter-context-schema-invalid" };
  if (adapter.deleterContext !== adapter.domain) return { ok: false, code: "wrong-deleter-domain" };
  if (!adapter.memoryOrders.every((order) => VALID_ORDERS.has(order))) return { ok: false, code: "memory-order-invalid" };
  if (!adapter.memoryOrders.includes("acquire") || !adapter.memoryOrders.includes("release")) return { ok: false, code: "memory-order-incoherent" };
  if (!VALID_PROGRESS.has(adapter.targetProgress)) return { ok: false, code: "target-progress-invalid" };
  if (!VALID_FAULT_BEHAVIORS.has(adapter.faultBehavior)) return { ok: false, code: "fault-behavior-invalid" };
  if (typeof adapter.ffiDrain !== "boolean") return { ok: false, code: "ffi-drain-schema" };
  if (!VALID_FOREIGN_BOUNDARIES.has(adapter.foreignBoundary)) return { ok: false, code: "foreign-boundary-invalid" };
  const ffiEvents = adapter.events.filter((event) => ["ffiUnregister", "inFlightDrain", "destroy", "unpin"].includes(event.op));
  if (adapter.foreignBoundary === "none" && (adapter.ffiDrain !== false || ffiEvents.length > 0)) return { ok: false, code: "foreign-boundary-events-forbidden" };
  if (adapter.foreignBoundary === "persistent-callback" && adapter.ffiDrain !== true) return { ok: false, code: "foreign-drain-missing" };
  if (adapter.events.some((event) => !event || typeof event !== "object" || !EVENT_NAMES.has(event.op))) return { ok: false, code: "event-name-invalid" };
  return { ok: true };
}

function deriveReclamationEvents(adapter) {
  const state = { registered: new Set(), drained: new Set(), readers: new Map(), linked: true, retired: 0, dropped: 0, reclaimed: 0, callbacks: 0, ffiUnregistered: false, inFlightDrained: false, destroyed: false, unpinned: false };
  let sawRetire = false;
  let sawQuiescence = false;
  let sawUnlink = false;
  let sawShutdown = false;
  let sawDrop = false;
  for (const rawEvent of adapter.events) {
    const op = rawEvent.op;
    const participant = rawEvent.participant;
    if (sawShutdown) return { ok: false, code: "event-after-shutdown" };
    if (op === "register") {
      if (!adapter.participants.includes(participant) || state.registered.has(participant)) return { ok: false, code: "event-registration-invalid" };
      state.registered.add(participant);
    } else if (op === "access") {
      if (!state.registered.has(participant)) return { ok: false, code: "access-before-register" };
      state.readers.set(participant, (state.readers.get(participant) ?? 0) + 1);
    } else if (op === "readerExit") {
      if ((state.readers.get(participant) ?? 0) === 0) return { ok: false, code: "reader-exit-underflow" };
      state.readers.set(participant, state.readers.get(participant) - 1);
    } else if (op === "unlink") {
      if (!state.linked || sawUnlink) return { ok: false, code: "unlink-double" };
      state.linked = false;
      sawUnlink = true;
    } else if (op === "retire") {
      if (!sawUnlink) return { ok: false, code: "retire-before-unlink" };
      if (sawRetire) return { ok: false, code: "retire-double" };
      sawRetire = true;
      state.retired += 1;
    } else if (op === "quiescence") {
      if (!sawRetire || [...state.readers.values()].some((count) => count !== 0) || sawQuiescence) return { ok: false, code: "quiescence-before-reader-drain" };
      sawQuiescence = true;
    } else if (op === "reclaim") {
      if (state.reclaimed > 0) return { ok: false, code: "reclaim-double" };
      if (!sawQuiescence || state.retired === 0) return { ok: false, code: "reclaim-before-quiescence" };
      if (state.dropped !== 1) return { ok: false, code: "reclaim-before-drop" };
      state.retired -= 1;
      state.reclaimed += 1;
    } else if (op === "drop") {
      if (sawDrop || !sawQuiescence || state.retired === 0) return { ok: false, code: "drop-double-or-before-quiescence" };
      sawDrop = true;
      state.dropped += 1;
    } else if (op === "participantDrain") {
      if (!adapter.participants.includes(participant) || !state.registered.has(participant) || (state.readers.get(participant) ?? 0) !== 0 || state.drained.has(participant)) return { ok: false, code: "participant-drain-with-readers" };
      state.drained.add(participant);
    } else if (op === "unregister") {
      if (!state.registered.has(participant) || !state.drained.has(participant) || (state.readers.get(participant) ?? 0) !== 0) return { ok: false, code: "unregister-before-drain" };
      state.registered.delete(participant);
    } else if (op === "ffiUnregister") {
      if (state.ffiUnregistered) return { ok: false, code: "ffi-unregister-double" };
      state.ffiUnregistered = true;
      state.callbacks = 1;
    } else if (op === "inFlightDrain") {
      if (!state.ffiUnregistered || state.inFlightDrained) return { ok: false, code: "ffi-drain-order-invalid" };
      state.callbacks = 0;
      state.inFlightDrained = true;
    } else if (op === "destroy") {
      if (!state.inFlightDrained || state.destroyed) return { ok: false, code: "destroy-before-ffi-drain" };
      state.destroyed = true;
    } else if (op === "unpin") {
      if (!state.destroyed || state.unpinned) return { ok: false, code: "unpin-before-destroy" };
      state.unpinned = true;
    } else if (op === "shutdown") {
      if (state.registered.size !== 0 || [...state.readers.values()].some((count) => count !== 0) || state.retired !== 0 || state.callbacks !== 0) return { ok: false, code: "shutdown-nonquiescent" };
      sawShutdown = true;
    }
  }
  if (state.retired !== 0) return { ok: false, code: "retired-not-reclaimed" };
  if (state.reclaimed !== 1) return { ok: false, code: "reclaim-count-invalid" };
  if (!sawUnlink || !sawRetire || !sawQuiescence || state.dropped !== 1 || state.registered.size !== 0 || state.drained.size !== adapter.participants.length || !sawShutdown) return { ok: false, code: "event-sequence-incomplete" };
  if (adapter.foreignBoundary === "persistent-callback" && (!state.ffiUnregistered || !state.inFlightDrained || !state.destroyed || !state.unpinned)) return { ok: false, code: "foreign-drain-sequence-incomplete" };
  return { ok: true, details: { events: adapter.events.length, dropped: state.dropped } };
}

function evaluateC(testCase) {
  if (testCase.route === "snapshot-cell" || testCase.route === "domain-service") {
    if (testCase.route === "snapshot-cell") {
      const payload = testCase.payload ?? {};
      if (!payload.transferable || !payload.shareable || !payload.lifetimeIndependent) return result(testCase, "rejected", "snapshot-payload-facts");
      if (testCase.retireCallback === true) return result(testCase, "rejected", "snapshot-retire-callback");
      if (!(testCase.operations ?? []).includes("read") || !(testCase.operations ?? []).includes("publish")) return result(testCase, "rejected", "snapshot-surface-incomplete");
      return result(testCase, "snapshot-current", "composes-snapshot-cell", { route: "SnapshotCell", customReclamationSurface: false });
    }
    return result(testCase, "domain-current", "composes-domain-or-service", { route: "domain-barrier-or-service", customReclamationSurface: false });
  }
  if (testCase.route !== "unsafe-adapter") return result(testCase, "rejected", "unknown-axis-c-route");
  const adapter = testCase.reclamation ?? {};
  if (adapter.registration === false) return result(testCase, "rejected", "registration-required");
  if (adapter.retire === "before-unlink") return result(testCase, "rejected", "retire-before-unlink");
  if (adapter.quiescence === "immediate-reclaim") return result(testCase, "rejected", "reclaim-before-quiescence");
  if (adapter.deleterContext && adapter.domain && adapter.deleterContext !== adapter.domain) return result(testCase, "rejected", "wrong-deleter-domain");
  if (adapter.shutdown === "stop-with-live-participants") return result(testCase, "rejected", "shutdown-nonquiescent");
  const schema = validateAdapterSchema(adapter);
  if (!schema.ok) return result(testCase, "rejected", schema.code, { missing: schema.missing });
  const events = deriveReclamationEvents(adapter);
  if (!events.ok) return result(testCase, "rejected", events.code, events.details);
  return result(testCase, "adapter-research", "unsafe-schema-complete", { route: "specialized-unsafe-adapter", unsafeCoreRequired: true, safeWrapperPromotion: "unproven" });
}

export function evaluateAtom1Case(testCase) {
  if (!testCase || !AXES.has(testCase.axis) || typeof testCase.id !== "string") throw new Error("ATOM1 case must have an axis and id");
  if (testCase.axis === "A") return evaluateA(testCase);
  if (testCase.axis === "B") return evaluateB(testCase);
  return evaluateC(testCase);
}

export function deriveAtom1(corpus) {
  return (corpus.cases ?? []).map(evaluateAtom1Case);
}

export function validateAtom1(corpus, { root } = {}) {
  const errors = [];
  if (corpus?.$schema !== "w-atom1-atomic-extensibility-cases-1") errors.push("ATOM1 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("ATOM1 corpus status must be design-oracle-input.");
  if (!Array.isArray(corpus?.cases) || corpus.cases.length < 20) {
    errors.push("ATOM1 corpus must contain at least 20 adversarial cases.");
    return { errors, results: [] };
  }
  const ids = new Set();
  for (const testCase of corpus.cases) {
    if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}`);
    ids.add(testCase.id);
    if (!AXES.has(testCase.axis)) errors.push(`invalid axis for ${testCase.id}`);
    if (root && typeof testCase.restaurantSymbol === "string") {
      const sources = ["synchronization.w", "memory.w", "abi.w"].map((name) => path.join(root, "reference", "last-light", name));
      if (!sources.some((file) => fs.existsSync(file) && fs.readFileSync(file, "utf8").includes(testCase.restaurantSymbol))) errors.push(`${testCase.id} has no Last Light symbol ${testCase.restaurantSymbol}.`);
    }
  }
  const results = deriveAtom1(corpus);
  const resultById = new Map(results.map((item) => [item.caseId, item]));
  const required = [
    "A-sign-epoch-derived-x64", "A-sign-epoch-derived-fallback", "A-sign-epoch-padding", "A-sign-epoch-pointer-field", "A-sign-epoch-too-wide-lockfree", "A-sign-epoch-misaligned", "A-sign-epoch-fetch-arithmetic", "A-sign-epoch-invalid-order", "A-sign-epoch-interface-drift", "A-sign-epoch-uninitialized", "A-sign-epoch-drop-field", "A-sign-epoch-unknown-field", "A-sign-epoch-forged-facts", "A-sign-epoch-scalar-invalid-state", "A-sign-epoch-scalar-generation-overflow", "A-sign-epoch-fallback-cooperative-blocking", "A-sign-epoch-fallback-parking-safe", "A-sign-epoch-lockfree-fallback-forbidden", "A-sign-epoch-fallback-signal-blocking", "A-sign-epoch-fallback-freestanding-blocking", "A-sign-epoch-raw-layout-candidate", "A-sign-epoch-enum-payload", "A-sign-epoch-custom-encoding", "A-sign-epoch-native-carrier-not-lockfree", "A-sign-epoch-semantic-contract-drift", "A-sign-epoch-wabi-carrier-drift", "A-sign-epoch-provider-digest-change", "A-sign-epoch-ffi-direct-carrier", "A-sign-epoch-layout-receipt-ignored", "A-sign-epoch-bool-bit", "A-sign-epoch-target-sized-field", "A-sign-epoch-wide96-lockfree", "A-sign-epoch-wide96-fallback", "A-sign-epoch-wide128-lockfree", "A-sign-epoch-wide128-lockfree-forbidden", "A-sign-epoch-zero-bit-enum",
    "B-menu-handle-generation", "B-menu-handle-stale-generation", "B-menu-handle-generation-wrap", "B-tagged-pointer-cas-without-reclamation", "B-tagged-pointer-specialized-adapter", "B-tagged-pointer-provenance-receipts",
    "C-menu-snapshot-cell", "C-reclamation-missing-registration", "C-reclamation-before-unlink", "C-reclamation-before-quiescence", "C-reclamation-unbounded-retired", "C-reclamation-wrong-deleter-domain", "C-reclamation-live-shutdown", "C-reclamation-missing-ffi-drain", "C-reclamation-complete-unsafe-schema", "C-reclamation-complete-ffi-schema", "C-reclamation-none-with-ffi-events", "C-reclamation-order-swapped", "C-reclamation-duplicate-participant", "C-reclamation-invalid-order", "C-reclamation-invalid-progress", "C-reclamation-invalid-fault", "C-reclamation-double-reclaim", "C-reclamation-double-drop", "C-reclamation-shutdown-retired", "C-reclamation-shutdown-reader", "C-reclamation-shutdown-callback", "C-reclamation-cross-participant-reader-exit",
  ];
  for (const id of required) if (!resultById.has(id)) errors.push(`ATOM1 required case missing: ${id}`);
  if (resultById.get("A-sign-epoch-derived-x64")?.status !== "candidate-research") errors.push("A derived x64 must remain Research.");
  if (resultById.get("A-sign-epoch-derived-fallback")?.status !== "fallback-declared") errors.push("A fallback must remain declared fallback.");
  if (resultById.get("B-menu-handle-stale-generation")?.code !== "stale-generation") errors.push("B stale handle must reject before dereference.");
  if (resultById.get("B-menu-handle-generation")?.dereferenceCount !== 1) errors.push("B valid handle must dereference exactly once.");
  if (resultById.get("B-menu-handle-stale-generation")?.dereferenceCount !== 0) errors.push("B stale handle must dereference zero times.");
  if (resultById.get("B-menu-handle-generation-wrap")?.dereferenceCount !== 0) errors.push("B wrapped generation must dereference zero times.");
  if (resultById.get("B-tagged-pointer-specialized-adapter")?.code !== "pointer-proof-missing") errors.push("B tagged pointer without provenance proofs must remain blocked.");
  if (resultById.get("B-tagged-pointer-provenance-receipts")?.status !== "adapter-research" || resultById.get("B-tagged-pointer-provenance-receipts")?.safeWrapperPromotion !== "unproven") errors.push("B complete pointer receipts must remain unsafe adapter Research.");
  if (resultById.get("C-menu-snapshot-cell")?.status !== "snapshot-current") errors.push("C SnapshotCell must remain the current route.");
  if (resultById.get("C-reclamation-complete-unsafe-schema")?.status !== "adapter-research") errors.push("C complete reclamation schema must remain unsafe adapter Research.");
  return { errors, results };
}
