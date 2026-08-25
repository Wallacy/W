import { mkdtemp, rm } from "node:fs/promises"
import { createHash } from "node:crypto"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const corpus = await Bun.file(resolve(import.meta.dir, "formatter-cases.json")).json()
const selectedIds = [
  "F0-repeat-array-semicolon",
  "F0-module-run-entry",
  "F0-canonical-bytes",
  "F0-value-block-boundary",
  "F0-postfix-statement-boundaries",
  "F0-labeled-repeat-loop",
  "F0-binary-and-postfix-wrapping",
  "F0-comment-attachment",
  "F0-declaration-order",
  "F0-compact-declarations",
  "F0-multiline-signature-and-call",
  "F0-parameter-contract-placement",
  "F0-labeled-control",
  "F0-contextual-named-parameter",
  "F0-consuming-receiver-grouping",
  "F0-const-call-parameter",
  "F0-effect-prefix-order",
  "F0-structured-transaction",
  "F0-language-lock",
  "F0-borrows-clause-source-order",
  "F0-optional-label-slots",
  "F0-contracts-and-source-order",
  "F0-enum-subset-switch",
  "F0-allocator-anonymous-contextual-call",
  "F0-allocator-named-override",
  "F0-spawn-domain-slots",
  "F0-explicit-capture-closure",
  "F0-opaque-foreign-body",
]

const CST = Object.freeze({
  // These ordinals are append-only in include/w_seed_parser.h. The witness
  // assertions below exercise every new kind and fail if the mapping drifts.
  DOCUMENT: 0,
  FUNCTION: 2,
  RETURN: 9,
  RETURN_TYPE: 5,
  PARAMETER_LIST: 3,
  TRIVIA: 24,
  BLOCK: 7,
  LET: 8,
  LABEL: 13,
  BREAK: 14,
  CONTINUE: 15,
  EXPRESSION: 17,
  TYPE: 18,
  WORD: 25,
  PUNCTUATION: 27,
  ARRAY: 20,
  ERROR: 21,
  MISSING: 22,
  IMPORT: 31,
  IMPORT_ITEM: 32,
  STRUCT: 33,
  FIELD: 34,
  TEST: 35,
  EXPECT: 36,
  ARGUMENT: 37,
  FOR: 38,
  TRANSACTION: 39,
  COMMIT: 40,
  BORROW_CLAUSE: 41,
  BORROW_PAIR: 42,
  SLOT_REF: 43,
  LOCK: 44,
  TYPE_DECLARATION: 45,
  ALIAS_DECLARATION: 46,
  GENERIC_PARAMETERS: 47,
  GENERIC_PARAMETER: 48,
  CONTRACT_ENVELOPE: 49,
  SWITCH_EXPRESSION: 50,
  SWITCH_ARM: 51,
  ALLOCATOR_BLOCK: 52,
  TUPLE_TYPE: 53,
  TUPLE_EXPRESSION: 54,
  SPAWN_STATEMENT: 55,
  FUNCTION_TYPE: 56,
  FUNCTION_TYPE_PARAMETERS: 57,
  CLOSURE_EXPRESSION: 58,
  CLOSURE_PARAMETERS: 59,
  CLOSURE_PARAMETER: 60,
  CAPTURE_EXPRESSION: 61,
  CAPTURE_ITEM: 62,
  FOREIGN_BODY: 29,
  FOREIGN_LANGUAGE_TAG: 63,
  FOREIGN_BODY_OWNER: 64,
  ENUM: 66,
  ENUM_CASE: 67,
  ENUM_CASE_PARAMETER: 68,
  ENUM_PATTERN: 69,
  WILDCARD_PATTERN: 70,
  LITERAL_PATTERN: 71,
})

const ISSUE = Object.freeze({
  UNEXPECTED_TOKEN: 1,
  MISSING_OWNER_CLOSE: 2,
  UNSUPPORTED_FORM: 6,
  FOREIGN_UNSUPPORTED: 9,
  FOREIGN_SCANNER: 10,
})

function fail(message) {
  throw new Error(`seed parser: ${message}`)
}

function run(command, args) {
  const execution = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (execution.exitCode !== 0) {
    fail(`${command} ${args.join(" ")} failed: ${execution.stderr.toString().trim()}`)
  }
  return execution
}

function inputText(input, id) {
  if (!input || !Array.isArray(input.lines) || input.lines.length === 0) {
    fail(`${id} has no input lines`)
  }
  if (input.lines.some((line) => typeof line !== "string" || line.includes("\n"))) {
    fail(`${id} input lines are invalid`)
  }
  const newline = input.newline === "crlf" ? "\r\n" : "\n"
  let source = input.lines.join(newline)
  if (input.finalNewline !== false) source += newline
  if (input.bom === true) source = `\uFEFF${source}`
  return Buffer.from(source, "utf8")
}

function outputText(output, id) {
  if (!Array.isArray(output) || output.length === 0) fail(`${id} has no output`)
  if (output.some((line) => typeof line !== "string" || /[\r\n]/u.test(line))) {
    fail(`${id} output lines are invalid`)
  }
  return Buffer.from(`${output.join("\n")}\n`, "utf8")
}

function parseProbe(text, bytes, label) {
  const lines = text.trim().split(/\r?\n/u).filter(Boolean)
  if (lines.length === 0) fail(`${label} produced no probe output`)
  const resultMatch =
    /^RESULT status=(\w+) nodes=(\d+) leaves=(\d+) issues=(\d+) consumed=(\d+) root=(\d+) length=(\d+)$/u.exec(lines[0])
  if (!resultMatch) fail(`${label} has invalid RESULT line`)
  const result = {
    status: resultMatch[1],
    nodeCount: Number(resultMatch[2]),
    leafCount: Number(resultMatch[3]),
    issueCount: Number(resultMatch[4]),
    consumed: Number(resultMatch[5]),
    root: Number(resultMatch[6]),
    length: Number(resultMatch[7]),
  }
  if (result.length !== bytes.length || result.consumed !== bytes.length) {
    fail(`${label} source length/consumed mismatch`)
  }
  const nodes = []
  const issues = []
  for (const line of lines.slice(1)) {
    const nodeMatch =
      /^NODE index=(\d+) kind=(\d+) flags=(\d+) start=(\d+) end=(\d+) first=(\d+) next=(\d+)$/u.exec(line)
    if (nodeMatch) {
      nodes.push({
        index: Number(nodeMatch[1]),
        kind: Number(nodeMatch[2]),
        flags: Number(nodeMatch[3]),
        start: Number(nodeMatch[4]),
        end: Number(nodeMatch[5]),
        first: Number(nodeMatch[6]),
        next: Number(nodeMatch[7]),
      })
      continue
    }
    const issueMatch =
      /^ISSUE index=(\d+) kind=(\d+) start=(\d+) end=(\d+) actual=(\d+) expected=(\d+)$/u.exec(line)
    if (issueMatch) {
      issues.push({
        index: Number(issueMatch[1]),
        kind: Number(issueMatch[2]),
        start: Number(issueMatch[3]),
        end: Number(issueMatch[4]),
        actual: Number(issueMatch[5]),
        expected: Number(issueMatch[6]),
      })
      continue
    }
    fail(`${label} has invalid probe line ${JSON.stringify(line)}`)
  }
  if (nodes.length !== result.nodeCount || issues.length !== result.issueCount) {
    fail(`${label} probe counts do not match RESULT`)
  }
  nodes.sort((left, right) => left.index - right.index)
  if (nodes.some((node, index) => node.index !== index)) fail(`${label} node indices are not dense`)
  if (result.root !== 0 || nodes[0]?.kind !== 0 || nodes[0]?.start !== 0 || nodes[0]?.end !== bytes.length) {
    fail(`${label} document root span/kind is invalid`)
  }
  const leaves = nodes.filter((node) => (node.flags & 1) !== 0)
  let cursor = 0
  for (const leaf of leaves) {
    if (leaf.start !== cursor || leaf.end < leaf.start || leaf.end > bytes.length) {
      fail(`${label} leaf partition gap/overlap at ${leaf.start}:${leaf.end} after ${cursor}`)
    }
    cursor = leaf.end
  }
  if (cursor !== bytes.length || leaves.length !== result.leafCount) {
    fail(`${label} leaf partition ends at ${cursor}, expected ${bytes.length}`)
  }
  for (const parent of nodes) {
    let child = parent.first
    let previousEnd = parent.start
    let guard = 0
    while (child !== 4294967295) {
      if (child >= nodes.length) fail(`${label} child index is outside node array`)
      const childNode = nodes[child]
      if (
        childNode.start < parent.start ||
        childNode.end > parent.end ||
        childNode.end < childNode.start ||
        childNode.start < previousEnd
      ) {
        fail(`${label} child containment/order failed for node ${parent.index}`)
      }
      previousEnd = childNode.end
      child = childNode.next
      guard += 1
      if (guard > nodes.length) fail(`${label} sibling cycle`)
    }
  }
  const signature = nodes
    .map((node) => `${node.kind}:${node.flags}:${node.start}:${node.end}:${node.first}:${node.next}`)
    .join("|")
  return { result, nodes, issues, signature }
}

function childrenOf(parsed, parent) {
  const children = []
  let child = parsed.nodes[parent]?.first ?? 4294967295
  let guard = 0
  while (child !== 4294967295) {
    if (child >= parsed.nodes.length) fail(`node ${parent} child is outside parsed CST`)
    children.push(parsed.nodes[child])
    child = parsed.nodes[child].next
    guard += 1
    if (guard > parsed.nodes.length) fail(`node ${parent} has a sibling cycle`)
  }
  return children
}

function descendants(parsed, parent) {
  const result = []
  const visit = (index) => {
    for (const child of childrenOf(parsed, index)) {
      result.push(child)
      visit(child.index)
    }
  }
  visit(parent)
  return result
}

function nodeText(parsed, bytes, node) {
  return bytes.subarray(node.start, node.end).toString("utf8")
}

function directKind(parsed, parent, kind) {
  return childrenOf(parsed, parent).filter((child) => child.kind === kind)
}

function assertClean(parsed, label) {
  if (parsed.result.status !== "complete" || parsed.result.issueCount !== 0) {
    fail(`${label} is not clean complete (${parsed.result.status}, ${parsed.result.issueCount} issues)`)
  }
  if (parsed.nodes.some((node) => node.kind === CST.ERROR || node.kind === CST.MISSING ||
      (node.flags & (1 << 2 | 1 << 3)) !== 0)) {
    fail(`${label} contains ERROR/MISSING CST nodes`)
  }
}

