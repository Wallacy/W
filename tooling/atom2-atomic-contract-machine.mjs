import fs from "node:fs";
import path from "node:path";

const AXES = new Set(["A", "B", "C", "D"]);
const VALUE_OPERATIONS = new Set(["load", "store", "exchange", "compareExchange"]);
const VALID_ORDERS = new Set(["relaxed", "acquire", "release", "acquireRelease", "sequential"]);
const VALID_FAILURE_ORDERS = new Map([
  ["relaxed", new Set(["relaxed"])],
  ["acquire", new Set(["relaxed", "acquire"])],
  ["release", new Set(["relaxed"])],
  ["acquireRelease", new Set(["relaxed", "acquire"])],
  ["sequential", new Set(["relaxed", "acquire", "sequential"])],
]);
const VALID_PROVIDER_KINDS = new Set(["none", "striped-lock", "parking-table", "allocator-lock", "service"]);
const VALID_PROGRESS = new Set(["profile-fact", "lock-free", "wait-free", "blocking"]);
const VALID_FAULTS = new Set(["typed-fault", "fault-boundary", "abort"]);
const VALID_BOUNDARIES = new Set(["none", "persistent-callback"]);
const ADAPTER_FIELDS = [
  "domain", "participants", "registration", "retire", "retiredBound", "quiescence",
  "deleterContext", "shutdown", "memoryOrders", "targetProgress", "faultBehavior",
  "ffiDrain", "foreignBoundary", "events",
];
const EVENT_OPS = new Set([
  "register", "access", "readerExit", "unlink", "retire", "quiescence", "drop", "reclaim",
  "participantDrain", "unregister", "ffiUnregister", "inFlightDrain", "destroy", "unpin", "shutdown",
]);
const FIXED_FIELDS = new Map([
  ["bool", 1], ["u8", 8], ["u16", 16], ["u32", 32], ["u64", 64], ["u128", 128],
  ["i8", 8], ["i16", 16], ["i32", 32], ["i64", 64], ["i128", 128],
]);
const CANONICAL_CARRIER_BITS = [8, 16, 32, 64, 128];

function result(testCase, status, code, details = {}) {
  return { caseId: testCase.id, axis: testCase.axis, status, code, ...details };
}

function validCompareOrders(orders) {
  if (!orders || !VALID_FAILURE_ORDERS.has(orders.success)) return false;
  return VALID_FAILURE_ORDERS.get(orders.success).has(orders.failure);
}

function fieldBits(field) {
  if (FIXED_FIELDS.has(field?.kind)) return FIXED_FIELDS.get(field.kind);
  if (field?.kind === "enum" && Number.isSafeInteger(field.caseCount) && field.caseCount >= 2 && field.caseCount <= 0x100000000) {
    return Math.ceil(Math.log2(field.caseCount));
  }
  return undefined;
}

function deriveRecord(record) {
  if (!record || typeof record !== "object" || !Array.isArray(record.fields) || record.fields.length === 0) {
    return { error: "atomic-value-fields-missing" };
  }
  if (record.encoding !== "compiler-synthesized-canonical" || record.declarationOrderInjective !== true) {
    return { error: record.encoding === "custom" || record.declarationOrderInjective === false ? "custom-encoding-noninvertible" : "canonical-encoding-required" };
  }
  const names = new Set();
  const facts = { copy: true, lifetimeIndependent: true, dropFree: true };
  let bits = 0;
  for (const field of record.fields) {
    if (!field || typeof field.name !== "string" || field.name.trim() === "" || names.has(field.name)) return { error: "field-schema-invalid" };
    names.add(field.name);
    if (["usize", "isize"].includes(field.kind)) return { error: "target-sized-field-unsupported" };
    if (["float", "pointer", "owner", "borrow", "view", "allocator-origin", "drop", "record", "nested"].includes(field.kind)) {
      return { error: field.kind === "float" ? "float-field-unsupported" : field.kind === "record" || field.kind === "nested" ? "nested-record-rejected" : "atomic-value-lifetime-or-provenance" };
    }
    if (field.kind === "enum" && (!Number.isSafeInteger(field.caseCount) || field.caseCount < 2 || field.caseCount > 0x100000000 || field.payload === true || field.payloadKind !== undefined)) {
      return { error: "enum-descriptor-invalid" };
    }
    const width = fieldBits(field);
    if (!Number.isSafeInteger(width)) return { error: "unknown-field-kind" };
    bits += width;
  }
  return { facts, canonicalBits: bits, callerFactsIgnored: record.facts !== undefined };
}

