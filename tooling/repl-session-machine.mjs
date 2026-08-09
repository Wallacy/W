import { createHash } from "node:crypto";

/**
 * Host-only PYN2 oracle.  It models the session protocol and facts supplied by
 * a normal W parser/checker.  It does not parse, type-check, lower, execute W,
 * resolve packages, or own resources.  The fixture is deliberately a state
 * machine: receipts are derived from operation facts, not expected booleans.
 */

export class ReplSessionError extends Error {
  constructor(code, details = {}) {
    super(code);
    this.code = code;
    this.details = details;
  }
}

const COMPLETE_PHASES = new Set(["ready", "degraded"]);
const INTERACTIVE_PHASES = new Set([
  "expression",
  "declaration",
  "statement",
  "loop",
  "call",
  "await",
  "spawn",
  "defer",
]);
const MUTATION_MODES = new Set(["take", "inout", "ref", "borrow", "view"]);
const COMMANDS = new Set(["status", "why", "history", "cancel", "reset", "quit"]);
const READ_OPERATIONS = new Set(["complete", "inspect", "status", "history", "why"]);
const OPERATION_NAMES = new Set([
  "open",
  "appendBuffer",
  "clearBuffer",
  "classify",
  "submit",
  "beginSubmission",
  "advanceSubmission",
  "finishSubmission",
  "enqueue",
  "admitNext",
  "complete",
  "inspect",
  "status",
  "history",
  "why",
  "cancel",
  "command",
  "reset",
  "restart",
  "quit",
  "verify",
]);

function clone(value) {
  return structuredClone(value);
}

function boundedPush(state, collection, value, countLimit, bytesKey = null, bytes = 0, evictedKey = null, canEvict = null) {
  state[collection].push(value);
  const actualBytes = bytesKey ? (bytes || byteLength(value)) : 0;
  if (bytesKey) state[bytesKey] = (state[bytesKey] ?? 0) + actualBytes;
  while (state[collection].length > countLimit || (bytesKey && state[bytesKey] > (state.limits?.[bytesKey] ?? Number.POSITIVE_INFINITY))) {
    const index = canEvict ? state[collection].findIndex(canEvict) : 0;
    if (index < 0) break;
    const [removed] = state[collection].splice(index, 1);
    if (bytesKey) state[bytesKey] = Math.max(0, state[bytesKey] - byteLength(removed ?? null));
    if (evictedKey) state[evictedKey] = (state[evictedKey] ?? 0) + 1;
  }
}

function executionFailure(operation) {
  const execution = operation.execution ?? {};
  if (execution.outcome === "failure" || execution.outcome === "runtime-error") return true;
  return (execution.events ?? []).some((event) => event === "failure" || event === "runtime-failure" || event?.outcome === "failure" || event?.outcome === "runtime-error");
}

function lifecycleFacts(operation) {
  const events = Array.isArray(operation.structuredEvents) ? operation.structuredEvents : [];
  if (events.length === 0) return { outcome: "ready", events: [{ kind: "owner", state: "settled" }, { kind: "join", outcome: "joined" }] };
  const forbidden = events.find((event) => event?.state === "detached" || event?.state === "escaped" || event?.state === "unfinished" || event?.outcome === "detached" || event?.outcome === "escaped" || event?.outcome === "non-join");
  if (forbidden) fail("structuredChildRejected", { event: clone(forbidden) });
  const failedCleanup = events.find((event) => event?.kind === "cleanup" && ["failure", "unknown"].includes(event.outcome));
  const joined = events.some((event) => event?.kind === "join" && event.outcome === "joined");
  const pending = events.find((event) => event?.kind === "child" && !["settled", "joined"].includes(event.state));
  const childIndex = events.findIndex((event) => event?.kind === "child");
  const cleanupIndex = events.findIndex((event) => event?.kind === "cleanup");
  const joinIndex = events.findIndex((event) => event?.kind === "join");
  if ((childIndex >= 0 && joinIndex >= 0 && joinIndex < childIndex) || (childIndex >= 0 && cleanupIndex >= 0 && cleanupIndex < childIndex)) fail("structuredChildOrderInvalid", { events: clone(events) });
  if (pending || (events.some((event) => event?.kind === "child") && !joined)) {
    fail("structuredChildRejected", { event: clone(pending ?? events.at(-1)) });
  }
  return { outcome: failedCleanup ? "failure" : "ready", events: clone(events), joined };
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") {
    return Object.fromEntries(Object.keys(value).sort().map((key) => [key, canonical(value[key])]));
  }
  return value;
}

export function sessionDigest(tag, value) {
  const bytes = typeof value === "string" ? value : JSON.stringify(canonical(value));
  return `sha256:${createHash("sha256").update(`${tag}\0${bytes}`, "utf8").digest("hex")}`;
}

function fail(code, details = {}) {
  throw new ReplSessionError(code, details);
}

function requireOpen(state, { allowClosed = false } = {}) {
  if (!state.session || state.phase === "empty") fail("sessionNotOpen");
  if (!allowClosed && (state.closed || state.phase === "closed")) fail("sessionClosed");
}

function requireReady(state) {
  requireOpen(state);
  if (!COMPLETE_PHASES.has(state.phase)) fail("sessionNotReady", { phase: state.phase });
  if (state.mutationBlocked) fail("sessionMutationBlocked");
  if (state.admission.active) fail("writerBusy", { requestId: state.admission.active.requestId });
}

function normalizeSource(source) {
  if (typeof source !== "string") fail("sourceRequired");
  return source.replace(/\r\n?/g, "\n");
}

function byteLength(value) {
  return Buffer.byteLength(typeof value === "string" ? value : JSON.stringify(value), "utf8");
}

function firstToken(source) {
  return /^\s*(\S+)/.exec(source)?.[1] ?? null;
}

function balancedSource(source) {
  const pairs = { "(": ")", "[": "]", "{": "}" };
  const closing = new Set(Object.values(pairs));
  const stack = [];
  let quote = null;
  let escaped = false;
  for (const character of source) {
    if (escaped) {
      escaped = false;
      continue;
    }
    if (quote && character === "\\") {
      escaped = true;
      continue;
    }
    if (quote) {
      if (character === quote) quote = null;
      continue;
    }
    if (character === '"' || character === "'") {
      quote = character;
      continue;
    }
    if (pairs[character]) stack.push(pairs[character]);
    else if (closing.has(character) && stack.pop() !== character) return { complete: false, invalid: true };
  }
  return { complete: stack.length === 0 && quote === null, invalid: false };
}

function parserFactsFor(source, operation = {}) {
  const facts = operation.frontendFacts?.parser ?? operation.parserFacts ?? {};
  if (facts.status === "invalid") {
    return {
      status: "invalid",
      diagnostic: {
        code: facts.code ?? "W-SESSION-0002",
        phase: "source.parse",
        reason: facts.reason ?? "parser rejected the source",
        facts: clone(facts),
      },
    };
  }
  if (facts.status === "incomplete") return { status: "incomplete", reason: facts.reason ?? "parser expects more input" };
  if (facts.status === "complete") return { status: "complete" };
  const balance = balancedSource(source);
  if (balance.invalid) {
    return {
      status: "invalid",
      diagnostic: { code: "W-SESSION-0002", phase: "source.parse", reason: "delimiter order", facts: { parser: "normal" } },
    };
  }
  if (!balance.complete || /(?:\\|\.\.\.|=>\s*)$/.test(source.trim())) {
    return { status: "incomplete", reason: "open delimiter or continuation" };
  }
  return { status: "complete" };
}

function checkerFactsFor(operation = {}) {
  const facts = operation.frontendFacts?.checker ?? operation.checkerFacts ?? {};
  const lookup = facts.lookup ?? operation.hirLookup ?? null;
  if (lookup?.availability === "invalidated" || lookup?.status === "unavailable") {
    return {
      status: "invalid",
      diagnostic: {
        code: "W-SESSION-0006",
        phase: "semantic.graph",
        reason: "invalidated binding unavailable",
        facts: clone({ ...facts, lookup }),
      },
    };
  }
  if (facts.status === "invalid" || facts.valid === false) {
    return {
      status: "invalid",
      diagnostic: {
        code: facts.code ?? "W-SESSION-0003",
        phase: "source.semantic",
        reason: facts.reason ?? "checker rejected the interactive wrapper",
        facts: clone(facts),
      },
    };
  }
  return { status: "complete", facts: clone(facts) };
}

function interactiveForm(source, operation = {}) {
  const explicit = operation.frontendFacts?.form ?? operation.form;
  if (explicit && INTERACTIVE_PHASES.has(explicit)) return explicit;
  const trimmed = source.trim();
  if (/^fn\b|^(?:let|var|type|const|import)\b/.test(trimmed)) return "declaration";
  if (/^for\b|^while\b|^loop\b/.test(trimmed)) return "loop";
  if (/^await\b/.test(trimmed)) return "await";
  if (/^spawn\b/.test(trimmed)) return "spawn";
  if (/^defer\b/.test(trimmed)) return "defer";
  if (/^call\b|\w+\s*\([^)]*\)\s*;?$/.test(trimmed)) return "call";
  if (/^(?:if|match|return|break|continue)\b/.test(trimmed)) return "statement";
  return "expression";
}

/** Classify facts from the normal parser/checker wrapper.  Module top-level
 * legality is intentionally not consulted: interactive statements are legal
 * in the wrapper even when the same text is not a .w module item. */
function classifySource(source, operation = {}) {
  const normalized = normalizeSource(source);
  const trimmed = normalized.trim();
  if (trimmed.length === 0) return { kind: "incomplete", reason: "empty-buffer", form: null, parseDiagnostic: null, semanticDiagnostic: null };
  if (trimmed.startsWith(":")) {
    const command = /^:([a-z]+)(?:\s+(.+))?$/i.exec(trimmed);
    if (command && COMMANDS.has(command[1].toLowerCase())) {
      return { kind: "command", command: command[1].toLowerCase(), argument: command[2] ?? null, parseDiagnostic: null, semanticDiagnostic: null };
    }
    return {
      kind: "invalid",
      reason: "unknown-context-command",
        code: "W-SESSION-0001",
      form: "command",
      parseDiagnostic: null,
      semanticDiagnostic: { code: "W-SESSION-0001", phase: "context.command", reason: "unknown contextual command", facts: { firstToken: firstToken(trimmed) } },
    };
  }
  const parser = parserFactsFor(normalized, operation);
  if (parser.status === "incomplete") return { kind: "incomplete", reason: parser.reason, form: interactiveForm(normalized, operation), parseDiagnostic: null, semanticDiagnostic: null };
  if (parser.status === "invalid") return { kind: "invalid", reason: parser.diagnostic.reason, code: parser.diagnostic.code, form: interactiveForm(normalized, operation), parseDiagnostic: parser.diagnostic, semanticDiagnostic: null };
  const checker = checkerFactsFor(operation);
  if (checker.status === "invalid") return { kind: "invalid", reason: checker.diagnostic.reason, code: checker.diagnostic.code, form: interactiveForm(normalized, operation), parseDiagnostic: null, semanticDiagnostic: checker.diagnostic };
  const form = interactiveForm(normalized, operation);
  return {
    kind: "complete",
    reason: "normal-wrapper-accepted",
    form,
    display: operation.tailExpression === false || /;\s*$/.test(trimmed) ? "discard" : "tail",
    moduleTopLevel: operation.moduleTopLevel ?? "not-evaluated",
    parseDiagnostic: null,
    semanticDiagnostic: null,
    facts: checker.facts,
  };
}

