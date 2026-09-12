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

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const manifestPath = resolve(import.meta.dir, "mlir0-windows-toolchain.json")
const materializedPath = join(defaultCacheDirectory(), MATERIALIZED_MANIFEST)
const smokePath = resolve(import.meta.dir, "smoke-mlir0-windows.mjs")
const helloFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const restaurantIfFixture = resolve(seedDirectory, "fixtures", "restaurant-if.w")
const restaurantEnumFixture = resolve(seedDirectory, "fixtures", "restaurant-enum.w")
const restaurantWhileFixture = resolve(seedDirectory, "fixtures", "restaurant-while.w")
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
const targetTriple = "x86_64-pc-windows-msvc"
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
  "--cse", "-O3", "/opt:ref", "/opt:icf", "/incremental:no"]) {
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
  const invalidComparisons = [
    ["Bool operands", "true == false"],
    ["String operands", '"a" != "b"'],
    ["mixed operands", "1 <= true"],
    ["comparison used as i64", "(1 < 2) + 3"],
  ]
  await writeFile(invalidSource, Buffer.from([0xc3]))
  await writeFile(unsupportedSource, "fn main() { noop(\"Other\") }\nentry(main)\n")
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
  expectExact(binary, ["run", restaurantIfFixture], 0, expectedIf,
    "Restaurant if fixture")
  expectExact(binary, ["run", restaurantEnumFixture], 0,
    Buffer.from("Courses 10/30/20\n", "utf8"),
    "Restaurant payloadless enum exhaustive switch fixture")
  expectExact(binary, ["run", restaurantWhileFixture], 0,
    Buffer.from("Served 3\n", "utf8"),
    "Restaurant structured natural while fixture")
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

  const buildHello = join(fixtureDirectory, "hello-build.exe")
  const buildRestaurantIf = join(fixtureDirectory, "restaurant-if-build.exe")
  const buildProcessInput = join(fixtureDirectory, "process-input-build.exe")
  const buildWrongTarget = join(fixtureDirectory, "wrong-target-build.exe")
  const buildMissingParent = join(fixtureDirectory, "missing", "artifact.exe")
  expectExact(binary, ["build", helloFixture, "--target", targetTriple,
    "--output", buildHello], 0, Buffer.alloc(0), "build Hello fixture")
  const builtHelloStats = await lstat(buildHello)
  assert(builtHelloStats.isFile() && !builtHelloStats.isSymbolicLink(),
    "build Hello did not produce a regular artifact")
  expectExact(buildHello, [], 0, Buffer.from("Hello, world!\n", "utf8"),
    "execute built Hello artifact")
  const helloBytes = await readFile(buildHello)
  expectBuildFailure(binary, ["build", helloFixture, "--target", targetTriple,
    "--output", buildHello], "reject existing build output")
  assert((await readFile(buildHello)).equals(helloBytes),
    "existing build output was modified")
  expectExact(binary, ["build", restaurantIfFixture, "--target", targetTriple,
    "--output", buildRestaurantIf], 0, Buffer.alloc(0),
    "build restaurant-if fixture")
  expectExact(buildRestaurantIf, [], 0, expectedIf,
    "execute built restaurant-if artifact")
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
  expectBuildFailure(binary, ["build", helloFixture, "--target",
    "x86_64-unknown-linux-gnu", "--output", buildWrongTarget],
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
  console.log(`W RUN Windows: native E2E passed toolchain=${defaultCacheDirectory()} sdk=${sdk.version} vs=${visualStudio.installationPath} wExeBytes=${binaryStats.size} peBytes=${exeMatch[1]} processPeBytes=${builtProcessInputStats.size} diskFreeBefore=${diskBefore} diskFreeAfter=${diskAfterRuns}`)
} finally {
  await rm(unsupportedBuildDirectory, { recursive: true, force: true })
  await rm(buildDirectory, { recursive: true, force: true })
  await rm(fixtureDirectory, { recursive: true, force: true })
}
