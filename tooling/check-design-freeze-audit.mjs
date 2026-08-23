import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { deriveSemanticRulePairs } from "./semantic-diagnostic-pairs.mjs";
import { ledgerIds, ledgerIdSet, rationaleText } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const classificationPath = process.env.W_DESIGN_FREEZE_CLASSIFICATION
  ? path.resolve(process.env.W_DESIGN_FREEZE_CLASSIFICATION)
  : path.join(toolingDirectory, "design-freeze-classification.json");
const classification = JSON.parse(fs.readFileSync(classificationPath, "utf8"));
const errors = [];
const categories = new Set([
  "source-backed-current",
  "oracle-backed-current",
  "research-gated",
  "implementation-evidence-gap",
  "superseded",
  "rejected",
]);
const authorityKinds = new Set([
  "source-case",
  "oracle-case",
  "design-contract",
  "design-freeze-gate",
  "design-absence",
  "superseding-decision",
]);
const evidenceKinds = new Set(["source", "oracle"]);

function fail(message) {
  errors.push(message);
}

function nonEmptyString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    fail(`${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

function digest(filePath) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(filePath)).digest("hex")}`;
}

function textDigest(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

// DESIGN.md is the only normative authority for implementation-evidence gaps.
// Resolve a section from its current heading and digest the exact slice so a
// broad or stale gate cannot masquerade as component evidence.
const designPath = path.join(wDirectory, "DESIGN.md");
const designText = fs.readFileSync(designPath, "utf8");
const designLines = designText.split(/\r?\n/);
const designDigest = digest(designPath);
const designHeadings = [];
for (const [index, line] of designLines.entries()) {
  const match = /^(#{2,6})\s+(.+?)\s*$/.exec(line);
  if (!match) continue;
  const sectionMatch = /^(\d+(?:\.\d+)*)\b/.exec(match[2]);
  if (!sectionMatch) continue;
  designHeadings.push({
    section: sectionMatch[1],
    heading: line,
    level: match[1].length,
    start: index,
  });
}
function designSectionInfo(section) {
  const record = designHeadings.find((candidate) => candidate.section === section);
  if (!record) return null;
  const next = designHeadings.find((candidate) => candidate.start > record.start && candidate.level <= record.level);
  const end = next?.start ?? designLines.length;
  return {
    section,
    heading: record.heading,
    sectionDigest: textDigest(designLines.slice(record.start, end).join("\n")),
  };
}
function designGate(ref) {
  return `DESIGN.md §${ref.section} / ${ref.heading.replace(/^#+\s+/, "").trim()}`;
}

function resolveRepositoryPath(value, location) {
  if (!nonEmptyString(value, `${location}.path`)) return null;
  if (path.isAbsolute(value)) {
    fail(`${location}.path must be repository-relative.`);
    return null;
  }
  const resolved = path.resolve(wDirectory, value);
  const relative = path.relative(wDirectory, resolved);
  if (relative.startsWith("..") || path.isAbsolute(relative)) {
    fail(`${location}.path escapes the repository.`);
    return null;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    fail(`${location}.path does not exist: ${value}.`);
    return null;
  }
  if (value.replaceAll("\\", "/").includes("history/")) {
    fail(`${location}.path must not use history as current authority.`);
  }
  return resolved;
}

function validateFileRef(ref, location) {
  if (!ref || typeof ref !== "object") {
    fail(`${location} must be an object.`);
    return null;
  }
  const resolved = resolveRepositoryPath(ref.path, location);
  if (!nonEmptyString(ref.sha256, `${location}.sha256`)) return resolved;
  if (resolved && ref.sha256 !== digest(resolved)) {
    fail(`${location}.sha256 is stale for ${ref.path}.`);
  }
  return resolved;
}

const rowsById = new Map();
for (const line of rationaleText.slice(rationaleText.indexOf("## 3. Ledger")).split("\n")) {
  if (!line.startsWith("| W-")) continue;
  const body = line.slice(2);
  const first = body.indexOf("|");
  const second = body.indexOf("|", first + 1);
  const trailing = body.lastIndexOf("|");
  const claimEnd = body.lastIndexOf("|", trailing - 1);
  rowsById.set(body.slice(0, first).trim(), {
    theme: body.slice(first + 1, second).trim(),
    claim: body.slice(second + 1, claimEnd).trim(),
  });
}
const legacySubstitutionCases = new Map();
const substitutions = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "substitution-cases.json"), "utf8"));
for (const testCase of substitutions.cases ?? []) legacySubstitutionCases.set(testCase.id, testCase);

const corpusNames = fs.readdirSync(toolingDirectory)
  .filter((name) => name.endsWith("-cases.json") && name !== "substitution-cases.json");
const oracleCases = new Map();
for (const name of corpusNames) {
  const corpusPath = path.join(toolingDirectory, name);
  const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
  for (const [index, testCase] of (corpus.cases ?? []).entries()) {
    const caseId = testCase.id ?? `${name}#${index + 1}`;
    oracleCases.set(caseId, { name, corpusPath, testCase, caseIndex: index });
  }
}

function validateSourceRef(sourceRef, location) {
  if (!sourceRef || typeof sourceRef !== "object") {
    fail(`${location} must be an object.`);
    return;
  }
  const resolved = validateFileRef(sourceRef, location);
  if (!nonEmptyString(sourceRef.symbol, `${location}.symbol`)) return;
  if (resolved && sourceRef.symbol.length > 200) fail(`${location}.symbol is too long.`);
}

