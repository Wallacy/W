import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { deriveIpc1, digestFile, validateIpc1 } from "./ipc1-mapped-ipc-machine.mjs";
import { validateIpc1StudyManifest } from "./ipc1-mapped-ipc-manifest.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "ipc1-mapped-ipc-cases.json");
const snapshotPath = path.join(toolingDirectory, "ipc1-mapped-ipc-results.snapshot.jsonl");
const studyDirectory = path.join(toolingDirectory, "studies", "ipc1-mapped-ipc");
const manifestPath = path.join(studyDirectory, "study.json");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
const writeSnapshot = process.argv.includes("--write");

const manifestErrors = validateIpc1StudyManifest(manifest, { studyDirectory });
const validation = validateIpc1(corpus, { root });
if (manifestErrors.length > 0 || validation.errors.length > 0) {
  process.stderr.write(`${[...manifestErrors, ...validation.errors].join("\n")}\n`);
  process.exit(1);
}

const derived = deriveIpc1(corpus);
if (derived.length !== validation.results.length) {
  process.stderr.write("IPC1 validator and projection disagree on case count.\n");
  process.exit(1);
}

const output = {
  schema: "w-ipc1-mapped-ipc-results-1",
  status: "design-oracle-output-ipc1",
  corpus: "tooling/ipc1-mapped-ipc-cases.json",
  corpusDigest: digestFile(corpusPath),
  metrics: {
    caseCount: derived.length,
    axisCounts: Object.fromEntries(["baseline", "immutable", "channel", "lifecycle", "provider"].map((axis) => [axis, derived.filter((item) => item.axis === axis).length])),
    statusCounts: Object.fromEntries([...new Set(derived.map((item) => item.status))].sort().map((status) => [status, derived.filter((item) => item.status === status).length])),
    targetProjectionCount: derived.reduce((count, item) => count + (item.targetProjections?.length ?? 0), 0),
    targetEquivalentCount: derived.filter((item) => item.targetEquivalent).length,
    unknownDurabilityCount: derived.filter((item) => item.code === "unknownDurability").length,
    faultedGenerationCount: derived.filter((item) => item.code === "generation-fault").length,
  },
  results: derived,
};
const snapshot = `${JSON.stringify(output)}\n`;
if (writeSnapshot) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("ipc1-mapped-ipc-results.snapshot.jsonl is stale. Run with --write.\n");
  process.exit(1);
}
process.stdout.write(`IPC1 mapped IPC: ${output.metrics.caseCount} cases, ${output.metrics.targetProjectionCount} target projections, ${output.metrics.unknownDurabilityCount} unknown durability outcomes.\n`);
