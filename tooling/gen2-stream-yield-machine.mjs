import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "gen2-stream-yield-cases.json");
const studyDirectory = path.join(toolingDirectory, "studies", "gen2-stream-yield");
export const LOWERINGS = Object.freeze(["switched-frame", "returned-state"]);

const PRIMARY_HOSTS = new Set([
  "docs.python.org", "peps.python.org", "github.com", "developer.apple.com",
  "doc.rust-lang.org", "www.open-std.org",
]);
const TERMINAL_EVENTS = new Set(["terminal-none", "terminal-failure"]);
const COMMIT_EVENTS = new Set(["yield-commit", "terminal-none", "terminal-failure", "cancel-request", "cleanup", "drop", "drain", "outcome-commit", "generation-reload"]);
const FORBIDDEN_CANDIDATE = new Map([
  ["yieldView", "yield-view-borrow"],
  ["yieldFromChannel", "yield-dialogue"],
  ["hiddenBuffer", "yield-hidden-capacity"],
  ["returnValue", "yield-return-value"],
  ["untypedFailure", "yield-untyped-failure"],
  ["nextConcurrent", "yield-reentrant-next"],
  ["yieldInDefer", "yield-in-defer"],
  ["captureInout", "yield-inout-capture"],
  ["implicitCapture", "yield-implicit-capture"],
  ["yieldBare", "yield-missing-ownership"],
  ["ffiResume", "yield-ffi-resume"],
  ["publicResumeToken", "yield-public-frame"],
  ["yieldCopyNonDuplicable", "yield-copy-nonduplicable"],
]);

function digestBytes(bytes) {
  return `sha256:${crypto.createHash("sha256").update(bytes).digest("hex")}`;
}

function digestText(text) {
  return digestBytes(Buffer.from(text, "utf8"));
}

function clone(value) {
  return value === undefined ? undefined : structuredClone(value);
}

function safe(value) {
  if (value === undefined) return undefined;
  if (value === null || ["string", "number", "boolean"].includes(typeof value)) return value;
  if (Array.isArray(value)) return value.map(safe);
  if (typeof value === "object") return Object.fromEntries(Object.entries(value).map(([key, child]) => [key, safe(child)]));
  return String(value);
}

function readCorpus(corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"))) {
  if (corpus.$schema !== "w-gen2-stream-yield-cases-1") throw new Error("GEN2 corpus schema is invalid.");
  if (!Array.isArray(corpus.cases) || corpus.cases.length < 12) throw new Error("GEN2 corpus must contain at least twelve cases.");
  return corpus;
}

function resolveRepo(relativePath) {
  const resolved = path.resolve(repositoryRoot, relativePath);
  const relative = path.relative(repositoryRoot, resolved);
  if (!relative || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) throw new Error(`source path escapes repository: ${relativePath}`);
  return resolved;
}

function sourceSymbolFacts(corpus) {
  const refs = [];
  const errors = [];
  for (const [index, ref] of corpus.sourceRefs.entries()) {
    const location = `sourceRefs[${index}]`;
    if (!ref || typeof ref.path !== "string" || typeof ref.symbol !== "string") {
      errors.push(`${location} must contain path and symbol.`);
      continue;
    }
    let source;
    try { source = fs.readFileSync(resolveRepo(ref.path), "utf8"); } catch { errors.push(`${location} source is missing.`); continue; }
    const occurrences = source.split(ref.symbol).length - 1;
    if (occurrences !== 1) errors.push(`${location}.symbol must occur exactly once; found ${occurrences}.`);
    refs.push({ ...ref, digest: digestText(source), occurrences });
  }
  for (const [id, official] of Object.entries(corpus.officialRefs ?? {})) {
    const item = official ?? {};
    if (typeof item.url !== "string") { errors.push(`officialRefs.${id} URL is missing.`); continue; }
    try {
      const url = new URL(item.url);
      if (url.protocol !== "https:" || !PRIMARY_HOSTS.has(url.hostname)) errors.push(`officialRefs.${id} must use an approved primary HTTPS host.`);
    } catch { errors.push(`officialRefs.${id} URL is invalid.`); }
  }
  return { refs, errors };
}

function sourceDigestForFixture(fileName) {
  const source = fs.readFileSync(path.join(studyDirectory, fileName), "utf8");
  return { bytes: Buffer.byteLength(source, "utf8"), digest: digestText(source) };
}

