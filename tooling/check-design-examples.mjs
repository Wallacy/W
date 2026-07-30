import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const designPath = path.resolve(toolingDirectory, "..", "DESIGN.md");
const lines = fs.readFileSync(designPath, "utf8").split(/\r?\n/);
const headings = [];

for (let index = 0; index < lines.length; index += 1) {
  const match = /^(#{2,4})\s+(.+)$/.exec(lines[index]);

  if (match) {
    headings.push({
      line: index + 1,
      level: match[1].length,
      title: match[2],
    });
  }
}

const gaps = [];
let leafCount = 0;

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

  if (!hasEvidence) {
    gaps.push(`${heading.line}: ${heading.title}`);
  }
}

if (gaps.length > 0) {
  process.stderr.write("Design sections without a local example:\n");
  process.stderr.write(`${gaps.join("\n")}\n`);
  process.exitCode = 1;
} else {
  process.stdout.write(`Design example coverage: ${leafCount}/${leafCount} leaf sections.\n`);
}
