import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export const TARGETS = ["posix", "windows"];
export const AXES = ["baseline", "immutable", "channel", "lifecycle", "provider"];

const OFFICIAL_HOSTS = new Set([
  "pubs.opengroup.org",
  "learn.microsoft.com",
  "doc.rust-lang.org",
  "docs.python.org",
]);
const VALID_ORDERS = new Set(["relaxed", "acquire", "release", "acquireRelease", "sequential"]);
const VALID_PROGRESS = new Set(["lock-free", "blocking", "polling"]);
const VALID_FIELD_KINDS = new Set(["fixed-scalar", "bytes-extent", "relative-offset", "relative-index", "atomic-control"]);
const FORBIDDEN_FIELD_KINDS = new Set(["nativePointer", "owner", "borrow", "view", "ref", "capability", "allocatorIdentity", "dropful", "usize", "isize", "target-native"]);
const REQUIRED_HEADER = ["magic", "version", "schema", "schemaId", "layoutId", "objectId", "schemaDigest", "layoutDigest", "length", "alignment", "endian", "generation"];
const REQUIRED_PROVIDER = [
  "targetKind", "objectIdentity", "generation", "authoritative", "profileKind", "accessRights", "leaseLifecycle",
  "addressIndependent", "allowedLayouts", "allowedSchemas", "atomic", "backing", "flushReceipt", "deleteBehavior", "crashOutcome",
];
const VALID_OPERATIONS = new Set([
  "map", "validate", "view", "drop-view", "unmap", "close", "open", "remove-name", "withdraw-name", "resize", "truncate", "sync",
  "stage", "validate", "hash", "publish-selector", "request-durability", "flush-data", "flush-metadata", "flush-selector", "receipt", "read", "observe-generation", "crash", "reopen",
  "snapshot", "wire", "service", "offer", "reserve", "write", "commit", "send", "receive", "release", "cancel",
  "fallback", "provider-open", "require", "compare", "stop-access", "register-callback", "unregister-callback", "drain",
  "callback", "owner-death", "acquire-guard", "hold-guard", "atomic", "materialize", "broker-name", "loan", "return-loan",
]);

/* These names were used by an earlier oracle. They are intentionally schema
 * errors now: a caller must describe ordered events and provider bindings. */
const LEGACY_FIELDS = new Set([
  "crashPoint", "cancelPhase", "crashSlot", "full", "ownerOrder", "unlinkWhileMapped", "removeNameWhileMapped",
  "targetDivergence", "forgedFacts", "supervisorReopen", "ffiCloseOrder", "callbackAfterUnmap", "accessAfterClose",
  "viewEscapeAfterUnmap", "resizeWhileMapped", "truncateWhileMapped", "lastUnmapSyncObjectUse", "hiddenLock",
  "hiddenAllocator", "hiddenScheduler", "fallbackProfile", "context", "ownerDeathNotification", "atomicUnsupported",
  "targetUnsupported", "requireDurable", "callerProviderFacts", "providerFacts", "atomic", "pointerFree", "relativeOffsets",
  "nativeValues", "containsOwners", "containsBorrows", "containsCapabilities", "containsDropful", "nativePointer", "portableImmediate", "race", "point", "capacity", "scope",
]);

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function digestText(value) {
  return `sha256:${crypto.createHash("sha256").update(String(value)).digest("hex")}`;
}

function nonEmpty(value) {
  return typeof value === "string" && value.trim() !== "";
}

function validDigest(value) {
  return typeof value === "string" && /^sha256:[0-9a-f]{64}$/u.test(value);
}

function canonicalWithout(value, keys = []) {
  const clone = structuredClone(value);
  for (const key of keys) delete clone[key];
  return clone;
}

function canonicalDigest(value, keys = ["digest", "layoutDigest", "schemaDigest"]) {
  return digestText(JSON.stringify(canonicalWithout(value, keys)));
}

function validPowerOfTwo(value) {
  return Number.isSafeInteger(value) && value > 0 && (value & (value - 1)) === 0;
}

function result(testCase, status, code, details = {}) {
  return { caseId: testCase.id, axis: testCase.axis, status, code, ...details };
}

function reject(testCase, code, details = {}) {
  return result(testCase, "rejected", code, details);
}

function accept(testCase, code, details = {}) {
  return result(testCase, "accepted", code, details);
}

function fault(testCase, code, details = {}) {
  return result(testCase, "faulted", code, { hiddenRepair: false, ...details });
}

function unknown(testCase, code, details = {}) {
  return result(testCase, "unknown", code, { unknownDurability: true, ...details });
}

function escapedCount(source, symbol) {
  const escaped = String(symbol).replace(/[.*+?^${}()|[\]\\]/gu, "\\$&");
  return (source.match(new RegExp(escaped, "gu")) ?? []).length;
}

function contained(root, relative) {
  if (!nonEmpty(relative)) return undefined;
  const resolved = path.resolve(root, relative);
  const relativeToRoot = path.relative(root, resolved);
  if (!relativeToRoot || relativeToRoot.startsWith(`..${path.sep}`) || path.isAbsolute(relativeToRoot)) return undefined;
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) return undefined;
  return resolved;
}

function operationName(operation) {
  return typeof operation === "string" ? operation : operation?.op;
}

function operationsOf(testCase) {
  return (testCase.operations ?? []).map((operation) => typeof operation === "string" ? { op: operation } : operation);
}

function findLegacyFields(value, location = "input", errors = []) {
  if (!value || typeof value !== "object") return errors;
  if (Array.isArray(value)) {
    value.forEach((entry, index) => findLegacyFields(entry, `${location}[${index}]`, errors));
    return errors;
  }
  for (const [key, entry] of Object.entries(value)) {
    if (LEGACY_FIELDS.has(key)) errors.push(`${location}.${key} is a legacy oracle field; use ordered operations.`);
    findLegacyFields(entry, `${location}.${key}`, errors);
  }
  return errors;
}

function validateOfficialSources(corpus, errors) {
  const ids = new Set();
  if (!Array.isArray(corpus.officialSources) || corpus.officialSources.length < 10) errors.push("IPC1 officialSources must contain POSIX, Windows, Rust, and Python primary references.");
  for (const [index, source] of (corpus.officialSources ?? []).entries()) {
    const location = `officialSources[${index}]`;
    let parsed;
    try { parsed = new URL(source?.url); } catch { parsed = undefined; }
    if (!nonEmpty(source?.id) || ids.has(source.id)) errors.push(`${location}.id must be unique.`);
    ids.add(source?.id);
    if (!parsed || parsed.protocol !== "https:" || !OFFICIAL_HOSTS.has(parsed.hostname)) errors.push(`${location}.url must use an official primary-source host.`);
    if (!nonEmpty(source?.claim)) errors.push(`${location}.claim must be non-empty.`);
  }
}

function validateAtomicFacts(atomic, location, errors, { robust = false } = {}) {
  if (!atomic || typeof atomic !== "object") {
    errors.push(`${location} must be an object.`);
    return;
  }
  if (atomic.processShared !== true) errors.push(`${location}.processShared must be true.`);
  for (const key of ["widths", "orders", "alignments", "progress"]) {
    if (!Array.isArray(atomic[key]) || atomic[key].length === 0) errors.push(`${location}.${key} must be non-empty.`);
  }
  if ((atomic.widths ?? []).some((value) => !Number.isSafeInteger(value) || value <= 0)) errors.push(`${location}.widths must contain positive integers.`);
  if ((atomic.alignments ?? []).some((value) => !validPowerOfTwo(value))) errors.push(`${location}.alignments must contain power-of-two integers.`);
  if ((atomic.orders ?? []).some((value) => !VALID_ORDERS.has(value))) errors.push(`${location}.orders contains an invalid memory order.`);
  if ((atomic.progress ?? []).some((value) => !VALID_PROGRESS.has(value))) errors.push(`${location}.progress contains an invalid progress fact.`);
  if ((atomic.progress ?? []).includes("wait-free")) errors.push(`${location}.progress wait-free requires a provider receipt and is not accepted by IPC1.`);
  if (!nonEmpty(atomic.waitWake) || !new Set(["polling", "kernel", "condition-variable"]).has(atomic.waitWake)) errors.push(`${location}.waitWake must name polling or an explicit wait/wake mechanism.`);
  if (new Set(atomic.widths ?? []).size !== (atomic.widths ?? []).length) errors.push(`${location}.widths must not contain duplicates.`);
  if (new Set(atomic.orders ?? []).size !== (atomic.orders ?? []).length) errors.push(`${location}.orders must not contain duplicates.`);
  if (robust && !(atomic.progress ?? []).includes("blocking")) errors.push(`${location}.robust profile must publish blocking progress.`);
}

