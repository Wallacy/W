import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIds } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
export const repositoryRoot = path.resolve(toolingDirectory, "..");
export const corpusPath = path.join(toolingDirectory, "aeg0-app-essentials-gate-cases.json");
export const snapshotPath = path.join(toolingDirectory, "aeg0-app-essentials-gate-results.snapshot.jsonl");
export const studyDirectory = path.join(toolingDirectory, "studies", "aeg0-app-essentials-gate");

export const DECISIONS = Object.freeze(["W-1454", "W-1455", "W-1456", "W-1457", "W-1458"]);
export const FAMILIES = Object.freeze({
  architecture: Object.freeze({ decision: "W-1454", gate: "AEG0-W-1454-capability-boundary" }),
  civil: Object.freeze({ decision: "W-1455", gate: "AEG0-W-1455-civil-profile" }),
  random: Object.freeze({ decision: "W-1456", gate: "AEG0-W-1456-random-profiles" }),
  codecs: Object.freeze({ decision: "W-1457", gate: "AEG0-W-1457-codec-limits" }),
  crypto: Object.freeze({ decision: "W-1458", gate: "AEG0-W-1458-secret-lifecycle" }),
});
export const VARIANTS = Object.freeze(["current", "rejected"]);
export const CURRENT_EVIDENCE = Object.freeze(["source-ref", "host-oracle", "mutation-checks", "snapshot", "thin-parse", "reserved-study"]);
export const MISSING_EVIDENCE = Object.freeze(["w-compile", "w-run", "compiler", "runtime", "provider", "ffi", "stress", "fault", "human-study", "model-study"]);
const TOP_LEVEL_KEYS = Object.freeze(["$schema", "status", "id", "title", "decisions", "families", "evidence", "stopCondition", "cases"]);
const CASE_KEYS = Object.freeze(["id", "family", "variant", "decisions", "source", "observations"]);
const SOURCE_KEYS = Object.freeze(["path", "symbol", "digest", "claim"]);
const FORBIDDEN_KEYS = new Set(["expected", "result", "route", "promotion", "score", "scores", "preference", "observedStatus"]);

