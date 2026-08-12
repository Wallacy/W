import { createHash } from "node:crypto"

export const semanticFields = [
  "resultType",
  "category",
  "flow",
  "ownerDelta",
  "effectSummary",
  "proofFacts",
  "evaluationGraph",
]

export const checkerContextFields = [
  "scope",
  "expected",
  "owners",
  "effects",
  "controls",
  "facts",
  "constMode",
]

export const interfaceSchema = {
  id: "w-ast-hir-s0",
  version: 1,
}

export const phases = [
  "source.lex",
  "source.parse",
  "source.format",
  "source.validate",
  "source.context",
  "source.entry",
  "source.resolution",
  "source.roots",
  "source.capability",
  "source.fetch",
  "source.provenance",
  "semantic.const",
  "semantic.type",
  "semantic.ownership",
  "semantic.effect",
  "semantic.flow",
  "semantic.capability",
  "interface",
  "link",
  "build",
  "package",
  "test",
]

const diagnosticEventKeys = ["op", "id", "code", "phase", "severity", "primary", "labels", "facts", "notes", "fixes", "root", "poison"]
const diagnosticFixtureKeys = ["code", "state", "phase", "defaultSeverity", "meaning", "requiredFacts", "labelRoles", "fixes"]

const phaseOrder = new Map(phases.map((phase, index) => [phase, index]))
const categories = new Set(["value", "shared-place", "exclusive-place", "type", "declaration"])
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
const severities = new Set(["error", "warning", "information"])
const applicabilities = new Set(["machine", "review", "placeholder"])
const secretKey = /(secret|password|token|credential|authorization|cookie|payload|rawsource|sourcetext|private)/i

export class SemanticDiagnosticMatrixError extends Error {
  constructor(code, detail = "") {
    super(detail ? `${code}: ${detail}` : code)
    this.name = "SemanticDiagnosticMatrixError"
    this.code = code
  }
}

function fail(code, detail) {
  throw new SemanticDiagnosticMatrixError(code, detail)
}

function isRecord(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value)
}

function exactKeys(value, keys, owner) {
  if (!isRecord(value)) fail("matrixRecordRequired", owner)
  const actual = Object.keys(value).sort()
  const expected = [...keys].sort()
  if (actual.length !== expected.length || actual.some((key, index) => key !== expected[index])) {
    fail("matrixUnexpectedFields", `${owner}: expected ${expected.join(",")}`)
  }
}

function byteCompare(left, right) {
  return Buffer.from(left).compare(Buffer.from(right))
}

function sortedStrings(values, owner) {
  if (!Array.isArray(values) || values.some((value) => typeof value !== "string" || value.length === 0)) {
    fail("matrixStringSetInvalid", owner)
  }
  const sorted = [...values].sort(byteCompare)
  if (new Set(values).size !== values.length || values.some((value, index) => value !== sorted[index])) {
    fail("matrixStringSetUnsorted", owner)
  }
}

function sortedObject(value) {
  if (Array.isArray(value)) return value.map(sortedObject)
  if (!isRecord(value)) return value
  return Object.fromEntries(Object.keys(value).sort(byteCompare).map((key) => [key, sortedObject(value[key])]))
}

function digest(text) {
  return `sha256:${createHash("sha256").update(text, "utf8").digest("hex")}`
}

function canonicalJson(value) {
  if (Array.isArray(value)) return `[${value.map(canonicalJson).join(",")}]`
  if (isRecord(value)) return `{${Object.keys(value).sort(byteCompare).map((key) => `${JSON.stringify(key)}:${canonicalJson(value[key])}`).join(",")}}`
  return JSON.stringify(value)
}

export function semanticValueDigest(value) {
  return digest(canonicalJson(value))
}

function sourceSpan(source, span, owner) {
  if (!isRecord(span) || !Number.isSafeInteger(span.startByte) || !Number.isSafeInteger(span.endByte)) {
    fail("matrixSpanInvalid", owner)
  }
  const sourceBytes = Buffer.byteLength(source, "utf8")
  if (span.startByte < 0 || span.endByte < span.startByte || span.endByte > sourceBytes) {
    fail("matrixSpanOutsideSource", owner)
  }
  const bytes = Buffer.from(source, "utf8")
  const boundary = (offset) => offset === 0 || offset === bytes.length || (bytes[offset] & 0xc0) !== 0x80
  if (!boundary(span.startByte) || !boundary(span.endByte)) fail("matrixSpanSplitsCodePoint", owner)
  return { startByte: span.startByte, endByte: span.endByte }
}

