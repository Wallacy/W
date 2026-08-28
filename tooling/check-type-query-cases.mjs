import fs from "node:fs"
import path from "node:path"

const root = path.resolve(import.meta.dirname, "..")
const corpusPath = path.join(root, "tooling", "type-query-cases.json")
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"))

function fail(message) {
  throw new Error(`type-query cases: ${message}`)
}

function read(relative) {
  return fs.readFileSync(path.join(root, relative), "utf8")
}

function requireString(value, owner) {
  if (typeof value !== "string" || value.length === 0) fail(`${owner} must be a non-empty string`)
}

if (corpus.$schema !== "w-type-query-cases-1") fail("unexpected schema")
if (corpus.status !== "design-oracle-input") fail("status must not claim implementation")
if (corpus.rule !== "W-1492") fail("cases must be owned by W-1492")
if (/implementation|compiler is ready|runtime is complete/iu.test(corpus.note ?? "")) fail("note claims implementation")

const benchmark = corpus.benchmarkDisposition
const blockers = [
  "verified-hir",
  "existential-layout",
  "type-query-lowering",
  "runtime-metadata",
  "native-backend",
  "language-benchmark-runner",
]
if (!benchmark || benchmark.status !== "deferred" || benchmark.taskId !== "runtime-type-identity-metadata-benchmark") {
  fail("benchmark disposition must remain deferred with the named task")
}
if (JSON.stringify(benchmark.blockers) !== JSON.stringify(blockers)) fail("benchmark blockers drifted")
if (JSON.stringify(benchmark.measureSeparately) !== JSON.stringify(["is", "as?", "type of", "info of"])) {
  fail("benchmark axes must remain separate")
}
if (JSON.stringify(benchmark.metrics) !== JSON.stringify([
  "latency-hot",
  "latency-cold",
  "throughput-hot",
  "throughput-cold",
  "binary-and-code-size",
  "existential-footprint",
  "metadata-reachability-and-stripping",
])) fail("benchmark metrics drifted")
const benchmarkVariants = [
  { id: "is-hit-monomorphic", axis: "is", outcome: "hit", dispatch: "monomorphic" },
  { id: "is-miss-monomorphic", axis: "is", outcome: "miss", dispatch: "monomorphic" },
  { id: "is-hit-polymorphic", axis: "is", outcome: "hit", dispatch: "polymorphic" },
  { id: "is-miss-polymorphic", axis: "is", outcome: "miss", dispatch: "polymorphic" },
  { id: "as-hit-monomorphic", axis: "as?", outcome: "hit", dispatch: "monomorphic" },
  { id: "as-miss-monomorphic", axis: "as?", outcome: "miss", dispatch: "monomorphic" },
  { id: "as-hit-polymorphic", axis: "as?", outcome: "hit", dispatch: "polymorphic" },
  { id: "as-miss-polymorphic", axis: "as?", outcome: "miss", dispatch: "polymorphic" },
  { id: "type-of-static", axis: "type of", subject: "static" },
  { id: "type-of-dynamic-concrete", axis: "type of", subject: "dynamic-concrete" },
  { id: "type-of-dynamic-existential", axis: "type of", subject: "dynamic-existential" },
  { id: "static-reachable-query", axis: "info of", subject: "static", metadata: "reachable", query: "executed" },
  { id: "dynamic-reachable-query", axis: "info of", subject: "dynamic", metadata: "reachable", query: "executed" },
  { id: "unreachable-type-stripped-control", axis: "info of", subject: "unreachable-type", metadata: "stripped", query: "not-executed", control: "unreachable" },
  { id: "reachable-Reflectable-conformance-without-query", axis: "info of", subject: "Reflectable-conformance", metadata: "reachable", query: "not-executed", control: "retention" },
  { id: "baseline-empty-loop", axis: "baseline", guard: "empty-loop" },
  { id: "baseline-dead-code-prevention", axis: "baseline", guard: "dead-code-prevention" },
]
if (JSON.stringify(benchmark.variants) !== JSON.stringify(benchmarkVariants)) fail("benchmark adversarial variants drifted")

