import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIds } from "./design-ledger.mjs";
import { validateProtocol } from "./hum0-human-review-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
export const repositoryRoot = path.resolve(toolingDirectory, "..");
export const corpusPath = path.join(toolingDirectory, "final-research-closure-cases.json");
export const studyDirectory = path.join(toolingDirectory, "studies", "final-research-closure");
export const snapshotPath = path.join(toolingDirectory, "final-research-closure-results.snapshot.jsonl");
export const manifestPath = path.join(studyDirectory, "manifest.json");
export const bundlePath = path.join(studyDirectory, "bundle.json");

export const DECISIONS = Object.freeze(["W-707", "W-731", "W-1408"]);
export const HISTORICAL_SNAPSHOT_LAST = "W-1450";
export const HISTORICAL_SNAPSHOT_IDS = Object.freeze(
  ledgerIds.filter((decisionId) => Number(decisionId.slice(2)) <= Number(HISTORICAL_SNAPSHOT_LAST.slice(2))),
);
export const PFU0_DECISIONS = Object.freeze(["W-1451", "W-1452", "W-1453"]);
export const PFU0_DISPOSITIONS = Object.freeze({
  "W-1451": "oracle-backed-current",
  "W-1452": "superseded",
  "W-1453": "superseded",
});
export const PFU0_SUPERSESSIONS = Object.freeze({
  "W-1452": "W-1480",
  "W-1453": "W-1516",
});
// W-1486 and W-1503 were active post-snapshot gates when FRC0 was authored.
// W-1517 and W-1518 close those design contracts; FRC0 preserves the old
// names as history while the current classification has no research residual.
export const HISTORICAL_POST_SNAPSHOT_RESEARCH_GATES = Object.freeze(["W-1486", "W-1503"]);
export const ACTIVE_RESEARCH_GATES = Object.freeze([]);
export const RESEARCH_STATE_INVENTORY_PATH = "tooling/research-state-inventory.json";
export const RESEARCH_STATE_CATEGORIES = Object.freeze([
  "historical",
  "rejected",
  "current-design-evidence-gap",
  "future-reopen-candidate",
]);
export const RESEARCH_STATE_FAMILY_IDS = Object.freeze([
  "raw-w218",
  "drc0",
  "avf0",
  "sec0",
  "ipc1",
  "gen1",
  "syn1",
  "hrd0",
  "dyn1",
  "atom1",
  "atom2",
  "cyc1",
  "brx2",
  "brx3",
  "w1504",
]);
export const DESIGN_ONLY_CLOSURES = Object.freeze({
  "W-1517": "oracle-backed-current",
  "W-1518": "oracle-backed-current",
});
export const DISPOSITIONS = Object.freeze({
  "W-707": "oracle-backed-current",
  "W-731": "oracle-backed-current",
  "W-1408": "oracle-backed-current",
});
export const GATES = Object.freeze({
  "W-707": Object.freeze({
    id: "FZ0-freeze-completeness",
    meaning: "frontend completeness protocol with source, CST, diagnostic, and workflow witnesses",
  }),
  "W-731": Object.freeze({
    id: "freeze-research-close",
    meaning: "one explicit disposition for every ledger decision and an exact list of active post-snapshot research gates",
  }),
  "W-1408": Object.freeze({
    id: "HUM0-promotion",
    meaning: "first-stop protocol and no automatic promotion with zero human/model records",
  }),
});
export const CURRENT_EVIDENCE = Object.freeze([
  "source-ref",
  "reused-corpus",
  "reused-machine",
  "host-oracle",
  "mutation-checks",
  "snapshot",
  "thin-parse",
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
export const REUSE = Object.freeze({
  "W-707": Object.freeze([
    "tooling/frontend-freeze-cases.json",
    "tooling/check-frontend-freeze.mjs",
    "tooling/frontend-freeze.snapshot.jsonl",
    "tooling/formatter-cases.json",
    "tooling/semantic-cases.json",
    "tooling/module-run-cases.json",
    "tooling/diagnostic-catalog.json",
  ]),
  "W-731": Object.freeze([
    "RATIONALE.md",
    "DESIGN.md",
    "tooling/design-freeze-classification.json",
    "tooling/check-design-freeze-audit.mjs",
    "DESIGN-INDEX.md",
    "tooling/check-study-bundles.mjs",
  ]),
  "W-1408": Object.freeze([
    "tooling/hum0-human-review-protocol.json",
    "tooling/hum0-human-review-machine.mjs",
    "tooling/check-hum0-human-review.mjs",
    "tooling/hum0-human-review-results.snapshot.jsonl",
    "tooling/studies/hum0-human-review/study.json",
    "tooling/studies/hum0-human-review/oracle.test.mjs",
  ]),
});

export const MANIFEST_ARTIFACTS = Object.freeze({
  corpus: "tooling/final-research-closure-cases.json",
  machine: "tooling/final-research-closure-machine.mjs",
  "root-checker": "tooling/check-final-research-closure.mjs",
  "nested-checker": "tooling/tree-sitter-w/check-frc0.mjs",
  snapshot: "tooling/final-research-closure-results.snapshot.jsonl",
  oracle: "tooling/studies/final-research-closure/oracle.test.mjs",
  bundle: "tooling/studies/final-research-closure/bundle.json",
  study: "tooling/studies/final-research-closure/study.json",
  readme: "tooling/studies/final-research-closure/README.md",
  "study-index": "tooling/studies/final-research-closure/INDEX.md",
  current: "tooling/studies/final-research-closure/current.w",
  adversarial: "tooling/studies/final-research-closure/adversarial.w",
  "fz0-corpus": "tooling/frontend-freeze-cases.json",
  "fz0-checker": "tooling/check-frontend-freeze.mjs",
  "fz0-snapshot": "tooling/frontend-freeze.snapshot.jsonl",
  "fz0-formatter": "tooling/formatter-cases.json",
  "fz0-semantic": "tooling/semantic-cases.json",
  "fz0-module-run": "tooling/module-run-cases.json",
  "fz0-diagnostics": "tooling/diagnostic-catalog.json",
  "hum0-protocol": "tooling/hum0-human-review-protocol.json",
  "hum0-machine": "tooling/hum0-human-review-machine.mjs",
  "hum0-checker": "tooling/check-hum0-human-review.mjs",
  "hum0-snapshot": "tooling/hum0-human-review-results.snapshot.jsonl",
  "hum0-study": "tooling/studies/hum0-human-review/study.json",
  "hum0-oracle": "tooling/studies/hum0-human-review/oracle.test.mjs",
  "freeze-checker": "tooling/check-design-freeze-audit.mjs",
  "study-checker": "tooling/check-study-bundles.mjs",
  classification: "tooling/design-freeze-classification.json",
  "research-state-inventory": RESEARCH_STATE_INVENTORY_PATH,
  ledger: "RATIONALE.md",
  design: "DESIGN.md",
  index: "DESIGN-INDEX.md",
});

const TOP_LEVEL_KEYS = Object.freeze([
  "$schema",
  "status",
  "id",
  "reuseOnly",
  "decisions",
  "dispositions",
  "evidence",
  "reuse",
  "historicalSnapshot",
  "historicalPostSnapshotResearchGates",
  "reopenedResearch",
  "activeResearchGates",
  "designOnlyClosures",
  "cases",
]);
const CASE_KEYS = Object.freeze(["id", "kind", "decisions", "gate", "mutation"]);
const MANIFEST_KEYS = Object.freeze([
  "$schema",
  "status",
  "id",
  "reuseOnly",
  "decisions",
  "artifacts",
  "evidence",
  "stopCondition",
]);
const FORBIDDEN_CASE_KEYS = new Set([
  "expected",
  "result",
  "status",
  "score",
  "scores",
  "preference",
  "promotion",
  "manualCount",
]);

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

function readJson(relativePath, baseDirectory = repositoryRoot) {
  const file = resolveInside(relativePath, baseDirectory);
  if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) throw new Error(`missing:${relativePath}`);
  return JSON.parse(fs.readFileSync(file, "utf8"));
}

export function loadResearchStateInventory({ root = repositoryRoot } = {}) {
  return readJson(RESEARCH_STATE_INVENTORY_PATH, path.resolve(root));
}

export function validateResearchStateInventory(inventory = loadResearchStateInventory()) {
  const errors = [];
  const topLevelKeys = ["$schema", "status", "id", "categories", "active", "families"];
  if (!exactKeys(inventory, topLevelKeys)) errors.push("research-state inventory keys are invalid.");
  if (inventory?.$schema !== "w-research-state-inventory-1" || !["normalization-in-progress", "authoritative-maintained-surface"].includes(inventory?.status) || inventory?.id !== "research-state-inventory") {
    errors.push("research-state inventory identity is invalid.");
  }
  if (!same(inventory?.categories, RESEARCH_STATE_CATEGORIES)) errors.push("research-state inventory category vocabulary is invalid.");
  if (!Array.isArray(inventory?.active) || inventory.active.length !== 0) errors.push("research-state inventory active must be exactly empty.");
  const families = Array.isArray(inventory?.families) ? inventory.families : [];
  const ids = families.map((family) => family?.id);
  if (families.length !== RESEARCH_STATE_FAMILY_IDS.length) errors.push("research-state inventory family count is invalid.");
  if (new Set(ids).size !== ids.length) errors.push("research-state inventory family IDs must be unique.");
  if (!same([...ids].sort(), [...RESEARCH_STATE_FAMILY_IDS].sort())) errors.push("research-state inventory family IDs are not exhaustive.");
  const normalizationPendingCount = families.filter((family) => family?.normalizationPending === true).length;
  if (inventory?.status === "normalization-in-progress" && normalizationPendingCount === 0) errors.push("research-state inventory status must close when no family is pending.");
  if (inventory?.status === "authoritative-maintained-surface" && normalizationPendingCount > 0) errors.push("research-state inventory cannot be authoritative while normalization is pending.");
  const familyKeys = ["id", "category", "decisionRefs", "successorDecisions", "implementationGaps", "normalizationPending", "artifacts"];
  const activeLookingCategories = new Set(["active", "open", "candidate", "research", "research-gated", "research/open"]);
  for (const [index, family] of families.entries()) {
    const location = `research-state inventory families[${index}]`;
    if (!exactKeys(family, familyKeys)) {
      errors.push(`${location} keys are invalid.`);
      continue;
    }
    if (!RESEARCH_STATE_FAMILY_IDS.includes(family.id)) errors.push(`${location}.id is unknown.`);
    if (!RESEARCH_STATE_CATEGORIES.includes(family.category)) errors.push(`${location}.category is outside the closed vocabulary.`);
    if (activeLookingCategories.has(family.category)) errors.push(`${location}.category cannot masquerade as active research.`);
    for (const field of ["decisionRefs", "successorDecisions", "implementationGaps"]) {
      if (!Array.isArray(family[field]) || family[field].some((value) => typeof value !== "string" || !/^W-[0-9]{3,4}$/u.test(value))) {
        errors.push(`${location}.${field} must contain W decision IDs.`);
      }
    }
    const successorDecisions = Array.isArray(family.successorDecisions) ? family.successorDecisions : [];
    const implementationGaps = Array.isArray(family.implementationGaps) ? family.implementationGaps : [];
    if (family.category !== "rejected" && successorDecisions.length === 0 && implementationGaps.length === 0) {
      errors.push(`${location} must name a successor decision or implementation gap.`);
    }
    if (typeof family.normalizationPending !== "boolean") errors.push(`${location}.normalizationPending must be boolean.`);
    if (!Array.isArray(family.artifacts) || family.artifacts.length === 0) errors.push(`${location}.artifacts must be non-empty.`);
    for (const artifact of family.artifacts ?? []) {
      const file = resolveInside(artifact);
      if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) errors.push(`${location} artifact path is missing or escapes: ${artifact}.`);
    }
  }
  return errors;
}

