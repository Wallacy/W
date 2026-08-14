import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const OFFICIAL_HOSTS = new Set(["www.open-std.org", "pubs.opengroup.org", "doc.rust-lang.org", "docs.python.org"]);
const REQUIRED_MISSING = ["w-compile", "w-run", "provider", "std-provider", "human-study", "model-study"];
const REQUIRED_CURRENT = ["source-ref", "official-primary", "host-oracle", "event-reducer", "mutation-checker", "snapshot"];

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function string(value) {
  return typeof value === "string" && value.trim() !== "";
}

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function fileAt(base, relative, location, errors) {
  if (!string(relative)) {
    errors.push(`${location}.path must be non-empty.`);
    return undefined;
  }
  const file = path.resolve(base, relative);
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push(`${location}.path is missing: ${relative}`);
    return undefined;
  }
  return file;
}

function checkDigest(file, expected, location, errors) {
  if (!file || !/^sha256:[0-9a-f]{64}$/u.test(expected ?? "")) {
    errors.push(`${location}.digest must be a lowercase sha256 digest.`);
    return;
  }
  const actual = digestFile(file);
  if (actual !== expected) errors.push(`${location}.digest is stale; expected ${actual}.`);
}

function checkSymbol(file, symbol, location, errors) {
  if (!string(symbol)) {
    errors.push(`${location}.symbol must be non-empty.`);
    return;
  }
  if (!fs.readFileSync(file, "utf8").includes(symbol)) errors.push(`${location}.symbol is absent: ${symbol}`);
}

function checkRefList(manifest, root, errors) {
  const seen = new Set();
  if (!Array.isArray(manifest.sourceRefs) || manifest.sourceRefs.length < 7) errors.push("DYN1 requires durable REPL/service/package/runtime source refs.");
  for (const [index, ref] of (manifest.sourceRefs ?? []).entries()) {
    const location = `sourceRefs[${index}]`;
    const key = `${ref?.path}\0${ref?.symbol}`;
    if (seen.has(key)) errors.push(`${location} duplicates ${key}.`);
    seen.add(key);
    const file = fileAt(root, ref?.path, location, errors);
    checkDigest(file, ref?.digest, location, errors);
    if (file) checkSymbol(file, ref?.symbol, location, errors);
    if (!string(ref?.claim)) errors.push(`${location}.claim must be non-empty.`);
  }
}

function checkFamilyRefs(manifest, root, errors) {
  const families = manifest?.familySourceRefs;
  if (!isObject(families)) {
    errors.push("DYN1 requires durable per-family source references.");
    return;
  }
  for (const family of ["A", "B", "C", "D"]) {
    const refs = families[family];
    if (!Array.isArray(refs) || refs.length === 0) {
      errors.push(`familySourceRefs.${family} must contain a source reference.`);
      continue;
    }
    const symbols = new Set();
    const digests = new Set();
    for (const [index, ref] of refs.entries()) {
      const location = `familySourceRefs.${family}[${index}]`;
      const file = fileAt(root, ref?.path, location, errors);
      checkDigest(file, ref?.digest, location, errors);
      if (file) checkSymbol(file, ref?.symbol, location, errors);
      if (symbols.has(ref?.symbol)) errors.push(`${location}.symbol duplicates within family ${family}.`);
      if (digests.has(ref?.digest)) errors.push(`${location}.digest duplicates within family ${family}.`);
      symbols.add(ref?.symbol);
      digests.add(ref?.digest);
      if (!string(ref?.claim)) errors.push(`${location}.claim must be non-empty.`);
    }
  }
}

function checkOfficialRefs(manifest, errors) {
  if (!Array.isArray(manifest.officialRefs) || manifest.officialRefs.length < 8) errors.push("DYN1 requires at least eight official primary references.");
  const urls = new Set();
  for (const [index, ref] of (manifest.officialRefs ?? []).entries()) {
    const location = `officialRefs[${index}]`;
    try {
      const url = new URL(ref.url);
      if (url.protocol !== "https:" || !OFFICIAL_HOSTS.has(url.hostname)) errors.push(`${location} must use an allowlisted official HTTPS host.`);
      if (urls.has(ref.url)) errors.push(`${location} duplicates an official URL.`);
      urls.add(ref.url);
    } catch {
      errors.push(`${location}.url is invalid.`);
    }
    if (!string(ref.claim)) errors.push(`${location}.claim must be non-empty.`);
  }
}

