// Host checker for design evidence; it never compiles or executes W.
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  runTabularCarrierProgram,
  validateTabularCarrierOperation,
} from "./tabular-carrier-machine.mjs";
import { ledgerIdSet as designDecisionIds } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "tabular-carrier-cases.json");
const snapshotPath = path.join(toolingDirectory, "tabular-carrier-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const requiredDecisionIds = Array.from({ length: 14 }, (_, index) => `W-${988 + index}`);
const requiredCaseIds = new Set([
  "TAB0-batch-publication-owned-columns",
  "TAB0-batch-unequal-column-length-rejected",
  "TAB0-empty-schema-explicit-row-count",
  "TAB0-empty-schema-without-row-count-rejected",
  "TAB0-null-is-not-nan",
  "TAB0-nonnullable-null-rejected",
  "TAB0-static-row-bind-preserves-field-order",
  "TAB0-static-row-type-mismatch-rejected",
  "TAB0-dynamic-bind-explicit-check",
  "TAB0-dynamic-bind-without-explicit-binding-rejected",
  "TAB0-dynamic-reorder-without-mapping-rejected",
  "TAB0-row-array-remains-row-algorithm",
  "TAB0-row-array-universal-carrier-rejected",
  "TAB0-dataframe-first-party-direction",
  "TAB0-dataframe-stable-std-rejected",
  "TAB0-row-synthesis-rejects-any",
  "TAB0-row-synthesis-rejects-nullable-top-level",
  "TAB0-static-descriptor-and-dynamic-name-select",
  "TAB0-reflection-string-selection-rejected",
  "TAB0-run-end-materialized-for-random-access",
  "TAB0-run-end-without-materialization-rejected",
  "TAB0-copy-policy-if-needed-device-transfer",
  "TAB0-copy-without-target-stays-on-device",
  "TAB0-copy-never-device-mismatch-rejected",
  "TAB0-lossy-conversion-requires-mapping",
  "TAB0-schema-reorder-requires-explicit-mapping",
  "TAB0-schema-reorder-explicit-mapping-accepted",
  "TAB0-stream-keeps-schema-identity",
  "TAB0-stream-schema-change-rejected",
  "TAB0-foreign-owner-releases-after-drain",
  "TAB0-foreign-release-before-view-drain-rejected",
  "TAB0-trusted-foreign-structure-still-validated",
  "TAB0-release-exactly-once-rejected",
  "TAB0-owned-export-consumes-owner",
  "TAB0-borrowed-export-is-scoped",
  "TAB0-owned-export-blocks-local-release",
  "TAB0-owned-export-blocks-local-borrow",
  "TAB0-untrusted-input-validates-bounds",
  "TAB0-untrusted-invalid-utf8-rejected",
  "TAB0-untrusted-invalid-offset-rejected",
  "TAB0-null-slots-sanitized-before-boundary",
  "TAB0-null-slot-initialized-marker-accepted",
  "TAB0-null-slots-nonzero-rejected",
  "TAB0-null-slot-uninitialized-marker-rejected",
  "TAB0-limits-cover-all-counts",
  "TAB0-aggregate-buffer-limit-rejected",
  "TAB0-total-bytes-limit-rejected",
  "TAB0-allocation-bytes-limit-rejected",
  "TAB0-string-bytes-limit-rejected",
  "TAB0-batch-byte-facts-required",
  "TAB0-overflow-fails-before-publication",
  "TAB0-arithmetic-overflow-fails-before-publication",
  "TAB0-schema-metadata-does-not-change-identity",
  "TAB0-extension-without-adapter-stays-opaque",
  "TAB0-extension-nominal-bind-needs-adapter",
  "TAB0-extension-static-bind-with-adapter",
  "TAB0-dlpacks-remain-tensor-direction",
  "TAB0-dlpacks-tabular-carrier-rejected",
  "TAB0-adapter-signatures-deferred-to-tab1",
  "TAB0-adapter-deferral-requires-formats",
  "TAB0-adapter-deferral-rejects-dlpack",
  "TAB0-adapter-deferral-rejects-order",
]);

const errors = [];
const caseIds = new Set();
const decisionCoverage = new Map(requiredDecisionIds.map((id) => [id, 0]));
const results = [];
let operationCount = 0;

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

function resolveReference(reference, location) {
  if (!requireString(reference?.path, `${location}.path`)) return;
  const resolved = path.resolve(toolingDirectory, reference.path);
  const relative = path.relative(wDirectory, resolved);
  if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
    errors.push(`${location}.path must stay inside the W repository.`);
    return;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${location}.path references a missing file.`);
    return;
  }
  if (
    requireString(reference.symbol, `${location}.symbol`) &&
    !fs.readFileSync(resolved, "utf8").includes(reference.symbol)
  ) {
    errors.push(`${location}.symbol is absent from ${reference.path}.`);
  }
}

function compactTrace(trace) {
  return trace.map((step) => ({
    index: step.index,
    operation: step.operation,
    ...(step.rejected ? { rejected: step.rejected } : { accepted: true }),
  }));
}

if (corpus.$schema !== "w-tabular-carrier-cases-1") {
  errors.push("tabular-carrier-cases.json must use schema w-tabular-carrier-cases-1.");
}
if (corpus.status !== "design-oracle-input") {
  errors.push("tabular-carrier-cases.json must have status design-oracle-input.");
}
if (corpus.machine !== "tabular-carrier-machine-tab0") {
  errors.push("tabular-carrier-cases.json must name the independent TAB0 machine.");
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  errors.push("tabular-carrier-cases.json must contain cases.");
}

for (const [caseIndex, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${caseIndex}]`;
  if (!/^TAB0-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase.id ?? "")) {
    errors.push(`${location}.id must use the TAB0-kebab-case form.`);
  } else if (caseIds.has(testCase.id)) {
    errors.push(`${location}.id duplicates ${testCase.id}.`);
  } else {
    caseIds.add(testCase.id);
  }

  if (!Array.isArray(testCase.references) || testCase.references.length === 0) {
    errors.push(`${location}.references must link to Last Light.`);
  } else {
    testCase.references.forEach((reference, referenceIndex) =>
      resolveReference(reference, `${location}.references[${referenceIndex}]`),
    );
  }

  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) {
    errors.push(`${location}.decisions must contain ledger IDs.`);
  } else {
    const localDecisions = new Set();
    for (const [decisionIndex, decision] of testCase.decisions.entries()) {
      const decisionLocation = `${location}.decisions[${decisionIndex}]`;
      if (!requireString(decision, decisionLocation)) continue;
      if (!designDecisionIds.has(decision)) errors.push(`${decisionLocation} references missing ledger entry ${decision}.`);
      if (!localDecisions.has(decision) && decisionCoverage.has(decision)) decisionCoverage.set(decision, decisionCoverage.get(decision) + 1);
      if (localDecisions.has(decision)) errors.push(`${decisionLocation} repeats ${decision}.`);
      localDecisions.add(decision);
    }
  }

  if (!Array.isArray(testCase.operations) || testCase.operations.length === 0) {
    errors.push(`${location}.operations must not be empty.`);
    continue;
  }
  operationCount += testCase.operations.length;
  testCase.operations.forEach((operation, operationIndex) => {
    if (!validateTabularCarrierOperation(operation)) {
      errors.push(`${location}.operations[${operationIndex}] is malformed.`);
    }
  });

  if (!testCase.expected || !["accepted", "rejected"].includes(testCase.expected.status)) {
    errors.push(`${location}.expected.status must be accepted or rejected.`);
    continue;
  }
  if (testCase.expected.status === "accepted") {
    if (testCase.expected.code !== undefined || testCase.expected.operation !== undefined) {
      errors.push(`${location}.expected accepted outcome cannot contain rejection fields.`);
    }
  } else if (
    !requireString(testCase.expected.code, `${location}.expected.code`) ||
    !Number.isInteger(testCase.expected.operation) ||
    testCase.expected.operation < 0
  ) {
    errors.push(`${location}.expected rejection must identify code and operation.`);
  }

  const actual = runTabularCarrierProgram(testCase.operations);
  for (const field of ["status", "code", "operation"]) {
    if (actual[field] !== testCase.expected[field]) {
      errors.push(
        `${location}.expected.${field} is ${JSON.stringify(testCase.expected[field])}; ` +
          `actual is ${JSON.stringify(actual[field])}.`,
      );
    }
  }
  results.push({
    caseId: testCase.id,
    status: actual.status,
    ...(actual.code ? { code: actual.code, operation: actual.operation } : {}),
    state: actual.state,
    trace: compactTrace(actual.trace),
  });
}

for (const requiredCaseId of requiredCaseIds) {
  if (!caseIds.has(requiredCaseId)) errors.push(`Missing required TAB0 case ${requiredCaseId}.`);
}
for (const [decision, count] of decisionCoverage.entries()) {
  if (count === 0) errors.push(`TAB0 decision ${decision} has no case coverage.`);
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const expectedSnapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
const acceptedCount = results.filter((result) => result.status === "accepted").length;
const rejectedCount = results.length - acceptedCount;
const summary =
  `Tabular carrier TAB0: ${results.length} cases, ${operationCount} operations, ` +
  `${acceptedCount} accepted, ${rejectedCount} rejected.`;

if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, expectedSnapshot);
  process.stdout.write(`${summary}\nUpdated ${path.basename(snapshotPath)}.\n`);
  process.exit(0);
}
if (!fs.existsSync(snapshotPath)) {
  process.stderr.write(`${path.basename(snapshotPath)} is missing; run with --write.\n`);
  process.exit(1);
}
if (fs.readFileSync(snapshotPath, "utf8") !== expectedSnapshot) {
  process.stderr.write(`${path.basename(snapshotPath)} is stale; run with --write.\n`);
  process.exit(1);
}
process.stdout.write(`${summary}\n`);