function generationRef(generation) {
  if (!generation) return null;
  return { id: generation.id, incarnation: generation.incarnation, display: generation.display, number: generation.number };
}

/** Display gN is a projection.  Only the opaque id plus incarnation matches. */
function generationMatches(generation, reference) {
  if (reference === undefined || reference === null) return true;
  if (!reference || typeof reference !== "object" || typeof reference.id !== "string" || !Number.isInteger(reference.incarnation)) return false;
  return generation.id === reference.id && generation.incarnation === reference.incarnation;
}

function requestedGeneration(state, reference) {
  if (reference === undefined || reference === null) return state.generation;
  return [...state.generationHistory].reverse().find((generation) => generationMatches(generation, reference)) ?? null;
}

function currentBinding(state, name) {
  return (state.generation?.bindings ?? [])
    .filter((binding) => binding.name === name && binding.availability === "available")
    .sort((left, right) => right.version - left.version)[0] ?? null;
}

function allBindingVersions(state, name) {
  return (state.generation?.bindings ?? []).filter((binding) => binding.name === name);
}

function parseNumeric(value) {
  if (typeof value === "number" && Number.isFinite(value)) return value;
  if (typeof value === "string" && /^[-+]?\d+(?:\.\d+)?$/.test(value.trim())) return Number(value);
  return null;
}

function dependencyNamesFromExpression(expression) {
  const names = String(expression ?? "").match(/\b[A-Za-z_]\w*\b/g) ?? [];
  return [...new Set(names.filter((name) => !["fn", "let", "var", "type", "const", "return"].includes(name)))];
}

function evaluateSnapshot(declaration, state) {
  if (declaration.value !== undefined) return clone(declaration.value);
  const expression = declaration.expression ?? "";
  const match = /^\s*([A-Za-z_]\w*)\s*\*\s*(-?\d+(?:\.\d+)?)\s*$/.exec(expression);
  if (match) {
    const value = parseNumeric(currentBinding(state, match[1])?.value);
    if (value !== null) return value * Number(match[2]);
  }
  const identifier = /^\s*([A-Za-z_]\w*)\s*$/.exec(expression);
  if (identifier) return clone(currentBinding(state, identifier[1])?.value ?? null);
  return declaration.valueDigest ?? sessionDigest("w-repl-snapshot-value", expression);
}

function bindingKind(declaration) {
  if (declaration.kind && ["snapshot", "compiled", "type"].includes(declaration.kind)) return declaration.kind;
  if (declaration.kind) fail("bindingKindInvalid", { kind: declaration.kind });
  return "snapshot";
}

function bindingVersion(state, name) {
  return Math.max(0, ...allBindingVersions(state, name).map((binding) => binding.version));
}

function normalizeDependency(dependency, state, declaration) {
  const source = typeof dependency === "string" ? { name: dependency } : dependency;
  if (!source || typeof source.name !== "string") fail("bindingDependencyInvalid", { binding: declaration.name });
  const target = source.bindingId ? (state.generation.bindings ?? []).find((binding) => binding.bindingId === source.bindingId && binding.availability === "available") : currentBinding(state, source.name);
  if (!target) fail(source.kind === "importSymbol" ? "importDependencyMissing" : "bindingDependencyMissing", { name: source.name, binding: declaration.name });
  if (source.version !== undefined && source.version !== target.version) fail("bindingDependencyVersionMismatch", { name: source.name, expected: source.version, actual: target.version });
  return {
    kind: source.kind ?? (bindingKind(declaration) === "compiled" ? "compiledLookup" : "softProvenance"),
    bindingId: target.bindingId,
    name: target.name,
    version: target.version,
    generationId: target.generationId,
    incarnation: target.incarnation,
  };
}

function dependencyEdges(state, declaration) {
  const explicit = declaration.frontendFacts?.hardDependencies ?? declaration.hardDependencies;
  const raw = Array.isArray(explicit) ? explicit : [];
  return raw.map((dependency) => normalizeDependency(dependency, state, declaration));
}

function softProvenance(state, declaration) {
  if (declaration.softProvenance) {
    return declaration.softProvenance.map((dependency) => normalizeDependency({ ...(typeof dependency === "string" ? { name: dependency } : dependency), kind: "softProvenance" }, state, declaration));
  }
  if (bindingKind(declaration) !== "snapshot") return [];
  return dependencyNamesFromExpression(declaration.expression).map((name) => normalizeDependency({ name, kind: "softProvenance" }, state, declaration));
}

function buildBinding(state, declaration, candidate) {
  if (!declaration || typeof declaration !== "object" || typeof declaration.name !== "string") fail("bindingDeclarationInvalid");
  if (!/^[A-Za-z_]\w*$/.test(declaration.name)) fail("bindingNameInvalid");
  const kind = bindingKind(declaration);
  const hardDependencies = dependencyEdges(state, declaration);
  const provenance = softProvenance(state, declaration);
  const value = kind === "snapshot" && !declaration.persistentResource && !declaration.persistentTask ? evaluateSnapshot(declaration, state) : declaration.value ?? null;
  const type = declaration.type ?? (typeof value === "number" ? "i32" : "opaque");
  const version = bindingVersion(state, declaration.name) + 1;
  const bindingId = `b:${state.session.incarnation}:${sessionDigest("w-repl-binding", { name: declaration.name, version, candidate: candidate.id }).slice(7, 23)}`;
  const persistent = declaration.persistentResource === true || declaration.persistentTask === true;
  const owner = declaration.owner ?? (persistent ? "generation" : "submission");
  const typeFacts = {
    copyable: declaration.copyable ?? (type === "i32" || type === "bool" || type === "f64" || type === "string"),
    moveOnly: declaration.moveOnly === true,
    snapshotSafe: declaration.snapshotSafe ?? (declaration.copyable ?? (type === "i32" || type === "bool" || type === "f64" || type === "string")),
  };
  return {
    bindingId,
    name: declaration.name,
    version,
    generationId: candidate.id,
    generation: candidate.display,
    createdGenerationId: candidate.id,
    createdGeneration: candidate.display,
    createdIncarnation: candidate.incarnation,
    incarnation: candidate.incarnation,
    availability: "available",
    kind,
    type,
    typeFacts,
    hirFingerprint: sessionDigest("w-repl-hir", { source: declaration.source ?? declaration.body ?? declaration.expression ?? null, type, kind }),
    layoutFingerprint: sessionDigest("w-repl-layout", { type, shape: declaration.layout ?? null }),
    effectFingerprint: sessionDigest("w-repl-effect", declaration.effects ?? []),
    hardDependencies,
    softProvenance: provenance,
    value,
    valueDigest: sessionDigest("w-repl-value", value),
    owner,
    scope: persistent ? "generation-owner" : "submission",
    ownerScopeId: null,
    resource: declaration.resource ?? null,
    invalidation: null,
  };
}

function makeGeneration(state, number, ordinal, bindings = [], status = "committed") {
  const id = `gen:${sessionDigest("w-repl-generation", {
    sessionId: state.session.sessionId,
    incarnation: state.session.incarnation,
    number,
    ordinal,
    sequence: state.generationSequence,
    bindings: bindings.map((binding) => binding.bindingId),
  }).slice(7)}`;
  const display = `g${number}`;
  return {
    id,
    incarnation: state.session.incarnation,
    display,
    number,
    status,
    committed: status === "committed" || status === "degraded",
    bindings,
    graphFingerprint: sessionDigest("w-repl-graph", bindings.map((binding) => ({
      bindingId: binding.bindingId,
      name: binding.name,
      version: binding.version,
      availability: binding.availability,
      hardDependencies: binding.hardDependencies,
      softProvenance: binding.softProvenance,
      hirFingerprint: binding.hirFingerprint,
      layoutFingerprint: binding.layoutFingerprint,
      effectFingerprint: binding.effectFingerprint,
    }))),
    scope: {
      scopeId: `graph:${state.session.incarnation}:${number}:${state.generationSequence}`,
      owner: "symbol-graph",
      children: [],
      waits: [],
      resources: [],
      state: "ready",
      parent: "session",
    },
    createdBy: ordinal,
  };
}

function generationHistoryRecord(generation) {
  const record = clone(generation);
  record.bindings = record.bindings.map((binding) => {
    const metadata = { ...binding };
    delete metadata.value;
    delete metadata.liveValue;
    return metadata;
  });
  record.scope = { ...record.scope, resources: record.scope.resources.map((resource) => ({ ...resource, liveValue: undefined })) };
  return record;
}

function trace(state, event, details = {}) {
  const record = { index: state.trace.length, event, details: clone(details), ordinal: state.session?.executionOrdinal ?? 0, generation: generationRef(state.generation), incarnation: state.session?.incarnation ?? null };
  state.trace.push(record);
  state.traceBytes += byteLength(record);
  while (state.trace.length > state.limits.traceCount || state.traceBytes > state.limits.traceBytes) {
    const removed = state.trace.shift();
    state.traceBytes = Math.max(0, state.traceBytes - byteLength(removed ?? null));
    state.traceEvicted += 1;
  }
}

function phaseTrace(state, phase, details = {}) {
  state.transactionTrace.push({ phase, details: clone(details), generation: generationRef(state.generation), incarnation: state.session?.incarnation ?? null });
}

function normalizeLimits(limits = {}) {
  return {
    sourceBytes: limits.sourceBytes ?? 64 * 1024,
    bindings: limits.bindings ?? 1024,
    edges: limits.edges ?? 4096,
    artifacts: limits.artifacts ?? 4096,
    historyCount: limits.historyCount ?? 128,
    historyBytes: limits.historyBytes ?? 1024 * 1024,
    tasks: limits.tasks ?? 256,
    resources: limits.resources ?? 256,
    outputBytes: limits.outputBytes ?? 1024 * 1024,
    outputCount: limits.outputCount ?? 256,
    diagnosticBytes: limits.diagnosticBytes ?? 256 * 1024,
    invalidationClosure: limits.invalidationClosure ?? 4096,
    drainTimeMs: limits.drainTimeMs ?? 5000,
    effectCount: limits.effectCount ?? 1024,
    effectBytes: limits.effectBytes ?? 256 * 1024,
    invalidationCount: limits.invalidationCount ?? 4096,
    invalidationBytes: limits.invalidationBytes ?? 512 * 1024,
    ticketCount: limits.ticketCount ?? 1024,
    ticketBytes: limits.ticketBytes ?? 256 * 1024,
    cancellationCount: limits.cancellationCount ?? 1024,
    cancellationBytes: limits.cancellationBytes ?? 256 * 1024,
    traceCount: limits.traceCount ?? 2048,
    traceBytes: limits.traceBytes ?? 512 * 1024,
    receiptCount: limits.receiptCount ?? 128,
  };
}

function diagnostic(code, reason, phase, facts = {}) {
  return { code, severity: "error", phase, reason, facts: clone(facts) };
}

