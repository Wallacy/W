import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { CURRENT_PATHS, runW1518Case } from "./machine.mjs";

const studyDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(studyDirectory, "..", "..", "..");
const ledgerPath = path.join(studyDirectory, "task-ledger.json");
const casesPath = path.join(studyDirectory, "cases.json");
const snapshotPath = path.join(studyDirectory, "results.snapshot.jsonl");
const ledger = JSON.parse(fs.readFileSync(ledgerPath, "utf8"));
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const errors = [];

function requiredString(value, location) {
  if (typeof value !== "string" || value.trim() === "") errors.push(`${location} must be a non-empty string.`);
}

function requiredArray(value, location, minimum = 1) {
  if (!Array.isArray(value) || value.length < minimum) {
    errors.push(`${location} must contain at least ${minimum} item(s).`);
    return false;
  }
  return true;
}

function exact(value, expected, location) {
  if (JSON.stringify(value) !== JSON.stringify(expected)) errors.push(`${location} must equal ${JSON.stringify(expected)}.`);
}

function sourceRefs(refs, location) {
  if (!requiredArray(refs, location)) return;
  for (const [index, ref] of refs.entries()) {
    const base = `${location}[${index}]`;
    requiredString(ref?.path, `${base}.path`);
    requiredString(ref?.anchor, `${base}.anchor`);
    requiredString(ref?.claim, `${base}.claim`);
    if (typeof ref?.path !== "string" || typeof ref?.anchor !== "string") continue;
    const resolved = path.resolve(repositoryRoot, ref.path);
    const relative = path.relative(repositoryRoot, resolved);
    if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
      errors.push(`${base}.path escapes the repository.`);
    } else if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
      errors.push(`${base}.path references a missing file.`);
    } else if (!fs.readFileSync(resolved, "utf8").includes(ref.anchor)) {
      errors.push(`${base}.anchor is absent from ${ref.path}.`);
    }
  }
}

function subset(expected, actual, location) {
  if (expected === null || typeof expected !== "object") {
    if (!Object.is(expected, actual)) errors.push(`${location} expected ${JSON.stringify(expected)}; actual ${JSON.stringify(actual)}.`);
    return;
  }
  if (actual === null || typeof actual !== "object") {
    errors.push(`${location} expected an object; actual ${JSON.stringify(actual)}.`);
    return;
  }
  for (const [key, value] of Object.entries(expected)) subset(value, actual[key], `${location}.${key}`);
}

const expectedPaths = {
  discovery: "/.well-known/w-registry.json",
  root: "/v1/root/<version>.dsse",
  timestamp: "/v1/timestamp.dsse",
  object: "/v1/o/sha256/<hex>",
  channel: "/v1/channels/<encoded-package-id>/<encoded-channel>/<encoded-target-profile>.json",
  search: "/v1/search",
};
const expectedTasks = ["RDX0", "PCB0", "WEC0", "TEV0", "SEV0", "SBX0", "RSX0", "ENT0"];
const expectedClaimsNotAllowed = [
  "compiler-available", "registry-available", "provider-conformant", "sandbox-isolated",
  "source-physically-discarded", "drm-inviolable", "ci-source-confidentiality",
  "zero-day-known-before-disclosure", "crypto-verified", "runner-available", "provider-receipt",
];

if (ledger.$schema !== "w-rdx0-binary-registry-execution-ledger-2") errors.push("ledger schema is invalid.");
if (ledger.id !== "RDX0" || ledger.decisionId !== "W-1518") errors.push("ledger must identify RDX0 under W-1518.");
if (ledger.status !== "complete-design-study" || ledger.supersededDecision !== "W-1486") errors.push("ledger status/supersession is invalid.");
if (ledger.languageSurface !== "none") errors.push("RDX0 must not register a language surface.");
if (ledger.statePolicy?.registrationOnly !== false || ledger.statePolicy?.designContractClosed !== true || ledger.statePolicy?.implementationClaimed !== false) {
  errors.push("statePolicy must close design without claiming implementation.");
}
if (ledger.statePolicy?.currentDesignDisposition !== undefined) errors.push("current disposition belongs to each task, not only the bundle.");
exact(ledger.candidatePaths, expectedPaths, "candidatePaths");
exact(ledger.serializationPolicy, {
  wOwnedPayload: "deterministic-CBOR",
  wOwnedEnvelope: "DSSE-role-specific",
  externalAttestation: "in-toto-Statement-v1/SLSA-provenance-v1.2-JSON-inside-DSSE",
  objectDigest: "exact-stored-bytes-including-envelope",
  dssePayload: "exact-payload-bytes",
  jsonConvenience: ["discovery", "search", "channel", "update"],
  jsonEncoding: "UTF-8-strict",
  duplicateKeys: "reject",
  canonicalSigningPayload: "deterministic-CBOR-selected-by-W-1518",
}, "serializationPolicy");
exact(ledger.evidenceBoundary?.claimsNotAllowed, expectedClaimsNotAllowed, "evidenceBoundary.claimsNotAllowed");
sourceRefs(ledger.sourceRefs, "sourceRefs");
if (!Array.isArray(ledger.officialRefs) || ledger.officialRefs.length < 6) errors.push("officialRefs must contain the six W-1518 primary references.");
for (const ref of ledger.officialRefs ?? []) if (ref.accessed !== "2026-09-03") errors.push("officialRefs must use accessed date 2026-09-03.");