function validateFacts(value, owner) {
  if (!isRecord(value)) fail("matrixFactsInvalid", owner)
  const visit = (entry, path) => {
    if (typeof entry === "string" && secretKey.test(path)) fail("matrixSecretFact", path)
    if (Array.isArray(entry)) {
      for (const [index, item] of entry.entries()) visit(item, `${path}[${index}]`)
      return
    }
    if (isRecord(entry)) {
      for (const [key, item] of Object.entries(entry)) {
        if (secretKey.test(key)) fail("matrixSecretFact", `${path}.${key}`)
        visit(item, `${path}.${key}`)
      }
    }
  }
  visit(value, owner)
}

function factMatches(value, type, owner) {
  if (type === "string" && typeof value === "string") return
  if (type === "integer" && Number.isInteger(value)) return
  if (type === "boolean" && typeof value === "boolean") return
  if (type === "string[]" && Array.isArray(value) && value.every((item) => typeof item === "string")) return
  if (type === "string-set" && Array.isArray(value)) {
    if (value.every((item) => typeof item === "string")) sortedStrings(value, owner)
    else fail("matrixRequiredFactInvalid", owner)
    return
  }
  fail("matrixRequiredFactInvalid", owner)
}

function validateSemanticResult(result, owner = "semanticResult") {
  exactKeys(result, semanticFields, owner)
  if (result.resultType !== null && (typeof result.resultType !== "string" || result.resultType.length === 0)) {
    fail("matrixResultTypeInvalid", owner)
  }
  if (!categories.has(result.category)) fail("matrixCategoryInvalid", owner)
  if ((result.resultType === null) !== (result.category === "declaration")) {
    fail("matrixDeclarationTypeMismatch", owner)
  }
  exactKeys(result.flow, result.flow.target === undefined ? ["kind"] : ["kind", "target"], `${owner}.flow`)
  if (!flowKinds.has(result.flow.kind)) fail("matrixFlowInvalid", owner)
  const needsTarget = ["return", "throw", "break", "continue", "commit", "cancel", "panic"].includes(result.flow.kind)
  if (needsTarget !== (typeof result.flow.target === "string" && result.flow.target.length > 0)) {
    fail("matrixFlowTargetInvalid", owner)
  }
  if ((result.flow.kind === "next") !== (result.resultType !== "Never")) {
    fail("matrixFlowResultMismatch", owner)
  }
  if (!Array.isArray(result.ownerDelta)) fail("matrixOwnerDeltaInvalid", owner)
  for (const [index, delta] of result.ownerDelta.entries()) {
    if (!isRecord(delta) || !ownerOperations.has(delta.operation) || typeof delta.place !== "string" || delta.place.length === 0) {
      fail("matrixOwnerDeltaInvalid", `${owner}[${index}]`)
    }
    for (const [key, value] of Object.entries(delta)) {
      if (!["operation", "place", "path", "type"].includes(key) || typeof value !== "string") {
        fail("matrixOwnerDeltaInvalid", `${owner}[${index}].${key}`)
      }
    }
  }
  exactKeys(result.effectSummary, ["signature", "control", "operational"], `${owner}.effectSummary`)
  for (const key of ["signature", "control", "operational"]) sortedStrings(result.effectSummary[key], `${owner}.effectSummary.${key}`)
  if (!Array.isArray(result.proofFacts)) fail("matrixProofFactsInvalid", owner)
  for (const [index, fact] of result.proofFacts.entries()) {
    exactKeys(fact, fact.value === undefined ? ["kind", "subject"] : ["kind", "subject", "value"], `${owner}.proofFacts[${index}]`)
    if (typeof fact.kind !== "string" || !fact.kind || typeof fact.subject !== "string" || !fact.subject) {
      fail("matrixProofFactsInvalid", `${owner}[${index}]`)
    }
    if (fact.value !== undefined && typeof fact.value !== "string") fail("matrixProofFactsInvalid", `${owner}[${index}]`)
  }
  exactKeys(result.evaluationGraph, ["nodes", "edges"], `${owner}.evaluationGraph`)
  if (!Array.isArray(result.evaluationGraph.nodes) || result.evaluationGraph.nodes.length === 0) {
    fail("matrixGraphEmpty", owner)
  }
  if (!Array.isArray(result.evaluationGraph.edges)) fail("matrixGraphInvalid", owner)
  const nodeIds = new Set()
  for (const [index, node] of result.evaluationGraph.nodes.entries()) {
    exactKeys(node, ["id", "operation", "subject"], `${owner}.evaluationGraph.nodes[${index}]`)
    if (typeof node.id !== "string" || !node.id || nodeIds.has(node.id) || typeof node.operation !== "string" || !node.operation || typeof node.subject !== "string" || !node.subject) {
      fail("matrixGraphNodeInvalid", `${owner}.evaluationGraph.nodes[${index}]`)
    }
    nodeIds.add(node.id)
  }
  for (const [index, edge] of result.evaluationGraph.edges.entries()) {
    exactKeys(edge, ["from", "to", "kind"], `${owner}.evaluationGraph.edges[${index}]`)
    if (!nodeIds.has(edge.from) || !nodeIds.has(edge.to) || typeof edge.kind !== "string" || !edge.kind) {
      fail("matrixGraphDanglingEdge", `${owner}.evaluationGraph.edges[${index}]`)
    }
  }
}

