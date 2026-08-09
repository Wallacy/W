import { createHash } from "node:crypto";

const DIGEST = /^sha256:[0-9a-f]{64}$/;
const OPAQUE = /^opaque:[A-Za-z0-9._:-]{1,128}$/;

export class NotebookExportError extends Error {
  constructor(code, details = {}) {
    super(code);
    this.name = "NotebookExportError";
    this.code = code;
    this.details = details;
  }
}

function fail(code, details = {}) {
  throw new NotebookExportError(code, details);
}

function clone(value) {
  return structuredClone(value);
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") return Object.fromEntries(Object.keys(value).sort().map((key) => [key, canonical(value[key])]));
  return value;
}

export function notebookDigest(tag, value) {
  return `sha256:${createHash("sha256").update(`${tag}\0${typeof value === "string" ? value : JSON.stringify(canonical(value))}`, "utf8").digest("hex")}`;
}

export function sourceText(source) {
  if (typeof source === "string") return source;
  if (Array.isArray(source) && source.every((line) => typeof line === "string")) return source.join("");
  fail("W-EXPORT-0001", { reason: "cell source must be a String or Array<String>" });
}

export function sourceDigest(source) {
  return notebookDigest("source", sourceText(source));
}

function planDigest(manifest) {
  const plan = manifest.plan;
  return notebookDigest("plan", {
    kind: plan.kind,
    entry: plan.entry,
    defaultEntry: plan.defaultEntry,
    roles: plan.roles,
    modules: (plan.modules ?? []).map((module) => ({ id: module.id, role: module.role, sourceCells: module.sourceCells, digest: module.digest })),
    target: manifest.target,
    lock: manifest.lock.digest,
    context: manifest.context.digest,
    toolchain: manifest.toolchainDigest,
    receipts: (manifest.receipts ?? []).map((receipt) => ({
      cellId: receipt.cellId,
      sourceDigest: receipt.sourceDigest,
      ordinal: receipt.ordinal,
      sessionId: receipt.sessionId,
      incarnation: receipt.incarnation,
      generationBefore: receipt.generationBefore,
      generationAfter: receipt.generationAfter,
      bindingRecords: receipt.bindingRecords,
      hardEdges: receipt.hardEdges,
      providerOutcomes: receipt.providerOutcomes,
      effectOutcomes: receipt.effectOutcomes,
      inputs: receipt.inputs,
      resourceStates: receipt.resourceStates,
    })),
  });
}

function boundedJsonBytes(value, limit, reason) {
  const stack = [{ value, depth: 1 }];
  const seen = new WeakSet();
  let bytes = 2;
  while (stack.length > 0) {
    const item = stack.pop();
    const current = item.value;
    if (item.depth > 64) fail("W-EXPORT-0001", { reason: `${reason} depth exceeded` });
    if (current === null) { bytes += 4; continue; }
    if (typeof current === "string") { bytes += Buffer.byteLength(JSON.stringify(current), "utf8"); }
    else if (typeof current === "number" || typeof current === "boolean") { bytes += Buffer.byteLength(JSON.stringify(current), "utf8"); }
    else if (typeof current === "undefined" || typeof current === "function" || typeof current === "symbol" || typeof current === "bigint") fail("W-EXPORT-0001", { reason: `${reason} is not serializable` });
    else if (typeof current === "object") {
      if (seen.has(current)) fail("W-EXPORT-0001", { reason: `${reason} contains a cycle` });
      seen.add(current);
      const entries = Array.isArray(current) ? current.map((child) => ["", child]) : Object.entries(current);
      bytes += entries.length;
      for (const [key, child] of entries) {
        if (key) bytes += Buffer.byteLength(JSON.stringify(key), "utf8") + 1;
        stack.push({ value: child, depth: item.depth + 1 });
      }
    }
    if (bytes > limit) fail("W-EXPORT-0001", { reason: `${reason} bound exceeded` });
  }
  return bytes;
}

function defaultLimits() {
  return { cells: 128, cellBytes: 65_536, sourceBytes: 262_144, outputBytes: 131_072, metadataBytes: 16_384, dependencyEdges: 512, receiptBytes: 65_536, auditBytes: 131_072, dependencyBytes: 65_536 };
}

function initialState() {
  return { phase: "empty", limits: defaultLimits(), notebook: null, cells: [], manifest: null, selected: [], pendingInvalidation: [], pendingRedefinition: [], order: [], output: null, audit: null, executions: 0, diagnostics: [] };
}

function requireLoaded(state) {
  if (state.phase === "empty") fail("W-EXPORT-0001", { reason: "notebook is not loaded" });
}

