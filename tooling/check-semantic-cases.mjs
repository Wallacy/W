import { createHash } from "node:crypto"
import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { resolve } from "node:path"
import { ledgerIdSet } from "./design-ledger.mjs"

const root = resolve(import.meta.dir, "..")
const design = await Bun.file(resolve(root, "DESIGN.md")).text()
const corpus = await Bun.file(resolve(import.meta.dir, "semantic-cases.json")).json()
const catalog = await Bun.file(resolve(import.meta.dir, "diagnostic-catalog.json")).json()
const snapshotPath = resolve(import.meta.dir, "semantic-diagnostics.snapshot.jsonl")
const resultSnapshotPath = resolve(import.meta.dir, "semantic-results.snapshot.jsonl")
const writeSnapshot = process.argv.includes("--write")

const phases = [
  "source.lex",
  "source.parse",
  "source.lower",
  "source.format",
  "source.validate",
  "source.context",
  "source.entry",
  "source.resolution",
  "source.roots",
  "source.capability",
  "source.fetch",
  "source.provenance",
  "semantic.resolve",
  "semantic.const",
  "semantic.type",
  "semantic.ownership",
  "semantic.effect",
  "semantic.flow",
  "semantic.capability",
  "interface",
  "abi",
  "link",
  "build",
  "package",
  "test",
  "lint",
]
const severities = new Set(["error", "warning", "information"])
const applicabilities = new Set(["machine", "review", "placeholder"])
const semanticFields = [
  "resultType",
  "category",
  "flow",
  "ownerDelta",
  "effectSummary",
  "proofFacts",
  "evaluationGraph",
]
const categories = new Set(["value", "shared-place", "exclusive-place", "type", "declaration"])
const ownerOperations = new Set([
  "initialize",
  "borrow",
  "mutate",
  "move",
  "pin",
  "capture",
  "end-borrow",
  "drop",
])
const flowKinds = new Set([
  "next",
  "return",
  "throw",
  "break",
  "continue",
  "commit",
  "cancel",
  "panic",
  "unreachable",
])

function fail(message) {
  throw new Error(`semantic cases: ${message}`)
}

function sortedObject(value) {
  if (Array.isArray(value)) {
    return value.map(sortedObject)
  }
  if (value === null || typeof value !== "object") {
    return value
  }

  return Object.fromEntries(
    Object.keys(value)
      .sort((left, right) => Buffer.from(left).compare(Buffer.from(right)))
      .map((key) => [key, sortedObject(value[key])]),
  )
}

function resolveSelector(source, selector, owner) {
  if (!selector || typeof selector.text !== "string" || selector.text.length === 0) {
    fail(`${owner} has an invalid source selector`)
  }

  const matches = []
  let offset = 0
  while (offset <= source.length) {
    const found = source.indexOf(selector.text, offset)
    if (found < 0) {
      break
    }
    matches.push(found)
    offset = found + Math.max(selector.text.length, 1)
  }

  const occurrence = selector.occurrence ?? 0
  if (!Number.isInteger(occurrence) || occurrence < 0 || occurrence >= matches.length) {
    fail(`${owner} selector ${JSON.stringify(selector.text)} has no occurrence ${occurrence}`)
  }
  if (selector.occurrence === undefined && matches.length !== 1) {
    fail(`${owner} selector ${JSON.stringify(selector.text)} is not unique`)
  }

  const start = matches[occurrence]
  return {
    startByte: Buffer.byteLength(source.slice(0, start), "utf8"),
    endByte: Buffer.byteLength(source.slice(0, start + selector.text.length), "utf8"),
  }
}

function sourceSpan(sourceId, source, selector, owner) {
  return { source: sourceId, ...resolveSelector(source, selector, owner) }
}

function sourceDigest(source) {
  return `sha256:${createHash("sha256").update(source, "utf8").digest("hex")}`
}

