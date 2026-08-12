import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { runAllocatorScope } from "./allocator-scope-machine.mjs"

const tooling = path.dirname(fileURLToPath(import.meta.url))
const corpus = JSON.parse(fs.readFileSync(path.join(tooling, "allocator-scope-cases.json"), "utf8"))
const snapshotPath = path.join(tooling, "allocator-scope-results.snapshot.jsonl")
const write = process.argv.includes("--write")
const errors = []
const ids = new Set()
const results = []

function matches(actual, expected, location) {
  if (Array.isArray(expected)) {
    if (!Array.isArray(actual) || actual.length !== expected.length) errors.push(`${location}: array`)
    else expected.forEach((value, index) => matches(actual[index], value, `${location}[${index}]`))
  } else if (expected && typeof expected === "object") {
    for (const [key, value] of Object.entries(expected)) matches(actual?.[key], value, `${location}.${key}`)
  } else if (actual !== expected) errors.push(`${location}: expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`)
}

if (corpus.$schema !== "w-allocator-scope-cases-asc0" || corpus.status !== "design-oracle-input") errors.push("schema/status")
for (const [index, item] of corpus.cases.entries()) {
  const location = `cases[${index}]`
  if (!/^ASC0-(?:POS|NEG)-[a-z0-9-]+$/.test(item.id ?? "") || ids.has(item.id)) errors.push(`${location}: id`)
  ids.add(item.id)
  if (!Array.isArray(item.decisions) || item.decisions.length === 0) errors.push(`${location}: decisions`)
  for (const ref of item.references ?? []) if (!fs.existsSync(path.resolve(tooling, ref.path))) errors.push(`${location}: missing reference`)
  const result = runAllocatorScope(item.input)
  matches(result, item.expect, item.id)
  if (item.kind === "positive" && result.accepted !== true) errors.push(`${item.id}: rejected positive`)
  if (item.kind === "negative" && result.accepted !== false) errors.push(`${item.id}: accepted negative`)
  results.push({ id: item.id, kind: item.kind, decisions: item.decisions, result })
}
const snapshot = `${results.map((item) => JSON.stringify(item)).join("\n")}\n`
if (write) fs.writeFileSync(snapshotPath, snapshot)
else if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== snapshot) errors.push("snapshot stale")
if (errors.length) { for (const error of errors) console.error(`- ${error}`); process.exit(1) }
console.log(`Allocator scope ASC0: ${results.length} cases.`)