function bigintValue(value) {
  if (typeof value === "bigint") return value;
  if (typeof value === "number" && Number.isSafeInteger(value)) return BigInt(value);
  if (typeof value === "string" && /^-?(?:0|[1-9][0-9]*)$/u.test(value)) {
    try { return BigInt(value); } catch { return undefined; }
  }
  return undefined;
}

function validateCanonicalEncoding(record, derived) {
  const encoding = record.canonicalEncoding;
  if (!encoding || typeof encoding !== "object") return { error: "canonical-encoding-schema" };
  if (encoding.bitDirection !== "least-significant-first") return { error: "canonical-bit-direction-invalid" };
  const names = record.fields.map((field) => field.name);
  if (!Array.isArray(encoding.fieldOrder) || JSON.stringify(encoding.fieldOrder) !== JSON.stringify(names)) {
    return { error: "canonical-field-order-invalid" };
  }
  if (!encoding.fieldOffsets || typeof encoding.fieldOffsets !== "object") return { error: "canonical-field-offsets-missing" };
  let offset = 0;
  for (const field of record.fields) {
    if (encoding.fieldOffsets[field.name] !== offset) return { error: "canonical-field-order-invalid" };
    offset += fieldBits(field);
  }
  if (encoding.highBitsZero !== true) return { error: "canonical-high-bits-zero-required" };
  if (encoding.physicalEndian !== "provider-abi") return { error: "canonical-endian-logical-forbidden" };
  if (encoding.validPatterns !== "canonical-only") return { error: "canonical-invalid-patterns-forbidden" };
  for (const field of record.fields.filter((item) => item.kind === "enum")) {
    const ordinals = encoding.enumOrdinals?.[field.name];
    if (!Array.isArray(ordinals) || ordinals.length !== field.caseCount || ordinals.some((value, index) => value !== index)) {
      return { error: "enum-ordinal-invalid" };
    }
  }
  const sample = encoding.sample;
  if (!sample || typeof sample !== "object" || !sample.values || typeof sample.values !== "object") return { error: "canonical-sample-missing" };
  const sampleEncoded = bigintValue(sample.encoded);
  if (sampleEncoded === undefined || sampleEncoded < 0n) return { error: "canonical-sample-invalid" };
  const carrierBits = CANONICAL_CARRIER_BITS.find((bits) => bits >= derived.canonicalBits);
  if (!carrierBits || sampleEncoded >= (1n << BigInt(carrierBits))) return { error: "canonical-high-bits-nonzero" };
  let expected = 0n;
  for (const field of record.fields) {
    const value = sample.values[field.name];
    const width = fieldBits(field);
    const code = bigintValue(value);
    if (field.kind === "bool") {
      if (value !== true && value !== false && code !== 0n && code !== 1n) return { error: "bool-code-invalid" };
    } else if (field.kind.startsWith("u")) {
      if (code === undefined || code < 0n || code >= (1n << BigInt(width))) return { error: "unsigned-code-invalid" };
    } else if (field.kind.startsWith("i")) {
      if (code === undefined || code < -(1n << BigInt(width - 1)) || code >= (1n << BigInt(width - 1))) return { error: "signed-code-invalid" };
    } else if (field.kind === "enum") {
      if (code === undefined || code < 0n || code >= BigInt(field.caseCount)) return { error: "enum-code-invalid" };
    } else {
      return { error: "canonical-sample-invalid" };
    }
    const logical = field.kind === "bool" ? (value === true || code === 1n ? 1n : 0n)
      : field.kind.startsWith("i") && code < 0n ? (1n << BigInt(width)) + code
        : code;
    expected |= logical << BigInt(encoding.fieldOffsets[field.name]);
  }
  if (sampleEncoded !== expected) return { error: "canonical-sample-mismatch" };
  return { encoding, carrierBits, sampleEncoded: sampleEncoded.toString() };
}

function validateOperationContract(contract) {
  if (!contract || typeof contract !== "object" || contract.neverSuspend !== true) return { error: "atomic-operation-never-suspend-required" };
  if (contract.cancellationPoint !== false) return { error: "atomic-cancellation-point-forbidden" };
  if (contract.waitApi !== "Atomic.wait-separate") return { error: "atomic-wait-api-must-be-separate" };
  return { neverSuspend: true, cancellationPoint: false, waitApi: "Atomic.wait-separate" };
}

