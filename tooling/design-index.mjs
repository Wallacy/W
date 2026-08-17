import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIds } from "./design-ledger.mjs";
import { deriveSemanticRulePairs } from "./semantic-diagnostic-pairs.mjs";
import { expandInterferenceLayoutOperations } from "./interference-layout-machine.mjs";
import {
  countKernelModuleOperations,
  prepareKernelModuleCase,
} from "./kernel-module-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const designPath = path.join(wDirectory, "DESIGN.md");
const rationalePath = path.join(wDirectory, "RATIONALE.md");
const indexPath = path.join(wDirectory, "DESIGN-INDEX.md");
const classificationPath = path.join(wDirectory, "tooling", "design-freeze-classification.json");
const designText = fs.readFileSync(designPath, "utf8");
const lines = designText.split(/\r?\n/);
const rationaleText = fs.readFileSync(rationalePath, "utf8");
const rationaleLines = rationaleText.split(/\r?\n/);
const designFreezeClassification = JSON.parse(fs.readFileSync(classificationPath, "utf8"));
const designFreezeEntries = designFreezeClassification.entries ?? [];
const designFreezeCategoryCounts = new Map();
for (const entry of designFreezeEntries) {
  designFreezeCategoryCounts.set(entry.category, (designFreezeCategoryCounts.get(entry.category) ?? 0) + 1);
}

function recursiveFiles(directory, predicate) {
  const result = [];

  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    const entryPath = path.join(directory, entry.name);

    if (entry.isDirectory()) {
      result.push(...recursiveFiles(entryPath, predicate));
    } else if (predicate(entryPath)) {
      result.push(entryPath);
    }
  }

  return result;
}

function approximateTokens(text) {
  return Math.ceil(Buffer.byteLength(text, "utf8") / 400) * 100;
}

function tableCell(value) {
  return value.replaceAll("|", "\\|");
}

function topLevelSections(sourceLines) {
  const sections = [];
  for (let index = 0; index < sourceLines.length; index += 1) {
    const match = /^## (\d+)\. (.+)$/.exec(sourceLines[index]);
    if (match) sections.push({ number: Number(match[1]), title: match[2], start: index + 1 });
  }
  for (let index = 0; index < sections.length; index += 1) {
    const section = sections[index];
    section.end = sections[index + 1]?.start - 1 || sourceLines.length;
    section.tokens = approximateTokens(sourceLines.slice(section.start - 1, section.end).join("\n"));
  }
  return sections;
}

const numberedSections = [];

for (let index = 0; index < lines.length; index += 1) {
  const match = /^## (\d+)\. (.+)$/.exec(lines[index]);

  if (match) {
    numberedSections.push({
      number: Number(match[1]),
      title: match[2],
      start: index + 1,
    });
  }
}

for (let index = 0; index < numberedSections.length; index += 1) {
  const section = numberedSections[index];
  section.end = numberedSections[index + 1]?.start - 1 || lines.length;
  section.tokens = approximateTokens(lines.slice(section.start - 1, section.end).join("\n"));
}

const rationaleSections = topLevelSections(rationaleLines);

const readingBundles = [
  {
    name: "orientação e superfície",
    sections: [0, 1, 2, 3, 4, 5, 6, 7, 8],
    purpose: "promessa, símbolos, source, módulos, funções e tipos",
  },
  {
    name: "segurança e execução",
    sections: [9, 10, 11, 12, 13],
    purpose: "ownership, errors, tasks, domains, services e entries",
  },
  {
    name: "std e performance",
    sections: [14, 15, 16, 17, 18, 19],
    purpose: "módulos, números, texto, tensors, custo, C e unsafe",
  },
  {
    name: "compiler e distribuição",
    sections: [20, 21, 22, 23],
    purpose: "frontend, HIR, packages, releases, tooling e protocolos",
  },
  {
    name: "validação e decisões",
    sections: [24, 25, 26],
    purpose: "freeze, Última Luz, gates e roadmap",
  },
];

function sectionFor(number) {
  return numberedSections.find((section) => section.number === number);
}

function bundleStats(bundle) {
  const sections = bundle.sections.map(sectionFor);
  const start = Math.min(...sections.map((section) => section.start));
  const end = Math.max(...sections.map((section) => section.end));
  const tokens = sections.reduce((total, section) => total + section.tokens, 0);
  return { ...bundle, start, end, tokens };
}

const headings = [];

