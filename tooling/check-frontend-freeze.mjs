import { createHash } from "node:crypto"
import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { resolve, relative } from "node:path"
import fs from "node:fs"
import { ledgerIdSet } from "./design-ledger.mjs"
import {
  EXPECTED_FRONTEND_FAMILIES,
  corpusGuardErrors,
  digestMatches,
  duplicateValues,
  isExpectedEcho,
  symbolOccurrenceCount,
} from "./frontend-freeze-guards.mjs"

const tooling = resolve(import.meta.dir)
const root = resolve(tooling, "..")
const cliArgs = process.argv.slice(2)
const preflightOnly = cliArgs.includes("--preflight-only")
const corpusOption = cliArgs.indexOf("--corpus")
if (corpusOption >= 0 && !cliArgs[corpusOption + 1]) {
  process.stderr.write("frontend freeze: --corpus requires a path\n")
  process.exit(2)
}
const corpusPath = corpusOption >= 0
  ? resolve(process.cwd(), cliArgs[corpusOption + 1])
  : resolve(tooling, "frontend-freeze-cases.json")
const corpus = await Bun.file(corpusPath).json()
const formatter = await Bun.file(resolve(tooling, "formatter-cases.json")).json()
const semantic = await Bun.file(resolve(tooling, "semantic-cases.json")).json()
const workflow = await Bun.file(resolve(tooling, "module-run-cases.json")).json()
const catalog = await Bun.file(resolve(tooling, "diagnostic-catalog.json")).json()
const snapshotPath = resolve(tooling, "frontend-freeze.snapshot.jsonl")
const writeSnapshot = cliArgs.includes("--write") && !preflightOnly

const expectedFamilies = EXPECTED_FRONTEND_FAMILIES
const errors = []
const formatterById = new Map((formatter.cases || []).map((item) => [item.id, item]))
const semanticById = new Map((semantic.cases || []).map((item) => [item.id, item]))
const workflowById = new Map()
for (const fixture of Object.values(workflow.fixtures || {})) {
  if (fixture.id) workflowById.set(fixture.id, fixture)
}
for (const item of workflow.cases || []) workflowById.set(item.id, item)

const temporary = preflightOnly ? undefined : await mkdtemp(resolve(tmpdir(), "w-frontend-freeze-"))
const treeSitter = resolve(tooling, "tree-sitter-w", "node_modules", "tree-sitter-cli", "tree-sitter.exe")
const grammar = resolve(tooling, "tree-sitter-w")

function fail(message) {
  errors.push("frontend freeze: " + message)
}

function digest(value) {
  return "sha256:" + createHash("sha256").update(value, "utf8").digest("hex")
}

function fileDigest(path) {
  return "sha256:" + createHash("sha256").update(fs.readFileSync(path)).digest("hex")
}

function canonical(value) {
  if (Array.isArray(value)) return "[" + value.map(canonical).join(",") + "]"
  if (value !== null && typeof value === "object") {
    return "{" + Object.keys(value).sort().map((key) => JSON.stringify(key) + ":" + canonical(value[key])).join(",") + "}"
  }
  return JSON.stringify(value)
}

function inputText(input, owner) {
  if (!input || !Array.isArray(input.lines) || input.lines.length === 0) {
    fail(owner + " has no formatter input lines")
    return ""
  }
  const newline = input.newline === "crlf" ? "\r\n" : "\n"
  let value = input.lines.join(newline)
  if (input.finalNewline !== false) value += newline
  if (input.bom === true) value = "\uFEFF" + value
  return value
}

function outputText(output, owner) {
  if (!Array.isArray(output) || output.length === 0) {
    fail(owner + " has no formatter output lines")
    return ""
  }
  return output.join("\n") + "\n"
}

function deriveRecoveryFacts(output) {
  const kinds = [...output.matchAll(/\((ERROR|MISSING)(?=\s|\))/gu)].map((match) => match[1])
  return { kinds: [...new Set(kinds)], count: kinds.length }
}

