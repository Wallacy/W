import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { buildGen2Snapshot, validateGen2Corpus } from "./gen2-stream-yield-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const studyDirectory = path.join(toolingDirectory, "studies", "gen2-stream-yield");
const corpusPath = path.join(toolingDirectory, "gen2-stream-yield-cases.json");
const manifestPath = path.join(studyDirectory, "study.json");
const bundlePath = path.join(studyDirectory, "bundle.json");
const snapshotPath = path.join(toolingDirectory, "gen2-stream-yield-results.snapshot.jsonl");

function fail(errors) {
  if (errors.length > 0) {
    console.error(errors.join("\n"));
    process.exitCode = 1;
  }
}

function digest(text) {
  return `sha256:${new Bun.CryptoHasher("sha256").update(text).digest("hex")}`;
}

function digestFile(filePath) {
  return digest(fs.readFileSync(filePath));
}

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

const corpus = readJson(corpusPath);
const manifest = readJson(manifestPath);
const bundle = readJson(bundlePath);
const errors = [];
const checked = validateGen2Corpus(corpus);
errors.push(...checked.errors);

if (manifest.$schema !== "w-substitution-study-bundle-1") errors.push("study manifest schema is invalid.");
if (manifest.id !== "R1-gen2-stream-yield") errors.push("study manifest id is invalid.");
if (manifest.decision?.status !== "promote-narrow-form") errors.push("study manifest must record the promoted narrow form.");
if (manifest.corpus?.path !== "../../gen2-stream-yield-cases.json") errors.push("study manifest corpus path is stale.");
if (manifest.corpus?.machine !== "../../gen2-stream-yield-machine.mjs") errors.push("study manifest machine path is stale.");
if (manifest.corpus?.checker !== "../../check-gen2-stream-yield.mjs") errors.push("study manifest checker path is stale.");
if (manifest.corpus?.hostTests !== "../../gen2-stream-yield-reference.test.mjs") errors.push("study manifest test path is stale.");
if (manifest.corpus?.snapshot !== "../../gen2-stream-yield-results.snapshot.jsonl") errors.push("study manifest snapshot path is stale.");
if (manifest.bundle !== "bundle.json") errors.push("study manifest bundle path is stale.");
if (manifest.oracle !== "oracle.test.mjs") errors.push("study manifest oracle path is stale.");
if (manifest.entry !== "gen2Fixture") errors.push("study manifest entry is stale.");
if (bundle.$schema !== "w-substitution-study-bundle-1" || bundle.id !== "R1-gen2-stream-yield") errors.push("GEN2 bundle identity is stale.");
if (bundle.entry !== "gen2Fixture") errors.push("GEN2 bundle entry must be gen2Fixture.");
if (bundle.decision?.status !== "promote-narrow-form" || bundle.decision?.generalGenerator !== "intentionally-rejected") errors.push("GEN2 bundle decision must promote only the narrow form.");
const bundleVariants = new Set();
for (const variant of bundle.variants ?? []) {
  if (bundleVariants.has(variant.id)) errors.push(`GEN2 bundle variant ${variant.id} is duplicated.`);
  bundleVariants.add(variant.id);
  const variantPath = path.join(studyDirectory, variant.path ?? "");
  if (!fs.existsSync(variantPath)) { errors.push(`GEN2 bundle variant ${variant.id} is missing.`); continue; }
  if (digestFile(variantPath) !== variant.digest) errors.push(`GEN2 bundle variant ${variant.id} digest is stale; expected ${digestFile(variantPath)}.`);
  if (!fs.readFileSync(variantPath, "utf8").includes(bundle.entry)) errors.push(`GEN2 bundle variant ${variant.id} lacks ${bundle.entry}.`);
}
const oraclePath = path.join(studyDirectory, bundle.oracle?.path ?? "");
if (!fs.existsSync(oraclePath)) errors.push("GEN2 bundle oracle is missing.");
else if (digestFile(oraclePath) !== bundle.oracle?.digest) errors.push(`GEN2 bundle oracle digest is stale; expected ${digestFile(oraclePath)}.`);
if (JSON.stringify(manifest.lowerings ?? []) !== JSON.stringify(["switched-frame", "returned-state"])) errors.push("study manifest lowerings are not independent.");

const fixture = fs.readFileSync(path.join(studyDirectory, "yield.w"), "utf8");
for (const forbidden of ["stream fn", "yield from", "yield view", "yield inout", "yield send", "buffer(", "resumeToken", "scheduler", "return stream {"]) {
  if (fixture.includes(forbidden)) errors.push(`yield fixture exposes forbidden construct ${forbidden}.`);
}
if ((fixture.match(/return stream <\[/g) ?? []).length !== 3) errors.push("yield fixture must contain three explicitly captured stream expressions.");
if ((fixture.match(/stream <\[take source\]>/g) ?? []).length !== 3) errors.push("yield fixture must capture each source with take at construction.");
if (!fixture.includes("yield take order") || !fixture.includes("yield copy line")) errors.push("yield fixture must show take and copy owned yield statements.");

const rejectedFixture = fs.readFileSync(path.join(studyDirectory, "rejected.txt"), "utf8");
for (const forbidden of ["stream fn", "yield send", "yield throw", "yield close", "yield from", "public frame", "resumeToken"]) {
  if (!rejectedFixture.includes(forbidden)) errors.push(`rejected fixture is missing ${forbidden} witness.`);
}
if (!/fn badOutside[\s\S]*\byield\s+take\s+order/.test(rejectedFixture)) errors.push("rejected fixture is missing yield-outside-stream witness.");
const diagnosticCatalog = fs.readFileSync(path.join(repositoryRoot, "tooling", "diagnostic-catalog.json"), "utf8");
for (const code of ["W-YIELD-0001", "W-YIELD-0002", "W-YIELD-0003", "W-YIELD-0004", "W-YIELD-0005", "W-YIELD-0006", "W-YIELD-0007", "W-YIELD-0008", "W-YIELD-0009", "W-YIELD-0010", "W-YIELD-0011"]) {
  if (!diagnosticCatalog.includes(`\"code\": \"${code}\"`)) errors.push(`diagnostic catalog is missing ${code}.`);
}

const snapshot = fs.existsSync(snapshotPath) ? fs.readFileSync(snapshotPath, "utf8") : "";
const expectedSnapshot = buildGen2Snapshot(corpus).text;
if (snapshot !== expectedSnapshot) errors.push("GEN2 snapshot is stale; run `bun tooling/check-gen2-stream-yield.mjs --write`.");

if (checked.decision.status !== "promote-narrow-form") errors.push(`machine decision is ${checked.decision.status}, expected promote-narrow-form.`);
if (checked.decision.ergonomicWins < 3) errors.push("machine did not observe three independent ergonomic wins.");
if (checked.decision.negativeCases < 8) errors.push("negative contract corpus is too small.");
for (const result of checked.results) {
  if (!result.pass) errors.push(`${result.id} failed its expected contract.`);
  if (!result.current.loweringsEquivalent || !result.yield.loweringsEquivalent) errors.push(`${result.id} diverges between lowerings.`);
}

if (process.argv.includes("--write") && errors.filter((error) => !error.startsWith("GEN2 snapshot is stale")).length === 0) {
  fs.writeFileSync(snapshotPath, expectedSnapshot, "utf8");
}

if (errors.length === 0) {
  console.log(`GEN2 ok: ${checked.results.length} cases; ${checked.decision.ergonomicWins} ergonomic wins; ${checked.decision.negativeCases} contract gates; promotion=${checked.decision.status}.`);
} else {
  fail(errors);
}