export function validateProviderProfile(profile, location = "provider") {
  const errors = [];
  if (!profile || typeof profile !== "object") return [`${location} must be an object.`];
  for (const key of REQUIRED_PROVIDER) if (profile[key] === undefined) errors.push(`${location}.${key} is required.`);
  if (!TARGETS.includes(profile.targetKind)) errors.push(`${location}.targetKind must be posix or windows.`);
  if (!nonEmpty(profile.objectIdentity) || !nonEmpty(profile.generation)) errors.push(`${location}.objectIdentity and generation are required.`);
  if (profile.authoritative !== true) errors.push(`${location}.authoritative must be true.`);
  if (!new Set(["file-durable", "volatile-channel", "robust-blocking"]).has(profile.profileKind)) errors.push(`${location}.profileKind is invalid.`);
  if (!Array.isArray(profile.accessRights) || profile.accessRights.length === 0 || !profile.accessRights.includes("read")) errors.push(`${location}.accessRights must be non-empty and include read.`);
  if (!Array.isArray(profile.leaseLifecycle) || profile.leaseLifecycle.length < 2) errors.push(`${location}.leaseLifecycle must record open and close.`);
  const lifecycleRequirements = profile.targetKind === "posix"
    ? (profile.backing?.kind === "file" ? ["open-file", "mmap", "munmap", "close"] : ["shm_open", "mmap", "munmap", "close"])
    : (profile.backing?.kind === "file" ? ["CreateFile", "CreateFileMapping", "MapViewOfFile", "UnmapViewOfFile", "CloseHandle"] : ["CreateFileMapping", "MapViewOfFile", "UnmapViewOfFile", "CloseHandle"]);
  if (Array.isArray(profile.leaseLifecycle) && lifecycleRequirements.some((operation) => !profile.leaseLifecycle.includes(operation))) errors.push(`${location}.leaseLifecycle lacks target and backing lifecycle operations.`);
  if (profile.addressIndependent !== true) errors.push(`${location}.addressIndependent must be true.`);
  if (Object.prototype.hasOwnProperty.call(profile, "layoutDigest") || Object.prototype.hasOwnProperty.call(profile, "schemaDigest")) errors.push(`${location} must use allowedLayouts and allowedSchemas; singular provider digests are ambiguous.`);
  if (!Array.isArray(profile.allowedLayouts) || profile.allowedLayouts.length === 0 || profile.allowedLayouts.some((entry) => !nonEmpty(entry?.layoutId) || !validDigest(entry?.digest))) errors.push(`${location}.allowedLayouts must publish layout IDs and digests.`);
  if (!Array.isArray(profile.allowedSchemas) || profile.allowedSchemas.length === 0 || profile.allowedSchemas.some((entry) => !nonEmpty(entry?.schemaId) || !validDigest(entry?.digest))) errors.push(`${location}.allowedSchemas must publish schema IDs and digests.`);
  validateAtomicFacts(profile.atomic, `${location}.atomic`, errors, { robust: profile.profileKind === "robust-blocking" });
  if (!profile.backing || typeof profile.backing !== "object" || !new Set(["file", "shm", "pagefile"]).has(profile.backing.kind) || typeof profile.backing.volatile !== "boolean" || typeof profile.backing.durable !== "boolean") errors.push(`${location}.backing must record file, shm, or pagefile volatility and durability.`);
  if (profile.backing?.kind === "file" && (profile.backing.volatile !== false || profile.backing.durable !== true)) errors.push(`${location}.file backing must be durable and non-volatile.`);
  if (profile.backing?.kind !== "file" && (profile.backing.volatile !== true || profile.backing.durable !== false)) errors.push(`${location}.shared/pagefile backing must be volatile and non-durable.`);
  if (!profile.flushReceipt || typeof profile.flushReceipt !== "object" || !Array.isArray(profile.flushReceipt.operations) || !nonEmpty(profile.flushReceipt.success) || !nonEmpty(profile.flushReceipt.failure)) errors.push(`${location}.flushReceipt must record operations, success, and failure.`);
  const dataFlush = profile.targetKind === "posix" ? "msync" : "FlushViewOfFile";
  const metadataFlush = profile.targetKind === "posix" ? "fsync" : "FlushFileBuffers";
  if (profile.backing?.durable && (!profile.flushReceipt.operations.includes(dataFlush) || !profile.flushReceipt.operations.includes(metadataFlush))) errors.push(`${location}.flushReceipt lacks target data and metadata operations.`);
  if (!profile.backing?.durable && profile.flushReceipt.operations.length !== 0) errors.push(`${location}.volatile profile must not publish a durability flush receipt.`);
  if (profile.backing?.durable && profile.flushReceipt.failure !== "unknownDurability") errors.push(`${location}.durable flush failure must be unknownDurability.`);
  if (!profile.backing?.durable && (profile.flushReceipt.success !== "volatile-only" || profile.flushReceipt.failure !== "unsupported")) errors.push(`${location}.volatile flush facts must be unsupported and non-durable.`);
  if (!profile.deleteBehavior || typeof profile.deleteBehavior !== "object" || !nonEmpty(profile.deleteBehavior.discovery) || !nonEmpty(profile.deleteBehavior.existingLease)) errors.push(`${location}.deleteBehavior must record target discovery and existing-lease behavior.`);
  if (profile.targetKind === "posix" && profile.backing?.kind === "shm" && !/shm_unlink/iu.test(profile.deleteBehavior?.discovery ?? "")) errors.push(`${location}.POSIX shm discovery must describe shm_unlink.`);
  if (profile.targetKind === "windows" && !/no portable unlink|kernel object/iu.test(profile.deleteBehavior?.discovery ?? "")) errors.push(`${location}.Windows discovery must not pretend to have POSIX unlink.`);
  if (!profile.crashOutcome || typeof profile.crashOutcome !== "object" || !nonEmpty(profile.crashOutcome.inFlight) || !nonEmpty(profile.crashOutcome.published)) errors.push(`${location}.crashOutcome must record inFlight and published outcomes.`);
  return errors;
}

function validateLayoutDescriptor(layout, location = "layout") {
  const errors = [];
  if (!layout || typeof layout !== "object") return [`${location} must be an object.`];
  if (!nonEmpty(layout.layoutId)) errors.push(`${location}.layoutId must be non-empty.`);
  if (!nonEmpty(layout.schemaId)) errors.push(`${location}.schemaId must be non-empty.`);
  if (!Number.isSafeInteger(layout.mappedExtent) || layout.mappedExtent <= 0) errors.push(`${location}.mappedExtent must be a positive safe integer.`);
  if (layout.canonicalRepresentation !== "fixed-width-little-endian") errors.push(`${location}.canonicalRepresentation must be fixed-width-little-endian.`);
  if (!validDigest(layout.layoutDigest) || !validDigest(layout.schemaDigest)) errors.push(`${location}.layoutDigest and schemaDigest must be sha256 digests.`);
  if (!Array.isArray(layout.segments) || layout.segments.length === 0) return [...errors, `${location}.segments must be non-empty.`];
  const segments = [];
  const fields = [];
  const names = new Set();
  for (const [segmentIndex, segment] of layout.segments.entries()) {
    const segmentLocation = `${location}.segments[${segmentIndex}]`;
    if (!nonEmpty(segment?.name) || names.has(segment.name)) errors.push(`${segmentLocation}.name must be unique.`);
    names.add(segment?.name);
    const cap0Slots = layout.cap0 === true && segment?.name === "slots";
    if (!Number.isSafeInteger(segment?.offset) || !Number.isSafeInteger(segment?.length) || segment.offset < 0 || (!cap0Slots && segment.length <= 0) || (cap0Slots && segment.length !== 0)) errors.push(`${segmentLocation} offset/length must be bounded integers.`);
    const end = Number.isSafeInteger(segment?.offset) && Number.isSafeInteger(segment?.length) ? segment.offset + segment.length : Infinity;
    if (!Number.isSafeInteger(end) || end > (layout.mappedExtent ?? 0)) errors.push(`${segmentLocation} exceeds mapped extent or overflows.`);
    segments.push({ ...segment, end });
    if (!Array.isArray(segment?.fields) || (!cap0Slots && segment.fields.length === 0) || (cap0Slots && segment.fields.length !== 0)) errors.push(`${segmentLocation}.fields must be empty only for cap0 slots.`);
    for (const [fieldIndex, field] of (segment?.fields ?? []).entries()) {
      const fieldLocation = `${segmentLocation}.fields[${fieldIndex}]`;
      if (FORBIDDEN_FIELD_KINDS.has(field?.kind)) errors.push(`${fieldLocation}.kind ${field.kind} is not relocatable.`);
      if (!VALID_FIELD_KINDS.has(field?.kind)) errors.push(`${fieldLocation}.kind is invalid.`);
      if (!Number.isSafeInteger(field?.offset) || !Number.isSafeInteger(field?.length) || field.offset < segment.offset || field.length <= 0) errors.push(`${fieldLocation} offset/length is invalid.`);
      if (!validPowerOfTwo(field?.alignment) || field.offset % field.alignment !== 0) errors.push(`${fieldLocation}.alignment is invalid.`);
      const endField = Number.isSafeInteger(field?.offset) && Number.isSafeInteger(field?.length) ? field.offset + field.length : Infinity;
      if (!Number.isSafeInteger(endField) || endField > end) errors.push(`${fieldLocation} exceeds segment or overflows.`);
      fields.push({ ...field, end: endField });
    }
  }
  const overlap = (a, b) => a.offset < b.end && b.offset < a.end;
  for (let index = 0; index < segments.length; index += 1) for (let other = index + 1; other < segments.length; other += 1) if (overlap(segments[index], segments[other])) errors.push(`${location}.segments overlap.`);
  for (let index = 0; index < fields.length; index += 1) for (let other = index + 1; other < fields.length; other += 1) if (overlap(fields[index], fields[other])) errors.push(`${location}.fields overlap.`);
  if (layout.wireCarrier === true && layout.cap0 === true && (layout.maxSlotCount !== 0 || layout.maxSlotSize !== 0)) errors.push(`${location}.cap0 must publish zero slot capacity.`);
  if (layout.wireCarrier === true && layout.cap0 !== true && (!Number.isSafeInteger(layout.maxSlotCount) || layout.maxSlotCount <= 0 || !Number.isSafeInteger(layout.maxSlotSize) || layout.maxSlotSize <= 0)) errors.push(`${location}.capN must publish positive slot bounds.`);
  if (validDigest(layout.layoutDigest) && canonicalDigest(layout, ["layoutDigest"]) !== layout.layoutDigest) errors.push(`${location}.layoutDigest does not match canonical descriptor.`);
  return errors;
}