for (let index = 0; index < lines.length; index += 1) {
  const match = /^(#{2,4})\s+(.+)$/.exec(lines[index]);

  if (match) {
    headings.push({ line: index + 1, level: match[1].length, title: match[2] });
  }
}

let leafCount = 0;
let evidencedLeafCount = 0;

for (let index = 0; index < headings.length; index += 1) {
  const heading = headings[index];
  let nextLine = lines.length + 1;
  let hasChild = false;

  for (let candidate = index + 1; candidate < headings.length; candidate += 1) {
    if (headings[candidate].level <= heading.level) {
      nextLine = headings[candidate].line;
      break;
    }

    hasChild = true;
  }

  if (hasChild) {
    continue;
  }

  leafCount += 1;
  const body = lines.slice(heading.line, nextLine - 1).join("\n");
  const hasEvidence =
    body.includes("```") ||
    body.includes("|---") ||
    body.includes("**Exemplo:") ||
    body.includes("reference/last-light");

  if (hasEvidence) {
    evidencedLeafCount += 1;
  }
}

const decisions = ledgerIds.map((id) => Number(id.slice(2)));

const structuralErrors = [];

for (let index = 0; index < numberedSections.length; index += 1) {
  if (numberedSections[index].number !== index) {
    structuralErrors.push(
      `Expected design section ${index}, found ${numberedSections[index].number}.`,
    );
  }
}

for (let index = 0; index < decisions.length; index += 1) {
  if (decisions[index] !== index + 1) {
    structuralErrors.push(
      `Expected decision W-${String(index + 1).padStart(3, "0")}, found W-${String(decisions[index]).padStart(3, "0")}.`,
    );
    break;
  }
}

if (decisions.length === 0) {
  structuralErrors.push("The decision ledger is empty.");
}

const viabilityHeading = "### 1.8 Catálogo comparativo de viabilidade";
const viabilityStart = rationaleLines.findIndex((line) => line === viabilityHeading);
const viabilityEnd = rationaleLines.findIndex(
  (line, index) => index > viabilityStart && line.startsWith("### 1."),
);
const viabilityRows = [];

if (viabilityStart < 0 || viabilityEnd <= viabilityStart) {
  structuralErrors.push("RATIONALE.md section 1.8 is required for viability metrics.");
}

for (const line of rationaleLines.slice(viabilityStart + 1, viabilityEnd)) {
  if (!line.startsWith("| ") || line.startsWith("|---") || line.includes("Classe vigente")) {
    continue;
  }

  const cells = line.split("|").slice(1, -1).map((cell) => cell.trim());

  if (cells.length >= 3) {
    viabilityRows.push({
      family: cells[0],
      classification: cells[1].replaceAll("**", ""),
    });
  }
}

const viabilityCounts = new Map();

for (const row of viabilityRows) {
  viabilityCounts.set(
    row.classification,
    (viabilityCounts.get(row.classification) || 0) + 1,
  );
}

const reviewStart = rationaleLines.findIndex((line) => line === "O corpus compara, no mínimo:");
const reviewCoverageStart = rationaleLines.findIndex(
  (line) => line === "### 1.1 Cobertura de substituições",
);

if (reviewStart < 0 || reviewCoverageStart <= reviewStart) {
  structuralErrors.push("RATIONALE.md section 1 comparison list and coverage heading are required.");
}

const malformedDiagnosticCodes = [
  ...new Set(designText.match(/\bW-(?:[A-Z]+-)+[0-9]{4}\b/g) ?? []),
].filter((code) => !/^W-[A-Z]+-[0-9]{4}$/.test(code));
if (malformedDiagnosticCodes.length > 0) {
  structuralErrors.push(
    `Diagnostic codes must use one family and four digits: ${malformedDiagnosticCodes.join(", ")}.`,
  );
}

if (structuralErrors.length > 0) {
  process.stderr.write(`${structuralErrors.join("\n")}\n`);
  process.exit(1);
}

const comparisonCount = rationaleLines
  .slice(reviewStart + 1, reviewCoverageStart)
  .filter((line) => line.startsWith("- ")).length;
const substitutionCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "substitution-cases.json"), "utf8"),
);
const structuredSubstitutionCases = new Set(
  substitutionCorpus.cases.map((testCase) => testCase.reviewItem.trim().replace(/[.;]$/, "")),
).size;
const substitutionDecisionIds = new Set(
  substitutionCorpus.cases.flatMap((testCase) => testCase.decisions),
);
const designFreezeAudit = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "design-freeze-audit.json"), "utf8"),
);
const explicitFreezeDecisionIds = new Set(
  designFreezeAudit.entries.map((entry) => entry.decision),
);
const multiAxisFreezeRequirements = designFreezeAudit.requirements.length;
const oracleCorpusFiles = [
  "semantic-cases.json",
  "formatter-cases.json",
  "memory-transition-cases.json",
  "allocation-cases.json",
  "layout-abi-cases.json",
  "execution-concurrency-cases.json",
  "runtime-liveness-cases.json",
  "lazy-behavior-cases.json",
  "ownership-execution-cases.json",
  "channel-cases.json",
  "context-local-cases.json",
  "interference-layout-cases.json",
  "scoped-lock-cases.json",
  "snapshot-cell-cases.json",
  "boundary-effect-cases.json",
  "service-recovery-cases.json",
  "package-release-cases.json",
  "module-run-cases.json",
  "repl-session-cases.json",
  "presentation-cases.json",
  "jupyter-cases.json",
  "notebook-export-cases.json",
  "wmeta-cases.json",
  "tabular-carrier-cases.json",
  "tabular-adapter-cases.json",
  "dlpack-cases.json",
  "device-execution-cases.json",
  "kernel-module-cases.json",
  "foreign-body-cases.json",
  "web-body-cases.json",
  "process-root-cases.json",
  "filesystem-cases.json",
  "io-error-cases.json",
  "operational-time-cases.json",
];
const oracleFreezeDecisionIds = new Set(
  oracleCorpusFiles.flatMap((file) => {
    const corpus = JSON.parse(
      fs.readFileSync(path.join(wDirectory, "tooling", file), "utf8"),
    );
    const decisions = corpus.cases.flatMap(
      (testCase) =>
        testCase.decisions ??
        (["module-run-cases.json", "repl-session-cases.json"].includes(file) ? corpus.decisions ?? [] : []),
    );
    const ruleCases = corpus.cases.filter((testCase) => testCase.rule !== undefined);
    const semanticPairDecisions = ruleCases.length > 0
      ? [...deriveSemanticRulePairs(ruleCases, ledgerIds).keys()]
      : [];
    return [...decisions, ...semanticPairDecisions];
  }),
);
const classifiedFreezeDecisionIds = new Set([
  ...substitutionDecisionIds,
  ...oracleFreezeDecisionIds,
  ...explicitFreezeDecisionIds,
]);
const freezeEvidenceOverlaps =
  substitutionDecisionIds.size +
  oracleFreezeDecisionIds.size +
  explicitFreezeDecisionIds.size -
  classifiedFreezeDecisionIds.size;
const substitutionSurface = JSON.parse(
  fs.readFileSync(
    path.join(wDirectory, "tooling", "substitution-surface.snapshot.json"),
    "utf8",
  ),
);
const measuredSubstitutionForms = substitutionSurface.cases.reduce(
  (count, testCase) => count + 1 + testCase.alternatives.length,
  0,
);
const selectedSurfaceLexemes = substitutionSurface.cases
  .map((testCase) => testCase.selected.metrics.surfaceLexemes)
  .sort((left, right) => left - right);
const selectedSurfaceLexemeTotal = selectedSurfaceLexemes.reduce(
  (total, count) => total + count,
  0,
);
const selectedSurfaceMiddle = Math.floor(selectedSurfaceLexemes.length / 2);
const selectedSurfaceLexemeMedian =
  selectedSurfaceLexemes.length % 2 === 1
    ? selectedSurfaceLexemes[selectedSurfaceMiddle]
    : (selectedSurfaceLexemes[selectedSurfaceMiddle - 1] +
        selectedSurfaceLexemes[selectedSurfaceMiddle]) /
      2;
const selectedSurfaceLexemeMaximum = selectedSurfaceLexemes.at(-1);
const studyBundleFiles = recursiveFiles(
  path.join(wDirectory, "tooling", "studies"),
  (file) => path.basename(file) === "bundle.json",
);
const studyBundles = studyBundleFiles.map((file) =>
  JSON.parse(fs.readFileSync(file, "utf8")),
);
const studyVariants = studyBundles.reduce(
  (count, bundle) => count + bundle.variants.length,
  0,
);
const studyTasks = studyBundles.reduce((count, bundle) => count + bundle.tasks.length, 0);
const studiedR0CaseIds = new Set(studyBundles.flatMap((bundle) => bundle.r0Cases));
const hum0Protocol = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "hum0-human-review-protocol.json"), "utf8"),
);
const hum0Slices = Array.isArray(hum0Protocol.slices) ? hum0Protocol.slices.length : 0;
const hum0Tasks = hum0Protocol.slices?.reduce(
  (total, slice) => total + (Array.isArray(slice.tasks) ? slice.tasks.length : 0),
  0,
) ?? 0;
const hum0HumanRecords = hum0Protocol.records?.human?.length ?? 0;
const hum0ModelRecords = hum0Protocol.records?.model?.length ?? 0;

