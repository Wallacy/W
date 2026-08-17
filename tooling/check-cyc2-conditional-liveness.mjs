import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { buildCYC2Snapshot, validateCYC2Corpus } from "./cyc2-conditional-liveness-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const corpusPath = path.join(toolingDirectory, "cyc2-conditional-liveness-cases.json");
const snapshotPath = path.join(toolingDirectory, "cyc2-conditional-liveness-results.snapshot.jsonl");
const studyDirectory = path.join(toolingDirectory, "studies", "cyc2-conditional-liveness");

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function fail(errors) {
  if (errors.length > 0) {
    console.error(errors.join("\n"));
    process.exitCode = 1;
  }
}

const corpus = readJson(corpusPath);
const checked = validateCYC2Corpus(corpus);
const errors = [...checked.errors];
const studyPath = path.join(studyDirectory, "study.json");
const bundlePath = path.join(studyDirectory, "bundle.json");
if (!fs.existsSync(studyPath) || !fs.existsSync(bundlePath)) errors.push("CYC2 study artifacts are missing.");
const study = fs.existsSync(studyPath) ? readJson(studyPath) : null;
const bundle = fs.existsSync(bundlePath) ? readJson(bundlePath) : null;
if (!study || study.$schema !== "w-cyc2-conditional-liveness-study-1" || study.id !== "CYC2") errors.push("CYC2 study manifest is stale.");
if (study && study.decision?.status !== "close-baseline") errors.push("CYC2 study decision must close the baseline.");
if (study && study.evidence?.classification !== "implementation-evidence-gap") errors.push("CYC2 runtime/provider evidence must be implementation-only.");
if (!bundle || bundle.$schema !== "w-substitution-study-bundle-1" || bundle.id !== "R1-cyc2-conditional-liveness") errors.push("CYC2 study bundle is stale.");
if (checked.metrics.baselineCompositions !== 3) errors.push("CYC2 must retain exactly three baseline compositions.");
if (checked.metrics.activeResearch !== 0) errors.push("CYC2 must not retain an active Research status.");
if (checked.metrics.collectorSideEffects !== 0) errors.push("CYC2 baseline cannot expose collector side effects.");
const snapshot = fs.existsSync(snapshotPath) ? fs.readFileSync(snapshotPath, "utf8") : "";
const expectedSnapshot = buildCYC2Snapshot(corpus).text;
if (snapshot !== expectedSnapshot) errors.push("CYC2 snapshot is stale; run `bun tooling/check-cyc2-conditional-liveness.mjs --write`.");

if (process.argv.includes("--write") && errors.filter((error) => !error.startsWith("CYC2 snapshot is stale")).length === 0) {
  fs.writeFileSync(snapshotPath, expectedSnapshot, "utf8");
}

if (errors.length === 0) {
  console.log(`CYC2 conditional liveness: ${checked.metrics.caseCount} cases, ${checked.metrics.baselineCompositions} baseline compositions, ${checked.metrics.intentionallyRejected} intentionally rejected, ${checked.metrics.implementationEvidenceGaps} implementation gaps.`);
} else {
  fail(errors);
}