function baseReceipt(state, requestId, ordinal, base, outcome, source) {
  return {
    requestId,
    sessionId: state.session.sessionId,
    incarnation: state.session.incarnation,
    incarnationBase: state.session.incarnation,
    incarnationFinal: state.session.incarnation,
    ordinal,
    prompt: ordinal === null ? null : `w[${ordinal}]`,
    sourceDigest: sessionDigest("w-repl-source-v1", normalizeSource(source)),
    generationBase: generationRef(base),
    generationFinal: generationRef(state.generation),
    outcome,
    effects: [],
    diagnostics: [],
    invalidation: [],
    cleanup: { stagedScope: "none", oldGeneration: null, drain: "not-required", forceBoundary: false },
    phases: [],
  };
}

function historyProjection(state, receipt, source) {
  const rawSource = state.historyPolicy.rawSource === "redact" ? null : normalizeSource(source);
  const record = {
    rawSource,
    sourceRedacted: rawSource === null,
    normalizedDigest: sessionDigest("w-repl-source-v1", normalizeSource(source)),
    status: receipt.outcome,
    ordinal: receipt.ordinal,
    requestId: receipt.requestId,
    sessionId: receipt.sessionId,
    incarnation: receipt.incarnation,
    generationBase: clone(receipt.generationBase),
    generationFinal: clone(receipt.generationFinal),
    effects: clone(receipt.effects),
    diagnostics: clone(receipt.diagnostics),
    invalidation: clone(receipt.invalidation),
    cleanup: clone(receipt.cleanup),
    degraded: receipt.outcome === "degraded",
  };
  record.bytes = byteLength(record);
  return record;
}

function reserveHistory(state, source, receipt) {
  if (state.limits.historyCount < 1) fail("historyQuota", { family: "historyCount", required: 1, limit: state.limits.historyCount });
  const projected = historyProjection(state, receipt, source);
  if (projected.bytes > state.limits.historyBytes) fail("historyQuota", { family: "historyBytes", required: projected.bytes, limit: state.limits.historyBytes, reserved: true });
  while (state.history.records.length >= state.limits.historyCount || state.history.bytes + projected.bytes > state.limits.historyBytes) {
    const removed = state.history.records.shift();
    if (!removed) break;
    state.history.bytes -= removed.bytes;
    state.history.evicted += 1;
  }
  state.history.reserved += projected.bytes;
  return projected;
}

function appendHistory(state, receipt, source, reserved) {
  const record = reserved ?? historyProjection(state, receipt, source);
  state.history.records.push(record);
  state.history.bytes += record.bytes;
  state.history.reserved = Math.max(0, state.history.reserved - record.bytes);
  receipt.historyStored = true;
}

function ensureDiagnosticQuota(state, receipt) {
  const bytes = byteLength(receipt.diagnostics);
  receipt.diagnosticBytes = bytes;
  if (bytes > state.limits.diagnosticBytes) fail("diagnosticQuota", { family: "diagnosticBytes", required: bytes, limit: state.limits.diagnosticBytes });
}

function incrementOrdinal(state) {
  state.session.executionOrdinal += 1;
  state.session.prompt = `w[${state.session.executionOrdinal}]`;
  return state.session.executionOrdinal;
}

function finalizeReceipt(state, receipt, source, reserved) {
  ensureDiagnosticQuota(state, receipt);
  receipt.generationFinal = generationRef(state.generation);
  receipt.phases = clone(state.transactionTrace);
  state.buffer = { text: "", classification: null, complete: false };
  state.receipts.push(clone(receipt));
  while (state.receipts.length > state.limits.receiptCount) state.receipts.shift();
  state.lastReceipt = clone(receipt);
  appendHistory(state, receipt, source, reserved);
  state.transactionTrace = [];
  state.lastResult = { kind: "receipt", receipt: clone(receipt) };
  return receipt;
}

function recordEffects(state, effects, receipt, failedExecution = false) {
  const observed = [];
  for (const effect of effects ?? []) {
    if (!effect || typeof effect !== "object") fail("effectInvalid");
    const transaction = effect.transaction ?? null;
    const events = Array.isArray(transaction?.events) ? transaction.events : [];
    const capability = transaction?.capability === "transaction";
    if (effect.rollbackClaim === "rolledBack" && !capability) fail("falseRollbackClaim", { provider: effect.provider ?? "external" });
    let durableOutcome = "observed";
    if (events.some((event) => event === "rolledBack" || event?.outcome === "rolledBack")) {
      if (!capability) fail("falseRollbackClaim", { provider: effect.provider ?? "external" });
      durableOutcome = "rolledBack";
    } else if (events.some((event) => event === "unknown" || event?.outcome === "unknown")) durableOutcome = "unknown";
    else if (events.some((event) => event === "committed" || event?.outcome === "committed")) durableOutcome = "committed";
    else if (failedExecution) durableOutcome = capability ? "unknown" : "observed";
    const item = {
      effectId: effect.effectId ?? `effect:${sessionDigest("w-repl-effect-id", { ordinal: receipt.ordinal, effect }).slice(7, 23)}`,
      provider: effect.provider ?? "external",
      kind: effect.kind ?? "observed",
      invocation: "observed",
      observed: true,
      transactionCapability: capability,
      providerEvents: clone(events),
      durableOutcome,
      disposition: durableOutcome === "rolledBack" ? "rolled-back" : "observed",
      payloadDigest: sessionDigest("w-repl-effect-payload", effect.payload ?? effect),
    };
    observed.push(item);
    boundedPush(state, "effects", item, state.limits.effectCount, "effectsBytes", byteLength(item), "effectsEvicted");
  }
  receipt.effects = observed;
}

function validateEffectFacts(effects) {
  for (const effect of effects ?? []) {
    if (!effect || typeof effect !== "object") fail("effectInvalid");
    const transaction = effect.transaction ?? null;
    const capability = transaction?.capability === "transaction";
    if (effect.rollbackClaim === "rolledBack" && !capability) fail("falseRollbackClaim", { provider: effect.provider ?? "external" });
  }
}

function rejectLegacyFacts(operation) {
  const forbidden = ["runtimeError", "adapterTransaction", "deferredNoFail", "snapshot", "compiled", "function", "postPublishFailure"];
  const present = forbidden.filter((key) => Object.hasOwn(operation, key));
  if (present.length) fail("legacyFactUnsupported", { fields: present });
  for (const mutation of operation.mutations ?? []) {
    const mutationFields = ["adapterTransaction", "deferredNoFail", "snapshot"].filter((key) => Object.hasOwn(mutation, key));
    if (mutationFields.length) fail("legacyFactUnsupported", { fields: mutationFields });
  }
  for (const declaration of operation.declarations ?? []) {
    const declarationFields = ["compiled", "function"].filter((key) => Object.hasOwn(declaration, key));
    if (declarationFields.length) fail("legacyFactUnsupported", { fields: declarationFields });
  }
  for (const effect of operation.effects ?? []) {
    if (Object.hasOwn(effect, "adapterTransaction") || Object.hasOwn(effect, "providerTransaction")) fail("legacyFactUnsupported", { fields: ["transaction"] });
  }
  if (Object.hasOwn(operation, "postPublishFailure") || Object.hasOwn(operation.drain ?? {}, "postPublishFailure")) fail("legacyFactUnsupported", { fields: ["postPublishFailure"] });
}

function edgeDependsOn(edge, target) {
  return edge.bindingId === target.bindingId && edge.version === target.version && edge.incarnation === target.incarnation;
}

function markOldBindings(state, changedNames, invalidation, candidate) {
  const active = state.generation.bindings.filter((binding) => binding.availability === "available");
  const changed = active.filter((binding) => changedNames.includes(binding.name));
  const invalidated = new Set();
  const queue = [...changed];
  const predecessor = new Map();
  while (queue.length) {
    const target = queue.shift();
    for (const binding of active) {
      if (binding.availability !== "available" || invalidated.has(binding.bindingId) || changed.includes(binding)) continue;
      const hard = binding.hardDependencies.find((edge) => edgeDependsOn(edge, target));
      if (hard) {
        invalidated.add(binding.bindingId);
        predecessor.set(binding.bindingId, { dependency: target.bindingId, edge: clone(hard) });
        queue.push(binding);
      }
    }
  }
  const closure = [...invalidated];
  if (closure.length > state.limits.invalidationClosure) fail("invalidationQuota", { family: "invalidationClosure", count: closure.length, limit: state.limits.invalidationClosure });
  const changedIds = new Set(changed.map((binding) => binding.bindingId));
  const changedNamesSet = new Set(changedNames);
  for (const binding of state.generation.bindings) {
    if (binding.availability !== "available") continue;
    if (changedIds.has(binding.bindingId)) binding.availability = "superseded";
    else if (invalidated.has(binding.bindingId)) {
      const edge = predecessor.get(binding.bindingId)?.edge ?? null;
      const dependency = predecessor.get(binding.bindingId)?.dependency ?? null;
      binding.availability = "invalidated";
      binding.invalidation = {
        reason: "redefined-hard-dependency",
        reasonCode: edge?.kind ?? "compiledLookup",
        replacedBy: candidate.display,
        dependencyBindingId: edge?.bindingId ?? dependency,
        dependencyVersion: edge?.version ?? null,
        closure: closure.map((bindingId) => bindingId),
      };
      invalidation.push({
        bindingId: binding.bindingId,
        name: binding.name,
        reason: binding.invalidation.reason,
        kind: binding.invalidation.reasonCode,
        dependencyBindingId: binding.invalidation.dependencyBindingId,
        dependencyVersion: binding.invalidation.dependencyVersion,
        dependencyName: edge?.name ?? null,
        closure: clone(closure),
      });
    }
  }
  return { changed, invalidated: [...invalidated], closure, changedNames: [...changedNamesSet], predecessor };
}

function mutationFacts(state, mutation, publishesNext = false) {
  if (!mutation || typeof mutation.name !== "string" || !MUTATION_MODES.has(mutation.mode)) fail("mutationInvalid");
  const binding = currentBinding(state, mutation.name);
  if (!binding) fail("bindingUnavailable", { name: mutation.name });
  const target = mutation.generationRef ?? mutation.generation;
  if (target !== undefined && (!target || typeof target !== "object" || typeof target.id !== "string" || !Number.isInteger(target.incarnation))) {
    fail("generationIdentityRequired", { name: mutation.name, target: clone(target) });
  }
  const stateMutation = mutation.operation !== "snapshot" && !(mutation.mode === "borrow" && mutation.escape === false) && !(mutation.mode === "ref" && mutation.escape === false);
  const crossesGeneration = target !== undefined ? !generationMatches(state.generation, target) : binding.generationId !== state.generation.id || (publishesNext && stateMutation);
  const typeFacts = { ...binding.typeFacts, ...(mutation.typeFacts ?? {}) };
  const lexicalBorrow = (mutation.mode === "borrow" || mutation.mode === "ref") && mutation.escape === false;
  const transactionProof = mutation.adapter?.capability === "transaction";
  const deferredProof = mutation.deferred?.proof === "no-fail-transfer";
  const snapshotProof = mutation.operation === "snapshot";
  const copyStage = crossesGeneration && typeFacts.copyable === true && ["assign", "add", "update", undefined].includes(mutation.operation) && mutation.mode !== "take";
  const permittedByFacts = !crossesGeneration || lexicalBorrow || copyStage || snapshotProof || transactionProof || deferredProof;
  if (crossesGeneration && !permittedByFacts) {
    const code = mutation.mode === "take" ? "W-SESSION-0007" : mutation.mode === "inout" ? "W-SESSION-0008" : mutation.mode === "ref" ? "W-SESSION-0009" : mutation.mode === "borrow" ? "W-SESSION-0010" : "W-SESSION-0011";
    fail("crossGenerationMutation", { name: mutation.name, mode: mutation.mode, generation: binding.generationId, typeFacts, suggestions: ["copy", "snapshot", "adapter transaction", "deferred no-fail"] , code });
  }
  return { name: mutation.name, mode: mutation.mode, operation: mutation.operation ?? null, crossesGeneration, copyStage, snapshotProof, transactionProof, deferredProof, lexicalBorrow, typeFacts, bindingId: binding.bindingId, stateMutation };
}