export function researchStateInventoryFacts(inventory = loadResearchStateInventory()) {
  const errors = validateResearchStateInventory(inventory);
  const families = Array.isArray(inventory?.families) ? inventory.families : [];
  const categoryCounts = Object.fromEntries(RESEARCH_STATE_CATEGORIES.map((category) => [category, families.filter((family) => family?.category === category).length]));
  const normalizationPendingCount = families.filter((family) => family?.normalizationPending === true).length;
  return {
    valid: errors.length === 0,
    active: Array.isArray(inventory?.active) ? inventory.active : [],
    status: inventory?.status ?? null,
    normalized: errors.length === 0 && normalizationPendingCount === 0 && inventory?.status === "authoritative-maintained-surface",
    familyCount: families.length,
    categoryCounts,
    normalizationPendingCount,
  };
}

function readJsonl(relativePath, baseDirectory = repositoryRoot) {
  const file = resolveInside(relativePath, baseDirectory);
  if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) throw new Error(`missing:${relativePath}`);
  return fs.readFileSync(file, "utf8").split(/\r?\n/).filter(Boolean).map((line) => JSON.parse(line));
}

function symbolCount(file, symbol) {
  if (!file || typeof symbol !== "string" || symbol.length === 0) return 0;
  return fs.readFileSync(file, "utf8").split(symbol).length - 1;
}

