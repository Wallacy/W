import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import {
  countKernelModuleOperations,
  deriveKernelModuleContract,
  prepareKernelModuleCase,
} from "./kernel-module-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const casesPath = path.join(toolingDirectory, "kernel-module-cases.json")
const snapshotPath = path.join(toolingDirectory, "kernel-module-results.snapshot.jsonl")
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"))
const errors = []
const ids = new Set()
const decisions = new Set()
const results = []

function matches(actual, expected, location) {
  if (Array.isArray(expected)) {
    if (!Array.isArray(actual) || actual.length !== expected.length) {
      errors.push(`${location}: array length`)
      return
    }
    expected.forEach((value, index) => matches(actual[index], value, `${location}[${index}]`))
    return
  }
  if (expected && typeof expected === "object") {
    if (!actual || typeof actual !== "object") {
      errors.push(`${location}: object`)
      return
    }
    for (const [key, value] of Object.entries(expected)) matches(actual[key], value, `${location}.${key}`)
    return
  }
  if (actual !== expected) {
    errors.push(`${location}: expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`)
  }
}

if (corpus.$schema !== "w-kernel-module-cases-1") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 20) errors.push("coverage")

let operations = 0
let accepted = 0
for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^KM0-(?:POS|NEG)-[a-z0-9-]+$/.test(item.id ?? "")) errors.push(`${location}: id`)
  if (ids.has(item.id)) errors.push(`${location}: duplicate`)
  ids.add(item.id)
  if (!new Set(["positive", "negative"]).has(item.kind)) errors.push(`${location}: kind`)
  if (!Array.isArray(item.decisions) || item.decisions.length === 0) errors.push(`${location}: decisions`)
  for (const decision of item.decisions ?? []) decisions.add(decision)

  const input = prepareKernelModuleCase(corpus, item)
  const result = deriveKernelModuleContract(input)
  matches(result, item.expect, item.id)
  if (item.kind === "positive" && result.accepted !== true) errors.push(`${item.id}: positive rejected`)
  if (item.kind === "negative" && result.accepted !== false) errors.push(`${item.id}: negative accepted`)
  if (result.accepted) accepted += 1
  operations += countKernelModuleOperations(input)
  results.push({ id: item.id, kind: item.kind, decisions: item.decisions, result })
}

for (const decision of ["W-1211", "W-1284", "W-1285", "W-1286", "W-1287"]) {
  const covered = corpus.cases.filter((item) => item.decisions.includes(decision))
  if (!covered.some((item) => item.kind === "positive")) errors.push(`${decision}: positive`)
  if (!covered.some((item) => item.kind === "negative")) errors.push(`${decision}: negative`)
}

const snapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`
if (process.argv.includes("--write")) fs.writeFileSync(snapshotPath, snapshot)
else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  errors.push("kernel-module-results.snapshot.jsonl is stale; run checker with --write")
}

if (errors.length > 0) {
  process.stderr.write(`${errors.slice(0, 80).join("\n")}\n`)
  process.exit(1)
}

process.stdout.write(
  `Kernel module KM0: ${corpus.cases.length} cases, ${operations} operations `
    + `(${accepted} accepted, ${corpus.cases.length - accepted} rejected).\n`,
)
