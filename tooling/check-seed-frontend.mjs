import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const executableSuffix = process.platform === "win32" ? ".exe" : ""

function fail(message) {
  throw new Error(`seed frontend: ${message}`)
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
  return result
}

function parseResult(output, label) {
  const lines = output.toString().split(/\r?\n/u)
  const line = lines.find((candidate) => candidate.startsWith("RESULT "))
  if (!line) fail(`${label} has no RESULT line`)
  const match = /^RESULT parse=(\d+) frontend=(\w+) modules=(\d+) imports=(\d+) structs=(\d+) enums=(\d+) enum_cases=(\d+) enum_case_parameters=(\d+) types=(\d+) functions=(\d+) params=(\d+) entries=(\d+) statements=(\d+) expressions=(\d+) arguments=(\d+) symbols=(\d+) facts=(\d+) diagnostics=(\d+) receipt=(\d+)$/u.exec(line)
  if (!match) fail(`${label} has an invalid RESULT line: ${line}`)
  return {
    parse: Number(match[1]),
    frontend: match[2],
    modules: Number(match[3]),
    imports: Number(match[4]),
    structs: Number(match[5]),
    enums: Number(match[6]),
    enum_cases: Number(match[7]),
    enum_case_parameters: Number(match[8]),
    types: Number(match[9]),
    functions: Number(match[10]),
    params: Number(match[11]),
    entries: Number(match[12]),
    statements: Number(match[13]),
    expressions: Number(match[14]),
    arguments: Number(match[15]),
    symbols: Number(match[16]),
    facts: Number(match[17]),
    diagnostics: Number(match[18]),
    receipt: Number(match[19]),
  }
}

function probe(executable, bytes, label) {
  const first = run(executable, [], { stdin: bytes })
  const second = run(executable, [], { stdin: bytes })
  if (!Buffer.from(first.stdout).equals(Buffer.from(second.stdout))) {
    fail(`${label} receipt/probe output is not deterministic`)
  }
  const parsed = parseResult(first.stdout, label)
  return { parsed, output: first.stdout.toString() }
}

function expectCompleteWitness(executable, bytes, label) {
  const result = probe(executable, bytes, label)
  if (result.parsed.parse !== 0 ||
      !["ok", "unsupported"].includes(result.parsed.frontend) ||
      result.parsed.diagnostics !== 0) {
    fail(`${label} did not cross the COMPLETE normalizer barrier`)
  }
  if (result.parsed.modules !== 1 ||
      (result.parsed.functions === 0 && result.parsed.enums === 0) ||
      result.parsed.types === 0 || result.parsed.receipt === 0) {
    fail(`${label} did not produce declaration/signature/receipt output`)
  }
  return result
}

function expectDiagnostic(executable, source, code, label) {
  const result = probe(executable, Buffer.from(source, "utf8"), label)
  if (result.parsed.frontend !== "diagnostics" || !result.output.includes(`DIAGNOSTIC code=${code} `)) {
    fail(`${label} did not report ${code}`)
  }
}

function expectUnsupported(executable, source, label) {
  const result = probe(executable, Buffer.from(source, "utf8"), label)
  if (result.parsed.frontend !== "unsupported" || result.parsed.facts === 0) {
    fail(`${label} did not retain an explicit unsupported fact`)
  }
}

function expectBarrier(executable, source, label) {
  const result = run(executable, [], { stdin: Buffer.from(source, "utf8") })
  const parsed = parseResult(result.stdout, label)
  if (parsed.parse === 0 || parsed.frontend !== "barrier") {
    fail(`${label} was not stopped by the recovered/fatal CST barrier`)
  }
}

function receiptText(text) {
  const bytes = Buffer.from(text, "utf8")
  return `${bytes.length}:${bytes.toString("hex")}`
}

function expectEnumWitness(
  executable,
  bytes,
  label,
  expectedCases,
  expectedParameters,
  expectedCaseNames = [],
  expectedPayloadLabels = [],
  expectedPayloadOwners = [],
) {
  const result = probe(executable, bytes, label)
  if (result.parsed.parse !== 0 ||
      !["ok", "unsupported"].includes(result.parsed.frontend) ||
      result.parsed.enums !== 1 ||
      result.parsed.enum_cases !== expectedCases ||
      result.parsed.enum_case_parameters !== expectedParameters ||
      result.parsed.diagnostics !== 0 ||
      result.parsed.receipt === 0) {
    fail(`${label} did not produce the expected COMPLETE enum receipt`)
  }
  const enumLines = result.output.split(/\r?\n/u).filter((line) => line.startsWith("enum="))
  const caseLines = result.output.split(/\r?\n/u).filter((line) => line.startsWith("enum-case="))
  const parameterLines = result.output.split(/\r?\n/u)
    .filter((line) => line.startsWith("enum-case-parameter="))
  if (enumLines.length !== 1 || caseLines.length !== expectedCases ||
      parameterLines.length !== expectedParameters) {
    fail(`${label} receipt enum records are incomplete`)
  }
  if (caseLines.some((line) => line.split("|")[1] !== "0")) {
    fail(`${label} case records do not retain enum ownership`)
  }
  if (expectedCaseNames.length !== 0) {
    const caseNames = caseLines.map((line) => line.split("|")[2])
    const expectedNames = expectedCaseNames.map(receiptText)
    if (caseNames.length !== expectedNames.length ||
        caseNames.some((name, index) => name !== expectedNames[index])) {
      fail(`${label} case receipt names are not source-ordered`)
    }
  }
  if (expectedPayloadLabels.length !== 0) {
    const labels = parameterLines.map((line) => {
      const start = line.indexOf("|label=") + "|label=".length
      const end = line.indexOf("|has-label=", start)
      return line.slice(start, end)
    })
    const expectedLabels = expectedPayloadLabels.map(receiptText)
    if (labels.length !== expectedLabels.length ||
        labels.some((value, index) => value !== expectedLabels[index])) {
      fail(`${label} payload labels are not source-ordered`)
    }
    const hasLabels = parameterLines.map((line) => line.includes("|has-label=1|"))
    if (hasLabels.some((value, index) => value !== (expectedPayloadLabels[index].length !== 0))) {
      fail(`${label} payload label-presence flags are inconsistent`)
    }
  }
  if (expectedPayloadOwners.length !== 0) {
    const owners = parameterLines.map((line) => line.split("|")[1])
    const expectedOwners = expectedPayloadOwners.map((owner) => String(owner))
    if (owners.length !== expectedOwners.length ||
        owners.some((owner, index) => owner !== expectedOwners[index])) {
      fail(`${label} payload owner associations are not source-ordered`)
    }
  }
  return result
}