function checkCrossGenerationMutations(state, mutations, operation = {}) {
  const publishesNext = (operation.declarations?.length ?? 0) > 0 || (mutations ?? []).some((mutation) => mutation.operation !== "snapshot" && !(mutation.mode === "borrow" && mutation.escape === false) && !(mutation.mode === "ref" && mutation.escape === false));
  return (mutations ?? []).map((mutation) => mutationFacts(state, mutation, publishesNext));
}

function validateOpenContext(operation) {
  const forbidden = ["scanCwd", "scanPath", "importEnvironment", "importEnv", "resolveDependency", "networkResolve", "pathDiscovery", "cwdDiscovery"];
  if (forbidden.some((key) => operation[key] === true)) fail("contextDiscoveryForbidden", { forbidden: forbidden.filter((key) => operation[key] === true) });
  if (operation.context && typeof operation.context !== "object") fail("contextInvalid");
  if (operation.context?.mode === "external" && operation.context.explicit !== true) fail("contextExplicitRequired");
  if (operation.context?.resolver === "dynamic" || operation.context?.network === true) fail("contextResolverForbidden");
}

function openSession(state, operation) {
  if (state.phase !== "empty") fail("sessionAlreadyOpen");
  validateOpenContext(operation);
  const sessionId = operation.sessionId ?? `session:${sessionDigest("w-repl-session", { index: state.trace.length }).slice(7, 23)}`;
  if (typeof sessionId !== "string" || sessionId.trim() === "") fail("sessionIdInvalid");
  state.session = {
    sessionId,
    incarnation: 1,
    executionOrdinal: 0,
    frontend: { parser: "normal", checker: "normal", hir: "normal" },
    prompt: "w[0]",
    lockDigest: operation.lockDigest ?? null,
    capabilityDigest: sessionDigest("w-repl-capabilities", operation.capabilities ?? ["stdio"]),
    contextDigest: sessionDigest("w-repl-context", operation.context ?? { mode: "ephemeral-std-only" }),
    context: {
      mode: operation.context?.mode ?? "ephemeral-std-only",
      external: operation.context?.explicit === true,
      resolver: "fixed-at-open",
      network: false,
      cwdScan: false,
      pathScan: false,
      environmentScan: false,
      lockFixed: true,
      capabilitiesFixed: true,
    },
  };
  state.limits = normalizeLimits(operation.limits);
  state.admission.tickets = state.admissionTickets;
  state.historyPolicy = { rawSource: operation.historyPolicy?.rawSource === "redact" ? "redact" : "memory" };
  state.generationSequence = 0;
  state.generation = makeGeneration(state, 0, 0, []);
  state.generationHistory.push(generationHistoryRecord(state.generation));
  state.phase = "ready";
  state.open = { context: clone(state.session.context), lockDigest: state.session.lockDigest, capabilityDigest: state.session.capabilityDigest, historyPolicy: clone(state.historyPolicy) };
  trace(state, "open", { sessionId, context: state.session.context, generation: generationRef(state.generation), generationDisplay: "g0" });
}

function classify(state, operation) {
  requireOpen(state);
  const source = normalizeSource(operation.source ?? state.buffer.text);
  if (byteLength(source) > state.limits.sourceBytes) fail("sourceQuota", { family: "sourceBytes" });
  const empty = state.buffer.text.trim().length === 0;
  if (operation.bufferEmpty !== undefined && operation.bufferEmpty !== empty) fail("contextCommandPosition");
  const classification = classifySource(source, operation);
  if (classification.kind === "command" && !empty) fail("contextCommandPosition");
  state.buffer = { text: source, classification: clone(classification), complete: classification.kind !== "incomplete" };
  state.lastClassification = clone(classification);
  state.lastResult = { kind: "classification", classification: clone(classification) };
  trace(state, "classify", { classification, parser: "normal", checker: "normal", hir: "normal", firstToken: firstToken(source) });
}

function appendBuffer(state, operation) {
  requireOpen(state);
  const addition = normalizeSource(operation.source ?? operation.text ?? "");
  const text = `${state.buffer.text}${addition}`;
  if (byteLength(text) > state.limits.sourceBytes) fail("sourceQuota", { family: "sourceBytes" });
  state.buffer = { text, classification: null, complete: false };
  state.lastResult = { kind: "buffer", action: "append", bytes: byteLength(text) };
  trace(state, "append-buffer", { bytes: byteLength(text) });
}

function clearBuffer(state) {
  requireOpen(state);
  state.buffer = { text: "", classification: null, complete: false };
  state.lastResult = { kind: "buffer", action: "clear" };
  trace(state, "clear-buffer", {});
}

function ensureRequestContext(state, operation) {
  if (operation.lockDigest !== undefined && operation.lockDigest !== state.session.lockDigest) fail("sessionLockFixed");
  if (operation.capabilities !== undefined && sessionDigest("w-repl-capabilities", operation.capabilities) !== state.session.capabilityDigest) fail("sessionCapabilitiesFixed");
  if (["resolveDependency", "networkResolve", "scanCwd", "scanPath", "importEnvironment", "ambientImport", "cwdImport", "pathImport"].some((key) => operation[key] === true)) fail("sessionContextForbidden");
}

function rejectSemanticSubmission(state, operation, source, base, error, existingReceipt = null, existingReservation = null) {
  if (!(error instanceof ReplSessionError)) throw error;
  const receipt = existingReceipt ?? baseReceipt(state, operation.requestId ?? `request:${state.trace.length + 1}`, incrementOrdinal(state), base, "rejected", source);
  receipt.outcome = "rejected";
  const ownershipCode = error.details?.mode === "take" ? "W-SESSION-0007" : error.details?.mode === "inout" ? "W-SESSION-0008" : error.details?.mode === "ref" ? "W-SESSION-0009" : error.details?.mode === "borrow" ? "W-SESSION-0010" : "W-SESSION-0011";
  const code = error.details?.code ?? (error.code === "falseRollbackClaim" ? "W-SESSION-0018" : error.code === "crossGenerationMutation" ? ownershipCode : error.code === "bindingUnavailable" || error.code === "bindingDependencyMissing" || error.code === "bindingDependencyVersionMismatch" || error.code === "importDependencyMissing" ? "W-SESSION-0006" : error.code === "historyQuota" ? "W-SESSION-0015" : error.code === "outputQuota" ? "W-SESSION-0016" : error.code === "bindingQuota" ? "W-SESSION-0019" : error.code === "edgeQuota" ? "W-SESSION-0020" : error.code === "artifactQuota" ? "W-SESSION-0021" : error.code === "scopeQuota" ? "W-SESSION-0022" : error.code === "invalidationQuota" ? "W-SESSION-0023" : error.code === "sourceQuota" ? "W-SESSION-0025" : error.code === "structuredChildRejected" || error.code === "structuredChildOrderInvalid" ? "W-SESSION-0017" : error.code === "drainConclusionFactRequired" ? "W-SESSION-0026" : error.code === "effectQuota" ? "W-SESSION-0027" : error.code === "ticketQuota" ? "W-SESSION-0028" : error.code === "generationIdentityRequired" ? "W-SESSION-0005" : "W-SESSION-0003");
  const phase = error.code === "drainConclusionFactRequired" ? "session.preflight" : error.code === "generationIdentityRequired" ? "session.admission" : "source.semantic";
  receipt.diagnostics = [diagnostic(code, error.code, phase, error.details)];
  if (error.code === "crossGenerationMutation") receipt.diagnostics[0].reason = "cross-generation ownership mutation rejected; use copy, snapshot, adapter, or deferred no-fail";
  const reservation = existingReservation ?? reserveHistory(state, source, receipt);
  finalizeReceipt(state, receipt, source, reservation);
  return receipt;
}

function changedNamesFor(operation, mutationFacts = []) {
  const declarations = operation.declarations ?? [];
  return [...new Set([
    ...declarations.map((declaration) => declaration.name),
    ...mutationFacts.filter((mutation) => mutation.stateMutation && mutation.copyStage).map((mutation) => mutation.name),
  ])];
}

function mutationDeclaration(state, mutation, facts, candidate) {
  const current = currentBinding(state, mutation.name);
  if (!current || !facts.copyStage) return null;
  let value = mutation.value;
  if (value === undefined && mutation.operation === "add") value = (parseNumeric(current.value) ?? 0) + (mutation.delta ?? 1);
  if (value === undefined && mutation.operation === "assign") value = mutation.assignedValue;
  if (value === undefined && mutation.operation === "update") value = mutation.updatedValue ?? current.value;
  return {
    name: mutation.name,
    kind: current.kind,
    type: current.type,
    value,
    expression: mutation.expression,
    copyable: current.typeFacts.copyable,
    moveOnly: current.typeFacts.moveOnly,
    hardDependencies: [],
    softProvenance: [],
    source: mutation.source ?? null,
  };
}