function digest(value, label) {
  if (typeof value !== "string" || !DIGEST.test(value)) fail("W-EXPORT-0003", { reason: `${label} is not a sha256 digest` });
  return value;
}

function normaliseCells(cells, limits, rawPolicy) {
  if (!Array.isArray(cells) || cells.length > limits.cells) fail("W-EXPORT-0001", { reason: "cell count exceeds bound" });
  const ids = new Set();
  let totalSource = 0;
  for (const cell of cells) {
    if (!cell || !/^[A-Za-z0-9_-]{1,64}$/.test(cell.id ?? "") || ids.has(cell.id)) fail("W-EXPORT-0001", { cellId: cell?.id, reason: "cell ID is invalid or duplicated" });
    if (!["code", "markdown", "raw"].includes(cell.cell_type)) fail("W-EXPORT-0001", { cellId: cell.id, reason: "cell type is invalid" });
    if (cell.cell_type === "raw" && rawPolicy === "reject") fail("W-EXPORT-0001", { cellId: cell.id, reason: "raw cells are rejected by export policy" });
    const source = sourceText(cell.source ?? "");
    const sourceBytes = Buffer.byteLength(source, "utf8");
    if (sourceBytes > limits.cellBytes || (totalSource += sourceBytes) > limits.sourceBytes) fail("W-EXPORT-0001", { cellId: cell.id, reason: "source bound exceeded" });
    const metadataBytes = boundedJsonBytes(cell.metadata ?? {}, limits.metadataBytes, "cell metadata");
    if (metadataBytes > limits.metadataBytes) fail("W-EXPORT-0001", { cellId: cell.id, reason: "cell metadata bound exceeded" });
    if (cell.cell_type !== "code" && Array.isArray(cell.outputs) && cell.outputs.length > 0) fail("W-EXPORT-0001", { cellId: cell.id, reason: "markdown and raw outputs are not export inputs" });
    const outputBytes = boundedJsonBytes(cell.outputs ?? [], limits.outputBytes, "cell outputs");
    if (outputBytes > limits.outputBytes) fail("W-EXPORT-0001", { cellId: cell.id, reason: "cell output bound exceeded" });
    ids.add(cell.id);
  }
  return cells.map((cell) => ({ ...clone(cell), source: sourceText(cell.source ?? "") }));
}

function receiptList(state) {
  const receipts = state.manifest?.receipts;
  if (!Array.isArray(receipts)) fail("W-EXPORT-0002", { reason: "receipt manifest is missing" });
  const byCell = new Map();
  for (const receipt of receipts) {
    if (!receipt || typeof receipt.cellId !== "string") fail("W-EXPORT-0002", { reason: "receipt cell identity is missing" });
    if (byCell.has(receipt.cellId)) fail("W-EXPORT-0002", { cellId: receipt.cellId, reason: "receipt is duplicated" });
    byCell.set(receipt.cellId, receipt);
    boundedJsonBytes(receipt, state.limits.receiptBytes, "receipt");
  }
  return byCell;
}

function bindingMap(receipt) {
  if (!Array.isArray(receipt.bindingRecords) || receipt.bindingRecords.length === 0) fail("W-EXPORT-0003", { cellId: receipt.cellId, reason: "binding records are missing" });
  const map = new Map();
  for (const record of receipt.bindingRecords) {
    if (!record || typeof record.id !== "string" || !Number.isInteger(record.version) || record.version < 1 || typeof record.fingerprint !== "string" || !DIGEST.test(record.fingerprint) || !Number.isInteger(record.creationIncarnation) || record.creationIncarnation < 1 || !OPAQUE.test(record.creationGenerationId ?? "")) fail("W-EXPORT-0003", { cellId: receipt.cellId, reason: "binding record is malformed" });
    if (map.has(record.id)) fail("W-EXPORT-0003", { cellId: receipt.cellId, reason: "binding record is duplicated" });
    map.set(record.id, record);
  }
  return map;
}

