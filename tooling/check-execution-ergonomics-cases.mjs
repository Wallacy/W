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
  "EE-POS-async-child-sync",
  "EE-POS-spawn-may",
  "EE-POS-scc-inference",
  "EE-INFO-public-widening",
  "EE-NEG-bare-may-suspend",
  "EE-NEG-blocking-sync",
  "EE-POS-sync-direct-entry",
  "EE-POS-sync-composition",
  "EE-NEG-sync-transitive-facet-loss",
  "EE-NEG-sync-invalid-call-poisons-caller",
  "EE-NEG-sync-unknown-uppercase-poisons-caller",
  "EE-POS-sync-scc-direct-entry",
  "EE-NEG-sync-explicit-await",
  "EE-NEG-sync-dynamic-path",
  "EE-INFO-direct-entry-breaking",
  "EE-NEG-sync-protocol-bodyless",
  "EE-NEG-sync-foreign-bodyless",
  "EE-POS-sync-indirect-facet",
  "EE-NEG-sync-erased-facet",
  "EE-POS-spawn-serial-domain",
  "EE-POS-spawn-main-domain",
  "EE-POS-spawn-parallel-domain",
  "EE-NEG-spawn-missing-domain",
  "EE-NEG-spawn-unknown-domain",
  "EE-NEG-spawn-declaration-placement",
  "EE-POS-barrier-read-write-read",
  "EE-POS-barrier-serial-domain",
  "EE-NEG-barrier-capability-missing",
  "EE-NEG-barrier-may-suspend",
  "EE-NEG-ordinary-domain-write",
  "EE-NEG-barrier-open-access-graph",
  "EE-POS-dynamic-serial-drain",
  "EE-NEG-dynamic-serial-authority",
  "EE-NEG-dynamic-serial-live-limit",
  "EE-NEG-dynamic-serial-lane-limit",
  "EE-NEG-dynamic-serial-aggregate-limit",
  "EE-NEG-dynamic-serial-closed-admission",
  "EE-NEG-dynamic-concurrent-lane",
  "EE-POS-contextual-execution-root",
  "EE-NEG-execution-escape",
  "EE-NEG-execution-service-crossing",
  "EE-NEG-execution-serialization",
  "EE-POS-doc-example",
  "EE-POS-doc-example-two-blocks",
  "EE-NEG-doc-example-ambient",
  "EE-NEG-record-named-marker",
  "EE-POS-named-remains-identifier",
  "EE-POS-parameter-contract-after-binding",
  "EE-NEG-parameter-contract-before-binding",
  "EE-NEG-copy-parameter-mode",
  "EE-NEG-initializer-contract-before-binding",
  "EE-POS-owned-place-explicit-ref-call",
  "EE-POS-existing-borrow-call",
  "EE-POS-fresh-owner-take-call",
  "EE-NEG-call-operation-mismatch",
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
    const policy = callableDeclaration(result, expected)?.params[expected.parameterIndex ?? 0]?.policy
    if (policy !== expected.callablePolicy) errors.push(`${item.id}: callable policy ${policy}`)
  }
  if (expected.forms && JSON.stringify(callableDeclaration(result, expected)?.params[expected.parameterIndex ?? 0]?.forms) !== JSON.stringify(expected.forms)) {
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
  const actualContractModes = callableDeclaration(result, expected)?.params
    .map((parameter) => parameter.contractMode)
  if (expected.contractModes
    && JSON.stringify(actualContractModes) !== JSON.stringify(expected.contractModes)) {
    errors.push(`${item.id}: parameter contract modes`)
  }
  if (expected.suspension) {
    const declaration = suspensionDeclaration(result, expected)
    if (!declaration) errors.push(`${item.id}: suspension ${expected.suspension}`)
  }
  if (expected.sourceSpelling) {
    const declaration = suspensionDeclaration(result, expected)
    if (!declaration) errors.push(`${item.id}: source spelling ${expected.sourceSpelling}`)
  }
  if (expected.directEntry) {
    const declaration = result.suspension.declarations.find((candidate) => candidate.name === (expected.callee ?? expected.declaration))
    if (declaration?.directEntry !== expected.directEntry) errors.push(`${item.id}: direct entry ${declaration?.directEntry ?? "missing"}`)
  }
  if (expected.callForm && !firstCall(result, expected.callForm, expected.callee)) errors.push(`${item.id}: call form ${expected.callForm}`)
  const syncCall = result.suspension.syncCalls.find((call) => call.callee === expected.callee)
  if (expected.syncCurrent !== undefined && syncCall?.eligible !== expected.syncCurrent) errors.push(`${item.id}: sync current contract`)
  if (expected.publishedSuspension !== undefined && syncCall?.publishedSuspension !== expected.publishedSuspension) errors.push(`${item.id}: sync published suspension`)
  if (expected.selectedEntrySuspension !== undefined && syncCall?.selectedEntrySuspension !== expected.selectedEntrySuspension) errors.push(`${item.id}: sync selected entry suspension`)
  if (expected.blocksThread !== undefined && syncCall?.blocksThread !== expected.blocksThread) errors.push(`${item.id}: sync blocksThread`)
  if (expected.createsTask !== undefined && syncCall?.createsTask !== expected.createsTask) errors.push(`${item.id}: sync createsTask`)
  if (expected.suspendsTask !== undefined && syncCall?.suspendsTask !== expected.suspendsTask) errors.push(`${item.id}: sync suspendsTask`)
  if (expected.sameExecutionContext !== undefined) {
    const sameExecutionContext = syncCall?.sameTask === true && syncCall?.sameContext === true && syncCall?.sameDomain === true
    if (sameExecutionContext !== expected.sameExecutionContext) errors.push(`${item.id}: sync execution context`)
  }
  if (expected.runtimeFallback !== undefined && syncCall?.runtimeFallback !== expected.runtimeFallback) errors.push(`${item.id}: sync runtime fallback`)
  if (expected.tryOrthogonal !== undefined && result.suspension.tryOrthogonal !== expected.tryOrthogonal) errors.push(`${item.id}: try orthogonality`)
  if (expected.childForm && !result.suspension.children.some((child) => child.form === expected.childForm)) errors.push(`${item.id}: child form ${expected.childForm}`)
  if (expected.childAccepts && JSON.stringify(result.suspension.children[0]?.accepts) !== JSON.stringify(expected.childAccepts)) errors.push(`${item.id}: child callable policy`)
  if (expected.sccSuspension) {
    const component = result.suspension.scc.find((component) => expected.members.every((member) => component.members.includes(member)))
    if (!component || component.suspension !== expected.sccSuspension) errors.push(`${item.id}: SCC inference`)
  }
  if (expected.sccDirectEntry) {
    const component = result.suspension.directEntryScc.find((candidate) =>
      expected.members.every((member) => candidate.members.includes(member)))
    if (!component || component.directEntry !== expected.sccDirectEntry) errors.push(`${item.id}: direct-entry SCC`)
    if (component?.terminationProven !== expected.terminationProven) errors.push(`${item.id}: termination proof`)
    if (component?.evaluationPerformed !== expected.evaluationPerformed) errors.push(`${item.id}: static SCC evaluation`)
  }
  if (expected.sourceBreaking !== undefined && result.suspension.public?.sourceBreaking !== expected.sourceBreaking) errors.push(`${item.id}: public widening`)
  if (expected.semanticInterfaceKeyChanged !== undefined && result.suspension.public?.semanticInterfaceKeyChanged !== expected.semanticInterfaceKeyChanged) errors.push(`${item.id}: semantic interface key`)
  if (expected.widening !== undefined && result.suspension.public?.widening !== expected.widening) errors.push(`${item.id}: widening`)
  if (expected.removable !== undefined && !codes.includes("W-SUSPEND-0002")) errors.push(`${item.id}: removable await`)
  if (expected.evaluation && JSON.stringify(result.suspension.staging) !== JSON.stringify(expected.evaluation)) errors.push(`${item.id}: staging`)
  if (expected.child === "structured" && result.suspension.children.length === 0) errors.push(`${item.id}: structured child`)
  if (expected.sameOptionalDomainForm !== undefined && result.placement.sameOptionalDomainForm !== expected.sameOptionalDomainForm) errors.push(`${item.id}: domain forms`)
  if (expected.domain !== undefined) {
    const dispatch = result.placement.dispatches.find((item) =>
      item.domain === expected.domain && (!expected.dispatchMode || item.mode === expected.dispatchMode))
    if (!dispatch) errors.push(`${item.id}: domain dispatch ${expected.domain}`)
    else {
      if (expected.scheduling !== undefined && dispatch.scheduling !== expected.scheduling) errors.push(`${item.id}: domain scheduling`)
      if (expected.overlapWithinTarget !== undefined && dispatch.overlapWithinTarget !== expected.overlapWithinTarget) errors.push(`${item.id}: domain overlap`)
      if (expected.barrierSupport !== undefined && dispatch.barrierSupport !== expected.barrierSupport) errors.push(`${item.id}: barrier support`)
    }
  }
  if (expected.place !== undefined) {
    const sequence = result.placement.loanSequences.find((item) => item.place === expected.place)
    if (!sequence) errors.push(`${item.id}: loan sequence ${expected.place}`)
    else {
      if (expected.loanClosed !== undefined && sequence.closed !== expected.loanClosed) errors.push(`${item.id}: loan closure`)
      if (expected.barrierEdges && JSON.stringify(sequence.edges) !== JSON.stringify(expected.barrierEdges)) errors.push(`${item.id}: barrier edges`)
    }
  }
  if (expected.executionMembers && JSON.stringify(result.execution.members) !== JSON.stringify(expected.executionMembers)) errors.push(`${item.id}: execution members`)
  if (expected.executionFacets && JSON.stringify(result.execution.facets) !== JSON.stringify(expected.executionFacets)) errors.push(`${item.id}: execution facets`)
  if (expected.contextual !== undefined && result.execution.contextual !== expected.contextual) errors.push(`${item.id}: execution contextual root`)
  if (expected.terminal && result.doctest.examples[0]?.terminals[0]?.kind !== expected.terminal) errors.push(`${item.id}: doctest terminal`)
  if (expected.exampleCount !== undefined && result.doctest.examples.length !== expected.exampleCount) errors.push(`${item.id}: doctest example count`)
  if (expected.hermetic !== undefined && result.doctest.hermetic !== expected.hermetic) errors.push(`${item.id}: doctest hermetic`)
  if (expected.releasePayload !== undefined && result.doctest.releasePayload !== expected.releasePayload) errors.push(`${item.id}: release payload`)
  if (expected.dynamicStatus !== undefined && result.dynamicSerial.status !== expected.dynamicStatus) errors.push(`${item.id}: dynamic status`)
  if (expected.dynamicPhase !== undefined && result.dynamicSerial.phase !== expected.dynamicPhase) errors.push(`${item.id}: dynamic phase`)
  if (expected.dynamicError !== undefined && result.dynamicSerial.error !== expected.dynamicError) errors.push(`${item.id}: dynamic error`)
  if (expected.poolReuse !== undefined && result.dynamicSerial.poolReuse !== expected.poolReuse) errors.push(`${item.id}: dynamic pool reuse`)
  if (expected.referenceExtendsOwner !== undefined && result.dynamicSerial.referenceExtendsOwner !== expected.referenceExtendsOwner) errors.push(`${item.id}: dynamic owner reference`)
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
      directEntryScc: result.suspension.directEntryScc,
      public: result.suspension.public,
    },
    placement: result.placement,
    process: result.process,
    doctest: result.doctest,
    dynamicSerial: result.dynamicSerial,
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
