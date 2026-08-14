import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { deriveCyc1, validateCyc1 } from "./cyc1-explicit-cycle-machine.mjs";
import { digestFile, validateCyc1StudyManifest } from "./cyc1-explicit-cycle-manifest.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "cyc1-explicit-cycle-cases.json");
const snapshotPath = path.join(toolingDirectory, "cyc1-explicit-cycle-results.snapshot.jsonl");
const studyDirectory = path.join(toolingDirectory, "studies", "cyc1-explicit-cycle-lifecycle");
const studyPath = path.join(studyDirectory, "study.json");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const study = JSON.parse(fs.readFileSync(studyPath, "utf8"));
const writeSnapshot = process.argv.includes("--write");

const manifestErrors = validateCyc1StudyManifest(study, { studyDirectory, repositoryRoot, allowStaleSnapshot: writeSnapshot });
if (manifestErrors.length > 0) {
  process.stderr.write(`${manifestErrors.join("\n")}\n`);
  process.exit(1);
}
const validation = validateCyc1(corpus, { root: repositoryRoot });
if (validation.errors.length > 0) {
  process.stderr.write(`${validation.errors.join("\n")}\n`);
  process.exit(1);
}

const statusCounts = {};
const familyCounts = {};
for (const testCase of corpus.cases) familyCounts[testCase.family] = (familyCounts[testCase.family] ?? 0) + 1;
for (const result of validation.results) statusCounts[result.status] = (statusCounts[result.status] ?? 0) + 1;
const output = {
  schema: "w-cyc1-explicit-cycle-results-1",
  status: "design-oracle-output",
  corpus: "tooling/cyc1-explicit-cycle-cases.json",
  corpusDigest: digestFile(corpusPath),
  metrics: {
    caseCount: validation.results.length,
    familyCounts,
    statusCounts,
    staticSccRejections: validation.results.filter((result) => result.code === "W-OWNERSHIP-0014").length,
    residualDiagnostics: validation.results.filter((result) => result.code === "W-MEMORY-0001").length,
    unknownBoundaries: validation.results.filter((result) => result.code === "W-MEMORY-UNKNOWN-BOUNDARY").length,
    conditionalLivenessResearch: validation.results.filter((result) => result.route === "conditional-liveness").length,
    collectorSideEffects: validation.results.filter((result) => result.mutation !== undefined && result.mutation !== "none").length,
  },
  results: validation.results,
};
const snapshot = `${JSON.stringify(output)}\n`;
if (writeSnapshot) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("cyc1-explicit-cycle-results.snapshot.jsonl is stale. Run with --write.\n");
  process.exit(1);
}

const derived = deriveCyc1(corpus);
if (derived.length !== validation.results.length) {
  process.stderr.write("CYC1 validator and projection disagree on case count.\n");
  process.exit(1);
}
process.stdout.write(
  `CYC1 explicit cycle lifecycle: ${output.metrics.caseCount} cases, ` +
  `${output.metrics.staticSccRejections} static SCC rejections, ` +
  `${output.metrics.residualDiagnostics} residual diagnostics, ` +
  `${output.metrics.unknownBoundaries} unknown boundaries, ` +
  `${output.metrics.conditionalLivenessResearch} conditional-liveness Research cases.\n`,
);
