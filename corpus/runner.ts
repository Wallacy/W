import { existsSync } from "node:fs";
import { mkdir, mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, extname, isAbsolute, join, relative, resolve, sep } from "node:path";

type Classification = "executable" | "frontend-only";
type Polarity = "positive" | "negative";

type PositiveExpectation = {
  parse: "success";
  observable: { kind: "stdout" | "cst"; value: string };
};

type NegativeExpectation = {
  parse: "failure";
  diagnostic: { code: string; message: string };
};

type CorpusCase = {
  id: string;
  source: string;
  snapshot: string;
  polarity: Polarity;
  classification: Classification;
  family: string;
  rules: string[];
  expect: PositiveExpectation | NegativeExpectation;
};

type Manifest = {
  schema: "w.corpus/1";
  edition: string;
  cases: CorpusCase[];
};

type CommandResult = {
  code: number;
  stdout: string;
  stderr: string;
};

type ParserDiagnostic = {
  kind: "ERROR" | "MISSING";
  expectedNode?: string;
  start: { row: number; column: number };
  end: { row: number; column: number };
};

const corpusRoot = import.meta.dir;
const wRoot = resolve(corpusRoot, "..");
const grammarRoot = join(wRoot, "tooling", "tree-sitter-w");
const updateSnapshots = process.argv.includes("--update");

function pathInside(root: string, candidate: string): string {
  const result = resolve(root, candidate);
  const relation = relative(root, result);
  if (relation === "" || (!relation.startsWith(`..${sep}`) && relation !== ".." && !isAbsolute(relation))) {
    return result;
  }
  throw new Error(`path escapes corpus root: ${candidate}`);
}

function normalize(text: string, sourcePath?: string): string {
  let value = text.replaceAll("\r\n", "\n").replaceAll("\r", "\n");
  if (sourcePath) {
    value = value.replaceAll(sourcePath, "<source>").replaceAll(sourcePath.replaceAll("\\", "/"), "<source>");
  }
  value = value.replace(
    /^<source>\tParse:\s+[0-9.]+ ms\t\s*[0-9]+ bytes\/ms\t/gm,
    "<source>\t",
  );
  return `${value.trimEnd()}\n`;
}

function parserDiagnostics(cst: string): ParserDiagnostic[] {
  const pattern = /\((ERROR|MISSING)(?:\s+([A-Za-z_][A-Za-z0-9_]*))?\s+\[(\d+),\s*(\d+)\]\s+-\s+\[(\d+),\s*(\d+)\]\)/g;
  return [...cst.matchAll(pattern)].map((match) => ({
    kind: match[1] as "ERROR" | "MISSING",
    ...(match[2] ? { expectedNode: match[2] } : {}),
    start: { row: Number(match[3]), column: Number(match[4]) },
    end: { row: Number(match[5]), column: Number(match[6]) },
  }));
}

function byteOffset(source: string, row: number, column: number): number {
  const bytes = Buffer.from(source.replaceAll("\r\n", "\n").replaceAll("\r", "\n"), "utf8");
  let line = 0;
  let lineStart = 0;
  for (let index = 0; index < bytes.length && line < row; index += 1) {
    if (bytes[index] === 0x0a) {
      line += 1;
      lineStart = index + 1;
    }
  }
  if (line !== row || lineStart + column > bytes.length) {
    throw new Error(`invalid parser point [${row}, ${column}]`);
  }
  return lineStart + column;
}

function diagnosticSnapshot(item: CorpusCase, source: string, parser: ParserDiagnostic): string {
  if (item.expect.parse !== "failure") throw new Error(`${item.id}: diagnostic requested for a positive case`);
  const payload = {
    schema: "w.diagnostics/1",
    case: item.id,
    source: item.source,
    diagnostics: [
      {
        code: item.expect.diagnostic.code,
        severity: "error",
        message: item.expect.diagnostic.message,
        primary: {
          start: { ...parser.start, byte: byteOffset(source, parser.start.row, parser.start.column) },
          end: { ...parser.end, byte: byteOffset(source, parser.end.row, parser.end.column) },
        },
        parser: {
          kind: parser.kind,
          ...(parser.expectedNode ? { expectedNode: parser.expectedNode } : {}),
        },
      },
    ],
  };
  return `${JSON.stringify(payload, null, 2)}\n`;
}

