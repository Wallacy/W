import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const designText = fs.readFileSync(path.join(wDirectory, "DESIGN.md"), "utf8");
const audit = JSON.parse(
  fs.readFileSync(path.join(toolingDirectory, "design-freeze-audit.json"), "utf8"),
);
const substitutions = JSON.parse(
  fs.readFileSync(path.join(toolingDirectory, "substitution-cases.json"), "utf8"),
);

const corpusFiles = [
  "semantic-cases.json",
  "formatter-cases.json",
  "memory-transition-cases.json",
  "execution-concurrency-cases.json",
  "boundary-effect-cases.json",
  "package-release-cases.json",
];
const categories = new Set([
  "semantic-contract",
  "implementation-choice",
  "probable-with-fallback",
  "historical-only",
  "project-policy",
  "source-waiver",
]);
const errors = [];

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

const ledgerRows = [...designText.matchAll(/^\| (W-\d{3,}) \| ([^|]+) \|/gm)].map(
  (match) => ({ id: match[1], theme: match[2].trim() }),
);
const ledgerIds = ledgerRows.map((row) => row.id);
const ledgerIdSet = new Set(ledgerIds);
const ledgerThemeById = new Map(ledgerRows.map((row) => [row.id, row.theme]));

for (const [index, decision] of ledgerIds.entries()) {
  const expected = `W-${String(index + 1).padStart(3, "0")}`;
  if (decision !== expected) {
    errors.push(`Decision ledger is not contiguous at ${decision}; expected ${expected}.`);
    break;
  }
}

const r0ByDecision = new Map();
for (const testCase of substitutions.cases ?? []) {
  for (const decision of testCase.decisions ?? []) {
    const cases = r0ByDecision.get(decision) ?? [];
    cases.push(testCase.id);
    r0ByDecision.set(decision, cases);
  }
}

const knownEvidenceIds = new Set((substitutions.cases ?? []).map((testCase) => testCase.id));
for (const file of corpusFiles) {
  const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, file), "utf8"));
  for (const testCase of corpus.cases ?? []) knownEvidenceIds.add(testCase.id);
}

const studiesDirectory = path.join(toolingDirectory, "studies");
for (const entry of fs.readdirSync(studiesDirectory, { withFileTypes: true })) {
  if (!entry.isDirectory()) continue;
  const bundlePath = path.join(studiesDirectory, entry.name, "bundle.json");
  if (!fs.existsSync(bundlePath)) continue;
  knownEvidenceIds.add(JSON.parse(fs.readFileSync(bundlePath, "utf8")).id);
}

if (audit.$schema !== "w-design-freeze-audit-1") {
  errors.push("design-freeze-audit.json must use schema w-design-freeze-audit-1.");
}
if (audit.status !== "design-oracle-input") {
  errors.push("design-freeze-audit.json must have status design-oracle-input.");
}
if (!Array.isArray(audit.entries)) {
  errors.push("design-freeze-audit.json must contain an entries array.");
}

const manualIds = new Set();
let previousManualNumber = 0;
for (const [index, entry] of (audit.entries ?? []).entries()) {
  const location = `entries[${index}]`;
  if (!requireString(entry.decision, `${location}.decision`)) continue;
  if (!ledgerIdSet.has(entry.decision)) {
    errors.push(`${location}.decision references missing ledger entry ${entry.decision}.`);
  }
  if (manualIds.has(entry.decision)) {
    errors.push(`${location}.decision duplicates ${entry.decision}.`);
  }
  if (r0ByDecision.has(entry.decision)) {
    errors.push(`${location}.decision is already classified by an R0 source comparison.`);
  }
  const manualNumber = Number(entry.decision.slice(2));
  if (manualNumber <= previousManualNumber) {
    errors.push(`${location}.decision must follow ascending ledger order.`);
  }
  previousManualNumber = manualNumber;
  manualIds.add(entry.decision);

  if (!categories.has(entry.category)) {
    errors.push(`${location}.category must be one of ${[...categories].join(", ")}.`);
  }
  requireString(entry.reason, `${location}.reason`);

  if (entry.category === "semantic-contract") {
    if (!Array.isArray(entry.evidence) || entry.evidence.length === 0) {
      errors.push(`${location}.evidence must contain at least one oracle case.`);
    } else {
      const localEvidence = new Set();
      for (const [evidenceIndex, evidence] of entry.evidence.entries()) {
        if (!requireString(evidence, `${location}.evidence[${evidenceIndex}]`)) continue;
        if (!knownEvidenceIds.has(evidence)) {
          errors.push(`${location}.evidence references unknown case ${evidence}.`);
        }
        if (localEvidence.has(evidence)) {
          errors.push(`${location}.evidence repeats ${evidence}.`);
        }
        localEvidence.add(evidence);
      }
    }
  }

  if (entry.category === "probable-with-fallback") {
    requireString(entry.fallback, `${location}.fallback`);
    requireString(entry.gate, `${location}.gate`);
  }
  if (entry.category === "source-waiver") {
    requireString(entry.waiver, `${location}.waiver`);
  }
}

const sourceDecisionIds = new Set(r0ByDecision.keys());
const classifiedIds = new Set([...sourceDecisionIds, ...manualIds]);
const unclassifiedIds = ledgerIds.filter((decision) => !classifiedIds.has(decision));

if (process.argv.includes("--require-complete") && unclassifiedIds.length > 0) {
  errors.push(
    `Design freeze requires all ${ledgerIds.length} decisions to be classified; ` +
      `${unclassifiedIds.length} remain.`,
  );
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

process.stdout.write(
  `Design freeze audit: ${classifiedIds.size}/${ledgerIds.length} decisions classified ` +
    `(${sourceDecisionIds.size} by R0 + ${manualIds.size} explicit); ` +
    `${unclassifiedIds.length} unclassified.\n`,
);

if (process.argv.includes("--list-unclassified")) {
  for (const decision of unclassifiedIds) {
    process.stdout.write(`${decision}\t${ledgerThemeById.get(decision)}\n`);
  }
}
