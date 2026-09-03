import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { deriveIpc1 } from "./ipc1-mapped-ipc-machine.mjs";
import { deriveAvf0 } from "./avf0-availability-feature-machine.mjs";
import { deriveSec0 } from "./sec0-security-model-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
export const repositoryRoot = path.resolve(toolingDirectory, "..");
export const corpusPath = path.join(toolingDirectory, "asic0-evidence-gap-closure-cases.json");
export const studyDirectory = path.join(toolingDirectory, "studies", "asic0-evidence-gap-closure");

export const DECISIONS = Object.freeze(["W-1355", "W-1359", "W-1420", "W-1425", "W-1435"]);
export const IMPLEMENTATION_GAP_MAP = Object.freeze({
  "W-1355": "W-1448",
  "W-1359": "W-1448",
  "W-1420": "W-1449",
  "W-1425": "W-1449",
  "W-1435": "W-1450",
});
export const GAP_CATEGORY = "implementation-evidence-gap";
export const PLANNED_IMPLEMENTATION_GAPS = Object.freeze({
  "W-1448": Object.freeze({
    category: GAP_CATEGORY,
    missing: Object.freeze(["windows-two-process", "provider-receipts", "crash-recovery", "durability", "w-compile", "w-run", "ffi", "stress"]),
  }),
  "W-1449": Object.freeze({
    category: GAP_CATEGORY,
    missing: Object.freeze(["compiler", "diagnostics", "provider-publication", "local-split", "fault-stress"]),
  }),
  "W-1450": Object.freeze({
    category: GAP_CATEGORY,
    missing: Object.freeze(["compiler", "runtime", "provider", "hardware", "sandbox", "attestation-verifier", "secret-lifecycle", "ffi", "fault-stress", "local-split"]),
  }),
});

const ipcEvidenceCases = Object.freeze({
  "IPC1-menu-immutable-publish": Object.freeze({ status: "accepted", code: "immutable-generation", route: null }),
  "IPC1-channel-capn-bounded": Object.freeze({ status: "accepted", code: "bounded-mapped-channel", route: null }),
  "IPC1-atomic-width-unsupported": Object.freeze({ status: "rejected", code: "atomic-width-unsupported", route: null }),
  "IPC1-hidden-lock-reject": Object.freeze({ status: "rejected", code: "hidden-provider-state", route: null }),
  "IPC1-provider-authoritative": Object.freeze({ status: "accepted", code: "provider-authoritative", route: null }),
  "IPC1-publish-after-durability-receipt": Object.freeze({ status: "accepted", code: "published-after-receipt", route: null }),
  "IPC1-fallback-to-snapshot": Object.freeze({ status: "accepted", code: "fallback-snapshot", route: "snapshot-wire-service" }),
  "IPC1-publish-before-durability-unknown": Object.freeze({ status: "unknown", code: "unknownDurability", route: null }),
  "IPC1-provider-durable-requirement-reject": Object.freeze({ status: "rejected", code: "durability-unavailable", route: null }),
});
const avfEvidenceCases = Object.freeze({
  "AVF0-availability-provider-bind": Object.freeze({ status: "accepted", code: "availabilityBound", route: "current-design-evidence-gap" }),
  "AVF0-availability-raw-version": Object.freeze({ status: "rejected", code: "availabilityEvidenceNotAuthoritative", route: "rejected" }),
  "AVF0-runtime-typed": Object.freeze({ status: "accepted", code: "featureEvaluated", route: "composable" }),
  "AVF0-runtime-grant-capability": Object.freeze({ status: "rejected", code: "runtimeFeatureAuthorityRejected", route: "rejected" }),
});
const secEvidenceCases = Object.freeze({
  "SEC0-profile-native": Object.freeze({ status: "accepted", code: "profileAdmitted", route: "current-design-evidence-gap" }),
  "SEC0-side-baseline": Object.freeze({ status: "accepted", code: "sideChannelBudgeted", route: "current-design-evidence-gap" }),
  "SEC0-patch-baseline": Object.freeze({ status: "accepted", code: "patchAttested", route: "current-design-evidence-gap" }),
  "SEC0-supply-baseline": Object.freeze({ status: "accepted", code: "supplyChainAdmitted", route: "current" }),
  "SEC0-profile-receipt-issuer": Object.freeze({ status: "rejected", code: "protectionReceiptInvalid", route: "rejected" }),
  "SEC0-feature-echo": Object.freeze({ status: "rejected", code: "callerEchoRejected", route: "rejected" }),
});

