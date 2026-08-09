import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { compactJupyterState, jupyterDigest, runJupyterProgram } from "./jupyter-machine.mjs";
import { ledgerIdSet } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const rootDirectory = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "jupyter-cases.json");
const snapshotPath = path.join(toolingDirectory, "jupyter-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const errors = [];
const results = [];
const covered = new Map();
let operationCount = 0;

function checkReference(reference, location) {
  if (!reference || typeof reference !== "object") return errors.push(`${location} must be an object.`);
  if (typeof reference.path !== "string" || !reference.path.startsWith("reference/last-light/")) return errors.push(`${location}.path must point into reference/last-light.`);
  const target = path.join(rootDirectory, reference.path);
  if (!fs.existsSync(target)) return errors.push(`${location}.path does not exist: ${reference.path}.`);
  if (typeof reference.symbol !== "string" || reference.symbol.length === 0) return errors.push(`${location}.symbol is required.`);
  const source = fs.readFileSync(target, "utf8");
  const escaped = reference.symbol.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  if (!new RegExp(`\\b${escaped}\\b`).test(source)) errors.push(`${location}.symbol is absent from ${reference.path}.`);
}

function assertCase(result, assertions, location) {
  if (!assertions) return;
  const state = result.state;
  const fail = (message) => errors.push(`${location}: ${message}`);
  if (assertions.ordinal !== undefined && state.executionOrdinal !== assertions.ordinal) fail(`ordinal expected ${assertions.ordinal}, got ${state.executionOrdinal}.`);
  if (assertions.reservedOrdinal !== undefined && state.reservedOrdinal !== assertions.reservedOrdinal) fail(`reserved ordinal expected ${assertions.reservedOrdinal}, got ${state.reservedOrdinal}.`);
  if (assertions.reservedOrdinals && JSON.stringify(state.requests.map((request) => request.reservedOrdinal)) !== JSON.stringify(assertions.reservedOrdinals)) fail("reserved ordinal order mismatch.");
  if (assertions.executionCount !== undefined && state.executionCount !== assertions.executionCount) fail(`execution count expected ${assertions.executionCount}, got ${state.executionCount}.`);
  if (assertions.generationOpaque && (typeof state.generation !== "string" || !state.generation.startsWith("opaque:"))) fail("generation identity is not opaque.");
  if (assertions.generation !== undefined && state.generation !== assertions.generation) fail(`generation expected ${assertions.generation}, got ${state.generation}.`);
  if (assertions.reply !== undefined && state.lastReply?.status !== assertions.reply) fail(`reply expected ${assertions.reply}, got ${state.lastReply?.status}.`);
  if (assertions.lifecycle && JSON.stringify(state.events.at(-1)?.sequence) !== JSON.stringify(assertions.lifecycle)) fail("lifecycle sequence mismatch.");
  if (assertions.diagnostic !== undefined && result.error?.code !== assertions.diagnostic) fail(`diagnostic expected ${assertions.diagnostic}, got ${result.error?.code}.`);
  if (assertions.shutdown !== undefined && state.shutdown !== assertions.shutdown) fail(`shutdown expected ${assertions.shutdown}, got ${state.shutdown}.`);
  if (assertions.interruptTerminated !== undefined && state.controls.find((control) => control.kind === "interrupt")?.terminated !== assertions.interruptTerminated) fail("interrupt termination claim mismatch.");
  if (assertions.reads !== undefined && state.reads !== assertions.reads) fail(`read count expected ${assertions.reads}, got ${state.reads}.`);
  if (assertions.readSource !== undefined && state.lastRead?.snapshot !== assertions.readSource) fail("read source mismatch.");
  if (assertions.plainText !== undefined && state.lastRead?.plainText !== assertions.plainText) fail("inspect plain text requirement mismatch.");
  if (assertions.passwordStored !== undefined && (state.stdin?.persisted ?? false) !== assertions.passwordStored) fail("password persistence mismatch.");
  if (assertions.stdinExportable !== undefined && (state.stdin?.exportable ?? null) !== assertions.stdinExportable) fail("stdin exportability mismatch.");
  if (assertions.metadataNamespace !== undefined && Object.keys(state.metadata.at(-1) ?? {})[0] !== assertions.metadataNamespace) fail("metadata namespace mismatch.");
  if (assertions.ordinals && JSON.stringify(state.requests.map((request) => request.ordinal)) !== JSON.stringify(assertions.ordinals)) fail("ordinal order mismatch.");
  if (assertions.requestOrder && JSON.stringify(state.requests.map((request) => request.requestId)) !== JSON.stringify(assertions.requestOrder)) fail("request FIFO order mismatch.");
  if (assertions.outputs !== undefined && state.outputs.length !== assertions.outputs) fail(`output count expected ${assertions.outputs}, got ${state.outputs.length}.`);
  if (assertions.expressionKeys && JSON.stringify(state.requests.at(-1)?.userExpressions.map((entry) => entry.key)) !== JSON.stringify(assertions.expressionKeys)) fail("user expression keys mismatch.");
  if (assertions.jsonUsed !== undefined && state.jsonUsed !== assertions.jsonUsed) fail("JSON decode must follow authentication.");
  if (assertions.queuedAborted !== undefined && !state.requests.some((request) => request.outcome === "queuedAborted" && request.reply === "error:WQueueAborted")) fail("queued execute was not marked WQueueAborted.");
  if (assertions.heartbeatEcho !== undefined && state.heartbeatEcho !== assertions.heartbeatEcho) fail("heartbeat echo mismatch.");
  if (assertions.unsupported !== undefined && !state.unsupported.includes(assertions.unsupported)) fail("unsupported optional message was not ignored.");
}

function expandOperations(testCase) {
  const base = corpus.base?.open ?? {};
  return (testCase.operations ?? []).map((operation) => {
    if (operation?.op !== "open" || operation.use !== "base") return operation;
    const open = structuredClone(base);
    if (operation.connectionPatch) open.connection = { ...open.connection, ...operation.connectionPatch };
    if (operation.kernelInfoPatch) open.kernelInfo = { ...open.kernelInfo, ...operation.kernelInfoPatch };
    if (operation.stop_on_error !== undefined) open.stop_on_error = operation.stop_on_error;
    if (operation.limits) open.limits = operation.limits;
    return { op: "open", ...open };
  });
}

if (corpus.$schema !== "w-jupyter-cases-1") errors.push("Jupyter corpus schema is invalid.");
if (corpus.status !== "design-oracle-input") errors.push("Jupyter corpus must be a design-oracle input.");
if (corpus.machine !== "jupyter-machine-pyn3") errors.push("Jupyter corpus machine name is invalid.");
for (const decision of corpus.decisions ?? []) {
  if (!/^W-11(?:1[2-9]|23)$/.test(decision)) errors.push(`unexpected Jupyter decision ${decision}.`);
  if (!ledgerIdSet.has(decision)) errors.push(`Jupyter decision ${decision} is absent from the RATIONALE ledger.`);
  covered.set(decision, { accepted: false, rejected: false });
}
for (const [index, reference] of (corpus.references ?? []).entries()) checkReference(reference, `references[${index}]`);

const ids = new Set();
for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  if (ids.has(testCase.id)) errors.push(`${location}.id is duplicated.`);
  ids.add(testCase.id);
  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) errors.push(`${location}.decisions must be non-empty.`);
  for (const decision of testCase.decisions ?? []) {
    if (!covered.has(decision)) errors.push(`${location} references undeclared decision ${decision}.`);
    else covered.get(decision)[testCase.expected?.status === "accepted" ? "accepted" : "rejected"] = true;
  }
  operationCount += testCase.operations?.length ?? 0;
  const result = runJupyterProgram(expandOperations(testCase));
  if (!testCase.expected || result.status !== testCase.expected.status) errors.push(`${location} expected ${testCase.expected?.status}, derived ${result.status}.`);
  assertCase(result, testCase.expected?.assertions, `${location}.expected.assertions`);
  const compact = compactJupyterState(result.state);
  results.push({ id: testCase.id, status: result.status, digest: jupyterDigest(compact), state: compact });
}
for (const [decision, states] of covered) if (!states.accepted || !states.rejected) errors.push(`${decision} lacks positive and negative Jupyter evidence.`);

const snapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
if (process.argv.includes("--write")) fs.writeFileSync(snapshotPath, snapshot);
else if (fs.readFileSync(snapshotPath, "utf8") !== snapshot) errors.push("jupyter-results.snapshot.jsonl is stale; run the checker with --write.");
if (errors.length > 0) {
  process.stderr.write(`${errors.map((entry) => `- ${entry}`).join("\n")}\n`);
  process.exit(1);
}
const accepted = results.filter((result) => result.status === "accepted").length;
process.stdout.write(`Jupyter PYN3: ${results.length} cases, ${operationCount} operations; ${accepted} accepted, ${results.length - accepted} rejected.\n`);
