import fs from "node:fs";
import path from "node:path";
import {
  deriveOwnerDigest,
  parseManifestDocument,
  parseBuildManifest,
} from "./w-manifest-data.mjs";

const ROOT = path.resolve(import.meta.dirname, "..");
const LAST_LIGHT = path.join(ROOT, "reference", "last-light");
const BUILD = path.join(LAST_LIGHT, "build.w");
const DIGEST = /^sha256:[0-9a-f]{64}$/u;

function fail(message) {
  throw new Error(`root-unification: ${message}`);
}

function text(file) {
  if (!fs.existsSync(file)) fail(`missing ${path.relative(ROOT, file)}`);
  const value = fs.readFileSync(file, "utf8");
  if (value.includes("\r")) fail(`${path.relative(ROOT, file)} must use LF`);
  return value;
}

function expectBuildOrder(label, source, expectedKinds) {
  let document;
  try {
    document = parseBuildManifest(source);
  } catch (error) {
    fail(`${label} unexpectedly rejected: ${error.message}`);
  }
  const actualKinds = document.records.map((record) => record.kind);
  if (document.kind !== "build_manifest" || JSON.stringify(actualKinds) !== JSON.stringify(expectedKinds)) {
    fail(`${label} returned ${JSON.stringify(actualKinds)} instead of ${JSON.stringify(expectedKinds)}`);
  }
}

function expectBuildError(label, source, expectedCode) {
  let caught;
  try {
    parseBuildManifest(source);
  } catch (error) {
    caught = error;
  }
  if (!caught || caught.code !== expectedCode) {
    fail(`${label} returned ${caught?.code ?? "accepted"} instead of ${expectedCode}`);
  }
}

expectBuildOrder("package-only parser shape", 'package { schema: "w.package/1" }', ["package"]);
expectBuildOrder("workspace-only parser shape", 'workspace { schema: "w.workspace/1" }', ["workspace"]);
expectBuildOrder(
  "package/workspace parser order",
  'package { schema: "w.package/1" } workspace { schema: "w.workspace/1" }',
  ["package", "workspace"],
);
expectBuildOrder(
  "workspace/package parser order",
  'workspace { schema: "w.workspace/1" } package { schema: "w.package/1" }',
  ["workspace", "package"],
);
expectBuildError("empty build parser shape", "", "manifestRootMissing");
expectBuildError(
  "duplicate package parser shape",
  'package { schema: "w.package/1" } package { schema: "w.package/1" }',
  "manifestDuplicateRoot",
);
expectBuildError(
  "duplicate workspace parser shape",
  'workspace { schema: "w.workspace/1" } workspace { schema: "w.workspace/1" }',
  "manifestDuplicateRoot",
);
expectBuildError(
  "third root parser shape",
  'package { schema: "w.package/1" } workspace { schema: "w.workspace/1" } deployment { schema: "w.deployment/1" }',
  "manifestRootInvalid",
);
// Empty source is valid module syntax in Tree-sitter; the build.w filename/root
// checker applies the data-only contract and rejects an empty physical build.
let legacyDocumentError;
try {
  parseManifestDocument('package { schema: "w.package/1" } workspace { schema: "w.workspace/1" }');
} catch (error) {
  legacyDocumentError = error;
}
if (legacyDocumentError?.code !== "manifestTrailingInput") {
  fail(`parseManifestDocument accepted multiple roots or returned ${legacyDocumentError?.code ?? "no error"}`);
}

const buildText = text(BUILD);
for (const obsolete of ["package.w", "workspace.w"]) {
  if (fs.existsSync(path.join(LAST_LIGHT, obsolete))) fail(`obsolete ${obsolete} remains`);
}
const horizon = text(path.join(LAST_LIGHT, "horizon_tool.w"));
const buildDocument = parseBuildManifest(buildText);
const packageDocument = buildDocument.package;
const workspaceDocument = buildDocument.workspace;

if (buildDocument.kind !== "build_manifest" || buildDocument.records.length !== 2 ||
    packageDocument?.kind !== "package" || workspaceDocument?.kind !== "workspace") fail("build manifest parser returned the wrong root shape");
if (Object.hasOwn(packageDocument, "resolution") || Object.hasOwn(packageDocument, "deployments")) {
  fail("combined build.w package must omit resolution and deployments; workspace is the sole owner");
}