function exactKeys(value, keys, owner) {
  if (!value || Array.isArray(value) || typeof value !== "object") {
    fail(`${owner} must be an object`)
  }
  const actual = Object.keys(value).sort()
  const expected = [...keys].sort()
  if (actual.length !== expected.length || actual.some((key, index) => key !== expected[index])) {
    fail(`${owner} fields must be ${expected.join(", ")}`)
  }
}

function orderedUniqueStrings(values, owner) {
  if (!Array.isArray(values) || values.some((value) => typeof value !== "string" || !value)) {
    fail(`${owner} must be an array of strings`)
  }
  const ordered = [...values].sort((left, right) => Buffer.from(left).compare(Buffer.from(right)))
  if (new Set(values).size !== values.length || values.some((value, index) => value !== ordered[index])) {
    fail(`${owner} must be unique and byte-sorted`)
  }
}

function validateSemanticResult(result, owner) {
  exactKeys(result, semanticFields, owner)
  if (result.resultType !== null && (typeof result.resultType !== "string" || !result.resultType)) {
    fail(`${owner}.resultType must be a type name or null`)
  }
  if (!categories.has(result.category)) {
    fail(`${owner}.category is invalid`)
  }
  if ((result.resultType === null) !== (result.category === "declaration")) {
    fail(`${owner}.resultType null is reserved for declarations`)
  }
  exactKeys(result.flow, result.flow.target === undefined ? ["kind"] : ["kind", "target"], `${owner}.flow`)
  if (!flowKinds.has(result.flow.kind)) {
    fail(`${owner}.flow.kind is invalid`)
  }
  const needsTarget = ["return", "throw", "break", "continue", "commit", "cancel", "panic"].includes(result.flow.kind)
  if (
    needsTarget
      ? typeof result.flow.target !== "string" || result.flow.target.length === 0
      : result.flow.target !== undefined
  ) {
    fail(`${owner}.flow has an invalid target`)
  }
  if ((result.flow.kind === "next") !== (result.resultType !== "Never")) {
    fail(`${owner}.resultType and flow continuation disagree`)
  }

  if (!Array.isArray(result.ownerDelta)) {
    fail(`${owner}.ownerDelta must be an array`)
  }
  for (const [index, delta] of result.ownerDelta.entries()) {
    if (
      !delta ||
      Array.isArray(delta) ||
      typeof delta.operation !== "string" ||
      !ownerOperations.has(delta.operation) ||
      typeof delta.place !== "string" ||
      !delta.place
    ) {
      fail(`${owner}.ownerDelta[${index}] is invalid`)
    }
    for (const key of Object.keys(delta)) {
      if (!["operation", "place", "path", "type"].includes(key) || typeof delta[key] !== "string") {
        fail(`${owner}.ownerDelta[${index}] has invalid field ${key}`)
      }
    }
  }

  exactKeys(
    result.effectSummary,
    ["signature", "control", "operational"],
    `${owner}.effectSummary`,
  )
  for (const effectClass of ["signature", "control", "operational"]) {
    orderedUniqueStrings(result.effectSummary[effectClass], `${owner}.effectSummary.${effectClass}`)
  }

  if (!Array.isArray(result.proofFacts)) {
    fail(`${owner}.proofFacts must be an array`)
  }
  for (const [index, fact] of result.proofFacts.entries()) {
    exactKeys(
      fact,
      fact.value === undefined ? ["kind", "subject"] : ["kind", "subject", "value"],
      `${owner}.proofFacts[${index}]`,
    )
    if (
      typeof fact.kind !== "string" ||
      !fact.kind ||
      typeof fact.subject !== "string" ||
      !fact.subject ||
      (fact.value !== undefined && typeof fact.value !== "string")
    ) {
      fail(`${owner}.proofFacts[${index}] is invalid`)
    }
  }

  exactKeys(result.evaluationGraph, ["nodes", "edges"], `${owner}.evaluationGraph`)
  if (!Array.isArray(result.evaluationGraph.nodes) || !Array.isArray(result.evaluationGraph.edges)) {
    fail(`${owner}.evaluationGraph needs node and edge arrays`)
  }
  const nodeIds = new Set()
  for (const [index, node] of result.evaluationGraph.nodes.entries()) {
    exactKeys(node, ["id", "operation", "subject"], `${owner}.evaluationGraph.nodes[${index}]`)
    if (
      typeof node.id !== "string" ||
      !node.id ||
      nodeIds.has(node.id) ||
      typeof node.operation !== "string" ||
      !node.operation ||
      typeof node.subject !== "string" ||
      !node.subject
    ) {
      fail(`${owner}.evaluationGraph.nodes[${index}] is invalid`)
    }
    nodeIds.add(node.id)
  }
  if (nodeIds.size === 0) {
    fail(`${owner}.evaluationGraph must contain a node`)
  }
  for (const [index, edge] of result.evaluationGraph.edges.entries()) {
    exactKeys(edge, ["from", "to", "kind"], `${owner}.evaluationGraph.edges[${index}]`)
    if (!nodeIds.has(edge.from) || !nodeIds.has(edge.to) || typeof edge.kind !== "string" || !edge.kind) {
      fail(`${owner}.evaluationGraph.edges[${index}] is invalid`)
    }
  }
}

