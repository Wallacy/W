import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIds, rationaleText } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryDirectory = path.resolve(toolingDirectory, "..");
const classificationPath = process.env.W_DESIGN_FREEZE_CLASSIFICATION
  ? path.resolve(process.env.W_DESIGN_FREEZE_CLASSIFICATION)
  : path.join(toolingDirectory, "design-freeze-classification.json");

function textDigest(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

function fileDigest(filePath) {
  return textDigest(fs.readFileSync(filePath));
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
    const next = headings.find((candidate) => candidate.start > record.start && candidate.level <= record.level);
    const end = next?.start ?? lines.length;
    return [record.section, {
      heading: record.heading,
      sectionDigest: textDigest(lines.slice(record.start, end).join("\n")),
    }];
  }));
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

function refreshFileReferences(value, counters, seen = new Set()) {
  if (!value || typeof value !== "object" || seen.has(value)) return;
  seen.add(value);
  if (typeof value.path === "string" && Object.hasOwn(value, "sha256")) {
    const next = fileDigest(repositoryFile(value.path));
    if (value.sha256 !== next) {
      value.sha256 = next;
      counters.fileDigests++;
    }
  }
  for (const nested of Object.values(value)) refreshFileReferences(nested, counters, seen);
}

function replaceIdentityText(value, oldSummary, newSummary, oldDigest, newDigest) {
  if (typeof value !== "string") return value;
  return value.replaceAll(oldDigest, newDigest).replaceAll(oldSummary, newSummary);
}

function refreshClaimReference(reference, rows, counters) {
  if (!reference || typeof reference !== "object" || typeof reference.decisionId !== "string") return;
  const row = rows.get(reference.decisionId);
  if (!row) throw new Error(`claim reference names unknown decision ${reference.decisionId}`);
  const nextDigest = textDigest(row.claim);
  if (Object.hasOwn(reference, "canonicalClaim") && reference.canonicalClaim !== row.claim) {
    reference.canonicalClaim = row.claim;
    counters.claimReferences++;
  }
  if (Object.hasOwn(reference, "claimDigest") && reference.claimDigest !== nextDigest) {
    reference.claimDigest = nextDigest;
    counters.claimReferences++;
  }
}

const classification = JSON.parse(fs.readFileSync(classificationPath, "utf8"));
const beforeShape = protectedClassificationShape(classification);
const rows = ledgerRows();
const sections = designSections();
const counters = { entries: 0, claimReferences: 0, fileDigests: 0, designSections: 0 };

if (!Array.isArray(classification.entries)) throw new Error("classification.entries must be an array");
if (classification.entries.length !== ledgerIds.length || rows.size !== ledgerIds.length) {
  throw new Error(`classification/ledger size mismatch: ${classification.entries.length}/${rows.size}/${ledgerIds.length}`);
}
const classifiedIds = classification.entries.map((entry) => entry.decisionId);
if (JSON.stringify(classifiedIds) !== JSON.stringify(ledgerIds)) {
  throw new Error("classification decision order differs from the current ledger");
}

classification.ledger.sha256 = fileDigest(repositoryFile("RATIONALE.md"));
classification.ledger.count = ledgerIds.length;
classification.ledger.first = ledgerIds[0];
classification.ledger.last = ledgerIds.at(-1);

for (const entry of classification.entries) {
  const row = rows.get(entry.decisionId);
  if (!row) throw new Error(`classification contains unknown decision ${entry.decisionId}`);
  const oldSummary = entry.summary;
  const oldDigest = entry.claimDigest;
  const nextDigest = textDigest(row.claim);
  const identityChanged = oldSummary !== row.theme || entry.canonicalClaim !== row.claim || oldDigest !== nextDigest;
  if (identityChanged) counters.entries++;
  entry.summary = row.theme;
  entry.canonicalClaim = row.claim;
  entry.claimDigest = nextDigest;
  if (identityChanged) {
    entry.reason = replaceIdentityText(entry.reason, oldSummary, row.theme, oldDigest, nextDigest);
    entry.stopCondition = replaceIdentityText(entry.stopCondition, oldSummary, row.theme, oldDigest, nextDigest);
  }
  if (entry.basisRef) entry.basisRef.claimDigest = nextDigest;
  refreshClaimReference(entry.supersessionClaim, rows, counters);
  refreshClaimReference(entry.authorityRef?.decisionBridge, rows, counters);
  for (const evidence of entry.evidence ?? []) refreshClaimReference(evidence.decisionBridge, rows, counters);
  if (entry.authorityRef?.path === "DESIGN.md" && typeof entry.authorityRef.section === "string" &&
      Object.hasOwn(entry.authorityRef, "sectionDigest")) {
    const section = sections.get(entry.authorityRef.section);
    if (!section) throw new Error(`unknown DESIGN.md section ${entry.authorityRef.section}`);
    if (entry.authorityRef.heading !== section.heading || entry.authorityRef.sectionDigest !== section.sectionDigest) {
      entry.authorityRef.heading = section.heading;
      entry.authorityRef.sectionDigest = section.sectionDigest;
      counters.designSections++;
    }
  }
}

refreshFileReferences(classification, counters);
if (protectedClassificationShape(classification) !== beforeShape) {
  throw new Error("refresh attempted to change reviewed classification structure");
}

fs.writeFileSync(classificationPath, `${JSON.stringify(classification, null, 2)}\n`);
process.stdout.write(
  `Design freeze evidence refreshed: ${counters.entries} ledger identities, ` +
  `${counters.claimReferences} linked claims, ${counters.fileDigests} file digests, ` +
  `${counters.designSections} DESIGN sections; reviewed categories unchanged.\n`,
);
