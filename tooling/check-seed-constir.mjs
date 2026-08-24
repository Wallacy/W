import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const suffix = process.platform === "win32" ? ".exe" : ""

function fail(message) {
  throw new Error(`seed ConstIR: ${message}`)
}

function run(command, args, options = {}) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
    ...options,
  })
  if (result.exitCode !== 0) {
    const stderr = result.stderr.toString().trim()
    fail(`${command} ${args.join(" ")} failed${stderr ? `: ${stderr}` : ""}`)
  }
  return result.stdout.toString()
}

function probe(executable, source, label) {
  const input = Buffer.isBuffer(source) ? source : Buffer.from(source, "utf8")
  const first = Bun.spawnSync({ cmd: [executable], cwd: root, stdin: input, stdout: "pipe", stderr: "pipe" })
  if (first.exitCode !== 0 && !first.stdout.toString().includes("CONSTIR ")) {
    fail(`${label} probe failed: ${first.stderr.toString().trim()}`)
  }
  const second = Bun.spawnSync({ cmd: [executable], cwd: root, stdin: input, stdout: "pipe", stderr: "pipe" })
  if (second.exitCode !== first.exitCode || first.stdout.toString() !== second.stdout.toString())
    fail(`${label} output is not deterministic`)
  return first.stdout.toString()
}

function line(output, prefix, label) {
  const found = output.split(/\r?\n/u).find((item) => item.startsWith(prefix))
  if (!found) fail(`${label} has no ${prefix.trim()} line`)
  return found
}

function parseConstir(output, label) {
  const value = line(output, "CONSTIR ", label)
  const match = /^CONSTIR status=(\w+) measured=(\w+) functions=(\d+) parameters=(\d+) nodes=(\d+) calls=(\d+) switch=(\d+) membership=(\d+) statements=(\d+) locals=(\d+) diagnostics=(\d+) receipt=(\d+)$/u.exec(value)
  if (!match) fail(`${label} has an invalid CONSTIR line: ${value}`)
  return {
    status: match[1], measured: match[2], functions: Number(match[3]),
    parameters: Number(match[4]), nodes: Number(match[5]), calls: Number(match[6]),
    switchArms: Number(match[7]), membership: Number(match[8]),
    statements: Number(match[9]), locals: Number(match[10]),
    diagnostics: Number(match[11]), receipt: Number(match[12]),
  }
}

function parseFunction(output, label) {
  const value = line(output, "FUNCTION ", label)
  const match = /^FUNCTION origin=(\d+) frontend=(\d+) typed=(\d+) lowerable=(\d+) digest=([0-9a-f]{64}) nodes=(\d+)$/u.exec(value)
  if (!match) fail(`${label} has an invalid FUNCTION line: ${value}`)
  return {
    origin: Number(match[1]), frontend: Number(match[2]), typed: Number(match[3]),
    lowerable: match[4] === "1", digest: match[5], nodes: Number(match[6]),
  }
}

function parseFunctionForFrontend(output, frontend, label) {
  const value = output.split(/\r?\n/u)
    .find((item) => item.startsWith("FUNCTION ") &&
      item.includes(` frontend=${frontend} `))
  if (!value) fail(`${label} has no FUNCTION frontend=${frontend} line`)
  const match = /^FUNCTION origin=(\d+) frontend=(\d+) typed=(\d+) lowerable=(\d+) digest=([0-9a-f]{64}) nodes=(\d+)$/u.exec(value)
  if (!match) fail(`${label} has an invalid FUNCTION line: ${value}`)
  return {
    origin: Number(match[1]), frontend: Number(match[2]), typed: Number(match[3]),
    lowerable: match[4] === "1", digest: match[5], nodes: Number(match[6]),
  }
}

function parseReceiptDigest(output, label) {
  const value = line(output, "RECEIPT ", label)
  const match = /^RECEIPT digest=([0-9a-f]{64})$/u.exec(value)
  if (!match) fail(`${label} has an invalid receipt digest line: ${value}`)
  return match[1]
}

function evalLines(output) {
  return output.split(/\r?\n/u).filter((item) => item.startsWith("EVAL "))
}

function pathLines(output) {
  return output.split(/\r?\n/u).filter((item) => item.startsWith("PATH "))
}

