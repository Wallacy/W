import assert from "node:assert/strict"
import test from "node:test"
import { createHash } from "node:crypto"
import { runMatrixCase } from "./semantic-diagnostic-matrix-machine.mjs"
import { deriveSemanticRulePairs } from "./semantic-diagnostic-pairs.mjs"

const catalog = await Bun.file(new URL("./diagnostic-catalog.json", import.meta.url)).json()

function base(events = []) {
  return {
    events: [
      { op: "source", id: "source", text: "café\n" },
      { op: "context", context: { scope: "test", expected: null, owners: {}, effects: {}, controls: {}, facts: {}, constMode: "runtime" } },
      { op: "interface", schemaId: "w-ast-hir-s0", version: 1, domains: [], backendSchemas: [] },
      { op: "node", id: "value", operation: "read", subject: "café", resultType: "Int", category: "value", flow: { kind: "next" }, ownerDelta: [], effectSummary: { signature: [], control: [], operational: [] }, proofFacts: [], edges: [] },
      { op: "focus", source: "source", startByte: 0, endByte: 5 },
      ...events,
    ],
  }
}

function diagnostic(overrides = {}) {
  return {
    op: "diagnostic",
    id: "d",
    code: "W-SEM-0001",
    phase: "semantic.type",
    severity: "error",
    primary: { source: "source", startByte: 0, endByte: 5 },
    labels: [],
    facts: { actual: "Int", expected: "Ticket" },
    notes: [],
    fixes: [],
    root: null,
    ...overrides,
  }
}

test("the matrix derives all seven SemanticResult fields", () => {
  const result = runMatrixCase(base(), catalog)
  assert.equal(result.status, "accepted")
  assert.deepEqual(Object.keys(result.state.semanticResult), ["resultType", "category", "flow", "ownerDelta", "effectSummary", "proofFacts", "evaluationGraph"])
})

test("D0 records expose only the normative field order and instances", () => {
  const result = runMatrixCase(base([diagnostic()]), catalog)
  assert.equal(result.status, "accepted")
  assert.deepEqual(Object.keys(result.state.diagnostics[0]), ["schemaVersion", "instance", "code", "phase", "severity", "primary", "labels", "facts", "notes", "fixes", "root"])
  assert.equal(result.state.diagnostics[0].root, null)
  assert.equal(result.state.diagnostics[0].instance, "D000001")
  assert.equal(result.state.diagnosticJsonl, `${JSON.stringify(result.state.diagnostics[0])}\n`)
})

test("a dangling graph edge is rejected", () => {
  const input = base()
  input.events.find((event) => event.op === "node").edges = [{ from: "value", to: "missing", kind: "next" }]
  assert.equal(runMatrixCase(input, catalog).error, "matrixGraphDanglingEdge")
})

test("implicit or global checker context is rejected", () => {
  const input = base()
  input.events = input.events.filter((event) => event.op !== "context")
  assert.equal(runMatrixCase(input, catalog).error, "matrixContextMissing")
})

test("a loop move cannot pass through a fixed-point back edge", () => {
  const input = base([
    { op: "loop", id: "loop", entry: [{ place: "value", state: "available" }], continues: [{ place: "value", state: "moved" }], backEdges: [{ target: "loop", state: [{ place: "value", state: "moved" }] }], breaks: [], widening: { mode: "monotone", acceptsMove: false } },
  ])
  assert.equal(runMatrixCase(input, catalog).error, "matrixLoopUnsafeMove")
})

test("the interface rejects backend-specific result schemas", () => {
  const input = base()
  input.events.find((event) => event.op === "interface").backendSchemas = ["backend-result-v1"]
  assert.equal(runMatrixCase(input, catalog).error, "matrixBackendSchemaPresent")
})

test("facts do not carry secrets", () => {
  const input = base()
  input.events.find((event) => event.op === "context").context.facts = { token: "private" }
  assert.equal(runMatrixCase(input, catalog).error, "matrixSecretFact")
})

test("a stale or overlapping fix is rejected", () => {
  const source = "unused\n"
  const digest = `sha256:${createHash("sha256").update(source, "utf8").digest("hex")}`
  const input = base([
    diagnostic({ code: "W-USE-0001", phase: "semantic.flow", facts: { resultType: "Int" }, fixes: [{ id: "discard-value", titleKey: "fix", applicability: "review", proof: { source: "source", digest }, edits: [{ source: "source", startByte: 0, endByte: 4, text: "let _ = " }, { source: "source", startByte: 3, endByte: 5, text: "x" }] }] }),
  ])
  input.events.find((event) => event.op === "source").text = source
  assert.equal(runMatrixCase(input, catalog).error, "matrixFixOverlap")
})

test("poison children cannot form a repeated cascade", () => {
  const input = base([diagnostic(), diagnostic({ id: "child", root: "d", poison: true })])
  assert.equal(runMatrixCase(input, catalog).error, "matrixPoisonCascade")
})

