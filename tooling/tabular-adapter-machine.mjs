// Host-pure TAB1 oracle. It does not parse, compile, or execute W.
//
// Each operation is a state transition: schema identity, snapshot offsets,
// tokenizer state, footer/page validation, IPC dictionary order, progress, and
// C ownership are derived from prior state. The corpus never supplies an
// expected error to this machine.
import crypto from "node:crypto";

export class TabularAdapterError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

const DEFAULT_LIMITS = Object.freeze({
  bytes: 1_048_576,
  rows: 65_536,
  fields: 256,
  nesting: 64,
  allocations: 2_097_152,
  messages: 65_536,
  chunks: 65_536,
  tokenBytes: 4096,
  ratio: 100,
  callbacks: 65_536,
});
const FORMATS = new Set(["csv", "parquet", "arrowStream", "arrowFile"]);
const SOURCE_KINDS = new Set(["byte", "snapshot"]);
const LAST_LIGHT_SYMBOLS = new Set([
  "telemetrySchema", "summarize", "uploadCsv", "archiveParquet", "handoffArrow",
  "importTrustedCArray", "encodeCsv", "tabularAdversaries",
]);

function clone(value) {
  return value === undefined ? undefined : JSON.parse(JSON.stringify(value));
}

function fail(code) {
  throw new TabularAdapterError(code);
}

function finiteInteger(value) {
  return Number.isSafeInteger(value) && value >= 0;
}

function positiveInteger(value) {
  return Number.isSafeInteger(value) && value > 0;
}

function limitsOf(value) {
  const limits = { ...DEFAULT_LIMITS, ...(value ?? {}) };
  for (const [name, limit] of Object.entries(limits)) {
    if (!positiveInteger(limit)) fail(`invalidLimit:${name}`);
  }
  return limits;
}

function canonicalType(type, depth = 0) {
  if (!type || typeof type !== "object") fail("invalidLogicalDescriptor");
  if (depth > DEFAULT_LIMITS.nesting) fail("invalidLogicalNesting");
  const kind = type.kind;
  if (typeof kind !== "string" || kind.length === 0) fail("invalidLogicalDescriptor");
  if (["integer", "float"].includes(kind) && !positiveInteger(type.width)) fail("invalidLogicalBounds");
  if (kind === "fixedDecimal" && (!positiveInteger(type.precision) || !finiteInteger(type.scale))) fail("invalidLogicalBounds");
  if (kind === "date" && ("precision" in type || "timezone" in type)) fail("invalidLogicalBounds");
  if (["time", "instant", "localDateTime"].includes(kind) &&
    (typeof type.precision !== "string" || "timezone" in type)) fail("invalidLogicalBounds");
  if (["option", "list"].includes(kind)) {
    canonicalType(type.of ?? type.element, depth + 1);
  }
  if (kind === "map") {
    canonicalType(type.key, depth + 1);
    canonicalType(type.value, depth + 1);
  }
  if (kind === "nested" && (typeof type.identity !== "string" || type.identity.length === 0)) {
    fail("invalidNestedIdentity");
  }
  if (kind === "extension") {
    if (typeof type.id !== "string" || type.id.length === 0 || !positiveInteger(type.version)) fail("invalidExtension");
    if (!Array.isArray(type.parameters) || type.parameters.length > DEFAULT_LIMITS.fields) fail("invalidExtension");
    if (type.parameters.some((parameter) => typeof parameter !== "string" && !Buffer.isBuffer(parameter))) fail("invalidExtension");
  }
  return type;
}

export function schemaIdentity(schema) {
  if (!schema || !Array.isArray(schema.fields) || schema.fields.length === 0) fail("invalidSchema");
  const names = new Set();
  const fields = schema.fields.map((field) => {
    if (!field || typeof field.name !== "string" || field.name.length === 0) fail("invalidFieldName");
    if (names.has(field.name)) fail("duplicateFieldName");
    names.add(field.name);
    return {
      name: field.name,
      logicalType: canonicalType(field.logicalType),
      nullable: field.nullable === true,
    };
  });
  return crypto.createHash("sha256").update(JSON.stringify(fields)).digest("hex");
}

