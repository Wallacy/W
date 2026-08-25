import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  corpusPath,
  mutationChecks,
  repositoryRoot,
  snapshotPath,
  snapshotText,
  studyDirectory,
  validateCorpus,
} from "./pfu0-pre-freeze-usability-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));

function digest(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, "utf8"));
}

function contained(file) {
  const relative = path.relative(repositoryRoot, path.resolve(file));
  return relative !== "" && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative);
}

function containsForbiddenKey(value) {
  if (Array.isArray(value)) return value.some(containsForbiddenKey);
  if (!value || typeof value !== "object") return false;
  return Object.entries(value).some(([key, child]) => ["expected", "result"].includes(key) || containsForbiddenKey(child));
}

function checkStudyArtifacts(errors) {
  const files = ["bundle.json", "study.json", "README.md", "INDEX.md", "oracle.test.mjs", "current.w", "candidate.txt", "adversarial.w"];
  for (const name of files) {
    const file = path.join(studyDirectory, name);
    if (!contained(file) || !fs.existsSync(file) || !fs.statSync(file).isFile()) errors.push(`PFU0 study artifact is missing: ${name}.`);
  }
  if (errors.length > 0) return;
  const bundle = readJson(path.join(studyDirectory, "bundle.json"));
  const study = readJson(path.join(studyDirectory, "study.json"));
  if (bundle.$schema !== "w-substitution-study-bundle-1" || bundle.status !== "design-oracle-input" || bundle.id !== "R1-pfu0-pre-freeze-usability") errors.push("PFU0 bundle identity is invalid.");
  if (study.$schema !== "w-pfu0-pre-freeze-usability-study-1" || study.status !== "design-oracle-input" || study.id !== "PFU0") errors.push("PFU0 study identity is invalid.");
  const variants = new Map((bundle.variants ?? []).map((variant) => [variant.id, variant]));
  if (variants.size !== 3 || !["current", "candidate", "adversarial"].every((id) => variants.has(id))) errors.push("PFU0 bundle must contain current, candidate, and adversarial variants.");
  for (const [id, variant] of variants) {
    const file = path.resolve(studyDirectory, variant.path ?? "");
    if (!contained(file) || !fs.existsSync(file) || digest(file) !== variant.digest || !fs.readFileSync(file, "utf8").includes(bundle.entry)) errors.push(`PFU0 bundle variant ${id} has a stale chain.`);
    if (id === "candidate" && (variant.language !== "w-reserved" || variant.parseEvidence?.status !== "reserved-not-parsed" || !String(variant.path).endsWith(".txt"))) errors.push("PFU0 candidate must remain reserved and not parseable.");
    if (id !== "candidate" && (variant.language !== "w" || variant.parseEvidence?.status !== "tree-sitter-parse" || !String(variant.path).endsWith(".w"))) errors.push(`PFU0 ${id} must use a thin parseable witness.`);
  }
  const candidateText = fs.readFileSync(path.join(studyDirectory, "candidate.txt"), "utf8");
  for (const token of ["build.w", "one or two records", "order-independent", "stream fn updates(...): Item throws Failure", "some Stream<Item,Failure>", "ServiceFailure", "willSet", "didSet", "modify + defer", "no final spelling or owner"]) if (!candidateText.includes(token)) errors.push(`PFU0 candidate text omits ${token}.`);
  if (candidateText.includes("-> some Stream")) errors.push("PFU0 candidate must not spell a direct some Stream return.");
  if (containsForbiddenKey(study)) errors.push("PFU0 study metadata must not echo expected or result.");
  const hiddenFields = new Set(bundle.blinding?.hide ?? []);
  if (!hiddenFields.has("expected")) errors.push("PFU0 bundle blinding must hide rubric-only expected metadata.");
  if (JSON.stringify(study).includes('"expected"') || JSON.stringify(study).includes('"result"')) errors.push("PFU0 study metadata must not echo expected or result.");
}

export function main(argv = process.argv.slice(2)) {
  const errors = [];
  let corpus;
  try {
    corpus = readJson(corpusPath);
  } catch (error) {
    errors.push(`PFU0 corpus cannot load: ${error instanceof Error ? error.message : "unknown error"}.`);
  }
  const checked = corpus ? validateCorpus(corpus) : { errors: [], results: [] };
  errors.push(...checked.errors);
  if (corpus && JSON.stringify(corpus).includes('"expected"')) errors.push("PFU0 corpus must not contain expected echo.");
  const mutations = mutationChecks();
  for (const [name, passed] of Object.entries(mutations)) if (passed !== true) errors.push(`PFU0 mutation check failed: ${name}.`);
  const candidates = checked.results.filter((result) => result.variant === "candidate");
  const candidateExpectations = new Map([
    ["manifest", ["accepted", "current-control"]],
    ["service", ["rejected", "rejected-route"]],
    ["property", ["rejected", "rejected-route"]],
  ]);
  if (candidates.length !== 3 || candidates.some((result) => {
    const expected = candidateExpectations.get(result.family);
    return !expected || result.status !== expected[0] || result.route !== expected[1] || result.promotion !== false;
  })) errors.push("PFU0 candidates must promote only the build manifest; stream fn and implicit observer spellings remain rejected routes.");
  checkStudyArtifacts(errors);
  if (corpus) {
    const projected = snapshotText(checked.results, mutations);
    if (argv.includes("--write")) fs.writeFileSync(snapshotPath, projected, "utf8");
    else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== projected) errors.push("PFU0 snapshot is stale. Run with --write.");
  }
  if (errors.length > 0) {
    process.stderr.write(`${errors.join("\n")}\n`);
    process.exitCode = 1;
    return false;
  }
  const accepted = checked.results.filter((result) => result.status === "accepted").length;
  const rejected = checked.results.filter((result) => result.status === "rejected").length;
  process.stdout.write(`PFU0 closure evidence: ${checked.results.length} cases, ${accepted} accepted, ${rejected} rejected; W-1451 through W-1453 are closed, while later research gates remain out of scope.\n`);
  return true;
}

if (import.meta.main) main();