function validateDeclaredRecoveryFacts(facts, owner) {
  if (!facts || !Array.isArray(facts.kinds) || !Number.isInteger(facts.count) || facts.count < 1) {
    fail(owner + " must declare positive recovery facts in preflight")
    return
  }
  if (facts.kinds.some((kind) => kind !== "ERROR" && kind !== "MISSING")) {
    fail(owner + " declares an unknown recovery node kind")
  }
}

function validateCanonicalOutput(output, owner) {
  const text = outputText(output, owner)
  if (text.startsWith("\uFEFF") || text.includes("\r") || text.includes("\t")) {
    fail(owner + " canonical output must be UTF-8/LF/space source")
  }
  if (!text.endsWith("\n") || text.endsWith("\n\n")) {
    fail(owner + " canonical output must have exactly one final newline")
  }
  for (const [lineIndex, line] of output.entries()) {
    if (/[ \t]$/u.test(line)) fail(owner + " canonical line " + (lineIndex + 1) + " has trailing whitespace")
    if ([...line].length > 120) fail(owner + " canonical line " + (lineIndex + 1) + " exceeds 120 columns")
    const indent = /^ */u.exec(line)[0].length
    if (line.length > 0 && indent % 2 !== 0) fail(owner + " canonical line " + (lineIndex + 1) + " has noncanonical indentation")
  }
  return text
}

function sourcePath(path, owner) {
  if (typeof path !== "string" || path.trim() === "") {
    fail(owner + ".path must be a non-empty string")
    return undefined
  }
  const resolved = resolve(root, path)
  const rel = relative(root, resolved)
  if (!rel || rel.startsWith("..") || /^[A-Za-z]:/.test(rel)) {
    fail(owner + ".path must stay inside the repository")
    return undefined
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    fail(owner + ".path references a missing file")
    return undefined
  }
  return resolved
}

function catalogEntry(code, owner) {
  const entry = (catalog.codes || []).find((item) => item.code === code)
  if (!entry || entry.state !== "active") {
    fail(owner + " references inactive or missing diagnostic " + code)
    return undefined
  }
  const profile = entry.profile ? (catalog.profiles || {})[entry.profile] : undefined
  const phase = entry.phase || (profile && profile.phase)
  const severity = entry.defaultSeverity || (profile && profile.defaultSeverity)
  if (!phase || !severity) fail(owner + " diagnostic " + code + " has no phase/severity in the catalog")
  return { entry, profile, phase, severity }
}

function factMatches(value, type) {
  if (type === "string") return typeof value === "string"
  if (type === "string[]") return Array.isArray(value) && value.every((item) => typeof item === "string")
  if (type === "string-set") return Array.isArray(value) && new Set(value).size === value.length && value.every((item) => typeof item === "string")
  if (type === "boolean") return typeof value === "boolean"
  if (type === "integer" || type === "nonnegative-integer") return Number.isInteger(value) && (type !== "nonnegative-integer" || value >= 0)
  if (type === "object") return value !== null && typeof value === "object" && !Array.isArray(value)
  return value !== undefined
}

async function parseText(text, id, allowRecovery = false) {
  const path = resolve(temporary, id + ".w")
  await Bun.write(path, text)
  const result = Bun.spawnSync({
    cmd: [treeSitter, "parse", "--grammar-path", grammar, "--no-ranges", path],
    cwd: grammar,
    stdout: "pipe",
    stderr: "pipe",
  })
  const output = result.stdout.toString()
  const recovery = /\((?:ERROR|MISSING)(?:\s|\))/u.test(output)
  if (result.exitCode !== 0 && !allowRecovery) fail(id + " failed to parse: " + result.stderr.toString().trim())
  return { output, recovery, recoveryFacts: deriveRecoveryFacts(output), digest: digest(text) }
}