function resolveLayout(testCase, layouts) {
  const layoutId = testCase.input?.layoutId;
  const layout = layouts?.[layoutId];
  if (!layout) return undefined;
  const resolved = structuredClone(layout);
  const mutation = testCase.input?.layoutMutation;
  if (!mutation || typeof mutation !== "object") return resolved;
  if (mutation.kind === "nativePointer") {
    const field = resolved.segments?.[1]?.fields?.[0];
    if (field) field.kind = "nativePointer";
  } else if (mutation.kind === "offset") {
    const segment = resolved.segments?.find((candidate) => candidate.name === (mutation.segment ?? "payload"));
    if (segment) segment.offset = mutation.value;
  } else if (mutation.kind === "extent") {
    resolved.mappedExtent = mutation.value;
  } else if (mutation.kind === "overlap") {
    const segment = resolved.segments?.find((candidate) => candidate.name === "payload");
    if (segment) { segment.offset = 32; segment.length = 4064; }
  } else if (mutation.kind === "overflow") {
    const segment = resolved.segments?.find((candidate) => candidate.name === "payload");
    if (segment) { segment.offset = Number.MAX_SAFE_INTEGER - 1; segment.length = 4; }
  } else if (mutation.kind === "hiddenMechanism") {
    resolved.hiddenMechanism = mutation.value;
  }
  return resolved;
}

function validateSchemaRegistry(schemas, location = "schemas") {
  const errors = [];
  if (!schemas || typeof schemas !== "object" || Object.keys(schemas).length < 3) return [`${location} must contain distinct menu, telemetry, and channel schemas.`];
  const digests = new Set();
  for (const [schemaId, schema] of Object.entries(schemas)) {
    if (!nonEmpty(schema?.schemaId) || schema.schemaId !== schemaId) errors.push(`${location}.${schemaId}.schemaId is inconsistent.`);
    if (!validDigest(schema?.digest)) errors.push(`${location}.${schemaId}.digest is invalid.`);
    if (validDigest(schema?.digest) && canonicalDigest(schema, ["digest"]) !== schema.digest) errors.push(`${location}.${schemaId}.digest does not match canonical schema.`);
    if (digests.has(schema?.digest)) errors.push(`${location}.${schemaId}.digest is duplicated.`);
    digests.add(schema?.digest);
  }
  return errors;
}

function validateHeader(header) {
  if (!header || typeof header !== "object") return "header-missing";
  for (const key of REQUIRED_HEADER) if (header[key] === undefined) return `header-${key}-missing`;
  if (header.magic !== "WIPC1") return "header-magic-invalid";
  if (!Number.isSafeInteger(header.version) || header.version < 1) return "header-version-invalid";
  if (!nonEmpty(header.schema)) return "header-schema-invalid";
  if (!nonEmpty(header.schemaId)) return "header-schemaId-invalid";
  if (!nonEmpty(header.layoutId)) return "header-layoutId-invalid";
  if (!nonEmpty(header.objectId)) return "header-objectId-invalid";
  if (!validDigest(header.layoutDigest)) return "header-layoutDigest-invalid";
  if (!validDigest(header.schemaDigest)) return "header-schemaDigest-invalid";
  if (!Number.isSafeInteger(header.length) || header.length <= 0) return "header-length-invalid";
  if (!validPowerOfTwo(header.alignment)) return "header-alignment-invalid";
  if (!new Set(["little", "big"]).has(header.endian)) return "header-endian-invalid";
  if (!Number.isSafeInteger(header.generation) || header.generation < 1) return "header-generation-invalid";
  return undefined;
}

function requiredAtomicSupported(profile, layout) {
  const requirement = layout?.control;
  if (!requirement || typeof requirement !== "object") return "atomic-requirement-missing";
  if (!profile.atomic.widths.includes(requirement.width)) return "atomic-width-unsupported";
  if (!profile.atomic.orders.includes(requirement.order)) return "atomic-order-unsupported";
  if (!profile.atomic.alignments.includes(requirement.alignment)) return "atomic-alignment-unsupported";
  if (!profile.atomic.progress.includes(requirement.progress)) return "atomic-progress-unsupported";
  if (requirement.waitWake && profile.atomic.waitWake !== requirement.waitWake) return "atomic-wait-wake-unsupported";
  return undefined;
}

function immutablePreflight(testCase, input, profile, layout, schemas) {
  if (!layout) return "layout-missing";
  const layoutErrors = validateLayoutDescriptor(layout, "layout");
  if (layoutErrors.length > 0) return layoutErrors[0].includes("overlap") ? "layout-overlap" : layoutErrors[0].includes("exceeds") || layoutErrors[0].includes("overflow") ? "layout-bounds" : layoutErrors[0].includes("relocatable") || layoutErrors[0].includes("kind") ? "non-relocatable-value" : "layout-invalid";
  const headerError = validateHeader(input.header);
  if (headerError) return headerError;
  const schema = schemas?.[input.header.schemaId];
  if (!schema || input.header.schema !== input.header.schemaId || layout.schemaId !== input.header.schemaId) return "schema-digest-mismatch";
  if (schema.digest !== input.header.schemaDigest) return "header-digest-mismatch";
  if (input.header.layoutId !== layout.layoutId || input.header.objectId === undefined) return "header-layout-object-mismatch";
  if (!profile.allowedLayouts?.some((entry) => entry.layoutId === layout.layoutId && entry.digest === layout.layoutDigest) || !profile.allowedSchemas?.some((entry) => entry.schemaId === input.header.schemaId && entry.digest === input.header.schemaDigest)) return "provider-layout-schema-mismatch";
  if (input.header.layoutDigest !== layout.layoutDigest || layout.schemaDigest !== input.header.schemaDigest) return "header-digest-mismatch";
  if (input.header.endian !== "little" || layout.canonicalRepresentation !== "fixed-width-little-endian") return "header-endian-invalid";
  if (input.header.length > layout.mappedExtent) return "extent-out-of-bounds";
  return undefined;
}