async function run(command: string, args: string[], cwd: string): Promise<CommandResult> {
  const child = Bun.spawn([command, ...args], {
    cwd,
    env: { ...process.env, NO_COLOR: "1" },
    stdout: "pipe",
    stderr: "pipe",
  });
  const [code, stdout, stderr] = await Promise.all([
    child.exited,
    new Response(child.stdout).text(),
    new Response(child.stderr).text(),
  ]);
  return { code, stdout, stderr };
}

function validateManifest(manifest: Manifest): void {
  if (manifest.schema !== "w.corpus/1") throw new Error(`unsupported schema: ${manifest.schema}`);
  if (!manifest.edition || manifest.cases.length < 1) throw new Error("manifest must declare edition and cases");

  const ids = new Set<string>();
  const sources = new Set<string>();
  const snapshots = new Set<string>();
  const diagnosticCodes = new Set<string>();
  for (const item of manifest.cases) {
    if (!/^[a-z][a-z0-9-]+$/.test(item.id)) throw new Error(`invalid case id: ${item.id}`);
    if (ids.has(item.id)) throw new Error(`duplicate case id: ${item.id}`);
    if (sources.has(item.source)) throw new Error(`duplicate source: ${item.source}`);
    if (snapshots.has(item.snapshot)) throw new Error(`duplicate snapshot: ${item.snapshot}`);
    ids.add(item.id);
    sources.add(item.source);
    snapshots.add(item.snapshot);

    if (item.source.split("/")[0] !== item.polarity) {
      throw new Error(`${item.id}: polarity must match its source directory`);
    }
    if (extname(item.source) !== ".w" || extname(item.snapshot) !== ".cst") {
      throw new Error(`${item.id}: source/snapshot extension is invalid`);
    }
    if (item.rules.length < 1 || !item.family) throw new Error(`${item.id}: family and rules are required`);
    if (item.polarity === "positive" && item.expect.parse !== "success") {
      throw new Error(`${item.id}: positive case must expect parse success`);
    }
    if (item.polarity === "negative" && item.expect.parse !== "failure") {
      throw new Error(`${item.id}: negative case must expect parse failure`);
    }
    if (item.expect.parse === "failure") {
      if (!/^W-SYN-[0-9]{4}$/.test(item.expect.diagnostic.code)) {
        throw new Error(`${item.id}: invalid diagnostic code ${item.expect.diagnostic.code}`);
      }
      if (diagnosticCodes.has(item.expect.diagnostic.code)) {
        throw new Error(`${item.id}: duplicate diagnostic code ${item.expect.diagnostic.code}`);
      }
      diagnosticCodes.add(item.expect.diagnostic.code);
    }
    if (item.classification === "executable") {
      if (item.expect.parse !== "success" || item.expect.observable.kind !== "stdout") {
        throw new Error(`${item.id}: executable case requires a stdout contract`);
      }
    }
  }

  const positives = manifest.cases.filter((item) => item.polarity === "positive");
  if (positives.length < 10 || positives.length > 20) {
    throw new Error(`phase 0 requires 10–20 positive programs; found ${positives.length}`);
  }
  const negativeFamilies = new Set(
    manifest.cases.filter((item) => item.polarity === "negative").map((item) => item.family),
  );
  const uncoveredFamilies = [...new Set(positives.map((item) => item.family))].filter(
    (family) => !negativeFamilies.has(family),
  );
  if (uncoveredFamilies.length > 0) {
    throw new Error(`positive families without a negative case: ${uncoveredFamilies.join(", ")}`);
  }
}

async function parseCase(
  cli: string,
  library: string,
  item: CorpusCase,
): Promise<{ cst: string; diagnostics: ParserDiagnostic[]; failed: boolean }> {
  const sourcePath = pathInside(corpusRoot, item.source);
  const result = await run(
    cli,
    ["parse", "--lib-path", library, "--lang-name", "w", "--cst", sourcePath],
    grammarRoot,
  );
  const cst = normalize(result.stdout, sourcePath);
  const diagnostics = parserDiagnostics(cst);
  const hasErrorNode = diagnostics.length > 0;

  if (result.code !== 0 && !hasErrorNode) {
    throw new Error(`${item.id}: parser failed without ERROR/MISSING\n${normalize(result.stderr)}`);
  }
  return { cst, diagnostics, failed: result.code !== 0 || hasErrorNode };
}

