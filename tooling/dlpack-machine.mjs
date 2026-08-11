import { createHash } from "node:crypto";

const KNOWN_FLAGS = new Set(["readOnly", "producerCopied", "subbytePadded"]);
const KNOWN_DTYPES = new Set([
  "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
  "f16", "bf16", "f32", "f64", "complex64", "complex128", "bool8",
]);
const REQUIRED_ALIGNMENT = {
  i8: 1, i16: 2, i32: 4, i64: 8,
  u8: 1, u16: 2, u32: 4, u64: 8,
  f16: 2, bf16: 2, f32: 4, f64: 8,
  complex64: 8, complex128: 16, bool8: 1,
};
const ELEMENT_BYTES = {
  i8: 1, i16: 2, i32: 4, i64: 8,
  u8: 1, u16: 2, u32: 4, u64: 8,
  f16: 2, bf16: 2, f32: 4, f64: 8,
  complex64: 8, complex128: 16, bool8: 1,
};
// DLPack 1.3 device kinds. The provider resolves these names to an opaque W
// identity. A future raw enum is rejected until a provider profile knows it.
const KNOWN_DEVICE_KINDS = new Set([
  "cpu", "cuda", "cudaHost", "opencl", "vulkan", "metal", "vpi",
  "rocm", "rocmHost", "extDev", "cudaManaged", "oneapi", "webgpu",
  "hexagon", "maia", "trainium", "tpu", "tpuHost",
]);

export class DLPackError extends Error {
  constructor(code, details = {}) {
    super(code);
    this.name = "DLPackError";
    this.code = code;
    this.details = { reason: "DLPack contract rejected this operation", ...details };
  }
}

function fail(code, details = {}) {
  throw new DLPackError(code, details);
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") {
    return Object.fromEntries(Object.keys(value).sort().map((key) => [key, canonical(value[key])]));
  }
  return value;
}

export function dlpackDigest(value) {
  return `sha256:${createHash("sha256").update(JSON.stringify(canonical(value))).digest("hex")}`;
}

function limitsFrom(input = {}) {
  const limits = {
    rank: 8,
    dimension: 1_000_000,
    elements: 16_777_216,
    spanBytes: 1_073_741_824,
    metadataBytes: 16_777_216,
    controlBytes: 16_777_216,
    leases: 16,
    releaseJobs: 1024,
    wait: 100_000,
    deadline: 100_000,
    ...input,
  };
  for (const [key, value] of Object.entries(limits)) {
    if (!Number.isSafeInteger(value) || value <= 0) fail("W-DLPACK-0014", { limit: key });
  }
  return limits;
}

function initialState() {
  return {
    phase: "empty",
    version: null,
    tensor: null,
    device: null,
    queue: null,
    flags: [],
    imported: null,
    dynamic: false,
    bound: false,
    views: 0,
    queuedReleaseJobs: 0,
    leases: 0,
    releaseCalls: 0,
    releaseCount: 0,
    deleterCalls: 0,
    ownerGeneration: 0,
    currentReleased: false,
    releaseRecords: [],
    dereferencedFields: [],
    providerResolved: false,
    providerDeviceKey: null,
    providerProfile: null,
    providerExtent: null,
    providerBaseAlignment: null,
    providerDigest: null,
    providerEvents: [],
    deviceResolutions: {},
    capsuleName: null,
    capsuleStatic: false,
    capsuleDestructor: "armed",
    capsuleConsumed: false,
    owner: null,
    wUnique: null,
    wEvents: [],
    python: { attached: false, gil: false, interpreter: "open", leases: 0, jobs: 0, events: [] },
    cExchangeCalls: [],
    receipts: [],
    events: [],
    copies: [],
    diagnostics: [],
    limits: limitsFrom(),
    result: null,
  };
}

function assertNoConclusionFields(operation) {
  const forbidden = new Set([
    "status", "diagnostic", "derived", "conclusion", "copied", "released",
    "explicit", "unique", "ready", "storageProven", "bits", "ownershipProof",
    "borrowed", "providerReceipt", "layoutProof", "provenance", "storageProfile",
    "dtypeFacts", "alignment", "happensBefore",
  ]);
  const visit = (value) => {
    if (!value || typeof value !== "object") return;
    for (const [key, child] of Object.entries(value)) {
      if (forbidden.has(key)) fail("W-DLPACK-0017", { field: key });
      visit(child);
    }
  };
  visit(operation);
}

function checkedAdd(left, right, limit, kind) {
  if (!Number.isSafeInteger(left) || !Number.isSafeInteger(right) || left < 0 || right < 0 || left > limit - right) {
    fail("W-DLPACK-0009", { kind, limit });
  }
  return left + right;
}

function checkedMul(left, right, limit, kind) {
  if (!Number.isSafeInteger(left) || !Number.isSafeInteger(right) || left < 0 || right < 0 ||
      (right !== 0 && left > Math.floor(limit / right))) {
    fail("W-DLPACK-0009", { kind, limit });
  }
  return left * right;
}

function deviceKey(device) {
  if (!device || typeof device !== "object" || typeof device.provider !== "string" || device.provider.length === 0 ||
      !Number.isSafeInteger(device.id) || device.id < 0 || typeof device.kind !== "string" ||
      !KNOWN_DEVICE_KINDS.has(device.kind)) {
    fail("W-DLPACK-0011", { reason: "device identity is not provider-scoped" });
  }
  return `${device.provider}:${device.kind}:${device.id}`;
}

