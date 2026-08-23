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
  const match = /^RESULT parse=(\d+) frontend=(\w+) modules=(\d+) imports=(\d+) structs=(\d+) types=(\d+) functions=(\d+) params=(\d+) entries=(\d+) statements=(\d+) expressions=(\d+) arguments=(\d+) symbols=(\d+) facts=(\d+) diagnostics=(\d+) receipt=(\d+)$/u.exec(line)
  if (!match) fail(`${label} has an invalid RESULT line: ${line}`)
  return {
    parse: Number(match[1]),
    frontend: match[2],
    modules: Number(match[3]),
    imports: Number(match[4]),
    structs: Number(match[5]),
    types: Number(match[6]),
    functions: Number(match[7]),
    params: Number(match[8]),
    entries: Number(match[9]),
    statements: Number(match[10]),
    expressions: Number(match[11]),
    arguments: Number(match[12]),
    symbols: Number(match[13]),
    facts: Number(match[14]),
    diagnostics: Number(match[15]),
    receipt: Number(match[16]),
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
  if (result.parsed.modules !== 1 || result.parsed.functions === 0 || result.parsed.types === 0 || result.parsed.receipt === 0) {
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
