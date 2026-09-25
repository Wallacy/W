import { mkdtemp, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { mlir0VersionRequirement } from "./mlir0-version-gate.mjs"

const root = resolve(import.meta.dir, "..")
const seed = resolve(root, "compiler", "seed-c")
const clang = process.env.W_SEED_TARGET_LAYOUT0_CLANG ?? "clang"
const targetProbeSource = [
  "_Static_assert(sizeof(void *) * 8 == 64, \"target pointer width\");",
  "int w_seed_target_layout_probe(void) { return 0; }",
  "",
].join("\n")
const targets = [
  { triple: "x86_64-pc-windows-msvc", label: "Windows x64 MSVC target" },
  { triple: "x86_64-unknown-linux-gnu", label: "Linux x64 GNU target" },
]

function llvmScalarProbeSource(dataLayout) {
  return [
    `target datalayout = "${dataLayout}"`,
    "define void @w_seed_target_layout_probe() {",
    "entry:",
    "  %bool = alloca i1",
    "  %i8 = alloca i8",
    "  %i16 = alloca i16",
    "  %i32 = alloca i32",
    "  %i64 = alloca i64",
    "  %i128 = alloca i128",
    "  %f16 = alloca half",
    "  %bf16 = alloca bfloat",
    "  %f32 = alloca float",
    "  %f64 = alloca double",
    "  %f128 = alloca fp128",
    "  ret void",
    "}",
    "",
  ].join("\n")
}

function fail(message) {
  throw new Error(`seed target layout0: ${message}`)
}

function run(command, args, cwd = root) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  })
  return {
    ...result,
    stdoutText: result.stdout.toString(),
    stderrText: result.stderr.toString(),
  }
}

function runRequired(command, args, label) {
  const result = run(command, args)
  if (result.exitCode !== 0) {
    const details = result.stderrText.trim() || result.stdoutText.trim()
    fail(`${label} failed${details ? `: ${details.slice(-2000)}` : ""}`)
  }
  return result
}

function extractExactlyOne(pattern, text, label) {
  const matches = [...text.matchAll(pattern)]
  if (matches.length !== 1 || typeof matches[0][1] !== "string")
    fail(`${label} did not produce exactly one target-derived value`)
  return matches[0][1]
}

function outputTripleMatches(requested, observed) {
  if (requested === "x86_64-pc-windows-msvc") {
    // Clang appends its observed MSVC compatibility version to this triple.
    return /^x86_64-pc-windows-msvc(?:\d+(?:\.\d+)*)?$/u.test(observed)
  }
  return observed === requested
}

function checkEmittedScalarAlignments(ir, target) {
  const expectedAllocas = [
    ["bool", "i1", 8],
    ["i8", "i8", 8],
    ["i16", "i16", 16],
    ["i32", "i32", 32],
    ["i64", "i64", 64],
    ["i128", "i128", 128],
    ["f16", "half", 16],
    ["bf16", "bfloat", 16],
    ["f32", "float", 32],
    ["f64", "double", 64],
    ["f128", "fp128", 128],
  ]

  const functionBody = ir.match(/define [^\n]*@w_seed_target_layout_probe\([^\n]*\)[^\n]*\{([\s\S]*?)^\}/mu)?.[1]
  if (typeof functionBody !== "string")
    fail(`${target.triple} LLVM scalar-allocation probe function is absent`)
  const actualAllocas = [...functionBody.matchAll(
    /^\s+%([-a-zA-Z$._0-9]+) = alloca (i\d+|half|bfloat|float|double|fp128), align (\d+)\s*$/gmu,
  )].map((match) => ({ name: match[1], type: match[2], alignmentBytes: Number(match[3]) }))
  if (actualAllocas.length !== expectedAllocas.length)
    fail(`${target.triple} LLVM alloca count is ${actualAllocas.length}, expected ${expectedAllocas.length}`)

  const observations = []
  for (const [index, [expectedName, expectedType, expectedBits]] of expectedAllocas.entries()) {
    const actual = actualAllocas[index]
    if (actual.name !== expectedName || actual.type !== expectedType)
      fail(`${target.triple} LLVM alloca ${index} is %${actual.name} ${actual.type}, expected %${expectedName} ${expectedType}`)
    const observedBits = actual.alignmentBytes * 8
    if (!Number.isSafeInteger(observedBits) || observedBits !== expectedBits) {
      fail(`${target.triple} LLVM ${expectedName} ABI alignment is ${observedBits} bits, expected ${expectedBits}`)
    }
    observations.push(`${expectedName}:${observedBits}`)
  }
  return observations.join(", ")
}

function runCUnit(binary, triple, toolchainIdentity, dataLayout) {
  const result = run(binary, [
    "--target", triple,
    "--toolchain", toolchainIdentity,
    "--data-layout", dataLayout,
  ])
  if (result.exitCode !== 0) {
    const details = result.stderrText.trim() || result.stdoutText.trim()
    fail(`C unit rejected ${triple}${details ? `: ${details}` : ""}`)
  }
  process.stdout.write(result.stdoutText)
}

