import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
export const repositoryRoot = path.resolve(toolingDirectory, "..");
export const corpusPath = path.join(toolingDirectory, "pfu0-pre-freeze-usability-cases.json");
export const studyDirectory = path.join(toolingDirectory, "studies", "pfu0-pre-freeze-usability");
export const snapshotPath = path.join(toolingDirectory, "pfu0-pre-freeze-usability-results.snapshot.jsonl");

export const DECISIONS = Object.freeze(["W-1451", "W-1452", "W-1453"]);
export const FAMILIES = Object.freeze({
  manifest: Object.freeze({ decision: "W-1451", gate: "PFU0-W-1451-manifest" }),
  service: Object.freeze({ decision: "W-1452", gate: "PFU0-W-1452-service-stream" }),
  property: Object.freeze({ decision: "W-1453", gate: "PFU0-W-1453-property-lifecycle" }),
});
export const VARIANTS = Object.freeze(["current", "candidate", "adversarial"]);
export const CURRENT_EVIDENCE = Object.freeze([
  "source-ref",
  "reused-current-contract",
  "host-oracle",
  "mutation-checks",
  "snapshot",
  "thin-parse",
  "reserved-study",
]);
export const MISSING_EVIDENCE = Object.freeze([
  "w-compile",
  "w-run",
  "compiler",
  "runtime",
  "provider",
  "human-study",
  "model-study",
]);
const REQUIRED_LIFECYCLE = Object.freeze([
  "init",
  "get",
  "replace",
  "modify-enter",
  "borrow",
  "resume",
  "drop-old",
  "drop-backing",
  "reentry",
  "panic",
  "oom",
  "concurrency",
  "service-boundary",
]);
const FORBIDDEN_KEYS = new Set([
  "expected",
  "result",
  "status",
  "route",
  "score",
  "scores",
  "preference",
  "promotion",
  "manualCount",
  "observedStatus",
]);
const TOP_LEVEL_KEYS = Object.freeze([
  "$schema",
  "status",
  "id",
  "title",
  "decisions",
  "families",
  "evidence",
  "stopCondition",
  "cases",
]);
const FAMILY_KEYS = Object.freeze(["id", "decision", "gate"]);
const CASE_KEYS = Object.freeze(["id", "family", "variant", "decisions", "source", "observations"]);
const SOURCE_KEYS = Object.freeze(["path", "symbol", "digest", "claim"]);

