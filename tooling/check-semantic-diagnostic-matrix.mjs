import { readFile } from "node:fs/promises"
import { resolve } from "node:path"
import { ledgerIdSet } from "./design-ledger.mjs"
import { runMatrixCase, checkerContextFields, semanticFields } from "./semantic-diagnostic-matrix-machine.mjs"
import { deriveSemanticRulePairs } from "./semantic-diagnostic-pairs.mjs"

const tooling = resolve(import.meta.dir)
const corpus = JSON.parse(await readFile(resolve(tooling, "semantic-diagnostic-matrix-cases.json"), "utf8"))
const catalog = JSON.parse(await readFile(resolve(tooling, "diagnostic-catalog.json"), "utf8"))
const snapshotPath = resolve(tooling, "semantic-diagnostic-matrix.snapshot.jsonl")
const writeSnapshot = process.argv.includes("--write")
if (corpus.$schema !== "w-semantic-diagnostic-matrix-1" || corpus.status !== "design-oracle-input") {
  throw new Error("semantic diagnostic matrix: invalid schema or status")
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) throw new Error("semantic diagnostic matrix: cases are missing")

const ids = new Set()
const decisions = new Set()
const coverage = { oracleAccepted: 0, oracleRejected: 0, semanticAccepted: 0, semanticRejected: 0, decisions: new Set(), contextFields: new Set(), semanticFields: new Set() }
const snapshotRecords = []
const pairs = deriveSemanticRulePairs(JSON.parse(await readFile(resolve(tooling, "semantic-cases.json"), "utf8")).cases, ledgerIdSet)
for (const [index, testCase] of corpus.cases.entries()) {
  if (!/^SDM0-(accepted|rejected)-[a-z0-9-]+$/.test(testCase.id)) throw new Error(`matrix case ${index} has an invalid id`)
  if (ids.has(testCase.id)) throw new Error(`matrix case ${testCase.id} is duplicated`)
  ids.add(testCase.id)
  if (!(testCase.kind === "accepted" || testCase.kind === "negative")) throw new Error(`${testCase.id} has invalid kind`)
  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) throw new Error(`${testCase.id} has no decisions`)
  for (const decision of testCase.decisions) {
    if (!ledgerIdSet.has(decision)) throw new Error(`${testCase.id} references unknown ${decision}`)
    decisions.add(decision)
  }
  const result = runMatrixCase(testCase.input, catalog)
  snapshotRecords.push({
    caseId: testCase.id,
    oracleStatus: result.status,
    semanticOutcome: result.state?.outcome?.status ?? null,
    error: result.error,
    contextFields: result.state?.contextCoverage ?? [],
    semanticFields: result.state ? Object.keys(result.state.semanticResult) : [],
    diagnosticCodes: result.state?.diagnostics?.map((diagnostic) => diagnostic.code) ?? [],
    diagnostics: result.state?.diagnostics ?? [],
    diagnosticJsonl: result.state?.diagnosticJsonl ?? null,
    outcome: result.state?.outcome ?? null,
    normalized: result.state?.normalized ?? false,
  })
  const expected = testCase.expect ?? {}
  if (testCase.kind === "accepted" && result.status !== "accepted") throw new Error(`${testCase.id} is a valid case but the oracle rejected it`)
  if (testCase.kind === "negative" && result.status !== "rejected") throw new Error(`${testCase.id} is an invalid case but the oracle accepted it`)
  if (result.status !== expected.status) throw new Error(`${testCase.id} expected ${expected.status}, got ${result.status} (${result.error ?? "ok"})`)
  if (expected.error !== undefined && result.error !== expected.error) throw new Error(`${testCase.id} expected error ${expected.error}, got ${result.error}`)
  if (expected.semanticOutcome !== undefined && result.state?.outcome?.status !== expected.semanticOutcome) throw new Error(`${testCase.id} semantic outcome mismatch`)
  const semanticOutcome = result.state?.outcome?.status ?? null
  if (result.status === "accepted") {
    coverage.oracleAccepted += 1
    if (semanticOutcome === "accepted") coverage.semanticAccepted += 1
    if (semanticOutcome === "rejected") coverage.semanticRejected += 1
    if (result.state.contextCoverage.length !== (expected.contextFields ?? checkerContextFields.length)) throw new Error(`${testCase.id} context coverage is incomplete`)
    if (Object.keys(result.state.semanticResult).length !== (expected.semanticFields ?? semanticFields.length)) throw new Error(`${testCase.id} semantic result coverage is incomplete`)
    for (const field of result.state.contextCoverage) coverage.contextFields.add(field)
    for (const field of Object.keys(result.state.semanticResult)) coverage.semanticFields.add(field)
    const expectedDiagnosticJsonl = result.state.diagnostics.length === 0 ? "" : `${result.state.diagnostics.map((diagnostic) => JSON.stringify(diagnostic)).join("\n")}\n`
    if (result.state.diagnosticJsonl !== expectedDiagnosticJsonl) throw new Error(`${testCase.id} D0 JSONL bytes are not canonical`)
    const outcomeKeys = Object.keys(result.state.outcome)
    const expectedOutcomeKeys = result.state.outcome.status === "accepted"
      ? ["schemaVersion", "sourceDigest", "focus", "status", "semanticResult"]
      : ["schemaVersion", "sourceDigest", "focus", "status", "failure"]
    if (outcomeKeys.join(",") !== expectedOutcomeKeys.join(",")) throw new Error(`${testCase.id} S0 outcome field order is not canonical`)
    if (expected.diagnostics !== undefined && result.state.diagnostics.length !== expected.diagnostics) throw new Error(`${testCase.id} diagnostic count mismatch`)
    if (expected.fixedPoint !== undefined && result.state.loops[0]?.fixedPoint !== expected.fixedPoint) throw new Error(`${testCase.id} fixed point mismatch`)
  } else {
    coverage.oracleRejected += 1
  }
  if (expected.code !== undefined && !result.state?.diagnostics?.some((diagnostic) => diagnostic.code === expected.code)) throw new Error(`${testCase.id} expected diagnostic ${expected.code}`)
  if (expected.lastCode !== undefined && result.state?.diagnostics?.at(-1)?.code !== expected.lastCode) throw new Error(`${testCase.id} expected last diagnostic ${expected.lastCode}`)
  if (expected.incomplete !== undefined && !result.state?.diagnostics?.some((diagnostic) => diagnostic.facts?.incomplete === expected.incomplete)) throw new Error(`${testCase.id} truncation flag mismatch`)
  if (expected.promoted !== undefined && result.state?.policy?.promotedWarnings?.find((warning) => warning.code === expected.promoted)?.severity !== "error") throw new Error(`${testCase.id} warning promotion mismatch`)
  if (testCase.kind === "negative" && testCase.decisions.length !== 1) throw new Error(`${testCase.id} must exercise one failure contract`)
}

