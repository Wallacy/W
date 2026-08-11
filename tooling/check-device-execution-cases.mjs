import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { runDeviceExecutionOperations } from "./device-execution-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const rootDirectory = path.resolve(toolingDirectory, "..")
const casesPath = path.join(toolingDirectory, "device-execution-cases.json")
const snapshotPath = path.join(toolingDirectory, "device-execution-results.snapshot.jsonl")
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

function expandOperations(item, index) {
  const expanded = []
  for (const [operationIndex, operation] of (item.operations ?? []).entries()) {
    if (typeof operation.$use !== "string") {
      expanded.push(structuredClone(operation))
      continue
    }
    const fixture = corpus.fixtures?.[operation.$use]
    if (!Array.isArray(fixture) || fixture.length === 0) {
      errors.push(`cases[${index}].operations[${operationIndex}]: fixture ${operation.$use}`)
      continue
    }
    if (operation.with !== undefined && fixture.length !== 1) {
      errors.push(`cases[${index}].operations[${operationIndex}]: fixture override arity`)
      continue
    }
    for (const fragment of fixture) {
      const base = structuredClone(fragment)
      const override = structuredClone(operation.with) ?? {}
      expanded.push({
        ...base,
        ...override,
        ...(override.limits ? { limits: { ...base.limits, ...override.limits } } : {}),
      })
    }
  }
  return expanded
}

if (corpus.$schema !== "w-device-execution-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 24) errors.push("coverage")

for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^DEV0-(?:accepted|rejected)-[a-z0-9-]+$/.test(item.id ?? "")) {
    errors.push(`${location}: id`)
  }
  if (ids.has(item.id)) errors.push(`${location}: duplicate id ${item.id}`)
  ids.add(item.id)
  if (!new Set(["accepted", "rejected"]).has(item.kind)) errors.push(`${location}: kind`)
  if (!Array.isArray(item.decisions) || item.decisions.length === 0) {
    errors.push(`${location}: decisions`)
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

  const operations = expandOperations(item, index)
  if (operations.length === 0) {
    errors.push(`${location}: operations`)
    continue
  }
  const result = runDeviceExecutionOperations(operations)
  const expected = item.expect ?? {}
  if (result.status !== item.kind) {
    errors.push(`${item.id}: kind ${item.kind} does not match derived ${result.status}`)
  }
  expectField(item.id, "status", result.status, expected.status)
  expectField(item.id, "error", result.error, expected.error)
  expectField(item.id, "phase", result.state.phase, expected.phase)
  expectField(
    item.id,
    "outcomes",
    Object.fromEntries(Object.entries(result.state.invocations).map(([id, value]) => [id, value.outcome])),
    expected.outcomes,
  )
  expectField(item.id, "happensBefore", result.state.happensBefore, expected.happensBefore)
  expectField(item.id, "quarantined", result.state.quarantined, expected.quarantined)
  expectField(
    item.id,
    "suppressedCompletions",
    result.state.suppressedCompletions,
    expected.suppressedCompletions,
  )
  expectField(item.id, "comparisons", result.state.comparisons, expected.comparisons)
  results.push({ caseId: item.id, operations, ...result })
}

for (const decision of [
  "W-1212",
  "W-1213",
  "W-1214",
  "W-1215",
  "W-1216",
  "W-1217",
  "W-1218",
]) {
  const covered = (corpus.cases ?? []).filter((item) => item.decisions?.includes(decision))
  if (!covered.some((item) => item.kind === "accepted")) {
    errors.push(`missing accepted coverage ${decision}`)
  }
  if (!covered.some((item) => item.kind === "rejected")) {
    errors.push(`missing rejected coverage ${decision}`)
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

const operationCount = results.reduce((total, item) => total + item.operations.length, 0)
const accepted = corpus.cases.filter((item) => item.kind === "accepted").length
const rejected = corpus.cases.length - accepted
process.stdout.write(
  `DEV0 device execution: ${corpus.cases.length} cases, ${operationCount} operations ` +
  `(${accepted} accepted, ${rejected} rejected).\n`,
)
