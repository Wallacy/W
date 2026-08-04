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
const implementationSection = numberedSections.find((section) => section.number === 27);
const implementationStart = implementationSection ? implementationSection.start - 1 : -1;

if (reviewStart < 0 || implementationStart < 0) {
  structuralErrors.push("Sections 26 and 27 are required for review metrics.");
}

if (structuralErrors.length > 0) {
  process.stderr.write(`${structuralErrors.join("\n")}\n`);
  process.exit(1);
}

const comparisonCount = lines
  .slice(reviewStart + 1, implementationStart)
  .filter((line) => line.startsWith("- ")).length;

const referenceDirectory = path.join(wDirectory, "reference", "last-light");
const rootReferenceSources = fs
  .readdirSync(referenceDirectory, { withFileTypes: true })
  .filter((entry) => entry.isFile() && entry.name.endsWith(".w")).length;
const allReferenceSources = recursiveFiles(referenceDirectory, (file) => file.endsWith(".w")).length;
const stdSources = recursiveFiles(path.join(wDirectory, "std"), (file) => file.endsWith(".w")).length;
const corpusFiles = recursiveFiles(
  path.join(wDirectory, "tooling", "tree-sitter-w", "test", "corpus"),
  (file) => file.endsWith(".txt"),
);
const corpusCases = corpusFiles.reduce((count, file) => {
  const content = fs.readFileSync(file, "utf8");
  return count + (content.match(/^---$/gm)?.length || 0);
}, 0);
const semanticCases = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "semantic-cases.json"), "utf8"),
).cases.length;
const diagnosticSnapshots = fs
  .readFileSync(path.join(wDirectory, "tooling", "semantic-diagnostics.snapshot.jsonl"), "utf8")
  .split(/\r?\n/)
  .filter(Boolean).length;
const diagnosticCatalogCount = JSON.parse(
  fs.readFileSync(path.join(wDirectory, "tooling", "diagnostic-catalog.json"), "utf8"),
).codes.length;
const referencedDiagnosticCount = new Set(
  [...designText.matchAll(/\b(W-[A-Z]+-[0-9]{4})\b/g)].map((match) => match[1]),
).size;
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
output.push(`| casos de ratificação comparativa | ${comparisonCount} |`);
output.push(`| casos do corpus Tree-sitter | ${corpusCases} |`);
output.push(`| casos do corpus semântico S0 | ${semanticCases} |`);
output.push(`| snapshots de diagnostic D0 | ${diagnosticSnapshots} |`);
output.push(`| codes D0 catalogados | ${diagnosticCatalogCount}/${referencedDiagnosticCount} |`);
output.push(`| sources W no root do Última Luz | ${rootReferenceSources} |`);
output.push(`| sources W em todo o Última Luz | ${allReferenceSources} |`);
output.push(`| sources W no rascunho da std | ${stdSources} |`);
output.push("");
output.push("A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.");
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