function sourceReferenceValid(reference) {
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
  return Object.entries(value).some(([key, child]) => FORBIDDEN_CASE_KEYS.has(key) || forbiddenKey(child));
}

function frontendFacts(state) {
  const corpus = state.frontend.corpus;
  const snapshot = state.frontend.snapshot;
  const families = Array.isArray(corpus.families) ? corpus.families : [];
  const familyIds = families.map((family) => family?.family);
  const expectedFamilies = ["G0", "G1", "G2", "G3", "G4", "G5"];
  const sourceDigests = families.map((family) => family?.sourceRef?.digest);
  const snapshotIds = snapshot.map((record) => record?.family);
  const sourceRefs = families.map((family) => {
    const file = resolveInside(family?.sourceRef?.path);
    return sourceReferenceValid(family?.sourceRef) && file && symbolCount(file, family.sourceRef.symbol) === 1;
  });
  const snapshotCoherent = families.every((family) => {
    const record = snapshot.find((candidate) => candidate?.family === family?.family);
    return record?.source?.digest === family?.sourceRef?.digest &&
      Array.isArray(record?.decisions) && record.decisions.every((decision) => /^W-[0-9]{3,4}$/u.test(decision));
  });
  const parserBoundary = corpus.status === "design-oracle-input" &&
    corpus.$schema === "w-frontend-freeze-cases-1" &&
    snapshot.every((record) => record?.schemaVersion === 1);
  const valid = same(familyIds, expectedFamilies) &&
    sourceRefs.length === 6 && sourceRefs.every(Boolean) &&
    snapshot.length === 6 && same(snapshotIds, expectedFamilies) &&
    snapshotCoherent && parserBoundary;
  return {
    valid,
    familyIds,
    familyCount: families.length,
    decisionCount: new Set(families.flatMap((family) => family?.decisions ?? [])).size,
    sourceRefsValid: sourceRefs.every(Boolean),
    snapshotCoherent,
    parserBoundary,
    sourceDigests,
  };
}

