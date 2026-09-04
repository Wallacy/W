import { createHash } from "node:crypto"
import { copyFile, lstat, mkdir, mkdtemp, readFile, readdir, rename, rm } from "node:fs/promises"
import { join, relative, resolve, sep } from "node:path"
import { tmpdir } from "node:os"
import {
  MATERIALIZED_MANIFEST,
  defaultCacheDirectory,
  validateManifest,
  validateMaterialized,
} from "./acquire-mlir0-windows.mjs"
import {
  findVisualStudio,
  findWindowsSdkKernel32,
  runWithVisualStudio,
} from "./windows-build-support.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const manifestPath = resolve(import.meta.dir, "mlir0-windows-toolchain.json")
const outputDirectory = resolve(root, "build", "w-windows")
const binaryPath = join(outputDirectory, "w.exe")

function fail(message) {
  throw new Error(`W Windows build: ${message}`)
}

function isContained(parent, candidate) {
  const pathRelative = relative(resolve(parent), resolve(candidate))
  return pathRelative === "" || (pathRelative !== ".." &&
    !pathRelative.startsWith(`..${sep}`) && !pathRelative.startsWith("../") &&
    !pathRelative.includes(":") && !pathRelative.startsWith("/"))
}

function requireCommand(name) {
  const command = Bun.which(name)
  if (command === null) fail(`${name} is unavailable; install it before running the offline build`)
  return command
}

function runChecked(label, command, args, cwd = root) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
  })
  if (result.exitCode !== 0) {
    const output = `${Buffer.from(result.stderr).toString()}${Buffer.from(result.stdout).toString()}`
      .trim().slice(-2000)
    fail(`${label} failed${output.length > 0 ? `: ${output}` : ""}`)
  }
  return result
}

function runWithVisualStudioChecked(label, vsDevCmd, command, args) {
  const result = runWithVisualStudio(vsDevCmd, command, args, { cwd: root })
  if (result.exitCode !== 0) {
    const output = `${Buffer.from(result.stderr).toString()}${Buffer.from(result.stdout).toString()}`
      .trim().slice(-2000)
    fail(`${label} failed${output.length > 0 ? `: ${output}` : ""}`)
  }
  return result
}

