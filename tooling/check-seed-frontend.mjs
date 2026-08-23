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
  const match = /^RESULT parse=(\d+) frontend=(\w+) modules=(\d+) imports=(\d+) structs=(\d+) enums=(\d+) enum_cases=(\d+) enum_case_parameters=(\d+) switch_arms=(\d+) types=(\d+) functions=(\d+) params=(\d+) entries=(\d+) statements=(\d+) expressions=(\d+) arguments=(\d+) symbols=(\d+) facts=(\d+) diagnostics=(\d+) receipt=(\d+)$/u.exec(line)
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
    switch_arms: Number(match[9]),
    types: Number(match[10]),
    functions: Number(match[11]),
    params: Number(match[12]),
    entries: Number(match[13]),
    statements: Number(match[14]),
    expressions: Number(match[15]),
    arguments: Number(match[16]),
    symbols: Number(match[17]),
    facts: Number(match[18]),
    diagnostics: Number(match[19]),
    receipt: Number(match[20]),
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

function expectOk(executable, bytes, label) {
  const input = typeof bytes === "string" ? Buffer.from(bytes, "utf8") : bytes
  const result = probe(executable, input, label)
  if (result.parsed.parse !== 0 || result.parsed.frontend !== "ok" ||
      result.parsed.facts !== 0 || result.parsed.diagnostics !== 0) {
    fail(`${label} was not a clean supported witness`)
  }
  return result
}

function receiptLines(output, prefix) {
  return output.split(/\r?\n/u).filter((line) => line.startsWith(prefix))
}

function expectSwitchReceipt(result, expectedCases, label, expectedPattern = 0) {
  const lines = receiptLines(result.output, "switch-arm=")
  if (lines.length !== expectedCases) {
    fail(`${label} receipt has ${lines.length} switch arms, expected ${expectedCases}`)
  }
  const records = lines.map((line) => {
    const fields = Object.fromEntries(line.split("|").map((field) => {
      const separator = field.indexOf("=")
      return separator < 0 ? [field, ""] : [field.slice(0, separator), field.slice(separator + 1)]
    }))
    return fields
  })
  for (const [index, record] of records.entries()) {
    if (record.owner === undefined || record.pattern !== String(expectedPattern) ||
        record.enum !== "0" || record.case !== String(index) ||
        record.supported !== "1" || record.result === undefined ||
        record["pattern-span"] === undefined || record.span === undefined) {
      fail(`${label} switch arm ${index} receipt identity/order is incomplete`)
    }
  }
  return records
}

