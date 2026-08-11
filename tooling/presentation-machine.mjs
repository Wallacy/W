import { createHash } from "node:crypto";

const SAFE_MEDIA = new Set(["text/plain", "application/json", "image/png", "image/jpeg"]);
const MISSING_MEDIA = new Set(["text/html", "image/svg+xml"]);
const ACTIVE_MEDIA = new Set([
  "text/javascript",
  "application/javascript",
  "application/vnd.jupyter.widget-view+json",
]);
const EFFECT_MASK = new Set(["borrow", "boundedAllocation", "writerWrite"]);
const IMAGE_MAGIC = {
  "image/png": [137, 80, 78, 71, 13, 10, 26, 10],
  "image/jpeg": [255, 216],
};

export class PresentationError extends Error {
  constructor(code, details = {}) {
    super(code);
    this.name = "PresentationError";
    this.code = code;
    this.details = details;
  }
}

function fail(code, details = {}) {
  throw new PresentationError(code, details);
}

function clone(value) {
  return structuredClone(value);
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") {
    return Object.fromEntries(Object.keys(value).sort().map((key) => [key, canonical(value[key])]));
  }
  return value;
}

export function presentationDigest(value) {
  return `sha256:${createHash("sha256").update(JSON.stringify(canonical(value))).digest("hex")}`;
}

function payloadBytes(payload) {
  if (typeof payload === "string") return Buffer.byteLength(payload, "utf8");
  if (Array.isArray(payload) && payload.every((value) => Number.isInteger(value) && value >= 0 && value <= 255)) return payload.length;
  if (payload === undefined) return 0;
  return null;
}

function jsonFacts(payload, limits) {
  const maximumNodes = Number.isSafeInteger(limits.jsonNodes) ? limits.jsonNodes : 128;
  const maximumDepth = Number.isSafeInteger(limits.jsonDepth) ? limits.jsonDepth : 16;
  const maximumStringBytes = Number.isSafeInteger(limits.jsonStringBytes) ? limits.jsonStringBytes : 4096;
  const maximumBytes = Number.isSafeInteger(limits.totalBytes) ? limits.totalBytes : 4096;
  const stack = [{ value: payload, depth: 1, leave: false }];
  const active = new WeakSet();
  let nodes = 0;
  let depth = 0;
  let bytes = 0;
  let stringBytes = 0;
  let cycle = false;
  let bounded = false;
  const add = (value) => {
    if (!Number.isSafeInteger(value) || value < 0) { bounded = true; return; }
    bytes = Math.min(maximumBytes + 1, bytes + value);
    if (bytes > maximumBytes) bounded = true;
  };
  while (stack.length > 0) {
    const item = stack.pop();
    if (item.leave) {
      active.delete(item.value);
      add(Array.isArray(item.value) ? 1 : 1);
      continue;
    }
    const value = item.value;
    nodes += 1;
    depth = Math.max(depth, item.depth);
    if (nodes > maximumNodes || item.depth > maximumDepth) { bounded = true; break; }
    if (value === null) { add(4); continue; }
    if (typeof value === "string") {
      const encoded = Buffer.byteLength(value, "utf8");
      stringBytes = Math.min(maximumStringBytes + 1, stringBytes + encoded);
      if (stringBytes > maximumStringBytes) bounded = true;
      let quoted;
      try { quoted = JSON.stringify(value); } catch { fail("W-PRESENTATION-0001", { reason: "payload is not serializable" }); }
      add(Buffer.byteLength(quoted, "utf8"));
      continue;
    }
    if (typeof value !== "object") {
      if (typeof value === "bigint" || typeof value === "function" || typeof value === "symbol" || value === undefined) {
        fail("W-PRESENTATION-0001", { reason: "payload is not serializable" });
      }
      add(Buffer.byteLength(JSON.stringify(value), "utf8"));
      continue;
    }
    if (active.has(value)) { cycle = true; bounded = true; break; }
    active.add(value);
    stack.push({ value, depth: item.depth, leave: true });
    if (Array.isArray(value)) {
      add(1);
      for (let index = value.length - 1; index >= 0; index -= 1) stack.push({ value: value[index], depth: item.depth + 1, leave: false });
    } else {
      const keys = Object.keys(value);
      add(1);
      for (let index = keys.length - 1; index >= 0; index -= 1) {
        const key = keys[index];
        add(Buffer.byteLength(JSON.stringify(key), "utf8") + 1);
        stack.push({ value: value[key], depth: item.depth + 1, leave: false });
      }
    }
  }
  return { nodes, depth, bytes, stringBytes, cycle, bounded };
}