function immutableLogical(testCase, input, profile, layout, schemas) {
  const preflight = immutablePreflight(testCase, input, profile, layout, schemas);
  if (preflight) return reject(testCase, preflight);
  const ops = operationsOf(testCase);
  const leases = new Map();
  const objects = new Map([[input.header.objectId, { objectId: input.header.objectId, generation: input.header.generation, validated: false }]]);
  let staged;
  let hashedStage = false;
  let selected = { objectId: input.header.objectId, generation: input.header.generation };
  let selectorPublished = false;
  let selectorFlush = false;
  let durabilityRequest;
  const generationFlushes = new Set();
  let terminalReceipt = false;
  let observedStale = false;
  let observedPair;
  let remapped = false;
  let publishedVisibility = false;
  const addresses = [];
  const fail = (code, details = {}) => reject(testCase, code, details);
  const activeLease = (process) => leases.get(process);
  const pairFor = (operation) => ({ objectId: operation.objectId ?? `generation-${operation.generation ?? input.header.generation}`, generation: operation.generation ?? input.header.generation });
  const selectedLease = () => [...leases.values()].find((lease) => lease.objectId === selected.objectId && lease.generation === selected.generation);
  for (const [index, operation] of ops.entries()) {
    const op = operationName(operation);
    if (op === "map") {
      const process = operation.process ?? "default";
      if (leases.has(process)) return fail("map-with-live-lease");
      const pair = pairFor(operation);
      if (observedPair && (pair.objectId !== observedPair.objectId || pair.generation !== observedPair.generation)) return fail("generation-observation-mismatch", { observed: observedPair, requested: pair });
      if (pair.generation < selected.generation) return fail("stale-generation");
      let object = objects.get(pair.objectId);
      if (!object && observedStale && pair.generation >= selected.generation) {
        object = { objectId: pair.objectId, generation: pair.generation, validated: false };
        objects.set(pair.objectId, object);
      }
      if (!object || object.generation !== pair.generation) return fail("generation-object-missing");
      leases.set(process, { ...pair, validated: false, view: false, address: operation.address ?? null });
      addresses.push(operation.address ?? null);
    } else if (op === "stage") {
      if (staged) return fail("duplicate-stage");
      const pair = pairFor(operation);
      if (pair.generation <= selected.generation) return fail("generation-not-monotonic");
      if (objects.has(pair.objectId) || [...leases.values()].some((lease) => lease.objectId === pair.objectId && lease.generation === pair.generation)) return fail("generation-reuse-with-live-lease");
      staged = { ...pair, validated: false };
      objects.set(pair.objectId, staged);
    } else if (op === "validate") {
      const pair = pairFor(operation);
      if (staged && pair.objectId === staged.objectId && pair.generation === staged.generation) staged.validated = true;
      else {
        const process = operation.process ?? [...leases.keys()][0];
        const lease = activeLease(process);
        if (!lease) return fail("validate-before-map");
        const object = objects.get(lease.objectId);
        if (!object || object.generation !== lease.generation) return fail("generation-object-missing");
        lease.validated = true;
        object.validated = true;
      }
    } else if (op === "hash") {
      if (!staged || staged.validated !== true) return fail("hash-before-validation");
      if (hashedStage) return fail("duplicate-generation-hash");
      hashedStage = true;
      staged.hashed = true;
    } else if (op === "view") {
      const process = operation.process ?? [...leases.keys()][0];
      const lease = activeLease(process);
      if (!lease || lease.validated !== true) return fail("view-before-validate");
      if (lease.view) return fail("duplicate-view");
      lease.view = true;
    } else if (op === "drop-view") {
      const process = operation.process ?? [...leases.keys()][0];
      const lease = activeLease(process);
      if (!lease || !lease.view) return fail("drop-view-without-view");
      lease.view = false;
    } else if (op === "publish-selector") {
      if (!staged || staged.validated !== true || staged.hashed !== true) return fail("selector-publish-before-hash");
      if (selectorPublished) return fail("duplicate-selector-publish");
      const selectorPair = pairFor(operation);
      if (selectorPair.objectId !== staged.objectId || selectorPair.generation !== staged.generation) return fail("selector-object-mismatch", { staged: { objectId: staged.objectId, generation: staged.generation }, requested: selectorPair });
      if (durabilityRequest) {
        const needed = ["flush-data", "flush-metadata"];
        if (needed.some((required) => !generationFlushes.has(required))) return fail("publish-before-generation-flush");
      }
      selected = { objectId: staged.objectId, generation: staged.generation };
      selectorPublished = true;
      publishedVisibility = true;
    } else if (op === "request-durability") {
      if (!staged || staged.validated !== true || staged.hashed !== true) return fail("durability-before-hash");
      if (selectorPublished) return fail("durability-after-selector-publish");
      if (durabilityRequest) return fail("duplicate-durability-request");
      if (!profile.backing.durable) return fail("durability-unavailable");
      if (operation.scope !== undefined) return fail("durability-scope-forbidden");
      durabilityRequest = "all";
    } else if (op === "flush-data" || op === "flush-metadata") {
      if (!durabilityRequest) return fail("flush-before-request");
      if (!staged || staged.validated !== true || staged.hashed !== true) return fail("flush-before-hash");
      if (generationFlushes.has(op)) return fail("duplicate-flush");
      generationFlushes.add(op);
    } else if (op === "flush-selector") {
      if (!selectorPublished) return fail("selector-flush-before-publish");
      if (selectorFlush) return fail("duplicate-selector-flush");
      selectorFlush = true;
    } else if (op === "receipt") {
      if (!durabilityRequest) return fail("receipt-before-request");
      if (terminalReceipt) return fail("duplicate-receipt");
      if (!selectorPublished) return fail("receipt-before-selector-publish");
      if (!selectorFlush) return fail("receipt-before-selector-flush");
      const needed = ["flush-data", "flush-metadata"];
      if (needed.some((required) => !generationFlushes.has(required))) return fail("durability-receipt-incomplete");
      terminalReceipt = true;
    } else if (op === "read") {
      const process = operation.process ?? [...leases.keys()][0];
      const lease = activeLease(process);
      if (!lease || !lease.view || lease.validated !== true) return fail("read-before-validate");
      if ((operation.leaseGeneration !== undefined && operation.leaseGeneration !== lease.generation) || (operation.objectId !== undefined && operation.objectId !== lease.objectId)) return fail("stale-generation");
      lease.read = true;
    } else if (op === "observe-generation") {
      const process = operation.process ?? [...leases.keys()][0];
      const lease = activeLease(process);
      if (!lease) return fail("observe-before-map");
      if (operation.objectId !== undefined && (!nonEmpty(operation.objectId) || operation.objectId !== lease.objectId) && operation.expectedGeneration === lease.generation) return fail("generation-observation-mismatch");
      if (operation.expectedGeneration !== undefined && operation.expectedGeneration !== lease.generation) {
        if (!nonEmpty(operation.objectId) || !Number.isSafeInteger(operation.expectedGeneration) || operation.expectedGeneration < 1) return fail("generation-observation-mismatch");
        observedStale = true;
        observedPair = { objectId: operation.objectId, generation: operation.expectedGeneration };
        if (operation.expectedGeneration > selected.generation) selected = { ...observedPair };
      }
    } else if (op === "unmap") {
      const process = operation.process ?? [...leases.keys()][0];
      const lease = activeLease(process);
      if (!lease) return fail("unmap-without-map");
      if (lease.view) return fail("unmap-active-view");
      leases.delete(process);
      if (observedStale && lease.generation < selected.generation) remapped = true;
    } else if (op === "close") {
      if ([...leases.values()].some((lease) => lease.view)) return fail("close-active-view");
      if (leases.size > 0) return fail("close-before-unmap");
    } else if (op === "crash") {
      if (!selectorPublished) return accept(testCase, "writer-crash-before-selector", { published: false, previousGenerationPreserved: true, generation: selected.generation, visibility: "previous" });
      if (durabilityRequest && !terminalReceipt) return unknown(testCase, "unknownDurability", { visibility: "published", generation: selected.generation, requested: durabilityRequest, receipt: "none" });
      if (terminalReceipt) return accept(testCase, "published-after-receipt", { published: true, durability: durabilityRequest, generation: selected.generation });
      return accept(testCase, "published-visibility-only", { published: true, generation: selected.generation });
    } else if (!["snapshot", "wire", "service"].includes(op)) {
      return fail("immutable-operation-invalid", { operation: op, index });
    }
  }
  if (observedStale && !remapped) return fail("remap-required");
  if (staged && !selectorPublished) return fail("unpublished-generation");
  if (durabilityRequest && !terminalReceipt) return fail("durability-receipt-required", { requested: durabilityRequest });
  if ([...leases.values()].some((lease) => lease.view)) return fail("active-view-at-terminal");
  if (leases.size > 0) return fail("active-lease-at-terminal");
  if (!ops.some((operation) => operationName(operation) === "crash") && !selectorPublished && !remapped) return fail("incomplete-immutable-trace");
  if (remapped) return accept(testCase, "generation-remap", { generation: selected.generation, lease: "new-generation", visibility: "observed" });
  return accept(testCase, "immutable-generation", {
    generation: selected.generation,
    objectIdentity: selected.objectId,
    addressesIndependent: new Set(addresses.filter((address) => address !== null)).size > 1 || addresses.length < 2,
    visibility: publishedVisibility ? "published" : "none",
    publication: publishedVisibility ? "validated-selector-release" : "none",
    durability: terminalReceipt ? durabilityRequest : "none",
    immutableObject: true,
  });
}

