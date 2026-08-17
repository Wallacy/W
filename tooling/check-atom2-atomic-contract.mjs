import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { buildAtom2Snapshot, validateAtom2 } from "./atom2-atomic-contract-machine.mjs";
import { digestFile, validateAtom2StudyManifest } from "./atom2-atomic-contract-manifest.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const studyDirectory = path.join(toolingDirectory, "studies", "atom2-atomic-contract");
const corpusPath = path.join(toolingDirectory, "atom2-atomic-contract-cases.json");
const snapshotPath = path.join(toolingDirectory, "atom2-atomic-contract-results.snapshot.jsonl");
const studyPath = path.join(studyDirectory, "study.json");
const diagnosticCatalogPath = path.join(toolingDirectory, "diagnostic-catalog.json");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const study = JSON.parse(fs.readFileSync(studyPath, "utf8"));
const diagnosticCatalog = JSON.parse(fs.readFileSync(diagnosticCatalogPath, "utf8"));
const errors = validateAtom2StudyManifest(study, { studyDirectory, allowPending: process.argv.includes("--write") });
const checked = validateAtom2(corpus, { root: repositoryRoot });
errors.push(...checked.errors);
const atomicDiagnosticProfiles = Object.freeze({
  "W-ATOMIC-0001": "atomic-type",
  "W-ATOMIC-0002": "atomic-type",
  "W-ATOMIC-0003": "atomic-interface",
  "W-ATOMIC-0004": "atomic-effect",
  "W-ATOMIC-0005": "atomic-effect",
  "W-ATOMIC-0006": "atomic-interface",
  "W-ATOMIC-0007": "atomic-ownership",
  "W-ATOMIC-0008": "atomic-ownership",
  "W-ATOMIC-0009": "atomic-ownership",
  "W-ATOMIC-0010": "atomic-ownership",
  "W-ATOMIC-0011": "atomic-ownership",
  "W-ATOMIC-0012": "atomic-effect",
  "W-ATOMIC-0013": "atomic-type",
  "W-ATOMIC-0014": "atomic-effect",
  "W-ATOMIC-0015": "atomic-effect",
  "W-ATOMIC-0016": "atomic-effect",
});
const expectedAtomicCodes = new Set(Object.keys(atomicDiagnosticProfiles));
const catalogAtomicEntries = diagnosticCatalog.codes.filter((entry) => entry.code?.startsWith("W-ATOMIC-"));
for (const [code, profile] of Object.entries(atomicDiagnosticProfiles)) {
  const entry = diagnosticCatalog.codes.find((candidate) => candidate.code === code);
  if (!entry || entry.state !== "active") {
    errors.push(`diagnostic catalog is missing active ${code}`);
    continue;
  }
  if (entry.profile !== profile) errors.push(`${code} must use profile ${profile}, got ${entry.profile ?? "none"}`);
  if (!diagnosticCatalog.profiles?.[profile]) errors.push(`${code} references missing profile ${profile}`);
}
for (const entry of catalogAtomicEntries) {
  if (!expectedAtomicCodes.has(entry.code)) errors.push(`diagnostic catalog has unexpected atomic code ${entry.code}`);
}
const snapshot = buildAtom2Snapshot(corpus);
const snapshotText = `${JSON.stringify({ schema: "w-atom2-atomic-contract-results-1", corpus: "tooling/atom2-atomic-contract-cases.json", corpusDigest: digestFile(corpusPath), ...snapshot })}\n`;
if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, snapshotText, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshotText) {
  errors.push("ATOM2 snapshot is stale; run `bun tooling/check-atom2-atomic-contract.mjs --write`.");
}
if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}
process.stdout.write(`ATOM2 atomic contract: ${checked.results.length} cases, ${snapshot.metrics.activeResearchStatuses} active Research statuses, ${snapshot.metrics.statusCounts["promoted-value-record"] ?? 0} promoted carriers, ${snapshot.metrics.statusCounts.rejected ?? 0} rejected.\n`);
