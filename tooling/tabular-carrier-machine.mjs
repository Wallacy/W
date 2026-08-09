// Independent TAB0 host model. It validates design operations and never executes W.
const COPY_POLICIES = new Set(["never", "ifNeeded", "always"]);
const TRUST_LEVELS = new Set(["trustedInProcess", "untrusted"]);
const ENCODINGS = new Set(["plain", "dictionary", "runEnd"]);
const DEVICES = new Set(["cpu", "gpu"]);
const REQUIRED_LIMIT_KEYS = [
  "rows",
  "columns",
  "fields",
  "buffers",
  "totalBytes",
  "allocationBytes",
  "nesting",
  "metadataBytes",
  "stringBytes",
  "chunks",
];

export class TabularCarrierError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function fail(code) {
  throw new TabularCarrierError(code);
}

function isRecord(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function clone(value) {
  return structuredClone(value);
}

function utf8Bytes(value) {
  return new TextEncoder().encode(value).byteLength;
}

function finiteNonNegativeInteger(value) {
  return Number.isSafeInteger(value) && value >= 0;
}

function nonEmptyString(value) {
  return typeof value === "string" && value.length > 0;
}

function validUtf8String(value) {
  if (typeof value !== "string") return false;
  try {
    const bytes = new TextEncoder().encode(value);
    return new TextDecoder("utf-8", { fatal: true }).decode(bytes) === value;
  } catch {
    return false;
  }
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

function schemaFields(schema) {
  if (!isRecord(schema) || !Array.isArray(schema.fields)) fail("invalidSchema");
  return schema.fields;
}

function validateField(field, index, limits) {
  if (!isRecord(field) || !validUtf8String(field.name) || !nonEmptyString(field.name) || !nonEmptyString(field.type)) {
    fail("invalidSchemaField");
  }
  if (utf8Bytes(field.name) > limits.stringBytes) fail("limitStringBytes");
  if (field.nullable !== true && field.nullable !== false) fail("invalidNullability");
  if (field.refinement !== undefined && !nonEmptyString(field.refinement)) {
    fail("invalidRefinement");
  }
  if (field.extensions !== undefined && !Array.isArray(field.extensions)) {
    fail("invalidExtensions");
  }
  validateExtensions(field.extensions ?? [], limits);
  if (index >= limits.fields) fail("limitFields");
}

function validateExtensions(extensions, limits) {
  if (!Array.isArray(extensions)) fail("invalidExtensions");
  for (const extension of extensions) {
    if (
      !isRecord(extension) ||
      !validUtf8String(extension.id) ||
      !nonEmptyString(extension.id) ||
      !finiteNonNegativeInteger(extension.version) ||
      !isRecord(extension.parameters)
    ) {
      fail("invalidExtension");
    }
    if (utf8Bytes(JSON.stringify(extension.parameters)) > limits.metadataBytes) {
      fail("limitMetadataBytes");
    }
  }
}

function validateSchema(schema, limits) {
  if (!isRecord(schema) || !Array.isArray(schema.fields)) fail("invalidSchema");
  if (schema.fields.length > limits.fields) fail("limitFields");
  const names = new Set();
  schema.fields.forEach((field, index) => {
    validateField(field, index, limits);
    if (names.has(field.name)) fail("duplicateFieldName");
    names.add(field.name);
  });
  if (schema.metadata !== undefined) {
    if (!isRecord(schema.metadata)) fail("invalidSchemaMetadata");
    const bytes = utf8Bytes(JSON.stringify(schema.metadata));
    if (bytes > limits.metadataBytes) fail("limitMetadataBytes");
  }
  validateExtensions(schema.extensions ?? [], limits);
  return schema;
}

function schemaHasExtensions(schema) {
  if (Array.isArray(schema.extensions) && schema.extensions.length > 0) return true;
  return schemaFields(schema).some((field) => Array.isArray(field.extensions) && field.extensions.length > 0);
}

export function schemaIdentity(schema) {
  if (!isRecord(schema) || !Array.isArray(schema.fields)) fail("invalidSchema");
  return JSON.stringify(
    {
      fields: schemaFields(schema).map((field) => ({
        name: field.name,
        type: field.type,
        nullable: field.nullable,
        refinement: field.refinement ?? null,
        extensions: canonical(field.extensions ?? []),
      })),
      extensions: canonical(schema.extensions ?? []),
    },
  );
}

function validateLimits(limits) {
  if (!isRecord(limits)) fail("invalidLimits");
  for (const key of REQUIRED_LIMIT_KEYS) {
    if (!finiteNonNegativeInteger(limits[key])) fail("invalidLimits");
  }
  return limits;
}

export const DEFAULT_LIMITS = Object.freeze({
  rows: 1_000_000,
  columns: 16_384,
  fields: 16_384,
  buffers: 1_000_000,
  totalBytes: 1_073_741_824,
  allocationBytes: 1_073_741_824,
  nesting: 64,
  metadataBytes: 1_048_576,
  stringBytes: 1_048_576,
  chunks: 1_000_000,
});

function effectiveLimits(override) {
  return validateLimits({ ...DEFAULT_LIMITS, ...(override ?? {}) });
}

function valueIsNull(value) {
  return value === null;
}

function valueIsNaN(value) {
  return (
    (typeof value === "number" && Number.isNaN(value)) ||
    (isRecord(value) && value.$value === "NaN")
  );
}

function stringPayloadBytes(value, fieldType) {
  if (!String(fieldType).includes("String")) return 0;
  if (typeof value === "string") return utf8Bytes(value);
  if (Array.isArray(value)) return value.reduce((total, item) => total + stringPayloadBytes(item, fieldType), 0);
  if (isRecord(value)) {
    return Object.values(value).reduce((total, item) => total + stringPayloadBytes(item, fieldType), 0);
  }
  return 0;
}

function validateColumn(column, field, rowCount, limits) {
  if (!isRecord(column) || !Array.isArray(column.values)) fail("invalidColumn");
  if (column.values.length !== rowCount) fail("unequalColumnLength");
  if (column.encoding !== undefined && !ENCODINGS.has(column.encoding)) {
    fail("unsupportedEncoding");
  }
  if (column.encoding === "runEnd") {
    fail("encodingNeedsMaterialization");
  }
  if (column.sourceEncoding !== undefined && !ENCODINGS.has(column.sourceEncoding)) {
    fail("unsupportedEncoding");
  }
  if (column.sourceEncoding === "runEnd" && (column.encoding !== "plain" || column.materialized !== true)) {
    fail("encodingNeedsMaterialization");
  }
  if (column.device !== undefined && !DEVICES.has(column.device)) fail("invalidDevice");
  if (!finiteNonNegativeInteger(column.bufferCount)) {
    fail("invalidBufferCount");
  }
  if (column.bufferCount > limits.buffers) fail("limitBuffers");
  let stringBytes = 0;
  for (const value of column.values) {
    if (valueIsNull(value) && !field.nullable) fail("nullInNonNullableField");
    stringBytes += stringPayloadBytes(value, field.type);
    if (!Number.isSafeInteger(stringBytes)) fail("arithmeticOverflow");
  }
  if (column.physicalNulls !== undefined) {
    if (!Array.isArray(column.physicalNulls) || column.physicalNulls.length !== rowCount) {
      fail("invalidPhysicalNulls");
    }
  }
  return { bufferCount: column.bufferCount, stringBytes };
}

function columnMap(columns) {
  const result = new Map();
  for (const column of columns) {
    if (!isRecord(column) || !nonEmptyString(column.name)) fail("invalidColumn");
    if (result.has(column.name)) fail("duplicateColumnName");
    result.set(column.name, column);
  }
  return result;
}

function validateBatchPayload(batch, schema, limits) {
  if (!isRecord(batch)) fail("invalidBatch");
  if (!finiteNonNegativeInteger(batch.rows)) fail("invalidRowCount");
  if (batch.rows > limits.rows) fail("limitRows");
  if (!Array.isArray(batch.columns)) fail("invalidColumns");
  if (batch.columns.length > limits.columns) fail("limitColumns");
  if (!finiteNonNegativeInteger(batch.totalBytes) || !finiteNonNegativeInteger(batch.allocationBytes)) {
    fail("invalidBatchBytes");
  }
  if (batch.totalBytes > limits.totalBytes) fail("limitTotalBytes");
  if (batch.allocationBytes > limits.allocationBytes) fail("limitAllocationBytes");
  const fields = schemaFields(schema);
  if (batch.columns.length !== fields.length) fail("columnFieldMismatch");
  const columns = columnMap(batch.columns);
  if (columns.size !== fields.length) fail("columnFieldMismatch");
  let aggregateBuffers = 0;
  let stringBytes = 0;
  for (const field of fields) {
    const column = columns.get(field.name);
    if (!column) fail("missingColumn");
    const facts = validateColumn(column, field, batch.rows, limits);
    aggregateBuffers += facts.bufferCount;
    stringBytes += facts.stringBytes;
    if (!Number.isSafeInteger(aggregateBuffers) || !Number.isSafeInteger(stringBytes)) {
      fail("arithmeticOverflow");
    }
    if (column.device !== undefined && column.device !== (batch.device ?? "cpu")) {
      fail("columnDeviceMismatch");
    }
  }
  if (aggregateBuffers > limits.buffers) fail("limitBuffers");
  if (stringBytes > limits.stringBytes) fail("limitStringBytes");
  if (fields.length === 0 && batch.explicitRows !== true) fail("emptySchemaNeedsExplicitRows");
  const clonedColumns = batch.columns.map(clone);
  return {
    ...batch,
    columns: clonedColumns,
    columnIndex: new Map(clonedColumns.map((column) => [column.name, column])),
  };
}

function validateRowType(rowType, schema, staticOnly = true) {
  if (!isRecord(rowType) || rowType.kind !== "struct" || rowType.protocol !== "data.Row") {
    fail("invalidRowType");
  }
  if (rowType.nullable === true) fail("nullableTopLevelRow");
  if (!Array.isArray(rowType.fields)) fail("invalidRowType");
  const unsupported = new Set([
    "Any",
    "service",
    "object",
    "task",
    "channel",
    "lock",
    "pointer",
    "ref",
    "view",
    "function",
    "closure",
    "foreignHandle",
  ]);
  for (const field of rowType.fields) {
    if (!isRecord(field) || !nonEmptyString(field.name) || !nonEmptyString(field.type)) {
      fail("unsupportedRowMember");
    }
    if (field.nullable !== true && field.nullable !== false) fail("invalidRowNullability");
    if (unsupported.has(field.type) || field.unsupported === true) fail("unsupportedRowMember");
  }
  if (staticOnly && rowType.synthesis !== "stored-fields-order") fail("rowSynthesisUnavailable");
  return rowType;
}

function bindStatic(rowType, schema, extensionAdapter = false) {
  validateRowType(rowType, schema, true);
  if (schemaHasExtensions(schema) && extensionAdapter !== true) {
    fail("extensionAdapterRequired");
  }
  const expected = rowType.fields.map((field) => ({
    name: field.name,
    type: field.type,
    nullable: field.nullable === true,
    refinement: field.refinement ?? null,
    extensions: field.extensions ?? [],
  }));
  const actual = schemaFields(schema).map((field) => ({
    name: field.name,
    type: field.type,
    nullable: field.nullable,
    refinement: field.refinement ?? null,
    extensions: field.extensions ?? [],
  }));
  if (!same(expected, actual)) fail("staticSchemaMismatch");
  return { mode: "static", identity: schemaIdentity(schema) };
}

function bindDynamic(rowType, schema) {
  validateRowType(rowType, schema, false);
  if (schemaHasExtensions(schema)) fail("extensionAdapterRequired");
  if (rowType.binding !== "explicit") fail("dynamicBindingRequired");
  const rowFields = new Map(rowType.fields.map((field) => [field.name, field]));
  if (rowFields.size !== rowType.fields.length) fail("duplicateRowField");
  const schemaNames = schemaFields(schema).map((field) => field.name);
  const rowNames = rowType.fields.map((field) => field.name);
  if (!same(schemaNames, rowNames)) fail("dynamicSchemaMismatch");
  for (const field of schemaFields(schema)) {
    const candidate = rowFields.get(field.name);
    if (
      !candidate ||
      candidate.type !== field.type ||
      (candidate.nullable === true) !== field.nullable ||
      (candidate.refinement ?? null) !== (field.refinement ?? null) ||
      !same(candidate.extensions ?? [], field.extensions ?? [])
    ) {
      fail("dynamicSchemaMismatch");
    }
  }
  if (rowFields.size !== schemaFields(schema).length) fail("dynamicExtraField");
  return { mode: "dynamic", identity: schemaIdentity(schema) };
}

function requireBatch(state, id) {
  const batch = state.batches[id];
  if (!batch) fail("unknownBatch");
  return batch;
}

function requireSchema(state, id) {
  const schema = state.schemas[id];
  if (!schema) fail("unknownSchema");
  return schema;
}

function requireOwner(state, id) {
  const owner = state.owners[id];
  if (!owner) fail("unknownOwner");
  return owner;
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "setLimits": {
      state.limits = effectiveLimits(operation.limits);
      return;
    }
    case "publishSchema": {
      if (!nonEmptyString(operation.schemaId) || state.schemas[operation.schemaId]) fail("duplicateSchema");
      const schema = clone(operation.schema);
      validateSchema(schema, state.limits);
      state.schemas[operation.schemaId] = {
        schema,
        identity: schemaIdentity(schema),
        published: true,
      };
      return;
    }
    case "publishBatch": {
      if (!nonEmptyString(operation.batchId) || state.batches[operation.batchId]) fail("duplicateBatch");
      const publishedSchema = requireSchema(state, operation.schemaId);
      const batch = validateBatchPayload(operation.batch, publishedSchema.schema, state.limits);
      if (batch.device !== undefined && !DEVICES.has(batch.device)) fail("invalidDevice");
      state.batches[operation.batchId] = {
        batch,
        schemaId: operation.schemaId,
        identity: publishedSchema.identity,
        immutable: true,
        device: batch.device ?? "cpu",
      };
      return;
    }
    case "selectColumn": {
      const batch = requireBatch(state, operation.batchId);
      if (operation.mode === "static") {
        if (!nonEmptyString(operation.descriptor)) fail("invalidFieldDescriptor");
        const field = batch.batch.columnIndex.get(operation.descriptor);
        if (!field) fail("unknownFieldDescriptor");
        state.selections.push({ batchId: operation.batchId, name: field.name, complexity: "O(1)", mode: "static" });
      } else if (operation.mode === "dynamic") {
        if (!nonEmptyString(operation.name) || operation.binding !== "typed") fail("dynamicBindingRequired");
        const field = batch.batch.columnIndex.get(operation.name);
        if (!field) fail("unknownColumnName");
        state.selections.push({ batchId: operation.batchId, name: field.name, complexity: "O(1)", mode: "dynamic" });
      } else {
        fail("invalidFieldSelection");
      }
      return;
    }
    case "readValue": {
      const batch = requireBatch(state, operation.batchId);
      if (!finiteNonNegativeInteger(operation.row) || operation.row >= batch.batch.rows) fail("rowOutOfBounds");
      const column = batch.batch.columnIndex.get(operation.column);
      if (!column) fail("unknownColumnName");
      state.reads.push({ batchId: operation.batchId, column: operation.column, row: operation.row, value: column.values[operation.row] });
      return;
    }
    case "checkNullSemantics": {
      const batch = requireBatch(state, operation.batchId);
      if (!finiteNonNegativeInteger(operation.row) || operation.row >= batch.batch.rows) fail("rowOutOfBounds");
      const column = batch.batch.columnIndex.get(operation.column);
      if (!column) fail("unknownColumnName");
      const value = column.values[operation.row];
      if (operation.expected === "null" && !valueIsNull(value)) fail("expectedNull");
      if (operation.expected === "value" && valueIsNull(value)) fail("unexpectedNull");
      if (operation.expected === "nan" && !valueIsNaN(value)) fail("expectedNaNValue");
      state.validations.push(`nullSemantics:${operation.expected}`);
      return;
    }
    case "scan": {
      const batch = requireBatch(state, operation.batchId);
      if (operation.column !== undefined && !batch.batch.columnIndex.has(operation.column)) {
        fail("unknownColumnName");
      }
      state.scans.push({ batchId: operation.batchId, rows: batch.batch.rows, complexity: "O(rows)" });
      return;
    }
    case "bindStatic": {
      const schema = requireSchema(state, operation.schemaId);
      state.bindings.push({ bindingId: operation.bindingId, ...bindStatic(operation.rowType, schema.schema, operation.extensionAdapter) });
      return;
    }
    case "bindDynamic": {
      const schema = requireSchema(state, operation.schemaId);
      state.bindings.push({ bindingId: operation.bindingId, ...bindDynamic(operation.rowType, schema.schema) });
      return;
    }
    case "bindArrayCarrier": {
      if (operation.use === "rowAlgorithm") {
        state.rowArrayUses += 1;
        return;
      }
      if (operation.use === "universalCarrier") fail("arrayRowNotTabularCarrier");
      fail("invalidArrayCarrierUse");
    }
    case "bindTableCarrier": {
      if (operation.layer === "firstPartyPackage") {
        state.rowArrayUses += 1;
        return;
      }
      if (operation.layer === "stableStd") fail("dataframeNotStable");
      fail("invalidTableCarrierLayer");
    }
    case "copy": {
      const batch = requireBatch(state, operation.batchId);
      if (!COPY_POLICIES.has(operation.policy)) fail("invalidCopyPolicy");
      const targetExplicit = operation.targetDevice !== undefined;
      const targetDevice = targetExplicit ? operation.targetDevice : batch.device;
      if (!DEVICES.has(targetDevice)) fail("invalidDevice");
      const deviceMismatch = targetDevice !== batch.device;
      if (operation.policy === "never" && (deviceMismatch || operation.payloadCopy === true)) {
        fail(deviceMismatch ? "copyNeverDeviceMismatch" : "copyNeverPayloadCopy");
      }
      if (operation.conversion && operation.conversion !== "exact") {
        fail("explicitConversionRequired");
      }
      const payloadCopyRequired = deviceMismatch || operation.payloadCopy === true;
      state.copies.push({
        batchId: operation.batchId,
        policy: operation.policy,
        targetDevice,
        targetExplicit,
        deviceTransferred: deviceMismatch,
        payloadCopyRequired,
        payloadCopied: operation.policy === "always" || (operation.policy === "ifNeeded" && payloadCopyRequired),
      });
      return;
    }
    case "convertSchema": {
      if (operation.mapping !== "explicit") fail("explicitMappingRequired");
      if (!Array.isArray(operation.fields)) fail("invalidMapping");
      state.validations.push(`schemaMapping:${operation.fields.length}`);
      return;
    }
    case "openStream": {
      if (state.streams[operation.streamId]) fail("duplicateStream");
      const schema = requireSchema(state, operation.schemaId);
      state.streams[operation.streamId] = { identity: schema.identity, schemaId: operation.schemaId, chunks: 0, closed: false };
      return;
    }
    case "emitChunk": {
      const stream = state.streams[operation.streamId];
      if (!stream || stream.closed) fail("unknownStream");
      if (stream.chunks >= state.limits.chunks) fail("limitChunks");
      const batch = requireBatch(state, operation.batchId);
      if (batch.identity !== stream.identity) fail("streamSchemaChange");
      stream.chunks += 1;
      return;
    }
    case "closeStream": {
      const stream = state.streams[operation.streamId];
      if (!stream || stream.closed) fail("unknownStream");
      stream.closed = true;
      return;
    }
    case "importForeign": {
      if (!nonEmptyString(operation.ownerId) || state.owners[operation.ownerId]) fail("duplicateOwner");
      if (!TRUST_LEVELS.has(operation.trust)) fail("invalidTrust");
      if (operation.trust === "untrusted") validateExternalPayload(operation.payload, state.limits);
      else validateTrustedForeignPayload(operation.payload, state.limits);
      state.owners[operation.ownerId] = {
        trust: operation.trust,
        releaseCount: 0,
        released: false,
        views: 0,
        waits: 0,
        children: 0,
        consumed: false,
        transferred: false,
        payload: clone(operation.payload),
      };
      return;
    }
    case "createView": {
      const owner = requireOwner(state, operation.ownerId);
      if (owner.released) fail("viewAfterRelease");
      if (owner.transferred) fail("ownerTransferred");
      owner.views += 1;
      state.views.push({ ownerId: operation.ownerId, active: true, scope: operation.scope ?? "local" });
      return;
    }
    case "drainView": {
      const view = state.views[operation.viewIndex];
      if (!view || !view.active) fail("unknownView");
      view.active = false;
      const owner = requireOwner(state, view.ownerId);
      owner.views -= 1;
      return;
    }
    case "retainWait": {
      const owner = requireOwner(state, operation.ownerId);
      if (owner.released) fail("waitAfterRelease");
      if (owner.transferred) fail("ownerTransferred");
      owner.waits += 1;
      return;
    }
    case "drainWait": {
      const owner = requireOwner(state, operation.ownerId);
      if (owner.waits === 0) fail("unknownWait");
      owner.waits -= 1;
      return;
    }
    case "retainChild": {
      const owner = requireOwner(state, operation.ownerId);
      if (owner.released) fail("childAfterRelease");
      if (owner.transferred) fail("ownerTransferred");
      owner.children += 1;
      return;
    }
    case "drainChild": {
      const owner = requireOwner(state, operation.ownerId);
      if (owner.children === 0) fail("unknownChild");
      owner.children -= 1;
      return;
    }
    case "releaseOwner": {
      const owner = requireOwner(state, operation.ownerId);
      if (owner.transferred) fail("ownerTransferred");
      if (
        owner.views !== 0 ||
        owner.waits !== 0 ||
        owner.children !== 0 ||
        state.exports.some((entry) => entry.ownerId === operation.ownerId && entry.active)
      ) fail("ownerStillInUse");
      if (owner.released) fail("releaseExactlyOnce");
      owner.releaseCount += 1;
      owner.released = true;
      return;
    }
    case "consumeOwnedExport": {
      const owner = requireOwner(state, operation.ownerId);
      if (owner.released || owner.consumed || owner.transferred) fail("ownedExportUnavailable");
      if (
        owner.views !== 0 ||
        owner.waits !== 0 ||
        owner.children !== 0 ||
        state.exports.some((entry) => entry.ownerId === operation.ownerId && entry.active)
      ) fail("ownerStillInUse");
      owner.consumed = true;
      owner.transferred = true;
      return;
    }
    case "borrowedExport": {
      const owner = requireOwner(state, operation.ownerId);
      if (owner.released) fail("borrowedExportUnavailable");
      if (owner.transferred) fail("ownerTransferred");
      state.exports.push({ ownerId: operation.ownerId, scope: operation.scope ?? "local", active: true });
      return;
    }
    case "drainBorrowedExport": {
      const entry = state.exports[operation.exportIndex];
      if (!entry || !entry.active) fail("unknownBorrowedExport");
      entry.active = false;
      return;
    }
    case "validateExternal": {
      validateExternalPayload(operation.payload, state.limits);
      state.validations.push("externalPayload");
      return;
    }
    case "sanitizeNulls": {
      if (!Array.isArray(operation.validity) || !Array.isArray(operation.physicalValues)) fail("invalidPhysicalNulls");
      if (operation.validity.length !== operation.physicalValues.length) fail("invalidPhysicalNulls");
      for (const valid of operation.validity) {
        if (typeof valid !== "boolean") fail("invalidValidity");
      }
      for (let index = 0; index < operation.validity.length; index += 1) {
        if (operation.validity[index] === false) {
          const physicalSlot = operation.physicalValues[index];
          const nullSlotInitialized =
            physicalSlot === 0 || (isRecord(physicalSlot) && physicalSlot.initialized === true);
          if (!nullSlotInitialized) fail("nullPhysicalSlotNotSanitized");
        }
      }
      state.validations.push("sanitizedNulls");
      return;
    }
    case "registerExtension": {
      const schema = requireSchema(state, operation.schemaId);
      if (operation.adapter !== true) {
        state.extensions.push({ schemaId: operation.schemaId, mode: "opaque" });
        return;
      }
      if (operation.bindNominal !== true) fail("extensionAdapterRequired");
      state.extensions.push({ schemaId: operation.schemaId, mode: "adapted" });
      return;
    }
    case "publicationOverflow": {
      for (const value of [operation.rows, operation.totalBytes, operation.allocationBytes]) {
        if (value !== undefined && (!Number.isSafeInteger(value) || value < 0)) fail("arithmeticOverflow");
      }
      if (
        operation.rows > state.limits.rows ||
        operation.totalBytes > state.limits.totalBytes ||
        operation.allocationBytes > state.limits.allocationBytes
      ) fail("limitBeforePublication");
      return;
    }
    case "deferAdapterSignatures": {
      const tab1Formats = ["csv", "parquet", "arrow"];
      if (!Array.isArray(operation.formats) || !same(operation.formats, tab1Formats)) fail("invalidAdapterDeferral");
      state.validations.push(`tab1AdaptersDeferred:${operation.formats.length}`);
      return;
    }
    case "classifyAdapter": {
      if (operation.format !== "dlpack") fail("unknownAdapterFormat");
      if (operation.domain === "tensor") {
        state.adapterClassifications.push({ format: "dlpack", domain: "tensor", status: "direction" });
        return;
      }
      if (operation.domain === "tabular") fail("dlpackTabularCarrier");
      fail("invalidAdapterDomain");
    }
    default:
      fail("unknownOperation");
  }
}

export function validateExternalPayload(payload, limits = DEFAULT_LIMITS) {
  const bounded = effectiveLimits(limits);
  if (!isRecord(payload)) fail("invalidForeignPayload");
  if (!Number.isSafeInteger(payload.bufferCount) || payload.bufferCount < 0) fail("invalidBufferCount");
  if (payload.bufferCount > bounded.buffers) fail("limitBuffers");
  if (!Array.isArray(payload.offsets) || !Array.isArray(payload.lengths)) fail("invalidOffsets");
  if (payload.offsets.length !== payload.lengths.length) fail("invalidOffsets");
  if (!finiteNonNegativeInteger(payload.byteLength)) fail("invalidForeignPayload");
  for (let index = 0; index < payload.offsets.length; index += 1) {
    const offset = payload.offsets[index];
    const length = payload.lengths[index];
    if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(length) || offset < 0 || length < 0) fail("invalidOffsets");
    if (!Number.isSafeInteger(offset + length)) fail("arithmeticOverflow");
    if (offset + length > (payload.byteLength ?? 0)) fail("offsetOutOfBounds");
  }
  if (payload.utf8Declared === true) {
    if (payload.utf8 !== undefined && typeof payload.utf8 !== "string") fail("invalidUtf8");
    if (payload.validUtf8 !== true) fail("invalidUtf8");
  }
  if (payload.nesting !== undefined && (!finiteNonNegativeInteger(payload.nesting) || payload.nesting > bounded.nesting)) fail("limitNesting");
  if (payload.bytes !== undefined && (!finiteNonNegativeInteger(payload.bytes) || payload.bytes > bounded.totalBytes)) fail("limitTotalBytes");
  return true;
}

