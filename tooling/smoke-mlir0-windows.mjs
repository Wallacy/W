import { lstat, mkdtemp, readdir, readFile, rm, statfs, writeFile } from "node:fs/promises"
import { homedir, tmpdir } from "node:os"
import { basename, dirname, isAbsolute, join, relative, resolve, sep } from "node:path"
import { MATERIALIZED_MANIFEST, defaultCacheDirectory } from "./acquire-mlir0-windows.mjs"

const repositoryRoot = resolve(import.meta.dir, "..")
const SMOKE_PREFIX = "w-mlir0-windows-smoke-"
const expectedOutput = Buffer.from("W native smoke\n", "utf8")
const requiredTools = [
  "mlir-opt.exe",
  "mlir-translate.exe",
  "llc.exe",
  "lld-link.exe",
]

function fail(message) {
  throw new Error(`MLIR0 Windows smoke: ${message}`)
}

function assert(condition, message) {
  if (!condition) fail(message)
}

function isContained(parent, candidate) {
  const relativePath = relative(resolve(parent), resolve(candidate))
  return relativePath === "" || (relativePath !== ".." &&
    !relativePath.startsWith(`..${sep}`) && !isAbsolute(relativePath))
}

function spawn(command, args, cwd) {
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

function runRequired(label, command, args, cwd) {
  const result = spawn(command, args, cwd)
  if (result.exitCode !== 0)
    fail(`${label} failed: ${(result.stderr.toString() || result.stdout.toString()).trim()}`)
  return result
}

function parseArguments(argumentsList) {
  let toolchain = defaultCacheDirectory()
  let sdk = undefined
  for (let index = 0; index < argumentsList.length; index += 1) {
    const argument = argumentsList[index]
    if (argument === "--toolchain" || argument === "--sdk") {
      const value = argumentsList[index + 1]
      if (typeof value !== "string" || value.length === 0)
        fail(`${argument} requires a path`)
      if (argument === "--toolchain") toolchain = value
      else sdk = value
      index += 1
    } else if (argument === "--help" || argument === "-h") {
      console.log("usage: bun tooling/smoke-mlir0-windows.mjs [--toolchain <cache>] [--sdk <Windows Kits root>]")
      return undefined
    } else fail(`unknown option: ${argument}`)
  }
  return { toolchain: resolve(toolchain), sdk: sdk === undefined ? undefined : resolve(sdk) }
}

async function readMaterialized(toolchain) {
  const manifestPath = join(toolchain, MATERIALIZED_MANIFEST)
  const source = await readFile(manifestPath, "utf8")
  let document
  try {
    document = JSON.parse(source)
  } catch (error) {
    fail(`materialized manifest is invalid JSON: ${error.message}`)
  }
  assert(document?.$schema === "w-seed-mlir0-windows-materialized-1" &&
    document.version === 1,
  "materialized manifest schema is invalid")
  assert(document.target?.triple === "x86_64-pc-windows-msvc",
    "materialized target is not Windows x86_64 MSVC")
  assert(document.distributionRole === "development-and-release-only" &&
    document.bundledWithW === false && document.extractedSizeIsWBudget === false,
  "materialized toolchain is not marked as development/release-only")
  assert(resolve(document.destination) === resolve(toolchain),
    "materialized destination does not match the requested cache")
  return document
}

async function resolveTool(toolchain, document, name) {
  const record = document.tools?.[name]
  assert(record && typeof record.relativePath === "string",
    `materialized tool record is missing: ${name}`)
  const pathValue = resolve(toolchain, record.relativePath)
  assert(isContained(toolchain, pathValue), `tool path escapes cache: ${name}`)
  const stats = await lstat(pathValue)
  assert(stats.isFile() && !stats.isSymbolicLink(),
    `materialized tool is not a regular file: ${name}`)
  return pathValue
}

async function findKernel32(explicitSdk) {
  const roots = []
  if (explicitSdk !== undefined) roots.push(explicitSdk)
  if (process.env.WindowsSdkDir !== undefined)
    roots.push(resolve(process.env.WindowsSdkDir))
  if (process.env["WindowsSdkDir"] !== undefined)
    roots.push(resolve(process.env["WindowsSdkDir"]))
  for (const variable of ["ProgramFiles(x86)", "ProgramW6432", "ProgramFiles"]) {
    if (process.env[variable] !== undefined)
      roots.push(join(process.env[variable], "Windows Kits", "10"))
  }
  roots.push(join(homedir(), "AppData", "Local", "Microsoft", "Windows Kits", "10"))
  const seen = new Set()
  for (const root of roots) {
    const normalizedRoot = resolve(root)
    if (seen.has(normalizedRoot)) continue
    seen.add(normalizedRoot)
    let entries
    try {
      entries = await readdir(join(normalizedRoot, "Lib"), { withFileTypes: true })
    } catch (error) {
      if (error?.code === "ENOENT") continue
      throw error
    }
    const versions = entries.filter((entry) => entry.isDirectory())
      .map((entry) => entry.name).sort().reverse()
    for (const version of versions) {
      const candidate = join(normalizedRoot, "Lib", version, "um", "x64", "kernel32.lib")
      try {
        const stats = await lstat(candidate)
        if (stats.isFile() && !stats.isSymbolicLink())
          return { path: candidate, root: normalizedRoot, version }
      } catch (error) {
        if (error?.code !== "ENOENT") throw error
      }
    }
  }
  fail("Windows SDK kernel32.lib was not found by explicit SDK probes")
}

async function diskFree(pathValue) {
  const value = await statfs(pathValue)
  return Number(value.bavail) * Number(value.bsize)
}

const options = parseArguments(process.argv.slice(2))
if (options !== undefined) {
  assert(process.platform === "win32" && process.arch === "x64",
    `native smoke requires Windows x86_64, got ${process.platform}/${process.arch}`)
  const toolchain = options.toolchain
  const document = await readMaterialized(toolchain)
  const tools = {}
  for (const name of requiredTools)
    tools[name] = await resolveTool(toolchain, document, name)
  const sdk = await findKernel32(options.sdk)
  const smokeDirectory = await mkdtemp(join(tmpdir(), SMOKE_PREFIX))
  assert(basename(smokeDirectory).startsWith(SMOKE_PREFIX),
    "smoke directory ownership guard failed")
  const diskBefore = await diskFree(dirname(smokeDirectory))
  const chainInputPath = join(smokeDirectory, "chain.mlir")
  const chainVerifiedPath = join(smokeDirectory, "chain.verified.mlir")
  const chainLlPath = join(smokeDirectory, "chain.ll")
  const chainObjectPath = join(smokeDirectory, "chain.obj")
  const irPath = join(smokeDirectory, "windows-smoke.ll")
  const objectPath = join(smokeDirectory, "windows-smoke.obj")
  const executablePath = join(smokeDirectory, "windows-smoke.exe")
  const ir = `target triple = "x86_64-pc-windows-msvc"
@message = private unnamed_addr constant [15 x i8] c"W native smoke\\0A", align 1

declare ptr @GetStdHandle(i32)
declare i32 @WriteFile(ptr, ptr, i32, ptr, ptr)
declare void @ExitProcess(i32)

define dso_local void @mainCRTStartup() {
entry:
  %stdout = call ptr @GetStdHandle(i32 -11)
  %written = alloca i32, align 4
  call i32 @WriteFile(ptr %stdout, ptr @message, i32 15, ptr %written, ptr null)
  call void @ExitProcess(i32 0)
  ret void
}
`
  try {
    await writeFile(chainInputPath,
      "module { llvm.func @w_windows_chain_probe() { llvm.return } }\n",
      "utf8")
    runRequired("mlir-opt Windows chain", tools["mlir-opt.exe"], [
      chainInputPath,
      "-o",
      chainVerifiedPath,
      "--verify-each",
    ], smokeDirectory)
    runRequired("mlir-translate Windows chain", tools["mlir-translate.exe"], [
      "--mlir-to-llvmir",
      chainVerifiedPath,
      "-o",
      chainLlPath,
    ], smokeDirectory)
    runRequired("llc translated Windows chain", tools["llc.exe"], [
      "-filetype=obj",
      "-mtriple=x86_64-pc-windows-msvc",
      chainLlPath,
      "-o",
      chainObjectPath,
    ], smokeDirectory)
    await writeFile(irPath, ir, "utf8")
    runRequired("llc version probe", tools["llc.exe"], ["--version"], smokeDirectory)
    runRequired("lld-link version probe", tools["lld-link.exe"], ["--version"], smokeDirectory)
    runRequired("llc Windows COFF object", tools["llc.exe"], [
      "-filetype=obj",
      "-mtriple=x86_64-pc-windows-msvc",
      irPath,
      "-o",
      objectPath,
    ], smokeDirectory)
    runRequired("lld-link Windows no-CRT executable", tools["lld-link.exe"], [
      "/entry:mainCRTStartup",
      "/subsystem:console",
      "/nodefaultlib",
      "/machine:x64",
      `/out:${executablePath}`,
      objectPath,
      sdk.path,
    ], smokeDirectory)
    const execution = spawn(executablePath, [], smokeDirectory)
    assert(execution.exitCode === 0,
      `smoke executable returned ${execution.exitCode}: ${execution.stderr.toString()}`)
    assert(execution.stderr.length === 0,
      `smoke executable wrote stderr: ${execution.stderr.toString()}`)
    assert(execution.stdout.equals(expectedOutput),
      `smoke stdout differs: ${JSON.stringify(execution.stdout.toString())}`)
    const diskAfter = await diskFree(dirname(smokeDirectory))
    const chainObjectSize = (await lstat(chainObjectPath)).size
    const objectSize = (await lstat(objectPath)).size
    const executableSize = (await lstat(executablePath)).size
    console.log(`MLIR0 Windows smoke: passed toolchain=${toolchain} sdk=${sdk.root} sdkVersion=${sdk.version} chainObjectBytes=${chainObjectSize} objectBytes=${objectSize} exeBytes=${executableSize} diskFreeBefore=${diskBefore} diskFreeAfter=${diskAfter}`)
  } finally {
    await rm(smokeDirectory, { recursive: true, force: true })
  }
}