function sameDevice(left, right) {
  return deviceKey(left) === deviceKey(right);
}

function providerResolve(state, operation) {
  const key = deviceKey(operation.device);
  const profile = operation.profile;
  if (!profile || typeof profile !== "object") {
    fail("W-DLPACK-0013", { reason: "provider profile receipt is missing" });
  }
  if (!Number.isSafeInteger(operation.allocationExtent) || operation.allocationExtent < 0) {
    fail("W-DLPACK-0013", { reason: "provider allocation extent receipt is missing" });
  }
  if (!Number.isSafeInteger(operation.baseAlignment) || operation.baseAlignment < 1) {
    fail("W-DLPACK-0012", { reason: "provider base alignment receipt is missing" });
  }
  state.providerResolved = true;
  state.providerDeviceKey = key;
  state.providerProfile = canonical(profile);
  state.providerExtent = operation.allocationExtent;
  state.providerBaseAlignment = operation.baseAlignment;
  state.providerDigest = dlpackDigest({ key, profile: state.providerProfile, extent: state.providerExtent, baseAlignment: state.providerBaseAlignment });
  state.deviceResolutions[key] = state.providerDigest;
  state.providerEvents.push({ kind: "provider-resolve", deviceKey: key, digest: state.providerDigest });
  state.events.push(`provider-resolved:${key}`);
}

function resolveDevice(state, operation) {
  const key = deviceKey(operation.device);
  const digest = dlpackDigest({ kind: "device-resolve", key });
  state.deviceResolutions[key] = digest;
  state.providerEvents.push({ kind: "device-resolve", deviceKey: key, digest });
  state.events.push(`device-resolved:${key}`);
}

function releaseDeleter(state, event) {
  if (state.currentReleased) fail("W-DLPACK-0024", { reason: "deleter called twice for this carrier generation" });
  state.currentReleased = true;
  state.releaseCount += 1;
  state.deleterCalls += 1;
  state.releaseRecords.push({ generation: state.ownerGeneration, kind: "deleter", event });
  if (state.phase === "capsuleUnconsumed" || event.includes("capsule")) state.capsuleDestructor = "fired";
  state.events.push(event);
}

function validateDType(dtype, providerProfile) {
  if (!dtype || typeof dtype.name !== "string" || !KNOWN_DTYPES.has(dtype.name)) fail("W-DLPACK-0006", { dtype: dtype?.name });
  if (dtype.endian !== undefined) fail("W-DLPACK-0007", { reason: "DLPack has no public endian field" });
  if (providerProfile?.nativeEndian !== true) fail("W-DLPACK-0007", { reason: "provider did not prove native endian" });
  if (dtype.lanes !== 1) fail("W-DLPACK-0008", { lanes: dtype.lanes });
  if (dtype.name === "complex64" || dtype.name === "complex128") {
    if (providerProfile?.storage?.[dtype.name] !== "proved") fail("W-DLPACK-0006", { dtype: dtype.name, reason: "complex storage mapping is not provider-proven" });
  }
  if (dtype.name === "bool8" && providerProfile?.storage?.bool8 !== "proved") fail("W-DLPACK-0006", { dtype: "bool8", reason: "8-bit bool storage mapping is not provider-proven" });
  return dtype.name;
}