function validateContext(context) {
  exactKeys(context, checkerContextFields, "CheckerContext")
  for (const field of checkerContextFields) {
    if (context[field] === undefined) fail("matrixContextFieldMissing", field)
  }
  if (typeof context.scope !== "string" || !context.scope) fail("matrixContextFieldInvalid", "scope")
  if (context.expected !== null && !isRecord(context.expected)) fail("matrixContextFieldInvalid", "expected")
  for (const field of ["owners", "effects", "controls", "facts"]) {
    if (!isRecord(context[field])) fail("matrixContextFieldInvalid", field)
  }
  if (!new Set(["runtime", "required", "allowed"]).has(context.constMode)) fail("matrixContextFieldInvalid", "constMode")
  validateFacts(context.facts, "CheckerContext.facts")
  return context
}

function validateInterface(event) {
  exactKeys(event, ["op", "schemaId", "version", "domains", "backendSchemas"], "AST→HIR interface")
  if (event.op !== "interface" || event.schemaId !== interfaceSchema.id || event.version !== interfaceSchema.version) {
    fail("matrixInterfaceSchemaMismatch")
  }
  if (!Array.isArray(event.domains) || !Array.isArray(event.backendSchemas) || event.backendSchemas.length !== 0) {
    fail("matrixBackendSchemaPresent")
  }
  for (const [index, domain] of event.domains.entries()) {
    if ("resultType" in domain || "category" in domain || "flow" in domain || "backend" in domain) {
      fail("matrixDomainSchemaMutation", domain.name ?? index)
    }
    exactKeys(domain, ["name", "facts"], `interface.domains[${index}]`)
    if (typeof domain.name !== "string" || !domain.name || !isRecord(domain.facts)) fail("matrixDomainFactsInvalid", domain.name)
    validateFacts(domain.facts, `interface.domains[${index}].facts`)
  }
  return { schemaId: event.schemaId, version: event.version, domains: event.domains.map((domain) => domain.name) }
}

function ownerState(value) {
  return value === "available" || value === "moved" || value === "borrowed" || value === "unknown" ? value : null
}

function validateLoop(event) {
  exactKeys(event, ["op", "id", "entry", "continues", "backEdges", "breaks", "widening"], "loop")
  if (event.op !== "loop" || typeof event.id !== "string" || !event.id) fail("matrixLoopInvalid")
  for (const field of ["entry", "continues", "backEdges", "breaks"]) {
    if (!Array.isArray(event[field])) fail("matrixLoopInvalid", field)
  }
  if (!isRecord(event.widening) || event.widening.mode !== "monotone" || event.widening.acceptsMove === true) {
    fail("matrixLoopUnsafeWidening")
  }
  const initial = new Map()
  for (const item of event.entry) {
    if (!isRecord(item) || typeof item.place !== "string" || !ownerState(item.state)) fail("matrixLoopEntryInvalid")
    if (initial.has(item.place)) fail("matrixLoopEntryInvalid", item.place)
    initial.set(item.place, item.state)
  }
  const transfer = (items, state, kind) => {
    const next = new Map(state)
    for (const item of items) {
      if (!isRecord(item) || typeof item.place !== "string" || !ownerState(item.state)) fail("matrixLoopTransferInvalid", kind)
      const previous = next.get(item.place)
      if (item.state === "moved" && previous === "moved") fail("matrixLoopUnsafeMove", item.place)
      if (item.state === "moved" && previous === "unknown") fail("matrixLoopUnsafeMove", item.place)
      next.set(item.place, item.state)
    }
    return next
  }
  let state = transfer(event.continues, initial, "continue")
  for (const edge of event.backEdges) {
    if (!isRecord(edge) || edge.target !== event.id || !Array.isArray(edge.state)) fail("matrixLoopBackEdgeInvalid")
    const edgeState = transfer(edge.state, state, "back-edge")
    for (const [place, value] of edgeState) {
      if (value === "moved" && initial.get(place) === "available") fail("matrixLoopUnsafeBackEdge", place)
    }
    state = edgeState
  }
  const breaks = event.breaks.map((items) => transfer(items, state, "break"))
  return {
    id: event.id,
    fixedPoint: true,
    entryPlaces: [...initial.keys()],
    continueCount: event.continues.length,
    backEdgeCount: event.backEdges.length,
    breakCount: breaks.length,
  }
}

