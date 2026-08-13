import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadCapabilityMatrix, validateCapabilityMatrix } from "./capability-matrix-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryDirectory = path.resolve(toolingDirectory, "..");
const corpusPath = path.resolve(process.argv.includes("--corpus") ? process.argv[process.argv.indexOf("--corpus") + 1] : path.join(toolingDirectory, "capability-matrix-cases.json"));
const snapshotPath = path.join(toolingDirectory, "capability-matrix-results.snapshot.jsonl");
const writeSnapshot = process.argv.includes("--write");
const corpus = loadCapabilityMatrix(corpusPath);
const { errors, results } = validateCapabilityMatrix(corpus, { root: repositoryDirectory, checkSources: true });

function digest(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const output = {
  schema: "w-capability-matrix-results-cap0",
  status: "design-oracle-output-cap0",
  corpus: path.relative(repositoryDirectory, corpusPath).replaceAll(path.sep, "/"),
  corpusDigest: digest(corpusPath),
  axisCount: results.length,
  metrics: {
    sourceRefCount: results.reduce((count, result) => count + result.sourceRefCount, 0),
    routeCounts: Object.fromEntries([...new Set(results.map((result) => result.derivedRoute))].sort().map((route) => [route, results.filter((result) => result.derivedRoute === route).length])),
    documentationQueuedCount: corpus.axes.filter((axis) => axis.documentation?.docsStatus === "queued").length,
    canonicalSourceCount: corpus.axes.filter((axis) => axis.lastLight?.canonicalSource?.digest).length,
    subcapabilityCount: corpus.axes.reduce((count, axis) => count + (axis.coverage?.subcapabilities?.length ?? 0), 0),
  },
  results,
};
const snapshot = `${JSON.stringify(output)}\n`;
if (writeSnapshot) fs.writeFileSync(snapshotPath, snapshot, "utf8");
else if (fs.existsSync(snapshotPath) && fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("capability-matrix-results.snapshot.jsonl is stale. Run with --write.\n");
  process.exit(1);
}
process.stdout.write(`CAP0 capability matrix: ${results.length} axes, ${output.metrics.sourceRefCount} source refs, ${output.metrics.subcapabilityCount} subcapabilities.\n`);
