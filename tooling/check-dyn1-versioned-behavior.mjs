import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { digestFile, validateDyn1StudyManifest } from "./dyn1-versioned-behavior-manifest.mjs";
import { REQUIRED_CASES, evaluateDyn1Case, validateDyn1 } from "./dyn1-versioned-behavior-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "dyn1-versioned-behavior-cases.json");
const snapshotPath = path.join(toolingDirectory, "dyn1-versioned-behavior-results.snapshot.jsonl");
const studyDirectory = path.join(toolingDirectory, "studies", "dyn1-versioned-behavior");
const studyPath = path.join(studyDirectory, "study.json");
const writeSnapshot = process.argv.includes("--write");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const study = JSON.parse(fs.readFileSync(studyPath, "utf8"));

const manifestErrors = validateDyn1StudyManifest(study, { studyDirectory, repositoryRoot, allowStaleSnapshot: writeSnapshot });
if (manifestErrors.length > 0) {
  process.stderr.write(`${manifestErrors.join("\n")}\n`);
  process.exit(1);
}
const validation = validateDyn1(corpus, { root: repositoryRoot });
if (validation.errors.length > 0) {
  process.stderr.write(`${validation.errors.join("\n")}\n`);
  process.exit(1);
}
for (const id of REQUIRED_CASES) {
  if (!validation.results.some((result) => result.caseId === id)) {
    process.stderr.write(`DYN1 required case missing from result: ${id}\n`);
    process.exit(1);
  }
}

const routeCounts = {};
const statusCounts = {};
const familyCounts = {};
for (const testCase of corpus.cases) familyCounts[testCase.family] = (familyCounts[testCase.family] ?? 0) + 1;
for (const result of validation.results) {
  routeCounts[result.route] = (routeCounts[result.route] ?? 0) + 1;
  statusCounts[result.status] = (statusCounts[result.status] ?? 0) + 1;
}
const output = {
  schema: "w-dyn1-versioned-behavior-results-1",
  status: "design-oracle-output",
  evidence: { kind: "host-design-oracle", eventDerived: true, claimsRuntime: false, claimsCompiler: false, claimsProvider: false },
  corpus: "tooling/dyn1-versioned-behavior-cases.json",
  corpusDigest: digestFile(corpusPath),
  metrics: {
    caseCount: validation.results.length,
    familyCounts,
    routeCounts,
    statusCounts,
    pairedProjections: validation.results.filter((result) => result.mode === "paired").length,
    staleGenerationRejections: validation.results.reduce((total, result) => total + (result.staleRejections?.length ?? 0), 0),
    exportDigests: validation.results.filter((result) => result.exportDigest !== null).length,
    rejectedMechanisms: validation.results.filter((result) => result.route === "intentionally-rejected").length,
    historicalCandidateCases: validation.results.filter((result) => result.route === "historical-candidate").length,
    degradedAfterSwitch: validation.results.filter((result) => result.status === "degraded").length,
    unknownCrashOutcomes: validation.results.filter((result) => result.crash?.decision === "unknown").length,
    nativeMappingRetained: validation.results.filter((result) => result.ffiRelease === "native-release-mapping-pinned").length,
    isolatedFullUnload: validation.results.filter((result) => result.ffiRelease === "isolated-stop-after-drain").length,
  },
  results: validation.results,
};
const snapshot = `${JSON.stringify(output)}\n`;
if (writeSnapshot) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("dyn1-versioned-behavior-results.snapshot.jsonl is stale. Run with --write.\n");
  process.exit(1);
}

const independent = validation.results.filter((result) => result.mode === "paired");
if (independent.some((result) => result.code === "projection-divergence")) {
  process.stderr.write("DYN1 corpus contains an unguarded projection divergence.\n");
  process.exit(1);
}
process.stdout.write(
  `DYN1 versioned behavior: ${output.metrics.caseCount} cases, ` +
    `${output.metrics.pairedProjections} paired projections, ` +
    `${output.metrics.rejectedMechanisms} rejected mechanisms, ` +
    `${output.metrics.historicalCandidateCases} historical candidate cases, ` +
    `${output.metrics.degradedAfterSwitch} degraded post-switch outcomes.\n`,
);
