import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  compactNotebookExportState,
  notebookDigest,
  runNotebookExportFixtureProgram,
} from "./notebook-export-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const rootDirectory = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "notebook-export-cases.json");
const snapshotPath = path.join(toolingDirectory, "notebook-export-results.snapshot.jsonl");
const designPath = path.join(rootDirectory, "DESIGN.md");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const design = fs.readFileSync(designPath, "utf8");
const errors = [];
const results = [];
const covered = new Map();
let operationCount = 0;

function checkReference(reference, location) {
  if (!reference || typeof reference !== "object") return errors.push(`${location} must be an object.`);
  if (typeof reference.path !== "string" || !reference.path.startsWith("reference/last-light/")) return errors.push(`${location}.path must point into reference/last-light.`);
  const target = path.join(rootDirectory, reference.path);
  if (!fs.existsSync(target)) return errors.push(`${location}.path does not exist: ${reference.path}.`);
  if (typeof reference.symbol !== "string" || reference.symbol.length === 0) return errors.push(`${location}.symbol is required.`);
  const source = fs.readFileSync(target, "utf8");
  const escaped = reference.symbol.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  if (!new RegExp(`\\b${escaped}\\b`).test(source)) errors.push(`${location}.symbol is absent from ${reference.path}.`);
}

function assertCase(result, assertions, location) {
  if (!assertions) return;
  const state = result.state;
  const fail = (message) => errors.push(`${location}: ${message}`);
  if (assertions.kind !== undefined && state.output?.kind !== assertions.kind) fail(`kind expected ${assertions.kind}, got ${state.output?.kind}.`);
  if (assertions.executed !== undefined && (state.output?.executed ?? false) !== assertions.executed) fail("execution flag mismatch.");
  if (assertions.audit === true && typeof state.audit !== "string") fail("audit digest is absent.");
  if (assertions.sourceDigestIsRoot === true && state.output?.sourceDigest !== state.output?.modules?.find((module) => module.role === "root")?.digest) fail("single-file source digest is not the root module digest.");
  if (assertions.sourceDigestNull === true && state.output?.sourceDigest !== null) fail("package source digest must be null.");
  if (assertions.order && JSON.stringify(state.order) !== JSON.stringify(assertions.order)) fail(`order expected ${JSON.stringify(assertions.order)}, got ${JSON.stringify(state.order)}.`);
  if (assertions.outputsExcluded === true && state.output?.source?.includes("not source")) fail("notebook output entered exported source.");
  if (assertions.companion === true && state.output?.companion?.length !== 1) fail("markdown companion was not retained explicitly.");
  if (assertions.trustNotProof === true && state.output?.executed !== false) fail("trust metadata changed export proof.");
  if (assertions.diagnostic !== undefined && result.error?.code !== assertions.diagnostic) fail(`diagnostic expected ${assertions.diagnostic}, got ${result.error?.code}.`);
  if (assertions.executions !== undefined && state.executions !== assertions.executions) fail(`executions expected ${assertions.executions}, got ${state.executions}.`);
}

if (corpus.$schema !== "w-notebook-export-cases-1") errors.push("notebook export corpus schema is invalid.");
if (corpus.status !== "design-oracle-input") errors.push("notebook export corpus must be a design-oracle input.");
if (corpus.machine !== "notebook-export-machine-pyn3") errors.push("notebook export machine name is invalid.");
for (const decision of corpus.decisions ?? []) {
  if (!/^W-112[0-4]$/.test(decision)) errors.push(`unexpected notebook export decision ${decision}.`);
  if (!design.includes(decision)) errors.push(`notebook export decision ${decision} is absent from DESIGN.md.`);
  covered.set(decision, { accepted: false, rejected: false });
}
for (const [index, reference] of (corpus.references ?? []).entries()) checkReference(reference, `references[${index}]`);

const ids = new Set();
for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  if (ids.has(testCase.id)) errors.push(`${location}.id is duplicated.`);
  ids.add(testCase.id);
  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) errors.push(`${location}.decisions must be non-empty.`);
  for (const decision of testCase.decisions ?? []) {
    if (!covered.has(decision)) errors.push(`${location} references undeclared decision ${decision}.`);
    else covered.get(decision)[testCase.expected?.status === "accepted" ? "accepted" : "rejected"] = true;
  }
  operationCount += testCase.operations?.length ?? 0;
  const result = runNotebookExportFixtureProgram(testCase.operations ?? []);
  if (!testCase.expected || result.status !== testCase.expected.status) errors.push(`${location} expected ${testCase.expected?.status}, derived ${result.status}.`);
  assertCase(result, testCase.expected?.assertions, `${location}.expected.assertions`);
  const compact = compactNotebookExportState(result.state);
  results.push({ id: testCase.id, status: result.status, digest: notebookDigest("snapshot", compact), state: compact });
}
for (const [decision, states] of covered) if (!states.accepted || !states.rejected) errors.push(`${decision} lacks positive and negative export evidence.`);

const snapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
if (process.argv.includes("--write")) fs.writeFileSync(snapshotPath, snapshot);
else if (fs.readFileSync(snapshotPath, "utf8") !== snapshot) errors.push("notebook-export-results.snapshot.jsonl is stale; run the checker with --write.");
if (errors.length > 0) {
  process.stderr.write(`${errors.map((entry) => `- ${entry}`).join("\n")}\n`);
  process.exit(1);
}
const accepted = results.filter((result) => result.status === "accepted").length;
process.stdout.write(`Notebook export PYN3: ${results.length} cases, ${operationCount} operations; ${accepted} accepted, ${results.length - accepted} rejected.\n`);
