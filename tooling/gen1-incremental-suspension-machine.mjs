import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const bundleDirectory = path.join(toolingDirectory, "studies", "gen1-incremental-suspension");

export const LOWERINGS = ["switched-resume-frame", "returned-continuation-state-loop"];

const operationKinds = new Set([
  "open", "pullAcquire", "pullItem", "pullNone", "pullFailure", "resumeAcquire", "resumeValue", "resumeNone", "resumeFailure",
  "borrowView", "endView", "escapeView", "beginClose", "cleanup", "drop", "drain", "commit",
  "spawn", "join", "openChannel", "sendOffer", "receiveMatch", "closeChannel", "cancel",
  "registerCallback", "callbackEnter", "callbackReturn", "unregisterCallback", "drainCallback", "destroyLease",
]);

function sourcePath(relativePath) {
  const resolved = path.resolve(bundleDirectory, relativePath);
  const relative = path.relative(wDirectory, resolved);
  if (!relative || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) throw new Error(`source path escapes repository: ${relativePath}`);
  return resolved;
}

function safeValue(value) {
  if (value === undefined) return undefined;
  if (value === null || ["string", "number", "boolean"].includes(typeof value)) return value;
  if (Array.isArray(value)) return value.map(safeValue);
  if (typeof value === "object") return Object.fromEntries(Object.entries(value).map(([key, child]) => [key, safeValue(child)]));
  return String(value);
}

function digest(text) { return `sha256:${crypto.createHash("sha256").update(text).digest("hex")}`; }

/* The shared part is schema validation, trace packing, and invariant comparison only.
 * The frame and returned-state engines below have separate state stores and handlers. */
function createRun(lowering) {
  return {
    lowering, status: "accepted", reason: null, operation: null,
    logicalTrace: [], physicalTrace: [], operationResults: [],
    channels: new Map(), callbacks: new Map(), cancellation: [], cleanup: [],
    typedResult: "pending", singleOwnerProof: true, commitHappensBefore: [],
  };
}

function trace(run, index, event, details = {}) {
  const item = { index, event, ...Object.fromEntries(Object.entries(details).filter(([, value]) => value !== undefined)) };
  run.logicalTrace.push(safeValue(item));
  const physical = run.lowering === LOWERINGS[0]
    ? { index, op: `frame:${event}`, packing: "suspended-frame-slots" }
    : { index, op: `state-loop:${event}`, packing: "returned-state-token" };
  run.physicalTrace.push(physical);
}

function accept(run, index, event, details = {}) {
  run.operationResults.push({ index, status: "accepted", event, ...safeValue(details) });
  trace(run, index, event, details);
}

function reject(run, index, reason, details = {}) {
  if (run.status === "rejected") return;
  run.status = "rejected";
  run.reason = reason;
  run.operation = index;
  if (["resume-token", "owner", "resume-terminal"].includes(reason)) run.singleOwnerProof = false;
  run.operationResults.push({ index, status: "rejected", reason, ...safeValue(details) });
  trace(run, index, "rejected", { reason, ...details });
}

function ownerGraph(owners) {
  return [...owners.values()].sort((a, b) => a.id.localeCompare(b.id)).map((owner) => ({
    id: owner.id, parent: owner.parent ?? null, phase: owner.phase,
    children: [...owner.children].sort(), views: [...owner.views].sort(),
    token: owner.token ?? null, tokenLane: owner.tokenLane ?? null,
    outcome: owner.outcome, cleanupDone: owner.cleanupDone, dropped: owner.dropped,
    drained: owner.drained, joined: owner.joined ?? false,
  }));
}

function compactChannels(channels) {
  return [...channels.values()].sort((a, b) => a.id.localeCompare(b.id)).map((channel) => ({
    id: channel.id, capacity: channel.capacity, queued: (channel.queue ?? channel.buffer ?? []).length,
    pendingOffers: (channel.offers ?? channel.tickets ?? []).filter((offer) => offer.pending || offer.state === "waiting").map((offer) => offer.id), closed: channel.closed,
  }));
}

function compactCallbacks(callbacks) {
  return [...callbacks.values()].sort((a, b) => a.lease.localeCompare(b.lease)).map((callback) => ({ lease: callback.lease, admitting: callback.admitting ?? (callback.phase === "admitting"), inFlight: callback.inFlight ?? callback.active ?? 0, drained: callback.drained ?? (callback.phase === "drained" || callback.phase === "destroyed"), destroyed: callback.destroyed ?? (callback.phase === "destroyed") }));
}

function finish(run, owners) {
  if (run.status === "accepted" && run.typedResult === "pending") run.typedResult = "open";
  run.commitHappensBefore = run.logicalTrace.filter((event) => [
    "send-commit", "receive-commit", "begin-close", "cleanup", "typed-drop", "runtime-drain", "outcome-commit",
    "callback-unregister", "callback-drain", "callback-destroy", "cancel-after-commit", "child-close",
  ].includes(event.event)).map(({ index, event, channel, sendId, owner, lease, child }) => ({
    index, event, ...(channel ? { channel } : {}), ...(sendId ? { sendId } : {}), ...(owner ? { owner } : {}), ...(lease ? { lease } : {}), ...(child ? { child } : {}),
  }));
  return {
    status: run.status,
    ...(run.reason ? { reason: run.reason, operation: run.operation } : {}),
    state: { ownerGraph: ownerGraph(owners), singleOwnerProof: run.singleOwnerProof, commitHappensBefore: run.commitHappensBefore,
      typedResult: run.typedResult, cancellation: run.cancellation, cleanup: run.cleanup,
      channels: compactChannels(run.channels), callbacks: compactCallbacks(run.callbacks) },
    logicalTrace: run.logicalTrace, physicalTrace: run.physicalTrace, operationResults: run.operationResults,
  };
}

