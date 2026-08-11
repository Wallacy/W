import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import {
  conditionToKind,
  deriveIoError,
  ioErrorKinds,
  ioOperations,
} from "./io-error-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const casesPath = path.join(toolingDirectory, "io-error-cases.json")
const snapshotPath = path.join(toolingDirectory, "io-error-results.snapshot.jsonl")
const write = process.argv.includes("--write")
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"))
const errors = []
const ids = new Set()
const decisions = new Set()
const coveredConditions = new Set()
const coveredOperations = new Set()
const results = []

function matches(actual, expected, location) {
  if (Array.isArray(expected)) {
    if (!Array.isArray(actual) || actual.length !== expected.length) {
      errors.push(`${location}: expected array length ${expected.length}`)
      return
    }
    expected.forEach((value, index) => matches(actual[index], value, `${location}[${index}]`))
    return
  }
  if (expected && typeof expected === "object") {
    if (!actual || typeof actual !== "object") {
      errors.push(`${location}: expected object`)
      return
    }
    for (const [key, value] of Object.entries(expected)) {
      matches(actual[key], value, `${location}.${key}`)
    }
    return
  }
  if (actual !== expected) {
    errors.push(`${location}: expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`)
  }
}

function exactArray(actual, expected, label) {
  if (!Array.isArray(actual) || actual.length !== expected.length
    || actual.some((value, index) => value !== expected[index])) {
    errors.push(`${label}: expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`)
  }
}

function operationCount(input) {
  const arrays = [input.helperOperations]
  return Object.keys(input ?? {}).length
    + Object.keys(input.cause ?? {}).length
    + arrays.reduce((total, value) => total + (Array.isArray(value) ? value.length : 0), 0)
}

if (corpus.$schema !== "w-io-error-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 40) errors.push("coverage")
exactArray(corpus.declaredKinds, ioErrorKinds, "declaredKinds")
exactArray(corpus.declaredOperations, ioOperations, "declaredOperations")

let operations = 0
let accepted = 0
let rejected = 0
for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^IOE0-(?:POS|NEG)-[a-z0-9-]+$/.test(item.id ?? "")) errors.push(`${location}: id`)
  if (ids.has(item.id)) errors.push(`${location}: duplicate ${item.id}`)
  ids.add(item.id)
  if (!["positive", "negative"].includes(item.kind)) errors.push(`${location}: kind`)
  if (!Array.isArray(item.decisions) || item.decisions.length === 0) {
    errors.push(`${location}: decisions`)
  }
  for (const decision of item.decisions ?? []) {
    decisions.add(decision)
    if (!["W-1308", "W-1309"].includes(decision)) {
      errors.push(`${location}: unexpected decision ${decision}`)
    }
  }

  const result = deriveIoError(item.input)
  matches(result, item.expect, item.id)
  if (item.kind === "positive" && result.accepted !== true) errors.push(`${item.id}: positive rejected`)
  if (item.kind === "negative" && result.accepted !== false) errors.push(`${item.id}: negative accepted`)
  if (result.accepted) accepted += 1
  else rejected += 1
  if (item.kind === "positive" && item.input?.subject === "error" && result.accepted) {
    coveredConditions.add(item.input.condition)
    coveredOperations.add(item.input.logicalOperation)
  }
  operations += operationCount(item.input)
  results.push({ id: item.id, kind: item.kind, decisions: item.decisions, result })
}

for (const decision of ["W-1308", "W-1309"]) {
  if (!decisions.has(decision)) errors.push(`missing decision ${decision}`)
}
for (const condition of Object.keys(conditionToKind)) {
  if (!coveredConditions.has(condition)) errors.push(`missing positive condition ${condition}`)
}
for (const operation of ioOperations) {
  if (!coveredOperations.has(operation)) errors.push(`missing positive operation ${operation}`)
}

const snapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`
if (write) fs.writeFileSync(snapshotPath, snapshot)
else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  errors.push("io-error-results.snapshot.jsonl is stale; run checker with --write")
}

if (errors.length > 0) {
  for (const error of errors) process.stderr.write(`- ${error}\n`)
  process.exit(1)
}

process.stdout.write(
  `I/O error IOE0: ${results.length} cases, ${operations} operations, `
    + `${accepted} accepted, ${rejected} rejected.\n`,
)
