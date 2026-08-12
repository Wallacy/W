import { createHash } from "node:crypto"

export const semanticResultFields = new Set([
  "resultType",
  "category",
  "flow",
  "ownerDelta",
  "effectSummary",
  "proofFacts",
  "evaluationGraph",
])

function canonicalJson(value) {
  if (Array.isArray(value)) return `[${value.map(canonicalJson).join(",")}]`
  if (value !== null && typeof value === "object") {
    return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonicalJson(value[key])}`).join(",")}}`
  }
  return JSON.stringify(value)
}

function valueDigest(value) {
  return `sha256:${createHash("sha256").update(canonicalJson(value), "utf8").digest("hex")}`
}

export function deriveSemanticRulePairs(cases, ledgerIds = null) {
  if (!Array.isArray(cases)) throw new Error("semantic pairs: cases must be an array")
  const known = ledgerIds ? new Set(ledgerIds) : null
  const byRule = new Map()
  const ids = new Set()
  for (const testCase of cases) {
    if (!testCase || typeof testCase !== "object") throw new Error("semantic pairs: case must be an object")
    if (typeof testCase.id !== "string" || ids.has(testCase.id)) throw new Error(`semantic pairs: duplicate or missing case id ${testCase.id}`)
    ids.add(testCase.id)
    if (typeof testCase.rule !== "string" || (known && !known.has(testCase.rule))) throw new Error(`semantic pairs: invalid rule on ${testCase.id}`)
    const pair = byRule.get(testCase.rule) ?? { positives: [], negatives: [] }
    if (testCase.kind === "positive") pair.positives.push(testCase)
    else if (testCase.kind === "negative") pair.negatives.push(testCase)
    else throw new Error(`semantic pairs: invalid kind on ${testCase.id}`)
    byRule.set(testCase.rule, pair)
  }
  for (const [rule, pair] of byRule) {
    if (pair.positives.length === 0) throw new Error(`semantic pairs: ${rule} has no positive case`)
    if (pair.negatives.length === 0) throw new Error(`semantic pairs: ${rule} has no negative case`)
    const positiveIds = new Set(pair.positives.map((testCase) => testCase.id))
    const used = new Set()
    for (const negative of pair.negatives) {
      if (!positiveIds.has(negative.baseline)) throw new Error(`semantic pairs: ${negative.id} baseline is not a positive of ${rule}`)
      if (used.has(negative.baseline)) throw new Error(`semantic pairs: ${negative.id} reuses baseline ${negative.baseline}`)
      used.add(negative.baseline)
      if (!semanticResultFields.has(negative.failureField)) throw new Error(`semantic pairs: ${negative.id} has invalid failureField`)
      if (!negative.failureEvidence || typeof negative.failureEvidence !== "object" || Array.isArray(negative.failureEvidence)) {
        throw new Error(`semantic pairs: ${negative.id} has no failureEvidence`)
      }
      const evidenceKeys = Object.keys(negative.failureEvidence).sort()
      if (evidenceKeys.join(",") !== "baselineValueDigest,field") throw new Error(`semantic pairs: ${negative.id} has invalid failureEvidence shape`)
      if (negative.failureEvidence.field !== negative.failureField) throw new Error(`semantic pairs: ${negative.id} failureEvidence field disagrees with failureField`)
      const baselineResult = pair.positives.find((positive) => positive.id === negative.baseline)?.expect?.semanticResult
      if (!baselineResult || negative.failureEvidence.baselineValueDigest !== valueDigest(baselineResult[negative.failureField])) {
        throw new Error(`semantic pairs: ${negative.id} failureEvidence digest does not match baseline`)
      }
      if (negative.expect?.failure?.field !== undefined && negative.expect.failure.field !== negative.failureField) {
        throw new Error(`semantic pairs: ${negative.id} failure field disagrees with expect.failure.field`)
      }
    }
    if (used.size !== pair.positives.length) throw new Error(`semantic pairs: ${rule} has an unpaired positive case`)
  }
  return byRule
}
