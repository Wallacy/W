import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function resolve(studyDirectory, relative) {
  return path.resolve(studyDirectory, relative ?? "");
}

function checkDigest(file, expected, location, errors, allowPending) {
  if (allowPending && expected === "sha256:pending") return;
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push(`${location} references a missing file.`);
    return;
  }
  if (digestFile(file) !== expected) errors.push(`${location} is stale; expected ${digestFile(file)}.`);
}

export function validateAtom2StudyManifest(manifest, { studyDirectory, allowPending = false } = {}) {
  const errors = [];
  if (manifest?.$schema !== "w-atom2-atomic-contract-study-1") errors.push("ATOM2 study schema is invalid.");
  if (manifest?.status !== "design-oracle-input") errors.push("ATOM2 study status must be design-oracle-input.");
  if (manifest?.id !== "ATOM2" || manifest?.gate !== "ATOM0-G1" || manifest?.supersedes !== "ATOM1") errors.push("ATOM2 study identity or successor is invalid.");
  if (!Array.isArray(manifest?.variants) || manifest.variants.length < 6) errors.push("ATOM2 study must contain the selected, current, unsafe, and rejected variants.");
  const ids = new Set();
  for (const variant of manifest?.variants ?? []) {
    if (ids.has(variant.id)) errors.push(`ATOM2 variant is duplicated: ${variant.id}`);
    ids.add(variant.id);
    const file = resolve(studyDirectory, variant.path);
    if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
      errors.push(`ATOM2 variant is missing: ${variant.path}`);
      continue;
    }
    if (variant.language === "w" && path.extname(file) !== ".w") errors.push(`ATOM2 W variant must use .w: ${variant.path}`);
    if (variant.language === "w-reserved" && path.extname(file) === ".w") errors.push(`ATOM2 reserved variant must not use .w: ${variant.path}`);
  }
  const symbols = new Set();
  for (const ref of manifest?.sourceRefs ?? []) {
    if (!ref || typeof ref.path !== "string" || typeof ref.symbol !== "string" || typeof ref.digest !== "string" || typeof ref.claim !== "string") {
      errors.push("ATOM2 source reference schema is invalid.");
      continue;
    }
    if (symbols.has(ref.symbol)) errors.push(`ATOM2 source symbol is duplicated: ${ref.symbol}`);
    symbols.add(ref.symbol);
    const file = resolve(studyDirectory, ref.path);
    checkDigest(file, ref.digest, `ATOM2 source reference ${ref.path}`, errors, false);
    if (fs.existsSync(file)) {
      const source = fs.readFileSync(file, "utf8");
      const count = (source.match(new RegExp(ref.symbol.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&"), "gu")) ?? []).length;
      if (count !== 1) errors.push(`ATOM2 source symbol must occur exactly once: ${ref.symbol} (${count})`);
    }
  }
  for (const kind of ["corpus", "oracle", "checker", "snapshot", "bundle", "studyOracle"]) {
    const entry = manifest?.[kind];
    checkDigest(resolve(studyDirectory, entry?.path), entry?.digest, `ATOM2 ${kind}`, errors, allowPending);
  }
  if (manifest?.decision?.status !== "close-design-gate") errors.push("ATOM2 decision must close the design gate.");
  if (manifest?.decision?.a !== "promote-canonical-value-record" || manifest?.decision?.b !== "checked-generation-owner-table" || manifest?.decision?.c !== "unsafe-adapter-permitted-implementation-gap" || manifest?.decision?.d !== "reject-universal-pointer-and-rcu") errors.push("ATOM2 decision fields are incomplete.");
  return errors;
}