if (!corpus.forbiddenSurface || !Array.isArray(corpus.forbiddenSurface.notProvided) || typeof corpus.forbiddenSurface.unavailableExamples !== "object" || Array.isArray(corpus.forbiddenSurface.unavailableExamples) || !Array.isArray(corpus.forbiddenSurface.userIdentifiersRemainLegal)) {
  fail("forbiddenSurface must separate unavailable built-ins from legal identifiers")
}
for (const spelling of [
  "std.reflect module",
  "compiler-owned reflect.* namespace",
  "typeof as a type query",
  "type(of:)",
  "TypeId.of<T>()",
]) {
  if (!corpus.forbiddenSurface.notProvided.includes(spelling)) fail(`forbidden surface is missing ${spelling}`)
  if (!Array.isArray(corpus.forbiddenSurface.unavailableExamples[spelling]) || corpus.forbiddenSurface.unavailableExamples[spelling].length === 0) {
    fail(`forbidden surface is missing an unavailable example for ${spelling}`)
  }
}
for (const identifier of ["reflect", "info", "of", "typeof"]) {
  if (!corpus.forbiddenSurface.userIdentifiersRemainLegal.includes(identifier)) fail(`contextual identifier allowance is missing ${identifier}`)
}

if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) fail("cases are empty")
const ids = new Set()
const expectedIds = new Set([
  "TQ-POS-is-exact-nominal",
  "TQ-POS-as-borrowed-existential",
  "TQ-POS-type-static-without-metadata",
  "TQ-POS-type-dynamic-borrowed",
  "TQ-POS-info-static-reflectable",
  "TQ-POS-info-dynamic-reflectable-existential",
  "TQ-POS-identity-invariants",
  "TQ-POS-metadata-id-invariants",
  "TQ-POS-contextual-identifiers",
  "TQ-ADV-type-namespace-homonym",
  "TQ-ADV-type-query-precedence",
  "TQ-ADV-type-query-parenthesized-subject",
  "TQ-NEG-is-no-dynamic-identity",
  "TQ-NEG-as-target-not-whole-composition",
  "TQ-NEG-as-borrow-escape",
  "TQ-NEG-query-missing-subject",
  "TQ-NEG-info-without-reflectable",
  "TQ-NEG-query-legacy-typeid",
  "TQ-ADV-cast-whitespace",
  "TQ-ADV-cast-chain-rejected",
  "TQ-NEG-legacy-reflect-surface",
  "TQ-NEG-legacy-reflect-import",
  "TQ-NEG-legacy-typeof",
  "TQ-NEG-legacy-typeof-function",
])
const diagnosticIds = new Set()
for (const [index, testCase] of corpus.cases.entries()) {
  const owner = `case ${index}`
  requireString(testCase.id, `${owner}.id`)
  if (!/^TQ-(?:POS|NEG|ADV)-[a-z0-9-]+$/u.test(testCase.id) || ids.has(testCase.id)) fail(`${owner} has an invalid or duplicate id`)
  ids.add(testCase.id)
  if (!["positive", "negative", "adversarial"].includes(testCase.kind)) fail(`${testCase.id} has an invalid kind`)
  requireString(testCase.fixture, `${testCase.id}.fixture`)
  const fixture = path.resolve(root, testCase.fixture)
  const relativeFixture = path.relative(root, fixture)
  if (!fs.existsSync(fixture) || relativeFixture.startsWith(`..${path.sep}`) || path.isAbsolute(relativeFixture)) fail(`${testCase.id} fixture escapes or does not exist`)
  if (relativeFixture.split(path.sep).includes("history") || relativeFixture.startsWith(`tooling${path.sep}tree-sitter-w${path.sep}src`)) fail(`${testCase.id} fixture uses forbidden generated/history material`)
  requireString(testCase.form, `${testCase.id}.form`)
  requireString(testCase.source, `${testCase.id}.source`)
  const fixtureText = fs.readFileSync(fixture, "utf8")
  if (!fixtureText.includes(testCase.source)) fail(`${testCase.id}.source is not present literally in ${testCase.fixture}`)
  if (testCase.pairedSource !== undefined) {
    requireString(testCase.pairedSource, `${testCase.id}.pairedSource`)
    if (!fixtureText.includes(testCase.pairedSource)) fail(`${testCase.id}.pairedSource is not present literally in ${testCase.fixture}`)
  }
  if (!testCase.expect || typeof testCase.expect !== "object" || Array.isArray(testCase.expect)) fail(`${testCase.id}.expect is invalid`)
  if (/W is implemented|compiler is ready|runtime is complete/iu.test(JSON.stringify(testCase))) fail(`${testCase.id} claims implementation`)

  const diagnostic = testCase.expect.diagnostic
  if (diagnostic !== undefined && diagnostic !== null) {
    if (!new Set(["W-TYPE-0124", "W-TYPE-0128", "W-TYPE-0130", "W-BORROW-0001", "W-PARSE-0020", "W-EXPR-0006"]).has(diagnostic)) fail(`${testCase.id} has an invalid diagnostic ${diagnostic}`)
    diagnosticIds.add(diagnostic)
  }
  if (testCase.kind === "positive" && diagnostic !== undefined && diagnostic !== null) fail(`${testCase.id} positive case has a diagnostic`)
  if (testCase.kind !== "positive" && (testCase.form === "legacy" || testCase.form === "legacy-query")) {
    const unavailableExamples = Object.values(corpus.forbiddenSurface.unavailableExamples).flat()
    if (!unavailableExamples.includes(testCase.source)) fail(`${testCase.id} does not name an unavailable core example`)
    requireString(testCase.resolutionContext, `${testCase.id}.resolutionContext`)
    if (testCase.form === "legacy" && (testCase.expect.status !== "rejected-legacy-surface" || testCase.expect.checkerClaim !== false)) fail(`${testCase.id} must remain a non-implementation legacy rejection oracle`)
    if (testCase.form === "legacy-query" && testCase.expect.checkerClaim !== false) fail(`${testCase.id} must remain a non-implementation query rejection oracle`)
  }
  if (testCase.form === "as?" && testCase.kind === "positive") {
    if (!/^ref [A-Za-z_][A-Za-z0-9_]*\?$/u.test(testCase.expect.resultType ?? "") || testCase.expect.sourceEvaluation !== "once") fail(`${testCase.id} lost borrowed result/evaluation contract`)
    if (testCase.expect.sourceKind !== "borrowed-protocol-existential" || testCase.expect.targetComposition !== "whole") fail(`${testCase.id} lost existential/whole-composition contract`)
    if (JSON.stringify(testCase.expect.effects) !== JSON.stringify(["no-copy", "no-move", "no-retain", "no-allocation", "no-repack"])) fail(`${testCase.id} lost no-repack effects`)
  }
}

