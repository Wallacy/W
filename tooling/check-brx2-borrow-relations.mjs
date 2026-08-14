import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIdSet as designDecisionIds } from "./design-ledger.mjs";
import {
  assertNoRuntimeLifetimeMetadata,
  deriveBorrowRelationCorpus,
} from "./brx2-borrow-relations-machine.mjs";
import { validateBRX2StudyManifest } from "./brx2-borrow-relations-manifest.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "brx2-borrow-relations-cases.json");
const snapshotPath = path.join(toolingDirectory, "brx2-borrow-relations-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const studyDirectory = path.join(toolingDirectory, "studies", "brx2-borrow-relations");
const studyManifestPath = path.join(studyDirectory, "study.json");
const studyManifest = JSON.parse(fs.readFileSync(studyManifestPath, "utf8"));
const errors = [];
const writeSnapshot = process.argv.includes("--write");

errors.push(...validateBRX2StudyManifest(studyManifest, { studyDirectory, root }));

function digestFile(file) {
  return "sha256:" + crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex");
}

function contained(relative, base, location) {
  if (typeof relative !== "string" || relative.trim() === "") {
    errors.push(location + " must be a path.");
    return undefined;
  }
  const file = path.resolve(base, relative);
  const rel = path.relative(root, file);
  if (rel.startsWith(".." + path.sep) || path.isAbsolute(rel) ||
    !fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push(location + " must reference a file inside the repository.");
    return undefined;
  }
  return file;
}

function symbolCount(file, symbol) {
  const specials = ".^$*+?()|[]{}\\";
  const escaped = [...String(symbol)].map((char) =>
    specials.includes(char) ? "\\" + char : char).join("");
  return (fs.readFileSync(file, "utf8").match(new RegExp("\\b" + escaped + "\\b", "gu")) ?? []).length;
}

function compare(actual, expected, location) {
  if (actual !== expected) errors.push(location + " expected " + JSON.stringify(expected) + "; got " + JSON.stringify(actual) + ".");
}

function rejectMappingEcho(value, location) {
  if (!value || typeof value !== "object") return;
  for (const [key, child] of Object.entries(value)) {
    if (/^(mapping|baseline|relation|edges|originSets)$/iu.test(key)) {
      errors.push(location + "." + key + " repeats oracle-derived mapping data.");
    }
    rejectMappingEcho(child, location + "." + key);
  }
}

function expectedInvocation(actual, expected, location) {
  for (const [key, value] of Object.entries(expected ?? {})) {
    if (key === "freshLoanCount") compare(actual.freshLoans.length, value, location + ".freshLoanCount");
    else if (key === "invocationEdgeCount") compare(actual.invocationEdges.length, value, location + ".invocationEdgeCount");
    else if (key === "erasure" && value === "preserves-mapping") {
      if (actual.erasure?.representation !== "any-fn" ||
        !actual.erasure.mapping || Object.keys(actual.erasure.mapping).length === 0) {
        errors.push(location + ".erasure must preserve a mapping.");
      }
    } else compare(actual[key], value, location + "." + key);
  }
}

function expectedResult(testCase, result, index) {
  const expected = testCase.expected ?? {};
  const location = "cases[" + index + "].expected";
  rejectMappingEcho(expected, location);
  compare(result.decision, expected.decision, location + ".decision");
  compare(result.route, expected.route, location + ".route");
  if (expected.relationExact !== undefined) compare(result.mapping.relationExact, expected.relationExact, location + ".relationExact");
  expectedInvocation(result.invocation, expected.invocation, location + ".invocation");
  if (Array.isArray(expected.diagnostics)) {
    const actual = result.diagnostics.map((item) => item.code).filter((code) => code !== "relationOmitted").sort().join("\0");
    const declared = expected.diagnostics.filter((code) => code !== "relationOmitted");
    compare(actual, [...declared].sort().join("\0"), location + ".diagnostics");
  }
  if (!assertNoRuntimeLifetimeMetadata(result)) errors.push(location + " emitted runtime lifetime metadata.");
}

if (corpus.$schema !== "w-brx2-borrow-relations-cases-1") errors.push("BRX2 corpus schema is invalid.");
if (corpus.status !== "design-oracle-input-brx2") errors.push("BRX2 corpus status is invalid.");
if (corpus.machine !== "brx2-borrow-relations-machine") errors.push("BRX2 corpus machine is invalid.");
if (!Array.isArray(corpus.cases) || corpus.cases.length < 30) errors.push("BRX2 corpus must contain at least 30 cases.");

const sourceBase = contained(corpus.sourceBase?.path, toolingDirectory, "sourceBase.path");
if (sourceBase) {
  compare(digestFile(sourceBase), corpus.sourceBase.digest, "sourceBase.digest");
  for (const symbol of corpus.sourceBase.symbols ?? []) {
    if (symbolCount(sourceBase, symbol) !== 1) errors.push("sourceBase symbol must occur exactly once: " + symbol);
  }
}

const seenIds = new Set();
const seenCoverage = new Set();
let derived = [];
for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = "cases[" + index + "]";
  if (!/^BRX2-[a-z0-9]+(?:-[a-z0-9]+)*$/u.test(testCase.id ?? "")) errors.push(location + ".id is not BRX2 kebab-case.");
  if (seenIds.has(testCase.id)) errors.push(location + ".id is duplicated.");
  seenIds.add(testCase.id);
  const source = contained(testCase.source?.path, toolingDirectory, location + ".source.path");
  if (source && sourceBase && path.resolve(source) !== path.resolve(sourceBase)) errors.push(location + ".source must use sourceBase.path.");
  if (source && symbolCount(source, testCase.source?.symbol) !== 1) errors.push(location + ".source.symbol must occur exactly once.");
  for (const decision of testCase.decisionIds ?? []) if (!designDecisionIds.has(decision)) errors.push(location + " references missing " + decision + ".");
  for (const item of testCase.coverage ?? []) seenCoverage.add(item);
  if (!testCase.declaration || typeof testCase.declaration !== "object") errors.push(location + ".declaration is required.");
  if (testCase.declaration?.problemTrace !== undefined ||
    testCase.declaration?.problemTraceKind !== undefined) {
    errors.push(location + ".declaration must not carry problemTrace; use case-level assay.");
  }
  if (testCase.assay !== undefined &&
    (testCase.assay?.kind !== "independent-assay" || !Array.isArray(testCase.assay.problemTrace))) {
    errors.push(location + ".assay must be an independent-assay problemTrace.");
  }
}
for (const item of corpus.coverage ?? []) if (!seenCoverage.has(item)) errors.push("corpus coverage is missing " + item + ".");

