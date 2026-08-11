import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { ledgerIdSet } from "./design-ledger.mjs"
import { runForeignBodyOperations } from "./foreign-body-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const rootDirectory = path.resolve(toolingDirectory, "..")
const corpusPath = path.join(toolingDirectory, "foreign-body-cases.json")
const snapshotPath = path.join(toolingDirectory, "foreign-body-results.snapshot.jsonl")
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"))
const errors = []
const ids = new Set()
const results = []

function equal(left, right) {
  return JSON.stringify(left) === JSON.stringify(right)
}

function expectField(caseId, field, actual, expected) {
  if (expected === undefined) return
  if (!equal(actual, expected)) {
    errors.push(`${caseId}: ${field} expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`)
  }
}

function expandOperation(operation) {
  if (operation.op !== "resolve" || operation.profile === undefined) return operation
  const profile = corpus.profiles?.[operation.profile]
  if (!profile) return operation
  return { op: "resolve", ...profile }
}

if (corpus.$schema !== "w-foreign-body-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 30) errors.push("coverage")

for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^FB0-(?:accepted|rejected|info)-[a-z0-9-]+$/.test(item.id ?? "")) {
    errors.push(`${location}: id`)
  }
  if (ids.has(item.id)) errors.push(`${location}: duplicate id ${item.id}`)
  ids.add(item.id)
  if (!new Set(["accepted", "rejected", "info"]).has(item.kind)) errors.push(`${location}: kind`)
  if (!Array.isArray(item.decisions) || item.decisions.length === 0) errors.push(`${location}: decisions`)
  for (const decision of item.decisions ?? []) {
    if (!ledgerIdSet.has(decision)) errors.push(`${location}: unknown decision ${decision}`)
  }
  if (!item.expect || typeof item.expect !== "object" || Array.isArray(item.expect)) {
    errors.push(`${location}: expect`)
  }
  if (!Array.isArray(item.operations) || item.operations.length === 0) {
    errors.push(`${location}: operations`)
    continue
  }

  const sourcePath = path.join(rootDirectory, item.source?.path ?? "")
  if (!fs.existsSync(sourcePath)) {
    errors.push(`${item.id}: missing source ${item.source?.path}`)
  } else if (!fs.readFileSync(sourcePath, "utf8").includes(item.source?.symbol ?? "")) {
    errors.push(`${item.id}: missing symbol ${item.source?.symbol}`)
  }

  const operations = item.operations.map(expandOperation)
  if (operations.some((operation) => operation.profile !== undefined)) {
    errors.push(`${item.id}: unknown resolve profile`)
  }
  const result = runForeignBodyOperations(operations)
  if (result.status !== item.kind) {
    errors.push(`${item.id}: kind ${item.kind} does not match derived ${result.status}`)
  }

  const expected = item.expect ?? {}
  const scan = result.state.scan
  const suffixSource = [...operations].reverse().find((operation) =>
    operation.op === "scan" || operation.op === "replaceSource")?.source
  const suffix = typeof suffixSource === "string" && scan
    ? Buffer.from(suffixSource).subarray(scan.nextOffset).toString()
    : undefined
  const bodyText = scan ? Buffer.from(scan.exactBytesHex, "hex").toString() : undefined

  expectField(item.id, "error", result.error, expected.error)
  expectField(item.id, "reason", result.errorFacts?.reason, expected.reason)
  expectField(item.id, "phase", result.state.phase, expected.phase)
  expectField(item.id, "language", result.state.language, expected.language)
  expectField(item.id, "authoritative", result.state.authoritative, expected.authoritative)
  expectField(item.id, "byteLength", scan?.byteLength, expected.byteLength)
  expectField(item.id, "closeOffset", scan?.closeOffset, expected.closeOffset)
  expectField(item.id, "maximumDepthObserved", scan?.maximumDepthObserved, expected.maximumDepthObserved)
  expectField(item.id, "suffix", suffix, expected.suffix)
  expectField(item.id, "revision", result.state.revision, expected.revision)
  expectField(item.id, "bodyText", bodyText, expected.bodyText)
  expectField(item.id, "mappedDiagnostics", result.state.mappedDiagnostics, expected.mappedDiagnostics)
  results.push({ caseId: item.id, ...result })
}

for (const decision of ["W-1261", "W-1262", "W-1263", "W-1264", "W-1265", "W-1266"]) {
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

const operationCount = corpus.cases.reduce((sum, item) => sum + item.operations.length, 0)
const accepted = corpus.cases.filter((item) => item.kind === "accepted").length
const rejected = corpus.cases.filter((item) => item.kind === "rejected").length
const information = corpus.cases.filter((item) => item.kind === "info").length
process.stdout.write(
  `Foreign bodies FB0: ${corpus.cases.length} cases, ${operationCount} operations ` +
  `(${accepted} accepted, ${rejected} rejected, ${information} info).\n`,
)
