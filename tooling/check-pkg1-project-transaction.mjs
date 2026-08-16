import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  derivePkg1,
  validatePkg1,
} from "./pkg1-project-transaction-machine.mjs";
import {
  digestFile,
  validatePkg1StudyManifest,
} from "./pkg1-project-transaction-manifest.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "pkg1-project-transaction-cases.json");
const snapshotPath = path.join(toolingDirectory, "pkg1-project-transaction-results.snapshot.jsonl");
const studyDirectory = path.join(toolingDirectory, "studies", "pkg1-project-transaction");
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const manifest = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const errors = [];

function resolveContained(relative, location) {
  const resolved = path.resolve(toolingDirectory, relative ?? "");
  const relativeToRoot = path.relative(root, resolved);
  if (relativeToRoot.startsWith(`..${path.sep}`) || path.isAbsolute(relativeToRoot) || !fs.existsSync(resolved)) {
    errors.push(`${location} is outside the repository or missing.`);
    return undefined;
  }
  return resolved;
}

if (!Array.isArray(corpus.officialSources) || corpus.officialSources.length < 5) errors.push("PKG1 official source allowlist is incomplete.");
const hosts = new Set(["pubs.opengroup.org", "learn.microsoft.com", "doc.rust-lang.org", "packaging.python.org"]);
const officialUrls = new Set();
for (const [index, source] of (corpus.officialSources ?? []).entries()) {
  let url;
  try { url = new URL(source.url); } catch { url = undefined; }
  if (!url || url.protocol !== "https:" || !hosts.has(url.hostname)) errors.push(`officialSources[${index}] is not an allowlisted HTTPS primary source.`);
  if (url && officialUrls.has(url.href)) errors.push(`officialSources[${index}] duplicates a URL.`);
  if (url) officialUrls.add(url.href);
  if (typeof source.claim !== "string" || source.claim.trim() === "") errors.push(`officialSources[${index}] claim is missing.`);
}

for (const [caseIndex, testCase] of (corpus.cases ?? []).entries()) {
  for (const [referenceIndex, reference] of (testCase.references ?? []).entries()) {
    const file = resolveContained(reference.path, `${testCase.id}.references[${referenceIndex}].path`);
    if (!file || typeof reference.symbol !== "string" || !fs.readFileSync(file, "utf8").includes(reference.symbol)) errors.push(`${testCase.id} reference symbol is absent.`);
  }
  if (!testCase.references?.length) errors.push(`${testCase.id} has no Last Light reference.`);
}

const validation = validatePkg1(corpus, { root });
errors.push(...validation.errors);
const results = derivePkg1(corpus);
const resultById = new Map(results.map((result) => [result.caseId, result]));
for (const testCase of corpus.cases ?? []) {
  const result = resultById.get(testCase.id);
  if (!result) continue;
  if (result.status !== testCase.expected.status || (testCase.expected.code && result.code !== testCase.expected.code)) {
    errors.push(`${testCase.id} expected ${testCase.expected.status}/${testCase.expected.code ?? "<any>"}, got ${result.status}/${result.code}.`);
  }
}

const manifestErrors = validatePkg1StudyManifest(manifest, {
  studyDirectory,
  repositoryRoot: root,
  corpus,
});
errors.push(...manifestErrors);

const mutationTests = [];
const stale = structuredClone(corpus);
stale.cases.find((testCase) => testCase.id === "PKG1-stale-concurrent-writer").operations = [];
stale.cases.find((testCase) => testCase.id === "PKG1-stale-concurrent-writer").fixtures = [];
mutationTests.push(["stale case removed", () => validatePkg1(stale).errors.length > 0]);
const forged = structuredClone(corpus);
forged.cases.find((testCase) => testCase.id === "PKG1-caller-echo-rejected").operations[0].result = { status: "accepted" };
mutationTests.push(["caller echo mutation", () => derivePkg1(forged).find((result) => result.caseId === "PKG1-caller-echo-rejected")?.code === "callerEchoRejected"]);
const digestForgery = structuredClone(manifest);
digestForgery.sourceRefs[0].digest = "sha256:ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
mutationTests.push(["source digest mutation", () => validatePkg1StudyManifest(digestForgery, { studyDirectory, repositoryRoot: root, corpus }).some((error) => error.includes("digest is stale"))]);
const duplicateRef = structuredClone(manifest);
duplicateRef.sourceRefs.push(structuredClone(duplicateRef.sourceRefs[0]));
mutationTests.push(["duplicate source ref mutation", () => validatePkg1StudyManifest(duplicateRef, { studyDirectory, repositoryRoot: root, corpus }).some((error) => error.includes("duplicates"))]);
const staleArtifact = structuredClone(manifest);
staleArtifact.artifactRefs[0].digest = "sha256:ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
mutationTests.push(["artifact digest mutation", () => validatePkg1StudyManifest(staleArtifact, { studyDirectory, repositoryRoot: root, corpus }).some((error) => error.includes("artifactRefs[0] digest is stale"))]);
const duplicateArtifact = structuredClone(manifest);
duplicateArtifact.artifactRefs.push(structuredClone(duplicateArtifact.artifactRefs[0]));
mutationTests.push(["duplicate artifact mutation", () => validatePkg1StudyManifest(duplicateArtifact, { studyDirectory, repositoryRoot: root, corpus }).some((error) => error.includes("artifactRefs") && error.includes("duplicates"))]);
for (const [name, check] of mutationTests) if (!check()) errors.push(`PKG1 mutation test failed: ${name}.`);

const output = {
  schema: "w-pkg1-project-transaction-results-1",
  status: "design-oracle-output",
  corpus: "tooling/pkg1-project-transaction-cases.json",
  corpusDigest: digestFile(casesPath),
  metrics: {
    caseCount: results.length,
    statusCounts: Object.fromEntries([...new Set(results.map((result) => result.status))].sort().map((status) => [status, results.filter((result) => result.status === status).length])),
    currentRoutes: ["identity-split", "atomic-replace"],
    researchRoutes: ["durable-provider-receipt"],
  },
  results,
};
const snapshot = `${JSON.stringify(output)}\n`;
if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}
if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("pkg1-project-transaction-results.snapshot.jsonl is stale; run with --write.\n");
  process.exit(1);
}
process.stdout.write(`PKG1 project transaction: ${results.length} cases, ${output.metrics.statusCounts.accepted ?? 0} accepted, ${output.metrics.statusCounts.rejected ?? 0} rejected, ${output.metrics.statusCounts.faulted ?? 0} faulted.\n`);