function validateTrustedForeignPayload(payload, limits) {
  if (!isRecord(payload) || !nonEmptyString(payload.schema)) fail("invalidForeignPayload");
  if (payload.bufferCount !== undefined && (!Number.isSafeInteger(payload.bufferCount) || payload.bufferCount < 0)) {
    fail("invalidBufferCount");
  }
  if (payload.bufferCount !== undefined && payload.bufferCount > limits.buffers) fail("limitBuffers");
  if (payload.byteLength !== undefined && !finiteNonNegativeInteger(payload.byteLength)) {
    fail("invalidForeignPayload");
  }
  if (payload.offsets !== undefined || payload.lengths !== undefined) {
    if (!Array.isArray(payload.offsets) || !Array.isArray(payload.lengths) || payload.offsets.length !== payload.lengths.length) {
      fail("invalidOffsets");
    }
    if (payload.byteLength === undefined) fail("invalidForeignPayload");
    for (let index = 0; index < payload.offsets.length; index += 1) {
      const offset = payload.offsets[index];
      const length = payload.lengths[index];
      if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(length) || offset < 0 || length < 0) fail("invalidOffsets");
      if (!Number.isSafeInteger(offset + length)) fail("arithmeticOverflow");
      if (payload.byteLength !== undefined && offset + length > payload.byteLength) fail("offsetOutOfBounds");
    }
  }
  if (payload.utf8Declared === true && payload.validUtf8 !== true) fail("invalidUtf8");
  if (payload.nesting !== undefined && (!finiteNonNegativeInteger(payload.nesting) || payload.nesting > limits.nesting)) {
    fail("limitNesting");
  }
  if (payload.bytes !== undefined && (!finiteNonNegativeInteger(payload.bytes) || payload.bytes > limits.totalBytes)) {
    fail("limitTotalBytes");
  }
  return true;
}