const sourceCorpus = JSON.parse(await readFile(resolve(tooling, "semantic-cases.json"), "utf8"))
const sourceRules = new Set(sourceCorpus.cases.map((testCase) => testCase.rule))
const requiredSourceRules = ["W-785", "W-788"]
for (const rule of requiredSourceRules) if (!sourceRules.has(rule)) throw new Error(`semantic source rule ${rule} is missing`)
for (const field of checkerContextFields) if (!coverage.contextFields.has(field)) throw new Error(`CheckerContext field ${field} has no derived evidence`)
for (const field of semanticFields) if (!coverage.semanticFields.has(field)) throw new Error(`SemanticResult field ${field} has no derived evidence`)

const expectedSnapshot = `${snapshotRecords.map((record) => JSON.stringify(record)).join("\n")}\n`
if (writeSnapshot) {
  await Bun.write(snapshotPath, expectedSnapshot)
} else {
  const currentSnapshot = await Bun.file(snapshotPath).text()
  if (currentSnapshot !== expectedSnapshot) throw new Error("semantic diagnostic matrix snapshot is stale; run with --write")
}

console.log(
  `Semantic diagnostic matrix: ${corpus.cases.length} cases (${coverage.oracleAccepted} oracle accepted, ${coverage.oracleRejected} oracle rejected; ` +
    `${coverage.semanticAccepted} semantic accepted, ${coverage.semanticRejected} semantic rejected), ` +
    `${decisions.size} decisions, ${coverage.contextFields.size}/${checkerContextFields.length} CheckerContext fields, ` +
    `${coverage.semanticFields.size}/${semanticFields.length} SemanticResult fields.`,
)
