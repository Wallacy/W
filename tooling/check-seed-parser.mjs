import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const corpus = await Bun.file(resolve(import.meta.dir, "formatter-cases.json")).json()
const selectedIds = [
  "F0-module-run-entry",
  "F0-canonical-bytes",
  "F0-value-block-boundary",
  "F0-postfix-statement-boundaries",
  "F0-labeled-repeat-loop",
  "F0-binary-and-postfix-wrapping",
]

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
      if (inputParsed.signature !== second.signature) fail(`${id} CST signature is not deterministic`)
      if (inputParsed.nodes.length === 0 || outputParsed.nodes.length === 0) fail(`${id} has no CST nodes`)
    }

    const handCases = [
      ["nested-generic-and-shift", Buffer.from("fn f(x:Array<Array<u8>>):Array<Array<u8>>{return flags >> 2}\n"), "complete"],
      ["spaced-head", Buffer.from("fn f(x:Array /* note */ <u8>){return x}\n"), "recovered", 7],
      ["try-question", Buffer.from("fn f(){return try? load()}\n"), "complete"],
      ["postfix-question", Buffer.from("fn f(){value?.open?}\n"), "complete"],
      ["newline-continuation", Buffer.from("fn f(){let result = transform\n  (input)}\n"), "complete"],
      ["semicolon-boundary", Buffer.from("fn f(){a();(b)c();[d,e]}\n"), "complete"],
      ["missing-close", Buffer.from("fn f(){return 1\n"), "recovered", 2],
      ["stray-continuation", Buffer.from("fn f(){else}\n"), "recovered", 3],
      ["mixed-root", Buffer.from("module m\npackage {name: \"x\"}\n"), "fatal", 4],
      ["unsupported-root", Buffer.from("package {name: \"x\"}\n"), "fatal", 5],
      ["value-if-missing-else", Buffer.from("fn f():Stage{return if ready{.ok}}\n"), "recovered", 8],
      ["foreign-fail-closed", Buffer.from("fn f(){foreign c { host body }}\n"), "fatal", 9],
    ]
    for (const [label, bytes, status, issue] of handCases) {
      const parsed = invoke(probe, bytes, label, status, issue)
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