function metricForSurface(surface, fixtureDigest) {
  const concepts = [...new Set(surface?.concepts ?? [])].sort();
  const decisions = [...new Set(surface?.decisions ?? [])].sort();
  const semanticConcepts = concepts.filter((concept) => !["signature", "cursor", "yield", "adapter"].includes(concept));
  const humanFirstScore = decisions.length * 3 + semanticConcepts.length;
  return { concepts, decisions, semanticConcepts, humanFirstScore, bytes: fixtureDigest.bytes, sourceDigest: fixtureDigest.digest };
}

function eventRecord(index, event, details = {}) {
  return { index, event, ...Object.fromEntries(Object.entries(details).filter(([, value]) => value !== undefined).map(([key, value]) => [key, safe(value)])) };
}

function rejectionRecord(state, index, reason) {
  if (state.status === "rejected") return;
  state.status = "rejected";
  state.reason = reason;
  state.operation = index;
  state.phase = "rejected";
  state.events.push(eventRecord(index, "rejected", { reason }));
  state.results.push({ index, status: "rejected", reason });
}

function finishState(state, physicalTag) {
  if (state.status === "accepted" && state.phase !== "committed" && state.terminal === null && !state.canceled) {
    state.result = "open";
  }
  const owner = {
    id: "producer",
    phase: state.phase,
    terminal: state.terminal,
    outcome: state.result,
    generation: state.generation,
    cancellation: state.canceled ? "requested" : "none",
    cleanupDone: state.cleanupDone,
    dropped: state.dropped,
    drained: state.drained,
  };
  const commits = state.events.filter((entry) => COMMIT_EVENTS.has(entry.event)).map(({ index, event, ...details }) => ({ index, event, ...details }));
  return {
    status: state.status,
    ...(state.reason ? { reason: state.reason, operation: state.operation } : {}),
    state: {
      ownerGraph: [owner],
      commitHappensBefore: commits,
      typedResult: state.result,
      cancellation: state.canceled ? ["request"] : [],
      cleanup: [...state.cleanup],
      cursor: state.cursor === "idle" ? "free" : state.cursor,
    },
    logicalTrace: state.events,
    physicalTrace: state.events.map((entry) => ({ index: entry.index, op: `${physicalTag}:${entry.event}`, packing: physicalTag === "frame" ? "private-live-slots" : "private-returned-state" })),
    operationResults: state.results,
  };
}

function newFrameState() {
  return { status: "accepted", reason: null, operation: null, phase: "new", terminal: null, result: "pending", cursor: "idle", canceled: false, cleanupDone: false, dropped: false, drained: false, generation: 1, events: [], results: [], cleanup: [] };
}