function defaultLimits() {
  return {
    totalBytes: 4096,
    representations: 4,
    textBytes: 2048,
    imageBytes: 3072,
    imageWidth: 4096,
    imageHeight: 4096,
    imagePixels: 16_777_216,
    jsonDepth: 16,
    jsonNodes: 128,
    jsonStringBytes: 2048,
    workUnits: 8192,
    tableRows: 8,
    tableColumns: 4,
  };
}

function initialState() {
  return {
    phase: "empty",
    limits: defaultLimits(),
    entries: [],
    bytes: 0,
    textBytes: 0,
    imageBytes: 0,
    jsonNodes: 0,
    jsonDepth: 0,
    jsonStringBytes: 0,
    workUnits: 0,
    effects: [],
    fallback: null,
    fallbackText: null,
    preview: null,
    tensor: null,
    sensor: null,
    cancelled: false,
    submissionOutcome: "unchanged",
    output: null,
    diagnostics: [],
  };
}

function requireOpen(state) {
  if (state.phase !== "open") fail("W-PRESENTATION-0008", { reason: "writer is not open" });
}

function consumeWork(state, units, reason = "presentation") {
  if (!Number.isSafeInteger(units) || units < 0) fail("W-PRESENTATION-0010", { reason: "work estimate is not bounded" });
  const next = state.workUnits + units;
  if (next > state.limits.workUnits) {
    fail("W-PRESENTATION-0004", { kind: "workUnits", maximum: state.limits.workUnits, reason });
  }
  state.workUnits = next;
}

function parseMedia(media) {
  if (typeof media !== "string" || media.length === 0 || media.length > 127 || /[\s;]/.test(media) || media !== media.toLowerCase()) {
    fail("W-PRESENTATION-0001", { media, reason: "invalid canonical media syntax" });
  }
  if (SAFE_MEDIA.has(media)) return { media, status: "safe" };
  if (/^application\/vnd\.w\.[a-z0-9-]+\.v[1-9][0-9]*\+json$/.test(media)) return { media, status: "safe-vendor-json" };
  if (MISSING_MEDIA.has(media)) fail("W-PRESENTATION-0006", { media, reason: "media provider or sanitizer missing" });
  if (ACTIVE_MEDIA.has(media) || /(^|[.+-])(javascript|widget)([.+-]|$)/.test(media)) {
    fail("W-PRESENTATION-0002", { media, reason: "active content rejected" });
  }
  fail("W-PRESENTATION-0006", { media, reason: "media provider missing" });
}

function assertNoConclusionFields(operation, fields) {
  for (const field of fields) {
    if (Object.prototype.hasOwnProperty.call(operation, field)) {
      fail("W-PRESENTATION-0001", { reason: `${field} is a checker conclusion, not payload evidence` });
    }
  }
}

function assertNoNestedConclusionFields(value, fields) {
  const forbidden = new Set(fields);
  const stack = [value];
  const seen = new WeakSet();
  while (stack.length > 0) {
    const current = stack.pop();
    if (!current || typeof current !== "object") continue;
    if (seen.has(current)) continue;
    seen.add(current);
    for (const [key, child] of Object.entries(current)) {
      if (forbidden.has(key)) fail("W-PRESENTATION-0001", { reason: `${key} is a checker conclusion, not payload evidence` });
      stack.push(child);
    }
  }
}

