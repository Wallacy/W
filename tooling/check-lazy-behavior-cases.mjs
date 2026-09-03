import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { runLazyBehaviorOperations } from "./lazy-behavior-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const rootDirectory = path.resolve(toolingDirectory, "..")
const casesPath = path.join(toolingDirectory, "lazy-behavior-cases.json")
const snapshotPath = path.join(toolingDirectory, "lazy-behavior-results.snapshot.jsonl")
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
    errors.push(
      `${caseId}: ${name} expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`,
    )
  }
}

function fixtureOperations(names, stack = []) {
  return (names ?? []).flatMap((name) => {
    if (stack.includes(name)) throw new Error(`Lazy fixture cycle at ${name}.`)
    const fixture = corpus.fixtures?.[name]
    if (!Array.isArray(fixture)) throw new Error(`Unknown Lazy fixture ${name}.`)
    return fixture
  })
}

if (corpus.$schema !== "w-lazy-behavior-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 24) errors.push("coverage")

for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^LZ0-(?:accepted|rejected|fault)-[a-z0-9-]+$/.test(item.id ?? "")) {
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
  const operations = [
    ...fixtureOperations(item.fixtures),
    ...(item.operations ?? []),
  ]
  if (operations.length === 0) {
    errors.push(`${location}: operations`)
    continue
  }

  if (typeof item.source?.path !== "string" || typeof item.source?.symbol !== "string") {
    errors.push(`${location}: source`)
  } else {
    const sourcePath = path.join(rootDirectory, item.source.path)
    if (!fs.existsSync(sourcePath)) {
      errors.push(`${item.id}: missing source ${item.source.path}`)
    } else if (!fs.readFileSync(sourcePath, "utf8").includes(item.source.symbol)) {
      errors.push(`${item.id}: missing symbol ${item.source.symbol}`)
    }
  }

  const result = runLazyBehaviorOperations(operations)
  const expected = item.expect ?? {}
  if (result.status !== item.kind) {
    errors.push(`${item.id}: kind ${item.kind} does not match derived ${result.status}`)
  }
  expectField(item.id, "status", result.status, expected.status)
  expectField(item.id, "error", result.error, expected.error)
  expectField(item.id, "phase", result.state.phase, expected.phase)
  expectField(item.id, "initializerRuns", result.state.initializerRuns, expected.initializerRuns)
  expectField(item.id, "publication", result.state.publication, expected.publication)
  expectField(
    item.id,
    "observedValues",
    result.state.observations.map((observation) => observation.value),
    expected.observedValues,
  )
  expectField(item.id, "waiterPhases", result.state.waiterPhases, expected.waiterPhases)
  expectField(item.id, "cancellations", result.state.cancellations, expected.cancellations)
  expectField(item.id, "captureDrops", result.state.captureDrops, expected.captureDrops)
  expectField(item.id, "valueDrops", result.state.valueDrops, expected.valueDrops)
  expectField(item.id, "mutableBorrowCount", result.state.mutableBorrowCount, expected.mutableBorrowCount)
  expectField(item.id, "happensBefore", result.state.happensBefore, expected.happensBefore)
  for (const event of expected.physicalContains ?? []) {
    if (!result.physical.trace.includes(event)) {
      errors.push(`${item.id}: physical trace does not contain ${event}`)
    }
  }
  results.push({ caseId: item.id, ...result })
}

for (const decision of ["W-1193", "W-1194", "W-1195", "W-1196"]) {
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

const operationCount = corpus.cases.reduce(
  (total, item) =>
    total + fixtureOperations(item.fixtures).length + (item.operations ?? []).length,
  0,
)
const accepted = corpus.cases.filter((item) => item.kind === "accepted").length
const rejected = corpus.cases.filter((item) => item.kind === "rejected").length
const faults = corpus.cases.filter((item) => item.kind === "fault").length
process.stdout.write(
  `LZ0 Lazy behavior: ${corpus.cases.length} cases, ${operationCount} operations ` +
    `(${accepted} accepted, ${rejected} rejected, ${faults} fault).\n`,
)