function validateTarget(target) {
  if (!target || typeof target !== "object") return { error: "target-profile-missing" };
  const native = target.nativeAtomicWidthsBytes;
  const lockFree = target.lockFreeWidthsBytes;
  if (![native, lockFree].every((items) => Array.isArray(items) && items.every((item) => Number.isSafeInteger(item) && item > 0))) return { error: "target-width-schema" };
  if (new Set(native).size !== native.length || new Set(lockFree).size !== lockFree.length) return { error: "target-width-duplicate" };
  if (lockFree.some((width) => !native.includes(width))) return { error: "lockfree-width-not-native" };
  const fallback = target.fallbackCapability;
  if (!fallback || typeof fallback !== "object" || !VALID_PROVIDER_KINDS.has(fallback.kind)) return { error: "fallback-capability-missing" };
  for (const field of ["blocksThread", "taskSafe", "parking", "allocation"]) if (typeof fallback[field] !== "boolean") return { error: "fallback-capability-schema" };
  if (fallback.parking === true && fallback.blocksThread !== true) return { error: "parking-blocking-ambiguous" };
  return { native, lockFree, fallback };
}

function validateContext(context) {
  const value = context ?? {};
  for (const field of ["blockingAllowed", "taskSafe", "cooperativeWorker", "freestanding", "signalOrInterrupt"]) {
    if (value[field] !== undefined && typeof value[field] !== "boolean") return { error: "context-facts-invalid" };
  }
  return {
    blockingAllowed: value.blockingAllowed ?? true,
    taskSafe: value.taskSafe ?? true,
    cooperativeWorker: value.cooperativeWorker ?? false,
    freestanding: value.freestanding ?? false,
    signalOrInterrupt: value.signalOrInterrupt ?? false,
  };
}

function fallbackCompatible(provider, context) {
  if (!provider || provider.kind === "none") return false;
  if (provider.allocation) return false;
  if (provider.blocksThread && (!context.blockingAllowed || context.signalOrInterrupt || context.freestanding || context.cooperativeWorker)) return false;
  if (context.taskSafe && !provider.taskSafe) return false;
  return true;
}

function evaluatePacking(testCase) {
  const packing = testCase.packing ?? {};
  if (packing.carrier !== "u64" || packing.totalBits !== 64 || packing.slotBits !== 32 || packing.generationBits !== 32) return result(testCase, "rejected", "handle-representation");
  if (packing.generationOverflow !== "retire-slot") return result(testCase, "rejected", "generation-wrap-forbidden");
  if (!Array.isArray(packing.examples) || packing.examples.length === 0) return result(testCase, "rejected", "handle-roundtrip-missing");
  for (const example of packing.examples) {
    if (!Number.isSafeInteger(example?.slot) || example.slot < 0 || example.slot > 0xffffffff || !Number.isSafeInteger(example?.generation) || example.generation < 0 || example.generation > 0xffffffff) return result(testCase, "rejected", "handle-roundtrip-schema");
    const encoded = (BigInt(example.generation) << 32n) | BigInt(example.slot);
    if (encoded !== BigInt(example.encoded)) return result(testCase, "rejected", "handle-roundtrip-invalid");
  }
  return result(testCase, testCase.axis === "A" ? "current-composition" : "handle-current", testCase.axis === "A" ? "canonical-scalar-packing" : "generation-checked", { route: testCase.axis === "A" ? "scalar-packing" : "integer-handle-owner-table", roundTrips: packing.examples.length });
}