for (const expected of expectedIds) if (!ids.has(expected)) fail(`missing required case ${expected}`)
for (const diagnostic of ["W-TYPE-0130", "W-TYPE-0124", "W-TYPE-0128", "W-BORROW-0001", "W-PARSE-0020", "W-EXPR-0006"]) {
  if (!diagnosticIds.has(diagnostic)) fail(`missing diagnostic witness ${diagnostic}`)
}

const catalog = JSON.parse(read("tooling/diagnostic-catalog.json"))
const reservedTypeDiagnostics = {
  "W-TYPE-0124": { phase: "semantic.type", requiredFacts: ["operation", "sourceType", "targetType", "reason"], labelRoles: ["type-subject", "target-type"] },
  "W-TYPE-0128": { phase: "semantic.type", requiredFacts: ["operation", "subjectType", "reason"], labelRoles: ["type-subject"] },
  "W-TYPE-0130": { phase: "semantic.type", requiredFacts: ["operation", "sourceIdentity", "targetType"], labelRoles: ["type-subject"] },
}
for (const [code, shape] of Object.entries(reservedTypeDiagnostics)) {
  const entry = catalog.codes?.find((candidate) => candidate.code === code)
  if (!entry || entry.state !== "reserved" || entry.phase !== shape.phase || entry.defaultSeverity !== "error") fail(`${code} must be a reserved semantic.type diagnostic`)
  if (JSON.stringify(Object.keys(entry.requiredFacts ?? {}).sort()) !== JSON.stringify([...shape.requiredFacts].sort())) fail(`${code} requiredFacts drifted`)
  if (JSON.stringify(Object.keys(entry.labelRoles ?? {}).sort()) !== JSON.stringify([...shape.labelRoles].sort())) fail(`${code} labelRoles drifted`)
}
for (const removed of ["W-TYPE-0125", "W-TYPE-0126", "W-TYPE-0129"]) {
  if (catalog.codes?.some((entry) => entry.code === removed)) fail(`${removed} must not be a catalog code`)
  if (JSON.stringify(corpus).includes(removed) || read("DESIGN.md").includes(removed)) fail(`${removed} must not be a current contract code`)
}

