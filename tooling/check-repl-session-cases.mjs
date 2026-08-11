import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  runReplSessionProgram,
  validateReplSessionOperation,
} from "./repl-session-machine.mjs";
import { ledgerIdSet } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const rootDirectory = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "repl-session-cases.json");
const snapshotPath = path.join(toolingDirectory, "repl-session-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const errors = [];
const caseIds = new Set();
const results = [];
let operationCount = 0;
const coveredOperations = new Set();

function getPath(value, dotted) {
  return dotted.split(".").reduce((current, key) => current?.[key], value);
}

function currentBinding(state, name) {
  return state.generation.bindings
    .filter((binding) => binding.name === name && binding.availability === "available")
    .sort((left, right) => right.version - left.version)[0] ?? null;
}

function metadataOnly(value) {
  if (Array.isArray(value)) return value.map(metadataOnly);
  if (value && typeof value === "object") {
    return Object.fromEntries(Object.entries(value).filter(([key]) => key !== "value" && key !== "liveValue").map(([key, entry]) => [key, metadataOnly(entry)]));
  }
  return value;
}

function resolveReference(reference, location) {
  if (!reference || typeof reference !== "object") {
    errors.push(`${location} must be an object.`);
    return;
  }
  if (typeof reference.path !== "string" || !reference.path.startsWith("reference/last-light/")) {
    errors.push(`${location}.path must point into reference/last-light.`);
    return;
  }
  const target = path.join(rootDirectory, reference.path);
  if (!fs.existsSync(target)) {
    errors.push(`${location}.path does not exist: ${reference.path}.`);
    return;
  }
  if (typeof reference.symbol !== "string" || reference.symbol.length === 0) {
    errors.push(`${location}.symbol is required.`);
    return;
  }
  const source = fs.readFileSync(target, "utf8");
  if (!new RegExp(`\\b${reference.symbol.replace(/[.*+?^${}()|[\\]\\]/g, "\\$&")}\\b`).test(source)) {
    errors.push(`${location}.symbol is absent from ${reference.path}.`);
  }
}

function compactState(state) {
  return {
    phase: state.phase,
    session: {
      sessionId: state.session?.sessionId ?? null,
      incarnation: state.session?.incarnation ?? null,
      executionOrdinal: state.session?.executionOrdinal ?? null,
      prompt: state.session ? `w[${state.session.executionOrdinal}]` : null,
      context: state.session?.context ?? null,
      lockDigest: state.session?.lockDigest ?? null,
      capabilityDigest: state.session?.capabilityDigest ?? null,
      frontend: state.session?.frontend ?? null,
    },
    generation: state.generation
      ? {
          id: state.generation.id,
          incarnation: state.generation.incarnation,
          display: state.generation.display,
          number: state.generation.number,
          status: state.generation.status,
          graphFingerprint: state.generation.graphFingerprint,
          bindings: state.generation.bindings.map(metadataOnly),
          scope: state.generation.scope,
        }
      : null,
    generationHistory: state.generationHistory.map((generation) => ({ id: generation.id, display: generation.display, status: generation.status })),
    staged: state.staged ? { requestId: state.staged.requestId, ordinal: state.staged.ordinal, generation: { id: state.staged.generation.id, display: state.staged.generation.display } } : null,
    ownerScopes: state.ownerScopes,
    admission: { writer: state.admission.writer, active: state.admission.active ? { requestId: state.admission.active.requestId, stage: state.admission.active.stage, cancelRequested: state.admission.active.cancelRequested } : null, queue: state.admission.queue, tickets: state.admission.tickets },
    effects: state.effects,
    outputs: state.outputs,
    receipts: state.receipts,
    lastReceipt: state.lastReceipt,
    history: state.history,
    invalidation: state.invalidation,
    cancellations: state.cancellations,
    reads: state.reads,
    mutationBlocked: state.mutationBlocked,
    lastClassification: state.lastClassification,
    lastResult: metadataOnly(state.lastResult),
    buffer: state.buffer,
    historyPolicy: state.historyPolicy,
  };
}

function assertCase(state, assertions, location) {
  if (!assertions) return;
  const failAssertion = (message) => errors.push(`${location}: ${message}`);
  if (assertions.ordinal !== undefined && state.session.executionOrdinal !== assertions.ordinal) failAssertion(`ordinal expected ${assertions.ordinal}, got ${state.session.executionOrdinal}.`);
  if (assertions.incarnation !== undefined && state.session.incarnation !== assertions.incarnation) failAssertion(`incarnation expected ${assertions.incarnation}, got ${state.session.incarnation}.`);
  if (assertions.generationDisplay !== undefined && state.generation?.display !== assertions.generationDisplay) failAssertion(`generation expected ${assertions.generationDisplay}, got ${state.generation?.display}.`);
  if (assertions.generationNumber !== undefined && state.generation?.number !== assertions.generationNumber) failAssertion(`generation number expected ${assertions.generationNumber}, got ${state.generation?.number}.`);
  if (assertions.prompt !== undefined && `w[${state.session.executionOrdinal}]` !== assertions.prompt) failAssertion(`prompt expected ${assertions.prompt}.`);
  if (assertions.mutationBlocked !== undefined && state.mutationBlocked !== assertions.mutationBlocked) failAssertion(`mutationBlocked expected ${assertions.mutationBlocked}.`);
  if (assertions.historyCount !== undefined && state.history.records.length !== assertions.historyCount) failAssertion(`history count expected ${assertions.historyCount}, got ${state.history.records.length}.`);
  if (assertions.historyCountAtMost !== undefined && state.history.records.length > assertions.historyCountAtMost) failAssertion(`history count exceeds ${assertions.historyCountAtMost}.`);
  if (assertions.historyBytesAtMost !== undefined && state.history.bytes > assertions.historyBytesAtMost) failAssertion(`history bytes exceeds ${assertions.historyBytesAtMost}.`);
  if (assertions.historyEvictedAtLeast !== undefined && state.history.evicted < assertions.historyEvictedAtLeast) failAssertion(`history evictions below ${assertions.historyEvictedAtLeast}.`);
  if (assertions.reads !== undefined && state.reads !== assertions.reads) failAssertion(`read count expected ${assertions.reads}, got ${state.reads}.`);
  if (assertions.generationHistoryLength !== undefined && state.generationHistory.length !== assertions.generationHistoryLength) failAssertion(`generation history length expected ${assertions.generationHistoryLength}.`);
  if (assertions.lastOutcome !== undefined && state.lastReceipt?.outcome !== assertions.lastOutcome) failAssertion(`last outcome expected ${assertions.lastOutcome}, got ${state.lastReceipt?.outcome}.`);
  if (assertions.receiptOutcomes !== undefined) {
    const outcomes = state.receipts.map((receipt) => receipt.outcome);
    if (JSON.stringify(outcomes) !== JSON.stringify(assertions.receiptOutcomes)) failAssertion(`receipt outcomes expected ${JSON.stringify(assertions.receiptOutcomes)}, got ${JSON.stringify(outcomes)}.`);
  }
  if (assertions.bindingValues) {
    for (const [name, expected] of Object.entries(assertions.bindingValues)) {
      const binding = currentBinding(state, name);
      if ((binding?.value ?? null) !== expected) failAssertion(`binding ${name} expected value ${JSON.stringify(expected)}, got ${JSON.stringify(binding?.value ?? null)}.`);
    }
  }
  if (assertions.bindingVersions) {
    for (const [name, expected] of Object.entries(assertions.bindingVersions)) {
      const versions = state.generation.bindings.filter((binding) => binding.name === name).sort((left, right) => left.version - right.version);
      const current = versions.find((binding) => binding.availability === "available") ?? versions.at(-1);
      if (!current || current.version !== expected.version) failAssertion(`binding ${name} version expected ${expected.version}, got ${current?.version}.`);
      if (expected.createdGeneration !== undefined && current.createdGeneration !== expected.createdGeneration) failAssertion(`binding ${name} created generation mismatch.`);
      if (expected.distinctFromPrevious && versions.length > 1 && versions.at(-1).createdGenerationId === versions.at(-2).createdGenerationId) failAssertion(`binding ${name} storage version did not change creation identity.`);
    }
  }
  if (assertions.newBindingGenerationMatchesReceipt) {
    const receiptGeneration = state.lastReceipt?.generationFinal?.id;
    const created = state.generation.bindings.filter((binding) => binding.createdGenerationId === receiptGeneration);
    if (!receiptGeneration || created.length === 0 || created.some((binding) => binding.createdGenerationId !== receiptGeneration)) failAssertion("published binding creation identity does not equal receipt final generation.");
  }
  if (assertions.hardDependency) {
    const binding = currentBinding(state, assertions.hardDependency.name);
    const edge = binding?.hardDependencies?.find((candidate) => candidate.name === assertions.hardDependency.dependency);
    if (!edge || edge.kind !== assertions.hardDependency.kind) failAssertion(`hard dependency ${assertions.hardDependency.name} -> ${assertions.hardDependency.dependency} is missing.`);
  }
  if (assertions.committedBindingNames !== undefined) {
    const names = state.generation.bindings.filter((binding) => binding.availability === "available").map((binding) => binding.name).sort();
    if (JSON.stringify(names) !== JSON.stringify([...assertions.committedBindingNames].sort())) failAssertion(`committed names expected ${JSON.stringify(assertions.committedBindingNames)}, got ${JSON.stringify(names)}.`);
  }
  if (assertions.unavailable !== undefined) {
    for (const name of assertions.unavailable) {
      if (!state.generation.bindings.some((binding) => binding.name === name && binding.availability === "invalidated")) failAssertion(`binding ${name} is not invalidated.`);
    }
  }
  if (assertions.invalidation) {
    const entry = state.invalidation.find((candidate) => candidate.name === assertions.invalidation.name);
    if (!entry) failAssertion(`invalidation for ${assertions.invalidation.name} is missing.`);
    else if (entry.reason !== assertions.invalidation.reason) failAssertion(`invalidation reason expected ${assertions.invalidation.reason}, got ${entry.reason}.`);
  }
  if (assertions.invalidationEdges) {
    for (const expected of assertions.invalidationEdges) {
      const entry = state.invalidation.find((candidate) => candidate.name === expected.name);
      if (!entry) failAssertion(`invalidation edge for ${expected.name} is missing.`);
      else {
        if (expected.kind !== undefined && entry.kind !== expected.kind) failAssertion(`invalidation edge kind for ${expected.name} expected ${expected.kind}.`);
        if (expected.dependencyBindingId !== undefined && entry.dependencyBindingId !== expected.dependencyBindingId) failAssertion(`invalidation dependency for ${expected.name} is not immediate.`);
        if (expected.dependencyName !== undefined && entry.dependencyName !== expected.dependencyName) failAssertion(`invalidation dependency name for ${expected.name} expected ${expected.dependencyName}.`);
      }
    }
  }
  if (assertions.lastReadSource !== undefined && state.lastResult?.source !== assertions.lastReadSource) failAssertion(`last read source expected ${assertions.lastReadSource}.`);
  if (assertions.lastReceiptDiagnostic !== undefined && state.lastReceipt?.diagnostics?.[0]?.code !== assertions.lastReceiptDiagnostic) failAssertion(`last diagnostic expected ${assertions.lastReceiptDiagnostic}, got ${state.lastReceipt?.diagnostics?.[0]?.code}.`);
  if (assertions.lastCleanupDrain !== undefined && state.lastReceipt?.cleanup?.drain !== assertions.lastCleanupDrain) failAssertion(`last cleanup drain expected ${assertions.lastCleanupDrain}, got ${state.lastReceipt?.cleanup?.drain}.`);
  if (assertions.lastReceiptForceBoundary !== undefined && state.lastReceipt?.cleanup?.forceBoundary !== assertions.lastReceiptForceBoundary) failAssertion(`force boundary expected ${assertions.lastReceiptForceBoundary}.`);
  if (assertions.lastReceiptReanalysed !== undefined && state.lastReceipt?.reanalysed !== assertions.lastReceiptReanalysed) failAssertion(`reanalysed expected ${assertions.lastReceiptReanalysed}.`);
  if (assertions.effectDispositions !== undefined) {
    const dispositions = state.lastReceipt?.effects?.map((effect) => effect.disposition) ?? [];
    if (JSON.stringify(dispositions) !== JSON.stringify(assertions.effectDispositions)) failAssertion(`effect dispositions mismatch.`);
  }
  if (assertions.externalOutputs !== undefined) {
    const count = state.outputs.committed.filter((output) => output.visibility === "external").length;
    if (count !== assertions.externalOutputs) failAssertion(`external output count expected ${assertions.externalOutputs}, got ${count}.`);
  }
  if (assertions.outputCandidateGeneration !== undefined && state.outputs.committed.at(-1)?.candidateGeneration?.display !== assertions.outputCandidateGeneration) failAssertion(`output candidate generation expected ${assertions.outputCandidateGeneration}.`);
  if (assertions.lastReceipt?.generationBase !== undefined && state.lastReceipt?.generationBase?.display !== assertions.lastReceipt.generationBase) failAssertion(`last receipt base generation mismatch.`);
  if (assertions.generationResourceOwners !== undefined) {
    const owners = state.generation.scope.resources.map((resource) => resource.owner);
    if (JSON.stringify(owners) !== JSON.stringify(assertions.generationResourceOwners)) failAssertion(`resource owners mismatch.`);
  }
  if (assertions.classification !== undefined && state.lastClassification?.kind !== assertions.classification) failAssertion(`classification expected ${assertions.classification}, got ${state.lastClassification?.kind}.`);
  if (assertions.activeRequest !== undefined && (state.admission.active?.requestId ?? null) !== assertions.activeRequest) failAssertion(`active request expected ${assertions.activeRequest}.`);
  if (assertions.ownerScopeCount !== undefined && state.ownerScopes.length !== assertions.ownerScopeCount) failAssertion(`owner scope count expected ${assertions.ownerScopeCount}, got ${state.ownerScopes.length}.`);
  if (assertions.ownerScopeStates !== undefined) {
    const states = state.ownerScopes.map((scope) => scope.state).sort();
    if (JSON.stringify(states) !== JSON.stringify([...assertions.ownerScopeStates].sort())) failAssertion(`owner scope states expected ${JSON.stringify(assertions.ownerScopeStates)}, got ${JSON.stringify(states)}.`);
  }
  if (assertions.outputDelivery) {
    const output = state.outputs.committed.at(-1);
    if (!output || output.deliveredBytes !== assertions.outputDelivery.deliveredBytes || output.budget !== assertions.outputDelivery.budget) failAssertion(`output delivery mismatch.`);
  }
  if (assertions.outputCountAtMost !== undefined && state.outputs.committed.length > assertions.outputCountAtMost) failAssertion(`output count exceeds ${assertions.outputCountAtMost}.`);
  if (assertions.outputEvictedAtLeast !== undefined && state.outputs.evicted < assertions.outputEvictedAtLeast) failAssertion(`output evictions below ${assertions.outputEvictedAtLeast}.`);
  if (assertions.effectsCount !== undefined && state.effects.length !== assertions.effectsCount) failAssertion(`effect count expected ${assertions.effectsCount}, got ${state.effects.length}.`);
  if (assertions.effectsEvictedAtLeast !== undefined && state.effectsEvicted < assertions.effectsEvictedAtLeast) failAssertion(`effect evictions below ${assertions.effectsEvictedAtLeast}.`);
  if (assertions.ticketCountAtMost !== undefined && state.admission.tickets.length > assertions.ticketCountAtMost) failAssertion(`ticket count exceeds ${assertions.ticketCountAtMost}.`);
  if (assertions.lastReceiptPhaseNames !== undefined) {
    const phases = state.lastReceipt?.phases?.map((phase) => phase.phase) ?? [];
    if (JSON.stringify(phases) !== JSON.stringify(assertions.lastReceiptPhaseNames)) failAssertion(`receipt phases mismatch: ${JSON.stringify(phases)}.`);
  }
  if (assertions.lastReceiptDurableOutcomes !== undefined) {
    const outcomes = state.lastReceipt?.effects?.map((effect) => effect.durableOutcome) ?? [];
    if (JSON.stringify(outcomes) !== JSON.stringify(assertions.lastReceiptDurableOutcomes)) failAssertion(`durable effect outcomes mismatch.`);
  }
  if (assertions.lastCancellationOrdinal !== undefined) {
    const cancellation = state.cancellations.at(-1);
    if ((cancellation?.ordinal ?? null) !== assertions.lastCancellationOrdinal) failAssertion(`cancellation ordinal expected ${assertions.lastCancellationOrdinal}.`);
  }
  if (assertions.historyRawSource !== undefined && state.history.records.at(-1)?.rawSource !== assertions.historyRawSource) failAssertion(`raw source policy mismatch.`);
  if (assertions.generationIdentityOpaque !== undefined && (state.generation?.id === state.generation?.display) !== !assertions.generationIdentityOpaque) failAssertion(`generation identity opacity mismatch.`);
}

if (corpus.$schema !== "w-repl-session-cases-1") errors.push("unexpected PYN2 corpus schema");
if (corpus.status !== "design-oracle-input") errors.push("PYN2 corpus must be design-oracle-input");
if (corpus.machine !== "repl-session-machine-pyn2") errors.push("PYN2 machine name is invalid");
if (!Array.isArray(corpus.decisions) || corpus.decisions.length === 0) errors.push("PYN2 corpus must declare ledger IDs");
for (const decision of corpus.decisions ?? []) {
  const number = Number(String(decision).slice(2));
  if (!(number === 974 || (number >= 1076 && number <= 1106) || number === 1246 || number === 1247) || !ledgerIdSet.has(decision)) errors.push(`unknown PYN2 ledger decision ${decision}`);
}
if (!Array.isArray(corpus.references) || corpus.references.length === 0) errors.push("PYN2 corpus references are required");
for (const [index, reference] of (corpus.references ?? []).entries()) resolveReference(reference, `references[${index}]`);
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) errors.push("PYN2 cases are required");