function deriveCandidate(state, operation, receipt, mutationFacts = []) {
  const declarations = operation.declarations ?? [];
  if (!Array.isArray(declarations)) fail("declarationsInvalid");
  if (new Set(declarations.map((declaration) => declaration.name)).size !== declarations.length) fail("duplicateBinding");
  const changedNames = changedNamesFor(operation, mutationFacts);
  const invalidation = [];
  const working = { ...state, generation: { ...state.generation, bindings: clone(state.generation.bindings) } };
  const nextNumber = state.generation.number + (changedNames.length > 0 ? 1 : 0);
  const candidateShell = { id: `candidate:${state.session.incarnation}:${state.generationSequence + 1}`, display: `g${nextNumber}`, number: nextNumber, incarnation: state.session.incarnation };
  const marks = changedNames.length ? markOldBindings(working, changedNames, invalidation, candidateShell) : { closure: [], changed: [] };
  const closure = marks.closure;
  const nextBindings = clone(working.generation.bindings);
  const temp = { ...state, generation: { ...state.generation, bindings: clone(state.generation.bindings) } };
  const candidate = { ...candidateShell, id: `gen:${sessionDigest("w-repl-candidate", { ordinal: receipt.ordinal, base: state.generation.id, changedNames }).slice(7)}` };
  for (const imported of operation.imports ?? []) {
    if (!imported || typeof imported.name !== "string") fail("importDependencyInvalid");
    normalizeDependency({ ...imported, kind: "importSymbol" }, state, { name: imported.name });
  }
  temp.generation.bindings = nextBindings;
  for (const declaration of declarations) {
    const binding = buildBinding(temp, declaration, candidate);
    nextBindings.push(binding);
    temp.generation.bindings = nextBindings;
  }
  for (const facts of mutationFacts) {
    const declaration = mutationDeclaration(state, operation.mutations?.find((mutation) => mutation.name === facts.name && (mutation.operation ?? null) === facts.operation) ?? {}, facts, candidate);
    if (!declaration) continue;
    const binding = buildBinding(temp, declaration, candidate);
    nextBindings.push(binding);
    temp.generation.bindings = nextBindings;
  }
  const totalBindings = nextBindings.length;
  const edgeCount = nextBindings.reduce((sum, binding) => sum + binding.hardDependencies.length, 0);
  const artifactCount = nextBindings.reduce((sum, binding) => sum + (binding.hirFingerprint ? 1 : 0), 0);
  if (totalBindings > state.limits.bindings) fail("bindingQuota", { family: "bindings", total: totalBindings, limit: state.limits.bindings });
  if (edgeCount > state.limits.edges) fail("edgeQuota", { family: "hardEdges", total: edgeCount, limit: state.limits.edges });
  if (artifactCount > state.limits.artifacts) fail("artifactQuota", { family: "hirArtifacts", total: artifactCount, limit: state.limits.artifacts });
  const next = nextNumber === state.generation.number
    ? state.generation
    : makeGeneration(state, nextNumber, receipt.ordinal, nextBindings, "committed");
  if (next !== state.generation) next.id = candidate.id;
  const newOwnerScopes = [];
  if (next !== state.generation) {
    next.scope.resources.push(...state.ownerScopes.filter((scope) => scope.active && !closure.includes(scope.scopeId)).map(clone));
    next.bindings = nextBindings;
    for (const declaration of declarations) {
      if (declaration.persistentResource === true || declaration.persistentTask === true) {
        const scopeId = `owner:${state.session.incarnation}:${sessionDigest("w-repl-owner-scope", { name: declaration.name, ordinal: receipt.ordinal }).slice(7, 23)}`;
        const ownerBinding = [...nextBindings].reverse().find((binding) => binding.name === declaration.name && binding.availability === "available");
        const resource = { scopeId, bindingName: declaration.name, bindingId: ownerBinding?.bindingId ?? null, resource: declaration.resource ?? declaration.name, kind: declaration.persistentTask ? "task" : "resource", state: "owned", parent: "session", generationId: next.id, active: true };
        newOwnerScopes.push(resource);
        next.scope.resources.push(clone(resource));
        const matching = next.bindings.find((binding) => binding.bindingId === resource.bindingId);
        if (matching) matching.ownerScopeId = scopeId;
      }
    }
  }
  if (next.scope.resources.length > state.limits.resources || next.scope.children.length > state.limits.tasks) fail("scopeQuota", { family: "resources-or-tasks" });
  receipt.invalidation = invalidation;
  return { next, invalidation, closure, superseded: marks.changed.map((binding) => binding.bindingId), changedNames, newOwnerScopes, publishesGraph: next !== state.generation, totalBindings, edgeCount, artifactCount, predecessor: marks.predecessor ?? new Map() };
}

function deriveDrainPlan(state, operation, base, candidate) {
  const targets = [];
  const invalidatedIds = new Set([...candidate.invalidation.map((entry) => entry.bindingId), ...(candidate.superseded ?? [])]);
  for (const binding of base.bindings) {
    if (invalidatedIds.has(binding.bindingId) || binding.availability === "superseded") {
      if (binding.ownerScopeId) targets.push(state.ownerScopes.find((scope) => scope.scopeId === binding.ownerScopeId));
    }
  }
  const uniqueTargets = targets.filter((scope, index, all) => scope && all.findIndex((candidateScope) => candidateScope.scopeId === scope.scopeId) === index);
  const events = Array.isArray(operation.resourceEvents) ? operation.resourceEvents : [];
  const plans = uniqueTargets.map((scope) => {
    const event = events.find((candidateEvent) => candidateEvent.scopeId === scope.scopeId || candidateEvent.resource === scope.resource || candidateEvent.bindingName === scope.bindingName) ?? {};
    const active = event.active ?? scope.active;
    const providerEvents = Array.isArray(event.events) ? event.events : [];
    const providerState = event.providerState ?? providerEvents.find((entry) => entry?.providerState)?.providerState ?? null;
    const factsKnown = ["replaceable", "unreplaceable", "foreign-retained"].includes(providerState);
    const replaceable = providerState === "replaceable";
    const deadline = event.deadlineMs ?? state.limits.drainTimeMs;
    const confirmation = operation.allowDrain?.confirmed === true && (!Array.isArray(operation.allowDrain.targets) || operation.allowDrain.targets.includes(scope.scopeId) || operation.allowDrain.targets.includes(scope.resource));
    return { scopeId: scope.scopeId, resource: scope.resource, active, providerState, replaceable, factsKnown, deadline, confirmation, cooperative: event.cooperative !== false, providerEvents: clone(providerEvents) };
  });
  if (operation.drain && Object.keys(operation.drain).some((key) => ["replaceability", "foreignRetention", "postPublishFailure", "quota", "deadline"].includes(key))) fail("drainConclusionFactRequired", { expected: "resourceEvents-and-allowDrain" });
  if ((operation.resourceEvents ?? []).some((event) => Object.hasOwn(event, "replaceable") || Object.hasOwn(event, "foreignRetention"))) fail("drainConclusionFactRequired", { expected: "providerState-and-events" });
  const reasons = [];
  for (const plan of plans) {
    if (!plan.factsKnown) reasons.push({ family: "providerFacts", plan });
    if (plan.replaceable === false) reasons.push({ family: "replaceability", plan });
    if (plan.providerState === "foreign-retained" || plan.providerEvents.some((entry) => entry?.outcome === "foreign-retained")) reasons.push({ family: "foreignRetention", plan });
    if (plan.active && !plan.confirmation) reasons.push({ family: "confirmation", plan });
    if (plan.deadline < 0 || plan.deadline > state.limits.drainTimeMs) reasons.push({ family: "drainDeadline", plan, limit: state.limits.drainTimeMs });
  }
  return { targets: plans, reasons, closure: uniqueTargets.map((scope) => scope.scopeId), requiresDrain: plans.length > 0 };
}

function outputPlan(state, operation, receipt, candidate) {
  const outputs = [];
  const projected = [...state.outputs.all];
  let projectedBytes = state.outputs.bytes;
  let bytes = 0;
  for (const output of operation.outputs ?? []) {
    const itemBytes = Number.isFinite(output.bytes) ? output.bytes : byteLength(output);
    bytes += itemBytes;
    const item = {
      requestId: receipt.requestId,
      ordinal: receipt.ordinal,
      candidateGeneration: generationRef(candidate),
      kind: output.kind ?? "display",
      dataDigest: sessionDigest("w-repl-output", output.data ?? output.text ?? output),
      visibility: output.external === true ? "external" : "staged",
      budget: "reserved",
      requestedBytes: itemBytes,
      deliveredBytes: itemBytes,
      reservedBytes: itemBytes,
    };
    outputs.push(item);
    projected.push(item);
    projectedBytes += itemBytes;
  }
  const total = projectedBytes;
  if (total > state.limits.outputBytes) {
    if (operation.outputPolicy === "truncate") {
      const allowed = Math.max(0, state.limits.outputBytes - state.outputs.bytes);
      let used = 0;
      for (const item of outputs) {
        const size = item.requestedBytes;
        const remaining = Math.max(0, allowed - used);
        const delivered = Math.min(size, remaining);
        item.deliveredBytes = delivered;
        item.reservedBytes = delivered;
        if (delivered === size) {
          used += delivered;
        } else if (delivered > 0) {
          item.truncated = true;
          item.budget = "truncated";
          used += delivered;
        } else {
          item.dropped = true;
          item.budget = "dropped";
        }
      }
    } else fail("outputQuota", { family: "outputBytes", required: total, limit: state.limits.outputBytes, reserved: true });
  }
  return outputs;
}

function clearActive(state) {
  state.admission.active = null;
  state.admission.writer = null;
  state.staged = null;
}

function commitOutputs(state, outputs, visibility) {
  for (const output of outputs ?? []) {
    const committed = { ...output, visibility, budget: output.truncated ? "truncated" : output.budget };
    state.outputs.all.push(committed);
    state.outputs.committed.push(committed);
    state.outputs.bytes += output.reservedBytes ?? 0;
    while (state.outputs.all.length > state.limits.outputCount) state.outputs.all.shift();
    while (state.outputs.committed.length > state.limits.outputCount) {
      const removed = state.outputs.committed.shift();
      state.outputs.bytes = Math.max(0, state.outputs.bytes - (removed?.reservedBytes ?? 0));
      state.outputs.evicted += 1;
    }
  }
}

function settleActive(state, active) {
  const { operation, receipt, source, candidate, outputs, historyReservation } = active;
  const runtimeFailure = executionFailure(operation) || active.lifecycle?.outcome === "failure";
  if (active.cancelRequested || runtimeFailure) {
    phaseTrace(state, "settling", { outcome: active.cancelRequested ? "cancelled" : "runtime-error", structuredChildren: active.lifecycle?.events ?? [], deferred: active.lifecycle?.outcome ?? "ready" });
    receipt.outcome = active.cancelRequested ? "cancelled" : "runtime-error";
    receipt.cleanup = { stagedScope: "dropped-e1", oldGeneration: null, drain: "not-required", forceBoundary: false, cancellation: active.cancelRequested ? "active-before-publish" : undefined };
    const external = outputs.filter((output) => output.visibility === "external");
    commitOutputs(state, external, "external");
    finalizeReceipt(state, receipt, source, historyReservation);
    state.phase = "ready";
    clearActive(state);
    return receipt;
  }
  phaseTrace(state, "settling", { outcome: candidate.publishesGraph ? "ready-for-publish" : "committed", publication: candidate.publishesGraph ? "pending" : "none", structuredChildren: active.lifecycle?.events ?? [], deferred: active.lifecycle?.outcome ?? "ready" });
  if (!candidate.publishesGraph) {
    receipt.outcome = "committed";
    receipt.cleanup = { stagedScope: "settled", oldGeneration: null, drain: "not-required", forceBoundary: false };
    commitOutputs(state, outputs, "committed");
    phaseTrace(state, "committed/ready", { generation: generationRef(state.generation), publication: "none" });
    finalizeReceipt(state, receipt, source, historyReservation);
    state.phase = "ready";
    clearActive(state);
    return receipt;
  }
  active.stage = "publishing";
  state.phase = "settling";
  return null;
}