const referenceDirectory = path.join(wDirectory, "reference", "last-light");
const rootReferenceSources = fs
  .readdirSync(referenceDirectory, { withFileTypes: true })
  .filter((entry) => entry.isFile() && entry.name.endsWith(".w")).length;
const allReferenceSources = recursiveFiles(referenceDirectory, (file) => file.endsWith(".w")).length;
const stdSources = recursiveFiles(path.join(wDirectory, "std"), (file) => file.endsWith(".w")).length;
const stdApiSurface = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "std-api-surface.snapshot.json"), "utf8"),
);
const stdApiModules = stdApiSurface.summary.modules;
const stdCatalogedApis = stdApiSurface.summary.catalogedApis;
const stdQualifiedReferenceSurfaces = stdApiSurface.summary.qualifiedReferenceSurfaces;
const stdReferenceRequirements = stdApiSurface.summary.referenceRequirements;
const stdContractedReferenceRequirements = stdApiSurface.summary.contractedReferenceRequirements;
const stdMissingReferenceRequirements = stdApiSurface.summary.missingReferenceRequirements;
const corpusFiles = recursiveFiles(
  path.join(wDirectory, "tooling", "tree-sitter-w", "test", "corpus"),
  (file) => file.endsWith(".txt"),
);
const corpusCases = corpusFiles.reduce((count, file) => {
  const content = fs.readFileSync(file, "utf8");
  return count + (content.match(/^---$/gm)?.length || 0);
}, 0);
const semanticCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "semantic-cases.json"), "utf8"),
);
const semanticCases = semanticCorpus.cases.length;
const semanticPositiveCases = semanticCorpus.cases.filter((testCase) => testCase.kind === "positive").length;
const semanticNegativeCases = semanticCorpus.cases.filter((testCase) => testCase.kind === "negative").length;
const semanticMatrixCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "semantic-diagnostic-matrix-cases.json"), "utf8"),
);
const semanticMatrixCases = semanticMatrixCorpus.cases.length;
const semanticMatrixAcceptedCases = semanticMatrixCorpus.cases.filter((testCase) => testCase.kind === "accepted").length;
const semanticMatrixOracleAcceptedCases = semanticMatrixCorpus.cases.filter((testCase) => testCase.kind === "accepted").length;
const semanticMatrixSemanticAcceptedCases = semanticMatrixCorpus.cases.filter((testCase) => testCase.expect?.semanticOutcome === "accepted" || (testCase.expect?.semanticOutcome === undefined && testCase.kind === "accepted")).length;
const semanticMatrixSemanticRejectedCases = semanticMatrixCorpus.cases.filter((testCase) => testCase.expect?.semanticOutcome === "rejected").length;
const semanticMatrixDecisions = new Set(semanticMatrixCorpus.cases.flatMap((testCase) => testCase.decisions ?? []));
const formatterCases = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "formatter-cases.json"), "utf8"),
).cases.length;
const memoryTransitionCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "memory-transition-cases.json"), "utf8"),
);
const memoryTransitionCases = memoryTransitionCorpus.cases.length;
const memoryTransitionOperations = memoryTransitionCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedMemoryTransitions = memoryTransitionCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const sharedControlCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "shared-control-cases.json"), "utf8"),
);
const sharedControlCases = sharedControlCorpus.cases.length;
const sharedControlOperations = sharedControlCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const sharedControlStatusCounts = Object.fromEntries(
  ["accepted", "error", "fault", "rejected"].map((status) => [
    status,
    sharedControlCorpus.cases.filter((testCase) => testCase.expected.status === status).length,
  ]),
);
const allocationCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "allocation-cases.json"), "utf8"),
);
const allocationCases = allocationCorpus.cases.length;
const allocationOperations = allocationCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedAllocationCases = allocationCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const layoutAbiCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "layout-abi-cases.json"), "utf8"),
);
const layoutAbiCases = layoutAbiCorpus.cases.length;
const layoutAbiOperations = layoutAbiCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedLayoutAbiCases = layoutAbiCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const executionConcurrencyCorpus = JSON.parse(
  fs.readFileSync(
    path.join(wDirectory, "tooling", "execution-concurrency-cases.json"),
    "utf8",
  ),
);
const executionConcurrencyCases = executionConcurrencyCorpus.cases.length;
const executionConcurrencyOperations = executionConcurrencyCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedExecutionConcurrencyCases = executionConcurrencyCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const runtimeLivenessCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "runtime-liveness-cases.json"), "utf8"),
);
const runtimeLivenessCases = runtimeLivenessCorpus.cases.length;
const runtimeLivenessOperations = runtimeLivenessCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedRuntimeLivenessCases = runtimeLivenessCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const lazyBehaviorCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "lazy-behavior-cases.json"), "utf8"),
);
const lazyFixtureOperations = Object.fromEntries(
  Object.entries(lazyBehaviorCorpus.fixtures ?? {}).map(([name, operations]) => [
    name,
    operations.length,
  ]),
);
const lazyBehaviorCases = lazyBehaviorCorpus.cases.length;
const lazyBehaviorOperations = lazyBehaviorCorpus.cases.reduce(
  (count, testCase) =>
    count +
    (testCase.operations ?? []).length +
    (testCase.fixtures ?? []).reduce(
      (fixtureCount, fixture) => fixtureCount + (lazyFixtureOperations[fixture] ?? 0),
      0,
    ),
  0,
);
const acceptedLazyBehaviorCases = lazyBehaviorCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const faultedLazyBehaviorCases = lazyBehaviorCorpus.cases.filter(
  (testCase) => testCase.kind === "fault",
).length;
const lazyBehaviorHostTests = (
  fs
    .readFileSync(path.join(wDirectory, "tooling", "lazy-behavior-reference.test.mjs"), "utf8")
    .match(/^test\(/gm) ?? []
).length;
const ownershipExecutionCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "ownership-execution-cases.json"), "utf8"),
);
const ownershipExecutionCases = ownershipExecutionCorpus.cases.length;
const ownershipExecutionOperations = ownershipExecutionCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedOwnershipExecutionCases = ownershipExecutionCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const channelCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "channel-cases.json"), "utf8"),
);
const channelCases = channelCorpus.cases.length;
const channelOperations = channelCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedChannelCases = channelCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const contextLocalCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "context-local-cases.json"), "utf8"),
);
const contextLocalCases = contextLocalCorpus.cases.length;
const contextLocalOperations = contextLocalCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedContextLocalCases = contextLocalCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const interferenceLayoutCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "interference-layout-cases.json"), "utf8"),
);
const interferenceLayoutCases = interferenceLayoutCorpus.cases.length;
const interferenceLayoutOperations = interferenceLayoutCorpus.cases.reduce(
  (count, testCase) =>
    count +
    expandInterferenceLayoutOperations(
      interferenceLayoutCorpus.fixtures,
      testCase.operations,
    ).length,
  0,
);
const acceptedInterferenceLayoutCases = interferenceLayoutCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const scopedLockCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "scoped-lock-cases.json"), "utf8"),
);
const scopedLockCases = scopedLockCorpus.cases.length;
const scopedLockOperations = scopedLockCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedScopedLockCases = scopedLockCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const faultedScopedLockCases = scopedLockCorpus.cases.filter(
  (testCase) => testCase.kind === "fault",
).length;
const snapshotCellCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "snapshot-cell-cases.json"), "utf8"),
);
const snapshotCellCases = snapshotCellCorpus.cases.length;
const snapshotCellOperations = snapshotCellCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedSnapshotCellCases = snapshotCellCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const faultedSnapshotCellCases = snapshotCellCorpus.cases.filter(
  (testCase) => testCase.kind === "fault",
).length;
const executionConcurrencySnapshots = fs
  .readFileSync(
    path.join(wDirectory, "tooling", "execution-concurrency-results.snapshot.jsonl"),
    "utf8",
  )
  .split(/\r?\n/)
  .filter(Boolean)
  .map((line) => JSON.parse(line));