function classificationFacts(state) {
  const classification = state.classification;
  const researchStateInventory = researchStateInventoryFacts(state.researchStateInventory);
  const entries = Array.isArray(classification.entries) ? classification.entries : [];
  const ids = entries.map((entry) => entry?.decisionId);
  const unique = new Set(ids);
  const complete = entries.length === ledgerIds.length &&
    unique.size === ledgerIds.length &&
    ledgerIds.every((decisionId) => unique.has(decisionId));
  const dispositions = entries.every((entry) =>
    typeof entry?.decisionId === "string" &&
    typeof entry?.category === "string" &&
    object(entry?.authorityRef) &&
    Array.isArray(entry?.evidence) &&
    typeof entry?.reason === "string" && entry.reason.includes(entry.decisionId),
  );
  const historicalEntries = entries.filter((entry) => HISTORICAL_SNAPSHOT_IDS.includes(entry?.decisionId));
  const historicalComplete = historicalEntries.length === HISTORICAL_SNAPSHOT_IDS.length &&
    new Set(historicalEntries.map((entry) => entry?.decisionId)).size === HISTORICAL_SNAPSHOT_IDS.length;
  const researchResidual = historicalEntries.filter((entry) => entry?.category === "research-gated").map((entry) => entry.decisionId);
  const reopenedCategories = Object.fromEntries(PFU0_DECISIONS.map((decisionId) => [
    decisionId,
    entries.find((entry) => entry?.decisionId === decisionId)?.category ?? null,
  ]));
  const reopenedResearch = same(reopenedCategories, PFU0_DISPOSITIONS);
  const pfuSupersessionValid = Object.entries(PFU0_SUPERSESSIONS).every(([decisionId, successorId]) => {
    const supersessionEntry = entries.find((entry) => entry?.decisionId === decisionId);
    const successorEntry = entries.find((entry) => entry?.decisionId === successorId);
    return supersessionEntry?.authorityRef?.kind === "superseding-decision" &&
      supersessionEntry.authorityRef.decisionId === successorId &&
      supersessionEntry.supersessionClaim?.decisionId === successorId &&
      successorEntry?.category === "implementation-evidence-gap";
  });
  const globalResearch = entries.filter((entry) => entry?.category === "research-gated").map((entry) => entry.decisionId);
  const globalResearchExact = same([...globalResearch].sort(), [...ACTIVE_RESEARCH_GATES].sort());
  const designOnlyClosures = Object.fromEntries(Object.keys(DESIGN_ONLY_CLOSURES).map((decisionId) => [
    decisionId,
    entries.find((entry) => entry?.decisionId === decisionId)?.category ?? null,
  ]));
  const designOnlyClosuresValid = Object.entries(DESIGN_ONLY_CLOSURES).every(([decisionId, category]) =>
    designOnlyClosures[decisionId] === category,
  );
  const targetCategories = Object.fromEntries(DECISIONS.map((decision) => [decision, entries.find((entry) => entry?.decisionId === decision)?.category ?? null]));
  const ledgerDigestValid = classification.ledger?.path === "RATIONALE.md" &&
    classification.ledger?.count === ledgerIds.length &&
    classification.ledger?.first === ledgerIds[0] &&
    classification.ledger?.last === ledgerIds.at(-1) &&
    classification.ledger?.sha256 === digestFile(path.join(repositoryRoot, "RATIONALE.md"));
  const valid = complete && historicalComplete && dispositions && researchResidual.length === 0 && reopenedResearch && pfuSupersessionValid && globalResearchExact && designOnlyClosuresValid && ledgerDigestValid && researchStateInventory.valid &&
    DECISIONS.every((decision) => targetCategories[decision] === DISPOSITIONS[decision]);
  return {
    valid,
    entryCount: entries.length,
    uniqueDecisionCount: unique.size,
    complete,
    historicalComplete,
    historicalSnapshot: {
      first: HISTORICAL_SNAPSHOT_IDS[0],
      last: HISTORICAL_SNAPSHOT_IDS.at(-1),
      count: HISTORICAL_SNAPSHOT_IDS.length,
      researchZero: researchResidual.length === 0,
    },
    dispositions,
    researchResidual,
    reopenedCategories,
    reopenedResearch,
    pfuSupersessionValid,
    globalResearch,
    globalResearchExact,
    activeResearchGates: ACTIVE_RESEARCH_GATES,
    historicalPostSnapshotResearchGates: HISTORICAL_POST_SNAPSHOT_RESEARCH_GATES,
    designOnlyClosures,
    designOnlyClosuresValid,
    targetCategories,
    ledgerDigestValid,
    researchStateInventory,
  };
}

function hum0Facts(state) {
  const protocol = state.hum0.protocol;
  const study = state.hum0.study;
  const snapshot = state.hum0.snapshot;
  const protocolErrors = validateProtocol(protocol, { root: repositoryRoot });
  const humanRecords = protocol.records?.human;
  const modelRecords = protocol.records?.model;
  const readiness = snapshot?.readiness;
  const zeroRecords = Array.isArray(humanRecords) && Array.isArray(modelRecords) &&
    humanRecords.length === 0 && modelRecords.length === 0;
  const noAutomaticPromotion = protocol.promotionPolicy === "no-automatic-promotion" &&
    typeof protocol.stopCondition === "string" && /first/i.test(protocol.stopCondition) &&
    /Research/i.test(protocol.stopCondition);
  const readinessCoherent = snapshot?.status === "protocol-readiness-output" &&
    readiness?.sliceCount === 8 && readiness?.taskCount === 32 &&
    readiness?.humanRecordCount === 0 && readiness?.modelRecordCount === 0 &&
    readiness?.humanResultsClaimed === false && readiness?.modelResultsClaimed === false &&
    study?.status === "protocol-ready" && study?.bundle === false &&
    study?.records?.human === 0 && study?.records?.model === 0;
  const valid = protocolErrors.length === 0 && zeroRecords && noAutomaticPromotion && readinessCoherent;
  return {
    valid,
    protocolErrors,
    sliceCount: Array.isArray(protocol.slices) ? protocol.slices.length : 0,
    taskCount: Array.isArray(protocol.slices) ? protocol.slices.reduce((sum, slice) => sum + (slice.tasks?.length ?? 0), 0) : 0,
    humanRecordCount: Array.isArray(humanRecords) ? humanRecords.length : -1,
    modelRecordCount: Array.isArray(modelRecords) ? modelRecords.length : -1,
    noAutomaticPromotion,
    readinessCoherent,
  };
}

export function loadState({ root = repositoryRoot } = {}) {
  const rootPath = path.resolve(root);
  return {
    frontend: {
      corpus: readJson("tooling/frontend-freeze-cases.json", rootPath),
      snapshot: readJsonl("tooling/frontend-freeze.snapshot.jsonl", rootPath),
    },
    classification: readJson("tooling/design-freeze-classification.json", rootPath),
    researchStateInventory: loadResearchStateInventory({ root: rootPath }),
    hum0: {
      protocol: readJson("tooling/hum0-human-review-protocol.json", rootPath),
      study: readJson("tooling/studies/hum0-human-review/study.json", rootPath),
      snapshot: readJsonl("tooling/hum0-human-review-results.snapshot.jsonl", rootPath)[0],
    },
  };
}

