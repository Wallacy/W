import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const OFFICIAL_HOSTS = new Set(["pubs.opengroup.org", "learn.microsoft.com", "doc.rust-lang.org", "packaging.python.org"]);

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function resolveFile(base, relative) {
  return path.resolve(base, relative ?? "");
}

function nonEmpty(value) {
  return typeof value === "string" && value.trim() !== "";
}

export function validatePkg1StudyManifest(manifest, { studyDirectory, repositoryRoot, corpus } = {}) {
  const errors = [];
  if (manifest?.$schema !== "w-pkg1-project-transaction-study-1") errors.push("PKG1 study schema is invalid.");
  if (manifest?.status !== "design-oracle-input") errors.push("PKG1 study status must be design-oracle-input.");
  if (manifest?.id !== "PKG1") errors.push("PKG1 study id is invalid.");
  if (!manifest?.routes || manifest.routes.identitySplit !== "current" || manifest.routes.atomicTransaction !== "current") errors.push("PKG1 identity split and transaction routes must be current.");
  if (manifest?.routes?.durableProviderReceipts !== "research") errors.push("PKG1 durable provider receipts must remain Research.");

  const official = manifest?.officialRefs;
  if (!Array.isArray(official) || official.length < 5) errors.push("PKG1 requires five official primary references.");
  const officialUrls = new Set();
  for (const [index, ref] of (official ?? []).entries()) {
    let url;
    try { url = new URL(ref?.url); } catch { url = undefined; }
    if (!url || url.protocol !== "https:" || !OFFICIAL_HOSTS.has(url.hostname)) errors.push(`officialRefs[${index}] must use an allowlisted official HTTPS host.`);
    const normalized = url?.toString();
    if (normalized && officialUrls.has(normalized)) errors.push(`officialRefs[${index}] duplicates an official URL.`);
    if (normalized) officialUrls.add(normalized);
    if (!nonEmpty(ref?.claim)) errors.push(`officialRefs[${index}].claim must be non-empty.`);
  }
  if (corpus?.officialSources && JSON.stringify((official ?? []).map(({ id, url, claim }) => ({ id, url, claim }))) !== JSON.stringify(corpus.officialSources)) {
    errors.push("PKG1 study official references must equal the corpus allowlist.");
  }

  const root = repositoryRoot ?? path.resolve(studyDirectory, "../../..");
  const refs = manifest?.sourceRefs;
  if (!Array.isArray(refs) || refs.length < 4) errors.push("PKG1 requires at least four Last Light source references.");
  const keys = new Set();
  for (const [index, ref] of (refs ?? []).entries()) {
    const file = resolveFile(studyDirectory, ref?.path);
    const relative = path.relative(root, file);
    if (!nonEmpty(ref?.path) || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative) || !fs.existsSync(file)) {
      errors.push(`sourceRefs[${index}].path is outside the repository or missing.`);
      continue;
    }
    if (!nonEmpty(ref?.symbol) || !nonEmpty(ref?.claim)) errors.push(`sourceRefs[${index}] requires symbol and claim.`);
    const key = `${relative.replaceAll(path.sep, "/")}\0${ref.symbol}`;
    if (keys.has(key)) errors.push(`sourceRefs[${index}] duplicates ${key}.`);
    keys.add(key);
    if (ref.digest !== digestFile(file)) errors.push(`sourceRefs[${index}] digest is stale.`);
    const source = fs.readFileSync(file, "utf8");
    if (ref.symbol && !source.includes(ref.symbol)) errors.push(`sourceRefs[${index}] symbol is absent.`);
  }

  for (const kind of ["corpus", "oracle"]) {
    const entry = manifest?.[kind];
    const file = resolveFile(studyDirectory, entry?.path);
    if (!fs.existsSync(file)) errors.push(`PKG1 ${kind} file is missing.`);
    else if (entry?.digest !== digestFile(file)) errors.push(`PKG1 ${kind} digest is stale.`);
  }
  const artifactRefs = manifest?.artifactRefs;
  if (!Array.isArray(artifactRefs) || artifactRefs.length < 7) errors.push("PKG1 requires durable artifact references for its oracle bundle.");
  const artifactPaths = new Set();
  for (const [index, ref] of (artifactRefs ?? []).entries()) {
    const file = resolveFile(studyDirectory, ref?.path);
    const relative = path.relative(root, file);
    if (!nonEmpty(ref?.path) || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative) || !fs.existsSync(file)) {
      errors.push(`artifactRefs[${index}].path is outside the repository or missing.`);
      continue;
    }
    const normalized = relative.replaceAll(path.sep, "/");
    if (artifactPaths.has(normalized)) errors.push(`artifactRefs[${index}] duplicates ${normalized}.`);
    artifactPaths.add(normalized);
    if (!nonEmpty(ref?.role)) errors.push(`artifactRefs[${index}].role is missing.`);
    if (ref?.digest !== digestFile(file)) errors.push(`artifactRefs[${index}] digest is stale.`);
  }
  for (const required of [
    "tooling/pkg1-project-transaction-cases.json",
    "tooling/pkg1-project-transaction-machine.mjs",
    "tooling/pkg1-project-transaction-manifest.mjs",
    "tooling/check-pkg1-project-transaction.mjs",
    "tooling/pkg1-project-transaction-reference.test.mjs",
    "tooling/pkg1-project-transaction-results.snapshot.jsonl",
    "tooling/studies/pkg1-project-transaction/bundle.json",
    "tooling/studies/pkg1-project-transaction/oracle.test.mjs",
  ]) {
    if (!artifactPaths.has(required)) errors.push(`artifactRefs is missing ${required}.`);
  }
  if (!Array.isArray(manifest?.evidence?.current) || !Array.isArray(manifest?.evidence?.missing)) errors.push("PKG1 evidence must separate current and missing facts.");
  for (const evidence of ["host-oracle", "source-ref", "official-primary-refs", "snapshot"]) if (!manifest?.evidence?.current?.includes(evidence)) errors.push(`PKG1 evidence.current is missing ${evidence}.`);
  for (const evidence of ["w-compile", "w-run", "provider", "real-filesystem-fault-probe", "human-study", "model-study"]) if (!manifest?.evidence?.missing?.includes(evidence)) errors.push(`PKG1 evidence.missing is missing ${evidence}.`);
  return errors;
}