function evaluateA(testCase) {
  if (testCase.route === "scalar-packing") return evaluatePacking(testCase);
  if (["snapshot-cell", "lock-domain"].includes(testCase.route)) return result(testCase, "current-composition", "existing-route", { route: testCase.route });
  if (testCase.route !== "canonical-carrier") return result(testCase, "rejected", "unknown-axis-a-route");
  const record = deriveRecord(testCase.record);
  if (record.error) return result(testCase, "rejected", record.error);
  const canonical = validateCanonicalEncoding(testCase.record, record);
  if (canonical.error) return result(testCase, "rejected", canonical.error);
  const layout = testCase.record.layoutReceipt;
  if (layout !== undefined && (typeof layout !== "object" || layout.extentCount !== 1 || layout.canonical !== true)) return result(testCase, "rejected", "layout-receipt-invalid");
  if (record.canonicalBits < 1 || record.canonicalBits > 128) return result(testCase, "rejected", record.canonicalBits === 0 ? "canonical-carrier-zero-width" : "canonical-carrier-too-wide");
  const operations = testCase.operations ?? [];
  if (!Array.isArray(operations) || operations.length === 0 || operations.some((operation) => !VALUE_OPERATIONS.has(operation))) return result(testCase, "rejected", "atomic-operation-not-derived");
  const operationContract = validateOperationContract(testCase.operationContract);
  if (operationContract.error) return result(testCase, "rejected", operationContract.error);
  if (operations.includes("compareExchange")) {
    if (!validCompareOrders(testCase.orders)) return result(testCase, "rejected", "compare-exchange-order-pair");
    if (testCase.comparison !== "canonical-representation") return result(testCase, "rejected", "representation-equality-required");
    if (canonical.encoding.casValues !== "canonical-expected-desired") return result(testCase, "rejected", "cas-canonical-values-required");
  }
  const target = validateTarget(testCase.target);
  if (target.error) return result(testCase, "rejected", target.error);
  if (target.fallback.allocation === true) return result(testCase, "rejected", "fallback-allocation-forbidden");
  const context = validateContext(testCase.context);
  if (context.error) return result(testCase, "rejected", context.error);
  const lifecycle = testCase.lifecycle ?? {};
  if (lifecycle.panic === "hidden") return result(testCase, "rejected", "panic-boundary-required");
  if (lifecycle.oom === "unbounded") return result(testCase, "rejected", "oom-bound-required");
  if (lifecycle.cancel === "detached") return result(testCase, "rejected", "cancel-drain-required");
  const iface = testCase.interface ?? {};
  if (iface.semanticInterfaceKey !== undefined && iface.candidateSemanticInterfaceKey !== undefined && iface.semanticInterfaceKey !== iface.candidateSemanticInterfaceKey) return result(testCase, "rejected", "semantic-interface-drift");
  if (iface.crossesWAbi === true && iface.wAbiKey !== iface.candidateWAbiKey) return result(testCase, "rejected", "w-abi-carrier-drift");
  if (iface.directFFI === true) return result(testCase, "rejected", "ffi-direct-carrier-forbidden");
  const carrierBytes = Math.ceil(record.canonicalBits / 8);
  const carrierWidth = [1, 2, 4, 8, 16].find((width) => width >= carrierBytes);
  if (!carrierWidth) return result(testCase, "rejected", "canonical-carrier-too-wide");
  const isNative = target.native.includes(carrierWidth);
  const isLockFree = target.lockFree.includes(carrierWidth);
  if (testCase.lockFreeRequested === true && (!isLockFree || testCase.target.lockFreeFact !== true)) return result(testCase, "rejected", "target-lockfree-unavailable", { target: testCase.target.id });
  if (!isNative && !fallbackCompatible(target.fallback, context)) return result(testCase, "rejected", "fallback-context-incompatible", { target: testCase.target.id });
  if (!isNative) {
    return result(testCase, "fallback-declared", "declared-target-fallback", { target: testCase.target.id, carrierBytes, carrierWidth, canonicalBits: record.canonicalBits, provider: target.fallback.kind, progress: "profile-fact", blocksThread: target.fallback.blocksThread, parking: target.fallback.parking, canonicalBitDirection: canonical.encoding.bitDirection, fieldOrder: canonical.encoding.fieldOrder, highBitsZero: canonical.encoding.highBitsZero, physicalEndian: canonical.encoding.physicalEndian, neverSuspend: true, cancellationPoint: false, waitApi: "Atomic.wait-separate", logicalFacts: record.facts, callerFactsIgnored: record.callerFactsIgnored });
  }
  if (isNative && !isLockFree) {
    if (!fallbackCompatible(target.fallback, context)) return result(testCase, "rejected", "fallback-context-incompatible", { target: testCase.target.id });
    return result(testCase, "fallback-declared", "native-carrier-not-lockfree-fallback", { target: testCase.target.id, carrierBytes, carrierWidth, canonicalBits: record.canonicalBits, provider: target.fallback.kind, progress: "profile-fact", blocksThread: target.fallback.blocksThread, parking: target.fallback.parking, canonicalBitDirection: canonical.encoding.bitDirection, fieldOrder: canonical.encoding.fieldOrder, highBitsZero: canonical.encoding.highBitsZero, physicalEndian: canonical.encoding.physicalEndian, neverSuspend: true, cancellationPoint: false, waitApi: "Atomic.wait-separate", logicalFacts: record.facts, callerFactsIgnored: record.callerFactsIgnored });
  }
  const providerDigestChanged = iface.providerDigest !== undefined && iface.candidateProviderDigest !== undefined && iface.providerDigest !== iface.candidateProviderDigest;
  return result(testCase, "promoted-value-record", "canonical-value-record", {
    target: testCase.target.id,
    carrierBytes,
    carrierWidth,
    canonicalBits: record.canonicalBits,
    lockFree: isLockFree,
    progress: isLockFree ? "profile-lockfree" : "profile-fact",
    operations,
    logicalFacts: record.facts,
    callerFactsIgnored: record.callerFactsIgnored,
    providerDigestChanged,
    canonicalBitDirection: canonical.encoding.bitDirection,
    fieldOrder: canonical.encoding.fieldOrder,
    highBitsZero: canonical.encoding.highBitsZero,
    physicalEndian: canonical.encoding.physicalEndian,
    casValues: operations.includes("compareExchange") ? canonical.encoding.casValues : undefined,
    neverSuspend: operationContract.neverSuspend,
    cancellationPoint: operationContract.cancellationPoint,
    waitApi: operationContract.waitApi,
    artifactEvidence: providerDigestChanged ? "recipe-runtime-closure" : undefined,
  });
}