function openOwner(owners, id, parent) {
  if (owners.has(id)) return null;
  const owner = { id, parent: parent ?? null, phase: "open", children: new Set(), views: new Set(), token: null, tokenLane: null,
    outcome: "pending", cleanupDone: false, dropped: false, drained: false, joined: false };
  owners.set(id, owner);
  if (parent && owners.has(parent)) owners.get(parent).children.add(id);
  return owner;
}

function validToken(owner, token, lane) { return owner && owner.token === token && owner.tokenLane === lane; }

function issueToken(owner, token, lane) {
  owner.token = token; owner.tokenLane = lane; owner.phase = "suspended";
}

function consumeToken(owner, token, lane) {
  if (!validToken(owner, token, lane)) return false;
  owner.token = null; owner.tokenLane = null; owner.phase = "open"; return true;
}

function frameChannelOperation(run, op, index) {
  if (["openChannel", "sendOffer", "receiveMatch", "closeChannel"].includes(op.op)) {
    if (op.op === "openChannel") {
      if (!Number.isInteger(op.capacity) || op.capacity < 0 || run.channels.has(op.channel)) { reject(run, index, "channel-capacity", { channel: op.channel }); return true; }
      run.channels.set(op.channel, { id: op.channel, capacity: op.capacity, queue: [], offers: [], closed: false });
      accept(run, index, "channel-open", { channel: op.channel, capacity: op.capacity }); return true;
    }
    const channel = run.channels.get(op.channel);
    if (!channel) { reject(run, index, "channel", { channel: op.channel }); return true; }
    if (op.op === "closeChannel") { channel.closed = true; accept(run, index, "channel-close", { channel: op.channel }); return true; }
    if (op.op === "sendOffer") {
      if (channel.closed) { reject(run, index, "channel-closed", { channel: op.channel }); return true; }
      if (channel.offers.some((offer) => offer.id === op.sendId)) { reject(run, index, "channel", { sendId: op.sendId }); return true; }
      const offer = { id: op.sendId, value: op.value ?? null, pending: true, committed: false, canceled: false };
      channel.offers.push(offer);
      if (channel.capacity > channel.queue.length) {
        offer.pending = false; offer.committed = true; channel.queue.push({ value: offer.value, sendId: offer.id });
        run.typedResult = channel.capacity === 1 ? "bounded-commit" : "send-commit";
        accept(run, index, "send-commit", { channel: op.channel, sendId: op.sendId });
      } else accept(run, index, "send-pending", { channel: op.channel, sendId: op.sendId });
      return true;
    }
    if (channel.queue.length > 0) {
      const item = channel.queue.shift();
      accept(run, index, "receive-commit", { channel: op.channel, value: item.value, sendId: item.sendId });
      const next = channel.offers.find((offer) => offer.pending && !offer.canceled);
      if (next && channel.capacity > 0) { next.pending = false; next.committed = true; channel.queue.push({ value: next.value, sendId: next.id }); }
      return true;
    }
    const pending = channel.offers.find((offer) => offer.pending && !offer.canceled);
    if (pending) {
      pending.pending = false; pending.committed = true; run.typedResult = "rendezvous-commit";
      accept(run, index, "receive-commit", { channel: op.channel, value: pending.value, sendId: pending.id }); return true;
    }
    if (channel.closed) { accept(run, index, "receive-none", { channel: op.channel }); return true; }
    reject(run, index, "channel-empty", { channel: op.channel }); return true;
  }
  return false;
}

function frameCallbackOperation(run, op, index) {
  if (!["registerCallback", "callbackEnter", "callbackReturn", "unregisterCallback", "drainCallback", "destroyLease"].includes(op.op)) return false;
  const current = run.callbacks.get(op.lease);
  if (op.op === "registerCallback") {
    if (current) { reject(run, index, "ffi-lifecycle", { lease: op.lease }); return true; }
    run.callbacks.set(op.lease, { lease: op.lease, admitting: true, inFlight: 0, drained: false, destroyed: false }); accept(run, index, "callback-register", { lease: op.lease }); return true;
  }
  if (!current) { reject(run, index, "ffi-lifecycle", { lease: op.lease }); return true; }
  if (op.op === "callbackEnter") {
    if (!current.admitting) { reject(run, index, "ffi-late", { lease: op.lease }); return true; }
    current.inFlight += 1; accept(run, index, "callback-enter", { lease: op.lease }); return true;
  }
  if (op.op === "callbackReturn") {
    if (current.inFlight === 0) { reject(run, index, "ffi-lifecycle", { lease: op.lease }); return true; }
    current.inFlight -= 1; accept(run, index, "callback-return", { lease: op.lease }); return true;
  }
  if (op.op === "unregisterCallback") { current.admitting = false; accept(run, index, "callback-unregister", { lease: op.lease }); return true; }
  if (op.op === "drainCallback") {
    if (current.admitting || current.inFlight !== 0) { reject(run, index, "ffi-drain", { lease: op.lease }); return true; }
    current.drained = true; run.typedResult = "lease-drained"; accept(run, index, "callback-drain", { lease: op.lease }); return true;
  }
  if (!current.drained) { reject(run, index, "ffi-drain", { lease: op.lease }); return true; }
  current.destroyed = true; accept(run, index, "callback-destroy", { lease: op.lease }); return true;
}