function buildFix(fix, sourceId, source, owner) {
  if (typeof fix.id !== "string" || typeof fix.titleKey !== "string") {
    fail(`${owner} fix has no stable identity`)
  }
  if (!applicabilities.has(fix.applicability)) {
    fail(`${owner} fix ${fix.id} has invalid applicability`)
  }
  if (typeof fix.text !== "string") {
    fail(`${owner} fix ${fix.id} has no edit text`)
  }

  const at = fix.at
  const selected = resolveSelector(source, at, `${owner} fix ${fix.id}`)
  if (at.edge !== "start" && at.edge !== "end") {
    fail(`${owner} fix ${fix.id} needs edge start or end`)
  }
  const position = at.edge === "start" ? selected.startByte : selected.endByte

  return {
    id: fix.id,
    titleKey: fix.titleKey,
    applicability: fix.applicability,
    preconditions: [{ source: sourceId, digest: sourceDigest(source) }],
    edits: [
      {
        source: sourceId,
        startByte: position,
        endByte: position,
        text: fix.text,
      },
    ],
  }
}

function factMatches(value, type) {
  if (type === "string") return typeof value === "string"
  if (type === "integer") return Number.isInteger(value)
  if (type === "boolean") return typeof value === "boolean"
  if (type === "string[]") {
    return Array.isArray(value) && value.every((item) => typeof item === "string")
  }
  if (type === "string-set") {
    if (!Array.isArray(value) || value.some((item) => typeof item !== "string")) {
      return false
    }
    const ordered = [...value].sort((left, right) => Buffer.from(left).compare(Buffer.from(right)))
    return new Set(value).size === value.length && value.every((item, index) => item === ordered[index])
  }
  fail(`catalog uses unsupported fact type ${JSON.stringify(type)}`)
}

if (corpus.$schema !== "w-semantic-cases-1") {
  fail("unexpected schema")
}
if (corpus.status !== "design-oracle-input") {
  fail("status must not claim compiler output")
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  fail("cases must be a non-empty array")
}

if (catalog.$schema !== "w-diagnostic-catalog-1" || catalog.status !== "projection-seed") {
  fail("diagnostic catalog has an unexpected schema or status")
}
if (!Array.isArray(catalog.codes) || catalog.codes.length === 0) {
  fail("diagnostic catalog is empty")
}
if (!catalog.profiles || Array.isArray(catalog.profiles)) {
  fail("diagnostic catalog has no profiles object")
}

const referencedDiagnosticCodes = [
  ...new Set([...design.matchAll(/\b(W-[A-Z]+-[0-9]{4})\b/g)].map((match) => match[1])),
].sort((left, right) => Buffer.from(left).compare(Buffer.from(right)))
const catalogByCode = new Map()

