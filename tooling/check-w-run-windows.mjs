import { existsSync } from "node:fs"
import { lstat, mkdir, mkdtemp, readdir, readFile, rm, statfs, symlink, unlink, writeFile } from "node:fs/promises"
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

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const manifestPath = resolve(import.meta.dir, "mlir0-windows-toolchain.json")
const materializedPath = join(defaultCacheDirectory(), MATERIALIZED_MANIFEST)
const smokePath = resolve(import.meta.dir, "smoke-mlir0-windows.mjs")
const helloFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const restaurantIfFixture = resolve(seedDirectory, "fixtures", "restaurant-if.w")
const restaurantEnumFixture = resolve(seedDirectory, "fixtures", "restaurant-enum.w")
const restaurantEnumSubsetFixture = resolve(seedDirectory, "fixtures", "restaurant-enum-subset.w")
const restaurantEnumPayloadFixture = resolve(seedDirectory, "fixtures", "restaurant-enum-payload.w")
const restaurantEnumBoolPayloadFixture = resolve(seedDirectory, "fixtures", "restaurant-enum-bool-payload.w")
const restaurantWhileFixture = resolve(seedDirectory, "fixtures", "restaurant-while.w")
const restaurantWhileMultiFixture = resolve(seedDirectory,
  "fixtures", "restaurant-while-multi.w")
const restaurantWhilePostFixture = resolve(seedDirectory,
  "fixtures", "restaurant-while-post.w")
const restaurantRepeatFixture = resolve(seedDirectory,
  "fixtures", "restaurant-repeat.w")
const restaurantWmoFixture = resolve(seedDirectory, "fixtures", "restaurant-wmo.w")
const restaurantAsyncJoinFixture = resolve(seedDirectory,
  "fixtures", "restaurant-async-join.w")
const restaurantAsyncYieldFixture = resolve(seedDirectory,
  "fixtures", "restaurant-async-yield.w")
const restaurantMainDispatchFixture = resolve(seedDirectory,
  "fixtures", "restaurant-main-dispatch0.w")
const restaurantMainCardinalityFixture = resolve(seedDirectory,
  "fixtures", "restaurant-main-cardinality0.w")
const restaurantComparisonsFixture = resolve(seedDirectory, "fixtures", "restaurant-comparisons.w")
const restaurantComparisonCompositionFixture = resolve(seedDirectory, "fixtures", "restaurant-comparison-composition.w")
const restaurantBoolShortCircuitFixture = resolve(seedDirectory, "fixtures", "restaurant-bool-short-circuit.w")
const restaurantScalarIfFixture = resolve(seedDirectory, "fixtures", "restaurant-scalar-if.w")
const restaurantInterpolationFixture = resolve(
  seedDirectory, "fixtures", "restaurant-interpolation.w")
const restaurantLinearFixture = resolve(seedDirectory, "fixtures", "restaurant-linear.w")
const restaurantRuntimeDivremFixture = resolve(seedDirectory,
  "fixtures", "restaurant-runtime-divrem.w")
const restaurantUnaryNegateFixture = resolve(seedDirectory,
  "fixtures", "restaurant-unary-negate.w")
const restaurantUnaryInterpolationFixture = resolve(seedDirectory,
  "fixtures", "restaurant-unary-interpolation.w")
const restaurantBitwiseFixture = resolve(seedDirectory,
  "fixtures", "restaurant-bitwise.w")
const restaurantUnsignedFixture = resolve(seedDirectory,
  "fixtures", "restaurant-unsigned.w")
const restaurantShiftsFixture = resolve(seedDirectory,
  "fixtures", "restaurant-shifts.w")
const restaurantPowerFixture = resolve(seedDirectory,
  "fixtures", "restaurant-power.w")
const restaurantPowerPrefixFixture = resolve(seedDirectory,
  "fixtures", "restaurant-power-prefix.w")
const restaurantCompoundFixture = resolve(seedDirectory,
  "fixtures", "restaurant-compound.w")
const restaurantF64StrictFixture = resolve(seedDirectory,
  "fixtures", "restaurant-f64-strict.w")
const restaurantUIntArithmeticFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-arithmetic.w")
const restaurantUIntWrappingAddFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-add.w")
const restaurantUIntWrappingSubtractFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-subtract.w")
const restaurantUIntWrappingMultiplyFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-multiply.w")
const restaurantUIntWrappingNegateFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-negate.w")
const restaurantUIntWrappingPowerFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-power.w")
const restaurantUIntWrappingShiftLeftFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-shift-left.w")
const restaurantUIntMaskedShiftLeftFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-masked-shift-left.w")
const restaurantUIntMaskedShiftRightFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-masked-shift-right.w")
const restaurantUIntLogicalShiftRightFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-logical-shift-right.w")
const restaurantUIntRotatedLeftFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-rotated-left.w")
const restaurantUIntRotatedRightFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-rotated-right.w")
const restaurantUIntCountOnesFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-count-ones.w")
const restaurantUIntCountZerosFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-count-zeros.w")
const restaurantUIntLeadingZerosFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-leading-zeros.w")
const restaurantUIntTrailingZerosFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-trailing-zeros.w")
const restaurantUIntReversedBitsFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-reversed-bits.w")
const restaurantUIntReversedBytesFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-reversed-bytes.w")
const restaurantUIntOverflowingAddFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-overflowing-add.w")
const restaurantUIntSaturatingAddFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-saturating-add.w")
const restaurantUIntSaturatingSubtractFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-saturating-subtract.w")
const restaurantUIntSaturatingMultiplyFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-saturating-multiply.w")
const restaurantUIntBitNotFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-bit-not.w")
const restaurantUIntBitwiseFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-bitwise.w")
const restaurantUIntCompoundFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-compound.w")
const restaurantMutationFixture = resolve(seedDirectory,
  "fixtures", "restaurant-mutation.w")
const restaurantConditionalMutationFixture = resolve(seedDirectory,
  "fixtures", "restaurant-conditional-mutation.w")
const restaurantBoolMutationFixture = resolve(seedDirectory,
  "fixtures", "restaurant-bool-mutation.w")
const restaurantBranchMutationFixture = resolve(seedDirectory,
  "fixtures", "restaurant-branch-mutation.w")
const restaurantMultiBranchMutationFixture = resolve(seedDirectory,
  "fixtures", "restaurant-branch-mutation-multi.w")
const processInputFixture = resolve(seedDirectory, "fixtures", "process-input0.w")
const processArgumentsCountFixture = resolve(seedDirectory, "fixtures",
  "process-arguments-count.w")
const processArgumentsOrderingFixture = resolve(seedDirectory, "fixtures",
  "process-arguments-ordering.w")
const processArgumentsCountSelectiveImportFixture = resolve(seedDirectory,
  "tests", "fixtures", "process-arguments-count-selective-import.w")
const processEnumPayloadFixture = resolve(seedDirectory, "fixtures",
  "process-enum-payload.w")
const localGraphFixture = resolve(seedDirectory, "fixtures", "local-graph",
  "app.w")
const targetTriple = "x86_64-pc-windows-msvc"
const linuxTargetTriple = "x86_64-unknown-linux-gnu"
const maxWindowsCommandLineChars = 32767
const expectedHelp =
  "usage: w check <path/file.w> [--json]\n" +
  "usage: w run <path/file.w> [-- <args...>]\n" +
  "usage: w build <path/file.w> --target <target> --output <artifact>\n"
const expectedWindowsErrorHelp = expectedHelp.replaceAll("\n", "\r\n")

function fail(message) {
  throw new Error(`W RUN Windows: ${message}`)
}

function parseArguments(argv) {
  let ci = false
  for (const argument of argv) {
    if (argument === "--ci" && !ci) ci = true
    else throw new Error(`unknown option: ${String(argument)}`)
  }
  return { ci }
}