function validateDecisionBridge(bridge, location, decisionId, citedDecisionIds) {
  if (!bridge || typeof bridge !== "object") {
    fail(`${location} must be an object.`);
    return false;
  }
  if (!nonEmptyString(bridge.decisionId, `${location}.decisionId`)) return false;
  if (bridge.decisionId === decisionId) {
    fail(`${location}.decisionId must bridge to a distinct ledger decision.`);
  }
  if (!(citedDecisionIds ?? []).includes(bridge.decisionId)) {
    fail(`${location}.decisionId must be cited by the oracle case.`);
  }
  const row = rowsById.get(bridge.decisionId);
  if (!row) {
    fail(`${location}.decisionId is not in the current ledger.`);
  } else {
    const expected = textDigest(row.claim);
    if (bridge.claimDigest !== expected) fail(`${location}.claimDigest is stale for ${bridge.decisionId}.`);
  }
  if (!nonEmptyString(bridge.relation, `${location}.relation`)) return false;
  if (!bridge.relation.includes(decisionId) || !bridge.relation.includes(bridge.decisionId)) {
    fail(`${location}.relation must name both ${decisionId} and ${bridge.decisionId}.`);
  }
  return true;
}

function validateEvidence(ref, location, decisionId) {
  if (!ref || typeof ref !== "object") {
    fail(`${location} must be an object.`);
    return;
  }
  if (!evidenceKinds.has(ref.kind)) fail(`${location}.kind must be source or oracle.`);
  validateFileRef(ref, location);
  if (!nonEmptyString(ref.caseId, `${location}.caseId`)) return;
  if (ref.kind === "source") {
    if (!legacySubstitutionCases.has(ref.caseId)) fail(`${location}.caseId is not a substitution case.`);
    const testCase = legacySubstitutionCases.get(ref.caseId);
    if (!(testCase.decisions ?? []).includes(decisionId)) fail(`${location}.caseId does not cite ${decisionId}.`);
  } else {
    const oracle = oracleCases.get(ref.caseId);
    if (!oracle) {
      fail(`${location}.caseId is not a corpus case.`);
    } else {
      const decisions = oracle.testCase.decisions ?? oracle.testCase.decisionIds ??
        (oracle.testCase.rule !== undefined ? [oracle.testCase.rule] : undefined) ??
        (["module-run-cases.json", "repl-session-cases.json"].includes(oracle.name)
          ? JSON.parse(fs.readFileSync(oracle.corpusPath, "utf8")).decisions
          : undefined);
      const citedDecisionIds = decisions ?? [];
      if (!citedDecisionIds.includes(decisionId)) {
        if (!ref.decisionBridge || !validateDecisionBridge(ref.decisionBridge, `${location}.decisionBridge`, decisionId, citedDecisionIds)) {
          fail(`${location}.caseId does not cite ${decisionId}.`);
        }
      } else if (ref.decisionBridge) {
        validateDecisionBridge(ref.decisionBridge, `${location}.decisionBridge`, decisionId, citedDecisionIds);
      }
      const expectedPath = `tooling/${oracle.name}`;
      if (ref.path.replaceAll("\\", "/") !== expectedPath) fail(`${location}.path does not match its case file.`);
      if (oracle.testCase.source && typeof oracle.testCase.source === "object" && !Array.isArray(oracle.testCase.source)) {
        if (oracle.testCase.source.path && oracle.testCase.source.symbol) {
          if (!ref.sourceRef) fail(`${location}.sourceRef is required for file-backed source.`);
          else validateSourceRef(ref.sourceRef, `${location}.sourceRef`);
        }
      }
    }
  }
  if (Object.prototype.hasOwnProperty.call(ref, "expected") || Object.prototype.hasOwnProperty.call(ref, "result")) {
    fail(`${location} must not echo expected or result fields.`);
  }
}

function validateAuthority(ref, location, entry) {
  if (!ref || typeof ref !== "object") {
    fail(`${location} must be an object.`);
    return;
  }
  if (!authorityKinds.has(ref.kind)) fail(`${location}.kind is unknown.`);
  validateFileRef(ref, location);
  if (ref.kind === "source-case" || ref.kind === "oracle-case") {
    if (!nonEmptyString(ref.caseId, `${location}.caseId`)) return;
    validateEvidence({ ...ref, kind: ref.kind === "source-case" ? "source" : "oracle" }, location, entry.decisionId);
  }
  if (ref.kind === "oracle-case" && ref.sourceRef) validateSourceRef(ref.sourceRef, `${location}.sourceRef`);
  if (ref.kind === "design-contract") {
    if (ref.path !== "DESIGN.md") fail(`${location}.path must point to DESIGN.md.`);
    if (!nonEmptyString(ref.section, `${location}.section`)) return;
    const current = designSectionInfo(ref.section);
    if (!current) {
      fail(`${location}.section does not identify a current DESIGN heading.`);
    } else {
      if (ref.heading !== current.heading) fail(`${location}.heading is stale for DESIGN.md §${ref.section}.`);
      if (ref.sectionDigest !== current.sectionDigest) fail(`${location}.sectionDigest is stale for DESIGN.md §${ref.section}.`);
    }
    if (ref.sha256 !== designDigest) fail(`${location}.sha256 is stale for DESIGN.md.`);
    if (ref.section === "24.4" && entry.gap?.component !== "design-freeze") {
      fail(`${location}.section 24.4 is reserved for a concrete design-freeze gate.`);
    }
  }
  if (ref.kind === "superseding-decision") {
    if (!nonEmptyString(ref.decisionId, `${location}.decisionId`)) return;
    if (ref.decisionId === entry.decisionId) fail(`${location}.decisionId self-references ${entry.decisionId}.`);
    if (!ledgerIdSet.has(ref.decisionId)) fail(`${location}.decisionId is not in the ledger.`);
    const sourceNumber = Number(entry.decisionId.slice(2));
    const successorNumber = Number(ref.decisionId.slice(2));
    if (!Number.isInteger(sourceNumber) || !Number.isInteger(successorNumber) || successorNumber <= sourceNumber) {
      fail(`${location}.decisionId must be a later decision than ${entry.decisionId}.`);
    }
    if (ref.path !== "RATIONALE.md" || ref.section !== "3. Ledger") {
      fail(`${location} must point to the current ledger section.`);
    }
  }
  if (ref.kind === "design-freeze-gate" || ref.kind === "design-absence") {
    if (ref.path !== "DESIGN.md") fail(`${location} must point to DESIGN.md.`);
    if (!["24.2", "24.4"].includes(ref.section)) fail(`${location}.section must be 24.2 or 24.4.`);
  }
}