function addRepresentation(state, media, payload, kind, details = {}) {
  requireOpen(state);
  parseMedia(media);
  if (state.entries.some((entry) => entry.media === media)) fail("W-PRESENTATION-0003", { media, reason: "media type is not unique" });
  if (state.entries.length + 1 > state.limits.representations) fail("W-PRESENTATION-0004", { kind: "representationCount", maximum: state.limits.representations });
  const size = payloadBytes(payload) ?? details.bytes;
  if (!Number.isSafeInteger(size) || size < 0) fail("W-PRESENTATION-0001", { reason: "payload is not serializable" });
  if (state.bytes + size > state.limits.totalBytes) fail("W-PRESENTATION-0004", { kind: "totalBytes", maximum: state.limits.totalBytes });
  if (kind === "text" && state.textBytes + size > state.limits.textBytes) fail("W-PRESENTATION-0004", { kind: "textBytes", maximum: state.limits.textBytes });
  if (kind === "image" && state.imageBytes + size > state.limits.imageBytes) fail("W-PRESENTATION-0004", { kind: "imageBytes", maximum: state.limits.imageBytes });
  consumeWork(state, Math.max(1, Math.ceil(size / 64)), `${media} representation`);
  state.entries.push({ media, kind, bytes: size, ...clone(details) });
  state.bytes += size;
  if (kind === "text") state.textBytes += size;
  if (kind === "image") state.imageBytes += size;
  return size;
}

function assertImageBytes(media, value) {
  if (!Array.isArray(value) || !value.every((byte) => Number.isInteger(byte) && byte >= 0 && byte <= 255)) {
    fail("W-PRESENTATION-0001", { reason: "image payload must be a byte vector" });
  }
  const magic = IMAGE_MAGIC[media];
  if (!magic.every((byte, index) => value[index] === byte)) fail("W-PRESENTATION-0001", { reason: "image encoding magic is invalid", media });
}

function pngDimensions(bytes) {
  if (bytes.length < 24) fail("W-PRESENTATION-0001", { reason: "PNG header is truncated" });
  const width = bytes[16] * 0x1000000 + bytes[17] * 0x10000 + bytes[18] * 0x100 + bytes[19];
  const height = bytes[20] * 0x1000000 + bytes[21] * 0x10000 + bytes[22] * 0x100 + bytes[23];
  return { width, height };
}

function jpegDimensions(bytes) {
  let index = 2;
  while (index + 8 < bytes.length) {
    if (bytes[index] !== 0xff) { index += 1; continue; }
    const marker = bytes[index + 1];
    if (marker === 0xd8 || marker === 0xd9) { index += 2; continue; }
    const length = bytes[index + 2] * 256 + bytes[index + 3];
    if (length < 2 || index + 2 + length > bytes.length) break;
    if ((marker >= 0xc0 && marker <= 0xc3) || (marker >= 0xc5 && marker <= 0xc7) || (marker >= 0xc9 && marker <= 0xcb) || (marker >= 0xcd && marker <= 0xcf)) {
      return { height: bytes[index + 5] * 256 + bytes[index + 6], width: bytes[index + 7] * 256 + bytes[index + 8] };
    }
    index += 2 + length;
  }
  fail("W-PRESENTATION-0001", { reason: "JPEG dimensions are missing" });
}

function assertImageBounds(state, dimensions) {
  const { width, height } = dimensions;
  if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0) fail("W-PRESENTATION-0001", { reason: "image dimensions are invalid" });
  if (width > state.limits.imageWidth) fail("W-PRESENTATION-0004", { kind: "imageWidth", maximum: state.limits.imageWidth });
  if (height > state.limits.imageHeight) fail("W-PRESENTATION-0004", { kind: "imageHeight", maximum: state.limits.imageHeight });
  const pixels = width * height;
  if (!Number.isSafeInteger(pixels) || pixels > state.limits.imagePixels) fail("W-PRESENTATION-0004", { kind: "imagePixels", maximum: state.limits.imagePixels });
  return { width, height, pixels };
}

function assertEffect(state, effect) {
  if (typeof effect !== "string" || !EFFECT_MASK.has(effect)) fail("W-PRESENTATION-0005", { effect, allowed: [...EFFECT_MASK].sort() });
  state.effects.push(effect);
}

function installCompilerFallback(state, text = "presentation unavailable") {
  state.fallback = "compilerSummary";
  state.fallbackText = String(text);
  while (Buffer.byteLength(state.fallbackText, "utf8") > 256) state.fallbackText = state.fallbackText.slice(0, -1);
  state.output = { media: "text/plain", text: state.fallbackText };
}