function publishActive(state, active) {
  const { candidate, receipt, outputs } = active;
  phaseTrace(state, "publish", { atomic: true, candidate: generationRef(candidate.next) });
  active.oldGeneration = clone(state.generation);
  state.generationSequence += 1;
  // deriveCandidate computes the opaque published GenerationId once.  Publish
  // must preserve it so new bindings point to the generation that exists.
  state.generation = { ...candidate.next };
  state.generation.bindings = candidate.next.bindings.map((binding) => ({ ...binding }));
  receipt.generationFinal = generationRef(state.generation);
  if (state.generation.id !== candidate.next.id) fail("generationIdentityMismatch", { candidate: candidate.next.id, published: state.generation.id });
  for (const newBinding of state.generation.bindings.filter((binding) => binding.createdGenerationId === candidate.id)) {
    if (newBinding.createdGenerationId !== receipt.generationFinal.id) fail("generationBindingIdentityMismatch", { bindingId: newBinding.bindingId, created: newBinding.createdGenerationId, published: receipt.generationFinal.id });
  }
  state.generation.graphFingerprint = sessionDigest("w-repl-graph", state.generation.bindings);
  state.generationHistory.push(generationHistoryRecord(state.generation));
  while (state.generationHistory.length > state.limits.historyCount) state.generationHistory.shift();
  for (const entry of candidate.invalidation) boundedPush(state, "invalidation", entry, state.limits.invalidationCount, "invalidationBytes", byteLength(entry), "invalidationEvicted");
  const drainedScopeIds = new Set(active.drainPlan.closure);
  state.ownerScopes = [
    ...state.ownerScopes.map((scope) => drainedScopeIds.has(scope.scopeId)
      ? { ...scope, state: "draining", pendingCleanup: true, drainingGenerationId: state.generation.id, active: true }
      : scope),
    ...candidate.newOwnerScopes.map((scope) => ({ ...scope, generationId: state.generation.id, incarnation: state.session.incarnation })),
  ];
  state.generation.scope.resources = state.ownerScopes.map(clone);
  commitOutputs(state, outputs, "committed");
  phaseTrace(state, "draining-old", { oldGeneration: generationRef(active.oldGeneration), closure: active.drainPlan.closure, admissionClosed: true, targets: active.drainPlan.targets });
  active.stage = "draining";
  active.published = true;
  state.phase = "draining-old";
  return null;
}

function drainActive(state, active) {
  const { operation, receipt, source, historyReservation } = active;
  const postEvents = Array.isArray(operation.drainEvents) ? operation.drainEvents : [];
  const postFailure = postEvents.some((event) => event.phase === "post-publish" && ["failure", "deadline", "foreign-non-cooperative"].includes(event.outcome));
  if (postFailure || active.cancelRequested) {
    state.generation.status = "degraded";
    state.generation.committed = true;
    state.generation.scope.state = "degraded";
    state.phase = "degraded";
    state.mutationBlocked = true;
    receipt.outcome = "degraded";
    receipt.cleanup = { stagedScope: "committed", oldGeneration: generationRef(active.oldGeneration), drain: "post-publish-failed", forceBoundary: false, degraded: true, admission: "closed", cancellation: active.cancelRequested ? "requested-after-publish" : "requested", children: "pending", waits: "pending", loans: "closed", drops: "pending" };
    receipt.diagnostics.push(diagnostic("W-SESSION-0013", "old generation drain failed after publication", "session.drain.post-publish", { events: postEvents, newGeneration: state.generation.id }));
    const failedIds = new Set(active.drainPlan.closure);
    state.ownerScopes = state.ownerScopes.map((scope) => failedIds.has(scope.scopeId) ? { ...scope, state: "faulted", pendingCleanup: true, active: true } : scope);
  } else {
    receipt.outcome = "committed";
    receipt.cleanup = { stagedScope: "committed", oldGeneration: active.drainPlan.requiresDrain ? generationRef(active.oldGeneration) : null, drain: active.drainPlan.requiresDrain ? "ready" : "not-required", forceBoundary: false, admission: "closed", cancellation: active.drainPlan.requiresDrain ? "requested" : "none", children: "drained", waits: "joined", loans: "closed", drops: "e1" };
    state.phase = "ready";
    const drainedIds = new Set(active.drainPlan.closure);
    state.ownerScopes = state.ownerScopes.filter((scope) => !drainedIds.has(scope.scopeId));
  }
  state.generation.scope.resources = state.ownerScopes.map(clone);
  phaseTrace(state, receipt.outcome === "degraded" ? "committed/degraded" : "committed/ready", { generation: generationRef(state.generation) });
  state.generation.scope.state = receipt.outcome === "degraded" ? "degraded" : "ready";
  state.generation.scope.resources = state.ownerScopes.map(clone);
  finalizeReceipt(state, receipt, source, historyReservation);
  clearActive(state);
  return receipt;
}

function beginSubmission(state, operation) {
  requireReady(state);
  ensureRequestContext(state, operation);
  rejectLegacyFacts(operation);
  const requestId = operation.requestId ?? `request:${state.trace.length + 1}`;
  const source = normalizeSource(operation.source ?? state.buffer.text);
  if (byteLength(source) > state.limits.sourceBytes) fail("sourceQuota", { family: "sourceBytes" });
  const classification = classifySource(source, operation);
  if (classification.kind === "command") fail("contextCommandRequiresCommandOperation");
  if (classification.kind === "incomplete") {
    state.buffer = { text: source, classification, complete: false };
    state.lastClassification = clone(classification);
    state.lastResult = { kind: "incomplete", classification: clone(classification), ordinal: state.session.executionOrdinal, generation: generationRef(state.generation) };
    trace(state, "incomplete", { requestId, classification });
    return null;
  }
  const base = state.generation;
  const requestedBase = requestedGeneration(state, operation.baseGeneration);
  const stale = operation.baseGeneration !== undefined && !generationMatches(base, operation.baseGeneration);
  if (stale && operation.reanalyse !== true) {
    const ordinal = incrementOrdinal(state);
    const receipt = baseReceipt(state, requestId, ordinal, requestedBase ?? base, "rejected", source);
    receipt.diagnostics = [diagnostic("W-SESSION-0005", "base generation is stale or not an opaque current identity", "session.admission", { requested: clone(operation.baseGeneration), current: generationRef(base) })];
    const reservation = reserveHistory(state, source, receipt);
    finalizeReceipt(state, receipt, source, reservation);
    return receipt;
  }
  if (classification.kind === "invalid") {
    const ordinal = incrementOrdinal(state);
    const receipt = baseReceipt(state, requestId, ordinal, base, "rejected", source);
    const detail = classification.parseDiagnostic ?? classification.semanticDiagnostic;
    receipt.diagnostics = [diagnostic(detail?.code ?? "W-SESSION-0003", detail?.reason ?? classification.reason, detail?.phase ?? "source.semantic", detail?.facts ?? { classification })];
    const reservation = reserveHistory(state, source, receipt);
    finalizeReceipt(state, receipt, source, reservation);
    return receipt;
  }
  const ordinal = incrementOrdinal(state);
  const receipt = baseReceipt(state, requestId, ordinal, base, "committed", source);
  if (stale && operation.reanalyse === true) {
    receipt.reanalysed = true;
    receipt.reanalysis = { previousRequestedBase: clone(operation.baseGeneration), currentOpaqueBase: generationRef(base) };
  }
  state.transactionTrace = [];
  phaseTrace(state, "collected", { sourceDigest: receipt.sourceDigest, form: classification.form, display: classification.display });
  phaseTrace(state, "parsed", { wrapper: "synthetic", parser: "normal", parseDiagnostic: classification.parseDiagnostic });
  phaseTrace(state, "checked", { checker: "normal", hir: "normal", semanticDiagnostic: classification.semanticDiagnostic, interactive: true });
  let mutationFacts;
  let candidate;
  let drainPlan;
  let historyReservation;
  let outputs;
  let lifecycle;
  try {
    mutationFacts = checkCrossGenerationMutations(state, operation.mutations, operation);
    validateEffectFacts(operation.effects);
    if ((operation.effects?.length ?? 0) > state.limits.effectCount || byteLength(operation.effects ?? []) > state.limits.effectBytes) fail("effectQuota", { family: "effects", total: operation.effects.length, bytes: byteLength(operation.effects ?? []), limit: state.limits.effectCount, byteLimit: state.limits.effectBytes });
    lifecycle = lifecycleFacts(operation);
    candidate = deriveCandidate(state, operation, receipt, mutationFacts);
    drainPlan = deriveDrainPlan(state, operation, base, candidate);
    historyReservation = reserveHistory(state, source, receipt);
    outputs = outputPlan(state, operation, receipt, candidate.next);
  } catch (error) {
    return rejectSemanticSubmission(state, operation, source, base, error, receipt, historyReservation);
  }
  if (drainPlan.reasons.length) {
    receipt.outcome = "rejected";
    const preflightCode = drainPlan.reasons.some((reason) => reason.family === "drainDeadline") ? "W-SESSION-0024" : "W-SESSION-0012";
    receipt.diagnostics.push(diagnostic(preflightCode, "drain preflight rejected before effects", "session.preflight", { reasons: drainPlan.reasons, closure: drainPlan.closure }));
    receipt.cleanup = { stagedScope: "dropped-e1", oldGeneration: generationRef(base), drain: "rejected-preflight", forceBoundary: false, preflight: drainPlan };
    state.transactionTrace.push({ phase: "preflight", details: { outcome: "rejected", ...drainPlan }, generation: generationRef(base), incarnation: state.session.incarnation });
    finalizeReceipt(state, receipt, source, historyReservation);
    return receipt;
  }
  state.admission.writer = { requestId, clientId: operation.clientId ?? "default", ticket: operation.ticket ?? null };
  state.admission.active = { requestId, operation: clone(operation), source, base: clone(base), receipt, candidate, drainPlan, outputs, historyReservation, lifecycle, stage: "executing", cancelRequested: false, published: false };
  state.staged = { requestId, ordinal, generation: candidate.next, bindings: candidate.next.bindings, visibility: "staged", snapshotBase: generationRef(base) };
  phaseTrace(state, "staged", { generation: generationRef(candidate.next), bindings: candidate.next.bindings.length, publishesGraph: candidate.publishesGraph, copyStages: mutationFacts.filter((mutation) => mutation.copyStage), automaticCopyStaging: mutationFacts.some((mutation) => mutation.copyStage) });
  phaseTrace(state, "preflight", { closure: drainPlan.closure, targets: drainPlan.targets, outcome: "ready" });
  state.phase = "staged";
  trace(state, "begin", { requestId, ordinal, base: generationRef(base), candidate: generationRef(candidate.next) });
  return null;
}

function advanceSubmission(state, operation = {}) {
  requireOpen(state);
  const active = state.admission.active;
  if (!active) fail("noActiveSubmission");
  if (operation.requestId && operation.requestId !== active.requestId) fail("requestNotActive");
  if (active.stage === "executing") {
    phaseTrace(state, "executing", { effects: active.operation.effects?.length ?? 0, outputs: active.outputs.length, ownerAsync: true, lifecycle: active.lifecycle?.events ?? [] });
    active.stage = "settling";
    state.phase = "executing";
    recordEffects(state, active.operation.effects ?? [], active.receipt, executionFailure(active.operation) || active.lifecycle?.outcome === "failure");
    trace(state, "advance", { requestId: active.requestId, phase: "executing" });
    return null;
  }
  if (active.stage === "settling") {
    state.phase = "settling";
    const result = settleActive(state, active);
    trace(state, "advance", { requestId: active.requestId, phase: "settling", outcome: result?.outcome ?? null });
    return result;
  }
  if (active.stage === "publishing") {
    const result = publishActive(state, active);
    trace(state, "advance", { requestId: active.requestId, phase: "publish" });
    return result;
  }
  if (active.stage === "draining") {
    const result = drainActive(state, active);
    trace(state, "advance", { requestId: active.requestId, phase: "draining-old", outcome: result?.outcome ?? null });
    return result;
  }
  fail("submissionPhaseInvalid", { phase: active.stage });
}

