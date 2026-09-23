import { spawnSync } from "node:child_process"
import { createHash } from "node:crypto"
import { lstat, mkdtemp, readFile, readdir, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { isAbsolute, join, resolve } from "node:path"
import { assertCrtFreeElf, assertElfNoExecutableStack } from "./check-w-run.mjs"

const root = resolve(import.meta.dir, "..")
const targetTriple = "x86_64-pc-windows-msvc"
const linuxTargetTriple = "x86_64-unknown-linux-gnu"
const maxWindowsCommandLineChars = 32767
const processArgumentsCountFixture = join(root, "compiler", "seed-c",
  "fixtures", "process-arguments-count.w")
const countOnlySource =
  "// Expected: exit 0; stdout `Exactly two arguments\\n` for count 2, " +
  "otherwise `Argument count N\\n`; " +
  "over 256 arguments exits 3 with empty stdout.\n" +
  "import std.process\n\n" +
  "async fn run(args: Arguments, ctx: Context): ExitCode {\n" +
  "  if args.count == 2 {\n" +
  "    print(\"Exactly two arguments\")\n" +
  "    return .success\n" +
  "  } else {\n" +
  "    print(\"Argument count ${args.count}\")\n" +
  "    return .success\n" +
  "  }\n" +
  "}\n\nentry(run)\n"
const fullLaneSource =
  "// Expected: exit 0; stdout `Argument count N; empty=BOOL\\n`; " +
  "over 256 arguments exits 3 with empty stdout.\n" +
  "import std.process\n\n" +
  "async fn run(args: Arguments, ctx: Context): ExitCode {\n" +
  "  print(\"Argument count ${args.count}; empty=${args.isEmpty}\")\n" +
  "  return .success\n" +
  "}\n\nentry(run)\n"

function fail(message) {
  throw new Error(`process argument-count parity: ${message}`)
}

function assert(condition, message) {
  if (!condition) fail(message)
}

function spawn(command, args, options = {}) {
  const result = spawnSync(command, args, {
    windowsHide: true,
    ...options,
  })
  if (result.error) fail(`${command} failed to start: ${result.error.message}`)
  return {
    exitCode: result.status,
    stdout: Buffer.from(result.stdout ?? Buffer.alloc(0)),
    stderr: Buffer.from(result.stderr ?? Buffer.alloc(0)),
  }
}

function expectExact(result, exitCode, stdout, label) {
  assert(result.exitCode === exitCode && result.stdout.equals(stdout) &&
    result.stderr.length === 0,
  `${label} differed: ${JSON.stringify({
    exitCode: result.exitCode,
    stdoutHex: result.stdout.toString("hex"),
    stderrHex: result.stderr.toString("hex"),
  })}`)
}

function build(w, source, output, target = targetTriple, auditDirectory) {
  const args = ["build", source, "--target", target, "--output", output]
  if (auditDirectory !== undefined)
    args.push("--audit-dir", auditDirectory)
  const result = spawn(w, args)
  expectExact(result, 0, Buffer.alloc(0), `build ${source}`)
}

const rawCommandLineCases = [
  { label: "no user arguments", rawTail: null, count: 0 },
  { label: "single argument", rawTail: "alpha", count: 1 },
  { label: "space-separated arguments", rawTail: "alpha beta", count: 2 },
  {
    label: "space and tab separators",
    rawTail: "alpha\tbeta \tgamma",
    count: 3,
  },
  { label: "quoted whitespace", rawTail: '"alpha beta" gamma', count: 2 },
  { label: "empty quoted argument", rawTail: '""', count: 1 },
  { label: "two empty quoted arguments", rawTail: '"" ""', count: 2 },
  { label: "quotes inside an argument", rawTail: 'alpha"" beta', count: 2 },
  // These assert parity with the current W adapter's quote toggling. They do
  // not claim the Windows CRT's backslash-before-quote decoding semantics.
  {
    label: "one backslash before quote",
    rawTail: String.raw`alpha\" beta gamma`,
    count: 1,
  },
  {
    label: "three backslashes before quotes",
    rawTail: String.raw`alpha\\\" beta\" gamma`,
    count: 2,
  },
  {
    label: "256 user arguments",
    rawTail: Array(256).fill("x").join(" "),
    count: 256,
  },
  {
    label: "257 user arguments",
    rawTail: Array(257).fill("x").join(" "),
    count: 257,
  },
]

function wslPath(pathValue) {
  const match = resolve(pathValue).match(/^([A-Za-z]):[\\/](.*)$/u)
  assert(match, `cannot map path into WSL2: ${pathValue}`)
  return `/mnt/${match[1].toLowerCase()}/${match[2].replaceAll("\\", "/")}`
}

async function verifyLinuxAuditTrace(directory, finalArtifact, wsl) {
  const expectedFiles = ["final-artifact", "input.mlir", "manifest.json",
    "optimized.ll", "output.ll", "output.obj", "verified.mlir", "wrt0.ll",
    "wrt0.obj"].sort()
  const entries = await readdir(directory, { withFileTypes: true })
  const actualFiles = entries.filter((entry) => entry.isFile())
    .map((entry) => entry.name).sort()
  assert(JSON.stringify(actualFiles) === JSON.stringify(expectedFiles),
    "Linux count-only audit trace inventory is not exact")
  const manifestBytes = await readFile(join(directory, "manifest.json"))
  const manifestText = manifestBytes.toString("utf8")
  const manifest = JSON.parse(manifestText)
  assert(manifest.schema === "w-seed-audit-trace-1" &&
    manifest.purpose === "development-only/non-ranking" &&
    manifest.closure_status === "inspection-required" &&
    manifest.product?.target === linuxTargetTriple &&
    manifest.product?.abi === "linux-gnu" &&
    manifest.product?.profile === "release" &&
    JSON.stringify(manifest.link?.inputs) ===
      JSON.stringify(["output.obj", "wrt0.obj"]),
  "Linux count-only audit does not bind the released multi-object product")
  const records = manifest.artifacts
  assert(Array.isArray(records) && records.length === 8 &&
    JSON.stringify(records.map((record) => record.name)) ===
      JSON.stringify(["input.mlir", "verified.mlir", "output.ll",
        "optimized.ll", "output.obj", "wrt0.ll", "wrt0.obj",
        "final-artifact"]),
  "Linux count-only audit does not enumerate every emitted artifact")
  const artifacts = new Map()
  for (const record of records) {
    const bytes = await readFile(join(directory, record.name))
    assert(record.bytes === bytes.length &&
      record.sha256 === createHash("sha256").update(bytes).digest("hex"),
    `Linux count-only audit digest/size does not match ${record.name}`)
    artifacts.set(record.name, bytes)
  }
  assert(artifacts.get("final-artifact").equals(await readFile(finalArtifact)),
    "Linux count-only audited artifact differs from its published output")

  const inputMlir = artifacts.get("input.mlir").toString("utf8")
  const optimizedIr = artifacts.get("optimized.ll").toString("utf8")
  assert(!inputMlir.includes("@w_seed_process_items") &&
    !optimizedIr.includes("w_seed_process_items") &&
    !optimizedIr.includes("strlen") &&
    !optimizedIr.includes("scan_data") &&
    !optimizedIr.includes("byte_address"),
  "Linux count-only lowering retained descriptors or argv byte scanning")
  const declarations = [...optimizedIr.matchAll(
    /^\s*declare\b[^\n]*?@(?:"([^"]+)"|([^\s(]+))\s*\(/gmu,
  )].map((match) => match[1] ?? match[2])
    .filter((name) => !name.startsWith("llvm."))
    .sort()
  assert(JSON.stringify(declarations) === JSON.stringify([
    "w_seed_process_argc", "w_seed_process_argv", "write",
  ]), `post-opt Linux count-only externals differ: ${declarations.join(",")}`)

  const readUndefined = (name, allowStrippedNoSymbols = false) => {
    const path = wslPath(join(directory, name))
    const result = spawn(wsl, ["-d", "Ubuntu", "--", "nm", "-u",
      path])
    const noSymbols = result.stderr.toString("utf8").trim() ===
      `nm: ${path}: no symbols`
    if (allowStrippedNoSymbols && noSymbols && result.stdout.length === 0)
      return []
    assert(result.exitCode === 0 && result.stderr.length === 0,
      `WSL nm failed for ${name}: ${result.stderr.toString("utf8")}`)
    return result.stdout.toString("utf8").split(/\r?\n/u)
      .map((line) => line.trim()).filter(Boolean)
      .map((line) => line.split(/\s+/u).at(-1)).sort()
  }
  assert(JSON.stringify(readUndefined("output.obj")) === JSON.stringify([
    "w_seed_process_argc", "w_seed_process_argv", "write",
  ]), "Linux count-only output object has a non-WRT undefined symbol")
  assert(JSON.stringify(readUndefined("wrt0.obj")) === JSON.stringify(["main"]),
    "Linux count-only WRT0 object has an unexpected undefined symbol")
  assert(readUndefined("final-artifact", true).length === 0,
    "Linux count-only final artifact has an undefined symbol")
  const sections = spawn(wsl, ["-d", "Ubuntu", "--", "readelf", "-SW",
    wslPath(join(directory, "output.obj"))])
  assert(sections.exitCode === 0 && sections.stderr.length === 0,
    "WSL readelf failed for the Linux count-only object")
  const bssSections = [...sections.stdout.toString("utf8").matchAll(
    /^\s*\[\s*\d+\]\s+\.bss\s+NOBITS\s+\S+\s+\S+\s+([0-9a-fA-F]+)/gmu,
  )]
  assert(bssSections.length <= 1 && (bssSections.length === 0 ||
    Number.parseInt(bssSections[0][1], 16) === 0),
  "Linux count-only object retained BSS (expected no process-items table)")

  const finalBytes = await readFile(finalArtifact)
  assertCrtFreeElf(finalBytes)
  assertElfNoExecutableStack(finalBytes)
}