export const REUSED_STUDIES = Object.freeze({
  "W-1355": Object.freeze({
    id: "IPC1", gapId: "W-1448", study: "tooling/studies/ipc1-mapped-ipc/study.json", corpus: "tooling/ipc1-mapped-ipc-cases.json", machine: "tooling/ipc1-mapped-ipc-machine.mjs", snapshot: "tooling/ipc1-mapped-ipc-results.snapshot.jsonl", corpusStatus: "design-oracle-input-ipc1", missing: Object.freeze(["w-compile", "w-run", "provider", "two-process-windows-probe", "crash-recovery", "durability", "human-study", "model-study"]), allowedCurrent: Object.freeze(["IPC1-channel-capn-bounded"]), allowedAdversarial: Object.freeze(["IPC1-atomic-width-unsupported"]), evidenceAllowed: Object.freeze(["IPC1-menu-immutable-publish", "IPC1-channel-capn-bounded", "IPC1-atomic-width-unsupported", "IPC1-hidden-lock-reject"]), evidenceCases: ipcEvidenceCases,
  }),
  "W-1359": Object.freeze({
    id: "IPC1", gapId: "W-1448", study: "tooling/studies/ipc1-mapped-ipc/study.json", corpus: "tooling/ipc1-mapped-ipc-cases.json", machine: "tooling/ipc1-mapped-ipc-machine.mjs", snapshot: "tooling/ipc1-mapped-ipc-results.snapshot.jsonl", corpusStatus: "design-oracle-input-ipc1", missing: Object.freeze(["w-compile", "w-run", "provider", "two-process-windows-probe", "crash-recovery", "durability", "human-study", "model-study"]), allowedCurrent: Object.freeze(["IPC1-provider-authoritative"]), allowedAdversarial: Object.freeze(["IPC1-provider-durable-requirement-reject"]), evidenceAllowed: Object.freeze(["IPC1-provider-authoritative", "IPC1-publish-after-durability-receipt", "IPC1-fallback-to-snapshot", "IPC1-publish-before-durability-unknown", "IPC1-provider-durable-requirement-reject"]), evidenceCases: ipcEvidenceCases,
  }),
  "W-1420": Object.freeze({
    id: "AVF0", gapId: "W-1449", study: "tooling/studies/avf0-availability-feature/study.json", corpus: "tooling/avf0-availability-feature-cases.json", machine: "tooling/avf0-availability-feature-machine.mjs", snapshot: "tooling/avf0-availability-feature-results.snapshot.jsonl", missing: Object.freeze(["w-compile", "w-run", "provider", "std-provider", "local-split", "fault-stress", "human-study", "model-study"]), allowedCurrent: Object.freeze(["AVF0-availability-provider-bind"]), allowedAdversarial: Object.freeze(["AVF0-availability-raw-version"]), evidenceAllowed: Object.freeze(["AVF0-availability-provider-bind", "AVF0-availability-raw-version"]), evidenceCases: avfEvidenceCases,
  }),
  "W-1425": Object.freeze({
    id: "AVF0", gapId: "W-1449", study: "tooling/studies/avf0-availability-feature/study.json", corpus: "tooling/avf0-availability-feature-cases.json", machine: "tooling/avf0-availability-feature-machine.mjs", missing: Object.freeze(["w-compile", "w-run", "provider", "std-provider", "local-split", "fault-stress", "human-study", "model-study"]), allowedCurrent: Object.freeze(["AVF0-runtime-typed"]), allowedAdversarial: Object.freeze(["AVF0-runtime-grant-capability"]), evidenceAllowed: Object.freeze(["AVF0-runtime-typed", "AVF0-runtime-grant-capability"]), evidenceCases: avfEvidenceCases,
  }),
  "W-1435": Object.freeze({
    id: "SEC0", gapId: "W-1450", study: "tooling/studies/sec0-security-model/study.json", corpus: "tooling/sec0-security-model-cases.json", machine: "tooling/sec0-security-model-machine.mjs", missing: Object.freeze(["w-compile", "w-run", "provider", "attestation", "hardware", "fault-stress", "local-split", "human-study", "model-study"]), allowedCurrent: Object.freeze(["SEC0-profile-native"]), allowedAdversarial: Object.freeze(["SEC0-profile-receipt-issuer"]), evidenceAllowed: Object.freeze(["SEC0-profile-native", "SEC0-side-baseline", "SEC0-patch-baseline", "SEC0-supply-baseline", "SEC0-profile-receipt-issuer", "SEC0-feature-echo"]), evidenceCases: secEvidenceCases,
  }),
});