function channelLogical(testCase, input, profile, layout, schemas) {
  if (layout?.hiddenMechanism) return reject(testCase, "hidden-provider-state", { mechanism: layout.hiddenMechanism });
  const layoutErrors = validateLayoutDescriptor(layout, "layout");
  if (layoutErrors.length > 0) return reject(testCase, layoutErrors[0].includes("overlap") ? "layout-overlap" : layoutErrors[0].includes("exceeds") || layoutErrors[0].includes("overflow") ? "layout-bounds" : layoutErrors[0].includes("relocatable") ? "non-relocatable-value" : "channel-layout-invalid");
  if (layout.wireCarrier !== true) return reject(testCase, "channel-wire-carrier-required");
  const header = input.header;
  if (!header) return reject(testCase, "channel-header-missing");
  const headerError = validateHeader(header);
  if (headerError) return reject(testCase, headerError);
  const schema = schemas?.[header.schemaId];
  if (!schema || header.schemaId !== layout.schemaId || schema.digest !== header.schemaDigest || header.layoutId !== layout.layoutId || header.layoutDigest !== layout.layoutDigest) return reject(testCase, "channel-header-mismatch");
  const cap0 = layout.cap0 === true;
  const slotsSegment = layout.segments?.find((segment) => segment.name === "slots");
  if (!slotsSegment) return reject(testCase, "channel-slots-segment-missing");
  if (header.length !== layout.mappedExtent) return reject(testCase, "channel-header-length-mismatch");
  if (!Number.isSafeInteger(header.slotCount) || !Number.isSafeInteger(header.slotSize) || header.slotCount < 0 || header.slotSize < 0) return reject(testCase, "channel-capacity-invalid");
  if (cap0 && (header.slotCount !== 0 || header.slotSize !== 0)) return reject(testCase, "channel-capacity-mismatch");
  if (!cap0 && (header.slotCount <= 0 || header.slotCount > layout.maxSlotCount || header.slotSize <= 0 || header.slotSize > layout.maxSlotSize)) return reject(testCase, "channel-capacity-mismatch");
  if (!Number.isSafeInteger(header.slotCount * header.slotSize) || header.slotCount * header.slotSize > slotsSegment.length) return reject(testCase, "channel-capacity-bounds");
  const atomicError = requiredAtomicSupported(profile, layout);
  const ops = operationsOf(testCase);
  if (atomicError) {
    if (ops.some((operation) => operationName(operation) === "fallback")) return accept(testCase, "fallback-snapshot", { route: "snapshot-wire-service", fallbackExplicit: true });
    return reject(testCase, atomicError);
  }
  let mapped = false;
  let validated = false;
  let view = false;
  let sawMap = false;
  let sawValidate = false;
  let sawView = false;
  let sawClose = false;
  let close = false;
  let faulted = false;
  let faultState;
  let faultGeneration;
  let faultProcess;
  let openedGeneration = header.generation;
  let recoveryState = "none";
  let reopened = false;
  let guardHeld = false;
  let unrelatedProcessCrash = false;
  let committedFullSurvived = false;
  const sends = new Map();
  const slots = new Map();
  const blocked = [];
  let materialized = 0;
  const fail = (code, details = {}) => reject(testCase, code, details);
  const firstEmpty = () => {
    for (let index = 0; index < header.slotCount; index += 1) if (![...slots.values()].some((slot) => slot.index === index && slot.state !== "empty")) return index;
    return undefined;
  };
  const activeSlot = (state) => [...sends.entries()].find(([, send]) => send.state === state);
  const validateWire = (operation, send) => {
      if (!Array.isArray(send.bytes) || send.bytes.length > (cap0 ? 1000 : header.slotSize)) return "payload-too-large";
    if (operation.schemaId !== undefined && operation.schemaId !== header.schemaId) return "slot-schema-mismatch";
    const expectedChecksum = digestText(JSON.stringify(send.bytes));
    if (operation.checksum !== undefined && operation.checksum !== expectedChecksum) return "slot-checksum-invalid";
    return undefined;
  };
  for (const [index, operation] of ops.entries()) {
    const op = operationName(operation);
    if (faulted && !["stop-access", "drain", "drop-view", "unmap", "close", "reopen"].includes(op)) return fail("faulted-generation-no-repair", { operation: op });
    const sendId = operation.sendId;
    if (op === "map") {
      if (mapped) return fail("map-with-live-view");
      if (operation.generation !== undefined && (!Number.isSafeInteger(operation.generation) || operation.generation !== openedGeneration)) return fail("channel-generation-mismatch", { headerGeneration: header.generation, currentGeneration: openedGeneration, requested: operation.generation });
      mapped = true;
      sawMap = true;
    } else if (op === "validate") {
      if (!mapped) return fail("validate-before-map");
      validated = true;
      sawValidate = true;
    } else if (op === "view") {
      if (!validated) return fail("view-before-validate");
      if (view) return fail("duplicate-view");
      view = true;
      sawView = true;
    } else if (op === "drop-view") {
      if (!view) return fail("drop-view-without-view");
      if (faulted && recoveryState !== "drained") return fail("drop-before-drain");
      view = false;
    } else if (op === "offer" || op === "reserve" || op === "send") {
      if (!view) return fail("send-before-view");
      if (!nonEmpty(sendId)) return fail("send-id-required");
      if (sends.has(sendId)) return fail("duplicate-send-id");
      if (cap0) {
        if ([...sends.values()].some((send) => !send.committed && !send.cancelled)) return fail("rendezvous-pair-busy");
        sends.set(sendId, { state: "offered", committed: false, slot: null, owner: "sender", process: operation.process ?? "writer" });
      } else {
        const slotIndex = firstEmpty();
        if (slotIndex === undefined) {
          blocked.push(sendId);
          sends.set(sendId, { state: "blocked", committed: false, slot: null, owner: "sender", process: operation.process ?? "writer" });
        } else {
          sends.set(sendId, { state: "writing", committed: false, slot: slotIndex, owner: "sender", process: operation.process ?? "writer" });
          slots.set(sendId, { index: slotIndex, state: "writing", sendId });
        }
      }
    } else if (op === "write") {
      const send = sends.get(sendId);
      if (!send || send.cancelled) return fail("write-without-offer");
      if (send.state === "blocked") return fail("write-while-backpressured");
      if (send.state !== "writing" && !(cap0 && send.state === "offered")) return fail("write-state-invalid");
      if (!Array.isArray(operation.bytes)) return fail("wire-bytes-required");
      if (operation.slotHeaderCorrupt === true) return fail("slot-header-invalid");
      send.bytes = operation.bytes;
      send.schemaId = operation.schemaId ?? header.schemaId;
      send.checksum = operation.checksum ?? digestText(JSON.stringify(operation.bytes));
      const wireError = validateWire(operation, send);
      if (wireError) return fail(wireError);
      send.written = true;
      if (!cap0) slots.get(sendId).state = "writing";
    } else if (op === "commit") {
      const send = sends.get(sendId);
      if (!send || send.cancelled) return fail("commit-without-offer");
      if (send.state === "blocked") return fail("commit-while-backpressured");
      if (cap0 && !send.received) return fail("rendezvous-pair-required");
      if (send.committed) return fail("duplicate-commit");
      if (!send.written) return fail("commit-before-write");
      const wireError = validateWire({}, send);
      if (wireError) return fail(wireError);
      send.committed = true;
      send.state = "full";
      send.owner = "mapped-generation";
      if (!cap0) slots.get(sendId).state = "full";
    } else if (op === "receive") {
      const send = sends.get(sendId);
      if (!send) return fail("receive-without-send");
      if (cap0) {
        if (send.state !== "offered") return fail("rendezvous-state-invalid");
        send.received = true;
      } else {
        if (!send.committed) return fail("receive-before-commit");
        if (send.state !== "full") return fail("receive-state-invalid");
        send.state = "reading";
        send.readerProcess = operation.process ?? "reader";
        slots.get(sendId).state = "reading";
      }
    } else if (op === "read" || op === "materialize") {
      const send = sends.get(sendId);
      if (!send || (!send.committed && !cap0) || (cap0 && !send.received)) return fail("read-before-receive");
      if (send.materialized) return fail("double-materialize");
      if (send.schemaId !== header.schemaId) return fail("slot-schema-mismatch");
      if (send.checksum !== digestText(JSON.stringify(send.bytes ?? []))) return fail("slot-checksum-invalid");
      send.materialized = true;
      send.owner = "receiver-new-owner";
      materialized += 1;
    } else if (op === "release") {
      const send = sends.get(sendId);
      if (!send || !send.materialized) return fail("release-before-materialize");
      send.state = "empty";
      if (!cap0) slots.get(sendId).state = "empty";
    } else if (op === "cancel") {
      const send = sends.get(sendId);
      if (!send) return fail("cancel-without-send");
      if (send.committed) return accept(testCase, "cancel-after-commit", { owner: "mapped-generation", transferred: true, sendId });
      send.cancelled = true;
      send.state = "cancelled";
      return accept(testCase, "cancel-before-commit", { owner: "sender", transferred: false, sendId });
    } else if (op === "crash") {
      const process = operation.process;
      if (!nonEmpty(process)) return fail("crash-process-required");
      const candidateEntries = [...sends.entries()].filter(([, send]) => send.process === process || send.readerProcess === process);
      const candidateEntry = candidateEntries.find(([, send]) => send.state === "writing")
        ?? candidateEntries.find(([, send]) => send.state === "reading")
        ?? candidateEntries.find(([, send]) => send.state === "full");
      const candidate = candidateEntry?.[1];
      if (candidate?.state === "writing" && process === candidate.process) {
        candidate.state = "faulted";
        if (!cap0) slots.get(candidateEntry[0]).state = "faulted";
        faulted = true; faultState = "writing"; faultProcess = process; faultGeneration = openedGeneration; recoveryState = "fault";
      } else if (candidate?.state === "reading" && process === candidate.readerProcess) {
        candidate.state = "faulted"; if (!cap0) slots.get(candidateEntry[0]).state = "faulted";
        faulted = true; faultState = "reading"; faultProcess = process; faultGeneration = openedGeneration; recoveryState = "fault";
      } else if (candidate?.state === "full" && process === candidate.process) {
        committedFullSurvived = true;
      } else if (candidate === undefined) {
        unrelatedProcessCrash = true;
      }
    } else if (op === "stop-access") {
      if (!faulted) return fail("stop-access-without-fault");
      if (recoveryState !== "fault") return fail("duplicate-stop-access");
      recoveryState = "stopped";
    } else if (op === "drain") {
      if (faulted && recoveryState !== "stopped") return fail("drain-before-stop-access");
      if (faulted) recoveryState = "drained";
    } else if (op === "reopen") {
      if (!faulted) return fail("reopen-without-fault");
      if (recoveryState !== "closed") return fail("reopen-before-cleanup");
      if (!Number.isSafeInteger(operation.generation) || operation.generation <= openedGeneration) return fail("reopen-generation-not-higher");
      openedGeneration = operation.generation;
      reopened = true;
      recoveryState = "reopened";
    } else if (op === "require") {
      if (profile.profileKind === "robust-blocking" && operation.execution === "cooperative-worker") return fail("fallback-context-incompatible");
      if (profile.profileKind === "robust-blocking" && operation.execution === "blocking-service") continue;
      return fail("channel-requirement-invalid", { requirement: operation.execution ?? null });
    } else if (op === "acquire-guard") {
      if (profile.profileKind !== "robust-blocking") return fail("robust-provider-required");
      guardHeld = false;
    } else if (op === "hold-guard") {
      if (profile.profileKind !== "robust-blocking") return fail("robust-provider-required");
      guardHeld = true;
    } else if (op === "owner-death") {
      if (profile.profileKind !== "robust-blocking") return fail("owner-death-provider-required");
      if (!guardHeld) return fail("owner-death-without-held-guard");
      faulted = true;
      faultState = "owner-death";
      faultProcess = operation.process ?? "writer";
      faultGeneration = openedGeneration;
      recoveryState = "fault";
    } else if (op === "unmap") {
      if (!mapped) return fail("unmap-without-map");
      if (view) return fail("unmap-active-view");
      if ([...sends.values()].some((send) => ["writing", "reading"].includes(send.state))) return fail("unmap-with-active-slot");
      if (faulted && recoveryState !== "drained") return fail("unmap-before-drain");
      mapped = false;
      if (faulted) recoveryState = recoveryState === "drained" ? "unmapped" : recoveryState;
    } else if (op === "close") {
      if (mapped) return fail("close-before-unmap");
      if (close) return fail("double-close");
      if (faulted && recoveryState !== "unmapped") return fail("close-before-unmap");
      close = true;
      sawClose = true;
      if (faulted && recoveryState !== "reopened") recoveryState = recoveryState === "unmapped" ? "closed" : recoveryState;
    } else if (!["snapshot", "wire", "service"].includes(op)) {
      return fail("channel-operation-invalid", { operation: op, index });
    }
  }
  if (blocked.length > 0 && !faulted && !ops.some((operation) => operationName(operation) === "crash")) return accept(testCase, "backpressure", { capacity: header.slotCount, blockedSendIds: blocked, senderOwnership: true });
  if (faulted && reopened) {
    if (recoveryState !== "reopened" || mapped || view || !sawClose) return fail("incomplete-recovery-trace");
    return accept(testCase, "generation-reopened", { previousFault: faultState, faultProcess, previousGeneration: faultGeneration, generation: openedGeneration, cleanup: ["stop-access", "drain", "drop-view", "unmap", "close"] });
  }
  if (faulted) return fault(testCase, "generation-fault", { slot: faultState, process: faultProcess, generation: faultGeneration });
  if (view || mapped) return fail("active-resource-at-terminal");
  if (!sawMap || !sawValidate || !sawView || !sawClose) return fail("incomplete-channel-trace");
  if (committedFullSurvived) {
    if (materialized < 1) return fail("committed-full-not-drained");
    return accept(testCase, "committed-full-survives-producer-crash", { capacity: header.slotCount, committedFullSurvived: true, owner: "receiver-new-owner", exactlyOnce: "at-most-one-owner-per-committed-slot" });
  }
  if (unrelatedProcessCrash) return accept(testCase, "unrelated-process-crash-no-fault", { capacity: header.slotCount, carrierOutcome: cap0 ? "rendezvous-channel" : "bounded-mapped-channel", owner: materialized > 0 ? "receiver-materializes-new-owner" : "mapped-wire-after-commit", unrelatedProcessCrash: true, exactlyOnce: "at-most-one-owner-per-committed-slot" });
  if (materialized > 0) return accept(testCase, cap0 ? "rendezvous-channel" : "bounded-mapped-channel", { capacity: header.slotCount, owner: "receiver-materializes-new-owner", exactlyOnce: "at-most-one-owner-per-committed-slot" });
  return accept(testCase, cap0 ? "rendezvous-channel" : "bounded-mapped-channel", { capacity: header.slotCount, owner: "mapped-wire-after-commit", exactlyOnce: "at-most-one-owner-per-committed-slot" });
}

