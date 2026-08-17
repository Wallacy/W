import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  digestFile,
  validateWloCorpus,
} from "./wlo1-closure-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "wlo1-closure-cases.json");
const machinePath = path.join(toolingDirectory, "wlo1-closure-machine.mjs");
const snapshotPath = path.join(toolingDirectory, "wlo1-closure-results.snapshot.jsonl");
const manifestPath = path.join(toolingDirectory, "studies", "r1c0-closure", "wlo1-manifest.json");
const checkerPath = fileURLToPath(import.meta.url);
const writeSnapshot = process.argv.includes("--write");
const errors = [];
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function validateArtifactDigestReceipt(value, name, actualDigest) {
  const receipt = value?.artifacts?.[name];
  if (!receipt || receipt.digest !== actualDigest) return ["staleArtifactDigest"];
  return [];
}

function deriveWloMetrics(results, profileId) {
  return {
    caseCount: results.length,
    acceptedCount: results.filter((result) => result.status === "accepted").length,
    rejectedCount: results.filter((result) => result.status === "rejected").length,
    primaryRoundtripCount: results.filter((result) => result.kind === "roundtrip" && result.status === "accepted").length,
    targetParityCount: results.filter((result) => result.kind === "target-parity" && result.logicalResultEqual).length,
    profileId,
    typedErrorCounts: Object.fromEntries(
      [...new Set(results.map((result) => result.error).filter(Boolean))]
        .sort()
        .map((error) => [error, results.filter((result) => result.error === error).length]),
    ),
  };
}

function validateWloMetrics(candidate, results, profileId) {
  const expected = deriveWloMetrics(results, profileId);
  const findings = [];
  for (const field of ["caseCount", "acceptedCount", "rejectedCount", "primaryRoundtripCount", "targetParityCount", "profileId"]) {
    if (candidate?.[field] !== expected[field]) findings.push("forgedMetric");
  }
  if (JSON.stringify(candidate?.typedErrorCounts) !== JSON.stringify(expected.typedErrorCounts)) findings.push("forgedMetric");
  return [...new Set(findings)];
}

function validateWloAuthority(value) {
  const findings = [];
  if (value?.codec?.payloadAuthority !== "logical-value-bytes-only") findings.push("payloadAuthorityDrift");
  if (value?.codec?.header !== "none") findings.push("headerDrift");
  if (value?.codec?.wAbiImpact !== "none" || value?.codec?.targetWAbiIndependent !== true) findings.push("codecWAbiAuthority");
  const targetKeys = ["id", "targetId", "bytes", "layoutReceipt", "interopSchemaReceipt", "wAbiImpact", "targetWAbiIndependent", "packageReceipt"];
  for (const target of value?.targetProjections ?? []) {
    if (JSON.stringify(Object.keys(target).sort()) !== JSON.stringify([...targetKeys].sort())) findings.push("unknownTargetReceiptField");
    if (target.wAbiImpact !== "none" || target.targetWAbiIndependent !== true) findings.push("targetWAbiAuthority");
    if (target.interopSchemaReceipt !== "WLO-String-v1-data-only") findings.push("targetInteropSchemaDrift");
  }
  return [...new Set(findings)];
}

