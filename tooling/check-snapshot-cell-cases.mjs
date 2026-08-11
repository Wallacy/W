import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { runSnapshotCellOperations } from "./snapshot-cell-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const rootDirectory = path.resolve(toolingDirectory, "..")
const casesPath = path.join(toolingDirectory, "snapshot-cell-cases.json")
const snapshotPath = path.join(toolingDirectory, "snapshot-cell-results.snapshot.jsonl")
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"))
const errors = []
const ids = new Set()
const results = []

function equal(actual, expected) {
  return JSON.stringify(actual) === JSON.stringify(expected)
}

function expectField(caseId, name, actual, expected) {
  if (expected === undefined) return
  if (!equal(actual, expected)) {
    errors.push(`${caseId}: ${name} expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`)
  }
}

if (corpus.$schema !== "w-snapshot-cell-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 20) errors.push("coverage")

for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^SP0-(?:accepted|rejected|fault)-[a-z0-9-]+$/.test(item.id ?? "")) {
    errors.push(`${location}: id`)
  }
  if (ids.has(item.id)) errors.push(`${location}: duplicate id ${item.id}`)
  ids.add(item.id)
  if (!new Set(["accepted", "rejected", "fault"]).has(item.kind)) {
    errors.push(`${location}: kind`)
  }
  if (!Array.isArray(item.decisions) || item.decisions.length === 0) {
    errors.push(`${location}: decisions`)
  }
  if (!Array.isArray(item.operations) || item.operations.length === 0) {
    errors.push(`${location}: operations`)
    continue
  }
  if (typeof item.source?.path !== "string" || typeof item.source?.symbol !== "string") {
    errors.push(`${location}: source`)
  } else {
    const sourcePath = path.join(rootDirectory, item.source.path)
    if (!fs.existsSync(sourcePath)) errors.push(`${item.id}: missing source ${item.source.path}`)
    else if (!fs.readFileSync(sourcePath, "utf8").includes(item.source.symbol)) {
      errors.push(`${item.id}: missing symbol ${item.source.symbol}`)
    }
  }

  const result = runSnapshotCellOperations(item.operations)
  const expected = item.expect ?? {}
  if (result.status !== item.kind) {
    errors.push(`${item.id}: kind ${item.kind} does not match derived ${result.status}`)
  }
  expectField(item.id, "status", result.status, expected.status)
  expectField(item.id, "error", result.error, expected.error)
  expectField(item.id, "phase", result.state.phase, expected.phase)
  expectField(item.id, "currentVersion", result.state.currentVersion, expected.currentVersion)
  expectField(item.id, "currentValue", result.state.currentValue, expected.currentValue)
  expectField(
    item.id,
    "observed",
    result.state.observations.map((observation) => observation.value),
    expected.observed,
  )
  expectField(
    item.id,
    "versionPhases",
    Object.values(result.state.versions).map((version) => version.phase),
    expected.versionPhases,
  )
  expectField(
    item.id,
    "readerOutcomes",
    Object.values(result.state.readers).map((reader) => reader.outcome),
    expected.readerOutcomes,
  )
  expectField(item.id, "drops", result.state.drops, expected.drops)
  expectField(
    item.id,
    "copies",
    result.state.copies.map((copy) => copy.value),
    expected.copies,
  )
  expectField(item.id, "publicationOrder", result.state.publicationOrder, expected.publicationOrder)
  expectField(item.id, "happensBefore", result.state.happensBefore, expected.happensBefore)
  results.push({ caseId: item.id, ...result })
}

for (const decision of ["W-1178", "W-1179", "W-1180"]) {
  if (!(corpus.cases ?? []).some((item) => item.decisions?.includes(decision))) {
    errors.push(`missing decision coverage ${decision}`)
  }
}

const snapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`
if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, snapshot)
} else if (!fs.existsSync(snapshotPath)) {
  errors.push("snapshot missing; run with --write")
} else if (fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  errors.push("snapshot stale; run with --write")
}

if (errors.length > 0) {
  process.stderr.write(`${errors.slice(0, 40).join("\n")}\n`)
  process.exit(1)
}

const operationCount = corpus.cases.reduce((total, item) => total + item.operations.length, 0)
const accepted = corpus.cases.filter((item) => item.kind === "accepted").length
const rejected = corpus.cases.filter((item) => item.kind === "rejected").length
const faults = corpus.cases.filter((item) => item.kind === "fault").length
process.stdout.write(
  `SP0 snapshot cell: ${corpus.cases.length} cases, ${operationCount} operations ` +
  `(${accepted} accepted, ${rejected} rejected, ${faults} fault).\n`,
)
