import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIds, rationaleText } from "./design-ledger.mjs";
import { caseDigest } from "./evidence-locality.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryDirectory = path.resolve(toolingDirectory, "..");
const classificationPath = process.env.W_DESIGN_FREEZE_CLASSIFICATION
  ? path.resolve(process.env.W_DESIGN_FREEZE_CLASSIFICATION)
  : path.join(toolingDirectory, "design-freeze-classification.json");
if (process.argv.length > 2) {
  throw new Error(`unknown argument: ${process.argv.slice(2).join(" ")}`);
}

function repositoryFile(relativePath) {
  if (typeof relativePath !== "string" || relativePath.length === 0 || path.isAbsolute(relativePath)) {
    throw new Error(`invalid repository-relative path: ${String(relativePath)}`);
  }
  if (relativePath.replaceAll("\\", "/").includes("history/")) {
    throw new Error(`history cannot be current authority: ${relativePath}`);
  }
  const resolved = path.resolve(repositoryDirectory, relativePath);
  const relative = path.relative(repositoryDirectory, resolved);
  if (relative.startsWith("..") || path.isAbsolute(relative)) {
    throw new Error(`path escapes the repository: ${relativePath}`);
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    throw new Error(`referenced file does not exist: ${relativePath}`);
  }
  return resolved;
}

function ledgerRows() {
  const rows = new Map();
  const ledgerStart = rationaleText.indexOf("## 3. Ledger");
  if (ledgerStart < 0) throw new Error("RATIONALE.md does not contain §3 Ledger");
  for (const line of rationaleText.slice(ledgerStart).split("\n")) {
    if (!line.startsWith("| W-")) continue;
    const body = line.slice(2);
    const first = body.indexOf("|");
    const second = body.indexOf("|", first + 1);
    const trailing = body.lastIndexOf("|");
    const claimEnd = body.lastIndexOf("|", trailing - 1);
    rows.set(body.slice(0, first).trim(), {
      theme: body.slice(first + 1, second).trim(),
      claim: body.slice(second + 1, claimEnd).trim(),
    });
  }
  return rows;
}

function designSections() {
  const designPath = repositoryFile("DESIGN.md");
  const lines = fs.readFileSync(designPath, "utf8").split(/\r?\n/);
  const headings = [];
  for (const [index, line] of lines.entries()) {
    const match = /^(#{2,6})\s+(.+?)\s*$/.exec(line);
    const sectionMatch = match && /^(\d+(?:\.\d+)*)\b/.exec(match[2]);
    if (!sectionMatch) continue;
    headings.push({ section: sectionMatch[1], heading: line, level: match[1].length, start: index });
  }
  return new Map(headings.map((record) => {
    return [record.section, {
      heading: record.heading,
    }];
  }));
}

function caseIndexes() {
  const source = new Map();
  const substitutions = JSON.parse(
    fs.readFileSync(repositoryFile("tooling/substitution-cases.json"), "utf8"),
  );
  for (const testCase of substitutions.cases ?? []) source.set(testCase.id, testCase);

  const oracle = new Map();
  const corpusNames = fs.readdirSync(toolingDirectory)
    .filter((name) => name.endsWith("-cases.json") && name !== "substitution-cases.json");
  const nestedCorpusNames = ["studies/rdx0-binary-registry-execution/cases.json"];
  for (const name of [...corpusNames, ...nestedCorpusNames]) {
    const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, name), "utf8"));
    for (const [index, testCase] of (corpus.cases ?? []).entries()) {
      oracle.set(testCase.id ?? `${name}#${index + 1}`, testCase);
    }
  }
  return { source, oracle };
}

function referencedCase(reference, indexes) {
  if (!reference || typeof reference !== "object") return null;
  if (reference.kind === "source" || reference.kind === "source-case") {
    if (reference.caseId === "W-1519-native-seed") return null;
    return indexes.source.get(reference.caseId) ?? null;
  }
  if (reference.kind === "oracle" || reference.kind === "oracle-case") {
    return indexes.oracle.get(reference.caseId) ?? null;
  }
  return null;
}

function protectedClassificationShape(classification) {
  return JSON.stringify({
    schema: classification.$schema,
    status: classification.status,
    selectionPolicy: classification.selectionPolicy,
    epochs: classification.epochs,
    preservedLegacyCoverage: classification.preservedLegacyCoverage,
    archiveGapDistribution: classification.archiveGapDistribution,
    categories: classification.categories,
    auditSamples: classification.auditSamples,
    entries: (classification.entries ?? []).map((entry) => ({
      decisionId: entry.decisionId,
      category: entry.category,
      basis: entry.basis,
      selection: entry.selection,
      authority: entry.authorityRef && {
        kind: entry.authorityRef.kind,
        path: entry.authorityRef.path,
        section: entry.authorityRef.section,
        caseId: entry.authorityRef.caseId,
        decisionId: entry.authorityRef.decisionId,
      },
      evidence: (entry.evidence ?? []).map((reference) => ({
        kind: reference.kind,
        path: reference.path,
        caseId: reference.caseId,
        sourcePath: reference.sourceRef?.path,
        bridgeDecisionId: reference.decisionBridge?.decisionId,
      })),
      gap: entry.gap,
      researchGate: entry.researchGate,
      researchExtension: entry.researchExtension,
      supersessionDecisionId: entry.supersessionClaim?.decisionId,
    })),
  });
}