const identityInvariant = corpus.cases.find((testCase) => testCase.id === "TQ-POS-identity-invariants")
if (identityInvariant?.expect?.identityEquivalence !== "value is T iff type of value == type of T" || identityInvariant.expect.castEquivalence !== "value as? T is some(ref payload) iff value is T; otherwise none" || identityInvariant.expect.accessorsAndUserCode !== "not-executed") fail("identity invariants drifted")
const metadataInvariant = corpus.cases.find((testCase) => testCase.id === "TQ-POS-metadata-id-invariants")
if (metadataInvariant?.expect?.staticIdEquivalence !== "(info of T).id == type of T" || metadataInvariant.expect.dynamicIdEquivalence !== "(info of value).id == type of value" || metadataInvariant.expect.accessorsAndUserCode !== "not-executed") fail("metadata id invariants drifted")

const design = read("DESIGN.md")
for (const spelling of ["W-1492", "W-TYPE-0130", "W-TYPE-0124", "W-TYPE-0128", "W-BORROW-0001", "W-PARSE-0020", "W-EXPR-0006", "type namespace first", "type of (value ==", "value is T iff type of value == type of T", "(info of T).id == type of T"]) {
  if (!design.includes(spelling)) fail(`DESIGN is missing ${spelling}`)
}
for (const removed of ["W-TYPE-0125", "W-TYPE-0126", "W-TYPE-0127", "W-TYPE-0129"]) if (design.includes(removed)) fail(`DESIGN retains removed diagnostic ${removed}`)
const grammar = read("tooling/tree-sitter-w/grammar.js")
for (const spelling of ["type_query_expression", "conditional_cast_expression", "token.immediate(\"?\")"]) {
  if (!grammar.includes(spelling)) fail(`grammar is missing ${spelling}`)
}
const keywordBlock = grammar.match(/const OTHER_KEYWORDS = \[(.*?)\];/su)?.[1] ?? ""
if (keywordBlock.includes('"of"') || keywordBlock.includes('"info"')) fail("contextual query words became global grammar keywords")
const portal = read("portal/w-syntax.js")
const portalKeywordBlock = portal.match(/const keywords = new Set\(\[(.*?)\]\);/su)?.[1] ?? ""
if (portalKeywordBlock.includes('"of"') || portalKeywordBlock.includes('"info"')) fail("contextual query words became global portal keywords")
const reflection = read("reference/last-light/reflection.w")
if (/reflect\.TypeId|import reflect from std|typeof value|type\(of:/u.test(reflection)) fail("Last Light reflection fixture retains legacy surface")

console.log(`type-query cases: ok (${corpus.cases.length} cases; design oracle only; benchmark deferred)`)
