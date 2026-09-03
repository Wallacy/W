import { mkdtemp, rm } from "node:fs/promises"
import { createHash } from "node:crypto"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const corpus = await Bun.file(resolve(import.meta.dir, "formatter-cases.json")).json()
const CST = Object.freeze({
  PARAMETER_LIST: 3,
  BLOCK: 7,
  LET_STATEMENT: 8,
  RETURN_STATEMENT: 9,
  EXPRESSION_STATEMENT: 16,
  EXPRESSION: 17,
  ARRAY: 20,
  SOURCE_PREFIX: 23,
  TRIVIA: 24,
  FOREIGN_BODY: 29,
  IMPORT: 31,
  STRUCT: 33,
  EXPECT_STATEMENT: 36,
  FOR_STATEMENT: 38,
  COMMIT_STATEMENT: 40,
  TYPE_DECLARATION: 45,
  ALIAS_DECLARATION: 46,
  SPAWN_STATEMENT: 55,
  VAR_STATEMENT: 65,
})
const FLAGS = Object.freeze({ RAW_LEAF: 1, TRIVIA: 2, ERROR: 4, MISSING: 8 })
const OPTIONAL_SEMICOLON_OWNERS = new Set([
  CST.LET_STATEMENT,
  CST.RETURN_STATEMENT,
  CST.EXPRESSION_STATEMENT,
  CST.IMPORT,
  CST.TYPE_DECLARATION,
  CST.ALIAS_DECLARATION,
  CST.EXPECT_STATEMENT,
  CST.FOR_STATEMENT,
  CST.COMMIT_STATEMENT,
  CST.SPAWN_STATEMENT,
  CST.VAR_STATEMENT,
])

function fail(message) {
  throw new Error(`seed formatter: ${message}`)
}

function run(command, args, options = {}) {
  const execution = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
    ...options,
  })
  if (execution.exitCode !== 0) {
    fail(`${command} ${args.join(" ")} failed: ${execution.stderr.toString().trim()}`)
  }
  return execution
}

function inputBytes(input, id) {
  if (!input || !Array.isArray(input.lines) || input.lines.length === 0) {
    fail(`${id} has no input lines`)
  }
  if (input.lines.some((line) => typeof line !== "string" || /[\r\n]/u.test(line))) {
    fail(`${id} input lines are invalid`)
  }
  const newline = input.newline === "crlf" ? "\r\n" : "\n"
  let source = input.lines.join(newline)
  if (input.finalNewline !== false) source += newline
  let bytes = Buffer.from(source, "utf8")
  if (input.bom === true) bytes = Buffer.concat([Buffer.from([0xef, 0xbb, 0xbf]), bytes])
  return bytes
}

function outputBytes(output, id) {
  if (!Array.isArray(output) || output.length === 0) fail(`${id} has no output`)
  if (output.some((line) => typeof line !== "string" || /[\r\n]/u.test(line))) {
    fail(`${id} output lines are invalid`)
  }
  return Buffer.from(`${output.join("\n")}\n`, "utf8")
}

function parseProbe(text, bytes, label) {
  const lines = text.trim().split(/\r?\n/u).filter(Boolean)
  const resultMatch =
    /^RESULT status=(\w+) nodes=(\d+) leaves=(\d+) issues=(\d+) consumed=(\d+) root=(\d+) length=(\d+)$/u.exec(lines[0] ?? "")
  if (!resultMatch) fail(`${label} parser probe has no RESULT line`)
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
    fail(`${label} parser probe length/consumed mismatch`)
  }
  const nodes = []
  const issues = []
  for (const line of lines.slice(1)) {
    const nodeMatch =
      /^NODE index=(\d+) kind=(\d+) flags=(\d+) start=(\d+) end=(\d+) first=(\d+) next=(\d+)$/u.exec(line)
    if (nodeMatch) {
      nodes.push({
        index: Number(nodeMatch[1]), kind: Number(nodeMatch[2]),
        flags: Number(nodeMatch[3]), start: Number(nodeMatch[4]),
        end: Number(nodeMatch[5]), first: Number(nodeMatch[6]),
        next: Number(nodeMatch[7]),
      })
      continue
    }
    const issueMatch =
      /^ISSUE index=(\d+) kind=(\d+) start=(\d+) end=(\d+) actual=(\d+) expected=(\d+)$/u.exec(line)
    if (issueMatch) issues.push({ kind: Number(issueMatch[2]) })
  }
  if (nodes.length !== result.nodeCount || issues.length !== result.issueCount) {
    fail(`${label} parser probe counts do not match RESULT`)
  }
  return { result, nodes, issues }
}

