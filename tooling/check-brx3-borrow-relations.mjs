import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { buildBRX3Snapshot, validateBRX3Corpus } from "./brx3-borrow-relations-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "brx3-borrow-relations-cases.json");
const snapshotPath = path.join(toolingDirectory, "brx3-borrow-relations-results.snapshot.jsonl");
const studyDirectory = path.join(toolingDirectory, "studies", "brx3-borrow-relations");

function digestFile(filePath) {
  return "sha256:" + crypto.createHash("sha256").update(fs.readFileSync(filePath)).digest("hex");
}

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
const checked = validateBRX3Corpus(corpus);
const errors = [...checked.errors];
if (corpus.sourceBase?.path !== "reference/last-light/borrow_relations_brx3.w") errors.push("BRX3 sourceBase path is stale.");
if (corpus.sourceBase?.symbol !== "selectPrimaryRelation") errors.push("BRX3 sourceBase symbol is stale.");
const sourcePath = path.join(repositoryRoot, corpus.sourceBase?.path ?? "");
if (!fs.existsSync(sourcePath)) errors.push("BRX3 source fixture is missing.");
if (fs.existsSync(sourcePath) && corpus.sourceBase?.digest !== undefined && digestFile(sourcePath) !== corpus.sourceBase.digest) errors.push("BRX3 sourceBase digest is stale.");
for (const [index, reference] of Object.entries(corpus.officialRefs ?? {})) {
  const url = new URL(reference.url);
  if (url.protocol !== "https:") errors.push(`officialRefs[${index}] must use HTTPS.`);
}
if (!fs.existsSync(studyDirectory)) errors.push("BRX3 study directory is missing.");
const study = fs.existsSync(path.join(studyDirectory, "study.json")) ? readJson(path.join(studyDirectory, "study.json")) : null;
if (!study || study.$schema !== "w-brx3-borrow-relations-study-1" || study.id !== "BRX3") errors.push("BRX3 study manifest is missing or stale.");
if (study && study.evidence?.missing?.includes("w-compile") !== true) errors.push("BRX3 must keep w-compile in implementation evidence gaps.");
if (study && study.decision?.status !== "promote-source-clause") errors.push("BRX3 study decision must promote only the source clause contract.");
const bundle = fs.existsSync(path.join(studyDirectory, "bundle.json")) ? readJson(path.join(studyDirectory, "bundle.json")) : null;
if (!bundle || bundle.$schema !== "w-substitution-study-bundle-1" || bundle.id !== "R1-brx3-borrow-relations") errors.push("BRX3 study bundle is missing or stale.");
const snapshot = fs.existsSync(snapshotPath) ? fs.readFileSync(snapshotPath, "utf8") : "";
const expectedSnapshot = buildBRX3Snapshot(corpus).text;
if (snapshot !== expectedSnapshot) errors.push("BRX3 snapshot is stale; run `bun tooling/check-brx3-borrow-relations.mjs --write`.");
if (checked.metrics.accepted < 8 || checked.metrics.rejected < 8) errors.push("BRX3 needs both positive and adversarial cases.");
if (checked.metrics.wAbiDrift !== 0) errors.push("BRX3 relation must not change WAbi.");

if (process.argv.includes("--write") && errors.filter((error) => !error.startsWith("BRX3 snapshot is stale")).length === 0) {
  fs.writeFileSync(snapshotPath, expectedSnapshot, "utf8");
}

if (errors.length === 0) {
  console.log(`BRX3 borrow relations: ${checked.metrics.caseCount} cases, ${checked.metrics.accepted} accepted, ${checked.metrics.rejected} rejected, ${checked.metrics.canonicalRelations} canonical relations.`);
} else {
  fail(errors);
}
