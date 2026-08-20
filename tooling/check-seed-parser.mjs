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
  LABEL: 13,
  BREAK: 14,
  CONTINUE: 15,
  EXPRESSION: 17,
  TYPE: 18,
  WORD: 25,
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
      if (inputParsed.signature !== second.signature) fail(`${id} CST signature is not deterministic`)
      if (inputParsed.nodes.length === 0 || outputParsed.nodes.length === 0) fail(`${id} has no CST nodes`)
    }

    const witness = await formattingWitnessBytes()
    const witnessParsed = invoke(probe, witness, "formatting.w", "complete")
    const witnessRepeat = invoke(probe, witness, "formatting.w:repeat", "complete")
    assertFormattingWitness(witnessParsed, witness)
    if (witnessParsed.signature !== witnessRepeat.signature) {
      fail("formatting.w CST signature is not deterministic")
    }

    const handCases = [
      ["nested-generic-and-shift", Buffer.from("fn f(x:Array<Array<u8>>):Array<Array<u8>>{return flags >> 2}\n"), "complete"],
      ["spaced-head", Buffer.from("fn f(x:Array /* note */ <u8>){return x}\n"), "recovered", 7],
      ["try-question", Buffer.from("fn f(){return try? load()}\n"), "complete"],
      ["export-async-function", Buffer.from("export async fn load(kitchen:Kitchen):Menu throws KitchenError{return try await kitchen.loadMenu()}\n"), "complete"],
      ["export-async-trivia", Buffer.from("export /*a*/ async /*b*/ fn f(){}\n"), "complete"],
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
      ["const-function-stop", Buffer.from("const fn f(){}\n"), "fatal", 6],
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
           label.startsWith("export-async-") || label === "while-labeled-stop") &&
          issue !== undefined &&
          parsed.issues[0]?.kind !== issue) {
        fail(`${label} first issue ${parsed.issues[0]?.kind} != ${issue}`)
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