if (classification.$schema !== "w-design-freeze-classification-1") fail("classification schema must be w-design-freeze-classification-1.");
if (classification.status !== "design-oracle-input") fail("classification status must be design-oracle-input.");
if (!classification.selectionPolicy || typeof classification.selectionPolicy !== "object" ||
    classification.selectionPolicy.kind !== "explicit-ledger-id") {
  fail("classification.selectionPolicy must require explicit-ledger-id selection.");
} else {
  if (!Array.isArray(classification.selectionPolicy.requiredFields) ||
      !classification.selectionPolicy.requiredFields.includes("decisionId") ||
      !classification.selectionPolicy.requiredFields.includes("category") ||
      !classification.selectionPolicy.requiredFields.includes("authorityRef")) {
    fail("classification.selectionPolicy.requiredFields must name the reviewed identity and authority fields.");
  }
  if (!Array.isArray(classification.selectionPolicy.forbiddenShortcuts) ||
      !["range", "epoch-default", "regex-default", "mass-default"].every((shortcut) =>
        classification.selectionPolicy.forbiddenShortcuts.includes(shortcut))) {
    fail("classification.selectionPolicy must forbid range, epoch-default, regex-default, and mass-default shortcuts.");
  }
}
if (!classification.ledger || classification.ledger.path !== "RATIONALE.md" || classification.ledger.section !== "3. Ledger") {
  fail("classification.ledger must point to RATIONALE.md §3 Ledger.");
} else {
  const rationalePath = path.join(wDirectory, "RATIONALE.md");
  if (classification.ledger.sha256 !== digest(rationalePath)) fail("classification ledger digest is stale.");
  if (classification.ledger.count !== ledgerIds.length) fail("classification ledger count is stale.");
  if (classification.ledger.first !== ledgerIds[0] || classification.ledger.last !== ledgerIds.at(-1)) fail("classification ledger bounds are stale.");
}
if (!Array.isArray(classification.categories) || classification.categories.length !== categories.size ||
    classification.categories.some((category) => !categories.has(category))) {
  fail("classification.categories must list exactly the six closed categories.");
}
if (!Array.isArray(classification.entries)) {
  fail("classification.entries must be an array.");
}

