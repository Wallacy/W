import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { deriveSyn1, digestFile, validateSyn1 } from "./syn1-typed-generation-machine.mjs";
import { validateSyn1StudyManifest } from "./syn1-typed-generation-manifest.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "syn1-typed-generation-cases.json");
const snapshotPath = path.join(toolingDirectory, "syn1-typed-generation-results.snapshot.jsonl");
const studyDirectory = path.join(toolingDirectory, "studies", "syn1-typed-generation");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const manifest = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const writeSnapshot = process.argv.includes("--write");

const validation = validateSyn1(corpus, { root });
const manifestErrors = validateSyn1StudyManifest(manifest, { studyDirectory });
if (validation.errors.length > 0 || manifestErrors.length > 0) {
  process.stderr.write(`${[...validation.errors, ...manifestErrors].join("\n")}\n`);
  process.exit(1);
}

const results = deriveSyn1(corpus);
const countBy = (items, key) => Object.fromEntries([...new Set(items.map((item) => item[key]))].sort().map((value) => [value, items.filter((item) => item[key] === value).length]));
const output = {
  schema: "w-syn1-typed-generation-results-3",
  status: "design-oracle-output-syn1",
  corpus: "tooling/syn1-typed-generation-cases.json",
  corpusDigest: digestFile(corpusPath),
  study: "tooling/studies/syn1-typed-generation/study.json",
  metrics: {
    caseCount: results.length,
    axisCounts: countBy(results, "axis"),
    routeCounts: countBy(results, "route"),
    statusCounts: countBy(results, "status"),
    codeCounts: countBy(results, "code"),
    targetVariantProjectionCount: results.reduce((count, result) => count + (result.targetVariants?.length ?? 0), 0),
    targetEquivalentCount: results.filter((result) => result.targetEquivalent === true).length,
    targetInterfaceChangeCount: results.filter((result) => result.targetInterfaceChanged === true).length,
    acceptedCandidateTraceCount: results.filter((result) => result.status === "accepted" && result.route === "research-candidate").length,
    actionResultPublishedCount: results.filter((result) => result.actionResultPublished === true).length,
    candidateInterfacePublishedCount: results.filter((result) => result.interfacePublished === true && result.route === "research-candidate").length,
    compilerCachePublishedCount: results.filter((result) => result.compilerCachePublished === true).length,
    treeSitterParsedCandidateFileCount: manifest.generatedArtifacts.length,
    interfaceChangeCount: results.filter((result) => result.interfaceChanged === true).length,
    sourceMapFixableCount: results.filter((result) => result.sourceMapFixable === true).length,
    observedTraceCount: results.filter((result) => Array.isArray(result.observedTrace) && result.observedTrace.length > 0).length,
    requiredPhaseTraceCount: results.filter((result) => Array.isArray(result.requiredPhaseTrace) && result.requiredPhaseTrace.length > 0).length,
    actionIdentityCount: results.filter((result) => result.targetVariants?.some((variant) => variant.actionIdentity)).length,
    discardCount: results.filter((result) => result.status === "discarded").length,
  },
  results,
};
const snapshot = `${JSON.stringify(output)}\n`;
if (writeSnapshot) fs.writeFileSync(snapshotPath, snapshot, "utf8");
else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("syn1-typed-generation-results.snapshot.jsonl is stale. Run with --write.\n");
  process.exit(1);
}
process.stdout.write(`SYN1 typed generation: ${output.metrics.caseCount} cases, ${output.metrics.acceptedCandidateTraceCount} accepted candidate traces, ${output.metrics.targetVariantProjectionCount} target projections.\n`);