function parseSource(parserProbe, bytes, label) {
  const execution = Bun.spawnSync({ cmd: [parserProbe], cwd: root, stdin: bytes, stdout: "pipe", stderr: "pipe" })
  if (execution.exitCode !== 0) {
    fail(`${label} parser probe failed: ${execution.stderr.toString().trim()}`)
  }
  return parseProbe(execution.stdout.toString(), bytes, label)
}

function formatSource(formatterProbe, bytes, label) {
  const execution = Bun.spawnSync({ cmd: [formatterProbe], cwd: root, stdin: bytes, stdout: "pipe", stderr: "pipe" })
  if (execution.exitCode !== 0) {
    fail(`${label} formatter probe failed: ${execution.stderr.toString().trim()}`)
  }
  return Buffer.from(execution.stdout)
}

function normalized(parsed, bytes, label = "CST") {
  const nodes = parsed.nodes
  const sentinel = 0xffffffff
  const seen = new Set()
  const active = new Set()
  const encoded = (node) => Buffer.from(bytes.subarray(node.start, node.end)).toString("base64")

  function children(index) {
    const result = []
    let child = nodes[index].first
    const links = new Set()
    while (child !== sentinel) {
      if (!Number.isInteger(child) || child < 0 || child >= nodes.length) {
        fail(`${label} child link ${child} is out of range`)
      }
      if (links.has(child)) fail(`${label} sibling cycle at ${child}`)
      links.add(child)
      result.push(child)
      child = nodes[child].next
    }
    return result
  }

  function visit(index, parentIndex = sentinel, siblingIndex = -1, siblingList = []) {
    if (!Number.isInteger(index) || index < 0 || index >= nodes.length) {
      fail(`${label} node ${index} is out of range`)
    }
    if (active.has(index)) fail(`${label} child cycle at ${index}`)
    if (seen.has(index)) fail(`${label} node ${index} is multiply reachable`)
    active.add(index)
    seen.add(index)
    const node = nodes[index]
    if ((node.flags & FLAGS.ERROR) !== 0 || (node.flags & FLAGS.MISSING) !== 0) {
      fail(`${label} contains ERROR/MISSING node ${index}`)
    }
    let signature = null
    if ((node.flags & FLAGS.RAW_LEAF) !== 0) {
      // Trivia and the source-prefix view carry no semantic structure.
      if ((node.flags & FLAGS.TRIVIA) === 0 && node.kind !== CST.SOURCE_PREFIX && node.start !== node.end) {
        const text = Buffer.from(bytes.subarray(node.start, node.end)).toString("utf8")
        const parent = parentIndex === sentinel ? null : nodes[parentIndex]
        const nextSemantic = siblingList.slice(siblingIndex + 1).find((candidate) => {
          const candidateNode = nodes[candidate]
          if ((candidateNode.flags & FLAGS.RAW_LEAF) === 0) return true
          if ((candidateNode.flags & FLAGS.TRIVIA) !== 0 || candidateNode.kind === CST.SOURCE_PREFIX) return false
          return candidateNode.start !== candidateNode.end
        })
        const nextText = nextSemantic === undefined
          ? ""
          : Buffer.from(bytes.subarray(nodes[nextSemantic].start, nodes[nextSemantic].end)).toString("utf8")
        // Statement/declaration terminators are optional in this seed. The
        // array repeat separator is syntax, so its ARRAY parent is retained.
        const optionalSemicolon = text === ";" && parent !== null && OPTIONAL_SEMICOLON_OWNERS.has(parent.kind)
        const optionalTrailingComma = text === "," &&
          (parent?.kind === CST.PARAMETER_LIST || parent?.kind === CST.EXPRESSION) && nextText === ")"
        if (optionalSemicolon || optionalTrailingComma) {
          signature = null
        } else {
          signature = `L${node.kind}:${encoded(node)}`
        }
      }
      if (node.first !== sentinel) fail(`${label} raw leaf ${index} has children`)
    } else {
      const parts = []
      const childIndices = children(index)
      for (let childPosition = 0; childPosition < childIndices.length; childPosition += 1) {
        const child = childIndices[childPosition]
        const childSignature = visit(child, index, childPosition, childIndices)
        if (childSignature !== null) parts.push(childSignature)
      }
      signature = `O${node.kind}[${parts.join(",")}]`
    }
    active.delete(index)
    return signature
  }

  const signature = visit(parsed.result.root)
  // Every parser node must be reachable from DOCUMENT. This catches a
  // formatter/parser owner reparenting that happens to preserve flat leaves.
  for (let index = 0; index < nodes.length; index += 1) {
    if (!seen.has(index)) fail(`${label} node ${index} is unreachable from DOCUMENT`)
  }
  return signature
}

