import fs from "node:fs";
import path from "node:path";
import {
  deriveOwnerDigest,
  parseManifestDocument,
} from "./w-manifest-data.mjs";

const ROOT = path.resolve(import.meta.dirname, "..");
const LAST_LIGHT = path.join(ROOT, "reference", "last-light");
const PACKAGE = path.join(LAST_LIGHT, "package.w");
const WORKSPACE = path.join(LAST_LIGHT, "workspace.w");
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

const packageText = text(PACKAGE);
const workspaceText = text(WORKSPACE);
const horizon = text(path.join(LAST_LIGHT, "horizon_tool.w"));
const packageDocument = parseManifestDocument(packageText);
const workspaceDocument = parseManifestDocument(workspaceText);

if (packageDocument.kind !== "package" || workspaceDocument.kind !== "workspace") fail("manifest parser returned the wrong root kind");

if (fs.existsSync(path.join(LAST_LIGHT, "package.lock"))) fail("obsolete package.lock remains");
if (fs.existsSync(path.join(LAST_LIGHT, "deployments"))) fail("obsolete deployments directory remains");
if (packageText.includes('.path("deployments/')) fail("package publication still includes deployment paths");
if (!packageText.includes('alias: "chart"') || !packageText.includes('package: "fiction/chart"')) fail("package.w does not declare chart dependency");
if (!horizon.includes("module horizon_tool") || !horizon.includes("entry(runHorizon)")) fail("horizon_tool.w is not an explicit-entry module");
if (/^script\s*\{/mu.test(horizon) || /^let\s+\w+/mu.test(horizon)) fail("horizon_tool.w still uses a header or top-level execution");

if (!workspaceText.includes('schema: "w.resolution/1"')) fail("workspace resolution schema is missing");
if (!workspaceText.includes('schema: "w.deployment/1"')) fail("workspace deployment schema is missing");
const deploymentNames = [...workspaceText.matchAll(/^      name: "(local|distributed|benchmark)"$/gmu)].map((match) => match[1]);
if (JSON.stringify(deploymentNames) !== JSON.stringify(["local", "distributed", "benchmark"])) fail(`deployment names are not closed: ${deploymentNames.join(", ")}`);
const rootEdges = [...workspaceText.matchAll(/alias: "([^"]+)"\s*,?\s*id: "(sha256:[0-9a-f]{64})"/gu)].map((match) => `${match[1]}=${match[2]}`);
const nodeIds = {
  restaurant: "sha256:22d0414c0b18f89bb91f3e2ea5b5368b557664f8f910a41102a5e7f4f28f3c67",
  chart: "sha256:3b51417f058a4a66d6166525d9fd588c97e79649db57d599fb7e559be24f8a44",
  menuCompiler: "sha256:3e896724d6f6f896039de431e424309492cd2b2b30b47c0cf33e1f0f1b064de0",
};
for (const [alias, id] of [["chart", nodeIds.chart], ["menuCompiler", nodeIds.menuCompiler]]) {
  if (!rootEdges.includes(`${alias}=${id}`)) fail(`missing content-derived resolution root edge ${alias}`);
}
for (const id of Object.values(nodeIds)) if (!workspaceText.includes(`id: "${id}"`) && !workspaceText.includes(`"${id}"`)) fail(`resolution node ${id} is missing`);
for (const digestValue of workspaceText.match(/sha256:[0-9a-f]{64}/gu) ?? []) if (!DIGEST.test(digestValue)) fail(`malformed digest ${digestValue}`);
for (const name of ["last-light/restaurant", "fiction/chart", "last-light/menu-compiler"]) if (!workspaceText.includes(`name: "${name}"`)) fail(`missing resolution package ${name}`);

const declaredOwnerDigest = workspaceDocument.resolution?.ownerDigest;
if (!DIGEST.test(declaredOwnerDigest ?? "")) fail("ownerDigest is missing or malformed");
const expected = deriveOwnerDigest(workspaceDocument);
if (declaredOwnerDigest !== expected) fail(`ownerDigest is stale: expected ${expected}, found ${declaredOwnerDigest}`);
const resolutionMutation = workspaceText.replace('resolver: "w.resolver/1"', 'resolver: "w.resolver/2"');
if (deriveOwnerDigest(parseManifestDocument(resolutionMutation)) !== expected) fail("resolution-only edits changed owner identity");
const deploymentMutation = workspaceText.replace('name: "local"', 'name: "local-mutated"');
if (deriveOwnerDigest(parseManifestDocument(deploymentMutation)) !== expected) fail("deployment-only edits changed owner identity");
const dependencyMutation = workspaceText.replace('"packages/menu-compiler"', '"packages/menu-compiler-mutated"');
if (deriveOwnerDigest(parseManifestDocument(dependencyMutation)) === expected) fail("member/dependency edits did not change owner identity");
const commentMutation = workspaceText.replace("// Data-only workspace for the Last Light reference product.\n", "// moved comment\n\n");
if (deriveOwnerDigest(parseManifestDocument(commentMutation)) !== expected) fail("comments changed owner identity");
const nestedOrderMutation = workspaceText.replace(
  'name: "linux-x64"\n        target: "x86_64-unknown-linux-gnu"\n        sandbox: "w.build-sandbox/1"',
  'sandbox: "w.build-sandbox/1"\n        name: "linux-x64"\n        target: "x86_64-unknown-linux-gnu"',
);
if (nestedOrderMutation === workspaceText) fail("nested order mutation did not apply");
if (deriveOwnerDigest(parseManifestDocument(nestedOrderMutation)) !== expected) fail("nested field order changed owner identity");

console.log(`root-unification: ok (${expected})`);