function applyOperation(state, operation) {
  if (!object(operation) || typeof operation.kind !== "string") throw new Error("mutation operation is invalid");
  if (operation.kind === "remove-fz0-family") {
    state.frontend.corpus.families = state.frontend.corpus.families.filter((family) => family.family !== operation.value);
    return;
  }
  if (operation.kind === "remove-classification-decision") {
    state.classification.entries = state.classification.entries.filter((entry) => entry.decisionId !== operation.value);
    return;
  }
  if (operation.kind === "append-human-record") {
    state.hum0.protocol.records.human.push({ participantIdHash: "sha256:" + "0".repeat(64) });
    return;
  }
  if (operation.kind === "append-model-record") {
    state.hum0.protocol.records.model.push({ provider: "forged" });
    return;
  }
  if (operation.kind === "add-preference") {
    state.hum0.protocol.preference = "manual";
    state.hum0.protocol.score = 1;
    return;
  }
  throw new Error(`unknown mutation ${operation.kind}`);
}

function applyMutation(state, mutation) {
  const result = clone(state);
  if (mutation === null) return result;
  if (!object(mutation) || !Array.isArray(mutation.operations) || mutation.operations.length === 0) throw new Error("mutation must contain operations");
  for (const operation of mutation.operations) applyOperation(result, operation);
  return result;
}

function deriveGate(decision, state) {
  if (decision === "W-707") return frontendFacts(state);
  if (decision === "W-731") return classificationFacts(state);
  if (decision === "W-1408") return hum0Facts(state);
  throw new Error(`unknown decision ${decision}`);
}

export function runFRC0Case(testCase, { state = loadState() } = {}) {
  const decision = testCase?.decisions?.[0];
  const mutatedState = applyMutation(state, testCase?.mutation ?? null);
  const facts = deriveGate(decision, mutatedState);
  const status = facts.valid ? "accepted" : "rejected";
  return {
    caseId: testCase?.id ?? "unknown",
    decision,
    gate: GATES[decision]?.id ?? null,
    status,
    evidenceState: "design-oracle-input",
    hostOnly: true,
    implementationClaimed: false,
    humanResultsClaimed: false,
    modelResultsClaimed: false,
    facts,
    digest: digestValue({ decision, status, facts }),
  };
}

export function validateCase(testCase, { state = loadState() } = {}) {
  const errors = [];
  if (!object(testCase) || !exactKeys(testCase, CASE_KEYS)) return ["case keys are invalid."];
  if (!/^FRC0-W-(707|731|1408)-(current|adversarial)$/u.test(testCase.id ?? "")) errors.push(`${testCase.id}: invalid case id.`);
  if (!["current-contract", "rejected-route"].includes(testCase.kind)) errors.push(`${testCase.id}: invalid case kind.`);
  if (!Array.isArray(testCase.decisions) || testCase.decisions.length !== 1 || !DECISIONS.includes(testCase.decisions[0])) errors.push(`${testCase.id}: exactly one target decision is required.`);
  const decision = testCase.decisions?.[0];
  if (decision && !testCase.id.startsWith(`FRC0-${decision}-`)) errors.push(`${testCase.id}: case id and decision differ.`);
  if (decision && testCase.gate !== GATES[decision]?.id) errors.push(`${testCase.id}: gate does not match decision.`);
  if (forbiddenKey(testCase)) errors.push(`${testCase.id}: caller-owned expected/result/metric field is forbidden.`);
  if (testCase.kind === "current-contract" && testCase.mutation !== null) errors.push(`${testCase.id}: current contract cannot carry a mutation.`);
  if (testCase.kind === "rejected-route" && (!object(testCase.mutation) || !Array.isArray(testCase.mutation.operations) || testCase.mutation.operations.length === 0)) errors.push(`${testCase.id}: rejected route requires a mutation.`);
  try {
    const result = runFRC0Case(testCase, { state });
    const expectedStatus = testCase.kind === "current-contract" ? "accepted" : "rejected";
    if (result.status !== expectedStatus) errors.push(`${testCase.id}: derived ${result.status} does not match route kind.`);
  } catch (error) {
    errors.push(`${testCase.id}: ${error instanceof Error ? error.message : "derivation failed"}.`);
  }
  return errors;
}

