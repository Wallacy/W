import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const ROOT = path.resolve(import.meta.dirname, "..");
const LAST_LIGHT = path.join(ROOT, "reference", "last-light");
const PACKAGE = path.join(LAST_LIGHT, "package.w");
const WORKSPACE = path.join(LAST_LIGHT, "workspace.w");
const ZERO = "sha256:" + "0".repeat(64);
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

function digest(value) {
  return `sha256:${crypto.createHash("sha256").update(value, "utf8").digest("hex")}`;
}

const packageText = text(PACKAGE);
const workspaceText = text(WORKSPACE);
const horizon = text(path.join(LAST_LIGHT, "horizon_tool.w"));

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

const match = /^\s+workspaceDigest: "(sha256:[0-9a-f]{64})"$/mu.exec(workspaceText);
if (!match || !DIGEST.test(match[1])) fail("workspaceDigest is missing or malformed");
function resolutionBasis(value) {
  const start = value.indexOf("  resolution: {");
  const end = value.indexOf("  deployments: [", start);
  if (start < 0 || end < 0) fail("resolution/deployments boundary is missing");
  const resolution = value.slice(start, end).replace(/workspaceDigest: "sha256:[0-9a-f]{64}"/u, `workspaceDigest: "${ZERO}"`);
  return value.slice(0, start) + resolution;
}
const canonical = resolutionBasis(workspaceText);
const expected = digest(canonical);
if (match[1] !== expected) fail(`workspaceDigest is stale: expected ${expected}, found ${match[1]}`);
const deploymentMutation = workspaceText.replace('name: "local"', 'name: "local-mutated"');
if (resolutionBasis(deploymentMutation) !== canonical) fail("deployment-only edits changed resolution identity");
const dependencyMutation = workspaceText.replace('alias: "chart"', 'alias: "chart-mutated"');
if (resolutionBasis(dependencyMutation) === canonical) fail("member/dependency edits did not change resolution identity");

console.log(`root-unification: ok (${expected})`);
