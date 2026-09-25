import { existsSync } from "node:fs"
import { lstat, mkdir, mkdtemp, readdir, readFile, rm, statfs, symlink, unlink, writeFile } from "node:fs/promises"
import { createHash } from "node:crypto"
import { tmpdir } from "node:os"
import { join, relative, resolve, isAbsolute } from "node:path"
import {
  MATERIALIZED_MANIFEST,
  defaultCacheDirectory,
  validateManifest,
} from "./acquire-mlir0-windows.mjs"
import {
  findVisualStudio,
  findWindowsSdkKernel32,
} from "./windows-build-support.mjs"
import { validateElfX64 } from "./executable-benchmark-runner.mjs"
import {
  assertCrtFreeElf,
  assertCrtFreeExecElf,
  assertElfNoExecutableStack,
} from "./check-w-run.mjs"
import { checkProcessArgumentCountParity } from
  "./check-process-argument-count.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const manifestPath = resolve(import.meta.dir, "mlir0-windows-toolchain.json")
const materializedPath = join(defaultCacheDirectory(), MATERIALIZED_MANIFEST)
const smokePath = resolve(import.meta.dir, "smoke-mlir0-windows.mjs")
const helloFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const ifFixture = resolve(seedDirectory, "fixtures", "if.w")
const enumFixture = resolve(seedDirectory, "fixtures", "enum.w")
const enumSubsetFixture = resolve(seedDirectory, "fixtures", "enum-subset.w")
const enumPayloadFixture = resolve(seedDirectory, "fixtures", "enum-payload.w")
const enumCfgJoinFixture = resolve(seedDirectory, "fixtures", "enum-cfg-join.w")
const enumCfgJoinOutput = Buffer.from("42/0\n", "utf8")
const enumBoolPayloadFixture = resolve(seedDirectory, "fixtures", "enum-bool-payload.w")
const whileFixture = resolve(seedDirectory, "fixtures", "while.w")
const whileMultiFixture = resolve(seedDirectory,
  "fixtures", "while-multi.w")
const whilePostFixture = resolve(seedDirectory,
  "fixtures", "while-post.w")
const whileBreakContinueFixture = resolve(seedDirectory,
  "fixtures", "while-break-continue.w")
const nestedLabeledWhileFixture = resolve(seedDirectory,
  "fixtures", "nested-labeled-while.w")
const nestedLoopTerminalReturnsFixture = resolve(seedDirectory,
  "fixtures", "nested-loop-terminal-returns.w")
const repeatFixture = resolve(seedDirectory,
  "fixtures", "repeat.w")
const wmoFixture = resolve(seedDirectory, "fixtures", "wmo.w")
const asyncJoinFixture = resolve(seedDirectory,
  "fixtures", "async-join.w")
const asyncYieldFixture = resolve(seedDirectory,
  "fixtures", "async-yield.w")
const mainDispatchFixture = resolve(seedDirectory,
  "fixtures", "main-dispatch0.w")
const mainCardinalityFixture = resolve(seedDirectory,
  "fixtures", "main-cardinality0.w")
const comparisonsFixture = resolve(seedDirectory, "fixtures", "comparisons.w")
const comparisonCompositionFixture = resolve(seedDirectory, "fixtures", "comparison-composition.w")
const integerComparisonFixture = resolve(seedDirectory,
  "fixtures", "integer-comparison.w")
const flatValueAggregatesFixture = resolve(seedDirectory,
  "fixtures", "flat-value-aggregates.w")
const flatValueAggregatesOutput = Buffer.from("7,5,26\n", "utf8")
const boolShortCircuitFixture = resolve(seedDirectory, "fixtures", "bool-short-circuit.w")
const scalarIfFixture = resolve(seedDirectory, "fixtures", "scalar-if.w")
const terminalReturnsFixture = resolve(seedDirectory, "fixtures", "terminal-returns.w")
const interpolationFixture = resolve(
  seedDirectory, "fixtures", "interpolation.w")
const linearFixture = resolve(seedDirectory, "fixtures", "linear.w")
const unaryNegateFixture = resolve(seedDirectory,
  "fixtures", "unary-negate.w")
const unaryInterpolationFixture = resolve(seedDirectory,
  "fixtures", "unary-interpolation.w")
const integerBitwiseFixture = resolve(seedDirectory,
  "fixtures", "integer-bitwise.w")
const unsignedFixture = resolve(seedDirectory,
  "fixtures", "unsigned.w")
const shiftsFixture = resolve(seedDirectory,
  "fixtures", "shifts.w")
const powerFixture = resolve(seedDirectory,
  "fixtures", "power.w")
const powerPrefixFixture = resolve(seedDirectory,
  "fixtures", "power-prefix.w")
const compoundFixture = resolve(seedDirectory,
  "fixtures", "compound.w")
const floatStrictFixture = resolve(seedDirectory,
  "fixtures", "float-strict.w")
const floatBitRepresentationFixture = resolve(seedDirectory,
  "fixtures", "float-bit-representation.w")
const numericWideningFixture = resolve(seedDirectory,
  "fixtures", "numeric-widening.w")
const checkedIntegerArithmeticFixture = resolve(seedDirectory,
  "fixtures", "checked-integer-arithmetic.w")
const integerWrappingFixture = resolve(seedDirectory,
  "fixtures", "integer-wrapping.w")
const integerPrefixFixture = resolve(seedDirectory,
  "fixtures", "integer-prefix.w")
const integerWideningFixture = resolve(seedDirectory,
  "fixtures", "integer-widening.w")
const integerTruncatingBitsFixture = resolve(seedDirectory,
  "fixtures", "integer-truncating-bits.w")
const integerSaturatingConversionFixture = resolve(seedDirectory,
  "fixtures", "integer-saturating-conversion.w")
const fixtureIntegerSaturatingConversionOutput = Buffer.from(
  "ss -128/7/127; us 7/127/127; su 0/200/255; " +
  "uu 7/255/255; UInt->Int 9223372036854775807\n", "utf8")
const uIntWrappingAddFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-add.w")
const uIntWrappingSubtractFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-subtract.w")
const uIntWrappingMultiplyFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-multiply.w")
const uIntWrappingNegateFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-negate.w")
const uIntWrappingPowerFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-power.w")
const uIntWrappingShiftLeftFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-shift-left.w")
const uIntRotatedLeftFixture = resolve(seedDirectory,
  "fixtures", "uint-rotated-left.w")
const uIntRotatedRightFixture = resolve(seedDirectory,
  "fixtures", "uint-rotated-right.w")
const uIntCountOnesFixture = resolve(seedDirectory,
  "fixtures", "uint-count-ones.w")
const uIntCountZerosFixture = resolve(seedDirectory,
  "fixtures", "uint-count-zeros.w")
const uIntLeadingZerosFixture = resolve(seedDirectory,
  "fixtures", "uint-leading-zeros.w")
const uIntTrailingZerosFixture = resolve(seedDirectory,
  "fixtures", "uint-trailing-zeros.w")
const uIntReversedBitsFixture = resolve(seedDirectory,
  "fixtures", "uint-reversed-bits.w")
const uIntReversedBytesFixture = resolve(seedDirectory,
  "fixtures", "uint-reversed-bytes.w")
const fixedIntegerBitPrimitivesFixture = resolve(seedDirectory,
  "fixtures", "fixed-integer-bit-primitives.w")
const fixedIntegerShiftPoliciesFixture = resolve(seedDirectory,
  "fixtures", "fixed-integer-shift-policies.w")
const fixedIntegerBitPrimitivesOutput = Buffer.from(
  "i8 3/5/1/1 74/127 82 -92/41\n" +
  "u8 4/4/0/1 105 150 45/75\n" +
  "i16 5/11/3/2 11336/32767 13330 9320/2330\n" +
  "u16 8/8/0/0 54673 43913 4951/50389\n" +
  "i32 13/19/3/3 510274632/2147483647 2018915346 610839792/152709948\n" +
  "u32 20/12/0/0 4155757969 4023233417 324508639/3302352631\n" +
  "i64 30/34/7/1 8553414939923104896/9223372036854775807 7984226321029210881 163971058432973532/40992764608243383\n" +
  "u64 32/32/0/4 597899502893742975 1167088121787636990 18282773015276577825/9182379272246532360\n",
  "utf8")
const fixedIntegerShiftPoliciesOutput = Buffer.from(
  "i8 -128/0/-64/64\nu8 128/0/64/64\n" +
  "i16 -32768/0/-16384/16384\nu16 32768/0/16384/16384\n" +
  "i32 -2147483648/0/-1073741824/1073741824\n" +
  "u32 2147483648/0/1073741824/1073741824\n" +
  "i64 -9223372036854775808/0/-4611686018427387904/4611686018427387904\n" +
  "u64 9223372036854775808/0/4611686018427387904/4611686018427387904\n" +
  "u64 small-value 2/64/64\n", "utf8")
const uIntOverflowingAddFixture = resolve(seedDirectory,
  "fixtures", "uint-overflowing-add.w")
const uIntOverflowingPowerFixture = resolve(seedDirectory,
  "fixtures", "uint-overflowing-power.w")
const uIntOverflowingFamilyFixture = resolve(seedDirectory,
  "fixtures", "uint-overflowing-family.w")
const uIntSaturatingAddFixture = resolve(seedDirectory,
  "fixtures", "uint-saturating-add.w")
const uIntSaturatingSubtractFixture = resolve(seedDirectory,
  "fixtures", "uint-saturating-subtract.w")
const uIntSaturatingMultiplyFixture = resolve(seedDirectory,
  "fixtures", "uint-saturating-multiply.w")
const uIntSaturatingPolicyFixture = resolve(seedDirectory,
  "fixtures", "uint-saturating-policy.w")
const uIntBitNotFixture = resolve(seedDirectory,
  "fixtures", "uint-bit-not.w")
const uIntBitwiseFixture = resolve(seedDirectory,
  "fixtures", "uint-bitwise.w")
const uIntCompoundFixture = resolve(seedDirectory,
  "fixtures", "uint-compound.w")
const mutationFixture = resolve(seedDirectory,
  "fixtures", "mutation.w")
const conditionalMutationFixture = resolve(seedDirectory,
  "fixtures", "conditional-mutation.w")
const boolMutationFixture = resolve(seedDirectory,
  "fixtures", "bool-mutation.w")
const branchMutationFixture = resolve(seedDirectory,
  "fixtures", "branch-mutation.w")
const multiBranchMutationFixture = resolve(seedDirectory,
  "fixtures", "branch-mutation-multi.w")
const processInputFixture = resolve(seedDirectory, "fixtures", "process-input0.w")
const processArgumentsCountFixture = resolve(seedDirectory, "fixtures",
  "process-arguments-count.w")
const processArgumentsOrderingFixture = resolve(seedDirectory, "fixtures",
  "process-arguments-ordering.w")
const processArgumentsCountSelectiveImportFixture = resolve(seedDirectory,
  "tests", "fixtures", "process-arguments-count-selective-import.w")
const processEnumPayloadFixture = resolve(seedDirectory, "fixtures",
  "process-enum-payload.w")
const processIntegerExactSuccessFixture = resolve(seedDirectory, "fixtures",
  "process-integer-exact-success.w")
const processIntegerExactErrorFixture = resolve(seedDirectory, "fixtures",
  "process-integer-exact-error.w")
const processFloatRoundingSuccessFixture = resolve(seedDirectory, "fixtures",
  "process-float-rounding-success.w")
const processFloatRoundingRuntimeIfFixture = resolve(seedDirectory, "fixtures",
  "process-float-rounding-runtime-if.w")
const processFloatRoundingErrorFixture = resolve(seedDirectory, "fixtures",
  "process-float-rounding-error.w")
const processIntegerExactRuntimeFixture = resolve(seedDirectory, "fixtures",
  "process-fixed-integer-arithmetic.w")
const checkedIntegerHelperFaultFixture = resolve(seedDirectory, "fixtures",
  "checked-integer-helper-fault.w")
const checkedIntegerHelperFaultOutput = Buffer.from("Begin 255\n", "utf8")
const checkedScalarIfJoinFixture = resolve(seedDirectory, "fixtures",
  "checked-scalar-if-join.w")
const checkedScalarIfJoinZeroOutput = Buffer.from("Joined -1\n", "utf8")
const checkedScalarIfJoinOneOutput = Buffer.from("Joined 0\n", "utf8")
const u64MixRoundFixture = resolve(seedDirectory, "fixtures",
  "u64_mix_round.w")
const u64MixRoundOutput = Buffer.from("Mix 5608831001354178255\n", "utf8")
const localGraphFixture = resolve(seedDirectory, "fixtures", "local-graph",
  "app.w")
const targetTriple = "x86_64-pc-windows-msvc"
const linuxTargetTriple = "x86_64-unknown-linux-gnu"
const maxWindowsCommandLineChars = 32767
const expectedHelp =
  "usage: w check <path/file.w> [--json]\n" +
  "usage: w run <path/file.w> [-- <args...>]\n" +
  "usage: w build <path/file.w> --target <target> --output <artifact> " +
  "[--pie <on|off>] [--audit-dir <new-directory>]\n" +
  "  --pie is a temporary Linux x86_64 seed option (default: on); " +
  "--audit-dir is development-only and non-ranking\n" +
  "usage: w bench process --exe <absolute-path> [options]\n" +
  "  options: --cwd <absolute-dir> --arg <value> --warmup <n> " +
  "--samples <n> --timeout-ms <n> --expect-exit <n> " +
  "--expect-stdout-hex <bytes> --expect-stderr-hex <bytes>\n" +
  "  Windows-native cold process measurement only; does not compile or " +
  "execute .w source\n"
const expectedWindowsErrorHelp = expectedHelp.replaceAll("\n", "\r\n")

function fail(message) {
  throw new Error(`W RUN Windows: ${message}`)
}

function parseArguments(argv) {
  let ci = false
  let testPeImportPolicy = false
  for (const argument of argv) {
    if (argument === "--ci" && !ci) ci = true
    else if (argument === "--test-pe-import-policy" && !testPeImportPolicy)
      testPeImportPolicy = true
    else throw new Error(`unknown option: ${String(argument)}`)
  }
  return { ci, testPeImportPolicy }
}

const { ci: ciMode, testPeImportPolicy } = parseArguments(process.argv.slice(2))

function assert(condition, message) {
  if (!condition) fail(message)
}

function unavailable(message) {
  if (ciMode) fail(`${message}; mandatory native CI prerequisite is unavailable`)
  console.log(`W RUN Windows: SKIP ${message}`)
  process.exit(0)
}

function spawn(command, args, cwd = root) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  })
  return {
    ...result,
    stdout: Buffer.from(result.stdout),
    stderr: Buffer.from(result.stderr),
  }
}

function shortOutput(bytes) {
  const value = bytes.toString().trim()
  return value.length > 2000 ? `${value.slice(-2000)}…` : value
}

function runRequired(label, command, args, cwd = root) {
  const result = spawn(command, args, cwd)
  if (result.exitCode !== 0)
    fail(`${label} failed${shortOutput(result.stderr) || shortOutput(result.stdout)
      ? `: ${shortOutput(result.stderr) || shortOutput(result.stdout)}` : ""}`)
  return result
}

function windowsCommandLineArgumentLength(value) {
  const text = String(value)
  let length = 2
  let slashes = 0
  for (let index = 0; index < text.length; index += 1) {
    const character = text[index]
    if (character === "\\") {
      slashes += 1
    } else if (character === '"') {
      length += slashes * 2 + 2
      slashes = 0
    } else {
      length += slashes + 1
      slashes = 0
    }
  }
  return length + slashes * 2
}