async function main() {
  const manifestPath = resolve(root, "tooling", "mlir0-toolchain.json")
  const manifest = await Bun.file(manifestPath).json()
  if (manifest?.$schema !== "w-seed-mlir0-toolchain-1" ||
      manifest?.status !== "pinned" ||
      manifest?.toolchain?.mlir !== "23.1.1" ||
      manifest?.toolchain?.clang !== "23.1.1" ||
      manifest?.toolchain?.llvm !== "23.1.1") {
    fail("the LLVM/Clang 23.1.1 repository pin is absent or changed")
  }

  const version = runRequired(clang, ["--version"], "Clang version query")
  const toolchainIdentity = version.stdoutText.split(/\r?\n/u, 1)[0]
  const versionRule = mlir0VersionRequirement({
    pinnedVersion: manifest.toolchain.clang,
    developmentPatchCompatibility: true,
  })
  if (!versionRule.pattern.test(toolchainIdentity)) {
    fail(`observed Clang is outside ${versionRule.description}: ${toolchainIdentity}`)
  }
  if (toolchainIdentity.length > 255 || /[^\x20-\x7e]/u.test(toolchainIdentity))
    fail("observed Clang identity is not a bounded printable ASCII string")

  const tempDirectory = await mkdtemp(join(tmpdir(), "w-seed-target-layout0-"))
  try {
    const testBinary = join(tempDirectory,
      process.platform === "win32" ? "target-layout-tests.exe" : "target-layout-tests")

    runRequired(clang, [
      "-std=c23", "-pedantic-errors", "-Wall", "-Wextra", "-Wconversion",
      "-Wsign-conversion", "-Wshadow", "-Werror",
      "-I", resolve(seed, "include"),
      resolve(seed, "src", "w_seed_target_layout0.c"),
      resolve(seed, "tests", "test_target_layout0.c"),
      "-o", testBinary,
    ], "strict C23 target-layout unit build")

    for (const target of targets) {
      const probePath = join(tempDirectory,
        `${target.triple.replaceAll(/[^a-zA-Z0-9-]/gu, "-")}-target.c`)
      const scalarProbePath = join(tempDirectory,
        `${target.triple.replaceAll(/[^a-zA-Z0-9-]/gu, "-")}-scalars.ll`)
      await writeFile(probePath, targetProbeSource, {
        encoding: "utf8",
        flag: "wx",
      })
      const ir = runRequired(clang, [
        `--target=${target.triple}`,
        "-std=c23",
        "-S",
        "-emit-llvm",
        "-x", "c",
        probePath,
        "-o", "-",
      ], `${target.label} LLVM data-layout probe`)
      const dataLayout = extractExactlyOne(
        /^target datalayout = "([^"]+)"\s*$/gmu,
        ir.stdoutText,
        `${target.label} LLVM data layout`,
      )
      const observedTriple = extractExactlyOne(
        /^target triple = "([^"]+)"\s*$/gmu,
        ir.stdoutText,
        `${target.label} emitted target triple`,
      )
      if (!outputTripleMatches(target.triple, observedTriple)) {
        fail(`${target.label} probe returned ${observedTriple}`)
      }
      if (!/^[A-Za-z0-9:._-]+$/u.test(dataLayout))
        fail(`${target.label} LLVM data layout contains unexpected syntax`)

      await writeFile(scalarProbePath, llvmScalarProbeSource(dataLayout), {
        encoding: "utf8",
        flag: "wx",
      })
      const scalarIR = runRequired(clang, [
        `--target=${target.triple}`,
        "-O0",
        "-S",
        "-emit-llvm",
        "-x", "ir",
        scalarProbePath,
        "-o", "-",
      ], `${target.label} LLVM scalar ABI-alignment probe`)
      const scalarDataLayout = extractExactlyOne(
        /^target datalayout = "([^"]+)"\s*$/gmu,
        scalarIR.stdoutText,
        `${target.label} typed LLVM data layout`,
      )
      if (scalarDataLayout !== dataLayout)
        fail(`${target.label} typed scalar probe changed the selected data layout`)
      const scalarTargetTriple = extractExactlyOne(
        /^target triple = "([^"]+)"\s*$/gmu,
        scalarIR.stdoutText,
        `${target.label} typed LLVM target triple`,
      )
      if (!outputTripleMatches(target.triple, scalarTargetTriple))
        fail(`${target.label} scalar probe returned ${scalarTargetTriple}`)
      const scalarAlignments = checkEmittedScalarAlignments(
        scalarIR.stdoutText, target,
      )

      runCUnit(testBinary, target.triple, toolchainIdentity, dataLayout)
      process.stdout.write(
        `LLVM data layout (${target.triple}; ${toolchainIdentity}): ${dataLayout}\n` +
        `LLVM ABI alignment bits (${target.triple}): ${scalarAlignments}\n`,
      )
    }
    process.stdout.write(
      "Authority: LLVM scalar layout only; no public W/C ABI or aggregate-layout claim.\n",
    )
  } finally {
    await rm(tempDirectory, { recursive: true, force: true })
  }
}

if (import.meta.main) {
  try {
    await main()
  } catch (error) {
    process.stderr.write(`${error.message}\n`)
    process.exitCode = 1
  }
}
