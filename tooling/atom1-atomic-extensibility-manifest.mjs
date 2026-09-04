import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function resolveFile(studyDirectory, relative) {
  return path.resolve(studyDirectory, relative ?? "");
}

export function validateAtom1StudyManifest(manifest, { studyDirectory } = {}) {
  const errors = [];
  if (manifest?.$schema !== "w-atomic-extensibility-study-1") errors.push("ATOM1 study schema is invalid.");
  if (manifest?.status !== "design-oracle-input") errors.push("ATOM1 study status must be design-oracle-input.");
  if (manifest?.id !== "ATOM1" || manifest?.gate !== "ATOM0-G1") errors.push("ATOM1 study must identify ATOM0-G1.");
  if (!Array.isArray(manifest?.variants) || manifest.variants.length < 5) errors.push("ATOM1 study must contain current and historical-candidate variants.");
  const variantIds = new Set();
  for (const variant of manifest?.variants ?? []) {
    if (typeof variant.id !== "string" || variant.id.trim() === "" || variantIds.has(variant.id)) errors.push(`ATOM1 variant id is missing or duplicated: ${variant?.id}`);
    variantIds.add(variant.id);
    const file = resolveFile(studyDirectory, variant.path);
    if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
      errors.push(`ATOM1 variant is missing: ${variant.path}`);
      continue;
    }
    if (variant.language === "w" && path.extname(file) !== ".w") errors.push(`ATOM1 W variant must use .w: ${variant.path}`);
    if (variant.language === "w-reserved" && path.extname(file) === ".w") errors.push(`ATOM1 reserved variant must not use .w: ${variant.path}`);
  }
  const symbols = new Set();
  const refs = new Set();
  for (const ref of manifest?.sourceRefs ?? []) {
    if (!ref || typeof ref.path !== "string" || typeof ref.symbol !== "string" || typeof ref.digest !== "string" || typeof ref.claim !== "string" || ref.claim.trim() === "") {
      errors.push(`ATOM1 source reference schema is invalid: ${ref?.path ?? "<missing>"}`);
      continue;
    }
    const key = `${ref.path}\0${ref.symbol}`;
    if (refs.has(key)) errors.push(`ATOM1 source reference is duplicated: ${ref.path}#${ref.symbol}`);
    refs.add(key);
    if (symbols.has(ref.symbol)) errors.push(`ATOM1 source symbol is duplicated: ${ref.symbol}`);
    symbols.add(ref.symbol);
    const file = resolveFile(studyDirectory, ref.path);
    if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
      errors.push(`ATOM1 source reference is missing: ${ref.path}`);
      continue;
    }
    if (digestFile(file) !== ref.digest) errors.push(`ATOM1 source reference digest is stale: ${ref.path}`);
    const source = fs.readFileSync(file, "utf8");
    const escaped = ref.symbol.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&");
    const count = (source.match(new RegExp(escaped, "gu")) ?? []).length;
    if (count === 0) errors.push(`ATOM1 source symbol is absent: ${ref.symbol}`);
    else if (count !== 1) errors.push(`ATOM1 source symbol must occur exactly once: ${ref.symbol} (${count})`);
  }
  for (const [kind, entry] of [["corpus", manifest?.corpus], ["oracle", manifest?.oracle]]) {
    const file = resolveFile(studyDirectory, entry?.path);
    if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
      errors.push(`ATOM1 ${kind} reference is missing: ${entry?.path}`);
      continue;
    }
    if (typeof entry?.digest !== "string" || digestFile(file) !== entry.digest) errors.push(`ATOM1 ${kind} digest is stale: ${entry?.path}`);
  }
  return errors;
}