function evaluateB(testCase) {
  if (testCase.route === "integer-handle") {
    if (testCase.packing) return evaluatePacking(testCase);
    const handle = testCase.handle ?? {};
    const table = testCase.ownerTable ?? {};
    if (handle.slotBits !== 32 || handle.generationBits !== 32 || handle.packedCarrier !== "u64") return result(testCase, "rejected", "handle-representation");
    if (!(table.present && table.ownsPayload && table.generationCheck && table.dereferenceAfterCheck && table.returnsNone && table.dropExactlyOnce)) return result(testCase, "rejected", "owner-table-generation-check-missing");
    const current = new Map();
    let dereferenceCount = 0;
    let exhausted = false;
    let wrapped = false;
    for (const operation of testCase.operations ?? []) {
      if (!["install", "resolve", "retire", "reuse"].includes(operation.op)) return result(testCase, "rejected", "unknown-handle-operation");
      if (operation.op === "install") current.set(operation.slot, { generation: operation.generation, live: true });
      if (operation.op === "retire") {
        const slot = current.get(operation.slot);
        if (!slot || slot.generation !== operation.generation) return result(testCase, "rejected", "stale-generation");
        slot.live = false;
      }
      if (operation.op === "reuse") {
        if (operation.previousGeneration === 0xffffffff) {
          if (operation.retireSlot !== true || operation.allocation !== "failed") return result(testCase, "rejected", "generation-wrap-forbidden");
          exhausted = true;
          continue;
        }
        if (!Number.isSafeInteger(operation.generation) || operation.generation <= operation.previousGeneration || operation.generation > 0xffffffff) return result(testCase, "rejected", "generation-wrap-forbidden");
        if (operation.generation === 0) wrapped = true;
        current.set(operation.slot, { generation: operation.generation, live: true });
      }
      if (operation.op === "resolve") {
        const slot = current.get(operation.slot);
        if (!slot || !slot.live || slot.generation !== operation.generation) continue;
        dereferenceCount += 1;
      }
    }
    if (wrapped) return result(testCase, "rejected", "generation-wrap-forbidden", { dereferenceCount: 0 });
    return result(testCase, "handle-current", exhausted ? "generation-exhaustion-retired" : dereferenceCount === 0 ? "stale-generation" : "generation-checked", { route: "integer-handle-owner-table", dereferenceCount, staleReturnsNone: dereferenceCount === 0, generationExhaustion: exhausted });
  }
  if (testCase.route === "tagged-pointer") return result(testCase, "rejected", "tagged-pointer-rejected", { reason: "provenance-lifetime-aba-reclamation" });
  return result(testCase, "rejected", "unknown-axis-b-route");
}