function validateTensor(state, tensor) {
  if (!tensor || typeof tensor !== "object") fail("W-DLPACK-0001", { reason: "tensor facts are missing" });
  // carrierKind is a wrapper/profile fact. It is checked before any managed
  // tensor field. A legacy wrapper therefore keeps the stable-prefix read set
  // empty and still releases its deleter once.
  if (tensor.carrierKind !== "versioned") {
    releaseDeleter(state, "legacy-release-before-deref");
    fail("W-DLPACK-0002", { reason: "legacy DLManagedTensor is rejected" });
  }
  const version = tensor.version;
  if (!version || !Number.isSafeInteger(version.major)) {
    releaseDeleter(state, "missing-prefix-release-before-deref");
    fail("W-DLPACK-0003", { reason: "stable version prefix is absent" });
  }
  if (version.major !== 1) {
    releaseDeleter(state, "major-mismatch-release-before-deref");
    fail("W-DLPACK-0003", { major: version.major, releaseCount: state.releaseCount });
  }
  if (!state.providerResolved || state.providerProfile?.trusted !== true) fail("W-DLPACK-0013", { reason: "trusted provider resolution/profile receipt is missing" });
  state.dereferencedFields.push("version.minor");
  if (!Number.isSafeInteger(version.minor) || version.minor < 0) fail("W-DLPACK-0004", { reason: "minor version is invalid" });
  if (version.minor > 3 && Array.isArray(tensor.unknownFields) && tensor.unknownFields.length > 0) fail("W-DLPACK-0004", { reason: "unknown minor fields" });
  state.version = { major: version.major, minor: version.minor };
  const flags = Array.isArray(tensor.flags) ? tensor.flags : [];
  state.dereferencedFields.push("flags");
  for (const flag of flags) if (!KNOWN_FLAGS.has(flag)) fail("W-DLPACK-0005", { flag });
  state.flags = [...flags].sort();
  state.dereferencedFields.push("dtype");
  const dtype = validateDType(tensor.dtype, state.providerProfile);
  state.dereferencedFields.push("ndim");
  const ndim = tensor.ndim;
  if (!Number.isSafeInteger(ndim) || ndim < 0 || ndim > state.limits.rank) fail("W-DLPACK-0009", { kind: "rank", rank: ndim, limit: state.limits.rank });
  state.dereferencedFields.push("shape", "strides");
  if (ndim === 0 && (tensor.shape !== null || tensor.strides !== null)) fail("W-DLPACK-0010", { reason: "rank-zero shape and strides must be null" });
  if (ndim > 0 && (!Array.isArray(tensor.shape) || tensor.shape.length !== ndim)) fail("W-DLPACK-0010", { reason: "shape length does not match rank" });
  const shape = ndim === 0 ? [] : tensor.shape;
  let elements = 1;
  for (const dimension of shape) {
    if (!Number.isSafeInteger(dimension) || dimension < 0 || dimension > state.limits.dimension) fail("W-DLPACK-0010", { reason: "dimension is invalid" });
    elements = checkedMul(elements, dimension, state.limits.elements, "elements");
  }
  if (ndim > 0 && (!Array.isArray(tensor.strides) || tensor.strides.length !== ndim)) fail("W-DLPACK-0010", { reason: "rank-positive strides must be non-null and exact" });
  if (Array.isArray(tensor.strides)) for (const stride of tensor.strides) if (!Number.isSafeInteger(stride) || stride < 0) fail("W-DLPACK-0010", { reason: "stride is invalid" });
  if (ndim > 0 && tensor.strideUnit !== undefined && tensor.strideUnit !== "elements") fail("W-DLPACK-0010", { reason: "strides must be element-based" });
  state.dereferencedFields.push("byteOffset");
  if (!Number.isSafeInteger(tensor.byteOffset) || tensor.byteOffset < 0) fail("W-DLPACK-0010", { reason: "byte offset is invalid" });
  const bytesPerElement = ELEMENT_BYTES[dtype];
  let maxOffsetElements = 0;
  let overlapDetected = false;
  if (ndim > 0) {
    for (let index = ndim - 1; index >= 0; index -= 1) {
      const term = checkedMul(shape[index] === 0 ? 0 : shape[index] - 1, tensor.strides[index], state.limits.elements, "spanElements");
      maxOffsetElements = checkedAdd(maxOffsetElements, term, state.limits.elements, "spanElements");
    }
    const axes = shape
      .map((dimension, index) => ({ dimension, stride: tensor.strides[index] }))
      .filter(({ dimension }) => dimension > 1)
      .sort((left, right) => left.stride - right.stride);
    let coveredElements = 1;
    for (const axis of axes) {
      if (axis.stride < coveredElements) overlapDetected = true;
      const delta = checkedMul(axis.dimension - 1, axis.stride, state.limits.elements, "overlapElements");
      coveredElements = checkedAdd(coveredElements, delta, state.limits.elements, "overlapElements");
    }
  }
  const overlapProof = state.providerProfile.layout?.overlap;
  if (overlapDetected && !(state.flags.includes("readOnly") && overlapProof === "readOnly")) {
    fail("W-DLPACK-0010", { reason: "overlapping layout lacks provider proof" });
  }
  const spanElements = elements === 0 ? 0 : checkedAdd(maxOffsetElements, 1, state.limits.elements, "spanElements");
  const span = elements === 0 ? 0 : checkedAdd(tensor.byteOffset, checkedMul(spanElements, bytesPerElement, state.limits.spanBytes, "spanBytes"), state.limits.spanBytes, "spanBytes");
  if (span > state.providerExtent) fail("W-DLPACK-0013", { reason: "allocation extent is smaller than checked span", span, allocationExtent: state.providerExtent });
  state.dereferencedFields.push("data");
  if (elements === 0 && tensor.dataPresent !== false) fail("W-DLPACK-0010", { reason: "zero-size tensors use null data" });
  if (elements > 0 && tensor.dataPresent !== true) fail("W-DLPACK-0010", { reason: "non-empty tensor requires data" });
  if (tensor.dataPresent && (state.providerBaseAlignment < REQUIRED_ALIGNMENT[dtype] || tensor.byteOffset % REQUIRED_ALIGNMENT[dtype] !== 0)) fail("W-DLPACK-0012", { required: REQUIRED_ALIGNMENT[dtype], baseAlignment: state.providerBaseAlignment, byteOffset: tensor.byteOffset });
  state.dereferencedFields.push("device");
  const device = tensor.device;
  const key = deviceKey(device);
  if (key !== state.providerDeviceKey) fail("W-DLPACK-0011", { reason: "provider device receipt does not match descriptor device" });
  state.device = { ...device, key };
  state.tensor = {
    dtype,
    ndim,
    shape,
    strides: tensor.strides === null ? null : [...tensor.strides],
    byteOffset: tensor.byteOffset,
    elements,
    spanBytes: span,
    dataPresent: tensor.dataPresent,
    alignment: state.providerBaseAlignment,
    provenance: "provider-receipt",
    layoutProof: overlapProof ?? null,
    overlap: overlapDetected,
  };
  return state.tensor;
}

function requirePhase(state, phases, reason) {
  if (!phases.includes(state.phase)) fail("W-DLPACK-0015", { phase: state.phase, expected: phases, reason });
}

