import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const OFFICIAL_HOSTS = new Set([
  "www.open-std.org",
  "pubs.opengroup.org",
  "doc.rust-lang.org",
  "docs.python.org",
  "github.com",
]);

const REQUIRED_CURRENT = ["source-ref", "official-primary", "host-oracle", "independent-local-reducer", "independent-split-reducer", "mutation-checker", "snapshot"];
const REQUIRED_MISSING = ["w-compile", "w-run", "provider", "std-provider", "isolation", "stress", "human-study", "model-study"];

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function string(value) {
  return typeof value === "string" && value.trim() !== "";
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

function checkDigest(file, expected, location, errors, allowStale = false) {
  if (!file || !/^sha256:[0-9a-f]{64}$/u.test(expected ?? "")) {
    errors.push(`${location}.digest must be a lowercase sha256 digest.`);
    return;
  }
  if (!allowStale) {
    const actual = digestFile(file);
    if (actual !== expected) errors.push(`${location}.digest is stale; expected ${actual}.`);
  }
}

function checkSourceRefs(manifest, repositoryRoot, errors) {
  if (!Array.isArray(manifest.sourceRefs) || manifest.sourceRefs.length < 7) {
    errors.push("HRD0 requires seven durable Last Light source references, including the shared nominal contract.");
    return;
  }
  const seen = new Set();
  for (const [index, ref] of manifest.sourceRefs.entries()) {
    const location = `sourceRefs[${index}]`;
    const key = `${ref?.path}\0${ref?.symbol}`;
    if (seen.has(key)) errors.push(`${location} duplicates ${key}.`);
    seen.add(key);
    const file = fileAt(repositoryRoot, ref?.path, location, errors);
    checkDigest(file, ref?.digest, location, errors);
    if (file && string(ref?.symbol) && !fs.readFileSync(file, "utf8").includes(ref.symbol)) errors.push(`${location}.symbol is absent: ${ref.symbol}`);
    if (!string(ref?.claim)) errors.push(`${location}.claim must be non-empty.`);
  }
  if (!manifest.sourceRefs.some((ref) => ref?.path === "reference/last-light/hot_reload_dev_contract.w" && ref?.symbol === "ReloadResult")) {
    errors.push("HRD0 sourceRefs must include the shared hot_reload_dev_contract ReloadResult witness.");
  }
}

function checkOfficialRefs(manifest, errors) {
  if (!Array.isArray(manifest.officialRefs) || manifest.officialRefs.length < 8) errors.push("HRD0 requires at least eight official primary references.");
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

function checkCrossStudies(manifest, studyDirectory, errors) {
  const required = new Set(["DYN1", "SYN1", "CAP0"]);
  if (!Array.isArray(manifest.crossStudies)) {
    errors.push("HRD0 crossStudies must link DYN1, SYN1, and CAP0.");
    return;
  }
  const seen = new Set();
  for (const [index, ref] of manifest.crossStudies.entries()) {
    const location = `crossStudies[${index}]`;
    if (!string(ref?.id) || seen.has(ref.id)) errors.push(`${location}.id is missing or duplicated.`);
    seen.add(ref?.id);
    const file = fileAt(studyDirectory, ref?.path, location, errors);
    checkDigest(file, ref?.digest, location, errors);
    if (!required.has(ref?.id)) errors.push(`${location}.id must be DYN1, SYN1, or CAP0.`);
  }
  for (const id of required) if (!seen.has(id)) errors.push(`HRD0 crossStudies is missing ${id}.`);
}

export function validateHotReloadStudyManifest(manifest, { studyDirectory, repositoryRoot, allowStaleSnapshot = false } = {}) {
  const errors = [];
  const studyRoot = studyDirectory ?? process.cwd();
  const root = repositoryRoot ?? path.resolve(studyRoot, "../..");
  if (manifest?.$schema !== "w-hrd0-hot-reload-dev-study-1") errors.push("HRD0 study schema is invalid.");
  if (manifest?.status !== "design-oracle-input") errors.push("HRD0 study status must be design-oracle-input.");
  if (manifest?.id !== "HRD0" || manifest?.gate !== "HRD0-G1") errors.push("HRD0 must identify HRD0-G1.");
  if (manifest?.languageSurface !== "none" || manifest?.profileSurface !== "absent" || manifest?.productionDynamicMode !== "rejected") errors.push("HRD0 must reject language/profile production surfaces.");
  for (const field of ["question", "recommendation", "stopCondition"]) if (!string(manifest?.[field])) errors.push(`HRD0 ${field} is required.`);
  if (!manifest?.decision || typeof manifest.decision !== "object") errors.push("HRD0 decision is required.");
  if (manifest?.decision?.runner !== "dev-only-tooling" || manifest?.decision?.normalUnits !== "reopened-by-existing-frontend" || manifest?.decision?.invocation !== "tooling-owned-unselected") errors.push("HRD0 decision must keep the runner tooling-owned and invocation unselected.");
  if (manifest?.decision?.release !== "rejected" || manifest?.decision?.activeFrameMutation !== "rejected") errors.push("HRD0 decision must reject release dynamic mode and active-frame mutation.");
  checkSourceRefs(manifest, root, errors);
  checkOfficialRefs(manifest, errors);
  checkCrossStudies(manifest, studyRoot, errors);
  if (!Array.isArray(manifest.evidence?.current) || !Array.isArray(manifest.evidence?.missing)) errors.push("HRD0 evidence must separate current and missing lists.");
  for (const item of REQUIRED_CURRENT) if (!manifest.evidence.current.includes(item)) errors.push(`HRD0 current evidence is missing ${item}.`);
  for (const item of REQUIRED_MISSING) if (!manifest.evidence.missing.includes(item)) errors.push(`HRD0 missing evidence is missing ${item}.`);
  if (manifest.evidence.current.some((item) => /compiler|runtime|provider-ready|production/iu.test(item))) errors.push("HRD0 current evidence must not claim compiler/runtime/provider/production readiness.");
  for (const kind of ["corpus", "machine", "checker", "snapshot", "studyOracle"]) {
    const entry = manifest[kind];
    const file = fileAt(studyRoot, entry?.path, kind, errors);
    if (!(kind === "snapshot" && allowStaleSnapshot)) checkDigest(file, entry?.digest, kind, errors);
  }
  if (!Number.isInteger(manifest.metrics?.caseCount) || manifest.metrics.caseCount < 20) errors.push("HRD0 metrics.caseCount must cover the required corpus.");
  if (!Array.isArray(manifest.diagnostics) || manifest.diagnostics.length < 10) errors.push("HRD0 requires a bounded diagnostic catalog.");
  const diagnosticIds = new Set();
  for (const [index, diagnostic] of (manifest.diagnostics ?? []).entries()) {
    const location = `diagnostics[${index}]`;
    if (!string(diagnostic?.id) || diagnosticIds.has(diagnostic.id)) errors.push(`${location}.id is missing or duplicated.`);
    diagnosticIds.add(diagnostic?.id);
    if (!string(diagnostic?.when) || !string(diagnostic?.meaning)) errors.push(`${location} requires when and meaning.`);
  }
  return errors;
}