for (const sourceEntry of catalog.codes) {
  let entry = sourceEntry
  if (sourceEntry.profile !== undefined) {
    const profile = catalog.profiles[sourceEntry.profile]
    if (!profile || typeof profile !== "object" || Array.isArray(profile)) {
      fail(`${sourceEntry.code} uses unknown profile ${JSON.stringify(sourceEntry.profile)}`)
    }
    const allowedKeys = new Set(["code", "state", "profile", "meaning"])
    if (Object.keys(sourceEntry).some((key) => !allowedKeys.has(key))) {
      fail(`${sourceEntry.code} overrides its diagnostic profile`)
    }
    entry = { ...profile, ...sourceEntry }
  }
  const familyWildcard = entry.code.replace(/[0-9]{4}$/, "*")
  const codeIsDocumented = design.includes(entry.code) || design.includes(familyWildcard)
  if (!/^W-[A-Z]+-[0-9]{4}$/.test(entry.code) || !codeIsDocumented) {
    fail(`catalog contains unknown code ${JSON.stringify(entry.code)}`)
  }
  if (catalogByCode.has(entry.code)) {
    fail(`catalog contains duplicate code ${entry.code}`)
  }
  if (!["active", "reserved", "retired"].includes(entry.state)) {
    fail(`${entry.code} has invalid lifecycle state`)
  }
  if (!phases.includes(entry.phase) || !severities.has(entry.defaultSeverity)) {
    fail(`${entry.code} has invalid phase or severity`)
  }
  if (typeof entry.meaning !== "string" || entry.meaning.length === 0) {
    fail(`${entry.code} has no meaning`)
  }
  for (const field of ["requiredFacts", "labelRoles", "fixes"]) {
    if (!entry[field] || Array.isArray(entry[field])) {
      fail(`${entry.code} has no ${field} object`)
    }
  }
  for (const type of Object.values(entry.requiredFacts)) {
    factMatches(undefined, type)
  }
  for (const [role, limits] of Object.entries(entry.labelRoles)) {
    if (
      !role ||
      !Number.isInteger(limits.minimum) ||
      limits.minimum < 0 ||
      (limits.maximum !== null &&
        (!Number.isInteger(limits.maximum) || limits.maximum < limits.minimum))
    ) {
      fail(`${entry.code} has invalid limits for label role ${role}`)
    }
  }
  for (const applicability of Object.values(entry.fixes)) {
    if (!applicabilities.has(applicability)) {
      fail(`${entry.code} has invalid fix applicability`)
    }
  }
  catalogByCode.set(entry.code, entry)
}

const diagnosticTablePatterns = [
  /\| `W-CONTRACT-0001`[\s\S]*?\| `W-CONTRACT-0005`[^\n]*\|/,
  /\| `W-PATTERN-0001`[\s\S]*?\| `W-MATCH-0003`[^\n]*\|/,
  /\| `W-PARSE-0020`[\s\S]*?\| `W-OWNERSHIP-0016`[^\n]*\|/,
  /#### 3\.6\.8 Diagnostics[\s\S]*?\| `W-CONST-0001`[\s\S]*?\| `W-CONST-0007`[^\n]*\|/,
  /\| `W-TYPE-0121`[^\n]*\|/,
  /#### 8\.7\.10 Diagnostics[\s\S]*?\| `W-GENERIC-0001`[\s\S]*?\| `W-GENERIC-0005`[^\n]*\|/,
  /\| `W-TYPE-0122`[^\n]*\|/,
  /\| `W-SEM-0001`[^\n]*\|\n(?:\|[^\n]*\|\n)*\| `W-CAPABILITY-0001`[^\n]*\|/,
]
const diagnosticTables = diagnosticTablePatterns.map((pattern) => design.match(pattern)?.[0])
if (diagnosticTables.some((table) => table === undefined)) {
  fail("a semantic diagnostic table is missing")
}