const synchronizationEdgeKinds = new Set(
  executionConcurrencySnapshots.flatMap((result) =>
    result.state.edges
      .filter((edge) => edge.kind !== "sequencedBefore")
      .map((edge) => edge.kind),
  ),
);
const boundaryEffectCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "boundary-effect-cases.json"), "utf8"),
);
const boundaryEffectCases = boundaryEffectCorpus.cases.length;
const boundaryEffectOperations = boundaryEffectCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedBoundaryEffectCases = boundaryEffectCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const serviceRecoveryCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "service-recovery-cases.json"), "utf8"),
);
function serviceRecoveryOperationCount(testCase) {
  return testCase.operations.reduce((count, operation) => {
    if (operation.$use === undefined) return count + 1;
    const fixture = serviceRecoveryCorpus.fixtures[operation.$use];
    if (!Array.isArray(fixture)) {
      throw new Error(`Unknown service-recovery fixture ${operation.$use}.`);
    }
    return count + fixture.length;
  }, 0);
}
const serviceRecoveryCases = serviceRecoveryCorpus.cases.length;
const serviceRecoveryOperations = serviceRecoveryCorpus.cases.reduce(
  (count, testCase) => count + serviceRecoveryOperationCount(testCase),
  0,
);
const acceptedServiceRecoveryCases = serviceRecoveryCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const packageReleaseCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "package-release-cases.json"), "utf8"),
);
function packageReleaseFixtureOperations(name, stack = []) {
  if (stack.includes(name)) throw new Error(`Package-release fixture cycle at ${name}.`);
  const fixture = packageReleaseCorpus.fixtures[name];
  if (!fixture) throw new Error(`Unknown package-release fixture ${name}.`);
  return (
    fixture.operations.length +
    (fixture.includes ?? []).reduce(
      (count, included) =>
        count + packageReleaseFixtureOperations(included, [...stack, name]),
      0,
    )
  );
}
const packageReleaseCases = packageReleaseCorpus.cases.length;
const packageReleaseOperations = packageReleaseCorpus.cases.reduce(
  (count, testCase) =>
    count +
    testCase.operations.length +
    (testCase.fixtures ?? []).reduce(
      (fixtureCount, fixture) =>
        fixtureCount + packageReleaseFixtureOperations(fixture),
      0,
    ),
  0,
);
const acceptedPackageReleaseCases = packageReleaseCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const moduleRunCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "module-run-cases.json"), "utf8"),
);
function moduleRunFixtureOperations(name, stack = []) {
  if (stack.includes(name)) throw new Error(`Module-run fixture cycle at ${name}.`);
  const fixture = moduleRunCorpus.fixtures[name];
  if (!fixture) throw new Error(`Unknown module-run fixture ${name}.`);
  return (
    (fixture.operations ?? []).length +
    (fixture.includes ?? []).reduce(
      (count, included) =>
        count + moduleRunFixtureOperations(included, [...stack, name]),
      0,
    )
  );
}
const moduleRunCases = moduleRunCorpus.cases.length;
const moduleRunOperations = moduleRunCorpus.cases.reduce(
  (count, testCase) =>
    count +
    (testCase.operations ?? []).length +
    (testCase.fixtures ?? []).reduce(
      (fixtureCount, fixture) =>
        fixtureCount + moduleRunFixtureOperations(fixture),
      0,
    ),
  0,
);
const acceptedModuleRunCases = moduleRunCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const replSessionCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "repl-session-cases.json"), "utf8"),
);
const replSessionCases = replSessionCorpus.cases.length;
const replSessionOperations = replSessionCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedReplSessionCases = replSessionCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const presentationCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "presentation-cases.json"), "utf8"),
);
const presentationCases = presentationCorpus.cases.length;
const presentationOperations = presentationCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedPresentationCases = presentationCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const jupyterCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "jupyter-cases.json"), "utf8"),
);
const jupyterCases = jupyterCorpus.cases.length;
const jupyterOperations = jupyterCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedJupyterCases = jupyterCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const notebookExportCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "notebook-export-cases.json"), "utf8"),
);
const notebookExportCases = notebookExportCorpus.cases.length;
const notebookExportOperations = notebookExportCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedNotebookExportCases = notebookExportCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const wmetaCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "wmeta-cases.json"), "utf8"),
);
const wmetaCases = wmetaCorpus.cases.length;
const acceptedWmetaCases = wmetaCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const tabularCarrierCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "tabular-carrier-cases.json"), "utf8"),
);
const tabularCarrierCases = tabularCarrierCorpus.cases.length;
const tabularCarrierOperations = tabularCarrierCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedTabularCarrierCases = tabularCarrierCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const tabularAdapterCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "tabular-adapter-cases.json"), "utf8"),
);
const tabularAdapterCases = tabularAdapterCorpus.cases.length;
const tabularAdapterOperations = tabularAdapterCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedTabularAdapterCases = tabularAdapterCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const dlpackCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "dlpack-cases.json"), "utf8"),
);
const dlpackCases = dlpackCorpus.cases.length;
const dlpackOperations = dlpackCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedDlpackCases = dlpackCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
).length;
const deviceExecutionCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "device-execution-cases.json"), "utf8"),
);
const deviceExecutionCases = deviceExecutionCorpus.cases.length;
const deviceExecutionOperations = deviceExecutionCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.reduce(
    (caseCount, operation) =>
      caseCount + (operation.$use ? deviceExecutionCorpus.fixtures[operation.$use].length : 1),
    0,
  ),
  0,
);
const acceptedDeviceExecutionCases = deviceExecutionCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const kernelModuleCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "kernel-module-cases.json"), "utf8"),
);
const kernelModuleCases = kernelModuleCorpus.cases.length;
const kernelModuleOperations = kernelModuleCorpus.cases.reduce(
  (count, testCase) => count + countKernelModuleOperations(
    prepareKernelModuleCase(kernelModuleCorpus, testCase),
  ),
  0,
);
const acceptedKernelModuleCases = kernelModuleCorpus.cases.filter(
  (testCase) => testCase.kind === "positive",
).length;
const foreignBodyCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "foreign-body-cases.json"), "utf8"),
);
const foreignBodyCases = foreignBodyCorpus.cases.length;
const foreignBodyOperations = foreignBodyCorpus.cases.reduce(
  (count, testCase) => count + testCase.operations.length,
  0,
);
const acceptedForeignBodyCases = foreignBodyCorpus.cases.filter(
  (testCase) => testCase.kind === "accepted",
).length;
const informationForeignBodyCases = foreignBodyCorpus.cases.filter(
  (testCase) => testCase.kind === "info",
).length;
const webBodyCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "web-body-cases.json"), "utf8"),
);
const webBodyCases = webBodyCorpus.cases.length;
const webBodyOperations = webBodyCorpus.cases.reduce(
  (count, testCase) =>
    count + Object.keys({ ...testCase.input, limits: testCase.input.limits ?? webBodyCorpus.limits }).length
      + (testCase.input.entries?.length ?? 0),
  0,
);
const acceptedWebBodyCases = webBodyCorpus.cases.filter(
  (testCase) => testCase.kind === "positive",
).length;
const processRootCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "process-root-cases.json"), "utf8"),
);
const processRootCases = processRootCorpus.cases.length;
const processRootOperations = processRootCorpus.cases.reduce(
  (count, testCase) =>
    count + Object.keys(testCase.input ?? {}).length
      + (testCase.input.values?.length ?? 0)
      + (testCase.input.calls?.length ?? 0)
      + (testCase.input.events?.length ?? 0),
  0,
);
const acceptedProcessRootCases = processRootCorpus.cases.filter(
  (testCase) => testCase.kind === "positive",
).length;
const filesystemCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "filesystem-cases.json"), "utf8"),
);
const filesystemCases = filesystemCorpus.cases.length;
const filesystemOperations = filesystemCorpus.cases.reduce(
  (count, testCase) => {
    const input = testCase.input ?? {};
    const arrays = [
      input.rights,
      input.parentRights,
      input.childRights,
      input.bytes,
      input.source,
      input.concurrentPrefix,
      input.steps,
      input.entries,
      input.events,
      input.operations,
      input.happensBefore,
      input.path?.steps,
      input.sourcePath?.steps,
      input.destinationPath?.steps,
    ];
    return count + Object.keys(input).length + Object.keys(input.rootLimits ?? {}).length + arrays.reduce(
      (total, value) => total + (Array.isArray(value) ? value.length : 0),
      0,
    );
  },
  0,
);
const acceptedFilesystemCases = filesystemCorpus.cases.filter(
  (testCase) => testCase.kind === "positive",
).length;
const ioErrorCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "io-error-cases.json"), "utf8"),
);
const ioErrorCases = ioErrorCorpus.cases.length;
const ioErrorOperations = ioErrorCorpus.cases.reduce(
  (count, testCase) => {
    const input = testCase.input ?? {};
    return count + Object.keys(input).length + Object.keys(input.cause ?? {}).length
      + (input.helperOperations?.length ?? 0);
  },
  0,
);
const acceptedIoErrorCases = ioErrorCorpus.cases.filter(
  (testCase) => testCase.kind === "positive",
).length;
const operationalTimeCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "operational-time-cases.json"), "utf8"),
);
const operationalTimeCases = operationalTimeCorpus.cases.length;
const operationalTimeOperations = operationalTimeCorpus.cases.reduce(
  (count, testCase) => {
    const input = testCase.input ?? {};
    return count + Object.keys(input).length
      + (input.capabilities?.length ?? 0)
      + (input.samples?.length ?? 0)
      + (input.advances?.length ?? 0);
  },
  0,
);
const acceptedOperationalTimeCases = operationalTimeCorpus.cases.filter(
  (testCase) => testCase.kind === "positive",
).length;
const diagnosticSnapshots = fs
  .readFileSync(path.join(wDirectory, "tooling", "semantic-diagnostics.snapshot.jsonl"), "utf8")
  .split(/\r?\n/)
  .filter(Boolean).length;