function catalogEntry(catalog, code) {
  const source = catalog.codes.find((entry) => entry.code === code)
  if (!source) return null
  if (source.profile !== undefined) {
    const profile = catalog.profiles[source.profile]
    if (!profile) fail("matrixProfileMissing", source.profile)
    const allowed = new Set(["code", "state", "profile", "meaning"])
    if (Object.keys(source).some((key) => !allowed.has(key))) fail("matrixProfileOverride", code)
    return { ...profile, ...source }
  }
  return source
}

function validateCatalogFixture(fixture) {
  if (!isRecord(fixture) || !Array.isArray(fixture.codes) || fixture.codes.length !== 1) fail("matrixCatalogFixtureInvalid")
  const entry = fixture.codes[0]
  exactKeys(entry, diagnosticFixtureKeys, "catalogFixture.code")
  if (!/^W-[A-Z]+-[0-9]{4}$/.test(entry.code) || entry.state !== "active" || !phases.includes(entry.phase) || !severities.has(entry.defaultSeverity)) {
    fail("matrixCatalogFixtureInvalid", entry.code)
  }
  if (typeof entry.meaning !== "string" || !entry.meaning || !isRecord(entry.requiredFacts) || !isRecord(entry.labelRoles) || !isRecord(entry.fixes)) {
    fail("matrixCatalogFixtureInvalid", entry.code)
  }
  for (const type of Object.values(entry.requiredFacts)) {
    if (!["string", "integer", "boolean", "string[]", "string-set"].includes(type)) fail("matrixCatalogFixtureInvalid", entry.code)
  }
  for (const limits of Object.values(entry.labelRoles)) {
    if (!isRecord(limits) || !Number.isInteger(limits.minimum) || limits.minimum < 0 || (limits.maximum !== null && (!Number.isInteger(limits.maximum) || limits.maximum < limits.minimum))) fail("matrixCatalogFixtureInvalid", entry.code)
  }
  for (const applicability of Object.values(entry.fixes)) if (!applicabilities.has(applicability)) fail("matrixCatalogFixtureInvalid", entry.code)
  return entry
}

export function mergeDiagnosticCatalog(catalog, fixture = null) {
  if (!isRecord(catalog) || !Array.isArray(catalog.codes) || !isRecord(catalog.profiles)) fail("matrixCatalogMissing")
  const expanded = catalog.codes.map((entry) => {
    if (entry.profile === undefined) return entry
    const profile = catalog.profiles[entry.profile]
    if (!profile) fail("matrixProfileMissing", entry.profile)
    const allowed = new Set(["code", "state", "profile", "meaning"])
    if (Object.keys(entry).some((key) => !allowed.has(key))) fail("matrixProfileOverride", entry.code)
    return { phase: profile.phase, defaultSeverity: profile.defaultSeverity, requiredFacts: profile.requiredFacts, labelRoles: profile.labelRoles, fixes: profile.fixes, code: entry.code, state: entry.state, meaning: entry.meaning }
  })
  const phaseSet = new Set(expanded.map((entry) => entry.phase))
  if (phaseSet.size !== phases.length || phases.some((phase) => !phaseSet.has(phase))) fail("matrixCatalogPhaseInventory")
  if (fixture === null) return { ...catalog, codes: expanded }
  const fixtureEntry = validateCatalogFixture(fixture)
  if (expanded.some((entry) => entry.code === fixtureEntry.code)) fail("matrixCatalogFixtureCollision", fixtureEntry.code)
  return { ...catalog, codes: [...expanded, fixtureEntry] }
}

function checkNoSecrets(value, owner) {
  validateFacts(value, owner)
  if (typeof value === "string" && secretKey.test(value)) fail("matrixSecretFact", owner)
}