function returnedChannelOperation(run, op, index, mutation) {
  if (["openChannel", "sendOffer", "receiveMatch", "closeChannel"].includes(op.op)) {
    if (op.op === "openChannel") {
      if (!Number.isInteger(op.capacity) || op.capacity < 0 || run.channels.has(op.channel)) { reject(run, index, "channel-capacity", { channel: op.channel }); return true; }
      run.channels.set(op.channel, { id: op.channel, capacity: op.capacity, buffer: [], tickets: [], closed: false });
      accept(run, index, "channel-open", { channel: op.channel, capacity: op.capacity }); return true;
    }
    const channel = run.channels.get(op.channel);
    if (!channel) { reject(run, index, "channel", { channel: op.channel }); return true; }
    if (op.op === "closeChannel") { channel.closed = true; accept(run, index, "channel-close", { channel: op.channel }); return true; }
    if (op.op === "sendOffer") {
      if (channel.closed) { reject(run, index, "channel-closed", { channel: op.channel }); return true; }
      if (channel.tickets.some((ticket) => ticket.id === op.sendId)) { reject(run, index, "channel", { sendId: op.sendId }); return true; }
      const ticket = { id: op.sendId, value: op.value ?? null, state: "waiting" };
      channel.tickets.push(ticket);
      if (channel.capacity > channel.buffer.length && mutation !== "returned-channel") {
        ticket.state = "committed"; channel.buffer.push({ value: ticket.value, sendId: ticket.id });
        run.typedResult = channel.capacity === 1 ? "bounded-commit" : "send-commit";
        accept(run, index, "send-commit", { channel: op.channel, sendId: op.sendId });
      } else accept(run, index, "send-pending", { channel: op.channel, sendId: op.sendId });
      return true;
    }
    if (channel.buffer.length > 0) {
      const item = channel.buffer.shift();
      accept(run, index, "receive-commit", { channel: op.channel, value: item.value, sendId: item.sendId });
      const next = channel.tickets.find((candidate) => candidate.state === "waiting");
      if (next && channel.capacity > 0) { next.state = "committed"; channel.buffer.push({ value: next.value, sendId: next.id }); }
      return true;
    }
    const pending = channel.tickets.find((candidate) => candidate.state === "waiting");
    if (pending) {
      pending.state = "committed"; run.typedResult = "rendezvous-commit";
      accept(run, index, "receive-commit", { channel: op.channel, value: pending.value, sendId: pending.id }); return true;
    }
    if (channel.closed) { accept(run, index, "receive-none", { channel: op.channel }); return true; }
    reject(run, index, "channel-empty", { channel: op.channel }); return true;
  }
  return false;
}

function returnedCallbackOperation(run, op, index, mutation) {
  if (!["registerCallback", "callbackEnter", "callbackReturn", "unregisterCallback", "drainCallback", "destroyLease"].includes(op.op)) return false;
  const current = run.callbacks.get(op.lease);
  if (op.op === "registerCallback") {
    if (current) { reject(run, index, "ffi-lifecycle", { lease: op.lease }); return true; }
    run.callbacks.set(op.lease, { lease: op.lease, phase: "admitting", active: 0 }); accept(run, index, "callback-register", { lease: op.lease }); return true;
  }
  if (!current) { reject(run, index, "ffi-lifecycle", { lease: op.lease }); return true; }
  if (op.op === "callbackEnter") {
    if (current.phase !== "admitting") { reject(run, index, "ffi-late", { lease: op.lease }); return true; }
    current.active += 1; accept(run, index, "callback-enter", { lease: op.lease }); return true;
  }
  if (op.op === "callbackReturn") {
    if (current.active === 0) { reject(run, index, "ffi-lifecycle", { lease: op.lease }); return true; }
    current.active -= 1; accept(run, index, "callback-return", { lease: op.lease }); return true;
  }
  if (op.op === "unregisterCallback") { current.phase = "unregistered"; accept(run, index, "callback-unregister", { lease: op.lease }); return true; }
  if (op.op === "drainCallback") {
    if (current.phase === "admitting" || current.active !== 0) { reject(run, index, "ffi-drain", { lease: op.lease }); return true; }
    if (mutation !== "returned-callback") current.phase = "drained";
    run.typedResult = "lease-drained"; accept(run, index, "callback-drain", { lease: op.lease }); return true;
  }
  if (current.phase !== "drained") { reject(run, index, "ffi-drain", { lease: op.lease }); return true; }
  current.phase = "destroyed"; accept(run, index, "callback-destroy", { lease: op.lease }); return true;
}

