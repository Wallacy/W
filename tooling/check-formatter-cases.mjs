import { createHash } from "node:crypto"
import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const design = await Bun.file(resolve(root, "DESIGN.md")).text()
const corpus = await Bun.file(resolve(import.meta.dir, "formatter-cases.json")).json()
const catalog = await Bun.file(resolve(import.meta.dir, "diagnostic-catalog.json")).json()
const snapshotPath = resolve(import.meta.dir, "formatter-diagnostics.snapshot.jsonl")
const writeSnapshot = process.argv.includes("--write")

function fail(message) {
  throw new Error(`formatter cases: ${message}`)
}

function digest(source) {
  return `sha256:${createHash("sha256").update(source, "utf8").digest("hex")}`
}

function inputText(input, owner) {
  if (!input || !Array.isArray(input.lines) || input.lines.length === 0) {
    fail(`${owner} has no input lines`)
  }
  if (input.lines.some((line) => typeof line !== "string" || line.includes("\n"))) {
    fail(`${owner} input lines must not contain newline characters`)
  }
  const newline = input.newline === "crlf" ? "\r\n" : "\n"
  if (input.newline !== undefined && input.newline !== "lf" && input.newline !== "crlf") {
    fail(`${owner} has an invalid input newline mode`)
  }

  let source = input.lines.join(newline)
  if (input.finalNewline !== false) {
    source += newline
  }
  if (input.bom === true) {
    source = `\uFEFF${source}`
  }
  return source
}

function outputText(output, owner) {
  if (!Array.isArray(output) || output.length === 0) {
    fail(`${owner} has no output lines`)
  }
  if (output.some((line) => typeof line !== "string" || /[\r\n]/.test(line))) {
    fail(`${owner} output lines must not contain newline characters`)
  }
  return `${output.join("\n")}\n`
}