export async function checkProcessArgumentCountParity(w,
  countOnlyExecutable, { includeLinuxChecks = true } = {}) {
  if (process.platform !== "win32" || process.arch !== "x64")
    fail("requires native Windows x64 so each test can supply a raw command line")
  assert(isAbsolute(w), "compiler path must be absolute")
  const compilerPath = resolve(w)
  const compilerStats = await lstat(compilerPath)
  assert(compilerStats.isFile() && !compilerStats.isSymbolicLink(),
    "compiler path must be a regular non-symlink file")
  const countOnlyIsProvided = countOnlyExecutable !== undefined
  let countOnlyPath
  if (countOnlyIsProvided) {
    assert(isAbsolute(countOnlyExecutable),
      "count-only executable path must be absolute")
    countOnlyPath = resolve(countOnlyExecutable)
    const countOnlyStats = await lstat(countOnlyPath)
    assert(countOnlyStats.isFile() && !countOnlyStats.isSymbolicLink(),
      "count-only executable must be a regular non-symlink file")
  }

  const startedAt = process.hrtime.bigint()
  const temporaryDirectory = await mkdtemp(join(tmpdir(),
    "w-process-argument-count-"))
  try {
    const countOnlySourcePath = join(temporaryDirectory, "count-only.w")
    const fullLanePath = join(temporaryDirectory, "full-lane.w")
    if (!countOnlyIsProvided) {
      countOnlyPath = join(temporaryDirectory, "count-only.exe")
      await writeFile(countOnlySourcePath, countOnlySource, { flag: "wx" })
      build(compilerPath, countOnlySourcePath, countOnlyPath)
    }
    const fullLaneOutput = join(temporaryDirectory, "full-lane.exe")
    await writeFile(fullLanePath, fullLaneSource, { flag: "wx" })
    build(compilerPath, fullLanePath, fullLaneOutput)

    for (const { label, rawTail, count } of rawCommandLineCases) {
      assert(countOnlyPath.length + (rawTail?.length ?? 0) + 4 <
        maxWindowsCommandLineChars,
      `${label} exceeds the bounded Windows command-line test limit`)
      const expectedExit = count > 256 ? 3 : 0
      const countOnlyExpectedOutput = count > 256
        ? Buffer.alloc(0)
        : Buffer.from(count === 2
          ? "Exactly two arguments\n"
          : `Argument count ${count}\n`, "utf8")
      const fullLaneExpectedOutput = count > 256
        ? Buffer.alloc(0)
        : Buffer.from(`Argument count ${count}; empty=${count === 0}\n`,
          "utf8")
      const rawArguments = rawTail === null ? [] : [rawTail]
      const options = rawTail === null
        ? { timeout: 5000 }
        : { timeout: 5000, windowsVerbatimArguments: true }
      const countOnly = spawn(countOnlyPath, rawArguments, options)
      const fullLane = spawn(fullLaneOutput, rawArguments, options)
      expectExact(countOnly, expectedExit, countOnlyExpectedOutput,
        `count-only adapter ${label}`)
      expectExact(fullLane, expectedExit, fullLaneExpectedOutput,
        `full adapter ${label}`)
      assert(countOnly.exitCode === fullLane.exitCode &&
        countOnly.stderr.equals(fullLane.stderr),
      `adapter count mismatch for ${label}`)
    }

    if (includeLinuxChecks) {
      const wsl = Bun.which("wsl.exe")
      assert(wsl, "WSL2 is required for the public Linux count-only witness")
      const linuxOutput = join(temporaryDirectory, "count-only-linux")
      const linuxAudit = join(temporaryDirectory, "count-only-linux-audit")
      build(compilerPath, processArgumentsCountFixture, linuxOutput,
        linuxTargetTriple, linuxAudit)
      await verifyLinuxAuditTrace(linuxAudit, linuxOutput, wsl)
      const linuxCountCases = [
        ["without user arguments", []],
        ["with one user argument", ["alpha"]],
        ["with one empty user argument", [""]],
        ["with two user arguments", ["alpha", "beta"]],
        ["with three user arguments", ["alpha", "beta", "gamma"]],
        ["with exactly 256 user arguments", Array.from({ length: 256 },
          () => "x")],
        ["with 257 user arguments", Array.from({ length: 257 }, () => "x")],
      ]
      for (const [label, argumentsList] of linuxCountCases) {
        const expectedExit = argumentsList.length > 256 ? 3 : 0
        const expectedOutput = expectedExit !== 0
          ? Buffer.alloc(0)
          : Buffer.from(argumentsList.length === 2
            ? "Exactly two arguments\n"
            : `Argument count ${argumentsList.length}\n`, "utf8")
        const result = spawn(wsl, ["-d", "Ubuntu", "--",
          wslPath(linuxOutput), ...argumentsList], { timeout: 5000 })
        expectExact(result, expectedExit, expectedOutput,
          `Linux count-only adapter ${label}`)
      }

    }

    const elapsedMs = Number(process.hrtime.bigint() - startedAt) / 1e6
    console.log(includeLinuxChecks
      ? `process argument-count parity: ${rawCommandLineCases.length} raw ` +
        `Windows commands, seven WSL vectors; post-opt/object/final CRT-free ` +
        `receipts passed (${Math.round(elapsedMs)} ms)`
      : `process argument-count parity: ${rawCommandLineCases.length} raw ` +
        `Windows commands; Linux receipts unchanged from focused gate ` +
        `(${Math.round(elapsedMs)} ms)`)
  } finally {
    await rm(temporaryDirectory, { recursive: true, force: true })
  }
}

if (import.meta.main) {
  const argumentsList = process.argv.slice(2)
  assert(argumentsList.length === 1 && isAbsolute(argumentsList[0]),
    "usage: bun tooling/check-process-argument-count.mjs <absolute-path-to-w.exe>")
  await checkProcessArgumentCountParity(resolve(argumentsList[0]))
}