/* Engine 1: a suspended frame has a program counter and explicit frame slots. */
function runFrameEngine(operations, mutation) {
  const run = createRun(LOWERINGS[0]); const frames = new Map();
  for (const [index, op] of operations.entries()) {
    if (run.status === "rejected") break;
    if (frameChannelOperation(run, op, index) || frameCallbackOperation(run, op, index)) continue;
    const frame = op.owner ? frames.get(op.owner) : null;
    if (op.op === "open") { if (!openOwner(frames, op.owner, op.parent)) reject(run, index, "owner", { owner: op.owner }); else accept(run, index, "owner-open", { owner: op.owner }); continue; }
    if (op.op === "spawn") { const parent = frames.get(op.owner); if (!parent || parent.phase !== "open" || !openOwner(frames, op.child, op.owner)) reject(run, index, "child", { owner: op.owner, child: op.child }); else accept(run, index, "child-spawn", { owner: op.owner, child: op.child }); continue; }
    if (["pullAcquire", "resumeAcquire"].includes(op.op)) {
      if (!frame) { reject(run, index, "owner", { owner: op.owner }); continue; }
      const lane = op.op === "pullAcquire" ? "pull" : "resume";
      if (frame.phase === "terminal" || frame.phase === "closed" || frame.phase === "canceled") { if (lane === "pull") accept(run, index, "stream-none", { owner: op.owner }); else reject(run, index, "resume-terminal", { owner: op.owner }); continue; }
      if (lane === "pull" && frame.views.size > 0) { reject(run, index, "borrow-next", { owner: op.owner }); continue; }
      if (frame.token !== null) { reject(run, index, "resume-token", { owner: op.owner }); continue; }
      issueToken(frame, op.token, lane); accept(run, index, lane === "pull" ? "pull-acquire" : "resume-acquire", { owner: op.owner, token: op.token }); continue;
    }
    if (["pullItem", "pullNone", "pullFailure", "resumeValue", "resumeNone", "resumeFailure"].includes(op.op)) {
      if (!frame) { reject(run, index, "owner", { owner: op.owner }); continue; }
      const lane = op.op.startsWith("pull") ? "pull" : "resume";
      if (lane === "resume" && ["terminal", "closed", "canceled"].includes(frame.phase)) { reject(run, index, "resume-terminal", { owner: op.owner }); continue; }
      if (!consumeToken(frame, op.token, lane)) { reject(run, index, "resume-token", { owner: op.owner, token: op.token }); continue; }
      if (op.op === "pullItem") { if (op.item?.ok === false && !op.item.error) { reject(run, index, "typed-failure", { owner: op.owner }); continue; } accept(run, index, op.item?.ok === false ? "result-error-item" : "pull-item", { owner: op.owner, value: op.item?.value, error: op.item?.error }); continue; }
      if (op.op === "pullNone" || op.op === "resumeNone") { frame.phase = "terminal"; frame.outcome = lane === "pull" ? "stream-none" : "resume-none"; run.typedResult = frame.outcome; accept(run, index, lane === "pull" ? "stream-none" : "resume-none", { owner: op.owner }); continue; }
      if (op.op === "pullFailure" || op.op === "resumeFailure") { if (!op.failure) { reject(run, index, "typed-failure", { owner: op.owner }); continue; } frame.phase = "terminal"; frame.outcome = `failure:${op.failure}`; run.typedResult = frame.outcome; accept(run, index, "terminal-failure", { owner: op.owner, failure: op.failure }); continue; }
      accept(run, index, "resume-value", { owner: op.owner, input: op.input, output: op.output }); continue;
    }
    if (op.op === "borrowView") { if (!frame || frame.phase !== "open") reject(run, index, "owner", { owner: op.owner }); else { frame.views.add(op.viewId); accept(run, index, "view-open", { owner: op.owner, viewId: op.viewId }); } continue; }
    if (op.op === "endView") { if (!frame || !frame.views.delete(op.viewId)) reject(run, index, "borrow-view", { owner: op.owner }); else accept(run, index, "view-close", { owner: op.owner, viewId: op.viewId }); continue; }
    if (op.op === "escapeView") { reject(run, index, "borrow-escape", { owner: op.owner, viewId: op.viewId }); continue; }
    if (op.op === "beginClose") { if (!frame || frame.phase === "closed" || frame.views.size > 0) reject(run, index, "cleanup-order", { owner: op.owner }); else { frame.phase = "closing"; accept(run, index, "begin-close", { owner: op.owner }); } continue; }
    if (op.op === "cleanup") { if (!frame || frame.phase !== "closing") reject(run, index, "cleanup-order", { owner: op.owner }); else { frame.cleanupDone = true; run.cleanup.push({ owner: op.owner, event: "cleanup" }); accept(run, index, "cleanup", { owner: op.owner }); } continue; }
    if (op.op === "drop") { if (!frame || !frame.cleanupDone || frame.dropped) reject(run, index, "cleanup-order", { owner: op.owner }); else { frame.dropped = true; accept(run, index, "typed-drop", { owner: op.owner }); } continue; }
    if (op.op === "drain") { if (!frame || !frame.dropped) reject(run, index, "cleanup-order", { owner: op.owner }); else { frame.drained = true; accept(run, index, "runtime-drain", { owner: op.owner }); } continue; }
    if (op.op === "commit") { if (!frame || frame.phase !== "closing" || !frame.cleanupDone || !frame.dropped || !frame.drained) reject(run, index, "cleanup-order", { owner: op.owner }); else { frame.phase = "closed"; if (mutation === "frame-commit") frame.outcome = "frame-mutated"; if (frame.outcome === "pending") frame.outcome = run.typedResult === "pending" ? "success" : run.typedResult; run.typedResult = frame.outcome; accept(run, index, "outcome-commit", { owner: op.owner, result: frame.outcome }); } continue; }
    if (op.op === "join") { const child = frames.get(op.child); if (!child || !child.parent || child.parent !== op.owner || child.phase !== "closed" || !child.cleanupDone || !child.dropped || !child.drained) reject(run, index, "child-drain", { child: op.child }); else { child.joined = true; frames.get(op.owner)?.children.delete(op.child); accept(run, index, "child-join", { owner: op.owner, child: op.child }); } continue; }
    if (op.op === "cancel") {
      const channel = [...run.channels.values()].find((candidate) => candidate.offers.some((offer) => offer.id === op.target));
      if (channel) { const offer = channel.offers.find((candidate) => candidate.id === op.target); if (offer.committed) { run.typedResult = "commit-wins"; run.cancellation.push({ target: op.target, outcome: "commit-wins" }); accept(run, index, "cancel-after-commit", { target: op.target }); } else if (offer.pending) { offer.pending = false; offer.canceled = true; run.typedResult = "owner-returned"; run.cancellation.push({ target: op.target, outcome: "owner-returned" }); accept(run, index, "cancel-before-commit", { target: op.target }); } else reject(run, index, "cancel", { target: op.target }); continue; }
      if (!frame || frame.phase === "closed" || frame.phase === "terminal") reject(run, index, "cancel", { target: op.target }); else { frame.phase = "canceled"; frame.outcome = "canceled"; run.cancellation.push({ target: op.target, outcome: "canceled" }); accept(run, index, "cancel", { target: op.target }); } continue;
    }
    reject(run, index, "operation", { op: op.op });
  }
  return finish(run, frames);
}