const tasks = Array.isArray(ledger.tasks) ? ledger.tasks : [];
exact(tasks.map((task) => task.id), expectedTasks, "tasks.ids");
for (const [index, task] of tasks.entries()) {
  const location = `tasks[${index}]`;
  if (!new Set(["Direção", "Pesquisa"]).has(task.stateAtRegistration)) errors.push(`${location}.stateAtRegistration is invalid.`);
  if (task.currentDesignDisposition !== "closed-by-W-1518") errors.push(`${location}.currentDesignDisposition is invalid.`);
  if (Object.hasOwn(task, "state")) errors.push(`${location}.state must not be presented as current.`);
  requiredString(task.title, `${location}.title`);
  requiredArray(task.dependencies, `${location}.dependencies`);
  requiredArray(task.outputs, `${location}.outputs`, 3);
  requiredArray(task.adversarialCases, `${location}.adversarialCases`, 5);
  requiredArray(task.evidence?.current, `${location}.evidence.current`);
  requiredArray(task.evidence?.missing, `${location}.evidence.missing`);
  requiredString(task.stopCondition, `${location}.stopCondition`);
}
if (ledger.completion?.registrationOnly !== false || ledger.completion?.designContractClosed !== true || ledger.completion?.implementationClaimed !== false) {
  errors.push("completion must represent a closed design contract without implementation claim.");
}
exact(ledger.completion?.taskIds, expectedTasks, "completion.taskIds");
exact(ledger.completion?.taskStatesAtRegistration, ["Direção", "Pesquisa"], "completion.taskStatesAtRegistration");
if (ledger.completion?.currentDesignDisposition !== "closed-by-W-1518") errors.push("completion.currentDesignDisposition is invalid.");

if (corpus.$schema !== "w-w1518-rdx0-binary-registry-execution-cases-1" ||
    corpus.status !== "design-oracle-input-1" ||
    corpus.machine !== "w1518-rdx0-binary-registry-execution-machine-1" ||
    JSON.stringify(corpus.decisions) !== JSON.stringify(["W-1518"]) ||
    corpus.evidenceBoundary !== "design-only; no crypto/server/provider/runner result") {
  errors.push("W-1518 corpus header is invalid.");
}

const caseIds = new Set();
const results = [];
for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  requiredString(testCase?.id, `${location}.id`);
  if (caseIds.has(testCase?.id)) errors.push(`${location}.id is duplicated.`);
  caseIds.add(testCase?.id);
  if (!Array.isArray(testCase?.decisions) || !testCase.decisions.includes("W-1518")) errors.push(`${location}.decisions must cite W-1518.`);
  const actual = runW1518Case(testCase);
  if (actual.status !== testCase?.expected?.status ||
      (actual.status === "rejected" && actual.code !== testCase.expected.code)) {
    errors.push(`${location} expected ${JSON.stringify(testCase.expected)}; actual ${JSON.stringify(actual)}.`);
  }
  subset(testCase.expected?.facts ?? {}, actual.facts, `${location}.expected.facts`);
  results.push({ caseId: testCase.id, status: actual.status, ...(actual.code ? { code: actual.code } : {}), facts: actual.facts ?? {} });
}
if (caseIds.size < 25) errors.push("W-1518 corpus must retain at least 25 positive/adversarial cases.");

const output = [
  JSON.stringify({ schema: "w-w1518-rdx0-results-1", status: "design-oracle-output-1", evidenceBoundary: "design-only" }),
  ...results.map((result) => JSON.stringify(result)),
].join("\n") + "\n";

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const accepted = results.filter((result) => result.status === "accepted").length;
const rejected = results.length - accepted;
const summary = `W-1518 RDX0 design oracle: ${results.length} cases, ${accepted} accepted, ${rejected} rejected; design-only.`;
if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, output);
  process.stdout.write(`${summary}\nUpdated ${path.basename(snapshotPath)}.\n`);
  process.exit(0);
}
if (!fs.existsSync(snapshotPath)) {
  process.stderr.write(`${path.basename(snapshotPath)} is missing; run with --write.\n`);
  process.exit(1);
}
if (fs.readFileSync(snapshotPath, "utf8") !== output) {
  process.stderr.write(`${path.basename(snapshotPath)} is stale; run with --write.\n`);
  process.exit(1);
}
process.stdout.write(`${summary}\n`);
