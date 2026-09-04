import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  assertNoRuntimeLifetimeMetadata,
  evaluateBorrowCase,
} from "./borrow-expressivity-machine.mjs";
import { ledgerIdSet as designDecisionIds } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "borrow-expressivity-cases.json");
const snapshotPath = path.join(toolingDirectory, "borrow-expressivity-results.snapshot.jsonl");
const writeSnapshot = process.argv.includes("--write");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const errors = [];
const caseIds = new Set();
const results = [];

function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

function symbolOccurrences(source, symbol) {
  const escaped = symbol.replace(/[.*+?^${}()|[\\]\\]/g, "\\\\$&");
  return [...source.matchAll(new RegExp("\\b" + escaped + "\\b", "gu"))].length;
}

function resolveReference(reference, location) {
  if (!requireString(reference?.path, `${location}.path`)) return undefined;
  const resolved = path.resolve(toolingDirectory, reference.path);
  const relative = path.relative(wDirectory, resolved);
  if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
    errors.push(`${location}.path must stay inside the W repository.`);
    return undefined;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${location}.path references a missing file.`);
    return undefined;
  }
  if (reference.symbol === undefined && Array.isArray(reference.symbols)) return resolved;
  if (requireString(reference.symbol, `${location}.symbol`)) {
    const source = fs.readFileSync(resolved, "utf8");
    const occurrences = symbolOccurrences(source, reference.symbol);
    if (occurrences === 0) {
      errors.push(`${location}.symbol is absent from ${reference.path}.`);
    } else if (occurrences !== 1) {
      errors.push(`${location}.symbol must occur exactly once in ${reference.path}; found ${occurrences}.`);
    }
  }
  return resolved;
}

function compare(actual, expected, location) {
  if (actual !== expected) errors.push(`${location} expected ${JSON.stringify(expected)}; got ${JSON.stringify(actual)}.`);
}

function compareObjectFields(actual, expected, location) {
  for (const [field, value] of Object.entries(expected ?? {})) {
    if (field === "freshLoanCount") {
      compare(actual?.freshLoans?.length, value, `${location}.${field}`);
    } else if (field === "invocationEdgeCount") {
      compare(actual?.invocationEdges?.length, value, `${location}.${field}`);
    } else if (field === "erasure") {
      if (value === "preserves-mapping") {
        const preserved = actual?.erasure?.representation === "any-fn" &&
          Array.isArray(actual.erasure?.mapping?.result) &&
          actual.erasure.mapping.result.length > 0;
        if (!preserved) errors.push(`${location}.erasure must preserve a non-empty mapping.`);
      } else if (value === "accepted-storage") {
        if (actual?.erasure?.representation !== "any-fn") {
          errors.push(`${location}.erasure must record any-fn storage.`);
        }
      } else {
        compare(actual?.erasure, value, `${location}.${field}`);
      }
    } else {
      compare(actual?.[field], value, `${location}.${field}`);
    }
  }
}

function rejectExpectedMappingEcho(value, location) {
  if (!value || typeof value !== "object") return;
  for (const [key, child] of Object.entries(value)) {
    if (/^(mapping|edges|baseline|relational)$/iu.test(key)) {
      errors.push(`${location}.${key} repeats an oracle-derived mapping; expected outcomes must not echo it.`);
    }
    rejectExpectedMappingEcho(child, `${location}.${key}`);
  }
}

function checkExpected(testCase, result, location) {
  const expected = testCase.expected ?? {};
  rejectExpectedMappingEcho(expected, `${location}.expected`);
  compare(result.decision, expected.decision, `${location}.expected.decision`);
  if (expected.baselineError !== undefined) {
    compare(result.mapping.baselineError?.code, expected.baselineError, `${location}.expected.baselineError`);
  }
  if (expected.relationalExact !== undefined) {
    compare(result.mapping.relationalExact, expected.relationalExact, `${location}.expected.relationalExact`);
  }
  compareObjectFields(result.invocation, expected.invocation, `${location}.expected.invocation`);
  if (expected.artifacts) {
    compare(result.artifacts.status, expected.artifacts.status, `${location}.expected.artifacts.status`);
    if (expected.artifacts.codes) {
      const actualCodes = result.artifacts.diagnostics.map((diagnostic) => diagnostic.code).sort();
      compare(actualCodes.join("\0"), [...expected.artifacts.codes].sort().join("\0"), `${location}.expected.artifacts.codes`);
    }
  }
  for (const [field, value] of Object.entries(expected.forms ?? {})) {
    compare(result.forms[field], value, `${location}.expected.forms.${field}`);
  }
  if (expected.runtimeLifetimeMetadataCount !== undefined) {
    compare(result.runtimeLifetimeMetadata.length, expected.runtimeLifetimeMetadataCount, `${location}.expected.runtimeLifetimeMetadataCount`);
  }
  if (!assertNoRuntimeLifetimeMetadata(result)) {
    errors.push(`${location} emitted runtime lifetime metadata.`);
  }
}

function snapshotText() {
  const header = {
    schema: "w-borrow-expressivity-results-brx0",
    status: "design-oracle-output-brx0",
    machine: corpus.machine,
    caseCount: results.length,
  };
  return `${JSON.stringify(header)}\n${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
}

if (corpus.$schema !== "w-borrow-expressivity-cases-1") {
  errors.push("borrow-expressivity-cases.json must use schema w-borrow-expressivity-cases-1.");
}
if (corpus.status !== "design-oracle-input-brx0") {
  errors.push("borrow-expressivity-cases.json must have status design-oracle-input-brx0.");
}
if (corpus.machine !== "borrow-expressivity-machine-brx0") {
  errors.push("borrow-expressivity-cases.json must name borrow-expressivity-machine-brx0.");
}
if (!Array.isArray(corpus.cases) || corpus.cases.length < 12) {
  errors.push("borrow-expressivity-cases.json must contain at least 12 adversarial cases.");
}

const sourceBase = resolveReference(corpus.sourceBase, "sourceBase");
if (sourceBase) {
  const actualDigest = digestFile(sourceBase);
  if (actualDigest !== corpus.sourceBase.digest) {
    errors.push(`sourceBase.digest is stale; expected ${actualDigest}.`);
  }
  for (const [index, symbol] of (corpus.sourceBase.symbols ?? []).entries()) {
    if (!requireString(symbol, `sourceBase.symbols[${index}]`)) continue;
    const source = fs.readFileSync(sourceBase, "utf8");
    const occurrences = symbolOccurrences(source, symbol);
    if (occurrences === 0) errors.push(`sourceBase.symbols[${index}] is absent from sourceBase.`);
    if (occurrences !== 1) errors.push(`sourceBase.symbols[${index}] must occur exactly once; found ${occurrences}.`);
  }
}
if (new Set(corpus.sourceBase?.symbols ?? []).size !== (corpus.sourceBase?.symbols ?? []).length) {
  errors.push("sourceBase.symbols must not contain duplicates.");
}

const seenCoverage = new Set();
for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  if (!/^BRX0-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase.id ?? "")) {
    errors.push(`${location}.id must use the BRX0-kebab-case form.`);
  } else if (caseIds.has(testCase.id)) {
    errors.push(`${location}.id duplicates ${testCase.id}.`);
  } else {
    caseIds.add(testCase.id);
  }
  const referenceFile = resolveReference(testCase.source, `${location}.source`);
  if (referenceFile && sourceBase && path.resolve(referenceFile) !== path.resolve(sourceBase)) {
    errors.push(`${location}.source must use sourceBase.path.`);
  }
  if (!Array.isArray(testCase.decisionIds) || testCase.decisionIds.length === 0) {
    errors.push(`${location}.decisionIds must link to design ledger entries.`);
  } else {
    for (const [decisionIndex, decision] of testCase.decisionIds.entries()) {
      if (!designDecisionIds.has(decision)) errors.push(`${location}.decisionIds[${decisionIndex}] references missing ${decision}.`);
    }
  }
  if (!Array.isArray(testCase.coverage) || testCase.coverage.length === 0) {
    errors.push(`${location}.coverage must not be empty.`);
  } else {
    testCase.coverage.forEach((item) => seenCoverage.add(item));
  }
  if (!testCase.declaration || typeof testCase.declaration !== "object") {
    errors.push(`${location}.declaration is required.`);
    continue;
  }
  const result = evaluateBorrowCase(testCase);
  checkExpected(testCase, result, location);
  results.push({
    id: result.id,
    decision: result.decision,
    baselineExact: result.mapping.baselineExact,
    relationalExact: result.mapping.relationalExact,
    baseline: result.mapping.baseline,
    relational: result.mapping.relational,
    baselineEdges: result.mapping.baselineEdges,
    relationalEdges: result.mapping.relationalEdges,
    baselineOriginSets: result.mapping.baselineOriginSets,
    relationalOriginSets: result.mapping.relationalOriginSets,
    baselineError: result.mapping.baselineError,
    relationalError: result.mapping.relationalError,
    invocation: result.invocation,
    artifacts: result.artifacts,
    forms: result.forms,
    runtimeLifetimeMetadata: result.runtimeLifetimeMetadata,
    digest: result.digest,
  });
}

for (const required of corpus.coverage ?? []) {
  if (!seenCoverage.has(required)) errors.push(`corpus coverage is missing ${required}.`);
}

if (writeSnapshot) {
  fs.writeFileSync(snapshotPath, snapshotText());
} else if (!fs.existsSync(snapshotPath)) {
  errors.push("borrow-expressivity snapshot is missing; run with --write.");
} else if (fs.readFileSync(snapshotPath, "utf8") !== snapshotText()) {
  errors.push("borrow-expressivity snapshot is stale; run with --write.");
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

process.stdout.write(
  `BRX0 borrow expressivity: ${results.length} cases, ` +
  `${results.filter((result) => result.decision === "accepted").length} accepted, ` +
  `${results.filter((result) => result.decision === "historical-candidate").length} historical candidate routes, ` +
  `${results.filter((result) => result.invocation.status === "rejected").length} invocation negatives.\n`,
);