function expectProgram(label, args, expectedStdout) {
  const result = Bun.spawnSync({
    cmd: [binaryPath, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
  })
  const stdout = Buffer.from(result.stdout)
  const stderr = Buffer.from(result.stderr)
  if (result.exitCode !== 0 || !stdout.equals(Buffer.from(expectedStdout, "utf8")) ||
      stderr.length !== 0) {
    fail(`${label} was not exact: ${JSON.stringify({
      exitCode: result.exitCode,
      stdout: stdout.toString(),
      stderr: stderr.toString(),
    })}`)
  }
}

async function installBinary(sourcePath) {
  await mkdir(outputDirectory, { recursive: true })
  const stagedPath = join(outputDirectory,
    `.w.exe-${process.pid}-${Date.now().toString(36)}.tmp`)
  try {
    await copyFile(sourcePath, stagedPath)
    const stagedStats = await lstat(stagedPath)
    if (!stagedStats.isFile() || stagedStats.isSymbolicLink() || stagedStats.size === 0)
      fail("staged w.exe is not a non-empty regular file")
    try {
      await rename(stagedPath, binaryPath)
    } catch (error) {
      if (!error || !["EEXIST", "EPERM", "ENOTEMPTY"].includes(error.code)) throw error
      await rm(binaryPath, { force: true })
      await rename(stagedPath, binaryPath)
    }
  } finally {
    await rm(stagedPath, { force: true })
  }
}

async function assertOnlyBinary() {
  const entries = await readdir(outputDirectory)
  if (entries.length !== 1 || entries[0] !== "w.exe")
    fail(`persistent output must contain only w.exe, found: ${entries.join(", ")}`)
}

async function main() {
  if (process.platform !== "win32" || process.arch !== "x64")
    fail(`requires native Windows x86_64, got ${process.platform}/${process.arch}`)
  const buildOptions = process.argv.slice(2)
  if (buildOptions.some((option) => option !== "--help" && option !== "--c11-recovery"))
    fail(`unknown option: ${process.argv.slice(2).join(" ")}`)
  if (process.argv.includes("--help")) {
    console.log("usage: bun tooling/build-w-windows.mjs [--c11-recovery]")
    return
  }
  const cStandard = process.argv.includes("--c11-recovery") ? "11" : "23"
  if (cStandard === "11")
    console.log("W Windows build: C23 is the primary standard; using explicit C11 recovery")

  const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
  const manifestErrors = validateManifest(manifest)
  if (manifestErrors.length > 0) fail(manifestErrors.join("; "))
  const manifestHash = createHash("sha256")
    .update(await readFile(manifestPath)).digest("hex")
  const cacheDirectory = defaultCacheDirectory()
  if (isContained(root, cacheDirectory))
    fail(`materialized cache must be outside the repository: ${cacheDirectory}`)
  const materializedPath = join(cacheDirectory, MATERIALIZED_MANIFEST)
  const materialized = await validateMaterialized(cacheDirectory, manifest, manifestHash)
  const toolPaths = {}
  for (const name of manifest.tools.required) {
    const record = materialized.tools?.[name]
    if (record === null || typeof record?.relativePath !== "string")
      fail(`validated materialization has no required ${name}`)
    const toolPath = resolve(cacheDirectory, record.relativePath)
    if (!isContained(cacheDirectory, toolPath)) fail(`tool path escapes cache: ${name}`)
    toolPaths[name] = toolPath
  }
  const sdk = await findWindowsSdkKernel32()
  const visualStudio = findVisualStudio()
  const cmake = requireCommand("cmake")
  const ninja = requireCommand("ninja")
  const buildDirectory = await mkdtemp(join(tmpdir(), "w-build-windows-"))
  try {
  const cmakeArguments = [
    "-S", seedDirectory,
    "-B", buildDirectory,
    "-G", "Ninja",
    `-DCMAKE_MAKE_PROGRAM=${ninja}`,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_C_COMPILER=cl",
    `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${buildDirectory}`,
    `-DW_SEED_C_STANDARD=${cStandard}`,
    "-DW_SEED_ENABLE_WINDOWS_NATIVE_RUN=ON",
    `-DW_MLIR0_WINDOWS_MLIR_OPT=${toolPaths["mlir-opt.exe"]}`,
    `-DW_MLIR0_WINDOWS_MLIR_TRANSLATE=${toolPaths["mlir-translate.exe"]}`,
    `-DW_MLIR0_WINDOWS_LLC=${toolPaths["llc.exe"]}`,
    `-DW_MLIR0_WINDOWS_LLD_LINK=${toolPaths["lld-link.exe"]}`,
    `-DW_MLIR0_WINDOWS_KERNEL32_LIB=${sdk.path}`,
  ]
  runWithVisualStudioChecked("CMake configure", visualStudio.devCommand,
    cmake, cmakeArguments)
  runWithVisualStudioChecked("w build", visualStudio.devCommand, cmake,
    ["--build", buildDirectory, "--target", "w", "--", "-j", "2"])
  const builtBinary = join(buildDirectory, "w.exe")
  const builtStats = await lstat(builtBinary)
  if (!builtStats.isFile() || builtStats.isSymbolicLink() || builtStats.size === 0)
    fail(`build did not produce a non-empty regular ${builtBinary}`)
  if (await lstat(materializedPath).catch(() => null) === null)
    fail("materialized manifest disappeared during the build")
  if (await lstat(join(buildDirectory, "bin", "mlir-opt.exe")).catch(() => null) !== null)
    fail("build output copied MLIR tools into the W build directory")

  await installBinary(builtBinary)
  await assertOnlyBinary()
  const binaryStats = await lstat(binaryPath)

  expectProgram("Hello smoke", ["run", join(seedDirectory, "fixtures", "hlo0-hello.w")],
    "Hello, world!\n")
  expectProgram("Restaurant smoke", ["run", join(seedDirectory, "fixtures", "restaurant-if.w")],
    "Kitchen open\nAfter service\nKitchen closed\nAfter service\n")
  console.log(`W Windows build: wExe=${binaryPath} bytes=${binaryStats.size} ` +
    `cache=${cacheDirectory} sdk=${sdk.version} vs=${visualStudio.installationPath} cStandard=${cStandard}`)
  const buildCommand = cStandard === "11"
    ? "bun run build:w-windows --c11-recovery"
    : "bun run build:w-windows"
  console.log(`Test: ${buildCommand}; ${binaryPath} run compiler/seed-c/fixtures/hlo0-hello.w`)
  } finally {
    await rm(buildDirectory, { recursive: true, force: true })
  }
}

if (import.meta.main) {
  main().catch((error) => {
    console.error(error.message)
    process.exitCode = 1
  })
}
