import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

function digest(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function containedFile(base, relative, boundary, location, errors) {
  if (typeof relative !== "string" || relative.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return undefined;
  }
  const resolved = path.resolve(base, relative);
  const relativeToBoundary = path.relative(boundary, resolved);
  if (
    relativeToBoundary === "" ||
    relativeToBoundary.startsWith(`..${path.sep}`) ||
    path.isAbsolute(relativeToBoundary)
  ) {
    errors.push(`${location} must resolve to a file inside ${boundary}.`);
    return undefined;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${location} references a missing file.`);
    return undefined;
  }
  return resolved;
}

export function validateSourceRefs({
  bundleDirectory,
  wDirectory,
  sourceBaseFile,
  sourceBaseSymbol,
  sourceRefs,
  location,
}) {
  const errors = [];
  if (sourceRefs === undefined) return errors;
  if (!Array.isArray(sourceRefs) || sourceRefs.length === 0) {
    errors.push(`${location}.sourceRefs must be a non-empty array when present.`);
    return errors;
  }

  const sourceBaseKey = sourceBaseFile && typeof sourceBaseSymbol === "string"
    ? `${path.relative(wDirectory, sourceBaseFile).replaceAll(path.sep, "/")}\0${sourceBaseSymbol}`
    : undefined;
  const sourceRefKeys = new Set();
  for (const [index, sourceRef] of sourceRefs.entries()) {
    const refLocation = `${location}.sourceRefs[${index}]`;
    if (!sourceRef || typeof sourceRef !== "object" || Array.isArray(sourceRef)) {
      errors.push(`${refLocation} must be an object with path, symbol, and digest.`);
      continue;
    }
    const refFile = containedFile(bundleDirectory, sourceRef.path, wDirectory, `${refLocation}.path`, errors);
    const hasSymbol = typeof sourceRef.symbol === "string" && sourceRef.symbol.trim() !== "";
    if (!hasSymbol) errors.push(`${refLocation}.symbol must be a non-empty string.`);
    if (!/^sha256:[0-9a-f]{64}$/.test(sourceRef.digest ?? "")) {
      errors.push(`${refLocation}.digest must use a lowercase sha256 digest.`);
    } else if (refFile && digest(refFile) !== sourceRef.digest) {
      errors.push(`${refLocation}.digest is stale; expected ${digest(refFile)}.`);
    }
    if (refFile && hasSymbol && !fs.readFileSync(refFile, "utf8").includes(sourceRef.symbol)) {
      errors.push(`${refLocation}.symbol is absent from the source reference.`);
    }
    if (refFile && hasSymbol) {
      const key = `${path.relative(wDirectory, refFile).replaceAll(path.sep, "/")}\0${sourceRef.symbol}`;
      if (sourceRefKeys.has(key)) errors.push(`${refLocation} duplicates source reference ${key}.`);
      sourceRefKeys.add(key);
      if (key === sourceBaseKey) errors.push(`${refLocation} duplicates sourceBase.`);
    }
  }
  return errors;
}