const formatterDiagnosticSnapshots = fs
  .readFileSync(path.join(wDirectory, "tooling", "formatter-diagnostics.snapshot.jsonl"), "utf8")
  .split(/\r?\n/)
  .filter(Boolean).length;
const semanticResultSnapshots = fs
  .readFileSync(path.join(wDirectory, "tooling", "semantic-results.snapshot.jsonl"), "utf8")
  .split(/\r?\n/)
  .filter(Boolean).length;
const diagnosticCatalog = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "diagnostic-catalog.json"), "utf8"),
);
const diagnosticCatalogCount = diagnosticCatalog.codes.length;
const diagnosticCatalogCodes = new Set(diagnosticCatalog.codes.map((entry) => entry.code));
const referencedDiagnosticCodes = [
  ...new Set([...designText.matchAll(/\b(W-[A-Z]+-[0-9]{4})\b/g)].map((match) => match[1])),
].sort((left, right) => Buffer.from(left).compare(Buffer.from(right)));
const referencedDiagnosticCount = referencedDiagnosticCodes.length;
const diagnosticFamilies = new Map();
for (const code of referencedDiagnosticCodes) {
  const family = code.split("-")[1];
  const current = diagnosticFamilies.get(family) ?? { referenced: 0, cataloged: 0 };
  current.referenced += 1;
  if (diagnosticCatalogCodes.has(code)) {
    current.cataloged += 1;
  }
  diagnosticFamilies.set(family, current);
}
const normativeGrammarSlices = headings.filter((heading) =>
  /\bGrammar normativa G\d+/.test(heading.title),
).length;

