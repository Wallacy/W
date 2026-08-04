import { createHash } from "node:crypto"
import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const design = await Bun.file(resolve(root, "DESIGN.md")).text()
const corpus = await Bun.file(resolve(import.meta.dir, "semantic-cases.json")).json()
const catalog = await Bun.file(resolve(import.meta.dir, "diagnostic-catalog.json")).json()
const snapshotPath = resolve(import.meta.dir, "semantic-diagnostics.snapshot.jsonl")
const writeSnapshot = process.argv.includes("--write")

const phases = [
  "source.lex",
  "source.parse",
  "source.lower",
  "source.format",
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
  fail(`catalog uses unsupported fact type ${JSON.stringify(type)}`)
}

if (corpus.$schema !== "w-semantic-cases-0") {
  fail("unexpected schema")
}
if (corpus.status !== "design-oracle-input") {
  fail("status must not claim compiler output")
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  fail("cases must be a non-empty array")
}

if (catalog.$schema !== "w-diagnostic-catalog-0" || catalog.status !== "projection-seed") {
  fail("diagnostic catalog has an unexpected schema or status")
}
if (!Array.isArray(catalog.codes) || catalog.codes.length === 0) {
  fail("diagnostic catalog is empty")
}

const referencedDiagnosticCodes = [
  ...new Set([...design.matchAll(/\b(W-[A-Z]+-[0-9]{4})\b/g)].map((match) => match[1])),
].sort((left, right) => Buffer.from(left).compare(Buffer.from(right)))
const catalogByCode = new Map()

for (const entry of catalog.codes) {
  if (!/^W-[A-Z]+-[0-9]{4}$/.test(entry.code) || !design.includes(entry.code)) {
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

const diagnosticTable = design.match(
  /\| `W-SEM-0001`[\s\S]*?\| `W-CAPABILITY-0001`[^\n]*\|/,
)
if (!diagnosticTable) {
  fail("S0 diagnostic table is missing")
}

const requiredDiagnostics = [
  ...diagnosticTable[0].matchAll(/`(W-[A-Z]+-[0-9]{4})`/g),
].map((match) => match[1])

const ids = new Set()
const coveredDiagnostics = new Set()
const codePhases = new Map()
const diagnosticCandidates = []

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
  if (!/^W-[0-9]{3}$/.test(testCase.rule) || !design.includes(`| ${testCase.rule} |`)) {
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
    if (!testCase.expect.resultType || !testCase.expect.flow) {
      fail(`${testCase.id} lacks a normalized positive result`)
    }
  } else if (diagnostics.length !== 1) {
    fail(`${testCase.id} must invert exactly one S0 rule`)
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

if (writeSnapshot) {
  await Bun.write(snapshotPath, expectedSnapshot)
} else if (!(await Bun.file(snapshotPath).exists())) {
  fail("diagnostic snapshot is missing; run with --write")
} else {
  const currentSnapshot = await Bun.file(snapshotPath).text()
  if (currentSnapshot !== expectedSnapshot) {
    fail("diagnostic snapshot is stale; review the change and run with --write")
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
  `Semantic cases: ${corpus.cases.length} syntax-valid cases, ${diagnosticCandidates.length} D0 snapshots, ${requiredDiagnostics.length}/${requiredDiagnostics.length} S0 diagnostics covered, ${catalogByCode.size}/${referencedDiagnosticCodes.length} referenced codes cataloged.`,
)
