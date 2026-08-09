import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  runExecutionProgram,
  validateExecutionOperation,
} from "./execution-concurrency-machine.mjs";
import { ledgerIdSet as designDecisionIds } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "execution-concurrency-cases.json");
const snapshotPath = path.join(
  toolingDirectory,
  "execution-concurrency-results.snapshot.jsonl",
);
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const errors = [];
const caseIds = new Set();
const results = [];
let operationCount = 0;
const requiredSynchronizationEdges = new Set([
  "parentToChild",
  "childToJoin",
  "channelSendCommit",
  "channelCapacityRelease",
  "channelCloseCommit",
  "serviceCallToTurn",
  "lockReleaseAcquire",
  "atomicReleaseAcquire",
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
    events: state.events,
    edges: state.edges,
    ...(Object.keys(state.storageLifetimes).length > 0
      ? { storageLifetimes: state.storageLifetimes }
      : {}),
    ...(Object.keys(state.atomicModificationOrder).length > 0
      ? { atomicModificationOrder: state.atomicModificationOrder }
      : {}),
    ...(state.sequentialOrder.length > 0 ? { sequentialOrder: state.sequentialOrder } : {}),
    ...(Object.keys(state.atomicExclusive).length > 0
      ? { atomicExclusive: state.atomicExclusive }
      : {}),
    lastFailFastTrigger: state.lastFailFastTrigger,
    lastArbitration: state.lastArbitration,
    lastRace: state.lastRace,
  };
}

if (corpus.$schema !== "w-execution-concurrency-cases-1") {
  errors.push(
    "execution-concurrency-cases.json must use schema w-execution-concurrency-cases-1.",
  );
}
if (corpus.status !== "design-oracle-input") {
  errors.push("execution-concurrency-cases.json must have status design-oracle-input.");
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  errors.push("execution-concurrency-cases.json must contain cases.");
}

for (const [caseIndex, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${caseIndex}]`;
  if (!/^E0-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase.id ?? "")) {
    errors.push(`${location}.id must use the E0-kebab-case form.`);
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

  if (testCase.decisions !== undefined) {
    if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) {
      errors.push(`${location}.decisions must be a non-empty array when present.`);
    } else {
      const localDecisions = new Set();
      for (const [decisionIndex, decision] of testCase.decisions.entries()) {
        const decisionLocation = `${location}.decisions[${decisionIndex}]`;
        if (!requireString(decision, decisionLocation)) continue;
        if (!designDecisionIds.has(decision)) {
          errors.push(`${decisionLocation} references missing ledger entry ${decision}.`);
        }
        if (localDecisions.has(decision)) {
          errors.push(`${decisionLocation} repeats ${decision}.`);
        }
        localDecisions.add(decision);
      }
    }
  }

  if (!Array.isArray(testCase.operations) || testCase.operations.length === 0) {
    errors.push(`${location}.operations must not be empty.`);
    continue;
  }
  operationCount += testCase.operations.length;
  testCase.operations.forEach((operation, operationIndex) => {
    if (!validateExecutionOperation(operation)) {
      errors.push(`${location}.operations[${operationIndex}] is malformed.`);
    }
  });

  if (!["accepted", "rejected"].includes(testCase.expected?.status)) {
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

  const actual = runExecutionProgram(testCase.operations);
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
      ...(step.operation.id ? { id: step.operation.id } : {}),
      ...(step.rejected ? { rejected: step.rejected } : { accepted: true }),
    })),
  });
}

const edgeKinds = new Set(results.flatMap((result) => result.state.edges.map((edge) => edge.kind)));
for (const edge of requiredSynchronizationEdges) {
  if (!edgeKinds.has(edge)) errors.push(`The E0 corpus does not cover ${edge}.`);
}
for (const edge of edgeKinds) {
  if (edge !== "sequencedBefore" && !requiredSynchronizationEdges.has(edge)) {
    errors.push(`The E0 corpus produced unknown synchronization edge ${edge}.`);
  }
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const expectedSnapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
const acceptedCount = results.filter((result) => result.status === "accepted").length;
const rejectedCount = results.length - acceptedCount;
const summary =
  `Execution concurrency: ${results.length} cases, ${operationCount} operations, ` +
  `${acceptedCount} accepted, ${rejectedCount} rejected, ` +
  `${requiredSynchronizationEdges.size}/8 happens-before origins.`;

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