function requireQueue(state, operationQueue, targetDevice, required) {
  const kind = targetDevice?.kind;
  if (kind === "cpu" && operationQueue === undefined) return null;
  if (kind === "cpu" && operationQueue !== undefined) fail("W-DLPACK-0020", { reason: "CPU does not accept an extra queue" });
  if (kind !== "cpu" && (required || operationQueue !== undefined) && !operationQueue) fail("W-DLPACK-0020", { reason: "stream device requires a provider queue" });
  if (!operationQueue) return null;
  if (operationQueue.rawStream !== undefined || operationQueue.stream !== undefined) fail("W-DLPACK-0020", { reason: "raw stream integers are not accepted" });
  if (operationQueue.providerReceipt !== undefined) fail("W-DLPACK-0017", { reason: "caller-supplied queue receipt is not proof" });
  const queueDevice = operationQueue.device;
  if (!queueDevice || !sameDevice(queueDevice, targetDevice)) fail("W-DLPACK-0021", { reason: "queue/device mismatch" });
  const key = deviceKey(queueDevice);
  if (targetDevice.kind !== "cpu" && (!state.queue || state.queue.device.key !== key || !state.queue.receipt || state.queue.receipt.happensBefore !== true)) fail("W-DLPACK-0022", { reason: "provider receipt lacks bindQueue/producerWait happens-before" });
  state.queue = {
    device: { ...queueDevice, key },
    receipt: state.queue?.receipt ?? null,
  };
  state.events.push("happens-before:" + state.queue.device.key);
  return state.queue;
}

function release(state, owner, event = "release") {
  if (state.currentReleased || state.phase === "released") fail("W-DLPACK-0024", { reason: "release called twice for this carrier generation" });
  state.currentReleased = true;
  state.releaseCalls += 1;
  state.releaseCount += 1;
  state.deleterCalls += 1;
  state.releaseRecords.push({ generation: state.ownerGeneration, kind: "release", owner, event });
  if (state.leases > 0) state.leases -= 1;
  if (state.phase === "capsuleUnconsumed" || event.includes("capsule")) state.capsuleDestructor = "fired";
  state.owner = owner;
  state.phase = "released";
  state.events.push(event);
}

function releaseForeignOnReject(state, event) {
  if (state.currentReleased || !["consumedRenamed", "leaseOwned", "dynamic", "imported"].includes(state.phase)) return;
  if (state.views > 0 || state.queuedReleaseJobs > 0) {
    state.phase = "quarantined";
    state.events.push(`${event}-quarantine`);
    return;
  }
  release(state, "consumer", event);
}

function closeImported(state, operation) {
  requirePhase(state, ["imported", "dynamic"], "close requires an imported owner");
  if (state.views > 0 || state.queuedReleaseJobs > 0) fail("W-DLPACK-0026", { reason: "views or release jobs remain" });
  if (operation.fail === true) {
    state.phase = "quarantined";
    state.events.push("close-failure-quarantine");
    fail("W-DLPACK-0027", { reason: "owner moved to quarantine" });
  }
  release(state, "consumer", "close-release");
}

function ensureOpenOwner(state, operation) {
  if (state.phase === "producerCreated") {
    state.phase = "capsuleUnconsumed";
    state.capsuleName = "dltensor_versioned";
    state.capsuleStatic = operation.static === true;
    state.events.push("capsule-created:dltensor_versioned");
  }
  if (state.phase === "capsuleUnconsumed") {
    state.phase = "consumedRenamed";
    state.capsuleConsumed = true;
    state.capsuleDestructor = "no-op";
    state.owner = "consumer";
    state.events.push("capsule-consumed:used_dltensor_versioned");
  }
  if (state.phase === "consumedRenamed") {
    if (state.leases >= state.limits.leases) fail("W-DLPACK-0014", { limit: "leases" });
    state.phase = "leaseOwned";
    state.leases += 1;
    state.events.push("lease-owned");
  }
  requirePhase(state, ["leaseOwned"], "open consumes capsule and lease internally");
}

