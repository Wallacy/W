import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  runLayoutAbiProgram,
  validateLayoutAbiOperation,
} from "./layout-abi-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "layout-abi-cases.json");
const snapshotPath = path.join(toolingDirectory, "layout-abi-results.snapshot.jsonl");
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

if (corpus.$schema !== "w-layout-abi-cases-l0") {
  errors.push("layout-abi-cases.json must use schema w-layout-abi-cases-l0.");
}
if (corpus.status !== "design-oracle-input-l0") {
  errors.push("layout-abi-cases.json must have status design-oracle-input-l0.");
}
if (corpus.machine !== "layout-abi-machine-l0") {
  errors.push("layout-abi-cases.json must name layout-abi-machine-l0.");
}
if (corpus.fixtures === null || typeof corpus.fixtures !== "object") {
  errors.push("layout-abi-cases.json must contain fixtures.");
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  errors.push("layout-abi-cases.json must contain cases.");
}

for (const [caseIndex, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${caseIndex}]`;
  if (!/^L0-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase.id ?? "")) {
    errors.push(`${location}.id must use the L0-kebab-case form.`);
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
    if (!validateLayoutAbiOperation(operation)) {
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

  const actual = runLayoutAbiProgram(testCase.operations, corpus.fixtures);
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
  JSON.stringify({ schema: "w-layout-abi-results-l0", status: "design-oracle-output-l0" }),
  ...results.map((result) => JSON.stringify(result)),
].join("\n") + "\n";
const acceptedCount = results.filter((result) => result.status === "accepted").length;
const rejectedCount = results.length - acceptedCount;
const summary =
  `Layout and ABI L0: ${results.length} cases, ${operationCount} operations, ` +
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