const CURRENT_EVIDENCE = Object.freeze(["source-ref", "reused-corpus", "reused-machine", "host-oracle", "mutation-checks", "snapshot", "thin-parse"]);
const MISSING_EVIDENCE = Object.freeze(["w-compile", "w-run", "compiler", "runtime", "provider", "hardware", "sandbox", "attestation-verifier", "ffi", "fault-stress", "local-split", "human-study", "model-study"]);
const TOP_LEVEL_KEYS = Object.freeze(["$schema", "status", "id", "reuseOnly", "decisions", "implementationGapMap", "plannedImplementationGaps", "decisionMap", "evidence", "reusedStudies", "cases"]);
const CASE_KEYS = Object.freeze(["id", "kind", "decisions", "source", "evidenceCaseIds", "evidenceRefs", "expect"]);
const SOURCE_KEYS = Object.freeze(["study", "corpus", "machine", "caseId"]);
const EVIDENCE_REF_KEYS = Object.freeze(["probeRefIds", "sourceRefIds"]);
const EXPECT_KEYS = Object.freeze(["status", "code", "route", "contractRoute", "implementationGapId", "gapCategory"]);

export class Asic0Error extends Error {
  constructor(code, details = {}) { super(code); this.code = code; this.details = details; }
}
export function digestFile(file) { return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`; }
export function same(left, right) { return JSON.stringify(left) === JSON.stringify(right); }
export function clone(value) { return structuredClone(value); }
export function resolveInside(relativePath, baseDirectory = repositoryRoot) {
  if (typeof relativePath !== "string" || relativePath.trim() === "") return null;
  const resolved = path.resolve(baseDirectory, relativePath);
  const relative = path.relative(repositoryRoot, resolved);
  if (relative === "" || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) return null;
  return resolved;
}
function fail(code, details = {}) { throw new Asic0Error(code, details); }
function object(value) { return value !== null && typeof value === "object" && !Array.isArray(value); }
function exactKeys(value, keys) { return object(value) && same(Object.keys(value).sort(), [...keys].sort()); }
function readJson(file, location) {
  if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) fail("missingReference", { location });
  try { return JSON.parse(fs.readFileSync(file, "utf8")); } catch { fail("invalidJson", { location }); }
}
function pathReference(relativePath, location) {
  const file = resolveInside(relativePath);
  if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) fail("pathEscapeOrMissing", { location, path: relativePath });
  return file;
}
function sourceDescriptor(testCase, decision, errors) {
  const source = testCase.source;
  if (!exactKeys(source, SOURCE_KEYS)) { errors.push(`${testCase.id}: source keys must be study, corpus, machine, caseId.`); return null; }
  const expected = REUSED_STUDIES[decision];
  if (!expected) { errors.push(`${testCase.id}: decision is not reusable.`); return null; }
  for (const field of ["study", "corpus", "machine"]) {
    if (source[field] !== expected[field]) errors.push(`${testCase.id}: source.${field} must reference ${expected[field]}.`);
    pathReference(source[field], `${testCase.id}.source.${field}`);
  }
  const allowed = testCase.kind === "current-contract" ? expected.allowedCurrent : expected.allowedAdversarial;
  if (!allowed.includes(source.caseId)) errors.push(`${testCase.id}: source.caseId is not an allowed ${testCase.kind} route.`);
  return expected;
}
function sourceCorpusFor(decision) { const descriptor = REUSED_STUDIES[decision]; return readJson(pathReference(descriptor.corpus, `${decision}.corpus`), `${decision}.corpus`); }
function deriveSource(decision, sourceCaseId) {
  const descriptor = REUSED_STUDIES[decision];
  const corpus = sourceCorpusFor(decision);
  const sourceCase = corpus.cases?.find((entry) => entry.id === sourceCaseId);
  if (!sourceCase) fail("sourceCaseMissing", { decision, sourceCaseId });
  let results;
  if (descriptor.id === "IPC1") results = deriveIpc1(corpus);
  else if (descriptor.id === "AVF0") results = deriveAvf0(corpus);
  else if (descriptor.id === "SEC0") results = deriveSec0(corpus);
  else fail("unknownStudy", { decision });
  const result = results.find((entry) => entry.caseId === sourceCaseId);
  if (!result) fail("sourceResultMissing", { decision, sourceCaseId });
  return { status: result.status, code: result.code, route: result.route ?? null, sourceCase, corpus };
}
function validateEvidenceRefs(testCase, descriptor, errors) {
  if (!exactKeys(testCase.evidenceRefs, EVIDENCE_REF_KEYS)) { errors.push(`${testCase.id}: evidenceRefs keys are invalid.`); return; }
  for (const key of EVIDENCE_REF_KEYS) if (!Array.isArray(testCase.evidenceRefs[key]) || new Set(testCase.evidenceRefs[key]).size !== testCase.evidenceRefs[key].length) errors.push(`${testCase.id}: evidenceRefs.${key} must be a unique array.`);
  const corpus = sourceCorpusFor(testCase.decisions[0]);
  const sourceIds = new Set((corpus.sourceRefs ?? []).map((entry) => entry.id));
  const probeIds = new Set((corpus.probeRefs ?? []).map((entry) => entry.id));
  for (const id of testCase.evidenceRefs.sourceRefIds ?? []) if (!sourceIds.has(id)) errors.push(`${testCase.id}: evidence source ref ${id} is not closed.`);
  for (const id of testCase.evidenceRefs.probeRefIds ?? []) if (!probeIds.has(id)) errors.push(`${testCase.id}: evidence probe ref ${id} is not closed.`);
  if (descriptor.id !== "IPC1" && ((testCase.evidenceRefs.sourceRefIds ?? []).length > 0 || (testCase.evidenceRefs.probeRefIds ?? []).length > 0)) errors.push(`${testCase.id}: non-IPC evidence refs are unsupported.`);
}
function validateEvidenceCases(testCase, descriptor, errors) {
  if (!Array.isArray(testCase.evidenceCaseIds) || testCase.evidenceCaseIds.length === 0 || new Set(testCase.evidenceCaseIds).size !== testCase.evidenceCaseIds.length) { errors.push(`${testCase.id}: evidenceCaseIds must be a unique non-empty array.`); return; }
  if (!testCase.evidenceCaseIds.includes(testCase.source.caseId)) errors.push(`${testCase.id}: evidenceCaseIds must include the primary source case.`);
  for (const caseId of testCase.evidenceCaseIds) {
    const expected = descriptor.evidenceCases[caseId];
    if (!expected || !descriptor.evidenceAllowed.includes(caseId)) { errors.push(`${testCase.id}: evidence case ${caseId} is not an allowed closed route.`); continue; }
    try {
      const actual = deriveSource(testCase.decisions[0], caseId);
      for (const field of ["status", "code", "route"]) if (!same(actual[field], expected[field])) errors.push(`${testCase.id}: evidence ${caseId} ${field} drifted.`);
    } catch (error) { errors.push(`${testCase.id}: evidence ${caseId} ${error instanceof Asic0Error ? error.code : "derivation failed"}.`); }
  }
}
export function deriveAsic0Case(testCase) {
  if (!object(testCase) || typeof testCase.id !== "string") fail("caseInvalid");
  const decision = testCase.decisions?.[0];
  const descriptor = REUSED_STUDIES[decision];
  if (!descriptor) fail("decisionInvalid", { decision });
  const source = deriveSource(decision, testCase.source.caseId);
  return { caseId: testCase.id, decision, kind: testCase.kind, status: source.status, code: source.code, route: source.route, contractRoute: testCase.kind === "current-contract" ? "current" : "rejected", implementationGapId: descriptor.gapId, gapCategory: GAP_CATEGORY, sourceCaseId: testCase.source.caseId, sourceStudy: descriptor.id };
}
function validateReuseStudy(decision, errors) {
  const descriptor = REUSED_STUDIES[decision];
  const study = readJson(pathReference(descriptor.study, `${decision}.study`), `${decision}.study`);
  if (study.status !== "design-oracle-input") errors.push(`${decision}: reused study must remain design-oracle-input.`);
  if (study.id !== descriptor.id) errors.push(`${decision}: reused study id is stale.`);
  if (!same(study.evidence?.missing, descriptor.missing)) errors.push(`${decision}: reused study missing evidence changed.`);
  const corpus = sourceCorpusFor(decision);
  if (corpus.status !== (descriptor.corpusStatus ?? "design-oracle-input")) errors.push(`${decision}: reused corpus status changed.`);
  return { study, corpus };
}
export function validateCase(testCase, { checkSource = true } = {}) {
  const errors = [];
  if (!object(testCase) || !exactKeys(testCase, CASE_KEYS)) return ["case keys are invalid."];
  if (!/^ASIC0-W-(1355|1359|1420|1425|1435)-(current|adversarial)$/u.test(testCase.id ?? "")) errors.push(`${testCase.id}: case id is invalid.`);
  if (!new Set(["current-contract", "rejected-route"]).has(testCase.kind)) errors.push(`${testCase.id}: case kind is invalid.`);
  if (!Array.isArray(testCase.decisions) || testCase.decisions.length !== 1 || !DECISIONS.includes(testCase.decisions[0])) errors.push(`${testCase.id}: exactly one target decision is required.`);
  const decision = testCase.decisions?.[0];
  if (testCase.kind === "current-contract" && !testCase.id.endsWith("-current")) errors.push(`${testCase.id}: current-contract id must end with current.`);
  if (testCase.kind === "rejected-route" && !testCase.id.endsWith("-adversarial")) errors.push(`${testCase.id}: rejected-route id must end with adversarial.`);
  if (decision && !testCase.id.startsWith(`ASIC0-${decision}-`)) errors.push(`${testCase.id}: case id and original decision differ.`);
  const descriptor = decision && sourceDescriptor(testCase, decision, errors);
  if (descriptor) { validateEvidenceCases(testCase, descriptor, errors); validateEvidenceRefs(testCase, descriptor, errors); }
  if (!exactKeys(testCase.expect, EXPECT_KEYS)) errors.push(`${testCase.id}: expect keys are invalid.`);
  if (testCase.expect?.implementationGapId !== (descriptor?.gapId ?? null)) errors.push(`${testCase.id}: implementation gap id is not mapped.`);
  if (testCase.expect?.gapCategory !== GAP_CATEGORY) errors.push(`${testCase.id}: gap category must remain ${GAP_CATEGORY}.`);
  if (testCase.expect?.contractRoute !== (testCase.kind === "current-contract" ? "current" : "rejected")) errors.push(`${testCase.id}: contract route does not match case kind.`);
  if (checkSource && descriptor) {
    try {
      const actual = deriveAsic0Case(testCase);
      for (const field of ["status", "code", "route", "contractRoute", "implementationGapId", "gapCategory"]) if (!same(actual[field], testCase.expect[field])) errors.push(`${testCase.id}: ${field} expected ${JSON.stringify(testCase.expect[field])}, derived ${JSON.stringify(actual[field])}.`);
    } catch (error) { errors.push(`${testCase.id}: ${error instanceof Asic0Error ? error.code : "source derivation failed"}.`); }
  }
  return errors;
}
export function validateCorpus(input = readJson(corpusPath, "ASIC0 corpus")) {
  const errors = [];
  if (!exactKeys(input, TOP_LEVEL_KEYS)) errors.push("ASIC0 corpus keys are invalid.");
  if (input.$schema !== "w-asic0-evidence-gap-closure-cases-1") errors.push("ASIC0 corpus schema is invalid.");
  if (input.status !== "design-oracle-input") errors.push("ASIC0 corpus status must be design-oracle-input.");
  if (input.id !== "ASIC0" || input.reuseOnly !== true) errors.push("ASIC0 corpus must be reuse-only.");
  if (!same(input.decisions, DECISIONS)) errors.push("ASIC0 decisions must cover W-1355, W-1359, W-1420, W-1425, W-1435 in order.");
  if (!same(input.implementationGapMap, IMPLEMENTATION_GAP_MAP)) errors.push("ASIC0 implementationGapMap changed.");
  if (!same(Object.keys(input.plannedImplementationGaps ?? {}).sort(), Object.keys(PLANNED_IMPLEMENTATION_GAPS).sort())) errors.push("ASIC0 plannedImplementationGaps keys changed.");
  for (const gapId of Object.keys(PLANNED_IMPLEMENTATION_GAPS)) { const expected = PLANNED_IMPLEMENTATION_GAPS[gapId]; const actual = input.plannedImplementationGaps?.[gapId]; if (!actual || actual.category !== expected.category || !same(actual.missing, expected.missing)) errors.push(`${gapId}: planned implementation gap changed.`); }
  if (!same(Object.keys(input.decisionMap ?? {}).sort(), [...DECISIONS].sort())) errors.push("ASIC0 decisionMap must cover each original decision once.");
  for (const decision of DECISIONS) { const map = input.decisionMap?.[decision]; const expected = REUSED_STUDIES[decision]; if (!map || map.implementationGapId !== expected.gapId || typeof map.gate !== "string" || typeof map.route !== "string" || typeof map.fallback !== "string" || !Array.isArray(map.preserve)) errors.push(`${decision}: decision map is incomplete.`); validateReuseStudy(decision, errors); }
  if (!exactKeys(input.evidence, ["current", "missing", "hostOnly"]) || input.evidence.hostOnly !== true) errors.push("ASIC0 evidence boundary is invalid.");
  if (!same(input.evidence?.current, CURRENT_EVIDENCE)) errors.push("ASIC0 evidence.current changed.");
  if (!same(input.evidence?.missing, MISSING_EVIDENCE)) errors.push("ASIC0 evidence.missing changed.");
  if ((input.evidence?.current ?? []).some((entry) => /implemented|runtime-executed|provider-ready|conformance/iu.test(entry))) errors.push("ASIC0 evidence.current must not claim readiness or conformance.");
  if (!same(input.reusedStudies, ["IPC1", "AVF0", "SEC0"])) errors.push("ASIC0 reusedStudies changed.");
  if (!Array.isArray(input.cases) || input.cases.length !== 10) errors.push("ASIC0 requires exactly ten primary decision cases.");
  const ids = new Set(); const counts = new Map(DECISIONS.map((decision) => [decision, { current: 0, adversarial: 0 }])); const results = [];
  for (const testCase of input.cases ?? []) {
    if (ids.has(testCase?.id)) errors.push(`${testCase?.id}: duplicate case id.`); ids.add(testCase?.id);
    const decision = testCase?.decisions?.[0]; const count = counts.get(decision); if (count) count[testCase.kind === "current-contract" ? "current" : "adversarial"] += 1;
    errors.push(...validateCase(testCase));
    try { results.push(deriveAsic0Case(testCase)); } catch (error) { results.push({ caseId: testCase?.id ?? "unknown", decision, status: "rejected", code: error instanceof Asic0Error ? error.code : "derivationFailed", route: "rejected", contractRoute: testCase?.kind === "current-contract" ? "current" : "rejected", implementationGapId: IMPLEMENTATION_GAP_MAP[decision] ?? null, gapCategory: GAP_CATEGORY }); }
  }
  for (const decision of DECISIONS) { const count = counts.get(decision); if (count.current !== 1 || count.adversarial !== 1) errors.push(`${decision}: requires exactly one current and one adversarial primary case.`); }
  return { errors, results };
}
export { CURRENT_EVIDENCE, MISSING_EVIDENCE, TOP_LEVEL_KEYS, CASE_KEYS, SOURCE_KEYS, EVIDENCE_REF_KEYS, EXPECT_KEYS };
