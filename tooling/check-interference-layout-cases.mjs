import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import {
  expandInterferenceLayoutOperations,
  runInterferenceLayoutOperations,
} from "./interference-layout-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const rootDirectory = path.resolve(toolingDirectory, "..")
const casesPath = path.join(toolingDirectory, "interference-layout-cases.json")
const snapshotPath = path.join(toolingDirectory, "interference-layout-results.snapshot.jsonl")
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

if (corpus.$schema !== "w-interference-layout-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 20) errors.push("coverage")

for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^IL0-(?:accepted|rejected)-[a-z0-9-]+$/.test(item.id ?? "")) {
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

  const operations = expandInterferenceLayoutOperations(corpus.fixtures, item.operations)
  const result = runInterferenceLayoutOperations(operations)
  if (result.status !== item.kind) {
    errors.push(`${item.id}: kind ${item.kind} does not match derived ${result.status}`)
  }
  const expected = item.expect ?? {}
  expectField(item.id, "status", result.status, expected.status)
  expectField(item.id, "error", result.error, expected.error)
  expectField(item.id, "selectionStatus", result.state.selection?.status ?? null, expected.selectionStatus)
  expectField(item.id, "selectionReason", result.state.selection?.reason ?? null, expected.selectionReason)
  expectField(
    item.id,
    "additionalBytes",
    result.state.selection?.layout?.additionalBytes ?? null,
    expected.additionalBytes,
  )
  expectField(
    item.id,
    "guaranteedExclusiveCacheLine",
    result.state.selection?.guaranteedExclusiveCacheLine ?? null,
    expected.guaranteedExclusiveCacheLine,
  )
  expectField(item.id, "partitionStatus", result.state.partition?.status ?? null, expected.partitionStatus)
  expectField(item.id, "globalAtomicPreserved", result.state.globalAtomicPreserved, expected.globalAtomicPreserved)
  expectField(
    item.id,
    "physicalPerformanceGuarantee",
    result.state.physicalBoundary?.performanceGuarantee ?? null,
    expected.physicalPerformanceGuarantee,
  )
  if (expected.traceIncludes) {
    for (const event of expected.traceIncludes) {
      if (!result.state.trace.includes(event)) errors.push(`${item.id}: missing trace ${event}`)
    }
  }
  results.push({ caseId: item.id, ...result })
}

for (const decision of ["W-1269", "W-1270", "W-1271", "W-1272", "W-1273"]) {
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
  (total, item) => total + expandInterferenceLayoutOperations(corpus.fixtures, item.operations).length,
  0,
)
const accepted = corpus.cases.filter((item) => item.kind === "accepted").length
const rejected = corpus.cases.length - accepted
process.stdout.write(
  `IL0 interference layout: ${corpus.cases.length} cases, ${operationCount} operations ` +
  `(${accepted} accepted, ${rejected} rejected).\n`,
)
