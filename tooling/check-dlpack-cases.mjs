import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { dlpackDigest, compactDLPackState, runDLPackProgram } from "./dlpack-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const rootDirectory = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "dlpack-cases.json");
const snapshotPath = path.join(toolingDirectory, "dlpack-results.snapshot.jsonl");
const designPath = path.join(rootDirectory, "DESIGN.md");
const diagnosticCatalogPath = path.join(toolingDirectory, "diagnostic-catalog.json");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const design = fs.readFileSync(designPath, "utf8");
const diagnosticCatalog = JSON.parse(fs.readFileSync(diagnosticCatalogPath, "utf8"));
const knownDiagnostics = new Set((diagnosticCatalog.codes ?? []).map((entry) => entry.code));
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
  if (typeof reference.path !== "string") {
    error(`${location}.path is required.`);
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
  if (assertions.releaseCalls !== undefined && state.releaseCalls !== assertions.releaseCalls) failAssertion(`releaseCalls expected ${assertions.releaseCalls}, got ${state.releaseCalls}.`);
  if (assertions.releaseCount !== undefined && state.releaseCount !== assertions.releaseCount) failAssertion(`releaseCount expected ${assertions.releaseCount}, got ${state.releaseCount}.`);
  if (assertions.deleterCalls !== undefined && state.deleterCalls !== assertions.deleterCalls) failAssertion(`deleterCalls expected ${assertions.deleterCalls}, got ${state.deleterCalls}.`);
  if (assertions.dereferencedFields !== undefined && JSON.stringify(state.dereferencedFields) !== JSON.stringify(assertions.dereferencedFields)) failAssertion("dereferencedFields mismatch.");
  if (assertions.capsuleDestructor !== undefined && state.capsuleDestructor !== assertions.capsuleDestructor) failAssertion("capsule destructor state mismatch.");
  if (assertions.spanBytes !== undefined && state.tensor?.spanBytes !== assertions.spanBytes) failAssertion(`spanBytes expected ${assertions.spanBytes}, got ${state.tensor?.spanBytes}.`);
  if (assertions.providerExtent !== undefined && state.providerExtent !== assertions.providerExtent) failAssertion(`provider extent expected ${assertions.providerExtent}, got ${state.providerExtent}.`);
  if (assertions.copyCount !== undefined && state.copies.length !== assertions.copyCount) failAssertion(`copy count expected ${assertions.copyCount}, got ${state.copies.length}.`);
  if (assertions.copies !== undefined) {
    for (const [index, expected] of assertions.copies.entries()) {
      const actual = state.copies[index];
      for (const [key, value] of Object.entries(expected)) {
        if (JSON.stringify(actual?.[key]) !== JSON.stringify(value)) failAssertion(`copy ${index} field ${key} mismatch.`);
      }
    }
  }
  if (assertions.receiptCount !== undefined && state.receipts.length !== assertions.receiptCount) failAssertion(`receipt count expected ${assertions.receiptCount}, got ${state.receipts.length}.`);
  if (assertions.receipts !== undefined) {
    for (const [index, expected] of assertions.receipts.entries()) {
      const actual = state.receipts[index];
      for (const [key, value] of Object.entries(expected)) {
        if (JSON.stringify(actual?.[key]) !== JSON.stringify(value)) failAssertion(`receipt ${index} field ${key} mismatch.`);
      }
    }
  }
  if (assertions.releaseRecords !== undefined) {
    for (const [index, expected] of assertions.releaseRecords.entries()) {
      const actual = state.releaseRecords[index];
      for (const [key, value] of Object.entries(expected)) {
        if (JSON.stringify(actual?.[key]) !== JSON.stringify(value)) failAssertion(`release record ${index} field ${key} mismatch.`);
      }
    }
    if (state.releaseRecords.length !== assertions.releaseRecords.length) failAssertion("release record count mismatch.");
  }
  if (assertions.eventsInclude !== undefined) {
    for (const event of assertions.eventsInclude) {
      if (!state.events.includes(event)) failAssertion(`event ${event} is absent.`);
    }
  }
  if (assertions.eventsExclude !== undefined) {
    for (const event of assertions.eventsExclude) {
      if (state.events.includes(event)) failAssertion(`event ${event} must be absent.`);
    }
  }
}

if (corpus.$schema !== "w-dlpack-cases-1") error("DLPack corpus schema is invalid.");
if (corpus.status !== "design-oracle-input") error("DLPack corpus must be a design-oracle input.");
if (corpus.machine !== "dlpack-machine-pyn4") error("DLPack corpus machine name is invalid.");
for (const decision of corpus.decisions ?? []) {
  if (!/^W-11(?:2[5-9]|3[0-9]|4[0-7])$/.test(decision)) error(`unexpected DLPack decision ${decision}.`);
  if (!design.includes(decision)) error(`DLPack decision ${decision} is absent from DESIGN.md.`);
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
  operationCount += testCase.operations?.length ?? 0;
  const result = runDLPackProgram(testCase.operations ?? []);
  if (!testCase.expected || result.status !== testCase.expected.status) error(`${location} expected ${testCase.expected?.status}, derived ${result.status}.`);
  if (result.error && !knownDiagnostics.has(result.error.code)) error(`${location} diagnostic ${result.error.code} is not cataloged.`);
  if (result.error) {
    const details = result.error.details ?? {};
    if (typeof details.phase !== "string") error(`${location} diagnostic ${result.error.code} lacks a phase fact.`);
    if (typeof details.actual !== "string") error(`${location} diagnostic ${result.error.code} lacks an actual fact.`);
    if (typeof details.construct !== "string") error(`${location} diagnostic ${result.error.code} lacks a construct fact.`);
    if (!Array.isArray(details.expected) || details.expected.length === 0) error(`${location} diagnostic ${result.error.code} lacks an expected fact set.`);
    if (typeof details.reason !== "string") error(`${location} diagnostic ${result.error.code} lacks a reason fact.`);
    if (!details.facts || typeof details.facts !== "object") error(`${location} diagnostic ${result.error.code} lacks diagnostic facts.`);
  }
  if (testCase.expected?.diagnostic !== undefined && result.error?.code !== testCase.expected.diagnostic) error(`${location} diagnostic expected ${testCase.expected.diagnostic}, got ${result.error?.code}.`);
  assertCase(result, testCase.expected?.assertions, `${location}.expected.assertions`);
  results.push({
    id: testCase.id,
    status: result.status,
    diagnostic: result.error?.code ?? null,
    diagnosticFacts: result.error?.details ?? null,
    digest: dlpackDigest(compactDLPackState(result.state)),
    state: compactDLPackState(result.state),
  });
}

for (const [decision, states] of covered) {
  if (!states.accepted || !states.rejected) error(`${decision} lacks positive and negative DLPack evidence.`);
}

const snapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
if (process.argv.includes("--write")) fs.writeFileSync(snapshotPath, snapshot);
else if (fs.readFileSync(snapshotPath, "utf8") !== snapshot) error("dlpack-results.snapshot.jsonl is stale; run the checker with --write.");

if (errors.length > 0) {
  process.stderr.write(`${errors.map((entry) => `- ${entry}`).join("\n")}\n`);
  process.exit(1);
}

const accepted = results.filter((result) => result.status === "accepted").length;
process.stdout.write(`DLPack PYN4: ${results.length} cases, ${operationCount} operations; ${accepted} accepted, ${results.length - accepted} rejected.\n`);