function splitTrees(output) {
  const text = output.replaceAll("\r\n", "\n").replace(/\u001b\[[0-9;]*m/g, "")
  const starts = [...text.matchAll(/^\(source_file\b/gm)].map((match) => match.index)
  return starts.map((start, index) => text.slice(start, starts[index + 1] ?? text.length).trim())
}

function hasRecovery(tree) {
  return tree.includes("(ERROR") || tree.includes("(MISSING")
}

function firstDifferentByte(left, right) {
  const leftBytes = Buffer.from(left, "utf8")
  const rightBytes = Buffer.from(right, "utf8")
  const limit = Math.min(leftBytes.length, rightBytes.length)
  let index = 0
  while (index < limit && leftBytes[index] === rightBytes[index]) {
    index += 1
  }
  return index
}

if (corpus.$schema !== "w-formatter-cases-0" || corpus.status !== "design-oracle-input") {
  fail("unexpected schema or status")
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  fail("corpus is empty")
}

const formatCatalog = catalog.codes?.find((entry) => entry.code === "W-FMT-0001")
if (
  !formatCatalog ||
  formatCatalog.phase !== "source.format" ||
  formatCatalog.defaultSeverity !== "error" ||
  formatCatalog.fixes?.["format-source"] !== "machine"
) {
  fail("W-FMT-0001 catalog entry is incomplete")
}

const ids = new Set()
const prepared = []
const diagnostics = []

for (const [index, testCase] of corpus.cases.entries()) {
  if (!/^F0-[a-z0-9-]+$/.test(testCase.id) || ids.has(testCase.id)) {
    fail(`invalid or duplicate id ${JSON.stringify(testCase.id)}`)
  }
  ids.add(testCase.id)

  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) {
    fail(`${testCase.id} has no decision links`)
  }
  for (const decision of testCase.decisions) {
    if (!/^W-[0-9]{3,}$/.test(decision) || !design.includes(`| ${decision} |`)) {
      fail(`${testCase.id} references unknown decision ${decision}`)
    }
  }

  const input = inputText(testCase.input, testCase.id)
  const output = outputText(testCase.output, testCase.id)
  if (input === output) {
    fail(`${testCase.id} does not exercise a formatting change`)
  }
  if (output.startsWith("\uFEFF") || output.includes("\r") || output.includes("\t")) {
    fail(`${testCase.id} output is not canonical UTF-8/LF/space source`)
  }
  if (!output.endsWith("\n") || output.endsWith("\n\n")) {
    fail(`${testCase.id} output needs exactly one final newline`)
  }

  for (const [lineIndex, line] of testCase.output.entries()) {
    if (/[ \t]$/.test(line)) {
      fail(`${testCase.id} line ${lineIndex + 1} has trailing whitespace`)
    }
    if ([...line].length > 120) {
      fail(`${testCase.id} line ${lineIndex + 1} exceeds 120 columns`)
    }
    const indent = /^ */.exec(line)[0].length
    if (line.length > 0 && indent % 2 !== 0) {
      fail(`${testCase.id} line ${lineIndex + 1} has noncanonical indentation`)
    }
  }

  if (!Array.isArray(testCase.requiredSemicolons)) {
    fail(`${testCase.id} has no requiredSemicolons array`)
  }
  const semicolonCount = [...output.matchAll(/;/g)].length
  if (semicolonCount !== testCase.requiredSemicolons.length) {
    fail(`${testCase.id} has an unclassified semicolon`)
  }

  const mutations = []
  for (const [mutationIndex, marker] of testCase.requiredSemicolons.entries()) {
    if (marker.role !== "statement-boundary" && marker.role !== "repeat-array") {
      fail(`${testCase.id} has an unknown semicolon role`)
    }
    const needle = `${marker.after};`
    const first = output.indexOf(needle)
    if (first < 0 || output.indexOf(needle, first + needle.length) >= 0) {
      fail(`${testCase.id} semicolon selector ${JSON.stringify(marker.after)} is not unique`)
    }
    const semicolon = first + marker.after.length
    mutations.push({
      id: `${testCase.id}-without-semicolon-${mutationIndex}`,
      source: `${output.slice(0, semicolon)}${output.slice(semicolon + 1)}`,
    })
  }

  const sourceId = `format/${testCase.id}.w`
  diagnostics.push({
    schemaVersion: 1,
    instance: `D${String(index + 1).padStart(6, "0")}`,
    code: "W-FMT-0001",
    phase: "source.format",
    severity: "error",
    primary: {
      source: sourceId,
      startByte: firstDifferentByte(input, output),
      endByte: firstDifferentByte(input, output),
    },
    labels: [],
    facts: {
      canonicalDigest: digest(output),
      sourceDigest: digest(input),
    },
    notes: [],
    fixes: [
      {
        id: "format-source",
        titleKey: "fix.format.source",
        applicability: "machine",
        preconditions: [{ source: sourceId, digest: digest(input) }],
        edits: [
          {
            source: sourceId,
            startByte: 0,
            endByte: Buffer.byteLength(input, "utf8"),
            text: output,
          },
        ],
      },
    ],
    root: null,
  })
  prepared.push({ id: testCase.id, input, output, mutations })
}

const expectedSnapshot = `${diagnostics.map((record) => JSON.stringify(record)).join("\n")}\n`
if (writeSnapshot) {
  await Bun.write(snapshotPath, expectedSnapshot)
} else if (!(await Bun.file(snapshotPath).exists())) {
  fail("diagnostic snapshot is missing; run with --write")
} else if ((await Bun.file(snapshotPath).text()) !== expectedSnapshot) {
  fail("diagnostic snapshot is stale; review the change and run with --write")
}

const grammar = resolve(import.meta.dir, "tree-sitter-w")
const treeSitter = resolve(grammar, "node_modules", "tree-sitter-cli", "tree-sitter.exe")
if (!(await Bun.file(treeSitter).exists())) {
  fail("Tree-sitter CLI is missing; install tooling/tree-sitter-w dependencies")
}

const temporary = await mkdtemp(resolve(tmpdir(), "w-formatter-cases-"))
try {
  const files = []
  const keys = []
  for (const testCase of prepared) {
    for (const [kind, source] of [["input", testCase.input], ["output", testCase.output]]) {
      const path = resolve(temporary, `${testCase.id}-${kind}.w`)
      await Bun.write(path, source)
      files.push(path)
      keys.push(`${testCase.id}:${kind}`)
    }
  }

  const parsed = Bun.spawnSync({
    cmd: [treeSitter, "parse", "--grammar-path", grammar, "--no-ranges", ...files],
    cwd: grammar,
    stdout: "pipe",
    stderr: "pipe",
  })
  const trees = splitTrees(parsed.stdout.toString())
  if (parsed.exitCode !== 0 || trees.length !== files.length) {
    fail(`canonical pair parsing failed\n${parsed.stderr.toString().trim()}`)
  }
  const treeByKey = new Map(keys.map((key, index) => [key, trees[index]]))

  for (const testCase of prepared) {
    const inputTree = treeByKey.get(`${testCase.id}:input`)
    const outputTree = treeByKey.get(`${testCase.id}:output`)
    if (hasRecovery(inputTree) || hasRecovery(outputTree)) {
      fail(`${testCase.id} input or output uses parser recovery`)
    }
    if (inputTree !== outputTree) {
      fail(`${testCase.id} changes the named CST`)
    }
  }

  const mutationFiles = []
  const mutationOwners = []
  for (const testCase of prepared) {
    for (const mutation of testCase.mutations) {
      const path = resolve(temporary, `${mutation.id}.w`)
      await Bun.write(path, mutation.source)
      mutationFiles.push(path)
      mutationOwners.push(testCase.id)
    }
  }

  if (mutationFiles.length > 0) {
    const mutated = Bun.spawnSync({
      cmd: [treeSitter, "parse", "--grammar-path", grammar, "--no-ranges", ...mutationFiles],
      cwd: grammar,
      stdout: "pipe",
      stderr: "pipe",
    })
    const mutationTrees = splitTrees(mutated.stdout.toString())
    if (mutationTrees.length !== mutationFiles.length) {
      fail("semicolon mutation parsing did not return every tree")
    }
    for (const [index, tree] of mutationTrees.entries()) {
      const canonicalTree = treeByKey.get(`${mutationOwners[index]}:output`)
      if (tree === canonicalTree) {
        fail(`${mutationOwners[index]} semicolon does not protect syntax`)
      }
    }
  }
} finally {
  await rm(temporary, { recursive: true, force: true })
}

console.log(
  `Formatter cases: ${prepared.length} CST-preserving pairs, ${prepared.reduce((count, item) => count + item.mutations.length, 0)} required semicolons, ${diagnostics.length} D0 snapshots.`,
)