function assertEnumWitness(parsed, bytes, label, expectedCases, expectedParameters, labels = []) {
  assertClean(parsed, label)
  const enums = parsed.nodes.filter((node) => node.kind === CST.ENUM)
  const cases = parsed.nodes.filter((node) => node.kind === CST.ENUM_CASE)
  const parameters = parsed.nodes.filter((node) => node.kind === CST.ENUM_CASE_PARAMETER)
  if (enums.length !== 1 || cases.length !== expectedCases ||
      parameters.length !== expectedParameters) {
    fail(`${label} enum/case/payload counts are ${enums.length}/${cases.length}/${parameters.length}`)
  }
  if (directKind(parsed, enums[0].index, CST.ENUM_CASE).length !== expectedCases) {
    fail(`${label} enum does not own ordered direct cases`)
  }
  const orderedCases = directKind(parsed, enums[0].index, CST.ENUM_CASE)
  if (orderedCases.length !== cases.length ||
      orderedCases.some((node, index) => node.index !== cases[index].index)) {
    fail(`${label} enum cases are not preserved in source order`)
  }
  const caseNames = orderedCases.map((node) => nodeText(parsed, bytes, node).trim().split(/\s|\(/u)[0])
  if (labels.length !== 0 &&
      (caseNames.length !== labels.length ||
       labels.some((name, index) => caseNames[index] !== name))) {
    fail(`${label} case names are not source-ordered: ${caseNames.join(", ")}`)
  }
  for (const parameter of parameters) {
    if (directKind(parsed, parameter.index, CST.TYPE).length !== 1) {
      fail(`${label} payload parameter does not own one TYPE`)
    }
  }
}

function assertForeignIsland(parsed, bytes, label, expectedBody) {
  assertClean(parsed, label)
  const tags = parsed.nodes.filter((node) => node.kind === CST.FOREIGN_LANGUAGE_TAG)
  const owners = parsed.nodes.filter((node) => node.kind === CST.FOREIGN_BODY_OWNER)
  const bodies = parsed.nodes.filter((node) => node.kind === CST.FOREIGN_BODY)
  if (tags.length !== 1 || owners.length !== 1 || bodies.length !== 1) {
    fail(`${label} foreign tag/owner/body counts are incomplete`)
  }
  if ((bodies[0].flags & 1) === 0 || nodeText(parsed, bytes, bodies[0]) !== expectedBody) {
    fail(`${label} foreign body leaf is not source-exact`)
  }
  if (directKind(parsed, owners[0].index, CST.FOREIGN_BODY).length !== 1) {
    fail(`${label} foreign owner does not contain its raw body leaf`)
  }
}

function assertClosureCapture(parsed, bytes, label) {
  assertClean(parsed, label)
  const functions = parsed.nodes.filter((node) => node.kind === CST.FUNCTION)
  if (functions.length !== 1) fail(`${label} does not contain one FUNCTION`)
  const returnTypes = directKind(parsed, functions[0].index, CST.RETURN_TYPE)
  if (returnTypes.length !== 1) fail(`${label} does not contain one RETURN_TYPE`)
  const functionTypes = parsed.nodes.filter((node) => node.kind === CST.FUNCTION_TYPE)
  if (functionTypes.length !== 1 ||
      !/^\s*(some|any)\s+(mut|take)\s*fn\s*\(\)\s*:\s*usize\s*$/u
        .test(nodeText(parsed, bytes, functionTypes[0]))) {
    fail(`${label} callable return type shape drifted`)
  }
  if (directKind(parsed, functionTypes[0].index, CST.FUNCTION_TYPE_PARAMETERS).length !== 1) {
    fail(`${label} callable type parameters owner is missing`)
  }
  const closures = parsed.nodes.filter((node) => node.kind === CST.CLOSURE_EXPRESSION)
  const captures = parsed.nodes.filter((node) => node.kind === CST.CAPTURE_EXPRESSION)
  const items = parsed.nodes.filter((node) => node.kind === CST.CAPTURE_ITEM)
  if (closures.length !== 1 || captures.length !== 1 || items.length !== 1) {
    fail(`${label} closure/capture owner counts are incomplete`)
  }
  if (nodeText(parsed, bytes, items[0]) !== "take next") {
    fail(`${label} capture item is not source-shaped`)
  }
  if (directKind(parsed, captures[0].index, CST.CLOSURE_EXPRESSION).length !== 1) {
    fail(`${label} CAPTURE_EXPRESSION does not own CLOSURE_EXPRESSION`)
  }
  const closure = closures[0]
  if (directKind(parsed, closure.index, CST.CLOSURE_PARAMETERS).length !== 1 ||
      directKind(parsed, closure.index, CST.BLOCK).length !== 1) {
    fail(`${label} closure parameter/body owners are incomplete`)
  }
}

function assertExplicitCapture(parsed, bytes, label, expectedItems, expectedParams) {
  assertClean(parsed, label)
  const captures = parsed.nodes.filter((node) => node.kind === CST.CAPTURE_EXPRESSION)
  const closures = parsed.nodes.filter((node) => node.kind === CST.CLOSURE_EXPRESSION)
  if (captures.length !== 1 || closures.length !== 1) {
    fail(`${label} explicit capture/closure owners are incomplete`)
  }
  if (parsed.nodes.filter((node) => node.kind === CST.CAPTURE_ITEM).length !== expectedItems ||
      parsed.nodes.filter((node) => node.kind === CST.CLOSURE_PARAMETER).length !== expectedParams) {
    fail(`${label} capture/closure parameter counts drifted`)
  }
  if (directKind(parsed, captures[0].index, CST.CLOSURE_EXPRESSION).length !== 1 ||
      directKind(parsed, closures[0].index, CST.CLOSURE_PARAMETERS).length !== 1) {
    fail(`${label} explicit capture ownership is not source-shaped`)
  }
  if (!nodeText(parsed, bytes, captures[0]).startsWith("<[")) {
    fail(`${label} capture owner does not preserve its opener`)
  }
}

function assertExplicitCaptureRecovery(parsed, bytes, label, options) {
  if (parsed.result.status !== "recovered" || parsed.result.issueCount === 0 ||
      parsed.issues[0]?.kind !== options.issue) {
    fail(`${label} recovery status/first issue drifted`)
  }
  const captures = parsed.nodes.filter((node) => node.kind === CST.CAPTURE_EXPRESSION)
  if (captures.length !== 1) fail(`${label} lost CAPTURE_EXPRESSION owner`)
  const missing = descendants(parsed, captures[0].index)
    .filter((node) => node.kind === CST.MISSING)
  if (Boolean(options.missing) !== (missing.length !== 0)) {
    fail(`${label} CAPTURE_EXPRESSION MISSING shape drifted`)
  }
  if (options.following) {
    const functions = parsed.nodes.filter((node) => node.kind === CST.FUNCTION)
    const blocks = functions.length === 1 ? directKind(parsed, functions[0].index, CST.BLOCK) : []
    const returns = blocks.length === 1 ? directKind(parsed, blocks[0].index, CST.RETURN) : []
    if (!returns.some((node) => nodeText(parsed, bytes, node) === "return next")) {
      fail(`${label} swallowed following return statement`)
    }
  }
}

function assertCallableType(parsed, bytes, label, options) {
  assertClean(parsed, label)
  const functionTypes = parsed.nodes.filter((node) => node.kind === CST.FUNCTION_TYPE)
  if (functionTypes.length !== 1 ||
      !options.pattern.test(nodeText(parsed, bytes, functionTypes[0]))) {
    fail(`${label} callable type text/owner drifted`)
  }
  if (directKind(parsed, functionTypes[0].index, CST.FUNCTION_TYPE_PARAMETERS).length !== 1) {
    fail(`${label} callable type parameter owner is missing`)
  }
  if (options.borrow && !descendants(parsed, functionTypes[0].index)
      .some((node) => node.kind === CST.BORROW_CLAUSE)) {
    fail(`${label} callable type BORROW_CLAUSE is missing`)
  }
}

function assertCallableTypeRecovery(parsed, bytes, label) {
  if (parsed.result.status !== "recovered" || parsed.result.issueCount === 0 ||
      parsed.issues[0]?.kind !== ISSUE.UNEXPECTED_TOKEN) {
    fail(`${label} callable type recovery status/first issue drifted`)
  }
  const functionTypes = parsed.nodes.filter((node) => node.kind === CST.FUNCTION_TYPE)
  if (functionTypes.length !== 1 ||
      !descendants(parsed, functionTypes[0].index)
        .some((node) => node.kind === CST.MISSING)) {
    fail(`${label} callable type recovery lost FUNCTION_TYPE/MISSING ownership`)
  }
}

function assertRepeatArray(parsed, bytes, label) {
  assertClean(parsed, label)
  const arrays = parsed.nodes.filter((node) => node.kind === CST.ARRAY)
  if (arrays.length !== 1) fail(`${label} does not contain one ARRAY node`)
  const expressions = directKind(parsed, arrays[0].index, CST.EXPRESSION)
  if (expressions.length !== 2) {
    fail(`${label} repeat ARRAY does not own two direct EXPRESSION children`)
  }
  if (nodeText(parsed, bytes, expressions[0]) !== "0" ||
      nodeText(parsed, bytes, expressions[1]) !== "16") {
    fail(`${label} repeat ARRAY expression spans are not source-shaped`)
  }
}

function assertSpawnDomainSlots(parsed, bytes, label, options = {}) {
  assertClean(parsed, label)
  const functions = parsed.nodes.filter((node) => node.kind === CST.FUNCTION)
  if (functions.length !== 1) fail(`${label} does not contain one FUNCTION node`)
  const functionNode = functions[0]
  const returnTypes = directKind(parsed, functionNode.index, CST.RETURN_TYPE)
  if (returnTypes.length !== 1) fail(`${label} does not contain one RETURN_TYPE`)
  const tupleTypes = parsed.nodes.filter((node) => node.kind === CST.TUPLE_TYPE)
  if (tupleTypes.length !== 1) fail(`${label} does not contain one TUPLE_TYPE`)
  const tupleType = tupleTypes[0]
  const typeItems = directKind(parsed, tupleType.index, CST.TYPE)
  const expectedTypeItems = options.typeItems ?? ["Int", "Int"]
  if (typeItems.length !== expectedTypeItems.length ||
      typeItems.some((node, index) => nodeText(parsed, bytes, node) !== expectedTypeItems[index])) {
    fail(`${label} TUPLE_TYPE item shape mismatch`)
  }
  if (tupleType.start < returnTypes[0].start || tupleType.end > returnTypes[0].end) {
    fail(`${label} TUPLE_TYPE is outside RETURN_TYPE`)
  }

  const blocks = directKind(parsed, functionNode.index, CST.BLOCK)
  if (blocks.length !== 1) fail(`${label} does not contain one function BLOCK`)
  const spawns = directKind(parsed, blocks[0].index, CST.SPAWN_STATEMENT)
    .sort((left, right) => left.start - right.start)
  if (spawns.length !== 2) fail(`${label} does not contain two direct SPAWN_STATEMENT nodes`)
  const envelopeTexts = []
  for (const spawn of spawns) {
    const words = directKind(parsed, spawn.index, CST.WORD)
    if (!words.some((node) => nodeText(parsed, bytes, node) === "spawn")) {
      fail(`${label} SPAWN_STATEMENT does not own spawn keyword`)
    }
    const envelopes = directKind(parsed, spawn.index, CST.CONTRACT_ENVELOPE)
    const lets = directKind(parsed, spawn.index, CST.LET)
    if (envelopes.length !== 1 || lets.length !== 1) {
      fail(`${label} SPAWN_STATEMENT owners are not CONTRACT_ENVELOPE + LET`)
    }
    envelopeTexts.push(nodeText(parsed, bytes, envelopes[0]))
    if (spawn.start !== words[0].start || spawn.end < lets[0].end) {
      fail(`${label} SPAWN_STATEMENT span is not source-shaped`)
    }
  }
  const expectedEnvelopes = options.envelopes ?? ["<.compute>", null]
  const namedSecond = options.namedSecond !== false
  if (envelopeTexts[0] !== expectedEnvelopes[0] ||
      (namedSecond ? !/^<domain:\s*\.compute>$/u.test(envelopeTexts[1]) :
       envelopeTexts[1] !== expectedEnvelopes[1])) {
    fail(`${label} domain envelopes are not source-shaped`)
  }

  const tupleExpressions = parsed.nodes.filter((node) => node.kind === CST.TUPLE_EXPRESSION)
  if (tupleExpressions.length !== 1) {
    fail(`${label} does not contain one TUPLE_EXPRESSION`)
  }
  const expressionItems = directKind(parsed, tupleExpressions[0].index, CST.EXPRESSION)
  const expectedExpressionItems = options.expressionItems ?? ["port", "starboard"]
  if (expressionItems.length !== expectedExpressionItems.length ||
      expressionItems.some((node, index) =>
        nodeText(parsed, bytes, node) !== expectedExpressionItems[index])) {
    fail(`${label} TUPLE_EXPRESSION item shape mismatch`)
  }
}

function assertSpawnRecovery(parsed, bytes, label, options) {
  if (parsed.result.status !== "recovered" || parsed.result.issueCount < 1) {
    fail(`${label} is not recovered with an issue`)
  }
  if (parsed.issues[0]?.kind !== options.issue) {
    fail(`${label} first issue ${parsed.issues[0]?.kind} != ${options.issue}`)
  }
  const functions = parsed.nodes.filter((node) => node.kind === CST.FUNCTION)
  if (functions.length !== 1) fail(`${label} does not contain one FUNCTION node`)
  const blocks = directKind(parsed, functions[0].index, CST.BLOCK)
  if (blocks.length !== 1) fail(`${label} does not contain one function BLOCK`)
  const spawns = directKind(parsed, blocks[0].index, CST.SPAWN_STATEMENT)
  if (spawns.length !== 1) fail(`${label} does not preserve one direct SPAWN_STATEMENT`)
  const spawn = spawns[0]
  const words = directKind(parsed, spawn.index, CST.WORD)
  if (!words.some((node) => nodeText(parsed, bytes, node) === "spawn")) {
    fail(`${label} SPAWN_STATEMENT lost its spawn keyword`)
  }
  const envelopes = directKind(parsed, spawn.index, CST.CONTRACT_ENVELOPE)
  if (envelopes.length !== 1 || envelopes[0].start < spawn.start ||
      envelopes[0].end > spawn.end) {
    fail(`${label} SPAWN_STATEMENT lost its CONTRACT_ENVELOPE owner`)
  }
  const envelopeMissing = directKind(parsed, envelopes[0].index, CST.MISSING)
  if (Boolean(options.envelopeMissing) !== (envelopeMissing.length === 1)) {
    fail(`${label} CONTRACT_ENVELOPE MISSING shape drifted`)
  }
  const lets = directKind(parsed, spawn.index, CST.LET)
  if (Boolean(options.let) !== (lets.length === 1)) {
    fail(`${label} SPAWN_STATEMENT LET ownership drifted`)
  }
  if (options.let) {
    const missing = directKind(parsed, lets[0].index, CST.MISSING)
    if (Boolean(options.letMissing) !== (missing.length === 1)) {
      fail(`${label} LET MISSING shape drifted`)
    }
  } else {
    const missing = directKind(parsed, spawn.index, CST.MISSING)
    if (Boolean(options.spawnMissing) !== (missing.length === 1)) {
      fail(`${label} SPAWN MISSING shape drifted`)
    }
  }
}

function assertSpawnSemicolonBoundary(parsed, bytes, label) {
  assertClean(parsed, label)
  const functions = parsed.nodes.filter((node) => node.kind === CST.FUNCTION)
  if (functions.length !== 1) fail(`${label} does not contain one FUNCTION node`)
  const blocks = directKind(parsed, functions[0].index, CST.BLOCK)
  if (blocks.length !== 1) fail(`${label} does not contain one function BLOCK`)
  const spawns = directKind(parsed, blocks[0].index, CST.SPAWN_STATEMENT)
  if (spawns.length !== 1) fail(`${label} does not contain one direct SPAWN_STATEMENT`)
  const spawn = spawns[0]
  const lets = directKind(parsed, spawn.index, CST.LET)
  if (lets.length !== 1) fail(`${label} SPAWN_STATEMENT does not own one LET`)
  const semicolon = directKind(parsed, lets[0].index, CST.PUNCTUATION)
    .find((node) => nodeText(parsed, bytes, node) === ";")
  if (!semicolon) fail(`${label} SPAWN LET does not own its semicolon`)
  const followingLets = directKind(parsed, blocks[0].index, CST.LET)
  if (followingLets.length !== 1 ||
      nodeText(parsed, bytes, followingLets[0]) !== "let after=next()") {
    fail(`${label} did not continue with direct sibling let after`)
  }
  if (spawn.end > followingLets[0].start) {
    fail(`${label} SPAWN/LET sibling spans overlap`)
  }
}

function assertAsyncFunction(parsed, bytes, label, exported = false) {
  assertClean(parsed, label)
  const functions = parsed.nodes.filter((node) => node.kind === CST.FUNCTION)
  if (functions.length !== 1) fail(`${label} does not contain one FUNCTION node`)
  const functionNode = functions[0]
  const words = directKind(parsed, functionNode.index, CST.WORD)
  const asyncWord = words.find((node) => nodeText(parsed, bytes, node) === "async")
  if (!asyncWord || (asyncWord.flags & 1) === 0) {
    fail(`${label} FUNCTION does not own raw async WORD directly`)
  }
  if (functionNode.start !== (exported ? bytes.indexOf(Buffer.from("export")) : bytes.indexOf(Buffer.from("async")))) {
    fail(`${label} FUNCTION does not start at its declaration prefix`)
  }
  const exportWord = words.find((node) => nodeText(parsed, bytes, node) === "export")
  if (exported !== Boolean(exportWord) || (exportWord && (exportWord.flags & 1) === 0)) {
    fail(`${label} export prefix shape is incorrect`)
  }
  if (directKind(parsed, functionNode.index, CST.ARGUMENT).length !== 0) {
    fail(`${label} FUNCTION has unexpected direct ARGUMENT node`)
  }
  if (directKind(parsed, functionNode.index, CST.PARAMETER_LIST).length !== 1 ||
      directKind(parsed, functionNode.index, CST.RETURN_TYPE).length !== 1 ||
      directKind(parsed, functionNode.index, CST.BLOCK).length !== 1) {
    fail(`${label} FUNCTION parameter/return/block owners are incomplete`)
  }
  if (!words.some((node) => nodeText(parsed, bytes, node) === "throws")) {
    fail(`${label} FUNCTION does not preserve raw throws WORD`)
  }
  const block = directKind(parsed, functionNode.index, CST.BLOCK)[0]
  if (functionNode.end !== block.end) fail(`${label} FUNCTION span does not end at BLOCK`)
  const returnNode = directKind(parsed, block.index, CST.RETURN)[0]
  const expression = returnNode && directKind(parsed, returnNode.index, CST.EXPRESSION)[0]
  if (!expression) fail(`${label} return statement does not own EXPRESSION`)
  const effectWords = descendants(parsed, expression.index)
    .filter((node) => node.kind === CST.WORD)
    .map((node) => nodeText(parsed, bytes, node))
  if (!effectWords.includes("try") || !effectWords.includes("await")) {
    fail(`${label} return EXPRESSION does not preserve try/await leaves`)
  }
}

function assertAsyncTrivia(parsed, bytes, label) {
  assertClean(parsed, label)
  const functions = parsed.nodes.filter((node) => node.kind === CST.FUNCTION)
  if (functions.length !== 1) fail(`${label} does not contain one FUNCTION node`)
  const functionNode = functions[0]
  if (functionNode.start !== 0 ||
      !directKind(parsed, functionNode.index, CST.WORD)
        .some((node) => nodeText(parsed, bytes, node) === "export") ||
      !directKind(parsed, functionNode.index, CST.WORD)
        .some((node) => nodeText(parsed, bytes, node) === "async")) {
    fail(`${label} declaration prefix is not directly owned by FUNCTION`)
  }
  const trivia = directKind(parsed, functionNode.index, CST.TRIVIA)
  if (!trivia.some((node) => nodeText(parsed, bytes, node) === "/*a*/") ||
      !trivia.some((node) => nodeText(parsed, bytes, node) === "/*b*/")) {
    fail(`${label} comments/trivia are not directly owned by FUNCTION`)
  }
}

function assertStructuredTransaction(parsed, bytes, label) {
  assertClean(parsed, label)
  const transactions = parsed.nodes.filter((node) => node.kind === CST.TRANSACTION)
  const commits = parsed.nodes.filter((node) => node.kind === CST.COMMIT)
  if (transactions.length !== 1 || commits.length !== 1) {
    fail(`${label} does not contain one transaction and one commit`)
  }
  const transaction = transactions[0]
  const transactionWords = directKind(parsed, transaction.index, CST.WORD)
    .map((node) => nodeText(parsed, bytes, node))
  if (!transactionWords.includes("transaction") || !transactionWords.includes("tx")) {
    fail(`${label} transaction does not preserve keyword and binding leaves`)
  }
  const providerExpressions = directKind(parsed, transaction.index, CST.EXPRESSION)
  if (providerExpressions.length !== 1 ||
      nodeText(parsed, bytes, providerExpressions[0]).trim() !== "tableLedger") {
    fail(`${label} transaction provider expression is not source-shaped`)
  }
  const transactionBlocks = directKind(parsed, transaction.index, CST.BLOCK)
  if (transactionBlocks.length !== 1) {
    fail(`${label} transaction does not own one BLOCK directly`)
  }
  const blockCommits = directKind(parsed, transactionBlocks[0].index, CST.COMMIT)
  if (blockCommits.length !== 1 ||
      nodeText(parsed, bytes, blockCommits[0]).trim() !== "commit receipt") {
    fail(`${label} transaction block does not own source-shaped COMMIT`)
  }
  const commitExpressions = directKind(parsed, blockCommits[0].index, CST.EXPRESSION)
  if (commitExpressions.length !== 1 ||
      nodeText(parsed, bytes, commitExpressions[0]).trim() !== "receipt") {
    fail(`${label} COMMIT does not own direct receipt EXPRESSION`)
  }
  const returnNode = parsed.nodes.find((node) => node.kind === CST.RETURN)
  const returnExpression = returnNode && directKind(parsed, returnNode.index, CST.EXPRESSION)[0]
  if (!returnExpression) fail(`${label} transaction return expression is missing`)
  const effectWords = descendants(parsed, returnExpression.index)
    .filter((node) => node.kind === CST.WORD)
    .map((node) => nodeText(parsed, bytes, node))
  if (!effectWords.includes("try") || !effectWords.includes("await")) {
    fail(`${label} transaction return does not preserve try/await leaves`)
  }
}

function assertLanguageLock(parsed, bytes, label, { lockCount = 1, prefix = null } = {}) {
  assertClean(parsed, label)
  const locks = parsed.nodes.filter((node) => node.kind === CST.LOCK)
  if (locks.length !== lockCount) fail(`${label} does not contain ${lockCount} LOCK nodes`)
  const lock = locks[0]
  const words = directKind(parsed, lock.index, CST.WORD)
  const target = directKind(parsed, lock.index, CST.EXPRESSION)
  const body = directKind(parsed, lock.index, CST.BLOCK)
  if (words.length !== 3 || target.length !== 1 || body.length !== 1) {
    fail(`${label} LOCK direct owners are not WORD x3, EXPRESSION, BLOCK`)
  }
  if (nodeText(parsed, bytes, words[0]) !== "lock" ||
      nodeText(parsed, bytes, words[1]) !== "as") {
    fail(`${label} LOCK keyword/binding order is not source-shaped`)
  }
  if (!(words[0].start < target[0].start &&
        target[0].end <= words[1].start &&
        words[1].end <= words[2].start &&
        words[2].end <= body[0].start)) {
    fail(`${label} LOCK owner order/spans are not source-shaped`)
  }
  if (lock.start !== words[0].start || lock.end !== body[0].end) {
    fail(`${label} LOCK span does not cover keyword through body`)
  }
  const sharedTypes = parsed.nodes.filter((node) => node.kind === CST.TYPE)
    .filter((node) => directKind(parsed, node.index, CST.WORD)
      .some((word) => nodeText(parsed, bytes, word) === "shared"))
  if (sharedTypes.length < 1) fail(`${label} does not preserve shared as a TYPE leaf`)
  const expressions = parsed.nodes.filter((node) => node.kind === CST.EXPRESSION)
  const outer = expressions.find((node) =>
    descendants(parsed, node.index).some((child) => child.index === lock.index))
  if (!outer) fail(`${label} LOCK has no owning outer EXPRESSION`)
  if (prefix !== null && !directKind(parsed, outer.index, CST.WORD)
      .some((word) => nodeText(parsed, bytes, word) === prefix)) {
    fail(`${label} outer EXPRESSION does not preserve ${prefix}`)
  }
}

function assertBorrowClause(parsed, bytes, label) {
  assertClean(parsed, label)
  const functions = parsed.nodes.filter((node) => node.kind === CST.FUNCTION)
  if (functions.length !== 1) fail(`${label} does not contain one FUNCTION node`)
  const functionNode = functions[0]
  const direct = childrenOf(parsed, functionNode.index)
  const positionOf = (kind) => direct.findIndex((node) => node.kind === kind)
  const positions = [CST.PARAMETER_LIST, CST.RETURN_TYPE, CST.BORROW_CLAUSE, CST.BLOCK]
    .map(positionOf)
  if (positions.some((position) => position < 0) ||
      !(positions[0] < positions[1] && positions[1] < positions[2] && positions[2] < positions[3])) {
    fail(`${label} FUNCTION direct child order does not place BORROW_CLAUSE before BLOCK`)
  }
  const clauses = directKind(parsed, functionNode.index, CST.BORROW_CLAUSE)
  if (clauses.length !== 1) fail(`${label} does not contain one direct BORROW_CLAUSE`)
  const clause = clauses[0]
  const pairs = directKind(parsed, clause.index, CST.BORROW_PAIR)
  if (pairs.length === 0) fail(`${label} BORROW_CLAUSE has no BORROW_PAIR children`)
  for (const pair of pairs) {
    const slots = directKind(parsed, pair.index, CST.SLOT_REF)
    if (slots.length < 2) fail(`${label} BORROW_PAIR does not own result and source SLOT_REFs`)
  }
  const firstPairSlots = directKind(parsed, pairs[0].index, CST.SLOT_REF)
    .map((node) => nodeText(parsed, bytes, node))
  if (firstPairSlots.join("|") !== "0|fallback|primary") {
    fail(`${label} first BORROW_PAIR source order is not 0|fallback|primary`)
  }
  if (!nodeText(parsed, bytes, clause).startsWith("borrows(")) {
    fail(`${label} BORROW_CLAUSE does not own its keyword`)
  }
  const viewTypes = parsed.nodes.filter((node) => node.kind === CST.TYPE)
    .filter((node) => directKind(parsed, node.index, CST.WORD)
      .some((word) => nodeText(parsed, bytes, word) === "view"))
  if (viewTypes.length < 1) fail(`${label} does not preserve view WORDs inside TYPE nodes`)
}

function assertPhase2OptionalLabels(parsed, bytes, label) {
  assertClean(parsed, label)
  if (parsed.nodes.filter((node) => node.kind === CST.GENERIC_PARAMETERS).length !== 1 ||
      parsed.nodes.filter((node) => node.kind === CST.GENERIC_PARAMETER).length !== 1) {
    fail(`${label} does not contain one generic parameter owner`)
  }
  const envelopes = parsed.nodes.filter((node) => node.kind === CST.CONTRACT_ENVELOPE)
  if (envelopes.length !== 2) fail(`${label} does not contain two contract envelopes`)
  envelopes.sort((left, right) => left.start - right.start)
  const structure = parsed.nodes.find((node) => node.kind === CST.STRUCT)
  if (!structure || directKind(parsed, structure.index, CST.GENERIC_PARAMETERS).length !== 1) {
    fail(`${label} STRUCT does not directly own GENERIC_PARAMETERS`)
  }
  for (const [index, envelope] of envelopes.entries()) {
    if (!directKind(parsed, envelope.index, CST.WORD)
      .some((word) => nodeText(parsed, bytes, word) === "ready")) {
      fail(`${label} contract envelope does not preserve contextual .ready`)
    }
    const hasLabel = directKind(parsed, envelope.index, CST.WORD)
      .some((word) => nodeText(parsed, bytes, word) === "state")
    if (index === 0 && hasLabel) fail(`${label} first envelope is not positional`)
    if (index === 1 && (!hasLabel ||
        !directKind(parsed, envelope.index, CST.PUNCTUATION)
          .some((punctuation) => nodeText(parsed, bytes, punctuation) === ":"))) {
      fail(`${label} second envelope does not preserve state: label`)
    }
  }
  const expressions = parsed.nodes.filter((node) => node.kind === CST.EXPRESSION)
  if (!expressions.some((expression) =>
      directKind(parsed, expression.index, CST.CONTRACT_ENVELOPE).length === 1)) {
    fail(`${label} expression postfix does not directly own a contract envelope`)
  }
}

function assertPhase2Contracts(parsed, bytes, label) {
  assertClean(parsed, label)
  const declarations = parsed.nodes.filter((node) => node.kind === CST.TYPE_DECLARATION)
    .sort((left, right) => left.start - right.start)
  if (declarations.length !== 2) fail(`${label} does not contain two TYPE_DECLARATION owners`)
  if (!(declarations[0].start < declarations[1].start)) {
    fail(`${label} type declaration source order is not preserved`)
  }
  const activeType = directKind(parsed, declarations[0].index, CST.TYPE)[0]
  const laterType = directKind(parsed, declarations[1].index, CST.TYPE)[0]
  if (!activeType || !laterType) fail(`${label} type declaration TYPE owners are missing`)
  if (directKind(parsed, activeType.index, CST.CONTRACT_ENVELOPE).length !== 2 ||
      directKind(parsed, laterType.index, CST.CONTRACT_ENVELOPE).length !== 1) {
    fail(`${label} sequential contract envelope ownership is not preserved`)
  }
  const predicate = directKind(parsed, activeType.index, CST.CONTRACT_ENVELOPE)[1]
  const list = directKind(parsed, laterType.index, CST.CONTRACT_ENVELOPE)[0]
  if (directKind(parsed, predicate.index, CST.EXPRESSION).length !== 1 ||
      directKind(parsed, list.index, CST.ARRAY).length !== 1) {
    fail(`${label} predicate/list static forms have wrong direct owners`)
  }
  if (!nodeText(parsed, bytes, activeType).includes("Array<Order>")) {
    fail(`${label} active type is not source-shaped`)
  }
}

function assertPhase2Switch(parsed, bytes, label) {
  assertClean(parsed, label)
  const aliases = parsed.nodes.filter((node) => node.kind === CST.ALIAS_DECLARATION)
  const switches = parsed.nodes.filter((node) => node.kind === CST.SWITCH_EXPRESSION)
  if (aliases.length !== 1 || switches.length !== 1) {
    fail(`${label} does not contain one ALIAS_DECLARATION and SWITCH_EXPRESSION`)
  }
  const switchNode = switches[0]
  const arms = directKind(parsed, switchNode.index, CST.SWITCH_ARM)
  if (arms.length !== 3) fail(`${label} SWITCH_EXPRESSION does not directly own three arms`)
  const expectedPatternKinds = [CST.ENUM_PATTERN, CST.ENUM_PATTERN, CST.ENUM_PATTERN]
  const expected = ["case .reserving", "case .preparing", "case .serving"]
  for (const [index, arm] of arms.entries()) {
    if (!nodeText(parsed, bytes, arm).trimStart().startsWith(expected[index])) {
      fail(`${label} SWITCH_ARM source order is not preserved`)
    }
    const patternOwners = childrenOf(parsed, arm.index).filter((child) =>
      Number(child.kind) === Number(CST.ENUM_PATTERN) ||
      Number(child.kind) === Number(CST.WILDCARD_PATTERN) ||
      Number(child.kind) === Number(CST.LITERAL_PATTERN))
    if (patternOwners.length !== 1 || Number(patternOwners[0].kind) !== Number(expectedPatternKinds[index])) {
      fail(`${label} SWITCH_ARM ${index} does not have exactly one direct enum pattern owner`)
    }
    if (directKind(parsed, arm.index, CST.EXPRESSION).length !== 1) {
      fail(`${label} SWITCH_ARM ${index} does not have exactly one direct result EXPRESSION`)
    }
  }
  const aliasType = directKind(parsed, aliases[0].index, CST.TYPE)[0]
  if (!aliasType || directKind(parsed, aliasType.index, CST.CONTRACT_ENVELOPE).length !== 1 ||
      directKind(parsed, directKind(parsed, aliasType.index, CST.CONTRACT_ENVELOPE)[0].index,
        CST.ARRAY).length !== 1) {
    fail(`${label} alias subset envelope/list ownership is incomplete`)
  }
}

function assertAllocatorBlock(parsed, bytes, label, expectedBinding = null) {
  assertClean(parsed, label)
  const blocks = parsed.nodes.filter((node) => node.kind === CST.ALLOCATOR_BLOCK)
  if (blocks.length !== 1) fail(`${label} does not contain one ALLOCATOR_BLOCK`)
  const allocator = blocks[0]
  const direct = childrenOf(parsed, allocator.index)
  const words = direct.filter((node) => node.kind === CST.WORD)
  const punctuation = direct.filter((node) => node.kind === CST.PUNCTUATION)
  const plans = directKind(parsed, allocator.index, CST.EXPRESSION)
  const bodies = directKind(parsed, allocator.index, CST.BLOCK)
  if (words.length < 1 || plans.length !== 1 || bodies.length !== 1) {
    fail(`${label} ALLOCATOR_BLOCK direct owners are incomplete`)
  }
  if (nodeText(parsed, bytes, words[0]) !== "allocator" ||
      allocator.start !== words[0].start || allocator.end < bodies[0].end ||
      plans[0].start <= words[0].end || plans[0].end > bodies[0].start) {
    fail(`${label} ALLOCATOR_BLOCK source span/order is not preserved`)
  }
  if (expectedBinding === null) {
    if (words.length !== 1) fail(`${label} anonymous allocator owns a binding WORD`)
  } else {
    if (words.length !== 2 || nodeText(parsed, bytes, words[1]) !== expectedBinding ||
        !punctuation.some((node) => nodeText(parsed, bytes, node) === ":")) {
      fail(`${label} named allocator binding/colon is not source-shaped`)
    }
  }
  const normalizedPlan = nodeText(parsed, bytes, plans[0]).replace(/\s+/gu, "")
  if (normalizedPlan !== ".fixed<capacity:64<iec.KiB>>") {
    fail(`${label} allocator plan is not source-shaped`)
  }
  if (!directKind(parsed, bodies[0].index, CST.LET).some((node) =>
      nodeText(parsed, bytes, node).includes("snapshot"))) {
    fail(`${label} allocator body does not own the snapshot LET`)
  }
}

function assertAllocatorWitness(parsed, bytes) {
  assertClean(parsed, "allocation.w:nested-allocator")
  const blocks = parsed.nodes.filter((node) => node.kind === CST.ALLOCATOR_BLOCK)
  if (blocks.length !== 2) fail("allocation.w nested witness does not contain two ALLOCATOR_BLOCKs")
  const named = blocks.filter((allocator) => directKind(parsed, allocator.index, CST.WORD)
    .some((word) => nodeText(parsed, bytes, word) === "outer" ||
      nodeText(parsed, bytes, word) === "inner"))
  if (named.length !== 2) fail("allocation.w nested witness does not preserve named allocators")
  const outer = blocks.find((allocator) => {
    const body = directKind(parsed, allocator.index, CST.BLOCK)[0]
    return body && descendants(parsed, body.index).some((node) => node.kind === CST.ALLOCATOR_BLOCK)
  })
  if (!outer) fail("allocation.w nested witness does not preserve nesting")
  const body = directKind(parsed, outer.index, CST.BLOCK)[0]
  if (!descendants(parsed, body.index).some((node) => node.kind === CST.ARGUMENT &&
      nodeText(parsed, bytes, node).includes("allocator: outer"))) {
    fail("allocation.w nested witness does not preserve explicit allocator override")
  }
}

function assertAllocatorRecovery(parsed, bytes, label, expectedIssue,
                                 missingPlan = false) {
  if (parsed.result.status !== "recovered" || parsed.result.issueCount === 0) {
    fail(`${label} is not recovered with an issue`)
  }
  if (parsed.issues[0]?.kind !== expectedIssue) {
    fail(`${label} first issue ${parsed.issues[0]?.kind} != ${expectedIssue}`)
  }
  const blocks = parsed.nodes.filter((node) => node.kind === CST.ALLOCATOR_BLOCK)
  if (blocks.length !== 1) fail(`${label} recovery lost ALLOCATOR_BLOCK owner`)
  const allocator = blocks[0]
  if (allocator.start > allocator.end) fail(`${label} allocator owner span is inverted`)
  if (missingPlan) {
    if (directKind(parsed, allocator.index, CST.EXPRESSION).length !== 0) {
      fail(`${label} fabricated an EXPRESSION for a missing plan`)
    }
    const missing = directKind(parsed, allocator.index, CST.MISSING)
    if (missing.length !== 1 || missing[0].start !== missing[0].end ||
        missing[0].start < allocator.start || missing[0].start > allocator.end) {
      fail(`${label} missing plan is not a zero-width owner child`)
    }
  }
  // parseProbe already proves leaf partition and child containment/order.
  if (bytes.length !== parsed.result.length) fail(`${label} recovery bytes drifted`)
}

function assertAllocatorBoundary(parsed, bytes) {
  assertClean(parsed, "allocator-semicolon-boundary")
  const allocator = parsed.nodes.find((node) => node.kind === CST.ALLOCATOR_BLOCK)
  if (!allocator) fail("allocator-semicolon-boundary lost ALLOCATOR_BLOCK")
  const plan = directKind(parsed, allocator.index, CST.EXPRESSION)[0]
  const body = directKind(parsed, allocator.index, CST.BLOCK)[0]
  const semicolons = childrenOf(parsed, allocator.index)
    .filter((node) => node.kind === CST.PUNCTUATION && nodeText(parsed, bytes, node) === ";")
  if (!plan || !body || semicolons.length !== 1 || plan.end > body.start ||
      semicolons[0].start < body.end || semicolons[0].start < plan.end) {
    fail("allocator-semicolon-boundary plan/body/semicolon ownership drifted")
  }
  const enclosing = parsed.nodes.find((node) => node.kind === CST.BLOCK &&
    directKind(parsed, node.index, CST.ALLOCATOR_BLOCK).length === 1)
  if (!enclosing || directKind(parsed, enclosing.index, CST.LET).length !== 1 ||
      !nodeText(parsed, bytes, directKind(parsed, enclosing.index, CST.LET)[0])
        .startsWith("let after=next()")) {
    fail("allocator-semicolon-boundary did not continue with the following let")
  }
}

function assertLabeledControl(parsed, bytes, label) {
  assertClean(parsed, label)
  const loops = parsed.nodes.filter((node) => node.kind === CST.FOR)
  const labels = parsed.nodes.filter((node) => node.kind === CST.LABEL)
  if (loops.length !== 2 || labels.length !== 1) {
    fail(`${label} does not contain one label and two FOR nodes`)
  }
  const labelNode = labels[0]
  const outerLoop = directKind(parsed, labelNode.index, CST.FOR)[0]
  if (!outerLoop) fail(`${label} LABEL does not own FOR directly`)
  if (!directKind(parsed, outerLoop.index, CST.BLOCK).length) {
    fail(`${label} outer FOR does not own BLOCK directly`)
  }
  const outerBlock = directKind(parsed, outerLoop.index, CST.BLOCK)[0]
  if (directKind(parsed, outerBlock.index, CST.FOR).length !== 1) {
    fail(`${label} outer FOR block does not own nested FOR`)
  }
  if (!directKind(parsed, outerLoop.index, CST.WORD).some((node) => nodeText(parsed, bytes, node) === "ref")) {
    fail(`${label} FOR ownership marker is not a raw source leaf`)
  }
  const continueNode = parsed.nodes.find((node) => node.kind === CST.CONTINUE)
  const breakNode = parsed.nodes.find((node) => node.kind === CST.BREAK)
  if (!continueNode || !breakNode || nodeText(parsed, bytes, continueNode).trimEnd() !== "continue scanRows" ||
      nodeText(parsed, bytes, breakNode).trimEnd() !== "break scanRows") {
    fail(`${label} break/continue label spans are not source-shaped`)
  }
}

function assertLabeledBlockWitness(parsed, bytes) {
  assertClean(parsed, "labeled-block-for-witness")
  const labels = parsed.nodes.filter((node) => node.kind === CST.LABEL)
  if (labels.length !== 2) fail("labeled block witness does not contain two labels")
  const blockLabel = labels.find((node) => directKind(parsed, node.index, CST.BLOCK).length === 1)
  if (!blockLabel) fail("labeled block witness LABEL does not own BLOCK")
  const block = directKind(parsed, blockLabel.index, CST.BLOCK)[0]
  const nestedLabel = directKind(parsed, block.index, CST.LABEL)[0]
  if (!nestedLabel || directKind(parsed, nestedLabel.index, CST.FOR).length !== 1) {
    fail("labeled block witness BLOCK does not own LABEL->FOR")
  }
  const breakNode = parsed.nodes.find((node) => node.kind === CST.BREAK)
  if (!breakNode || nodeText(parsed, bytes, breakNode).trimEnd() !== "break assembleWord") {
    fail("labeled block witness break label span is not source-shaped")
  }
}

function assertMarkerVector(parsed, bytes) {
  assertClean(parsed, "for-marker-vector")
  const block = parsed.nodes.find((node) => node.kind === CST.BLOCK)
  if (!block) fail("for-marker-vector has no function block")
  const loops = directKind(parsed, block.index, CST.FOR)
  if (loops.length !== 3) fail("for-marker-vector does not contain three FOR nodes")
  for (const [loop, marker] of loops.map((node, index) => [node, ["ref", "inout", "copy"][index]])) {
    if (!directKind(parsed, loop.index, CST.WORD).some((node) => nodeText(parsed, bytes, node) === marker)) {
      fail(`for-marker-vector is missing raw marker ${marker}`)
    }
  }
}

function assertFormattingWitness(parsed, bytes) {
  assertClean(parsed, "formatting.w")
  const imports = parsed.nodes.filter((node) => node.kind === CST.IMPORT)
  if (imports.length !== 1 || directKind(parsed, imports[0].index, CST.IMPORT_ITEM).length !== 2) {
    fail("formatting.w import/items CST shape is not closed")
  }
  const structs = parsed.nodes.filter((node) => node.kind === CST.STRUCT)
  if (structs.length !== 1 || directKind(parsed, structs[0].index, CST.FIELD).length !== 2) {
    fail("formatting.w struct/fields CST shape is not closed")
  }
  const tests = parsed.nodes.filter((node) => node.kind === CST.TEST)
  const expects = parsed.nodes.filter((node) => node.kind === CST.EXPECT)
  if (tests.length !== 1 || expects.length !== 1 ||
      !descendants(parsed, tests[0].index).some((node) => node.index === expects[0].index)) {
    fail("formatting.w TEST does not own EXPECT")
  }
  const expectExpression = directKind(parsed, expects[0].index, CST.EXPRESSION)[0]
  const comparison = Buffer.from("value == \"Last Light\"")
  const comparisonStart = bytes.indexOf(comparison)
  const expressionText = expectExpression ? nodeText(parsed, bytes, expectExpression) : ""
  if (!expectExpression || comparisonStart < 0 || expectExpression.start !== comparisonStart ||
      !expressionText.startsWith(comparison.toString("utf8")) ||
      expressionText.slice(comparison.length).trim() !== "" ||
      expects[0].start !== bytes.indexOf(Buffer.from("expect value")) ||
      expects[0].end !== expectExpression.end) {
    fail("formatting.w EXPECT does not own the complete comparison expression")
  }
  const argumentsInOrder = parsed.nodes.filter((node) => node.kind === CST.ARGUMENT)
    .sort((left, right) => left.start - right.start)
  if (argumentsInOrder.length < 3) fail("formatting.w argument nodes are missing")
  const labeledArgumentTexts = argumentsInOrder.map((node) => nodeText(parsed, bytes, node))
  if (!labeledArgumentTexts.includes("value: value") ||
      !labeledArgumentTexts.includes("expected: expected")) {
    fail("formatting.w labeled arguments are not source-shaped")
  }
}

async function formattingWitnessBytes() {
  const bridgePath = resolve(root, "tooling", "studies", "r1-source-boundaries", "bundle.json")
  const bridge = await Bun.file(bridgePath).json()
  const sourceRef = bridge.sourceBase
  if (!sourceRef || sourceRef.symbol !== "oneLine" ||
      sourceRef.path !== "../../../reference/last-light/formatting.w" ||
      !/^sha256:[0-9a-f]{64}$/u.test(sourceRef.digest ?? "")) {
    fail("formatting.w sourceRef bridge is invalid")
  }
  const sourcePath = resolve(bridgePath, "..", sourceRef.path)
  const bytes = Buffer.from(await Bun.file(sourcePath).arrayBuffer())
  const digest = `sha256:${createHash("sha256").update(bytes).digest("hex")}`
  if (digest !== sourceRef.digest) fail("formatting.w sourceRef digest is stale")
  return bytes
}

async function sourceBackedFragment(relativePath, startMarker, endMarker, label) {
  const sourcePath = resolve(root, relativePath)
  const bytes = Buffer.from(await Bun.file(sourcePath).arrayBuffer())
  const startNeedle = Buffer.from(startMarker, "utf8")
  const endNeedle = Buffer.from(endMarker, "utf8")
  const start = bytes.indexOf(startNeedle)
  const duplicateStart = start >= 0 ? bytes.indexOf(startNeedle, start + 1) : -1
  const end = bytes.indexOf(endNeedle)
  const duplicateEnd = end >= 0 ? bytes.indexOf(endNeedle, end + 1) : -1
  if (start < 0 || duplicateStart >= 0 || end < 0 || duplicateEnd >= 0 ||
      end <= start) {
    fail(`${label} source markers are missing, duplicated, or out of order`)
  }
  return bytes.subarray(start, end)
}

function invoke(probe, bytes, label, expectedStatus, expectedIssue) {
  const execution = Bun.spawnSync({
    cmd: [probe],
    cwd: root,
    stdin: bytes,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (execution.exitCode !== 0) {
    fail(`${label} probe exited ${execution.exitCode}: ${execution.stderr.toString().trim()}`)
  }
  const parsed = parseProbe(execution.stdout.toString(), bytes, label)
  if (parsed.result.status !== expectedStatus) {
    fail(`${label} status ${parsed.result.status} != ${expectedStatus}`)
  }
  if (expectedIssue !== undefined && !parsed.issues.some((issue) => issue.kind === expectedIssue)) {
    fail(`${label} does not contain internal issue kind ${expectedIssue}`)
  }
  return parsed
}

async function main() {
  if (corpus.$schema !== "w-formatter-cases-0" || corpus.status !== "design-oracle-input") {
    fail("formatter corpus is not the design-oracle input")
  }
  const cases = new Map(corpus.cases.map((testCase) => [testCase.id, testCase]))
  for (const id of selectedIds) if (!cases.has(id)) fail(`missing formatter case ${id}`)

  const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-parser-"))
  try {
    run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
    run("cmake", ["--build", buildDirectory])
    run("ctest", ["--test-dir", buildDirectory, "--output-on-failure"])
    const probeName = process.platform === "win32" ? "w_seed_parser_probe.exe" : "w_seed_parser_probe"
    const probe = join(buildDirectory, probeName)
    if (!(await Bun.file(probe).exists())) fail(`probe is missing at ${probe}`)

    for (const id of selectedIds) {
      const testCase = cases.get(id)
      const input = inputText(testCase.input, id)
      const output = outputText(testCase.output, id)
      const inputParsed = invoke(probe, input, `${id}:input`, "complete")
      const outputParsed = invoke(probe, output, `${id}:output`, "complete")
      const second = invoke(probe, input, `${id}:repeat`, "complete")
      assertClean(inputParsed, `${id}:input`)
      assertClean(outputParsed, `${id}:output`)
      assertClean(second, `${id}:repeat`)
      if (id === "F0-repeat-array-semicolon") {
        assertRepeatArray(inputParsed, input, `${id}:input`)
        assertRepeatArray(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-labeled-control") {
        assertLabeledControl(inputParsed, input, `${id}:input`)
        assertLabeledControl(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-effect-prefix-order") {
        assertAsyncFunction(inputParsed, input, `${id}:input`)
        assertAsyncFunction(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-structured-transaction") {
        assertStructuredTransaction(inputParsed, input, `${id}:input`)
        assertStructuredTransaction(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-language-lock") {
        assertLanguageLock(inputParsed, input, `${id}:input`)
        assertLanguageLock(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-borrows-clause-source-order") {
        assertBorrowClause(inputParsed, input, `${id}:input`)
        assertBorrowClause(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-optional-label-slots") {
        assertPhase2OptionalLabels(inputParsed, input, `${id}:input`)
        assertPhase2OptionalLabels(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-contracts-and-source-order") {
        assertPhase2Contracts(inputParsed, input, `${id}:input`)
        assertPhase2Contracts(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-enum-subset-switch") {
        assertPhase2Switch(inputParsed, input, `${id}:input`)
        assertPhase2Switch(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-allocator-anonymous-contextual-call") {
        assertAllocatorBlock(inputParsed, input, `${id}:input`)
        assertAllocatorBlock(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-allocator-named-override") {
        assertAllocatorBlock(inputParsed, input, `${id}:input`, "scratch")
        assertAllocatorBlock(outputParsed, output, `${id}:output`, "scratch")
      }
      if (id === "F0-spawn-domain-slots") {
        assertSpawnDomainSlots(inputParsed, input, `${id}:input`)
        assertSpawnDomainSlots(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-explicit-capture-closure") {
        assertClosureCapture(inputParsed, input, `${id}:input`)
        assertClosureCapture(outputParsed, output, `${id}:output`)
      }
      if (id === "F0-opaque-foreign-body") {
        const expectedBody = testCase.opaqueForeignBodies?.[0]
        if (typeof expectedBody !== "string") fail(`${id} has no pinned body text`)
        assertForeignIsland(inputParsed, input, `${id}:input`, expectedBody)
        assertForeignIsland(outputParsed, output, `${id}:output`, expectedBody)
      }
      if (inputParsed.signature !== second.signature) fail(`${id} CST signature is not deterministic`)
      if (inputParsed.nodes.length === 0 || outputParsed.nodes.length === 0) fail(`${id} has no CST nodes`)
    }

    const serviceStageWitness = await sourceBackedFragment(
      "reference/last-light/domain.w",
      "export enum ServiceStage {",
      "export alias CancelledStage",
      "domain.w ServiceStage enum witness",
    )
    const serviceStageParsed = invoke(
      probe, serviceStageWitness, "domain.w:ServiceStage", "complete",
    )
    assertEnumWitness(serviceStageParsed, serviceStageWitness,
      "domain.w:ServiceStage", 6, 0,
      ["accepted", "reserving", "preparing", "serving", "completed", "cancelled"])
    const serviceStageRepeat = invoke(
      probe, serviceStageWitness, "domain.w:ServiceStage:repeat", "complete",
    )
    if (serviceStageParsed.signature !== serviceStageRepeat.signature) {
      fail("domain.w ServiceStage enum CST signature is not deterministic")
    }

    const domainErrorWitness = await sourceBackedFragment(
      "reference/last-light/domain.w",
      "export enum DomainError: Error {",
      "export fn add(",
      "domain.w DomainError enum witness",
    )
    const domainErrorParsed = invoke(
      probe, domainErrorWitness, "domain.w:DomainError", "complete",
    )
    assertEnumWitness(domainErrorParsed, domainErrorWitness,
      "domain.w:DomainError", 5, 6,
      ["invalidGuestCount", "invalidTransition", "unknownOrder",
        "currencyMismatch", "overflow"])
    const domainErrorRepeat = invoke(
      probe, domainErrorWitness, "domain.w:DomainError:repeat", "complete",
    )
    if (domainErrorParsed.signature !== domainErrorRepeat.signature) {
      fail("domain.w DomainError enum CST signature is not deterministic")
    }

    const witness = await formattingWitnessBytes()
    const witnessParsed = invoke(probe, witness, "formatting.w", "complete")
    const witnessRepeat = invoke(probe, witness, "formatting.w:repeat", "complete")
    assertFormattingWitness(witnessParsed, witness)
    if (witnessParsed.signature !== witnessRepeat.signature) {
      fail("formatting.w CST signature is not deterministic")
    }

    const executionWitness = await sourceBackedFragment(
      "reference/last-light/execution.w",
      "export async fn mixPair(",
      "export async fn mixOnThermalLane(",
      "execution.w mixPair witness",
    )
    const executionParsed = invoke(
      probe, executionWitness, "execution.w:mixPair", "complete",
    )
    assertSpawnDomainSlots(executionParsed, executionWitness, "execution.w:mixPair", {
      typeItems: ["MixingResult", "MixingResult"],
      expressionItems: ["leftResult", "rightResult"],
      envelopes: ["<.compute>", "<.compute>"],
      namedSecond: false,
    })
    const executionRepeat = invoke(
      probe, executionWitness, "execution.w:mixPair:repeat", "complete",
    )
    if (executionParsed.signature !== executionRepeat.signature) {
      fail("execution.w mixPair witness CST signature is not deterministic")
    }

    const genericsWitness = await sourceBackedFragment(
      "reference/last-light/generics.w",
      "export alias EnabledFeature",
      "extension<T: Display",
      "generics.w static-alias witness",
    )
    const genericsParsed = invoke(
      probe, genericsWitness, "generics.w:static-aliases", "complete",
    )
    assertClean(genericsParsed, "generics.w:static-aliases")
    if (genericsParsed.nodes.filter((node) => node.kind === CST.ALIAS_DECLARATION).length !== 10 ||
        genericsParsed.nodes.filter((node) => node.kind === CST.CONTRACT_ENVELOPE).length !== 10) {
      fail("generics.w static-alias witness owner counts are incomplete")
    }
    const genericsRepeat = invoke(
      probe, genericsWitness, "generics.w:static-aliases:repeat", "complete",
    )
    if (genericsParsed.signature !== genericsRepeat.signature) {
      fail("generics.w static-alias witness CST signature is not deterministic")
    }

    const enumAliasWitness = await sourceBackedFragment(
      "reference/last-light/enum_contracts.w",
      "export alias WorkStage",
      "export alias ActiveStage",
      "enum_contracts.w alias witness",
    )
    const enumAliasParsed = invoke(
      probe, enumAliasWitness, "enum_contracts.w:alias", "complete",
    )
    assertClean(enumAliasParsed, "enum_contracts.w:alias")
    if (enumAliasParsed.nodes.filter((node) => node.kind === CST.ALIAS_DECLARATION).length !== 1 ||
        enumAliasParsed.nodes.filter((node) => node.kind === CST.CONTRACT_ENVELOPE).length !== 1 ||
        enumAliasParsed.nodes.filter((node) => node.kind === CST.ARRAY).length !== 1) {
      fail("enum_contracts.w alias witness owner counts are incomplete")
    }
    const enumAliasRepeat = invoke(
      probe, enumAliasWitness, "enum_contracts.w:alias:repeat", "complete",
    )
    if (enumAliasParsed.signature !== enumAliasRepeat.signature) {
      fail("enum_contracts.w alias witness CST signature is not deterministic")
    }

    const switchWitness = await sourceBackedFragment(
      "reference/last-light/enum_contracts.w",
      "export fn nextWorkStage",
      "export fn routeAcceptedOrder",
      "enum_contracts.w switch witness",
    )
    const switchWitnessParsed = invoke(
      probe, switchWitness, "enum_contracts.w:switch", "complete",
    )
    assertClean(switchWitnessParsed, "enum_contracts.w:switch")
    const switchWitnessNode = switchWitnessParsed.nodes
      .find((node) => node.kind === CST.SWITCH_EXPRESSION)
    if (!switchWitnessNode ||
        directKind(switchWitnessParsed, switchWitnessNode.index, CST.SWITCH_ARM).length !== 2) {
      fail("enum_contracts.w switch witness does not preserve two direct arms")
    }
    const switchWitnessRepeat = invoke(
      probe, switchWitness, "enum_contracts.w:switch:repeat", "complete",
    )
    if (switchWitnessParsed.signature !== switchWitnessRepeat.signature) {
      fail("enum_contracts.w switch witness CST signature is not deterministic")
    }

    const allocatorWitness = await sourceBackedFragment(
      "reference/last-light/allocation.w",
      "fn nestedAllocatorScopes()",
      "fn rootDefaultConstruction(",
      "allocation.w nested allocator witness",
    )
    const allocatorWitnessParsed = invoke(
      probe, allocatorWitness, "allocation.w:nested-allocator", "complete",
    )
    assertAllocatorWitness(allocatorWitnessParsed, allocatorWitness)
    const allocatorWitnessRepeat = invoke(
      probe, allocatorWitness, "allocation.w:nested-allocator:repeat", "complete",
    )
    if (allocatorWitnessParsed.signature !== allocatorWitnessRepeat.signature) {
      fail("allocation.w nested allocator witness CST signature is not deterministic")
    }

    const callablesPath = resolve(root, "reference", "last-light", "callables.w")
    const callablesSource = await Bun.file(callablesPath).bytes()
    const callablesDigest = createHash("sha256").update(callablesSource).digest("hex")
    if (callablesDigest !== "9b8c63d17e3293322ac8e589fa87092c73cfb912b0985260275a733d28ee0368") {
      fail(`callables.w source digest changed: ${callablesDigest}`)
    }
    const callablesFull = invoke(probe, callablesSource, "callables.w:full", "complete")
    assertClean(callablesFull, "callables.w:full")
    const callablesFullRepeat = invoke(
      probe, callablesSource, "callables.w:full:repeat", "complete",
    )
    if (callablesFull.signature !== callablesFullRepeat.signature) {
      fail("callables.w full source CST signature is not deterministic")
    }

    const callablesWitness = await sourceBackedFragment(
      "reference/last-light/callables.w",
      "export fn ticketSequence(",
      "export fn finalManifest(",
      "callables.w ticketSequence witness",
    )
    const callablesParsed = invoke(
      probe, callablesWitness, "callables.w:ticketSequence", "complete",
    )
    assertClosureCapture(callablesParsed, callablesWitness,
      "callables.w:ticketSequence")
    const callablesRepeat = invoke(
      probe, callablesWitness, "callables.w:ticketSequence:repeat", "complete",
    )
    if (callablesParsed.signature !== callablesRepeat.signature) {
      fail("callables.w ticketSequence witness CST signature is not deterministic")
    }

    const handCases = [
      ["nested-generic-and-shift", Buffer.from("fn f(x:Array<Array<u8>>):Array<Array<u8>>{return flags >> 2}\n"), "complete"],
      ["generic-declarations", Buffer.from("struct Box<_ state:State>{value:state}\nfn id<T:Order>(value:T):T{return value}\ntype Alias<T:Order> = Array<T>\nalias Legacy<U> = Array<Array<u8>>\n"), "complete"],
      ["contract-static-forms", Buffer.from("type A=Base<Widget><.ready><state:.ready><(count<=4)><[.a,.b]>\n"), "complete"],
      ["switch-three-arms", Buffer.from("fn f(stage:Stage):String{return switch stage{case .a:\"A\" case .b:\"B\" case .c:\"C\"}}\n"), "complete"],
      ["switch-semicolon-arms", Buffer.from("fn f(stage:Stage):String{return switch stage{case .a:\"A\";case .b:\"B\";case .c:\"C\";}}\n"), "complete"],
      ["switch-qualified-pattern", Buffer.from("fn f(stage:Stage):String{return switch stage{case Stage.a:\"A\" case Stage.b:\"B\"}}\n"), "complete"],
      ["switch-wildcard-pattern", Buffer.from("fn f(stage:Stage):String{return switch stage{case .a:\"A\" case _:\"rest\"}}\n"), "complete"],
      ["switch-literal-pattern", Buffer.from("fn f(stage:Stage):String{return switch stage{case 1:\"one\"}}\n"), "complete"],
      ["switch-boolean-qualified-recovery", Buffer.from("fn f(stage:Stage):String{return switch stage{case true.member:\"A\"}}\n"), "recovered"],
      ["switch-payload-pattern", Buffer.from("fn f(stage:Stage):String{return switch stage{case .a(value):\"A\"}}\n"), "recovered"],
      ["spaced-head", Buffer.from("fn f(x:Array /* note */ <u8>){return x}\n"), "recovered", 7],
      ["spaced-generic", Buffer.from("fn f <T>(x:T):T{return x}\n"), "recovered", 7],
      ["missing-generic-name", Buffer.from("fn f<:T>(x:T):T{return x}\n"), "recovered", 1],
      ["missing-generic-colon", Buffer.from("fn f<T Order>(x:T):T{return x}\n"), "recovered", 2],
      ["missing-generic-type", Buffer.from("fn f<T:>(x:T):T{return x}\n"), "recovered", 1],
      ["missing-generic-close", Buffer.from("fn f<T(x:T):T{return x}\n"), "recovered", 2],
      ["static-list-empty", Buffer.from("type A=Base<[]>\n"), "complete"],
      ["static-list-trailing-comma", Buffer.from("type A=Base<[.a,]>\n"), "complete"],
      ["static-list-malformed", Buffer.from("type A=Base<[.a,,.b]>\n"), "recovered", 1],
      ["static-predicate-malformed", Buffer.from("type A=Base<(.count<=1;>\n"), "recovered", 2],
      ["static-named-missing-value", Buffer.from("type A=Base<state:>\n"), "recovered", 1],
      ["switch-missing-arm", Buffer.from("fn f(x:X):String{return switch x{}}\n"), "recovered", 1],
      ["switch-missing-colon", Buffer.from("fn f(x:X):String{return switch x{case .a \"A\"}}\n"), "recovered", 1],
      ["switch-missing-close", Buffer.from("fn f(x:X):String{return switch x{case .a:\"A\"}\n"), "recovered", 2],
      ["ordinary-import-module-path", Buffer.from("import module.path\n"), "complete"],
      ["ordinary-import-single-segment", Buffer.from("import std\n"), "complete"],
      ["ordinary-import-wildcard", Buffer.from("import * from module.path\n"), "complete"],
      ["ordinary-import-named", Buffer.from("import {first,second} from module.path\n"), "complete"],
      ["ordinary-import-alias", Buffer.from("import alias from module.path\n"), "complete"],
      ["ordinary-import-missing-from", Buffer.from("import * module.path\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["ordinary-import-alias-missing-from", Buffer.from("import alias module.path\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["ordinary-import-missing-path", Buffer.from("import alias from\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["ordinary-import-wildcard-missing-path", Buffer.from("import * from\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["ordinary-import-trailing-path-dot", Buffer.from("import module.\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["import-after-type", Buffer.from("type A=Array<u8>\nimport {x} from module.path\n"), "fatal", 6],
      ["import-after-function", Buffer.from("fn f(){}\nimport {x} from module.path\n"), "fatal", 6],
      ["spaced-comparison", Buffer.from("fn f(left:Bool,right:Bool):Bool{return left < right}\n"), "complete"],
      ["try-question", Buffer.from("fn f(){return try? load()}\n"), "complete"],
      ["export-async-function", Buffer.from("export async fn load(kitchen:Kitchen):Menu throws KitchenError{return try await kitchen.loadMenu()}\n"), "complete"],
      ["export-async-trivia", Buffer.from("export /*a*/ async /*b*/ fn f(){}\n"), "complete"],
      ["const-function", Buffer.from("const fn f(){}\n"), "complete"],
      ["export-const-function", Buffer.from("export const fn f(){}\n"), "complete"],
      ["async-await-try-order", Buffer.from("async fn f(){return await try value()}\n"), "complete"],
      ["async-missing-name", Buffer.from("async fn\n"), "recovered", 1],
      ["async-missing-parameter-close", Buffer.from("async fn f(a:T{}\n"), "recovered", 2],
      ["async-missing-block-close", Buffer.from("async fn f(){return 1\n"), "recovered", 2],
      ["async-lone-stop", Buffer.from("async\n"), "fatal", 6],
      ["async-duplicate-stop", Buffer.from("async async fn f(){}\n"), "fatal", 6],
      ["async-struct-stop", Buffer.from("async struct S {}\n"), "fatal", 6],
      ["async-test-stop", Buffer.from("async test \"bad\" for f {}\n"), "fatal", 6],
      ["async-entry-stop", Buffer.from("async entry(f)\n"), "fatal", 6],
      ["export-async-struct-stop", Buffer.from("export async struct S {}\n"), "fatal", 6],
      ["static-function-stop", Buffer.from("static fn f(){}\n"), "fatal", 6],
      ["const-async-function-stop", Buffer.from("const async fn f(){}\n"), "fatal", 6],
      ["const-unsafe-function-stop", Buffer.from("const unsafe fn f(){}\n"), "fatal", 6],
      ["async-const-function-stop", Buffer.from("async const fn f(){}\n"), "fatal", 6],
      ["duplicate-const-function-stop", Buffer.from("const const fn f(){}\n"), "fatal", 6],
      ["lone-const-stop", Buffer.from("const\n"), "fatal", 6],
      ["unsafe-function-stop", Buffer.from("unsafe fn f(){}\n"), "fatal", 6],
      ["receiver-function-stop", Buffer.from("take fn f(){}\n"), "fatal", 6],
      ["transaction-simple", Buffer.from("fn f(){return transaction tx=provider{commit value}}\n"), "complete"],
      ["transaction-commit-outside", Buffer.from("fn f(){commit value}\n"), "complete"],
      ["transaction-commit-empty", Buffer.from("fn f(){commit}\n"), "complete"],
      ["transaction-nested", Buffer.from("fn f(){return transaction outer=provider{commit transaction inner=provider{commit value}}}\n"), "complete"],
      ["transaction-missing-binding", Buffer.from("fn f(){return transaction = provider{commit value}}\n"), "recovered", 1],
      ["transaction-missing-equals", Buffer.from("fn f(){return transaction tx provider{commit value}}\n"), "recovered", 1],
      ["transaction-missing-provider", Buffer.from("fn f(){return transaction tx= {commit value}}\n"), "recovered", 1],
      ["transaction-missing-block", Buffer.from("fn f(){return transaction tx=provider}\n"), "recovered", 2],
      ["transaction-missing-close", Buffer.from("fn f(){return transaction tx=provider{commit value\n"), "recovered", 2],
      ["transaction-malformed-commit", Buffer.from("fn f(){commit value else}\n"), "recovered", 3],
      ["transaction-contract-stop", Buffer.from("fn f(){return transaction<.serializable> tx=provider{commit value}}\n"), "fatal", 6],
      ["allocator-anonymous-contextual-call", Buffer.from("fn stage(allocator memory:ref Allocator,title:ref String,dishes menuDishes:ref Array<String>):MenuSnapshot{allocator .fixed<capacity:64<iec.KiB>>{let snapshot=stage(ref title,dishes:ref dishes)}}\n"), "complete"],
      ["allocator-named-override", Buffer.from("fn caller(allocator memory:ref Allocator,title:ref String){allocator scratch:.fixed<capacity:64<iec.KiB>>{let snapshot=stage(allocator:ref memory,ref title)}}\n"), "complete"],
      ["allocator-nested", Buffer.from("fn nested(){allocator outer:.fixed<capacity:64<iec.KiB>>{allocator inner:.fixed<capacity:64<iec.KiB>>{let local=Array<String>()}let portable=Array<String>(allocator:outer)}}\n"), "complete"],
      ["allocator-missing-plan", Buffer.from("fn f(){allocator {let value=1}}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["allocator-missing-open", Buffer.from("fn f(){allocator .fixed<capacity:64<iec.KiB>> let value=1}\n"), "recovered", ISSUE.MISSING_OWNER_CLOSE],
      ["allocator-missing-close", Buffer.from("fn f(){allocator .fixed<capacity:64<iec.KiB>>{let value=1}\n"), "recovered", ISSUE.MISSING_OWNER_CLOSE],
      ["allocator-nonword-binding", Buffer.from("fn f(){allocator 0:.fixed<capacity:64<iec.KiB>>{let value=1}}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["allocator-extra-colon", Buffer.from("fn f(){allocator .fixed<capacity:64<iec.KiB>>:{let value=1}}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["allocator-malformed-envelope", Buffer.from("fn f(){allocator .fixed<capacity:64<iec.KiB>{let value=1}}\n"), "recovered", ISSUE.MISSING_OWNER_CLOSE],
      ["allocator-semicolon-boundary", Buffer.from("fn f(){allocator .fixed<capacity:64<iec.KiB>>{let snapshot=stage(ref title)};let after=next()}\n"), "complete"],
      ["allocator-try-stop", Buffer.from("fn f(){try allocator .fixed<capacity:64<iec.KiB>>{let value=1}}\n"), "fatal", 6],
      ["allocator-root-stop", Buffer.from("allocator .fixed<capacity:64<iec.KiB>>{}\n"), "fatal", 6],
      ["tuple-type-three", Buffer.from("fn f():(A,B,C){}\n"), "complete"],
      ["tuple-type-trailing-comma", Buffer.from("fn f():(A,B,){}\n"), "complete"],
      ["tuple-expression-three", Buffer.from("fn f(){return (a,b,c,)}\n"), "complete"],
      ["tuple-parentheses", Buffer.from("fn f(){return (value)}\n"), "complete"],
      ["unit-type", Buffer.from("fn f():(){}\n"), "complete"],
      ["callable-type-bare", Buffer.from("fn f():fn(usize):usize{return 0}\n"), "complete"],
      ["callable-type-any-take-effects", Buffer.from("fn f():any take fn(ref usize):usize throws Error borrows(0:[0]){return 0}\n"), "complete"],
      ["callable-type-missing-close", Buffer.from("fn f():some mut fn(usize{return 0}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["capture-single-empty-params", Buffer.from("fn f(){return <[take next]>()=>next}\n"), "complete"],
      ["capture-all-modes", Buffer.from("fn f(){return <[copy gate,ref data,weak token,take id,]>()=>value}\n"), "complete"],
      ["capture-typed-params", Buffer.from("fn f(){return <[copy gate]>(x:usize,y:usize,)=>x+y}\n"), "complete"],
      ["capture-empty", Buffer.from("fn f(){return <[]> () => value return next}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["capture-invalid-mode", Buffer.from("fn f(){return <[inout x]>()=>value return next}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["capture-missing-name", Buffer.from("fn f(){return <[take]>()=>value return next}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["capture-missing-comma", Buffer.from("fn f(){return <[take x ref y]>()=>value return next}\n"), "recovered", ISSUE.MISSING_OWNER_CLOSE],
      ["capture-missing-close-square", Buffer.from("fn f(){return <[take x>()=>value return next}\n"), "recovered", ISSUE.MISSING_OWNER_CLOSE],
      ["capture-missing-close-angle", Buffer.from("fn f(){return <[take x] () => value return next}\n"), "recovered", ISSUE.MISSING_OWNER_CLOSE],
      ["capture-missing-close-paren", Buffer.from("fn f(){return <[take x]>(x=>x return next}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["capture-missing-arrow", Buffer.from("fn f(){return <[take x]>() value return next}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["capture-missing-body", Buffer.from("fn f(){return <[take x]>()=>} return next\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["capture-missing-block-close", Buffer.from("fn f(){return <[take x]>()=>{return value}\n"), "recovered", ISSUE.MISSING_OWNER_CLOSE],
      ["tuple-type-parenthesized", Buffer.from("fn f():(A){}\n"), "recovered", 1],
      ["tuple-type-singleton", Buffer.from("fn f():(A,){}\n"), "recovered", 1],
      ["tuple-expression-singleton", Buffer.from("fn f(){return (value,)}\n"), "recovered", 1],
      ["spawn-positional", Buffer.from("fn f(){spawn<.compute> let value=work()}\n"), "complete"],
      ["spawn-named", Buffer.from("fn f(){spawn<domain:.compute> let value=work()}\n"), "complete"],
      ["spawn-consecutive", Buffer.from("fn f(){spawn<.compute> let left=work()spawn<domain:.compute> let right=work()}\n"), "complete"],
      ["spawn-semicolon-boundary", Buffer.from("fn f(){spawn<.compute> let value=work();let after=next()}\n"), "complete"],
      ["spawn-missing-let", Buffer.from("fn f(){spawn<.compute> value=work()}\n"), "recovered", 1],
      ["spawn-missing-binder", Buffer.from("fn f(){spawn<.compute> let =work()}\n"), "recovered", 1],
      ["spawn-missing-equals", Buffer.from("fn f(){spawn<.compute> let value work()}\n"), "recovered", 1],
      ["spawn-missing-close", Buffer.from("fn f(){spawn<.compute let value=work()}\n"), "recovered", 2],
      ["spawn-stop-after-allocator", Buffer.from("fn f(){spawn let value=work()}\n"), "fatal", 6],
      ["spawn-root-stop", Buffer.from("spawn<.compute> let value=work()\n"), "fatal", 6],
      ["language-lock-plain", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as value{copy value}}\n"), "complete"],
      ["language-lock-await", Buffer.from("fn f(state:shared Ledger):Ledger{return await lock state as value{copy value}}\n"), "complete"],
      ["language-lock-try", Buffer.from("fn f(state:shared Ledger):Ledger{return try lock state as value{copy value}}\n"), "complete"],
      ["language-lock-member", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state.current as value{copy value}}\n"), "complete"],
      ["language-lock-group", Buffer.from("fn f(state:shared Ledger):Ledger{return lock (state) as value{copy value}}\n"), "complete"],
      ["language-lock-semantic-await-body", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as value{await work()}}\n"), "complete"],
      ["language-lock-semantic-ref-body", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as value{ref value}}\n"), "complete"],
      ["language-lock-nested", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as value{lock state as nested{copy nested}}}\n"), "complete"],
      ["language-lock-missing-as", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state value{copy value}}\n"), "recovered", 1],
      ["language-lock-missing-target", Buffer.from("fn f(state:shared Ledger):Ledger{return lock as value{copy value}}\n"), "recovered", 1],
      ["language-lock-missing-binding", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as{copy state}}\n"), "recovered", 1],
      ["language-lock-missing-body", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as value}\n"), "recovered", 2],
      ["language-lock-missing-close", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as value{copy value}\n"), "recovered", 2],
      ["language-lock-nonword-binding", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as 0{copy value}}\n"), "recovered", 1],
      ["language-lock-paren-body", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as value(copy value)}\n"), "recovered", 2],
      ["language-lock-extra-as", Buffer.from("fn f(state:shared Ledger):Ledger{return lock state as value as other{copy value}}\n"), "recovered", 2],
      ["borrow-view-two-pairs", Buffer.from("fn pick(primary: ref S, fallback: ref S): view S borrows(0: [fallback, primary], 1: [1,]) { return primary }\n"), "complete"],
      ["borrow-view-param-return", Buffer.from("fn pick(primary: view S, fallback: ref S): view S borrows(0: [fallback, primary]) { return primary }\n"), "complete"],
      ["borrow-slot-lexical", Buffer.from("fn pick(primary: ref S): view S borrows(1.5: [unknown], 99: [primary,]) { return primary }\n"), "complete"],
      ["borrow-duplicate-result", Buffer.from("fn pick(primary: ref S): view S borrows(7: [primary], 7: [unknown]) { return primary }\n"), "complete"],
      ["borrow-comments", Buffer.from("fn pick(primary: ref S): view S borrows(0: [/*x*/ primary, /*y*/ 1,], /*z*/ 1: [primary,]) { return primary }\n"), "complete"],
      ["borrow-contextual-identifier", Buffer.from("fn id(): S { borrows }\n"), "complete"],
      ["borrow-empty-clause", Buffer.from("fn f(a: ref S): view S borrows() { return a }\n"), "recovered", 1],
      ["borrow-empty-sources", Buffer.from("fn f(a: ref S): view S borrows(0: []) { return a }\n"), "recovered", 1],
      ["borrow-missing-result", Buffer.from("fn f(a: ref S): view S borrows(: [a]) { return a }\n"), "recovered", 1],
      ["borrow-missing-colon", Buffer.from("fn f(a: ref S): view S borrows(0 [a]) { return a }\n"), "recovered", 1],
      ["borrow-missing-open", Buffer.from("fn f(a: ref S): view S borrows(0: a) { return a }\n"), "recovered", 1],
      ["borrow-missing-close-square", Buffer.from("fn f(a: ref S): view S borrows(0: [a) { return a }\n"), "recovered", 2],
      ["borrow-missing-close-paren", Buffer.from("fn f(a: ref S): view S borrows(0: [a] { return a }\n"), "recovered", 2],
      ["borrow-missing-comma", Buffer.from("fn f(a: ref S): view S borrows(0: [a 1]) { return a }\n"), "recovered", 2],
      ["borrow-before-throws", Buffer.from("fn f(a: ref S): view S borrows(0: [a]) throws E { return a }\n"), "recovered", 2],
      ["borrow-duplicate-clause", Buffer.from("fn f(a: ref S): view S borrows(0: [a]) borrows(0: [a]) { return a }\n"), "recovered", 2],
      ["borrow-after-body", Buffer.from("fn f(a: ref S): view S { return a } borrows(0: [a])\n"), "recovered"],
      ["borrow-missing-view-base", Buffer.from("fn f(a: ref S): view { return a }\n"), "recovered", 1],
      ["postfix-question", Buffer.from("fn f(){value?.open?}\n"), "complete"],
      ["newline-continuation", Buffer.from("fn f(){let result = transform\n  (input)}\n"), "complete"],
      ["semicolon-boundary", Buffer.from("fn f(){a();(b)c();[d,e]}\n"), "complete"],
      ["missing-close", Buffer.from("fn f(){return 1\n"), "recovered", 2],
      ["stray-continuation", Buffer.from("fn f(){else}\n"), "recovered", 3],
      ["mixed-root", Buffer.from("module m\npackage {name: \"x\"}\n"), "fatal", 4],
      ["unsupported-root", Buffer.from("package {name: \"x\"}\n"), "fatal", 5],
      ["value-if-missing-else", Buffer.from("fn f():Stage{return if ready{.ok}}\n"), "recovered", 8],
      ["foreign-fail-closed", Buffer.from("fn f(){foreign c { host body }}\n"), "fatal", 9],
      ["missing-import-from", Buffer.from("import {x} module.path\n"), "fatal", 6],
      ["empty-import-items", Buffer.from("import {} from module.path\n"), "fatal", 6],
      ["trailing-import-dot", Buffer.from("import {x} from module.\n"), "recovered", 1],
      ["import-after-declaration", Buffer.from("fn f(){}\nimport {x} from module.path\n"), "fatal", 6],
      ["export-unsupported-target", Buffer.from("export test \"bad\" for f {}\n"), "fatal", 6],
      ["expect-outside-test", Buffer.from("fn f(){expect value == other}\n"), "fatal", 6],
      ["root-const-fail-closed", Buffer.from("const value:T\n"), "fatal", 6],
      ["root-take-fail-closed", Buffer.from("take value\n"), "fatal", 6],
      ["enum-empty-payload", Buffer.from("enum E { empty() }\n"), "recovered"],
      ["enum-missing-comma", Buffer.from("enum E { pair(A B) }\n"), "recovered"],
      ["enum-missing-colon", Buffer.from("enum E { pair(label Type) }\n"), "recovered"],
      ["enum-missing-close", Buffer.from("enum E { pair(A, label: B }\n"), "recovered"],
      ["enum-case-comma", Buffer.from("enum E { a, b }\n"), "recovered"],
      ["enum-unsupported-member", Buffer.from("enum E { export static fn make() {} }\n"), "recovered"],
      ["enum-context", Buffer.from("fn f(){ enum E { a } }\n"), "fatal", ISSUE.UNSUPPORTED_FORM],
      ["var-owner", Buffer.from("fn f(){var value=1}\n"), "complete"],
      ["var-missing-name", Buffer.from("fn f(){var =1}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["var-missing-equals", Buffer.from("fn f(){var value 1}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["test-optional-target", Buffer.from("test \"fixture\" {}\n"), "complete"],
      ["test-malformed-target", Buffer.from("test \"fixture\" for {}\n"), "recovered", ISSUE.UNEXPECTED_TOKEN],
      ["for-marker-vector", Buffer.from("fn markers(rows:Rows){for ref row in rows{}for inout item in rows{}for copy value in rows{}}\n"), "complete"],
      ["for-in-operator-and-nested", Buffer.from("fn expr(rows:Rows,flags:Flags){for row in rows in flags{}for value in (rows[0]){}}\n"), "complete"],
      ["labeled-block-for-witness", Buffer.from("fn scan(rows:Rows){assembleWord:{scanRows:for ref row in rows{for value in row{if value==0{continue scanRows} if value>31{break assembleWord}}}}}\n"), "complete"],
      ["for-missing-binder", Buffer.from("fn f(rows:Rows){for ref in rows{}}\n"), "recovered", 1],
      ["for-missing-unqualified-binder", Buffer.from("fn f(rows:Rows){for in rows{}}\n"), "recovered", 1],
      ["for-missing-in", Buffer.from("fn f(rows:Rows){for ref row rows{}}\n"), "recovered", 1],
      ["for-missing-iterable", Buffer.from("fn f(rows:Rows){for ref row in {}}\n"), "recovered", 1],
      ["for-missing-block", Buffer.from("fn f(rows:Rows){for ref row in rows}\n"), "recovered", 2],
      ["for-missing-close", Buffer.from("fn f(rows:Rows){for ref row in rows{}\n"), "recovered", 2],
      ["while-labeled-stop", Buffer.from("fn f(rows:Rows){outer:while rows{}}\n"), "fatal", 6],
      ["root-for-fail-closed", Buffer.from("for row in rows{}\n"), "fatal", 6],
      ["for-async-marker", Buffer.from("fn f(rows:Rows){for async value in rows{}}\n"), "fatal", 6],
      ["for-await-marker", Buffer.from("fn f(rows:Rows){for await value in rows{}}\n"), "fatal", 6],
      ["for-try-await-marker", Buffer.from("fn f(rows:Rows){for try await value in rows{}}\n"), "fatal", 6],
      ["for-take-marker", Buffer.from("fn f(rows:Rows){for take value in rows{}}\n"), "fatal", 6],
      ["for-take-iterable", Buffer.from("fn f(rows:Rows){for row in take rows{}}\n"), "complete"],
      ["missing-parameter-colon", Buffer.from("fn f(a T){}\n"), "recovered", 1],
      ["missing-parameter-close", Buffer.from("fn f(a:T{}\n"), "recovered", 2],
      ["missing-import-close", Buffer.from("import {x from module.path\n"), "recovered", 2],
      ["malformed-parameter-label", Buffer.from("fn f(named audit extra:Audit){}\n"), "recovered", 1],
    ]
    for (const [label, bytes, status, issue] of handCases) {
      const parsed = invoke(probe, bytes, label, status, issue)
      if ((label.startsWith("for-") || label.startsWith("async-") ||
           label.startsWith("borrow-") ||
           label.startsWith("export-async-") || label.startsWith("allocator-") ||
           label === "while-labeled-stop") &&
          issue !== undefined &&
          parsed.issues[0]?.kind !== issue) {
        fail(`${label} first issue ${parsed.issues[0]?.kind} != ${issue}`)
      }
      if (label === "allocator-missing-plan") {
        assertAllocatorRecovery(parsed, bytes, label, issue, true)
      }
      if (label === "allocator-missing-open" || label === "allocator-missing-close" ||
          label === "allocator-nonword-binding" || label === "allocator-extra-colon" ||
          label === "allocator-malformed-envelope") {
        assertAllocatorRecovery(parsed, bytes, label, issue)
      }
      if (label === "allocator-semicolon-boundary") {
        assertAllocatorBoundary(parsed, bytes)
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label === "spawn-semicolon-boundary") {
        assertSpawnSemicolonBoundary(parsed, bytes, label)
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        assertSpawnSemicolonBoundary(repeated, bytes, `${label}:repeat`)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label === "spawn-missing-let") {
        assertSpawnRecovery(parsed, bytes, label, {
          issue: ISSUE.UNEXPECTED_TOKEN,
          envelopeMissing: false,
          let: false,
          spawnMissing: true,
        })
      }
      if (label === "spawn-missing-binder" || label === "spawn-missing-equals") {
        assertSpawnRecovery(parsed, bytes, label, {
          issue: ISSUE.UNEXPECTED_TOKEN,
          envelopeMissing: false,
          let: true,
          letMissing: true,
        })
      }
      if (label === "spawn-missing-close") {
        assertSpawnRecovery(parsed, bytes, label, {
          issue: ISSUE.MISSING_OWNER_CLOSE,
          envelopeMissing: true,
          let: false,
          spawnMissing: false,
        })
      }
      if (label === "for-marker-vector") assertMarkerVector(parsed, bytes)
      if (label === "for-take-iterable") assertClean(parsed, label)
      if (label === "transaction-simple" || label === "transaction-commit-outside" ||
          label === "transaction-commit-empty" || label === "transaction-nested") {
        assertClean(parsed, label)
        const transactions = parsed.nodes.filter((node) => node.kind === CST.TRANSACTION)
        const commits = parsed.nodes.filter((node) => node.kind === CST.COMMIT)
        const expectedCounts = {
          "transaction-simple": [1, 1],
          "transaction-commit-outside": [0, 1],
          "transaction-commit-empty": [0, 1],
          "transaction-nested": [2, 2],
        }[label]
        if (transactions.length !== expectedCounts[0] || commits.length !== expectedCounts[1]) {
          fail(`${label} has ${transactions.length} TRANSACTION and ${commits.length} COMMIT nodes`)
        }
        if (label === "transaction-commit-empty" &&
            directKind(parsed, commits[0].index, CST.EXPRESSION).length !== 0) {
          fail(`${label} unexpectedly owns an EXPRESSION`)
        }
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label.startsWith("language-lock-")) {
        if (status === "complete") {
          const expectedLockCount = label === "language-lock-nested" ? 2 : 1
          const expectedPrefix = label === "language-lock-await" ? "await" :
            label === "language-lock-try" ? "try" : null
          assertLanguageLock(parsed, bytes, label, {
            lockCount: expectedLockCount,
            prefix: expectedPrefix,
          })
        }
        if (label === "language-lock-missing-target") {
          const locks = parsed.nodes.filter((node) => node.kind === CST.LOCK)
          if (locks.length !== 1) fail(`${label} does not contain one LOCK node`)
          if (directKind(parsed, locks[0].index, CST.EXPRESSION).length !== 0) {
            fail(`${label} fabricated a direct EXPRESSION for the missing target`)
          }
          if (directKind(parsed, locks[0].index, CST.MISSING).length < 1) {
            fail(`${label} does not preserve a MISSING target marker`)
          }
        }
        if (label === "language-lock-nested") {
          const locks = parsed.nodes.filter((node) => node.kind === CST.LOCK)
          const outer = locks.find((candidate) => locks.some((other) =>
            other.index !== candidate.index && candidate.start < other.start &&
            other.end < candidate.end))
          const inner = locks.find((candidate) => candidate.index !== outer?.index)
          if (!outer || !inner) fail(`${label} has no outer and inner LOCK pair`)
          const blocks = directKind(parsed, outer.index, CST.BLOCK)
          if (blocks.length !== 1 ||
              !descendants(parsed, blocks[0].index).some((node) => node.index === inner.index)) {
            fail(`${label} inner LOCK is not inside the outer LOCK block`)
          }
        }
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label === "export-async-function") {
        assertAsyncFunction(parsed, bytes, label, true)
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label === "export-async-trivia") {
        assertAsyncTrivia(parsed, bytes, label)
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label === "async-await-try-order") {
        assertClean(parsed, label)
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label === "labeled-block-for-witness") {
        assertLabeledBlockWitness(parsed, bytes)
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label === "generic-declarations") {
        assertClean(parsed, label)
        if (parsed.nodes.filter((node) => node.kind === CST.GENERIC_PARAMETERS).length !== 4 ||
            parsed.nodes.filter((node) => node.kind === CST.GENERIC_PARAMETER).length !== 4) {
          fail(`${label} generic declaration owners are incomplete`)
        }
        const structure = parsed.nodes.find((node) => node.kind === CST.STRUCT)
        const generic = structure && directKind(parsed, structure.index, CST.GENERIC_PARAMETERS)[0]
        const parameter = generic && directKind(parsed, generic.index, CST.GENERIC_PARAMETER)[0]
        if (!parameter || !directKind(parsed, parameter.index, CST.WORD)
          .some((word) => nodeText(parsed, bytes, word) === "_") ||
            !directKind(parsed, parameter.index, CST.WORD)
              .some((word) => nodeText(parsed, bytes, word) === "state") ||
            !directKind(parsed, parameter.index, CST.PUNCTUATION)
              .some((punctuation) => nodeText(parsed, bytes, punctuation) === ":") ||
            directKind(parsed, parameter.index, CST.TYPE).length !== 1) {
          fail(`${label} _ state:State parameter leaves/TYPE are not preserved`)
        }
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) fail(`${label} CST signature is not deterministic`)
      }
      if (label === "contract-static-forms") {
        assertClean(parsed, label)
        if (parsed.nodes.filter((node) => node.kind === CST.CONTRACT_ENVELOPE).length !== 5 ||
            parsed.nodes.filter((node) => node.kind === CST.ARRAY).length !== 1) {
          fail(`${label} static contract forms are incomplete`)
        }
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) fail(`${label} CST signature is not deterministic`)
      }
      if (label === "capture-single-empty-params") {
        assertExplicitCapture(parsed, bytes, label, 1, 0)
      }
      if (label === "callable-type-bare") {
        assertCallableType(parsed, bytes, label, {
          pattern: /^fn\s*\(\s*usize\s*\)\s*:\s*usize$/u,
          borrow: false,
        })
      }
      if (label === "callable-type-any-take-effects") {
        assertCallableType(parsed, bytes, label, {
          pattern: /^any\s+take\s+fn\s*\(\s*ref\s+usize\s*\)\s*:\s*usize\s+throws\s+Error\s+borrows\(0:\[0\]\)$/u,
          borrow: true,
        })
      }
      if (label === "callable-type-missing-close") {
        assertCallableTypeRecovery(parsed, bytes, label)
      }
      if (label.startsWith("callable-type-")) {
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label === "capture-all-modes") {
        assertExplicitCapture(parsed, bytes, label, 4, 0)
      }
      if (label === "capture-typed-params") {
        assertExplicitCapture(parsed, bytes, label, 1, 2)
      }
      if (label.startsWith("capture-") && status === "complete") {
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label === "capture-empty" || label === "capture-invalid-mode") {
        assertExplicitCaptureRecovery(parsed, bytes, label, {
          issue: ISSUE.UNEXPECTED_TOKEN,
          missing: false,
        })
      }
      if (label === "capture-missing-name") {
        assertExplicitCaptureRecovery(parsed, bytes, label, {
          issue: ISSUE.UNEXPECTED_TOKEN,
          missing: true,
        })
      }
      if (label === "capture-missing-comma" ||
          label === "capture-missing-close-square" ||
          label === "capture-missing-close-angle") {
        assertExplicitCaptureRecovery(parsed, bytes, label, {
          issue: ISSUE.MISSING_OWNER_CLOSE,
          missing: true,
        })
      }
      if (label === "capture-missing-block-close") {
        assertExplicitCaptureRecovery(parsed, bytes, label, {
          issue: ISSUE.MISSING_OWNER_CLOSE,
          missing: false,
        })
      }
      if (label === "capture-missing-close-paren") {
        assertExplicitCaptureRecovery(parsed, bytes, label, {
          issue: ISSUE.UNEXPECTED_TOKEN,
          missing: true,
        })
      }
      if (label === "capture-missing-arrow") {
        assertExplicitCaptureRecovery(parsed, bytes, label, {
          issue: ISSUE.UNEXPECTED_TOKEN,
          missing: true,
          following: true,
        })
      }
      if (label === "capture-missing-body") {
        assertExplicitCaptureRecovery(parsed, bytes, label, {
          issue: ISSUE.UNEXPECTED_TOKEN,
          missing: false,
        })
      }
      if (label.startsWith("capture-") && status === "recovered") {
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} recovery CST signature is not deterministic`)
        }
      }
      if (label === "switch-three-arms") {
        assertClean(parsed, label)
        const switchNode = parsed.nodes.find((node) => node.kind === CST.SWITCH_EXPRESSION)
        if (!switchNode || directKind(parsed, switchNode.index, CST.SWITCH_ARM).length !== 3) {
          fail(`${label} does not preserve three direct SWITCH_ARM owners`)
        }
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) fail(`${label} CST signature is not deterministic`)
      }
      if (label === "switch-qualified-pattern") {
        assertClean(parsed, label)
        const switchNode = parsed.nodes.find((node) => node.kind === CST.SWITCH_EXPRESSION)
        const arms = switchNode ? directKind(parsed, switchNode.index, CST.SWITCH_ARM) : []
        if (arms.length !== 2 || arms.some((arm) => {
          const patterns = childrenOf(parsed, arm.index).filter((child) =>
            child.kind === CST.ENUM_PATTERN || child.kind === CST.WILDCARD_PATTERN ||
            child.kind === CST.LITERAL_PATTERN)
          return patterns.length !== 1 || patterns[0].kind !== CST.ENUM_PATTERN ||
            directKind(parsed, arm.index, CST.EXPRESSION).length !== 1
        })) {
          fail(`${label} qualified arms do not preserve enum pattern/result owners`)
        }
      }
      if (label === "switch-wildcard-pattern") {
        assertClean(parsed, label)
        const switchNode = parsed.nodes.find((node) => node.kind === CST.SWITCH_EXPRESSION)
        const arms = switchNode ? directKind(parsed, switchNode.index, CST.SWITCH_ARM) : []
        if (arms.length !== 2 ||
            childrenOf(parsed, arms[1].index).filter((child) => child.kind === CST.WILDCARD_PATTERN).length !== 1 ||
            directKind(parsed, arms[1].index, CST.EXPRESSION).length !== 1) {
          fail(`${label} wildcard arm does not preserve exact pattern/result owners`)
        }
      }
      if (label === "switch-literal-pattern") {
        assertClean(parsed, label)
        const switchNode = parsed.nodes.find((node) => node.kind === CST.SWITCH_EXPRESSION)
        const arms = switchNode ? directKind(parsed, switchNode.index, CST.SWITCH_ARM) : []
        if (arms.length !== 1 ||
            childrenOf(parsed, arms[0].index).filter((child) => child.kind === CST.LITERAL_PATTERN).length !== 1 ||
            directKind(parsed, arms[0].index, CST.EXPRESSION).length !== 1) {
          fail(`${label} literal arm does not preserve exact pattern/result owners`)
        }
      }
      if (label === "switch-boolean-qualified-recovery") {
        const switchNode = parsed.nodes.find((node) => node.kind === CST.SWITCH_EXPRESSION)
        const arms = switchNode ? directKind(parsed, switchNode.index, CST.SWITCH_ARM) : []
        if (arms.length !== 1 ||
            childrenOf(parsed, arms[0].index).filter((child) => child.kind === CST.ENUM_PATTERN).length !== 0 ||
            childrenOf(parsed, arms[0].index).filter((child) => child.kind === CST.LITERAL_PATTERN).length !== 1) {
          fail(`${label} treated a boolean literal as a qualified enum pattern`)
        }
      }
      if (label === "spaced-comparison") {
        assertClean(parsed, label)
        if (parsed.nodes.some((node) => node.kind === CST.CONTRACT_ENVELOPE)) {
          fail(`${label} converted a spaced comparison into a contract envelope`)
        }
      }
      if (label === "borrow-view-two-pairs" || label === "borrow-view-param-return" ||
          label === "borrow-slot-lexical" || label === "borrow-duplicate-result" ||
          label === "borrow-comments" || label === "borrow-contextual-identifier") {
        assertClean(parsed, label)
        if (label === "borrow-view-two-pairs" || label === "borrow-view-param-return") {
          assertBorrowClause(parsed, bytes, label)
        }
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} CST signature is not deterministic`)
        }
      }
      if (label.startsWith("enum-")) {
        const repeated = invoke(probe, bytes, `${label}:repeat`, status, issue)
        if (parsed.signature !== repeated.signature) {
          fail(`${label} recovery CST signature is not deterministic`)
        }
      }
      if (status === "fatal" && parsed.result.issueCount !== 1) {
        fail(`${label} fatal result has ${parsed.result.issueCount} issues, expected one`)
      }
    }
    console.log(`Seed C parser: ${selectedIds.length} F0 input/output IDs + ${handCases.length} hand cases, caller-owned CST/range/recovery checks passed`)
  } finally {
    await rm(buildDirectory, { recursive: true, force: true })
  }
}

if (import.meta.main) await main()