function runOperation(state, operation) {
  assertNoConclusionFields(operation);
  const op = operation?.op;
  if (op === "limits") {
    requirePhase(state, ["empty"], "limits must be fixed before producer publication");
    state.limits = limitsFrom(operation.values);
  } else if (op === "providerResolve") {
    requirePhase(state, ["empty"], "provider resolution precedes producer publication");
    providerResolve(state, operation);
  } else if (op === "deviceResolve") {
    requirePhase(state, ["producerCreated", "capsuleUnconsumed", "consumedRenamed", "leaseOwned", "imported", "dynamic", "tensor"], "target device resolution requires a live resource");
    resolveDevice(state, operation);
  } else if (op === "create") {
    requirePhase(state, ["empty"], "producer creation is one-shot");
    state.ownerGeneration = 1;
    state.currentReleased = false;
    state.phase = "producerCreated";
    validateTensor(state, operation.tensor);
    state.owner = "producer";
    state.events.push("producer-created");
  } else if (op === "capsule") {
    requirePhase(state, ["producerCreated"], "capsule creation requires producer owner");
    state.phase = "capsuleUnconsumed";
    state.capsuleName = operation.name ?? "dltensor_versioned";
    state.capsuleStatic = operation.static === true;
    if (state.capsuleName === "dltensor") {
      releaseDeleter(state, "legacy-capsule-release-before-deref");
      fail("W-DLPACK-0002", { reason: "legacy capsule name is rejected" });
    }
    if (state.capsuleName !== "dltensor_versioned") {
      releaseDeleter(state, "unknown-capsule-release-before-deref");
      fail("W-DLPACK-0023", { reason: "unknown capsule name" });
    }
    state.events.push(`${state.capsuleStatic ? "static-" : ""}capsule-created:` + state.capsuleName);
  } else if (op === "dropUnconsumed") {
    requirePhase(state, ["capsuleUnconsumed"], "dropUnconsumed requires an unconsumed versioned capsule");
    releaseDeleter(state, "drop-unconsumed-capsule");
    state.owner = "producer";
    state.phase = "released";
  } else if (op === "capsuleDestructor") {
    if (state.capsuleConsumed || state.capsuleDestructor === "no-op") {
      state.events.push("consumed-capsule-destructor-no-op");
    } else if (state.phase === "capsuleUnconsumed") {
      releaseDeleter(state, "capsule-destructor-release");
      state.phase = "released";
    } else {
      fail("W-DLPACK-0024", { reason: "capsule destructor has no live unconsumed owner" });
    }
  } else if (op === "consume") {
    requirePhase(state, ["capsuleUnconsumed"], "capsule is one-shot");
    if (operation.name && operation.name !== state.capsuleName) fail("W-DLPACK-0023", { reason: "capsule rename mismatch" });
    state.phase = "consumedRenamed";
    state.capsuleConsumed = true;
    state.capsuleDestructor = "no-op";
    state.owner = "consumer";
    state.events.push("capsule-consumed:used_dltensor_versioned");
  } else if (op === "lease") {
    requirePhase(state, ["consumedRenamed"], "lease follows capsule consumption");
    if (state.leases >= state.limits.leases) fail("W-DLPACK-0014", { limit: "leases" });
    state.phase = "leaseOwned";
    state.leases += 1;
    state.events.push("lease-owned");
  } else if (op === "bindQueue") {
    requirePhase(state, ["leaseOwned", "imported", "dynamic", "tensor", "capsuleUnconsumed", "consumedRenamed"], "queue requires a resource");
    if (!operation.queue || !operation.queue.device) fail("W-DLPACK-0022", { reason: "bindQueue requires a provider-minted queue identity" });
    if (operation.queue.rawStream !== undefined || operation.queue.stream !== undefined) fail("W-DLPACK-0020", { reason: "raw stream integers are not accepted" });
    if (operation.queue.providerReceipt !== undefined) fail("W-DLPACK-0017", { reason: "caller-supplied queue receipt is not proof" });
    const queueDevice = operation.queue.device;
    if (!sameDevice(queueDevice, state.device)) fail("W-DLPACK-0021", { reason: "queue/device mismatch" });
    const key = deviceKey(queueDevice);
    state.queue = { device: { ...queueDevice, key }, receipt: null };
    state.events.push(`queue-bound:${key}`);
  } else if (op === "producerWait") {
    requirePhase(state, ["leaseOwned", "imported", "dynamic", "tensor", "capsuleUnconsumed", "consumedRenamed"], "producer wait requires a resource");
    if (!state.queue) fail("W-DLPACK-0022", { reason: "producerWait requires bindQueue" });
    state.queue.receipt = {
      kind: "provider",
      deviceKey: state.queue.device.key,
      happensBefore: true,
      digest: dlpackDigest({ event: "producerWait", queue: state.queue.device.key }),
    };
    state.providerEvents.push({ kind: "producer-wait", deviceKey: state.queue.device.key, digest: state.queue.receipt.digest });
    state.events.push(`producer-wait:${state.queue.device.key}`);
  } else if (op === "queue") {
    fail("W-DLPACK-0022", { reason: "queue must be provider-minted by bindQueue and producerWait" });
  } else if (op === "open") {
    try {
      ensureOpenOwner(state, operation);
      if (state.flags.includes("producerCopied")) fail("W-DLPACK-0018", { reason: "zero-copy open rejects producer-copied" });
      if (state.flags.includes("subbytePadded")) fail("W-DLPACK-0006", { reason: "typed open rejects subbyte-padded storage" });
      requireQueue(state, operation.queue, state.device, state.device?.kind !== "cpu");
      if (operation.element && operation.element !== state.tensor.dtype) fail("W-DLPACK-0019", { reason: "typed dtype mismatch" });
      if (operation.shape && JSON.stringify(operation.shape) !== JSON.stringify(state.tensor.shape)) fail("W-DLPACK-0019", { reason: "typed shape mismatch" });
      state.phase = "imported";
      state.imported = "foreignZeroCopy";
      state.events.push("open-zero-copy");
    } catch (error) {
      releaseForeignOnReject(state, "open-reject-release");
      throw error;
    }
  } else if (op === "openDynamic") {
    try {
      ensureOpenOwner(state, operation);
      requireQueue(state, operation.queue, state.device, state.device?.kind !== "cpu");
      state.phase = "dynamic";
      state.dynamic = true;
      state.imported = "foreignDynamic";
      state.events.push("open-dynamic");
    } catch (error) {
      releaseForeignOnReject(state, "open-dynamic-reject-release");
      throw error;
    }
  } else if (op === "bind") {
    try {
      requirePhase(state, ["dynamic"], "bind consumes dynamic owner");
      if (state.bound) fail("W-DLPACK-0019", { reason: "dynamic owner was already bound" });
      if (state.flags.includes("producerCopied")) fail("W-DLPACK-0018", { reason: "dynamic bind cannot publish a copied owner as zero-copy ImportedTensor" });
      if (operation.element !== state.tensor.dtype || JSON.stringify(operation.shape) !== JSON.stringify(state.tensor.shape) ||
          (operation.strides !== undefined && JSON.stringify(operation.strides) !== JSON.stringify(state.tensor.strides))) {
        fail("W-DLPACK-0019", { reason: "exact dtype/shape/layout bind failed" });
      }
      state.phase = "imported";
      state.bound = true;
      state.imported = "foreignZeroCopy";
      state.events.push("dynamic-bind");
    } catch (error) {
      releaseForeignOnReject(state, "dynamic-bind-reject-release");
      throw error;
    }
  } else if (op === "view") {
    requirePhase(state, ["imported"], "view requires imported owner");
    const callback = operation.callback ?? {};
    if (callback.scope !== "lexical" || callback.escapes === true) fail("W-DLPACK-0025", { reason: "view escaped callback scope" });
    if (callback.inout === true) fail("W-DLPACK-0025", { reason: "view callback cannot produce inout" });
    if (callback.drained !== true) fail("W-DLPACK-0025", { reason: "structured work was not drained" });
    state.views += 1;
    state.events.push("view-borrow");
    state.views -= 1;
    state.events.push("view-drained");
  } else if (op === "materialize") {
    try {
      if (state.phase === "producerCreated") ensureOpenOwner(state, operation);
      requirePhase(state, ["leaseOwned", "imported"], "materialize requires an owned foreign source");
      const target = operation.target;
      const targetKey = deviceKey(target);
      if (!state.deviceResolutions[targetKey]) fail("W-DLPACK-0011", { reason: "target device is not provider-resolved" });
      requireQueue(state, operation.queue, target, target.kind !== "cpu");
      const sourceKey = state.device?.key ?? "managed";
      const transfer = state.device ? !sameDevice(state.device, target) : true;
      const payloadCopyCount = state.tensor?.spanBytes > 0 ? 1 : 0;
      const metadataAllocation = 1;
      release(state, "consumer", "materialize-source-release");
      state.copies.push({ payloadCopyCount, producerCopied: state.flags.includes("producerCopied"), wMaterialized: true, transfer, metadataAllocation, from: sourceKey, to: targetKey });
      state.phase = "tensor";
      state.owner = "w";
      state.wUnique = true;
      state.wEvents.push("materialize-owner-created");
      state.device = { ...target, key: targetKey };
      state.receipts.push({ copy: "materialize", payloadCopyCount, producerCopied: state.flags.includes("producerCopied"), wMaterialized: true, transfer, metadataAllocation, target: targetKey, providerDigest: state.providerDigest, targetDeviceDigest: state.deviceResolutions[targetKey], releaseState: "released" });
      state.events.push("materialize-request");
    } catch (error) {
      releaseForeignOnReject(state, "materialize-reject-release");
      throw error;
    }
  } else if (op === "copyToHost") {
    requirePhase(state, ["tensor"], "copy-to-host requires tensor owner");
    if (operation.target?.kind !== "cpu") fail("W-DLPACK-0028", { reason: "copy-to-host must name a CPU target explicitly" });
    const targetKey = deviceKey(operation.target);
    if (!state.deviceResolutions[targetKey]) fail("W-DLPACK-0011", { reason: "copy target device is not provider-resolved" });
    const payloadCopyCount = state.tensor.spanBytes > 0 ? 1 : 0;
    const transfer = !sameDevice(state.device, operation.target);
    const metadataAllocation = 1;
    state.copies.push({ payloadCopyCount, producerCopied: false, wMaterialized: true, transfer, metadataAllocation, from: state.device.key, to: targetKey });
    state.receipts.push({ copy: "copyToHost", payloadCopyCount, producerCopied: false, wMaterialized: true, transfer, metadataAllocation, target: targetKey, providerDigest: state.providerDigest, targetDeviceDigest: state.deviceResolutions[targetKey], releaseState: "not-applicable" });
    state.events.push("copy-to-host-request");
  } else if (op === "alias" || op === "borrow") {
    requirePhase(state, ["tensor"], `${op} requires a W Tensor owner`);
    state.wUnique = false;
    state.wEvents.push(`w-${op}`);
    state.events.push(`w-${op}`);
  } else if (op === "export") {
    requirePhase(state, ["tensor"], "export consumes W Tensor");
    if (state.owner !== "w") fail("W-DLPACK-0029", { reason: "export requires a consuming W owner" });
    if (state.wUnique !== true) fail("W-DLPACK-0029", { reason: "writable export requires derived unique W ownership" });
    requireQueue(state, operation.queue, state.device, state.device.kind !== "cpu");
    state.ownerGeneration += 1;
    state.currentReleased = false;
    state.phase = "producerCreated";
    state.owner = "producer";
    state.flags = [];
    state.receipts.push({
      copy: "export",
      payloadCopyCount: 0,
      producerCopied: false,
      wMaterialized: false,
      transfer: false,
      metadataAllocation: 1,
      target: state.device.key,
      providerDigest: state.providerDigest,
      targetDeviceDigest: state.deviceResolutions[state.device.key],
      writable: true,
      releaseState: "pending",
    });
    state.events.push("export-consuming");
  } else if (op === "release") {
    if (state.phase === "released") fail("W-DLPACK-0024", { reason: "release called twice" });
    requirePhase(state, ["capsuleUnconsumed", "imported", "dynamic", "tensor", "producerCreated"], "release requires live owner");
    const event = state.phase === "capsuleUnconsumed"
      ? "capsule-destructor-release"
      : (state.ownerGeneration > 1 && state.phase === "producerCreated" ? "exported-owner-release" : "deleter-release");
    release(state, "consumer", event);
  } else if (op === "close") {
    closeImported(state, operation);
  } else if (op === "cancel") {
    if (state.phase === "capsuleUnconsumed") {
      release(state, "producer", "abandoned-capsule-release");
    } else if (["imported", "dynamic", "tensor", "leaseOwned"].includes(state.phase)) {
      if (operation.drained !== true || state.queuedReleaseJobs > 0) fail("W-DLPACK-0026", { reason: "cancellation skipped consumer drain" });
      state.phase = "draining";
      state.events.push("cancel-drain");
      release(state, "consumer", "cancel-release");
    } else fail("W-DLPACK-0015", { phase: state.phase, reason: "nothing cancellable" });
  } else if (op === "python") {
    const action = operation.action;
    if (action === "attach") {
      if (state.python.interpreter !== "open") fail("W-DLPACK-0030", { reason: "thread cannot attach after finalization" });
      state.python.attached = true;
      state.events.push("python-thread-attached");
    }
    else if (action === "gil") {
      if (!state.python.attached || state.python.interpreter !== "open") fail("W-DLPACK-0030", { reason: "attached thread state is required" });
      state.python.gil = true;
      state.events.push("python-gil-held");
    } else if (action === "lease") {
      if (!state.python.attached || !state.python.gil || state.python.interpreter !== "open") fail("W-DLPACK-0030", { reason: "GIL/interpreter lease proof is missing" });
      state.python.leases += 1;
      state.python.events.push("lease-acquired");
      state.events.push("python-lease-child");
    } else if (action === "releaseLease") {
      if (state.python.leases <= 0) fail("W-DLPACK-0030", { reason: "Python child lease release has no matching lease" });
      state.python.leases -= 1;
      state.python.events.push("lease-released");
      state.events.push("python-lease-release");
    } else if (action === "job") {
      if (!state.python.attached || !state.python.gil || state.python.interpreter !== "open") fail("W-DLPACK-0030", { reason: "Python release job requires attached GIL thread" });
      state.python.jobs += 1;
      state.python.events.push("job-queued");
      state.events.push("python-job-queued");
    } else if (action === "releaseJob") {
      if (state.python.jobs <= 0) fail("W-DLPACK-0030", { reason: "Python release job has no matching job" });
      state.python.jobs -= 1;
      state.python.events.push("job-released");
      state.events.push("python-job-release");
    } else if (action === "drain") {
      if (!state.python.attached || !state.python.gil) fail("W-DLPACK-0030", { reason: "drain requires attached GIL thread" });
      if (state.python.leases > 0 || state.python.jobs > 0) fail("W-DLPACK-0030", { reason: "child leases and release jobs must drain before finalization" });
      state.python.interpreter = "draining";
      state.python.events.push("drain-started");
      state.events.push("python-lease-drain");
    } else if (action === "release") {
      if (state.python.interpreter !== "draining" || state.python.leases > 0 || state.python.jobs > 0) fail("W-DLPACK-0030", { reason: "release requires drained interpreter children and jobs" });
      state.python.gil = false;
      state.python.attached = false;
      state.python.events.push("thread-released");
      state.events.push("python-thread-released");
    } else if (action === "finalize") {
      if (state.python.interpreter !== "draining" || state.python.leases > 0 || state.python.jobs > 0 || state.python.gil) fail("W-DLPACK-0030", { reason: "finalization before lease/job drain" });
      state.python.interpreter = "finalized";
      state.python.events.push("interpreter-finalized");
      state.events.push("python-finalized");
    } else if (action === "callback") {
      if (state.python.interpreter !== "open" || state.python.gil !== true) {
        state.events.push("python-late-callback-quarantine");
        fail("W-DLPACK-0030", { reason: "callback crossed finalization boundary; callback quarantined" });
      }
      state.events.push("python-callback");
    } else fail("W-DLPACK-0030", { reason: "unknown Python boundary action" });
  } else if (op === "untrusted") {
    fail("W-DLPACK-0013", { reason: "DLPack is not a serialization or network format" });
  } else if (op === "hiddenCopy") {
    fail("W-DLPACK-0028", { reason: "payload copy must be named by materialize or copyToHost" });
  } else if (op === "cExchange") {
    const valid = operation.bridge === "python" && operation.apiStatic === true &&
      operation.capsuleName === "dlpack_exchange_api" && operation.gilHeld === true &&
      operation.ownership === "borrowed" && operation.escapes !== true &&
      operation.suspends !== true && operation.streamResolved === true &&
      operation.controlReturned === true && operation.ownerHeldUntilWorkDrained === true;
    if (!valid) {
      fail("W-DLPACK-0032", { reason: "C Exchange N0 must be static, Python-scoped, non-owning, non-suspending, stream-resolved, and keep the producer through work drain" });
    }
    state.cExchangeCalls.push({ scope: "callback", ownership: "borrowed", stream: "producer-current" });
    state.events.push("python-c-exchange-n0-returned");
  } else if (op === "leaseReserve") {
    if (state.leases >= state.limits.leases) fail("W-DLPACK-0014", { limit: "leases" });
    state.leases += 1;
    state.events.push("lease-reserved");
  } else if (op === "releaseJob") {
    if (state.queuedReleaseJobs >= state.limits.releaseJobs) fail("W-DLPACK-0014", { limit: "releaseJobs" });
    state.queuedReleaseJobs += 1;
    state.events.push("release-job-queued");
  } else if (op === "wait") {
    if (!Number.isSafeInteger(operation.units) || operation.units < 0 || operation.units > state.limits.wait ||
        (operation.deadline !== undefined && (!Number.isSafeInteger(operation.deadline) || operation.deadline > state.limits.deadline))) {
      fail("W-DLPACK-0014", { limit: "wait/deadline" });
    }
    state.events.push("bounded-wait");
  } else if (op === "allocation") {
    if (!Number.isSafeInteger(operation.metadataBytes) || operation.metadataBytes < 0 || operation.metadataBytes > state.limits.metadataBytes ||
        !Number.isSafeInteger(operation.controlBytes) || operation.controlBytes < 0 || operation.controlBytes > state.limits.controlBytes) {
      fail("W-DLPACK-0016", { reason: "metadata/control allocation exceeds limits" });
    }
    state.events.push("bounded-metadata-allocation");
  } else if (op === "receipt") {
    const raw = JSON.stringify(operation.value ?? {});
    if (/pointer|capsuleAddress|secret|gilToken|interpreterPointer/i.test(raw)) fail("W-DLPACK-0031", { reason: "receipt contains redacted fields" });
    state.receipts.push({
      phase: state.phase,
      device: state.device?.key ?? null,
      queue: state.queue?.device.key ?? null,
      providerDigest: state.providerDigest,
      queueReceiptDigest: state.queue?.receipt?.digest ?? null,
      releaseState: state.currentReleased ? "released" : "pending",
      events: [...state.events],
    });
  } else {
    fail("W-DLPACK-0001", { reason: `unknown operation ${op}` });
  }
}