function assertComplete(parsed, label) {
  if (parsed.result.status !== "complete" || parsed.result.issueCount !== 0) {
    fail(`${label} is not COMPLETE/0 (${parsed.result.status}/${parsed.result.issueCount})`)
  }
}

function runProbeSet(formatterProbe, parserProbe) {
  let exact = 0
  for (const fixture of corpus.cases) {
    const source = inputBytes(fixture.input, fixture.id)
    const expected = outputBytes(fixture.output, fixture.id)
    const sourceParsed = parseSource(parserProbe, source, fixture.id)
    assertComplete(sourceParsed, `${fixture.id} input`)
    const formatted = formatSource(formatterProbe, source, fixture.id)
    if (!formatted.equals(expected)) {
      fail(`${fixture.id} output bytes differ from formatter oracle`)
    }
    const formattedParsed = parseSource(parserProbe, formatted, `${fixture.id} formatted`)
    assertComplete(formattedParsed, `${fixture.id} formatted`)
    const sourceSignature = normalized(sourceParsed, source, `${fixture.id} source`)
    const formattedSignature = normalized(formattedParsed, formatted, `${fixture.id} formatted`)
    if (sourceSignature !== formattedSignature) {
      let firstDifference = 0
      while (firstDifference < sourceSignature.length &&
             firstDifference < formattedSignature.length &&
             sourceSignature[firstDifference] === formattedSignature[firstDifference]) {
        firstDifference += 1
      }
      fail(`${fixture.id} normalized CST token/owner signature changed at ${firstDifference}: ` +
           `${sourceSignature.slice(firstDifference, firstDifference + 80)} != ` +
           `${formattedSignature.slice(firstDifference, firstDifference + 80)}`)
    }
    const repeated = formatSource(formatterProbe, formatted, `${fixture.id} repeat`)
    if (!repeated.equals(formatted)) fail(`${fixture.id} formatter is not idempotent`)
    exact += 1
  }
  return exact
}

