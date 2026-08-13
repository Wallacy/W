import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { ledgerIdSet as designDecisionIds } from "./design-ledger.mjs"
import { runSharedControl } from "./shared-control-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const wDirectory = path.resolve(toolingDirectory, "..")
const casesPath = path.join(toolingDirectory, "shared-control-cases.json")
const snapshotPath = path.join(toolingDirectory, "shared-control-results.snapshot.jsonl")
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"))
const errors = []
const ids = new Set()
const results = []
let operationCount = 0

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`)
    return false
  }
  return true
}

function compare(actual, expected, location) {
  if (Array.isArray(expected)) {
    if (!Array.isArray(actual) || actual.length !== expected.length) {
      errors.push(`${location} array mismatch.`)
      return
    }
    expected.forEach((value, index) => compare(actual[index], value, `${location}[${index}]`))
    return
  }
  if (expected && typeof expected === "object") {
    if (!actual || typeof actual !== "object") {
      errors.push(`${location} must be an object.`)
      return
    }
    for (const [key, value] of Object.entries(expected)) compare(actual[key], value, `${location}.${key}`)
    return
  }
  if (actual !== expected) errors.push(`${location}: expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`)
}

function resolveReference(reference, location) {
  if (!reference || !requireString(reference.path, `${location}.path`)) return
  const resolved = path.resolve(toolingDirectory, reference.path)
  const relative = path.relative(wDirectory, resolved)
  if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
    errors.push(`${location}.path escapes the W repository.`)
    return
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${location}.path references a missing file.`)
    return
  }
  if (requireString(reference.symbol, `${location}.symbol`) && !fs.readFileSync(resolved, "utf8").includes(reference.symbol)) {
    errors.push(`${location}.symbol is absent from ${reference.path}.`)
  }
}

function validateSidecar(value, location) {
  if (value === undefined) return
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    errors.push(`${location} must be an object sidecar.`)
    return
  }
  if (value.payloadShareable !== undefined && typeof value.payloadShareable !== "boolean") {
    errors.push(`${location}.payloadShareable must be boolean.`)
  }
  if (value.counterThreadSafe !== undefined && typeof value.counterThreadSafe !== "boolean") {
    errors.push(`${location}.counterThreadSafe must be boolean.`)
  }
  for (const forbidden of ["originMobility", "allOriginsMobility", "controlBlockMobility", "payloadMobility"]) {
    if (value[forbidden] !== undefined) errors.push(`${location}.${forbidden} must be derived, not caller supplied.`)
  }
}

if (corpus.$schema !== "w-shared-control-cases-shc0") errors.push("schema")
if (corpus.status !== "design-oracle-input-shc0") errors.push("status")
if (corpus.machine !== "shared-control-machine-shc0") errors.push("machine")
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) errors.push("cases")

for (const [caseIndex, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${caseIndex}]`
  if (!/^SHC0-(?:POS|NEG)-[a-z0-9-]+$/.test(testCase.id ?? "")) errors.push(`${location}.id form`)
  if (ids.has(testCase.id)) errors.push(`${location}.id duplicate`)
  ids.add(testCase.id)
  if (!new Set(["positive", "negative"]).has(testCase.kind)) errors.push(`${location}.kind`)
  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) errors.push(`${location}.decisions`)
  for (const [decisionIndex, decision] of (testCase.decisions ?? []).entries()) {
    if (!designDecisionIds.has(decision)) errors.push(`${location}.decisions[${decisionIndex}] unknown ${decision}`)
  }
  if (!Array.isArray(testCase.references) || testCase.references.length === 0) errors.push(`${location}.references`)
  for (const [referenceIndex, reference] of (testCase.references ?? []).entries()) resolveReference(reference, `${location}.references[${referenceIndex}]`)
  if (!Array.isArray(testCase.operations) || testCase.operations.length === 0) errors.push(`${location}.operations`)
  validateSidecar(testCase.analysisFacts, `${location}.analysisFacts`)
  if (testCase.outerAllocatorLease !== undefined && testCase.outerAllocatorLease !== "ASC0") {
    errors.push(`${location}.outerAllocatorLease must be ASC0.`)
  }
  for (const [operationIndex, operation] of (testCase.operations ?? []).entries()) {
    validateSidecar(operation.analysisFacts, `${location}.operations[${operationIndex}].analysisFacts`)
    if (operation.providerProfile && (operation.providerProfile.payloadShareable !== undefined || operation.providerProfile.counterThreadSafe !== undefined || operation.providerProfile.allOriginsMobility !== undefined)) {
      errors.push(`${location}.operations[${operationIndex}].providerProfile must not carry HIR/lowering facts.`)
    }
  }
  operationCount += testCase.operations?.length ?? 0
  if (!testCase.expected || !["accepted", "error", "fault", "rejected"].includes(testCase.expected.status)) errors.push(`${location}.expected.status`)

  const actual = runSharedControl({ operations: testCase.operations })
  if (testCase.kind === "positive" && actual.status !== "accepted") errors.push(`${testCase.id} positive rejected`)
  if (testCase.kind === "negative" && actual.status === "accepted") errors.push(`${testCase.id} negative accepted`)
  compare(actual, testCase.expected, `${testCase.id}.expected`)
  results.push({
    caseId: testCase.id,
    status: actual.status,
    ...(actual.code ? { code: actual.code, operation: actual.operation } : {}),
    facts: actual.facts,
    state: actual.state,
    trace: actual.trace,
  })
}

const snapshot = [
  JSON.stringify({ schema: "w-shared-control-results-shc0", status: "design-oracle-output-shc0" }),
  ...results.map((result) => JSON.stringify(result)),
].join("\n") + "\n"

if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, snapshot)
} else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  errors.push("snapshot stale")
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`)
  process.exit(1)
}

const counts = Object.fromEntries(["accepted", "error", "fault", "rejected"].map((status) => [status, results.filter((result) => result.status === status).length]))
process.stdout.write(`Shared control SHC0: ${results.length} cases, ${operationCount} operations, ${counts.accepted} accepted, ${counts.error} errors, ${counts.fault} faults, ${counts.rejected} rejected.\n`)