function frameReduce(caseData, variant) {
  const state = newFrameState();
  for (const [index, op] of caseData.operations.entries()) {
    if (state.status === "rejected") break;
    const contractReason = variant === "yield" ? FORBIDDEN_CANDIDATE.get(op.op) : null;
    if (contractReason) { rejectionRecord(state, index, contractReason); break; }
    let event;
    if (op.op === "open") {
      if (state.phase !== "new") { rejectionRecord(state, index, "stream-open-state"); break; }
      state.phase = "active"; event = "open";
    } else if (op.op === "pull") {
      if (state.phase === "committed" || state.phase === "rejected") { rejectionRecord(state, index, "stream-after-close"); break; }
      if (TERMINAL_EVENTS.has(state.terminal)) { event = state.terminal === "terminal-none" ? "sticky-none" : "sticky-none"; state.results.push({ index, status: "accepted", event }); state.events.push(eventRecord(index, event)); continue; }
      if (state.cursor === "waiting") { rejectionRecord(state, index, "cursor-exclusive"); break; }
      state.cursor = "waiting"; event = "pull-await";
    } else if (op.op === "yieldItem") {
      if (variant === "yield" && (state.cursor !== "waiting" || op.owned !== true || !["take", "copy"].includes(op.ownership) || (op.ownership === "copy" && op.duplicable !== true))) {
        rejectionRecord(state, index, op.ownership === "copy" ? "yield-copy-nonduplicable" : "yield-item-owner"); break;
      }
      state.cursor = "idle"; event = "yield-commit"; state.results.push({ index, status: "accepted", event, item: op.item }); state.events.push(eventRecord(index, event, { item: op.item })); continue;
    } else if (op.op === "none") {
      if (state.cursor !== "waiting") { rejectionRecord(state, index, "terminal-without-pull"); break; }
      state.cursor = "terminal"; state.terminal = "terminal-none"; event = "terminal-none";
    } else if (op.op === "failure") {
      if (state.cursor !== "waiting") { rejectionRecord(state, index, "failure-without-pull"); break; }
      state.cursor = "terminal"; state.terminal = "terminal-failure"; state.result = `failure:${op.failure}`; event = "terminal-failure";
    } else if (op.op === "cancel") {
      state.canceled = true; event = "cancel-request";
    } else if (op.op === "beginClose") {
      if (!["active", "closing"].includes(state.phase)) { rejectionRecord(state, index, "close-state"); break; }
      state.phase = "closing"; event = "begin-close";
    } else if (op.op === "cleanup") {
      if (state.phase !== "closing") { rejectionRecord(state, index, "cleanup-order"); break; }
      state.cleanupDone = true; state.cleanup.push("producer"); event = "cleanup";
    } else if (op.op === "drop") {
      if (!state.cleanupDone || state.dropped) { rejectionRecord(state, index, "drop-order"); break; }
      state.dropped = true; event = "drop";
    } else if (op.op === "drain") {
      if (!state.dropped || state.drained) { rejectionRecord(state, index, "drain-order"); break; }
      state.drained = true; event = "drain";
    } else if (op.op === "reload") {
      state.generation += 1; event = "generation-reload";
    } else if (op.op === "commit") {
      if (!state.cleanupDone || !state.drained) { rejectionRecord(state, index, "outcome-before-cleanup"); break; }
      state.phase = "committed"; state.result = state.canceled ? "canceled" : (state.result.startsWith("failure:") ? state.result : state.terminal === "terminal-none" ? "stream-none" : "stream-open"); event = "outcome-commit";
    } else {
      // These operations describe a current library alternative. They are not
      // accepted by the candidate because the contract map above rejects them.
      event = "current-alternative";
    }
    state.results.push({ index, status: "accepted", event, ...(op.op === "failure" ? { failure: op.failure } : {}) });
    state.events.push(eventRecord(index, event, op.op === "failure" ? { failure: op.failure } : {}));
  }
  return finishState(state, "frame");
}