function expectWildcardSwitchReceipt(result, label) {
  const lines = receiptLines(result.output, "switch-arm=")
  if (lines.length !== 1) fail(`${label} wildcard receipt has the wrong arm count`)
  const fields = Object.fromEntries(lines[0].split("|").map((field) => {
    const separator = field.indexOf("=")
    return separator < 0 ? [field, ""] : [field.slice(0, separator), field.slice(separator + 1)]
  }))
  if (fields.pattern !== "1" || fields.enum !== "0" ||
      fields.case !== "4294967295" || fields.supported !== "1" ||
      fields.result === undefined || fields["pattern-span"] === undefined ||
      fields.span === undefined) {
    fail(`${label} wildcard switch arm does not retain enum identity`)
  }
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
  const stageLabelSource = Buffer.concat([
    serviceStage,
    Buffer.from(
      "\nexport fn stageLabel(stage: ServiceStage): String {\n" +
      "  return switch stage {\n" +
      "    case .accepted: \"A\"\n" +
      "    case .reserving: \"R\"\n" +
      "    case .preparing: \"P\"\n" +
      "    case .serving: \"S\"\n" +
      "    case .completed: \"C\"\n" +
      "    case .cancelled: \"X\"\n" +
      "  }\n" +
      "}\n",
      "utf8",
    ),
  ])
  const stageLabel = expectOk(
    probeExecutable, stageLabelSource, "domain.w ServiceStage stageLabel",
  )
  if (stageLabel.parsed.enums !== 1 || stageLabel.parsed.enum_cases !== 6 ||
      stageLabel.parsed.switch_arms !== 6 || stageLabel.parsed.functions !== 1) {
    fail("domain.w ServiceStage stageLabel counts are incomplete")
  }
  expectSwitchReceipt(stageLabel, 6, "domain.w ServiceStage stageLabel")
  const stageRepeat = probe(
    probeExecutable, stageLabelSource, "domain.w ServiceStage stageLabel:repeat",
  )
  if (stageLabel.output !== stageRepeat.output) {
    fail("domain.w ServiceStage stageLabel receipt is not deterministic")
  }

  const stageEnumPrefix = Buffer.from(
    "export enum ServiceStage { accepted reserving preparing serving completed cancelled }\n",
    "utf8",
  )
  const wildcardStage = expectOk(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from(
        "fn stageDefault(stage: ServiceStage): String { return switch stage { case _: \"X\" } }\n",
        "utf8",
      ),
    ]),
    "enum wildcard switch identity",
  )
  expectWildcardSwitchReceipt(wildcardStage, "enum wildcard switch identity")
  const switchBase = (arms) => Buffer.concat([
    stageEnumPrefix,
    Buffer.from(
      "fn stageLabel(stage: ServiceStage): String { return switch stage { " +
      arms + " } }\n",
      "utf8",
    ),
  ])
  expectDiagnostic(
    probeExecutable,
    switchBase(
      "case .accepted: \"A\" case .reserving: \"R\" case .preparing: \"P\" " +
      "case .serving: \"S\" case .completed: \"C\"",
    ),
    "W-MATCH-0001",
    "enum switch missing arm",
  )
  expectDiagnostic(
    probeExecutable,
    switchBase(
      "case .accepted: \"A\" case .accepted: \"A2\" case .reserving: \"R\" " +
      "case .preparing: \"P\" case .serving: \"S\" case .completed: \"C\" " +
      "case .cancelled: \"X\"",
    ),
    "W-MATCH-0002",
    "enum switch duplicate arm",
  )
  expectDiagnostic(
    probeExecutable,
    switchBase(
      "case _: \"rest\" case .accepted: \"A\"",
    ),
    "W-MATCH-0002",
    "enum switch wildcard then arm",
  )
  expectDiagnostic(
    probeExecutable,
    switchBase(
      "case .accepted: \"A\" case .reserving: 1 case .preparing: \"P\" " +
      "case .serving: \"S\" case .completed: \"C\" case .cancelled: \"X\"",
    ),
    "W-TYPE-0120",
    "enum switch branch type conflict",
  )
  expectDiagnostic(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from(
        "fn narrow(stage: ServiceStage): u8 { return switch stage { " +
        "case .accepted: 1_u16 case .reserving: 2_u16 case .preparing: 3_u16 " +
        "case .serving: 4_u16 case .completed: 5_u16 case .cancelled: 6_u16 } }\n",
        "utf8",
      ),
    ]),
    "W-TYPE-0122",
    "enum switch real join then narrowing",
  )
  expectOk(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from(
        "fn widen(stage: ServiceStage): u16 { return switch stage { " +
        "case .accepted: 1_u8 case .reserving: 2_u8 case .preparing: 3_u8 " +
        "case .serving: 4_u8 case .completed: 5_u8 case .cancelled: 6_u8 } }\n",
        "utf8",
      ),
    ]),
    "enum switch widening join",
  )
  expectUnsupported(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from(
        "fn postfix(stage: ServiceStage): String { return switch stage { " +
        "case .accepted: \"A\" case .reserving: \"R\" case .preparing: \"P\" " +
        "case .serving: \"S\" case .completed: \"C\" case .cancelled: \"X\" }.length }\n" +
        "fn binary(stage: ServiceStage): String { return switch stage { " +
        "case .accepted: \"A\" case .reserving: \"R\" case .preparing: \"P\" " +
        "case .serving: \"S\" case .completed: \"C\" case .cancelled: \"X\" } + \"x\" }\n",
        "utf8",
      ),
    ]),
    "enum switch composed outer expression",
  )

  expectOk(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from("fn short(): ServiceStage { return .preparing }\n", "utf8"),
    ]),
    "short enum value in typed return",
  )
  expectOk(
    probeExecutable,
    "enum Stage { ready }\nfn value(): Stage { return .ready }\n",
    "payloadless enum case remains a value",
  )
  expectOk(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from("fn qualified(): ServiceStage { return ServiceStage.preparing }\n", "utf8"),
    ]),
    "qualified enum value in typed return",
  )
  expectOk(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from(
        "fn acceptStage(value: ServiceStage): ServiceStage { return value }\n" +
        "fn localCall(): ServiceStage { return acceptStage(.preparing) }\n",
        "utf8",
      ),
    ]),
    "short enum value in local call argument",
  )
  expectOk(
    probeExecutable,
    Buffer.from(
      "import { externalFn } from extdep\n" +
      "enum Stage { ready }\n" +
      "fn externalCall(): u32 { return externalFn(.ready) }\n",
      "utf8",
    ),
    "short enum value in external call argument",
  )
  expectDiagnostic(
    probeExecutable,
    Buffer.from(
      "import { externalFn } from extdep\n" +
      "enum Stage { ready }\n" +
      "enum Stage { other }\n" +
      "fn ambiguousExternalCall(): u32 { return externalFn(.ready) }\n",
      "utf8",
    ),
    "W-MATCH-0003",
    "ambiguous local enum is not selected for external call",
  )
  expectDiagnostic(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from("fn untyped(): ServiceStage { let value = .preparing return value }\n", "utf8"),
    ]),
    "W-MATCH-0003",
    "short enum value without expected type",
  )
  expectUnsupported(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from("fn wrongCase(): ServiceStage { return .missing }\n", "utf8"),
    ]),
    "wrong enum case remains an explicit fact",
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
  const domainConstructorSource = Buffer.concat([
    serviceStage,
    domainError,
    Buffer.from(
      "\nfn makeError(from: ServiceStage, to: ServiceStage): DomainError {\n" +
      "  return .invalidTransition(from: from, to: to)\n" +
      "}\n",
      "utf8",
    ),
  ])
  const constructorResult = expectOk(
    probeExecutable, domainConstructorSource,
    "domain.w DomainError invalidTransition constructor",
  )
  if (constructorResult.parsed.enums !== 2 ||
      constructorResult.parsed.enum_cases !== 11 ||
      constructorResult.parsed.enum_case_parameters !== 6 ||
      constructorResult.parsed.arguments !== 2 ||
      constructorResult.parsed.functions !== 1) {
    fail("domain.w constructor counts are incomplete")
  }
  const constructorCaseLines = receiptLines(constructorResult.output, "enum-case=")
  const constructorParameterLines = receiptLines(
    constructorResult.output, "enum-case-parameter=",
  )
  if (constructorCaseLines.length !== 11 || constructorParameterLines.length !== 6 ||
      !constructorCaseLines.some((line) => line.includes("|17:696e76616c69645472616e736974696f6e|")) ||
      !constructorParameterLines.some((line) => line.includes("|label=4:66726f6d|has-label=1|")) ||
      !constructorParameterLines.some((line) => line.includes("|label=2:746f|has-label=1|"))) {
    fail("domain.w constructor receipt does not retain case labels/identity")
  }
  expectDiagnostic(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from(
        "enum DomainError { invalidTransition(from: ServiceStage, to: ServiceStage) }\n" +
        "fn makeError(from: ServiceStage, to: ServiceStage): DomainError { " +
        "return .invalidTransition(wrong: from, to: to) }\n",
        "utf8",
      ),
    ]),
    "W-LABEL-0005",
    "enum constructor wrong label",
  )
  expectDiagnostic(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from(
        "enum DomainError { invalidTransition(from: ServiceStage, to: ServiceStage) }\n" +
        "fn makeError(from: ServiceStage, to: ServiceStage): DomainError { " +
        "return .invalidTransition(from: from) }\n",
        "utf8",
      ),
    ]),
    "W-LABEL-0005",
    "enum constructor wrong arity",
  )
  expectDiagnostic(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from(
        "enum DomainError { invalidTransition(from: ServiceStage, to: ServiceStage) }\n" +
        "fn makeError(from: ServiceStage, to: ServiceStage): DomainError { " +
        "return .invalidTransition(to: to, from: from) }\n",
        "utf8",
      ),
    ]),
    "W-LABEL-0005",
    "enum constructor inverted labels",
  )
  expectDiagnostic(
    probeExecutable,
    Buffer.concat([
      stageEnumPrefix,
      Buffer.from(
        "enum DomainError { invalidTransition(from: ServiceStage, to: ServiceStage) }\n" +
        "fn makeError(from: ServiceStage, to: ServiceStage): DomainError { " +
        "return .invalidTransition(from: from, from: to) }\n",
        "utf8",
      ),
    ]),
    "W-LABEL-0006",
    "enum constructor repeated previous label",
  )
  expectDiagnostic(
    probeExecutable,
    "enum Numeric { value(value: u8) }\nfn make(): Numeric { return .value(value: 300_u16) }\n",
    "W-TYPE-0122",
    "enum constructor numeric narrowing",
  )
  expectDiagnostic(
    probeExecutable,
    "enum Stage { ready }\nfn a(): Stage { return .ready() }\nfn b(): Stage { return Stage.ready() }\n",
    "W-LABEL-0005",
    "enum value zero-arity call",
  )
  const course = await sourceBackedFragment(
    "reference/last-light/domain.w",
    "export enum Course {",
    "export const fn courseLabel",
    "domain.w Course members",
  )
  expectBarrier(probeExecutable, course, "domain.w Course unsupported members")
  expectUnsupported(probeExecutable, "export enum Box<T> { value(T) }\n", "generic enum")
  expectOk(probeExecutable, "enum E { a }\nfn f(): E { return .a }\nentry(f)\n", "enum case expression")
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
  const manyCaseNames = Array.from({ length: 70 }, (_, index) => `case${index}`)
  const manyCases = manyCaseNames.join(" ")
  const manyArms = manyCaseNames.map((name, index) => `case .${name}: \"${index}\"`).join(" ")
  const manyEnumSource = `enum Many { ${manyCases} }\nfn all(value: Many): String { return switch value { ${manyArms} } }\n`
  const manyResult = expectOk(probeExecutable, Buffer.from(manyEnumSource, "utf8"),
    "enum switch exhaustiveness over 70 cases")
  if (manyResult.parsed.enum_cases !== 70 || manyResult.parsed.switch_arms !== 70) {
    fail("enum switch >64 case witness was truncated")
  }
  expectSwitchReceipt(manyResult, 70, "enum switch exhaustiveness over 70 cases")
  const manyMissingArms = manyCaseNames.slice(0, 69)
    .map((name, index) => `case .${name}: \"${index}\"`).join(" ")
  expectDiagnostic(
    probeExecutable,
    Buffer.from(
      `enum Many { ${manyCases} }\nfn missing(value: Many): String { return switch value { ${manyMissingArms} } }\n`,
      "utf8",
    ),
    "W-MATCH-0001",
    "enum switch >64 missing case",
  )
  expectBarrier(probeExecutable, "fn f(): () { if 1 { return }\n", "recovered CST")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}

console.log("seed frontend: witnesses, diagnostics, unsupported barrier, and deterministic receipt passed")
