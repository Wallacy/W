import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  runAllocationProgram,
  validateAllocationOperation,
} from "./allocation-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "allocation-cases.json");
const snapshotPath = path.join(toolingDirectory, "allocation-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const designDecisionIds = new Set(
  [...fs.readFileSync(path.join(wDirectory, "DESIGN.md"), "utf8").matchAll(/^\| (W-\d{3,}) \|/gm)]
    .map((match) => match[1]),
);
const errors = [];
const caseIds = new Set();
const results = [];
let operationCount = 0;

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

function validateReference(referenceId, location) {
  if (!requireString(referenceId, location)) return;
  const reference = corpus.references?.[referenceId];
  if (reference === undefined) {
    errors.push(`${location} names missing reference ${referenceId}.`);
    return;
  }
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

function validateProvider(name, profile) {
  const location = `fixtures.providers.${name}`;
  for (const field of ["maximumBytes", "maximumAlignment", "sizeClass"]) {
    if (!Number.isSafeInteger(profile?.[field]) || profile[field] <= 0) {
      errors.push(`${location}.${field} must be a positive safe integer.`);
    }
  }
  for (const [field, allowed] of [
    ["failureMode", ["return", "abort"]],
    ["resize", ["none", "inPlace", "remap"]],
    ["mobility", ["local", "crossDomain"]],
  ]) {
    if (!allowed.includes(profile?.[field])) {
      errors.push(`${location}.${field} has an unsupported value.`);
    }
  }
  requireString(profile?.allocateDomain, `${location}.allocateDomain`);
  requireString(profile?.deallocateDomain, `${location}.deallocateDomain`);
  for (const operation of ["allocate", "resize", "deallocate"]) {
    if (!new Set(["general", "bounded", "lockFree", "waitFree"]).has(
      profile?.progress?.[operation],
    )) {
      errors.push(`${location}.progress.${operation} has an unsupported value.`);
    }
  }
  for (const field of ["movesOnResize", "deferredReuse", "bulkRelease"]) {
    if (typeof profile?.[field] !== "boolean") {
      errors.push(`${location}.${field} must be Boolean.`);
    }
  }
  requireString(profile?.homeDomain, `${location}.homeDomain`);
}

if (corpus.$schema !== "w-physical-allocation-cases-a0") {
  errors.push("allocation-cases.json must use schema w-physical-allocation-cases-a0.");
}
if (corpus.status !== "design-oracle-input-a0") {
  errors.push("allocation-cases.json must have status design-oracle-input-a0.");
}
if (corpus.machine !== "allocation-machine-a0") {
  errors.push("allocation-cases.json must name allocation-machine-a0.");
}
if (corpus.fixtures?.providers === null || typeof corpus.fixtures?.providers !== "object") {
  errors.push("allocation-cases.json must contain provider fixtures.");
} else {
  for (const [name, profile] of Object.entries(corpus.fixtures.providers)) {
    validateProvider(name, profile);
  }
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  errors.push("allocation-cases.json must contain cases.");
}

for (const [caseIndex, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${caseIndex}]`;
  if (!/^A0-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase.id ?? "")) {
    errors.push(`${location}.id must use the A0-kebab-case form.`);
  } else if (caseIds.has(testCase.id)) {
    errors.push(`${location}.id duplicates ${testCase.id}.`);
  } else {
    caseIds.add(testCase.id);
  }

  if (!Array.isArray(testCase.references) || testCase.references.length === 0) {
    errors.push(`${location}.references must link to Last Light.`);
  } else {
    testCase.references.forEach((reference, referenceIndex) =>
      validateReference(reference, `${location}.references[${referenceIndex}]`),
    );
  }

  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) {
    errors.push(`${location}.decisions must be a non-empty array.`);
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

  if (!Array.isArray(testCase.operations) || testCase.operations.length === 0) {
    errors.push(`${location}.operations must not be empty.`);
    continue;
  }
  operationCount += testCase.operations.length;
  testCase.operations.forEach((operation, operationIndex) => {
    if (!validateAllocationOperation(operation)) {
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

  const actual = runAllocationProgram(testCase.operations, corpus.fixtures);
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
    trace: actual.trace,
  });
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const expectedSnapshot = [
  JSON.stringify({ schema: "w-physical-allocation-results-a0", status: "design-oracle-output-a0" }),
  ...results.map((result) => JSON.stringify(result)),
].join("\n") + "\n";
const acceptedCount = results.filter((result) => result.status === "accepted").length;
const rejectedCount = results.length - acceptedCount;
const summary =
  `Physical allocation A0: ${results.length} cases, ${operationCount} operations, ` +
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