function assertCanMove(output, label) {
  const parsed = parseConstir(output, label)
  const functionRecord = parseFunction(output, label)
  const receiptDigest = parseReceiptDigest(output, label)
  if (parsed.status !== "ok" || parsed.measured !== "ok" || parsed.functions !== 1 ||
      parsed.parameters !== 2 || parsed.nodes === 0 || parsed.switchArms !== 6 ||
      parsed.membership !== 8 || parsed.diagnostics !== 0 || parsed.receipt === 0 ||
      !functionRecord.lowerable || functionRecord.nodes === 0)
    fail(`${label} did not lower canMove as ConstIR D1`)
  const expected = new Map([
    ["0:1", true], ["0:5", true], ["1:2", true], ["1:5", true],
    ["2:3", true], ["2:5", true], ["3:4", true], ["3:5", true],
  ])
  const lines = evalLines(output)
  if (lines.length !== 36) fail(`${label} evaluated ${lines.length} combinations`)
  for (const item of lines) {
    const match = /^EVAL from=(\d+) to=(\d+) ok=(\d+) diag=(\d+) steps=(\d+)$/u.exec(item)
    if (!match) fail(`${label} has an invalid EVAL line: ${item}`)
    const key = `${match[1]}:${match[2]}`
    const actual = match[3] === "1"
    if (actual !== (expected.get(key) ?? false) || match[4] !== "0" || Number(match[5]) === 0)
      fail(`${label} has wrong canMove result for ${key}`)
  }
  return { parsed, functionRecord, receiptDigest, evals: lines }
}

function assertStagePath(output, label) {
  const parsed = parseConstir(output, label)
  const canMove = parseFunctionForFrontend(output, 0, label)
  const path = parseFunctionForFrontend(output, 1, label)
  const receiptDigest = parseReceiptDigest(output, label)
  if (parsed.status !== "ok" || parsed.measured !== "ok" || parsed.functions !== 2 ||
      parsed.parameters !== 3 || parsed.nodes === 0 || parsed.calls !== 2 ||
      parsed.switchArms !== 6 || parsed.membership !== 8 || parsed.statements !== 6 ||
      parsed.locals !== 1 || parsed.diagnostics !== 0 || parsed.receipt === 0 ||
      !canMove.lowerable || canMove.nodes === 0 || !path.lowerable || path.nodes === 0)
    fail(`${label} did not lower isValidStagePath as statement ConstIR`)
  const expected = new Map([
    ["empty", false], ["singleton", true], ["default", true],
    ["prefix", true], ["cancel-accepted", true], ["cancel-reserving", true],
    ["cancel-preparing", true], ["cancel-serving", true], ["skipped", false],
    ["reverse", false], ["terminal-out", false], ["duplicate", false],
  ])
  const lines = pathLines(output)
  if (lines.length !== expected.size) fail(`${label} evaluated ${lines.length} stage paths`)
  for (const item of lines) {
    const match = /^PATH case=([a-z-]+) status=(\d+) kind=(\d+) bool=(\d+) diag=(\d+) steps=(\d+) heap=(\d+)$/u.exec(item)
    if (!match) fail(`${label} has an invalid PATH line: ${item}`)
    const name = match[1]
    const actual = match[4] === "1"
    if (!expected.has(name) || actual !== expected.get(name) || match[2] !== "0" ||
        match[3] !== "1" || match[5] !== "0" || Number(match[6]) === 0 ||
        match[7] !== "0")
      fail(`${label} has wrong isValidStagePath result for ${name}`)
  }
  return { parsed, canMove, path, receiptDigest, paths: lines }
}

function fragment(bytes, startMarker, endMarker, label) {
  const startNeedle = Buffer.from(startMarker, "utf8")
  const endNeedle = Buffer.from(endMarker, "utf8")
  const start = bytes.indexOf(startNeedle)
  const end = bytes.indexOf(endNeedle, start + startNeedle.length)
  if (start < 0 || end < 0 || end <= start || bytes.indexOf(startNeedle, start + 1) >= 0)
    fail(`${label} markers are missing or duplicated`)
  return bytes.subarray(start, end)
}