function validateFacts(state, receipt, bindings) {
  if (!Array.isArray(receipt.providerOutcomes) || receipt.providerOutcomes.some((entry) => !entry || typeof entry.provider !== "string" || !["ok", "missing", "rejected"].includes(entry.status))) fail("W-EXPORT-0005", { cellId: receipt.cellId, reason: "provider outcome facts are missing" });
  if (!Array.isArray(receipt.effectOutcomes) || receipt.effectOutcomes.some((entry) => !entry || typeof entry.kind !== "string" || !["known", "unknown", "rejected"].includes(entry.status))) fail("W-EXPORT-0005", { cellId: receipt.cellId, reason: "effect outcome facts are missing" });
  const inputDenied = new Set(["value", "password", "token", "secretValue", "raw", "live"]);
  if (!Array.isArray(receipt.inputs) || receipt.inputs.some((entry) => !entry || typeof entry.kind !== "string" || !["literal", "binding", "explicitParameter"].includes(entry.kind) || !["resolved", "unresolved"].includes(entry.state) || entry.secret !== false || [...inputDenied].some((field) => Object.prototype.hasOwnProperty.call(entry, field)) || (entry.kind === "explicitParameter" && (entry.parameterized !== true || typeof entry.parameterId !== "string" || entry.parameterId.length === 0)))) fail("W-EXPORT-0005", { cellId: receipt.cellId, reason: "input facts are missing or secret" });
  const resourceDenied = new Set(["value", "pointer", "path", "time", "trust", "liveValue", "handle"]);
  if (!Array.isArray(receipt.resourceStates) || receipt.resourceStates.some((entry) => !entry || typeof entry.kind !== "string" || !["stable", "closed", "live", "degraded"].includes(entry.state) || (entry.state === "stable" && entry.serializable !== true && entry.closed !== true) || entry.capability === true || entry.foreign === true || entry.owner === "live" || [...resourceDenied].some((field) => Object.prototype.hasOwnProperty.call(entry, field)))) fail("W-EXPORT-0005", { cellId: receipt.cellId, reason: "resource facts are missing" });
  const blocked = receipt.outcome !== "committed" || receipt.providerOutcomes.some((entry) => entry.status !== "ok") || receipt.effectOutcomes.some((entry) => entry.status !== "known") || receipt.inputs.some((entry) => entry.state !== "resolved") || receipt.resourceStates.some((entry) => !["stable", "closed"].includes(entry.state));
  if (blocked) fail("W-EXPORT-0005", { cellId: receipt.cellId, reason: "structured provider, effect, input, or resource facts block export" });
  const edgeKeys = new Set();
  for (const edge of receipt.hardEdges ?? []) {
    const key = edge && `${edge.bindingId}:${edge.version}:${edge.kind}:${edge.cellId}`;
    const edgeKinds = new Set(["compiledLookup", "typeLayout", "constEval", "witness", "resourceOwner", "importSymbol"]);
    if (!edge || edgeKeys.has(key) || typeof edge.bindingId !== "string" || !Number.isInteger(edge.version) || !edgeKinds.has(edge.kind) || typeof edge.pyn2Kind !== "string" || edge.pyn2Kind !== edge.kind || typeof edge.cellId !== "string" || !Number.isInteger(edge.incarnation) || edge.incarnation < 1 || !OPAQUE.test(edge.generationId ?? "")) fail("W-EXPORT-0006", { cellId: receipt.cellId, reason: "hard edge is malformed" });
    edgeKeys.add(key);
  }
}

