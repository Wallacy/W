import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { digestFile, validateHotReloadStudyManifest } from "./hot-reload-dev-manifest.mjs";
import { REQUIRED_CASES, evaluateHotReloadCase, evaluateHotReloadMutation, validateHotReload } from "./hot-reload-dev-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "hot-reload-dev-cases.json");
const snapshotPath = path.join(toolingDirectory, "hot-reload-dev-results.snapshot.jsonl");
const studyDirectory = path.join(toolingDirectory, "studies", "hrd0-hot-reload-dev");
const studyPath = path.join(studyDirectory, "study.json");
const writeSnapshot = process.argv.includes("--write");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const study = JSON.parse(fs.readFileSync(studyPath, "utf8"));

const manifestErrors = validateHotReloadStudyManifest(study, { studyDirectory, repositoryRoot, allowStaleSnapshot: writeSnapshot });
if (manifestErrors.length > 0) {
  process.stderr.write(`${manifestErrors.join("\n")}\n`);
  process.exit(1);
}

const validation = validateHotReload(corpus, { root: repositoryRoot });
if (validation.errors.length > 0) {
  process.stderr.write(`${validation.errors.join("\n")}\n`);
  process.exit(1);
}
for (const id of REQUIRED_CASES) {
  if (!validation.results.some((result) => result.caseId === id)) {
    const testCase = corpus.cases.find((item) => item.id === id);
    if (!testCase) {
      process.stderr.write(`HRD0 required case missing from corpus: ${id}\n`);
      process.exit(1);
    }
  }
}

const results = validation.results.map((result, index) => ({ ...result, caseId: corpus.cases[index].id }));
const mutationResults = (corpus.adversarialMutations ?? []).map((mutation) => ({
  ...evaluateHotReloadMutation(mutation, { corpus }),
  expectedCode: mutation.expectedCode,
}));
const routeCounts = {};
const statusCounts = {};
const familyCounts = {};
for (const testCase of corpus.cases) familyCounts[testCase.family] = (familyCounts[testCase.family] ?? 0) + 1;
for (const result of results) {
  routeCounts[result.route] = (routeCounts[result.route] ?? 0) + 1;
  statusCounts[result.status] = (statusCounts[result.status] ?? 0) + 1;
  if (result.status === "pending") {
    process.stderr.write(`HRD0 case remained pending: ${result.caseId}\n`);
    process.exit(1);
  }
  if (result.mode === "paired" && result.code === "projection-divergence") {
    process.stderr.write(`HRD0 paired projections diverged: ${result.caseId}\n`);
    process.exit(1);
  }
}
for (const result of mutationResults) {
  if (result.code !== result.expectedCode) {
    process.stderr.write(`HRD0 cleanup mutation ${result.mutationId} expected ${result.expectedCode}, got ${result.code}.\n`);
    process.exit(1);
  }
}

const byId = new Map(results.map((result) => [result.caseId, result]));
if (byId.get("HRD0-C-generated-module-reopen-research")?.status !== "research") {
  process.stderr.write("HRD0 generated module candidate must remain Research.\n");
  process.exit(1);
}
if (byId.get("HRD0-C-invocation-spelling-unresolved")?.code !== "invocation-not-selected") {
  process.stderr.write("HRD0 invocation spelling must remain tooling-owned and unselected.\n");
  process.exit(1);
}
if (byId.get("HRD0-D-production-reload-rejected")?.route !== "intentionally-rejected") {
  process.stderr.write("HRD0 production dynamic mode must be rejected.\n");
  process.exit(1);
}

const output = {
  schema: "w-hrd0-hot-reload-dev-results-1",
  status: "design-oracle-output",
  evidence: { kind: "host-design-oracle", eventDerived: true, claimsCompiler: false, claimsRuntime: false, claimsProvider: false, claimsProductionFeature: false },
  corpus: "tooling/hot-reload-dev-cases.json",
  corpusDigest: digestFile(corpusPath),
  metrics: {
    caseCount: results.length,
    familyCounts,
    routeCounts,
    statusCounts,
    pairedProjections: results.filter((result) => result.mode === "paired").length,
    staleGenerationRejections: results.reduce((total, result) => total + (result.staleRejections?.length ?? 0), 0),
    rejectedMechanisms: results.filter((result) => result.route === "intentionally-rejected").length,
    researchCases: results.filter((result) => result.route === "research").length,
    languageSurfaceCases: results.filter((result) => result.languageSurface === "none").length,
    cleanupChecked: results.filter((result) => result.cleanupOrder?.length > 0).length,
    adversarialMutations: mutationResults.length,
  },
  results,
  mutations: mutationResults,
};

const snapshot = `${JSON.stringify(output)}\n`;
if (writeSnapshot) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("hot-reload-dev-results.snapshot.jsonl is stale. Run with --write.\n");
  process.exit(1);
}

process.stdout.write(
  `HRD0 hot reload dev oracle: ${output.metrics.caseCount} cases, ` +
    `${output.metrics.pairedProjections} paired projections, ` +
    `${output.metrics.researchCases} Research cases, ` +
    `${output.metrics.rejectedMechanisms} rejected routes.\n`,
);
