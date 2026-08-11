import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  ledgerIds,
  ledgerIdSet,
  ledgerRows,
  ledgerThemeById,
} from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
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
  "allocation-cases.json",
  "layout-abi-cases.json",
  "execution-concurrency-cases.json",
  "runtime-liveness-cases.json",
  "lazy-behavior-cases.json",
  "ownership-execution-cases.json",
  "channel-cases.json",
  "scoped-lock-cases.json",
  "snapshot-cell-cases.json",
  "boundary-effect-cases.json",
  "service-recovery-cases.json",
  "package-release-cases.json",
  "script-workflow-cases.json",
  "repl-session-cases.json",
  "presentation-cases.json",
  "jupyter-cases.json",
  "notebook-export-cases.json",
  "wmeta-cases.json",
  "tabular-carrier-cases.json",
  "tabular-adapter-cases.json",
  "dlpack-cases.json",
  "device-execution-cases.json",
];
const categories = new Set([
  "semantic-contract",
  "implementation-choice",
  "probable-with-fallback",
  "historical-only",
  "project-policy",
  "source-waiver",
]);
const evidenceAxes = new Set(["source", "oracle", "explicit"]);
const errors = [];

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

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
const oracleByDecision = new Map();
for (const file of corpusFiles) {
  const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, file), "utf8"));
  for (const [caseIndex, testCase] of (corpus.cases ?? []).entries()) {
    knownEvidenceIds.add(testCase.id);
    const decisions =
      testCase.decisions ??
      (["script-workflow-cases.json", "repl-session-cases.json"].includes(file) ? corpus.decisions : undefined);
    if (decisions === undefined) continue;
    if (!Array.isArray(decisions) || decisions.length === 0) {
      errors.push(`${file}.cases[${caseIndex}].decisions must be a non-empty array.`);
      continue;
    }
    const localDecisions = new Set();
    for (const [decisionIndex, decision] of decisions.entries()) {
      const location = `${file}.cases[${caseIndex}].decisions[${decisionIndex}]`;
      if (!requireString(decision, location)) continue;
      if (!ledgerIdSet.has(decision)) {
        errors.push(`${location} references missing ledger entry ${decision}.`);
      }
      if (localDecisions.has(decision)) {
        errors.push(`${location} repeats ${decision}.`);
      }
      localDecisions.add(decision);
      const cases = oracleByDecision.get(decision) ?? [];
      cases.push(testCase.id);
      oracleByDecision.set(decision, cases);
    }
  }
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
if (!Array.isArray(audit.requirements)) {
  errors.push("design-freeze-audit.json must contain a requirements array.");
}

const requirementByDecision = new Map();
for (const [index, requirement] of (audit.requirements ?? []).entries()) {
  const location = `requirements[${index}]`;
  if (!requireString(requirement.decision, `${location}.decision`)) continue;
  if (!ledgerIdSet.has(requirement.decision)) {
    errors.push(`${location}.decision references missing ledger entry ${requirement.decision}.`);
  }
  if (requirementByDecision.has(requirement.decision)) {
    errors.push(`${location}.decision duplicates ${requirement.decision}.`);
  }
  if (!Array.isArray(requirement.axes) || requirement.axes.length < 2) {
    errors.push(`${location}.axes must contain at least two evidence axes.`);
  } else {
    const localAxes = new Set();
    for (const [axisIndex, axis] of requirement.axes.entries()) {
      if (!evidenceAxes.has(axis)) {
        errors.push(`${location}.axes[${axisIndex}] must be source, oracle, or explicit.`);
      }
      if (localAxes.has(axis)) {
        errors.push(`${location}.axes[${axisIndex}] repeats ${axis}.`);
      }
      localAxes.add(axis);
    }
  }
  requireString(requirement.reason, `${location}.reason`);
  requirementByDecision.set(requirement.decision, requirement);
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
  if (oracleByDecision.has(entry.decision)) {
    errors.push(`${location}.decision is already classified by an oracle corpus case.`);
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
const oracleDecisionIds = new Set(oracleByDecision.keys());
const classifiedIds = new Set([
  ...sourceDecisionIds,
  ...oracleDecisionIds,
  ...manualIds,
]);
const unclassifiedIds = ledgerIds.filter((decision) => !classifiedIds.has(decision));
const crossAxisOverlaps =
  sourceDecisionIds.size + oracleDecisionIds.size + manualIds.size - classifiedIds.size;
const decisionsByAxis = {
  source: sourceDecisionIds,
  oracle: oracleDecisionIds,
  explicit: manualIds,
};
for (const [decision, requirement] of requirementByDecision) {
  for (const axis of requirement.axes ?? []) {
    if (evidenceAxes.has(axis) && !decisionsByAxis[axis].has(decision)) {
      errors.push(`${decision} requires the ${axis} evidence axis.`);
    }
  }
}

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
    `(${sourceDecisionIds.size} source, ${oracleDecisionIds.size} oracle, ` +
    `${manualIds.size} explicit; ${crossAxisOverlaps} cross-axis overlaps); ` +
    `${requirementByDecision.size} multi-axis requirements; ` +
    `${unclassifiedIds.length} unclassified.\n`,
);

if (process.argv.includes("--list-unclassified")) {
  for (const decision of unclassifiedIds) {
    process.stdout.write(`${decision}\t${ledgerThemeById.get(decision)}\n`);
  }
}