function validateManifest(state) {
  requireLoaded(state);
  const manifest = state.manifest;
  if (!manifest || manifest.version !== 2 || !manifest.lock || !manifest.context || typeof manifest.target !== "string" || !manifest.plan || !["single-file", "package"].includes(manifest.plan.kind)) fail("W-EXPORT-0002", { reason: "receipt manifest is incomplete" });
  digest(manifest.lock.digest, "lock digest");
  digest(manifest.context.digest, "context digest");
  digest(manifest.toolchainDigest, "toolchain digest");
  digest(manifest.plan.contentDigest, "plan content digest");
  const selected = state.selected.length > 0 ? [...state.selected] : state.cells.filter((cell) => cell.cell_type !== "raw").map((cell) => cell.id);
  if (new Set(selected).size !== selected.length) fail("W-EXPORT-0001", { reason: "selection contains duplicate cell IDs" });
  const cellById = new Map(state.cells.map((cell) => [cell.id, cell]));
  if (selected.some((cellId) => !cellById.has(cellId) || cellById.get(cellId).cell_type === "raw")) fail("W-EXPORT-0001", { reason: "selection contains unknown or raw cell" });
  const byCell = receiptList(state);
  const codeSelected = selected.filter((cellId) => cellById.get(cellId).cell_type === "code");
  const selectedBindingIds = new Set(codeSelected.flatMap((cellId) => (byCell.get(cellId)?.bindingRecords ?? []).map((record) => record.id)));
  const invalidated = manifest.invalidation?.cells ?? [];
  const redefinitions = manifest.redefinitions ?? [];
  if (!Array.isArray(invalidated) || !Array.isArray(redefinitions)) fail("W-EXPORT-0004", { reason: "invalidation facts are malformed" });
  if (invalidated.some((cellId) => typeof cellId !== "string") || redefinitions.some((entry) => !entry || typeof entry.bindingId !== "string")) fail("W-EXPORT-0004", { reason: "invalidation facts are malformed" });
  if (invalidated.some((cellId) => selected.includes(cellId)) || redefinitions.some((entry) => selected.includes(entry.bindingId))) fail("W-EXPORT-0004", { reason: "invalidation or redefinition requires new receipts" });
  const ordinals = new Set();
  const bindingByCell = new Map();
  let edgeCount = 0;
  if (!Array.isArray(manifest.plan.modules) || manifest.plan.modules.some((module) => !module || typeof module.id !== "string" || !/^[A-Za-z0-9_-]{1,64}$/.test(module.id) || !DIGEST.test(module.digest ?? "")) || new Set((manifest.plan.modules ?? []).map((module) => module.id)).size !== (manifest.plan.modules ?? []).length) fail("W-EXPORT-0003", { reason: "content-addressed module plan is malformed" });
  const allCode = state.cells.filter((cell) => cell.cell_type === "code").map((cell) => cell.id);
  for (const cellId of allCode) {
    const receipt = byCell.get(cellId);
    if (!receipt) continue;
    if (receipt.sourceDigest !== sourceDigest(cellById.get(cellId).source)) fail("W-EXPORT-0003", { cellId, reason: "source digest mismatch" });
    digest(receipt.sourceDigest, "source digest");
    if (!Number.isInteger(receipt.ordinal) || receipt.ordinal <= 0 || ordinals.has(receipt.ordinal) || !OPAQUE.test(receipt.generationBefore ?? "") || !OPAQUE.test(receipt.generationAfter ?? "") || typeof receipt.sessionId !== "string" || receipt.sessionId.length === 0 || !Number.isInteger(receipt.incarnation) || receipt.incarnation < 1 || receipt.toolchainDigest !== manifest.toolchainDigest || receipt.lockDigest !== manifest.lock.digest || receipt.contextDigest !== manifest.context.digest || (receipt.target !== undefined && receipt.target !== manifest.target) || typeof receipt.effectDigest !== "string" || !DIGEST.test(receipt.effectDigest) || !["committed", "degraded", "failed"].includes(receipt.outcome)) fail("W-EXPORT-0003", { cellId, reason: "receipt generation, session, lock, or effect proof is incomplete" });
    ordinals.add(receipt.ordinal);
    const bindings = bindingMap(receipt);
    bindingByCell.set(cellId, bindings);
    validateFacts(state, receipt, bindings);
    edgeCount += receipt.hardEdges?.length ?? 0;
    if (edgeCount > state.limits.dependencyEdges) fail("W-EXPORT-0002", { cellId, reason: "dependency edge quota exceeded" });
    boundedJsonBytes(receipt.hardEdges ?? [], state.limits.dependencyBytes, "dependency edges");
  }
  for (const cellId of codeSelected) if (!byCell.has(cellId)) fail("W-EXPORT-0002", { cellId, reason: "selected code cell lacks exactly one receipt" });
  const committedChain = [...allCode.filter((cellId) => byCell.has(cellId))].sort((left, right) => byCell.get(left).ordinal - byCell.get(right).ordinal || left.localeCompare(right));
  for (let index = 1; index < committedChain.length; index += 1) {
    const previous = byCell.get(committedChain[index - 1]);
    const current = byCell.get(committedChain[index]);
    if (previous.sessionId !== current.sessionId || previous.incarnation !== current.incarnation || previous.toolchainDigest !== current.toolchainDigest || previous.lockDigest !== current.lockDigest || previous.contextDigest !== current.contextDigest || (previous.target ?? manifest.target) !== (current.target ?? manifest.target) || previous.generationAfter !== current.generationBefore) fail("W-EXPORT-0003", { cellId: current.cellId, reason: "receipt generations are not a committed chain" });
  }
  for (const [cellId, receipt] of byCell) {
    if (!cellById.has(cellId) || cellById.get(cellId).cell_type !== "code") fail("W-EXPORT-0002", { cellId, reason: "receipt is not for a code cell" });
  }
  const invalidationClosure = new Set(invalidated);
  const redefinedBindings = new Set(redefinitions.map((entry) => entry?.bindingId).filter((bindingId) => typeof bindingId === "string"));
  let closureChanged = true;
  while (closureChanged) {
    closureChanged = false;
    for (const cellId of allCode) {
      const receipt = byCell.get(cellId);
      if (!receipt) continue;
      for (const edge of receipt.hardEdges ?? []) {
        if (invalidationClosure.has(edge.cellId) || redefinedBindings.has(edge.bindingId)) {
          if (!invalidationClosure.has(cellId)) { invalidationClosure.add(cellId); closureChanged = true; }
          redefinedBindings.add(edge.bindingId);
        }
      }
    }
  }
  if ([...invalidationClosure].some((cellId) => selected.includes(cellId)) || [...redefinedBindings].some((bindingId) => selectedBindingIds.has(bindingId))) fail("W-EXPORT-0004", { reason: "invalidation or redefinition requires new receipts" });
  for (const cellId of codeSelected) {
    const receipt = byCell.get(cellId);
    for (const edge of receipt.hardEdges ?? []) {
      if (!selected.includes(edge.cellId)) fail("W-EXPORT-0004", { cellId, dependency: edge.cellId, reason: "dependency is outside selected closure" });
      const target = bindingByCell.get(edge.cellId)?.get(edge.bindingId);
      if (!target || target.version !== edge.version || target.creationIncarnation !== edge.incarnation || target.creationGenerationId !== edge.generationId) fail("W-EXPORT-0006", { cellId, reason: "dependency binding identity does not match closure" });
    }
  }
  boundedJsonBytes(manifest, state.limits.auditBytes, "manifest");
  state.selected = selected;
  return { selected, cellById, byCell, codeSelected };
}

