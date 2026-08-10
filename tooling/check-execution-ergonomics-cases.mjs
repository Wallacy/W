import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { deriveExecutionErgonomics, summarizeDiagnostics } from "./execution-ergonomics-machine.mjs"

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url))
const casesPath = path.join(toolingDirectory, "execution-ergonomics-cases.json")
const snapshotPath = path.join(toolingDirectory, "execution-ergonomics-results.snapshot.jsonl")
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"))
const errors = []
const results = []
const caseIds = new Set()
const allowedKinds = new Set(["positive", "negative", "information"])
const requiredIds = new Set([
  "EE-POS-direct-never",
  "EE-POS-callable-external-internal-label",
  "EE-POS-label-shapes-disjoint",
  "EE-POS-await-may",
  "EE-POS-async-let-sync",
  "EE-POS-spawn-may",
  "EE-POS-scc-inference",
  "EE-INFO-public-widening",
  "EE-NEG-bare-may-suspend",
  "EE-NEG-blocking-sync",
  "EE-NEG-spawn-serial",
  "EE-POS-spawn-nonserial-domain",
  "EE-NEG-spawn-declaration-placement",
  "EE-POS-implicit-process-projections",
  "EE-NEG-process-escape",
  "EE-NEG-process-service-crossing",
  "EE-NEG-process-serialization",
  "EE-POS-doc-example",
  "EE-POS-doc-example-two-blocks",
  "EE-NEG-doc-example-ambient",
  "EE-POS-flat-std",
  "EE-NEG-flat-std-tier-field",
])

if (corpus.$schema !== "w-execution-ergonomics-cases-2") errors.push("schema")
if (corpus.status !== "design-oracle-input") errors.push("status")
if (!Array.isArray(corpus.cases) || corpus.cases.length < 25) errors.push("coverage")

function firstCall(result, expectedForm, expectedCallee) {
  return result.suspension.calls.find((call) => call.callForm === expectedForm && (!expectedCallee || call.callee === expectedCallee))
}

function callableDeclaration(result, expected) {
  const name = expected.callableName ?? expected.declaration
  return result.labels.declarations.find((declaration) => !name || declaration.name === name)
}

function suspensionDeclaration(result, expected) {
  const name = expected.callee ?? expected.declaration
  return result.suspension.declarations.find((declaration) => {
    if (name && declaration.name !== name) return false
    return declaration.suspension === expected.suspension || declaration.sourceSpelling === expected.sourceSpelling
  })
}

