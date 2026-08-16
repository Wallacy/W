import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const SHA256 = /^sha256:[0-9a-f]{64}$/;
const ALLOWED_HOSTS = new Set(["blog.cloudflare.com", "docs.swift.org", "openfeature.dev"]);

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function verifyRef(ref, base, errors, label) {
  if (!ref || typeof ref !== "object" || Array.isArray(ref)) {
    errors.push(`${label} must be an object.`);
    return;
  }
  if (typeof ref.path !== "string" || ref.path === "") {
    errors.push(`${label}.path is missing.`);
    return;
  }
  const file = path.resolve(base, ref.path);
  const relative = path.relative(base, file);
  if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative) || !fs.existsSync(file)) {
    errors.push(`${label}.path is outside its base or missing.`);
    return;
  }
  if (!SHA256.test(ref.digest ?? "") || ref.digest !== digestFile(file)) errors.push(`${label}.digest is stale.`);
  if (ref.symbol !== undefined) {
    const text = fs.readFileSync(file, "utf8");
    const occurrences = typeof ref.symbol === "string" && ref.symbol !== "" ? text.split(ref.symbol).length - 1 : 0;
    if (occurrences !== 1) errors.push(`${label}.symbol must occur exactly once; got ${occurrences}.`);
  }
}

export function validateAvf0StudyManifest(study, { studyDirectory, repositoryRoot }) {
  const errors = [];
  if (study?.$schema !== "w-avf0-availability-feature-study-1") errors.push("study schema mismatch.");
  if (study?.status !== "design-oracle-input") errors.push("study status mismatch.");
  if (study?.id !== "AVF0") errors.push("study id mismatch.");
  const routes = Object.fromEntries((study?.routeMatrix ?? []).map((entry) => [entry.axis, entry.disposition]));
  if (routes.package !== "current" || routes.availability !== "research" || routes.runtime !== "composable" || routes.composition !== "composable") {
    errors.push("route matrix must keep package/current, availability/research, runtime/composition composable.");
  }
  for (const [index, ref] of (study?.variants ?? []).entries()) verifyRef(ref, studyDirectory, errors, `variants[${index}]`);
  for (const [index, ref] of (study?.artifactRefs ?? []).entries()) verifyRef(ref, repositoryRoot, errors, `artifactRefs[${index}]`);
  for (const [index, ref] of (study?.sourceRefs ?? []).entries()) verifyRef(ref, repositoryRoot, errors, `sourceRefs[${index}]`);
  const seenUrls = new Set();
  for (const [index, ref] of (study?.officialRefs ?? []).entries()) {
    let url;
    try { url = new URL(ref.url); } catch { url = undefined; }
    if (!url || url.protocol !== "https:" || !ALLOWED_HOSTS.has(url.hostname)) errors.push(`officialRefs[${index}] is not allowlisted.`);
    if (url && seenUrls.has(url.href)) errors.push(`officialRefs[${index}] is duplicated.`);
    if (url) seenUrls.add(url.href);
  }
  if ((study?.officialRefs ?? []).length !== 5) errors.push("study must retain exactly five AVF0 primary refs.");
  if (study?.metrics?.caseCount !== 38 || study?.metrics?.authorityRejections !== 7) errors.push("study metrics are stale.");
  if (!(study?.evidence?.missing ?? []).includes("w-compile") || !(study?.evidence?.missing ?? []).includes("provider")) {
    errors.push("missing compiler/provider evidence must remain explicit.");
  }
  return errors;
}