function clearPartialEntries(state) {
  state.entries = [];
  state.bytes = 0;
  state.textBytes = 0;
  state.imageBytes = 0;
  state.jsonNodes = 0;
  state.jsonDepth = 0;
  state.jsonStringBytes = 0;
}

function finish(state) {
  requireOpen(state);
  if (!state.entries.some((entry) => entry.media === "text/plain")) fail("W-PRESENTATION-0007", { reason: "text/plain is required" });
  state.phase = "finished";
  state.output = { representations: state.entries.length, bytes: state.bytes, fallback: state.fallback };
}

function deriveFallback(state, facts) {
  requireOpen(state);
  if (!facts || typeof facts !== "object") fail("W-PRESENTATION-0010", { reason: "fallback eligibility facts are missing" });
  const derivesSuccess = (candidate) => {
    if (!candidate || candidate.conforms !== true || !Array.isArray(candidate.effects) || !Array.isArray(candidate.trace)) return false;
    if (candidate.effects.some((effect) => !EFFECT_MASK.has(effect))) return false;
    const attempted = candidate.trace.some((event) => event?.event === "attempt");
    const succeeded = candidate.trace.some((event) => event?.event === "success");
    const failed = candidate.trace.some((event) => ["failure", "cancel", "timeout"].includes(event?.event));
    return attempted && succeeded && !failed;
  };
  const presentable = facts.presentable;
  const display = facts.display;
  const compiler = facts.compiler;
  if (derivesSuccess(presentable)) state.fallback = "presentable";
  else if (derivesSuccess(display)) state.fallback = "display";
  else if (compiler?.sourceKind === "compilerSummary" && Array.isArray(compiler.trace) &&
      compiler.trace.some((event) => event?.event === "attempt") &&
      compiler.trace.some((event) => event?.event === "success") &&
      typeof compiler.text === "string" && compiler.text.length > 0) {
    if (Buffer.byteLength(compiler.text, "utf8") > 256) fail("W-PRESENTATION-0004", { kind: "textBytes", maximum: 256 });
    if (/\b(secret|token|password|pointer|capability|raw[_-]?frame)\b/i.test(compiler.text)) fail("W-PRESENTATION-0010", { reason: "compiler fallback text contains private data" });
    state.fallback = "compilerSummary";
    state.fallbackText = compiler.text;
  } else fail("W-PRESENTATION-0010", { reason: "no eligible fallback remains" });
}

function redactError(state, facts) {
  requireOpen(state);
  const deny = new Set(["secret", "token", "password", "pointer", "capability", "rawframe", "raw_frame", "live"]);
  const canonicalMessages = new Set(["presentation failed", "presentation cancelled", "presentation fallback"]);
  if (!facts || typeof facts.code !== "string" || typeof facts.publicMessage !== "string" || !Array.isArray(facts.privateFields)) fail("W-PRESENTATION-0010", { reason: "structured redaction facts are missing" });
  const fields = facts.privateFields.map((field) => {
    if (typeof field === "string") return field.toLowerCase();
    if (field && Array.isArray(field.path)) return field.path.at(-1)?.toString().toLowerCase();
    return null;
  });
  if (fields.some((field) => !field || !deny.has(field))) fail("W-PRESENTATION-0010", { reason: "private field facts are malformed" });
  if (!canonicalMessages.has(facts.publicMessage)) fail("W-PRESENTATION-0010", { reason: "public error is not canonical redacted text" });
  state.error = { code: facts.code, message: facts.publicMessage, redacted: fields.length > 0 };
}