const entriesById = new Map();
for (const [index, entry] of (classification.entries ?? []).entries()) {
  const location = `entries[${index}]`;
  if (!entry || typeof entry !== "object") {
    fail(`${location} must be an object.`);
    continue;
  }
  if (!nonEmptyString(entry.decisionId, `${location}.decisionId`)) continue;
  if (entriesById.has(entry.decisionId)) fail(`${location}.decisionId duplicates ${entry.decisionId}.`);
  entriesById.set(entry.decisionId, entry);
  if (!ledgerIdSet.has(entry.decisionId)) fail(`${location}.decisionId is not in the current ledger.`);
  const row = rowsById.get(entry.decisionId);
  if (!row) continue;
  if (entry.summary !== row.theme) fail(`${location}.summary does not match ${entry.decisionId}.`);
  if (entry.canonicalClaim !== row.claim) fail(`${location}.canonicalClaim does not match ${entry.decisionId}.`);
  const expectedClaimDigest = `sha256:${crypto.createHash("sha256").update(row.claim).digest("hex")}`;
  if (entry.claimDigest !== expectedClaimDigest) fail(`${location}.claimDigest is stale for ${entry.decisionId}.`);
  if (!categories.has(entry.category)) fail(`${location}.category is not closed.`);
  if (!nonEmptyString(entry.basis, `${location}.basis`)) continue;
  const expectedBasis = {
    "source-backed-current": "linked-source-comparison-case",
    "oracle-backed-current": "linked-host-oracle-case",
    "research-gated": "explicit-research-or-evidence-gate",
    "implementation-evidence-gap": "current-ledger-claim-without-executable-witness",
    superseded: "explicit-ledger-supersession",
    rejected: "explicit-ledger-rejection",
  }[entry.category];
  if (expectedBasis && entry.basis !== expectedBasis) {
    fail(`${location}.basis does not match ${entry.category}.`);
  }
  if (entry.selection !== "explicit-ledger-id") {
    fail(`${location}.selection must be explicit-ledger-id; range/default selection is forbidden.`);
  }
  if (!entry.basisRef || typeof entry.basisRef !== "object") {
    fail(`${location}.basisRef must identify the ledger row.`);
  } else {
    validateFileRef(entry.basisRef, `${location}.basisRef`);
    if (entry.basisRef.kind !== "ledger-row") fail(`${location}.basisRef.kind must be ledger-row.`);
    if (entry.basisRef.path !== "RATIONALE.md" || entry.basisRef.section !== "3. Ledger") {
      fail(`${location}.basisRef must point to RATIONALE.md §3 Ledger.`);
    }
    if (entry.basisRef.decisionId !== entry.decisionId) {
      fail(`${location}.basisRef.decisionId must equal ${entry.decisionId}.`);
    }
    if (entry.basisRef.claimDigest !== entry.claimDigest) {
      fail(`${location}.basisRef.claimDigest must equal ${entry.decisionId}.claimDigest.`);
    }
  }
  validateAuthority(entry.authorityRef, `${location}.authorityRef`, entry);
  if (!Array.isArray(entry.evidence)) {
    fail(`${location}.evidence must be an array.`);
  } else {
    const refs = new Set();
    for (const [evidenceIndex, ref] of entry.evidence.entries()) {
      const refLocation = `${location}.evidence[${evidenceIndex}]`;
      validateEvidence(ref, refLocation, entry.decisionId);
      const key = `${ref.kind}:${ref.path}:${ref.caseId}`;
      if (refs.has(key)) fail(`${refLocation} duplicates ${key}.`);
      refs.add(key);
    }
  }
  if (!nonEmptyString(entry.reason, `${location}.reason`)) continue;
  if (!entry.reason.includes(entry.decisionId) || !entry.reason.includes(entry.summary) || !entry.reason.includes(entry.claimDigest)) {
    fail(`${location}.reason must identify its decision, summary, and claim digest.`);
  }
  const evidenceKindsForEntry = new Set((entry.evidence ?? []).map((ref) => ref.kind));
  if (entry.category === "source-backed-current" && !evidenceKindsForEntry.has("source")) {
    fail(`${location} needs source evidence for source-backed-current.`);
  }
  if (entry.category === "oracle-backed-current" && !evidenceKindsForEntry.has("oracle")) {
    fail(`${location} needs oracle evidence for oracle-backed-current.`);
  }
  if (entry.category === "source-backed-current" && entry.authorityRef?.kind !== "source-case") {
    fail(`${location}.authorityRef must be a source-case.`);
  }
  if (entry.category === "oracle-backed-current" && entry.authorityRef?.kind !== "oracle-case") {
    fail(`${location}.authorityRef must be an oracle-case.`);
  }
  if (entry.category === "rejected" && entry.authorityRef?.kind !== "design-absence") {
    fail(`${location}.authorityRef must be design-absence.`);
  }
  if (entry.category === "superseded" && entry.authorityRef?.kind !== "superseding-decision") {
    fail(`${location}.authorityRef must identify a later superseding decision.`);
  }
  if (entry.category === "implementation-evidence-gap" && entry.authorityRef?.kind !== "design-contract") {
    fail(`${location}.authorityRef must identify the exact current DESIGN contract section.`);
  }
  if (entry.category === "superseded" && entry.authorityRef?.decisionId && entriesById.has(entry.authorityRef.decisionId)) {
    const successor = entriesById.get(entry.authorityRef.decisionId);
    if (["superseded", "rejected"].includes(successor.category)) {
      fail(`${location}.authorityRef.decisionId must point to a current decision, not ${successor.category}.`);
    }
  }
  if (entry.category === "implementation-evidence-gap") {
    if (!entry.gap || typeof entry.gap !== "object") {
      fail(`${location}.gap must name the concrete component and gate.`);
    } else {
      nonEmptyString(entry.gap.component, `${location}.gap.component`);
      nonEmptyString(entry.gap.gate, `${location}.gap.gate`);
      if (entry.authorityRef?.kind === "design-contract") {
        const expectedGate = designGate(entry.authorityRef);
        if (entry.gap.gate !== expectedGate) {
          fail(`${location}.gap.gate must equal its design-contract authority (${expectedGate}).`);
        }
      }
      if (!Array.isArray(entry.gap.missingEvidence) || entry.gap.missingEvidence.length === 0) {
        fail(`${location}.gap.missingEvidence must list concrete missing evidence.`);
      } else {
        for (const [missingIndex, missing] of entry.gap.missingEvidence.entries()) {
          nonEmptyString(missing, `${location}.gap.missingEvidence[${missingIndex}]`);
        }
      }
    }
    if (!nonEmptyString(entry.stopCondition, `${location}.stopCondition`)) continue;
    if (!entry.stopCondition.includes(entry.decisionId)) fail(`${location}.stopCondition must identify ${entry.decisionId}.`);
    if (entry.gap?.component && !entry.stopCondition.includes(entry.gap.component)) {
      fail(`${location}.stopCondition must identify component ${entry.gap.component}.`);
    }
    if (entry.gap?.gate && !entry.stopCondition.includes(entry.gap.gate)) {
      fail(`${location}.stopCondition must identify gate ${entry.gap.gate}.`);
    }
    for (const missing of entry.gap?.missingEvidence ?? []) {
      if (!entry.stopCondition.includes(missing)) fail(`${location}.stopCondition omits missing evidence ${missing}.`);
    }
    if (entry.gap?.component?.startsWith("ledger-contract-")) {
      fail(`${location}.gap.component must be a concrete subsystem, not a per-ID fallback.`);
    }
    if (entry.reason && entry.gap?.component && !entry.reason.includes(entry.gap.component)) {
      fail(`${location}.reason must identify component ${entry.gap.component}.`);
    }
    if (entry.reason && entry.gap?.gate && !entry.reason.includes(entry.gap.gate)) {
      fail(`${location}.reason must identify gate ${entry.gap.gate}.`);
    }
  } else if (entry.gap !== undefined) {
    fail(`${location}.gap is only valid for implementation-evidence-gap.`);
  }
  if (entry.category === "research-gated") {
    if (!entry.researchGate || typeof entry.researchGate !== "object") {
      fail(`${location}.researchGate must name the concrete research gate.`);
    } else {
      nonEmptyString(entry.researchGate.id, `${location}.researchGate.id`);
      nonEmptyString(entry.researchGate.evidenceState, `${location}.researchGate.evidenceState`);
      nonEmptyString(entry.researchGate.stopCondition, `${location}.researchGate.stopCondition`);
      if (!entry.researchGate.stopCondition.includes(entry.decisionId)) {
        fail(`${location}.researchGate.stopCondition must identify ${entry.decisionId}.`);
      }
      if (!entry.researchGate.stopCondition.includes(entry.researchGate.id)) {
        fail(`${location}.researchGate.stopCondition must identify gate ${entry.researchGate.id}.`);
      }
      if (!entry.researchGate.stopCondition.includes("independent case") ||
          !entry.researchGate.stopCondition.includes("fresh digest") ||
          !entry.researchGate.stopCondition.includes("reviewed promotion decision")) {
        fail(`${location}.researchGate.stopCondition must require independent case, fresh digest, and reviewed promotion.`);
      }
    }
    if (!nonEmptyString(entry.stopCondition, `${location}.stopCondition`)) continue;
    if (!entry.stopCondition.includes(entry.decisionId)) fail(`${location}.stopCondition must identify ${entry.decisionId}.`);
    if (entry.researchGate?.id && !entry.stopCondition.includes(entry.researchGate.id)) {
      fail(`${location}.stopCondition must identify gate ${entry.researchGate.id}.`);
    }
    if (entry.reason && entry.researchGate?.id && !entry.reason.includes(entry.researchGate.id)) {
      fail(`${location}.reason must identify gate ${entry.researchGate.id}.`);
    }
  } else if (entry.researchGate !== undefined) {
    fail(`${location}.researchGate is only valid for research-gated.`);
  }
  if (["source-backed-current", "oracle-backed-current"].includes(entry.category) && entry.researchExtension !== undefined) {
    const extension = entry.researchExtension;
    const extensionLocation = `${location}.researchExtension`;
    if (!extension || typeof extension !== "object") {
      fail(`${extensionLocation} must be an object.`);
    } else {
      nonEmptyString(extension.id, `${extensionLocation}.id`);
      nonEmptyString(extension.evidenceState, `${extensionLocation}.evidenceState`);
      nonEmptyString(extension.stopCondition, `${extensionLocation}.stopCondition`);
      if (!extension.stopCondition.includes(entry.decisionId)) fail(`${extensionLocation}.stopCondition must identify ${entry.decisionId}.`);
      if (extension.id && !extension.stopCondition.includes(extension.id)) fail(`${extensionLocation}.stopCondition must identify ${extension.id}.`);
      if (!extension.stopCondition.includes("independent case") ||
          !extension.stopCondition.includes("fresh digest") ||
          !extension.stopCondition.includes("reviewed promotion decision")) {
        fail(`${extensionLocation}.stopCondition must require independent case, fresh digest, and reviewed promotion.`);
      }
    }
  } else if (entry.researchExtension !== undefined) {
    fail(`${location}.researchExtension is only valid on current source/oracle categories.`);
  }
  if (entry.category === "superseded") {
    const claim = entry.supersessionClaim;
    const claimLocation = `${location}.supersessionClaim`;
    if (!claim || typeof claim !== "object") {
      fail(`${claimLocation} must identify the exact successor claim.`);
    } else {
      if (claim.decisionId !== entry.authorityRef?.decisionId) {
        fail(`${claimLocation}.decisionId must equal authorityRef.decisionId.`);
      }
      const successorRow = rowsById.get(claim.decisionId);
      if (!successorRow) {
        fail(`${claimLocation}.decisionId must identify a current ledger row.`);
      } else {
        if (claim.canonicalClaim !== successorRow.claim) fail(`${claimLocation}.canonicalClaim does not match ${claim.decisionId}.`);
        if (claim.claimDigest !== textDigest(successorRow.claim)) fail(`${claimLocation}.claimDigest is stale for ${claim.decisionId}.`);
      }
      nonEmptyString(claim.relation, `${claimLocation}.relation`);
      if (claim.relation && (!claim.relation.includes(entry.decisionId) || !claim.relation.includes(claim.decisionId))) {
        fail(`${claimLocation}.relation must name both ${entry.decisionId} and ${claim.decisionId}.`);
      }
      if (/is retired and .*same design surface/i.test(claim.relation ?? "")) {
        fail(`${claimLocation}.relation must state the concrete semantic successor, not a generic same-surface fallback.`);
      }
    }
  } else if (entry.supersessionClaim !== undefined) {
    fail(`${location}.supersessionClaim is only valid for superseded.`);
  }
  if (!['research-gated', 'implementation-evidence-gap'].includes(entry.category) && entry.stopCondition !== undefined) {
    fail(`${location}.stopCondition is only valid for research-gated or implementation-evidence-gap.`);
  }
  const allText = JSON.stringify(entry);
  if (/compiler\s+is\s+implemented|runtime\s+is\s+implemented|provider\s+is\s+ready/i.test(allText)) {
    fail(`${location} claims implementation without an implementation evidence category.`);
  }
}

