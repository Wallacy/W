import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { validateProbeEvidence } from "./ipc1-mapped-ipc-probe.mjs";

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function digestValue(value) {
  return `sha256:${crypto.createHash("sha256").update(JSON.stringify(value)).digest("hex")}`;
}

function nonEmpty(value) {
  return typeof value === "string" && value.trim() !== "";
}

function validDigest(value) {
  return typeof value === "string" && /^sha256:[0-9a-f]{64}$/u.test(value);
}

function fileAt(base, relative, boundary = path.resolve(base, "../../..")) {
  const file = path.resolve(base, relative ?? "");
  const relativeToBoundary = path.relative(boundary, file);
  if (!relativeToBoundary || relativeToBoundary.startsWith(`..${path.sep}`) || path.isAbsolute(relativeToBoundary)) return undefined;
  return file;
}

function occurrenceCount(source, symbol) {
  const escaped = String(symbol).replace(/[.*+?^${}()|[\]\\]/gu, "\\$&");
  return (source.match(new RegExp(escaped, "gu")) ?? []).length;
}

export function validateIpc1StudyManifest(manifest, { studyDirectory } = {}) {
  const errors = [];
  if (manifest?.$schema !== "w-ipc1-mapped-ipc-study-1") errors.push("IPC1 study schema is invalid.");
  if (manifest?.status !== "design-oracle-input") errors.push("IPC1 study status must be design-oracle-input.");
  if (manifest?.id !== "IPC1" || manifest?.gate !== "IPC0-R1") errors.push("IPC1 study must identify IPC0-R1.");
  if (!Array.isArray(manifest?.variants) || manifest.variants.length < 6) errors.push("IPC1 study must contain current, current-design-evidence-gap, and rejected variants.");
  const variantIds = new Set();
  for (const [index, variant] of (manifest?.variants ?? []).entries()) {
    const location = `IPC1 variants[${index}]`;
    if (typeof variant?.id !== "string" || variant.id.trim() === "" || variantIds.has(variant.id)) errors.push(`${location}.id is missing or duplicated.`);
    variantIds.add(variant?.id);
    if (!["current", "current-design-evidence-gap", "rejected"].includes(variant?.role)) errors.push(`${location}.role is invalid.`);
    const file = fileAt(studyDirectory, variant?.path, studyDirectory);
    if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) { errors.push(`${location} is missing.`); continue; }
    if (variant.language === "w" && path.extname(file) !== ".w") errors.push(`${location} W variant must use .w.`);
    if (variant.language === "w-reserved" && path.extname(file) === ".w") errors.push(`${location} reserved variant must not use .w.`);
    if (variant.language === "w-reserved" && variant.parseEvidence?.status !== "reserved-not-parsed") errors.push(`${location} reserved variant must record reserved-not-parsed.`);
    if (variant.language === "w" && variant.parseEvidence?.status !== "tree-sitter-parse") errors.push(`${location} W variant must record tree-sitter-parse.`);
    if (typeof variant.parseEvidence?.note !== "string" || variant.parseEvidence.note.trim() === "") errors.push(`${location}.parseEvidence.note is required.`);
    if (typeof variant.digest !== "string" || digestFile(file) !== variant.digest) errors.push(`${location}.digest is stale.`);
    if (typeof manifest.entry !== "string" || !fs.readFileSync(file, "utf8").includes(manifest.entry)) errors.push(`${location} does not contain ${manifest.entry}.`);
  }
  const refs = new Set();
  const symbols = new Set();
  for (const [index, ref] of (manifest?.sourceRefs ?? []).entries()) {
    const location = `IPC1 sourceRefs[${index}]`;
    if (!ref || typeof ref.path !== "string" || typeof ref.symbol !== "string" || typeof ref.claim !== "string" || ref.claim.trim() === "") { errors.push(`${location} schema is invalid.`); continue; }
    const key = `${ref.path}\0${ref.symbol}`;
    if (refs.has(key)) errors.push(`${location} is duplicated.`);
    refs.add(key);
    if (symbols.has(ref.symbol)) errors.push(`${location}.symbol must be unique.`);
    symbols.add(ref.symbol);
    const file = fileAt(studyDirectory, ref.path);
    if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) { errors.push(`${location} is missing.`); continue; }
    if (typeof ref.digest !== "string" || digestFile(file) !== ref.digest) errors.push(`${location}.digest is stale.`);
    const count = occurrenceCount(fs.readFileSync(file, "utf8"), ref.symbol);
    if (count !== 1) errors.push(`${location}.symbol must occur exactly once (${count}).`);
  }
  for (const [kind, entry] of [["corpus", manifest?.corpus], ["oracle", manifest?.oracle]]) {
    const file = fileAt(studyDirectory, entry?.path);
    if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) errors.push(`IPC1 ${kind} reference is missing.`);
    else if (typeof entry?.digest !== "string" || digestFile(file) !== entry.digest) errors.push(`IPC1 ${kind} digest is stale.`);
  }
  const providerBindings = new Set();
  let corpusData;
  const corpusFile = fileAt(studyDirectory, manifest?.corpus?.path);
  if (corpusFile && fs.existsSync(corpusFile)) {
    try { corpusData = JSON.parse(fs.readFileSync(corpusFile, "utf8")); } catch { errors.push("IPC1 corpus reference is not valid JSON."); }
  }
  if (!Array.isArray(manifest?.providerRefs) || manifest.providerRefs.length < 6) errors.push("IPC1 providerRefs must bind all durable, volatile, and robust profiles.");
  for (const [index, reference] of (manifest?.providerRefs ?? []).entries()) {
    const location = `IPC1 providerRefs[${index}]`;
    if (!nonEmpty(reference?.binding) || providerBindings.has(reference.binding)) errors.push(`${location}.binding must be unique.`);
    providerBindings.add(reference?.binding);
    const profile = corpusData?.providers?.[reference?.binding];
    if (!profile) { errors.push(`${location}.binding is missing from the corpus.`); continue; }
    if (reference.targetKind !== profile.targetKind || reference.objectIdentity !== profile.objectIdentity) errors.push(`${location} target or object identity is stale.`);
    if (!validDigest(reference.digest) || digestValue(profile) !== reference.digest) errors.push(`${location}.digest is stale.`);
    if (!nonEmpty(reference?.claim)) errors.push(`${location}.claim is required.`);
    else if (!/^Design fixture \(not a provider receipt\):/u.test(reference.claim)) errors.push(`${location}.claim must identify a design fixture, not a provider receipt.`);
  }
  const probeIds = new Set();
  const observedTargets = new Set();
  for (const [index, probe] of (manifest?.probeRefs ?? []).entries()) {
    const location = `IPC1 probeRefs[${index}]`;
    if (!nonEmpty(probe?.id) || probeIds.has(probe.id)) errors.push(`${location}.id must be unique.`);
    probeIds.add(probe?.id);
    if (!new Set(["posix", "windows"]).has(probe?.target)) errors.push(`${location}.target is invalid.`);
    if (probe?.status !== "observed-design-evidence") errors.push(`${location}.status must remain observed-design-evidence.`);
    const file = fileAt(studyDirectory, probe?.path);
    if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) { errors.push(`${location} is missing.`); continue; }
    const digestMatches = typeof probe?.digest === "string" && digestFile(file) === probe.digest;
    if (!digestMatches) errors.push(`${location}.digest is stale.`);
    const probeErrors = validateProbeEvidence({ probe, receiptFile: file, boundaryRoot: path.resolve(studyDirectory, "../../.."), location }).errors;
    errors.push(...probeErrors);
    if (digestMatches && probeErrors.length === 0 && probe?.status === "observed-design-evidence" && new Set(["posix", "windows"]).has(probe?.target)) observedTargets.add(probe.target);
  }
  if (!observedTargets.has("posix")) errors.push("IPC1 probeRefs must include an observed POSIX probe.");
  if (!Array.isArray(manifest?.evidence?.current) || !Array.isArray(manifest?.evidence?.missing)) errors.push("IPC1 evidence must separate current and missing.");
  const current = Array.isArray(manifest?.evidence?.current) ? manifest.evidence.current : [];
  const missing = Array.isArray(manifest?.evidence?.missing) ? manifest.evidence.missing : [];
  for (const evidence of ["tree-sitter-parse", "host-oracle", "target-projections", "official-primary-refs"]) if (!current.includes(evidence)) errors.push(`IPC1 evidence.current must include ${evidence}.`);
  for (const evidence of ["w-compile", "w-run", "provider", "two-process-posix-probe", "two-process-windows-probe", "crash-recovery", "durability", "human-study", "model-study"]) if (!current.includes(evidence) && !missing.includes(evidence)) errors.push(`IPC1 evidence must classify ${evidence} as current or missing.`);
  if (current.includes("two-process-posix-probe") !== observedTargets.has("posix")) errors.push("IPC1 evidence.current two-process-posix-probe must derive from an observed POSIX receipt.");
  if (current.includes("two-process-windows-probe") !== observedTargets.has("windows")) errors.push("IPC1 evidence.current two-process-windows-probe must derive from an observed Windows receipt.");
  if (current.some((item) => /implemented|runtime-executed|provider-ready/iu.test(item))) errors.push("IPC1 evidence.current must not claim implementation or provider readiness.");
  if (missing.includes("provider") && (manifest?.providerRefs ?? []).some((reference) => /provider[- ]?ready|provider readiness|runtime[- ]?executed|authoritative.*receipt/iu.test(reference?.claim ?? ""))) errors.push("IPC1 providerRefs must not claim provider readiness while provider evidence is missing.");
  const overlap = current.filter((item) => missing.includes(item));
  if (overlap.length > 0) errors.push(`IPC1 evidence overlaps: ${overlap.join(", ")}.`);
  return errors;
}