function initialState() {
  return {
    schema: "w-tabular-adapter-state-2",
    limits: { ...DEFAULT_LIMITS },
    schemas: {},
    sources: {},
    reads: [],
    batches: {},
    streams: {},
    views: [],
    copies: [],
    progress: [],
    publications: [],
    csv: {},
    parquet: {},
    arrow: {},
    dictionaries: {},
    handles: {},
    artifacts: {},
    provenance: [],
    validations: [],
    cancellation: { requested: false, drained: false, progress: null },
  };
}

function compact(state) {
  return {
    schema: state.schema,
    limits: state.limits,
    schemas: state.schemas,
    sources: state.sources,
    reads: state.reads,
    batches: state.batches,
    streams: state.streams,
    views: state.views,
    copies: state.copies,
    progress: state.progress,
    publications: state.publications,
    csv: state.csv,
    parquet: state.parquet,
    arrow: state.arrow,
    dictionaries: state.dictionaries,
    handles: state.handles,
    artifacts: state.artifacts,
    provenance: state.provenance,
    validations: state.validations,
    cancellation: state.cancellation,
  };
}

function requireSchema(state, id) {
  if (!state.schemas[id]) fail("unknownSchema");
  return state.schemas[id];
}

function requireSource(state, id) {
  if (!state.sources[id]) fail("unknownSource");
  return state.sources[id];
}

function requireBatch(state, id) {
  if (!state.batches[id]) fail("unknownBatch");
  return state.batches[id];
}

function requireCsv(state, id) {
  if (!state.csv[id]) fail("csvConfigMissing");
  return state.csv[id];
}

function requireParquet(state, id) {
  if (!state.parquet[id]) fail("parquetPlanMissing");
  return state.parquet[id];
}

function requireArrow(state, id) {
  if (!state.arrow[id]) fail("arrowSessionMissing");
  return state.arrow[id];
}