function refreshReference(reference, sections, indexes, counters) {
  if (!reference || typeof reference !== "object") return;
  const testCase = referencedCase(reference, indexes);
  if (testCase) {
    const next = caseDigest(testCase);
    if (reference.caseDigest !== next) {
      reference.caseDigest = next;
      counters.caseDigests++;
    }
    return;
  }
  if (["design-contract", "design-freeze-gate", "design-absence"].includes(reference.kind)) {
    const section = sections.get(reference.section);
    if (!section) throw new Error(`unknown DESIGN.md section ${reference.section}`);
    if (reference.heading !== section.heading) {
      reference.heading = section.heading;
      counters.designHeadings++;
    }
  }
}

function replaceIdentityText(value, oldSummary, newSummary) {
  if (typeof value !== "string") return value;
  return value.replaceAll(oldSummary, newSummary);
}

function refreshClaimReference(reference, rows, counters) {
  if (!reference || typeof reference !== "object" || typeof reference.decisionId !== "string") return;
  const row = rows.get(reference.decisionId);
  if (!row) throw new Error(`claim reference names unknown decision ${reference.decisionId}`);
  if (Object.hasOwn(reference, "canonicalClaim") && reference.canonicalClaim !== row.claim) {
    reference.canonicalClaim = row.claim;
    counters.claimReferences++;
  }
}

const classification = JSON.parse(fs.readFileSync(classificationPath, "utf8"));
if (classification.$schema !== "w-design-freeze-classification-2") {
  throw new Error("classification schema must be w-design-freeze-classification-2");
}
const beforeShape = protectedClassificationShape(classification);
const rows = ledgerRows();
const sections = designSections();
const indexes = caseIndexes();
const counters = {
  entries: 0,
  claimReferences: 0,
  designHeadings: 0,
  caseDigests: 0,
};

if (!Array.isArray(classification.entries)) throw new Error("classification.entries must be an array");
if (classification.entries.length !== ledgerIds.length || rows.size !== ledgerIds.length) {
  throw new Error(`classification/ledger size mismatch: ${classification.entries.length}/${rows.size}/${ledgerIds.length}`);
}
const classifiedIds = classification.entries.map((entry) => entry.decisionId);
if (JSON.stringify(classifiedIds) !== JSON.stringify(ledgerIds)) {
  throw new Error("classification decision order differs from the current ledger");
}

classification.ledger.count = ledgerIds.length;
classification.ledger.first = ledgerIds[0];
classification.ledger.last = ledgerIds.at(-1);

for (const entry of classification.entries) {
  const row = rows.get(entry.decisionId);
  if (!row) throw new Error(`classification contains unknown decision ${entry.decisionId}`);
  const oldSummary = entry.summary;
  const identityChanged = oldSummary !== row.theme || entry.canonicalClaim !== row.claim;
  if (identityChanged) counters.entries++;
  entry.summary = row.theme;
  entry.canonicalClaim = row.claim;
  if (identityChanged) {
    entry.reason = replaceIdentityText(entry.reason, oldSummary, row.theme);
    entry.stopCondition = replaceIdentityText(entry.stopCondition, oldSummary, row.theme);
  }
  refreshClaimReference(entry.supersessionClaim, rows, counters);
  refreshClaimReference(entry.authorityRef?.decisionBridge, rows, counters);
  for (const evidence of entry.evidence ?? []) refreshClaimReference(evidence.decisionBridge, rows, counters);
  if (entry.authorityRef?.path === "DESIGN.md" && typeof entry.authorityRef.section === "string") {
    const section = sections.get(entry.authorityRef.section);
    if (!section) throw new Error(`unknown DESIGN.md section ${entry.authorityRef.section}`);
    if (entry.authorityRef.heading !== section.heading) {
      entry.authorityRef.heading = section.heading;
      counters.designHeadings++;
    }
  }
  refreshReference(entry.basisRef, sections, indexes, counters);
  refreshReference(entry.authorityRef, sections, indexes, counters);
  for (const evidence of entry.evidence ?? []) {
    refreshReference(evidence, sections, indexes, counters);
  }
}

if (protectedClassificationShape(classification) !== beforeShape) {
  throw new Error("refresh attempted to change reviewed classification structure");
}

fs.writeFileSync(classificationPath, `${JSON.stringify(classification, null, 2)}\n`);
process.stdout.write(
  `Design freeze evidence refreshed: ${counters.entries} ledger identities, ` +
  `${counters.claimReferences} linked canonical claims, ${counters.caseDigests} exact case digests, ` +
  `${counters.designHeadings} DESIGN headings; reviewed categories unchanged.\n`,
);