export function validateCorpus(input = readJson("tooling/final-research-closure-cases.json")) {
  const errors = [];
  if (!exactKeys(input, TOP_LEVEL_KEYS)) errors.push("FRC0 corpus keys are invalid.");
  if (input.$schema !== "w-final-research-closure-cases-1") errors.push("FRC0 corpus schema is invalid.");
  if (input.status !== "design-oracle-input") errors.push("FRC0 corpus status must be design-oracle-input.");
  if (input.id !== "FRC0" || input.reuseOnly !== true) errors.push("FRC0 corpus must be reuse-only.");
  if (!same(input.decisions, DECISIONS)) errors.push("FRC0 decisions must contain W-707, W-731, and W-1408 in order.");
  if (!same(input.dispositions, DISPOSITIONS)) errors.push("FRC0 dispositions must keep all three decisions oracle-backed-current.");
  if (!exactKeys(input.evidence, ["current", "missing", "hostOnly"]) || input.evidence.hostOnly !== true) errors.push("FRC0 evidence boundary is invalid.");
  if (!same(input.evidence?.current, CURRENT_EVIDENCE)) errors.push("FRC0 evidence.current changed.");
  if (!same(input.evidence?.missing, MISSING_EVIDENCE)) errors.push("FRC0 evidence.missing changed.");
  if (!exactKeys(input.historicalSnapshot, ["first", "last", "count", "researchZero"]) ||
      input.historicalSnapshot.first !== HISTORICAL_SNAPSHOT_IDS[0] ||
      input.historicalSnapshot.last !== HISTORICAL_SNAPSHOT_LAST ||
      input.historicalSnapshot.count !== HISTORICAL_SNAPSHOT_IDS.length ||
      input.historicalSnapshot.researchZero !== true) {
    errors.push("FRC0 historical snapshot must close only W-001 through W-1450 with Research=0.");
  }
  if (!same(input.historicalPostSnapshotResearchGates, HISTORICAL_POST_SNAPSHOT_RESEARCH_GATES)) {
    errors.push("FRC0 must preserve W-1486/W-1503 as historical post-snapshot gates.");
  }
  if (!same(input.activeResearchGates, ACTIVE_RESEARCH_GATES)) {
    errors.push("FRC0 activeResearchGates must be an empty current residual after W-1517/W-1518.");
  }
  if (!same(input.designOnlyClosures, DESIGN_ONLY_CLOSURES)) {
    errors.push("FRC0 designOnlyClosures must list W-1517 and W-1518 without implementation claims.");
  }
  if (!exactKeys(input.reopenedResearch, ["decisions", "dispositions", "gate"]) ||
      !same(input.reopenedResearch.decisions, PFU0_DECISIONS) ||
      !same(input.reopenedResearch.dispositions, PFU0_DISPOSITIONS) ||
      input.reopenedResearch.gate !== "PFU0-pre-freeze-usability") {
    errors.push("FRC0 reopenedResearch must preserve the exact PFU0 dispositions and the W-1452/W-1453 supersessions.");
  }
  if (!exactKeys(input.reuse, DECISIONS)) errors.push("FRC0 reuse map must cover each decision exactly once.");
  for (const decision of DECISIONS) {
    if (!same(input.reuse?.[decision], REUSE[decision])) errors.push(`${decision}: reuse corpus changed.`);
    for (const relativePath of input.reuse?.[decision] ?? []) {
      const file = resolveInside(relativePath);
      if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) errors.push(`${decision}: reuse path escapes or is missing: ${relativePath}.`);
    }
  }
  let state;
  try {
    state = loadState();
    errors.push(...validateResearchStateInventory(state.researchStateInventory));
  } catch (error) {
    errors.push(`FRC0 source state cannot load: ${error instanceof Error ? error.message : "unknown"}.`);
  }
  if (!Array.isArray(input.cases) || input.cases.length !== 6) errors.push("FRC0 requires exactly six cases.");
  const ids = new Set();
  const counts = new Map(DECISIONS.map((decision) => [decision, { current: 0, adversarial: 0 }]));
  const results = [];
  for (const testCase of input.cases ?? []) {
    if (ids.has(testCase?.id)) errors.push(`${testCase?.id}: duplicate case id.`);
    ids.add(testCase?.id);
    const decision = testCase?.decisions?.[0];
    const count = counts.get(decision);
    if (count) count[testCase.kind === "current-contract" ? "current" : "adversarial"] += 1;
    if (state) {
      errors.push(...validateCase(testCase, { state }));
      try { results.push(runFRC0Case(testCase, { state })); } catch { /* validation reports derivation failure */ }
    }
  }
  for (const decision of DECISIONS) {
    const count = counts.get(decision);
    if (count.current !== 1 || count.adversarial !== 1) errors.push(`${decision}: requires one current and one adversarial case.`);
  }
  return { errors, results };
}

export function validateManifest(manifest = readJson("tooling/studies/final-research-closure/manifest.json")) {
  const errors = [];
  if (!object(manifest)) return ["manifest must be an object."];
  if (!exactKeys(manifest, MANIFEST_KEYS)) errors.push("manifest keys are invalid.");
  if (manifest.$schema !== "w-final-research-closure-manifest-1") errors.push("manifest schema is invalid.");
  if (manifest.status !== "design-oracle-input" || manifest.id !== "FRC0" || manifest.reuseOnly !== true) errors.push("manifest status/id/reuseOnly is invalid.");
  if (!same(manifest.decisions, DECISIONS)) errors.push("manifest decisions are invalid.");
  const artifacts = manifest.artifacts;
  if (!Array.isArray(artifacts) || artifacts.length !== Object.keys(MANIFEST_ARTIFACTS).length) errors.push("manifest artifact count is invalid.");
  const ids = new Set();
  const roles = new Set();
  const paths = new Set();
  for (const [index, artifact] of (artifacts ?? []).entries()) {
    const location = `manifest.artifacts[${index}]`;
    if (!object(artifact) || !exactKeys(artifact, ["id", "role", "path", "digest"])) {
      errors.push(`${location} keys are invalid.`);
      continue;
    }
    if (ids.has(artifact.id)) errors.push(`${location} duplicate id.`);
    if (roles.has(artifact.role)) errors.push(`${location} duplicate role.`);
    if (paths.has(artifact.path)) errors.push(`${location} duplicate path.`);
    ids.add(artifact.id); roles.add(artifact.role); paths.add(artifact.path);
    if (MANIFEST_ARTIFACTS[artifact.role] !== artifact.path) errors.push(`${location} role/path is invalid.`);
    const file = resolveInside(artifact.path);
    if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) errors.push(`${location}.path escapes or is missing.`);
    else if (!/^sha256:[0-9a-f]{64}$/u.test(artifact.digest ?? "") || digestFile(file) !== artifact.digest) errors.push(`${location}.digest is stale or invalid.`);
  }
  for (const [role, expectedPath] of Object.entries(MANIFEST_ARTIFACTS)) {
    const artifact = (artifacts ?? []).find((candidate) => candidate?.role === role);
    if (!artifact) errors.push(`manifest is missing role ${role}.`);
    else if (artifact.path !== expectedPath) errors.push(`manifest role ${role} path is invalid.`);
  }
  if (!exactKeys(manifest.evidence, ["current", "missing", "hostOnly"]) || manifest.evidence.hostOnly !== true ||
      !same(manifest.evidence.current, CURRENT_EVIDENCE) || !same(manifest.evidence.missing, MISSING_EVIDENCE)) errors.push("manifest evidence boundary is invalid.");
  const bundle = readJson("tooling/studies/final-research-closure/bundle.json");
  const study = readJson("tooling/studies/final-research-closure/study.json");
  if (study.manifest?.path !== "manifest.json") errors.push("study manifest reference must be path-only.");
  const bundleArtifact = (artifacts ?? []).find((artifact) => artifact?.role === "bundle");
  if (bundleArtifact && digestFile(bundlePath) !== bundleArtifact.digest) errors.push("manifest bundle digest is stale.");
  const corpusArtifact = (artifacts ?? []).find((artifact) => artifact?.role === "corpus");
  if (corpusArtifact && digestFile(corpusPath) !== corpusArtifact.digest) errors.push("manifest corpus digest is stale.");
  if (bundle.$schema !== "w-substitution-study-bundle-1" || bundle.status !== "design-oracle-input") errors.push("bundle chain is invalid.");
  return errors;
}