function requireFile(relativePath, digest, label) {
  const resolved = path.resolve(repositoryRoot, relativePath);
  const relativeToRoot = path.relative(repositoryRoot, resolved);
  if (path.isAbsolute(relativeToRoot) || relativeToRoot === ".." || relativeToRoot.startsWith(`..${path.sep}`)) {
    errors.push(`${label} escapes repository root: ${relativePath}`);
    return;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${label} references a missing file: ${relativePath}`);
    return;
  }
  const actual = digestFile(resolved);
  if (actual !== digest) errors.push(`${label} digest is stale; expected ${actual}.`);
}

if (manifest.$schema !== "w-wlo1-closure-manifest-1") errors.push("WLO1 manifest schema mismatch.");
if (manifest.status !== "design-oracle-input") errors.push("WLO1 manifest must remain design-oracle-input.");
if (manifest.id !== "WLO1") errors.push("WLO1 manifest id mismatch.");
const manifestKeys = ["$schema", "status", "id", "codec", "source", "logicalValue", "limits", "targetProjections", "artifacts", "officialRefs", "forbidden", "evidence", "stopCondition"];
if (JSON.stringify(Object.keys(manifest).sort()) !== JSON.stringify([...manifestKeys].sort())) errors.push("WLO1 manifest has an unexpected authority field.");
const codecKeys = ["profileFamily", "encoding", "rfc", "schemaId", "version", "payloadAuthority", "automaticUniversalEncoding", "wAbiImpact", "targetWAbiIndependent", "header"];
if (JSON.stringify(Object.keys(manifest.codec ?? {}).sort()) !== JSON.stringify([...codecKeys].sort())) errors.push("WLO1 codec receipt has an unexpected field.");
if (manifest.codec?.header !== "none") errors.push("WLO1 cannot add a payload header.");
if (manifest.codec?.profileFamily !== "schema-profiled-data-codec" || manifest.codec?.automaticUniversalEncoding !== false) errors.push("WLO1 must be explicitly profiled, not universal.");
if (manifest.codec?.wAbiImpact !== "none" || manifest.codec?.targetWAbiIndependent !== true) errors.push("WLO1 cannot become W ABI authority.");
if (manifest.codec?.encoding !== "deterministic-cbor" || manifest.codec?.rfc !== "8949") {
  errors.push("WLO1 manifest must pin deterministic CBOR RFC 8949.");
}
if (manifest.logicalValue?.canonicalBytes !== corpus.codec?.canonicalBytes) {
  errors.push("WLO1 manifest and corpus canonical bytes differ.");
}
if (JSON.stringify(Object.keys(manifest.source ?? {}).sort()) !== JSON.stringify(["digest", "path", "symbol"].sort())) errors.push("WLO1 source receipt has an unexpected field.");
if (JSON.stringify(Object.keys(manifest.logicalValue ?? {}).sort()) !== JSON.stringify(["canonicalBytes", "inputUtf8", "resultUtf8", "type"].sort())) errors.push("WLO1 logical receipt has an unexpected field.");
if (manifest.logicalValue?.inputUtf8 !== manifest.logicalValue?.resultUtf8) {
  errors.push("WLO1 logical input/result must be identical.");
}
const targetKeys = ["id", "targetId", "bytes", "layoutReceipt", "interopSchemaReceipt", "wAbiImpact", "targetWAbiIndependent", "packageReceipt"];
if (manifest.targetProjections?.length !== 2) errors.push("WLO1 manifest needs two target projections.");
else {
  const targetIds = manifest.targetProjections.map((target) => target.id);
  if (JSON.stringify([...targetIds].sort()) !== JSON.stringify(["native", "portable"])) errors.push("WLO1 target IDs drifted.");
  const bytes = manifest.targetProjections.map((target) => target.bytes);
  if (new Set(bytes).size !== 1) errors.push("WLO1 target projections must have byte-identical payloads.");
for (const target of manifest.targetProjections) {
    if (JSON.stringify(Object.keys(target).sort()) !== JSON.stringify([...targetKeys].sort())) errors.push(`WLO1 target ${target.id} has an unexpected field.`);
    if (target.layoutReceipt === "logical-bytes-authority") errors.push("WLO1 layout receipt cannot be payload authority.");
    if (target.interopSchemaReceipt !== "WLO-String-v1-data-only" || target.wAbiImpact !== "none" || target.targetWAbiIndependent !== true) errors.push(`WLO1 target ${target.id} receipt drifted.`);
  }
}
for (const finding of validateWloAuthority(manifest)) errors.push(`WLO1 authority validation failed: ${finding}.`);
if (manifest.source?.path !== corpus.source?.path || manifest.source?.symbol !== corpus.source?.symbol) {
  errors.push("WLO1 source refs differ between manifest and corpus.");
}
if (JSON.stringify(manifest.officialRefs ?? []) !== JSON.stringify([{ url: "https://www.rfc-editor.org/rfc/rfc8949", role: "deterministic-cbor" }])) errors.push("WLO1 official RFC receipt drifted.");
const sourceFile = path.resolve(repositoryRoot, manifest.source.path);
const sourceRelative = path.relative(repositoryRoot, sourceFile);
if (path.isAbsolute(sourceRelative) || sourceRelative === ".." || sourceRelative.startsWith(`..${path.sep}`)) errors.push("WLO1 source path escapes repository root.");
if (!fs.existsSync(sourceFile) || !fs.readFileSync(sourceFile, "utf8").includes(manifest.source.symbol)) {
  errors.push("WLO1 source symbol is missing.");
} else if (digestFile(sourceFile) !== manifest.source.digest) {
  errors.push(`WLO1 source digest is stale; expected ${digestFile(sourceFile)}.`);
}
const artifactKeys = ["path", "digest"];
if (JSON.stringify(Object.keys(manifest.artifacts ?? {}).sort()) !== JSON.stringify(["checker", "corpus", "machine", "snapshot"].sort())) {
  errors.push("WLO1 artifact set has an unexpected authority field.");
}
for (const [name, artifact] of Object.entries(manifest.artifacts ?? {})) {
  if (JSON.stringify(Object.keys(artifact).sort()) !== JSON.stringify([...artifactKeys].sort())) errors.push(`WLO1 artifact ${name} has an unexpected field.`);
  requireFile(artifact.path, artifact.digest, `WLO1 ${name}`);
}

const validation = validateWloCorpus(corpus);
errors.push(...validation.errors);
const expectedErrors = new Map(
  corpus.cases
    .filter((testCase) => testCase.expectedError)
    .map((testCase) => [testCase.id, testCase.expectedError]),
);
const requiredTypedErrors = new Set(["invalidMajorType", "versionMismatch", "schemaMismatch", "duplicateField", "unknownField", "invalidUtf8", "nonCanonicalLength", "trailingBytes", "payloadLimit", "forbiddenRepresentation", "editorTreeNotAuthority"]);
for (const result of validation.results) {
  if (expectedErrors.has(result.caseId) && result.error !== expectedErrors.get(result.caseId)) {
    errors.push(`${result.caseId} expected ${expectedErrors.get(result.caseId)} but derived ${result.error}.`);
  }
  if (!expectedErrors.has(result.caseId) && result.status !== "accepted") {
    errors.push(`${result.caseId} is an unplanned rejection: ${result.error}.`);
  }
}
for (const error of requiredTypedErrors) if (!validation.results.some((result) => result.error === error)) errors.push(`WLO1 typed error ${error} is missing.`);
if (validation.results.filter((result) => result.status === "accepted").length !== 3) {
  errors.push("WLO1 must retain exactly three accepted host-oracle results.");
}
if (validation.results.filter((result) => result.status === "rejected").length !== 11) {
  errors.push("WLO1 must retain exactly eleven typed negative results.");
}

const metrics = {
  ...deriveWloMetrics(validation.results, corpus.codec.schemaId),
  claimsCompiler: false,
  claimsRuntime: false,
  claimsProvider: false,
  claimsHumanOrModel: false,
};
for (const finding of validateWloMetrics(metrics, validation.results, corpus.codec.schemaId)) errors.push(`WLO1 metric validation failed: ${finding}.`);
const wloMutationManifest = clone(manifest);
wloMutationManifest.artifacts.corpus.digest = "sha256:deadbeef";
const forgedWloMetrics = { ...metrics, caseCount: metrics.caseCount + 1 };
const wloAuthorityMutation = clone(manifest);
wloAuthorityMutation.codec.header = "wlo1-header";
wloAuthorityMutation.targetProjections[0].wAbiImpact = "w-abi";
const wloTargetFieldMutation = clone(manifest);
wloTargetFieldMutation.targetProjections[0].unexpected = true;
const output = {
  schema: "w-wlo1-closure-results-1",
  status: "design-oracle-output",
  evidence: {
    kind: "host-design-oracle",
    claimsCompiler: false,
    claimsRuntime: false,
    claimsProvider: false,
    claimsHumanOrModel: false,
  },
  corpus: "tooling/wlo1-closure-cases.json",
  corpusDigest: digestFile(corpusPath),
  manifest: "tooling/studies/r1c0-closure/wlo1-manifest.json",
  metrics,
  results: validation.results,
  mutations: {
    staleArtifactDigestDetected: validateArtifactDigestReceipt(wloMutationManifest, "corpus", digestFile(corpusPath)).includes("staleArtifactDigest"),
    forgedMetricDetected: validateWloMetrics(forgedWloMetrics, validation.results, corpus.codec.schemaId).includes("forgedMetric"),
    wrongTargetAuthorityDetected: validateWloAuthority(wloAuthorityMutation).some((finding) => ["headerDrift", "targetWAbiAuthority"].includes(finding)),
    unknownTargetReceiptDetected: validateWloAuthority(wloTargetFieldMutation).includes("unknownTargetReceiptField"),
    nonCanonicalLengthRejected: validation.results.some((result) => result.error === "nonCanonicalLength"),
  },
};
const snapshot = `${JSON.stringify(output)}\n`;
if (writeSnapshot) {
  fs.writeFileSync(snapshotPath, snapshot, "utf8");
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  errors.push("wlo1-closure-results.snapshot.jsonl is stale; run with --write.");
}
if (fs.existsSync(snapshotPath) && manifest.artifacts?.snapshot?.digest !== digestFile(snapshotPath)) {
  if (!writeSnapshot) errors.push(`WLO1 snapshot digest is stale; expected ${digestFile(snapshotPath)}.`);
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}
process.stdout.write(
  `WLO1 closure: ${metrics.caseCount} cases, ${metrics.acceptedCount} accepted, ` +
    `${metrics.rejectedCount} typed negatives, ${metrics.targetParityCount} target-parity case.\n`,
);
