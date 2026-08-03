import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const designPath = path.resolve(toolingDirectory, "..", "DESIGN.md");
const lines = fs.readFileSync(designPath, "utf8").split(/\r?\n/);

function usage(message) {
  if (message) {
    process.stderr.write(`${message}\n\n`);
  }

  process.stderr.write(
    [
      "Usage:",
      "  node tooling/design-slice.mjs --section 12",
      "  node tooling/design-slice.mjs --heading 12.13",
      "  node tooling/design-slice.mjs --id W-711 [--context 2]",
      "",
      "The command reads DESIGN.md and writes only the requested interval.",
      "It never edits the canonical document.",
    ].join("\n") + "\n",
  );
  process.exit(2);
}

function argument(name) {
  const index = process.argv.indexOf(name);
  return index < 0 ? undefined : process.argv[index + 1];
}

function headingAt(lineIndex) {
  const match = /^(#{2,4})\s+(.+)$/.exec(lines[lineIndex]);
  return match ? { level: match[1].length, title: match[2] } : undefined;
}

function intervalForHeading(lineIndex) {
  const heading = headingAt(lineIndex);

  if (!heading) {
    return undefined;
  }

  let end = lines.length;

  for (let index = lineIndex + 1; index < lines.length; index += 1) {
    const candidate = headingAt(index);

    if (candidate && candidate.level <= heading.level) {
      end = index;
      break;
    }
  }

  return { start: lineIndex, end };
}

function printInterval(interval, label) {
  const content = lines.slice(interval.start, interval.end).join("\n");
  process.stdout.write(`<!-- design-slice: ${label} lines ${interval.start + 1}-${interval.end} -->\n`);
  process.stdout.write(`${content}\n`);
}

const section = argument("--section");
const heading = argument("--heading");
const id = argument("--id");

if ([section, heading, id].filter((value) => value !== undefined).length !== 1) {
  usage("Choose exactly one of --section, --heading, or --id.");
}

if (section !== undefined) {
  if (!/^\d+$/.test(section)) {
    usage(`Invalid section: ${section}`);
  }

  const lineIndex = lines.findIndex((line) => line.startsWith(`## ${section}.`));

  if (lineIndex < 0) {
    usage(`Section not found: ${section}`);
  }

  printInterval(intervalForHeading(lineIndex), `section ${section}`);
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

  printInterval(intervalForHeading(lineIndex), `heading ${heading}`);
  process.exit(0);
}

if (!/^W-\d{3,}$/.test(id)) {
  usage(`Invalid decision ID: ${id}`);
}

const lineIndex = lines.findIndex((line) => line.startsWith(`| ${id} |`));

if (lineIndex < 0) {
  usage(`Decision not found: ${id}`);
}

const contextText = argument("--context") || "2";
const context = Number(contextText);

if (!Number.isInteger(context) || context < 0) {
  usage(`Invalid context: ${contextText}`);
}

const start = Math.max(0, lineIndex - context);
const end = Math.min(lines.length, lineIndex + context + 1);
printInterval({ start, end }, `${id} with context ${context}`);
