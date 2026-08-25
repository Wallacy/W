import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  corpusPath,
  mutationChecks,
  researchZero,
  repositoryRoot,
  snapshotPath,
  snapshotText,
  studyDirectory,
  validateCorpus,
} from "./aeg0-app-essentials-gate-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
function digest(file) { return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`; }
function readJson(file) { return JSON.parse(fs.readFileSync(file, "utf8")); }
function contained(file) {
  const relative = path.relative(repositoryRoot, path.resolve(file));
  return relative !== "" && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative);
}
function forbiddenKey(value) {
  if (Array.isArray(value)) return value.some(forbiddenKey);
  if (!value || typeof value !== "object") return false;
  return Object.entries(value).some(([key, child]) => ["expected", "result", "route", "promotion"].includes(key) || forbiddenKey(child));
}
function checkStudyArtifacts(errors) {
  const files = ["bundle.json", "study.json", "README.md", "INDEX.md", "oracle.test.mjs", "current.w", "candidate.txt", "adversarial.w"];
  for (const name of files) {
    const file = path.join(studyDirectory, name);
    if (!contained(file) || !fs.existsSync(file) || !fs.statSync(file).isFile()) errors.push(`AEG0 study artifact is missing: ${name}.`);
  }
  if (errors.length > 0) return;
  const bundle = readJson(path.join(studyDirectory, "bundle.json"));
  const study = readJson(path.join(studyDirectory, "study.json"));
  if (bundle.$schema !== "w-substitution-study-bundle-1" || bundle.status !== "design-oracle-input" || bundle.id !== "R1-aeg0-app-essentials-gate") errors.push("AEG0 bundle identity is invalid.");
  if (study.$schema !== "w-aeg0-app-essentials-gate-study-1" || study.status !== "design-oracle-input" || study.id !== "AEG0") errors.push("AEG0 study identity is invalid.");
  const variants = new Map((bundle.variants ?? []).map((variant) => [variant.id, variant]));
  if (variants.size !== 3 || !["current", "candidate", "adversarial"].every((id) => variants.has(id))) errors.push("AEG0 bundle must contain current, candidate, and adversarial variants.");
  for (const [id, variant] of variants) {
    const file = path.resolve(studyDirectory, variant.path ?? "");
    if (!contained(file) || !fs.existsSync(file) || digest(file) !== variant.digest || !fs.readFileSync(file, "utf8").includes(bundle.entry)) errors.push(`AEG0 bundle variant ${id} has a stale chain.`);
    if (id === "candidate" && (variant.language !== "w-reserved" || variant.parseEvidence?.status !== "reserved-not-parsed" || !String(variant.path).endsWith(".txt"))) errors.push("AEG0 candidate must remain reserved and not parsed.");
    if (id !== "candidate" && (variant.language !== "w" || variant.parseEvidence?.status !== "tree-sitter-parse" || !String(variant.path).endsWith(".w"))) errors.push(`AEG0 ${id} must use a thin parseable witness.`);
  }
  const candidateText = fs.readFileSync(path.join(studyDirectory, "candidate.txt"), "utf8");
  for (const token of ["capability", "UtcTimestamp", "secure", "deterministic", "ByteSource", "SecretHandle", "no new syntax"]) if (!candidateText.includes(token)) errors.push(`AEG0 candidate text omits ${token}.`);
  if (forbiddenKey(study)) errors.push("AEG0 study metadata must not echo expected, result, route, or promotion.");
  if (!new Set(bundle.blinding?.hide ?? []).has("expected")) errors.push("AEG0 bundle blinding must hide rubric-only expected metadata.");
}

export function main(argv = process.argv.slice(2)) {
  const errors = [];
  let corpus;
  try { corpus = readJson(corpusPath); } catch (error) { errors.push(`AEG0 corpus cannot load: ${error instanceof Error ? error.message : "unknown error"}.`); }
  const checked = corpus ? validateCorpus(corpus) : { errors: [], results: [] };
  errors.push(...checked.errors);
  if (corpus && JSON.stringify(corpus).includes('"expected"')) errors.push("AEG0 corpus must not contain expected echo.");
  const mutations = mutationChecks();
  for (const [name, passed] of Object.entries(mutations)) if (passed !== true) errors.push(`AEG0 mutation check failed: ${name}.`);
  if (!researchZero()) errors.push("AEG0 requires Research=0 through W-1459 and W-1454..W-1458 oracle-backed-current.");
  checkStudyArtifacts(errors);
  if (corpus) {
    const projected = snapshotText(checked.results, mutations);
    if (argv.includes("--write")) fs.writeFileSync(snapshotPath, projected, "utf8");
    else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== projected) errors.push("AEG0 snapshot is stale. Run with --write.");
  }
  if (errors.length > 0) { process.stderr.write(`${errors.join("\n")}\n`); process.exitCode = 1; return false; }
  const accepted = checked.results.filter((result) => result.status === "accepted").length;
  const rejected = checked.results.filter((result) => result.status === "rejected").length;
  process.stdout.write(`AEG0 app-essentials-gate: ${checked.results.length} cases, ${accepted} current accepted, ${rejected} rejected; historical Research=0 through W-1459 and mutation guards green.\n`);
  return true;
}
if (import.meta.main) main();