const requiredDiagnostics = [
  ...new Set(
    diagnosticTables.flatMap((table) =>
      [...table.matchAll(/`(W-[A-Z]+-[0-9]{4})`/g)].map((match) => match[1]),
    ),
  ),
].filter((code) => !code.startsWith("W-PARSE-"))

const ids = new Set()
const coveredDiagnostics = new Set()
const codePhases = new Map()
const diagnosticCandidates = []
const resultCandidates = []
const casesById = new Map(corpus.cases.map((testCase) => [testCase.id, testCase]))
const usedBaselines = new Set()

for (const [sourceOrdinal, testCase] of corpus.cases.entries()) {
  if (!/^S0-(POS|NEG)-[a-z0-9-]+$/.test(testCase.id)) {
    fail(`invalid id ${JSON.stringify(testCase.id)}`)
  }
  if (ids.has(testCase.id)) {
    fail(`duplicate id ${testCase.id}`)
  }
  ids.add(testCase.id)

  if (testCase.kind !== "positive" && testCase.kind !== "negative") {
    fail(`${testCase.id} has invalid kind`)
  }
  if (!/^W-[0-9]{3,}$/.test(testCase.rule) || !ledgerIdSet.has(testCase.rule)) {
    fail(`${testCase.id} references an unknown decision`)
  }
  if (!Array.isArray(testCase.source) || testCase.source.length === 0) {
    fail(`${testCase.id} has no source`)
  }
  if (testCase.source.some((line) => typeof line !== "string" || line.includes("\r"))) {
    fail(`${testCase.id} source must be an array of LF-safe strings`)
  }

  const source = `${testCase.source.join("\n")}\n`
  const sourceId = `semantic/${testCase.id}.w`
  const diagnostics = testCase.expect?.diagnostics
  if (!Array.isArray(diagnostics)) {
    fail(`${testCase.id} has no diagnostics array`)
  }

  if (testCase.kind === "positive") {
    if (diagnostics.length !== 0) {
      fail(`${testCase.id} is positive but expects diagnostics`)
    }
    if (testCase.baseline !== undefined || testCase.failureField !== undefined) {
      fail(`${testCase.id} is positive but declares a failure baseline`)
    }
    validateSemanticResult(testCase.expect.semanticResult, `${testCase.id}.expect.semanticResult`)
    const focus = sourceSpan(sourceId, source, testCase.expect.focus, `${testCase.id} focus`)
    resultCandidates.push({
      schemaVersion: 1,
      case: testCase.id,
      sourceDigest: sourceDigest(source),
      focus,
      status: "accepted",
      semanticResult: testCase.expect.semanticResult,
    })
  } else if (diagnostics.length !== 1) {
    fail(`${testCase.id} must invert exactly one S0 rule`)
  } else {
    if (!semanticFields.includes(testCase.failureField)) {
      fail(`${testCase.id} has invalid failureField`)
    }
    const baseline = casesById.get(testCase.baseline)
    if (
      !baseline ||
      baseline.kind !== "positive" ||
      baseline.rule !== testCase.rule ||
      usedBaselines.has(testCase.baseline)
    ) {
      fail(`${testCase.id} needs a unique positive baseline for ${testCase.rule}`)
    }
    usedBaselines.add(testCase.baseline)
    resultCandidates.push({
      schemaVersion: 1,
      case: testCase.id,
      sourceDigest: sourceDigest(source),
      focus: sourceSpan(sourceId, source, diagnostics[0].primary, `${testCase.id} failure focus`),
      status: "rejected",
      baseline: testCase.baseline,
      failureField: testCase.failureField,
      diagnosticCodes: diagnostics.map((diagnostic) => diagnostic.code),
    })
  }

  for (const [diagnosticOrdinal, diagnostic] of diagnostics.entries()) {
    const owner = `${testCase.id} diagnostic ${diagnosticOrdinal}`
    if (!requiredDiagnostics.includes(diagnostic.code)) {
      fail(`${owner} uses ${diagnostic.code} outside S0`)
    }
    if (!phases.includes(diagnostic.phase) || !design.includes(diagnostic.phase)) {
      fail(`${owner} has an unknown phase`)
    }
    if (!severities.has(diagnostic.severity)) {
      fail(`${owner} has an invalid severity`)
    }
    if (!diagnostic.facts || Array.isArray(diagnostic.facts)) {
      fail(`${owner} has no facts object`)
    }
    for (const field of ["labels", "notes", "fixes"]) {
      if (!Array.isArray(diagnostic[field])) {
        fail(`${owner} has no ${field} array`)
      }
    }

    const previousPhase = codePhases.get(diagnostic.code)
    if (previousPhase && previousPhase !== diagnostic.phase) {
      fail(`${diagnostic.code} uses both ${previousPhase} and ${diagnostic.phase}`)
    }
    codePhases.set(diagnostic.code, diagnostic.phase)

    const catalogEntry = catalogByCode.get(diagnostic.code)
    if (!catalogEntry || catalogEntry.state !== "active") {
      fail(`${owner} has no active catalog entry`)
    }
    if (
      catalogEntry.phase !== diagnostic.phase ||
      catalogEntry.defaultSeverity !== diagnostic.severity
    ) {
      fail(`${owner} disagrees with its catalog phase or severity`)
    }
    for (const [fact, type] of Object.entries(catalogEntry.requiredFacts)) {
      if (!factMatches(diagnostic.facts[fact], type)) {
        fail(`${owner} has invalid required fact ${fact}`)
      }
    }

    const roleCounts = new Map()
    for (const label of diagnostic.labels) {
      if (!(label.role in catalogEntry.labelRoles)) {
        fail(`${owner} uses unknown label role ${label.role}`)
      }
      roleCounts.set(label.role, (roleCounts.get(label.role) ?? 0) + 1)
    }
    for (const [role, limits] of Object.entries(catalogEntry.labelRoles)) {
      const count = roleCounts.get(role) ?? 0
      if (count < limits.minimum || (limits.maximum !== null && count > limits.maximum)) {
        fail(`${owner} violates cardinality for label role ${role}`)
      }
    }
    for (const fix of diagnostic.fixes) {
      if (catalogEntry.fixes[fix.id] !== fix.applicability) {
        fail(`${owner} fix ${fix.id} disagrees with the catalog`)
      }
    }

    const primary = sourceSpan(sourceId, source, diagnostic.primary, `${owner} primary`)
    const labels = diagnostic.labels.map((label, index) => {
      if (typeof label.role !== "string" || label.role.length === 0) {
        fail(`${owner} label ${index} has no role`)
      }
      return {
        role: label.role,
        span: sourceSpan(sourceId, source, label.selector, `${owner} label ${index}`),
      }
    })
    const notes = diagnostic.notes.map((note, index) => {
      if (typeof note.key !== "string" || note.key.length === 0) {
        fail(`${owner} note ${index} has no key`)
      }
      const result = {
        key: note.key,
        arguments: sortedObject(note.arguments ?? {}),
      }
      if (note.selector) {
        result.span = sourceSpan(sourceId, source, note.selector, `${owner} note ${index}`)
      }
      return result
    })
    const fixes = diagnostic.fixes.map((fix) => buildFix(fix, sourceId, source, owner))

    diagnosticCandidates.push({
      sourceOrdinal,
      diagnosticOrdinal,
      phaseOrdinal: phases.indexOf(diagnostic.phase),
      record: {
        schemaVersion: 1,
        instance: "",
        code: diagnostic.code,
        phase: diagnostic.phase,
        severity: diagnostic.severity,
        primary,
        labels,
        facts: sortedObject(diagnostic.facts),
        notes,
        fixes,
        root: null,
      },
    })
    coveredDiagnostics.add(diagnostic.code)
  }
}

