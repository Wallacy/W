import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { deriveSec0, validateSec0, PROFILES } from "./sec0-security-model-machine.mjs";

const tooling = import.meta.dir;
const root = path.resolve(tooling, "..");
const corpusPath = path.join(tooling, "sec0-security-model-cases.json");
const snapshotPath = path.join(tooling, "sec0-security-model-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const errors = [...validateSec0(corpus).errors];
const digestFile = (file) => `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;

for (const [id, source] of Object.entries(corpus.sources ?? {})) {
  const file = path.resolve(root, source.path ?? "");
  const relative = path.relative(root, file);
  if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative) || !fs.existsSync(file)) {
    errors.push(`source ${id} is outside the repository or missing.`);
    continue;
  }
  if (source.digest !== undefined && source.digest !== digestFile(file)) errors.push(`source ${id} digest is stale.`);
  const text = fs.readFileSync(file, "utf8");
  const occurrences = typeof source.symbol === "string" && source.symbol !== "" ? text.split(source.symbol).length - 1 : 0;
  if (occurrences !== 1) errors.push(`source ${id} symbol must occur exactly once; got ${occurrences}.`);
  if (typeof source.claim !== "string" || source.claim.trim() === "") errors.push(`source ${id} claim is missing.`);
}

const allowedHosts = new Set([
  "developers.cloudflare.com",
  "www.kernel.org",
  "webassembly.github.io",
  "wasi.dev",
  "www.rfc-editor.org",
  "www.sigstore.dev",
]);
const officialUrls = new Set();
for (const [index, source] of (corpus.officialSources ?? []).entries()) {
  let url;
  try { url = new URL(source.url); } catch { url = undefined; }
  if (!url || url.protocol !== "https:" || !allowedHosts.has(url.hostname)) errors.push(`officialSources[${index}] is not an allowlisted primary URL.`);
  if (url && officialUrls.has(url.href)) errors.push(`officialSources[${index}] duplicates a URL.`);
  if (url) officialUrls.add(url.href);
  if (typeof source.claim !== "string" || source.claim.trim() === "") errors.push(`officialSources[${index}] claim is missing.`);
}
if ((corpus.officialSources ?? []).length < 5) errors.push("SEC0 official source set is incomplete.");

const results = deriveSec0(corpus);
for (const [index, testCase] of corpus.cases.entries()) {
  const result = results[index];
  if (result.status !== testCase.expected.status || result.code !== testCase.expected.code) {
    errors.push(`${testCase.id} expected ${testCase.expected.status}/${testCase.expected.code}, got ${result.status}/${result.code}.`);
  }
}

const count = (field) => Object.fromEntries(
  [...new Set(results.map((result) => result[field]))].sort().map((value) => [value, results.filter((result) => result[field] === value).length]),
);
const profileCases = results.filter((result) => result.axis === "profileIsolation");
for (const profile of PROFILES) if (!profileCases.some((result) => result.profile === profile && result.status === "accepted")) errors.push(`profile ${profile} lacks an accepted case.`);
const authorityCodes = new Set([
  "safeAuthorityRejected",
  "ambientAuthorityRejected",
  "capabilityAmplificationRejected",
  "featureSecurityAuthorityRejected",
  "deploymentWeakensProductMinimum",
  "tenantBoundaryRejected",
  "undefinedBehaviorRejected",
]);
const output = {
  schema: "w-sec0-security-model-results-1",
  status: "design-oracle-output",
  corpus: "tooling/sec0-security-model-cases.json",
  corpusDigest: digestFile(corpusPath),
  metrics: {
    caseCount: results.length,
    kindCounts: count("axis"),
    routeCounts: count("route"),
    statusCounts: count("status"),
    profileCount: PROFILES.length,
    acceptedProfiles: new Set(profileCases.filter((result) => result.status === "accepted").map((result) => result.profile)).size,
    currentAccepted: results.filter((result) => result.status === "accepted" && result.route === "current").length,
    researchAccepted: results.filter((result) => result.status === "accepted" && result.route === "research").length,
    authorityRejections: results.filter((result) => authorityCodes.has(result.code)).length,
    callerEchoRejections: results.filter((result) => result.code === "callerEchoRejected").length,
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
  process.stderr.write("SEC0 snapshot is stale; run with --write.\n");
  process.exit(1);
}
process.stdout.write(`SEC0 security model: ${results.length} cases, ${output.metrics.statusCounts.accepted ?? 0} accepted, ${output.metrics.statusCounts.rejected ?? 0} rejected, ${PROFILES.length} profiles.\n`);