try {
  derived = deriveBorrowRelationCorpus(corpus);
  for (const [index, result] of derived.entries()) expectedResult(corpus.cases[index], result, index);
} catch (error) {
  errors.push("BRX2 machine failed: " + (error?.message ?? String(error)));
}

const output = {
  schema: "w-brx2-borrow-relations-results-1",
  status: "design-oracle-output-brx2",
  corpus: "tooling/brx2-borrow-relations-cases.json",
  corpusDigest: digestFile(corpusPath),
  metrics: {
    caseCount: derived.length,
    routeCounts: Object.fromEntries([...new Set(derived.map((item) => item.route))].sort().map((route) => [
      route, derived.filter((item) => item.route === route).length,
    ])),
    decisionCounts: Object.fromEntries([...new Set(derived.map((item) => item.decision))].sort().map((decision) => [
      decision, derived.filter((item) => item.decision === decision).length,
    ])),
    relationExactCount: derived.filter((item) => item.mapping.relationExact).length,
    exactResearchCandidateCount: derived.filter((item) => item.mapping.relationExact && item.route === "research").length,
    exactRejectedCount: derived.filter((item) => item.mapping.relationExact && item.decision === "rejected").length,
    invocationNegativeCount: derived.filter((item) => item.invocation.status === "rejected").length,
    declarationDecisionCounts: Object.fromEntries([...new Set(derived.map((item) => item.declarationDecision))].sort().map((decision) => [
      decision, derived.filter((item) => item.declarationDecision === decision).length,
    ])),
    invocationStatusCounts: Object.fromEntries([...new Set(derived.map((item) => item.invocationStatus))].sort().map((status) => [
      status, derived.filter((item) => item.invocationStatus === status).length,
    ])),
    declarationAcceptedInvocationRejectedCount: derived.filter((item) =>
      item.declarationDecision === "accepted" && item.invocationStatus === "rejected").length,
    diagnosticCount: derived.reduce((sum, item) => sum + item.diagnostics.length, 0),
  },
  results: derived.map((result) => ({
    id: result.id, route: result.route, decision: result.decision,
    declarationDecision: result.declarationDecision, invocationStatus: result.invocationStatus,
    assay: result.assay,
    baselineExact: result.mapping.baselineExact, relationExact: result.mapping.relationExact,
    relationApplicable: result.mapping.relationApplicable,
    effectiveMapping: result.mapping.effective, effectiveEdges: result.mapping.effectiveEdges,
    effectiveOriginSets: result.mapping.effectiveOriginSets,
    baseline: result.mapping.baseline, relation: result.mapping.relation,
    baselineEdges: result.mapping.baselineEdges, relationEdges: result.mapping.relationEdges,
    baselineOriginSets: result.mapping.baselineOriginSets, relationOriginSets: result.mapping.relationOriginSets,
    relationDigest: result.mapping.relationDigest, invocation: result.invocation,
    artifacts: result.artifacts, interfaces: result.interfaces, abi: result.abi,
    diagnostics: result.diagnostics, digest: result.digest,
  })),
};

const snapshot = JSON.stringify(output) + "\n";
if (writeSnapshot) fs.writeFileSync(snapshotPath, snapshot, "utf8");
else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  errors.push("BRX2 snapshot is stale; run with --write.");
}

if (errors.length > 0) {
  process.stderr.write(errors.join("\n") + "\n");
  process.exit(1);
}

process.stdout.write(
  "BRX2 borrow relations: " + output.metrics.caseCount + " cases, " +
  output.metrics.relationExactCount + " exact relation candidates, " +
  output.metrics.invocationNegativeCount + " invocation negatives.\n",
);