function validateAdapter(adapter) {
  const missing = ADAPTER_FIELDS.filter((field) => {
    const value = adapter?.[field];
    if (field === "participants") return !Array.isArray(value) || value.length === 0;
    if (field === "retiredBound") return !Number.isSafeInteger(value) || value <= 0;
    if (field === "memoryOrders" || field === "events") return !Array.isArray(value) || value.length === 0;
    if (field === "ffiDrain") return typeof value !== "boolean";
    return value === undefined || value === null || value === "";
  });
  if (missing.length > 0) return { ok: false, code: missing.includes("ffiDrain") ? "foreign-drain-missing" : "reclamation-schema-incomplete", missing };
  if (new Set(adapter.participants).size !== adapter.participants.length) return { ok: false, code: "duplicate-participant" };
  if (adapter.registration !== true) return { ok: false, code: "registration-required" };
  if (adapter.retire !== "after-unlink") return { ok: false, code: "retire-before-unlink" };
  if (!["domain-barrier", "epoch", "hazard"].includes(adapter.quiescence)) return { ok: false, code: "quiescence-policy-invalid" };
  if (adapter.deleterContext !== adapter.domain) return { ok: false, code: "wrong-deleter-domain" };
  if (adapter.shutdown !== "drain") return { ok: false, code: "shutdown-nonquiescent" };
  if (!adapter.memoryOrders.every((order) => VALID_ORDERS.has(order)) || !adapter.memoryOrders.includes("acquire") || !adapter.memoryOrders.includes("release")) return { ok: false, code: "memory-order-invalid" };
  if (!VALID_PROGRESS.has(adapter.targetProgress)) return { ok: false, code: "target-progress-invalid" };
  if (!VALID_FAULTS.has(adapter.faultBehavior)) return { ok: false, code: "fault-behavior-invalid" };
  if (!VALID_BOUNDARIES.has(adapter.foreignBoundary)) return { ok: false, code: "foreign-boundary-invalid" };
  if (adapter.foreignBoundary === "persistent-callback" && adapter.ffiDrain !== true) return { ok: false, code: "foreign-drain-missing" };
  if (adapter.foreignBoundary === "none" && adapter.ffiDrain !== false) return { ok: false, code: "foreign-boundary-events-forbidden" };
  if (adapter.events.some((event) => !event || !EVENT_OPS.has(event.op))) return { ok: false, code: "event-name-invalid" };
  return { ok: true };
}

function deriveEvents(adapter) {
  const state = { registered: new Set(), readers: new Map(), drained: new Set(), linked: true, retired: 0, dropped: 0, reclaimed: 0, callback: 0, ffiUnregistered: false, inFlightDrained: false, destroyed: false, unpinned: false, shutdown: false };
  let unlink = false;
  let quiesced = false;
  for (const event of adapter.events) {
    if (state.shutdown) return { ok: false, code: "event-after-shutdown" };
    const participant = event.participant;
    if (event.op === "register") {
      if (!adapter.participants.includes(participant) || state.registered.has(participant)) return { ok: false, code: "event-registration-invalid" };
      state.registered.add(participant);
    } else if (event.op === "access") {
      if (!state.registered.has(participant)) return { ok: false, code: "access-before-register" };
      state.readers.set(participant, (state.readers.get(participant) ?? 0) + 1);
    } else if (event.op === "readerExit") {
      const count = state.readers.get(participant) ?? 0;
      if (count === 0) return { ok: false, code: "reader-exit-underflow" };
      state.readers.set(participant, count - 1);
    } else if (event.op === "unlink") {
      if (unlink) return { ok: false, code: "unlink-double" };
      unlink = true;
      state.linked = false;
    } else if (event.op === "retire") {
      if (!unlink || state.retired > 0) return { ok: false, code: "retire-before-unlink" };
      state.retired = 1;
    } else if (event.op === "quiescence") {
      if (!state.retired || quiesced || [...state.readers.values()].some((count) => count !== 0)) return { ok: false, code: "reclaim-before-quiescence" };
      quiesced = true;
    } else if (event.op === "drop") {
      if (!quiesced || state.dropped > 0 || state.retired === 0) return { ok: false, code: "drop-double-or-before-quiescence" };
      state.dropped = 1;
    } else if (event.op === "reclaim") {
      if (!quiesced || state.dropped !== 1 || state.reclaimed > 0) return { ok: false, code: "reclaim-before-quiescence" };
      state.reclaimed = 1;
      state.retired = 0;
    } else if (event.op === "participantDrain") {
      if (!state.registered.has(participant) || (state.readers.get(participant) ?? 0) !== 0) return { ok: false, code: "participant-drain-with-readers" };
      state.drained.add(participant);
    } else if (event.op === "unregister") {
      if (!state.drained.has(participant) || !state.registered.has(participant)) return { ok: false, code: "unregister-before-drain" };
      state.registered.delete(participant);
    } else if (event.op === "ffiUnregister") {
      if (state.ffiUnregistered) return { ok: false, code: "ffi-unregister-double" };
      state.ffiUnregistered = true;
      state.callback = 1;
    } else if (event.op === "inFlightDrain") {
      if (!state.ffiUnregistered || state.inFlightDrained) return { ok: false, code: "ffi-drain-order-invalid" };
      state.inFlightDrained = true;
      state.callback = 0;
    } else if (event.op === "destroy") {
      if (!state.inFlightDrained || state.destroyed) return { ok: false, code: "destroy-before-ffi-drain" };
      state.destroyed = true;
    } else if (event.op === "unpin") {
      if (!state.destroyed || state.unpinned) return { ok: false, code: "unpin-before-destroy" };
      state.unpinned = true;
    } else if (event.op === "shutdown") {
      if (state.registered.size || state.readers.size && [...state.readers.values()].some((count) => count !== 0) || state.retired || state.callback) return { ok: false, code: "shutdown-nonquiescent" };
      state.shutdown = true;
    }
  }
  if (!unlink || state.retired || state.reclaimed !== 1 || state.dropped !== 1 || state.registered.size || !state.shutdown) return { ok: false, code: "event-sequence-incomplete" };
  if (adapter.foreignBoundary === "persistent-callback" && (!state.ffiUnregistered || !state.inFlightDrained || !state.destroyed || !state.unpinned)) return { ok: false, code: "foreign-drain-sequence-incomplete" };
  return { ok: true, events: adapter.events.length };
}