for (const decisionId of ledgerIds) {
  if (!entriesById.has(decisionId)) fail(`classification is missing ${decisionId}.`);
}
for (const decisionId of entriesById.keys()) {
  if (!ledgerIdSet.has(decisionId)) fail(`classification has a ledger addition ${decisionId}.`);
}

for (const entry of entriesById.values()) {
  if (entry.category !== "superseded" || entry.authorityRef?.kind !== "superseding-decision") continue;
  const successor = entriesById.get(entry.authorityRef.decisionId);
  if (!successor) continue;
  if (["superseded", "rejected"].includes(successor.category)) {
    fail(`${entry.decisionId}.authorityRef.decisionId must point to a current decision, not ${successor.category}.`);
  }
}

// Fixed semantic sentinels keep the classification tied to reviewed mapping
// families. These are intentionally literal IDs, not generated from ranges or
// epochs, so a broad/default mutation cannot silently move an entire family.
const fixedGapContracts = new Map([
  ["W-001", ["syntax-and-types", "7"]],
  ["W-010", ["modules-and-visibility", "6"]],
  ["W-011", ["standard-library", "15"]],
  ["W-012", ["types-and-generics", "8"]],
  ["W-013", ["types-and-generics", "8.6"]],
  ["W-015", ["types-and-generics", "8.7"]],
  ["W-018", ["memory-and-ownership", "9"]],
  ["W-019", ["types-and-generics", "8.5"]],
  ["W-022", ["standard-library", "17"]],
  ["W-028", ["errors-and-cleanup", "11"]],
  ["W-031", ["property-behaviors", "10"]],
  ["W-036", ["execution", "12"]],
  ["W-044", ["syntax-and-grammar", "3.5"]],
  ["W-046", ["execution", "13"]],
  ["W-055", ["syntax-and-types", "5"]],
  ["W-059", ["standard-library", "16"]],
  ["W-069", ["packages-and-release", "21"]],
  ["W-075", ["compiler-bootstrap", "20"]],
  ["W-078", ["execution", "12.10"]],
  ["W-080", ["syntax-and-grammar", "5"]],
  ["W-084", ["design-oracles", "24"]],
  ["W-090", ["ffi-and-abi", "19"]],
  ["W-096", ["design-freeze", "24"]],
  ["W-228", ["allocation", "9"]],
  ["W-266", ["compiler-bootstrap", "3.6"]],
  ["W-283", ["protocols", "23"]],
  ["W-304", ["memory-and-ownership", "9.8"]],
  ["W-336", ["memory-and-ownership", "9.3"]],
  ["W-338", ["memory-and-ownership", "9.9"]],
  ["W-345", ["memory-and-ownership", "9.7"]],
  ["W-659", ["implementation-plan", "26"]],
  ["W-918", ["memory-and-ownership", "9.4"]],
  ["W-1418", ["types-and-protocols", "8.2"]],
  ["W-1384", ["borrow-relations-implementation", "9.2"]],
]);
for (const [decisionId, [component, section]] of fixedGapContracts) {
  const entry = entriesById.get(decisionId);
  if (!entry || entry.category !== "implementation-evidence-gap") {
    fail(`fixed gap assertion ${decisionId} must remain implementation-evidence-gap.`);
    continue;
  }
  if (entry.gap?.component !== component) fail(`fixed gap assertion ${decisionId} must use component ${component}.`);
  if (entry.authorityRef?.kind !== "design-contract" || entry.authorityRef.section !== section) {
    fail(`fixed gap assertion ${decisionId} must use DESIGN section ${section}.`);
  }
}
const fixedCategoryAssertions = [
  ["W-1381", "oracle-backed-current", "BRX3-protocol-union"],
  ["W-1382", "oracle-backed-current", "BRX3-witness-divergence"],
  ["W-1383", "oracle-backed-current", "BRX3-source-order-canonical"],
  ["W-1436", "oracle-backed-current", "BRX3-body-primary"],
];
for (const [decisionId, category, authorityId] of fixedCategoryAssertions) {
  const entry = entriesById.get(decisionId);
  if (!entry || entry.category !== category) {
    fail(`fixed category assertion ${decisionId} must remain ${category}.`);
    continue;
  }
  const actualId = category === "research-gated" ? entry.researchGate?.id : entry.authorityRef?.caseId;
  if (actualId !== authorityId) fail(`fixed category assertion ${decisionId} must use ${authorityId}.`);
}
const baselineExtension = entriesById.get("W-1436");
if (baselineExtension && (baselineExtension.researchExtension !== undefined ||
    baselineExtension.authorityRef?.decisionBridge?.decisionId !== "W-1351")) {
  fail("fixed baseline assertion W-1436 must keep BRX3 current and the W-1351 bridge without an active Research extension.");
}
const fixedSupersession = entriesById.get("W-281");
if (!fixedSupersession || fixedSupersession.category !== "superseded" ||
    fixedSupersession.authorityRef?.decisionId !== "W-1290" ||
    fixedSupersession.supersessionClaim?.decisionId !== "W-1290") {
  fail("fixed supersession assertion W-281 must point to semantic successor W-1290.");
}
const fixedSupersessionTargets = new Map([
  ["W-038", "W-1172"],
  ["W-039", "W-1162"],
  ["W-926", "W-1162"],
  ["W-1245", "W-1415"],
]);
for (const [decisionId, successorId] of fixedSupersessionTargets) {
  const entry = entriesById.get(decisionId);
  if (!entry || entry.category !== "superseded" || entry.authorityRef?.decisionId !== successorId ||
      entry.supersessionClaim?.decisionId !== successorId) {
    fail(`fixed supersession assertion ${decisionId} must point to semantic successor ${successorId}.`);
  }
}