const build = await mkdtemp(join(tmpdir(), "w-seed-constir-check-"))
try {
  run("cmake", ["-S", seedDirectory, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", build, "--target", "w_seed_constir_probe", "w_seed_constir_tests", "--", "-j", "2"])
  run(join(build, `w_seed_constir_tests${suffix}`), [])
  const executable = join(build, `w_seed_constir_probe${suffix}`)
  const domain = Buffer.from(await Bun.file(resolve(root, "reference/last-light/domain.w")).arrayBuffer())
  const serviceStage = fragment(domain, "export enum ServiceStage {", "export alias CancelledStage", "ServiceStage")
  const cancelled = fragment(domain, "export alias CancelledStage =", "export const fn canMove", "CancelledStage")
  const canMove = fragment(domain, "export const fn canMove", "export const fn isValidStagePath", "canMove")
  const isValidStagePath = fragment(domain, "export const fn isValidStagePath", "export enum PartySize", "isValidStagePath")
  const witness = Buffer.concat([serviceStage, cancelled, canMove])
  const first = assertCanMove(probe(executable, witness, "domain.w canMove"), "domain.w canMove")
  const second = assertCanMove(probe(executable, witness, "domain.w canMove repeat"), "domain.w canMove repeat")
  if (first.functionRecord.digest !== second.functionRecord.digest ||
      first.parsed.receipt !== second.parsed.receipt ||
      first.receiptDigest !== second.receiptDigest ||
      first.evals.join("\n") !== second.evals.join("\n"))
    fail("repeat lower/eval changed digest, receipt size, value, or counters")

  const pathWitness = Buffer.concat([serviceStage, cancelled, canMove, isValidStagePath])
  const pathFirst = assertStagePath(probe(executable, pathWitness, "domain.w isValidStagePath"),
                                    "domain.w isValidStagePath")
  const pathSecond = assertStagePath(probe(executable, pathWitness, "domain.w isValidStagePath repeat"),
                                     "domain.w isValidStagePath repeat")
  if (pathFirst.path.digest !== pathSecond.path.digest ||
      pathFirst.receiptDigest !== pathSecond.receiptDigest ||
      pathFirst.parsed.receipt !== pathSecond.parsed.receipt ||
      pathFirst.paths.join("\n") !== pathSecond.paths.join("\n"))
    fail("repeat stage-path lower/eval changed digest, receipt size, value, or counters")

  const witnessText = witness.toString("utf8")
  const renamed = witnessText.replace("from current", "from sourceStage")
    .replace("to next", "to targetStage")
    .replaceAll("switch current", "switch sourceStage")
    .replaceAll("next in", "targetStage in")
  const renamedResult = assertCanMove(probe(executable, renamed, "parameter rename"), "parameter rename")
  if (renamedResult.functionRecord.digest !== first.functionRecord.digest)
    fail("parameter rename changed the semantic ConstIR digest")
  const trivia = witnessText.replaceAll("\n", "\n\n  ")
  const triviaResult = assertCanMove(probe(executable, trivia, "trivia variation"), "trivia variation")
  if (triviaResult.functionRecord.digest !== first.functionRecord.digest)
    fail("trivia changed the semantic ConstIR digest")
  const changed = witnessText.replace("next in (.reserving, .cancelled)", "next in (.preparing, .cancelled)")
  const changedOutput = probe(executable, changed, "semantic variation")
  const changedParsed = parseConstir(changedOutput, "semantic variation")
  const changedFunction = parseFunction(changedOutput, "semantic variation")
  if (changedParsed.status !== "ok" || changedParsed.diagnostics !== 0 ||
      !changedFunction.lowerable || changedFunction.nodes === 0 ||
      changedFunction.digest === first.functionRecord.digest)
    fail("semantic body change did not change the ConstIR digest")

  const manyNames = Array.from({ length: 70 }, (_, index) => `c${index}`)
  const manySource = `enum Many { ${manyNames.join(" ")} }\n` +
    `const fn contains(value: Many): Bool { return value in (${manyNames.map((name) => `.${name}`).join(", ")}) }\n`
  const manyOutput = probe(executable, manySource, "membership over 64 cases")
  const many = parseConstir(manyOutput, "membership over 64 cases")
  if (many.status !== "ok" || many.membership !== 70 || many.diagnostics !== 0)
    fail("membership over 64 cases was truncated or not lowerable")

  const unsupported = probe(executable, "const fn bad(value: u32): u32 { let local = value return local }\n", "non-lowerable body")
  const unsupportedParsed = parseConstir(unsupported, "non-lowerable body")
  const unsupportedFunction = parseFunction(unsupported, "non-lowerable body")
  const unsupportedDiagnostics = unsupported.split(/\r?\n/u)
    .filter((item) => item.startsWith("DIAG "))
  if (unsupportedParsed.diagnostics !== 1 || unsupportedFunction.lowerable || unsupportedFunction.nodes !== 0 ||
      unsupportedDiagnostics.length !== 1 || !unsupportedDiagnostics[0].startsWith("DIAG code=1 ") ||
      !unsupported.includes("CONSTIR status=ok"))
    fail("non-lowerable function did not publish one W-CONST-0001 root")
} finally {
  await rm(build, { recursive: true, force: true })
}

console.log("seed ConstIR: source-backed canMove/isValidStagePath, digest, membership, and barriers passed")
