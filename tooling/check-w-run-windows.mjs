import { existsSync } from "node:fs"
import { lstat, mkdtemp, readdir, readFile, rm, statfs, writeFile } from "node:fs/promises"
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
const restaurantInterpolationFixture = resolve(
  seedDirectory, "fixtures", "restaurant-interpolation.w")
const restaurantLinearFixture = resolve(seedDirectory, "fixtures", "restaurant-linear.w")
const expectedHelp =
  "usage: w check <path/file.w> [--json]\n" +
  "usage: w run <path/file.w> [-- <args...>]\n"
const expectedWindowsErrorHelp = expectedHelp.replaceAll("\n", "\r\n")

function fail(message) {
  throw new Error(`W RUN Windows: ${message}`)
}

function assert(condition, message) {
  if (!condition) fail(message)
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

function assertPeX64(bytes, label) {
  assert(bytes.length >= 0x40 && bytes[0] === 0x4d && bytes[1] === 0x5a,
    `${label} is not an MZ image`)
  const peOffset = bytes.readUInt32LE(0x3c)
  assert(peOffset + 6 <= bytes.length && bytes.subarray(peOffset, peOffset + 4)
    .equals(Buffer.from("PE\0\0", "ascii")) && bytes.readUInt16LE(peOffset + 4) === 0x8664,
  `${label} is not a PE x64 image`)
}

if (process.platform !== "win32" || process.arch !== "x64") {
  console.log(`W RUN Windows: SKIP ${process.platform}/${process.arch}`)
  process.exit(0)
}

const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
const manifestErrors = validateManifest(manifest)
assert(manifestErrors.length === 0, manifestErrors.join("; "))
assert(manifest.buildBoundary?.configuration?.cStandard === "23" &&
  manifest.buildBoundary?.configuration?.recoveryCStandard === "11" &&
  manifest.buildBoundary?.configuration?.cStandardPolicy ===
    "C23-primary; C11-explicit-recovery-only",
"Windows C standard policy is not explicit")
const cStandard = "11"
console.log("W RUN Windows: C23 primary is unsupported by this MSVC/CMake; using explicit C11 recovery")
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
  "-mtriple=x86_64-pc-windows-msvc", "/nodefaultlib"]) {
  assert(`${runSource}\n${emitterSource}`.includes(marker),
    `native Windows implementation marker is missing: ${marker}`)
}
assert(emitterSource.includes("llvm.mlir.zero") &&
  !emitterSource.includes("HeapAlloc") && !emitterSource.includes("HeapFree"),
"Windows emitter must use the bounded global buffer without Heap APIs")
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
  assertNoNewResidue(residueBefore, await snapshotResidue())

  const cmakeArguments = [
    "-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=cl",
    `-DCMAKE_MAKE_PROGRAM=${ninja}`,
    `-DW_SEED_C_STANDARD=${cStandard}`,
    "-DW_SEED_ENABLE_WINDOWS_NATIVE_RUN=ON",
    `-DW_MLIR0_WINDOWS_MLIR_OPT=${materialized.tools["mlir-opt.exe"].absolutePath}`,
    `-DW_MLIR0_WINDOWS_MLIR_TRANSLATE=${materialized.tools["mlir-translate.exe"].absolutePath}`,
    `-DW_MLIR0_WINDOWS_LLC=${materialized.tools["llc.exe"].absolutePath}`,
    `-DW_MLIR0_WINDOWS_LLD_LINK=${materialized.tools["lld-link.exe"].absolutePath}`,
    `-DW_MLIR0_WINDOWS_KERNEL32_LIB=${sdk.path}`,
  ]
  runWithVs("native CMake configure", vsDevCmd, cmake, cmakeArguments)
  runWithVs("native w build", vsDevCmd, cmake,
    ["--build", buildDirectory, "--target", "w", "--", "-j", "2"])
  const binary = join(buildDirectory, "w.exe")
  const binaryStats = await lstat(binary)
  assert(binaryStats.isFile() && !binaryStats.isSymbolicLink(),
    "native build did not produce w.exe")
  assert(!existsSync(join(buildDirectory, "bin", "mlir-opt.exe")),
    "native build copied the external MLIR toolchain")

  const invalidSource = join(fixtureDirectory, "invalid.w")
  const unsupportedSource = join(fixtureDirectory, "unsupported.w")
  await writeFile(invalidSource, Buffer.from([0xc3]))
  await writeFile(unsupportedSource, "fn main() { noop(\"Other\") }\nentry(main)\n")
  const expectedIf = Buffer.from(
    "Kitchen open\nAfter service\nKitchen closed\nAfter service\n", "utf8")
  expectExact(binary, ["--help"], 0, Buffer.from(expectedHelp), "w --help")
  expectExact(binary, ["run", helloFixture], 0,
    Buffer.from("Hello, world!\n", "utf8"), "Hello fixture")
  expectExact(binary, ["run", restaurantIfFixture], 0, expectedIf,
    "Restaurant if fixture")
  expectExact(binary, ["run", restaurantInterpolationFixture], 0,
    Buffer.from("Table 42 remains open\n", "utf8"),
    "Restaurant interpolation fixture")
  expectExact(binary, ["run", restaurantLinearFixture], 0,
    Buffer.from("Table 42 remains open\nKitchen is ready\n", "utf8"),
    "Restaurant linear fixture")
  expectExact(binary, ["run", helloFixture, "--", "arbitrary", "--entry", ""],
    0, Buffer.from("Hello, world!\n", "utf8"), "forwarded program arguments")
  expectSourceFailure(binary, invalidSource, "invalid UTF-8 source")
  expectSourceFailure(binary, unsupportedSource, "unsupported source")
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
  console.log(`W RUN Windows: native E2E passed toolchain=${defaultCacheDirectory()} sdk=${sdk.version} vs=${visualStudio.installationPath} wExeBytes=${binaryStats.size} peBytes=${exeMatch[1]} diskFreeBefore=${diskBefore} diskFreeAfter=${diskAfterRuns}`)
} finally {
  await rm(unsupportedBuildDirectory, { recursive: true, force: true })
  await rm(buildDirectory, { recursive: true, force: true })
  await rm(fixtureDirectory, { recursive: true, force: true })
}
