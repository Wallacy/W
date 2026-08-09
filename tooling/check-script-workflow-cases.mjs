import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIdSet } from "./design-ledger.mjs";
import {
  runScriptWorkflowProgram,
  validateScriptWorkflowOperation,
} from "./script-workflow-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "script-workflow-cases.json");
const snapshotPath = path.join(toolingDirectory, "script-workflow-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const errors = [];
const caseIds = new Set();
const coveredOperations = new Set();
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
  if (!reference || typeof reference !== "object") {
    errors.push(`${location} must be an object.`);
    return;
  }
  if (!requireString(reference.path, `${location}.path`)) return;
  if (!requireString(reference.symbol, `${location}.symbol`)) return;
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
  if (!fs.readFileSync(resolved, "utf8").includes(reference.symbol)) {
    errors.push(`${location}.symbol is absent from ${reference.path}.`);
  }
}

function expandFixture(name, stack = []) {
  if (stack.includes(name)) {
    errors.push(`Fixture cycle: ${[...stack, name].join(" -> ")}.`);
    return [];
  }
  const fixture = corpus.fixtures?.[name];
  if (!fixture) {
    errors.push(`Unknown fixture ${name}.`);
    return [];
  }
  return [
    ...(fixture.includes ?? []).flatMap((included) => expandFixture(included, [...stack, name])),
    ...(fixture.operations ?? []),
  ];
}

function compactState(state) {
  return {
    phase: state.phase,
    source: {
      path: state.source.path,
      kind: state.source.kind,
      headerDigest: state.source.headerDigest,
      parseEvidence: state.source.parseEvidence,
      textDigest: state.source.textDigest,
      entry: state.source.entry,
    },
    context: {
      mode: state.context.mode,
      reason: state.context.reason,
      packageRoot: state.context.packageRoot,
      explanation: state.context.explanation,
    },
    roots: state.roots,
    imports: state.imports,
    resolution: {
      validated: state.resolution.validated,
      digest: state.resolution.digest,
      rootDigest: state.resolution.rootDigest,
      authority: state.resolution.authority,
      selectionDigest: state.resolution.selectionDigest,
      selectedContext: state.resolution.selectedContext,
      closure: state.resolution.closure,
      cas: state.resolution.cas,
      packages: state.resolution.packages,
      artifacts: state.resolution.artifacts,
      requiredHandleRecordDigests: state.resolution.requiredHandleRecordDigests,
      selectedActionOutputs: state.resolution.selectedActionOutputs,
      consumedActionOutputs: state.resolution.consumedActionOutputs,
    },
    fetches: state.fetches,
    artifacts: state.artifacts,
    capabilities: state.capabilities,
    product: state.product,
    run: state.run,
    cleanup: state.cleanup,
    edits: state.edits,
    promotion: state.promotion,
  };
}

if (corpus.$schema !== "w-script-workflow-cases-1") errors.push("unexpected script workflow schema");
if (corpus.status !== "design-oracle-input") errors.push("script workflow corpus must be design-oracle-input");
if (corpus.machine !== "script-workflow-machine-pyn1") errors.push("script workflow machine name is invalid");
if (!Array.isArray(corpus.decisions) || corpus.decisions.length === 0) {
  errors.push("script workflow corpus must declare ledger IDs");
} else {
  for (const decision of corpus.decisions) {
    if (!/^W-10(4[6-9]|[5-6][0-9]|7[0-5])$/.test(decision) || !ledgerIdSet.has(decision)) {
      errors.push(`unknown PYN1 ledger decision ${decision}`);
    }
  }
}
if (!corpus.fixtures || typeof corpus.fixtures !== "object") errors.push("fixtures are required");
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) errors.push("cases are required");

for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  if (!/^PYN1-[a-z0-9-]+$/.test(testCase.id ?? "") || caseIds.has(testCase.id)) {
    errors.push(`${location}.id is invalid or duplicated.`);
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
  const operations = [
    ...(testCase.fixtures ?? []).flatMap((fixture) => expandFixture(fixture)),
    ...(testCase.operations ?? []),
  ];
  if (operations.length === 0) {
    errors.push(`${location}.operations must not be empty.`);
    continue;
  }
  operationCount += operations.length;
  operations.forEach((operation, operationIndex) => {
    if (!validateScriptWorkflowOperation(operation)) {
      errors.push(`${location}.operations[${operationIndex}] is malformed.`);
    } else {
      coveredOperations.add(operation.op);
    }
  });
  if (!testCase.expected || !["accepted", "rejected"].includes(testCase.expected.status)) {
    errors.push(`${location}.expected.status must be accepted or rejected.`);
    continue;
  }
  if (testCase.expected.status === "rejected") {
    if (!requireString(testCase.expected.code, `${location}.expected.code`) || testCase.expected.at !== "last") {
      errors.push(`${location}.expected rejection must identify code at the last operation.`);
    }
  }
  const actual = runScriptWorkflowProgram(operations);
  if (actual.status !== testCase.expected.status) {
    errors.push(`${testCase.id} expected ${testCase.expected.status}, got ${actual.status}.`);
  }
  if (testCase.expected.status === "rejected") {
    if (actual.code !== testCase.expected.code) {
      errors.push(`${testCase.id} expected ${testCase.expected.code}, got ${actual.code}.`);
    }
    if (actual.operation !== operations.length - 1) {
      errors.push(`${testCase.id} rejected before the final operation.`);
    }
  }
  results.push({
    caseId: testCase.id,
    status: actual.status,
    ...(actual.code ? { code: actual.code, operation: actual.operation } : {}),
    state: compactState(actual.state),
    trace: actual.state.trace,
  });
}

const requiredOperations = new Set([
  "parseHeader",
  "selectContext",
  "resolveRoots",
  "validateImports",
  "validateResolution",
  "admitFetch",
  "verifyArtifact",
  "admitCapabilities",
  "buildEphemeral",
  "runEntry",
  "cleanup",
  "contextExplanation",
  "scriptAdd",
  "scriptRemove",
  "scriptResolve",
  "promote",
]);
for (const operation of requiredOperations) {
  if (!coveredOperations.has(operation)) errors.push(`corpus does not cover ${operation}`);
}
const accepted = results.filter((result) => result.status === "accepted").length;
const rejected = results.length - accepted;
if (accepted === 0 || rejected === 0) errors.push("corpus must contain accepted and rejected cases");

const expectedSnapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, expectedSnapshot);
} else if (!fs.existsSync(snapshotPath)) {
  errors.push("snapshot is missing; run with --write");
} else if (fs.readFileSync(snapshotPath, "utf8") !== expectedSnapshot) {
  errors.push("snapshot is stale; run with --write after reviewing the change");
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

process.stdout.write(
  `Script workflow oracle: ${results.length} cases, ${operationCount} operations, ${accepted} accepted, ${rejected} rejected.\n`,
);
