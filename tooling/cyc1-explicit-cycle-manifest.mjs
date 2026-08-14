import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function nonEmpty(value) {
  return typeof value === "string" && value.trim() !== "";
}

function resolveFile(base, relative) {
  return path.resolve(base, relative ?? "");
}

function checkFile(base, relative, digest, location, errors) {
  if (!nonEmpty(relative)) {
    errors.push(`${location}.path must be non-empty.`);
    return undefined;
  }
  const file = resolveFile(base, relative);
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push(`${location}.path references a missing file: ${relative}`);
    return undefined;
  }
  if (!/^sha256:[0-9a-f]{64}$/.test(digest ?? "") || digestFile(file) !== digest) {
    errors.push(`${location}.digest is stale or malformed.`);
  }
  return file;
}

function checkSymbol(file, symbol, location, errors, unique = true) {
  if (!nonEmpty(symbol)) {
    errors.push(`${location}.symbol must be non-empty.`);
    return;
  }
  const source = fs.readFileSync(file, "utf8");
  if (!source.includes(symbol)) {
    errors.push(`${location}.symbol is absent: ${symbol}`);
    return;
  }
  if (unique) {
    const escaped = symbol.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&");
    const count = (source.match(new RegExp(escaped, "gu")) ?? []).length;
    if (count !== 1) errors.push(`${location}.symbol must occur exactly once: ${symbol} (${count})`);
  }
}

const OFFICIAL_HOSTS = new Set(["www.open-std.org", "doc.rust-lang.org", "docs.python.org", "docs.swift.org", "github.com"]);

export function validateCyc1StudyManifest(manifest, { studyDirectory, repositoryRoot, allowStaleSnapshot = false } = {}) {
  const errors = [];
  const root = repositoryRoot ?? path.resolve(studyDirectory, "../..");
  if (manifest?.$schema !== "w-cyc1-explicit-cycle-lifecycle-study-1") errors.push("CYC1 study schema is invalid.");
  if (manifest?.status !== "design-oracle-input") errors.push("CYC1 study status must be design-oracle-input.");
  if (manifest?.id !== "CYC1" || manifest?.gate !== "CYC0-G1") errors.push("CYC1 study must identify CYC0-G1.");
  if (!nonEmpty(manifest?.question) || !nonEmpty(manifest?.recommendation) || !nonEmpty(manifest?.stopCondition)) errors.push("CYC1 study question, recommendation, and stopCondition are required.");
  if (!Array.isArray(manifest?.variants) || manifest.variants.length < 5) errors.push("CYC1 study must contain current, Research, and rejected variants.");
  const variantIds = new Set();
  for (const [index, variant] of (manifest?.variants ?? []).entries()) {
    const location = `variants[${index}]`;
    if (!isValidVariant(variant, variantIds, location, errors)) continue;
    const file = checkFile(studyDirectory, variant.path, variant.digest, location, errors);
    if (!file) continue;
    if (variant.language === "w" && path.extname(file) !== ".w") errors.push(`${location}.language w requires a .w witness.`);
    if (variant.language === "w-reserved" && path.extname(file) === ".w") errors.push(`${location}.w-reserved must not use a .w witness.`);
    if (variant.language === "w" && !fs.readFileSync(file, "utf8").includes("CycleFixture")) errors.push(`${location} must contain CycleFixture.`);
  }
  const sourceKeys = new Set();
  for (const [index, ref] of (manifest?.sourceRefs ?? []).entries()) {
    const location = `sourceRefs[${index}]`;
    if (!isValidSourceRef(ref, sourceKeys, location, errors)) continue;
    const file = checkFile(studyDirectory, ref.path, ref.digest, location, errors);
    // A source reference names a stable concept, not a unique declaration.
    // Current Last Light fixtures can expose that concept at several call
    // sites (for example a callback helper and its state transition).  The
    // path+symbol key is still unique in this manifest, while occurrence
    // multiplicity is not a stale-reference condition.
    if (file) checkSymbol(file, ref.symbol, location, errors, false);
  }
  for (const [kind, entry] of ["corpus", "oracle", "referenceTest", "checker", "snapshot", "studyOracle"].map((kind) => [kind, manifest?.[kind]])) {
    if (kind === "snapshot" && allowStaleSnapshot) {
      // `check-cyc1-explicit-cycle.mjs --write` refreshes this derived file.
    } else {
      checkFile(studyDirectory, entry?.path, entry?.digest, kind, errors);
    }
  }
  if (!Array.isArray(manifest?.officialPrimaryRefs) || manifest.officialPrimaryRefs.length < 4) {
    errors.push("CYC1 study must contain at least four official primary references.");
  }
  for (const ref of manifest?.officialPrimaryRefs ?? []) {
    try {
      const url = new URL(ref.url);
      if (!OFFICIAL_HOSTS.has(url.hostname)) errors.push(`officialPrimaryRefs contains a non-primary host: ${ref.url}`);
    } catch {
      errors.push(`officialPrimaryRefs contains an invalid URL: ${ref?.url}`);
    }
    if (!nonEmpty(ref.claim)) errors.push("officialPrimaryRefs claims must be non-empty.");
  }
  if (!Array.isArray(manifest?.evidence?.current) || !Array.isArray(manifest?.evidence?.missing)) errors.push("CYC1 evidence must separate current and missing lists.");
  for (const evidence of ["tree-sitter-parse", "host-oracle", "source-ref"]) if (!manifest?.evidence?.current?.includes(evidence)) errors.push(`CYC1 current evidence is missing ${evidence}.`);
  if ((manifest?.evidence?.current ?? []).some((evidence) => /compiler-ready|runtime-executed|provider-ready/iu.test(evidence))) errors.push("CYC1 current evidence must not claim compiler, runtime, or provider readiness.");
  for (const evidence of ["w-compile", "w-run", "provider", "human-study", "model-study"]) if (!manifest?.evidence?.missing?.includes(evidence)) errors.push(`CYC1 missing evidence is missing ${evidence}.`);
  if (manifest?.recommendation?.match(/collector API|new syntax|W is implemented/iu)) errors.push("CYC1 recommendation must not promote a collector, API, syntax, or implementation.");
  return errors;
}

function isValidVariant(variant, ids, location, errors) {
  if (!variant || !nonEmpty(variant.id) || ids.has(variant.id)) {
    errors.push(`${location}.id is missing or duplicated.`);
    return false;
  }
  ids.add(variant.id);
  if (!["current", "research-candidate", "rejected-witness"].includes(variant.role)) errors.push(`${location}.role is invalid.`);
  if (!nonEmpty(variant.language)) errors.push(`${location}.language is required.`);
  return true;
}

function isValidSourceRef(ref, keys, location, errors) {
  if (!ref || !nonEmpty(ref.path) || !nonEmpty(ref.symbol) || !nonEmpty(ref.digest) || !nonEmpty(ref.claim)) {
    errors.push(`${location} source reference schema is invalid.`);
    return false;
  }
  const key = `${ref.path}\0${ref.symbol}`;
  if (keys.has(key)) errors.push(`${location} duplicates ${key}.`);
  keys.add(key);
  return true;
}