function lifecycleLogical(testCase, input, profile, layout) {
  const ops = operationsOf(testCase);
  let mapped = false;
  let view = false;
  let closed = false;
  let callback = false;
  let stopped = false;
  let drained = false;
  let nameRemoved = false;
  let withdrawn = false;
  let generation = 1;
  let activeLoan = false;
  let sawMap = false;
  let sawView = false;
  let sawClose = false;
  let sawStop = false;
  const closeOrder = [];
  const fail = (code, details = {}) => reject(testCase, code, details);
  for (const [index, operation] of ops.entries()) {
    const op = operationName(operation);
    if (op === "map") {
      if (mapped) return fail("map-with-live-view");
      mapped = true; view = false; sawMap = true; generation = operation.generation ?? generation;
    } else if (op === "view") {
      if (!mapped) return fail("view-before-map");
      if (view) return fail("duplicate-view");
      view = true; sawView = true;
    } else if (op === "drop-view") {
      if (!view) return fail("drop-view-without-view");
      if (stopped && !drained) return fail("drop-before-drain");
      if (callback) return fail("drop-before-unregister");
      view = false; closeOrder.push("drop-view");
    } else if (op === "register-callback") {
      if (!view) return fail("callback-before-view");
      if (callback) return fail("duplicate-callback");
      if (stopped) return fail("callback-after-stop");
      callback = true;
    } else if (op === "stop-access") {
      if (stopped) return fail("duplicate-stop-access");
      stopped = true; sawStop = true; closeOrder.push("stop-access");
    } else if (op === "unregister-callback") {
      if (!callback) return fail("unregister-without-callback");
      if (!stopped) return fail("unregister-before-stop-access");
      callback = false; closeOrder.push("unregister-callback");
    } else if (op === "drain") {
      if (!stopped) return fail("drain-before-stop-access");
      if (callback) return fail("drain-before-unregister");
      if (drained) return fail("duplicate-drain");
      drained = true; closeOrder.push("drain");
    } else if (op === "callback") { if (!callback || stopped || !view) return fail("lease-access-after-close"); }
    else if (op === "read" || op === "sync") {
      if (op === "sync" && !mapped) return fail("sync-object-after-last-unmap");
      if (!view || stopped || closed) return fail("lease-access-after-close");
      if (operation.generation !== undefined && operation.generation !== generation) return fail("generation-mismatch");
    } else if (op === "loan") {
      if (!view) return fail("loan-before-view");
      activeLoan = true;
    } else if (op === "return-loan") {
      if (!activeLoan) return fail("return-loan-without-loan");
      activeLoan = false;
    } else if (op === "remove-name" || op === "withdraw-name") {
      nameRemoved = true;
      withdrawn = op === "withdraw-name";
      if (withdrawn) return reject(testCase, "name-withdrawal-unsupported", { targetKind: profile.targetKind });
    } else if (op === "resize" || op === "truncate") {
      if (mapped) return fail("resize-live-view");
    } else if (op === "unmap") {
      if (!mapped) return fail("unmap-without-map");
      if (callback || activeLoan) return fail("unmap-with-active-lease");
      if (stopped && !drained) return fail("unmap-before-drain");
      if (view) return fail("unmap-active-view");
      mapped = false;
      closeOrder.push("unmap");
    } else if (op === "close") {
      if (mapped) return fail("close-before-unmap");
      if (callback || activeLoan || view) return fail("close-active-lease");
      if (stopped && !drained) return fail("close-before-drain");
      if (closed) return fail("double-close");
      closed = true;
      sawClose = true;
      closeOrder.push("close");
    } else if (op === "open") {
      if (operation.generation !== undefined && operation.generation !== generation) return fail("generation-mismatch");
    } else if (!["snapshot", "wire", "service", "broker-name"].includes(op)) return fail("lifecycle-operation-invalid", { operation: op, index });
  }
  if (!sawMap || !sawView || !sawClose) return fail("incomplete-lifecycle-trace");
  if (mapped || view || callback || activeLoan || !closed) return fail("lifecycle-resources-not-closed");
  if (nameRemoved) return accept(testCase, "name-discovery-target-specific", { normalizedLifecycle: "existing-leases-remain-until-own-close", targetKind: profile.targetKind, discovery: profile.deleteBehavior.discovery, withdrawn, closeOrder });
  if (sawStop) {
    if (callback || !drained || !closeOrder.includes("unregister-callback") || closeOrder.indexOf("stop-access") > closeOrder.indexOf("unregister-callback") || closeOrder.indexOf("unregister-callback") > closeOrder.indexOf("drain") || closeOrder.indexOf("drain") > closeOrder.indexOf("drop-view") || closeOrder.indexOf("drop-view") > closeOrder.indexOf("unmap") || closeOrder.indexOf("unmap") > closeOrder.indexOf("close")) return fail("ffi-close-order-incomplete");
    return accept(testCase, "ffi-lease-close", { callbackAccess: false, closeOrder });
  }
  return accept(testCase, "lifecycle-explicit", { targetKind: profile.targetKind });
}

function providerLogical(testCase, input, profile, layout) {
  const ops = operationsOf(testCase);
  const requirement = layout?.control;
  if (ops.some((operation) => operationName(operation) === "fallback")) {
    const unsupported = requirement && (requiredAtomicSupported(profile, layout) !== undefined);
    if (unsupported) return accept(testCase, "fallback-snapshot", { route: "snapshot-wire-service", fallbackExplicit: true });
  }
  if (ops.some((operation) => operationName(operation) === "require" && operation.backing === "durable") && !profile.backing.durable) return reject(testCase, "durability-unavailable", { provider: profile.objectIdentity });
  if (ops.some((operation) => operationName(operation) === "require" && operation.target === "windows-immediate-unlink") && profile.targetKind === "windows") return reject(testCase, "name-withdrawal-unsupported", { targetKind: profile.targetKind });
  return accept(testCase, "provider-authoritative", { providerBinding: profile.objectIdentity, backing: profile.backing.kind, durable: profile.backing.durable });
}

function logicalOutcome(testCase, profile, layout, schemas) {
  const input = testCase.input ?? {};
  if (testCase.axis === "baseline") return accept(testCase, "snapshot-wire-service", { route: "current-composition" });
  if (testCase.axis === "immutable") return immutableLogical(testCase, input, profile, layout, schemas);
  if (testCase.axis === "channel") return channelLogical(testCase, input, profile, layout, schemas);
  if (testCase.axis === "lifecycle") return lifecycleLogical(testCase, input, profile, layout);
  if (testCase.axis === "provider") return providerLogical(testCase, input, profile, layout);
  return reject(testCase, "axis-invalid");
}

