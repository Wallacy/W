import fs from "node:fs";
import path from "node:path";
import {
  buildAuthoritySnapshot,
  deriveAuthorityCase,
  sha256Bytes,
  validateAuthorityRegistryCorpus,
} from "./authority-registry-machine.mjs";

const tooling = import.meta.dir;
const root = path.resolve(tooling, "..");
const corpusPath = path.join(tooling, "authority-registry-cases.json");
const snapshotPath = path.join(tooling, "authority-registry-results.snapshot.jsonl");
const corpusText = fs.readFileSync(corpusPath, "utf8");
const corpus = JSON.parse(corpusText);
const validation = validateAuthorityRegistryCorpus(corpus);
if (validation.errors.length > 0) {
  console.error(validation.errors.join("\n"));
  process.exit(1);
}

function failGate(message) {
  console.error(`authority registry: ${message}`);
  process.exit(1);
}

function requireText(text, needle, label) {
  if (!text.includes(needle)) failGate(`${label} is missing`);
}

function requireUniqueText(text, needle, label) {
  const count = text.split(needle).length - 1;
  if (count !== 1) failGate(`${label} must occur once (found ${count})`);
}

const currentCase = corpus.cases.find((entry) => entry.id === "AUL0-W-1469-current");
const current = deriveAuthorityCase(currentCase, corpus).result;
if (current.status !== "accepted" || current.code !== "authorityLineageVerified" ||
    current.continuity?.observedRootVersion !== 2 ||
    !(current.origin instanceof Uint8Array) ||
    !(current.receipt instanceof Uint8Array) ||
    !(current.record instanceof Uint8Array))
  failGate("AUL0-W-1469-current did not produce the accepted bounded authority bytes");

const originRef = `{ object: "${current.originDigest}", length: ${current.origin.length} }`;
const originMarker = `origin: ${originRef}`;
if (!currentCase || JSON.stringify(currentCase.decisions) !== JSON.stringify(["W-1469"]) ||
    currentCase.sourceRef?.path !== "reference/last-light/build.w" ||
    currentCase.sourceRef?.symbol !== originMarker ||
    corpus.sourceRefs?.[0]?.path !== "reference/last-light/build.w" ||
    corpus.sourceRefs?.[0]?.symbol !== originMarker)
  failGate("AUL0-W-1469-current is not source-backed by the Last Light origin marker");

const evidenceRef = `object: "${current.receiptDigest}"\n          length: ${current.receipt.length}`;
const recordRef = `{ object: "${current.recordDigest}", length: ${current.record.length} }`;
const lastLightBuildPath = path.join(root, "reference", "last-light", "build.w");
const lastLightMenuPath = path.join(root, "reference", "last-light", "packages", "menu-compiler", "build.w");
const atlasBuildPath = path.join(root, "reference", "syntax-atlas", "build.w");
const lastLightBuild = fs.readFileSync(lastLightBuildPath, "utf8");
const lastLightMenu = fs.readFileSync(lastLightMenuPath, "utf8");
const atlasBuild = fs.readFileSync(atlasBuildPath, "utf8");

requireUniqueText(lastLightBuild, originMarker, "Last Light AuthorityOrigin lock marker");
requireText(lastLightBuild, "    authorities: [", "Last Light resolution authorities table");
requireText(lastLightBuild, "        kind: .registry", "registry authority kind");
requireText(lastLightBuild, '        locator: "w"', "registry authority locator");
requireText(lastLightBuild, `        origin: ${originRef}`, "full AuthorityOrigin CAS ref");
requireText(lastLightBuild, `          ${evidenceRef}`, "full AuthorityEvidence CAS ref");
requireText(lastLightBuild, "          observedRootVersion: 2", "observed root version evidence");
requireText(lastLightBuild, `        record: ${recordRef}`, "full AuthorityRecord CAS ref");
if (lastLightBuild.includes("trustedGenesis") || lastLightBuild.includes("trustedCheckpoint"))
  failGate("Last Light lock promotes trust inputs");

for (const packageName of ["last-light/restaurant", "fiction/chart", "last-light/menu-compiler"]) {
  requireText(
    lastLightBuild,
    `authority: ${originRef}\n        name: "${packageName}"`,
    `${packageName} package authority-origin ref`,
  );
}
if (lastLightBuild.includes('authority: "w"'))
  failGate("Last Light package node still uses the locator alias as authority");

requireText(
  lastLightBuild,
  'package {\n  schema: "w.package/1"\n  authority: .registry("w")\n  name: "last-light/restaurant"',
  "Last Light package manifest authority",
);
requireText(
  lastLightMenu,
  'package {\n  schema: "w.package/1"\n  authority: .registry("w")\n  name: "last-light/menu-compiler"',
  "menu compiler package manifest authority",
);
requireText(
  atlasBuild,
  'package {\n  schema: "w.package/1"\n  authority: .registry("w")\n  name: "atlas/syntax"',
  "syntax atlas package manifest authority",
);

const expected = buildAuthoritySnapshot(corpus, sha256Bytes(new TextEncoder().encode(corpusText)));
if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, `${JSON.stringify(expected)}\n`, "utf8");
  console.log(`wrote ${path.relative(path.resolve(tooling, ".."), snapshotPath)}`);
  process.exit(0);
}
const actual = JSON.parse(fs.readFileSync(snapshotPath, "utf8").trim());
if (JSON.stringify(actual) !== JSON.stringify(expected)) {
  console.error("authority registry snapshot is stale");
  process.exit(1);
}
console.log(`authority registry: ${expected.metrics.caseCount} cases, ${expected.metrics.acceptedCount} accepted, ${expected.metrics.rejectedCount} rejected`);
