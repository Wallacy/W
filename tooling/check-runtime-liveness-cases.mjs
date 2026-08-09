import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  runRuntimeLivenessProgram,
  validateRuntimeLivenessOperation,
} from "./runtime-liveness-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "runtime-liveness-cases.json");
const snapshotPath = path.join(toolingDirectory, "runtime-liveness-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const designDecisionIds = new Set(
  [...fs.readFileSync(path.join(wDirectory, "DESIGN.md"), "utf8").matchAll(/^\| (W-\d{3,}) \|/gm)].map(
    (match) => match[1],
  ),
);
const errors = [];
const caseIds = new Set();
const results = [];
let operationCount = 0;

const requiredCaseIds = new Set([
  "E1-closure-full-drain-and-split-reclaim",
  "E1-empty-scope-finishes-cleanup",
  "E1-outcome-before-cleanup-rejected",
  "E1-cleanup-lifo-rejected",
  "E1-typed-drop-order-rejected",
  "E1-cleanup-created-wait-drains",
  "E1-cleanup-wait-must-drain",
  "E1-cleanup-frame-wait-rejected",
  "E1-cleanup-wait-without-node-rejected",
  "E1-child-admission-after-body-settled-rejected",
  "E1-wait-admission-after-body-settled-rejected",
  "E1-preexisting-wait-must-drain-before-children",
  "E1-cleanup-budget-rejected",
  "E1-cleanup-error-is-diagnostic",
  "E1-cleanup-panic-no-normal-outcome",
  "E1-cleanup-deadline-no-normal-outcome",
  "E1-provider-success-wins-cancel-request",
  "E1-provider-canceled-after-request",
  "E1-late-completion-suppresses-callback",
  "E1-completion-before-cancel-request",
  "E1-cancel-idempotent-after-commit",
  "E1-stale-generation-does-not-resume-new-slot",
  "E1-child-drain-required",
  "E1-child-drain-then-closure",
  "E1-runtime-resource-drain-required",
  "E1-frame-reclaim-splits-from-join",
  "E1-frame-runtime-ref-blocks-reclaim",
  "E1-blocking-unbounded-adapter-rejected",
  "E1-blocking-kill-is-boundary-failure",
  "E1-premature-frame-reclaim-rejected",
  "E1-shutdown-admission-closes",
  "E1-shutdown-graceful-sequence",
  "E1-shutdown-quiescence-waits-for-root",
  "E1-shutdown-grace-escalates-boundary",
  "E1-stale-completion-after-drain-is-late",
  "E1-cancel-request-is-not-completion",
  "E1-cancel-before-submit-drains-locally",
  "E1-retain-then-expire-outcome-cell",
  "E1-cleanup-child-cannot-escape-node",
  "E1-generation-completion-does-not-cross-service",
  "E1-external-deadlock-detector-not-promised",
]);

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

function resolveReference(reference, location) {
  if (!requireString(reference.path, `${location}.path`)) return;
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

function compactState(state) {
  return {
    tasks: state.tasks,
    waits: state.waits,
    resources: state.resources,
    generations: state.generations,
    shutdown: state.shutdown,
    admittedStarts: state.admittedStarts,
    hostCleanupRegistryReleased: state.hostCleanupRegistryReleased,
  };
}

if (corpus.$schema !== "w-runtime-liveness-cases-1") {
  errors.push("runtime-liveness-cases.json must use schema w-runtime-liveness-cases-1.");
}
if (corpus.status !== "design-oracle-input") {
  errors.push("runtime-liveness-cases.json must have status design-oracle-input.");
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  errors.push("runtime-liveness-cases.json must contain cases.");
}

for (const [caseIndex, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${caseIndex}]`;
  if (!/^E1-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase.id ?? "")) {
    errors.push(`${location}.id must use the E1-kebab-case form.`);
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
    if (!validateRuntimeLivenessOperation(operation)) {
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

  const actual = runRuntimeLivenessProgram(testCase.operations);
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
    state: compactState(actual.state),
    trace: actual.trace.map((step) => ({
      index: step.index,
      op: step.operation.op,
      ...(step.operation.task ? { task: step.operation.task } : {}),
      ...(step.operation.operationId ? { operationId: step.operation.operationId } : {}),
      ...(step.rejected ? { rejected: step.rejected } : { accepted: true }),
    })),
  });
}

for (const caseId of requiredCaseIds) {
  if (!caseIds.has(caseId)) errors.push(`Missing required E1 case ${caseId}.`);
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const expectedSnapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
const acceptedCount = results.filter((result) => result.status === "accepted").length;
const rejectedCount = results.length - acceptedCount;
const summary =
  `Runtime liveness E1: ${results.length} cases, ${operationCount} operations, ` +
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
