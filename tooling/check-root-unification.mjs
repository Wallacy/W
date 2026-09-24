import fs from "node:fs";
import path from "node:path";
import {
  deriveBuildPlanDigest,
  derivePackageDigest,
  derivePackageSetDigest,
  derivePublicationPackageDigest,
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

expectBuildOrder("single-package root", 'package { schema: "w.package/1" root: "." }', ["package"]);
expectBuildOrder(
  "package records followed by coordinator",
  'package { schema: "w.package/1" root: "." } package { schema: "w.package/1" root: "tools/x" } build { schema: "w.build/1" }',
  ["package", "package", "build"],
);
expectBuildOrder(
  "coordinator may precede packages",
  'build { schema: "w.build/1" } package { schema: "w.package/1" root: "." } package { schema: "w.package/1" root: "tools/x" }',
  ["build", "package", "package"],
);
expectBuildError("empty build parser shape", "", "manifestRootMissing");
expectBuildError("workspace root rejected", 'workspace { schema: "w.workspace/1" }', "manifestRootInvalid");
expectBuildError("direct root kind cannot be overridden by a field", 'package { schema: "w.package/1" root: "." kind: "build" }', "manifestRootKindFieldInvalid");
expectBuildError("multiple packages need coordinator", 'package { schema: "w.package/1" root: "." } package { schema: "w.package/1" root: "tools/x" }', "manifestBuildCoordinatorRequired");
expectBuildError("duplicate coordinator", 'package { schema: "w.package/1" root: "." } build { schema: "w.build/1" } build { schema: "w.build/1" }', "manifestDuplicateRoot");
expectBuildError("build-only document has no package", 'build { schema: "w.build/1" }', "manifestRootMissing");
expectBuildError("coordinator cannot include another build file", 'package { schema: "w.package/1" root: "." } build { schema: "w.build/1" include: .path("build.w") }', "manifestBuildFieldUnknown");
expectBuildError("package root cannot name a nested build file", 'package { schema: "w.package/1" root: "packages/tool/build.w" }', "manifestPackageRootInvalid");
expectBuildError("duplicate exact package root", 'package { schema: "w.package/1" root: "." } package { schema: "w.package/1" root: "." } build { schema: "w.build/1" }', "manifestDuplicatePackageRoot");
for (const root of ["../escape", "a/../escape", "*", "packages/*", "/absolute", "C:/absolute", "", "a//b"]) {
  expectBuildError(`unsafe root ${JSON.stringify(root)}`, `package { schema: "w.package/1" root: "${root}" }`, "manifestPackageRootInvalid");
}

// The data-only build.w filename/root checker rejects extra roots; the one-record
// parser remains useful for decoding a package record inside focused fixtures.
let legacyDocumentError;
try {
  parseManifestDocument('package { schema: "w.package/1" root: "." } build { schema: "w.build/1" }');
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
const coordinator = buildDocument.build;
const packages = buildDocument.packages;

if (buildDocument.kind !== "build_manifest" || packages.length !== 2 || coordinator?.kind !== "build") {
  fail("Last Light build manifest must have two direct package records and one build coordinator");
}
if (packages.map((record) => record.name).sort().join(",") !== "last-light/menu-compiler,last-light/restaurant") {
  fail("Last Light direct package set is incomplete");
}
if (Object.hasOwn(packages[0], "resolution") || Object.hasOwn(packages[0], "deployments")) {
  fail("local resolution and deployment facts must live only in the build coordinator");
}
if (fs.existsSync(path.join(LAST_LIGHT, "packages", "menu-compiler", "build.w"))) fail("nested build root remains");
if (fs.existsSync(path.join(LAST_LIGHT, "package.lock"))) fail("obsolete package.lock remains");
if (fs.existsSync(path.join(LAST_LIGHT, "deployments"))) fail("obsolete deployments directory remains");
if (buildText.includes('.path("deployments/')) fail("package publication still includes deployment paths");
if (!buildText.includes('alias: "chart"') || !buildText.includes('package: "fiction/chart"')) fail("build.w does not declare chart dependency");
if (!horizon.includes("module horizon_tool") || !horizon.includes("entry(runHorizon)")) fail("horizon_tool.w is not an explicit-entry module");
if (/^script\s*\{/mu.test(horizon) || /^let\s+\w+/mu.test(horizon)) fail("horizon_tool.w still uses a header or top-level execution");
if (coordinator.default?.package !== "last-light/restaurant" || coordinator.default?.product !== "last-light-native") {
  fail("build coordinator must carry the exact package/product default selector");
}
if (coordinator.resolution?.schema !== "w.resolution/1") fail("local resolution schema is missing");
if (!Array.isArray(coordinator.deployments) || coordinator.deployments.length !== 3) fail("local deployment plans are missing");
const deploymentNames = coordinator.deployments.map(({ name }) => name);
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

for (const packageRecord of packages) {
  const absoluteRoot = path.resolve(LAST_LIGHT, packageRecord.root);
  if (!absoluteRoot.startsWith(`${LAST_LIGHT}${path.sep}`) && absoluteRoot !== LAST_LIGHT) fail(`package root escapes Last Light: ${packageRecord.root}`);
  if (!fs.statSync(absoluteRoot).isDirectory()) fail(`package root is not a directory: ${packageRecord.root}`);
  if (!Object.hasOwn(packageRecord, "build")) fail(`package-authored build requirements/profiles/recipes missing for ${packageRecord.name}`);
}

const expectedPackageSetDigest = derivePackageSetDigest(packages);
const declaredPackageSetDigest = coordinator.resolution.packageSetDigest;
if (!DIGEST.test(declaredPackageSetDigest ?? "")) fail("packageSetDigest is missing or malformed");
if (declaredPackageSetDigest !== expectedPackageSetDigest) fail(`packageSetDigest is stale: expected ${expectedPackageSetDigest}, found ${declaredPackageSetDigest}`);
const packageDigestByName = Object.fromEntries(packages.map((record) => [record.name, derivePackageDigest(record)]));
const relocatedPackage = { ...packages[0], root: "relocated/exact-root" };
if (derivePackageDigest(relocatedPackage) !== derivePackageDigest(packages[0])) fail("local root path entered public package identity");
if (derivePackageSetDigest([...packages].reverse()) !== expectedPackageSetDigest) fail("package record order changed package-set identity");
const menuCompilerStart = buildText.indexOf('name: "last-light/menu-compiler"');
const authoredBuildMutation = menuCompilerStart < 0 ? buildText :
  buildText.slice(0, menuCompilerStart) + buildText.slice(menuCompilerStart).replace("network: .deny", "network: .allow");
if (authoredBuildMutation === buildText) fail("package-authored build requirement mutation did not apply");
const authoredPackages = parseBuildManifest(authoredBuildMutation).packages;
if (derivePackageDigest(authoredPackages.find(({ name }) => name === "last-light/menu-compiler")) === packageDigestByName["last-light/menu-compiler"]) {
  fail("package-authored build requirements were excluded from public package identity");
}

const localPatchCoordinator = { ...coordinator, patches: [{ package: "last-light/restaurant", replace: "local-source" }] };
if (derivePackageSetDigest(packages) !== expectedPackageSetDigest) fail("local patches changed public package-set identity");
let publicationPatchError;
try {
  derivePublicationPackageDigest(packages.find(({ name }) => name === "last-light/restaurant"), localPatchCoordinator);
} catch (error) {
  publicationPatchError = error;
}
if (publicationPatchError?.code !== "manifestLocalPatchUnpublishable") fail("publication accepted a package with a local patch");

const originalPlanDigest = deriveBuildPlanDigest(buildDocument);
const reorderedBuildDocument = { ...buildDocument, packages: [...packages].reverse() };
if (deriveBuildPlanDigest(reorderedBuildDocument) !== originalPlanDigest) fail("package record order changed build-plan identity");
const relocatedBuildDocument = structuredClone(buildDocument);
relocatedBuildDocument.packages[0].root = "relocated/exact-root";
if (derivePackageSetDigest(relocatedBuildDocument.packages) !== expectedPackageSetDigest) fail("local root changed public package-set identity");
if (deriveBuildPlanDigest(relocatedBuildDocument) === originalPlanDigest) fail("local root binding did not change build-plan identity");
const authoredBuildDocument = parseBuildManifest(authoredBuildMutation);
if (deriveBuildPlanDigest(authoredBuildDocument) === originalPlanDigest) fail("package-authored build requirements did not change build-plan identity");
const selectedProductMutation = buildText.replace('product: "last-light-native"', 'product: "other-product"');
if (derivePackageSetDigest(parseBuildManifest(selectedProductMutation).packages) !== expectedPackageSetDigest) fail("local default selection changed package identity");
if (deriveBuildPlanDigest(parseBuildManifest(selectedProductMutation)) === originalPlanDigest) fail("local selector change did not change build-plan identity");
const resolutionMutation = buildText.replace('resolver: "w.resolver/1"', 'resolver: "w.resolver/2"');
if (derivePackageSetDigest(parseBuildManifest(resolutionMutation).packages) !== expectedPackageSetDigest) fail("local resolver change changed public package identity");
if (deriveBuildPlanDigest(parseBuildManifest(resolutionMutation)) === originalPlanDigest) fail("local resolver change did not change build-plan identity");
const commentMutation = buildText.replace("// Unified data-only build manifest: direct package records and one local build coordinator.\n", "// relocated comment\n");
if (derivePackageSetDigest(parseBuildManifest(commentMutation).packages) !== expectedPackageSetDigest) fail("comments changed package identity");
const nestedOrderMutation = buildText.replace(
  'name: "linux-x64"\n        target: "x86_64-unknown-linux-gnu"\n        sandbox: "w.build-sandbox/1"',
  'sandbox: "w.build-sandbox/1"\n        name: "linux-x64"\n        target: "x86_64-unknown-linux-gnu"',
);
if (nestedOrderMutation === buildText) fail("nested order mutation did not apply");
if (deriveBuildPlanDigest(parseBuildManifest(nestedOrderMutation)) !== originalPlanDigest) fail("named field order changed build-plan identity");

console.log(`root-unification: ok (${expectedPackageSetDigest}; ${originalPlanDigest})`);