async function parseFile(path, id) {
  const source = await Bun.file(path).text()
  const result = Bun.spawnSync({
    cmd: [treeSitter, "parse", "--grammar-path", grammar, "--no-ranges", path],
    cwd: grammar,
    stdout: "pipe",
    stderr: "pipe",
  })
  const output = result.stdout.toString()
  const recovery = /\((?:ERROR|MISSING)(?:\s|\))/u.test(output)
  if (result.exitCode !== 0) fail(id + " failed to parse: " + result.stderr.toString().trim())
  return { output, recovery, recoveryFacts: deriveRecoveryFacts(output), digest: digest(source) }
}

function validateD0(diagnostic, owner) {
  if (!diagnostic || typeof diagnostic !== "object") {
    fail(owner + " must provide a D0 diagnostic record")
    return undefined
  }
  if (typeof diagnostic.code !== "string" || typeof diagnostic.phase !== "string") {
    fail(owner + " D0 must provide code and phase")
    return undefined
  }
  if (!diagnostic.facts || Array.isArray(diagnostic.facts) || typeof diagnostic.facts !== "object") {
    fail(owner + " D0 must provide facts")
  }
  const entry = catalogEntry(diagnostic.code, owner)
  if (entry && entry.phase !== diagnostic.phase) fail(owner + " D0 phase " + diagnostic.phase + " disagrees with catalog " + entry.phase)
  const requiredFacts = (entry && (entry.entry.requiredFacts || (entry.profile && entry.profile.requiredFacts))) || {}
  for (const [fact, type] of Object.entries(requiredFacts)) {
    if (!factMatches(diagnostic.facts && diagnostic.facts[fact], type)) fail(owner + " D0 fact " + fact + " does not match catalog type " + type)
  }
  return {
    code: diagnostic.code,
    phase: diagnostic.phase,
    severity: (entry && entry.severity) || "error",
    facts: diagnostic.facts || {},
  }
}

function validateSourceRef(entry, index, seenSourceRefs) {
  const owner = "families[" + index + "].sourceRef"
  const path = sourcePath(entry.sourceRef && entry.sourceRef.path, owner)
  if (!path) return undefined
  const symbol = entry.sourceRef && entry.sourceRef.symbol
  if (typeof symbol !== "string" || symbol.length === 0) {
    fail(owner + ".symbol must be a non-empty string")
    return undefined
  }
  const text = fs.readFileSync(path, "utf8")
  if (symbolOccurrenceCount(text, symbol) !== 1) fail(owner + ".symbol must occur exactly once")
  const actual = fileDigest(path)
  if (!digestMatches(actual, entry.sourceRef && entry.sourceRef.digest)) fail(owner + ".digest is stale; expected " + actual)
  const key = relative(root, path).replaceAll("\\", "/") + "\0" + symbol
  if (duplicateValues([...seenSourceRefs, key]).includes(key)) fail(owner + " duplicates source reference " + key)
  seenSourceRefs.add(key)
  return { path, symbol, text, digest: actual, key }
}

function validateFormatterCase(id, owner, seenFormatterRefs) {
  const item = formatterById.get(id)
  if (!item) {
    fail(owner + " references missing formatter case " + id)
    return undefined
  }
  if (duplicateValues([...seenFormatterRefs, id]).includes(id)) fail(owner + " duplicates formatter case " + id)
  seenFormatterRefs.add(id)
  const input = inputText(item.input, owner + "." + id)
  const output = outputText(item.output, owner + "." + id)
  validateCanonicalOutput(item.output, owner + "." + id)
  if (isExpectedEcho(input, output)) fail(owner + "." + id + " is an expected echo, not a formatting pair")
  if (!Array.isArray(item.requiredSemicolons)) fail(owner + "." + id + " has no requiredSemicolons array")
  return { item, input, output }
}

