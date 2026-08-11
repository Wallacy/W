import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { ledgerIdSet } from "./design-ledger.mjs"
import { runChannelOperations } from "./channel-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const rootDirectory = path.resolve(toolingDirectory, "..")
const casesPath = path.join(toolingDirectory, "channel-cases.json")
const snapshotPath = path.join(toolingDirectory, "channel-results.snapshot.jsonl")
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"))
const errors = []
const ids = new Set()
const results = []

function equal(actual, expected) {
  return JSON.stringify(actual) === JSON.stringify(expected)
}

function expectSubset(actual, expected, location) {
  if (Array.isArray(expected) || expected === null || typeof expected !== "object") {
    if (!equal(actual, expected)) {
      errors.push(
        `${location} expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`,
      )
    }
    return
  }
  if (actual === null || typeof actual !== "object" || Array.isArray(actual)) {
    errors.push(`${location} expected object, actual ${JSON.stringify(actual)}`)
    return
  }
  for (const [key, value] of Object.entries(expected)) {
    expectSubset(actual[key], value, `${location}.${key}`)
  }
}

if (corpus.$schema !== "w-channel-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 35) errors.push("coverage")

for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^CH0-(?:accepted|rejected)-[a-z0-9-]+$/.test(item.id ?? "")) {
    errors.push(`${location}: id`)
  }
  if (ids.has(item.id)) errors.push(`${location}: duplicate id ${item.id}`)
  ids.add(item.id)
  if (!new Set(["accepted", "rejected"]).has(item.kind)) {
    errors.push(`${location}: kind`)
  }
  if (!Array.isArray(item.operations) || item.operations.length === 0) {
    errors.push(`${location}: operations`)
    continue
  }
  if (!Array.isArray(item.decisions) || item.decisions.length === 0) {
    errors.push(`${location}: decisions`)
  } else {
    for (const decision of item.decisions) {
      if (!ledgerIdSet.has(decision)) errors.push(`${item.id}: unknown decision ${decision}`)
    }
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

  const result = runChannelOperations(item.operations)
  if (result.status !== item.kind) {
    errors.push(`${item.id}: kind ${item.kind} does not match derived ${result.status}`)
  }
  const { physicalStrategy, ...expectedResult } = item.expect ?? {}
  expectSubset(result, expectedResult, item.id)
  if (physicalStrategy !== undefined) {
    expectSubset(
      result.physical.strategy,
      physicalStrategy,
      `${item.id}.physicalStrategy`,
    )
  }

  if (item.kind === "accepted") {
    if (item.operations.at(-1)?.op !== "finish") {
      errors.push(`${item.id}: accepted case must end with finish`)
    }
    const unterminated = Object.entries(result.state.terminalItems)
      .filter(([, terminal]) => terminal === null)
      .map(([id]) => id)
    if (unterminated.length > 0) {
      errors.push(`${item.id}: unterminated items ${unterminated.join(", ")}`)
    }
  }
  results.push({ caseId: item.id, ...result })
}

for (const decision of [
  "W-458",
  "W-459",
  "W-460",
  "W-461",
  "W-462",
  "W-463",
  "W-464",
  "W-465",
  "W-466",
  "W-467",
  "W-471",
  "W-472",
]) {
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
  process.stderr.write(`${errors.slice(0, 80).join("\n")}\n`)
  process.exit(1)
}

const operationCount = corpus.cases.reduce(
  (total, item) => total + item.operations.length,
  0,
)
const accepted = corpus.cases.filter((item) => item.kind === "accepted").length
const rejected = corpus.cases.length - accepted
process.stdout.write(
  `CH0 channel: ${corpus.cases.length} cases, ${operationCount} operations ` +
  `(${accepted} accepted, ${rejected} rejected).\n`,
)