function diagnosticRecord(event, sourceById, catalog, index, policy) {
  if (!isRecord(event)) fail("matrixDiagnosticInvalid")
  for (const key of Object.keys(event)) if (!diagnosticEventKeys.includes(key)) fail("matrixUnexpectedFields", `diagnostic.${key}`)
  if (typeof event.id !== "string" || !event.id) fail("matrixDiagnosticInvalid", "id")
  exactKeys(event.primary, ["source", "startByte", "endByte"], `${event.code ?? "diagnostic"}.primary`)
  if (event.root !== null && typeof event.root !== "string") fail("matrixDiagnosticInvalid", "root")
  if (event.poison !== undefined && typeof event.poison !== "boolean") fail("matrixDiagnosticInvalid", "poison")
  if (!isRecord(event.facts)) fail("matrixDiagnosticShape", event.code)
  if (!Array.isArray(event.labels) || !Array.isArray(event.notes) || !Array.isArray(event.fixes)) fail("matrixDiagnosticShape", event.code)
  if (!/^W-[A-Z]+-[0-9]{4}$/.test(event.code)) fail("matrixDiagnosticNamespace", event.code)
  const entry = catalogEntry(catalog, event.code)
  if (!entry || entry.state !== "active") fail("matrixDiagnosticCatalog", event.code)
  if (!phases.includes(event.phase) || event.phase !== entry.phase) fail("matrixDiagnosticPhase", event.code)
  if (!severities.has(event.severity) || event.severity !== entry.defaultSeverity) fail("matrixDiagnosticSeverity", event.code)
  const source = sourceById.get(event.primary.source)
  if (source === undefined) fail("matrixDiagnosticSource", event.primary.source)
  const primary = sourceSpan(source.text, event.primary, `${event.code}.primary`)
  checkNoSecrets(event.facts, `${event.code}.facts`)
  for (const [fact, type] of Object.entries(entry.requiredFacts ?? {})) factMatches(event.facts[fact], type, `${event.code}.facts.${fact}`)
  const labels = event.labels.map((label, labelIndex) => {
    exactKeys(label, ["role", "source", "startByte", "endByte"], `${event.code}.labels[${labelIndex}]`)
    if (typeof label.role !== "string" || !label.role) fail("matrixDiagnosticLabel", event.code)
    if (!(label.role in (entry.labelRoles ?? {}))) fail("matrixDiagnosticLabel", label.role)
    const labelSource = sourceById.get(label.source)
    if (labelSource === undefined) fail("matrixDiagnosticSource", label.source)
    return { role: label.role, span: sourceSpan(labelSource.text, label, `${event.code}.label`) }
  })
  const roleCounts = new Map(labels.map((label) => [label.role, (labels.filter((item) => item.role === label.role).length)]))
  for (const [role, limit] of Object.entries(entry.labelRoles ?? {})) {
    const count = roleCounts.get(role) ?? 0
    if (count < limit.minimum || (limit.maximum !== null && count > limit.maximum)) fail("matrixDiagnosticLabelCardinality", `${event.code}.${role}`)
  }
  const notes = event.notes.map((note, noteIndex) => {
    if (!isRecord(note) || typeof note.key !== "string" || !note.key || !isRecord(note.arguments ?? {})) fail("matrixDiagnosticNote", `${event.code}.${noteIndex}`)
    checkNoSecrets(note.arguments ?? {}, `${event.code}.notes[${noteIndex}]`)
    return { key: note.key, arguments: sortedObject(note.arguments ?? {}) }
  })
  const fixes = []
  for (const [fixIndex, fix] of event.fixes.entries()) {
    exactKeys(fix, ["id", "titleKey", "applicability", "proof", "edits"], `${event.code}.fixes[${fixIndex}]`)
    if (!applicabilities.has(fix.applicability) || entry.fixes?.[fix.id] !== fix.applicability) fail("matrixFixApplicability", fix.id)
    if (!isRecord(fix.proof) || typeof fix.proof.source !== "string" || typeof fix.proof.digest !== "string") fail("matrixFixProofMissing", fix.id)
    const proofSource = sourceById.get(fix.proof.source)
    if (proofSource === undefined || fix.proof.digest !== digest(proofSource.text)) fail("matrixFixStale", fix.id)
    if (!Array.isArray(fix.edits) || fix.edits.length === 0) fail("matrixFixEditsMissing", fix.id)
    const edits = fix.edits.map((edit) => {
      exactKeys(edit, ["source", "startByte", "endByte", "text"], `${event.code}.fixes[${fixIndex}].edit`)
      const editSource = sourceById.get(edit.source)
      if (editSource === undefined || typeof edit.text !== "string") fail("matrixFixEditInvalid", fix.id)
      const span = sourceSpan(editSource.text, edit, `${event.code}.fixes[${fixIndex}].edit`)
      return { source: edit.source, ...span, text: edit.text }
    })
    const bySource = new Map()
    for (const edit of edits) {
      const list = bySource.get(edit.source) ?? []
      list.push(edit)
      bySource.set(edit.source, list)
    }
    for (const [sourceId, list] of bySource) {
      list.sort((left, right) => left.startByte - right.startByte || left.endByte - right.endByte)
      for (let editIndex = 1; editIndex < list.length; editIndex += 1) {
        if (list[editIndex - 1].endByte > list[editIndex].startByte) fail("matrixFixOverlap", `${fix.id}:${sourceId}`)
      }
    }
    fixes.push({ id: fix.id, titleKey: fix.titleKey, applicability: fix.applicability, preconditions: [{ source: fix.proof.source, digest: fix.proof.digest }], edits })
  }
  let severity = event.severity
  if (policy?.promote?.includes(event.code)) {
    if (event.severity !== "warning") fail("matrixPolicyPromotionInvalid", event.code)
    severity = "error"
  }
  if (policy?.demote?.includes(event.code) || (policy?.demote?.length ?? 0) > 0 && entry.defaultSeverity === "error") fail("matrixPolicyDemotesError", event.code)
  return {
    schemaVersion: 1,
    id: event.id ?? `diag-${index}`,
    code: event.code,
    phase: event.phase,
    severity,
    primary: { source: event.primary.source, ...primary },
    labels,
    facts: sortedObject(event.facts),
    notes,
    fixes,
    root: event.root ?? null,
    poison: event.poison === true,
    sourceSeverity: event.severity,
    sourceOrdinal: sourceById.get(event.primary.source).ordinal,
    diagnosticOrdinal: index,
  }
}