function dependencyOrder(state, facts) {
  const selectedCode = new Set(facts.codeSelected);
  const indegree = new Map(facts.codeSelected.map((id) => [id, 0]));
  const edges = new Map(facts.codeSelected.map((id) => [id, []]));
  for (const cellId of facts.codeSelected) {
    for (const edge of facts.byCell.get(cellId).hardEdges ?? []) {
      if (!selectedCode.has(edge.cellId)) continue;
      edges.get(edge.cellId).push(cellId);
      indegree.set(cellId, indegree.get(cellId) + 1);
    }
  }
  const effects = facts.codeSelected.filter((id) => (facts.byCell.get(id).effectOutcomes ?? []).some((entry) => entry.kind !== "pure")).sort((a, b) => facts.byCell.get(a).ordinal - facts.byCell.get(b).ordinal || a.localeCompare(b));
  for (let index = 1; index < effects.length; index += 1) { edges.get(effects[index - 1]).push(effects[index]); indegree.set(effects[index], indegree.get(effects[index]) + 1); }
  const ready = [...indegree.entries()].filter(([, degree]) => degree === 0).map(([id]) => id);
  const orderBy = (id) => facts.byCell.get(id).ordinal;
  ready.sort((a, b) => orderBy(a) - orderBy(b) || a.localeCompare(b));
  const ordered = [];
  while (ready.length > 0) {
    const id = ready.shift();
    ordered.push(id);
    for (const next of edges.get(id)) { indegree.set(next, indegree.get(next) - 1); if (indegree.get(next) === 0) ready.push(next); }
    ready.sort((a, b) => orderBy(a) - orderBy(b) || a.localeCompare(b));
  }
  if (ordered.length !== facts.codeSelected.length) fail("W-EXPORT-0006", { reason: "dependency cycle has no lossless linearization" });
  state.order = [...ordered, ...state.selected.filter((id) => !facts.codeSelected.includes(id))];
  return state.order;
}

