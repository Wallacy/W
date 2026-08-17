import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  bundlePath,
  corpusPath,
  manifestPath,
  mutationChecks,
  projectResults,
  snapshotPath,
  validateBundle,
  validateCorpus,
  validateManifest,
} from "./final-research-closure-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));

function snapshotText(value) {
  return value.map((record) => JSON.stringify(record)).join("\n") + "\n";
}

export function main(argv = process.argv.slice(2)) {
  const errors = [];
  let corpus;
  let manifest;
  let bundle;
  try {
    corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
    manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
    bundle = JSON.parse(fs.readFileSync(bundlePath, "utf8"));
  } catch (error) {
    errors.push(`FRC0 input cannot load: ${error instanceof Error ? error.message : "unknown error"}.`);
  }

  let corpusResult = { errors: [], results: [] };
  if (corpus) {
    corpusResult = validateCorpus(corpus);
    errors.push(...corpusResult.errors);
  }
  if (manifest) errors.push(...validateManifest(manifest));
  if (bundle) errors.push(...validateBundle(bundle));

  const mutations = mutationChecks();
  for (const [name, passed] of Object.entries(mutations)) {
    if (passed !== true) errors.push(`FRC0 mutation check failed: ${name}.`);
  }

  const projected = projectResults(corpusResult.results, mutations);
  if (argv.includes("--write")) {
    fs.writeFileSync(snapshotPath, snapshotText(projected), "utf8");
  } else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshotText(projected)) {
    errors.push("final-research-closure-results.snapshot.jsonl is stale. Run with --write.");
  }

  if (errors.length > 0) {
    process.stderr.write(`${errors.join("\n")}\n`);
    process.exitCode = 1;
    return false;
  }

  const accepted = corpusResult.results.filter((result) => result.status === "accepted").length;
  const rejected = corpusResult.results.filter((result) => result.status === "rejected").length;
  process.stdout.write(`FRC0 final-research-closure: ${corpusResult.results.length} cases, ${accepted} current accepted, ${rejected} adversarial rejected; mutation guards green.\n`);
  return true;
}

if (import.meta.main) main();