function finishSubmission(state, operation = {}) {
  requireOpen(state);
  const active = state.admission.active;
  if (!active) fail("noActiveSubmission");
  const result = [];
  while (state.admission.active) {
    const receipt = advanceSubmission(state, { requestId: operation.requestId ?? active.requestId });
    if (receipt) result.push(receipt);
  }
  return result.at(-1) ?? null;
}

function submitNow(state, operation) {
  if (state.admission.active) {
    if (operation.queue === true) {
      enqueue(state, { request: operation });
      return null;
    }
    fail("writerBusy");
  }
  const immediate = beginSubmission(state, operation);
  if (immediate) return immediate;
  if (!state.admission.active) return state.lastReceipt;
  return finishSubmission(state, { requestId: operation.requestId });
}

function enqueue(state, operation) {
  requireOpen(state);
  if (!operation.request || typeof operation.request !== "object") fail("requestInvalid");
  if (byteLength(operation.request.source ?? "") > state.limits.sourceBytes) fail("sourceQuota", { family: "sourceBytes" });
  const ticket = ++state.admission.sequence;
  const request = { ...clone(operation.request), ticket };
  if (state.admissionTickets.length >= state.limits.ticketCount && !state.admissionTickets.some((entry) => entry.status !== "queued")) fail("ticketQuota", { family: "tickets", limit: state.limits.ticketCount });
  state.admission.queue.push(request);
  boundedPush(state, "admissionTickets", { ticket, requestId: request.requestId ?? null, clientId: request.clientId ?? "default", status: "queued" }, state.limits.ticketCount, "ticketBytes", 0, "ticketsEvicted", (entry) => entry.status !== "queued");
  state.admission.tickets = state.admissionTickets;
  state.lastResult = { kind: "queued", requestId: request.requestId ?? null, ticket, queueLength: state.admission.queue.length };
  trace(state, "enqueue", { requestId: request.requestId ?? null, ticket, queueLength: state.admission.queue.length });
}

function admitNext(state, operation = {}) {
  requireReady(state);
  const request = state.admission.queue.shift();
  if (!request) {
    state.lastResult = { kind: "queue-empty" };
    return;
  }
  const ticket = state.admission.tickets.find((entry) => entry.ticket === request.ticket);
  if (ticket) ticket.status = "admitted";
  const immediate = beginSubmission(state, { ...request, queue: false });
  const receipt = immediate ?? (operation.stepwise === true ? null : finishSubmission(state, { requestId: request.requestId }));
  state.lastResult = { kind: "admitted", receipt: clone(receipt), ticket: request.ticket, queueLength: state.admission.queue.length };
  return receipt;
}

function immutableSnapshot(state) {
  const bindings = (state.generation?.bindings ?? []).filter((binding) => binding.availability === "available").map((binding) => ({
    name: binding.name,
    bindingId: binding.bindingId,
    generation: binding.generation,
    generationId: binding.generationId,
    createdGenerationId: binding.createdGenerationId,
    createdGeneration: binding.createdGeneration,
    createdIncarnation: binding.createdIncarnation,
    incarnation: binding.incarnation,
    version: binding.version,
    kind: binding.kind,
    type: binding.type,
    value: clone(binding.value),
    valueDigest: binding.valueDigest,
    hardDependencies: clone(binding.hardDependencies),
    softProvenance: clone(binding.softProvenance),
  }));
  return Object.freeze({ generation: generationRef(state.generation), bindings: Object.freeze(bindings), graphFingerprint: state.generation.graphFingerprint, phase: state.phase });
}

function readOnly(state, operation) {
  requireOpen(state);
  if (operation.includeStaged === true || operation.visibility === "staged") fail("stagedVisibilityForbidden");
  const snapshot = immutableSnapshot(state);
  if (operation.op === "inspect") {
    state.lastResult = { kind: "inspect", source: "committed-snapshot", snapshot, binding: operation.name ? snapshot.bindings.find((binding) => binding.name === operation.name) ?? null : null };
  } else if (operation.op === "complete") {
    const prefix = operation.prefix ?? "";
    state.lastResult = { kind: "completion", source: "committed-snapshot", generation: snapshot.generation, names: snapshot.bindings.map((binding) => binding.name).filter((name) => name.startsWith(prefix)).sort() };
  } else if (operation.op === "history") {
    state.lastResult = { kind: "history", count: state.history.records.length, bytes: state.history.bytes, reserved: state.history.reserved, ordinals: state.history.records.map((record) => record.ordinal), evicted: state.history.evicted };
  } else if (operation.op === "why") {
    const matches = state.invalidation.filter((entry) => entry.name === operation.name || entry.closure?.includes(operation.name) || entry.closure?.includes(currentBinding(state, operation.name)?.bindingId));
    state.lastResult = { kind: "why", name: operation.name ?? null, source: "committed-snapshot", invalidation: clone(matches) };
  } else {
    state.lastResult = { kind: "status", source: "committed-snapshot", session: { ...state.session, prompt: `w[${state.session.executionOrdinal}]` }, generation: generationRef(state.generation), phase: state.phase, mutationBlocked: state.mutationBlocked, queueLength: state.admission.queue.length, activeRequest: state.admission.active?.requestId ?? null };
  }
  state.reads += 1;
  trace(state, operation.op, { source: "committed-snapshot", generation: generationRef(state.generation), activeRequest: state.admission.active?.requestId ?? null });
}

function cancel(state, operation) {
  requireOpen(state);
  const requestId = operation.requestId ?? operation.target;
  if (!requestId) fail("requestIdRequired");
  const queuedIndex = state.admission.queue.findIndex((request) => request.requestId === requestId);
  if (queuedIndex >= 0) {
    const [request] = state.admission.queue.splice(queuedIndex, 1);
    const ticket = state.admission.tickets.find((entry) => entry.ticket === request.ticket);
    if (ticket) ticket.status = "cancelled";
    boundedPush(state, "cancellations", { requestId, ticket: request.ticket, disposition: "queued-removed", ordinal: null }, state.limits.cancellationCount, "cancellationBytes", 0, "cancellationsEvicted");
    state.lastResult = { kind: "canceled", requestId, disposition: "queued-removed", ordinal: null };
  } else if (state.admission.active?.requestId === requestId) {
    state.admission.active.cancelRequested = true;
    boundedPush(state, "cancellations", { requestId, disposition: "active-requested", ordinal: state.admission.active.receipt.ordinal }, state.limits.cancellationCount, "cancellationBytes", 0, "cancellationsEvicted");
    state.lastResult = { kind: "canceled", requestId, disposition: "active-requested", ordinal: state.admission.active.receipt.ordinal };
  } else {
    boundedPush(state, "cancellations", { requestId, disposition: "not-found", ordinal: null }, state.limits.cancellationCount, "cancellationBytes", 0, "cancellationsEvicted");
    state.lastResult = { kind: "canceled", requestId, disposition: "not-found", ordinal: null };
  }
  trace(state, "cancel", { requestId, active: state.admission.active?.requestId === requestId });
}

function preflightReset(state, operation, oldGeneration) {
  const targets = state.ownerScopes.filter((scope) => scope.active);
  const events = Array.isArray(operation.resourceEvents) ? operation.resourceEvents : [];
  const reasons = [];
  const plans = targets.map((scope) => {
    const event = events.find((candidate) => candidate.scopeId === scope.scopeId || candidate.resource === scope.resource) ?? {};
    const providerEvents = Array.isArray(event.events) ? event.events : [];
    const providerState = event.providerState ?? providerEvents.find((entry) => entry?.providerState)?.providerState ?? null;
    const factsKnown = ["replaceable", "unreplaceable", "foreign-retained"].includes(providerState);
    const replaceable = providerState === "replaceable";
    const confirmation = operation.allowDrain?.confirmed === true;
    if (!factsKnown) reasons.push({ family: "providerFacts", scopeId: scope.scopeId });
    if (replaceable === false) reasons.push({ family: "replaceability", scopeId: scope.scopeId });
    if (providerState === "foreign-retained" || providerEvents.some((entry) => entry?.outcome === "foreign-retained")) reasons.push({ family: "foreignRetention", scopeId: scope.scopeId });
    if (scope.active && !confirmation) reasons.push({ family: "confirmation", scopeId: scope.scopeId });
    return { scopeId: scope.scopeId, resource: scope.resource, providerState, factsKnown, replaceable, confirmation, event: clone(event) };
  });
  return { oldGeneration: generationRef(oldGeneration), targets: plans, reasons, closure: plans.map((plan) => plan.scopeId) };
}

