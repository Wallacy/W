import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const SHA256 = /^sha256:[0-9a-f]{64}$/u;
const ALLOWED_HOSTS = new Set([
  "developers.cloudflare.com",
  "www.kernel.org",
  "webassembly.github.io",
  "wasi.dev",
  "www.rfc-editor.org",
  "www.sigstore.dev",
]);
const PROFILES = new Set([
  "trusted-native-cpu",
  "sandboxed-native-process",
  "wasm-component",
  "multi-tenant-isolate",
  "embedded-freestanding",
  "fpga-asic-hardware",
]);

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function verifyRef(ref, base, errors, label, { requireClaim = false } = {}) {
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
  if (requireClaim && (typeof ref.claim !== "string" || ref.claim.trim() === "")) errors.push(`${label}.claim is missing.`);
  if (ref.symbol !== undefined) {
    const text = fs.readFileSync(file, "utf8");
    const occurrences = typeof ref.symbol === "string" && ref.symbol !== "" ? text.split(ref.symbol).length - 1 : 0;
    if (occurrences !== 1) errors.push(`${label}.symbol must occur exactly once; got ${occurrences}.`);
  }
}

export function validateSec0StudyManifest(study, { studyDirectory, repositoryRoot }) {
  const errors = [];
  if (study?.$schema !== "w-sec0-security-model-study-1") errors.push("study schema mismatch.");
  if (study?.status !== "design-oracle-input") errors.push("study status mismatch.");
  if (study?.id !== "SEC0" || study?.gate !== "SEC0-R1") errors.push("study identity mismatch.");
  const routes = Object.fromEntries((study?.routeMatrix ?? []).map((entry) => [entry.axis, entry.disposition]));
  for (const [axis, disposition] of Object.entries({ safe: "current", profiles: "research", deployment: "research", composition: "composable" })) {
    if (routes[axis] !== disposition) errors.push(`route ${axis} must be ${disposition}.`);
  }
  for (const profile of study?.profiles ?? []) if (!PROFILES.has(profile)) errors.push(`unknown profile ${profile}.`);
  if ((study?.profiles ?? []).length !== PROFILES.size) errors.push("study must enumerate all SEC0 profiles.");
  for (const [index, ref] of (study?.variants ?? []).entries()) verifyRef(ref, studyDirectory, errors, `variants[${index}]`, { requireClaim: false });
  for (const [index, ref] of (study?.artifactRefs ?? []).entries()) verifyRef(ref, repositoryRoot, errors, `artifactRefs[${index}]`);
  for (const [index, ref] of (study?.sourceRefs ?? []).entries()) verifyRef(ref, repositoryRoot, errors, `sourceRefs[${index}]`, { requireClaim: true });
  const seenUrls = new Set();
  for (const [index, ref] of (study?.officialRefs ?? []).entries()) {
    let url;
    try { url = new URL(ref.url); } catch { url = undefined; }
    if (!url || url.protocol !== "https:" || !ALLOWED_HOSTS.has(url.hostname)) errors.push(`officialRefs[${index}] is not allowlisted.`);
    if (url && seenUrls.has(url.href)) errors.push(`officialRefs[${index}] is duplicated.`);
    if (url) seenUrls.add(url.href);
    if (typeof ref.claim !== "string" || ref.claim.trim() === "") errors.push(`officialRefs[${index}].claim is missing.`);
  }
  if ((study?.officialRefs ?? []).length < 5) errors.push("study must retain at least five SEC0 primary refs.");
  const metrics = study?.metrics ?? {};
  if (metrics.caseCount !== 101 || metrics.accepted !== 24 || metrics.rejected !== 77 || metrics.currentAccepted !== 11 || metrics.researchAccepted !== 13 || metrics.authorityRejections !== 16 || metrics.profileCount !== 6) errors.push("study metrics are stale.");
  if (!(study?.evidence?.missing ?? []).includes("w-compile") || !(study?.evidence?.missing ?? []).includes("provider") || !(study?.evidence?.missing ?? []).includes("attestation")) errors.push("compiler, provider, and attestation gaps must remain explicit.");
  return errors;
}