for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  if (!/^PYN2-[a-z0-9-]+$/.test(testCase.id ?? "") || caseIds.has(testCase.id)) errors.push(`${location}.id is invalid or duplicated.`);
  else caseIds.add(testCase.id);
  if (!Array.isArray(testCase.references) || testCase.references.length === 0) errors.push(`${location}.references must link to Last Light.`);
  else testCase.references.forEach((reference, referenceIndex) => resolveReference(reference, `${location}.references[${referenceIndex}]`));
  const operations = testCase.operations ?? [];
  if (!Array.isArray(operations) || operations.length === 0) {
    errors.push(`${location}.operations must not be empty.`);
    continue;
  }
  operationCount += operations.length;
  for (const [operationIndex, operation] of operations.entries()) {
    if (!validateReplSessionOperation(operation)) errors.push(`${location}.operations[${operationIndex}] is malformed.`);
    else coveredOperations.add(operation.op);
  }
  if (!testCase.expected || !["accepted", "rejected"].includes(testCase.expected.status)) {
    errors.push(`${location}.expected.status must be accepted or rejected.`);
    continue;
  }
  const actual = runReplSessionProgram(operations);
  if (actual.status !== testCase.expected.status) errors.push(`${testCase.id} expected ${testCase.expected.status}, got ${actual.status}.`);
  if (testCase.expected.status === "rejected") {
    if (actual.code !== testCase.expected.code) errors.push(`${testCase.id} expected ${testCase.expected.code}, got ${actual.code}.`);
    if (testCase.expected.at !== "last" || actual.operation !== operations.length - 1) errors.push(`${testCase.id} rejection must occur at the last operation.`);
  } else {
    assertCase(actual.state, testCase.expected.assertions, testCase.id);
  }
  results.push({ caseId: testCase.id, status: actual.status, ...(actual.code ? { code: actual.code, operation: actual.operation } : {}), state: compactState(actual.state), trace: actual.state.trace });
}

const requiredOperations = new Set(["open", "appendBuffer", "clearBuffer", "classify", "submit", "beginSubmission", "advanceSubmission", "finishSubmission", "enqueue", "admitNext", "complete", "inspect", "status", "history", "why", "cancel", "command", "reset", "restart", "quit"]);
for (const operation of requiredOperations) if (!coveredOperations.has(operation)) errors.push(`PYN2 corpus does not cover ${operation}`);
const accepted = results.filter((result) => result.status === "accepted").length;
const rejected = results.length - accepted;
if (accepted === 0 || rejected === 0) errors.push("PYN2 corpus must contain accepted and rejected cases");

const expectedSnapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
if (process.argv.includes("--write")) fs.writeFileSync(snapshotPath, expectedSnapshot);
else if (!fs.existsSync(snapshotPath)) errors.push("PYN2 snapshot is missing; run with --write");
else if (fs.readFileSync(snapshotPath, "utf8") !== expectedSnapshot) errors.push("PYN2 snapshot is stale; run with --write after reviewing the change");

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}
process.stdout.write(`REPL session oracle: ${results.length} cases, ${operationCount} operations, ${accepted} accepted, ${rejected} rejected.\n`);