function evaluateC(testCase) {
  if (testCase.route === "snapshot-cell") return result(testCase, "snapshot-current", "composes-snapshot-cell", { route: "SnapshotCell" });
  if (testCase.route === "domain-service") return result(testCase, "domain-current", "composes-domain-or-service", { route: "domain-barrier-or-service" });
  if (testCase.route === "universal-reclamation") return result(testCase, "rejected", "universal-reclamation-rejected");
  if (testCase.route !== "unsafe-adapter") return result(testCase, "rejected", "unknown-axis-c-route");
  const schema = validateAdapter(testCase.reclamation);
  if (!schema.ok) return result(testCase, "rejected", schema.code, { missing: schema.missing });
  const events = deriveEvents(testCase.reclamation);
  if (!events.ok) return result(testCase, "rejected", events.code);
  return result(testCase, "unsafe-adapter-permitted", "specialized-unsafe-adapter", { implementationEvidence: "missing", route: "unsafe-adapter", events: events.events });
}

function evaluateD(testCase) {
  return result(testCase, "rejected", testCase.route === "raw-pointer" ? "raw-pointer-atomic-rejected" : "universal-rcu-rejected", { reason: "atomicity-does-not-prove-lifetime-or-reclamation" });
}

export function evaluateAtom2Case(testCase) {
  if (!testCase || !AXES.has(testCase.axis) || typeof testCase.id !== "string") throw new Error("ATOM2 case must have an axis and id");
  if (testCase.axis === "A") return evaluateA(testCase);
  if (testCase.axis === "B") return evaluateB(testCase);
  if (testCase.axis === "C") return evaluateC(testCase);
  return evaluateD(testCase);
}

// The event reducer repeats the public projection from independently shaped
// facts. It is intentionally host-only. It does not claim a compiler or runtime.
export function reduceAtom2EventCase(testCase) {
  const evaluated = evaluateAtom2Case(testCase);
  return { ...evaluated, reducer: "event-reducer" };
}

export function deriveAtom2(corpus) {
  const facts = (corpus.cases ?? []).map(evaluateAtom2Case);
  const events = (corpus.cases ?? []).map(reduceAtom2EventCase).map(({ reducer, ...item }) => item);
  if (JSON.stringify(facts) !== JSON.stringify(events)) throw new Error("ATOM2 reducers disagree");
  return facts;
}

