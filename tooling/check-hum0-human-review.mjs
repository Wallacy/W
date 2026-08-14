import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { digestFile, makeSnapshot, validateProtocol } from "./hum0-human-review-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const protocolPath = path.join(toolingDirectory, "hum0-human-review-protocol.json");
const snapshotPath = path.join(toolingDirectory, "hum0-human-review-results.snapshot.jsonl");
const studyPath = path.join(toolingDirectory, "studies", "hum0-human-review", "study.json");
const protocol = JSON.parse(fs.readFileSync(protocolPath, "utf8"));
const study = JSON.parse(fs.readFileSync(studyPath, "utf8"));
const errors = validateProtocol(protocol, { root: repositoryRoot });
const readiness = {
  slices: protocol.slices?.length ?? 0,
  tasks: protocol.slices?.reduce((total, slice) => total + (slice.tasks?.length ?? 0), 0) ?? 0,
  human: protocol.records?.human?.length ?? 0,
  model: protocol.records?.model?.length ?? 0,
};
if (study.status !== "protocol-ready" || study.id !== "HUM0" || study.bundle !== false || study.slices !== readiness.slices || study.tasks !== readiness.tasks || study.records?.human !== readiness.human || study.records?.model !== readiness.model) {
  errors.push("study.json does not mirror HUM0 protocol readiness.");
}
const studyProtocolPath = path.resolve(path.dirname(studyPath), study.protocol?.path ?? "");
if (study.protocol?.digest !== digestFile(studyProtocolPath)) errors.push("study.protocol.digest is stale.");
for (const [name, artifact] of Object.entries(study.artifacts ?? {})) {
  const artifactPath = path.resolve(path.dirname(studyPath), artifact.path ?? "");
  if (artifact.digest !== digestFile(artifactPath)) errors.push(`study.artifacts.${name}.digest is stale.`);
}
if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const snapshot = `${JSON.stringify(makeSnapshot(protocol, errors, { root: repositoryRoot }))}\n`;
if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("hum0-human-review-results.snapshot.jsonl is stale. Run with --write.\n");
  process.exit(1);
}

process.stdout.write("HUM0 protocol-ready: 8 slices, 32 tasks, 0 human records, 0 model records; no score or preference computed.\n");
