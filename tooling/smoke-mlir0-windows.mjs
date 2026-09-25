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
  "opt.exe",
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

function runRequired(label, command, args, cwd, env = undefined) {
  const raw = env === undefined
    ? spawn(command, args, cwd)
    : Bun.spawnSync({
      cmd: [command, ...args],
      cwd,
      env,
      stdout: "pipe",
      stderr: "pipe",
    })
  const result = {
    ...raw,
    stdout: Buffer.from(raw.stdout),
    stderr: Buffer.from(raw.stderr),
  }
  if (result.exitCode !== 0)
    fail(`${label} failed: ${(result.stderr.toString() || result.stdout.toString()).trim()}`)
  return result
}

function parseArguments(argumentsList) {
  let toolchain = defaultCacheDirectory()
  let sdk = undefined
  let wide = false
  for (let index = 0; index < argumentsList.length; index += 1) {
    const argument = argumentsList[index]
    if (argument === "--toolchain" || argument === "--sdk") {
      const value = argumentsList[index + 1]
      if (typeof value !== "string" || value.length === 0)
        fail(`${argument} requires a path`)
      if (argument === "--toolchain") toolchain = value
      else sdk = value
      index += 1
    } else if (argument === "--wide") {
      wide = true
    } else if (argument === "--help" || argument === "-h") {
      console.log("usage: bun tooling/smoke-mlir0-windows.mjs [--toolchain <cache>] [--sdk <Windows Kits root>] [--wide]")
      return undefined
    } else fail(`unknown option: ${argument}`)
  }
  return { toolchain: resolve(toolchain), sdk: sdk === undefined ? undefined : resolve(sdk), wide }
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

async function resolveArchiveTool(toolchain, document, name) {
  const matches = document.archiveEntries?.filter((entry) =>
    entry?.type === "file" && basename(entry.path).toLowerCase() ===
      name.toLowerCase()) ?? []
  assert(matches.length === 1,
    `pinned archive must contain one unambiguous ${name}`)
  const pathValue = resolve(toolchain, ...matches[0].path.split("/"))
  assert(isContained(toolchain, pathValue), `${name} escapes the toolchain cache`)
  const stats = await lstat(pathValue)
  assert(stats.isFile() && !stats.isSymbolicLink(),
    `pinned archive tool is not a regular file: ${name}`)
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

async function runWideProcessProbe(smokeDirectory, tools, sdk) {
  const cmake = Bun.which("cmake")
  const ninja = Bun.which("ninja")
  const compiler = ["cc", "gcc", "clang", "cl"].map((name) =>
    Bun.which(name)).find(Boolean)
  assert(cmake && ninja && compiler,
    "wide probe requires CMake, Ninja, and a C23-capable seed compiler")
  const seedDirectory = resolve(repositoryRoot, "compiler", "seed-c")
  const buildDirectory = join(smokeDirectory, "wide-seed-build")
  const env = { ...process.env, CC: compiler }
  runRequired("wide seed configure", cmake, ["-S", seedDirectory, "-B",
    buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"],
  smokeDirectory, env)
  runRequired("wide seed build", cmake, ["--build", buildDirectory, "--target",
    "w_seed_mlir0_tests", "--parallel", "2"], smokeDirectory, env)
  const unitPath = join(buildDirectory, "w_seed_mlir0_tests.exe")
  const generated = spawn(unitPath,
    ["--emit-process-wide-scalar-helpers-windows"], smokeDirectory)
  assert(generated.exitCode === 0 && generated.stderr.length === 0 &&
    generated.stdout.length > 0 && generated.stdout.toString("utf8")
      .startsWith("// w-seed-mlir0-process-executable-9\n"),
  `wide process artifact generation failed: ${generated.stderr.toString()}`)

  const inputPath = join(smokeDirectory, "wide-process.mlir")
  const verifiedPath = join(smokeDirectory, "wide-process.verified.mlir")
  const llvmPath = join(smokeDirectory, "wide-process.ll")
  const unoptimizedPath = join(smokeDirectory,
    "wide-process.unoptimized.ll")
  const postOptPath = join(smokeDirectory, "wide-process.postopt.ll")
  const unoptimizedObjectPath = join(smokeDirectory,
    "wide-process.unoptimized.obj")
  const objectPath = join(smokeDirectory, "wide-process.obj")
  const unoptimizedExecutablePath = join(smokeDirectory,
    "wide-process.unoptimized.exe")
  const executablePath = join(smokeDirectory, "wide-process.exe")
  await writeFile(inputPath, generated.stdout)
  const wideOptLabel = "wide Windows LLVM opt"
  const wideLlcLabel = "wide Windows llc object"
  const wideLinkLabel = "wide Windows no-CRT link"
  const wideImportsLabel = "wide Windows final imports"
  runRequired("wide Windows mlir-opt", tools["mlir-opt.exe"], [inputPath,
    "-o", verifiedPath, "--convert-scf-to-cf", "--convert-cf-to-llvm",
    "--verify-each"], smokeDirectory)
  runRequired("wide Windows mlir-translate", tools["mlir-translate.exe"], [
    "--mlir-to-llvmir", verifiedPath, "-o", llvmPath,
  ], smokeDirectory)
  runRequired(`${wideOptLabel} unoptimized`, tools["opt.exe"], ["-O0",
    "-verify-each", "-S", llvmPath, "-o", unoptimizedPath], smokeDirectory)
  runRequired(`${wideOptLabel} release O3`, tools["opt.exe"], ["-O3",
    "-verify-each", "-S", llvmPath, "-o", postOptPath], smokeDirectory)
  const unoptimizedText = await readFile(unoptimizedPath, "utf8")
  const optimizedText = await readFile(postOptPath, "utf8")
  const expectedExternals = [
    "ExitProcess", "GetCommandLineW", "GetStdHandle", "WriteFile",
  ]
  const externalDeclarations = (llvmText) => [...llvmText.matchAll(
    /^declare\s+[^\n]*@([A-Za-z0-9_.$-]+)\(/gmu)]
    .map((match) => match[1]).filter((name) => !name.startsWith("llvm.")).sort()
  for (const [label, llvmText] of [
    ["unoptimized", unoptimizedText], ["O3", optimizedText],
  ]) {
    const externals = externalDeclarations(llvmText)
    assert(JSON.stringify(externals) === JSON.stringify(expectedExternals),
      `wide ${label} post-opt externals differ from kernel32 closure: ${externals.join(", ")}`)
  }
  assert(unoptimizedText.includes("define internal i128 @w_fn_0(") &&
    /define internal i128 @w_fn_1\([^)]*i128/u.test(unoptimizedText) &&
    unoptimizedText.includes("call i128 @w_fn_0(") &&
    unoptimizedText.includes("call i128 @w_fn_1(") &&
    unoptimizedText.includes("define void @mainCRTStartup()") &&
    !unoptimizedText.includes("define i128 @mainCRTStartup(") &&
    unoptimizedText.includes("phi i128") &&
    ["eq", "ne", "slt", "sle", "sgt", "sge", "ult", "ule", "ugt",
      "uge"].every((predicate) =>
      unoptimizedText.includes(`icmp ${predicate} i128`)) &&
    ["and", "or", "xor"].every((operation) =>
      unoptimizedText.includes(`${operation} i128`)),
  "wide Windows LLVM translation lost internal helper SSA or i128 operations")
  const products = [
    { label: "unoptimized", llvm: unoptimizedPath,
      object: unoptimizedObjectPath, executable: unoptimizedExecutablePath,
      llcOptimization: "-O0" },
    { label: "O3", llvm: postOptPath, object: objectPath,
      executable: executablePath, llcOptimization: "-O3" },
  ]
  let releaseImports = []
  for (const product of products) {
    runRequired(`${wideLlcLabel} ${product.label}`, tools["llc.exe"], [
      product.llcOptimization, "-filetype=obj",
      "-mtriple=x86_64-pc-windows-msvc", product.llvm, "-o", product.object,
    ], smokeDirectory)
    const defined = runRequired(`wide Windows ${product.label} object symbols`,
      tools["llvm-nm.exe"], ["--format=posix", "--defined-only", product.object],
      smokeDirectory).stdout.toString("utf8")
    assert(/^mainCRTStartup\s+T\s/mu.test(defined),
      `wide Windows ${product.label} object lost the external process root`)
    if (product.label === "unoptimized")
      assert(/^w_fn_0\s+t\s/mu.test(defined) && /^w_fn_1\s+t\s/mu.test(defined),
        "wide Windows unoptimized helpers are not local")
    const undefined = runRequired(
      `wide Windows ${product.label} object undefineds`, tools["llvm-nm.exe"],
      ["--format=posix", "--undefined-only", product.object], smokeDirectory)
      .stdout.toString("utf8")
    const undefinedNames = [...undefined.matchAll(
      /^([A-Za-z0-9_.$?@-]+)\s+U\s/gmu)].map((match) =>
      match[1].replace(/^__imp_/u, "")).sort()
    assert(JSON.stringify(undefinedNames) === JSON.stringify(expectedExternals),
      `wide Windows ${product.label} object undefineds differ from kernel32 closure: ${undefinedNames.join(", ")}`)
    if (product.label === "unoptimized") {
      const relocations = runRequired("wide Windows unoptimized object relocations",
        tools["llvm-objdump.exe"], ["-r", product.object], smokeDirectory)
        .stdout.toString("utf8")
      const disassembly = runRequired("wide Windows unoptimized object disassembly",
        tools["llvm-objdump.exe"], ["-d", product.object], smokeDirectory)
        .stdout.toString("utf8")
      assert(/IMAGE_REL_AMD64_REL32\s+w_fn_0\s*$/mu.test(relocations) &&
        /IMAGE_REL_AMD64_REL32\s+w_fn_1\s*$/mu.test(relocations) &&
        disassembly.includes("mainCRTStartup"),
      "wide Windows unoptimized object lacks helper-call relocations or the external process root")
    }

    runRequired(`${wideLinkLabel} ${product.label}`, tools["lld-link.exe"], [
      "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
      "/machine:x64", `/out:${product.executable}`, product.object, sdk.path,
    ], smokeDirectory)
    const imports = runRequired(`${wideImportsLabel} ${product.label}`,
      tools["llvm-readobj.exe"], ["--coff-imports", product.executable],
      smokeDirectory).stdout.toString("utf8")
    const dllNames = [...imports.matchAll(/Name:\s+([^\r\n]+)/gu)]
      .map((match) => match[1].trim())
      .filter((name) => name.toLowerCase().endsWith(".dll"))
    const importedNames = [...imports.matchAll(/Symbol:\s+([^\r\n(]+)/gu)]
      .map((match) => match[1].trim()).sort()
    assert(dllNames.length > 0 && dllNames.every((name) =>
      name.toLowerCase() === "kernel32.dll") &&
      JSON.stringify(importedNames) === JSON.stringify(expectedExternals),
    `wide Windows ${product.label} imports escaped the exact kernel32 set: ${JSON.stringify({ dllNames, importedNames })}`)
    if (product.label === "O3") releaseImports = importedNames
  }

  for (const args of [[], ["probe"]]) {
    const expected = args.length === 0
      ? { exitCode: 0, stdout: "wide core\n" }
      : { exitCode: 1, stdout: "wide core failure\n" }
    const unoptimizedExecution = spawn(unoptimizedExecutablePath, args,
      smokeDirectory)
    const optimizedExecution = spawn(executablePath, args, smokeDirectory)
    for (const [label, execution] of [
      ["unoptimized", unoptimizedExecution], ["O3", optimizedExecution],
    ])
      assert(execution.exitCode === expected.exitCode && execution.stderr.length === 0 &&
        execution.stdout.equals(Buffer.from(expected.stdout, "utf8")),
      `wide Windows ${label} execution differed for ${args.length} args: exit=${execution.exitCode}, stdout=${JSON.stringify(execution.stdout.toString())}, stderr=${execution.stderr.toString()}`)
    assert(unoptimizedExecution.exitCode === optimizedExecution.exitCode &&
      unoptimizedExecution.stdout.equals(optimizedExecution.stdout) &&
      unoptimizedExecution.stderr.equals(optimizedExecution.stderr),
    `wide Windows observable behavior differs between unoptimized and O3 for ${args.length} args`)
  }
  return {
    objectBytes: (await lstat(objectPath)).size,
    executableBytes: (await lstat(executablePath)).size,
    unoptimizedExecutableBytes:
      (await lstat(unoptimizedExecutablePath)).size,
    imports: releaseImports,
  }
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
  if (options.wide)
    for (const name of ["llvm-nm.exe", "llvm-objdump.exe", "llvm-readobj.exe"])
      tools[name] = await resolveArchiveTool(toolchain, document, name)
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
    runRequired("LLVM opt version probe", tools["opt.exe"], ["--version"], smokeDirectory)
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
    const wideEvidence = options.wide
      ? await runWideProcessProbe(smokeDirectory, tools, sdk)
      : undefined
    const diskAfter = await diskFree(dirname(smokeDirectory))
    const chainObjectSize = (await lstat(chainObjectPath)).size
    const objectSize = (await lstat(objectPath)).size
    const executableSize = (await lstat(executablePath)).size
    console.log(`MLIR0 Windows smoke: passed toolchain=${toolchain} sdk=${sdk.root} sdkVersion=${sdk.version} chainObjectBytes=${chainObjectSize} objectBytes=${objectSize} exeBytes=${executableSize} wideProcess=${wideEvidence === undefined ? "not-run" : JSON.stringify(wideEvidence)} diskFreeBefore=${diskBefore} diskFreeAfter=${diskAfter}`)
  } finally {
    await rm(smokeDirectory, { recursive: true, force: true })
  }
}