function object(value) { return value !== null && typeof value === "object" && !Array.isArray(value); }
function same(left, right) { return JSON.stringify(left) === JSON.stringify(right); }
export function clone(value) { return structuredClone(value); }
export function digestFile(file) { return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`; }
function digestValue(value) { return `sha256:${crypto.createHash("sha256").update(JSON.stringify(value)).digest("hex")}`; }
function exactKeys(value, keys) { return object(value) && same(Object.keys(value).sort(), [...keys].sort()); }
function forbiddenKey(value) {
  if (Array.isArray(value)) return value.some(forbiddenKey);
  if (!object(value)) return false;
  return Object.entries(value).some(([key, child]) => FORBIDDEN_KEYS.has(key) || forbiddenKey(child));
}
function resolveInside(relativePath, baseDirectory = repositoryRoot) {
  if (typeof relativePath !== "string" || relativePath.trim() === "") return null;
  const resolved = path.resolve(baseDirectory, relativePath);
  const relative = path.relative(repositoryRoot, resolved);
  if (relative === "" || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) return null;
  return resolved;
}
function sourceRefValid(reference) {
  const file = resolveInside(reference?.path);
  if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) return false;
  if (!/^sha256:[0-9a-f]{64}$/u.test(reference?.digest ?? "") || digestFile(file) !== reference.digest) return false;
  if (typeof reference.symbol !== "string" || reference.symbol.length === 0) return false;
  return fs.readFileSync(file, "utf8").split(reference.symbol).length - 1 === 1;
}
function bool(value) { return value === true; }
function all(values) { return values.every(bool); }

function evaluateArchitecture(facts) {
  const contract = facts?.operationContract;
  const valid = object(facts) && all([
    facts.capabilityNominal, !facts.initializerPublic, facts.providerExplicit, facts.profileExplicit,
    facts.digestPinned, facts.ownerBound, facts.generationBound, facts.operationsExplicit,
    facts.portableValuesSeparated, facts.handlesLocal, facts.childTransfer === "borrow-or-move",
    facts.hostRebind, facts.testProviderExplicit, !facts.ambientLookup,
    contract?.effect, contract?.error, contract?.ownership, contract?.bounds,
    contract?.complexity, contract?.cancellation,
  ]);
  return { valid, reason: valid ? "nominal-capability-boundary" : "implicit-authority-or-contract-gap" };
}
function evaluateCivil(facts) {
  const valid = object(facts) && all([
    facts.operationalStdTimeUnchanged, facts.civilPackageSeparate, facts.utcTimestampDistinct,
    facts.utcTimestampPortable, facts.instantDeadlineLocal, facts.civilValuesExplicit,
    facts.conversionProvider, facts.databaseProfile, facts.conversionZoneCalendarProfile,
    facts.profileVersionDigest, facts.dstPolicy === "reject-or-explicit-policy", facts.localeExplicit,
    facts.calendarExplicit, facts.timezoneExplicit, facts.noAmbientCivil,
    !facts.wallClockDrivesDeadline, facts.leapSecondPolicy === "profile",
    facts.smearPolicy === "profile", !facts.automaticConversion,
  ]);
  return { valid, reason: valid ? "separate-civil-profile" : "ambient-or-operational-civil-mix" };
}
function evaluateRandom(facts) {
  const valid = object(facts) && all([
    facts.secureProviderBacked, !facts.secureSeedAccepted, !facts.secureFallback, !facts.secureDowngrade,
    facts.exactBoundedBytes, facts.uniformCheckedInteger, facts.typedErrors,
    facts.deterministicExplicitSeed, facts.deterministicReplayable, !facts.deterministicSatisfiesSecure,
    facts.drawOrderOwnerLocal, !facts.implicitInheritance, facts.contextProjectsSameContract,
    !facts.handlesWireValue, facts.testReceiptPortable,
    facts.deterministicReceiptSeedProfileOnly, facts.secureSeedDrawBytesNotInReceiptLogDiagnostic,
  ]);
  return { valid, reason: valid ? "secure-deterministic-profile-split" : "seed-fallback-inheritance-or-wire-leak" };
}
function evaluateCodecs(facts) {
  const quotas = facts?.quotasSeparate;
  const valid = object(facts) && all([
    facts.byteSourceExplicit, facts.byteSinkExplicit, facts.profileExplicit, facts.profileDigestPinned,
    facts.streaming, facts.finiteLimits, quotas?.encoded, quotas?.logical, quotas?.allocation,
    quotas?.depth, quotas?.ratio, facts.typedOffsetErrors, facts.typedProgress,
    !facts.cancelRollback, facts.committedBytesRemain, facts.dictionaryOwnerExplicit,
    facts.stateOwnerExplicit, !facts.filenameInference, !facts.magicInference,
    !facts.localeInference, !facts.environmentInference, facts.codecIdentitySeparate,
    facts.schemaIdentitySeparate, facts.compressionIdentitySeparate, !facts.reflectionUniversal,
  ]);
  return { valid, reason: valid ? "explicit-codec-and-compression-boundary" : "inference-quota-or-reflection-route" };
}
function evaluateCrypto(facts) {
  const lifecyclePaths = [
    ["acquire", "active", "revoking", "revoked", "released"],
    ["acquire", "active", "expired", "released"],
  ];
  const valid = object(facts) && all([
    facts.packageExplicit, facts.providerCapability, facts.deploymentBound, facts.algorithmTyped,
    facts.profilePinned, !facts.algorithmString, !facts.fallback, !facts.downgrade,
    facts.handleOpaque, facts.nonextractableDefault, facts.purposeScoped, facts.audienceScoped,
    facts.generationScoped, facts.moveOnly, lifecyclePaths.some((lifecycle) => same(facts.lifecycle, lifecycle)),
    facts.revokeBlocksAdmission, facts.admittedOperationsDrain, facts.hostRotationExpiryZeroization,
    !facts.secretPortable, facts.publicOutputsPortable, !facts.wireStorageLogDiagnosticReceipt,
    facts.childTransfer === "borrow-or-move", facts.serviceRebind,
  ]);
  return { valid, reason: valid ? "scoped-secret-lifecycle" : "plaintext-wire-fallback-or-lifecycle-gap" };
}
function evaluateCase(testCase) {
  const derived = testCase.family === "architecture" ? evaluateArchitecture(testCase.observations)
    : testCase.family === "civil" ? evaluateCivil(testCase.observations)
      : testCase.family === "random" ? evaluateRandom(testCase.observations)
        : testCase.family === "codecs" ? evaluateCodecs(testCase.observations)
          : testCase.family === "crypto" ? evaluateCrypto(testCase.observations)
            : { valid: false, reason: "unknown-family" };
  return {
    caseId: testCase.id,
    family: testCase.family,
    variant: testCase.variant,
    decision: testCase.decisions?.[0] ?? null,
    status: derived.valid ? "accepted" : "rejected",
    route: derived.valid ? "current-control" : "rejected-route",
    promotion: false,
    facts: derived,
    factsDigest: digestValue(derived),
    evidenceState: "design-oracle-input",
    hostOnly: true,
    implementationClaimed: false,
    humanResultsClaimed: false,
    modelResultsClaimed: false,
  };
}

export function loadCorpus({ root = repositoryRoot } = {}) {
  const file = path.resolve(root, "tooling", "aeg0-app-essentials-gate-cases.json");
  return JSON.parse(fs.readFileSync(file, "utf8"));
}
export function validateCase(testCase) {
  const errors = [];
  if (!object(testCase) || !exactKeys(testCase, CASE_KEYS)) return ["case keys are invalid."];
  if (!/^AEG0-W-145[4-8]-(current|[a-z-]+)$/u.test(testCase.id ?? "")) errors.push(`${testCase.id}: invalid case id.`);
  if (!Object.hasOwn(FAMILIES, testCase.family)) errors.push(`${testCase.id}: unknown family.`);
  if (!VARIANTS.includes(testCase.variant)) errors.push(`${testCase.id}: unknown variant.`);
  const family = FAMILIES[testCase.family];
  if (family && (!same(testCase.decisions, [family.decision]) || !testCase.id.startsWith(`AEG0-${family.decision}-`))) errors.push(`${testCase.id}: family decision mismatch.`);
  if (!exactKeys(testCase.source, SOURCE_KEYS) || !sourceRefValid(testCase.source)) errors.push(`${testCase.id}: source reference is stale, incomplete, or invalid.`);
  if (!object(testCase.observations)) errors.push(`${testCase.id}: observations must be an object.`);
  if (forbiddenKey(testCase)) errors.push(`${testCase.id}: expected/result/caller outcome echo is forbidden.`);
  try {
    const result = evaluateCase(testCase);
    const expectedStatus = testCase.variant === "current" ? "accepted" : "rejected";
    if (result.status !== expectedStatus) errors.push(`${testCase.id}: facts derive ${result.status}, not the variant route.`);
  } catch (error) { errors.push(`${testCase.id}: ${error instanceof Error ? error.message : "derivation failed"}.`); }
  return errors;
}
export function validateCorpus(input = loadCorpus()) {
  const errors = [];
  if (!exactKeys(input, TOP_LEVEL_KEYS)) errors.push("AEG0 corpus keys are invalid.");
  if (input.$schema !== "w-aeg0-app-essentials-gate-cases-1" || input.status !== "design-oracle-input" || input.id !== "AEG0") errors.push("AEG0 identity is invalid.");
  if (!same(input.decisions, DECISIONS)) errors.push("AEG0 decisions must contain W-1454 through W-1458 in order.");
  const familyIds = new Set();
  if (!Array.isArray(input.families) || input.families.length !== 5) errors.push("AEG0 requires exactly five families.");
  for (const family of input.families ?? []) {
    if (!exactKeys(family, ["id", "decision", "gate"])) errors.push("AEG0 family keys are invalid.");
    if (familyIds.has(family.id)) errors.push(`AEG0 duplicate family ${family.id}.`);
    familyIds.add(family.id);
    if (!Object.hasOwn(FAMILIES, family.id) || FAMILIES[family.id].decision !== family.decision || FAMILIES[family.id].gate !== family.gate) errors.push(`AEG0 family ${family.id} metadata is invalid.`);
  }
  if (!same([...familyIds].sort(), Object.keys(FAMILIES).sort())) errors.push("AEG0 family inventory is incomplete.");
  if (!exactKeys(input.evidence, ["current", "missing", "hostOnly"]) || input.evidence.hostOnly !== true || !same(input.evidence.current, CURRENT_EVIDENCE) || !same(input.evidence.missing, MISSING_EVIDENCE)) errors.push("AEG0 evidence boundary is invalid.");
  if (typeof input.stopCondition !== "string" || !input.stopCondition.includes("stale") || !input.stopCondition.includes("Research")) errors.push("AEG0 stop condition is incomplete.");
  if (!Array.isArray(input.cases) || input.cases.length < 12) errors.push("AEG0 requires at least twelve cases.");
  const ids = new Set();
  const familyCounts = new Map(Object.keys(FAMILIES).map((family) => [family, { current: 0, rejected: 0 }]));
  const results = [];
  for (const testCase of input.cases ?? []) {
    if (ids.has(testCase?.id)) errors.push(`${testCase?.id}: duplicate case id.`);
    ids.add(testCase?.id);
    if (familyCounts.has(testCase?.family) && VARIANTS.includes(testCase?.variant)) familyCounts.get(testCase.family)[testCase.variant] += 1;
    errors.push(...validateCase(testCase));
    try { results.push(evaluateCase(testCase)); } catch { /* validation reports derivation failure */ }
  }
  for (const [family, counts] of familyCounts) {
    if (counts.current < 1 || counts.rejected < 1) errors.push(`${family}: requires current and rejected cases.`);
  }
  const currentCount = results.filter((result) => result.status === "accepted").length;
  const rejectedCount = results.filter((result) => result.status === "rejected").length;
  if (currentCount < 5 || rejectedCount < 7) errors.push("AEG0 requires at least five current and seven rejected derived routes.");
  return { errors, results };
}

export function researchZero({ root = repositoryRoot } = {}) {
  try {
    const classification = JSON.parse(fs.readFileSync(path.resolve(root, "tooling", "design-freeze-classification.json"), "utf8"));
    const entries = new Map((classification.entries ?? []).map((entry) => [entry.decisionId, entry]));
    const historicalIds = ledgerIds.filter((id) => Number(id.slice(2)) <= 1459);
    return historicalIds.every((id) => entries.has(id) && entries.get(id).category !== "research-gated") &&
      DECISIONS.every((id) => entries.get(id)?.category === "oracle-backed-current");
  } catch { return false; }
}

export function mutationChecks() {
  const checks = {};
  const corpus = loadCorpus();
  const mutate = (caseId, fn) => { const copy = clone(corpus); fn(copy.cases.find((testCase) => testCase.id === caseId).observations); return validateCorpus(copy); };
  checks.ambientAuthorityRejected = mutate("AEG0-W-1454-current", (facts) => { facts.ambientLookup = true; }).results.find((result) => result.caseId === "AEG0-W-1454-current")?.status === "rejected";
  checks.seededSecureRejected = mutate("AEG0-W-1456-current", (facts) => { facts.secureSeedAccepted = true; }).results.find((result) => result.caseId === "AEG0-W-1456-current")?.status === "rejected";
  checks.secureFallbackRejected = mutate("AEG0-W-1456-current", (facts) => { facts.secureFallback = true; }).results.find((result) => result.caseId === "AEG0-W-1456-current")?.status === "rejected";
  checks.secureReceiptLeakRejected = mutate("AEG0-W-1456-current", (facts) => { facts.secureSeedDrawBytesNotInReceiptLogDiagnostic = false; }).results.find((result) => result.caseId === "AEG0-W-1456-current")?.status === "rejected";
  checks.ambiguousLifecycleRejected = mutate("AEG0-W-1458-current", (facts) => { facts.lifecycle = ["acquire", "active", "expired", "revoked", "released"]; }).results.find((result) => result.caseId === "AEG0-W-1458-current")?.status === "rejected";
  checks.plaintextSerializationRejected = mutate("AEG0-W-1458-current", (facts) => { facts.wireStorageLogDiagnosticReceipt = true; }).results.find((result) => result.caseId === "AEG0-W-1458-current")?.status === "rejected";
  checks.codecInferenceRejected = mutate("AEG0-W-1457-current", (facts) => { facts.filenameInference = true; }).results.find((result) => result.caseId === "AEG0-W-1457-current")?.status === "rejected";
  checks.staleSourceDigestRejected = (() => { const copy = clone(corpus); copy.cases[0].source.digest = `sha256:${"0".repeat(64)}`; return validateCorpus(copy).errors.some((error) => error.includes("stale") || error.includes("source reference")); })();
  checks.callerEchoRejected = (() => { const copy = clone(corpus); copy.cases[0].expected = { status: "accepted" }; return validateCorpus(copy).errors.some((error) => error.includes("echo") || error.includes("keys")); })();
  checks.researchZero = researchZero();
  return checks;
}
export function projectResults(results, mutations) {
  return [...results.map((result) => ({ caseId: result.caseId, family: result.family, variant: result.variant, decision: result.decision, status: result.status, route: result.route, promotion: result.promotion, evidenceState: result.evidenceState, hostOnly: result.hostOnly, implementationClaimed: result.implementationClaimed, humanResultsClaimed: result.humanResultsClaimed, modelResultsClaimed: result.modelResultsClaimed, factsDigest: result.factsDigest })), { kind: "integrity-mutations", checks: mutations }];
}
export function snapshotText(results, mutations) { return `${projectResults(results, mutations).map((record) => JSON.stringify(record)).join("\n")}\n`; }
export { CASE_KEYS, SOURCE_KEYS, TOP_LEVEL_KEYS };

if (import.meta.main) {
  const checked = validateCorpus();
  const mutations = mutationChecks();
  if (checked.errors.length > 0 || Object.values(mutations).some((value) => value !== true)) {
    console.error([...checked.errors, ...Object.entries(mutations).filter(([, value]) => value !== true).map(([name]) => `mutation failed: ${name}`)].join("\n"));
    process.exitCode = 1;
  } else process.stdout.write(snapshotText(checked.results, mutations));
}