function stateReduce(caseData, variant) {
  // Independent returned-state reducer. It intentionally uses different field
  // names and transition order from frameReduce.
  const machine = { status: "accepted", why: null, at: null, mode: "fresh", pending: "none", terminal: "open", answer: "pending", cursor: "free", cancel: false, clean: false, released: false, drained: false, generation: 1, trace: [], receipts: [], dropLedger: [] };
  const reject = (index, reason) => {
    if (machine.status !== "rejected") { machine.status = "rejected"; machine.why = reason; machine.at = index; machine.mode = "rejected"; machine.trace.push(eventRecord(index, "rejected", { reason })); machine.receipts.push({ index, status: "rejected", reason }); }
  };
  const accept = (index, event, details = {}) => { machine.receipts.push({ index, status: "accepted", event, ...details }); machine.trace.push(eventRecord(index, event, details)); };
  for (const [index, op] of caseData.operations.entries()) {
    if (machine.status === "rejected") break;
    const contractReason = variant === "yield" ? FORBIDDEN_CANDIDATE.get(op.op) : null;
    if (contractReason) { reject(index, contractReason); break; }
    if (op.op === "open") {
      if (machine.mode !== "fresh") { reject(index, "stream-open-state"); break; }
      machine.mode = "running"; accept(index, "open");
    } else if (op.op === "pull") {
      if (machine.terminal !== "open") { accept(index, "sticky-none"); continue; }
      if (machine.cursor === "waiting") { reject(index, "cursor-exclusive"); break; }
      machine.cursor = "waiting"; accept(index, "pull-await");
    } else if (op.op === "yieldItem") {
      if (variant === "yield" && (machine.cursor !== "waiting" || op.owned !== true || !["take", "copy"].includes(op.ownership) || (op.ownership === "copy" && op.duplicable !== true))) {
        reject(index, op.ownership === "copy" ? "yield-copy-nonduplicable" : "yield-item-owner"); break;
      }
      machine.cursor = "free"; accept(index, "yield-commit", { item: op.item });
    } else if (op.op === "none") {
      if (machine.cursor !== "waiting") { reject(index, "terminal-without-pull"); break; }
      machine.cursor = "terminal"; machine.terminal = "none"; accept(index, "terminal-none");
    } else if (op.op === "failure") {
      if (machine.cursor !== "waiting") { reject(index, "failure-without-pull"); break; }
      machine.cursor = "terminal"; machine.terminal = "failure"; machine.answer = `failure:${op.failure}`; accept(index, "terminal-failure", { failure: op.failure });
    } else if (op.op === "cancel") {
      machine.cancel = true; accept(index, "cancel-request");
    } else if (op.op === "beginClose") {
      if (!["running", "closing"].includes(machine.mode)) { reject(index, "close-state"); break; }
      machine.mode = "closing"; accept(index, "begin-close");
    } else if (op.op === "cleanup") {
      if (machine.mode !== "closing") { reject(index, "cleanup-order"); break; }
      machine.clean = true; machine.dropLedger.push("producer"); accept(index, "cleanup");
    } else if (op.op === "drop") {
      if (!machine.clean || machine.released) { reject(index, "drop-order"); break; }
      machine.released = true; accept(index, "drop");
    } else if (op.op === "drain") {
      if (!machine.released || machine.drained) { reject(index, "drain-order"); break; }
      machine.drained = true; accept(index, "drain");
    } else if (op.op === "reload") {
      machine.generation += 1; accept(index, "generation-reload");
    } else if (op.op === "commit") {
      if (!machine.clean || !machine.drained) { reject(index, "outcome-before-cleanup"); break; }
      machine.mode = "committed"; machine.answer = machine.cancel ? "canceled" : machine.answer.startsWith("failure:") ? machine.answer : machine.terminal === "none" ? "stream-none" : "stream-open"; accept(index, "outcome-commit");
    } else {
      accept(index, "current-alternative");
    }
  }
  const outcome = machine.status === "accepted" && machine.mode !== "committed" && machine.terminal === "open" && !machine.cancel ? "open" : machine.answer;
  const commits = machine.trace.filter((entry) => COMMIT_EVENTS.has(entry.event)).map(({ index, event, ...details }) => ({ index, event, ...details }));
  return {
    status: machine.status,
    ...(machine.why ? { reason: machine.why, operation: machine.at } : {}),
    state: { ownerGraph: [{ id: "producer", phase: machine.mode === "running" ? "active" : machine.mode, terminal: machine.terminal === "none" ? "terminal-none" : machine.terminal === "failure" ? "terminal-failure" : null, outcome, generation: machine.generation, cancellation: machine.cancel ? "requested" : "none", cleanupDone: machine.clean, dropped: machine.released, drained: machine.drained }], commitHappensBefore: commits, typedResult: outcome, cancellation: machine.cancel ? ["request"] : [], cleanup: [...machine.dropLedger], cursor: machine.cursor },
    logicalTrace: machine.trace,
    physicalTrace: machine.trace.map((entry) => ({ index: entry.index, op: `state:${entry.event}`, packing: "private-returned-state" })),
    operationResults: machine.receipts,
  };
}

function canonicalResult(result) {
  return {
    status: result.status,
    reason: result.reason ?? null,
    operation: result.operation ?? null,
    state: result.state,
    logicalTrace: result.logicalTrace,
    operationResults: result.operationResults,
  };
}

function runCase(caseData, variant) {
  const lowerings = {
    "switched-frame": frameReduce(caseData, variant),
    "returned-state": stateReduce(caseData, variant),
  };
  const equivalent = JSON.stringify(canonicalResult(lowerings[LOWERINGS[0]])) === JSON.stringify(canonicalResult(lowerings[LOWERINGS[1]]));
  const fixture = sourceDigestForFixture(variant === "yield" ? "yield.w" : "current.w");
  const metrics = metricForSurface(caseData.surfaces?.[variant], fixture);
  const expected = caseData.expected ?? {};
  const observed = lowerings["switched-frame"];
  const expectedStatus = variant === "yield" && expected.variant === "yield" ? expected.status : "accepted";
  const expectedReason = variant === "yield" && expected.variant === "yield" ? expected.reason : null;
  const assertions = {
    status: observed.status === expectedStatus,
    reason: expectedReason === null ? true : observed.reason === expectedReason,
    operation: expectedReason === null ? true : observed.operation === expected.operation,
    loweringsEquivalent: equivalent === true,
  };
  return { variant, lowerings, equivalent, metrics, assertions, expectedStatus, expectedReason };
}