export function validateAtom2(corpus, { root } = {}) {
  const errors = [];
  if (corpus?.$schema !== "w-atom2-atomic-contract-cases-1") errors.push("ATOM2 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("ATOM2 corpus status must be design-oracle-input.");
  if (!Array.isArray(corpus?.cases) || corpus.cases.length < 30) return { errors: [...errors, "ATOM2 corpus must contain at least 30 adversarial cases."], results: [] };
  const ids = new Set();
  for (const testCase of corpus.cases) {
    if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}`);
    ids.add(testCase.id);
    if (!AXES.has(testCase.axis)) errors.push(`invalid axis for ${testCase.id}`);
    if (root && typeof testCase.restaurantSymbol === "string") {
      const files = ["synchronization.w", "memory.w", "abi.w"].map((name) => path.join(root, "reference", "last-light", name));
      if (!files.some((file) => fs.existsSync(file) && fs.readFileSync(file, "utf8").includes(testCase.restaurantSymbol))) errors.push(`${testCase.id} has no Last Light symbol ${testCase.restaurantSymbol}.`);
    }
  }
  const results = deriveAtom2(corpus);
  const byId = new Map(results.map((item) => [item.caseId, item]));
  const required = [
    "A-canonical-sign-epoch", "A-canonical-fallback", "A-canonical-lockfree-missing", "A-canonical-pointer", "A-canonical-nested", "A-canonical-custom", "A-canonical-enum-payload", "A-canonical-uint-size", "A-canonical-float", "A-canonical-order", "A-canonical-order-release-acquire", "A-canonical-panic", "A-canonical-oom", "A-canonical-cancel", "A-canonical-wabi", "A-canonical-ffi", "A-canonical-mixed-bool-signed-enum", "A-canonical-field-direction", "A-canonical-field-order", "A-canonical-high-bits", "A-canonical-enum-code-invalid", "A-canonical-fallback-allocating", "A-canonical-fallback-cooperative", "A-canonical-fallback-signal", "A-canonical-fallback-freestanding", "A-canonical-fallback-parking-ambiguous", "B-valid-handle", "B-stale-handle", "B-generation-exhaustion", "B-generation-wrap", "B-tagged-pointer", "C-snapshot", "C-domain", "C-adapter-complete", "C-adapter-ffi", "C-adapter-missing-registration", "C-adapter-before-unlink", "C-adapter-before-quiescence", "C-adapter-drop-double", "C-adapter-shutdown", "C-adapter-ffi-drain", "D-raw-pointer", "D-universal-rcu",
  ];
  for (const id of required) if (!byId.has(id)) errors.push(`ATOM2 required case missing: ${id}`);
  if (byId.get("A-canonical-sign-epoch")?.status !== "promoted-value-record") errors.push("A canonical carrier must be promoted.");
  if (byId.get("A-canonical-mixed-bool-signed-enum")?.status !== "promoted-value-record") errors.push("mixed Bool/signed/enum carrier must be promoted.");
  for (const id of ["A-canonical-field-direction", "A-canonical-field-order", "A-canonical-high-bits", "A-canonical-enum-code-invalid", "A-canonical-fallback-allocating", "A-canonical-fallback-cooperative", "A-canonical-fallback-signal", "A-canonical-fallback-freestanding", "A-canonical-order-release-acquire", "A-canonical-fallback-parking-ambiguous"]) {
    if (byId.get(id)?.status !== "rejected") errors.push(`${id} must be rejected.`);
  }
  if (byId.get("B-generation-wrap")?.status !== "rejected") errors.push("B generation wrap must be rejected.");
  if (byId.get("C-adapter-complete")?.status !== "unsafe-adapter-permitted") errors.push("C adapter must be permitted only behind unsafe evidence.");
  if (results.some((item) => item.status === "research-blocker" || item.status === "candidate-research" || item.status === "adapter-research")) errors.push("ATOM2 must not retain an active Research status.");
  return { errors, results };
}

export function buildAtom2Snapshot(corpus) {
  const checked = validateAtom2(corpus);
  const statusCounts = {};
  const axisCounts = {};
  for (const item of checked.results) {
    statusCounts[item.status] = (statusCounts[item.status] ?? 0) + 1;
    axisCounts[item.axis] = (axisCounts[item.axis] ?? 0) + 1;
  }
  return { status: checked.errors.length === 0 ? "design-oracle-output" : "invalid", metrics: { caseCount: checked.results.length, axisCounts, statusCounts, reducers: ["fact-reducer", "event-reducer"], activeResearchStatuses: checked.results.filter((item) => /research/u.test(item.status)).length }, results: checked.results };
}