function resetSession(state, operation, source = ":reset") {
  requireOpen(state);
  if (state.admission.active) fail("writerBusy");
  const old = state.generation;
  const preflightSnapshot = {
    incarnation: state.session.incarnation,
    generation: clone(state.generation),
    phase: state.phase,
    mutationBlocked: state.mutationBlocked,
    ownerScopes: clone(state.ownerScopes),
    admission: { writer: clone(state.admission.writer), active: clone(state.admission.active), queue: clone(state.admission.queue) },
    effects: clone(state.effects),
    effectsBytes: state.effectsBytes,
  };
  const requestId = operation.requestId ?? `reset:${state.trace.length + 1}`;
  const ordinal = incrementOrdinal(state);
  const receipt = baseReceipt(state, requestId, ordinal, old, "committed", source);
  state.transactionTrace = [];
  phaseTrace(state, "collected", { reset: true });
  phaseTrace(state, "parsed", { command: source });
  phaseTrace(state, "checked", { reset: true });
  const plan = preflightReset(state, operation, old);
  if (plan.reasons.length) {
    receipt.outcome = "rejected";
    receipt.diagnostics = [diagnostic("W-SESSION-0012", "reset drain preflight rejected", "session.preflight", plan)];
    receipt.cleanup = { stagedScope: "dropped-e1", oldGeneration: generationRef(old), drain: "rejected-preflight", forceBoundary: false, preflight: plan };
    phaseTrace(state, "preflight", { outcome: "rejected", ...plan });
    const reservation = reserveHistory(state, source, receipt);
    const afterPreflight = {
      incarnation: state.session.incarnation,
      generation: clone(state.generation),
      phase: state.phase,
      mutationBlocked: state.mutationBlocked,
      ownerScopes: clone(state.ownerScopes),
      admission: { writer: clone(state.admission.writer), active: clone(state.admission.active), queue: clone(state.admission.queue) },
      effects: clone(state.effects),
      effectsBytes: state.effectsBytes,
    };
    if (JSON.stringify(afterPreflight) !== JSON.stringify(preflightSnapshot) || receipt.effects.length !== 0) fail("resetPreflightMutated", { before: preflightSnapshot, after: afterPreflight });
    finalizeReceipt(state, receipt, source, reservation);
    return receipt;
  }
  phaseTrace(state, "staged", { newIncarnation: state.session.incarnation + 1 });
  phaseTrace(state, "preflight", { outcome: "ready", ...plan });
  const resetQueue = state.admission.queue.splice(0);
  for (const request of resetQueue) {
    const ticket = state.admission.tickets.find((entry) => entry.ticket === request.ticket);
    if (ticket) ticket.status = "reset-boundary";
    boundedPush(state, "cancellations", { requestId: request.requestId ?? null, ticket: request.ticket ?? null, disposition: "reset-boundary", ordinal: null }, state.limits.cancellationCount, "cancellationBytes", 0, "cancellationsEvicted");
  }
  const previousIncarnation = state.session.incarnation;
  state.session.incarnation += 1;
  receipt.incarnationFinal = state.session.incarnation;
  state.generationSequence = 0;
  state.generation = makeGeneration(state, 0, ordinal, []);
  state.generationHistory = [generationHistoryRecord(state.generation)];
  state.ownerScopes = state.ownerScopes.map((scope) => plan.closure.includes(scope.scopeId) ? { ...scope, state: "draining", pendingCleanup: true, drainingIncarnation: state.session.incarnation } : scope);
  state.invalidation = [];
  state.mutationBlocked = false;
  state.phase = "ready";
  state.staged = null;
  phaseTrace(state, "publish", { atomic: true, incarnation: state.session.incarnation, display: "g0" });
  phaseTrace(state, "draining-old", { oldGeneration: generationRef(old), admissionClosed: true, closure: plan.closure });
  const postEvents = Array.isArray(operation.drainEvents) ? operation.drainEvents : [];
  if (postEvents.some((event) => event.phase === "post-publish" && event.outcome !== "ready")) {
    state.phase = "degraded";
    state.generation.status = "degraded";
    state.mutationBlocked = true;
    receipt.outcome = "degraded";
    receipt.cleanup = { stagedScope: "committed", oldGeneration: generationRef(old), drain: "post-publish-failed", forceBoundary: operation.force === true, degraded: true, queued: resetQueue.map((request) => request.requestId ?? null) };
    receipt.diagnostics.push(diagnostic("W-SESSION-0013", "reset drain failed after publication", "session.drain.post-publish", { oldIncarnation: previousIncarnation, newIncarnation: state.session.incarnation, events: postEvents }));
    state.ownerScopes = state.ownerScopes.map((scope) => plan.closure.includes(scope.scopeId) ? { ...scope, state: "faulted", pendingCleanup: true, active: true } : scope);
  } else {
    receipt.cleanup = { stagedScope: "committed", oldGeneration: generationRef(old), drain: plan.closure.length ? "ready" : "not-required", forceBoundary: operation.force === true, queued: resetQueue.map((request) => request.requestId ?? null), cancellation: "requested", children: "drained", waits: "joined", drops: "e1" };
    const drainedIds = new Set(plan.closure);
    state.ownerScopes = state.ownerScopes.filter((scope) => !drainedIds.has(scope.scopeId));
  }
  state.generation.scope.resources = state.ownerScopes.map(clone);
  phaseTrace(state, receipt.outcome === "degraded" ? "committed/degraded" : "committed/ready", { generation: generationRef(state.generation), incarnation: state.session.incarnation });
  const reservation = reserveHistory(state, source, receipt);
  finalizeReceipt(state, receipt, source, reservation);
  return receipt;
}

function closeSession(state, operation, source = ":quit") {
  requireOpen(state);
  if (state.admission.active) {
    state.admission.active.cancelRequested = true;
    finishSubmission(state, { requestId: state.admission.active.requestId });
  }
  const requestId = operation.requestId ?? `quit:${state.trace.length + 1}`;
  const ordinal = incrementOrdinal(state);
  const old = state.generation;
  const receipt = baseReceipt(state, requestId, ordinal, old, "committed", source);
  state.transactionTrace = [];
  phaseTrace(state, "collected", { quit: true });
  phaseTrace(state, "preflight", { closeAdmission: true, ownerScopes: state.ownerScopes.length });
  const plan = preflightReset(state, { ...operation, allowDrain: operation.allowDrain ?? { confirmed: operation.force === true } }, old);
  const reservation = reserveHistory(state, source, receipt);
  if (plan.reasons.length && operation.force !== true) {
    receipt.outcome = "degraded";
    receipt.cleanup = { stagedScope: "none", oldGeneration: generationRef(old), drain: "force-boundary-required", forceBoundary: false, close: "admission-closed" };
    receipt.diagnostics.push(diagnostic("W-SESSION-0014", "session close could not drain owner scopes", "session.close", plan));
    state.ownerScopes = state.ownerScopes.map((scope) => plan.closure.includes(scope.scopeId) ? { ...scope, state: "closing", pendingCleanup: true } : scope);
    state.admission.queue = [];
    state.closed = false;
    state.phase = "closing";
    state.mutationBlocked = true;
  } else {
    const forced = plan.reasons.length > 0 && operation.force === true;
    receipt.cleanup = { stagedScope: "none", oldGeneration: generationRef(old), drain: forced ? "forced" : "ready", forceBoundary: forced, close: "admission-closed", ownerScopes: forced ? "force-boundary" : "drained" };
    if (forced) state.ownerScopes = state.ownerScopes.map((scope) => plan.closure.includes(scope.scopeId) ? { ...scope, state: "force-boundary", pendingCleanup: true, active: false, forceBoundary: true } : scope);
    else state.ownerScopes = state.ownerScopes.filter((scope) => !plan.closure.includes(scope.scopeId));
  }
  phaseTrace(state, receipt.outcome === "degraded" ? "committed/degraded" : "committed/ready", { close: true });
  finalizeReceipt(state, receipt, source, reservation);
  state.admission.queue = [];
  if (receipt.outcome !== "degraded") {
    state.closed = true;
    state.phase = "closed";
  }
  state.lastResult = { kind: "quit", receipt: clone(receipt) };
  trace(state, "quit", { forceBoundary: receipt.cleanup.forceBoundary });
  return receipt;
}

function command(state, operation) {
  requireOpen(state);
  if (typeof operation.text !== "string") fail("commandRequired");
  if (state.buffer.text.trim() !== "") fail("contextCommandPosition");
  const match = /^\s*:([a-z]+)(?:\s+(.+))?$/i.exec(operation.text);
  if (!match || !COMMANDS.has(match[1].toLowerCase())) fail("unknownContextCommand");
  const name = match[1].toLowerCase();
  const argument = match[2] ?? null;
  if (name === "why" && !argument) fail("commandArgumentRequired");
  switch (name) {
    case "status": readOnly(state, { op: "status" }); break;
    case "history": readOnly(state, { op: "history" }); break;
    case "why": readOnly(state, { op: "why", name: argument }); break;
    case "cancel": {
      const target = argument ?? state.admission.active?.requestId ?? state.admission.queue[0]?.requestId ?? null;
      if (target) cancel(state, { requestId: target });
      else {
        state.lastResult = { kind: "canceled", requestId: null, disposition: "no-active-request", ordinal: null };
        trace(state, "cancel", { requestId: null, active: false });
      }
      break;
    }
    case "reset": resetSession(state, { ...(operation.reset ?? {}), requestId: operation.requestId }, operation.text); break;
    case "quit": closeSession(state, operation, operation.text); break;
    default: fail("unknownContextCommand");
  }
}

function verify(state, operation) {
  requireOpen(state, { allowClosed: true });
  const checks = operation.checks ?? {};
  const actual = {
    sessionId: state.session?.sessionId ?? null,
    incarnation: state.session?.incarnation ?? null,
    ordinal: state.session?.executionOrdinal ?? null,
    prompt: state.session ? `w[${state.session.executionOrdinal}]` : null,
    generation: generationRef(state.generation),
    phase: state.phase,
    mutationBlocked: state.mutationBlocked,
    historyCount: state.history.records.length,
    historyBytes: state.history.bytes,
    queueLength: state.admission.queue.length,
    lastOutcome: state.lastReceipt?.outcome ?? null,
    activeRequest: state.admission.active?.requestId ?? null,
  };
  for (const [key, expected] of Object.entries(checks)) if (JSON.stringify(actual[key]) !== JSON.stringify(expected)) fail("verificationMismatch", { key, expected, actual: actual[key] });
  state.lastResult = { kind: "verify", actual };
}

function applyOperation(state, operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string" || !OPERATION_NAMES.has(operation.op)) fail("invalidOperation");
  switch (operation.op) {
    case "open": openSession(state, operation); break;
    case "appendBuffer": appendBuffer(state, operation); break;
    case "clearBuffer": clearBuffer(state); break;
    case "classify": classify(state, operation); break;
    case "submit": submitNow(state, operation); break;
    case "beginSubmission": beginSubmission(state, operation); break;
    case "advanceSubmission": advanceSubmission(state, operation); break;
    case "finishSubmission": finishSubmission(state, operation); break;
    case "enqueue": enqueue(state, operation); break;
    case "admitNext": admitNext(state, operation); break;
    case "complete":
    case "inspect":
    case "status":
    case "history":
    case "why": readOnly(state, operation); break;
    case "cancel": cancel(state, operation); break;
    case "command": command(state, operation); break;
    case "reset": resetSession(state, operation); break;
    case "restart": resetSession(state, operation, ":restart"); break;
    case "quit": closeSession(state, operation); break;
    case "verify": verify(state, operation); break;
    default: fail("invalidOperation");
  }
}

export function createReplSessionState() {
  return {
    phase: "empty",
    session: null,
    open: null,
    limits: normalizeLimits(),
    historyPolicy: { rawSource: "memory" },
    buffer: { text: "", classification: null, complete: false },
    lastClassification: null,
    generation: null,
    generationSequence: 0,
    generationHistory: [],
    ownerScopes: [],
    staged: null,
    admissionTickets: [],
    ticketBytes: 0,
    admission: { writer: null, active: null, queue: [], tickets: [], sequence: 0 },
    effects: [],
    effectsBytes: 0,
    effectsEvicted: 0,
    outputs: { all: [], committed: [], bytes: 0, evicted: 0 },
    receipts: [],
    lastReceipt: null,
    history: { records: [], bytes: 0, reserved: 0, evicted: 0 },
    invalidation: [],
    invalidationBytes: 0,
    invalidationEvicted: 0,
    cancellations: [],
    cancellationBytes: 0,
    cancellationsEvicted: 0,
    reads: 0,
    mutationBlocked: false,
    closed: false,
    lastResult: null,
    transactionTrace: [],
    trace: [],
    traceBytes: 0,
    traceEvicted: 0,
  };
}

export function validateReplSessionOperation(operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") return false;
  if (!OPERATION_NAMES.has(operation.op)) return false;
  if (operation.op === "open" && operation.sessionId !== undefined && typeof operation.sessionId !== "string") return false;
  if (["submit", "classify", "appendBuffer", "beginSubmission"].includes(operation.op) && operation.source !== undefined && typeof operation.source !== "string") return false;
  return true;
}

export function runReplSessionProgram(operations) {
  const state = createReplSessionState();
  const traceRecords = [];
  for (const [index, operation] of operations.entries()) {
    const before = clone(state);
    try {
      if (!validateReplSessionOperation(operation)) fail("invalidOperation");
      applyOperation(state, operation);
      traceRecords.push({ index, operation: clone(operation), before, after: clone(state) });
    } catch (error) {
      if (!(error instanceof ReplSessionError)) throw error;
      traceRecords.push({ index, operation: clone(operation), before, rejected: error.code, details: error.details });
      state.trace.push({ index: state.trace.length, event: "reject", details: { code: error.code, operation: operation.op } });
      return { status: "rejected", code: error.code, operation: index, state, trace: traceRecords };
    }
  }
  return { status: "accepted", state, trace: traceRecords };
}