export function runPresentationProgram(operations = []) {
    const state = initialState();
  try {
    for (const operation of operations) {
      const op = operation?.op;
      assertNoConclusionFields(operation ?? {}, ["userCode", "sourceFallback"]);
      if (op === "open") {
        if (state.phase !== "empty") fail("W-PRESENTATION-0008", { reason: "writer already opened" });
        state.phase = "open";
        state.limits = { ...defaultLimits(), ...(operation.limits ?? {}) };
        if (operation.submissionOutcome !== undefined && !["ready", "committed", "degraded", "failed", "cancelled", "unchanged"].includes(operation.submissionOutcome)) fail("W-PRESENTATION-0010", { reason: "submission outcome fact is invalid" });
        state.submissionOutcome = operation.submissionOutcome ?? "unchanged";
      } else if (op === "effect") {
        requireOpen(state);
        assertEffect(state, operation.effect);
      } else if (op === "media") {
        requireOpen(state);
        state.lastMedia = parseMedia(operation.media);
      } else if (op === "text") {
        assertEffect(state, "writerWrite");
        if (typeof operation.value !== "string") fail("W-PRESENTATION-0001", { reason: "text payload is not String" });
        addRepresentation(state, "text/plain", operation.value, "text");
      } else if (op === "png" || op === "jpeg") {
        assertEffect(state, "writerWrite");
        const media = op === "png" ? "image/png" : "image/jpeg";
        assertImageBytes(media, operation.bytes);
        const dimensions = op === "png" ? pngDimensions(operation.bytes) : jpegDimensions(operation.bytes);
        const bounded = assertImageBounds(state, dimensions);
        addRepresentation(state, media, operation.bytes, "image", bounded);
      } else if (op === "json" || op === "vendorJson") {
        assertEffect(state, "writerWrite");
        const media = op === "json" ? "application/json" : operation.media;
        if (op === "vendorJson") parseMedia(media);
        const payload = operation.payload;
        if (payload === undefined) fail("W-PRESENTATION-0001", { reason: "JSON payload is missing" });
        const facts = jsonFacts(payload, state.limits);
        if (facts.cycle || facts.bounded) fail("W-PRESENTATION-0004", { kind: facts.depth > state.limits.jsonDepth ? "jsonDepth" : facts.stringBytes > state.limits.jsonStringBytes ? "jsonStringBytes" : "json", maximum: facts.depth > state.limits.jsonDepth ? state.limits.jsonDepth : facts.stringBytes > state.limits.jsonStringBytes ? state.limits.jsonStringBytes : state.limits.totalBytes, reason: facts.cycle ? "cycle" : "payload bound" });
        consumeWork(state, Math.max(1, facts.nodes), "JSON traversal");
        const size = addRepresentation(state, media, payload, op === "json" ? "json" : "vendor-json", facts);
        state.jsonNodes += facts.nodes;
        state.jsonDepth = Math.max(state.jsonDepth, facts.depth);
        state.jsonStringBytes += facts.stringBytes;
        if (state.jsonNodes > state.limits.jsonNodes || state.jsonDepth > state.limits.jsonDepth || state.jsonStringBytes > state.limits.jsonStringBytes) {
          fail("W-PRESENTATION-0004", { kind: state.jsonDepth > state.limits.jsonDepth ? "jsonDepth" : state.jsonStringBytes > state.limits.jsonStringBytes ? "jsonStringBytes" : "jsonNodes", maximum: state.limits.jsonNodes, bytes: size });
        }
      } else if (op === "tablePreview") {
        requireOpen(state);
        assertNoConclusionFields(operation, ["collect", "bytes", "nodes", "depth", "sourceElements"]);
        const plan = operation.plan;
        assertNoNestedConclusionFields(plan, ["collect", "bytes", "nodes", "depth", "sourceElements"]);
        const source = plan?.source;
        if (!plan || !source || !["boundedView", "stream"].includes(source.kind) || typeof source.hasMore !== "boolean" || !Number.isInteger(plan.inspectedRows) || !Number.isInteger(plan.emittedRows) || !Number.isInteger(plan.columns) || !Number.isInteger(source.rows) || plan.inspectedRows < 0 || plan.emittedRows < 0 || plan.columns < 0 || source.rows < 0 || plan.emittedRows > plan.inspectedRows || plan.inspectedRows > source.rows || source.hasMore !== (source.rows > plan.inspectedRows)) fail("W-PRESENTATION-0001", { reason: "table preview plan is not bounded" });
        if (source.kind === "stream" && source.bounded !== true) fail("W-PRESENTATION-0009", { work: "unbounded stream" });
        if (plan.inspectedRows > state.limits.tableRows || plan.emittedRows > state.limits.tableRows || plan.columns > state.limits.tableColumns) fail("W-PRESENTATION-0004", { kind: "tablePreview", maximum: state.limits.tableRows });
        consumeWork(state, Math.max(1, plan.inspectedRows * Math.max(1, plan.columns)), "table preview");
        state.preview = { sourceRows: source.rows, inspectedRows: plan.inspectedRows, emittedRows: plan.emittedRows, hasMore: source.hasMore, columns: plan.columns, collected: false };
      } else if (op === "tensorSummary") {
        requireOpen(state);
        assertNoConclusionFields(operation, ["copyToHost", "copied"]);
        const plan = operation.plan;
        assertNoNestedConclusionFields(plan, ["copyToHost", "copied", "materialization", "userCode"]);
        if (!plan || typeof plan.device !== "string" || typeof plan.shape !== "string" || typeof plan.dtype !== "string" || !plan.storage || !["metadata", "bytes"].includes(plan.storage.view)) fail("W-PRESENTATION-0001", { reason: "tensor summary plan is incomplete" });
        if (plan.storage.view === "bytes") fail("W-PRESENTATION-0009", { work: "device copy" });
        consumeWork(state, 1, "tensor metadata summary");
        state.tensor = { device: plan.device, shape: plan.shape, dtype: plan.dtype, copied: false };
      } else if (op === "sensorSummary") {
        requireOpen(state);
        const plan = operation.plan;
        if (!plan || plan.source !== "stableSnapshot" || typeof plan.distance !== "number" || typeof plan.watcher !== "string") fail("W-PRESENTATION-0005", { reason: "sensor plan reads a live resource" });
        assertEffect(state, "borrow");
        state.sensor = { distance: plan.distance, watcher: plan.watcher, live: false };
      } else if (op === "fallback") {
        assertNoNestedConclusionFields(operation.facts, ["eligible", "outcome", "source", "fallbackSource", "userCode"]);
        deriveFallback(state, operation.facts);
      } else if (op === "redactedError") {
        redactError(state, operation.facts);
      } else if (["displayUpdate", "clearOutput", "liveProgress"].includes(op)) {
        fail("W-PRESENTATION-0002", { reason: "live presentation mutation is outside the append-only baseline" });
      } else if (op === "cancel" || op === "timeout" || op === "failure") {
        requireOpen(state);
        state.cancelled = op === "cancel";
        state.phase = "rejected";
        const code = op === "cancel" ? "W-PRESENTATION-0008" : "W-PRESENTATION-0010";
        const facts = { kind: op, bounded: true, submissionOutcome: state.submissionOutcome };
        state.diagnostics.push({ code, facts });
        clearPartialEntries(state);
        installCompilerFallback(state, op === "cancel" ? "presentation cancelled" : "presentation fallback");
        return { status: "rejected", error: { code, details: facts }, state: clone(state) };
      } else if (op === "finish") {
        finish(state);
      } else {
        fail("invalidOperation", { operation: op });
      }
    }
    return { status: "accepted", state: clone(state) };
  } catch (error) {
    if (!(error instanceof PresentationError)) throw error;
    if (state.phase === "open") {
      state.phase = "rejected";
      state.diagnostics.push({ code: error.code, facts: clone(error.details) });
      clearPartialEntries(state);
      installCompilerFallback(state);
    }
    return { status: "rejected", error: { code: error.code, details: clone(error.details) }, state: clone(state) };
  }
}

export function compactPresentationState(state) {
  return {
    phase: state.phase,
    entries: state.entries.map(({ media, kind, bytes: count, width, height, pixels }) => ({ media, kind, bytes: count, ...(width ? { width } : {}), ...(height ? { height } : {}), ...(pixels ? { pixels } : {}) })),
    bytes: state.bytes,
    workUnits: state.workUnits,
    effects: state.effects,
    fallback: state.fallback,
    fallbackText: state.fallbackText,
    preview: state.preview,
    tensor: state.tensor,
    sensor: state.sensor,
    error: state.error ?? null,
    cancelled: state.cancelled,
    submissionOutcome: state.submissionOutcome,
    output: state.output,
    diagnostics: state.diagnostics,
  };
}