test("diagnostics normalize order and preserve truncation failure", () => {
  const input = base([{ op: "limit", maximum: 1 }, diagnostic({ id: "second", primary: { source: "source", startByte: 0, endByte: 1 } }), diagnostic({ id: "first" })])
  input.events.find((event) => event.op === "source").text = "value\n"
  const result = runMatrixCase(input, catalog)
  assert.equal(result.status, "accepted")
  assert.equal(result.state.diagnostics.at(-1).code, "W-DIAGNOSTIC-0001")
  assert.equal(result.state.diagnostics.at(-1).facts.incomplete, true)
})

test("complete diagnostic permutations produce byte-identical JSONL", () => {
  const first = base([diagnostic({ id: "root" }), diagnostic({ id: "child", root: "root", primary: { source: "source", startByte: 1, endByte: 2 } })])
  const second = base([diagnostic({ id: "child", root: "root", primary: { source: "source", startByte: 1, endByte: 2 } }), diagnostic({ id: "root" })])
  const left = runMatrixCase(first, catalog)
  const right = runMatrixCase(second, catalog)
  assert.equal(left.state.diagnosticJsonl, right.state.diagnosticJsonl)
})

test("diagnostic records reject unknown fields before output", () => {
  const input = base([diagnostic({ extra: "must-not-leak" })])
  assert.equal(runMatrixCase(input, catalog).error, "matrixUnexpectedFields")
})

test("policy cannot demote an error or suppress source intent", () => {
  const input = base([{ op: "policy", promote: [], demote: ["W-SEM-0001"], sourceSuppression: false }])
  assert.equal(runMatrixCase(input, catalog).error, "matrixPolicyDemotesError")
  const suppressed = base([{ op: "policy", promote: [], demote: [], sourceSuppression: true }])
  assert.equal(runMatrixCase(suppressed, catalog).error, "matrixSourceSuppression")
})

test("promotion requires a validated local warning fixture", () => {
  const input = base([
    { op: "policy", promote: ["W-SDM-9001"], demote: [], sourceSuppression: false },
    diagnostic({ code: "W-SDM-9001", phase: "semantic.effect", severity: "warning", facts: { actual: "fixture" } }),
  ])
  input.catalogFixture = { codes: [{ code: "W-SDM-9001", state: "active", phase: "semantic.effect", defaultSeverity: "warning", meaning: "fixture", requiredFacts: { actual: "string" }, labelRoles: {}, fixes: {} }] }
  const result = runMatrixCase(input, catalog)
  assert.equal(result.status, "accepted")
  assert.equal(result.state.diagnostics[0].severity, "error")
  assert.deepEqual(result.state.policy.promotedWarnings, [{ code: "W-SDM-9001", severity: "error" }])
})

test("caller warning records are not accepted by policy", () => {
  const input = base([{ op: "policy", promote: [], demote: [], sourceSuppression: false, warnings: [] }])
  assert.equal(runMatrixCase(input, catalog).error, "matrixUnexpectedFields")
})

test("malformed namespace and invalid UTF-8 boundaries are rejected", () => {
  assert.equal(runMatrixCase(base([diagnostic({ code: "W-TYPE-CONTRACT-1" })]), catalog).error, "matrixDiagnosticNamespace")
  const invalid = base()
  invalid.events.find((event) => event.op === "focus").startByte = 4
  assert.equal(runMatrixCase(invalid, catalog).error, "matrixSpanSplitsCodePoint")
})

test("an intent event is not inferred from source trivia", () => {
  assert.equal(runMatrixCase(base([{ op: "intent", text: "alternate statement" }]), catalog).error, "matrixIntentGuessing")
})

test("source pair validation rejects wrong field, missing positive, and wrong rule", () => {
  const positive = { id: "S0-POS-test", kind: "positive", rule: "W-785" }
  const result = { resultType: "Ticket", category: "value", flow: { kind: "next" }, ownerDelta: [], effectSummary: { signature: [], control: [], operational: [] }, proofFacts: [], evaluationGraph: { nodes: [{ id: "n", operation: "read", subject: "x" }], edges: [] } }
  const negative = { id: "S0-NEG-test", kind: "negative", rule: "W-785", baseline: "S0-POS-test", failureField: "flow", failureEvidence: { field: "ownerDelta", baselineValueDigest: "sha256:wrong" }, expect: { semanticResult: result } }
  assert.throws(() => deriveSemanticRulePairs([{ ...positive, expect: { semanticResult: result } }, negative], ["W-785"]), /failureEvidence field/)
  assert.throws(() => deriveSemanticRulePairs([{ ...negative, failureEvidence: undefined }], ["W-785"]), /no positive/)
  assert.throws(() => deriveSemanticRulePairs([{ ...positive, rule: "W-999" }, { ...negative, rule: "W-999", failureEvidence: { field: "flow", baselineValueDigest: "sha256:wrong" } }], ["W-785"]), /invalid rule/)
})

test("causal ordering keeps children after their root", () => {
  const input = base([diagnostic({ id: "child", root: "root", primary: { source: "source", startByte: 0, endByte: 1 } }), diagnostic({ id: "root", primary: { source: "source", startByte: 0, endByte: 1 } })])
  const result = runMatrixCase(input, catalog)
  assert.equal(result.status, "accepted")
  assert.equal(result.state.diagnostics[0].instance, "D000001")
  assert.equal(result.state.diagnostics[1].instance, "D000002")
  assert.equal(result.state.diagnostics[1].root, "D000001")
})