function validateBundle(bundle = readJson("tooling/studies/final-research-closure/bundle.json")) {
  const errors = [];
  if (!object(bundle)) return ["bundle must be an object."];
  if (bundle.$schema !== "w-substitution-study-bundle-1") errors.push("bundle schema is invalid.");
  if (bundle.status !== "design-oracle-input" || bundle.entry !== "finalResearchClosure") errors.push("bundle status or entry is invalid.");
  const base = path.dirname(bundlePath);
  const baseFile = resolveInside(bundle.sourceBase?.path, base);
  if (!baseFile || !fs.existsSync(baseFile) || digestFile(baseFile) !== bundle.sourceBase?.digest || symbolCount(baseFile, bundle.sourceBase?.symbol) !== 1) errors.push("bundle sourceBase chain is invalid.");
  if (!Array.isArray(bundle.sourceRefs) || bundle.sourceRefs.length < 4) errors.push("bundle sourceRefs are incomplete.");
  const sourceKeys = new Set();
  for (const [index, reference] of (bundle.sourceRefs ?? []).entries()) {
    const file = resolveInside(reference?.path, base);
    const key = `${reference?.path}\0${reference?.symbol}`;
    if (!file || !fs.existsSync(file) || !/^sha256:[0-9a-f]{64}$/u.test(reference?.digest ?? "") || digestFile(file) !== reference.digest || symbolCount(file, reference.symbol) !== 1) errors.push(`bundle sourceRef[${index}] is stale or invalid.`);
    if (sourceKeys.has(key)) errors.push(`bundle sourceRef[${index}] is duplicated.`);
    sourceKeys.add(key);
  }
  if (!Array.isArray(bundle.variants) || bundle.variants.length !== 2) errors.push("bundle variants must contain current and adversarial.");
  const variantIds = new Set((bundle.variants ?? []).map((variant) => variant?.id));
  if (!same([...variantIds].sort(), ["adversarial", "current"])) errors.push("bundle variant IDs are invalid.");
  const variantDigests = new Set();
  for (const variant of bundle.variants ?? []) {
    const file = resolveInside(variant?.path, base);
    if (!file || !fs.existsSync(file) || variant?.language !== "w" || variant?.parseEvidence?.status !== "tree-sitter-parse" || digestFile(file) !== variant?.digest || !fs.readFileSync(file, "utf8").includes(bundle.entry)) errors.push(`bundle variant ${variant?.id} is invalid.`);
    variantDigests.add(variant?.digest);
  }
  if (variantDigests.size !== 2) errors.push("bundle variants must have distinct digests.");
  if (!same((bundle.tasks ?? []).map((task) => task.kind), ["explain", "recall", "repair", "change"])) errors.push("bundle task kinds are invalid.");
  if (!same(bundle.presentationOrders, [["current", "adversarial"], ["adversarial", "current"]])) errors.push("bundle presentation orders are invalid.");
  if (!same(bundle.blinding?.participantLabels, { current: "A", adversarial: "B" })) errors.push("bundle participant labels are invalid.");
  if (!same(bundle.blinding?.hide, ["id", "role", "path", "changedConstructs"])) errors.push("bundle blinding fields are invalid.");
  const oracle = resolveInside(bundle.oracle?.path, base);
  if (!oracle || !fs.existsSync(oracle) || digestFile(oracle) !== bundle.oracle?.digest) errors.push("bundle oracle chain is invalid.");
  if (!same(bundle.evidence?.missing, MISSING_EVIDENCE) || !(bundle.evidence?.current ?? []).includes("host-oracle")) errors.push("bundle evidence boundary is invalid.");
  return errors;
}