/* Engine 2: each suspension returns a state record and linear continuation token. */
function runContinuationEngine(operations, mutation) {
  const run = createRun(LOWERINGS[1]); const states = new Map();
  for (const [index, op] of operations.entries()) {
    if (run.status === "rejected") break;
    if (returnedChannelOperation(run, op, index, mutation) || returnedCallbackOperation(run, op, index, mutation)) continue;
    const state = op.owner ? states.get(op.owner) : null;
    if (op.op === "open") { if (states.has(op.owner)) reject(run, index, "owner", { owner: op.owner }); else { states.set(op.owner, { id: op.owner, parent: op.parent ?? null, state: "ready", returned: null, children: new Set(), views: new Set(), outcome: "pending", cleanupDone: false, dropped: false, drained: false, joined: false }); if (op.parent && states.has(op.parent)) states.get(op.parent).children.add(op.owner); accept(run, index, "owner-open", { owner: op.owner }); } continue; }
    if (op.op === "spawn") { const parent = states.get(op.owner); if (!parent || parent.state !== "ready" || states.has(op.child)) reject(run, index, "child", { owner: op.owner, child: op.child }); else { states.set(op.child, { id: op.child, parent: op.owner, state: "ready", returned: null, children: new Set(), views: new Set(), outcome: "pending", cleanupDone: false, dropped: false, drained: false, joined: false }); parent.children.add(op.child); accept(run, index, "child-spawn", { owner: op.owner, child: op.child }); } continue; }
    if (["pullAcquire", "resumeAcquire"].includes(op.op)) {
      if (!state) { reject(run, index, "owner", { owner: op.owner }); continue; }
      const lane = op.op === "pullAcquire" ? "pull" : "resume";
      if (["terminal", "closed", "canceled"].includes(state.state)) { if (lane === "pull") accept(run, index, "stream-none", { owner: op.owner }); else reject(run, index, "resume-terminal", { owner: op.owner }); continue; }
      if (lane === "pull" && state.views.size > 0) { reject(run, index, "borrow-next", { owner: op.owner }); continue; }
      if (state.returned) { reject(run, index, "resume-token", { owner: op.owner }); continue; }
      state.returned = { token: op.token, lane, input: null, output: null }; state.state = "waiting"; accept(run, index, lane === "pull" ? "pull-acquire" : "resume-acquire", { owner: op.owner, token: op.token }); continue;
    }
    if (["pullItem", "pullNone", "pullFailure", "resumeValue", "resumeNone", "resumeFailure"].includes(op.op)) {
      if (op.op.startsWith("resume") && state && ["terminal", "closed", "canceled"].includes(state.state)) { reject(run, index, "resume-terminal", { owner: op.owner }); continue; }
      if (!state || !state.returned || state.returned.token !== op.token || state.returned.lane !== (op.op.startsWith("pull") ? "pull" : "resume")) { reject(run, index, "resume-token", { owner: op.owner, token: op.token }); continue; }
      const lane = op.op.startsWith("pull") ? "pull" : "resume"; state.returned = null; state.state = "ready";
      if (op.op === "pullItem") { if (op.item?.ok === false && !op.item.error) reject(run, index, "typed-failure", { owner: op.owner }); else accept(run, index, op.item?.ok === false ? "result-error-item" : "pull-item", { owner: op.owner, value: op.item?.value, error: op.item?.error }); continue; }
      if (op.op === "pullNone" || op.op === "resumeNone") { state.state = "terminal"; state.outcome = lane === "pull" ? "stream-none" : "resume-none"; run.typedResult = state.outcome; accept(run, index, lane === "pull" ? "stream-none" : "resume-none", { owner: op.owner }); continue; }
      if (op.op === "pullFailure" || op.op === "resumeFailure") { if (!op.failure) { reject(run, index, "typed-failure", { owner: op.owner }); continue; } state.state = "terminal"; state.outcome = `failure:${op.failure}`; run.typedResult = state.outcome; accept(run, index, "terminal-failure", { owner: op.owner, failure: op.failure }); continue; }
      state.returned = { token: null, lane: "value", input: op.input, output: op.output }; accept(run, index, "resume-value", { owner: op.owner, input: op.input, output: op.output }); state.returned = null; continue;
    }
    if (op.op === "borrowView") { if (!state || state.state !== "ready") reject(run, index, "owner", { owner: op.owner }); else { state.views.add(op.viewId); accept(run, index, "view-open", { owner: op.owner, viewId: op.viewId }); } continue; }
    if (op.op === "endView") { if (!state || !state.views.delete(op.viewId)) reject(run, index, "borrow-view", { owner: op.owner }); else accept(run, index, "view-close", { owner: op.owner, viewId: op.viewId }); continue; }
    if (op.op === "escapeView") { reject(run, index, "borrow-escape", { owner: op.owner, viewId: op.viewId }); continue; }
    if (op.op === "beginClose") { if (!state || state.state === "closed" || state.views.size > 0) reject(run, index, "cleanup-order", { owner: op.owner }); else { state.state = "closing"; accept(run, index, "begin-close", { owner: op.owner }); } continue; }
    if (op.op === "cleanup") { if (!state || state.state !== "closing") reject(run, index, "cleanup-order", { owner: op.owner }); else { state.cleanupDone = true; run.cleanup.push({ owner: op.owner, event: "cleanup" }); accept(run, index, "cleanup", { owner: op.owner }); } continue; }
    if (op.op === "drop") { if (!state || !state.cleanupDone || state.dropped) reject(run, index, "cleanup-order", { owner: op.owner }); else { state.dropped = true; accept(run, index, "typed-drop", { owner: op.owner }); } continue; }
    if (op.op === "drain") { if (!state || !state.dropped) reject(run, index, "cleanup-order", { owner: op.owner }); else { state.drained = true; accept(run, index, "runtime-drain", { owner: op.owner }); } continue; }
    if (op.op === "commit") { if (!state || state.state !== "closing" || !state.cleanupDone || !state.dropped || !state.drained) reject(run, index, "cleanup-order", { owner: op.owner }); else { if (mutation !== "returned-commit") state.state = "closed"; if (state.outcome === "pending") state.outcome = run.typedResult === "pending" ? "success" : run.typedResult; run.typedResult = state.outcome; accept(run, index, "outcome-commit", { owner: op.owner, result: state.outcome }); } continue; }
    if (op.op === "join") { const child = states.get(op.child); if (!child || child.parent !== op.owner || child.state !== "closed" || !child.cleanupDone || !child.dropped || !child.drained) reject(run, index, "child-drain", { child: op.child }); else { child.joined = true; states.get(op.owner)?.children.delete(op.child); accept(run, index, "child-join", { owner: op.owner, child: op.child }); } continue; }
    if (op.op === "cancel") { const channel = [...run.channels.values()].find((candidate) => candidate.tickets?.some((ticket) => ticket.id === op.target)); if (channel) { const ticket = channel.tickets.find((candidate) => candidate.id === op.target); if (ticket.state === "committed") { run.typedResult = "commit-wins"; run.cancellation.push({ target: op.target, outcome: "commit-wins" }); accept(run, index, "cancel-after-commit", { target: op.target }); } else if (ticket.state === "waiting") { ticket.state = "canceled"; run.typedResult = "owner-returned"; run.cancellation.push({ target: op.target, outcome: "owner-returned" }); accept(run, index, "cancel-before-commit", { target: op.target }); } else reject(run, index, "cancel", { target: op.target }); continue; } if (!state || ["closed", "terminal"].includes(state.state)) reject(run, index, "cancel", { target: op.target }); else { state.state = "canceled"; state.outcome = "canceled"; run.cancellation.push({ target: op.target, outcome: "canceled" }); accept(run, index, "cancel", { target: op.target }); } continue; }
    reject(run, index, "operation", { op: op.op });
  }
  const projected = new Map([...states.entries()].map(([id, state]) => [id, { id, parent: state.parent, phase: ({ ready: "open", waiting: "suspended" }[state.state] ?? state.state), children: state.children, views: state.views, token: state.returned?.token ?? null, tokenLane: state.returned?.lane ?? null, outcome: state.outcome, cleanupDone: state.cleanupDone, dropped: state.dropped, drained: state.drained, joined: state.joined }]));
  return finish(run, projected);
}