function assertWindowsCommandLineSafe(command, args, label) {
  if (process.platform !== "win32") return
  const length = windowsCommandLineArgumentLength(command) +
    args.reduce((total, argument) =>
      total + 1 + windowsCommandLineArgumentLength(argument), 0) + 1
  assert(length <= maxWindowsCommandLineChars,
    `${label} exceeds the Windows command-line limit: ${length} > ` +
    `${maxWindowsCommandLineChars}`)
}

function outsideRepository(pathValue) {
  const pathRelative = relative(root, resolve(pathValue))
  return isAbsolute(pathRelative) || pathRelative === ".." ||
    pathRelative.startsWith("..\\") || pathRelative.startsWith("../")
}

function cmdQuote(value) {
  const text = String(value)
  return /[\s"&|<>^]/u.test(text)
    ? `"${text.replaceAll('"', '""')}"`
    : text
}

function wslPath(pathValue) {
  const match = resolve(pathValue).match(/^([A-Za-z]):[\\/](.*)$/u)
  assert(match, `cannot map path into WSL2: ${pathValue}`)
  return `/mnt/${match[1].toLowerCase()}/${match[2].replaceAll("\\", "/")}`
}

function runWithVs(label, vsDevCmd, command, args) {
  // Pass command words as separate argv entries. Bun's Windows process
  // quoting escapes embedded quotes in a single /c string, which makes
  // `call "...VsDevCmd.bat"` fail before the developer environment loads.
  return runRequired(label, "cmd.exe", ["/d", "/s", "/c", "call", vsDevCmd,
    "-arch=x64", ">nul", "&&", command, ...args])
}

async function diskFree(pathValue) {
  const value = await statfs(pathValue)
  return Number(value.bavail) * Number(value.bsize)
}

async function snapshotResidue() {
  const entries = await readdir(tmpdir(), { withFileTypes: true })
  return new Set(entries.filter((entry) => entry.isDirectory() &&
    entry.name.startsWith("w-run-temp-")).map((entry) => entry.name))
}

function assertNoNewResidue(before, after) {
  const added = [...after].filter((name) => !before.has(name))
  assert(added.length === 0, `native runner left temporary directories: ${JSON.stringify(added)}`)
}

async function assertNoBuildResidue(directory, label = "public build") {
  const entries = await readdir(directory, { withFileTypes: true })
  const residue = entries.filter((entry) => entry.name.startsWith(".w-build-") ||
    entry.name.startsWith(".w-audit-"))
  assert(residue.length === 0,
    `${label} left staging entries: ${JSON.stringify(residue)}`)
}

async function readMaterialized(manifest) {
  const document = JSON.parse(await readFile(materializedPath, "utf8"))
  assert(document?.$schema === "w-seed-mlir0-windows-materialized-1" &&
    document.version === 1, "materialized manifest schema is invalid")
  assert(resolve(document.destination) === resolve(defaultCacheDirectory()),
    "materialized destination does not match the default external cache")
  assert(document.asset?.sha256 === manifest.asset?.sha256 &&
    document.asset?.sizeBytes === manifest.asset?.sizeBytes,
  "materialized asset does not match the checked-in pin")
  assert(document.distributionRole === "development-and-release-only" &&
    document.bundledWithW === false && document.extractedSizeIsWBudget === false,
  "materialized cache is not separated from W distribution")
  for (const name of ["mlir-opt.exe", "mlir-translate.exe", "opt.exe",
    "llc.exe", "lld-link.exe"]) {
    const record = document.tools?.[name]
    assert(record?.relativePath && !isAbsolute(record.relativePath),
      `materialized tool path is not relative: ${name}`)
    const pathValue = resolve(defaultCacheDirectory(), record.relativePath)
    const stats = await lstat(pathValue)
    assert(stats.isFile() && !stats.isSymbolicLink(),
      `materialized tool is not a regular file: ${name}`)
    assert(stats.size === record.sizeBytes, `materialized tool size changed: ${name}`)
    document.tools[name].absolutePath = pathValue
  }
  return document
}

function expectExact(binary, args, expectedExit, expectedStdout, label) {
  assertWindowsCommandLineSafe(binary, args, label)
  const result = spawn(binary, args)
  assert(result.exitCode === expectedExit && result.stdout.equals(expectedStdout) &&
    result.stderr.length === 0,
  `${label} was not exact: ${JSON.stringify({
    exitCode: result.exitCode,
    stdout: result.stdout.toString(),
    stderr: result.stderr.toString(),
  })}`)
}

function expectSourceFailure(binary, pathValue, label) {
  expectExact(binary, ["run", pathValue], 2, Buffer.alloc(0), label)
}

function expectBuildFailure(binary, args, label) {
  expectExact(binary, args, 2, Buffer.alloc(0), label)
}

async function verifyLinuxAuditTrace(directory, finalArtifact) {
  const expectedFiles = ["final-artifact", "input.mlir", "manifest.json",
    "optimized.ll", "output.ll", "output.obj", "verified.mlir", "wrt0.ll",
    "wrt0.obj"].sort()
  const entries = await readdir(directory, { withFileTypes: true })
  const actualFiles = entries.filter((entry) => entry.isFile())
    .map((entry) => entry.name).sort()
  assert(JSON.stringify(actualFiles) === JSON.stringify(expectedFiles),
    "Windows-host Linux audit trace inventory is not exact")
  const manifestBytes = await readFile(join(directory, "manifest.json"))
  const manifestText = manifestBytes.toString("utf8")
  const manifest = JSON.parse(manifestText)
  assert(manifest.schema === "w-seed-audit-trace-1" &&
    manifest.purpose === "development-only/non-ranking" &&
    manifest.closure_status === "inspection-required" &&
    manifest.product?.target === "x86_64-unknown-linux-gnu" &&
    manifest.product?.abi === "linux-gnu" &&
    manifest.product?.profile === "release" &&
    JSON.stringify(manifest.link?.inputs) ===
      JSON.stringify(["output.obj", "wrt0.obj"]),
  "Windows-host audit manifest does not bind the Linux multi-object product")
  assert(/^[0-9a-f]{64}$/u.test(manifest.compiler_binary_sha256) &&
    /^[0-9a-f]{64}$/u.test(manifest.source_id_sha256) &&
    Array.isArray(manifest.tools) && manifest.tools.length === 5 &&
    manifest.tools.every((tool) => /^[0-9a-f]{64}$/u.test(tool.sha256)),
  "Windows-host audit manifest hashes are incomplete")
  assert(!manifestText.includes(defaultCacheDirectory()) &&
    !manifestText.includes("W_MLIR0_TOOLCHAIN_ROOT") &&
    !manifestText.includes("argv") && !manifestText.includes("environment"),
  "Windows-host audit manifest includes a local path or invocation history")
  const records = manifest.artifacts
  assert(Array.isArray(records) && records.length === 8 &&
    JSON.stringify(records.map((record) => record.name)) ===
      JSON.stringify(["input.mlir", "verified.mlir", "output.ll",
        "optimized.ll", "output.obj", "wrt0.ll", "wrt0.obj",
        "final-artifact"]),
  "Windows-host audit manifest does not enumerate each emitted object")
  for (const record of records) {
    const bytes = await readFile(join(directory, record.name))
    assert(record.bytes === bytes.length &&
      record.sha256 === createHash("sha256").update(bytes).digest("hex"),
    `Windows-host audit manifest digest does not match ${record.name}`)
  }
  assert((await readFile(join(directory, "final-artifact"))).equals(
    await readFile(finalArtifact)),
  "Windows-host audit artifact is not byte-identical to the published output")
}

function assertPeX64(bytes, label) {
  assert(bytes.length >= 0x40 && bytes[0] === 0x4d && bytes[1] === 0x5a,
    `${label} is not an MZ image`)
  const peOffset = bytes.readUInt32LE(0x3c)
  assert(peOffset + 24 <= bytes.length && bytes.subarray(peOffset, peOffset + 4)
    .equals(Buffer.from("PE\0\0", "ascii")) && bytes.readUInt16LE(peOffset + 4) === 0x8664,
  `${label} is not a PE x64 image`)
}

function peSectionNames(bytes, label) {
  assertPeX64(bytes, label)
  const peOffset = bytes.readUInt32LE(0x3c)
  const sectionCount = bytes.readUInt16LE(peOffset + 6)
  const optionalHeaderSize = bytes.readUInt16LE(peOffset + 20)
  const tableOffset = peOffset + 24 + optionalHeaderSize
  assert(tableOffset + sectionCount * 40 <= bytes.length,
    `${label} has a truncated section table`)
  return Array.from({ length: sectionCount }, (_, index) => {
    const start = tableOffset + index * 40
    const terminator = bytes.indexOf(0, start)
    const end = terminator >= start && terminator < start + 8
      ? terminator : start + 8
    return bytes.subarray(start, end).toString("ascii")
  })
}

function peImportTable(bytes, label) {
  assertPeX64(bytes, label)
  const peOffset = bytes.readUInt32LE(0x3c)
  const fileHeader = peOffset + 4
  const sectionCount = bytes.readUInt16LE(fileHeader + 2)
  const optionalHeaderSize = bytes.readUInt16LE(fileHeader + 16)
  const optionalHeader = fileHeader + 20
  assert(optionalHeaderSize >= 128 &&
    optionalHeader + optionalHeaderSize <= bytes.length &&
    bytes.readUInt16LE(optionalHeader) === 0x20b,
  `${label} has an invalid PE32+ optional header`)
  const directoryCount = bytes.readUInt32LE(optionalHeader + 108)
  assert(directoryCount >= 2,
    `${label} has no complete PE import directory`)
  const importRva = bytes.readUInt32LE(optionalHeader + 120)
  const importSize = bytes.readUInt32LE(optionalHeader + 124)
  assert(importRva !== 0 && importSize >= 40 && importSize % 20 === 0,
    `${label} has an invalid PE import directory`)
  const sizeOfHeaders = bytes.readUInt32LE(optionalHeader + 60)
  const sectionTable = optionalHeader + optionalHeaderSize
  assert(sectionTable + sectionCount * 40 <= bytes.length,
    `${label} has a truncated PE section table`)
  const sections = Array.from({ length: sectionCount }, (_, index) => {
    const start = sectionTable + index * 40
    return {
      virtualSize: bytes.readUInt32LE(start + 8),
      virtualAddress: bytes.readUInt32LE(start + 12),
      rawSize: bytes.readUInt32LE(start + 16),
      rawPointer: bytes.readUInt32LE(start + 20),
    }
  })
  const rvaToOffset = (rva, length) => {
    assert(Number.isInteger(rva) && rva >= 0 && rva <= 0xffffffff &&
      Number.isInteger(length) && length > 0 && rva + length <= 0x100000000,
    `${label} has an invalid PE RVA range`)
    const matches = []
    if (rva < sizeOfHeaders) {
      assert(rva + length <= sizeOfHeaders && rva + length <= bytes.length,
        `${label} has an out-of-bounds PE header RVA`)
      matches.push(rva)
    }
    for (const section of sections) {
      const delta = rva - section.virtualAddress
      if (delta >= 0 && delta + length <= section.rawSize) {
        const offset = section.rawPointer + delta
        assert(offset + length <= bytes.length,
          `${label} has an out-of-bounds PE section RVA`)
        matches.push(offset)
      }
    }
    assert(matches.length === 1,
      matches.length === 0
        ? `${label} has an unmapped PE RVA 0x${rva.toString(16)}`
        : `${label} has an ambiguous PE RVA 0x${rva.toString(16)}`)
    return matches[0]
  }
  const readRvaAsciiString = (rva, description) => {
    const offset = rvaToOffset(rva, 1)
    const terminator = bytes.indexOf(0, offset)
    assert(terminator > offset,
      `${label} has an invalid or unterminated PE ${description}`)
    rvaToOffset(rva, terminator - offset + 1)
    for (let index = offset; index < terminator; index += 1)
      assert(bytes[index] >= 0x21 && bytes[index] <= 0x7e,
        `${label} has a non-printable PE ${description}`)
    return bytes.subarray(offset, terminator).toString("ascii")
  }
  const readThunkNames = (thunkRva, description) => {
    const names = []
    let terminated = false
    const maximumEntries = Math.min(65536, Math.floor(bytes.length / 8))
    for (let index = 0; index < maximumEntries; index += 1) {
      const currentRva = thunkRva + index * 8
      const entryOffset = rvaToOffset(currentRva, 8)
      const thunk = bytes.readBigUInt64LE(entryOffset)
      if (thunk === 0n) {
        terminated = true
        break
      }
      assert((thunk & 0x8000000000000000n) === 0n,
        `${label} has an ordinal ${description}; named provider imports are required`)
      assert(thunk <= 0xffffffffn && Number(thunk) <= 0xfffffffd,
        `${label} has an invalid PE import-by-name RVA`)
      const nameRva = Number(thunk) + 2
      rvaToOffset(Number(thunk), 3)
      names.push(readRvaAsciiString(nameRva, "import symbol name"))
    }
    assert(terminated && names.length > 0,
      `${label} has an unterminated or empty PE ${description}`)
    return names.sort()
  }
  const importOffset = rvaToOffset(importRva, importSize)
  const entries = []
  const seenDlls = new Set()
  let terminated = false
  for (let index = 0; index < importSize / 20; index += 1) {
    const descriptor = importOffset + index * 20
    const fields = Array.from({ length: 5 }, (_, field) =>
      bytes.readUInt32LE(descriptor + field * 4))
    if (fields.every((field) => field === 0)) {
      terminated = true
      break
    }
    const [originalFirstThunk, , , nameRva, firstThunk] = fields
    assert(nameRva !== 0,
      `${label} has an import descriptor without a DLL name`)
    assert(firstThunk !== 0,
      `${label} has an import descriptor without an import address table`)
    const name = readRvaAsciiString(nameRva, "import DLL name").toLowerCase()
    assert(/^[a-z0-9._-]+\.dll$/u.test(name),
      `${label} has an invalid PE import DLL name: ${name}`)
    assert(!seenDlls.has(name),
      `${label} has duplicate PE import descriptors for ${name}`)
    seenDlls.add(name)
    const lookupRva = originalFirstThunk || firstThunk
    const symbols = readThunkNames(lookupRva, `${name} import table`)
    const addressTableOffset = rvaToOffset(firstThunk,
      (symbols.length + 1) * 8)
    assert(bytes.readBigUInt64LE(addressTableOffset + symbols.length * 8) === 0n,
      `${label} has an unterminated ${name} import address table`)
    entries.push({ dll: name, symbols })
  }
  assert(terminated && entries.length > 0,
    `${label} has an unterminated or empty PE import table`)
  return entries.sort((left, right) => left.dll < right.dll ? -1 :
    left.dll > right.dll ? 1 : 0)
}

function expectedKernel32Symbols(route, label) {
  assert(route !== null && typeof route === "object" &&
    typeof route.usesProcessArgumentAdapter === "boolean" &&
    typeof route.writesStdout === "boolean",
  `${label} has no explicit Windows import route`)
  const symbols = ["ExitProcess"]
  if (route.usesProcessArgumentAdapter) symbols.push("GetCommandLineW")
  if (route.writesStdout) symbols.push("GetStdHandle", "WriteFile")
  return symbols.sort()
}

function assertKernel32OnlyImports(bytes, label, route) {
  const imports = peImportTable(bytes, label)
  const expected = [{
    dll: "kernel32.dll",
    symbols: expectedKernel32Symbols(route, label),
  }]
  assert(JSON.stringify(imports) === JSON.stringify(expected),
    `${label} has imports outside its exact Kernel32 route allowlist: ` +
      `expected ${JSON.stringify(expected)}, observed ${JSON.stringify(imports)}`)
  console.log(`W RUN Windows: imports ${label}: ${JSON.stringify(imports)}`)
  return imports
}

function expectPeImportFailure(operation, expectedMessage, label) {
  let caught
  try {
    operation()
  } catch (error) {
    caught = error
  }
  assert(caught instanceof Error && caught.message.includes(expectedMessage),
    `${label} was not rejected with ${JSON.stringify(expectedMessage)}; ` +
      `observed ${caught?.message ?? "acceptance"}`)
}

function syntheticPeImports({
  dll = "kernel32.dll",
  symbols = ["ExitProcess", "GetStdHandle", "WriteFile"],
  importDirectory = true,
  importSize = 40,
  originalFirstThunk = 0x1040,
  firstThunk = 0x1100,
  ambiguousSectionMapping = false,
  ordinal,
} = {}) {
  const bytes = Buffer.alloc(0x600)
  const peOffset = 0x80
  const fileHeader = peOffset + 4
  const optionalHeader = fileHeader + 20
  const sectionTable = optionalHeader + 0xf0
  bytes.write("MZ", 0, "ascii")
  bytes.writeUInt32LE(peOffset, 0x3c)
  bytes.write("PE\0\0", peOffset, "binary")
  bytes.writeUInt16LE(0x8664, fileHeader)
  bytes.writeUInt16LE(ambiguousSectionMapping ? 2 : 1, fileHeader + 2)
  bytes.writeUInt16LE(0xf0, fileHeader + 16)
  bytes.writeUInt16LE(0x20b, optionalHeader)
  bytes.writeUInt32LE(0x200, optionalHeader + 60)
  bytes.writeUInt32LE(16, optionalHeader + 108)
  if (importDirectory) {
    bytes.writeUInt32LE(0x1000, optionalHeader + 120)
    bytes.writeUInt32LE(importSize, optionalHeader + 124)
  }
  bytes.write(".rdata", sectionTable, "ascii")
  bytes.writeUInt32LE(0x400, sectionTable + 8)
  bytes.writeUInt32LE(0x1000, sectionTable + 12)
  bytes.writeUInt32LE(0x400, sectionTable + 16)
  bytes.writeUInt32LE(0x200, sectionTable + 20)
  if (ambiguousSectionMapping) {
    const secondSection = sectionTable + 40
    bytes.write(".other", secondSection, "ascii")
    bytes.writeUInt32LE(0x400, secondSection + 8)
    bytes.writeUInt32LE(0x1000, secondSection + 12)
    bytes.writeUInt32LE(0x400, secondSection + 16)
    bytes.writeUInt32LE(0x200, secondSection + 20)
  }

  const sectionOffset = (rva) => 0x200 + (rva - 0x1000)
  const writeString = (offset, value) => {
    bytes.write(value, offset, "ascii")
    bytes[offset + Buffer.byteLength(value, "ascii")] = 0
  }
  const descriptor = sectionOffset(0x1000)
  bytes.writeUInt32LE(originalFirstThunk, descriptor)
  bytes.writeUInt32LE(0, descriptor + 4)
  bytes.writeUInt32LE(0, descriptor + 8)
  bytes.writeUInt32LE(0x1030, descriptor + 12)
  bytes.writeUInt32LE(firstThunk, descriptor + 16)
  writeString(sectionOffset(0x1030), dll)
  let nameOffset = sectionOffset(0x1080)
  const lookupTableRva = originalFirstThunk || firstThunk
  if (ordinal !== undefined && lookupTableRva !== 0) {
    bytes.writeBigUInt64LE(0x8000000000000000n | BigInt(ordinal),
      sectionOffset(lookupTableRva))
    bytes.writeBigUInt64LE(0x8000000000000000n | BigInt(ordinal),
      sectionOffset(0x1100))
  } else if (lookupTableRva !== 0) {
    for (let index = 0; index < symbols.length; index += 1) {
      const symbol = symbols[index]
      const hintNameRva = 0x1080 + (nameOffset - sectionOffset(0x1080))
      bytes.writeBigUInt64LE(BigInt(hintNameRva),
        sectionOffset(lookupTableRva) + index * 8)
      bytes.writeBigUInt64LE(BigInt(hintNameRva), sectionOffset(0x1100) + index * 8)
      bytes.writeUInt16LE(0, nameOffset)
      writeString(nameOffset + 2, symbol)
      nameOffset += 3 + Buffer.byteLength(symbol, "ascii")
    }
  }
  return bytes
}

function runPeImportPolicySelfTests() {
  const simpleOutput = syntheticPeImports()
  const processAdapterOutput = syntheticPeImports({
    symbols: ["ExitProcess", "GetCommandLineW", "GetStdHandle", "WriteFile"],
  })
  const processAdapterNoOutput = syntheticPeImports({
    symbols: ["ExitProcess", "GetCommandLineW"],
  })
  const standaloneOutputRoute = {
    usesProcessArgumentAdapter: false,
    writesStdout: true,
  }
  const processAdapterNoOutputRoute = {
    usesProcessArgumentAdapter: true,
    writesStdout: false,
  }
  const argumentsOutput = {
    usesProcessArgumentAdapter: true,
    writesStdout: true,
  }
  assertKernel32OnlyImports(simpleOutput, "synthetic standalone-output product",
    standaloneOutputRoute)
  assertKernel32OnlyImports(processAdapterOutput,
    "synthetic process-adapter-output product",
    argumentsOutput)
  assertKernel32OnlyImports(processAdapterNoOutput,
    "synthetic process-error product", processAdapterNoOutputRoute)
  assertKernel32OnlyImports(syntheticPeImports({
    originalFirstThunk: 0,
  }), "synthetic FirstThunk lookup product", standaloneOutputRoute)
  expectPeImportFailure(() => assertKernel32OnlyImports(processAdapterOutput,
    "synthetic over-imported standalone product", standaloneOutputRoute),
  "exact Kernel32 route allowlist", "argument import outside standalone route")
  expectPeImportFailure(() => peImportTable(syntheticPeImports({
    importDirectory: false,
  }), "synthetic missing import directory"), "invalid PE import directory",
  "missing import directory")
  expectPeImportFailure(() => peImportTable(syntheticPeImports({
    importSize: 20,
  }), "synthetic missing descriptor terminator"), "invalid PE import directory",
  "ambiguous unterminated descriptor table")
  expectPeImportFailure(() => peImportTable(syntheticPeImports({
    ambiguousSectionMapping: true,
  }), "synthetic ambiguous RVA mapping"), "ambiguous PE RVA",
  "ambiguous section mapping")
  expectPeImportFailure(() => peImportTable(syntheticPeImports({
    originalFirstThunk: 0,
    firstThunk: 0,
  }), "synthetic descriptor without thunk table"),
  "without an import address table", "ambiguous descriptor without thunk table")
  expectPeImportFailure(() => assertKernel32OnlyImports(
    syntheticPeImports({ dll: "user32.dll" }), "synthetic unexpected DLL",
    standaloneOutputRoute), "exact Kernel32 route allowlist", "unexpected DLL")
  expectPeImportFailure(() => assertKernel32OnlyImports(syntheticPeImports({
    symbols: ["ExitProcess", "GetStdHandle", "WriteFile", "VirtualAlloc"],
  }), "synthetic unexpected symbol", standaloneOutputRoute),
  "exact Kernel32 route allowlist", "unexpected Kernel32 symbol")
  expectPeImportFailure(() => peImportTable(syntheticPeImports({ ordinal: 42 }),
    "synthetic ordinal import"), "ordinal", "ordinal import")
  expectPeImportFailure(() => peImportTable(syntheticPeImports({ symbols: [] }),
    "synthetic empty thunk table"), "empty PE kernel32.dll import table",
  "empty import symbol table")
  console.log("W RUN Windows: PE import policy self-test passed " +
    "(process-error=ExitProcess/GetCommandLineW; " +
    "standalone=ExitProcess/GetStdHandle/WriteFile; " +
    "process-adapter-output=ExitProcess/GetCommandLineW/GetStdHandle/WriteFile)")
}

if (testPeImportPolicy) {
  runPeImportPolicySelfTests()
  process.exit(0)
}

if (process.platform !== "win32" || process.arch !== "x64") {
  unavailable(`${process.platform}/${process.arch}`)
}

const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
const manifestErrors = validateManifest(manifest)
assert(manifestErrors.length === 0, manifestErrors.join("; "))
assert(manifest.buildBoundary?.configuration?.cStandard === "23" &&
  manifest.buildBoundary?.configuration?.recoveryCStandard === "11" &&
  manifest.buildBoundary?.configuration?.cStandardPolicy ===
    "C23-requested; MSVC-clatest-preview-correctness-only; C11-explicit-recovery-only",
"Windows C standard policy is not explicit")
const cStandard = "11"
console.log(
  "W RUN Windows: C23 request is an MSVC /std:clatest preview only; " +
  "using explicit C11 recovery",
)
assert(outsideRepository(defaultCacheDirectory()),
  "default toolchain cache must be outside the repository")
assert(manifest.runtimeBoundary?.network === "forbidden" &&
  manifest.runtimeBoundary?.pathSearch === "forbidden" &&
  manifest.runtimeBoundary?.shell === "forbidden",
"runtime boundary is not closed")
const materialized = await readMaterialized(manifest)
const sdk = await findWindowsSdkKernel32()
const visualStudio = findVisualStudio()
const vsDevCmd = visualStudio.devCommand
const cmake = Bun.which("cmake")
const ninja = Bun.which("ninja")
assert(cmake && ninja, "CMake/Ninja are unavailable to the Windows gate")

const runSource = await readFile(resolve(seedDirectory, "cli", "run.c"), "utf8")
const buildSource = await readFile(resolve(seedDirectory, "cli", "build.c"), "utf8")
const emitterSource = await readFile(resolve(seedDirectory, "src", "w_seed_mlir0.c"),
  "utf8")
const windowsSourceStart = runSource.indexOf("#elif defined(_WIN32)")
const windowsSourceEnd = runSource.indexOf("#else", windowsSourceStart)
assert(windowsSourceStart >= 0 && windowsSourceEnd > windowsSourceStart,
  "native Windows runner section is missing")
const windowsSource = runSource.slice(windowsSourceStart, windowsSourceEnd)
for (const forbidden of ["wsl.exe", "process.env.PATH", "exec(", "shell: true", "clang"]) {
  assert(!windowsSource.includes(forbidden),
    `native Windows runner contains forbidden boundary: ${forbidden}`)
}
for (const marker of ["CreateProcessW", "lpApplicationName", "CREATE_NEW",
  "GetStdHandle", "WriteFile", "ExitProcess", "mainCRTStartup",
  "-mtriple=x86_64-pc-windows-msvc", "/nodefaultlib", "--canonicalize",
  "--cse", "-O3", "no-builtin-wcslen", "no-builtin-strlen",
  "-relocation-model=static", "-no-pie", "--no-dynamic-linker",
  "--gc-sections", "noexecstack",
  "/Brepro", "/opt:ref", "/opt:icf", "/incremental:no",
  "/merge:.pdata=.rdata"]) {
  assert(`${runSource}\n${emitterSource}`.includes(marker),
    `native Windows implementation marker is missing: ${marker}`)
}
assert(!runSource.includes("--disable-simplify-libcalls"),
  "cli/run.c must preserve LLVM libcall simplification")
assert(emitterSource.includes("llvm.mlir.zero") &&
  !emitterSource.includes("HeapAlloc") && !emitterSource.includes("HeapFree"),
"Windows emitter must use the bounded global buffer without Heap APIs")
assert(runSource.includes("W_SEED_RUN_COMPILE_PROFILE_DEV"),
  "cli/run.c does not select the development compile profile for w run")
assert(buildSource.includes("W_SEED_RUN_COMPILE_PROFILE_RELEASE"),
  "cli/build.c does not select the release compile profile for w build")
assert(buildSource.includes("--audit-dir") &&
  buildSource.includes("build_windows_copy_audit_bundle") &&
  runSource.includes("retain_audit_trace") &&
  runSource.includes("manifest.json") && runSource.includes("wrt0.obj"),
"Windows-host Linux audit trace route is incomplete")
for (const name of ["mlir-opt.exe", "mlir-translate.exe", "opt.exe",
  "llc.exe", "lld-link.exe"])
  runRequired(`${name} version`, materialized.tools[name].absolutePath, ["--version"])

const unsupportedBuildDirectory = await mkdtemp(join(tmpdir(), "w-run-windows-disabled-"))
const buildDirectory = await mkdtemp(join(tmpdir(), "w-run-windows-build-"))
const fixtureDirectory = await mkdtemp(join(tmpdir(), "w-run-windows-fixtures-"))
const residueBefore = await snapshotResidue()
const diskBefore = await diskFree(buildDirectory)
try {
  const disabledCmakeArguments = [
    "-S", seedDirectory, "-B", unsupportedBuildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=cl",
    `-DCMAKE_MAKE_PROGRAM=${ninja}`, `-DW_SEED_C_STANDARD=${cStandard}`, "-DW_SEED_ENABLE_WINDOWS_NATIVE_RUN=OFF",
  ]
  runWithVs("disabled Windows configure", vsDevCmd, cmake,
    disabledCmakeArguments)
  runWithVs("disabled Windows w build", vsDevCmd, cmake,
    ["--build", unsupportedBuildDirectory, "--target", "w", "--", "-j", "2"])
  const disabledBinary = join(unsupportedBuildDirectory, "w.exe")
  expectExact(disabledBinary, ["run", helloFixture], 2, Buffer.alloc(0),
    "disabled native run")
  const disabledArtifact = join(fixtureDirectory, "disabled-build.exe")
  expectBuildFailure(disabledBinary, ["build", helloFixture, "--target",
    targetTriple, "--output", disabledArtifact], "disabled native build")
  assert(!existsSync(disabledArtifact), "disabled native build left an artifact")
  assertNoNewResidue(residueBefore, await snapshotResidue())

  const nativeCmakeArguments = (toolOverrides = {}) => [
    "-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=cl",
    `-DCMAKE_MAKE_PROGRAM=${ninja}`,
    `-DW_SEED_C_STANDARD=${cStandard}`,
    "-DW_SEED_ENABLE_WINDOWS_NATIVE_RUN=ON",
    `-DW_MLIR0_WINDOWS_MLIR_OPT=${toolOverrides.mlirOpt ?? materialized.tools["mlir-opt.exe"].absolutePath}`,
    `-DW_MLIR0_WINDOWS_MLIR_TRANSLATE=${toolOverrides.mlirTranslate ?? materialized.tools["mlir-translate.exe"].absolutePath}`,
    `-DW_MLIR0_WINDOWS_LLVM_OPT=${toolOverrides.llvmOpt ?? materialized.tools["opt.exe"].absolutePath}`,
    `-DW_MLIR0_WINDOWS_LLC=${toolOverrides.llc ?? materialized.tools["llc.exe"].absolutePath}`,
    `-DW_MLIR0_WINDOWS_LLD_LINK=${toolOverrides.linkDriver ?? materialized.tools["lld-link.exe"].absolutePath}`,
    `-DW_MLIR0_WINDOWS_KERNEL32_LIB=${sdk.path}`,
  ]
  function configureNative(label, toolOverrides = {}) {
    runWithVs(`${label} configure`, vsDevCmd, cmake,
      nativeCmakeArguments(toolOverrides))
    runWithVs(`${label} w build`, vsDevCmd, cmake,
      ["--build", buildDirectory, "--target", "w", "--", "-j", "2"])
  }
  configureNative("native")
  const binary = join(buildDirectory, "w.exe")
  const binaryStats = await lstat(binary)
  assert(binaryStats.isFile() && !binaryStats.isSymbolicLink(),
    "native build did not produce w.exe")
  assert(!existsSync(join(buildDirectory, "bin", "mlir-opt.exe")),
    "native build copied the external MLIR toolchain")
  assert(!existsSync(join(buildDirectory, "bin", "opt.exe")),
    "native build copied the external LLVM optimizer")

  const invalidSource = join(fixtureDirectory, "invalid.w")
  const invalidTruncatingBits = join(fixtureDirectory,
    "invalid-truncating-bits.w")
  const invalidSaturatingLabel = join(fixtureDirectory,
    "invalid-saturating-label.w")
  const invalidNumericWidening = join(fixtureDirectory,
    "invalid-numeric-widening.w")
  const runtimeCheckedI8Overflow = join(fixtureDirectory,
    "runtime-checked-i8-overflow.w")
  const runtimeCheckedU16Underflow = join(fixtureDirectory,
    "runtime-checked-u16-underflow.w")
  const runtimeCheckedI16MultiplyOverflow = join(fixtureDirectory,
    "runtime-checked-i16-multiply-overflow.w")
  const runtimeShiftFaults = []
  const runtimeNegationMinimums = []
  const runtimeDivisionZero = join(fixtureDirectory,
    "runtime-division-zero.w")
  const runtimeDivisionOverflow = join(fixtureDirectory,
    "runtime-division-overflow.w")
  const runtimeRemainderZero = join(fixtureDirectory,
    "runtime-remainder-zero.w")
  const runtimeUnsignedDivisionZero = join(fixtureDirectory,
    "runtime-unsigned-division-zero.w")
  const runtimeUnsignedRemainderZero = join(fixtureDirectory,
    "runtime-unsigned-remainder-zero.w")
  const runtimeMinimumRemainder = join(fixtureDirectory,
    "runtime-minimum-remainder.w")
  const unsupportedSource = join(fixtureDirectory, "unsupported.w")
  const privateGraphDirectory = join(fixtureDirectory, "private-graph")
  const privateGraphRoot = join(privateGraphDirectory, "app.w")
  const privateGraphLibrary = join(privateGraphDirectory, "lib.w")
  const invalidComparisons = [
    ["Bool operands", "true == false"],
    ["String operands", '"a" != "b"'],
    ["mixed operands", "1 <= true"],
    ["comparison used as i64", "(1 < 2) + 3"],
  ]
  await writeFile(invalidSource, Buffer.from([0xc3]))
  await writeFile(invalidTruncatingBits,
    "fn main() { print(\"must not commit\") " +
    "let result = i8(exactly: 258_i16) }\nentry(main)\n", "utf8")
  await writeFile(invalidSaturatingLabel,
    "entry { print(\"must not commit\") " +
    "let value = i8(saturating: 128_i16, other: 0_i16) }\n", "utf8")
  await writeFile(invalidNumericWidening,
    "entry { print(\"must not commit\") let value: f32 = 1_i32 }\n", "utf8")
  await writeFile(runtimeCheckedI8Overflow,
    "fn add(left: i8, right: i8): i8 { return left + right }\n" +
    "entry { print(\"must not commit\") " +
    "let result = add(left: 127_i8, right: 1_i8) print(\"${result}\") }\n")
  await writeFile(runtimeCheckedU16Underflow,
    "fn subtract(left: u16, right: u16): u16 { var result = left " +
    "result -= right return result }\n" +
    "entry { print(\"must not commit\") " +
    "let result = subtract(left: 0_u16, right: 1_u16) print(\"${result}\") }\n")
  await writeFile(runtimeCheckedI16MultiplyOverflow,
    "fn multiply(left: i16, right: i16): i16 { return left * right }\n" +
    "entry { print(\"must not commit\") " +
    "let result = multiply(left: 200_i16, right: 200_i16) " +
    "print(\"${result}\") }\n")
  for (const [name, type, value, count, operator, label] of [
    ["runtime-shift-count-i8", "i8", "1_i8", "8_u64", ">>", "signed i8 shift count at logical width"],
    ["runtime-shift-count-u8", "u8", "1_u8", "8_u64", ">>", "unsigned u8 shift count at logical width"],
    ["runtime-signed-shift-loss-i8", "i8", "64_i8", "1_u64", "<<", "signed i8 shift loses a sign bit"],
    ["runtime-negative-shift-loss-i8", "i8", "-64_i8", "2_u64", "<<", "negative signed i8 shift loses high bits"],
    ["runtime-unsigned-shift-loss-u8", "u8", "128_u8", "1_u64", "<<", "unsigned u8 shift loses a high bit"],
  ]) {
    const path = join(fixtureDirectory, `${name}.w`)
    await writeFile(path,
      "// Expected exit: nonzero (trap)\n// Expected stdout: <empty>\n" +
      `fn shift(value: ${type}, count: UInt): ${type} { return value ${operator} count }\n` +
      "entry { print(\"must not commit\") " +
      `let result = shift(value: ${value}, count: ${count}) ` +
      "print(\"${result}\") }\n")
    runtimeShiftFaults.push([path, label])
  }
  for (const [name, type, value, count] of [
    ["runtime-logical-shift-count-i8", "i8", "1_i8", "8_u64"],
    ["runtime-logical-shift-count-u64", "u64", "1_u64", "64_u64"],
  ]) {
    const path = join(fixtureDirectory, `${name}.w`)
    await writeFile(path,
      "// Expected exit: nonzero (trap)\n// Expected stdout: <empty>\n" +
      `fn shift(value: ${type}, count: UInt): ${type} { ` +
      `return ${type}.logicalShiftRight(value, count) }\n` +
      "entry { print(\"must not commit\") " +
      `let result = shift(value: ${value}, count: ${count}) ` +
      "print(\"${result}\") }\n")
    runtimeShiftFaults.push([path,
      `${type}.logicalShiftRight rejects a count at logical width`])
  }
  for (const [type, maximum, suffix] of [
    ["i8", "127", "_i8"],
    ["i16", "32767", "_i16"],
    ["i32", "2147483647", "_i32"],
    ["i64", "9223372036854775807", "_i64"],
    ["Int", "9223372036854775807", "_i64"],
  ]) {
    const path = join(fixtureDirectory,
      `runtime-${type.toLowerCase()}-negation-minimum.w`)
    await writeFile(path,
      "fn negate(value: " + type + "): " + type + " { return -value }\n" +
      "entry { print(\"must not commit\") " +
      "let result = negate(value: ~" + maximum + suffix + ") " +
      "print(\"${result}\") }\n")
    runtimeNegationMinimums.push([path, `${type} unary negation of logical minimum`])
  }
  await writeFile(runtimeDivisionZero,
    "fn divide(value: i64, by divisor: i64): i64 { return value / divisor }\n" +
    "entry { print(\"must not commit\") " +
    "let result = divide(value: 8, by: 0) print(\"${result}\") }\n")
  await writeFile(runtimeDivisionOverflow,
    "fn divide(value: i64, by divisor: i64): i64 { return value / divisor }\n" +
    "entry { print(\"must not commit\") let result = divide(" +
    "value: 0 - 9223372036854775807 - 1, by: 0 - 1) " +
    "print(\"${result}\") }\n")
  await writeFile(runtimeRemainderZero,
    "fn remainder(value: i64, by divisor: i64): i64 { return value % divisor }\n" +
    "entry { print(\"must not commit\") " +
    "let result = remainder(value: 8, by: 0) print(\"${result}\") }\n")
  await writeFile(runtimeUnsignedDivisionZero,
    "fn divide(value: u64, by divisor: u64): u64 { return value / divisor }\n" +
    "entry { print(\"must not commit\") " +
    "let result = divide(value: 8_u64, by: 0_u64) print(\"${result}\") }\n")
  await writeFile(runtimeUnsignedRemainderZero,
    "fn remainder(value: u64, by divisor: u64): u64 { return value % divisor }\n" +
    "entry { print(\"must not commit\") " +
    "let result = remainder(value: 8_u64, by: 0_u64) print(\"${result}\") }\n")
  await writeFile(runtimeMinimumRemainder,
    "fn remainder(value: i64, by divisor: i64): i64 { return value % divisor }\n" +
    "entry { let result = remainder(" +
    "value: 0 - 9223372036854775807 - 1, by: 0 - 1) " +
    "print(\"${result}\") }\n")
  await writeFile(unsupportedSource, "fn main() { noop(\"Other\") }\nentry(main)\n")
  await mkdir(privateGraphDirectory)
  await writeFile(privateGraphRoot,
    "import { helper as h } from lib\n" +
    "fn run() { let value = h() print(\"answer ${value}\") }\n" +
    "entry(run)\n")
  await writeFile(privateGraphLibrary,
    "module lib\nfn helper(): i64 { return 42 }\n")
  for (const [index, [label, expression]] of invalidComparisons.entries()) {
    const path = join(fixtureDirectory, `invalid_comparison_${index}.w`)
    await writeFile(path,
      `fn main() { let result = ${expression} print("\${result}") }\nentry(main)\n`)
    invalidComparisons[index] = [label, path]
  }
  const expectedIf = Buffer.from(
    "Kitchen open\nAfter service\nKitchen closed\nAfter service\n", "utf8")
  expectExact(binary, ["--help"], 0, Buffer.from(expectedHelp), "w --help")
  expectExact(binary, ["build", "--help"], 0, Buffer.from(expectedHelp),
    "w build --help")
  expectExact(binary, ["run", helloFixture], 0,
    Buffer.from("Hello, world!\n", "utf8"), "Hello fixture")
  expectExact(binary, ["run", flatValueAggregatesFixture], 0,
    flatValueAggregatesOutput,
    "flat tuple and immutable value-struct source through development w run")
  expectExact(binary, ["run", localGraphFixture], 0,
    Buffer.from("answer 42\n", "utf8"),
    "resolved local-module graph fixture")
  expectSourceFailure(binary, privateGraphRoot,
    "private cross-module symbol")
  expectExact(binary, ["run", ifFixture], 0, expectedIf,
    "Restaurant if fixture")
  expectExact(binary, ["run", enumFixture], 0,
    Buffer.from("Courses 10/30/20\n", "utf8"),
    "Restaurant payloadless enum exhaustive switch fixture")
  expectExact(binary, ["run", enumSubsetFixture], 0,
    Buffer.from("Work 1/2\n", "utf8"),
    "Restaurant payloadless enum subset switch fixture")
  expectExact(binary, ["run", enumPayloadFixture], 0,
    Buffer.from("Bills 32/44/10/7\n", "utf8"),
    "Restaurant enum payload return, reordered captures, and shared variant storage")
  expectExact(binary, ["run", enumCfgJoinFixture], 0,
    enumCfgJoinOutput,
    "bounded payload enum through one typed if join and exhaustive switch")
  const enumPayloadMutation = join(fixtureDirectory, "enum-payload-mutation.w")
  const enumPayloadSource = await readFile(enumPayloadFixture, "utf8")
  const enumPayloadMutatedSource = enumPayloadSource
    .replace(".main(tax: tax, price: price)", ".main(tax: tax + 1, price: price)")
    .replace("price: 30, tax: 2", "price: -30, tax: 2")
    .replace(".dessert(price: 7)", ".dessert(price: -7)")
  assert(enumPayloadMutatedSource !== enumPayloadSource,
    "enum payload mutation did not change its source")
  await writeFile(enumPayloadMutation, enumPayloadMutatedSource, "utf8")
  expectExact(binary, ["run", enumPayloadMutation], 0,
    Buffer.from("Bills -27/45/10/-7\n", "utf8"),
    "Enum payload runtime arithmetic and negative source mutation")
  expectExact(binary, ["run", enumBoolPayloadFixture], 0,
    Buffer.from("States true/false/false/true; charges 17/31; licensed true\n", "utf8"),
    "Restaurant Bool and i64 payload union with reordered fields and captures")
  const enumBoolMutation = join(fixtureDirectory, "enum-bool-payload-mutation.w")
  const enumBoolSource = await readFile(enumBoolPayloadFixture, "utf8")
  const enumBoolMutatedSource = enumBoolSource
    .replace("licensed: true, open: open", "licensed: false, open: open")
    .replace(".charge(amount: 17)", ".charge(amount: -17)")
    .replace(".checked(amount: 31, open: true)", ".checked(amount: -31, open: false)")
  assert(enumBoolMutatedSource !== enumBoolSource,
    "enum Bool payload mutation did not change its source")
  await writeFile(enumBoolMutation, enumBoolMutatedSource, "utf8")
  expectExact(binary, ["run", enumBoolMutation], 0,
    Buffer.from("States true/false/false/false; charges -17/-31; licensed false\n", "utf8"),
    "Enum Bool byte independence and signed-i64 payload mutation")
  for (const mixed of [false, true]) {
    const fields = Array.from({ length: 9 }, (_, index) => `b${index}`)
    const payload = fields.map(field => `${field}: Bool`).join(", ")
    const sourcePath = join(fixtureDirectory, `enum-bool-${mixed ? "mixed" : "pure"}-lanes.w`)
    // Stay within the native subset's eight-function bound without weakening
    // coverage: each executable reads one half of the same nine-field value.
    for (const readFields of [fields.slice(0, 5), fields.slice(5)]) {
      const readers = readFields.map(field =>
        `fn read${field}(state: Flags): Bool { return switch state { ` +
        `case .bits(${field}: let value, ...): value case .none: false ` +
        (mixed ? "case .amount(value: _): false " : "") + "} }").join("\n")
      for (const inverted of [false, true]) {
        const value = index => (index % 2 === 0) !== inverted
        const argumentsText = [...fields].reverse().map(field =>
          `${field}: ${value(Number(field.slice(1)))}`).join(", ")
        const bindings = readFields.map(field =>
          `let ${field} = read${field}(state: state)`).join("\n")
        const interpolation = readFields.map(field => "${" + field + "}").join("/")
        await writeFile(sourcePath,
          `enum Flags { none bits(${payload}) ${mixed ? "amount(value: i64)" : ""} }\n` +
          readers + `\nentry { let state: Flags = .bits(${argumentsText})\n` +
          bindings + `\nprint("${interpolation}") }\n`, "utf8")
        expectExact(binary, ["run", sourcePath], 0,
          Buffer.from(readFields.map(field => String(value(Number(field.slice(1))))).join("/") + "\n", "utf8"),
          `Enum Bool ${mixed ? "mixed" : "pure"} byte lanes ${readFields.join(",")} with reversed labels (${inverted ? "inverted" : "alternating"})`)
      }
    }
  }
  expectExact(binary, ["run", whileFixture], 0,
    Buffer.from("Served 3\n", "utf8"),
    "Restaurant structured natural while fixture")
  expectExact(binary, ["run", whileMultiFixture], 0,
    Buffer.from("Served 9\n", "utf8"),
    "Restaurant structured multi-carrier natural while fixture")
  expectExact(binary, ["run", whilePostFixture], 0,
    Buffer.from("Final 9\n", "utf8"),
    "Restaurant post-loop SSA continuation fixture")
  expectExact(binary, ["run", whileBreakContinueFixture], 0,
    Buffer.from("0,4,8\n", "utf8"),
    "verified loop CFG with break and continue")
  expectExact(binary, ["run", nestedLabeledWhileFixture], 0,
    Buffer.from("0,1,3\n", "utf8"),
    "verified nested labeled loop CFG")
  expectExact(binary, ["run", nestedLoopTerminalReturnsFixture], 0,
    Buffer.from("-1,1,3\n", "utf8"),
    "verified nested loop CFG with terminal return branches")
  expectExact(binary, ["run", repeatFixture], 0,
    Buffer.from("Receipt digits 1/5\n", "utf8"),
    "Restaurant post-test repeat fixture")
  expectExact(binary, ["run", wmoFixture], 0,
    Buffer.from("Bill 42\n", "utf8"),
    "Restaurant whole-module product closure fixture")
  expectExact(binary, ["run", asyncJoinFixture], 0,
    Buffer.from("Prepared 42\n", "utf8"),
    "Restaurant virtual structured-task elision fixture")
  expectExact(binary, ["run", asyncYieldFixture], 0,
    Buffer.from("Prepared 88\n", "utf8"),
    "Restaurant virtual Task with statically discharged yields")
  expectExact(binary, ["run", mainDispatchFixture], 0,
    Buffer.from("Dispatched 88\n", "utf8"),
    "Restaurant physical main-domain dispatch")
  expectExact(binary, ["run", mainCardinalityFixture], 0,
    Buffer.from("Dispatched 92\n", "utf8"),
    "Restaurant bounded main-domain cardinality")
  expectExact(binary, ["run", comparisonsFixture], 0,
    Buffer.from("Seat party\nSeat party\nWaitlist\n", "utf8"),
    "Restaurant signed-i64 admission comparison")
  expectExact(binary, ["run", integerComparisonFixture], 0,
    Buffer.from(
      "i8 false/true/true/true/false/false\n" +
      "u8 false/true/false/false/true/true\n" +
      "i16 true/false/false/true/false/true\n" +
      "u16 false/true/true/true/false/false\n" +
      "i32 false/true/true/true/false/false\n" +
      "u32 false/true/false/false/true/true\n" +
      "i64 false/true/true/true/false/false\n" +
      "u64 false/true/false/false/true/true\n" +
      "Int false/true/true/true/false/false\n" +
      "UInt true/false/false/true/false/true\n" +
      "widen u8->i16 true\n", "utf8"),
    "Restaurant fixed-width integer comparison family")
  expectExact(binary, ["run", comparisonCompositionFixture], 0,
    Buffer.from(
      "false/true/true/true/false/false\n" +
      "true/false/false/true/false/true\n" +
      "false/true/false/false/true/true\n" +
      "false/true/true/true/false/false\n" +
      "false/true/false/false/true/true\nAllowed true\nAllowed false\n", "utf8"),
    "Restaurant comparison operators, signed endpoints, and Bool composition")
  expectExact(binary, ["run", integerBitwiseFixture], 0,
    Buffer.from(
      "i8 10/-81/-91\n" +
      "u8 10/175/165\n" +
      "i16 2570/-20561/-23131\n" +
      "u16 2570/44975/42405\n" +
      "i32 168430090/-1347440721/-1515870811\n" +
      "u32 168430090/2947526575/2779096485\n" +
      "i64 723401728380766730/-5787213827046133841/-6510615555426900571\n" +
      "u64 723401728380766730/12659530246663417775/11936128518282651045\n" +
      "Int 723401728380766730/-5787213827046133841/-6510615555426900571\n" +
      "UInt 723401728380766730/12659530246663417775/11936128518282651045\n" +
      "Widened -13\n" +
      "Mixed 255\n",
      "utf8"),
    "Fixed-width signed/unsigned integer bitwise family")
  expectExact(binary, ["run", boolShortCircuitFixture], 0,
    Buffer.from(
      "Override checked\nClosed allowed true\nCapacity checked\n" +
      "Open allowed true\n", "utf8"),
    "Restaurant Bool short-circuit fixture")
  expectExact(binary, ["run", scalarIfFixture], 0,
    Buffer.from("Open 5; closed 2\n", "utf8"),
    "Restaurant scalar-if fixture")
  expectExact(binary, ["run", terminalReturnsFixture], 0,
    Buffer.from("-1,0,1\n", "utf8"),
    "terminal scalar branch returns")
  expectExact(binary, ["run", interpolationFixture], 0,
    Buffer.from("Table 42 remains open\n", "utf8"),
    "Restaurant interpolation fixture")
  expectExact(binary, ["run", linearFixture], 0,
    Buffer.from("Table 42 remains open\nKitchen is ready\n", "utf8"),
    "Restaurant linear fixture")
  expectExact(binary, ["run", unaryNegateFixture], 0,
    Buffer.from("Balance -7\n", "utf8"),
    "Restaurant checked runtime unary negation")
  expectExact(binary, ["run", unaryInterpolationFixture], 0,
    Buffer.from("Balance -7\n", "utf8"),
    "Restaurant direct unary interpolation")
  expectExact(binary, ["run", unsignedFixture], 0,
    Buffer.from("Unsigned 18446744073709551615\n", "utf8"),
    "Restaurant full-width UInt parameter, return, and interpolation")
  expectExact(binary, ["run", shiftsFixture], 0,
    Buffer.from(
      "i8 -16/-128\nu8 32/128\ni16 -4096/-32768\nu16 8192/32768\n" +
      "i32 -268435456/-2147483648\nu32 536870912/2147483648\n" +
      "i64 -1152921504606846976/-9223372036854775808\n" +
      "u64 2305843009213693952/9223372036854775808\n" +
      "Int -1152921504606846976/-9223372036854775808\n" +
      "UInt 2305843009213693952/9223372036854775808\n", "utf8"),
    "Restaurant checked signed and unsigned shifts across logical widths")
  expectExact(binary, ["run", powerFixture], 0,
    Buffer.from("Power -27/1024/1/512\n", "utf8"),
    "Restaurant checked signed and unsigned power")
  expectExact(binary, ["run", powerPrefixFixture], 0,
    Buffer.from("Power prefix -4/4/512/-9/-27\n", "utf8"),
    "Restaurant prefix and power precedence")
  expectExact(binary, ["run", compoundFixture], 0,
    Buffer.from("Compound 11\n", "utf8"),
    "Restaurant checked compound assignment")
  expectExact(binary, ["run", floatStrictFixture], 0,
    Buffer.from("Float strict ok\n", "utf8"),
    "Restaurant strict f32/f64 arithmetic and IEEE comparisons")
  expectExact(binary, ["run", floatBitRepresentationFixture], 0,
    Buffer.from(
      "Float bits f32 2147483648/2139095040/2143363909 " +
      "f64 9223372036854775808/9218868437227405312/9221140253039434428\n",
      "utf8"),
    "Restaurant exact f32/f64 bit representation round trips")
  expectExact(binary, ["run", numericWideningFixture], 0,
    Buffer.from("Numeric widen ok\n", "utf8"),
    "Restaurant exact implicit integer/float widening")
  expectExact(binary, ["run", checkedIntegerArithmeticFixture], 0,
    Buffer.from(
      "i8 -9/-15/-36; divrem -4/0; compound -2\nu8 43/37/120; divrem 13/1; compound 2\n" +
      "i16 -970/-1030/-30000; divrem -33/-10; compound -12\n" +
      "u16 1030/970/30000; divrem 33/10; compound 8\n" +
      "i32 -117000/-123000/-360000000; divrem -40/0; compound -2\n" +
      "u32 100300/99700/30000000; divrem 333/100; compound 98\n" +
      "i64 -600000/-1200000/-270000000000; divrem -3/0; compound -2\n" +
      "u64 6000000000/4000000000/5000000000000000000; divrem 5/0; compound 999999998\n" +
      "Int -4000000000/-6000000000/-5000000000000000000; divrem -5/0; compound -2\n" +
      "UInt 9000000000/3000000000/18000000000000000000; divrem 2/0; compound 2999999998\n", "utf8"),
    "Restaurant checked signed/unsigned integer arithmetic family")
  expectExact(binary, ["run", integerPrefixFixture], 0,
    Buffer.from(
      "i8 -7/-43\ni16 -7/-43\ni32 -7/-43\ni64 -7/-43\n" +
      "Int -7/-43\nu8 170\nu16 65450\nu32 4294967210\n" +
      "u64 18446744073709551530\nUInt 18446744073709551530\n" +
      "literal -7\n", "utf8"),
    "Restaurant integer prefix width and signedness family")
  expectExact(binary, ["run", runtimeMinimumRemainder], 0,
    Buffer.from("0\n", "utf8"),
    "runtime signed minimum remainder by negative one")
  for (const [pathValue, label] of [
    [runtimeDivisionZero, "runtime signed division by zero"],
    [runtimeDivisionOverflow, "runtime signed division overflow"],
    [runtimeRemainderZero, "runtime signed remainder by zero"],
    [runtimeUnsignedDivisionZero, "runtime unsigned division by zero"],
    [runtimeUnsignedRemainderZero, "runtime unsigned remainder by zero"],
  ]) {
    const failure = spawn(binary, ["run", pathValue])
    assert(failure.exitCode !== 0 && failure.stdout.length === 0 &&
      failure.stderr.length === 0,
      `${label} must trap silently without committing buffered output: ${JSON.stringify({
        exitCode: failure.exitCode,
        stdout: failure.stdout.toString(),
        stderr: failure.stderr.toString(),
      })}`)
  }
  for (const [path, label] of [
    [runtimeCheckedI8Overflow, "signed i8 checked addition overflow"],
    [runtimeCheckedU16Underflow, "unsigned u16 checked compound subtraction underflow"],
    [runtimeCheckedI16MultiplyOverflow, "signed i16 checked multiplication overflow"],
    ...runtimeShiftFaults,
  ]) {
    const failure = spawn(binary, ["run", path])
    assert(failure.exitCode !== 0 && failure.stdout.length === 0 &&
      failure.stderr.length === 0,
      `${label} must trap silently without committing buffered output: ${JSON.stringify({
        exitCode: failure.exitCode,
        stdout: failure.stdout.toString(),
        stderr: failure.stderr.toString(),
      })}`)
  }
  for (const [path, label] of runtimeNegationMinimums) {
    const failure = spawn(binary, ["run", path])
    assert(failure.exitCode !== 0 && failure.stdout.length === 0 &&
      failure.stderr.length === 0,
      `${label} must trap silently without committing output: ${JSON.stringify({
        exitCode: failure.exitCode,
        stdout: failure.stdout.toString(),
        stderr: failure.stderr.toString(),
      })}`)
  }
  expectExact(binary, ["run", integerWrappingFixture], 0,
    Buffer.from(
      "i8/u8 -128/0\ni16/u16 32767/2\ni32/u32 -2/4294967295\n" +
      "i64/u64 -9223372036854775808/0\n" +
      "Int/UInt -9223372036854775808/18446744073709551615\n", "utf8"),
    "Restaurant fixed-width integer wrapping policy")
  expectExact(binary, ["run", integerWideningFixture], 0,
    Buffer.from("Widen -7/200/202/203\n", "utf8"),
    "Restaurant implicit integer widening policy")
  expectExact(binary, ["run", integerTruncatingBitsFixture], 0,
    Buffer.from("Trunc 2/-7/-6/18446744073709551609/-1\n", "utf8"),
    "Restaurant explicit fixed-width truncatingBits family")
  expectExact(binary, ["run", integerSaturatingConversionFixture], 0,
    fixtureIntegerSaturatingConversionOutput,
    "Restaurant four-quadrant saturating conversions and UInt-to-Int alias")
  expectSourceFailure(binary, invalidTruncatingBits,
    "wrong truncatingBits label fails before output commit")
  expectSourceFailure(binary, invalidSaturatingLabel,
    "wrong saturating label fails before output commit")
  expectSourceFailure(binary, invalidNumericWidening,
    "inexact implicit i32-to-f32 conversion fails before output commit")
  expectExact(binary, ["run", uIntWrappingAddFixture], 0,
    Buffer.from("Wrapped 0\n", "utf8"),
    "Restaurant UInt wrappingAdd at the unsigned maximum")
  expectExact(binary, ["run", uIntWrappingSubtractFixture], 0,
    Buffer.from("Wrapped 18446744073709551615\n", "utf8"),
    "Restaurant UInt wrappingSubtract below zero")
  expectExact(binary, ["run", uIntWrappingMultiplyFixture], 0,
    Buffer.from("Wrapped 18446744073709551614\n", "utf8"),
    "Restaurant UInt wrappingMultiply at the unsigned maximum")
  expectExact(binary, ["run", uIntWrappingNegateFixture], 0,
    Buffer.from("Wrapped 18446744073709551615\n", "utf8"),
    "Restaurant UInt wrappingNegate of one")
  expectExact(binary, ["run", uIntWrappingPowerFixture], 0,
    Buffer.from("Wrapped 12157665459056928801\n", "utf8"),
    "Restaurant UInt wrappingPower of three to forty")
  expectExact(binary, ["run", uIntWrappingShiftLeftFixture], 0,
    Buffer.from("Wrapped 18446744073709551614\n", "utf8"),
    "Restaurant UInt wrappingShiftLeft with a valid count")
  expectExact(binary, ["run", fixedIntegerShiftPoliciesFixture], 0,
    fixedIntegerShiftPoliciesOutput,
    "Fixed-width named shift policies and signed/unsigned edge cases")
  expectExact(binary, ["run", uIntRotatedLeftFixture], 0,
    Buffer.from("Rotated 3\n", "utf8"),
    "Restaurant UInt rotatedLeft reduces count modulo bit width")
  expectExact(binary, ["run", uIntRotatedRightFixture], 0,
    Buffer.from("Rotated 9223372036854775809\n", "utf8"),
    "Restaurant UInt rotatedRight reduces count modulo bit width")
  expectExact(binary, ["run", uIntCountOnesFixture], 0,
    Buffer.from("Ones 32\n", "utf8"),
    "Restaurant UInt countOnes uses full-width population count")
  expectExact(binary, ["run", uIntCountZerosFixture], 0,
    Buffer.from("Zeros 32\n", "utf8"),
    "Restaurant UInt countZeros derives the full-width complement count")
  expectExact(binary, ["run", uIntLeadingZerosFixture], 0,
    Buffer.from("Leading 56/64\n", "utf8"),
    "Restaurant UInt countLeadingZeros preserves the zero boundary")
  expectExact(binary, ["run", uIntTrailingZerosFixture], 0,
    Buffer.from("Trailing 12/64\n", "utf8"),
    "Restaurant UInt countTrailingZeros preserves the zero boundary")
  expectExact(binary, ["run", uIntReversedBitsFixture], 0,
    Buffer.from("Bits 17848844570815808640\n", "utf8"),
    "Restaurant UInt reversedBits preserves the complete logical width")
  expectExact(binary, ["run", uIntReversedBytesFixture], 0,
    Buffer.from("Bytes 17279655951921914625\n", "utf8"),
    "Restaurant UInt reversedBytes is independent of host endianness")
  expectExact(binary, ["run", fixedIntegerBitPrimitivesFixture], 0,
    fixedIntegerBitPrimitivesOutput,
    "Fixed-width signed and unsigned bit-primitives family")
  expectExact(binary, ["run", uIntOverflowingAddFixture], 0,
    Buffer.from("Overflowing 0/true/11/false\n", "utf8"),
    "Restaurant UInt overflowingAdd returns wrapped value and overflow flag")
  expectExact(binary, ["run", uIntOverflowingPowerFixture], 0,
    Buffer.from(
      "Overflowing power 9223372036854775808/false; 0/true; 1/true; " +
      "1/false\n", "utf8"),
    "Restaurant UInt overflowingPower preserves sticky overflow")
  expectExact(binary, ["run", uIntOverflowingFamilyFixture], 0,
    Buffer.from(
      "Overflowing family add 0/true,11/false; subtract 41/false," +
      "18446744073709551615/true; multiply 42/false," +
      "18446744073709551614/true; negate 0/false," +
      "18446744073709551615/true; power 9223372036854775808/false," +
      "0/true,1/true,1/false\n",
      "utf8"),
    "Restaurant UInt overflowing family preserves all operation flags")
  expectExact(binary, ["run", uIntSaturatingAddFixture], 0,
    Buffer.from("Saturated 18446744073709551615/11\n", "utf8"),
    "Restaurant UInt saturatingAdd clamps overflow without trapping")
  expectExact(binary, ["run", uIntSaturatingSubtractFixture], 0,
    Buffer.from("Saturated subtract 0/10\n", "utf8"),
    "Restaurant UInt saturatingSubtract clamps underflow without trapping")
  expectExact(binary, ["run", uIntSaturatingMultiplyFixture], 0,
    Buffer.from("Saturated multiply 18446744073709551615/42\n", "utf8"),
    "Restaurant UInt saturatingMultiply clamps overflow without trapping")
  expectExact(binary, ["run", uIntSaturatingPolicyFixture], 0,
    Buffer.from(
      "Saturating policy add 18446744073709551615/11; subtract 0/10; " +
      "multiply 18446744073709551615/42; negate 0/0; power " +
      "8/18446744073709551615/1\n", "utf8"),
    "Restaurant UInt saturating policy covers the complete family")
  expectExact(binary, ["run", uIntBitNotFixture], 0,
    Buffer.from("UInt not 18446744073709551615\n", "utf8"),
    "Restaurant UInt bitwise complement")
  expectExact(binary, ["run", uIntBitwiseFixture], 0,
    Buffer.from(
      "Not 18446744073709551615\nAnd 0\nOr 18446744073709551615\n" +
      "Xor 18446744073709551615\nOnes 32\nZeros 32\nLeading 56\n" +
      "Leading zero 64\nTrailing 12\nTrailing zero 64\n", "utf8"),
    "Restaurant UInt bit-primitives family")
  expectExact(binary, ["run", uIntCompoundFixture], 0,
    Buffer.from(
      "UInt compound 4611686018427387907/4611686018427387906/" +
      "9223372036854775812/4611686018427387906/4611686018427387906/" +
      "4611686018427387906/9223372036854775812/4611686018427387906/" +
      "2/87/95\n", "utf8"),
    "Restaurant UInt compound assignment")
  expectExact(binary, ["run", mutationFixture], 0,
    Buffer.from("Open 6\n", "utf8"),
    "Restaurant straight-line local mutation")
  expectExact(binary, ["run", conditionalMutationFixture], 0,
    Buffer.from("Open 6; closed 4\n", "utf8"),
    "Restaurant conditional mutation merged through SSA")
  expectExact(binary, ["run", boolMutationFixture], 0,
    Buffer.from("Open true; closed false\n", "utf8"),
    "Restaurant Boolean local mutation")
  expectExact(binary, ["run", branchMutationFixture], 0,
    Buffer.from("Open 6; closed 4\n", "utf8"),
    "Restaurant branch-local mutation merge")
  expectExact(binary, ["run", multiBranchMutationFixture], 0,
    Buffer.from("Open 18; closed -4\n", "utf8"),
    "Restaurant multi-branch mutation merge")
  expectExact(binary, ["run", helloFixture, "--", "arbitrary", "--entry", ""],
    0, Buffer.from("Hello, world!\n", "utf8"), "forwarded program arguments")
  expectExact(binary, ["run", processInputFixture], 2,
    Buffer.from("missing\n", "utf8"), "public process input without arguments")
  expectExact(binary, ["run", processInputFixture, "--", "payload"], 0,
    Buffer.from("received\n", "utf8"), "public process input with one argument")
  expectExact(binary, ["run", processInputFixture, "--", ""], 0,
    Buffer.from("received\n", "utf8"), "public process input with empty argument")
  expectExact(binary, ["run", processIntegerExactSuccessFixture], 0,
    Buffer.alloc(0), "public exact integer conversion success")
  expectExact(binary, ["run", processIntegerExactErrorFixture], 1,
    Buffer.alloc(0), "public exact integer conversion typed error")
  expectExact(binary, ["run", processFloatRoundingSuccessFixture], 0,
    Buffer.from("Rounded 2\n", "utf8"),
    "public constant float rounding success")
  expectExact(binary, ["run", processFloatRoundingRuntimeIfFixture], 0,
    Buffer.from("Rounded 2\n", "utf8"),
    "public runtime conditional float rounding without arguments")
  expectExact(binary, ["run", processFloatRoundingRuntimeIfFixture, "--", "x"],
    0, Buffer.from("Rounded 4\n", "utf8"),
    "public runtime conditional float rounding with one argument")
  expectExact(binary, ["run", processFloatRoundingErrorFixture], 1,
    Buffer.alloc(0), "public constant float rounding typed error")
  expectExact(binary, ["run", processIntegerExactRuntimeFixture], 0,
    Buffer.from("Arithmetic 0/4/1\n", "utf8"),
    "public runtime fixed-integer arithmetic success")
  expectExact(binary, ["run", processIntegerExactRuntimeFixture, "--",
    ...Array.from({ length: 126 }, () => "x")], 0,
    Buffer.from("Arithmetic 126/9/127\n", "utf8"),
    "public runtime fixed-integer arithmetic success boundary")
  expectExact(binary, ["run", processIntegerExactRuntimeFixture, "--",
    ...Array.from({ length: 127 }, () => "x")], 2, Buffer.alloc(0),
    "public runtime fixed-integer arithmetic structured fault")
  expectExact(binary, ["run", processIntegerExactRuntimeFixture, "--",
    ...Array.from({ length: 128 }, () => "x")], 1, Buffer.alloc(0),
  "public runtime exact integer conversion out of range")
  expectExact(binary, ["run", checkedIntegerHelperFaultFixture], 0,
    checkedIntegerHelperFaultOutput,
    "public checked integer helper success")
  expectExact(binary, ["run", checkedIntegerHelperFaultFixture, "--", "x"],
    2, Buffer.alloc(0),
    "public checked integer helper arithmetic fault")
  expectExact(binary, ["run", checkedIntegerHelperFaultFixture, "--",
    ...Array.from({ length: 256 }, () => "x")], 1, Buffer.alloc(0),
    "public checked integer helper conversion precedence")
  expectExact(binary, ["run", checkedScalarIfJoinFixture], 0,
    checkedScalarIfJoinZeroOutput, "public checked scalar-if join zero case")
  expectExact(binary, ["run", checkedScalarIfJoinFixture, "--", "x"], 0,
    checkedScalarIfJoinOneOutput, "public checked scalar-if join one case")
  expectExact(binary, ["run", checkedScalarIfJoinFixture, "--", "x", "x"],
    2, Buffer.alloc(0), "public checked scalar-if join arithmetic fault")
  expectExact(binary, ["run", checkedScalarIfJoinFixture, "--",
    ...Array.from({ length: 128 }, () => "x")], 1, Buffer.alloc(0),
    "public checked scalar-if join conversion precedence")
  expectExact(binary, ["run", u64MixRoundFixture, "--",
    "alpha", "beta", "gamma"], 0, u64MixRoundOutput,
  "Windows public runtime u64 mix-round with three user arguments")
  expectExact(binary, ["run", processEnumPayloadFixture], 7,
    Buffer.from("arguments-missing count=0 amount=17 over-limit=false\n", "utf8"),
    "public enum payload process input without arguments")
  expectExact(binary, ["run", processEnumPayloadFixture, "--", ""], 0,
    Buffer.from("arguments-present count=1 amount=17 over-limit=false\n", "utf8"),
    "public enum payload process input with empty argument")
  expectExact(binary, ["run", processEnumPayloadFixture, "--", "alpha", "beta"], 0,
    Buffer.from("arguments-present count=2 amount=17 over-limit=false\n", "utf8"),
    "public enum payload process input with two arguments")
  expectExact(binary, ["run", processEnumPayloadFixture, "--",
    "alpha", "beta", "gamma"], 0,
    Buffer.from("arguments-present count=3 amount=17 over-limit=true\n", "utf8"),
    "public enum payload process input with three arguments")
  expectExact(binary, ["run", processArgumentsOrderingFixture], 0,
    Buffer.from("Argument mode compact: count=0\n", "utf8"),
    "ordered process count input without arguments")
  expectExact(binary, ["run", processArgumentsOrderingFixture, "--", ""], 0,
    Buffer.from("Argument mode compact: count=1\n", "utf8"),
    "ordered process count input with empty argument")
  expectExact(binary,
    ["run", processArgumentsOrderingFixture, "--", "alpha", "beta"], 0,
    Buffer.from("Argument mode extended: count=2\n", "utf8"),
    "ordered process count input with two arguments")
  expectExact(binary, ["run", processArgumentsOrderingFixture, "--",
    "alpha", "beta", "gamma"], 0,
    Buffer.from("Argument mode extended: count=3\n", "utf8"),
    "ordered process count input with three arguments")

  const buildHello = join(fixtureDirectory, "hello-build.exe")
  const buildFlatValueAggregates = join(fixtureDirectory,
    "flat-value-aggregates-build.exe")
  const buildEnumCfgJoin = join(fixtureDirectory,
    "enum-cfg-join-build.exe")
  const flatValueAggregatesWindowsRoute = {
    usesProcessArgumentAdapter: false,
    writesStdout: true,
  }
  const buildWindowsTargetPie = join(fixtureDirectory,
    "pie-on-windows-target-build.exe")
  const buildWindowsTargetNoPie = join(fixtureDirectory,
    "pie-off-windows-target-build.exe")
  const buildLocalGraph = join(fixtureDirectory, "local-graph-build.exe")
  const buildPrivateGraph = join(fixtureDirectory, "private-graph-build.exe")
  const buildRestaurantIf = join(fixtureDirectory, "if-build.exe")
  const buildRestaurantRepeat = join(fixtureDirectory,
    "repeat-build.exe")
  const buildWhileBreakContinue = join(fixtureDirectory,
    "while-break-continue-build.exe")
  const buildNestedLabeledWhile = join(fixtureDirectory,
    "nested-labeled-while-build.exe")
  const buildNestedLoopTerminalReturns = join(fixtureDirectory,
    "nested-loop-terminal-returns-build.exe")
  const buildRestaurantMainDispatch = join(fixtureDirectory,
    "main-dispatch-build.exe")
  const buildRestaurantMainCardinality = join(fixtureDirectory,
    "main-cardinality-build.exe")
  const buildProcessInput = join(fixtureDirectory, "process-input-build.exe")
  const buildProcessIntegerExactSuccess = join(fixtureDirectory,
    "process-integer-exact-success-build.exe")
  const buildProcessIntegerExactError = join(fixtureDirectory,
    "process-integer-exact-error-build.exe")
  const buildProcessFloatRoundingSuccess = join(fixtureDirectory,
    "process-float-rounding-success-build.exe")
  const buildProcessFloatRoundingRuntimeIf = join(fixtureDirectory,
    "process-float-rounding-runtime-if-build.exe")
  const buildProcessFloatRoundingError = join(fixtureDirectory,
    "process-float-rounding-error-build.exe")
  const buildProcessIntegerExactRuntime = join(fixtureDirectory,
    "process-fixed-integer-arithmetic-build.exe")
  const buildCheckedIntegerHelperFault = join(fixtureDirectory,
    "checked-integer-helper-fault-build.exe")
  const buildCheckedScalarIfJoin = join(fixtureDirectory,
    "checked-scalar-if-join-build.exe")
  const buildU64MixRound = join(fixtureDirectory,
    "u64-mix-round-build.exe")
  const buildProcessArgumentsCount = join(fixtureDirectory,
    "process-arguments-count-build.exe")
  const buildProcessArgumentsOrdering = join(fixtureDirectory,
    "process-arguments-ordering-build.exe")
  const buildProcessArgumentsCountSelectiveImport = join(fixtureDirectory,
    "process-arguments-count-selective-import-build.exe")
  const buildProcessEnumPayload = join(fixtureDirectory,
    "process-enum-payload-build.exe")
  const buildWrongTarget = join(fixtureDirectory, "wrong-target-build.exe")
  const buildLinuxTarget = join(fixtureDirectory,
    "main-dispatch-linux")
  const buildLinuxHelloNoPie = join(fixtureDirectory, "hello-linux-no-pie")
  const buildLinuxHelloAudit = join(fixtureDirectory, "hello-linux-audit")
  const linuxHelloAuditTrace = join(fixtureDirectory, "hello-linux-audit-trace")
  const buildLinuxFlatValueAggregates = join(fixtureDirectory,
    "flat-value-aggregates-linux")
  const linuxFlatValueAggregatesAuditTrace = join(fixtureDirectory,
    "flat-value-aggregates-linux-audit")
  const failedLinuxAuditProduct = join(fixtureDirectory,
    "failed-linux-audit-product")
  const failedLinuxAuditTrace = join(fixtureDirectory,
    "failed-linux-audit-trace")
  const existingLinuxAuditTrace = join(fixtureDirectory,
    "existing-linux-audit-trace")
  const existingLinuxAuditMarker = join(existingLinuxAuditTrace, "keep")
  const reparseLinuxAuditTarget = join(fixtureDirectory,
    "reparse-linux-audit-target")
  const reparseLinuxAuditPath = join(fixtureDirectory,
    "reparse-linux-audit")
  const reparseLinuxAuditParent = join(fixtureDirectory,
    "reparse-linux-audit-parent")
  const escapeLinuxAuditPath =
    `${fixtureDirectory}\\..\\w-run-windows-audit-escape`
  const buildMissingParent = join(fixtureDirectory, "missing", "artifact.exe")
  expectExact(binary, ["build", helloFixture, "--target", targetTriple,
    "--output", buildHello], 0, Buffer.alloc(0), "build Hello fixture")
  const builtHelloStats = await lstat(buildHello)
  assert(builtHelloStats.isFile() && !builtHelloStats.isSymbolicLink(),
    "build Hello did not produce a regular artifact")
  expectExact(buildHello, [], 0, Buffer.from("Hello, world!\n", "utf8"),
    "execute built Hello artifact")
  expectExact(binary, ["build", flatValueAggregatesFixture, "--target",
    targetTriple, "--output", buildFlatValueAggregates], 0,
  Buffer.alloc(0),
  "build flat tuple and immutable value-struct family in Windows Release")
  const flatValueAggregatesWindowsBytes = await readFile(
    buildFlatValueAggregates)
  assertPeX64(flatValueAggregatesWindowsBytes,
    "built flat tuple and immutable value-struct Windows artifact")
  assertKernel32OnlyImports(flatValueAggregatesWindowsBytes,
    "built flat tuple and immutable value-struct Windows artifact",
    flatValueAggregatesWindowsRoute)
  expectExact(buildFlatValueAggregates, [], 0, flatValueAggregatesOutput,
    "execute Windows Release flat tuple and immutable value-struct product")
  expectExact(binary, ["build", enumCfgJoinFixture, "--target", targetTriple,
    "--output", buildEnumCfgJoin], 0, Buffer.alloc(0),
  "build bounded enum CFG join in Windows Release")
  const enumCfgJoinWindowsBytes = await readFile(buildEnumCfgJoin)
  assertPeX64(enumCfgJoinWindowsBytes,
    "built bounded enum CFG join Windows artifact")
  assertKernel32OnlyImports(enumCfgJoinWindowsBytes,
    "built bounded enum CFG join Windows artifact",
    flatValueAggregatesWindowsRoute)
  expectExact(buildEnumCfgJoin, [], 0, enumCfgJoinOutput,
    "execute Windows Release bounded enum CFG join product")
  expectExact(binary, ["build", localGraphFixture, "--target", targetTriple,
    "--output", buildLocalGraph], 0, Buffer.alloc(0),
    "build resolved local-module graph")
  expectExact(buildLocalGraph, [], 0, Buffer.from("answer 42\n", "utf8"),
    "execute built local-module graph artifact")
  expectBuildFailure(binary, ["build", privateGraphRoot, "--target",
    targetTriple, "--output", buildPrivateGraph],
  "reject private cross-module build")
  assert(!existsSync(buildPrivateGraph),
    "private cross-module build left an artifact")
  const helloBytes = await readFile(buildHello)
  const helloSections = peSectionNames(helloBytes, "built Hello artifact")
  assert(helloSections.includes(".rdata") && !helloSections.includes(".pdata"),
    "Release Hello must fold unwind metadata into the read-only section")
  expectBuildFailure(binary, ["build", helloFixture, "--target", targetTriple,
    "--output", buildHello], "reject existing build output")
  assert((await readFile(buildHello)).equals(helloBytes),
    "existing build output was modified")
  expectExact(binary, ["build", ifFixture, "--target", targetTriple,
    "--output", buildRestaurantIf], 0, Buffer.alloc(0),
    "build if fixture")
  expectExact(buildRestaurantIf, [], 0, expectedIf,
    "execute built if artifact")
  expectExact(binary, ["build", repeatFixture, "--target", targetTriple,
    "--output", buildRestaurantRepeat], 0, Buffer.alloc(0),
    "build repeat fixture")
  const builtRestaurantRepeatStats = await lstat(buildRestaurantRepeat)
  assert(builtRestaurantRepeatStats.isFile() &&
    !builtRestaurantRepeatStats.isSymbolicLink(),
    "build repeat did not produce a regular artifact")
  assertPeX64(await readFile(buildRestaurantRepeat),
    "built repeat artifact")
  expectExact(buildRestaurantRepeat, [], 0,
    Buffer.from("Receipt digits 1/5\n", "utf8"),
    "execute built repeat artifact")
  expectExact(binary, ["build", whileBreakContinueFixture, "--target",
    targetTriple, "--output", buildWhileBreakContinue], 0,
    Buffer.alloc(0), "build verified loop CFG fixture")
  assertKernel32OnlyImports(await readFile(buildWhileBreakContinue),
    "built verified loop CFG artifact",
    { usesProcessArgumentAdapter: false, writesStdout: true })
  expectExact(buildWhileBreakContinue, [], 0,
    Buffer.from("0,4,8\n", "utf8"),
    "execute built verified loop CFG artifact")
  expectExact(binary, ["build", nestedLabeledWhileFixture, "--target",
    targetTriple, "--output", buildNestedLabeledWhile], 0,
    Buffer.alloc(0), "build verified nested labeled loop CFG fixture")
  assertKernel32OnlyImports(await readFile(buildNestedLabeledWhile),
    "built verified nested labeled loop CFG artifact",
    { usesProcessArgumentAdapter: false, writesStdout: true })
  expectExact(buildNestedLabeledWhile, [], 0,
    Buffer.from("0,1,3\n", "utf8"),
    "execute built verified nested labeled loop CFG artifact")
  expectExact(binary, ["build", nestedLoopTerminalReturnsFixture, "--target",
    targetTriple, "--output", buildNestedLoopTerminalReturns], 0,
    Buffer.alloc(0), "build verified nested loop terminal-return CFG fixture")
  assertKernel32OnlyImports(await readFile(buildNestedLoopTerminalReturns),
    "built verified nested loop terminal-return CFG artifact",
    { usesProcessArgumentAdapter: false, writesStdout: true })
  expectExact(buildNestedLoopTerminalReturns, [], 0,
    Buffer.from("-1,1,3\n", "utf8"),
    "execute built verified nested loop terminal-return CFG artifact")
  expectExact(binary, ["build", mainDispatchFixture, "--target",
    targetTriple, "--output", buildRestaurantMainDispatch], 0,
  Buffer.alloc(0), "build restaurant main-domain dispatch fixture")
  assertPeX64(await readFile(buildRestaurantMainDispatch),
    "built restaurant main-domain dispatch artifact")
  expectExact(buildRestaurantMainDispatch, [], 0,
    Buffer.from("Dispatched 88\n", "utf8"),
    "execute built restaurant main-domain dispatch artifact")
  expectExact(binary, ["build", mainCardinalityFixture, "--target",
    targetTriple, "--output", buildRestaurantMainCardinality], 0,
  Buffer.alloc(0), "build restaurant main-domain cardinality fixture")
  assertPeX64(await readFile(buildRestaurantMainCardinality),
    "built restaurant main-domain cardinality artifact")
  expectExact(buildRestaurantMainCardinality, [], 0,
    Buffer.from("Dispatched 92\n", "utf8"),
    "execute built restaurant main-domain cardinality artifact")
  expectExact(binary, ["build", processInputFixture, "--target", targetTriple,
    "--output", buildProcessInput], 0, Buffer.alloc(0),
    "build public process-input fixture")
  const builtProcessInputStats = await lstat(buildProcessInput)
  assert(builtProcessInputStats.isFile() &&
    !builtProcessInputStats.isSymbolicLink(),
    "build process-input did not produce a regular artifact")
  assertPeX64(await readFile(buildProcessInput), "built process-input artifact")
  expectExact(buildProcessInput, [], 2, Buffer.from("missing\n", "utf8"),
    "execute built process-input artifact without arguments")
  expectExact(buildProcessInput, ["payload"], 0,
    Buffer.from("received\n", "utf8"),
    "execute built process-input artifact with one argument")
  expectExact(buildProcessInput,
    Array.from({ length: 257 }, () => "x"), 3, Buffer.alloc(0),
    "reject process-input descriptor overflow without partial output")
  expectExact(binary, ["build", processIntegerExactSuccessFixture, "--target",
    targetTriple, "--output", buildProcessIntegerExactSuccess], 0,
  Buffer.alloc(0), "build exact integer conversion success fixture")
  assertPeX64(await readFile(buildProcessIntegerExactSuccess),
    "built exact integer conversion success artifact")
  expectExact(buildProcessIntegerExactSuccess, [], 0, Buffer.alloc(0),
    "execute built exact integer conversion success artifact")
  expectExact(binary, ["build", processIntegerExactErrorFixture, "--target",
    targetTriple, "--output", buildProcessIntegerExactError], 0,
  Buffer.alloc(0), "build exact integer conversion typed-error fixture")
  assertPeX64(await readFile(buildProcessIntegerExactError),
    "built exact integer conversion typed-error artifact")
  expectExact(buildProcessIntegerExactError, [], 1, Buffer.alloc(0),
    "execute built exact integer conversion typed-error artifact")
  expectExact(binary, ["build", processFloatRoundingSuccessFixture, "--target",
    targetTriple, "--output", buildProcessFloatRoundingSuccess], 0,
  Buffer.alloc(0), "build constant float rounding success fixture")
  assertPeX64(await readFile(buildProcessFloatRoundingSuccess),
    "built constant float rounding success artifact")
  assertKernel32OnlyImports(await readFile(buildProcessFloatRoundingSuccess),
    "built constant float rounding success artifact",
    { usesProcessArgumentAdapter: true, writesStdout: true })
  expectExact(buildProcessFloatRoundingSuccess, [], 0,
    Buffer.from("Rounded 2\n", "utf8"),
    "execute built constant float rounding success artifact")
  expectExact(binary, ["build", processFloatRoundingRuntimeIfFixture,
    "--target", targetTriple, "--output", buildProcessFloatRoundingRuntimeIf], 0,
  Buffer.alloc(0), "build runtime conditional float rounding fixture")
  const processFloatRoundingRuntimeIfBytes =
    await readFile(buildProcessFloatRoundingRuntimeIf)
  assertPeX64(processFloatRoundingRuntimeIfBytes,
    "built runtime conditional float rounding artifact")
  assertKernel32OnlyImports(processFloatRoundingRuntimeIfBytes,
    "built runtime conditional float rounding artifact",
    { usesProcessArgumentAdapter: true, writesStdout: true })
  expectExact(buildProcessFloatRoundingRuntimeIf, [], 0,
    Buffer.from("Rounded 2\n", "utf8"),
    "execute built runtime conditional float rounding without arguments")
  expectExact(buildProcessFloatRoundingRuntimeIf, ["x"], 0,
    Buffer.from("Rounded 4\n", "utf8"),
    "execute built runtime conditional float rounding with one argument")
  expectExact(binary, ["build", processFloatRoundingErrorFixture, "--target",
    targetTriple, "--output", buildProcessFloatRoundingError], 0,
  Buffer.alloc(0), "build constant float rounding typed-error fixture")
  assertPeX64(await readFile(buildProcessFloatRoundingError),
    "built constant float rounding typed-error artifact")
  assertKernel32OnlyImports(await readFile(buildProcessFloatRoundingError),
    "built constant float rounding typed-error artifact",
    { usesProcessArgumentAdapter: true, writesStdout: false })
  expectExact(buildProcessFloatRoundingError, [], 1, Buffer.alloc(0),
    "execute built constant float rounding typed-error artifact")
  expectExact(binary, ["build", processIntegerExactRuntimeFixture, "--target",
    targetTriple, "--output", buildProcessIntegerExactRuntime], 0,
  Buffer.alloc(0), "build runtime fixed-integer arithmetic fixture")
  assertPeX64(await readFile(buildProcessIntegerExactRuntime),
    "built runtime fixed-integer arithmetic artifact")
  expectExact(buildProcessIntegerExactRuntime, [], 0,
    Buffer.from("Arithmetic 0/4/1\n", "utf8"),
    "execute built runtime fixed-integer arithmetic success")
  expectExact(buildProcessIntegerExactRuntime,
    Array.from({ length: 126 }, () => "x"), 0,
    Buffer.from("Arithmetic 126/9/127\n", "utf8"),
    "execute built runtime fixed-integer arithmetic success boundary")
  expectExact(buildProcessIntegerExactRuntime,
    Array.from({ length: 127 }, () => "x"), 2, Buffer.alloc(0),
    "execute built runtime fixed-integer arithmetic structured fault")
  expectExact(buildProcessIntegerExactRuntime,
    Array.from({ length: 128 }, () => "x"), 1, Buffer.alloc(0),
  "execute built runtime exact integer conversion out of range")
  expectExact(binary, ["build", checkedIntegerHelperFaultFixture, "--target",
    targetTriple, "--output", buildCheckedIntegerHelperFault], 0,
  Buffer.alloc(0), "build checked integer helper fault fixture")
  assertPeX64(await readFile(buildCheckedIntegerHelperFault),
    "built checked integer helper fault artifact")
  assertKernel32OnlyImports(await readFile(buildCheckedIntegerHelperFault),
    "built checked integer helper fault artifact",
    { usesProcessArgumentAdapter: true, writesStdout: true })
  expectExact(buildCheckedIntegerHelperFault, [], 0,
    checkedIntegerHelperFaultOutput,
    "execute built checked integer helper success")
  expectExact(buildCheckedIntegerHelperFault, ["x"], 2, Buffer.alloc(0),
    "execute built checked integer helper arithmetic fault")
  expectExact(buildCheckedIntegerHelperFault,
    Array.from({ length: 256 }, () => "x"), 1, Buffer.alloc(0),
    "execute built checked integer helper conversion precedence")
  expectExact(binary, ["build", checkedScalarIfJoinFixture, "--target",
    targetTriple, "--output", buildCheckedScalarIfJoin], 0,
    Buffer.alloc(0), "build checked scalar-if join fixture")
  const checkedScalarIfJoinBytes = await readFile(buildCheckedScalarIfJoin)
  assertPeX64(checkedScalarIfJoinBytes,
    "built checked scalar-if join artifact")
  assertKernel32OnlyImports(checkedScalarIfJoinBytes,
    "built checked scalar-if join artifact",
    { usesProcessArgumentAdapter: true, writesStdout: true })
  expectExact(buildCheckedScalarIfJoin, [], 0, checkedScalarIfJoinZeroOutput,
    "execute built checked scalar-if join zero case")
  expectExact(buildCheckedScalarIfJoin, ["x"], 0,
    checkedScalarIfJoinOneOutput,
    "execute built checked scalar-if join one case")
  expectExact(buildCheckedScalarIfJoin, ["x", "x"], 2, Buffer.alloc(0),
    "execute built checked scalar-if join arithmetic fault")
  expectExact(buildCheckedScalarIfJoin,
    Array.from({ length: 128 }, () => "x"), 1, Buffer.alloc(0),
    "execute built checked scalar-if join conversion precedence")
  expectExact(binary, ["build", u64MixRoundFixture, "--target",
    targetTriple, "--output", buildU64MixRound], 0, Buffer.alloc(0),
  "build Windows runtime u64 mix-round fixture")
  const u64MixRoundBytes = await readFile(buildU64MixRound)
  assertPeX64(u64MixRoundBytes, "built runtime u64 mix-round artifact")
  assertKernel32OnlyImports(u64MixRoundBytes,
    "built runtime u64 mix-round artifact",
    { usesProcessArgumentAdapter: true, writesStdout: true })
  expectExact(buildU64MixRound, ["alpha", "beta", "gamma"], 0,
    u64MixRoundOutput,
    "execute built Windows runtime u64 mix-round with three user arguments")
  expectExact(binary, ["build", processArgumentsCountFixture, "--target",
    targetTriple, "--output", buildProcessArgumentsCount], 0,
    Buffer.alloc(0), "build public process-arguments-count fixture")
  const builtProcessArgumentsCountStats = await lstat(buildProcessArgumentsCount)
  assert(builtProcessArgumentsCountStats.isFile() &&
    !builtProcessArgumentsCountStats.isSymbolicLink(),
    "build process-arguments-count did not produce a regular artifact")
  const processArgumentsCountBytes = await readFile(buildProcessArgumentsCount)
  assertPeX64(processArgumentsCountBytes,
    "built process-arguments-count artifact")
  assertKernel32OnlyImports(processArgumentsCountBytes,
    "built process-arguments-count artifact",
    { usesProcessArgumentAdapter: true, writesStdout: true })
  expectExact(binary, ["build", processArgumentsCountSelectiveImportFixture,
    "--target", targetTriple, "--output", buildProcessArgumentsCountSelectiveImport],
    0, Buffer.alloc(0),
    "build selective-import process-arguments-count fixture")
  const flatImportBytes = processArgumentsCountBytes
  const selectiveImportBytes = await readFile(buildProcessArgumentsCountSelectiveImport)
  assertPeX64(selectiveImportBytes,
    "built selective-import process-arguments-count artifact")
  assertKernel32OnlyImports(selectiveImportBytes,
    "built selective-import process-arguments-count artifact",
    { usesProcessArgumentAdapter: true, writesStdout: true })
  const firstImportDifference = selectiveImportBytes.findIndex(
    (value, index) => flatImportBytes[index] !== value)
  assert(selectiveImportBytes.equals(flatImportBytes),
    "flat and selective std.process imports produced different PE bytes" +
    ` (sizes ${flatImportBytes.length}/${selectiveImportBytes.length}, ` +
    `first difference ${firstImportDifference})`)
  const argumentCountCases = [
    ["without user arguments", []],
    ["with one empty argument", [""]],
    ["with two ordinary arguments", ["alpha", "beta"]],
    ["with exactly 256 user arguments", Array.from({ length: 256 }, () => "x")],
  ]
  for (const [label, argumentsList] of argumentCountCases) {
    assert(argumentsList.length <= 256,
      `process-arguments-count case exceeds the 256-argument bound: ${label}`)
    const expectedOutput = argumentsList.length === 2
      ? "Exactly two arguments\n"
      : `Argument count ${argumentsList.length}\n`
    expectExact(buildProcessArgumentsCount, argumentsList, 0,
      Buffer.from(expectedOutput, "utf8"),
      `execute built process-arguments-count artifact ${label}`)
    expectExact(buildProcessArgumentsCountSelectiveImport, argumentsList, 0,
      Buffer.from(expectedOutput, "utf8"),
      `execute selective-import process-arguments-count artifact ${label}`)
  }
  expectExact(binary, ["build", processArgumentsOrderingFixture, "--target",
    targetTriple, "--output", buildProcessArgumentsOrdering], 0,
    Buffer.alloc(0), "build ordered process-arguments fixture")
  const builtProcessArgumentsOrderingStats = await lstat(
    buildProcessArgumentsOrdering)
  assert(builtProcessArgumentsOrderingStats.isFile() &&
    !builtProcessArgumentsOrderingStats.isSymbolicLink(),
    "build ordered process-arguments did not produce a regular artifact")
  const processArgumentsOrderingBytes = await readFile(buildProcessArgumentsOrdering)
  assertPeX64(processArgumentsOrderingBytes,
    "built ordered process-arguments artifact")
  assertKernel32OnlyImports(processArgumentsOrderingBytes,
    "built ordered process-arguments artifact",
    { usesProcessArgumentAdapter: true, writesStdout: true })
  const orderedArgumentCountCases = [
    ["without user arguments", [], "Argument mode compact: count=0\n"],
    ["with one empty argument", [""], "Argument mode compact: count=1\n"],
    ["with two ordinary arguments", ["alpha", "beta"],
      "Argument mode extended: count=2\n"],
    ["with three ordinary arguments", ["alpha", "beta", "gamma"],
      "Argument mode extended: count=3\n"],
  ]
  for (const [label, argumentsList, expectedOutput] of orderedArgumentCountCases)
    expectExact(buildProcessArgumentsOrdering, argumentsList, 0,
      Buffer.from(expectedOutput, "utf8"),
      `execute built ordered process-arguments artifact ${label}`)
  expectExact(binary, ["build", processEnumPayloadFixture, "--target", targetTriple,
    "--output", buildProcessEnumPayload], 0, Buffer.alloc(0),
    "build public enum payload process fixture")
  const builtProcessEnumPayloadStats = await lstat(buildProcessEnumPayload)
  assert(builtProcessEnumPayloadStats.isFile() &&
    !builtProcessEnumPayloadStats.isSymbolicLink(),
    "build enum payload process fixture did not produce a regular artifact")
  assertPeX64(await readFile(buildProcessEnumPayload),
    "built enum payload process artifact")
  expectExact(buildProcessEnumPayload, [], 7,
    Buffer.from("arguments-missing count=0 amount=17 over-limit=false\n", "utf8"),
    "execute built enum payload process artifact without arguments")
  expectExact(buildProcessEnumPayload, [""], 0,
    Buffer.from("arguments-present count=1 amount=17 over-limit=false\n", "utf8"),
    "execute built enum payload process artifact with empty argument")
  expectExact(buildProcessEnumPayload, ["alpha", "beta"], 0,
    Buffer.from("arguments-present count=2 amount=17 over-limit=false\n", "utf8"),
    "execute built enum payload process artifact with two arguments")
  expectExact(buildProcessEnumPayload, ["alpha", "beta", "gamma"], 0,
    Buffer.from("arguments-present count=3 amount=17 over-limit=true\n", "utf8"),
    "execute built enum payload process artifact with three arguments")
  expectExact(binary, ["build", mainCardinalityFixture, "--target",
    linuxTargetTriple, "--output", buildLinuxTarget], 0, Buffer.alloc(0),
  "cross-build restaurant main-domain cardinality for Linux")
  const linuxBytes = await readFile(buildLinuxTarget)
  const linuxLayout = validateElfX64(linuxBytes, "w")
  assert(linuxLayout.elfLayout?.class === "ELF64" &&
    linuxLayout.elfLayout?.machine === "x86-64" &&
    linuxLayout.elfLayout?.type === "pie",
  "cross-built main-domain artifact is not a clean Linux x86-64 PIE")
  const wsl = Bun.which("wsl.exe")
  assert(wsl, "WSL2 is unavailable for cross-built Linux target execution")
  await checkProcessArgumentCountParity(binary, buildProcessArgumentsCount,
    { includeLinuxChecks: false })
  expectExact(wsl, ["-d", "Ubuntu", "--", wslPath(buildLinuxTarget)], 0,
    Buffer.from("Dispatched 92\n", "utf8"),
  "execute cross-built main-domain cardinality Linux artifact through WSL2")
  expectExact(binary, ["build", helloFixture, "--target", linuxTargetTriple,
    "--output", buildLinuxHelloNoPie, "--pie", "off"], 0, Buffer.alloc(0),
  "cross-build Hello as a non-PIE Linux executable")
  const linuxNoPieBytes = await readFile(buildLinuxHelloNoPie)
  assertCrtFreeExecElf(linuxNoPieBytes)
  assertElfNoExecutableStack(linuxNoPieBytes)
  expectExact(wsl, ["-d", "Ubuntu", "--", wslPath(buildLinuxHelloNoPie)], 0,
    Buffer.from("Hello, world!\n", "utf8"),
  "execute cross-built non-PIE Hello through WSL2")
  expectExact(binary, ["build", helloFixture, "--target", linuxTargetTriple,
    "--output", buildLinuxHelloAudit, "--audit-dir", linuxHelloAuditTrace],
  0, Buffer.alloc(0), "cross-build one Linux product with audit trace")
  const linuxAuditBytes = await readFile(buildLinuxHelloAudit)
  assertCrtFreeElf(linuxAuditBytes)
  assertElfNoExecutableStack(linuxAuditBytes)
  await verifyLinuxAuditTrace(linuxHelloAuditTrace, buildLinuxHelloAudit)
  expectExact(wsl, ["-d", "Ubuntu", "--", wslPath(buildLinuxHelloAudit)], 0,
    Buffer.from("Hello, world!\n", "utf8"),
  "execute cross-built audited Linux Hello through WSL2")
  expectExact(binary, ["build", flatValueAggregatesFixture, "--target",
    linuxTargetTriple, "--output", buildLinuxFlatValueAggregates,
    "--audit-dir", linuxFlatValueAggregatesAuditTrace], 0,
  Buffer.alloc(0),
  "cross-build flat tuple and immutable value-struct family with audit")
  const linuxFlatValueAggregatesBytes = await readFile(
    buildLinuxFlatValueAggregates)
  assertCrtFreeElf(linuxFlatValueAggregatesBytes)
  assertElfNoExecutableStack(linuxFlatValueAggregatesBytes)
  await verifyLinuxAuditTrace(linuxFlatValueAggregatesAuditTrace,
    buildLinuxFlatValueAggregates)
  expectExact(wsl, ["-d", "Ubuntu", "--",
    wslPath(buildLinuxFlatValueAggregates)], 0,
  flatValueAggregatesOutput,
  "execute cross-built Release flat tuple and immutable value-struct product through WSL2")

  await mkdir(existingLinuxAuditTrace)
  await writeFile(existingLinuxAuditMarker, "preserve existing audit data\n")
  try {
    expectBuildFailure(binary, ["build", helloFixture, "--target",
      linuxTargetTriple, "--output", failedLinuxAuditProduct, "--audit-dir",
      existingLinuxAuditTrace], "reject existing audit directory")
    assert(!existsSync(failedLinuxAuditProduct) &&
      (await readFile(existingLinuxAuditMarker)).equals(
        Buffer.from("preserve existing audit data\n")),
    "existing Linux audit directory was modified or product published")
  } finally {
    await rm(existingLinuxAuditTrace, { recursive: true, force: true })
  }

  await mkdir(reparseLinuxAuditTarget)
  await symlink(reparseLinuxAuditTarget, reparseLinuxAuditPath, "junction")
  await symlink(fixtureDirectory, reparseLinuxAuditParent, "junction")
  try {
    expectBuildFailure(binary, ["build", helloFixture, "--target",
      linuxTargetTriple, "--output", failedLinuxAuditProduct, "--audit-dir",
      reparseLinuxAuditPath], "reject reparse audit target")
    expectBuildFailure(binary, ["build", helloFixture, "--target",
      linuxTargetTriple, "--output", failedLinuxAuditProduct, "--audit-dir",
      join(reparseLinuxAuditParent, "trace")],
    "reject reparse audit parent")
    expectBuildFailure(binary, ["build", helloFixture, "--target",
      linuxTargetTriple, "--output", failedLinuxAuditProduct, "--audit-dir",
      escapeLinuxAuditPath], "reject escaping audit path")
    assert(!existsSync(failedLinuxAuditProduct) &&
      !existsSync(escapeLinuxAuditPath),
    "invalid audit path published a product or escaped trace")
  } finally {
    await unlink(reparseLinuxAuditPath)
    await unlink(reparseLinuxAuditParent)
    await rm(reparseLinuxAuditTarget, { recursive: true, force: true })
  }
  expectBuildFailure(binary, ["build", privateGraphRoot, "--target",
    linuxTargetTriple, "--output", failedLinuxAuditProduct, "--audit-dir",
    failedLinuxAuditTrace], "remove failed cross-build audit staging")
  assert(!existsSync(failedLinuxAuditProduct) &&
    !existsSync(failedLinuxAuditTrace),
  "failed cross-build published a partial audit trace")
  await assertNoBuildResidue(fixtureDirectory,
    "Windows-host Linux audit success and failures")

  expectBuildFailure(binary, ["build", helloFixture, "--target",
    "aarch64-unknown-linux-gnu", "--output", buildWrongTarget],
    "reject unsupported build target")
  assert(!existsSync(buildWrongTarget), "wrong-target build left an artifact")
  expectBuildFailure(binary, ["build", helloFixture, "--target", targetTriple,
    "--output", buildMissingParent], "reject missing build output parent")
  assert(!existsSync(buildMissingParent), "missing-parent build left an artifact")
  const reparseTarget = join(fixtureDirectory, "reparse-target")
  const reparseOutput = join(fixtureDirectory, "reparse-output.exe")
  await mkdir(reparseTarget)
  await symlink(reparseTarget, reparseOutput, "junction")
  try {
    expectBuildFailure(binary, ["build", helloFixture, "--target", targetTriple,
      "--output", reparseOutput], "reject reparse build output")
    const reparseStats = await lstat(reparseOutput)
    assert(reparseStats.isSymbolicLink(),
      "rejected reparse build output was modified")
  } finally {
    await unlink(reparseOutput)
    await rm(reparseTarget, { recursive: true, force: true })
  }
  await assertNoBuildResidue(fixtureDirectory)

  const failingTool = process.env.ComSpec
  assert(failingTool && existsSync(failingTool),
    "ComSpec is unavailable for native tool-stage failure checks")
  for (const role of ["mlirOpt", "mlirTranslate", "llvmOpt", "llc", "linkDriver"]) {
    configureNative(`native ${role} failure`, { [role]: failingTool })
    const failureOutput = join(fixtureDirectory, `${role}-failure.exe`)
    expectBuildFailure(binary, ["build", helloFixture, "--target", targetTriple,
      "--output", failureOutput], `${role} build stage failure`)
    assert(!existsSync(failureOutput), `${role} build left an artifact`)
    await assertNoBuildResidue(fixtureDirectory, `${role} build stage failure`)
    configureNative(`native ${role} restore`)
  }
  await assertNoBuildResidue(fixtureDirectory)
  expectSourceFailure(binary, invalidSource, "invalid UTF-8 source")
  expectSourceFailure(binary, unsupportedSource, "unsupported source")
  for (const [label, path] of invalidComparisons)
    expectSourceFailure(binary, path, `comparison rejects ${label}`)
  const unsupportedOption = spawn(binary, ["run", "--entry", helloFixture])
  assert(unsupportedOption.exitCode === 2 && unsupportedOption.stdout.length === 0 &&
    unsupportedOption.stderr.toString() === expectedWindowsErrorHelp,
  "unsupported run option was not rejected with exact usage")
  for (const [mode, output] of [["on", buildWindowsTargetPie],
    ["off", buildWindowsTargetNoPie]]) {
    const explicitWindowsTargetPie = spawn(binary, ["build", helloFixture,
      "--target", targetTriple, "--output", output, "--pie", mode])
    assert(explicitWindowsTargetPie.exitCode === 2 &&
      explicitWindowsTargetPie.stdout.length === 0 &&
      explicitWindowsTargetPie.stderr.toString() === expectedWindowsErrorHelp &&
      !existsSync(output),
    `explicit PIE mode ${mode} was not rejected on Windows without staging`)
  }

  const smoke = runRequired("native PE size smoke", process.execPath,
    [smokePath, "--toolchain", defaultCacheDirectory(), "--sdk", sdk.root])
  const smokeText = `${smoke.stdout}\n${smoke.stderr}`
  const exeMatch = smokeText.match(/exeBytes=(\d+)/u)
  assert(exeMatch !== null, "native PE size was not reported by the smoke")
  assert(!smokeText.includes("clang"), "native smoke unexpectedly used Clang")
  const diskAfterRuns = await diskFree(buildDirectory)
  const residueAfter = await snapshotResidue()
  assertNoNewResidue(residueBefore, residueAfter)
  console.log(`W RUN Windows: native E2E passed toolchain=${defaultCacheDirectory()} sdk=${sdk.version} vs=${visualStudio.installationPath} wExeBytes=${binaryStats.size} peBytes=${exeMatch[1]} processPeBytes=${builtProcessInputStats.size} processArgumentsCountPeBytes=${builtProcessArgumentsCountStats.size} diskFreeBefore=${diskBefore} diskFreeAfter=${diskAfterRuns}`)
} finally {
  await rm(unsupportedBuildDirectory, { recursive: true, force: true })
  await rm(buildDirectory, { recursive: true, force: true })
  await rm(fixtureDirectory, { recursive: true, force: true })
}