const unusedBaselines = corpus.cases
  .filter((testCase) => testCase.kind === "positive" && !usedBaselines.has(testCase.id))
  .map((testCase) => testCase.id)
if (unusedBaselines.length > 0) {
  fail(`positive cases without a paired inversion: ${unusedBaselines.join(", ")}`)
}

const missing = requiredDiagnostics.filter((code) => !coveredDiagnostics.has(code))
if (missing.length > 0) {
  fail(`missing negative cases for ${missing.join(", ")}`)
}

diagnosticCandidates.sort(
  (left, right) =>
    left.sourceOrdinal - right.sourceOrdinal ||
    left.record.primary.startByte - right.record.primary.startByte ||
    left.phaseOrdinal - right.phaseOrdinal ||
    Buffer.from(left.record.code).compare(Buffer.from(right.record.code)) ||
    left.diagnosticOrdinal - right.diagnosticOrdinal,
)

for (const [index, candidate] of diagnosticCandidates.entries()) {
  candidate.record.instance = `D${String(index + 1).padStart(6, "0")}`
}

const expectedSnapshot = `${diagnosticCandidates
  .map((candidate) => JSON.stringify(candidate.record))
  .join("\n")}\n`
const expectedResultSnapshot = `${resultCandidates.map((result) => JSON.stringify(result)).join("\n")}\n`