export function runGen1Program(operations, { lowering = LOWERINGS[0], mutate } = {}) {
  if (!LOWERINGS.includes(lowering)) throw new Error(`unknown lowering ${lowering}`);
  return lowering === LOWERINGS[0] ? runFrameEngine(operations, mutate) : runContinuationEngine(operations, mutate);
}

export function compareGen1Lowerings(operations, options = {}) {
  const switched = runGen1Program(operations, { lowering: LOWERINGS[0], mutate: options.mutate });
  const returned = runGen1Program(operations, { lowering: LOWERINGS[1], mutate: options.mutate });
  const logicalEqual = JSON.stringify(switched.logicalTrace) === JSON.stringify(returned.logicalTrace);
  const ownerGraphEqual = JSON.stringify(switched.state.ownerGraph) === JSON.stringify(returned.state.ownerGraph);
  const commitEqual = JSON.stringify(switched.state.commitHappensBefore) === JSON.stringify(returned.state.commitHappensBefore);
  const resultEqual = switched.state.typedResult === returned.state.typedResult;
  const cancellationEqual = JSON.stringify(switched.state.cancellation) === JSON.stringify(returned.state.cancellation);
  const cleanupEqual = JSON.stringify(switched.state.cleanup) === JSON.stringify(returned.state.cleanup);
  const singleOwnerEqual = switched.state.singleOwnerProof === returned.state.singleOwnerProof;
  const physicalDifferent = JSON.stringify(switched.physicalTrace) !== JSON.stringify(returned.physicalTrace);
  return { switched, returned, equivalence: { logicalEqual, ownerGraphEqual, commitEqual, resultEqual, cancellationEqual, cleanupEqual, singleOwnerEqual, physicalDifferent,
    equivalent: logicalEqual && ownerGraphEqual && commitEqual && resultEqual && cancellationEqual && cleanupEqual && singleOwnerEqual && physicalDifferent } };
}