export function validateTabularCarrierOperation(operation) {
  return isRecord(operation) && nonEmptyString(operation.op);
}

function initialState() {
  return {
    schema: "w-tabular-carrier-state-1",
    limits: { ...DEFAULT_LIMITS },
    schemas: {},
    batches: {},
    bindings: [],
    selections: [],
    reads: [],
    scans: [],
    rowArrayUses: 0,
    copies: [],
    streams: {},
    owners: {},
    views: [],
    exports: [],
    validations: [],
    extensions: [],
    adapterClassifications: [],
  };
}

function compactState(state) {
  return {
    schema: state.schema,
    limits: state.limits,
    schemas: Object.fromEntries(Object.entries(state.schemas).map(([id, value]) => [id, { identity: value.identity, fields: value.schema.fields.length }])),
    batches: Object.fromEntries(Object.entries(state.batches).map(([id, value]) => [id, { schemaId: value.schemaId, identity: value.identity, rows: value.batch.rows, device: value.device, immutable: value.immutable }])),
    bindings: state.bindings,
    selections: state.selections,
    reads: state.reads,
    scans: state.scans,
    rowArrayUses: state.rowArrayUses,
    copies: state.copies,
    streams: state.streams,
    owners: Object.fromEntries(Object.entries(state.owners).map(([id, value]) => [id, { trust: value.trust, releaseCount: value.releaseCount, released: value.released, views: value.views, waits: value.waits, children: value.children, consumed: value.consumed, transferred: value.transferred }])),
    views: state.views,
    exports: state.exports,
    validations: state.validations,
    extensions: state.extensions,
    adapterClassifications: state.adapterClassifications,
  };
}

export function runTabularCarrierProgram(operations) {
  const state = initialState();
  const trace = [];
  for (const [index, operation] of (operations ?? []).entries()) {
    const before = compactState(state);
    try {
      if (!validateTabularCarrierOperation(operation)) fail("malformedOperation");
      applyOperation(state, operation);
      trace.push({ index, operation: clone(operation), before, after: compactState(state) });
    } catch (error) {
      const code = error instanceof TabularCarrierError ? error.code : "hostOracleFailure";
      trace.push({ index, operation: clone(operation), before, rejected: code });
      return { status: "rejected", code, operation: index, state: compactState(state), trace };
    }
  }
  return { status: "accepted", state: compactState(state), trace };
}
