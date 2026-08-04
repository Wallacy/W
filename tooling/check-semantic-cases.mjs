import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const design = await Bun.file(resolve(root, "DESIGN.md")).text()
const corpus = await Bun.file(resolve(import.meta.dir, "semantic-cases.json")).json()

function fail(message) {
  throw new Error(`semantic cases: ${message}`)
}

if (corpus.$schema !== "w-semantic-cases-0") {
  fail("unexpected schema")
}

if (corpus.status !== "design-oracle-input") {
  fail("status must not claim compiler output")
}

if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  fail("cases must be a non-empty array")
}

const diagnosticTable = design.match(
  /\| `W-SEM-0001`[\s\S]*?\| `W-CAPABILITY-0001`[^\n]*\|/,
)

if (!diagnosticTable) {
  fail("S0 diagnostic table is missing")
}

const requiredDiagnostics = [
  ...diagnosticTable[0].matchAll(/`(W-[A-Z]+-[0-9]{4})`/g),
].map((match) => match[1])

const ids = new Set()
const coveredDiagnostics = new Set()

for (const testCase of corpus.cases) {
  if (!/^S0-(POS|NEG)-[a-z0-9-]+$/.test(testCase.id)) {
    fail(`invalid id ${JSON.stringify(testCase.id)}`)
  }

  if (ids.has(testCase.id)) {
    fail(`duplicate id ${testCase.id}`)
  }
  ids.add(testCase.id)

  if (testCase.kind !== "positive" && testCase.kind !== "negative") {
    fail(`${testCase.id} has invalid kind`)
  }

  if (!/^W-[0-9]{3}$/.test(testCase.rule) || !design.includes(`| ${testCase.rule} |`)) {
    fail(`${testCase.id} references an unknown decision`)
  }

  if (!Array.isArray(testCase.source) || testCase.source.length === 0) {
    fail(`${testCase.id} has no source`)
  }

  if (testCase.source.some((line) => typeof line !== "string" || line.includes("\r"))) {
    fail(`${testCase.id} source must be an array of LF-safe strings`)
  }

  const diagnostics = testCase.expect?.diagnostics
  if (!Array.isArray(diagnostics)) {
    fail(`${testCase.id} has no diagnostics array`)
  }

  if (testCase.kind === "positive") {
    if (diagnostics.length !== 0) {
      fail(`${testCase.id} is positive but expects diagnostics`)
    }
    if (!testCase.expect.resultType || !testCase.expect.flow) {
      fail(`${testCase.id} lacks a normalized positive result`)
    }
  } else if (diagnostics.length !== 1) {
    fail(`${testCase.id} must invert exactly one S0 rule`)
  }

  for (const diagnostic of diagnostics) {
    if (!requiredDiagnostics.includes(diagnostic.code)) {
      fail(`${testCase.id} uses diagnostic ${diagnostic.code} outside S0`)
    }
    if (typeof diagnostic.primary !== "string" || diagnostic.primary.length === 0) {
      fail(`${testCase.id} diagnostic has no primary selector`)
    }
    coveredDiagnostics.add(diagnostic.code)
  }
}

const missing = requiredDiagnostics.filter((code) => !coveredDiagnostics.has(code))
if (missing.length > 0) {
  fail(`missing negative cases for ${missing.join(", ")}`)
}

const grammar = resolve(import.meta.dir, "tree-sitter-w")
const treeSitter = resolve(grammar, "node_modules", "tree-sitter-cli", "tree-sitter.exe")
if (!(await Bun.file(treeSitter).exists())) {
  fail("Tree-sitter CLI is missing; install tooling/tree-sitter-w dependencies")
}

const temporary = await mkdtemp(resolve(tmpdir(), "w-semantic-cases-"))
try {
  const files = []
  for (const testCase of corpus.cases) {
    const path = resolve(temporary, `${testCase.id}.w`)
    await Bun.write(path, `${testCase.source.join("\n")}\n`)
    files.push(path)
  }

  const parsed = Bun.spawnSync({
    cmd: [treeSitter, "parse", "--grammar-path", grammar, "--quiet", "--stat", ...files],
    cwd: grammar,
    stdout: "pipe",
    stderr: "pipe",
  })

  if (parsed.exitCode !== 0) {
    const output = `${parsed.stdout.toString()}\n${parsed.stderr.toString()}`.trim()
    fail(`semantic source is not syntactically valid\n${output}`)
  }
} finally {
  await rm(temporary, { recursive: true, force: true })
}

console.log(
  `Semantic cases: ${corpus.cases.length} syntax-valid cases, ${requiredDiagnostics.length}/${requiredDiagnostics.length} S0 diagnostics covered.`,
)