function validatePlan(state, facts) {
  const orderedCode = state.order.filter((cellId) => facts.codeSelected.includes(cellId));
  const plan = state.manifest.plan;
  if (orderedCode.length === 0) {
    if (plan.entry !== null) fail("W-EXPORT-0003", { reason: "empty plan has a default entry" });
    if (!Array.isArray(plan.roles) || plan.roles.length !== 0 || !Array.isArray(plan.modules) || plan.modules.length !== 0) fail("W-EXPORT-0003", { reason: "empty plan roles and modules are not explicit" });
    if (!DIGEST.test(plan.contentDigest ?? "")) fail("W-EXPORT-0003", { reason: "empty plan content digest is not explicit" });
    return;
  }
  if (typeof plan.entry !== "string" || !facts.codeSelected.includes(plan.entry) || !plan.defaultEntry || plan.defaultEntry.cellId !== plan.entry || typeof plan.defaultEntry.bindingId !== "string" || !Number.isInteger(plan.defaultEntry.version) || plan.defaultEntry.version < 1 || typeof plan.defaultEntry.moduleId !== "string" || plan.defaultEntry.moduleId.length === 0) fail("W-EXPORT-0003", { reason: "plan default entry binding is not explicit" });
  const entryReceipt = facts.byCell.get(plan.entry);
  let entryBinding = null;
  for (const record of entryReceipt?.bindingRecords ?? []) if (record.id === plan.defaultEntry.bindingId) { entryBinding = record; break; }
  if (!entryBinding || entryBinding.version !== plan.defaultEntry.version) fail("W-EXPORT-0003", { reason: "plan default entry binding does not match receipt" });
  if (!Array.isArray(plan.roles) || plan.roles.some((role) => !role || typeof role.cellId !== "string" || !orderedCode.includes(role.cellId) || !["root", "pure", "effect", "support"].includes(role.role)) || new Set(plan.roles.map((role) => role.cellId)).size !== plan.roles.length || orderedCode.some((cellId) => !plan.roles.some((role) => role.cellId === cellId)) || plan.roles.length !== orderedCode.length) fail("W-EXPORT-0003", { reason: "plan cell roles are incomplete" });
  if (!Array.isArray(plan.modules)) fail("W-EXPORT-0003", { reason: "content-addressed module plan is malformed" });
  if (orderedCode.length > 0 && plan.modules.length === 0) fail("W-EXPORT-0003", { reason: "single-file plan needs a root module" });
  const modules = plan.modules;
  if (plan.kind === "package" && modules.length < 2) fail("W-EXPORT-0003", { reason: "package plan needs multiple modules" });
  if (plan.kind === "single-file" && modules.length !== 1) fail("W-EXPORT-0003", { reason: "single-file plan needs one root module" });
  if (modules.filter((module) => module?.role === "root").length !== 1) fail("W-EXPORT-0003", { reason: "plan must name one root module" });
  const assigned = new Set();
  for (const module of modules) {
    if (!module || typeof module.id !== "string" || module.id.length === 0 || !["root", "pure", "effect", "support"].includes(module.role) || !Array.isArray(module.sourceCells) || module.sourceCells.length === 0 || module.sourceCells.some((cellId) => !orderedCode.includes(cellId) || assigned.has(cellId))) fail("W-EXPORT-0003", { reason: "module assignment is not lossless" });
    const canonicalCells = orderedCode.filter((cellId) => module.sourceCells.includes(cellId));
    if (canonicalCells.length !== module.sourceCells.length || JSON.stringify(canonicalCells) !== JSON.stringify(module.sourceCells)) fail("W-EXPORT-0003", { reason: "module source order is not canonical" });
    module.sourceCells.forEach((cellId) => assigned.add(cellId));
    const moduleSource = canonicalCells.map((cellId) => facts.cellById.get(cellId).source).join("\n");
    const moduleDigest = sourceDigest(moduleSource);
    if (!DIGEST.test(module.digest ?? "") || module.digest !== moduleDigest) fail("W-EXPORT-0003", { reason: "module digest mismatches assigned source" });
  }
  if (assigned.size !== orderedCode.length) fail("W-EXPORT-0003", { reason: "module plan omits code source" });
  if (orderedCode.some((cellId) => !modules.some((module) => module.sourceCells.includes(cellId)))) fail("W-EXPORT-0003", { reason: "module plan omits code assignment" });
  const defaultModule = modules.find((module) => module.id === plan.defaultEntry.moduleId);
  if (!defaultModule || defaultModule.role !== "root" || !defaultModule.sourceCells.includes(plan.entry)) fail("W-EXPORT-0003", { reason: "default entry binding is outside root module assignment" });
  state.manifest.plan.modules = modules;
  const expectedDigest = planDigest(state.manifest);
  if (!DIGEST.test(plan.contentDigest ?? "") || plan.contentDigest !== expectedDigest) fail("W-EXPORT-0003", { reason: "plan content digest is not content-addressed" });
}

function outputMediaDigests(state, facts) {
  return state.order.map((cellId) => {
    const cell = facts.cellById.get(cellId);
    return { cellId, digests: (cell.outputs ?? []).map((output) => notebookDigest("media", output)) };
  }).filter((entry) => facts.cellById.get(entry.cellId).cell_type === "code");
}