function stripSource(source) {
  let output = ""; let mode = "code"; let quote = "";
  for (let i = 0; i < source.length; i += 1) {
    const c = source[i]; const n = source[i + 1];
    if (mode === "line") { if (c === "\n") { output += "\n"; mode = "code"; } else output += " "; continue; }
    if (mode === "block") { if (c === "*" && n === "/") { output += "  "; i += 1; mode = "code"; } else output += c === "\n" ? "\n" : " "; continue; }
    if (mode === "string") { if (c === "\\") { output += "  "; i += 1; } else if (c === quote) { output += " "; mode = "code"; } else output += c === "\n" ? "\n" : " "; continue; }
    if (c === "/" && n === "/") { output += "  "; i += 1; mode = "line"; continue; }
    if (c === "/" && n === "*") { output += "  "; i += 1; mode = "block"; continue; }
    if (["\"", "'", "`"].includes(c)) { output += " "; quote = c; mode = "string"; continue; }
    output += c;
  }
  return output;
}

function tokens(source) {
  const stripped = stripSource(source); const result = []; const re = /[A-Za-z_][A-Za-z0-9_]*/gu; let match;
  while ((match = re.exec(stripped))) result.push({ value: match[0], start: match.index, line: stripped.slice(0, match.index).split("\n").length });
  return { stripped, tokens: result };
}

function declarationPattern(symbol) {
  return new RegExp(`\\b(?:export\\s+)?(?:async\\s+)?(?:fn|struct|enum|protocol|type)\\s+${symbol}\\b`, "gu");
}

export function extractSourceSlice(source, symbol) {
  if (!/^[A-Za-z_][A-Za-z0-9_]*$/u.test(symbol ?? "")) return { count: 0, slice: null };
  const matches = [...source.matchAll(declarationPattern(symbol))];
  if (matches.length !== 1) return { count: matches.length, slice: null };
  const start = matches[0].index;
  let open = source.indexOf("{", start);
  if (open < 0) return { count: 1, slice: source.slice(start).trim() };
  let depth = 0; let quote = null; let line = false; let block = false;
  for (let index = open; index < source.length; index += 1) {
    const current = source[index]; const next = source[index + 1];
    if (line) { if (current === "\n") line = false; continue; }
    if (block) { if (current === "*" && next === "/") { block = false; index += 1; } continue; }
    if (quote) { if (current === "\\") index += 1; else if (current === quote) quote = null; continue; }
    if (current === "/" && next === "/") { line = true; index += 1; continue; }
    if (current === "/" && next === "*") { block = true; index += 1; continue; }
    if (["\"", "'", "`"].includes(current)) { quote = current; continue; }
    if (current === "{") depth += 1;
    else if (current === "}" && --depth === 0) return { count: 1, slice: source.slice(start, index + 1).trim() };
  }
  return { count: 1, slice: null };
}

export function measureSourceSlice(source, symbol, options = {}) {
  const extracted = extractSourceSlice(source, symbol);
  if (!extracted.slice) return { ...measureSourceText("", { ...options, id: `${options.id ?? "source"}#${symbol}` }), symbol, applicable: false, occurrenceCount: extracted.count, sourceDigest: null };
  return { ...measureSourceText(extracted.slice, { ...options, id: `${options.id ?? "source"}#${symbol}` }), symbol, applicable: true, occurrenceCount: extracted.count, sourceDigest: digest(extracted.slice) };
}