const output = [];
output.push("# Índice gerado do design W");
output.push("");
output.push("> Gerado por `tooling/design-index.mjs`. Não edite este arquivo.");
output.push("> `DESIGN.md` continua sendo a única fonte normativa. `RATIONALE.md` é complementar e não normativo.");
output.push("");
output.push("## Contexto mínimo");
output.push("");
output.push("1. Leia este índice para localizar a seção necessária.");
output.push("2. Leia somente o intervalo correspondente em `DESIGN.md`.");
output.push("3. Use `RATIONALE.md` somente para IDs, evidência, alternativas e proveniência.");
output.push("4. Busque o ID W quando a tarefa alterar uma decisão.");
output.push("5. Abra o produto Última Luz somente para o exemplo afetado.");
output.push("6. Não leia `tooling/tree-sitter-w/src/` como source. Essa pasta é gerada.");
output.push("");
output.push("## Snapshot calculado");
output.push("");
output.push("| Métrica | Valor |");
output.push("|---|---:|");
output.push(`| linhas de \`DESIGN.md\` | ${lines.length} |`);
output.push(`| tokens aproximados de \`DESIGN.md\` | ${approximateTokens(designText)} |`);
output.push(`| linhas de \`RATIONALE.md\` | ${rationaleLines.length} |`);
output.push(`| tokens aproximados de \`RATIONALE.md\` | ${approximateTokens(rationaleText)} |`);
output.push(`| seções numeradas | ${numberedSections.length} |`);
output.push(`| seções terminais com evidência local | ${evidencedLeafCount}/${leafCount} |`);
output.push(
  `| decisões | ${decisions.length} (W-${String(decisions[0]).padStart(3, "0")}–W-${String(decisions.at(-1)).padStart(3, "0")}) |`,
);
output.push(`| famílias de viabilidade | ${viabilityRows.length} |`);
output.push(`| slices normativos de grammar | ${normativeGrammarSlices} |`);
output.push(`| requisitos de ratificação comparativa | ${comparisonCount} |`);
output.push(
  `| casos de substituição estruturados | ${structuredSubstitutionCases}/${comparisonCount} |`,
);
output.push(
  `| decisões referenciadas por casos R0 | ${substitutionDecisionIds.size}/${decisions.length} |`,
);
output.push(
  `| decisões classificadas para design freeze | ${designFreezeEntries.length}/${decisions.length} (` +
    [...designFreezeCategoryCounts.entries()].map(([category, count]) => `${count} ${category}`).join("; ") + ") |",
);
output.push(
  `| decisões com evidência legada de fonte/oráculo | ${classifiedFreezeDecisionIds.size}/${decisions.length} ` +
    `(${substitutionDecisionIds.size} source + ${oracleFreezeDecisionIds.size} oracle + ${explicitFreezeDecisionIds.size} explícitas; ${freezeEvidenceOverlaps} overlaps) |`,
);
output.push(
  `| decisões ainda sem classe de freeze | ${decisions.length - designFreezeEntries.length} |`,
);
output.push(`| decisões com múltiplos eixos obrigatórios | ${multiAxisFreezeRequirements} |`);
output.push(`| formas R0 com baseline estática | ${measuredSubstitutionForms} |`);
output.push(
  `| surface lexemes das formas vigentes R0 | ${selectedSurfaceLexemeTotal} total; mediana ${selectedSurfaceLexemeMedian}; máximo ${selectedSurfaceLexemeMaximum} |`,
);
output.push(`| bundles executáveis R1 | ${studyBundles.length} |`);
output.push(`| variantes/tarefas R1 | ${studyVariants}/${studyTasks} |`);
output.push(
  `| casos R0 promovidos a R1 | ${studiedR0CaseIds.size}/${structuredSubstitutionCases} |`,
);
output.push(
  `| protocolo HUM0 | ${hum0Slices} slices/${hum0Tasks} tasks; ${hum0HumanRecords} human records/${hum0ModelRecords} model records; structure-only |`,
);
output.push(`| casos do corpus Tree-sitter | ${corpusCases} |`);
output.push(`| pares canônicos do formatter F0 | ${formatterCases} |`);
output.push(
  `| casos/operações do kernel de memória M1 | ${memoryTransitionCases}/${memoryTransitionOperations} (${acceptedMemoryTransitions} aceitos + ${memoryTransitionCases - acceptedMemoryTransitions} rejeitados) |`,
);
output.push(
  `| casos/operações do control block shared SHC0 | ${sharedControlCases}/${sharedControlOperations} (${sharedControlStatusCounts.accepted} aceitos + ${sharedControlStatusCounts.error} errors + ${sharedControlStatusCounts.fault} faults + ${sharedControlStatusCounts.rejected} rejeitados) |`,
);
output.push(
  `| casos/operações do kernel de allocation físico A0 | ${allocationCases}/${allocationOperations} (${acceptedAllocationCases} aceitos + ${allocationCases - acceptedAllocationCases} rejeitados) |`,
);
output.push(
  `| casos/operações do kernel de layout e ABI L0 | ${layoutAbiCases}/${layoutAbiOperations} (${acceptedLayoutAbiCases} aceitos + ${layoutAbiCases - acceptedLayoutAbiCases} rejeitados) |`,
);
output.push(
  "| casos/operações do kernel de execução E0 | " +
    `${executionConcurrencyCases}/${executionConcurrencyOperations} ` +
    `(${acceptedExecutionConcurrencyCases} aceitos + ` +
    `${executionConcurrencyCases - acceptedExecutionConcurrencyCases} rejeitados; ` +
    `${synchronizationEdgeKinds.size}/10 origens happens-before) |`,
);
output.push(
  `| casos/operações do kernel de runtime closure E1 | ` +
    `${runtimeLivenessCases}/${runtimeLivenessOperations} ` +
    `(${acceptedRuntimeLivenessCases} aceitos + ` +
    `${runtimeLivenessCases - acceptedRuntimeLivenessCases} rejeitados; ` +
    `sete testes host) |`,
);
output.push(
  `| casos/operações do behavior Lazy LZ0 | ` +
    `${lazyBehaviorCases}/${lazyBehaviorOperations} ` +
    `(${acceptedLazyBehaviorCases} aceitos + ` +
    `${lazyBehaviorCases - acceptedLazyBehaviorCases - faultedLazyBehaviorCases} rejeitados + ` +
    `${faultedLazyBehaviorCases} fault; ${lazyBehaviorHostTests} testes host) |`,
);
output.push(
  `| casos/operações da composição de ownership e execução MX0 | ` +
    `${ownershipExecutionCases}/${ownershipExecutionOperations} ` +
    `(${acceptedOwnershipExecutionCases} aceitos + ` +
    `${ownershipExecutionCases - acceptedOwnershipExecutionCases} rejeitados; ` +
    `14 testes host) |`,
);
output.push(
  `| casos/operações de channel bounded CH0 | ` +
    `${channelCases}/${channelOperations} ` +
    `(${acceptedChannelCases} aceitos + ` +
    `${channelCases - acceptedChannelCases} rejeitados; 12 testes host) |`,
);
output.push(
  `| casos/operações de contexto local CTX0 | ` +
    `${contextLocalCases}/${contextLocalOperations} ` +
    `(${acceptedContextLocalCases} aceitos + ` +
    `${contextLocalCases - acceptedContextLocalCases} rejeitados; seis testes host) |`,
);
output.push(
  `| casos/operações de layout de interferência IL0 | ` +
    `${interferenceLayoutCases}/${interferenceLayoutOperations} ` +
    `(${acceptedInterferenceLayoutCases} aceitos + ` +
    `${interferenceLayoutCases - acceptedInterferenceLayoutCases} rejeitados; ` +
    `nove testes host) |`,
);
output.push(
  `| casos/operações de lock da linguagem LM1 | ` +
    `${scopedLockCases}/${scopedLockOperations} ` +
    `(${acceptedScopedLockCases} aceitos + ` +
    `${scopedLockCases - acceptedScopedLockCases - faultedScopedLockCases} rejeitados + ` +
    `${faultedScopedLockCases} fault; 11 testes host) |`,
);
output.push(
  `| casos/operações do carrier de snapshot SP0 | ` +
    `${snapshotCellCases}/${snapshotCellOperations} ` +
    `(${acceptedSnapshotCellCases} aceitos + ` +
    `${snapshotCellCases - acceptedSnapshotCellCases - faultedSnapshotCellCases} rejeitados + ` +
    `${faultedSnapshotCellCases} fault; sete testes host) |`,
);
output.push(
  `| casos/operações do kernel de boundary effects B0 | ` +
    `${boundaryEffectCases}/${boundaryEffectOperations} ` +
    `(${acceptedBoundaryEffectCases} aceitos + ` +
    `${boundaryEffectCases - acceptedBoundaryEffectCases} rejeitados) |`,
);
output.push(
  `| casos/operações de service recovery SR0 | ` +
    `${serviceRecoveryCases}/${serviceRecoveryOperations} ` +
    `(${acceptedServiceRecoveryCases} aceitos + ` +
    `${serviceRecoveryCases - acceptedServiceRecoveryCases} rejeitados; ` +
    `17 testes host) |`,
);
output.push(
  `| casos/operações do kernel de packages e releases P0 | ` +
    `${packageReleaseCases}/${packageReleaseOperations} ` +
    `(${acceptedPackageReleaseCases} aceitos + ` +
    `${packageReleaseCases - acceptedPackageReleaseCases} rejeitados) |`,
);
output.push(
  `| casos/operações do workflow module-run RU0 | ` +
    `${moduleRunCases}/${moduleRunOperations} ` +
    `(${acceptedModuleRunCases} aceitos + ` +
    `${moduleRunCases - acceptedModuleRunCases} rejeitados) |`,
);
output.push(
  `| casos/operações da sessão transacional PYN2 | ` +
    `${replSessionCases}/${replSessionOperations} ` +
    `(${acceptedReplSessionCases} aceitos + ` +
    `${replSessionCases - acceptedReplSessionCases} rejeitados) |`,
);
output.push(
  `| casos/operações de apresentação PYN3 | ${presentationCases}/${presentationOperations} ` +
    `(${acceptedPresentationCases} aceitos + ` +
    `${presentationCases - acceptedPresentationCases} rejeitados; host oracle não executa W) |`,
);
output.push(
  `| casos/operações do adapter Jupyter PYN3 | ${jupyterCases}/${jupyterOperations} ` +
    `(${acceptedJupyterCases} aceitos + ` +
    `${jupyterCases - acceptedJupyterCases} rejeitados; host oracle não executa W) |`,
);
output.push(
  `| casos/operações do export notebook PYN3 | ${notebookExportCases}/${notebookExportOperations} ` +
    `(${acceptedNotebookExportCases} aceitos + ` +
    `${notebookExportCases - acceptedNotebookExportCases} rejeitados; host oracle não executa W) |`,
);
output.push(
  `| casos do container WMeta1 W0 | ${wmetaCases} ` +
    `(${acceptedWmetaCases} aceitos + ${wmetaCases - acceptedWmetaCases} rejeitados; ` +
    `2 readers independentes) |`,
);
output.push(
  `| casos/operações do carrier tabular TAB0 | ${tabularCarrierCases}/${tabularCarrierOperations} ` +
    `(${acceptedTabularCarrierCases} aceitos + ` +
    `${tabularCarrierCases - acceptedTabularCarrierCases} rejeitados; ` +
    `host oracle não executa W) |`,
);
output.push(
  `| casos/operações dos adapters tabulares TAB1 | ${tabularAdapterCases}/${tabularAdapterOperations} ` +
    `(${acceptedTabularAdapterCases} aceitos + ` +
    `${tabularAdapterCases - acceptedTabularAdapterCases} rejeitados; ` +
    `host oracle não executa W) |`,
);
output.push(
  `| casos/operações do carrier DLPack PYN4 | ${dlpackCases}/${dlpackOperations} ` +
    `(${acceptedDlpackCases} aceitos + ${dlpackCases - acceptedDlpackCases} rejeitados; ` +
    `host oracle não executa W) |`,
);
output.push(
  `| casos/operações de device execution DEV0 | ` +
    `${deviceExecutionCases}/${deviceExecutionOperations} ` +
    `(${acceptedDeviceExecutionCases} aceitos + ` +
    `${deviceExecutionCases - acceptedDeviceExecutionCases} rejeitados; ` +
    `host oracle não executa W) |`,
);
output.push(
  `| casos/operações da síntese de kernel KM0 | ` +
    `${kernelModuleCases}/${kernelModuleOperations} ` +
    `(${acceptedKernelModuleCases} aceitos + ` +
    `${kernelModuleCases - acceptedKernelModuleCases} rejeitados; ` +
    `host oracle não executa W) |`,
);
output.push(
  `| casos/operações de body estrangeiro FB0 | ` +
    `${foreignBodyCases}/${foreignBodyOperations} ` +
    `(${acceptedForeignBodyCases} aceitos + ` +
    `${foreignBodyCases - acceptedForeignBodyCases - informationForeignBodyCases} rejeitados + ` +
    `${informationForeignBodyCases} informações; host oracle não executa adapter) |`,
);
output.push(
  `| casos/operações de Web bodies WB0 | ${webBodyCases}/${webBodyOperations} ` +
    `(${acceptedWebBodyCases} aceitos + ${webBodyCases - acceptedWebBodyCases} rejeitados; ` +
    `host oracle não executa compiler/provider) |`,
);
output.push(
  `| casos/operações do root de processo PR0 | ${processRootCases}/${processRootOperations} ` +
    `(${acceptedProcessRootCases} aceitos + ${processRootCases - acceptedProcessRootCases} rejeitados; ` +
    `host oracle não executa W/provider) |`,
);
output.push(
  `| casos/operações do filesystem FS0 | ${filesystemCases}/${filesystemOperations} ` +
    `(${acceptedFilesystemCases} aceitos + ${filesystemCases - acceptedFilesystemCases} rejeitados; ` +
    `host oracle não executa syscalls/provider) |`,
);
output.push(
  `| casos/operações de erro portátil de I/O IOE0 | ${ioErrorCases}/${ioErrorOperations} ` +
    `(${acceptedIoErrorCases} aceitos + ${ioErrorCases - acceptedIoErrorCases} rejeitados; ` +
    `host oracle não executa W/provider) |`,
);
output.push(
  `| casos/operações de tempo operacional TIME0 | ${operationalTimeCases}/${operationalTimeOperations} ` +
    `(${acceptedOperationalTimeCases} aceitos + ` +
    `${operationalTimeCases - acceptedOperationalTimeCases} rejeitados; ` +
    `host oracle não executa clock/timer/provider) |`,
);
output.push(
  `| casos do corpus semântico S0 | ${semanticCases} (${semanticPositiveCases} positivos + ${semanticNegativeCases} negativos) |`,
);
output.push(
  `| matriz host SDM0 | ${semanticMatrixCases} (${semanticMatrixOracleAcceptedCases} oracle aceitos + ${semanticMatrixCases - semanticMatrixOracleAcceptedCases} oracle rejeitados; ${semanticMatrixSemanticAcceptedCases} outcomes aceitos + ${semanticMatrixSemanticRejectedCases} rejeitados; ${semanticMatrixDecisions.size} decisões) |`,
);
output.push(`| outcomes SemanticResult S0 | ${semanticResultSnapshots} |`);
output.push(`| snapshots de diagnostic D0 | ${diagnosticSnapshots} |`);
output.push(`| snapshots F0 no formato D0 | ${formatterDiagnosticSnapshots} |`);
output.push(`| codes D0 catalogados | ${diagnosticCatalogCount}/${referencedDiagnosticCount} |`);
output.push(`| sources W no root do Última Luz | ${rootReferenceSources} |`);
output.push(`| sources W em todo o Última Luz | ${allReferenceSources} |`);
output.push(`| sources W no rascunho da std | ${stdSources} |`);
output.push(`| módulos/APIs catalogados da std | ${stdApiModules}/${stdCatalogedApis} |`);
output.push(`| superfícies qualificadas da std usadas pelo Última Luz | ${stdQualifiedReferenceSurfaces} |`);
output.push(
  `| requisitos do Última Luz com contrato std | ${stdContractedReferenceRequirements}/${stdReferenceRequirements} |`,
);
output.push(
  `| requisitos do Última Luz ausentes na std | ${stdMissingReferenceRequirements}/${stdReferenceRequirements} |`,
);
output.push("");
output.push("A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.");
output.push("");
output.push("## Cobertura do catálogo D0");
output.push("");
output.push("| Família | Catalogados | Referenciados |");
output.push("|---|---:|---:|");
for (const [family, counts] of [...diagnosticFamilies].sort(([left], [right]) =>
  Buffer.from(left).compare(Buffer.from(right)),
)) {
  output.push(`| ${family} | ${counts.cataloged} | ${counts.referenced} |`);
}
output.push("");
output.push("## Navegação por seção");
output.push("");
output.push("| Seção | Linhas | Tokens aproximados | Tema |");
output.push("|---:|---:|---:|---|");

