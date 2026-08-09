import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIdSet } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const rationaleText = fs.readFileSync(path.join(wDirectory, "RATIONALE.md"), "utf8");
const corpus = JSON.parse(
  fs.readFileSync(path.join(toolingDirectory, "substitution-cases.json"), "utf8"),
);
const lines = rationaleText.split(/\r?\n/);
const errors = [];
const requiredMeasures = [
  "semantic-correctness",
  "recall",
  "repair-time",
  "token-count",
];

function normalizeReviewItem(value) {
  return value.trim().replace(/[.;]$/, "");
}

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }

  return true;
}

function checkSource(source, location) {
  if (!Array.isArray(source) || source.length === 0) {
    errors.push(`${location} must contain source lines.`);
    return;
  }

  for (const [index, line] of source.entries()) {
    if (!requireString(line, `${location}[${index}]`)) {
      continue;
    }

    if (/\r|\n/.test(line)) {
      errors.push(`${location}[${index}] must contain exactly one source line.`);
    }
  }
}

const reviewStart = lines.findIndex((line) => line === "O corpus compara, no mínimo:");
const coverageStart = lines.findIndex((line) => line === "### 1.1 Cobertura de substituições");

if (reviewStart < 0 || coverageStart <= reviewStart) {
  errors.push("RATIONALE.md must contain the section 1 review list before section 1.1.");
}

const reviewItems = lines
  .slice(reviewStart + 1, coverageStart)
  .filter((line) => line.startsWith("- "))
  .map((line) => normalizeReviewItem(line.slice(2)));
const reviewItemSet = new Set(reviewItems);

if (reviewItems.length === 0 || reviewItemSet.size !== reviewItems.length) {
  errors.push(
    `Section 26 must contain a non-empty set of unique review items; found ${reviewItems.length} items and ${reviewItemSet.size} unique items.`,
  );
}

if (corpus.$schema !== "w-substitution-cases-1") {
  errors.push("substitution-cases.json must use schema w-substitution-cases-1.");
}

if (corpus.status !== "design-oracle-input") {
  errors.push("substitution-cases.json must have status design-oracle-input.");
}

if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  errors.push("substitution-cases.json must contain at least one case.");
}

const caseIds = new Set();
const coveredItems = new Set();
const coveredDecisions = new Set();

for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;

  if (!/^R0-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase.id ?? "")) {
    errors.push(`${location}.id must use the R0-kebab-case form.`);
  } else if (caseIds.has(testCase.id)) {
    errors.push(`${location}.id duplicates ${testCase.id}.`);
  } else {
    caseIds.add(testCase.id);
  }

  if (requireString(testCase.reviewItem, `${location}.reviewItem`)) {
    const normalizedItem = normalizeReviewItem(testCase.reviewItem);

    if (!reviewItemSet.has(normalizedItem)) {
      errors.push(`${location}.reviewItem does not match a RATIONALE section 1 review item.`);
    }

    if (coveredItems.has(normalizedItem)) {
      errors.push(`${location}.reviewItem is already covered by another case.`);
    }

    coveredItems.add(normalizedItem);
  }

  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) {
    errors.push(`${location}.decisions must contain at least one decision ID.`);
  } else {
    const localDecisions = new Set();

    for (const decision of testCase.decisions) {
      if (!/^W-\d{3,}$/.test(decision)) {
        errors.push(`${location}.decisions contains invalid ID ${decision}.`);
      } else if (!ledgerIdSet.has(decision)) {
        errors.push(`${location}.decisions references missing ledger entry ${decision}.`);
      } else if (localDecisions.has(decision)) {
        errors.push(`${location}.decisions repeats ${decision}.`);
      } else {
        localDecisions.add(decision);
        coveredDecisions.add(decision);
      }
    }
  }

  requireString(testCase.task, `${location}.task`);

  if (!testCase.selected || typeof testCase.selected !== "object") {
    errors.push(`${location}.selected must describe the selected form.`);
  } else {
    requireString(testCase.selected.language, `${location}.selected.language`);
    checkSource(testCase.selected.source, `${location}.selected.source`);
  }

  if (!Array.isArray(testCase.alternatives) || testCase.alternatives.length === 0) {
    errors.push(`${location}.alternatives must contain at least one substituted form.`);
  } else {
    const names = new Set();

    for (const [alternativeIndex, alternative] of testCase.alternatives.entries()) {
      const alternativeLocation = `${location}.alternatives[${alternativeIndex}]`;
      if (requireString(alternative.name, `${alternativeLocation}.name`)) {
        if (names.has(alternative.name)) {
          errors.push(`${alternativeLocation}.name duplicates ${alternative.name}.`);
        }
        names.add(alternative.name);
      }
      requireString(alternative.language, `${alternativeLocation}.language`);
      checkSource(alternative.source, `${alternativeLocation}.source`);
      requireString(
        alternative.expectedDifference,
        `${alternativeLocation}.expectedDifference`,
      );
    }
  }

  if (
    !Array.isArray(testCase.measures) ||
    testCase.measures.length !== requiredMeasures.length ||
    requiredMeasures.some((measure) => !testCase.measures.includes(measure)) ||
    new Set(testCase.measures).size !== requiredMeasures.length
  ) {
    errors.push(
      `${location}.measures must contain each required measure exactly once: ${requiredMeasures.join(", ")}.`,
    );
  }
}

const uncoveredItems = reviewItems.filter((item) => !coveredItems.has(item));

if (process.argv.includes("--require-complete") && uncoveredItems.length > 0) {
  errors.push(
    `Complete coverage requires one case for each of the ${reviewItems.length} review items; ${uncoveredItems.length} items remain.`,
  );
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

process.stdout.write(
  `Substitution cases: ${coveredItems.size}/${reviewItems.length} review items covered across ${coveredDecisions.size} decisions; ${uncoveredItems.length} uncovered.\n`,
);

if (process.argv.includes("--list-uncovered")) {
  for (const item of uncoveredItems) {
    process.stdout.write(`- ${item}\n`);
  }
}
