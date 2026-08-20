import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const corpus = await Bun.file(resolve(import.meta.dir, "formatter-cases.json")).json()
const snapshotLines = (await Bun.file(resolve(import.meta.dir, "formatter-diagnostics.snapshot.jsonl")).text())
  .split(/\r?\n/u).filter(Boolean)

function fail(message) {
  throw new Error(`seed diagnostic: ${message}`)
}

function run(command, args) {
  const execution = Bun.spawnSync({
    cmd: [command, ...args], cwd: root, stdout: "pipe", stderr: "pipe",
  })
  if (execution.exitCode !== 0) {
    fail(`${command} ${args.join(" ")} failed: ${execution.stderr.toString().trim()}`)
  }
  return execution
}

function inputBytes(input, id) {
  if (!input || !Array.isArray(input.lines) || input.lines.length === 0) {
    fail(`${id} has no input lines`)
  }
  if (input.lines.some((line) => typeof line !== "string" || /[\r\n]/u.test(line))) {
    fail(`${id} input lines are invalid`)
  }
  const newline = input.newline === "crlf" ? "\r\n" : "\n"
  let source = input.lines.join(newline)
  if (input.finalNewline !== false) source += newline
  let bytes = Buffer.from(source, "utf8")
  if (input.bom === true) bytes = Buffer.concat([Buffer.from([0xef, 0xbb, 0xbf]), bytes])
  return bytes
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-diagnostic-"))
try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory])
  run("ctest", ["--test-dir", buildDirectory, "--output-on-failure"])
  const extension = process.platform === "win32" ? ".exe" : ""
  const probe = join(buildDirectory, `w_seed_diagnostic_probe${extension}`)
  if (snapshotLines.length !== corpus.cases.length) {
    fail(`snapshot has ${snapshotLines.length} records for ${corpus.cases.length} formatter cases`)
  }
  for (let index = 0; index < corpus.cases.length; index += 1) {
    const fixture = corpus.cases[index]
    const expectedLine = snapshotLines[index]
    const expected = JSON.parse(expectedLine)
    if (expected.instance !== `D${String(index + 1).padStart(6, "0")}` ||
        expected.code !== "W-FMT-0001" ||
        expected.phase !== "source.format") {
      fail(`snapshot record ${index + 1} has unexpected identity`)
    }
    const source = inputBytes(fixture.input, fixture.id)
    const sourceId = `format/${fixture.id}.w`
    const execution = Bun.spawnSync({
      cmd: [probe, expected.instance, sourceId], cwd: root,
      stdin: source, stdout: "pipe", stderr: "pipe",
    })
    if (execution.exitCode !== 0) {
      fail(`${fixture.id} diagnostic probe failed: ${execution.stderr.toString().trim()}`)
    }
    const actual = execution.stdout.toString()
    if (actual !== expectedLine) {
      let firstDifference = 0
      while (firstDifference < actual.length && firstDifference < expectedLine.length &&
             actual[firstDifference] === expectedLine[firstDifference]) firstDifference += 1
      fail(`${fixture.id} JSONL differs at ${firstDifference}: ` +
           `${JSON.stringify(actual.slice(firstDifference, firstDifference + 80))} != ` +
           `${JSON.stringify(expectedLine.slice(firstDifference, firstDifference + 80))}`)
    }
  }
  const mappingProbe = join(buildDirectory, `w_seed_diagnostic_mapping_probe${extension}`)
  const runMapping = (mode, instance, sourceId, source) => {
    const execution = Bun.spawnSync({
      cmd: [mappingProbe, mode, instance, sourceId], cwd: root,
      stdin: source, stdout: "pipe", stderr: "pipe",
    })
    if (execution.exitCode !== 0) {
      fail(`${mode} mapping probe failed: ${execution.stderr.toString().trim()}`)
    }
    const raw = execution.stdout.toString()
    if (raw.length === 0 || raw.endsWith("\n")) {
      fail(`${mode} mapping record has an unexpected terminator`)
    }
    let record
    try {
      record = JSON.parse(raw)
    } catch (error) {
      fail(`${mode} mapping record is not JSON: ${error}`)
    }
    if (JSON.stringify(record) !== raw) {
      fail(`${mode} mapping record changes field order or escaping`)
    }
    return record
  }
  const lexRecord = runMapping("lex", "D000001", "lex", Buffer.from([0x22]))
  const expectedLex = {
    schemaVersion: 1, instance: "D000001", code: "W-LEX-0001",
    phase: "source.lex", severity: "error",
    primary: { source: "lex", startByte: 1, endByte: 1 },
    labels: [{ role: "opening-delimiter", span: {
      source: "lex", startByte: 0, endByte: 1,
    } }],
    facts: { construct: "string-literal", delimiter: "quote", reachedEof: true },
    notes: [], fixes: [], root: null,
  }
  if (JSON.stringify(lexRecord) !== JSON.stringify(expectedLex)) {
    fail(`lex mapping record differs: ${JSON.stringify(lexRecord)}`)
  }
  const parseRecord = runMapping("parse", "D000002", "parse", Buffer.from("if ("))
  const expectedParse = {
    schemaVersion: 1, instance: "D000002", code: "W-PARSE-0001",
    phase: "source.parse", severity: "error",
    primary: { source: "parse", startByte: 0, endByte: 2 }, labels: [],
    facts: { actual: "word", construct: "grammar owner", expected: ["statement"] },
    notes: [], fixes: [], root: null,
  }
  if (JSON.stringify(parseRecord) !== JSON.stringify(expectedParse)) {
    fail(`parse mapping record differs: ${JSON.stringify(parseRecord)}`)
  }
  console.log(`Seed C diagnostics: ${corpus.cases.length}/${snapshotLines.length} W-FMT-0001 JSONL records exact; lex/parse schema records exact; C capacity/escaping gates passed`)
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