export function compactDLPackState(state) {
  return {
    phase: state.phase,
    version: state.version,
    tensor: state.tensor,
    device: state.device ? { key: state.device.key, kind: state.device.kind, provider: state.device.provider } : null,
    queue: state.queue ? { device: state.queue.device.key, receipt: state.queue.receipt } : null,
    flags: state.flags,
    imported: state.imported,
    dynamic: state.dynamic,
    bound: state.bound,
    wUnique: state.wUnique,
    wEvents: state.wEvents,
    leases: state.leases,
    queuedReleaseJobs: state.queuedReleaseJobs,
    releaseCalls: state.releaseCalls,
    releaseCount: state.releaseCount,
    deleterCalls: state.deleterCalls,
    ownerGeneration: state.ownerGeneration,
    releaseRecords: state.releaseRecords,
    dereferencedFields: state.dereferencedFields,
    provider: {
      resolved: state.providerResolved,
      deviceKey: state.providerDeviceKey,
      profileDigest: state.providerDigest,
      allocationExtent: state.providerExtent,
      baseAlignment: state.providerBaseAlignment,
      events: state.providerEvents,
      deviceResolutions: state.deviceResolutions,
    },
    capsule: { name: state.capsuleName, static: state.capsuleStatic, consumed: state.capsuleConsumed, destructor: state.capsuleDestructor },
    copies: state.copies,
    receipts: state.receipts,
    python: state.python,
    cExchangeCalls: state.cExchangeCalls,
    events: state.events,
  };
}

