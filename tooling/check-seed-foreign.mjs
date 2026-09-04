import { mkdtemp, rm } from "node:fs/promises"
import { createHash } from "node:crypto"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { runForeignBodyOperations, scanForeignBody } from "./foreign-body-machine.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const corpus = await Bun.file(resolve(import.meta.dir, "foreign-body-cases.json")).json()

function fail(message) {
  throw new Error(`seed foreign: ${message}`)
}

function run(command, args) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (result.exitCode !== 0) {
    fail(`${command} ${args.join(" ")} failed: ${result.stderr.toString().trim()}`)
  }
  return result
}

function operationBytes(operation) {
  if (typeof operation.source === "string") return Buffer.from(operation.source, "utf8")
  if (typeof operation.sourceHex === "string" && /^(?:[0-9a-fA-F]{2})*$/.test(operation.sourceHex)) {
    return Buffer.from(operation.sourceHex, "hex")
  }
  fail("scan operation has no byte source")
}

function parseProbe(text, label) {
  const line = text.trim()
  const accepted = /^RESULT accepted=1 profile=(\d+) body_start=(\d+) body_end=(\d+) close=(\d+) next=(\d+) max_body=(\d+) max_depth=(\d+) observed_depth=(\d+) terminal=(\d+) digest=([0-9a-f]{64})$/u.exec(line)
  if (accepted) {
    return {
      accepted: true,
      profile: Number(accepted[1]),
      bodyStart: Number(accepted[2]),
      bodyEnd: Number(accepted[3]),
      close: Number(accepted[4]),
      next: Number(accepted[5]),
      maxBody: Number(accepted[6]),
      maxDepth: Number(accepted[7]),
      observedDepth: Number(accepted[8]),
      terminal: Number(accepted[9]),
      digest: accepted[10],
    }
  }
  const rejected = /^RESULT accepted=0 kind=(\d+) terminal=(\d+) primary_start=(\d+) primary_end=(\d+) has_close=(\d+) close=(\d+) max_body=(\d+) max_depth=(\d+)$/u.exec(line)
  if (rejected) {
    return {
      accepted: false,
      kind: Number(rejected[1]),
      terminal: Number(rejected[2]),
      primaryStart: Number(rejected[3]),
      primaryEnd: Number(rejected[4]),
      hasClose: Number(rejected[5]) !== 0,
      close: Number(rejected[6]),
      maxBody: Number(rejected[7]),
      maxDepth: Number(rejected[8]),
    }
  }
  fail(`${label} invalid probe output ${JSON.stringify(line)}`)
}

function expectedErrorKind(reason) {
  return {
    invalidUtf8: 3,
    nulByte: 4,
    missingOpen: 5,
    missingClose: 6,
    doubleQuote: 7,
    singleQuote: 7,
    newlineInLiteral: 7,
    blockComment: 8,
    preprocessorDirective: 9,
    lineSplice: 10,
    bodyBytes: 11,
    braceDepth: 12,
  }[reason]
}

function expectedScan(operation) {
  try {
    const scan = scanForeignBody(operation, "c-inline-1")
    return { accepted: true, scan }
  } catch (error) {
    return { accepted: false, reason: error?.facts?.reason }
  }
}

function expandOperation(operation) {
  if (operation.op !== "resolve" || operation.profile === undefined) return operation
  const profile = corpus.profiles?.[operation.profile]
  if (!profile) fail(`unknown resolve profile ${operation.profile}`)
  return { op: "resolve", ...profile }
}

