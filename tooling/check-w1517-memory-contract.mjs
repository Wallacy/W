import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIdSet as designDecisionIds } from "./design-ledger.mjs";
import { runW1517Case } from "./w1517-memory-contract-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const casesPath = path.join(toolingDirectory, "w1517-memory-contract-cases.json");
const snapshotPath = path.join(toolingDirectory, "w1517-memory-contract-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const errors = [];
const caseIds = new Set();
const results = [];

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function subset(expected, actual, location) {
  if (!isObject(expected)) {
    if (!Object.is(expected, actual)) errors.push(`${location} expected ${JSON.stringify(expected)}; actual ${JSON.stringify(actual)}.`);
    return;
  }
  if (!isObject(actual)) {
    errors.push(`${location} expected an object; actual ${JSON.stringify(actual)}.`);
    return;
  }
  for (const [key, value] of Object.entries(expected)) subset(value, actual[key], `${location}.${key}`);
}

if (corpus.$schema !== "w-w1517-memory-contract-cases-1") errors.push("W-1517 corpus schema is invalid.");
if (corpus.status !== "design-oracle-input-1") errors.push("W-1517 corpus status is invalid.");
if (corpus.machine !== "w1517-memory-contract-machine-1") errors.push("W-1517 corpus machine is invalid.");
if (corpus.evidenceBoundary !== "design-only; no provider-conformant or implementation result") {
  errors.push("W-1517 corpus must disclose its design-only evidence boundary.");
}
if (!Array.isArray(corpus.decisions) || corpus.decisions.length !== 1 || corpus.decisions[0] !== "W-1517") {
  errors.push("W-1517 corpus must declare exactly decision W-1517.");
}

for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  const id = testCase?.id;
  if (!/^W1517-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(id ?? "")) {
    errors.push(`${location}.id must use W1517-kebab-case.`);
  } else if (caseIds.has(id)) {
    errors.push(`${location}.id duplicates ${id}.`);
  } else {
    caseIds.add(id);
  }
  if (!Array.isArray(testCase?.decisions) || !testCase.decisions.includes("W-1517")) {
    errors.push(`${location}.decisions must cite W-1517.`);
  }
  for (const decision of testCase?.decisions ?? []) {
    if (!designDecisionIds.has(decision)) errors.push(`${location} cites missing ledger decision ${decision}.`);
  }
  if (!testCase?.input || typeof testCase.input !== "object") errors.push(`${location}.input must be an object.`);
  if (!testCase?.expected || !["accepted", "rejected"].includes(testCase.expected.status)) {
    errors.push(`${location}.expected.status must be accepted or rejected.`);
  }
  if (testCase?.expected?.status === "rejected" && typeof testCase.expected.code !== "string") {
    errors.push(`${location}.expected.code is required for rejection.`);
  }
  const actual = runW1517Case(testCase);
  if (actual.status !== testCase?.expected?.status ||
      (actual.status === "rejected" && actual.code !== testCase.expected.code)) {
    errors.push(`${location} expected ${JSON.stringify(testCase.expected)}; actual ${JSON.stringify(actual)}.`);
  }
  if (testCase?.expected?.facts) subset(testCase.expected.facts, actual.facts, `${location}.expected.facts`);
  results.push({ caseId: id, status: actual.status, ...(actual.code ? { code: actual.code } : {}), facts: actual.facts ?? {} });
}

if (caseIds.size < 16) errors.push("W-1517 corpus must retain the adversarial minimum of 16 cases.");

if (errors.length) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const output = [
  JSON.stringify({ schema: "w-w1517-memory-contract-results-1", status: "design-oracle-output-1" }),
  ...results.map((result) => JSON.stringify(result)),
].join("\n") + "\n";
const accepted = results.filter((result) => result.status === "accepted").length;
const rejected = results.length - accepted;
const summary = `W-1517 design oracle: ${results.length} cases, ${accepted} accepted, ${rejected} rejected.`;

if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, output);
  process.stdout.write(`${summary}\nUpdated ${path.basename(snapshotPath)}.\n`);
  process.exit(0);
}
if (!fs.existsSync(snapshotPath)) {
  process.stderr.write(`${path.basename(snapshotPath)} is missing; run with --write.\n`);
  process.exit(1);
}
if (fs.readFileSync(snapshotPath, "utf8") !== output) {
  process.stderr.write(`${path.basename(snapshotPath)} is stale; run with --write.\n`);
  process.exit(1);
}
process.stdout.write(`${summary}\n`);