function physicalLayoutResult(testCase, layouts) {
  const layout = resolveLayout(testCase, layouts);
  if (!layout) return { valid: false, code: "layout-missing" };
  const errors = validateLayoutDescriptor(layout, "layout");
  return { valid: errors.length === 0, code: errors.length > 0 ? (errors[0].includes("overlap") ? "layout-overlap" : errors[0].includes("exceeds") || errors[0].includes("overflow") ? "layout-bounds" : "layout-invalid") : undefined };
}

/* The POSIX reducer has its own event vocabulary and lifecycle state. */
export function reducePosixCase(testCase, profile, { layouts, schemas } = {}) {
  const input = testCase.input ?? {};
  const layout = resolveLayout(testCase, layouts);
  const logical = logicalOutcome(testCase, profile, layout, schemas);
  const events = [];
  let views = 0;
  let named = true;
  let objectAlive = true;
  let validated = false;
  let typedViewCreated = false;
  let descriptor = true;
  for (const operation of operationsOf(testCase)) {
    const op = operationName(operation);
    if (op === "map") { views += 1; events.push({ op: profile.backing.kind === "file" ? "mmap-file" : "mmap-shm", address: operation.address ?? null, generation: operation.generation ?? null }); }
    else if (op === "validate") { validated = physicalLayoutResult(testCase, layouts).valid && (testCase.axis === "channel" || !validateHeader(input.header)); events.push({ op: "validate-header-layout", valid: validated }); }
    else if (op === "view") { typedViewCreated = validated; events.push({ op: "typed-view", created: typedViewCreated }); }
    else if (op === "drop-view") events.push({ op: "drop-view" });
    else if (op === "unmap") { views = Math.max(0, views - 1); events.push({ op: "munmap", remainingViews: views }); if (views === 0 && !named) objectAlive = false; }
    else if (op === "close") { descriptor = false; events.push({ op: "close-descriptor", descriptor: true }); if (views === 0 && !named) objectAlive = false; }
    else if (op === "remove-name" || op === "withdraw-name") { named = false; events.push({ op: profile.backing.kind === "shm" ? "shm_unlink" : "unlink-file-name", withdrawal: op === "withdraw-name", refsRetained: views > 0 }); }
    else if (op === "flush-data") events.push({ op: profile.targetKind === "posix" ? "msync" : "FlushViewOfFile", receipt: profile.backing.durable });
    else if (op === "flush-metadata") events.push({ op: "fsync", receipt: profile.backing.durable });
    else if (op === "request-durability") events.push({ op: "durability-request", scope: operation.scope });
    else if (op === "hash") events.push({ op: "generation-hash" });
    else if (op === "receipt") events.push({ op: "durability-receipt", scope: operation.scope ?? null });
    else if (op === "publish-selector") events.push({ op: "release-selector-publish", generation: operation.generation ?? input.header?.generation ?? null });
    else if (op === "flush-selector") events.push({ op: "selector-sync" });
    else if (op === "crash") events.push({ op: "process-crash", process: operation.process ?? null });
    else if (op === "register-callback") events.push({ op: "register-callback" });
    else if (op === "stop-access") events.push({ op: "stop-access" });
    else if (op === "unregister-callback") events.push({ op: "unregister-callback" });
    else if (op === "drain") events.push({ op: "drain" });
    else if (op === "loan") events.push({ op: "loan" });
    else if (op === "return-loan") events.push({ op: "return-loan" });
    else if (op === "sync") events.push({ op: "process-shared-sync", mapped: views > 0 });
    else if (op === "atomic") events.push({ op: "process-shared-atomic", width: layout?.control?.width ?? null, order: layout?.control?.order ?? null });
  }
  return {
    target: "posix",
    provider: profile.objectIdentity,
    physical: { events, named, views, objectAlive, descriptor, typedViewCreated, syncObjectAfterLastUnmap: views === 0 && operationsOf(testCase).some((operation) => operationName(operation) === "sync"), addressesIndependent: profile.addressIndependent },
    logical,
  };
}

/* The Windows reducer models mapping handles and views. It never borrows the
 * POSIX reducer's event names or deletion assumptions. */
export function reduceWindowsCase(testCase, profile, { layouts, schemas } = {}) {
  const input = testCase.input ?? {};
  const layout = resolveLayout(testCase, layouts);
  const logical = logicalOutcome(testCase, profile, layout, schemas);
  const events = [];
  let views = 0;
  let handles = 1;
  let nameOpen = true;
  let objectAlive = true;
  let validated = false;
  let typedViewCreated = false;
  for (const operation of operationsOf(testCase)) {
    const op = operationName(operation);
    if (op === "map") { views += 1; events.push({ op: profile.backing.kind === "file" ? "MapViewOfFile-file" : "MapViewOfFile-pagefile", address: operation.address ?? null, generation: operation.generation ?? null }); }
    else if (op === "validate") { validated = physicalLayoutResult(testCase, layouts).valid && (testCase.axis === "channel" || !validateHeader(input.header)); events.push({ op: "validate-header-layout", valid: validated }); }
    else if (op === "view") { typedViewCreated = validated; events.push({ op: "typed-view", created: typedViewCreated }); }
    else if (op === "drop-view") events.push({ op: "drop-view" });
    else if (op === "unmap") { views = Math.max(0, views - 1); events.push({ op: "UnmapViewOfFile", remainingViews: views }); if (views === 0 && handles === 0) objectAlive = false; }
    else if (op === "close") { handles = Math.max(0, handles - 1); events.push({ op: "CloseHandle", remainingHandles: handles }); if (views === 0 && handles === 0) objectAlive = false; }
    else if (op === "remove-name" || op === "withdraw-name") events.push({ op: "named-object-discovery", supported: false, withdrawal: op === "withdraw-name", refsRetained: views + handles > 0 });
    else if (op === "flush-data") events.push({ op: "FlushViewOfFile", receipt: profile.backing.durable });
    else if (op === "flush-metadata") events.push({ op: "FlushFileBuffers", receipt: profile.backing.durable });
    else if (op === "request-durability") events.push({ op: "durability-request", scope: operation.scope });
    else if (op === "hash") events.push({ op: "generation-hash" });
    else if (op === "receipt") events.push({ op: "durability-receipt", scope: operation.scope ?? null });
    else if (op === "publish-selector") events.push({ op: "release-selector-publish", generation: operation.generation ?? input.header?.generation ?? null });
    else if (op === "flush-selector") events.push({ op: "selector-sync" });
    else if (op === "crash") events.push({ op: "process-crash", process: operation.process ?? null });
    else if (op === "register-callback") events.push({ op: "RegisterCallback" });
    else if (op === "stop-access") events.push({ op: "StopAccess" });
    else if (op === "unregister-callback") events.push({ op: "UnregisterCallback" });
    else if (op === "drain") events.push({ op: "Drain" });
    else if (op === "loan") events.push({ op: "Loan" });
    else if (op === "return-loan") events.push({ op: "ReturnLoan" });
    else if (op === "atomic") events.push({ op: "Interlocked", width: layout?.control?.width ?? null, order: layout?.control?.order ?? null });
  }
  return {
    target: "windows",
    provider: profile.objectIdentity,
    physical: { events, nameOpen, views, handles, objectAlive, typedViewCreated, addressesIndependent: profile.addressIndependent },
    logical,
  };
}

function compactLogical(outcome) {
  return {
    status: outcome.status,
    code: outcome.code,
    route: outcome.route ?? null,
    unknownDurability: outcome.unknownDurability ?? false,
    visibility: outcome.visibility ?? null,
    generation: outcome.generation ?? null,
    normalizedLifecycle: outcome.normalizedLifecycle ?? null,
    previousGenerationPreserved: outcome.previousGenerationPreserved ?? null,
    transferred: outcome.transferred ?? null,
    hiddenRepair: outcome.hiddenRepair ?? null,
    fallbackExplicit: outcome.fallbackExplicit ?? null,
    committedFullSurvived: outcome.committedFullSurvived ?? null,
    unrelatedProcessCrash: outcome.unrelatedProcessCrash ?? null,
    owner: outcome.owner ?? null,
    carrierOutcome: outcome.carrierOutcome ?? null,
    addressesIndependent: outcome.addressesIndependent ?? null,
    publication: outcome.publication ?? null,
    durability: outcome.durability ?? null,
    snapshotBytesEqual: outcome.snapshotBytesEqual ?? null,
    snapshotDigest: outcome.snapshotDigest ?? null,
  };
}

function snapshotBytesForTarget(testCase, target) {
  const byTarget = testCase.input?.snapshotBytesByTarget;
  if (byTarget && Array.isArray(byTarget[target])) return byTarget[target];
  return Array.isArray(testCase.input?.snapshotBytes) ? testCase.input.snapshotBytes : [];
}

