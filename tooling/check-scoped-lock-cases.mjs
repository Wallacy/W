import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { runScopedLockOperations } from "./scoped-lock-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const rootDirectory = path.resolve(toolingDirectory, "..")
const casesPath = path.join(toolingDirectory, "scoped-lock-cases.json")
const snapshotPath = path.join(toolingDirectory, "scoped-lock-results.snapshot.jsonl")
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"))
const errors = []
const ids = new Set()
const results = []

function equal(left, right) {
  return JSON.stringify(left) === JSON.stringify(right)
}

function expectField(caseId, name, actual, expected) {
  if (expected === undefined) return
  if (!equal(actual, expected)) {
    errors.push(`${caseId}: ${name} expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`)
  }
}

function firstOwner(result) {
  return Object.values(result.state.owners)[0] ?? null
}

if (corpus.$schema !== "w-language-lock-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 30) errors.push("coverage")

for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^LM1-(?:accepted|rejected|fault)-[a-z0-9-]+$/.test(item.id ?? "")) {
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

  const result = runScopedLockOperations(item.operations)
  const expected = item.expect ?? {}
  const owner = firstOwner(result)
  if (result.status !== item.kind) {
    errors.push(`${item.id}: kind ${item.kind} does not match derived ${result.status}`)
  }
  expectField(item.id, "status", result.status, expected.status)
  expectField(item.id, "error", result.error, expected.error)
  expectField(item.id, "allocation", owner?.allocation, expected.allocation)
  expectField(item.id, "phase", owner?.phase, expected.phase)
  expectField(item.id, "value", owner?.value, expected.value)
  expectField(item.id, "holder", owner?.holder, expected.holder)
  expectField(item.id, "waiters", owner?.waiters, expected.waiters)
  expectField(item.id, "drops", owner?.drops, expected.drops)
  expectField(item.id, "bodyEvaluations", owner?.bodyEvaluations, expected.bodyEvaluations)
  expectField(item.id, "reads", result.state.reads, expected.reads)
  expectField(item.id, "selections", result.state.selections, expected.selections)
  expectField(item.id, "failedBoundaries", result.state.failedBoundaries, expected.failedBoundaries)
  expectField(
    item.id,
    "outcomes",
    owner?.outcomes.map((outcome) => outcome.outcome),
    expected.outcomes,
  )
  expectField(
    item.id,
    "outcomeCancellations",
    owner?.outcomes.map((outcome) => outcome.cancellation),
    expected.outcomeCancellations,
  )
  expectField(item.id, "cancellations", owner?.cancellations, expected.cancellations)
  expectField(item.id, "happensBefore", owner?.happensBefore, expected.happensBefore)
  expectField(
    item.id,
    "tryResults",
    result.state.receipts
      .filter((receipt) => receipt.operation === "try")
      .map((receipt) => receipt.result),
    expected.tryResults,
  )
  results.push({ caseId: item.id, ...result })
}

for (const decision of ["W-1256", "W-1257", "W-1258", "W-1259", "W-1260"]) {
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
  process.stderr.write(`${errors.slice(0, 60).join("\n")}\n`)
  process.exit(1)
}

const operationCount = corpus.cases.reduce((total, item) => total + item.operations.length, 0)
const accepted = corpus.cases.filter((item) => item.kind === "accepted").length
const rejected = corpus.cases.filter((item) => item.kind === "rejected").length
const faults = corpus.cases.filter((item) => item.kind === "fault").length
process.stdout.write(
  `LM1 language lock: ${corpus.cases.length} cases, ${operationCount} operations ` +
  `(${accepted} accepted, ${rejected} rejected, ${faults} fault).\n`,
)
