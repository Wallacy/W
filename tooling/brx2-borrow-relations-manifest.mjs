import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const ARTIFACT_IDS = ["sourceBase", "corpus", "machine", "checker", "snapshot", "bundle"];
const WITNESS_ROLES = new Set(["alternative", "research-candidate", "rejected-witness"]);

function digestFile(file) {
  return "sha256:" + crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex");
}

function validDigest(value) {
  return typeof value === "string" && /^sha256:[0-9a-f]{64}$/u.test(value);
}

function resolveFile(relative, studyDirectory, root, location, errors) {
  if (typeof relative !== "string" || relative.trim() === "") {
    errors.push(location + " path is required.");
    return undefined;
  }
  const file = path.resolve(studyDirectory, relative);
  const rel = path.relative(root, file);
  if (rel.startsWith(".." + path.sep) || path.isAbsolute(rel) ||
    !fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push(location + " path must reference a file inside the repository.");
    return undefined;
  }
  return file;
}

function symbolCount(file, symbol) {
  const specials = ".^$*+?()|[]{}\\";
  const escaped = [...String(symbol)].map((char) =>
    specials.includes(char) ? "\\" + char : char).join("");
  return (fs.readFileSync(file, "utf8").match(new RegExp("\\b" + escaped + "\\b", "gu")) ?? []).length;
}

function parseJsonOrFirstLine(file, location, errors) {
  try {
    const text = fs.readFileSync(file, "utf8").trim();
    const source = file.endsWith(".json") ? text : text.split(/\r?\n/u)[0];
    return JSON.parse(source);
  } catch {
    errors.push(location + " must contain JSON metadata.");
    return undefined;
  }
}

export function validateBRX2StudyManifest(manifest, options = {}) {
  const studyDirectory = options.studyDirectory ?? path.resolve("tooling/studies/brx2-borrow-relations");
  const root = options.root ?? path.resolve(studyDirectory, "../../..");
  const errors = [];
  if (!manifest || typeof manifest !== "object") return ["BRX2 study manifest must be an object."];
  if (manifest.$schema !== "w-brx2-borrow-relations-study-1") errors.push("BRX2 study schema is invalid.");
  if (manifest.status !== "design-oracle-input") errors.push("BRX2 study status is invalid.");
  if (manifest.id !== "BRX2" || manifest.gate !== "BRX0-R2") errors.push("BRX2 study identity or gate is invalid.");
  if (manifest.bundle !== "bundle.json") errors.push("BRX2 study bundle identity is invalid.");

  const refs = manifest.artifactRefs;
  if (!Array.isArray(refs) || refs.length !== ARTIFACT_IDS.length) {
    errors.push("BRX2 artifactRefs must list sourceBase, corpus, machine, checker, snapshot, and bundle exactly once.");
  }
  const seenIds = new Set();
  const seenPaths = new Set();
  const byId = new Map();
  for (const [index, ref] of (refs ?? []).entries()) {
    const location = `artifactRefs[${index}]`;
    if (!ARTIFACT_IDS.includes(ref?.id)) errors.push(location + ".id is not an allowed BRX2 artifact.");
    if (seenIds.has(ref?.id)) errors.push(location + ".id is duplicated.");
    seenIds.add(ref?.id);
    if (seenPaths.has(ref?.path)) errors.push(location + ".path is duplicated.");
    seenPaths.add(ref?.path);
    const file = resolveFile(ref?.path, studyDirectory, root, location, errors);
    if (!validDigest(ref?.digest)) errors.push(location + ".digest must use lowercase sha256.");
    if (file && validDigest(ref?.digest) && digestFile(file) !== ref.digest) errors.push(location + ".digest is stale.");
    if (file) {
      byId.set(ref.id, { ...ref, file });
      if (ref.schema !== undefined) {
        const metadata = parseJsonOrFirstLine(file, location, errors);
        if (metadata && (metadata.$schema ?? metadata.schema) !== ref.schema) errors.push(location + ".schema does not match the file.");
      }
      if (ref.identity !== undefined) {
        const metadata = ref.schema !== undefined ? parseJsonOrFirstLine(file, location, errors) : null;
        const found = metadata?.id === ref.identity || fs.readFileSync(file, "utf8").includes(ref.identity);
        if (!found) errors.push(location + ".identity is not present in the file.");
      }
    }
  }
  for (const id of ARTIFACT_IDS) if (!byId.has(id)) errors.push("artifactRefs is missing " + id + ".");

  const sourceRef = byId.get("sourceBase");
  if (sourceRef) {
    if (manifest.sourceBase?.path !== sourceRef.path || manifest.sourceBase?.digest !== sourceRef.digest ||
      manifest.sourceBase?.symbol !== sourceRef.symbol) {
      errors.push("sourceBase must match its durable artifact ref.");
    }
    if (symbolCount(sourceRef.file, manifest.sourceBase?.symbol) !== 1) {
      errors.push("sourceBase.symbol must occur exactly once.");
    }
  }
  for (const id of ["corpus", "machine", "checker", "snapshot"]) {
    if (manifest[id] !== byId.get(id)?.path) errors.push(id + " must match its durable artifact ref.");
  }
  if (byId.get("bundle") && manifest.bundle !== byId.get("bundle").path) errors.push("bundle must match its durable artifact ref.");

  const corpusRef = byId.get("corpus");
  const corpus = corpusRef && parseJsonOrFirstLine(corpusRef.file, "corpus", errors);
  if (corpus) {
    if (corpus.$schema !== "w-brx2-borrow-relations-cases-1" || corpus.id !== "BRX2-relation-owned-borrow") {
      errors.push("corpus schema or identity is invalid.");
    }
    if (corpus.sourceBase?.digest !== manifest.sourceBase?.digest) errors.push("corpus sourceBase digest is stale.");
  }
  const bundleRef = byId.get("bundle");
  const bundle = bundleRef && parseJsonOrFirstLine(bundleRef.file, "bundle", errors);
  if (bundle && (bundle.$schema !== "w-substitution-study-bundle-1" || bundle.id !== "R1-brx2-borrow-relations")) {
    errors.push("bundle schema or identity is invalid.");
  }

  if (!Array.isArray(manifest.witnesses) || manifest.witnesses.length === 0) {
    errors.push("BRX2 witnesses must be listed.");
  }
  const witnessPaths = new Set();
  for (const [index, witness] of (manifest.witnesses ?? []).entries()) {
    const location = `witnesses[${index}]`;
    if (!WITNESS_ROLES.has(witness?.role)) errors.push(location + ".role is invalid.");
    if (witnessPaths.has(witness?.path)) errors.push(location + ".path is duplicated.");
    witnessPaths.add(witness?.path);
    const file = resolveFile(witness?.path, studyDirectory, root, location, errors);
    if (!validDigest(witness?.digest)) errors.push(location + ".digest must use lowercase sha256.");
    if (file && validDigest(witness?.digest) && digestFile(file) !== witness.digest) errors.push(location + ".digest is stale.");
  }
  return errors;
}
