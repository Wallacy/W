import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  deriveInputStimulus,
  digestFile,
  validateProtocol,
} from "./hum0-human-review-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const protocolPath = path.join(toolingDirectory, "hum0-human-review-protocol.json");

function repositoryFile(relativePath) {
  if (typeof relativePath !== "string" || relativePath.length === 0 || path.isAbsolute(relativePath)) {
    throw new Error(`invalid repository-relative path: ${String(relativePath)}`);
  }
  const resolved = path.resolve(repositoryRoot, relativePath);
  const relative = path.relative(repositoryRoot, resolved);
  if (relative.startsWith("..") || path.isAbsolute(relative)) {
    throw new Error(`path escapes the repository: ${relativePath}`);
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    throw new Error(`referenced file does not exist: ${relativePath}`);
  }
  return resolved;
}

function protectedShape(protocol) {
  const copy = structuredClone(protocol);
  for (const slice of copy.slices ?? []) {
    for (const reference of [...(slice.sourceRefs ?? []), ...(slice.oracleRefs ?? [])]) {
      reference.digest = "<mechanical-file-digest>";
    }
    for (const input of slice.inputs ?? []) {
      if (input.stimulus) input.stimulus.derivedStimulusDigest = "<mechanical-stimulus-digest>";
    }
  }
  return JSON.stringify(copy);
}

const protocol = JSON.parse(fs.readFileSync(protocolPath, "utf8"));
const beforeShape = protectedShape(protocol);
let fileDigests = 0;
let stimuli = 0;

for (const slice of protocol.slices ?? []) {
  for (const reference of [...(slice.sourceRefs ?? []), ...(slice.oracleRefs ?? [])]) {
    const next = digestFile(repositoryFile(reference.path));
    if (reference.digest !== next) {
      reference.digest = next;
      fileDigests++;
    }
  }
  for (const input of slice.inputs ?? []) {
    const derived = deriveInputStimulus(slice, input, { root: repositoryRoot });
    const nonDigestErrors = derived.errors.filter((error) => !error.includes("derivedStimulusDigest is stale"));
    if (nonDigestErrors.length > 0 || !derived.digest) {
      throw new Error(nonDigestErrors.join("\n") || `could not derive stimulus ${slice.id}/${input.id}`);
    }
    if (input.stimulus.derivedStimulusDigest !== derived.digest) {
      input.stimulus.derivedStimulusDigest = derived.digest;
      stimuli++;
    }
  }
}

if (protectedShape(protocol) !== beforeShape) {
  throw new Error("refresh attempted to change reviewed HUM0 protocol structure");
}
const errors = validateProtocol(protocol, { root: repositoryRoot });
if (errors.length > 0) throw new Error(errors.join("\n"));
fs.writeFileSync(protocolPath, `${JSON.stringify(protocol, null, 2)}\n`);
process.stdout.write(
  `HUM0 evidence refreshed: ${fileDigests} file digests, ${stimuli} derived stimuli; ` +
  "reviewed tasks and protocol structure unchanged.\n",
);