if (writeSnapshot) {
  await Bun.write(snapshotPath, expectedSnapshot)
  await Bun.write(resultSnapshotPath, expectedResultSnapshot)
} else if (!(await Bun.file(snapshotPath).exists())) {
  fail("diagnostic snapshot is missing; run with --write")
} else {
  const currentSnapshot = await Bun.file(snapshotPath).text()
  if (currentSnapshot !== expectedSnapshot) {
    fail("diagnostic snapshot is stale; review the change and run with --write")
  }
}

if (!writeSnapshot) {
  if (!(await Bun.file(resultSnapshotPath).exists())) {
    fail("semantic result snapshot is missing; run with --write")
  }
  const currentResultSnapshot = await Bun.file(resultSnapshotPath).text()
  if (currentResultSnapshot !== expectedResultSnapshot) {
    fail("semantic result snapshot is stale; review the change and run with --write")
  }
}

const grammar = resolve(import.meta.dir, "tree-sitter-w")
const treeSitter = resolve(grammar, "node_modules", "tree-sitter-cli", "tree-sitter.exe")
if (!(await Bun.file(treeSitter).exists())) {
  fail("Tree-sitter CLI is missing; install tooling/tree-sitter-w dependencies")
}

const temporary = await mkdtemp(resolve(tmpdir(), "w-semantic-cases-"))
try {
  const files = []
  for (const testCase of corpus.cases) {
    const path = resolve(temporary, `${testCase.id}.w`)
    await Bun.write(path, `${testCase.source.join("\n")}\n`)
    files.push(path)
  }

  const parsed = Bun.spawnSync({
    cmd: [treeSitter, "parse", "--grammar-path", grammar, "--quiet", "--stat", ...files],
    cwd: grammar,
    stdout: "pipe",
    stderr: "pipe",
  })

  if (parsed.exitCode !== 0) {
    const output = `${parsed.stdout.toString()}\n${parsed.stderr.toString()}`.trim()
    fail(`semantic source is not syntactically valid\n${output}`)
  }
} finally {
  await rm(temporary, { recursive: true, force: true })
}

console.log(
  `Semantic cases: ${corpus.cases.length} syntax-valid cases, ${resultCandidates.length} SemanticResult outcomes, ${diagnosticCandidates.length} D0 snapshots, ${requiredDiagnostics.length}/${requiredDiagnostics.length} semantic diagnostics covered, ${catalogByCode.size}/${referencedDiagnosticCodes.length} referenced codes cataloged.`,
)