const researchGateOwners = new Map();
for (const entry of entriesById.values()) {
  if (entry.category === "research-gated" && entry.researchGate?.id) {
    const owners = researchGateOwners.get(entry.researchGate.id) ?? [];
    owners.push(entry.decisionId);
    researchGateOwners.set(entry.researchGate.id, owners);
  }
}
for (const [gateId, owners] of researchGateOwners) {
  if (owners.length > 1) fail(`research gate ${gateId} is reused by ${owners.join(", ")}; each open question needs an explicit gate.`);
}
const researchExtensionOwners = new Map();
for (const entry of entriesById.values()) {
  if (entry.researchExtension?.id) {
    const owners = researchExtensionOwners.get(entry.researchExtension.id) ?? [];
    owners.push(entry.decisionId);
    researchExtensionOwners.set(entry.researchExtension.id, owners);
  }
}
for (const [extensionId, owners] of researchExtensionOwners) {
  if (owners.length > 1) fail(`research extension ${extensionId} is reused by ${owners.join(", ")}; extension IDs must be decision-specific.`);
}

function validateAuditSamples() {
  if (!classification.auditSamples || typeof classification.auditSamples !== "object") {
    fail("classification.auditSamples must provide stratified samples.");
    return;
  }
  const epochById = new Map();
  if (!Array.isArray(classification.epochs) || classification.epochs.length !== 6) {
    fail("classification.epochs must list the six explicit audit epochs.");
  } else {
    for (const [epochIndex, epoch] of classification.epochs.entries()) {
      if (!epoch || typeof epoch !== "object" || !nonEmptyString(epoch.id, `classification.epochs[${epochIndex}].id`) ||
          !nonEmptyString(epoch.from, `classification.epochs[${epochIndex}].from`) ||
          !nonEmptyString(epoch.to, `classification.epochs[${epochIndex}].to`)) continue;
      const from = Number(String(epoch.from).slice(2));
      const to = Number(String(epoch.to).slice(2));
      if (!Number.isInteger(from) || !Number.isInteger(to) || from > to) {
        fail(`classification.epochs[${epochIndex}] has invalid bounds.`);
        continue;
      }
      for (let number = from; number <= to; number++) epochById.set(`W-${String(number).padStart(3, "0")}`, epoch.id);
    }
  }
  const validateSet = (samples, label, expected, property) => {
    if (!samples || typeof samples !== "object") {
      fail(`classification.auditSamples.${label} must be an object.`);
      return;
    }
    const populationByKey = new Map(expected.map((key) => [key, []]));
    for (const entry of entriesById.values()) {
      const key = property === "category" ? entry.category : epochById.get(entry.decisionId);
      if (populationByKey.has(key)) populationByKey.get(key).push(entry.decisionId);
    }
    for (const key of expected) {
      const ids = samples[key];
      const population = populationByKey.get(key) ?? [];
      const requiredMinimum = Math.min(10, population.length);
      if (!Array.isArray(ids) || ids.length < requiredMinimum) {
        fail(`classification.auditSamples.${label}.${key} must contain at least ${requiredMinimum} IDs (population ${population.length}).`);
        continue;
      }
      const seen = new Set();
      const observedDiversity = new Set();
      for (const [index, decisionId] of ids.entries()) {
        if (seen.has(decisionId)) {
          fail(`classification.auditSamples.${label}.${key} contains duplicate decision ID ${decisionId}.`);
        }
        seen.add(decisionId);
        const entry = entriesById.get(decisionId);
        if (!entry) {
          fail(`classification.auditSamples.${label}.${key}[${index}] is not a classified ledger ID.`);
          continue;
        }
        if (property === "category") {
          const epoch = epochById.get(decisionId);
          if (epoch) observedDiversity.add(epoch);
        } else if (categories.has(entry.category)) {
          observedDiversity.add(entry.category);
        }
        if (property === "category" && entry.category !== key) {
          fail(`classification.auditSamples.${label}.${key}[${index}] has category ${entry.category}.`);
        }
        if (property === "epoch" && epochById.get(decisionId) !== key) {
          fail(`classification.auditSamples.${label}.${key}[${index}] is outside epoch ${key}.`);
        }
      }
      const availableDiversity = new Set(
        population.map((decisionId) => property === "category"
          ? epochById.get(decisionId)
          : entriesById.get(decisionId)?.category).filter(Boolean),
      );
      const requiredDiversity = Math.min(3, availableDiversity.size);
      if (observedDiversity.size < requiredDiversity) {
        const dimension = property === "category" ? "epochs" : "categories";
        fail(`classification.auditSamples.${label}.${key} must cover at least ${requiredDiversity} ${dimension} (available ${availableDiversity.size}).`);
      }
    }
  };
  validateSet(classification.auditSamples.byCategory, "byCategory", [...categories], "category");
  validateSet(classification.auditSamples.byEpoch, "byEpoch", (classification.epochs ?? []).map((epoch) => epoch.id), "epoch");
}