function applyOperation(state, operation) {
  if (!operation || typeof operation.op !== "string") fail("malformedOperation");

  switch (operation.op) {
    case "setLimits":
      state.limits = limitsOf(operation.limits);
      return;

    case "registerSchema": {
      if (typeof operation.schemaId !== "string" || state.schemas[operation.schemaId]) fail("duplicateSchema");
      if (!Array.isArray(operation.schema?.fields) || operation.schema.fields.length > state.limits.fields) fail("limitFields");
      const identity = schemaIdentity(operation.schema);
      state.schemas[operation.schemaId] = {
        identity,
        fields: operation.schema.fields.length,
        fieldDescriptors: clone(operation.schema.fields),
        owner: operation.owner ?? "TabularTelemetryRow",
      };
      return;
    }

    case "openSource": {
      if (typeof operation.sourceId !== "string" || state.sources[operation.sourceId]) fail("duplicateSource");
      if (!SOURCE_KINDS.has(operation.kind)) fail("invalidSourceKind");
      if (!finiteInteger(operation.byteCount) || operation.byteCount > state.limits.bytes) fail("limitBytes");
      if (operation.kind === "snapshot" && operation.stable !== true) fail("sourceNotStable");
      state.sources[operation.sourceId] = {
        kind: operation.kind,
        byteCount: operation.byteCount,
        stable: operation.stable !== false,
        digest: operation.digest ?? null,
        cursor: operation.kind === "byte" ? 0 : null,
      };
      return;
    }

    case "mutateSource": {
      const source = requireSource(state, operation.sourceId);
      if (source.kind !== "snapshot") fail("positionalSourceRequired");
      source.stable = false;
      source.digest = operation.newDigest ?? "changed";
      return;
    }

    case "readSnapshot": {
      const source = requireSource(state, operation.sourceId);
      if (source.kind !== "snapshot") fail("positionalSourceRequired");
      if (!source.stable) fail("sourceNotStable");
      if (!positiveInteger(operation.maximum) || !finiteInteger(operation.offset)) fail("invalidOffset");
      if (operation.offset > source.byteCount) fail("offsetOutOfBounds");
      const available = source.byteCount - operation.offset;
      const length = Math.min(operation.maximum, available);
      state.reads.push({ sourceId: operation.sourceId, offset: operation.offset, length, short: length < operation.maximum });
      state.validations.push("snapshot-positional-read");
      return;
    }

    case "decode": {
      if (!FORMATS.has(operation.format)) fail("invalidFormat");
      const source = requireSource(state, operation.sourceId);
      const schema = requireSchema(state, operation.schemaId);
      if ((operation.format === "parquet" || operation.format === "arrowFile") && source.kind !== "snapshot") fail("snapshotRequired");
      if (source.kind === "snapshot" && source.stable !== true) fail("sourceNotStable");
      if (!positiveInteger(operation.rows) || operation.rows > state.limits.rows) fail("limitRows");
      if (typeof operation.batchId !== "string" || state.batches[operation.batchId]) fail("duplicateBatch");
      state.batches[operation.batchId] = {
        format: operation.format,
        schemaId: operation.schemaId,
        identity: schema.identity,
        rows: operation.rows,
        published: true,
        owner: operation.owner ?? schema.owner,
      };
      state.publications.push({ batchId: operation.batchId, format: operation.format, rows: operation.rows, identity: schema.identity });
      return;
    }

    case "decodeReject": {
      const reasons = new Set([
        "fieldConversion", "invalidUtf8", "rowWidth", "duplicateHeader", "invalidFooter",
        "invalidOffset", "decompressionRatio", "logicalMismatch", "schemaDivergence",
      ]);
      if (!reasons.has(operation.reason)) fail("unknownDecodeReason");
      fail(operation.reason);
    }

    case "openStream": {
      const schema = requireSchema(state, operation.schemaId);
      if (state.streams[operation.streamId]) fail("duplicateStream");
      state.streams[operation.streamId] = {
        identity: schema.identity,
        batches: 0,
        closed: false,
        serial: true,
        format: operation.format ?? "generic",
      };
      return;
    }

    case "emitBatch": {
      const stream = state.streams[operation.streamId];
      const batch = requireBatch(state, operation.batchId);
      if (!stream || stream.closed) fail("unknownStream");
      if (stream.identity !== batch.identity) fail("streamSchemaChange");
      if (stream.batches >= state.limits.messages) fail("limitMessages");
      stream.batches += 1;
      state.validations.push("batch-published-after-validation");
      return;
    }

    case "closeStream": {
      const stream = state.streams[operation.streamId];
      if (!stream || stream.closed) fail("unknownStream");
      stream.closed = true;
      return;
    }

    case "borrowView": {
      const batch = requireBatch(state, operation.batchId);
      if (batch.released) fail("batchReleased");
      const schema = requireSchema(state, batch.schemaId);
      const field = schema.fieldDescriptors.find((candidate) => candidate.name === operation.field);
      if (!field) fail("unknownField");
      const kind = field.logicalType.kind;
      if (!field.nullable || (kind !== "string" && kind !== "bytes")) fail("viewOnlyForBorrowedType");
      const expectedType = kind === "string" ? "String?" : "Bytes?";
      if (operation.type !== expectedType) fail("fieldTypeMismatch");
      state.views.push({ batchId: operation.batchId, field: operation.field, active: true, owner: batch.identity });
      return;
    }

    case "copyColumn": {
      const batch = requireBatch(state, operation.batchId);
      if (batch.released) fail("batchReleased");
      const schema = requireSchema(state, batch.schemaId);
      const field = schema.fieldDescriptors.find((candidate) => candidate.name === operation.field);
      if (!field) fail("unknownField");
      state.copies.push({
        batchId: operation.batchId,
        field: operation.field,
        logicalKind: field.logicalType.kind,
        materialized: true,
        owner: batch.identity,
      });
      return;
    }

    case "drainView": {
      const view = state.views[operation.index];
      if (!view || !view.active) fail("unknownView");
      view.active = false;
      return;
    }

    case "releaseBatch": {
      const batch = requireBatch(state, operation.batchId);
      if (state.views.some((view) => view.active && view.batchId === operation.batchId)) fail("borrowedViewEscapes");
      batch.released = true;
      return;
    }

    case "encodeProgress": {
      if (!FORMATS.has(operation.format)) fail("invalidFormat");
      const batch = requireBatch(state, operation.batchId);
      if (operation.mode === "batch" && operation.batchMode !== "ref") fail("batchEncodeMustBorrow");
      if (operation.mode === "stream" && operation.batchMode !== "take") fail("streamEncodeMustConsume");
      if (!finiteInteger(operation.bytes) || !finiteInteger(operation.records)) fail("invalidProgress");
      if (operation.bytes > state.limits.bytes) fail("limitBytes");
      if (operation.records > batch.rows) fail("progressExceedsRows");
      const previous = state.progress.at(-1);
      if (previous && (operation.bytes < previous.bytesCommitted || operation.records < previous.completeRecords)) fail("progressRegressed");
      state.progress.push({
        format: operation.format,
        batchId: operation.batchId,
        mode: operation.mode,
        bytesCommitted: operation.bytes,
        completeRecords: operation.records,
        partialRecord: operation.partialRecord === true,
      });
      if (operation.transaction === true) fail("encoderNotTransaction");
      return;
    }

    case "streamFailure": {
      if (state.progress.length === 0) fail("noProgress");
      if (operation.failure !== "batch" && operation.failure !== "sink") fail("unknownStreamFailure");
      state.validations.push(`stream-failure:${operation.failure}:progress-retained`);
      return;
    }

    case "errorAfterPublish":
      if (state.publications.length === 0) fail("noPublication");
      state.validations.push("published-batches-retained-after-error");
      return;

    case "cancel": {
      state.cancellation.requested = true;
      const waits = operation.waits;
      if (!finiteInteger(waits)) fail("invalidWaitCount");
      state.cancellation.progress = state.progress.at(-1) ?? null;
      state.cancellation.drained = operation.drain === "drained";
      if (!state.cancellation.drained) fail("cancelDrainRequired");
      return;
    }

    case "csvConfig": {
      if (state.csv[operation.configId]) fail("duplicateCsvConfig");
      const profile = operation.profile;
      const dialect = operation.dialect ?? {};
      if (profile === "portable") {
        if (dialect.header !== "required" || dialect.delimiter !== "," || dialect.quote !== '"' || dialect.whitespace !== "preserve") fail("csvPortableDefaults");
        if (dialect.nullDecode === undefined || dialect.boolTrue !== "true" || dialect.boolFalse !== "false" || dialect.nonfinite !== "reject") fail("csvPortableTokens");
      } else if (profile === "rfc4180") {
        if (dialect.terminator !== "CRLF" || dialect.header === undefined) fail("csvRfc4180RequiresCrLf");
      } else if (profile !== "custom") {
        fail("csvUnknownProfile");
      }
      if (dialect.delimiter === dialect.quote) fail("csvDelimiterQuoteCollision");
      if (dialect.nullDecode === "tokens" && dialect.nullToken === dialect.boolTrue) fail("csvTokenCollision");
      if (dialect.nullEncode === "token" && (dialect.nullToken === dialect.delimiter || dialect.nullToken === dialect.quote)) fail("csvTokenCollision");
      if (dialect.boolTrue !== undefined && dialect.boolTrue === dialect.boolFalse) fail("csvBoolTokenCollision");
      state.csv[operation.configId] = { profile, dialect, records: {}, headers: null, encoded: false };
      state.validations.push(`csv-config:${profile}`);
      return;
    }

    case "csvHeader": {
      const config = requireCsv(state, operation.configId);
      if (config.headers) fail("csvHeaderAlreadyRead");
      if (!Array.isArray(operation.fields) || operation.fields.length === 0) fail("csvEmptyHeader");
      const names = new Set();
      for (const field of operation.fields) {
        if (typeof field !== "string" || field.length === 0) fail("csvEmptyHeader");
        if (names.has(field)) fail("csvDuplicateHeader");
        names.add(field);
      }
      config.headers = operation.fields.slice();
      return;
    }

    case "csvChunk": {
      const config = requireCsv(state, operation.configId);
      if (!finiteInteger(operation.record) || typeof operation.text !== "string") fail("csvChunkMalformed");
      if (operation.text.includes("\r") && !operation.text.includes("\r\n")) fail("csvBareCr");
      const current = config.records[operation.record] ?? { text: "", quoteParity: 0 };
      current.text += operation.text;
      let quoteCount = 0;
      for (const char of operation.text) if (char === '"') quoteCount += 1;
      current.quoteParity = (current.quoteParity + quoteCount) % 2;
      config.records[operation.record] = current;
      if (operation.final === true && current.quoteParity !== 0) fail("csvUnterminatedQuote");
      if (operation.final === true) state.validations.push("csv-record-closed");
      return;
    }

    case "csvNull": {
      const config = requireCsv(state, operation.configId);
      if (operation.value === "" && config.dialect.nullDecode === "empty") state.validations.push("csv-empty-is-null");
      else if (operation.value === "" && config.dialect.nullDecode === "none") state.validations.push("csv-empty-is-string");
      else if (config.dialect.nullDecode === "tokens" && operation.value === config.dialect.nullToken) state.validations.push("csv-token-is-null");
      else if (operation.value === config.dialect.nullToken && config.dialect.nullDecode !== "tokens") fail("csvNullTokenDisabled");
      else state.validations.push("csv-value-is-nonnull");
      return;
    }

    case "csvWidth":
      if (!positiveInteger(operation.expected) || !finiteInteger(operation.found)) fail("csvWidthMalformed");
      if (operation.expected !== operation.found) fail("csvRowWidth");
      state.validations.push("csv-width-validated");
      return;

    case "csvScalar": {
      const config = requireCsv(state, operation.configId);
      if (operation.kind === "bool" && operation.token !== config.dialect.boolTrue && operation.token !== config.dialect.boolFalse) fail("csvBoolToken");
      if (operation.kind === "float" && operation.finite !== true) fail("csvNonfiniteDisabled");
      state.validations.push(`csv-scalar:${operation.kind}`);
      return;
    }

    case "csvEncode": {
      const config = requireCsv(state, operation.configId);
      if (config.profile !== "portable" && config.profile !== "rfc4180" && config.profile !== "custom") fail("csvUnknownProfile");
      if (operation.nullValue === true && config.dialect.nullEncode === "unavailable") fail("csvMissingNullRepresentation");
      if (operation.formula === "escapeForSpreadsheet" && operation.lossless === true) fail("csvFormulaChangesData");
      config.encoded = true;
      state.validations.push("csv-canonical-writer");
      return;
    }

    case "parquetPreflight": {
      const source = requireSource(state, operation.sourceId);
      if (source.kind !== "snapshot") fail("parquetSnapshotRequired");
      if (!source.stable) fail("sourceNotStable");
      if (operation.magic !== "PAR1" || operation.footerBytes <= 0 || operation.footerBytes > state.limits.bytes) fail("parquetPreflight");
      if (operation.offsets !== "bounded" || operation.sizes !== "bounded") fail("parquetOffsetSize");
      state.parquet[operation.planId] = { sourceId: operation.sourceId, profile: operation.profile ?? "portable", pages: 0, committed: false, complete: false };
      state.validations.push("parquet-footer-preflight");
      return;
    }

    case "parquetPage": {
      const plan = requireParquet(state, operation.planId);
      if (!positiveInteger(operation.encodedBytes) || !positiveInteger(operation.decodedBytes)) fail("parquetPageSize");
      if (operation.encodedBytes > state.limits.bytes || operation.decodedBytes > state.limits.bytes) fail("parquetPageLimit");
      if (operation.decodedBytes / operation.encodedBytes > state.limits.ratio) fail("parquetDecompressionRatio");
      plan.pages += 1;
      state.validations.push("parquet-page-bounded");
      return;
    }

    case "parquetMapping": {
      if (operation.mode !== "exact" && operation.mode !== "project") fail("parquetBindingPolicy");
      if (operation.mode === "project" && operation.mapping !== "explicit") fail("explicitMappingRequired");
      if (operation.logicalMismatch === true) fail("parquetLogicalMismatch");
      state.validations.push(`parquet-mapping:${operation.mode}`);
      return;
    }

    case "parquetKey": {
      const plan = requireParquet(state, operation.planId);
      if (operation.encrypted !== true) fail("parquetKeyWithoutEncryption");
      if (operation.scope !== "parquetFooter" && operation.scope !== "parquetPage") fail("parquetKeyScope");
      if (operation.resolver !== "scoped") fail("parquetKeyRequired");
      plan.keyScope = operation.scope;
      state.validations.push("parquet-scoped-key-resolver");
      return;
    }

    case "parquetStatistics":
      if (!["defined", "undefined", "nan", "malformed"].includes(operation.ordering)) fail("parquetStatisticsState");
      if ((operation.ordering === "undefined" || operation.ordering === "nan" || operation.ordering === "malformed") && operation.policy === "error") fail("parquetStatisticsMalformed");
      state.validations.push(`parquet-statistics:${operation.policy}`);
      return;

    case "parquetPlan": {
      if (!positiveInteger(operation.rowGroupBytes) || !positiveInteger(operation.pageBytes)) fail("parquetPlanBounds");
      if (operation.deterministic === true && (typeof operation.codecDigest !== "string" || typeof operation.providerDigest !== "string")) fail("parquetMissingDeterministicDigest");
      state.validations.push("parquet-writer-plan-pinned");
      return;
    }

    case "parquetCommit": {
      const plan = requireParquet(state, operation.planId);
      if (operation.status === "complete") {
        plan.complete = true;
        plan.committed = true;
        state.artifacts[operation.planId] = "valid-file";
      } else if (operation.status === "failure") {
        plan.complete = false;
        plan.committed = false;
        state.artifacts[operation.planId] = "incomplete-discard";
      } else fail("parquetCommitState");
      return;
    }

    case "arrowSession": {
      if (state.arrow[operation.sessionId]) fail("arrowSessionDuplicate");
      if (operation.container !== "stream" && operation.container !== "file") fail("arrowContainer");
      state.arrow[operation.sessionId] = { container: operation.container, dictionaries: new Set(), messages: 0, callbackInFlight: false, released: false };
      state.validations.push(`arrow-session:${operation.container}`);
      return;
    }

    case "arrowMessage": {
      const session = requireArrow(state, operation.sessionId);
      if (session.released || session.callbackInFlight) fail("arrowCallbackSerial");
      if (!["schema", "recordBatch", "dictionary"].includes(operation.kind)) fail("arrowMessageKind");
      session.messages += 1;
      if (session.messages > state.limits.messages) fail("arrowMessageLimit");
      session.callbackInFlight = true;
      if (operation.kind === "schema") session.schema = operation.identity;
      if (operation.kind === "recordBatch" && operation.identity !== session.schema) fail("arrowSchemaDivergence");
      session.callbackInFlight = false;
      return;
    }

    case "arrowDictionary": {
      const session = requireArrow(state, operation.sessionId);
      if (operation.event === "use" && !session.dictionaries.has(operation.id)) fail("arrowDictionaryBeforeDefinition");
      if (operation.event === "definition") session.dictionaries.add(operation.id);
      if (operation.event === "replacement" && session.container === "file") fail("arrowFileDictionaryReplacement");
      if (operation.event !== "use" && operation.event !== "definition" && operation.event !== "replacement") fail("arrowDictionaryEvent");
      return;
    }

    case "arrowBuffer": {
      const session = requireArrow(state, operation.sessionId);
      if (operation.bytes > state.limits.bytes || !positiveInteger(operation.bytes)) fail("arrowBodyLimit");
      if (operation.endian !== "native") fail("arrowNonNativeEndian");
      if (operation.aligned !== true) fail("arrowAlignmentRequired");
      if (operation.copyPolicy === "never" && operation.copyRequired === true) fail("copyNever");
      session.buffers = (session.buffers ?? 0) + 1;
      return;
    }

    case "arrowCImport": {
      if (operation.trust !== "trusted") fail("untrustedCBridge");
      if (operation.schemaMatch !== true) fail("cSchemaMismatch");
      if (operation.device !== "cpu") fail("cDeviceNotCpu");
      if (!positiveInteger(operation.maxBytes) || !positiveInteger(operation.maxCallbacks)) fail("cImportLimit");
      if (operation.kind === "stream" &&
        (!positiveInteger(operation.maxConcurrent) ||
         !positiveInteger(operation.maxQueued) ||
         !positiveInteger(operation.maxJobs))) fail("cImportQuota");
      state.validations.push(operation.kind === "stream" ? "arrow-c-stream-bounded-adapter" : "arrow-c-array-validated");
      return;
    }

    case "arrowCNext": {
      const session = requireArrow(state, operation.sessionId);
      if (session.container !== "stream") fail("cStreamRequired");
      if (session.callbackInFlight) fail("arrowCallbackSerial");
      if (!positiveInteger(operation.quota)) fail("cImportQuota");
      session.callbackInFlight = true;
      session.callbackInFlight = false;
      return;
    }

    case "deferCExportStream":
      fail("cStreamExportDeferred");

    case "releaseC": {
      const handle = state.handles[operation.handleId] ?? { releaseCount: 0 };
      if (handle.releaseCount !== 0) fail("releaseExactlyOnce");
      handle.releaseCount = 1;
      state.handles[operation.handleId] = handle;
      return;
    }

    case "mapping":
      if (operation.mode !== "explicit" || operation.total !== true) fail("explicitMappingRequired");
      state.validations.push("mapping-explicit");
      return;

    case "recordProvenance":
      if (typeof operation.format !== "string" || typeof operation.profile !== "string" ||
        typeof operation.providerVersion !== "string" || typeof operation.providerDigest !== "string" ||
        typeof operation.schemaIdentity !== "string" || typeof operation.copyPolicy !== "string" ||
        typeof operation.materialization !== "string" || !operation.options) fail("provenanceIncomplete");
      state.provenance.push({
        format: operation.format,
        profile: operation.profile,
        providerVersion: operation.providerVersion,
        providerDigest: operation.providerDigest,
        schemaIdentity: operation.schemaIdentity,
        copyPolicy: operation.copyPolicy,
        materialization: operation.materialization,
        options: clone(operation.options),
      });
      return;

    default:
      fail("unknownOperation");
  }
}

export function runTabularAdapterProgram(operations) {
  const state = initialState();
  const trace = [];
  for (const [index, operation] of (operations ?? []).entries()) {
    const before = compact(state);
    try {
      applyOperation(state, operation);
      trace.push({ index, operation: clone(operation), before, after: compact(state) });
    } catch (error) {
      const code = error instanceof TabularAdapterError ? error.code : "hostOracleFailure";
      trace.push({ index, operation: clone(operation), before, rejected: code });
      return { status: "rejected", code, operation: index, state: compact(state), trace };
    }
  }
  return { status: "accepted", state: compact(state), trace };
}

export function validateTabularAdapterOperation(operation) {
  return Boolean(operation && typeof operation.op === "string");
}

export function lastLightSymbols() {
  return [...LAST_LIGHT_SYMBOLS].sort();
}