if (fs.existsSync(path.join(LAST_LIGHT, "package.lock"))) fail("obsolete package.lock remains");
if (fs.existsSync(path.join(LAST_LIGHT, "deployments"))) fail("obsolete deployments directory remains");
if (buildText.includes('.path("deployments/')) fail("package publication still includes deployment paths");
if (!buildText.includes('alias: "chart"') || !buildText.includes('package: "fiction/chart"')) fail("build.w does not declare chart dependency");
if (!horizon.includes("module horizon_tool") || !horizon.includes("entry(runHorizon)")) fail("horizon_tool.w is not an explicit-entry module");
if (/^script\s*\{/mu.test(horizon) || /^let\s+\w+/mu.test(horizon)) fail("horizon_tool.w still uses a header or top-level execution");

if (!buildText.includes('schema: "w.resolution/1"')) fail("workspace resolution schema is missing");
if (!buildText.includes('schema: "w.deployment/1"')) fail("workspace deployment schema is missing");
const deploymentNames = [...buildText.matchAll(/^      name: "(local|distributed|benchmark)"$/gmu)].map((match) => match[1]);
if (JSON.stringify(deploymentNames) !== JSON.stringify(["local", "distributed", "benchmark"])) fail(`deployment names are not closed: ${deploymentNames.join(", ")}`);
const rootEdges = [...buildText.matchAll(/alias: "([^"]+)"\s*,?\s*id: "(sha256:[0-9a-f]{64})"/gu)].map((match) => `${match[1]}=${match[2]}`);
const nodeIds = {
  restaurant: "sha256:22d0414c0b18f89bb91f3e2ea5b5368b557664f8f910a41102a5e7f4f28f3c67",
  chart: "sha256:3b51417f058a4a66d6166525d9fd588c97e79649db57d599fb7e559be24f8a44",
  menuCompiler: "sha256:3e896724d6f6f896039de431e424309492cd2b2b30b47c0cf33e1f0f1b064de0",
};
for (const [alias, id] of [["chart", nodeIds.chart], ["menuCompiler", nodeIds.menuCompiler]]) {
  if (!rootEdges.includes(`${alias}=${id}`)) fail(`missing content-derived resolution root edge ${alias}`);
}
for (const id of Object.values(nodeIds)) if (!buildText.includes(`id: "${id}"`) && !buildText.includes(`"${id}"`)) fail(`resolution node ${id} is missing`);
for (const digestValue of buildText.match(/sha256:[0-9a-f]{64}/gu) ?? []) if (!DIGEST.test(digestValue)) fail(`malformed digest ${digestValue}`);
for (const name of ["last-light/restaurant", "fiction/chart", "last-light/menu-compiler"]) if (!buildText.includes(`name: "${name}"`)) fail(`missing resolution package ${name}`);

const memberBuildPackages = new Map();
for (const member of workspaceDocument.members ?? []) {
  if (typeof member !== "string" || member.startsWith("/") || member.includes("..")) fail(`invalid workspace member ${member}`);
  const memberBuild = path.join(LAST_LIGHT, member, "build.w");
  const memberDocument = parseBuildManifest(text(memberBuild));
  if (!memberDocument.package || (member !== "." && memberDocument.workspace)) fail(`workspace member lacks package build without nested workspace: ${member}`);
  if (member !== "." && (Object.hasOwn(memberDocument.package, "resolution") || Object.hasOwn(memberDocument.package, "deployments"))) {
    fail(`workspace member package must omit resolution and deployments: ${member}`);
  }
  memberBuildPackages.set(member, memberDocument.package.name);
}
if (memberBuildPackages.size !== (workspaceDocument.members ?? []).length) fail("workspace members contain duplicate paths");

const declaredOwnerDigest = workspaceDocument.resolution?.ownerDigest;
if (!DIGEST.test(declaredOwnerDigest ?? "")) fail("ownerDigest is missing or malformed");
const expected = deriveOwnerDigest(workspaceDocument);
if (declaredOwnerDigest !== expected) fail(`ownerDigest is stale: expected ${expected}, found ${declaredOwnerDigest}`);
const resolutionMutation = buildText.replace('resolver: "w.resolver/1"', 'resolver: "w.resolver/2"');
if (deriveOwnerDigest(parseBuildManifest(resolutionMutation).workspace) !== expected) fail("resolution-only edits changed owner identity");
const deploymentMutation = buildText.replace('name: "local"', 'name: "local-mutated"');
if (deriveOwnerDigest(parseBuildManifest(deploymentMutation).workspace) !== expected) fail("deployment-only edits changed owner identity");
const dependencyMutation = buildText.replace('"packages/menu-compiler"', '"packages/menu-compiler-mutated"');
if (deriveOwnerDigest(parseBuildManifest(dependencyMutation).workspace) === expected) fail("member/dependency edits did not change owner identity");
const commentMutation = buildText.replace("// Data-only workspace for the Last Light reference product.\n", "// moved comment\n\n");
if (deriveOwnerDigest(parseBuildManifest(commentMutation).workspace) !== expected) fail("comments changed owner identity");
const nestedOrderMutation = buildText.replace(
  'name: "linux-x64"\n        target: "x86_64-unknown-linux-gnu"\n        sandbox: "w.build-sandbox/1"',
  'sandbox: "w.build-sandbox/1"\n        name: "linux-x64"\n        target: "x86_64-unknown-linux-gnu"',
);
if (nestedOrderMutation === buildText) fail("nested order mutation did not apply");
if (deriveOwnerDigest(parseBuildManifest(nestedOrderMutation).workspace) !== expected) fail("nested field order changed owner identity");

console.log(`root-unification: ok (${expected})`);
