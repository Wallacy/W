import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { deriveAtom1, validateAtom1 } from "./atom1-atomic-extensibility-machine.mjs";
import { digestFile, validateAtom1StudyManifest } from "./atom1-atomic-extensibility-manifest.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "atom1-atomic-extensibility-cases.json");
const snapshotPath = path.join(toolingDirectory, "atom1-atomic-extensibility-results.snapshot.jsonl");
const studyDirectory = path.join(toolingDirectory, "studies", "atom1-atomic-extensibility");
const studyManifestPath = path.join(studyDirectory, "study.json");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const writeSnapshot = process.argv.includes("--write");

const manifest = JSON.parse(fs.readFileSync(studyManifestPath, "utf8"));
const manifestErrors = validateAtom1StudyManifest(manifest, { studyDirectory });
if (manifestErrors.length > 0) {
  process.stderr.write(`${manifestErrors.join("\n")}\n`);
  process.exit(1);
}

const validation = validateAtom1(corpus, { root });
if (validation.errors.length > 0) {
  process.stderr.write(`${validation.errors.join("\n")}\n`);
  process.exit(1);
}

const output = {
  schema: "w-atom1-atomic-extensibility-results-1",
  status: "design-oracle-output",
  corpus: "tooling/atom1-atomic-extensibility-cases.json",
  corpusDigest: digestFile(corpusPath),
  metrics: {
    caseCount: validation.results.length,
    axisCounts: Object.fromEntries(["A", "B", "C"].map((axis) => [axis, validation.results.filter((item) => item.axis === axis).length])),
    statusCounts: Object.fromEntries([...new Set(validation.results.map((item) => item.status))].sort().map((status) => [status, validation.results.filter((item) => item.status === status).length])),
  },
  results: validation.results,
};
const snapshot = `${JSON.stringify(output)}\n`;
if (writeSnapshot) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("atom1-atomic-extensibility-results.snapshot.jsonl is stale. Run with --write.\n");
  process.exit(1);
}

const derived = deriveAtom1(corpus);
if (derived.length !== validation.results.length) {
  process.stderr.write("ATOM1 validator and projection disagree on case count.\n");
  process.exit(1);
}
process.stdout.write(
  `ATOM1 atomic extensibility: ${output.metrics.caseCount} cases, ` +
  `${output.metrics.axisCounts.A}/${output.metrics.axisCounts.B}/${output.metrics.axisCounts.C} axes, ` +
  `${Object.keys(output.metrics.statusCounts).length} derived statuses.\n`,
);