for (const [index, item] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`
  if (!/^EE-(?:POS|NEG|INFO)-[a-z0-9-]+$/.test(item.id ?? "")) errors.push(`${location}: id`)
  if (caseIds.has(item.id)) errors.push(`${location}: duplicate id ${item.id}`)
  caseIds.add(item.id)
  if (!allowedKinds.has(item.kind) || typeof item.source !== "string") {
    errors.push(`${location}: shape`)
    continue
  }
  const result = deriveExecutionErgonomics(item.source, item.input ?? {})
  const codes = summarizeDiagnostics(result)
  const expected = item.expect ?? {}
  const hasCallableExpectation = expected.callablePolicy || expected.forms || expected.callableExternal !== undefined || expected.callableInternal !== undefined || expected.callShapes
  if (hasCallableExpectation && !(expected.callableName || expected.declaration)) errors.push(`${item.id}: callable expectation needs name`)
  if ((expected.suspension || expected.sourceSpelling) && !(expected.callee || expected.declaration)) errors.push(`${item.id}: suspension expectation needs name`)
  if (expected.diagnostic && !codes.includes(expected.diagnostic)) {
    errors.push(`${item.id}: expected ${expected.diagnostic}; actual ${codes.join(",") || "none"}`)
  }
  if (!expected.diagnostic && (item.kind === "positive" || item.kind === "information") && codes.length > 0) {
    errors.push(`${item.id}: unexpected diagnostics ${codes.join(",")}`)
  }
  if (expected.genericPolicy) {
    const policy = result.labels.generic.heads[0]?.parameters.find((parameter) => parameter.policy !== "type")?.policy
    if (policy !== expected.genericPolicy) errors.push(`${item.id}: generic policy ${policy}`)
  }
  if (expected.genericIdentity && result.labels.generic.identities[0]?.sameAsNext !== (expected.genericIdentity === "same")) {
    errors.push(`${item.id}: generic identity`)
  }
  if (expected.callablePolicy) {
    const policy = callableDeclaration(result, expected)?.params[0]?.policy
    if (policy !== expected.callablePolicy) errors.push(`${item.id}: callable policy ${policy}`)
  }
  if (expected.forms && JSON.stringify(callableDeclaration(result, expected)?.params[0]?.forms) !== JSON.stringify(expected.forms)) {
    errors.push(`${item.id}: forms`)
  }
  if (expected.callableExternal !== undefined && callableDeclaration(result, expected)?.params[expected.parameterIndex ?? 0]?.external !== expected.callableExternal) {
    errors.push(`${item.id}: callable external`)
  }
  if (expected.callableInternal !== undefined && callableDeclaration(result, expected)?.params[expected.parameterIndex ?? 0]?.internal !== expected.callableInternal) {
    errors.push(`${item.id}: callable internal`)
  }
  if (expected.callShapes && JSON.stringify(callableDeclaration(result, expected)?.callShapes) !== JSON.stringify(expected.callShapes)) {
    errors.push(`${item.id}: complete call shapes`)
  }
  if (expected.suspension) {
    const declaration = suspensionDeclaration(result, expected)
    if (!declaration) errors.push(`${item.id}: suspension ${expected.suspension}`)
  }
  if (expected.sourceSpelling) {
    const declaration = suspensionDeclaration(result, expected)
    if (!declaration) errors.push(`${item.id}: source spelling ${expected.sourceSpelling}`)
  }
  if (expected.callForm && !firstCall(result, expected.callForm, expected.callee)) errors.push(`${item.id}: call form ${expected.callForm}`)
  if (expected.childForm && !result.suspension.children.some((child) => child.form === expected.childForm)) errors.push(`${item.id}: child form ${expected.childForm}`)
  if (expected.childAccepts && JSON.stringify(result.suspension.children[0]?.accepts) !== JSON.stringify(expected.childAccepts)) errors.push(`${item.id}: child callable policy`)
  if (expected.sccSuspension) {
    const component = result.suspension.scc.find((component) => expected.members.every((member) => component.members.includes(member)))
    if (!component || component.suspension !== expected.sccSuspension) errors.push(`${item.id}: SCC inference`)
  }
  if (expected.sourceBreaking !== undefined && result.suspension.public?.sourceBreaking !== expected.sourceBreaking) errors.push(`${item.id}: public widening`)
  if (expected.widening !== undefined && result.suspension.public?.widening !== expected.widening) errors.push(`${item.id}: widening`)
  if (expected.removable !== undefined && !codes.includes("W-SUSPEND-0002")) errors.push(`${item.id}: removable await`)
  if (expected.evaluation && JSON.stringify(result.suspension.staging) !== JSON.stringify(expected.evaluation)) errors.push(`${item.id}: staging`)
  if (expected.child === "structured" && result.suspension.children.length === 0) errors.push(`${item.id}: structured child`)
  if (expected.sameOptionalDomainForm !== undefined && result.placement.sameOptionalDomainForm !== expected.sameOptionalDomainForm) errors.push(`${item.id}: domain forms`)
  if (expected.projections && JSON.stringify(result.process.projections) !== JSON.stringify(expected.projections)) errors.push(`${item.id}: process projections`)
  if (expected.readOnly !== undefined && result.process.readOnly !== expected.readOnly) errors.push(`${item.id}: process read-only`)
  if (expected.terminal && result.doctest.examples[0]?.terminals[0]?.kind !== expected.terminal) errors.push(`${item.id}: doctest terminal`)
  if (expected.exampleCount !== undefined && result.doctest.examples.length !== expected.exampleCount) errors.push(`${item.id}: doctest example count`)
  if (expected.hermetic !== undefined && result.doctest.hermetic !== expected.hermetic) errors.push(`${item.id}: doctest hermetic`)
  if (expected.releasePayload !== undefined && result.doctest.releasePayload !== expected.releasePayload) errors.push(`${item.id}: release payload`)
  if (expected.hasTierField !== undefined && result.std.hasTierField !== expected.hasTierField) errors.push(`${item.id}: tier field`)
  if (expected.authority && JSON.stringify(result.std.authorities) !== JSON.stringify(expected.authority)) errors.push(`${item.id}: std authority`)
  results.push({
    caseId: item.id,
    kind: item.kind,
    diagnostics: codes,
    labels: {
      callable: result.labels.declarations.map((declaration) => ({ name: declaration.name, params: declaration.params, callShapes: declaration.callShapes })),
      generic: result.labels.generic.heads.map((head) => ({ name: head.name, parameters: head.parameters })),
    },
    suspension: {
      declarations: result.suspension.declarations,
      children: result.suspension.children,
      scc: result.suspension.scc,
      public: result.suspension.public,
    },
    placement: result.placement,
    process: result.process,
    doctest: result.doctest,
    std: result.std,
  })
}

for (const id of requiredIds) if (!caseIds.has(id)) errors.push(`missing required case ${id}`)

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`)
  process.exit(1)
}

const expectedSnapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`
const counts = results.reduce((accumulator, result) => {
  accumulator[result.kind] = (accumulator[result.kind] ?? 0) + 1
  return accumulator
}, {})
const summary = `Execution ergonomics: ${results.length} cases, ${counts.positive ?? 0} positive, ${counts.negative ?? 0} negative, ${counts.information ?? 0} information.`

if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, expectedSnapshot)
  process.stdout.write(`${summary}\nUpdated ${path.basename(snapshotPath)}.\n`)
  process.exit(0)
}
if (!fs.existsSync(snapshotPath)) {
  process.stderr.write(`${path.basename(snapshotPath)} is missing; run with --write.\n`)
  process.exit(1)
}
if (fs.readFileSync(snapshotPath, "utf8") !== expectedSnapshot) {
  process.stderr.write(`${path.basename(snapshotPath)} is stale; run with --write.\n`)
  process.exit(1)
}
process.stdout.write(`${summary}\n`)