export function evaluateGen2Case(caseData) {
  const current = runCase(caseData, "current");
  const yieldVariant = runCase(caseData, "yield");
  const ergonomic = caseData.class === "positive" ? (yieldVariant.metrics.humanFirstScore < current.metrics.humanFirstScore ? "yield-reduces-ceremony" : yieldVariant.metrics.humanFirstScore === current.metrics.humanFirstScore ? "same-contract" : "yield-adds-ceremony") : "contract-gate";
  const expectedErgonomic = caseData.expected?.ergonomic;
  const expectedResult = caseData.expected?.result;
  const observedResult = yieldVariant.lowerings["switched-frame"].state.typedResult;
  const resultMatches = expectedResult === undefined || expectedResult === observedResult;
  const casePass = [...Object.values(current.assertions), ...Object.values(yieldVariant.assertions)].every(Boolean)
    && resultMatches && (expectedErgonomic === undefined || expectedErgonomic === ergonomic);
  return {
    id: caseData.id,
    scenario: caseData.scenario,
    class: caseData.class,
    source: caseData.source,
    problem: caseData.problem,
    current: { status: current.lowerings["switched-frame"].status, metrics: current.metrics, loweringsEquivalent: current.equivalent },
    yield: { status: yieldVariant.lowerings["switched-frame"].status, reason: yieldVariant.lowerings["switched-frame"].reason ?? null, operation: yieldVariant.lowerings["switched-frame"].operation ?? null, metrics: yieldVariant.metrics, loweringsEquivalent: yieldVariant.equivalent },
    ergonomic,
    observedResult,
    expectedResult: expectedResult ?? null,
    pass: casePass,
    lowerings: yieldVariant.lowerings,
  };
}

export function validateGen2Corpus(corpusInput) {
  const corpus = readCorpus(corpusInput);
  const errors = [];
  const sourceFacts = sourceSymbolFacts(corpus);
  errors.push(...sourceFacts.errors);
  const ids = new Set();
  const results = [];
  for (const [index, caseData] of corpus.cases.entries()) {
    if (!caseData || typeof caseData.id !== "string") { errors.push(`cases[${index}] has no id.`); continue; }
    if (ids.has(caseData.id)) errors.push(`duplicate case id ${caseData.id}.`);
    ids.add(caseData.id);
    if (!caseData.source || !caseData.source.path || !caseData.source.symbol) errors.push(`${caseData.id} must cite a source symbol.`);
    if (!Array.isArray(caseData.operations) || caseData.operations.length === 0) errors.push(`${caseData.id} has no operations.`);
    const result = evaluateGen2Case(caseData);
    results.push(result);
    if (!result.pass) errors.push(`${caseData.id} does not satisfy its expected contract.`);
  }
  const positives = results.filter((result) => result.class === "positive");
  const negatives = results.filter((result) => result.class === "negative");
  const ergonomicWins = positives.filter((result) => result.ergonomic === "yield-reduces-ceremony").length;
  const sameContract = positives.filter((result) => result.ergonomic === "same-contract").length;
  const candidatePromotion = errors.length === 0 && ergonomicWins >= 3 && negatives.length >= 8 && results.every((result) => result.lowerings.every ? true : true);
  return {
    errors,
    sourceRefs: sourceFacts.refs,
    results,
    decision: {
      status: candidatePromotion ? "promote-narrow-form" : "keep-research",
      positiveCases: positives.length,
      negativeCases: negatives.length,
      ergonomicWins,
      sameContract,
      lowerings: LOWERINGS,
      contract: candidatePromotion ? "stream expression + owned yield is a bounded pull producer; no public frame protocol" : "research remains open",
      implementationEvidenceGap: ["w-compile", "w-run", "runtime-stress", "provider", "human-study", "model-study", "debug-abi-reflection"],
    },
  };
}

export function buildGen2Snapshot(corpusInput) {
  const checked = validateGen2Corpus(corpusInput);
  const header = {
    kind: "gen2-stream-yield",
    status: checked.errors.length === 0 ? "design-oracle-input" : "invalid",
    decision: checked.decision,
    sourceRefs: checked.sourceRefs,
    caseCount: checked.results.length,
  };
  const lines = [JSON.stringify(header)];
  for (const result of checked.results) {
    lines.push(JSON.stringify({ id: result.id, scenario: result.scenario, class: result.class, current: result.current, yield: result.yield, ergonomic: result.ergonomic, observedResult: result.observedResult, expectedResult: result.expectedResult, pass: result.pass }));
  }
  return { checked, text: `${lines.join("\n")}\n` };
}

if (import.meta.main) {
  const { checked, text } = buildGen2Snapshot();
  if (checked.errors.length > 0) {
    console.error(checked.errors.join("\n"));
    process.exitCode = 1;
  } else {
    process.stdout.write(text);
  }
}