validateAuditSamples();

// Preserve the previous audit's source/oracle coverage as an invariant.
const oldCorpusNames = new Set([
  "semantic-cases.json", "formatter-cases.json", "memory-transition-cases.json", "allocation-cases.json",
  "layout-abi-cases.json", "execution-concurrency-cases.json", "runtime-liveness-cases.json", "lazy-behavior-cases.json",
  "ownership-execution-cases.json", "channel-cases.json", "context-local-cases.json", "interference-layout-cases.json",
  "scoped-lock-cases.json", "snapshot-cell-cases.json", "boundary-effect-cases.json", "service-recovery-cases.json",
  "package-release-cases.json", "module-run-cases.json", "repl-session-cases.json", "presentation-cases.json",
  "jupyter-cases.json", "notebook-export-cases.json", "wmeta-cases.json", "tabular-carrier-cases.json",
  "tabular-adapter-cases.json", "dlpack-cases.json", "device-execution-cases.json", "kernel-module-cases.json",
  "foreign-body-cases.json", "web-body-cases.json", "process-root-cases.json", "filesystem-cases.json",
  "io-error-cases.json", "operational-time-cases.json", "aeg0-app-essentials-gate-cases.json",
]);
const legacySourceIds = new Set((substitutions.cases ?? []).flatMap((testCase) => testCase.decisions ?? []));
const legacyOracleIds = new Set();
for (const name of oldCorpusNames) {
  const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, name), "utf8"));
  const rules = (corpus.cases ?? []).filter((testCase) => testCase.rule !== undefined);
  for (const testCase of corpus.cases ?? []) {
    const decisions = testCase.decisions ?? (["module-run-cases.json", "repl-session-cases.json"].includes(name) ? corpus.decisions : undefined);
    for (const decisionId of decisions ?? []) legacyOracleIds.add(decisionId);
  }
  if (rules.length > 0) for (const [decisionId] of deriveSemanticRulePairs(rules, ledgerIds)) legacyOracleIds.add(decisionId);
}
const oldAudit = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "design-freeze-audit.json"), "utf8"));
const expectedLegacy = {
  source: legacySourceIds.size,
  oracle: legacyOracleIds.size,
  explicit: (oldAudit.entries ?? []).length,
  overlap: [...legacySourceIds].filter((decisionId) => legacyOracleIds.has(decisionId)).length,
};
for (const [key, value] of Object.entries(expectedLegacy)) {
  if (classification.preservedLegacyCoverage?.[key] !== value) fail(`preservedLegacyCoverage.${key} must remain ${value}.`);
}