function validateDiagnosticCausality(records) {
  const byId = new Map()
  for (const record of records) {
    if (byId.has(record.id)) fail("matrixDiagnosticDuplicateId", record.id)
    byId.set(record.id, record)
  }
  const poisoned = new Set()
  for (const record of records) {
    if (record.root !== null) {
      const root = byId.get(record.root)
      if (!root) fail("matrixDiagnosticDanglingRoot", record.id)
      if (root.root !== null) fail("matrixDiagnosticRootNotRoot", record.id)
      if (root.poison) poisoned.add(record.id)
    }
    if (record.poison) poisoned.add(record.id)
  }
  for (const record of records) if (record.root !== null && poisoned.has(record.id) && record.poison !== false) fail("matrixPoisonCascade", record.id)
}

function sortDiagnostics(records) {
  const sorted = [...records].sort((left, right) =>
    left.sourceOrdinal - right.sourceOrdinal ||
    left.primary.startByte - right.primary.startByte ||
    (phaseOrder.get(left.phase) ?? Number.MAX_SAFE_INTEGER) - (phaseOrder.get(right.phase) ?? Number.MAX_SAFE_INTEGER) ||
    byteCompare(left.code, right.code) ||
    byteCompare(left.id, right.id),
  )
  const byId = new Map(sorted.map((record) => [record.id, record]))
  const depth = (record) => {
    let current = record
    let value = 0
    const seen = new Set()
    while (current.root !== null) {
      if (seen.has(current.id)) fail("matrixDiagnosticRootCycle", current.id)
      seen.add(current.id)
      current = byId.get(current.root)
      if (!current) fail("matrixDiagnosticDanglingRoot", record.id)
      value += 1
    }
    return value
  }
  return sorted.sort((left, right) => {
    const leftRoot = left.root === null ? left : byId.get(left.root)
    const rightRoot = right.root === null ? right : byId.get(right.root)
    return (leftRoot?.sourceOrdinal ?? 0) - (rightRoot?.sourceOrdinal ?? 0) ||
      (leftRoot?.primary.startByte ?? 0) - (rightRoot?.primary.startByte ?? 0) ||
      depth(left) - depth(right) ||
      left.primary.startByte - right.primary.startByte ||
      (phaseOrder.get(left.phase) ?? Number.MAX_SAFE_INTEGER) - (phaseOrder.get(right.phase) ?? Number.MAX_SAFE_INTEGER) ||
      byteCompare(left.code, right.code) ||
      byteCompare(left.id, right.id)
  })
}

function makeTruncation(limit, emitted, sourceId) {
  return {
    schemaVersion: 1,
    id: "diag-limit",
    code: "W-DIAGNOSTIC-0001",
    phase: "build",
    severity: "error",
    primary: { source: sourceId, startByte: 0, endByte: 0 },
    labels: [],
    facts: { emitted, incomplete: true, limit },
    notes: [],
    fixes: [],
    root: null,
    poison: false,
    sourceOrdinal: 0,
    diagnosticOrdinal: Number.MAX_SAFE_INTEGER,
    sourceSeverity: "error",
  }
}

function canonicalDiagnostic(record, instanceById) {
  return {
    schemaVersion: 1,
    instance: instanceById.get(record.id),
    code: record.code,
    phase: record.phase,
    severity: record.severity,
    primary: record.primary,
    labels: record.labels,
    facts: record.facts,
    notes: record.notes,
    fixes: record.fixes,
    root: record.root === null ? null : instanceById.get(record.root),
  }
}

function canonicalDiagnosticJsonl(records) {
  return records.length === 0 ? "" : `${records.map((record) => JSON.stringify(record)).join("\n")}\n`
}

