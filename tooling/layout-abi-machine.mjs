const NOTE_SCHEMA = "w-abi-logical-note-l0-v1";

const DEFAULT_LIMITS = Object.freeze({
  noteBytes: 64 * 1024 * 1024,
  stringBytes: 1024 * 1024,
  representations: 1_048_576,
  symbols: 1_048_576,
  fieldsPerRecord: 65_535,
  nesting: 64,
});

const SUPPORTED_REQUIRED_FEATURES = new Set([
  "exact-link-v1",
  "representation-map-v1",
  "semantic-import-key-v1",
]);

const C_SCALARS = new Set([
  "c.bool",
  "c.char",
  "c.schar",
  "c.uchar",
  "c.short",
  "c.ushort",
  "c.int",
  "c.uint",
  "c.long",
  "c.ulong",
  "c.longLong",
  "c.ulongLong",
  "c.float",
  "c.double",
  "c.size",
  "c.ptrdiff",
]);

export class LayoutAbiError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function fail(code) {
  throw new LayoutAbiError(code);
}

function isRecord(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function nonEmptyString(value) {
  return typeof value === "string" && value.length > 0;
}

function utf8Bytes(value) {
  return new TextEncoder().encode(value).byteLength;
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (!isRecord(value)) return value;

  return Object.fromEntries(
    Object.keys(value)
      .sort()
      .map((key) => [key, canonical(value[key])]),
  );
}

function same(left, right) {
  return JSON.stringify(canonical(left)) === JSON.stringify(canonical(right));
}

function clone(value) {
  return structuredClone(value);
}

function mergeFixture(base, patch) {
  if (!isRecord(patch)) return clone(base);
  const result = clone(base);
  for (const [key, value] of Object.entries(patch)) {
    if (isRecord(value) && isRecord(result[key])) {
      result[key] = mergeFixture(result[key], value);
    } else {
      result[key] = clone(value);
    }
  }
  return result;
}

function nestingDepth(value, depth = 0) {
  if (Array.isArray(value)) {
    return value.reduce(
      (maximum, item) => Math.max(maximum, nestingDepth(item, depth + 1)),
      depth,
    );
  }
  if (isRecord(value)) {
    return Object.values(value).reduce(
      (maximum, item) => Math.max(maximum, nestingDepth(item, depth + 1)),
      depth,
    );
  }
  return depth;
}

function validateStrings(value, limit) {
  if (typeof value === "string") {
    if (utf8Bytes(value) > limit) fail("abiReaderLimitExceeded");
    return;
  }
  if (Array.isArray(value)) {
    value.forEach((item) => validateStrings(item, limit));
    return;
  }
  if (isRecord(value)) {
    for (const [key, item] of Object.entries(value)) {
      validateStrings(key, limit);
      validateStrings(item, limit);
    }
  }
}

function indexUnique(items, key, duplicateCode) {
  const result = new Map();
  for (const item of items) {
    const identity = item?.[key];
    if (!nonEmptyString(identity)) fail("malformedAbiNote");
    if (result.has(identity)) fail(duplicateCode);
    result.set(identity, item);
  }
  return result;
}

function validateUniqueStrings(items, duplicateCode, malformedCode = "malformedAbiNote") {
  const seen = new Set();
  for (const item of items) {
    if (!nonEmptyString(item)) fail(malformedCode);
    if (seen.has(item)) fail(duplicateCode);
    seen.add(item);
  }
}

function validateWAbiDescriptor(descriptor) {
  if (!isRecord(descriptor)) fail("malformedAbiNote");
  const required = [
    "abiRevision",
    "producer",
    "target",
    "dataLayout",
    "callingConventionRevision",
    "representationPolicyRevision",
    "panicModel",
    "cleanupModel",
  ];
  if (!required.every((field) => nonEmptyString(descriptor[field]))) {
    fail("malformedAbiNote");
  }
  if (!isRecord(descriptor.runtimeAbiRevisions)) fail("malformedAbiNote");
  if (!Array.isArray(descriptor.hardeningAbiFacts)) fail("malformedAbiNote");
  for (const [name, revision] of Object.entries(descriptor.runtimeAbiRevisions)) {
    if (!nonEmptyString(name) || !nonEmptyString(revision)) fail("malformedAbiNote");
  }
  validateUniqueStrings(descriptor.hardeningAbiFacts, "duplicateHardeningAbiFact");
}

export function validateAbiNote(note, limits = DEFAULT_LIMITS) {
  if (note === null || note === undefined) fail("missingAbiNote");
  if (!isRecord(note)) fail("malformedAbiNote");
  if (note.schema !== NOTE_SCHEMA) fail("unsupportedAbiNoteSchema");

  const effectiveLimits = { ...DEFAULT_LIMITS, ...limits };
  const encodedBytes = utf8Bytes(JSON.stringify(note));
  if (encodedBytes > effectiveLimits.noteBytes) fail("abiReaderLimitExceeded");
  if (nestingDepth(note) > effectiveLimits.nesting) fail("abiReaderLimitExceeded");
  validateStrings(note, effectiveLimits.stringBytes);

  if (!Array.isArray(note.requiredFeatures)) fail("malformedAbiNote");
  validateUniqueStrings(note.requiredFeatures, "duplicateRequiredAbiFeature");
  for (const feature of note.requiredFeatures) {
    if (!SUPPORTED_REQUIRED_FEATURES.has(feature)) {
      fail("unknownRequiredAbiFeature");
    }
  }

  validateWAbiDescriptor(note.wAbi);
  if (!nonEmptyString(note.semanticInterfaceKey)) fail("malformedAbiNote");
  if (!nonEmptyString(note.recipeDigest)) fail("malformedAbiNote");
  if (!Array.isArray(note.representations) || !Array.isArray(note.symbols)) {
    fail("malformedAbiNote");
  }
  if (note.representations.length > effectiveLimits.representations) {
    fail("abiReaderLimitExceeded");
  }
  if (note.symbols.length > effectiveLimits.symbols) {
    fail("abiReaderLimitExceeded");
  }

  const representations = indexUnique(
    note.representations,
    "id",
    "duplicateRepresentationIdentity",
  );
  for (const representation of representations.values()) {
    if (!nonEmptyString(representation.fingerprint)) fail("malformedAbiNote");
    if (!Array.isArray(representation.fields)) fail("malformedAbiNote");
    if (representation.fields.length > effectiveLimits.fieldsPerRecord) {
      fail("abiReaderLimitExceeded");
    }
  }

  const symbols = indexUnique(note.symbols, "name", "duplicateAbiSymbol");
  for (const symbol of symbols.values()) {
    if (!["wExact", "foreignC"].includes(symbol.boundary)) {
      fail("malformedAbiNote");
    }
    if (!nonEmptyString(symbol.semanticSignature)) fail("malformedAbiNote");
    if (!nonEmptyString(symbol.physicalSignature)) fail("malformedAbiNote");
    if (!Array.isArray(symbol.representations)) fail("malformedAbiNote");
    validateUniqueStrings(symbol.representations, "duplicateSymbolRepresentation");
    for (const identity of symbol.representations) {
      if (!representations.has(identity)) fail("missingRepresentation");
    }
  }

  if (!Array.isArray(note.imports)) fail("malformedAbiNote");
  const imports = indexUnique(note.imports, "id", "duplicateAbiImport");
  for (const imported of imports.values()) {
    if (
      !nonEmptyString(imported.symbol) ||
      !nonEmptyString(imported.providerInterfaceKey) ||
      !nonEmptyString(imported.semanticSignature) ||
      !nonEmptyString(imported.physicalSignature) ||
      !isRecord(imported.representations)
    ) {
      fail("malformedAbiNote");
    }
    for (const [identity, fingerprint] of Object.entries(imported.representations)) {
      if (!nonEmptyString(identity) || !nonEmptyString(fingerprint)) {
        fail("malformedAbiNote");
      }
    }
  }
  if (note.runtimeRequirements !== undefined) {
    if (!Array.isArray(note.runtimeRequirements)) fail("malformedAbiNote");
    validateUniqueStrings(note.runtimeRequirements, "duplicateRuntimeRequirement");
  }
  return true;
}

function noteIndexes(note) {
  return {
    representations: new Map(note.representations.map((entry) => [entry.id, entry])),
    symbols: new Map(note.symbols.map((entry) => [entry.name, entry])),
    imports: new Map(note.imports.map((entry) => [entry.id, entry])),
  };
}

export function linkWExact(consumer, provider, importId) {
  validateAbiNote(consumer);
  validateAbiNote(provider);

  if (!same(consumer.wAbi, provider.wAbi)) fail("abiKeyMismatch");

  const consumerIndexes = noteIndexes(consumer);
  const providerIndexes = noteIndexes(provider);
  const importContract = consumerIndexes.imports.get(importId);
  if (!importContract) fail("missingAbiImport");
  if (provider.semanticInterfaceKey !== importContract.providerInterfaceKey) {
    fail("providerInterfaceMismatch");
  }

  const symbol = providerIndexes.symbols.get(importContract.symbol);
  if (!symbol || symbol.boundary !== "wExact") fail("missingAbiSymbol");
  if (symbol.semanticSignature !== importContract.semanticSignature) {
    fail("semanticCallMismatch");
  }
  if (symbol.physicalSignature !== importContract.physicalSignature) {
    fail("physicalCallMismatch");
  }

  const expectedIdentities = Object.keys(importContract.representations ?? {}).sort();
  const actualIdentities = [...symbol.representations].sort();
  if (!same(expectedIdentities, actualIdentities)) fail("representationSetMismatch");

  for (const identity of expectedIdentities) {
    const providerRepresentation = providerIndexes.representations.get(identity);
    if (!providerRepresentation) fail("missingRepresentation");
    if (providerRepresentation.fingerprint !== importContract.representations[identity]) {
      fail("representationMismatch");
    }
  }

  return true;
}

export function comparePhysicalCalls(caller, callee) {
  if (!isRecord(caller) || !isRecord(callee)) fail("malformedPhysicalCall");
  if (caller.callingConvention !== callee.callingConvention) {
    fail("physicalCallMismatch");
  }
  if (!same(caller.result, callee.result)) fail("physicalCallMismatch");
  if (!same(caller.parameters, callee.parameters)) fail("physicalCallMismatch");
  if (!same(caller.hidden, callee.hidden)) fail("physicalCallMismatch");
  return true;
}

export function chooseAbiArtifact({
  exactArtifact,
  sourceAvailable,
  declaredBoundary,
  boundaryArtifact,
}) {
  if (exactArtifact === true) return "exactArtifact";
  if (sourceAvailable === true) return "rebuildSource";
  if (boundaryArtifact === true && declaredBoundary === true) {
    return "canonicalBoundary";
  }
  if (boundaryArtifact === true) fail("implicitBoundaryFallback");
  fail("noCompatibleAbiArtifact");
}

function validateRuntimeContract(facade) {
  if (facade.hiddenRuntimeContext === true) fail("cFacadeRuntimeContextInvalid");
  const contextCount = facade.parameters.filter(
    (carrier) => carrier.kind === "pointer" && carrier.role === "runtimeContext",
  ).length;
  if (facade.runtime === "none") {
    if (
      facade.contextual === true ||
      contextCount !== 0 ||
      facade.createContext !== undefined ||
      facade.destroyContext !== undefined
    ) {
      fail("cFacadeRuntimeContextInvalid");
    }
    return;
  }
  if (facade.runtime !== "explicitContext") fail("cFacadeRuntimeContextInvalid");
  if (!nonEmptyString(facade.createContext) || !nonEmptyString(facade.destroyContext)) {
    fail("cFacadeRuntimeContextInvalid");
  }
  if (facade.contextual === true) {
    if (contextCount !== 1) fail("cFacadeRuntimeContextInvalid");
  } else if (contextCount !== 0) {
    fail("cFacadeRuntimeContextInvalid");
  }
}

function validateCCarrier(carrier, position) {
  if (!isRecord(carrier)) fail("cCarrierRequired");
  switch (carrier.kind) {
    case "void":
      return;
    case "scalar":
      if (!C_SCALARS.has(carrier.type)) fail("cCarrierRequired");
      return;
    case "record":
      if (carrier.representation !== "cCanonical") fail("cCarrierRequired");
      return;
    case "pointer": {
      if (!nonEmptyString(carrier.pointee)) fail("cCarrierRequired");
      if (carrier.ownership === "borrowed") {
        if (position === "result") fail("borrowedCarrierMayEscape");
        if (carrier.retention !== "call") fail("borrowedCarrierMayEscape");
        if (carrier.sequence === true && !nonEmptyString(carrier.extentParameter)) {
          fail("borrowedCarrierNeedsExtent");
        }
        return;
      }
      if (carrier.ownership === "owned") {
        if (carrier.callerFree === true) fail("callerFreeForbidden");
        const hasDestroySymbol = nonEmptyString(carrier.destroySymbol);
        const hasContextDrop = carrier.dropCallback === true && carrier.dropContext === true;
        if (hasDestroySymbol === hasContextDrop) {
          fail("ownedCarrierNeedsDestroy");
        }
        return;
      }
      if (["opaque", "out", "runtimeContext"].includes(carrier.role)) return;
      fail("cCarrierRequired");
      return;
    }
    case "callback":
      if (carrier.retention === "call") return;
      if (
        carrier.retention !== "persistent" ||
        carrier.context !== true ||
        carrier.pinnedLease !== true ||
        carrier.destroy !== true ||
        carrier.unregister !== true
      ) {
        fail("callbackLeaseIncomplete");
      }
      return;
    case "validatedInteger":
      if (!C_SCALARS.has(carrier.type) || carrier.validated !== true) {
        fail("foreignValueNeedsValidation");
      }
      return;
    default:
      fail("cCarrierRequired");
  }
}

export function validateCFacade(facade) {
  if (!isRecord(facade)) fail("malformedCFacade");
  if (facade.unsafe !== true || facade.abi !== "c") fail("cFacadeRequiresUnsafe");
  if (facade.generic || facade.capture || facade.async || facade.throws) {
    fail("cFacadeQualifierUnsupported");
  }
  if (facade.panic === "forbid") {
    if (facade.panicFree !== true) fail("cFacadePanicPolicyInvalid");
  } else if (facade.panic !== "abortProcess") {
    fail("cFacadePanicPolicyInvalid");
  }
  if (!Array.isArray(facade.parameters)) fail("malformedCFacade");
  validateRuntimeContract(facade);
  facade.parameters.forEach((carrier) => validateCCarrier(carrier, "parameter"));
  validateCCarrier(facade.result, "result");
  return true;
}

export function validateCRecord(record) {
  if (!isRecord(record)) fail("malformedCRecord");
  if (record.kind === "union") fail("foreignUnionNeedsWrapper");
  if (record.kind !== "struct") fail("malformedCRecord");
  if (record.manualPacked === true) fail("manualPackedLayoutUnsupported");
  if (!["headerImport", "generatedFacade"].includes(record.origin)) {
    fail("cRecordNeedsTargetLayout");
  }
  if (!nonEmptyString(record.target) || !nonEmptyString(record.layoutDigest)) {
    fail("cRecordNeedsTargetLayout");
  }
  if (!Number.isInteger(record.size) || record.size <= 0) fail("malformedCRecord");
  if (
    !Number.isInteger(record.alignment) ||
    record.alignment <= 0 ||
    (record.alignment & (record.alignment - 1)) !== 0 ||
    record.size % record.alignment !== 0
  ) {
    fail("malformedCRecord");
  }
  if (!Array.isArray(record.fields)) fail("malformedCRecord");

  const fields = [...record.fields].sort((left, right) => left.offset - right.offset);
  const fieldNames = new Set();
  let previousEnd = 0;
  for (const field of fields) {
    if (
      !nonEmptyString(field.name) ||
      !Number.isInteger(field.offset) ||
      !Number.isInteger(field.size) ||
      !Number.isInteger(field.alignment) ||
      field.offset < 0 ||
      field.size <= 0 ||
      field.alignment <= 0 ||
      (field.alignment & (field.alignment - 1)) !== 0 ||
      field.offset + field.size > record.size
    ) {
      fail("malformedCRecord");
    }
    if (fieldNames.has(field.name)) fail("duplicateCRecordField");
    fieldNames.add(field.name);
    if (field.bitField === true || field.flexibleArray === true) {
      fail("foreignCFieldNeedsWrapper");
    }
    if (field.offset < previousEnd && field.size > 0) fail("cRecordOverlap");
    previousEnd = Math.max(previousEnd, field.offset + field.size);
    const aligned = field.offset % field.alignment === 0;
    if (!aligned && record.origin === "generatedFacade") {
      fail("generatedCRecordMustBeNatural");
    }
    if (!aligned && field.borrowable === true) fail("unalignedBorrowForbidden");
  }
  return true;
}

export function validateBoundaryValue(value) {
  switch (value.boundary) {
    case "internal":
      return true;
    case "wExact":
      if (!nonEmptyString(value.representationFingerprint)) {
        fail("missingRepresentation");
      }
      return true;
    case "foreignC":
      if (!["cScalar", "cRecord", "cPointer", "cFunction"].includes(value.carrier)) {
        fail("cCarrierRequired");
      }
      return true;
    case "wire":
    case "component":
      if (!nonEmptyString(value.schemaDigest) || value.containsPointer === true) {
        fail("schemaBoundaryRequired");
      }
      return true;
    default:
      fail("unknownAbiBoundary");
  }
}

export function validateForeignRecovery(recovery) {
  if (recovery.target === "rawPointer") {
    if (recovery.source !== "cPointer") fail("foreignPointerProvenanceMissing");
    return true;
  }
  if (recovery.target === "safeBorrow") {
    if (
      recovery.source !== "cPointer" ||
      recovery.ownerProof !== true ||
      recovery.lifetimeProof !== true ||
      recovery.boundsProof !== true ||
      recovery.alignmentProof !== true ||
      recovery.noEscape !== true
    ) {
      fail("foreignBorrowNeedsProof");
    }
    return true;
  }
  if (["closedEnum", "refinement"].includes(recovery.target)) {
    if (recovery.source !== "cInteger" || recovery.validated !== true) {
      fail("foreignValueNeedsValidation");
    }
    return true;
  }
  fail("malformedForeignRecovery");
}

export function validateHeaderPair(pair) {
  if (
    !nonEmptyString(pair.headerTarget) ||
    !nonEmptyString(pair.libraryTarget) ||
    !nonEmptyString(pair.headerDigest) ||
    !nonEmptyString(pair.indexedHeaderDigest) ||
    !nonEmptyString(pair.libraryDigest) ||
    !nonEmptyString(pair.indexedLibraryDigest) ||
    pair.headerTarget !== pair.libraryTarget ||
    pair.headerDigest !== pair.indexedHeaderDigest ||
    pair.libraryDigest !== pair.indexedLibraryDigest
  ) {
    fail("headerSliceMismatch");
  }
  if (pair.headerHasTimestamp === true || pair.headerHasLocalPath === true) {
    fail("nonReproducibleCHeader");
  }
  return true;
}

function freshState() {
  return {
    artifacts: {},
    links: [],
    choices: [],
    validations: [],
  };
}

function summarizeState(state) {
  return {
    schema: "w-layout-abi-state-l0",
    artifacts: Object.keys(state.artifacts).sort(),
    links: clone(state.links),
    choices: clone(state.choices),
    validations: clone(state.validations),
  };
}

function fixture(fixtures, family, name) {
  const value = fixtures?.[family]?.[name];
  if (value === undefined) fail("missingLayoutAbiFixture");
  return clone(value);
}

function operationFixture(fixtures, family, operation) {
  return mergeFixture(fixture(fixtures, family, operation.fixture), operation.patch);
}

function applyOperation(state, operation, fixtures) {
  switch (operation.op) {
    case "registerArtifact": {
      const note = operationFixture(fixtures, "artifacts", operation);
      validateAbiNote(note, operation.limits);
      const identity = operation.as ?? operation.fixture;
      if (state.artifacts[identity]) fail("duplicateArtifactIdentity");
      state.artifacts[identity] = note;
      return;
    }
    case "linkWExact": {
      const consumer = state.artifacts[operation.consumer];
      const provider = state.artifacts[operation.provider];
      if (!consumer || !provider) fail("missingArtifact");
      linkWExact(consumer, provider, operation.importId);
      state.links.push(`${operation.consumer}->${operation.provider}:${operation.importId}`);
      return;
    }
    case "comparePhysicalCalls": {
      comparePhysicalCalls(
        fixture(fixtures, "callShapes", operation.caller),
        fixture(fixtures, "callShapes", operation.callee),
      );
      state.validations.push(`call:${operation.caller}=${operation.callee}`);
      return;
    }
    case "chooseArtifact": {
      const choice = chooseAbiArtifact(operation);
      state.choices.push(choice);
      return;
    }
    case "validateCFacade":
      validateCFacade(operationFixture(fixtures, "cFacades", operation));
      state.validations.push(`c-facade:${operation.fixture}`);
      return;
    case "validateCRecord":
      validateCRecord(operationFixture(fixtures, "cRecords", operation));
      state.validations.push(`c-record:${operation.fixture}`);
      return;
    case "validateBoundaryValue":
      validateBoundaryValue(operationFixture(fixtures, "boundaryValues", operation));
      state.validations.push(`boundary:${operation.fixture}`);
      return;
    case "validateForeignRecovery":
      validateForeignRecovery(operationFixture(fixtures, "foreignRecoveries", operation));
      state.validations.push(`foreign:${operation.fixture}`);
      return;
    case "validateHeaderPair":
      validateHeaderPair(operationFixture(fixtures, "headerPairs", operation));
      state.validations.push(`header:${operation.fixture}`);
      return;
    default:
      fail("unknownLayoutAbiOperation");
  }
}

export function validateLayoutAbiOperation(operation) {
  if (!isRecord(operation) || !nonEmptyString(operation.op)) return false;
  switch (operation.op) {
    case "registerArtifact":
      return nonEmptyString(operation.fixture) &&
        (operation.as === undefined || nonEmptyString(operation.as)) &&
        (operation.patch === undefined || isRecord(operation.patch));
    case "linkWExact":
      return ["consumer", "provider", "importId"].every((field) =>
        nonEmptyString(operation[field]),
      );
    case "comparePhysicalCalls":
      return nonEmptyString(operation.caller) && nonEmptyString(operation.callee);
    case "chooseArtifact":
      return ["exactArtifact", "sourceAvailable", "declaredBoundary", "boundaryArtifact"]
        .every((field) => typeof operation[field] === "boolean");
    case "validateCFacade":
    case "validateCRecord":
    case "validateBoundaryValue":
    case "validateForeignRecovery":
    case "validateHeaderPair":
      return nonEmptyString(operation.fixture) &&
        (operation.patch === undefined || isRecord(operation.patch));
    default:
      return false;
  }
}

export function runLayoutAbiProgram(operations, fixtures = {}) {
  const state = freshState();
  const trace = [];

  for (const [index, operation] of operations.entries()) {
    const before = summarizeState(state);
    try {
      applyOperation(state, operation, fixtures);
      trace.push({ index, operation: clone(operation), before, after: summarizeState(state) });
    } catch (error) {
      if (!(error instanceof LayoutAbiError)) throw error;
      trace.push({ index, operation: clone(operation), before, rejected: error.code });
      return {
        status: "rejected",
        code: error.code,
        operation: index,
        state: summarizeState(state),
        trace,
      };
    }
  }

  return { status: "accepted", state: summarizeState(state), trace };
}

export const layoutAbiConstants = Object.freeze({
  noteSchema: NOTE_SCHEMA,
  defaultLimits: DEFAULT_LIMITS,
});