export function validateDyn1StudyManifest(manifest, { studyDirectory, repositoryRoot, allowStaleSnapshot = false } = {}) {
  const errors = [];
  const studyRoot = studyDirectory ?? process.cwd();
  const root = repositoryRoot ?? path.resolve(studyRoot, "../..");
  if (manifest?.$schema !== "w-dyn1-versioned-behavior-study-1") errors.push("DYN1 study schema is invalid.");
  if (manifest?.status !== "design-oracle-input") errors.push("DYN1 study status must be design-oracle-input.");
  if (manifest?.id !== "DYN1" || manifest?.gate !== "DYN0-G1") errors.push("DYN1 must identify DYN0-G1.");
  for (const field of ["question", "recommendation", "stopCondition"]) if (!string(manifest?.[field])) errors.push(`DYN1 ${field} is required.`);
  if (manifest?.languageDesign !== "partial") errors.push("DYN1 languageDesign must remain partial until provider and compiler evidence exist.");
  if (manifest?.stdEvidence !== "partial" || manifest?.providerEvidence !== "missing") errors.push("DYN1 std/provider evidence must remain partial/missing.");
  if (manifest?.exactGap?.id !== "DYN0-persistent-generation-reference" || manifest?.exactGap?.route !== "research" || manifest?.exactGap?.languageSurface !== "unresolved" || manifest?.exactGap?.providerGap !== true) errors.push("DYN1 exact gap must remain an unresolved candidate language surface alongside the provider gap.");
  const variantIds = new Set();
  if (!Array.isArray(manifest?.variants) || manifest.variants.length < 5) errors.push("DYN1 requires current, Research, and rejected variants.");
  for (const [index, variant] of (manifest?.variants ?? []).entries()) {
    const location = `variants[${index}]`;
    if (!string(variant?.id) || variantIds.has(variant.id)) errors.push(`${location}.id is missing or duplicated.`);
    variantIds.add(variant?.id);
    if (!["current", "research-candidate", "rejected-witness"].includes(variant?.role)) errors.push(`${location}.role is invalid.`);
    const file = fileAt(studyRoot, variant?.path, location, errors);
    checkDigest(file, variant?.digest, location, errors);
    if (variant?.language === "w" && !variant.path?.endsWith(".w")) errors.push(`${location}.language w requires a .w witness.`);
    if (variant?.language === "w-reserved" && variant.path?.endsWith(".w")) errors.push(`${location}.w-reserved cannot use a .w witness.`);
    if (file && !fs.readFileSync(file, "utf8").includes("VersionedBehaviorFixture")) errors.push(`${location} must retain the fixture marker.`);
  }
  checkRefList(manifest, root, errors);
  checkFamilyRefs(manifest, root, errors);
  checkOfficialRefs(manifest, errors);
  const cap = manifest?.composition?.capabilityMatrix;
  if (cap?.id !== "CAP0" || cap?.subcapability !== "DYN0-versioned-change" || cap?.classification !== "composable" || cap?.gate !== "CAP0") errors.push("DYN1 must retain the CAP0 DYN0-versioned-change composable gate.");
  const capCases = fileAt(studyRoot, cap?.casesPath, "composition.capabilityMatrix.cases", errors);
  const capSnapshot = fileAt(studyRoot, cap?.snapshotPath, "composition.capabilityMatrix.snapshot", errors);
  checkDigest(capCases, cap?.casesDigest, "composition.capabilityMatrix.cases", errors);
  checkDigest(capSnapshot, cap?.snapshotDigest, "composition.capabilityMatrix.snapshot", errors);
  const dyn0 = manifest?.composition?.dyn0;
  if (dyn0?.id !== "DYN0" || dyn0?.subcapability !== "DYN0-versioned-change" || dyn0?.classification !== "composable" || dyn0?.gate !== "DYN0-G1" || dyn0?.evidence !== "partial") errors.push("DYN1 must keep DYN0 composable with partial evidence.");
  if (!Array.isArray(manifest?.routeMatrix) || JSON.stringify(manifest.routeMatrix.map((item) => item.axis)) !== JSON.stringify(["A", "B", "C", "D"])) errors.push("DYN1 routeMatrix must list A, B, C, D in order.");
  for (const route of manifest?.routeMatrix ?? []) {
    if (!string(route.problem) || !string(route.disposition)) errors.push("DYN1 routeMatrix entries require problem and disposition.");
  }
  if (!Array.isArray(manifest?.docsQueue) || manifest.docsQueue.length < 4) errors.push("DYN1 requires a bounded docs queue.");
  for (const [index, item] of (manifest.docsQueue ?? []).entries()) {
    if (!string(item?.language) || !string(item?.url) || !string(item?.pseudocode) || item.pseudocode.length > 280) errors.push(`docsQueue[${index}] must contain bounded original pseudocode and an official URL.`);
  }
  if (!Array.isArray(manifest?.evidence?.current) || !Array.isArray(manifest?.evidence?.missing)) errors.push("DYN1 evidence must separate current and missing lists.");
  for (const item of REQUIRED_CURRENT) if (!manifest?.evidence?.current?.includes(item)) errors.push(`DYN1 current evidence is missing ${item}.`);
  for (const item of REQUIRED_MISSING) if (!manifest?.evidence?.missing?.includes(item)) errors.push(`DYN1 missing evidence is missing ${item}.`);
  if ((manifest?.evidence?.current ?? []).some((item) => /compiler|runtime|provider-ready/iu.test(item))) errors.push("DYN1 current evidence must not claim compiler/runtime/provider readiness.");
  for (const kind of ["corpus", "machine", "referenceTest", "checker", "snapshot", "studyOracle"]) {
    const entry = manifest?.[kind];
    const file = fileAt(studyRoot, entry?.path, kind, errors);
    if (!(kind === "snapshot" && allowStaleSnapshot)) checkDigest(file, entry?.digest, kind, errors);
  }
  if (manifest?.forbiddenMechanisms?.length !== 11) errors.push("DYN1 must enumerate all eleven rejected mechanisms.");
  if (!Array.isArray(manifest?.artifactRefs) || manifest.artifactRefs.length !== (manifest?.variants?.length ?? 0)) errors.push("DYN1 artifactRefs must mirror every variant with an exact role and digest.");
  for (const [index, artifact] of (manifest?.artifactRefs ?? []).entries()) {
    const variant = manifest.variants?.find((item) => item.id === artifact?.id);
    if (!variant || artifact.role !== variant.role || artifact.path !== variant.path || artifact.digest !== variant.digest) errors.push(`artifactRefs[${index}] must match its variant role, path, and digest.`);
  }
  return errors;
}
