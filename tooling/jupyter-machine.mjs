import { createHash, createHmac, timingSafeEqual } from "node:crypto";

const CHANNELS = new Set(["shell", "control", "stdin"]);
const PORT_NAMES = ["shell", "iopub", "stdin", "control", "heartbeat"];
const OFFICIAL_CONNECTION_FIELDS = new Set(["transport", "ip", "signature_scheme", "key", "shell_port", "iopub_port", "stdin_port", "control_port", "hb_port", "curve_publickey", "curve_secretkey", "kernel_name"]);
const FILE_FACT_FIELDS = new Set(["permissions", "remoteAllowed", "logSecret", "persistSecret"]);
const READ_REQUESTS = new Set(["complete_request", "inspect_request", "is_complete_request", "history_request"]);
const DENY_FIELDS = new Set(["secret", "password", "raw_frames", "private_key", "token", "live_value"]);
const Z85 = /^[0-9a-zA-Z.\-:+=^!/*?&<>()\[\]{}@%$#]{40}$/;
const OPAQUE = /^opaque:[A-Za-z0-9._:-]{1,128}$/;

export class JupyterError extends Error {
  constructor(code, details = {}) {
    super(code);
    this.name = "JupyterError";
    this.code = code;
    this.details = details;
  }
}

function fail(code, details = {}) {
  throw new JupyterError(code, details);
}

function clone(value) {
  return structuredClone(value);
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") return Object.fromEntries(Object.keys(value).sort().map((key) => [key, canonical(value[key])]));
  return value;
}

export function jupyterDigest(value) {
  return `sha256:${createHash("sha256").update(JSON.stringify(canonical(value))).digest("hex")}`;
}

function defaultLimits() {
  return { frames: 64, frameBytes: 16_384, heartbeatBytes: 4096, heartbeatRing: 32, jsonDepth: 16, metadataBytes: 2048, replay: 64, pending: 8, stdin: 1, history: 32, buffers: 8, unsupported: 16, outputBytes: 8192 };
}

function initialState() {
  return {
    phase: "empty",
    limits: defaultLimits(),
    authenticated: false,
    jsonUsed: false,
    frames: 0,
    frameBytes: 0,
    replay: [],
    authenticatedRequests: new Map(),
    consumedRequests: [],
    consumedFacts: new Map(),
    queue: [],
    requests: [],
    events: [],
    executionCount: 0,
    reservedOrdinal: null,
    generation: "opaque:gen0",
    session: null,
    incarnation: null,
    clientSession: null,
    executionOrdinal: 0,
    history: [],
    outputs: [],
    controls: [],
    reads: 0,
    metadata: [],
    features: [],
    kernelInfo: null,
    shutdown: null,
    lastReply: null,
    lastRead: null,
    stdin: null,
    diagnostics: [],
    mutationBlocked: false,
  };
}

function requireOpen(state) {
  if (state.phase === "empty") fail("W-JUPYTER-0007", { reason: "kernel is not open" });
  if (state.phase === "closed") fail("W-JUPYTER-0008", { reason: "kernel is closed" });
}

function checkKernelspec(spec, policy) {
  if (!spec || !Array.isArray(spec.argv) || !spec.argv.includes("{connection_file}") || spec.display_name !== "W" || spec.language !== "w" || spec.interrupt_mode !== "message" || spec.kernel_protocol_version !== "5.5") {
    fail("W-JUPYTER-0007", { reason: "kernelspec is not deterministic" });
  }
  if (spec.features !== undefined) fail("W-JUPYTER-0007", { reason: "features belong to kernel_info_reply" });
  const advertisedEncryption = spec.metadata?.supported_encryption;
  if (policy?.requireCurve === true && advertisedEncryption !== "curve") fail("W-JUPYTER-0001", { reason: "Curve policy is not advertised" });
  if (advertisedEncryption !== undefined && advertisedEncryption !== "curve") fail("W-JUPYTER-0007", { reason: "kernelspec encryption advertisement is invalid" });
}

function checkKernelInfo(info, state) {
  if (!info || info.implementation !== "W" || typeof info.implementation_version !== "string" || info.implementation_version.length > 128 || info.protocol_version !== "5.5" || !info.language_info || info.language_info.name !== "w" || info.language_info.mimetype !== "text/x-w" || info.language_info.file_extension !== ".w" || typeof info.language_info.version !== "string" || info.language_info.version.length > 128 || typeof info.banner !== "string" || info.banner.length === 0 || info.banner.length > 4096 || !Array.isArray(info.supported_features) || info.supported_features.length !== 0) {
    fail("W-JUPYTER-0007", { reason: "kernel_info_reply does not advertise implemented features" });
  }
  state.features = [];
  state.kernelInfo = { implementation: info.implementation, implementation_version: info.implementation_version, protocol_version: info.protocol_version, supported_features: [], language_info: clone(info.language_info), banner: info.banner };
}

function checkConnection(state, connection, policy, fileFacts) {
  if (!connection || typeof connection !== "object" || Array.isArray(connection) || Object.keys(connection).some((field) => !OFFICIAL_CONNECTION_FIELDS.has(field))) fail("W-JUPYTER-0001", { reason: "connection contains unknown or non-official fields" });
  if (!fileFacts || typeof fileFacts !== "object" || Array.isArray(fileFacts) || Object.keys(fileFacts).some((field) => !FILE_FACT_FIELDS.has(field)) || fileFacts.permissions !== "user-only" || fileFacts.remoteAllowed !== false || fileFacts.logSecret !== false || fileFacts.persistSecret !== false) fail("W-JUPYTER-0001", { reason: "connection file security facts are not host-verified" });
  if (connection.transport !== "tcp" || connection.ip !== "127.0.0.1") fail("W-JUPYTER-0001", { reason: "connection is not loopback" });
  if (connection.kernel_name !== undefined && (typeof connection.kernel_name !== "string" || connection.kernel_name.length === 0 || connection.kernel_name.length > 128)) fail("W-JUPYTER-0001", { reason: "kernel_name is not bounded" });
  if (!/^hmac-(sha256|sha384|sha512)$/.test(connection.signature_scheme) || typeof connection.key !== "string" || Buffer.byteLength(connection.key, "utf8") < 1 || Buffer.byteLength(connection.key, "utf8") > 256) fail("W-JUPYTER-0001", { reason: "HMAC key policy is invalid" });
  const ports = { shell: connection.shell_port, iopub: connection.iopub_port, stdin: connection.stdin_port, control: connection.control_port, heartbeat: connection.hb_port };
  if (PORT_NAMES.some((name) => !Number.isInteger(ports[name]) || ports[name] < 1 || ports[name] > 65_535) || new Set(PORT_NAMES.map((name) => ports[name])).size !== PORT_NAMES.length) fail("W-JUPYTER-0001", { reason: "five named ports must be bounded and unique" });
  if (policy?.requireCurve === true) {
    if (!Z85.test(connection.curve_publickey ?? "") || !Z85.test(connection.curve_secretkey ?? "")) fail("W-JUPYTER-0001", { reason: "Curve Z85 policy is incomplete" });
  }
  state.connection = { signatureScheme: connection.signature_scheme, keyLength: Buffer.byteLength(connection.key, "utf8"), ports: { ...ports }, curve: policy?.requireCurve === true, curveSockets: policy?.requireCurve === true ? [...PORT_NAMES] : [] };
  state.fileFacts = { permissions: "user-only", remoteAllowed: false, logSecret: false, persistSecret: false };
}

function serialisePart(part) {
  if (typeof part !== "string") fail("W-JUPYTER-0001", { reason: "message part is not serialized" });
  return part;
}

function jsonDepth(value, maximum) {
  const stack = [{ value, depth: 1 }];
  const seen = new WeakSet();
  let depth = 0;
  while (stack.length > 0) {
    const item = stack.pop();
    depth = Math.max(depth, item.depth);
    if (depth > maximum) return depth;
    if (!item.value || typeof item.value !== "object") continue;
    if (seen.has(item.value)) return maximum + 1;
    seen.add(item.value);
    const children = Array.isArray(item.value) ? item.value : Object.values(item.value);
    for (const child of children) stack.push({ value: child, depth: item.depth + 1 });
  }
  return depth;
}

function frameParts(frame, limits) {
  const identities = frame.identities ?? [];
  if (!Array.isArray(identities) || identities.some((identity) => typeof identity !== "string" || identity.length > 256)) fail("W-JUPYTER-0001", { reason: "identity frame is invalid" });
  if (frame.delimiter !== "<IDS|MSG>") fail("W-JUPYTER-0001", { reason: "Jupyter delimiter is missing" });
  const header = serialisePart(frame.header);
  const parent = serialisePart(frame.parent_header);
  const metadata = serialisePart(frame.metadata);
  const content = serialisePart(frame.content);
  const buffers = frame.buffers ?? [];
  if (!Array.isArray(buffers) || buffers.length > limits.buffers || buffers.some((buffer) => typeof buffer !== "string" || Buffer.byteLength(buffer, "utf8") > limits.frameBytes)) fail("W-JUPYTER-0002", { reason: "buffer quota exceeded" });
  const frameCount = identities.length + 6 + buffers.length;
  if (frame.frameCount !== undefined && frame.frameCount !== frameCount) fail("W-JUPYTER-0002", { reason: "message frame count is invalid", frameCount });
  return { identities, header, parent, metadata, content, buffers, frameCount };
}

function hmacFor(key, signatureScheme, parts) {
  const algorithm = signatureScheme.replace("hmac-", "");
  return createHmac(algorithm, key).update(parts.header + (parts.parent ?? parts.parent_header) + parts.metadata + parts.content).digest("hex");
}

function validateWireContent(msgType, channel, content) {
  if (!content || typeof content !== "object" || Array.isArray(content)) fail("W-JUPYTER-0001", { reason: "wire content is not an object" });
  const allowed = {
    execute_request: new Set(["code", "silent", "store_history", "user_expressions", "allow_stdin", "stop_on_error"]),
    complete_request: new Set(["code", "cursor_pos"]),
    inspect_request: new Set(["code", "cursor_pos", "detail_level"]),
    is_complete_request: new Set(["code"]),
    history_request: new Set(["output", "hist_access_type", "session", "start", "stop", "n"]),
    interrupt_request: new Set(),
    shutdown_request: new Set(["restart"]),
    input_reply: new Set(["value"]),
    kernel_info_request: new Set(),
  }[msgType];
  if (allowed && Object.keys(content).some((key) => !allowed.has(key))) fail("W-JUPYTER-0001", { reason: "wire content contains host-only facts", msgType });
  if (msgType === "execute_request") {
    if (typeof content.code !== "string" || typeof content.silent !== "boolean" || typeof content.store_history !== "boolean" || typeof content.allow_stdin !== "boolean" || typeof content.stop_on_error !== "boolean" || (content.user_expressions !== undefined && (!content.user_expressions || Array.isArray(content.user_expressions) || typeof content.user_expressions !== "object" || Object.values(content.user_expressions).some((source) => typeof source !== "string")))) fail("W-JUPYTER-0001", { reason: "execute_request wire fields are invalid" });
  }
  if (msgType === "input_reply" && typeof content.value !== "string") fail("W-JUPYTER-0001", { reason: "input_reply wire fields are invalid" });
  if (["complete_request", "inspect_request"].includes(msgType) && (typeof content.code !== "string" || !Number.isInteger(content.cursor_pos) || content.cursor_pos < 0)) fail("W-JUPYTER-0001", { reason: "cursor request wire fields are invalid" });
  if (msgType === "is_complete_request" && typeof content.code !== "string") fail("W-JUPYTER-0001", { reason: "is_complete request wire fields are invalid" });
  if (msgType === "history_request" && (content.output !== undefined && typeof content.output !== "boolean" || content.hist_access_type !== undefined && typeof content.hist_access_type !== "string" || content.n !== undefined && (!Number.isInteger(content.n) || content.n < 0) || content.start !== undefined && (!Number.isInteger(content.start) || content.start < 0) || content.stop !== undefined && (!Number.isInteger(content.stop) || content.stop < 0))) fail("W-JUPYTER-0001", { reason: "history request wire fields are invalid" });
  if (channel === "stdin" && msgType !== "input_reply") fail("W-JUPYTER-0001", { reason: "stdin accepts input_reply only" });
  if (channel === "control" && !["interrupt_request", "shutdown_request"].includes(msgType)) fail("W-JUPYTER-0001", { reason: "control accepts interrupt/shutdown only" });
}

function receiveFrame(state, frame) {
  requireOpen(state);
  const channel = frame.channel;
  if (!CHANNELS.has(channel)) fail("W-JUPYTER-0001", { reason: "message channel is not a kernel input socket" });
  const parts = frameParts(frame, state.limits);
  const totalBytes = [...parts.identities, frame.delimiter, frame.signature, parts.header, parts.parent, parts.metadata, parts.content, ...parts.buffers].reduce((sum, part) => sum + Buffer.byteLength(String(part), "utf8"), 0);
  if (!Number.isInteger(totalBytes) || totalBytes > state.limits.frameBytes) fail("W-JUPYTER-0002", { reason: "frame byte quota exceeded" });
  state.frames += 1;
  state.frameBytes = Math.min(Number.MAX_SAFE_INTEGER, state.frameBytes + totalBytes);
  state.frameRing = [...(state.frameRing ?? []), { bytes: totalBytes, msgId: null }].slice(-state.limits.frames);
  const expected = hmacFor(state.key, state.signatureScheme, parts);
  if (typeof frame.signature !== "string" || !/^[0-9a-f]+$/.test(frame.signature) || frame.signature.length !== expected.length || !timingSafeEqual(Buffer.from(frame.signature), Buffer.from(expected))) fail("W-JUPYTER-0001", { reason: "HMAC failed", jsonUsed: state.jsonUsed });
  if (state.replay.includes(frame.signature)) fail("W-JUPYTER-0001", { reason: "replay" });
  // JSON is parsed only after delimiter, quota, and HMAC checks succeed.
  let header;
  let parent;
  let metadata;
  let content;
  try {
    header = JSON.parse(parts.header);
    parent = JSON.parse(parts.parent);
    metadata = JSON.parse(parts.metadata);
    content = JSON.parse(parts.content);
  } catch {
    fail("W-JUPYTER-0001", { reason: "serialized JSON invalid", jsonUsed: state.jsonUsed });
  }
  validateWireContent(header.msg_type, channel, content);
  const depth = Math.max(jsonDepth(header, state.limits.jsonDepth), jsonDepth(parent, state.limits.jsonDepth), jsonDepth(metadata, state.limits.jsonDepth), jsonDepth(content, state.limits.jsonDepth));
  if (depth > state.limits.jsonDepth) fail("W-JUPYTER-0002", { reason: "JSON depth quota exceeded" });
  if (typeof header.msg_id !== "string" || header.msg_id.length === 0 || header.msg_id.length > 128 || typeof header.session !== "string" || header.session.length === 0 || header.session.length > 128 || typeof header.username !== "string" || header.username.length === 0 || header.username.length > 128 || typeof header.date !== "string" || header.date.length === 0 || header.date.length > 128 || Number.isNaN(Date.parse(header.date)) || typeof header.msg_type !== "string" || header.msg_type.length === 0 || header.msg_type.length > 128 || header.version !== "5.5") fail("W-JUPYTER-0001", { reason: "authenticated header is incomplete" });
  state.jsonUsed = true;
  state.authenticated = true;
  state.replay.push(frame.signature);
  if (state.replay.length > state.limits.replay) state.replay.shift();
  const request = { msgId: header.msg_id, channel, msgType: header.msg_type, clientSession: header.session, username: header.username, identities: parts.identities, parent, metadata, content: channel === "stdin" && header.msg_type === "input_reply" ? {} : content };
  if (state.authenticatedRequests.has(request.msgId)) fail("W-JUPYTER-0001", { reason: "message id replay", msgId: request.msgId });
  if (state.consumedFacts.has(request.msgId)) fail("W-JUPYTER-0001", { reason: "message id replay", msgId: request.msgId });
  if (state.authenticatedRequests.size >= state.limits.pending + state.limits.replay) fail("W-JUPYTER-0002", { reason: "authenticated request ring quota exceeded" });
  state.authenticatedRequests.set(request.msgId, request);
  state.clientSession ??= request.clientSession;
  if (request.msgType === "kernel_info_request") state.kernelInfoRequest = request.msgId;
  if (!READ_REQUESTS.has(request.msgType) && request.msgType !== "execute_request" && request.msgType !== "interrupt_request" && request.msgType !== "shutdown_request" && request.msgType !== "kernel_info_request" && !(request.channel === "stdin" && request.msgType === "input_reply")) {
    state.unsupported = [...(state.unsupported ?? []), request.msgType].slice(-state.limits.unsupported);
    consumeRequest(state, request.msgId);
  }
  return request;
}

function requestFor(state, msgId, channel, type) {
  const request = state.authenticatedRequests.get(msgId);
  if (!request || request.channel !== channel || request.msgType !== type) fail("W-JUPYTER-0001", { reason: "operation does not match authenticated request", msgId, channel, type });
  return request;
}

function consumeRequest(state, msgId) {
  if (state.consumedFacts.has(msgId)) fail("W-JUPYTER-0001", { reason: "authenticated request was consumed twice", msgId });
  const request = state.authenticatedRequests.get(msgId);
  if (!request) fail("W-JUPYTER-0001", { reason: "authenticated request is not pending", msgId });
  state.authenticatedRequests.delete(msgId);
  state.consumedFacts.set(msgId, { msgId: request.msgId, channel: request.channel, msgType: request.msgType, clientSession: request.clientSession, parentMsgId: typeof request.parent?.msg_id === "string" ? request.parent.msg_id : null });
  state.consumedRequests.push(msgId);
  if (state.consumedRequests.length > state.limits.replay) {
    const old = state.consumedRequests.shift();
    state.consumedFacts.delete(old);
  }
}

function walkFields(value) {
  const fields = [];
  const stack = [value];
  const seen = new WeakSet();
  while (stack.length > 0) {
    const current = stack.pop();
    if (!current || typeof current !== "object") continue;
    if (seen.has(current)) continue;
    seen.add(current);
    for (const [key, child] of Object.entries(current)) {
      fields.push(key.toLowerCase());
      stack.push(child);
    }
  }
  return fields;
}

function metadata(state, value) {
  if (!value || typeof value !== "object" || Object.keys(value).some((key) => key !== "w")) fail("W-JUPYTER-0007", { reason: "metadata is not namespaced" });
  const w = value.w;
  if (!w || typeof w.schema !== "number" || w.session !== state.session || w.incarnation !== state.incarnation || w.generation !== state.generation || !w.outcome || typeof w.outcome.status !== "string") fail("W-JUPYTER-0007", { reason: "W metadata identity fields are incomplete" });
  if (walkFields(value).some((field) => DENY_FIELDS.has(field))) fail("W-JUPYTER-0007", { reason: "metadata contains a denied secret field" });
  let size;
  try { size = Buffer.byteLength(JSON.stringify(value), "utf8"); } catch { fail("W-JUPYTER-0007", { reason: "metadata is not serializable" }); }
  if (size > state.limits.metadataBytes) fail("W-JUPYTER-0002", { reason: "metadata quota exceeded" });
  state.metadata.push(clone(value));
}

function lifecycle(state, requestId, outputs) {
  const sequence = ["busy", "process", "reply"];
  if (outputs > 0) sequence.push("outputs");
  sequence.push("idle");
  state.events.push({ requestId, sequence });
  return sequence;
}

function validateOutputs(state, requestId, outputs) {
  if (!Array.isArray(outputs) || outputs.length > state.limits.buffers) fail("W-JUPYTER-0002", { reason: "output quota exceeded" });
  for (const output of outputs) {
    let outputBytes;
    try { outputBytes = Buffer.byteLength(JSON.stringify(output), "utf8"); } catch { fail("W-JUPYTER-0002", { reason: "output is not serializable" }); }
    if (!output || typeof output.media !== "string" || (!["text/plain", "application/json", "image/png", "image/jpeg"].includes(output.media) && !/^application\/vnd\.[a-z0-9.-]+\+json$/.test(output.media)) || outputBytes > state.limits.outputBytes || (output.parentMsgId !== undefined && output.parentMsgId !== requestId)) fail("W-JUPYTER-0002", { reason: "output media or parent is invalid" });
  }
}

function executeOne(state, operation, request) {
  requireOpen(state);
  const content = request.content ?? {};
  const analysis = operation.analysis ?? {};
  const requestId = request.msgId;
  const silent = content.silent === true;
  const storeHistory = silent ? false : content.store_history === true;
  const readOnly = analysis.readOnly === true;
  const effectFree = analysis.effectFree === true;
  const mutation = analysis.mutation === true;
  if (mutation && state.mutationBlocked) fail("W-JUPYTER-0005", { reason: "degraded publication blocks mutation" });
  const pyn2 = analysis.pyn2 ?? {};
  const outcome = pyn2.status ?? (mutation ? "committed" : "ready");
  const publication = pyn2.publication ?? "committed";
  if ((silent || !storeHistory) && (!readOnly || !effectFree || mutation)) fail("W-JUPYTER-0005", { reason: "silent or no-history request is not read-only and effect-free" });
  const expressionResults = analysis.userExpressionResults ?? {};
  if (content.user_expressions && typeof content.user_expressions === "object" && !Array.isArray(content.user_expressions) && Object.keys(content.user_expressions).length > 0) {
    if (outcome !== "ready" && outcome !== "committed") fail("W-JUPYTER-0005", { reason: "user expressions run only after main success" });
    if (!expressionResults || typeof expressionResults !== "object" || Array.isArray(expressionResults)) fail("W-JUPYTER-0005", { reason: "user expression host results are missing" });
    for (const key of Object.keys(content.user_expressions)) {
      const result = expressionResults[key];
      if (!result || !["ok", "error"].includes(result.status) || result.mutates === true) fail("W-JUPYTER-0005", { reason: "user expression result is not per-key" });
    }
  }
  if (analysis.stdin) {
    if (content.allow_stdin !== true || analysis.stdin.waiters !== 1 || analysis.stdin.originShellMsgId !== requestId || typeof analysis.stdin.password !== "boolean") fail("W-JUPYTER-0006", { reason: "stdin waiter is not routed to origin shell" });
    if (state.stdin?.originShellMsgId) fail("W-JUPYTER-0006", { reason: "stdin already has a waiter" });
    state.stdin = { originShellMsgId: requestId, clientSession: request.clientSession, waiters: 1, passwordPrompted: analysis.stdin.password, exportable: false, persisted: false };
    state.stdinRequest = { originShellMsgId: requestId, identities: request.identities, clientSession: request.clientSession, parentExecute: requestId, password: analysis.stdin.password };
  }
  if (storeHistory && state.history.length >= state.limits.history) fail("W-JUPYTER-0002", { reason: "history quota exceeded" });
  let reservedOrdinal = null;
  if (storeHistory) {
    state.executionCount += 1;
    reservedOrdinal = state.executionCount;
    state.history.push({ requestId, ordinal: reservedOrdinal });
  }
  state.reservedOrdinal = reservedOrdinal;
  const generationBefore = state.generation;
  if (outcome === "committed" && mutation) {
    state.generation = `opaque:${jupyterDigest({ before: generationBefore, requestId }).slice(7, 23)}`;
    if (publication === "degraded") state.mutationBlocked = true;
  }
  const reply = outcome === "ready" || outcome === "committed" ? "ok" : outcome === "cancelled" ? "error:WCancelled" : outcome === "queuedAborted" ? "error:WQueueAborted" : "error";
  const outputs = silent ? [] : (Array.isArray(analysis.outputs) ? analysis.outputs : []);
  validateOutputs(state, requestId, outputs);
  const sequence = lifecycle(state, requestId, outputs.length);
  state.outputs.push(...outputs.map((output, index) => ({ requestId, index, parent: requestId, media: output.media ?? "text/plain" })));
  state.executionOrdinal = state.executionCount;
  state.lastReply = { requestId, status: reply, executionCount: state.executionCount, reservedOrdinal, generationBefore, generationAfter: state.generation, sequence };
  state.requests.push({ requestId, reservedOrdinal, executionCount: state.executionCount, generation: state.generation, silent, storeHistory, outcome, reply, userExpressions: Object.keys(content.user_expressions ?? {}).map((key) => ({ key })) });
  if (analysis.metadata) metadata(state, analysis.metadata);
  return state.lastReply;
}

function control(state, operation, request) {
  requireOpen(state);
  const content = request.content ?? {};
  const analysis = operation.analysis ?? {};
  if (request.msgType === "interrupt_request") {
    if (Object.keys(content).length !== 0 || analysis.admitted !== true) fail("W-JUPYTER-0008", { reason: "interrupt admission not confirmed" });
    const sequence = lifecycle(state, request.msgId, 0);
    state.controls.push({ kind: "interrupt", requestId: request.msgId, admitted: true, terminated: analysis.terminated === true, sequence });
  } else if (request.msgType === "shutdown_request") {
    if (typeof content.restart !== "boolean" || analysis.safeClose !== true || state.queue.length > 0) fail("W-JUPYTER-0008", { reason: "shutdown before safe drain" });
    const sequence = lifecycle(state, request.msgId, 0);
    state.controls.push({ kind: "shutdown", requestId: request.msgId, safeClose: true, drained: true, sequence, restart: content.restart });
    state.shutdown = "ok";
    state.phase = "closed";
  } else fail("W-JUPYTER-0008", { reason: "unknown control request" });
}

function readRequest(state, operation, request) {
  requireOpen(state);
  const content = request.content ?? {};
  const analysis = operation.analysis ?? {};
  if (!READ_REQUESTS.has(request.msgType)) fail("W-JUPYTER-0009", { reason: "request is not read-only" });
  if (analysis.offsetUnit && analysis.offsetUnit !== "unicode-codepoint") fail("W-JUPYTER-0009", { reason: "cursor offset is not a Unicode code point" });
  if (analysis.readsStaging === true || analysis.executes === true) fail("W-JUPYTER-0009", { reason: "read request observed staging or execution" });
  const analyzer = analysis.analyzer ?? {};
  if (request.msgType === "inspect_request" && analyzer.plainText !== true) fail("W-JUPYTER-0009", { reason: "inspect reply must contain text/plain" });
  if (request.msgType === "history_request" && content.hist_access_type !== "tail") fail("W-JUPYTER-0009", { reason: "history baseline exposes tail only" });
  if (request.msgType === "history_request" && ["range", "search"].includes(content.hist_access_type)) fail("W-JUPYTER-0009", { reason: "history range/search is unsupported" });
  if (request.msgType === "is_complete_request" && !["complete", "incomplete", "invalid", "unknown"].includes(analyzer.status)) fail("W-JUPYTER-0009", { reason: "is_complete status is invalid" });
  const outputs = Array.isArray(analysis.outputs) ? analysis.outputs : [];
  validateOutputs(state, request.msgId, outputs);
  state.reads += 1;
  const sequence = lifecycle(state, request.msgId, outputs.length);
  state.outputs.push(...outputs.map((output, index) => ({ requestId: request.msgId, index, parent: request.msgId, media: output.media })));
  state.lastRead = { requestId: request.msgId, kind: request.msgType, plainText: request.msgType === "inspect_request" ? analyzer.plainText === true : false, status: analyzer.status ?? null, snapshot: "committed", sequence };
}

function stdinReply(state, request) {
  requireOpen(state);
  if (request.channel !== "stdin" || request.msgType !== "input_reply") fail("W-JUPYTER-0006", { reason: "stdin reply is not on stdin input_reply" });
  if (!state.stdin || state.stdin.waiters !== 1 || state.stdin.originShellMsgId.length === 0) fail("W-JUPYTER-0006", { reason: "stdin reply has no origin waiter" });
  if (request.clientSession !== state.stdin.clientSession || request.parent?.msg_id !== state.stdin.originShellMsgId) fail("W-JUPYTER-0006", { reason: "stdin reply is not correlated to origin shell" });
  const sequence = lifecycle(state, request.msgId, 0);
  state.stdin = { ...state.stdin, waiters: 0, replied: true, persisted: false, exportable: false };
  state.stdinReplySequence = sequence;
  state.stdinRequest = null;
}

function drain(state) {
  requireOpen(state);
  let abortExecutes = false;
  while (state.queue.length > 0) {
    const item = state.queue.shift();
    const request = state.authenticatedRequests.get(item.frameMsgId);
    if (!request) fail("W-JUPYTER-0001", { reason: "queued request lacks authenticated frame" });
    if (abortExecutes && request.msgType === "execute_request") {
      consumeRequest(state, request.msgId);
      const sequence = lifecycle(state, request.msgId, 0);
      state.lastReply = { requestId: request.msgId, status: "error:WQueueAborted", executionCount: state.executionCount, reservedOrdinal: null, generationBefore: state.generation, generationAfter: state.generation, sequence };
      state.requests.push({ requestId: request.msgId, reservedOrdinal: null, executionCount: state.executionCount, generation: state.generation, outcome: "queuedAborted", reply: "error:WQueueAborted" });
      continue;
    }
    consumeRequest(state, request.msgId);
    if (request.msgType === "execute_request") executeOne(state, item, request);
    else if (READ_REQUESTS.has(request.msgType)) readRequest(state, item, request);
    else if (request.msgType === "interrupt_request" || request.msgType === "shutdown_request") control(state, item, request);
    if (request.msgType === "execute_request" && state.lastReply?.status === "error" && request.content?.stop_on_error === true) abortExecutes = true;
  }
}

function expandFrameOperation(operation, key, signatureScheme) {
  if (!operation.request) return operation;
  const request = operation.request;
  if (request.channel === "heartbeat" || request.msgType === "heartbeat_request" || request.msgType === "input_request") fail("W-JUPYTER-0001", { reason: "heartbeat and input_request are outgoing operations" });
  const header = {
    msg_id: request.msgId,
    session: request.session ?? "client",
    username: request.username ?? "client",
    date: request.date ?? "2026-01-01T00:00:00Z",
    msg_type: request.msgType,
    version: request.version ?? "5.5",
  };
  const parts = { identities: request.identities ?? ["client"], delimiter: "<IDS|MSG>", header: JSON.stringify(header), parent_header: JSON.stringify(request.parent ?? {}), metadata: JSON.stringify(request.metadata ?? {}), content: JSON.stringify(request.content ?? {}), buffers: request.buffers ?? [] };
  const signature = hmacFor(key, signatureScheme, parts);
  return { op: "frame", channel: request.channel, ...parts, signature: request.tamperSignature ? `${signature.slice(0, -1)}${signature.endsWith("0") ? "1" : "0"}` : signature };
}

export function prepareJupyterOperations(operations = []) {
  const open = operations.find((operation) => operation?.op === "open") ?? {};
  const key = open.connection?.key ?? "case-key";
  const signatureScheme = open.connection?.signature_scheme ?? "hmac-sha256";
  return operations.map((operation) => expandFrameOperation(operation, key, signatureScheme));
}

export function runJupyterProgram(operations = []) {
  const state = initialState();
  try {
    for (const operation of prepareJupyterOperations(operations)) {
      switch (operation?.op) {
        case "open":
          if (state.phase !== "empty") fail("W-JUPYTER-0007", { reason: "kernel already open" });
          state.phase = "open";
          state.limits = { ...defaultLimits(), ...(operation.limits ?? {}) };
          checkKernelspec(operation.kernelspec, operation.policy);
          checkConnection(state, operation.connection, operation.policy, operation.fileFacts);
          state.key = operation.connection.key;
          state.signatureScheme = operation.connection.signature_scheme;
          checkKernelInfo(operation.kernelInfo, state);
          const host = operation.host ?? {};
          if (typeof host.sessionId !== "string" || host.sessionId.length === 0 || host.sessionId.length > 128 || !Number.isInteger(host.incarnation) || host.incarnation < 1 || !OPAQUE.test(host.generationId ?? "") || !Number.isInteger(host.executionOrdinal) || host.executionOrdinal < 0) fail("W-JUPYTER-0007", { reason: "host PYN2 session state is incomplete" });
          state.session = host.sessionId;
          state.incarnation = host.incarnation;
          state.generation = host.generationId;
          state.executionOrdinal = host.executionOrdinal;
          state.executionCount = host.executionOrdinal;
          break;
        case "frame":
          receiveFrame(state, operation);
          break;
        case "kernelInfoReply":
          if (!state.kernelInfoRequest) fail("W-JUPYTER-0007", { reason: "kernel_info_reply has no request" });
          consumeRequest(state, state.kernelInfoRequest);
          lifecycle(state, state.kernelInfoRequest, 0);
          state.kernelInfoReply = state.kernelInfoRequest;
          break;
        case "execute": {
          const request = requestFor(state, operation.frameMsgId, "shell", "execute_request");
          if (state.authenticatedRequests.has(request.msgId)) consumeRequest(state, request.msgId);
          executeOne(state, operation, request);
          break;
        }
        case "lifecycle": {
          const request = state.authenticatedRequests.get(operation.frameMsgId) ?? state.consumedFacts.get(operation.frameMsgId);
          if (!request || request.msgType !== "execute_request") fail("W-JUPYTER-0001", { reason: "lifecycle lacks execute request" });
          const expected = ["busy", "process", "reply"];
          if ((operation.analysis?.outputs?.length ?? 0) > 0) expected.push("outputs");
          expected.push("idle");
          if (JSON.stringify(operation.sequence) !== JSON.stringify(expected)) fail("W-JUPYTER-0003", { expected, actual: operation.sequence });
          break;
        }
        case "control": {
          const request = state.authenticatedRequests.get(operation.frameMsgId);
          if (!request || request.channel !== "control" || !["interrupt_request", "shutdown_request"].includes(request.msgType)) fail("W-JUPYTER-0001", { reason: "control request is not authenticated" });
          consumeRequest(state, request.msgId);
          control(state, operation, request);
          break;
        }
        case "heartbeat": {
          if (typeof operation.bytes !== "string" || Buffer.byteLength(operation.bytes, "utf8") > state.limits.heartbeatBytes) fail("W-JUPYTER-0002", { reason: "heartbeat echo exceeds quota" });
          const bytes = Buffer.byteLength(operation.bytes, "utf8");
          state.heartbeatBytes = bytes;
          state.heartbeatRing = [...(state.heartbeatRing ?? []), { bytes: operation.bytes, size: bytes }].slice(-state.limits.heartbeatRing);
          state.heartbeatEcho = operation.bytes;
          break;
        }
        case "stdinReply": {
          const request = state.authenticatedRequests.get(operation.frameMsgId);
          if (!request) fail("W-JUPYTER-0001", { reason: "stdin reply is unauthenticated" });
          consumeRequest(state, request.msgId);
          stdinReply(state, request);
          break;
        }
        case "read": {
          const request = state.authenticatedRequests.get(operation.frameMsgId);
          if (!request) fail("W-JUPYTER-0001", { reason: "read request is unauthenticated" });
          consumeRequest(state, request.msgId);
          readRequest(state, operation, request);
          break;
        }
        case "enqueue":
          requireOpen(state);
          if (state.queue.length >= state.limits.pending) fail("W-JUPYTER-0002", { reason: "pending request quota exceeded" });
          {
            const queued = state.authenticatedRequests.get(operation.frameMsgId);
            if (!queued || queued.channel !== "shell" || !["execute_request", ...READ_REQUESTS].includes(queued.msgType)) fail("W-JUPYTER-0001", { reason: "only authenticated shell requests may enter the FIFO" });
          }
          state.queue.push({ frameMsgId: operation.frameMsgId, analysis: clone(operation.analysis ?? {}) });
          break;
        case "drain":
          drain(state);
          break;
        case "metadata":
          requireOpen(state);
          if (typeof operation.frameMsgId !== "string" || state.lastReply?.requestId !== operation.frameMsgId) fail("W-JUPYTER-0007", { reason: "metadata is not attached to an authenticated reply" });
          metadata(state, operation.value);
          break;
        default:
          fail("invalidOperation", { reason: "unknown operation", operation: operation?.op });
      }
    }
    const publicState = { ...state, authenticatedRequests: undefined, key: undefined, signatureScheme: undefined };
    return { status: "accepted", state: clone(publicState) };
  } catch (error) {
    if (!(error instanceof JupyterError)) throw error;
    state.phase = state.phase === "closed" ? "closed" : "rejected";
    state.diagnostics = [{ code: error.code, facts: clone(error.details) }];
    const publicState = { ...state, authenticatedRequests: undefined, key: undefined, signatureScheme: undefined };
    return { status: "rejected", error: { code: error.code, details: clone(error.details) }, state: clone(publicState) };
  }
}

export function compactJupyterState(state) {
  return {
    phase: state.phase,
    authenticated: state.authenticated,
    jsonUsed: state.jsonUsed,
    frames: state.frames,
    frameBytes: state.frameBytes,
    replay: state.replay,
    queue: state.queue.map((request) => request.frameMsgId),
    executionCount: state.executionCount,
    reservedOrdinal: state.reservedOrdinal,
    generation: state.generation,
    session: state.session,
    incarnation: state.incarnation,
    clientSession: state.clientSession,
    connection: state.connection ?? null,
    fileFacts: state.fileFacts ?? null,
    executionOrdinal: state.executionOrdinal,
    history: state.history,
    events: state.events,
    requests: state.requests,
    lastReply: state.lastReply ?? null,
    outputs: state.outputs,
    controls: state.controls,
    reads: state.reads,
    lastRead: state.lastRead ?? null,
    metadata: state.metadata,
    features: state.features,
    kernelInfo: state.kernelInfo,
    unsupported: state.unsupported ?? [],
    heartbeatEcho: state.heartbeatEcho ?? null,
    heartbeatBytes: state.heartbeatBytes ?? 0,
    heartbeatRing: state.heartbeatRing ?? [],
    shutdown: state.shutdown,
    stdin: state.stdin,
    stdinRequest: state.stdinRequest ?? null,
    stdinReplySequence: state.stdinReplySequence ?? null,
    mutationBlocked: state.mutationBlocked,
    diagnostics: state.diagnostics ?? [],
  };
}