export function evaluateIpc1Case(testCase, { providers = {}, layouts = {}, schemas = {} } = {}) {
  const bindings = testCase.providerBindings ?? {};
  const targetIds = Array.isArray(testCase.targets) && testCase.targets.length > 0 ? testCase.targets : TARGETS;
  const projections = [];
  for (const target of targetIds) {
    const binding = bindings[target];
    const profile = providers[binding];
    if (!profile) {
      projections.push({ target, provider: binding ?? null, logical: reject(testCase, "provider-binding-missing"), physical: { target, binding } });
      continue;
    }
    projections.push(target === "posix" ? reducePosixCase(testCase, profile, { layouts, schemas }) : reduceWindowsCase(testCase, profile, { layouts, schemas }));
  }
  const compact = projections.map((projection) => compactLogical(projection.logical));
  const targetEquivalent = compact.length > 0 && compact.every((candidate) => JSON.stringify(candidate) === JSON.stringify(compact[0]));
  const base = projections[0]?.logical ?? reject(testCase, "provider-binding-missing");
  const snapshotBytes = Object.fromEntries(targetIds.map((target) => [target, snapshotBytesForTarget(testCase, target)]));
  const snapshotDigests = Object.fromEntries(Object.entries(snapshotBytes).map(([target, bytes]) => [target, digestText(JSON.stringify(bytes))]));
  const snapshotValues = Object.values(snapshotBytes);
  const snapshotBytesEqual = snapshotValues.length === 0 || snapshotValues.every((bytes) => JSON.stringify(bytes) === JSON.stringify(snapshotValues[0]));
  const snapshotDigest = snapshotDigests[targetIds[0]] ?? digestText("[]");
  for (const projection of projections) {
    projection.physical.snapshotBytes = snapshotBytes[projection.target];
    projection.physical.snapshotDigest = snapshotDigests[projection.target];
    projection.logical.snapshotBytesEqual = snapshotBytesEqual;
  }
  base.snapshotDigest = snapshotDigest;
  if (!snapshotBytesEqual) return reject(testCase, "snapshot-bytes-diverge", { targetEquivalent: false, snapshotBytesEqual: false, snapshotDigest, targetProjections: projections, logical: compactLogical(base) });
  if (!targetEquivalent) return reject(testCase, "target-divergence", { targetEquivalent: false, snapshotBytesEqual, snapshotDigest, targetProjections: projections, logical: compactLogical(base) });
  return { ...base, targetEquivalent, targetProjections: projections, logical: { ...compactLogical(base), snapshotBytesEqual, snapshotDigest } };
}

export function validateIpc1(corpus, { root = process.cwd() } = {}) {
  const errors = [];
  if (corpus?.$schema !== "w-ipc1-mapped-ipc-1") errors.push("IPC1 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input-ipc1") errors.push("IPC1 corpus status must be design-oracle-input-ipc1.");
  if (corpus?.id !== "IPC1" || corpus?.gate !== "IPC0-R1") errors.push("IPC1 corpus must identify CAP0 gate IPC0-R1.");
  validateOfficialSources(corpus, errors);
  errors.push(...validateSchemaRegistry(corpus.schemas, "schemas"));
  const providerErrors = {};
  const objectIdentities = new Set();
  for (const [binding, profile] of Object.entries(corpus.providers ?? {})) {
    providerErrors[binding] = validateProviderProfile(profile, `providers.${binding}`);
    errors.push(...providerErrors[binding]);
    if (objectIdentities.has(profile?.objectIdentity)) errors.push(`providers.${binding}.objectIdentity is duplicated.`);
    objectIdentities.add(profile?.objectIdentity);
  }
  if (Object.keys(corpus.providers ?? {}).length < 6) errors.push("IPC1 providers must separate durable files, volatile mappings, and robust profiles.");
  for (const target of TARGETS) if (!Object.values(corpus.providers ?? {}).some((profile) => profile?.targetKind === target)) errors.push(`IPC1 providers must include ${target} profiles.`);
  for (const [layoutId, layout] of Object.entries(corpus.layouts ?? {})) {
    errors.push(...validateLayoutDescriptor(layout, `layouts.${layoutId}`));
    if (layout?.layoutId !== layoutId) errors.push(`layouts.${layoutId}.layoutId must match its key.`);
    if (!corpus.schemas?.[layout?.schemaId] || corpus.schemas[layout.schemaId].digest !== layout.schemaDigest) errors.push(`layouts.${layoutId}.schemaDigest must match its schema registry entry.`);
  }
  for (const [binding, profile] of Object.entries(corpus.providers ?? {})) {
    for (const entry of profile.allowedLayouts ?? []) if (!corpus.layouts?.[entry.layoutId] || corpus.layouts[entry.layoutId].layoutDigest !== entry.digest) errors.push(`providers.${binding}.allowedLayouts contains an unknown or stale layout binding.`);
    for (const entry of profile.allowedSchemas ?? []) if (!corpus.schemas?.[entry.schemaId] || corpus.schemas[entry.schemaId].digest !== entry.digest) errors.push(`providers.${binding}.allowedSchemas contains an unknown or stale schema binding.`);
  }
  const refs = new Map();
  const refKeys = new Set();
  const symbols = new Set();
  for (const [index, reference] of (corpus.sourceRefs ?? []).entries()) {
    const location = `sourceRefs[${index}]`;
    if (!nonEmpty(reference?.id) || refs.has(reference.id)) errors.push(`${location}.id must be unique.`);
    const file = contained(root, reference?.path);
    if (!file) { errors.push(`${location}.path references a missing or out-of-tree file.`); continue; }
    if (!validDigest(reference.digest) || digestFile(file) !== reference.digest) errors.push(`${location}.digest is stale.`);
    const symbol = reference.symbol;
    if (!nonEmpty(symbol)) errors.push(`${location}.symbol must be non-empty.`);
    const key = `${reference.path}\0${symbol}`;
    if (refKeys.has(key)) errors.push(`${location} duplicates source reference ${key}.`);
    refKeys.add(key);
    refs.set(reference.id, reference);
    if (symbols.has(symbol)) errors.push(`${location}.symbol must be globally unique.`);
    symbols.add(symbol);
    const count = escapedCount(fs.readFileSync(file, "utf8"), symbol);
    if (count !== 1) errors.push(`${location}.symbol must occur exactly once (${count}).`);
  }
  const caseIds = new Set();
  const results = [];
  for (const [index, testCase] of (corpus.cases ?? []).entries()) {
    const location = `cases[${index}]`;
    if (!/^IPC1-[a-z0-9]+(?:-[a-z0-9]+)*$/u.test(testCase?.id ?? "") || caseIds.has(testCase.id)) errors.push(`${location}.id is invalid or duplicated.`);
    caseIds.add(testCase?.id);
    if (!AXES.includes(testCase?.axis)) errors.push(`${location}.axis is invalid.`);
    if (!Array.isArray(testCase?.targets) || JSON.stringify([...testCase.targets].sort()) !== JSON.stringify([...TARGETS].sort())) errors.push(`${location}.targets must include POSIX and Windows.`);
    if (!testCase?.providerBindings || typeof testCase.providerBindings !== "object") errors.push(`${location}.providerBindings must select authoritative provider bindings.`);
    else {
      for (const target of TARGETS) {
        const binding = testCase.providerBindings[target];
        if (!nonEmpty(binding) || !corpus.providers?.[binding]) errors.push(`${location}.providerBindings.${target} is unknown or missing.`);
        else if (corpus.providers[binding].targetKind !== target) errors.push(`${location}.providerBindings.${target} targets the wrong kind.`);
      }
      if (new Set(Object.values(testCase.providerBindings)).size !== TARGETS.length) errors.push(`${location}.providerBindings must not duplicate a provider binding.`);
    }
    if (!Array.isArray(testCase?.sourceRefIds) || testCase.sourceRefIds.length === 0) errors.push(`${location}.sourceRefIds must be non-empty.`);
    if (Array.isArray(testCase?.sourceRefIds) && new Set(testCase.sourceRefIds).size !== testCase.sourceRefIds.length) errors.push(`${location}.sourceRefIds must not contain duplicates.`);
    for (const refId of testCase?.sourceRefIds ?? []) if (!refs.has(refId)) errors.push(`${location}.sourceRefIds references unknown ${refId}.`);
    if (!Array.isArray(testCase?.operations)) errors.push(`${location}.operations must be an array.`);
    else for (const [operationIndex, operation] of testCase.operations.entries()) {
      const name = operationName(operation);
      if (!VALID_OPERATIONS.has(name)) errors.push(`${location}.operations[${operationIndex}] is unknown.`);
      if (operation && typeof operation === "object" && operation.slot !== undefined) errors.push(`${location}.operations[${operationIndex}].slot is a legacy selector; crash actor and state are derived.`);
    }
    findLegacyFields(testCase?.input, `${location}.input`, errors);
    findLegacyFields(testCase?.operations, `${location}.operations`, errors);
    if (testCase?.axis !== "baseline" && !nonEmpty(testCase?.input?.layoutId)) errors.push(`${location}.input.layoutId is required for mapped candidates.`);
    if (testCase?.input?.layoutId && !corpus.layouts?.[testCase.input.layoutId]) errors.push(`${location}.input.layoutId is unknown.`);
    const evaluated = evaluateIpc1Case(testCase, { providers: corpus.providers, layouts: corpus.layouts, schemas: corpus.schemas });
    if (!testCase.expected || testCase.expected.status !== evaluated.status || testCase.expected.code !== evaluated.code) errors.push(`${location}.expected does not match the independent result.`);
    results.push(evaluated);
  }
  for (const id of corpus.requiredCaseIds ?? []) if (!caseIds.has(id)) errors.push(`IPC1 required case is missing: ${id}.`);
  return { errors, results };
}

export function deriveIpc1(corpus) {
  return (corpus.cases ?? []).map((testCase) => evaluateIpc1Case(testCase, { providers: corpus.providers, layouts: corpus.layouts, schemas: corpus.schemas }));
}

export { digestText };