function validateSemanticPair(positiveId, negativeId, failureField, owner, seenSemanticRefs) {
  const positive = semanticById.get(positiveId)
  const negative = semanticById.get(negativeId)
  if (!positive || !negative) {
    fail(owner + " references missing S0 pair " + positiveId + "/" + negativeId)
    return undefined
  }
  for (const id of [positiveId, negativeId]) {
    if (duplicateValues([...seenSemanticRefs, id]).includes(id)) fail(owner + " duplicates semantic case " + id)
    seenSemanticRefs.add(id)
  }
  if (positive.kind !== "positive" || negative.kind !== "negative") fail(owner + " does not use positive/negative S0 kinds")
  if (negative.baseline !== positiveId || negative.rule !== positive.rule) fail(owner + " S0 pair does not share rule and baseline")
  if (negative.failureField !== failureField) fail(owner + " failureField " + failureField + " disagrees with " + negative.id)
  if (!positive.expect || !positive.expect.semanticResult || (positive.expect.diagnostics || []).length !== 0) fail(owner + " positive S0 case is not a clean SemanticResult")
  if (!negative.expect || !Array.isArray(negative.expect.diagnostics) || negative.expect.diagnostics.length !== 1) fail(owner + " negative S0 case must contain one D0 root")
  const diagnostic = negative.expect && negative.expect.diagnostics && negative.expect.diagnostics[0]
  const d0 = validateD0(diagnostic, owner + "." + negativeId)
  const baselineValue = positive.expect && positive.expect.semanticResult && positive.expect.semanticResult[failureField]
  const expectedEvidence = digest(canonical(baselineValue))
  if (!negative.failureEvidence || negative.failureEvidence.field !== failureField || negative.failureEvidence.baselineValueDigest !== expectedEvidence) {
    fail(owner + " rejects expected echo or stale failureEvidence for " + failureField)
  }
  const positiveSource = positive.source.join("\n") + "\n"
  const negativeSource = negative.source.join("\n") + "\n"
  if (isExpectedEcho(positiveSource, negativeSource)) fail(owner + " positive and negative source are an expected echo")
  return { positive, negative, d0, positiveSource, negativeSource }
}

function validateWorkflowCases(outcomes, owner, seenWorkflowRefs) {
  if (!Array.isArray(outcomes) || outcomes.length === 0) {
    fail(owner + " must list one or more linked workflow outcomes")
    return []
  }
  const ids = outcomes.map((outcome) => outcome && outcome.id)
  for (const id of duplicateValues(ids)) fail(owner + " duplicates workflow case " + id)
  for (const outcome of outcomes) {
    const id = outcome && outcome.id
    const item = workflowById.get(id)
    if (!item) {
      fail(owner + " references missing workflow case " + id)
      continue
    }
    if (duplicateValues([...seenWorkflowRefs, id]).includes(id)) fail(owner + " duplicates workflow case " + id)
    seenWorkflowRefs.add(id)
    if (!item.expected || item.expected.status !== "rejected") fail(owner + "." + id + " must be a rejected adversarial case")
    if (typeof outcome.reason !== "string" || outcome.reason.trim() === "") fail(owner + "." + id + " has no waiver reason")
    if (outcome.code !== item.expected.code || outcome.at !== item.expected.at) fail(owner + "." + id + " waiver outcome does not match RU0 module-run")
  }
  return outcomes
}

function validateWorkflowCase(id, expected, owner, seenWorkflowRefs) {
  if (!id) return undefined
  const item = workflowById.get(id)
  if (!item) {
    fail(owner + " references missing workflow case " + id)
    return undefined
  }
  if (!item.expected || item.expected.status !== "accepted") {
    fail(owner + "." + id + " must be an accepted positive case")
  }
  if (expected && canonical(item.expected) !== canonical(expected)) {
    fail(owner + "." + id + " expected outcome is stale")
  }
  if (seenWorkflowRefs) {
    if (duplicateValues([...seenWorkflowRefs, id]).includes(id)) fail(owner + " duplicates workflow case " + id)
    seenWorkflowRefs.add(id)
  }
  return item
}