export function measureSourceText(source, { id = "source", language = "w", parseEvidence = null, path: sourcePathValue = null, hiddenStatePolicy = "public" } = {}) {
  const { stripped, tokens: words } = tokens(source); const values = words.map((word) => word.value);
  const positions = (set) => words.filter((word) => set.has(word.value)).map((word) => ({ token: word.value, line: word.line, offset: word.start }));
  const declarations = [];
  for (let i = 0; i < values.length - 1; i += 1) {
    if (values[i] !== "export") continue;
    if (values[i + 1] === "async" && values[i + 2] === "fn") declarations.push(`async fn:${values[i + 3] ?? ""}`);
    else if (["protocol", "struct", "enum", "fn", "type"].includes(values[i + 1])) declarations.push(`${values[i + 1]}:${values[i + 2] ?? ""}`);
  }
  const count = (set) => positions(set).length;
  const publicConcepts = new Set([...declarations, ...values.filter((value) => ["Stream", "Channel", "Task", "State", "Session", "Failure", "Error", "Step"].includes(value))]);
  const strippedLoc = stripped.split(/\r?\n/u).filter((line) => line.trim()).length;
  return {
    variant: id, language, parseEvidence: parseEvidence?.status ?? null, sourcePath: sourcePathValue, sourceDigest: digest(source), loc: strippedLoc,
    publicConcepts: [...publicConcepts].sort(), publicConceptCount: publicConcepts.size,
    explicitOwnerHandoffs: positions(new Set(["take", "ref", "inout", "send", "receive", "spawn", "join", "rehome", "move"])),
    explicitEffectPoints: positions(new Set(["throws", "try", "await", "Failure", "Error"])),
    explicitCleanupPoints: positions(new Set(["defer", "cancel", "close", "drain", "drop", "cleanup"])),
    hiddenStateCount: hiddenStatePolicy === "compiler-owned" ? 0 : count(new Set(["yield", "continuation", "generator", "resumable"])),
    hiddenStatePolicy,
    publicDeclarations: declarations.sort(), publicDeclarationCount: declarations.length, publicTypeAbiAdditions: declarations.length,
    sourceOperations: positions(new Set(["next", "resume", "send", "receive", "await", "spawn", "join", "cancel", "close", "drain", "step"])),
    capacityFacts: positions(new Set(["capacity"])), metricBasis: "lexical structural positions after comments and strings are stripped",
  };
}

export function measureVariant(variant) {
  const file = sourcePath(variant.path); return measureSourceText(fs.readFileSync(file, "utf8"), { id: variant.id, language: variant.language, parseEvidence: variant.parseEvidence, path: variant.path, hiddenStatePolicy: variant.hiddenStatePolicy ?? "public" });
}

export function measureBundleVariants(bundle) { return (bundle.variants ?? []).map(measureVariant); }

export function validateGen1Operation(operation) {
  if (!operation || typeof operation !== "object" || Array.isArray(operation) || !operationKinds.has(operation.op)) return false;
  const forbidden = ["commit", "concurrent", "late", "pairedSend", "pairedReceive", "failureMode", "phase", "frame", "lifetime", "abi"];
  if (forbidden.some((field) => Object.prototype.hasOwnProperty.call(operation, field))) return false;
  if (["open", "pullAcquire", "pullItem", "pullNone", "pullFailure", "resumeAcquire", "resumeValue", "resumeNone", "resumeFailure", "borrowView", "endView", "escapeView", "beginClose", "cleanup", "drop", "drain", "commit", "spawn"].includes(operation.op) && typeof operation.owner !== "string") return false;
  if (["openChannel", "sendOffer", "receiveMatch", "closeChannel"].includes(operation.op) && typeof operation.channel !== "string") return false;
  if (["registerCallback", "callbackEnter", "callbackReturn", "unregisterCallback", "drainCallback", "destroyLease"].includes(operation.op) && typeof operation.lease !== "string") return false;
  if (["pullAcquire", "resumeAcquire"].includes(operation.op) && typeof operation.token !== "string") return false;
  if (["pullItem", "pullNone", "pullFailure", "resumeValue", "resumeNone", "resumeFailure"].includes(operation.op) && (typeof operation.owner !== "string" || typeof operation.token !== "string")) return false;
  return true;
}

export function validateVariantDisposition(variant) {
  const expected = {
    "stream-structured": { role: "selected", disposition: "current-composable", language: "w", path: "stream-structured.w" },
    "nominal-state-machine": { role: "alternative", disposition: "current-composable", language: "w", path: "nominal-state-machine.w" },
    "dual-bounded-channels": { role: "alternative", disposition: "current-composable", language: "w", path: "dual-bounded-channels.w" },
    "compiler-stream-block": { role: "research-candidate", disposition: "research-candidate", language: "w-reserved", path: "compiler-stream-block.txt", hiddenStatePolicy: "compiler-owned" },
    "public-resumable-frame": { role: "rejected-witness", disposition: "intentionally-rejected", language: "w-reserved", path: "public-resumable-frame.txt", hiddenStatePolicy: "public" },
  }[variant?.id];
  return Boolean(expected && variant.role === expected.role && variant.disposition === expected.disposition && variant.language === expected.language && variant.path === expected.path && /^sha256:[0-9a-f]{64}$/u.test(variant.digest ?? "") && (expected.hiddenStatePolicy === undefined || (variant.hiddenStatePolicy ?? "public") === expected.hiddenStatePolicy));
}
