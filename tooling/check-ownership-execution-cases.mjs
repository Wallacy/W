import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { runOwnershipExecutionOperations } from "./ownership-execution-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const rootDirectory = path.resolve(toolingDirectory, "..")
const casesPath = path.join(toolingDirectory, "ownership-execution-cases.json")
const snapshotPath = path.join(toolingDirectory, "ownership-execution-results.snapshot.jsonl")
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"))
const errors = []
const ids = new Set()
const results = []

function equal(actual, expected) {
  return JSON.stringify(actual) === JSON.stringify(expected)
}

function expectField(caseId, field, actual, expected) {
  if (expected === undefined) return
  if (!equal(actual, expected)) {
    errors.push(
      `${caseId}: ${field} expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`,
    )
  }
}

if (corpus.$schema !== "w-ownership-execution-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 30) errors.push("coverage")

for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^MX0-(?:accepted|rejected)-[a-z0-9-]+$/.test(item.id ?? "")) {
    errors.push(`${location}: id`)
  }
  if (ids.has(item.id)) errors.push(`${location}: duplicate id ${item.id}`)
  ids.add(item.id)
  if (!new Set(["accepted", "rejected"]).has(item.kind)) errors.push(`${location}: kind`)
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

  const result = runOwnershipExecutionOperations(item.operations)
  const expected = item.expect ?? {}
  if (result.status !== item.kind) {
    errors.push(`${item.id}: kind ${item.kind} does not match derived ${result.status}`)
  }
  expectField(item.id, "status", result.status, expected.status)
  expectField(item.id, "error", result.error, expected.error)
  expectField(item.id, "scope", result.state.scope, expected.scope)
  expectField(
    item.id,
    "parentSuspended",
    result.state.parentSuspended,
    expected.parentSuspended,
  )
  expectField(
    item.id,
    "ownerValues",
    Object.fromEntries(
      Object.entries(expected.ownerValues ?? {}).map(([owner]) => [
        owner,
        result.state.owners[owner]?.value,
      ]),
    ),
    expected.ownerValues,
  )
  expectField(
    item.id,
    "ownerLocations",
    Object.fromEntries(
      Object.entries(expected.ownerLocations ?? {}).map(([owner]) => [
        owner,
        result.state.owners[owner]?.location,
      ]),
    ),
    expected.ownerLocations,
  )
  expectField(
    item.id,
    "taskPhases",
    Object.values(result.state.tasks).map((task) => task.phase),
    expected.taskPhases,
  )
  expectField(
    item.id,
    "outcomes",
    Object.values(result.state.tasks).map((task) => task.outcome),
    expected.outcomes,
  )
  expectField(
    item.id,
    "bodyStarted",
    Object.values(result.state.tasks).map((task) => task.bodyStarted),
    expected.bodyStarted,
  )
  expectField(
    item.id,
    "cancelRequested",
    Object.values(result.state.tasks).map((task) => task.cancelRequested),
    expected.cancelRequested,
  )
  expectField(
    item.id,
    "frameReclaimed",
    Object.values(result.state.tasks).map((task) => task.frameReclaimed),
    expected.frameReclaimed,
  )
  expectField(
    item.id,
    "loanPhases",
    Object.values(result.state.loans).map((loan) => loan.phase),
    expected.loanPhases,
  )
  expectField(
    item.id,
    "droppedOwners",
    result.state.drops.map((drop) => drop.owner),
    expected.droppedOwners,
  )
  expectField(
    item.id,
    "happensBefore",
    result.state.happensBefore,
    expected.happensBefore,
  )
  if (
    expected.physicalPrefix !== undefined &&
    !result.physical.trace[0]?.startsWith(expected.physicalPrefix)
  ) {
    errors.push(
      `${item.id}: physicalPrefix expected ${expected.physicalPrefix}, ` +
      `actual ${result.physical.trace[0] ?? null}`,
    )
  }
  results.push({ caseId: item.id, ...result })
}

for (const decision of ["W-1185", "W-1186", "W-1187", "W-1188"]) {
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
  process.stderr.write(`${errors.slice(0, 50).join("\n")}\n`)
  process.exit(1)
}

const operationCount = corpus.cases.reduce(
  (total, item) => total + item.operations.length,
  0,
)
const accepted = corpus.cases.filter((item) => item.kind === "accepted").length
const rejected = corpus.cases.filter((item) => item.kind === "rejected").length
process.stdout.write(
  `MX0 ownership/execution: ${corpus.cases.length} cases, ${operationCount} operations ` +
  `(${accepted} accepted, ${rejected} rejected).\n`,
)