function object(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function same(left, right) {
  return JSON.stringify(left) === JSON.stringify(right);
}

export function clone(value) {
  return structuredClone(value);
}

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function digestValue(value) {
  return `sha256:${crypto.createHash("sha256").update(JSON.stringify(value)).digest("hex")}`;
}

function exactKeys(value, keys) {
  return object(value) && same(Object.keys(value).sort(), [...keys].sort());
}

export function resolveInside(relativePath, baseDirectory = repositoryRoot) {
  if (typeof relativePath !== "string" || relativePath.trim() === "") return null;
  const resolved = path.resolve(baseDirectory, relativePath);
  const relative = path.relative(repositoryRoot, resolved);
  if (relative === "" || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) return null;
  return resolved;
}

function symbolCount(file, symbol) {
  if (!file || typeof symbol !== "string" || symbol.length === 0) return 0;
  return fs.readFileSync(file, "utf8").split(symbol).length - 1;
}

function sourceRefValid(reference) {
  const file = resolveInside(reference?.path);
  return Boolean(
    file && fs.existsSync(file) && fs.statSync(file).isFile() &&
    /^sha256:[0-9a-f]{64}$/u.test(reference?.digest ?? "") &&
    digestFile(file) === reference.digest &&
    symbolCount(file, reference.symbol) === 1,
  );
}

function forbiddenKey(value) {
  if (Array.isArray(value)) return value.some(forbiddenKey);
  if (!object(value)) return false;
  return Object.entries(value).some(([key, child]) => FORBIDDEN_KEYS.has(key) || forbiddenKey(child));
}

function sortedStrings(value) {
  return Array.isArray(value) ? [...value].sort() : [];
}

function commonManifestFacts(facts) {
  const rootFiles = sortedStrings(facts?.rootFiles);
  const packageRecords = facts?.packageRecords;
  const workspaceRecords = facts?.workspaceRecords;
  const recordCount = (Number.isInteger(packageRecords) ? packageRecords : -1) +
    (Number.isInteger(workspaceRecords) ? workspaceRecords : -1);
  const base = object(facts) && facts.dataOnly === true &&
    Number.isInteger(packageRecords) && packageRecords >= 0 && packageRecords <= 1 &&
    Number.isInteger(workspaceRecords) && workspaceRecords >= 0 && workspaceRecords <= 1 &&
    recordCount >= 1 && recordCount <= 2 &&
    facts.inlinePackage !== true && facts.nestedWorkspace !== true && facts.glob !== true &&
    facts.environmentalScan !== true && facts.executableSource !== true && facts.duplicateOwner !== true &&
    facts.owner !== "both" && facts.owner !== "none";
  if (!base || rootFiles.length === 0 || rootFiles.some((file) => !["package.w", "workspace.w", "build.w"].includes(file))) {
    return { accepted: false, reason: "manifest-root-or-data-boundary" };
  }
  const packageOnly = same(rootFiles, ["package.w"]) && packageRecords === 1 && workspaceRecords === 0 && facts.owner === "package";
  const workspaceOnly = same(rootFiles, ["workspace.w"]) && packageRecords === 0 && workspaceRecords === 1 && facts.owner === "workspace";
  const colocated = same(rootFiles, ["package.w", "workspace.w"]) && packageRecords === 1 && workspaceRecords === 1 && facts.owner === "workspace";
  const candidateBuild = same(rootFiles, ["build.w"]) &&
    ((workspaceRecords === 1 && facts.owner === "workspace") ||
      (workspaceRecords === 0 && packageRecords === 1 && facts.owner === "package"));
  if (!(packageOnly || workspaceOnly || colocated || candidateBuild)) return { accepted: false, reason: "manifest-owner-shape" };
  const targets = sortedStrings(facts.memberTargets);
  const packageTargets = sortedStrings(facts.memberBuildPackages);
  if (!same(targets, packageTargets)) return { accepted: false, reason: "member-without-package" };
  if (workspaceOnly || colocated || (candidateBuild && workspaceRecords === 1)) {
    if (targets.length === 0) return { accepted: false, reason: "workspace-member-missing" };
  } else if (targets.length !== 0) {
    return { accepted: false, reason: "package-member-unexpected" };
  }
  if (Array.isArray(facts.memberBuildWorkspaces) && facts.memberBuildWorkspaces.length > 0) {
    return { accepted: false, reason: "nested-workspace-member" };
  }
  return {
    accepted: true,
    reason: candidateBuild ? "single-build-data-only" : "separate-package-workspace-roots",
    form: candidateBuild ? "build.w" : rootFiles.join("+")
  };
}

function evaluateManifest(observations) {
  const samples = Array.isArray(observations?.samples) ? observations.samples : [];
  const results = samples.map(commonManifestFacts);
  const valid = samples.length > 0 && results.every((result) => result.accepted);
  return {
    valid,
    route: valid && results.some((result) => result.form === "build.w") ? "candidate-research" : valid ? "current-control" : "rejected-route",
    results,
  };
}

function evaluateServiceSample(facts) {
  if (!object(facts) || facts.await !== true || facts.closedTurn !== true ||
      facts.channelImplicit === true || facts.capacityImplicit === true || facts.clientStream === true ||
      facts.bidi === true || facts.serviceRefAwait === false || facts.mailboxDistinct === false) {
    return { accepted: false, reason: "service-boundary-or-hidden-transport" };
  }
  if (facts.kind === "unary") {
    return { accepted: facts.streamOpen === "none" && typeof facts.returnType === "string" && !facts.returnType.includes("Stream"), reason: "unary-control" };
  }
  if (facts.kind === "explicit-stream") {
    return { accepted: facts.streamOpen === "explicit" && facts.consumer === "for try await" && typeof facts.returnType === "string" && facts.returnType.startsWith("some Stream"), reason: "explicit-stream-control" };
  }
  if (facts.kind === "channel") {
    return { accepted: Number.isInteger(facts.channelCapacity) && facts.channelCapacity >= 0 && same(sortedStrings(facts.channelEndpoints), ["receive", "send"]), reason: "explicit-channel-control" };
  }
  if (facts.kind === "mailbox") {
    return { accepted: facts.mailbox === true && facts.streamOpen === "none" && facts.channelCapacity === null, reason: "mailbox-control" };
  }
  if (facts.kind === "service-stream-fn") {
    return {
      accepted: facts.serviceDeclaration === true && facts.serverOutput === true &&
        facts.declaration === "stream fn updates(...): Item throws Failure" &&
        facts.streamFunction === true && facts.producerAsync === true && facts.streamOpen === "explicit" &&
        facts.consumer === "for try await" && facts.normalizedReturn === "some Stream<Item,Failure>" &&
        facts.admissionFailure === "ServiceFailure" && facts.terminalFailure === "Failure",
      reason: "stream-fn-candidate",
    };
  }
  return { accepted: false, reason: "unknown-service-route" };
}

function evaluateService(observations) {
  const samples = Array.isArray(observations?.samples) ? observations.samples : [];
  const results = samples.map(evaluateServiceSample);
  const valid = samples.length > 0 && results.every((result) => result.accepted);
  const candidate = samples.some((sample) => sample?.kind === "service-stream-fn");
  return { valid, route: valid ? (candidate ? "candidate-research" : "current-control") : "rejected-route", results };
}

function evaluateProperty(observations) {
  if (!object(observations)) return { valid: false, route: "rejected-route", reason: "property-observations-missing" };
  const lifecycle = new Set(observations.lifecycle ?? []);
  const lifecycleComplete = REQUIRED_LIFECYCLE.every((step) => lifecycle.has(step));
  const baseline = observations.get === true && observations.set === true && observations.modify === true &&
    observations.deferResume === true && observations.largeCow === true && observations.noncopyable === true &&
    observations.invariantAfterBorrow === true && observations.cacheInvalidation === "explicit" &&
    observations.externalNotification === "method-service-channel" && lifecycleComplete;
  const unsafeOldValue = observations.oldValueCopy !== "none" || observations.noncopyableOwner === "none";
  const ambiguousObserver = observations.observerBypassAmbiguous === true || observations.externalNotificationImplicit === true;
  const candidate = observations.form === "local-hook-comparison" && observations.gateDecision === "compare-no-promotion";
  const valid = baseline && !unsafeOldValue && !ambiguousObserver;
  return {
    valid,
    route: valid ? (candidate ? "candidate-research" : "current-control") : "rejected-route",
    reason: valid ? (candidate ? "lifecycle-comparison" : "accessor-lifecycle-control") :
      (!lifecycleComplete ? "lifecycle-missing" : unsafeOldValue ? "hidden-old-value-copy" : "observer-bypass-ambiguous"),
    lifecycle: [...lifecycle].sort(),
  };
}

function evaluateCase(testCase) {
  let derived;
  if (testCase.family === "manifest") derived = evaluateManifest(testCase.observations);
  else if (testCase.family === "service") derived = evaluateService(testCase.observations);
  else if (testCase.family === "property") derived = evaluateProperty(testCase.observations);
  else derived = { valid: false, route: "rejected-route", reason: "unknown-family" };
  const status = derived.valid ? "accepted" : "rejected";
  return {
    caseId: testCase.id,
    family: testCase.family,
    variant: testCase.variant,
    decision: testCase.decisions?.[0] ?? null,
    status,
    route: derived.route,
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
  const file = path.resolve(root, "tooling", "pfu0-pre-freeze-usability-cases.json");
  return JSON.parse(fs.readFileSync(file, "utf8"));
}

export function validateCase(testCase) {
  const errors = [];
  if (!object(testCase) || !exactKeys(testCase, CASE_KEYS)) return ["case keys are invalid."];
  if (!/^PFU0-W-(1451|1452|1453)-(current|candidate|adversarial)$/u.test(testCase.id ?? "")) errors.push(`${testCase.id}: invalid case id.`);
  if (!Object.hasOwn(FAMILIES, testCase.family)) errors.push(`${testCase.id}: unknown family.`);
  if (!VARIANTS.includes(testCase.variant)) errors.push(`${testCase.id}: unknown variant.`);
  const family = FAMILIES[testCase.family];
  if (family && (!same(testCase.decisions, [family.decision]) || !testCase.id.startsWith(`PFU0-${family.decision}-`))) errors.push(`${testCase.id}: family decision mismatch.`);
  if (!exactKeys(testCase.source, SOURCE_KEYS) || !sourceRefValid(testCase.source)) errors.push(`${testCase.id}: source reference is stale, incomplete, or invalid.`);
  if (!object(testCase.observations)) errors.push(`${testCase.id}: observations must be an object.`);
  if (forbiddenKey(testCase)) errors.push(`${testCase.id}: expected/result/status/route/metric echo is forbidden.`);
  try {
    const result = evaluateCase(testCase);
    const expectedStatus = testCase.variant === "adversarial" ? "rejected" : "accepted";
    if (result.status !== expectedStatus) errors.push(`${testCase.id}: facts derive ${result.status}, not the variant route.`);
  } catch (error) {
    errors.push(`${testCase.id}: ${error instanceof Error ? error.message : "derivation failed"}.`);
  }
  return errors;
}

export function validateCorpus(input = loadCorpus()) {
  const errors = [];
  if (!exactKeys(input, TOP_LEVEL_KEYS)) errors.push("PFU0 corpus keys are invalid.");
  if (input.$schema !== "w-pfu0-pre-freeze-usability-cases-1") errors.push("PFU0 corpus schema is invalid.");
  if (input.status !== "design-oracle-input" || input.id !== "PFU0") errors.push("PFU0 status or id is invalid.");
  if (!same(input.decisions, DECISIONS)) errors.push("PFU0 decisions must contain W-1451, W-1452, and W-1453 in order.");
  if (!Array.isArray(input.families) || input.families.length !== 3) errors.push("PFU0 requires exactly three families.");
  const familyIds = new Set();
  for (const family of input.families ?? []) {
    if (!exactKeys(family, FAMILY_KEYS)) { errors.push("PFU0 family keys are invalid."); continue; }
    if (familyIds.has(family.id)) errors.push(`PFU0 duplicate family ${family.id}.`);
    familyIds.add(family.id);
    if (!Object.hasOwn(FAMILIES, family.id) || FAMILIES[family.id].decision !== family.decision || FAMILIES[family.id].gate !== family.gate) errors.push(`PFU0 family ${family.id} metadata is invalid.`);
  }
  if (!same([...familyIds].sort(), Object.keys(FAMILIES).sort())) errors.push("PFU0 family inventory is incomplete.");
  if (!exactKeys(input.evidence, ["current", "missing", "hostOnly"]) || input.evidence.hostOnly !== true || !same(input.evidence.current, CURRENT_EVIDENCE) || !same(input.evidence.missing, MISSING_EVIDENCE)) errors.push("PFU0 evidence boundary is invalid.");
  if (typeof input.stopCondition !== "string" || !input.stopCondition.includes("fresh") || !input.stopCondition.includes("ID-derived")) errors.push("PFU0 stop condition is incomplete.");
  if (!Array.isArray(input.cases) || input.cases.length !== 9) errors.push("PFU0 requires exactly nine cases.");
  const ids = new Set();
  const counts = new Map(Object.keys(FAMILIES).flatMap((family) => VARIANTS.map((variant) => [`${family}:${variant}`, 0])));
  const results = [];
  for (const testCase of input.cases ?? []) {
    if (ids.has(testCase?.id)) errors.push(`${testCase?.id}: duplicate case id.`);
    ids.add(testCase?.id);
    const key = `${testCase?.family}:${testCase?.variant}`;
    if (counts.has(key)) counts.set(key, counts.get(key) + 1);
    errors.push(...validateCase(testCase));
    try { results.push(evaluateCase(testCase)); } catch { /* case validation reports derivation failure */ }
  }
  for (const [key, count] of counts) if (count !== 1) errors.push(`${key}: requires exactly one case.`);
  return { errors, results };
}

export function mutationChecks() {
  const checks = {};
  const corpus = loadCorpus();
  const manifest = clone(corpus);
  manifest.cases.find((testCase) => testCase.id === "PFU0-W-1451-current").observations.samples[0].inlinePackage = true;
  checks.inlinePackageRejected = validateCorpus(manifest).results.find((result) => result.caseId === "PFU0-W-1451-current")?.status === "rejected";
  const emptyBuild = clone(corpus);
  const emptyBuildSample = emptyBuild.cases.find((testCase) => testCase.id === "PFU0-W-1451-candidate").observations.samples[0];
  emptyBuildSample.packageRecords = 0;
  emptyBuildSample.workspaceRecords = 0;
  checks.emptyBuildRejected = validateCorpus(emptyBuild).results.find((result) => result.caseId === "PFU0-W-1451-candidate")?.status === "rejected";
  const incompatibleOwner = clone(corpus);
  incompatibleOwner.cases.find((testCase) => testCase.id === "PFU0-W-1451-candidate").observations.samples[1].owner = "package";
  checks.incompatibleBuildOwnerRejected = validateCorpus(incompatibleOwner).results.find((result) => result.caseId === "PFU0-W-1451-candidate")?.status === "rejected";
  const duplicateRecord = clone(corpus);
  duplicateRecord.cases.find((testCase) => testCase.id === "PFU0-W-1451-candidate").observations.samples[0].packageRecords = 2;
  checks.duplicateBuildRecordRejected = validateCorpus(duplicateRecord).results.find((result) => result.caseId === "PFU0-W-1451-candidate")?.status === "rejected";
  const service = clone(corpus);
  service.cases.find((testCase) => testCase.id === "PFU0-W-1452-candidate").observations.samples[0].channelImplicit = true;
  checks.implicitChannelRejected = validateCorpus(service).results.find((result) => result.caseId === "PFU0-W-1452-candidate")?.status === "rejected";
  const serviceFailureBoundary = clone(corpus);
  serviceFailureBoundary.cases.find((testCase) => testCase.id === "PFU0-W-1452-candidate").observations.samples[0].admissionFailure = "Failure";
  checks.serviceFailureBoundaryRejected = validateCorpus(serviceFailureBoundary).results.find((result) => result.caseId === "PFU0-W-1452-candidate")?.status === "rejected";
  const generalStreamFn = clone(corpus);
  generalStreamFn.cases.find((testCase) => testCase.id === "PFU0-W-1452-candidate").observations.samples[0].kind = "general-stream-fn";
  checks.generalStreamFnRejected = validateCorpus(generalStreamFn).results.find((result) => result.caseId === "PFU0-W-1452-candidate")?.status === "rejected";
  const property = clone(corpus);
  property.cases.find((testCase) => testCase.id === "PFU0-W-1453-current").observations.oldValueCopy = "hidden";
  checks.hiddenOldValueCopyRejected = validateCorpus(property).results.find((result) => result.caseId === "PFU0-W-1453-current")?.status === "rejected";
  const observer = clone(corpus);
  observer.cases.find((testCase) => testCase.id === "PFU0-W-1453-current").observations.observerBypassAmbiguous = true;
  checks.observerBypassRejected = validateCorpus(observer).results.find((result) => result.caseId === "PFU0-W-1453-current")?.status === "rejected";
  const callerEcho = clone(corpus);
  callerEcho.cases[0].expected = { status: "accepted" };
  checks.expectedEchoRejected = validateCorpus(callerEcho).errors.some((error) => error.includes("keys") || error.includes("echo"));
  const staleSource = clone(corpus);
  staleSource.cases[0].source.digest = `sha256:${"0".repeat(64)}`;
  checks.staleSourceRejected = validateCorpus(staleSource).errors.some((error) => error.includes("stale") || error.includes("source reference"));
  return checks;
}

export function projectResults(results, mutations) {
  return [
    ...results.map((result) => ({
      caseId: result.caseId,
      family: result.family,
      variant: result.variant,
      decision: result.decision,
      status: result.status,
      route: result.route,
      promotion: result.promotion,
      evidenceState: result.evidenceState,
      hostOnly: result.hostOnly,
      implementationClaimed: result.implementationClaimed,
      humanResultsClaimed: result.humanResultsClaimed,
      modelResultsClaimed: result.modelResultsClaimed,
      factsDigest: result.factsDigest,
    })),
    { kind: "integrity-mutations", checks: mutations },
  ];
}

export function snapshotText(results, mutations) {
  return `${projectResults(results, mutations).map((record) => JSON.stringify(record)).join("\n")}\n`;
}

export { CASE_KEYS, FAMILY_KEYS, SOURCE_KEYS, TOP_LEVEL_KEYS };

if (import.meta.main) {
  const checked = validateCorpus();
  const mutations = mutationChecks();
  if (checked.errors.length > 0 || Object.values(mutations).some((value) => value !== true)) {
    console.error([...checked.errors, ...Object.entries(mutations).filter(([, value]) => value !== true).map(([name]) => `mutation failed: ${name}`)].join("\n"));
    process.exitCode = 1;
  } else {
    process.stdout.write(snapshotText(checked.results, mutations));
  }
}