function exportNotebook(state, operation) {
  requireLoaded(state);
  if (operation.execute === true || operation.replay === true || operation.modules !== undefined || operation.entries !== undefined) fail("W-EXPORT-0007", { reason: "export operation cannot execute, replay, or choose modules" });
  const facts = validateManifest(state);
  dependencyOrder(state, facts);
  validatePlan(state, facts);
  const source = state.manifest.plan.kind !== "single-file" ? null : state.manifest.plan.modules.length > 0 ? state.manifest.plan.modules[0].sourceCells.map((cellId) => facts.cellById.get(cellId).source).join("\n") : "";
  const modules = state.manifest.plan.modules.map((module) => ({ id: module.id, role: module.role, sourceCells: [...module.sourceCells], source: module.sourceCells.map((cellId) => facts.cellById.get(cellId).source).join("\n"), digest: module.digest }));
  const companion = state.cells.filter((cell) => cell.cell_type === "markdown").map((cell) => ({ cellId: cell.id, source: cell.source }));
  const mediaDigests = outputMediaDigests(state, facts);
  const rootModule = state.manifest.plan.kind === "single-file" ? state.manifest.plan.modules[0] : null;
  const sourceDigestValue = rootModule?.digest ?? null;
  const audit = {
    lockDigest: state.manifest.lock.digest,
    contextDigest: state.manifest.context.digest,
    toolchainDigest: state.manifest.toolchainDigest,
    target: state.manifest.target,
    sessionId: facts.codeSelected.length > 0 ? facts.byCell.get(facts.codeSelected[0]).sessionId : null,
    incarnation: facts.codeSelected.length > 0 ? facts.byCell.get(facts.codeSelected[0]).incarnation : null,
    planDigest: state.manifest.plan.contentDigest,
    contentDigest: state.manifest.plan.contentDigest,
    sourceDigest: sourceDigestValue,
    plan: {
      kind: state.manifest.plan.kind,
      entry: state.manifest.plan.entry,
      defaultEntry: state.manifest.plan.defaultEntry ?? null,
      roles: state.manifest.plan.roles ?? [],
      modules: modules.map(({ id, role, sourceCells, digest }) => ({ id, role, sourceCells, digest })),
    },
    moduleDigests: state.manifest.plan.modules.map((module) => ({ id: module.id, digest: module.digest })),
    receiptOrder: state.order.filter((cellId) => facts.codeSelected.includes(cellId)).map((cellId) => {
      const receipt = facts.byCell.get(cellId);
      return { cellId, ordinal: receipt.ordinal, sourceDigest: receipt.sourceDigest, sessionId: receipt.sessionId, incarnation: receipt.incarnation, generationBefore: receipt.generationBefore, generationAfter: receipt.generationAfter, outcome: receipt.outcome, effectDigest: receipt.effectDigest, bindings: receipt.bindingRecords, edges: (receipt.hardEdges ?? []).map((edge) => ({ bindingId: edge.bindingId, version: edge.version, kind: edge.kind, cellId: edge.cellId, incarnation: edge.incarnation, generationId: edge.generationId })), effects: receipt.effectOutcomes, providers: receipt.providerOutcomes, inputs: receipt.inputs, resources: receipt.resourceStates };
    }),
    outputMediaDigests: mediaDigests,
  };
  const auditDigest = notebookDigest("audit", audit);
  boundedJsonBytes(audit, state.limits.auditBytes, "audit");
  state.audit = auditDigest;
  state.output = { kind: state.manifest.plan.kind, source, modules, companion, contentDigest: state.manifest.plan.contentDigest, sourceDigest: sourceDigestValue, auditDigest, outputMediaDigests: mediaDigests, executed: false, hiddenReplay: false };
  state.phase = "exported";
}

export function prepareNotebookExportOperations(operations = []) {
  let cells = [];
  let selected = null;
  return operations.map((operation) => {
    if (operation?.op === "load") { cells = clone(operation.cells ?? []); return operation; }
    if (operation?.op === "select") { selected = [...(operation.cellIds ?? [])]; return operation; }
    if (operation?.op !== "manifest" || !operation.value) return operation;
    const value = clone(operation.value);
    const code = cells.filter((cell) => cell.cell_type === "code" && (selected === null || selected.includes(cell.id)));
    const codeById = new Map(code.map((cell) => [cell.id, cell]));
    for (const receipt of value.receipts ?? []) {
      const cell = codeById.get(receipt.cellId);
      if (receipt.sourceDigest === "auto" && cell) receipt.sourceDigest = sourceDigest(cell.source);
    }
    if (value.plan && Array.isArray(value.plan.modules)) {
      for (const module of value.plan.modules) {
        if (module.digest === "auto" && Array.isArray(module.sourceCells) && module.sourceCells.every((cellId) => codeById.has(cellId))) {
          module.digest = sourceDigest(module.sourceCells.map((cellId) => codeById.get(cellId).source).join("\n"));
        }
      }
      if (value.plan.contentDigest === "auto" && value.plan.modules.every((module) => DIGEST.test(module.digest ?? ""))) value.plan.contentDigest = planDigest(value);
    }
    return { ...operation, value };
  });
}