export function runDLPackProgram(operations = []) {
  const state = initialState();
  try {
    for (const operation of operations) runOperation(state, operation);
    state.result = "accepted";
    return { status: "accepted", state, result: compactDLPackState(state) };
  } catch (error) {
    if (!(error instanceof DLPackError)) throw error;
    const originalDetails = error.details ?? {};
    error.details = {
      ...originalDetails,
      phase: typeof originalDetails.phase === "string" ? originalDetails.phase : "interface",
      actual: typeof originalDetails.actual === "string" ? originalDetails.actual : JSON.stringify(originalDetails.actual ?? error.code),
      construct: typeof originalDetails.construct === "string" ? originalDetails.construct : "DLPack carrier contract",
      expected: Array.isArray(originalDetails.expected)
        ? originalDetails.expected.map((value) => String(value))
        : [String(originalDetails.expected ?? "valid provider and carrier facts")],
      facts: {
        ...(originalDetails.facts ?? {}),
        ownerPhase: state.phase,
        releaseCount: state.releaseCount,
        dereferencedFields: [...state.dereferencedFields],
        events: [...state.events],
      },
    };
    state.phase = state.phase === "quarantined" ? "quarantined" : "rejected";
    state.diagnostics.push(error.code);
    return { status: "rejected", state, result: compactDLPackState(state), error };
  }
}