export function mutationChecks() {
  const checks = {};
  const corpus = readJson("tooling/final-research-closure-cases.json");
  const manifest = readJson("tooling/studies/final-research-closure/manifest.json");
  const bundle = readJson("tooling/studies/final-research-closure/bundle.json");
  const state = loadState();
  const staleDigest = clone(manifest);
  staleDigest.artifacts.find((artifact) => artifact.role === "machine").digest = `sha256:${"0".repeat(64)}`;
  checks.staleDigestRejected = validateManifest(staleDigest).some((error) => error.includes("digest"));

  const callerEcho = clone(corpus);
  callerEcho.cases[0].expected = { status: "accepted" };
  checks.callerEchoRejected = validateCorpus(callerEcho).errors.some((error) => error.includes("caller-owned") || error.includes("keys"));

  const manualCount = clone(corpus);
  manualCount.cases[0].manualCount = 6;
  checks.manualCountRejected = validateCorpus(manualCount).errors.some((error) => error.includes("caller-owned") || error.includes("keys"));

  const forgedHuman = clone(state);
  forgedHuman.hum0.protocol.records.human.push({ participantIdHash: `sha256:${"0".repeat(64)}` });
  checks.forgedHumanRecordRejected = hum0Facts(forgedHuman).valid === false;
  const forgedModel = clone(state);
  forgedModel.hum0.protocol.records.model.push({ provider: "forged" });
  checks.forgedModelRecordRejected = hum0Facts(forgedModel).valid === false;

  const preference = clone(state);
  preference.hum0.protocol.preference = "manual";
  preference.hum0.protocol.score = 1;
  checks.preferenceScoreRejected = hum0Facts(preference).valid === false;

  const missingDecision = clone(corpus);
  missingDecision.decisions = missingDecision.decisions.slice(0, 2);
  checks.missingDecisionRejected = validateCorpus(missingDecision).errors.some((error) => error.includes("decisions"));
  const duplicateDecision = clone(corpus);
  duplicateDecision.decisions.push("W-707");
  checks.duplicateDecisionRejected = validateCorpus(duplicateDecision).errors.some((error) => error.includes("decisions"));
  const duplicateCase = clone(corpus);
  duplicateCase.cases[1].id = duplicateCase.cases[0].id;
  checks.duplicateCaseRejected = validateCorpus(duplicateCase).errors.some((error) => error.includes("duplicate case id"));

  const escapedSource = clone(manifest);
  escapedSource.artifacts.find((artifact) => artifact.role === "classification").path = "../../outside.json";
  checks.sourceEscapeRejected = validateManifest(escapedSource).some((error) => error.includes("escapes") || error.includes("role/path"));

  const wrongCategory = clone(corpus);
  wrongCategory.dispositions["W-731"] = "research-gated";
  checks.wrongCategoryRejected = validateCorpus(wrongCategory).errors.some((error) => error.includes("dispositions"));

  const researchResidual = clone(state);
  researchResidual.classification.entries.find((entry) => entry.decisionId === "W-1450").category = "research-gated";
  checks.researchResidualRejected = classificationFacts(researchResidual).valid === false;

  const reopenedCategory = clone(state);
  reopenedCategory.classification.entries.find((entry) => entry.decisionId === "W-1451").category = "research-gated";
  checks.reopenedResearchRejected = classificationFacts(reopenedCategory).valid === false;

  const extraResearch = clone(state);
  extraResearch.classification.entries.push({
    decisionId: "W-1454",
    category: "research-gated",
    authorityRef: {},
    evidence: [],
    reason: "W-1454 hidden research gate",
  });
  checks.extraResearchGateRejected = classificationFacts(extraResearch).valid === false;

  const reopenedClosedGate = clone(state);
  reopenedClosedGate.classification.entries.find((entry) => entry.decisionId === "W-1486").category = "research-gated";
  checks.closedPostSnapshotGateReopenedRejected = classificationFacts(reopenedClosedGate).valid === false;

  checks.activeResearchResidualExact = classificationFacts(state).globalResearchExact &&
    classificationFacts(state).globalResearch.length === 0;

  const injectedActiveState = clone(state.researchStateInventory);
  injectedActiveState.active = ["W-9999"];
  checks.researchInventoryInjectedActiveRejected = !researchStateInventoryFacts(injectedActiveState).valid;

  const omittedFamily = clone(state.researchStateInventory);
  omittedFamily.families.pop();
  checks.researchInventoryOmittedFamilyRejected = !researchStateInventoryFacts(omittedFamily).valid;

  const duplicateFamily = clone(state.researchStateInventory);
  duplicateFamily.families.push(clone(duplicateFamily.families[0]));
  checks.researchInventoryDuplicateFamilyRejected = !researchStateInventoryFacts(duplicateFamily).valid;

  const invalidCategory = clone(state.researchStateInventory);
  invalidCategory.families[0].category = "research-gated";
  checks.researchInventoryInvalidCategoryRejected = !researchStateInventoryFacts(invalidCategory).valid;

  const missingSuccessor = clone(state.researchStateInventory);
  const successorFamily = missingSuccessor.families.find((family) => family.id === "w1504");
  successorFamily.successorDecisions = [];
  successorFamily.implementationGaps = [];
  checks.researchInventoryMissingSuccessorRejected = !researchStateInventoryFacts(missingSuccessor).valid;

  const missingCase = clone(corpus);
  missingCase.cases.pop();
  checks.missingCaseRejected = validateCorpus(missingCase).errors.some((error) => error.includes("exactly six"));
  return checks;
}

export function projectResults(results, mutations) {
  return [
    ...results.map((result) => ({
      caseId: result.caseId,
      decision: result.decision,
      gate: result.gate,
      status: result.status,
      evidenceState: result.evidenceState,
      hostOnly: result.hostOnly,
      implementationClaimed: result.implementationClaimed,
      humanResultsClaimed: result.humanResultsClaimed,
      modelResultsClaimed: result.modelResultsClaimed,
      factsDigest: digestValue(result.facts),
    })),
    { kind: "integrity-mutations", checks: mutations },
  ];
}

export { CASE_KEYS, MANIFEST_KEYS, TOP_LEVEL_KEYS, validateBundle };