function runNotebookExportSemanticProgram(operations = []) {
  const state = initialState();
  try {
    for (const operation of operations) {
      switch (operation?.op) {
        case "load":
          if (state.phase !== "empty") fail("W-EXPORT-0001", { reason: "notebook already loaded" });
          state.limits = { ...defaultLimits(), ...(operation.limits ?? {}) };
          state.cells = normaliseCells(operation.cells, state.limits, operation.rawPolicy ?? "exclude");
          state.notebook = { nbformat: operation.nbformat, nbformat_minor: operation.nbformat_minor, trust: operation.trust === true };
          if (operation.nbformat !== 4 || !Number.isInteger(operation.nbformat_minor) || operation.nbformat_minor < 0) fail("W-EXPORT-0001", { reason: "nbformat 4 document required" });
          state.phase = "loaded";
          break;
        case "manifest":
          requireLoaded(state);
          state.manifest = clone(operation.value);
          if (state.pendingInvalidation.length > 0) state.manifest.invalidation = { ...(state.manifest.invalidation ?? {}), cells: [...(state.manifest.invalidation?.cells ?? []), ...state.pendingInvalidation] };
          if (state.pendingRedefinition.length > 0) state.manifest.redefinitions = [...(state.manifest.redefinitions ?? []), ...state.pendingRedefinition];
          break;
        case "select":
          requireLoaded(state);
          {
            const cellsById = new Map(state.cells.map((cell) => [cell.id, cell]));
            if (!Array.isArray(operation.cellIds) || new Set(operation.cellIds).size !== operation.cellIds.length || operation.cellIds.some((cellId) => !cellsById.has(cellId) || cellsById.get(cellId).cell_type === "raw")) fail("W-EXPORT-0001", { reason: "selection contains duplicate, unknown, or raw cell" });
          }
          state.selected = [...operation.cellIds];
          break;
        case "invalidate":
          requireLoaded(state);
          if (state.manifest) state.manifest.invalidation = { ...(state.manifest.invalidation ?? {}), cells: [...(state.manifest.invalidation?.cells ?? []), operation.cellId] };
          else state.pendingInvalidation.push(operation.cellId);
          break;
        case "redefine":
          requireLoaded(state);
          const fact = { bindingId: operation.bindingId, previous: operation.previous, next: operation.next };
          if (state.manifest) state.manifest.redefinitions = [...(state.manifest.redefinitions ?? []), fact];
          else state.pendingRedefinition.push(fact);
          break;
        case "validate":
          const facts = validateManifest(state);
          dependencyOrder(state, facts);
          validatePlan(state, facts);
          state.phase = "validated";
          break;
        case "export":
          exportNotebook(state, operation);
          break;
        case "replay":
          fail("W-EXPORT-0007", { reason: "hidden replay attempted" });
        default:
          fail("invalidOperation", { reason: "unknown operation", operation: operation?.op });
      }
    }
    return { status: "accepted", state: clone(state) };
  } catch (error) {
    if (!(error instanceof NotebookExportError)) throw error;
    state.phase = "rejected";
    state.diagnostics.push({ code: error.code, facts: clone(error.details) });
    return { status: "rejected", error: { code: error.code, details: clone(error.details) }, state: clone(state) };
  }
}

export function runNotebookExportProgram(operations = []) {
  return runNotebookExportSemanticProgram(operations);
}

export function runNotebookExportFixtureProgram(operations = []) {
  return runNotebookExportSemanticProgram(prepareNotebookExportOperations(operations));
}

export function compactNotebookExportState(state) {
  return {
    phase: state.phase,
    notebook: state.notebook,
    cells: state.cells.map((cell) => ({ id: cell.id, cell_type: cell.cell_type, source: cell.source })),
    selected: state.selected,
    order: state.order,
    output: state.output ? { kind: state.output.kind, source: state.output.source, modules: state.output.modules?.map((module) => ({ id: module.id, role: module.role, sourceCells: module.sourceCells, digest: module.digest })) ?? [], companionCellIds: state.output.companion?.map((entry) => entry.cellId) ?? [], auditDigest: state.output.auditDigest, contentDigest: state.output.contentDigest, sourceDigest: state.output.sourceDigest, outputMediaDigests: state.output.outputMediaDigests, executed: state.output.executed, hiddenReplay: state.output.hiddenReplay } : null,
    audit: state.audit,
    executions: state.executions,
    diagnostics: state.diagnostics,
  };
}