for (const guardError of corpusGuardErrors(corpus, expectedFamilies)) fail(guardError)
if (corpus.$schema !== "w-frontend-freeze-cases-1" || corpus.status !== "design-oracle-input") fail("invalid schema or status")
if (!Array.isArray(corpus.families) || corpus.families.length !== expectedFamilies.length) fail("families must contain exactly six G0-G5 entries")
if (!preflightOnly && !fs.existsSync(treeSitter)) fail("Tree-sitter CLI is missing")

const seenFamilies = new Set()
const seenSourceRefs = new Set()
const seenFormatterRefs = new Set()
const seenSemanticRefs = new Set()
const seenWorkflowRefs = new Set()
const records = []

for (const [index, entry] of (corpus.families || []).entries()) {
  const owner = "families[" + index + "]"
  if (!expectedFamilies.includes(entry.family)) fail(owner + ".family must be G0-G5")
  if (seenFamilies.has(entry.family)) fail(owner + ".family duplicates " + entry.family)
  seenFamilies.add(entry.family)
  if (!/^FZ0-G[0-5]-[a-z0-9-]+$/.test(entry.id || "")) fail(owner + ".id is invalid")
  if (!Array.isArray(entry.decisions) || entry.decisions.length === 0) fail(owner + ".decisions is empty")
  for (const decision of entry.decisions || []) if (!ledgerIdSet.has(decision)) fail(owner + ".decisions references missing " + decision)
  const source = validateSourceRef(entry, index, seenSourceRefs)
  if (!source) continue
  if (!preflightOnly) {
    const parsedSource = await parseFile(source.path, entry.id)
    if (parsedSource.recovery) fail(owner + ".sourceRef has parser recovery")
  }

  const positive = entry.positive || {}
  const formatterIds = positive.formatterCases || (positive.formatterCase ? [positive.formatterCase] : [])
  if (formatterIds.length === 0) fail(owner + ".positive has no formatter case")
  const formats = formatterIds.map((id) => validateFormatterCase(id, owner + ".positive", seenFormatterRefs)).filter(Boolean)
  const workflowPositive = validateWorkflowCase(positive.workflowCase, positive.workflowExpected, owner + ".positive", seenWorkflowRefs)
  const positiveEvidence = formats.flatMap((item) => [item.input, item.output])
  if (positive.semanticCase) {
    const positiveSemantic = semanticById.get(positive.semanticCase)
    if (!positiveSemantic) fail(owner + ".positive references missing semantic case " + positive.semanticCase)
    else positiveEvidence.push(positiveSemantic.source.join("\n") + "\n")
  }
  if (workflowPositive) positiveEvidence.push(JSON.stringify(workflowPositive))
  const combined = [source.text, ...positiveEvidence]
  if (!(positive.markers || []).some((marker) => source.text.includes(marker))) {
    fail(owner + " positive markers do not exercise its Last Light sourceRef")
  }
  if (!(positive.markers || []).some((marker) => positiveEvidence.some((text) => text.includes(marker)))) {
    fail(owner + " positive markers do not exercise its F0/S0/workflow evidence")
  }
  for (const marker of positive.markers || []) {
    if (typeof marker !== "string" || !combined.some((text) => text.includes(marker))) fail(owner + " marker " + JSON.stringify(marker) + " is absent from source/evidence")
  }
  const parsedFormats = []
  if (!preflightOnly) {
    for (const [formatIndex, format] of formats.entries()) {
      const inputTree = await parseText(format.input, entry.id + "-format-" + formatIndex + "-input")
      const outputTree = await parseText(format.output, entry.id + "-format-" + formatIndex + "-output")
      if (inputTree.recovery || outputTree.recovery) fail(owner + " formatter case " + format.item.id + " uses recovery")
      if (inputTree.output !== outputTree.output) fail(owner + " formatter case " + format.item.id + " changes named CST")
      parsedFormats.push({ id: format.item.id, sourceDigest: inputTree.digest, canonicalDigest: outputTree.digest })
    }
  }

  const adversarial = entry.adversarial
  if (!adversarial || typeof adversarial.kind !== "string") {
    fail(owner + " has no adversarial evidence")
    continue
  }
  if (typeof adversarial.failureField !== "string" || adversarial.failureField.trim() === "") {
    fail(owner + ".adversarial must provide an exact failureField")
  }
  let adversarialRecord
  if (adversarial.kind === "semantic-inversion") {
    const pair = validateSemanticPair(positive.semanticCase, adversarial.semanticCase, adversarial.failureField, owner, seenSemanticRefs)
    if (!pair) continue
    if (!preflightOnly) {
      const positiveTree = await parseText(pair.positiveSource, entry.id + "-semantic-positive")
      const negativeTree = await parseText(pair.negativeSource, entry.id + "-semantic-negative")
      if (positiveTree.recovery || negativeTree.recovery) fail(owner + " S0 source uses parser recovery")
    }
    if (pair.d0 && adversarial.diagnostic) {
      const expected = adversarial.diagnostic
      if (pair.d0.code !== expected.code || pair.d0.phase !== expected.phase || canonical(pair.d0.facts) !== canonical(expected.facts || {})) fail(owner + " D0 does not match the exact S0 diagnostic record")
    }
    if (adversarial.baseline !== pair.positive.id) fail(owner + " adversarial baseline is stale or does not name the positive S0 case")
    if (!adversarial.failureEvidence || canonical(adversarial.failureEvidence) !== canonical(pair.negative.failureEvidence)) {
      fail(owner + " adversarial failureEvidence is stale or does not match the negative S0 case")
    }
    if (!entry.decisions.includes(pair.negative.rule)) fail(owner + " does not list its S0 rule " + pair.negative.rule)
    adversarialRecord = { kind: adversarial.kind, positive: pair.positive.id, negative: pair.negative.id, failureField: adversarial.failureField, diagnostic: pair.d0 }
  } else if (adversarial.kind === "syntax-invalid") {
    const basis = validateFormatterCase(adversarial.basis && adversarial.basis.formatterCase, owner + ".adversarial", seenFormatterRefs)
    if (!basis) continue
    const remove = adversarial.basis && adversarial.basis.remove
    if (typeof remove !== "string" || remove.length === 0 || basis.output.split(remove).length - 1 !== 1) {
      fail(owner + " syntax mutation must remove one exact token sequence")
      continue
    }
    const d0 = validateD0(adversarial.diagnostic, owner + ".adversarial")
    if (d0 && d0.phase !== "source.parse") fail(owner + " syntax D0 must be source.parse")
    const declaredMutation = adversarial.diagnostic && adversarial.diagnostic.facts && adversarial.diagnostic.facts.mutation
    if (!declaredMutation || declaredMutation.basis !== basis.item.id || declaredMutation.removed !== remove) {
      fail(owner + " syntax D0 mutation/basis facts do not match declared evidence")
    }
    if (preflightOnly) {
      validateDeclaredRecoveryFacts(declaredMutation && declaredMutation.recovery, owner + ".adversarial")
      adversarialRecord = { kind: adversarial.kind, basis: basis.item.id, removed: remove, recovery: declaredMutation && declaredMutation.recovery, failureField: adversarial.failureField, diagnostic: d0 }
    } else {
      const mutated = basis.output.replace(remove, "")
      const mutationTree = await parseText(mutated, entry.id + "-syntax-mutation", true)
      if (!mutationTree.recovery) fail(owner + " syntax mutation does not produce parser recovery")
      const expectedMutation = {
        basis: basis.item.id,
        removed: remove,
        recovery: mutationTree.recoveryFacts,
      }
      if (!declaredMutation || canonical(declaredMutation) !== canonical(expectedMutation)) {
        fail(owner + " syntax D0 mutation/basis/recovery facts do not match derived evidence")
      }
      adversarialRecord = { kind: adversarial.kind, basis: basis.item.id, removed: remove, mutationDigest: mutationTree.digest, recovery: mutationTree.recoveryFacts, failureField: adversarial.failureField, diagnostic: d0 }
    }
  } else if (adversarial.kind === "source-waiver") {
    if (typeof adversarial.reason !== "string" || adversarial.reason.trim() === "") fail(owner + " source waiver has no reason")
    if (adversarial.semanticCase || adversarial.baseline) fail(owner + " source waiver cannot carry an S0 pair")
    const workflowOutcomes = validateWorkflowCases(adversarial.workflowOutcomes, owner + ".adversarial", seenWorkflowRefs)
    const d0 = validateD0(adversarial.diagnostic, owner + ".adversarial")
    if (d0 && (!d0.facts.waiver || canonical(d0.facts.workflowOutcomes) !== canonical(workflowOutcomes))) {
      fail(owner + " waiver D0 must carry the exact linked RU0 module-run outcomes and waiver flag")
    }
    adversarialRecord = { kind: adversarial.kind, workflowOutcomes, reason: adversarial.reason, failureField: adversarial.failureField, diagnostic: d0 }
  } else {
    fail(owner + " uses unknown adversarial kind " + adversarial.kind)
    continue
  }
  const evidenceDecisions = new Set(formats.flatMap((format) => format.item.decisions || []))
  if (positive.semanticCase) evidenceDecisions.add(semanticById.get(positive.semanticCase) && semanticById.get(positive.semanticCase).rule)
  if (adversarial.kind === "semantic-inversion") evidenceDecisions.add(semanticById.get(adversarial.semanticCase) && semanticById.get(adversarial.semanticCase).rule)
  for (const decision of entry.decisions) if (!evidenceDecisions.has(decision)) fail(owner + " decision " + decision + " is not exercised by linked F0/S0 evidence")
  records.push({
    schemaVersion: 1,
    id: entry.id,
    family: entry.family,
    construction: entry.construction,
    decisions: entry.decisions,
    source: { path: relative(root, source.path).replaceAll("\\", "/"), symbol: source.symbol, digest: source.digest },
    workflow: workflowPositive ? { id: workflowPositive.id, expected: workflowPositive.expected } : undefined,
    formatter: parsedFormats,
    adversarial: adversarialRecord,
  })
}

