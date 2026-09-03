import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { deriveAvf0, validateAvf0 } from "./avf0-availability-feature-machine.mjs";

const tooling = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(tooling, "..");
const corpusPath = path.join(tooling, "avf0-availability-feature-cases.json");
const snapshotPath = path.join(tooling, "avf0-availability-feature-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const errors = [...validateAvf0(corpus).errors];

function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

for (const [id, source] of Object.entries(corpus.sources ?? {})) {
  const file = path.resolve(tooling, source.path ?? "");
  const relative = path.relative(root, file);
  if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative) || !fs.existsSync(file)) {
    errors.push(`source ${id} is outside the repository or missing.`);
    continue;
  }
  if (source.digest !== digestFile(file)) errors.push(`source ${id} digest is stale.`);
  const text = fs.readFileSync(file, "utf8");
  const occurrences = typeof source.symbol === "string" && source.symbol !== "" ? text.split(source.symbol).length - 1 : 0;
  if (occurrences !== 1) errors.push(`source ${id} symbol must occur exactly once; got ${occurrences}.`);
  if (typeof source.claim !== "string" || source.claim.trim() === "") errors.push(`source ${id} claim is missing.`);
}

const allowedHosts = new Set(["blog.cloudflare.com", "docs.swift.org", "openfeature.dev"]);
const officialUrls = new Set();
for (const [index, source] of (corpus.officialSources ?? []).entries()) {
  let url;
  try { url = new URL(source.url); } catch { url = undefined; }
  if (!url || url.protocol !== "https:" || !allowedHosts.has(url.hostname)) errors.push(`officialSources[${index}] is not an allowlisted primary URL.`);
  if (url && officialUrls.has(url.href)) errors.push(`officialSources[${index}] duplicates a URL.`);
  if (url) officialUrls.add(url.href);
  if (typeof source.claim !== "string" || source.claim.trim() === "") errors.push(`officialSources[${index}] claim is missing.`);
}
if ((corpus.officialSources ?? []).length < 5) errors.push("AVF0 official source set is incomplete.");

const results = deriveAvf0(corpus);
for (const [index, testCase] of corpus.cases.entries()) {
  const result = results[index];
  if (result.status !== testCase.expected.status || result.code !== testCase.expected.code) {
    errors.push(`${testCase.id} expected ${testCase.expected.status}/${testCase.expected.code}, got ${result.status}/${result.code}.`);
  }
}

const count = (field) => Object.fromEntries(
  [...new Set(results.map((result) => result[field]))].sort().map((value) => [value, results.filter((result) => result[field] === value).length]),
);
const output = {
  schema: "w-avf0-availability-feature-results-1",
  status: "design-oracle-output",
  corpus: "tooling/avf0-availability-feature-cases.json",
  corpusDigest: digestFile(corpusPath),
  metrics: {
    caseCount: results.length,
    axisCounts: count("axis"),
    routeCounts: count("route"),
    statusCounts: count("status"),
    packageCurrent: results.filter((result) => result.axis === "package" && result.route === "current").length,
    availabilityEvidenceGap: results.filter((result) => result.axis === "availability" && result.route === "current-design-evidence-gap").length,
    runtimeComposable: results.filter((result) => result.axis === "runtime" && result.route === "composable").length,
    authorityRejections: results.filter((result) => ["availabilityCannotGrantCapability", "availabilityCannotGrantEffect", "runtimeFeatureAuthorityRejected", "featureCannotNarrowAvailability"].includes(result.code)).length,
  },
  results,
};

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}
const snapshot = `${JSON.stringify(output)}\n`;
if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write("AVF0 snapshot is stale; run with --write.\n");
  process.exit(1);
}
process.stdout.write(`AVF0 availability/features: ${results.length} cases, ${output.metrics.statusCounts.accepted ?? 0} accepted, ${output.metrics.statusCounts.rejected ?? 0} rejected.\n`);