const { ci: ciMode } = parseArguments(process.argv.slice(2))

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
  const residue = entries.filter((entry) => entry.name.startsWith(".w-build-"))
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
  for (const name of ["mlir-opt.exe", "mlir-translate.exe", "llc.exe", "lld-link.exe"]) {
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

function assertPeX64(bytes, label) {
  assert(bytes.length >= 0x40 && bytes[0] === 0x4d && bytes[1] === 0x5a,
    `${label} is not an MZ image`)
  const peOffset = bytes.readUInt32LE(0x3c)
  assert(peOffset + 6 <= bytes.length && bytes.subarray(peOffset, peOffset + 4)
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
  "--cse", "-O3", "/Brepro", "/opt:ref", "/opt:icf", "/incremental:no",
  "/merge:.pdata=.rdata"]) {
  assert(`${runSource}\n${emitterSource}`.includes(marker),
    `native Windows implementation marker is missing: ${marker}`)
}
assert(emitterSource.includes("llvm.mlir.zero") &&
  !emitterSource.includes("HeapAlloc") && !emitterSource.includes("HeapFree"),
"Windows emitter must use the bounded global buffer without Heap APIs")
assert(runSource.includes("W_SEED_RUN_COMPILE_PROFILE_DEV"),
  "cli/run.c does not select the development compile profile for w run")
assert(buildSource.includes("W_SEED_RUN_COMPILE_PROFILE_RELEASE"),
  "cli/build.c does not select the release compile profile for w build")
for (const name of ["mlir-opt.exe", "mlir-translate.exe", "llc.exe", "lld-link.exe"])
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

  const invalidSource = join(fixtureDirectory, "invalid.w")
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
  expectExact(binary, ["run", localGraphFixture], 0,
    Buffer.from("answer 42\n", "utf8"),
    "resolved local-module graph fixture")
  expectSourceFailure(binary, privateGraphRoot,
    "private cross-module symbol")
  expectExact(binary, ["run", restaurantIfFixture], 0, expectedIf,
    "Restaurant if fixture")
  expectExact(binary, ["run", restaurantEnumFixture], 0,
    Buffer.from("Courses 10/30/20\n", "utf8"),
    "Restaurant payloadless enum exhaustive switch fixture")
  expectExact(binary, ["run", restaurantEnumSubsetFixture], 0,
    Buffer.from("Work 1/2\n", "utf8"),
    "Restaurant payloadless enum subset switch fixture")
  expectExact(binary, ["run", restaurantEnumPayloadFixture], 0,
    Buffer.from("Bills 32/44/10/7\n", "utf8"),
    "Restaurant enum payload return, reordered captures, and shared variant storage")
  const enumPayloadMutation = join(fixtureDirectory, "enum-payload-mutation.w")
  const enumPayloadSource = await readFile(restaurantEnumPayloadFixture, "utf8")
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
  expectExact(binary, ["run", restaurantEnumBoolPayloadFixture], 0,
    Buffer.from("States true/false/false/true; charges 17/31; licensed true\n", "utf8"),
    "Restaurant Bool and i64 payload union with reordered fields and captures")
  const enumBoolMutation = join(fixtureDirectory, "enum-bool-payload-mutation.w")
  const enumBoolSource = await readFile(restaurantEnumBoolPayloadFixture, "utf8")
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
  expectExact(binary, ["run", restaurantWhileFixture], 0,
    Buffer.from("Served 3\n", "utf8"),
    "Restaurant structured natural while fixture")
  expectExact(binary, ["run", restaurantWhileMultiFixture], 0,
    Buffer.from("Served 9\n", "utf8"),
    "Restaurant structured multi-carrier natural while fixture")
  expectExact(binary, ["run", restaurantWhilePostFixture], 0,
    Buffer.from("Final 9\n", "utf8"),
    "Restaurant post-loop SSA continuation fixture")
  expectExact(binary, ["run", restaurantRepeatFixture], 0,
    Buffer.from("Receipt digits 1/5\n", "utf8"),
    "Restaurant post-test repeat fixture")
  expectExact(binary, ["run", restaurantWmoFixture], 0,
    Buffer.from("Bill 42\n", "utf8"),
    "Restaurant whole-module product closure fixture")
  expectExact(binary, ["run", restaurantAsyncJoinFixture], 0,
    Buffer.from("Prepared 42\n", "utf8"),
    "Restaurant virtual structured-task elision fixture")
  expectExact(binary, ["run", restaurantAsyncYieldFixture], 0,
    Buffer.from("Prepared 88\n", "utf8"),
    "Restaurant virtual Task with statically discharged yields")
  expectExact(binary, ["run", restaurantMainDispatchFixture], 0,
    Buffer.from("Dispatched 88\n", "utf8"),
    "Restaurant physical main-domain dispatch")
  expectExact(binary, ["run", restaurantMainCardinalityFixture], 0,
    Buffer.from("Dispatched 92\n", "utf8"),
    "Restaurant bounded main-domain cardinality")
  expectExact(binary, ["run", restaurantComparisonsFixture], 0,
    Buffer.from("Seat party\nSeat party\nWaitlist\n", "utf8"),
    "Restaurant signed-i64 admission comparison")
  expectExact(binary, ["run", restaurantComparisonCompositionFixture], 0,
    Buffer.from(
      "false/true/true/true/false/false\n" +
      "true/false/false/true/false/true\n" +
      "false/true/false/false/true/true\n" +
      "false/true/true/true/false/false\n" +
      "false/true/false/false/true/true\nAllowed true\nAllowed false\n", "utf8"),
    "Restaurant comparison operators, signed endpoints, and Bool composition")
  expectExact(binary, ["run", restaurantBitwiseFixture], 0,
    Buffer.from("Flags 14/-15\n", "utf8"),
    "Signed-i64 bitwise precedence, complement, and runtime lowering")
  expectExact(binary, ["run", restaurantBoolShortCircuitFixture], 0,
    Buffer.from(
      "Override checked\nClosed allowed true\nCapacity checked\n" +
      "Open allowed true\n", "utf8"),
    "Restaurant Bool short-circuit fixture")
  expectExact(binary, ["run", restaurantScalarIfFixture], 0,
    Buffer.from("Open 5; closed 2\n", "utf8"),
    "Restaurant scalar-if fixture")
  expectExact(binary, ["run", restaurantInterpolationFixture], 0,
    Buffer.from("Table 42 remains open\n", "utf8"),
    "Restaurant interpolation fixture")
  expectExact(binary, ["run", restaurantLinearFixture], 0,
    Buffer.from("Table 42 remains open\nKitchen is ready\n", "utf8"),
    "Restaurant linear fixture")
  expectExact(binary, ["run", restaurantRuntimeDivremFixture], 0,
    Buffer.from("Each 7; left 2\n", "utf8"),
    "Restaurant checked runtime division/remainder")
  expectExact(binary, ["run", restaurantUnaryNegateFixture], 0,
    Buffer.from("Balance -7\n", "utf8"),
    "Restaurant checked runtime unary negation")
  expectExact(binary, ["run", restaurantUnaryInterpolationFixture], 0,
    Buffer.from("Balance -7\n", "utf8"),
    "Restaurant direct unary interpolation")
  expectExact(binary, ["run", restaurantUnsignedFixture], 0,
    Buffer.from("Unsigned 18446744073709551615\n", "utf8"),
    "Restaurant full-width UInt parameter, return, and interpolation")
  expectExact(binary, ["run", restaurantShiftsFixture], 0,
    Buffer.from("Shifts -4/15/-48/48\n", "utf8"),
    "Restaurant checked signed and unsigned shifts")
  expectExact(binary, ["run", restaurantPowerFixture], 0,
    Buffer.from("Power -27/1024/1/512\n", "utf8"),
    "Restaurant checked signed and unsigned power")
  expectExact(binary, ["run", restaurantPowerPrefixFixture], 0,
    Buffer.from("Power prefix -4/4/512/-9/-27\n", "utf8"),
    "Restaurant prefix and power precedence")
  expectExact(binary, ["run", restaurantCompoundFixture], 0,
    Buffer.from("Compound 11\n", "utf8"),
    "Restaurant checked compound assignment")
  expectExact(binary, ["run", restaurantF64StrictFixture], 0,
    Buffer.from("Float strict ok\n", "utf8"),
    "Restaurant strict f64 arithmetic and IEEE comparisons")
  expectExact(binary, ["run", restaurantUIntArithmeticFixture], 0,
    Buffer.from(
      "UInt 9223372036854775810/9223372036854775809/21; div 7; rem 2; " +
      "cmp true/true/true/true/false/true/true\n", "utf8"),
    "Restaurant checked UInt arithmetic and comparisons")
  expectExact(binary, ["run", restaurantUIntWrappingAddFixture], 0,
    Buffer.from("Wrapped 0\n", "utf8"),
    "Restaurant UInt wrappingAdd at the unsigned maximum")
  expectExact(binary, ["run", restaurantUIntWrappingSubtractFixture], 0,
    Buffer.from("Wrapped 18446744073709551615\n", "utf8"),
    "Restaurant UInt wrappingSubtract below zero")
  expectExact(binary, ["run", restaurantUIntWrappingMultiplyFixture], 0,
    Buffer.from("Wrapped 18446744073709551614\n", "utf8"),
    "Restaurant UInt wrappingMultiply at the unsigned maximum")
  expectExact(binary, ["run", restaurantUIntWrappingNegateFixture], 0,
    Buffer.from("Wrapped 18446744073709551615\n", "utf8"),
    "Restaurant UInt wrappingNegate of one")
  expectExact(binary, ["run", restaurantUIntWrappingPowerFixture], 0,
    Buffer.from("Wrapped 12157665459056928801\n", "utf8"),
    "Restaurant UInt wrappingPower of three to forty")
  expectExact(binary, ["run", restaurantUIntWrappingShiftLeftFixture], 0,
    Buffer.from("Wrapped 18446744073709551614\n", "utf8"),
    "Restaurant UInt wrappingShiftLeft with a valid count")
  expectExact(binary, ["run", restaurantUIntMaskedShiftLeftFixture], 0,
    Buffer.from("Masked 2\n", "utf8"),
    "Restaurant UInt maskedShiftLeft reduces count modulo bit width")
  expectExact(binary, ["run", restaurantUIntMaskedShiftRightFixture], 0,
    Buffer.from("Masked 64\n", "utf8"),
    "Restaurant UInt maskedShiftRight reduces count modulo bit width")
  expectExact(binary, ["run", restaurantUIntLogicalShiftRightFixture], 0,
    Buffer.from("Logical 64\n", "utf8"),
    "Restaurant UInt logicalShiftRight uses zero fill")
  expectExact(binary, ["run", restaurantUIntRotatedLeftFixture], 0,
    Buffer.from("Rotated 3\n", "utf8"),
    "Restaurant UInt rotatedLeft reduces count modulo bit width")
  expectExact(binary, ["run", restaurantUIntRotatedRightFixture], 0,
    Buffer.from("Rotated 9223372036854775809\n", "utf8"),
    "Restaurant UInt rotatedRight reduces count modulo bit width")
  expectExact(binary, ["run", restaurantUIntCountOnesFixture], 0,
    Buffer.from("Ones 32\n", "utf8"),
    "Restaurant UInt countOnes uses full-width population count")
  expectExact(binary, ["run", restaurantUIntCountZerosFixture], 0,
    Buffer.from("Zeros 32\n", "utf8"),
    "Restaurant UInt countZeros derives the full-width complement count")
  expectExact(binary, ["run", restaurantUIntLeadingZerosFixture], 0,
    Buffer.from("Leading 56/64\n", "utf8"),
    "Restaurant UInt countLeadingZeros preserves the zero boundary")
  expectExact(binary, ["run", restaurantUIntTrailingZerosFixture], 0,
    Buffer.from("Trailing 12/64\n", "utf8"),
    "Restaurant UInt countTrailingZeros preserves the zero boundary")
  expectExact(binary, ["run", restaurantUIntReversedBitsFixture], 0,
    Buffer.from("Bits 17848844570815808640\n", "utf8"),
    "Restaurant UInt reversedBits preserves the complete logical width")
  expectExact(binary, ["run", restaurantUIntReversedBytesFixture], 0,
    Buffer.from("Bytes 17279655951921914625\n", "utf8"),
    "Restaurant UInt reversedBytes is independent of host endianness")
  expectExact(binary, ["run", restaurantUIntOverflowingAddFixture], 0,
    Buffer.from("Overflowing 0/true/11/false\n", "utf8"),
    "Restaurant UInt overflowingAdd returns wrapped value and overflow flag")
  expectExact(binary, ["run", restaurantUIntSaturatingAddFixture], 0,
    Buffer.from("Saturated 18446744073709551615/11\n", "utf8"),
    "Restaurant UInt saturatingAdd clamps overflow without trapping")
  expectExact(binary, ["run", restaurantUIntSaturatingSubtractFixture], 0,
    Buffer.from("Saturated subtract 0/10\n", "utf8"),
    "Restaurant UInt saturatingSubtract clamps underflow without trapping")
  expectExact(binary, ["run", restaurantUIntSaturatingMultiplyFixture], 0,
    Buffer.from("Saturated multiply 18446744073709551615/42\n", "utf8"),
    "Restaurant UInt saturatingMultiply clamps overflow without trapping")
  expectExact(binary, ["run", restaurantUIntBitNotFixture], 0,
    Buffer.from("UInt not 18446744073709551615\n", "utf8"),
    "Restaurant UInt bitwise complement")
  expectExact(binary, ["run", restaurantUIntBitwiseFixture], 0,
    Buffer.from("UInt bits 18446744073709551615\n", "utf8"),
    "Restaurant UInt binary bitwise operations")
  expectExact(binary, ["run", restaurantUIntCompoundFixture], 0,
    Buffer.from("UInt compound 95\n", "utf8"),
    "Restaurant UInt compound bitwise mutation")
  expectExact(binary, ["run", restaurantMutationFixture], 0,
    Buffer.from("Open 6\n", "utf8"),
    "Restaurant straight-line local mutation")
  expectExact(binary, ["run", restaurantConditionalMutationFixture], 0,
    Buffer.from("Open 6; closed 4\n", "utf8"),
    "Restaurant conditional mutation merged through SSA")
  expectExact(binary, ["run", restaurantBoolMutationFixture], 0,
    Buffer.from("Open true; closed false\n", "utf8"),
    "Restaurant Boolean local mutation")
  expectExact(binary, ["run", restaurantBranchMutationFixture], 0,
    Buffer.from("Open 6; closed 4\n", "utf8"),
    "Restaurant branch-local mutation merge")
  expectExact(binary, ["run", restaurantMultiBranchMutationFixture], 0,
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
  expectExact(binary, ["run", processEnumPayloadFixture], 7,
    Buffer.from("enum-missing true\n", "utf8"),
    "public enum payload process input without arguments")
  expectExact(binary, ["run", processEnumPayloadFixture, "--", ""], 0,
    Buffer.from("enum-received false\n", "utf8"),
    "public enum payload process input with empty argument")
  expectExact(binary, ["run", processEnumPayloadFixture, "--", "payload"], 0,
    Buffer.from("enum-received false\n", "utf8"),
    "public enum payload process input with one argument")
  expectExact(binary, ["run", processArgumentsOrderingFixture], 0,
    Buffer.from("Kitchen seats 0 guests\n", "utf8"),
    "ordered process count input without arguments")
  expectExact(binary, ["run", processArgumentsOrderingFixture, "--", ""], 0,
    Buffer.from("Kitchen seats 1 guests\n", "utf8"),
    "ordered process count input with empty argument")
  expectExact(binary,
    ["run", processArgumentsOrderingFixture, "--", "alpha", "beta"], 0,
    Buffer.from("Banquet seats 2 guests\n", "utf8"),
    "ordered process count input with two arguments")

  const buildHello = join(fixtureDirectory, "hello-build.exe")
  const buildLocalGraph = join(fixtureDirectory, "local-graph-build.exe")
  const buildPrivateGraph = join(fixtureDirectory, "private-graph-build.exe")
  const buildRestaurantIf = join(fixtureDirectory, "restaurant-if-build.exe")
  const buildRestaurantRepeat = join(fixtureDirectory,
    "restaurant-repeat-build.exe")
  const buildRestaurantMainDispatch = join(fixtureDirectory,
    "restaurant-main-dispatch-build.exe")
  const buildRestaurantMainCardinality = join(fixtureDirectory,
    "restaurant-main-cardinality-build.exe")
  const buildProcessInput = join(fixtureDirectory, "process-input-build.exe")
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
    "restaurant-main-dispatch-linux")
  const buildMissingParent = join(fixtureDirectory, "missing", "artifact.exe")
  expectExact(binary, ["build", helloFixture, "--target", targetTriple,
    "--output", buildHello], 0, Buffer.alloc(0), "build Hello fixture")
  const builtHelloStats = await lstat(buildHello)
  assert(builtHelloStats.isFile() && !builtHelloStats.isSymbolicLink(),
    "build Hello did not produce a regular artifact")
  expectExact(buildHello, [], 0, Buffer.from("Hello, world!\n", "utf8"),
    "execute built Hello artifact")
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
  expectExact(binary, ["build", restaurantIfFixture, "--target", targetTriple,
    "--output", buildRestaurantIf], 0, Buffer.alloc(0),
    "build restaurant-if fixture")
  expectExact(buildRestaurantIf, [], 0, expectedIf,
    "execute built restaurant-if artifact")
  expectExact(binary, ["build", restaurantRepeatFixture, "--target", targetTriple,
    "--output", buildRestaurantRepeat], 0, Buffer.alloc(0),
    "build restaurant-repeat fixture")
  const builtRestaurantRepeatStats = await lstat(buildRestaurantRepeat)
  assert(builtRestaurantRepeatStats.isFile() &&
    !builtRestaurantRepeatStats.isSymbolicLink(),
    "build restaurant-repeat did not produce a regular artifact")
  assertPeX64(await readFile(buildRestaurantRepeat),
    "built restaurant-repeat artifact")
  expectExact(buildRestaurantRepeat, [], 0,
    Buffer.from("Receipt digits 1/5\n", "utf8"),
    "execute built restaurant-repeat artifact")
  expectExact(binary, ["build", restaurantMainDispatchFixture, "--target",
    targetTriple, "--output", buildRestaurantMainDispatch], 0,
  Buffer.alloc(0), "build restaurant main-domain dispatch fixture")
  assertPeX64(await readFile(buildRestaurantMainDispatch),
    "built restaurant main-domain dispatch artifact")
  expectExact(buildRestaurantMainDispatch, [], 0,
    Buffer.from("Dispatched 88\n", "utf8"),
    "execute built restaurant main-domain dispatch artifact")
  expectExact(binary, ["build", restaurantMainCardinalityFixture, "--target",
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
  expectExact(binary, ["build", processArgumentsCountFixture, "--target",
    targetTriple, "--output", buildProcessArgumentsCount], 0,
    Buffer.alloc(0), "build public process-arguments-count fixture")
  const builtProcessArgumentsCountStats = await lstat(buildProcessArgumentsCount)
  assert(builtProcessArgumentsCountStats.isFile() &&
    !builtProcessArgumentsCountStats.isSymbolicLink(),
    "build process-arguments-count did not produce a regular artifact")
  assertPeX64(await readFile(buildProcessArgumentsCount),
    "built process-arguments-count artifact")
  expectExact(binary, ["build", processArgumentsCountSelectiveImportFixture,
    "--target", targetTriple, "--output", buildProcessArgumentsCountSelectiveImport],
    0, Buffer.alloc(0),
    "build selective-import process-arguments-count fixture")
  const flatImportBytes = await readFile(buildProcessArgumentsCount)
  const selectiveImportBytes = await readFile(buildProcessArgumentsCountSelectiveImport)
  assertPeX64(selectiveImportBytes,
    "built selective-import process-arguments-count artifact")
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
  assertPeX64(await readFile(buildProcessArgumentsOrdering),
    "built ordered process-arguments artifact")
  const orderedArgumentCountCases = [
    ["without user arguments", [], "Kitchen seats 0 guests\n"],
    ["with one empty argument", [""], "Kitchen seats 1 guests\n"],
    ["with two ordinary arguments", ["alpha", "beta"],
      "Banquet seats 2 guests\n"],
    ["with exactly 256 user arguments", Array.from({ length: 256 }, () => "x"),
      "Banquet seats 256 guests\n"],
  ]
  for (const [label, argumentsList, expectedOutput] of orderedArgumentCountCases)
    expectExact(buildProcessArgumentsOrdering, argumentsList, 0,
      Buffer.from(expectedOutput, "utf8"),
      `execute built ordered process-arguments artifact ${label}`)
  expectExact(buildProcessArgumentsOrdering,
    Array.from({ length: 257 }, () => "x"), 3, Buffer.alloc(0),
    "reject ordered process descriptor overflow without partial output")
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
    Buffer.from("enum-missing true\n", "utf8"),
    "execute built enum payload process artifact without arguments")
  expectExact(buildProcessEnumPayload, [""], 0,
    Buffer.from("enum-received false\n", "utf8"),
    "execute built enum payload process artifact with empty argument")
  expectExact(buildProcessEnumPayload, ["payload"], 0,
    Buffer.from("enum-received false\n", "utf8"),
    "execute built enum payload process artifact with one argument")
  expectExact(binary, ["build", restaurantMainCardinalityFixture, "--target",
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
  expectExact(wsl, ["-d", "Ubuntu", "--", wslPath(buildLinuxTarget)], 0,
    Buffer.from("Dispatched 92\n", "utf8"),
  "execute cross-built main-domain cardinality Linux artifact through WSL2")
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
  for (const role of ["mlirOpt", "mlirTranslate", "llc", "linkDriver"]) {
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