for (const family of expectedFamilies) if (!seenFamilies.has(family)) fail("missing family " + family)

const expectedSnapshot = records.map((record) => JSON.stringify(record)).join("\n") + "\n"
if (!preflightOnly) {
  if (writeSnapshot) await Bun.write(snapshotPath, expectedSnapshot)
  else if (!fs.existsSync(snapshotPath)) fail("frontend-freeze.snapshot.jsonl is missing; run with --write")
  else if ((await Bun.file(snapshotPath).text()) !== expectedSnapshot) fail("frontend-freeze.snapshot.jsonl is stale; run with --write")
}

if (temporary) await rm(temporary, { recursive: true, force: true })

if (errors.length > 0) {
  process.stderr.write(errors.join("\n") + "\n")
  process.exit(1)
}

const syntaxCount = records.filter((record) => record.adversarial.kind === "syntax-invalid").length
const semanticCount = records.filter((record) => record.adversarial.kind === "semantic-inversion").length
const waiverCount = records.filter((record) => record.adversarial.kind === "source-waiver").length
const decisionCount = new Set(records.flatMap((record) => record.decisions)).size
console.log("Frontend freeze: " + records.length + "/6 G0-G5 families, " + syntaxCount + " syntax-invalid, " + semanticCount + " S0 inversions, " + waiverCount + " source waivers, " + decisionCount + " decisions.")