const legacyExplicitIds = new Set((oldAudit.entries ?? []).map((entry) => entry.decision ?? entry.decisionId).filter(Boolean));
const legacyClassifiedIds = new Set([...legacySourceIds, ...legacyOracleIds, ...legacyExplicitIds]);
const archiveUnclassifiedIds = ledgerIds.filter((decisionId) => !legacyClassifiedIds.has(decisionId));
if (classification.archiveGapDistribution?.count !== archiveUnclassifiedIds.length) {
  fail(`archiveGapDistribution.count must remain ${archiveUnclassifiedIds.length}.`);
}
const archiveByCategory = Object.fromEntries([...categories].map((category) => [category, 0]));
const archiveByEpoch = Object.fromEntries((classification.epochs ?? []).map((epoch) => [epoch.id,
  Object.fromEntries([...categories].map((category) => [category, 0]))]));
for (const decisionId of archiveUnclassifiedIds) {
  const entry = entriesById.get(decisionId);
  if (!entry) continue;
  if (categories.has(entry.category)) archiveByCategory[entry.category]++;
  const number = Number(decisionId.slice(2));
  const epoch = (classification.epochs ?? []).find((candidate) =>
    number >= Number(candidate.from?.slice(2)) && number <= Number(candidate.to?.slice(2)));
  if (epoch && categories.has(entry.category)) archiveByEpoch[epoch.id][entry.category]++;
}
for (const category of categories) {
  if (classification.archiveGapDistribution?.byCategory?.[category] !== archiveByCategory[category]) {
    fail(`archiveGapDistribution.byCategory.${category} must remain ${archiveByCategory[category]}.`);
  }
}
for (const [epochId, countsForEpoch] of Object.entries(archiveByEpoch)) {
  for (const category of categories) {
    if (classification.archiveGapDistribution?.byEpoch?.[epochId]?.[category] !== countsForEpoch[category]) {
      fail(`archiveGapDistribution.byEpoch.${epochId}.${category} must remain ${countsForEpoch[category]}.`);
    }
  }
}

const counts = Object.fromEntries([...categories].map((category) => [category, 0]));
for (const entry of entriesById.values()) if (categories.has(entry.category)) counts[entry.category]++;
if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}
const evidenceCount = [...entriesById.values()].filter((entry) => entry.evidence.length > 0).length;
process.stdout.write(
  `Design freeze classification: ${entriesById.size}/${ledgerIds.length} decisions classified; ` +
  `${JSON.stringify(counts)}; ${evidenceCount} entries retain source/oracle evidence.\n`,
);
if (process.argv.includes("--list-unclassified")) {
  for (const entry of entriesById.values()) {
    if (entry.category === "implementation-evidence-gap") process.stdout.write(`${entry.decisionId}\t${entry.summary}\n`);
  }
}