function assertForeignPreserved(parserProbe, source, formatted) {
  const sourceParsed = parseSource(parserProbe, source, "foreign source")
  const formattedParsed = parseSource(parserProbe, formatted, "foreign formatted")
  const bodies = (parsed, bytes) => parsed.nodes
    .filter((node) => node.kind === CST.FOREIGN_BODY && (node.flags & FLAGS.RAW_LEAF) !== 0)
    .map((node) => Buffer.from(bytes.subarray(node.start, node.end)).toString("base64"))
  if (JSON.stringify(bodies(sourceParsed, source)) !== JSON.stringify(bodies(formattedParsed, formatted))) {
    fail("foreign body bytes changed")
  }
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-formatter-"))
try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory])
  run("ctest", ["--test-dir", buildDirectory, "--output-on-failure"])
  const extension = process.platform === "win32" ? ".exe" : ""
  const formatterProbe = join(buildDirectory, `w_seed_formatter_probe${extension}`)
  const parserProbe = join(buildDirectory, `w_seed_parser_probe${extension}`)
  const exact = runProbeSet(formatterProbe, parserProbe)

  const callables = await Bun.file(resolve(root, "reference", "last-light", "callables.w")).bytes()
  const callablesDigest = createHash("sha256").update(callables).digest("hex")
  if (callablesDigest !== "00357e941c038cdebdebfbf12f12a0e327f6899765946b81c77680b4c0b7a154") {
    fail(`callables source digest changed: ${callablesDigest}`)
  }
  const callablesParsed = parseSource(parserProbe, callables, "callables source")
  assertComplete(callablesParsed, "callables source")
  const formattedCallables = formatSource(formatterProbe, callables, "callables")
  const formattedCallablesParsed = parseSource(parserProbe, formattedCallables, "callables formatted")
  assertComplete(formattedCallablesParsed, "callables formatted")
  if (normalized(callablesParsed, callables, "callables source") !==
      normalized(formattedCallablesParsed, formattedCallables, "callables formatted")) {
    fail("callables normalized CST token/owner signature changed")
  }
  if (!formatSource(formatterProbe, formattedCallables, "callables repeat").equals(formattedCallables)) {
    fail("callables formatter is not idempotent")
  }
  const mutationText = Buffer.from(callables).toString("utf8")
    .replace("import iec from std", "import  iec  from   std")
    .replace("  var nextTicket = ticketSequence(40)", "\n\n  var nextTicket = ticketSequence(40)")
  const mutated = Buffer.from(mutationText)
  const mutatedParsed = parseSource(parserProbe, mutated, "callables layout mutation")
  assertComplete(mutatedParsed, "callables layout mutation")
  if (!formatSource(formatterProbe, mutated, "callables mutation").equals(formattedCallables)) {
    fail("callables layout mutation did not converge")
  }

  const compactBlock = Buffer.from("fn f(){let value=1}\n")
  const multilineBlock = Buffer.from("fn f(){\n  let value=1\n}\n")
  if (!formatSource(formatterProbe, compactBlock, "compact block").equals(
      formatSource(formatterProbe, multilineBlock, "multiline block"))) {
    fail("compact and multiline one-statement blocks diverged")
  }
  const compactParams = Buffer.from("fn f(a:T,b:U){return a}\n")
  const multilineParams = Buffer.from("fn f(\n  a:T,\n  b:U\n){return a}\n")
  if (!formatSource(formatterProbe, compactParams, "compact params").equals(
      formatSource(formatterProbe, multilineParams, "multiline params"))) {
    fail("compact and multiline short parameter lists diverged")
  }
  const nearLimitHead = "fn " + "n".repeat(90)
  const overLimitHead = "fn " + "n".repeat(110)
  const nearLimit = formatSource(
    formatterProbe, Buffer.from(`${nearLimitHead}(a:T,b:U){return a}\n`),
    "near-limit function head",
  )
  const overLimit = formatSource(
    formatterProbe, Buffer.from(`${overLimitHead}(a:T,b:U){return a}\n`),
    "over-limit function head",
  )
  const nearFirstLine = nearLimit.toString("utf8").split("\n", 1)[0]
  const overFirstLine = overLimit.toString("utf8").split("\n", 1)[0]
  if (nearFirstLine.length > 120 || overFirstLine.length > 120 ||
      !overLimit.toString("utf8").includes("\n  a: T")) {
    fail("120-column function-head boundary is not canonical")
  }
  const longParameters = Array.from({length: 14}, (_, index) =>
    `parameter${index}:LongType`).join(",")
  const longCompact = Buffer.from(`fn long(${longParameters}){return parameter0}\n`)
  const longMultiline = Buffer.from(`fn long(\n${longParameters.split(",").map((item, index, items) =>
    `  ${item}${index + 1 < items.length ? "," : ""}`).join("\n")}\n){return parameter0}\n`)
  const longCompactFormatted = formatSource(formatterProbe, longCompact, "long compact params")
  const longMultilineFormatted = formatSource(formatterProbe, longMultiline, "long multiline params")
  if (!longCompactFormatted.equals(longMultilineFormatted) ||
      !longCompactFormatted.toString("utf8").includes("\n  parameter0: LongType")) {
    fail("long parameter lists did not converge to canonical multiline layout")
  }

  const formatting = await Bun.file(resolve(root, "reference", "last-light", "formatting.w")).bytes()
  const formattingParsed = parseSource(parserProbe, formatting, "formatting source")
  assertComplete(formattingParsed, "formatting source")
  const foreignCase = corpus.cases.find((fixture) => fixture.id === "F0-opaque-foreign-body")
  const foreignSource = inputBytes(foreignCase.input, foreignCase.id)
  assertForeignPreserved(parserProbe, foreignSource, formatSource(formatterProbe, foreignSource, foreignCase.id))
  console.log(`Seed C formatter: ${exact}/${corpus.cases.length} oracle outputs, COMPLETE/0 reparses, normalized CST identity, idempotence, callables/foreign/capacity gates passed`)
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