async function sourceBackedFragment(relativePath, startMarker, endMarker, label) {
  const bytes = Buffer.from(await Bun.file(resolve(root, relativePath)).arrayBuffer())
  const startNeedle = Buffer.from(startMarker, "utf8")
  const endNeedle = Buffer.from(endMarker, "utf8")
  const start = bytes.indexOf(startNeedle)
  const end = bytes.indexOf(endNeedle, start + startNeedle.length)
  if (start < 0 || end < 0 || end <= start ||
      bytes.indexOf(startNeedle, start + 1) >= 0) {
    fail(`${label} source markers are missing or duplicated`)
  }
  return bytes.subarray(start, end)
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-frontend-check-"))
try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory, "--target", "w_seed_frontend_probe", "w_seed_frontend_tests", "--", "-j", "2"])
  const tests = run(join(buildDirectory, `w_seed_frontend_tests${executableSuffix}`), [])
  if (tests.stdout.length === 0 && tests.stderr.length !== 0) fail("frontend unit test emitted an error")
  const probeExecutable = join(buildDirectory, `w_seed_frontend_probe${executableSuffix}`)

  for (const relativePath of [
    "reference/last-light/horizon_tool.w",
    "reference/last-light/formatting.w",
    "reference/last-light/numerics.w",
  ]) {
    const bytes = Buffer.from(await Bun.file(resolve(root, relativePath)).arrayBuffer())
    expectCompleteWitness(probeExecutable, bytes, relativePath)
  }

  const serviceStage = await sourceBackedFragment(
    "reference/last-light/domain.w",
    "export enum ServiceStage {",
    "export alias CancelledStage",
    "domain.w ServiceStage",
  )
  expectEnumWitness(
    probeExecutable,
    serviceStage,
    "domain.w ServiceStage",
    6,
    0,
    ["accepted", "reserving", "preparing", "serving", "completed", "cancelled"],
  )
  const domainError = await sourceBackedFragment(
    "reference/last-light/domain.w",
    "export enum DomainError: Error {",
    "export fn add(",
    "domain.w DomainError",
  )
  expectEnumWitness(
    probeExecutable,
    domainError,
    "domain.w DomainError",
    5,
    6,
    ["invalidGuestCount", "invalidTransition", "unknownOrder", "currencyMismatch", "overflow"],
    ["", "from", "to", "", "expected", "found"],
    [0, 1, 1, 2, 3, 3],
  )
  const course = await sourceBackedFragment(
    "reference/last-light/domain.w",
    "export enum Course {",
    "export const fn courseLabel",
    "domain.w Course members",
  )
  expectBarrier(probeExecutable, course, "domain.w Course unsupported members")
  expectUnsupported(probeExecutable, "export enum Box<T> { value(T) }\n", "generic enum")
  expectUnsupported(probeExecutable, "enum E { a }\nfn f(): E { return .a }\nentry(f)\n", "enum case expression")
  expectBarrier(probeExecutable, "fn f(){ enum E { a } }\n", "enum contextual fail-closed")
  expectEnumWitness(
    probeExecutable,
    Buffer.from(
      "enum Callbacks { positional(fn(named value: u32): Bool) " +
      "labeled(handler: fn(named value: u32): Bool) }\n",
      "utf8",
    ),
    "enum function-type payload labels",
    2,
    2,
    ["positional", "labeled"],
    ["", "handler"],
    [0, 1],
  )

  expectDiagnostic(
    probeExecutable,
    "fn f(): () { if 1 { return } }\nentry(f)\n",
    "W-SEM-0001",
    "integer condition",
  )
  expectDiagnostic(
    probeExecutable,
    "fn f(value: u32): u16 { return value }\nentry(f)\n",
    "W-TYPE-0122",
    "implicit narrowing",
  )
  expectDiagnostic(
    probeExecutable,
    "fn callee(value: u32): u32 { return value }\nfn f(): u32 { return callee(other: 1) }\nentry(f)\n",
    "W-LABEL-0005",
    "unknown call label",
  )
  expectUnsupported(
    probeExecutable,
    "fn f(): u32 { return missing }\nentry(f)\n",
    "unresolved local",
  )
  expectUnsupported(
    probeExecutable,
    "fn f(): u32 { return 1 << 2 }\nentry(f)\n",
    "unsupported operator",
  )
  expectBarrier(probeExecutable, "fn f(): () { if 1 { return }\n", "recovered CST")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}

console.log("seed frontend: witnesses, diagnostics, unsupported barrier, and deterministic receipt passed")
