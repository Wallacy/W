import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  compactPresentationState,
  presentationDigest,
  runPresentationProgram,
} from "./presentation-machine.mjs";
import { ledgerIdSet } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const rootDirectory = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "presentation-cases.json");
const snapshotPath = path.join(toolingDirectory, "presentation-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const errors = [];
const results = [];
const covered = new Map();
let operationCount = 0;

function error(message) {
  errors.push(message);
}

function checkReference(reference, location) {
  if (!reference || typeof reference !== "object") {
    error(`${location} must be an object.`);
    return;
  }
  if (typeof reference.path !== "string" || !reference.path.startsWith("reference/last-light/")) {
    error(`${location}.path must point into reference/last-light.`);
    return;
  }
  const target = path.join(rootDirectory, reference.path);
  if (!fs.existsSync(target)) {
    error(`${location}.path does not exist: ${reference.path}.`);
    return;
  }
  if (typeof reference.symbol !== "string" || reference.symbol.length === 0) {
    error(`${location}.symbol is required.`);
    return;
  }
  const source = fs.readFileSync(target, "utf8");
  const escaped = reference.symbol.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  if (!new RegExp(`\\b${escaped}\\b`).test(source)) error(`${location}.symbol is absent from ${reference.path}.`);
}

function assertCase(result, assertions, location) {
  if (!assertions) return;
  const state = result.state;
  const failAssertion = (message) => error(`${location}: ${message}`);
  if (assertions.phase !== undefined && state.phase !== assertions.phase) failAssertion(`phase expected ${assertions.phase}, got ${state.phase}.`);
  if (assertions.representations !== undefined && state.entries.length !== assertions.representations) failAssertion(`representation count expected ${assertions.representations}, got ${state.entries.length}.`);
  if (assertions.plainText !== undefined && !state.entries.some((entry) => entry.media === "text/plain") !== !assertions.plainText) failAssertion("plain text presence mismatch.");
  if (assertions.previewCollected !== undefined && (state.preview?.collected ?? null) !== assertions.previewCollected) failAssertion("preview collection fact mismatch.");
  if (assertions.tensorCopied !== undefined && (state.tensor?.copied ?? null) !== assertions.tensorCopied) failAssertion("tensor copy fact mismatch.");
  if (assertions.fallback !== undefined && state.fallback !== assertions.fallback) failAssertion(`fallback expected ${assertions.fallback}, got ${state.fallback}.`);
  if (assertions.submissionOutcome !== undefined && state.submissionOutcome !== assertions.submissionOutcome) failAssertion(`submission outcome expected ${assertions.submissionOutcome}, got ${state.submissionOutcome}.`);
  if (assertions.redacted !== undefined && state.error?.redacted !== assertions.redacted) failAssertion("redaction fact mismatch.");
  if (assertions.output === null && state.output !== null) failAssertion("output must remain absent.");
  if (assertions.diagnostic !== undefined && result.error?.code !== assertions.diagnostic) failAssertion(`diagnostic expected ${assertions.diagnostic}, got ${result.error?.code}.`);
}

if (corpus.$schema !== "w-presentation-cases-1") error("presentation corpus schema is invalid.");
if (corpus.status !== "design-oracle-input") error("presentation corpus must be a design-oracle input.");
if (corpus.machine !== "presentation-machine-pyn3") error("presentation corpus machine name is invalid.");
for (const decision of corpus.decisions ?? []) {
  if (!/^W-11(?:0[7-9]|1[0-1]|23)$/.test(decision)) error(`unexpected presentation decision ${decision}.`);
  if (!ledgerIdSet.has(decision)) error(`presentation decision ${decision} is absent from the RATIONALE ledger.`);
  covered.set(decision, { accepted: false, rejected: false });
}
for (const [index, reference] of (corpus.references ?? []).entries()) checkReference(reference, `references[${index}]`);

const ids = new Set();
for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  if (ids.has(testCase.id)) error(`${location}.id is duplicated.`);
  ids.add(testCase.id);
  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) error(`${location}.decisions must be non-empty.`);
  for (const decision of testCase.decisions ?? []) {
    if (!covered.has(decision)) error(`${location} references undeclared decision ${decision}.`);
    else covered.get(decision)[testCase.expected?.status === "accepted" ? "accepted" : "rejected"] = true;
  }
  for (const [referenceIndex, reference] of (testCase.references ?? []).entries()) checkReference(reference, `${location}.references[${referenceIndex}]`);
  operationCount += testCase.operations?.length ?? 0;
  const result = runPresentationProgram(testCase.operations ?? []);
  if (!testCase.expected || result.status !== testCase.expected.status) error(`${location} expected ${testCase.expected?.status}, derived ${result.status}.`);
  assertCase(result, testCase.expected?.assertions, `${location}.expected.assertions`);
  const compact = compactPresentationState(result.state);
  results.push({ id: testCase.id, status: result.status, digest: presentationDigest(compact), state: compact });
}

for (const [decision, states] of covered) {
  if (!states.accepted || !states.rejected) error(`${decision} lacks positive and negative presentation evidence.`);
}

const snapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
if (process.argv.includes("--write")) fs.writeFileSync(snapshotPath, snapshot);
else if (fs.readFileSync(snapshotPath, "utf8") !== snapshot) error("presentation-results.snapshot.jsonl is stale; run the checker with --write.");

if (errors.length > 0) {
  process.stderr.write(`${errors.map((entry) => `- ${entry}`).join("\n")}\n`);
  process.exit(1);
}

const accepted = results.filter((result) => result.status === "accepted").length;
process.stdout.write(`Presentation PYN3: ${results.length} cases, ${operationCount} operations; ${accepted} accepted, ${results.length - accepted} rejected.\n`);