function checkScan(operation, probe, label) {
  const bytes = operationBytes(operation)
  const maxBody = operation.maximumBodyBytes ?? 64 * 1024
  const maxDepth = operation.maximumBraceDepth ?? 256
  const result = Bun.spawnSync({
    cmd: [probe, `--max-body=${maxBody}`, `--max-depth=${maxDepth}`],
    cwd: root,
    stdin: bytes,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (result.exitCode !== 0) fail(`${label} probe exited ${result.exitCode}`)
  const actual = parseProbe(result.stdout.toString(), label)
  const expected = expectedScan({ ...operation, maximumBodyBytes: maxBody, maximumBraceDepth: maxDepth })
  if (actual.accepted !== expected.accepted) {
    fail(`${label} accepted=${actual.accepted} expected=${expected.accepted}`)
  }
  if (!expected.accepted) {
    const kind = expectedErrorKind(expected.reason)
    if (kind === undefined || actual.kind !== kind) {
      fail(`${label} error kind ${actual.kind} does not map ${expected.reason}`)
    }
    if (actual.primaryStart > bytes.length || actual.primaryEnd < actual.primaryStart) {
      fail(`${label} primary span is outside source`)
    }
    return { actual, expected, bytes }
  }
  const scan = expected.scan
  const expectedDigest = createHash("sha256").update(bytes.subarray(scan.bodyStart, scan.bodyEnd)).digest("hex")
  if (actual.profile !== 0 || actual.bodyStart !== scan.bodyStart || actual.bodyEnd !== scan.bodyEnd ||
      actual.close !== scan.closeOffset || actual.next !== scan.nextOffset ||
      actual.observedDepth !== scan.maximumDepthObserved || actual.digest !== expectedDigest) {
    fail(`${label} source-validation fields differ from FB0 scan oracle`)
  }
  if (actual.bodyEnd - actual.bodyStart > maxBody || actual.maxBody !== maxBody || actual.maxDepth !== maxDepth) {
    fail(`${label} limits are not caller-owned and explicit`)
  }
  if (actual.next > bytes.length || bytes[actual.close] !== 0x7d) {
    fail(`${label} close/next span is outside source`)
  }
  return { actual, expected, bytes }
}

async function checkHardwareWitness(scannerProbe, parserProbe) {
  const source = Buffer.from(await Bun.file(resolve(root, "reference", "last-light", "hardware.w")).arrayBuffer())
  const marker = Buffer.from("unsafe fn<C> legacyProbeStatus", "utf8")
  const start = source.indexOf(marker)
  const duplicate = start >= 0 ? source.indexOf(marker, start + 1) : -1
  if (start < 0 || duplicate >= 0) fail("hardware witness marker is missing or duplicated")
  const opening = source.indexOf(0x7b, start + marker.length)
  if (opening < 0) fail("hardware witness opening brace is missing")
  const scan = checkScan(
    { source: source.subarray(opening).toString("utf8") },
    scannerProbe,
    "hardware.w:legacyProbeStatus",
  )
  if (!scan.actual.accepted) fail("hardware witness scanner rejected current source")
  const bodyStart = opening + scan.actual.bodyStart
  const bodyEnd = opening + scan.actual.bodyEnd
  const close = opening + scan.actual.close
  const next = opening + scan.actual.next
  const bodyDigest = `sha256:${createHash("sha256").update(source.subarray(bodyStart, bodyEnd)).digest("hex")}`
  if (bodyStart !== 1135 || bodyEnd !== 1338 || close !== 1338 || next !== 1339 ||
      bodyDigest !== "sha256:8aede5643732489afcbb39ebfc4080689736825e9d11aca275a271fe678b5567") {
    fail("hardware witness body range or digest drifted")
  }
  const functionBytes = source.subarray(start, next)
  const result = Bun.spawnSync({
    cmd: [parserProbe],
    cwd: root,
    stdin: functionBytes,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (result.exitCode !== 0) fail(`hardware parser probe exited ${result.exitCode}`)
  const lines = result.stdout.toString().trim().split(/\r?\n/u)
  const resultMatch = /^RESULT status=(\w+) nodes=(\d+) leaves=(\d+) issues=(\d+) consumed=(\d+) root=(\d+) length=(\d+)$/u.exec(lines[0] ?? "")
  if (!resultMatch || resultMatch[1] !== "complete" || Number(resultMatch[4]) !== 0 ||
      Number(resultMatch[5]) !== functionBytes.length) {
    fail("hardware parser witness is not complete and lossless")
  }
  const nodes = lines.slice(1).map((line) => {
    const match = /^NODE index=(\d+) kind=(\d+) flags=(\d+) start=(\d+) end=(\d+) first=(\d+) next=(\d+)$/u.exec(line)
    return match ? {
      kind: Number(match[2]), flags: Number(match[3]), start: Number(match[4]), end: Number(match[5]),
    } : null
  }).filter((node) => node !== null)
  const bodies = nodes.filter((node) => node.kind === 29)
  if (bodies.length !== 1 || bodies[0].start !== bodyStart - start ||
      bodies[0].end !== bodyEnd - start || (bodies[0].flags & 1) === 0 ||
      !source.subarray(start + bodies[0].start, start + bodies[0].end)
        .equals(source.subarray(bodyStart, bodyEnd))) {
    fail("hardware parser witness FOREIGN_BODY leaf is not source-exact")
  }
  if (nodes.filter((node) => node.kind === 63).length !== 1 ||
      nodes.filter((node) => node.kind === 64).length !== 1) {
    fail("hardware parser witness language/owner nodes are incomplete")
  }
}

function checkTamper(probe) {
  const source = Buffer.from("{return 0;}")
  const original = Bun.spawnSync({
    cmd: [probe], cwd: root, stdin: source, stdout: "pipe", stderr: "pipe",
  })
  const parsed = parseProbe(original.stdout.toString(), "tamper:original")
  if (!parsed.accepted) fail("tamper: original scan rejected")
  const mutated = Buffer.from(source)
  mutated[parsed.bodyStart] ^= 1
  const changed = createHash("sha256").update(mutated.subarray(parsed.bodyStart, parsed.bodyEnd)).digest("hex")
  if (changed === parsed.digest) fail("tamper: body digest did not change")
  if (parsed.bodyStart !== 1 || parsed.bodyEnd !== 10 || parsed.close !== 10 || parsed.next !== 11) {
    fail("tamper: canonical span changed")
  }
}

function runProbe(probe, bytes, label) {
  const result = Bun.spawnSync({
    cmd: [probe], cwd: root, stdin: bytes, stdout: "pipe", stderr: "pipe",
  })
  if (result.exitCode !== 0) fail(`${label} probe exited ${result.exitCode}`)
  return parseProbe(result.stdout.toString(), label)
}

function checkSuffixPreservation(probe) {
  const base = Buffer.from("{x}", "ascii")
  const original = runProbe(probe, base, "suffix:original")
  if (!original.accepted) fail("suffix: original scan rejected")
  const suffixes = [Buffer.from([0xff]), Buffer.from([0])]
  for (const [index, suffix] of suffixes.entries()) {
    const actual = runProbe(probe, Buffer.concat([base, suffix]), `suffix:after-${index}`)
    if (!actual.accepted || actual.profile !== original.profile ||
        actual.bodyStart !== original.bodyStart || actual.bodyEnd !== original.bodyEnd ||
        actual.close !== original.close || actual.next !== original.next ||
        actual.digest !== original.digest) {
      fail(`suffix: invalid byte after close changed source-validation result ${index}`)
    }
  }
  const invalidBody = [
    Buffer.from([0x7b, 0xc0, 0x80, 0x7d]),
    Buffer.from([0x7b, 0x78, 0x00, 0x7d]),
  ]
  const expectedKinds = [3, 4]
  for (const [index, bytes] of invalidBody.entries()) {
    const actual = runProbe(probe, bytes, `suffix:inside-${index}`)
    if (actual.accepted || actual.kind !== expectedKinds[index]) {
      fail(`suffix: invalid byte inside body was not rejected ${index}`)
    }
  }
}

async function main() {
  if (corpus.$schema !== "w-foreign-body-cases-1" || corpus.status !== "design-oracle-input") {
    fail("FB0 corpus is not the design-oracle input")
  }
  const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-foreign-"))
  try {
    run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
    run("cmake", ["--build", buildDirectory])
    run("ctest", ["--test-dir", buildDirectory, "--output-on-failure"])
    const suffix = process.platform === "win32" ? ".exe" : ""
    const probe = join(buildDirectory, `w_seed_foreign_probe${suffix}`)
    const parserProbe = join(buildDirectory, `w_seed_parser_probe${suffix}`)
    if (!(await Bun.file(probe).exists())) fail(`probe is missing at ${probe}`)
    if (!(await Bun.file(parserProbe).exists())) fail(`parser probe is missing at ${parserProbe}`)

    let scanCount = 0
    for (const item of corpus.cases ?? []) {
      const operations = (item.operations ?? []).map(expandOperation)
      const expected = runForeignBodyOperations(operations)
      const scannerProfile = operations.find((operation) => operation.op === "resolve")?.scannerProfile
      let operationIndex = 0
      for (const operation of operations) {
        if (operation.op !== "scan" && operation.op !== "replaceSource") continue
        if (scannerProfile !== "c-inline-1") continue
        checkScan(operation, probe, `${item.id}:${operationIndex}`)
        scanCount += 1
        operationIndex += 1
      }
      if (item.kind === "accepted" && expected.status !== "accepted") {
        fail(`${item.id} FB0 operation chain did not remain accepted`)
      }
    }
    checkTamper(probe)
    checkSuffixPreservation(probe)
    await checkHardwareWitness(probe, parserProbe)
    process.stdout.write(
      `Seed C foreign scanner: ${scanCount} FB0 C scan operations + hardware.w ` +
      "source-backed witness + tamper/range + suffix-boundary checks, " +
      "source-validation only (no adapter or build publication) passed\n",
    )
  } finally {
    await rm(buildDirectory, { recursive: true, force: true })
  }
}

await main()
