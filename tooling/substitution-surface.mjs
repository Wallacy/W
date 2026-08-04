import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const corpusPath = path.join(toolingDirectory, "substitution-cases.json");
const snapshotPath = path.join(toolingDirectory, "substitution-surface.snapshot.json");
const corpusBytes = fs.readFileSync(corpusPath);
const corpus = JSON.parse(corpusBytes.toString("utf8"));

function digest(bytes) {
  return `sha256:${crypto.createHash("sha256").update(bytes).digest("hex")}`;
}

function sourceText(lines) {
  return lines.join("\n");
}

function surfaceLexemes(source) {
  return (
    source.match(
      /#*"(?:\\.|[^"\\])*"#*|b?'(?:\\.|[^'\\])*'|[\p{L}_][\p{L}\p{N}_]*|\p{N}[\p{N}_]*(?:\.\p{N}[\p{N}_]*)?|[^\s]/gu,
    ) ?? []
  ).length;
}

function metrics(text) {
  return {
    utf8Bytes: Buffer.byteLength(text, "utf8"),
    codePoints: Array.from(text).length,
    nonWhitespaceCodePoints: Array.from(text).filter(
      (character) => !/\s/u.test(character),
    ).length,
    lines: text === "" ? 0 : text.split("\n").length,
    surfaceLexemes: surfaceLexemes(text),
  };
}

function measuredForm(name, form) {
  const text = sourceText(form.source);

  return {
    name,
    language: form.language,
    sourceDigest: digest(Buffer.from(text, "utf8")),
    metrics: metrics(text),
  };
}

const snapshot = {
  $schema: "w-substitution-surface-1",
  status: "deterministic-surface-baseline",
  corpusDigest: digest(corpusBytes),
  metricVersion: "unicode-surface-1",
  cases: corpus.cases.map((testCase) => ({
    id: testCase.id,
    task: metrics(testCase.task),
    selected: measuredForm("selected", testCase.selected),
    alternatives: testCase.alternatives.map((alternative) =>
      measuredForm(alternative.name, alternative),
    ),
  })),
};
const expected = `${JSON.stringify(snapshot, null, 2)}\n`;
const alternativeCount = snapshot.cases.reduce(
  (count, testCase) => count + testCase.alternatives.length,
  0,
);
const summary = `Substitution surface: ${snapshot.cases.length} cases, ${alternativeCount} alternatives, ${snapshot.cases.length + alternativeCount} measured forms.`;

if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, expected);
  process.stdout.write(`${summary}\nUpdated ${path.basename(snapshotPath)}.\n`);
  process.exit(0);
}

if (!fs.existsSync(snapshotPath)) {
  process.stderr.write(`${path.basename(snapshotPath)} is missing; run with --write.\n`);
  process.exit(1);
}

const actual = fs.readFileSync(snapshotPath, "utf8");

if (actual !== expected) {
  process.stderr.write(`${path.basename(snapshotPath)} is stale; run with --write.\n`);
  process.exit(1);
}

process.stdout.write(`${summary}\n`);