function deriveOutcome(sourceById, focus, result, diagnostics) {
  const firstSource = [...sourceById.values()].sort((left, right) => left.ordinal - right.ordinal)[0]
  const diagnostic = diagnostics.find((record) => record.severity === "error") ?? null
  return {
    schemaVersion: 1,
    sourceDigest: digest(firstSource.text),
    focus: { source: focus.source, ...sourceSpan(sourceById.get(focus.source).text, focus, "outcome.focus") },
    status: diagnostic ? "rejected" : "accepted",
    ...(diagnostic ? { failure: { code: diagnostic.code, phase: diagnostic.phase, facts: diagnostic.facts } } : { semanticResult: result }),
  }
}

export function runSemanticDiagnosticMatrix(input, catalog) {
  if (!isRecord(input) || !Array.isArray(input.events)) fail("matrixInputInvalid")
  const inputKeys = Object.keys(input)
  if (inputKeys.some((key) => !["events", "catalogFixture", "catalogMutation"].includes(key))) fail("matrixUnexpectedFields", "input")
  let effectiveCatalog = catalog
  if (input.catalogMutation !== undefined) {
    exactKeys(input.catalogMutation, ["op", "code", "field", "value"], "catalogMutation")
    if (input.catalogMutation.op !== "profile-override" && input.catalogMutation.op !== "phase-mismatch" && input.catalogMutation.op !== "lifecycle") fail("matrixCatalogMutationInvalid")
    if (typeof input.catalogMutation.code !== "string" || typeof input.catalogMutation.field !== "string") fail("matrixCatalogMutationInvalid")
    const codes = catalog.codes.map((entry) => {
      if (entry.code !== input.catalogMutation.code) return entry
      if (input.catalogMutation.op === "profile-override") return { ...entry, [input.catalogMutation.field]: input.catalogMutation.value }
      if (input.catalogMutation.op === "phase-mismatch") return { ...entry, phase: input.catalogMutation.value }
      return { ...entry, state: input.catalogMutation.value }
    })
    effectiveCatalog = { ...catalog, codes }
  }
  effectiveCatalog = mergeDiagnosticCatalog(effectiveCatalog, input.catalogFixture ?? null)
  const sourceById = new Map()
  let context = null
  let interfaceRecord = null
  let resultNodes = []
  let loops = []
  let diagnostics = []
  let focus = null
  let policy = { promote: [], demote: [] }
  let diagnosticLimit = null
  for (const [ordinal, event] of input.events.entries()) {
    if (!isRecord(event) || typeof event.op !== "string") fail("matrixEventInvalid", ordinal)
    if (event.op === "source") {
      exactKeys(event, ["op", "id", "text"], "source")
      if (typeof event.id !== "string" || !event.id || typeof event.text !== "string") fail("matrixSourceInvalid")
      if (sourceById.has(event.id)) fail("matrixSourceDuplicate", event.id)
      sourceById.set(event.id, { id: event.id, text: event.text, ordinal: sourceById.size })
    } else if (event.op === "context") {
      if (context !== null) fail("matrixContextDuplicate")
      context = validateContext(event.context)
    } else if (event.op === "interface") {
      if (interfaceRecord !== null) fail("matrixInterfaceDuplicate")
      interfaceRecord = validateInterface(event)
    } else if (event.op === "node") {
      exactKeys(event, ["op", "id", "operation", "subject", "resultType", "category", "flow", "ownerDelta", "effectSummary", "proofFacts", "edges"], "node")
      if (typeof event.id !== "string" || !event.id || typeof event.operation !== "string" || !event.operation || typeof event.subject !== "string" || !event.subject) fail("matrixNodeInvalid")
      resultNodes.push(event)
      if (event.focus) focus = event.focus
    } else if (event.op === "loop") {
      loops.push(validateLoop(event))
    } else if (event.op === "focus") {
      exactKeys(event, ["op", "source", "startByte", "endByte"], "focus")
      focus = event
    } else if (event.op === "policy") {
      exactKeys(event, ["op", "promote", "demote", "sourceSuppression"], "policy")
      if (!Array.isArray(event.promote) || !Array.isArray(event.demote)) fail("matrixPolicyInvalid")
      if (event.sourceSuppression === true) fail("matrixSourceSuppression")
      if (event.demote.length > 0) fail("matrixPolicyDemotesError")
      policy = { promote: event.promote, demote: event.demote }
    } else if (event.op === "limit") {
      exactKeys(event, ["op", "maximum"], "limit")
      if (!Number.isSafeInteger(event.maximum) || event.maximum <= 0) fail("matrixLimitInvalid")
      diagnosticLimit = event.maximum
    } else if (event.op === "diagnostic") {
      diagnostics.push(event)
    } else if (event.op === "truncation") {
      exactKeys(event, ["op", "mode"], "truncation")
      if (event.mode !== "sentinel") fail("matrixSilentTruncation")
    } else if (event.op === "intent") {
      fail("matrixIntentGuessing")
    } else {
      fail("matrixUnknownEvent", event.op)
    }
  }
  if (sourceById.size === 0) fail("matrixSourceMissing")
  for (const [ordinal, sourceId] of [...sourceById.keys()].sort(byteCompare).entries()) sourceById.get(sourceId).ordinal = ordinal
  if (context === null) fail("matrixContextMissing")
  if (interfaceRecord === null) fail("matrixInterfaceMissing")
  if (resultNodes.length === 0) fail("matrixNodeMissing")
  const nodeIds = new Set()
  const graphNodes = []
  const graphEdges = []
  let resultType = null
  let category = "declaration"
  let flow = { kind: "next" }
  const ownerDelta = []
  const effects = { signature: new Set(), control: new Set(), operational: new Set() }
  const proofFacts = []
  for (const node of resultNodes) {
    if (nodeIds.has(node.id)) fail("matrixGraphNodeDuplicate", node.id)
    nodeIds.add(node.id)
    graphNodes.push({ id: node.id, operation: node.operation, subject: node.subject })
    resultType = node.resultType
    category = node.category
    flow = node.flow
    ownerDelta.push(...node.ownerDelta)
    for (const key of ["signature", "control", "operational"]) for (const effect of node.effectSummary[key]) effects[key].add(effect)
    proofFacts.push(...node.proofFacts)
    for (const edge of node.edges) graphEdges.push(edge)
  }
  const semanticResult = {
    resultType,
    category,
    flow,
    ownerDelta,
    effectSummary: Object.fromEntries(Object.entries(effects).map(([key, values]) => [key, [...values].sort(byteCompare)])),
    proofFacts,
    evaluationGraph: { nodes: graphNodes, edges: graphEdges },
  }
  validateSemanticResult(semanticResult)
  for (const edge of graphEdges) if (!nodeIds.has(edge.from) || !nodeIds.has(edge.to)) fail("matrixGraphDanglingEdge", `${edge.from}->${edge.to}`)
  if (!focus) focus = { source: [...sourceById.keys()][0], startByte: 0, endByte: 0 }
  const focusSource = sourceById.get(focus.source)
  if (!focusSource) fail("matrixFocusSourceMissing", focus.source)
  sourceSpan(focusSource.text, focus, "focus")
  if (diagnostics.length > 0 && !catalog) fail("matrixCatalogMissing")
  const records = sortDiagnostics(diagnostics.map((event, index) => diagnosticRecord(event, sourceById, effectiveCatalog, index, policy)))
  validateDiagnosticCausality(records)
  for (const code of policy.promote) {
    if (!records.some((record) => record.code === code && record.sourceSeverity === "warning")) fail("matrixPolicyPromotionMissing", code)
  }
  let visibleDiagnostics = records
  if (diagnosticLimit !== null && records.length > diagnosticLimit) {
    visibleDiagnostics = [...records.slice(0, diagnosticLimit), makeTruncation(diagnosticLimit, diagnosticLimit, [...sourceById.keys()][0])]
  }
  const instanceById = new Map(visibleDiagnostics.map((record, index) => [record.id, `D${String(index + 1).padStart(6, "0")}`]))
  const canonicalDiagnostics = visibleDiagnostics.map((record) => canonicalDiagnostic(record, instanceById))
  const outcome = deriveOutcome(sourceById, focus, semanticResult, visibleDiagnostics)
  const promotedWarnings = records.filter((record) => record.sourceSeverity === "warning" && policy.promote.includes(record.code)).map((record) => ({ code: record.code, severity: "error" }))
  return {
    status: "accepted",
    error: null,
    state: {
      context,
      contextCoverage: checkerContextFields,
      interface: interfaceRecord,
      loops,
      semanticResult,
      diagnostics: canonicalDiagnostics,
      diagnosticJsonl: canonicalDiagnosticJsonl(canonicalDiagnostics),
      policy: {
        promotedWarnings,
      },
      outcome,
      sourceDigests: Object.fromEntries([...sourceById.values()].map((source) => [source.id, digest(source.text)])),
      normalized: true,
    },
  }
}

export function runMatrixCase(input, catalog) {
  try {
    return runSemanticDiagnosticMatrix(input, catalog)
  } catch (error) {
    if (!(error instanceof SemanticDiagnosticMatrixError)) throw error
    return { status: "rejected", error: error.code, state: null }
  }
}

export function inspectSemanticFields(result) {
  validateSemanticResult(result)
  return semanticFields
}
