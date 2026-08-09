import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const designPath = path.join(wDirectory, "DESIGN.md");
const indexPath = path.join(wDirectory, "DESIGN-INDEX.md");
const designText = fs.readFileSync(designPath, "utf8");
const lines = designText.split(/\r?\n/);

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
    name: "SDK e performance",
    sections: [14, 15, 16, 17, 18, 19],
    purpose: "tiers, números, texto, tensors, custo, C e unsafe",
  },
  {
    name: "compiler e distribuição",
    sections: [20, 21, 22, 23],
    purpose: "frontend, HIR, packages, releases, tooling e protocolos",
  },
  {
    name: "validação e decisões",
    sections: [24, 25, 26, 27, 28, 29],
    purpose: "viabilidade, Última Luz, gates, roadmap e ledger",
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

const decisions = [];

for (const line of lines) {
  const match = /^\| W-(\d{3,}) \|/.exec(line);

  if (match) {
    decisions.push(Number(match[1]));
  }
}

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

const viabilityStart = numberedSections.find((section) => section.number === 24)?.start;
const viabilitySectionEnd = numberedSections.find((section) => section.number === 25)?.start;
const viabilitySubsectionStart = lines.findIndex(
  (line, index) => index + 1 > viabilityStart && line.startsWith("### 24."),
);
const viabilityEnd =
  viabilitySubsectionStart >= 0 ? viabilitySubsectionStart + 1 : viabilitySectionEnd;
const viabilityRows = [];

if (!viabilityStart || !viabilityEnd) {
  structuralErrors.push("Sections 24 and 25 are required for viability metrics.");
}

for (const line of lines.slice(viabilityStart, viabilityEnd - 1)) {
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

const reviewStart = lines.findIndex((line) => line === "O corpus compara, no mínimo:");
const reviewCoverageStart = lines.findIndex(
  (line) => line === "### 26.1 Cobertura de substituições",
);

if (reviewStart < 0 || reviewCoverageStart <= reviewStart) {
  structuralErrors.push("The section 26 comparison list and coverage heading are required.");
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

const comparisonCount = lines
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
  "boundary-effect-cases.json",
  "package-release-cases.json",
  "wmeta-cases.json",
];
const oracleFreezeDecisionIds = new Set(
  oracleCorpusFiles.flatMap((file) => {
    const corpus = JSON.parse(
      fs.readFileSync(path.join(wDirectory, "tooling", file), "utf8"),
    );
    return corpus.cases.flatMap((testCase) => testCase.decisions ?? []);
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
const wmetaCorpus = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "wmeta-cases.json"), "utf8"),
);
const wmetaCases = wmetaCorpus.cases.length;
const acceptedWmetaCases = wmetaCorpus.cases.filter(
  (testCase) => testCase.expected.status === "accepted",
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
output.push("> `DESIGN.md` continua sendo a única fonte de verdade.");
output.push("");
output.push("## Contexto mínimo");
output.push("");
output.push("1. Leia este índice para localizar a seção necessária.");
output.push("2. Leia somente o intervalo correspondente em `DESIGN.md`.");
output.push("3. Busque o ID W quando a tarefa alterar uma decisão.");
output.push("4. Abra o produto Última Luz somente para o exemplo afetado.");
output.push("5. Não leia `tooling/tree-sitter-w/src/` como source. Essa pasta é gerada.");
output.push("");
output.push("## Snapshot calculado");
output.push("");
output.push("| Métrica | Valor |");
output.push("|---|---:|");
output.push(`| linhas de \`DESIGN.md\` | ${lines.length} |`);
output.push(`| tokens aproximados de \`DESIGN.md\` | ${approximateTokens(designText)} |`);
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
  `| decisões classificadas para design freeze | ${classifiedFreezeDecisionIds.size}/${decisions.length} (${substitutionDecisionIds.size} source + ${oracleFreezeDecisionIds.size} oracle + ${explicitFreezeDecisionIds.size} explícitas; ${freezeEvidenceOverlaps} overlaps) |`,
);
output.push(
  `| decisões ainda sem classe de freeze | ${decisions.length - classifiedFreezeDecisionIds.size} |`,
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
output.push(`| casos do corpus Tree-sitter | ${corpusCases} |`);
output.push(`| pares canônicos do formatter F0 | ${formatterCases} |`);
output.push(
  `| casos/operações do kernel de memória M1 | ${memoryTransitionCases}/${memoryTransitionOperations} (${acceptedMemoryTransitions} aceitos + ${memoryTransitionCases - acceptedMemoryTransitions} rejeitados) |`,
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
    `${synchronizationEdgeKinds.size}/8 origens happens-before) |`,
);
output.push(
  `| casos/operações do kernel de runtime closure E1 | ` +
    `${runtimeLivenessCases}/${runtimeLivenessOperations} ` +
    `(${acceptedRuntimeLivenessCases} aceitos + ` +
    `${runtimeLivenessCases - acceptedRuntimeLivenessCases} rejeitados; ` +
    `sete testes host) |`,
);
output.push(
  `| casos/operações do kernel de boundary effects B0 | ` +
    `${boundaryEffectCases}/${boundaryEffectOperations} ` +
    `(${acceptedBoundaryEffectCases} aceitos + ` +
    `${boundaryEffectCases - acceptedBoundaryEffectCases} rejeitados) |`,
);
output.push(
  `| casos/operações do kernel de packages e releases P0 | ` +
    `${packageReleaseCases}/${packageReleaseOperations} ` +
    `(${acceptedPackageReleaseCases} aceitos + ` +
    `${packageReleaseCases - acceptedPackageReleaseCases} rejeitados) |`,
);
output.push(
  `| casos do container WMeta1 W0 | ${wmetaCases} ` +
    `(${acceptedWmetaCases} aceitos + ${wmetaCases - acceptedWmetaCases} rejeitados; ` +
    `2 readers independentes) |`,
);
output.push(
  `| casos do corpus semântico S0 | ${semanticCases} (${semanticPositiveCases} positivos + ${semanticNegativeCases} negativos) |`,
);
output.push(`| outcomes SemanticResult S0 | ${semanticResultSnapshots} |`);
output.push(`| snapshots de diagnostic D0 | ${diagnosticSnapshots} |`);
output.push(`| snapshots F0 no formato D0 | ${formatterDiagnosticSnapshots} |`);
output.push(`| codes D0 catalogados | ${diagnosticCatalogCount}/${referencedDiagnosticCount} |`);
output.push(`| sources W no root do Última Luz | ${rootReferenceSources} |`);
output.push(`| sources W em todo o Última Luz | ${allReferenceSources} |`);
output.push(`| sources W no rascunho da std | ${stdSources} |`);
output.push(`| módulos/APIs catalogados da std SDK0 | ${stdApiModules}/${stdCatalogedApis} |`);
output.push(`| superfícies qualificadas da std usadas pelo Última Luz | ${stdQualifiedReferenceSurfaces} |`);
output.push(
  `| requisitos do Última Luz com contrato std SDK0 | ${stdContractedReferenceRequirements}/${stdReferenceRequirements} |`,
);
output.push(
  `| requisitos do Última Luz ausentes na std SDK0 | ${stdMissingReferenceRequirements}/${stdReferenceRequirements} |`,
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