for (const section of numberedSections) {
  output.push(
    `| ${section.number} | ${section.start}–${section.end} | ${section.tokens} | ${tableCell(section.title)} |`,
  );
}

output.push("");
output.push("## Navegação compacta de RATIONALE");
output.push("");
output.push("| Seção | Linhas | Tokens aproximados | Tema |");
output.push("|---:|---:|---:|---|");
for (const section of rationaleSections) {
  output.push(
    `| ${section.number} | ${section.start}–${section.end} | ${section.tokens} | ${tableCell(section.title)} |`,
  );
}

output.push("");
output.push("## Bundles de leitura");
output.push("");
output.push(
  "Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.",
);
output.push("");
output.push("| Bundle | Seções | Linhas | Tokens aproximados | Foco |");
output.push("|---|---:|---:|---:|---|");

for (const bundle of readingBundles.map(bundleStats)) {
  output.push(
    `| ${tableCell(bundle.name)} | ${bundle.sections.join(", ")} | ${bundle.start}–${bundle.end} | ${bundle.tokens} | ${tableCell(bundle.purpose)} |`,
  );
}

output.push("");
output.push("O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.");

output.push("");
output.push("## Classificação de viabilidade");
output.push("");
output.push("| Classe | Famílias |");
output.push("|---|---:|");