async function main(): Promise<void> {
  const manifest = JSON.parse(await readFile(join(corpusRoot, "manifest.json"), "utf8")) as Manifest;
  validateManifest(manifest);

  const executableName = process.platform === "win32" ? "tree-sitter.cmd" : "tree-sitter";
  const cli = join(grammarRoot, "node_modules", ".bin", executableName);
  if (!existsSync(cli)) {
    throw new Error("tree-sitter CLI not installed; run `npm install` in W/tooling/tree-sitter-w");
  }

  const tempRoot = await mkdtemp(join(tmpdir(), "w-corpus-"));
  const libraryExtension = process.platform === "win32" ? ".dll" : process.platform === "darwin" ? ".dylib" : ".so";
  const library = join(tempRoot, `tree-sitter-w${libraryExtension}`);
  const build = await run(cli, ["build", "--output", library, grammarRoot], grammarRoot);
  if (build.code !== 0) throw new Error(`unable to build parser\n${normalize(build.stderr)}`);

  let positiveCount = 0;
  let negativeCount = 0;
  let executableCount = 0;
  let diagnosticCount = 0;

  try {
    for (const item of manifest.cases) {
      const first = await parseCase(cli, library, item);
      const second = await parseCase(cli, library, item);
      if (
        first.cst !== second.cst ||
        first.failed !== second.failed ||
        JSON.stringify(first.diagnostics) !== JSON.stringify(second.diagnostics)
      ) {
        throw new Error(`${item.id}: parser output is not deterministic across two runs`);
      }

      const expectedFailure = item.expect.parse === "failure";
      if (first.failed !== expectedFailure) {
        const received = first.failed ? "failure" : "success";
        throw new Error(`${item.id}: expected parse ${item.expect.parse}, received ${received}`);
      }

      const snapshotPath = pathInside(corpusRoot, item.snapshot);
      if (updateSnapshots) {
        await mkdir(dirname(snapshotPath), { recursive: true });
        await Bun.write(snapshotPath, first.cst);
      } else {
        if (!existsSync(snapshotPath)) throw new Error(`${item.id}: missing snapshot; run with --update`);
        const expected = normalize(await readFile(snapshotPath, "utf8"));
        if (first.cst !== expected) throw new Error(`${item.id}: CST differs from ${item.snapshot}`);
      }

      if (item.expect.parse === "failure") {
        if (first.diagnostics.length !== 1) {
          throw new Error(`${item.id}: expected one parser diagnostic, received ${first.diagnostics.length}`);
        }
        const source = await readFile(pathInside(corpusRoot, item.source), "utf8");
        const actualDiagnostic = diagnosticSnapshot(item, source, first.diagnostics[0]);
        const diagnosticPath = pathInside(corpusRoot, `diagnostics/${item.id}.json`);
        if (updateSnapshots) {
          await mkdir(dirname(diagnosticPath), { recursive: true });
          await Bun.write(diagnosticPath, actualDiagnostic);
        } else {
          if (!existsSync(diagnosticPath)) throw new Error(`${item.id}: missing diagnostic snapshot`);
          const expectedDiagnostic = normalize(await readFile(diagnosticPath, "utf8"));
          if (actualDiagnostic !== expectedDiagnostic) {
            throw new Error(`${item.id}: diagnostic differs from diagnostics/${item.id}.json`);
          }
        }
        diagnosticCount += 1;
      }

      if (item.polarity === "positive") positiveCount += 1;
      else negativeCount += 1;
      if (item.classification === "executable") executableCount += 1;
    }
  } finally {
    await rm(tempRoot, { recursive: true, force: true });
  }

  console.log(
    `corpus ${manifest.schema}: ${positiveCount} positive, ${negativeCount} negative, ` +
      `${executableCount} executable contracts, ${diagnosticCount} diagnostics; ` +
      `deterministic CST: ok${updateSnapshots ? "; snapshots updated" : ""}`,
  );
}

await main().catch((error: unknown) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
});
