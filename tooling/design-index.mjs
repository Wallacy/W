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
const viabilityEnd = numberedSections.find((section) => section.number === 25)?.start;
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
output.push(`| comparações de revisão ainda previstas | ${comparisonCount} |`);
output.push(`| casos do corpus Tree-sitter | ${corpusCases} |`);
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

for (const row of viabilityRows.filter((row) => row.classification.startsWith("Pesquisa"))) {
  output.push(`- ${row.family} — ${row.classification}`);
}

output.push("");
output.push("## Comandos de leitura");
output.push("");
output.push("```powershell");
output.push("rg -n '^## 12\\.|^### 12\\.' W/DESIGN.md");
output.push("rg -n 'W-688' W/DESIGN.md");
output.push("rg -n -C 4 'transaction' W/DESIGN.md");
output.push("node W/tooling/design-index.mjs --check");
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
    process.stderr.write("DESIGN-INDEX.md is stale. Run: node W/tooling/design-index.mjs --write\n");
    process.exitCode = 1;
  } else {
    process.stdout.write("Design index is current.\n");
  }
} else {
  process.stderr.write("Usage: node W/tooling/design-index.mjs --write|--check\n");
  process.exitCode = 2;
}
