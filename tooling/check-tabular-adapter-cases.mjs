// Validate the TAB1 host oracle and its cases. This script never executes W.
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  runTabularAdapterProgram,
  validateTabularAdapterOperation,
  lastLightSymbols,
} from "./tabular-adapter-machine.mjs";
import { ledgerIdSet } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const rootDirectory = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "tabular-adapter-cases.json");
const snapshotPath = path.join(toolingDirectory, "tabular-adapter-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const vectors = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "tabular-adapter-byte-vectors.json"), "utf8"));
const lastLight = fs.readFileSync(path.join(rootDirectory, "reference/last-light/data_formats.w"), "utf8");
const errors = [];

function digest(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

if (corpus.$schema !== "w-tabular-adapter-cases-1") errors.push("invalid cases schema");
if (corpus.status !== "design-oracle-input") errors.push("cases must be design-oracle-input");
if (vectors.$schema !== "w-tabular-adapter-byte-vectors-1" || vectors.status !== "design-oracle-input") {
  errors.push("byte vectors must be design-oracle-input");
}
for (const vector of vectors.vectors ?? []) {
  if (typeof vector.valid !== "boolean" && vector.id !== "csv-portable-header-crlf" && vector.id !== "csv-rfc4180-dquote-escape") {
    errors.push(`${vector.id}: vector must state valid true/false`);
  }
  const actual = Buffer.from(vector.text ?? "", "utf8").toString("hex");
  if (actual !== vector.hex) errors.push(`${vector.id}: byte vector is not exact`);
}
if (!(vectors.vectors ?? []).some((vector) => vector.valid === false)) errors.push("byte vectors need a negative boundary");
if (!(vectors.vectors ?? []).some((vector) => vector.valid === true)) errors.push("byte vectors need a positive canonical boundary");

for (let number = 1006; number <= 1045; number += 1) {
  const id = `W-${number}`;
  if (!ledgerIdSet.has(id)) errors.push(`missing reserved decision ${id}`);
  if (!corpus.decisionIds?.includes(id)) errors.push(`cases do not cover ${id}`);
}

const requiredIds = [
  "TAB1-data-publish-and-identity",
  "TAB1-data-borrow-view-copy-owner",
  "TAB1-decode-error-after-publication",
  "TAB1-encode-partial-progress",
  "TAB1-cancel-drains-waits",
  "TAB1-csv-portable-canonical",
  "TAB1-csv-duplicate-header",
  "TAB1-parquet-snapshot-preflight",
  "TAB1-parquet-encrypted-key-required",
  "TAB1-arrow-ipc-preflight",
  "TAB1-arrow-copy-never",
  "TAB1-arrow-trusted-c-release-once",
  "TAB1-source-instability-rejected",
];
const seen = new Set();
const results = [];
const symbols = new Set(lastLightSymbols().filter((symbol) => new RegExp(`(?:fn|struct|enum)\\s+${symbol}\\b`).test(lastLight)));
if (symbols.size < 6) errors.push("Last Light route symbol scan is unexpectedly small");
let operationCount = 0;

for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  if (typeof testCase.id !== "string" || testCase.id.length === 0) errors.push(`${location}.id is required`);
  if (seen.has(testCase.id)) errors.push(`${location}.id duplicates ${testCase.id}`);
  seen.add(testCase.id);
  if (!Array.isArray(testCase.operations) || testCase.operations.length === 0) errors.push(`${testCase.id}: operations required`);
  operationCount += testCase.operations?.length ?? 0;
  if (typeof testCase.symbol !== "string" || !symbols.has(testCase.symbol)) {
    errors.push(`${testCase.id}: symbol must link to Last Light data_formats.w`);
  }
  for (const operation of testCase.operations ?? []) {
    if (!validateTabularAdapterOperation(operation)) errors.push(`${testCase.id}: malformed operation`);
    if (JSON.stringify(operation).includes("unlimited")) errors.push(`${testCase.id}: unlimited is forbidden`);
  }
  if (!testCase.expected || !["accepted", "rejected"].includes(testCase.expected.status)) {
    errors.push(`${testCase.id}: expected status required`);
    continue;
  }
  const actual = runTabularAdapterProgram(testCase.operations);
  if (actual.status !== testCase.expected.status) {
    errors.push(`${testCase.id}: expected ${testCase.expected.status}, got ${actual.status}`);
  }
  if (testCase.expected.code !== undefined && actual.code !== testCase.expected.code) {
    errors.push(`${testCase.id}: expected ${testCase.expected.code}, got ${actual.code ?? "none"}`);
  }
  for (const decision of testCase.decisions ?? []) {
    if (!ledgerIdSet.has(decision)) errors.push(`${testCase.id}: unknown decision ${decision}`);
  }
  results.push({
    id: testCase.id,
    status: actual.status,
    ...(actual.code ? { code: actual.code } : {}),
    ...(actual.operation !== undefined ? { operation: actual.operation } : {}),
    stateDigest: digest(JSON.stringify(actual.state)),
    traceLength: actual.trace.length,
  });
}

for (const id of requiredIds) if (!seen.has(id)) errors.push(`missing required case ${id}`);
const positives = results.filter((result) => result.status === "accepted").length;
const negatives = results.filter((result) => result.status === "rejected").length;
if (positives < 12 || negatives < 12) errors.push("TAB1 cases need positive and negative coverage");
if (new Set(corpus.decisionIds ?? []).size !== (corpus.decisionIds ?? []).length) errors.push("decisionIds must be unique");
if (operationCount < 70) errors.push(`TAB1 corpus needs at least 70 derived operations; got ${operationCount}`);

const serialized = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
if (process.argv.includes("--write") && errors.length === 0) {
  fs.writeFileSync(snapshotPath, serialized);
} else if (!fs.existsSync(snapshotPath)) {
  errors.push("tabular-adapter-results.snapshot.jsonl is missing; run with --write");
} else if (fs.readFileSync(snapshotPath, "utf8") !== serialized) {
  errors.push("tabular-adapter-results.snapshot.jsonl is stale; run with --write");
}

if (errors.length > 0) {
  for (const error of errors) console.error(`- ${error}`);
  process.exit(1);
}

console.log(
  `TAB1 adapter cases: ${results.length} cases, ${positives} accepted, ${negatives} rejected, ` +
  `${new Set(corpus.decisionIds).size} decisions covered.`,
);