for (const [classification, count] of [...viabilityCounts.entries()].sort()) {
  output.push(`| ${tableCell(classification)} | ${count} |`);
}

output.push("");
output.push("## Pesquisas explícitas");
output.push("");

const explicitResearch = viabilityRows.filter((row) => row.classification.startsWith("Pesquisa"));

for (const row of explicitResearch) {
  output.push(`- ${row.family} — ${row.classification}`);
}

if (explicitResearch.length === 0) {
  output.push("- Nenhuma família sem classificação de viabilidade.");
}

output.push("");
output.push("## Comandos de leitura");
output.push("");
output.push("```powershell");
output.push("bun tooling/design-slice.mjs --section 12");
output.push("bun tooling/design-slice.mjs --heading 12.13");
output.push("bun tooling/design-slice.mjs --id W-711 --context 2");
output.push("bun tooling/design-slice.mjs --rationale-heading 1.3");
output.push("rg -n -C 4 'transaction' DESIGN.md");
output.push("bun tooling/design-index.mjs --check");
output.push("```");
output.push("");

const generated = `${output.join("\n")}\n`;
const mode = process.argv[2] || "--check";

if (mode === "--write") {
  fs.writeFileSync(indexPath, generated);
  process.stdout.write(`Updated ${path.relative(process.cwd(), indexPath)}\n`);
} else if (mode === "--check") {
  const current = fs.existsSync(indexPath) ? fs.readFileSync(indexPath, "utf8") : "";

  if (current !== generated) {
    process.stderr.write("DESIGN-INDEX.md is stale. Run: bun tooling/design-index.mjs --write\n");
    process.exitCode = 1;
  } else {
    process.stdout.write("Design index is current.\n");
  }
} else {
  process.stderr.write("Usage: bun tooling/design-index.mjs --write|--check\n");
  process.exitCode = 2;
}
