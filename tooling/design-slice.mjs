import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIdSet } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const designPath = path.resolve(toolingDirectory, "..", "DESIGN.md");
const rationalePath = path.resolve(toolingDirectory, "..", "RATIONALE.md");
const lines = fs.readFileSync(designPath, "utf8").split(/\r?\n/);
const rationaleLines = fs.readFileSync(rationalePath, "utf8").split(/\r?\n/);

function usage(message) {
  if (message) {
    process.stderr.write(`${message}\n\n`);
  }

  process.stderr.write(
    [
      "Usage:",
      "  bun tooling/design-slice.mjs --section 12",
      "  bun tooling/design-slice.mjs --heading 12.13",
      "  bun tooling/design-slice.mjs --id W-711 [--context 2]",
      "  bun tooling/design-slice.mjs --rationale-heading 1.3",
      "",
      "The command reads DESIGN.md or RATIONALE.md and writes only the requested interval.",
      "It never edits the canonical document.",
    ].join("\n") + "\n",
  );
  process.exit(2);
}

function argument(name) {
  const index = process.argv.indexOf(name);
  return index < 0 ? undefined : process.argv[index + 1];
}

function headingAt(sourceLines, lineIndex) {
  const match = /^(#{2,4})\s+(.+)$/.exec(sourceLines[lineIndex]);
  return match ? { level: match[1].length, title: match[2] } : undefined;
}

function intervalForHeading(sourceLines, lineIndex) {
  const heading = headingAt(sourceLines, lineIndex);

  if (!heading) {
    return undefined;
  }

  let end = sourceLines.length;

  for (let index = lineIndex + 1; index < sourceLines.length; index += 1) {
    const candidate = headingAt(sourceLines, index);

    if (candidate && candidate.level <= heading.level) {
      end = index;
      break;
    }
  }

  return { start: lineIndex, end };
}

function printInterval(sourceLines, interval, label) {
  const content = sourceLines.slice(interval.start, interval.end).join("\n");
  process.stdout.write(`<!-- design-slice: ${label} lines ${interval.start + 1}-${interval.end} -->\n`);
  process.stdout.write(`${content}\n`);
}

const section = argument("--section");
const heading = argument("--heading");
const id = argument("--id");
const rationaleHeading = argument("--rationale-heading");

if ([section, heading, id, rationaleHeading].filter((value) => value !== undefined).length !== 1) {
  usage("Choose exactly one of --section, --heading, --id, or --rationale-heading.");
}

if (section !== undefined) {
  if (!/^\d+$/.test(section)) {
    usage(`Invalid section: ${section}`);
  }

  const lineIndex = lines.findIndex((line) => line.startsWith(`## ${section}.`));

  if (lineIndex < 0) {
    usage(`Section not found: ${section}`);
  }

  printInterval(lines, intervalForHeading(lines, lineIndex), `section ${section}`);
  process.exit(0);
}

if (heading !== undefined) {
  const lineIndex = lines.findIndex((line) => {
    const match = /^(#{2,4})\s+(.+)$/.exec(line);
    return match && (match[2] === heading || match[2].startsWith(`${heading} `));
  });

  if (lineIndex < 0) {
    usage(`Heading not found: ${heading}`);
  }

  printInterval(lines, intervalForHeading(lines, lineIndex), `heading ${heading}`);
  process.exit(0);
}

if (rationaleHeading !== undefined) {
  const lineIndex = rationaleLines.findIndex((line) => {
    const match = /^(#{2,4})\s+(.+)$/.exec(line);
    return match && (match[2] === rationaleHeading || match[2].startsWith(`${rationaleHeading} `));
  });

  if (lineIndex < 0) {
    usage(`Rationale heading not found: ${rationaleHeading}`);
  }

  printInterval(rationaleLines, intervalForHeading(rationaleLines, lineIndex), `rationale heading ${rationaleHeading}`);
  process.exit(0);
}

if (!/^W-\d{3,}$/.test(id) || !ledgerIdSet.has(id)) {
  usage(`Invalid decision ID: ${id}`);
}

const lineIndex = rationaleLines.findIndex((line) => line.startsWith(`| ${id} |`));

if (lineIndex < 0) {
  usage(`Decision not found: ${id}`);
}

const contextText = argument("--context") || "2";
const context = Number(contextText);

if (!Number.isInteger(context) || context < 0) {
  usage(`Invalid context: ${contextText}`);
}

const start = Math.max(0, lineIndex - context);
const end = Math.min(rationaleLines.length, lineIndex + context + 1);
printInterval(rationaleLines, { start, end }, `${id} with context ${context} in RATIONALE.md`);
