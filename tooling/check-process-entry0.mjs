import { createHash } from "node:crypto"
import { existsSync } from "node:fs"
import { lstat, mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { isAbsolute, join, relative, resolve } from "node:path"

import {
  MATERIALIZED_MANIFEST,
  defaultCacheDirectory,
  validateManifest,
  validateMaterialized,
} from "./acquire-mlir0-windows.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const includeDirectory = resolve(seedDirectory, "include")
const manifestPath = resolve(import.meta.dir, "mlir0-windows-toolchain.json")
const canonicalFixture = resolve(seedDirectory, "fixtures", "process-entry0.w")
const targetTriple = "x86_64-pc-windows-msvc"
const schemaComment = "// w-seed-mlir0-process-handler-1\n"
const gccPath = "C:\\Strawberry\\c\\bin\\gcc.exe"
/* Bun reports the low byte of Windows STATUS_ILLEGAL_INSTRUCTION for a
 * trapped native child on this host: 0xc000001d -> 29. */
const generatedTrapExitCode = 0x1d
const requiredToolNames = Object.freeze([
  "mlir-opt.exe",
  "mlir-translate.exe",
  "llc.exe",
  "lld-link.exe",
])
const commandTimeout = 180_000

function fail(message) {
  throw new Error(`PROCESS-ENTRY0: ${message}`)
}

function assert(condition, message) {
  if (!condition) fail(message)
}

function skip(message) {
  console.log(`PROCESS-ENTRY0: SKIP ${message}`)
  process.exit(0)
}

function contained(parent, candidate) {
  const child = relative(resolve(parent), resolve(candidate))
  return child === "" ||
    (child !== ".." && !child.startsWith("..\\") &&
      !child.startsWith("../") && !isAbsolute(child))
}

function bytesOf(value) {
  return Buffer.from(value === undefined || value === null ? [] : value)
}

function spawn(command, args, cwd = root, env) {
  const options = {
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
    timeout: commandTimeout,
  }
  if (env !== undefined) options.env = env
  let result
  try {
    result = Bun.spawnSync(options)
  } catch (error) {
    fail(`child infrastructure error for ${command}: ${error.message}`)
  }
  if (result.exitedDueToTimeout === true)
    fail(`child timed out for ${command}`)
  if (!Number.isInteger(result.exitCode))
    fail(`child did not return an exit code for ${command}`)
  return {
    ...result,
    stdout: bytesOf(result.stdout),
    stderr: bytesOf(result.stderr),
  }
}

function shortOutput(bytes) {
  const text = bytes.toString().trim()
  return text.length > 3000 ? `…${text.slice(-3000)}` : text
}

function runRequired(label, command, args, cwd = root, env) {
  const result = spawn(command, args, cwd, env)
  if (result.exitCode !== 0) {
    const output = shortOutput(result.stderr) || shortOutput(result.stdout)
    fail(`${label} failed with exit ${String(result.exitCode)}${
      output.length === 0 ? "" : `: ${output}`}`)
  }
  return result
}

async function regularFile(pathValue, label) {
  let stats
  try {
    stats = await lstat(pathValue)
  } catch (error) {
    fail(`${label} is unavailable: ${error.message}`)
  }
  assert(stats.isFile() && !stats.isSymbolicLink(),
    `${label} is not a regular file: ${pathValue}`)
  assert(stats.size > 0, `${label} is empty: ${pathValue}`)
  return stats
}

function assertNoOutput(result, label) {
  assert(result.stdout.length === 0 && result.stderr.length === 0,
    `${label} wrote process output: ${JSON.stringify({
      stdout: result.stdout.toString(),
      stderr: result.stderr.toString(),
    })}`)
}

function assertPeX64(bytes, label) {
  assert(bytes.length >= 0x40 && bytes[0] === 0x4d && bytes[1] === 0x5a,
    `${label} is not a PE image`)
  const peOffset = bytes.readUInt32LE(0x3c)
  assert(peOffset + 6 <= bytes.length &&
    bytes.subarray(peOffset, peOffset + 4).equals(Buffer.from("PE\0\0", "ascii")) &&
    bytes.readUInt16LE(peOffset + 4) === 0x8664,
  `${label} is not a PE x64 image`)
}

function assertCoffX64(bytes, label) {
  assert(bytes.length >= 20 && bytes.readUInt16LE(0) === 0x8664,
    `${label} is not an x64 COFF object`)
}

function verifyHandlerArtifact(bytes, label = "process artifact") {
  const text = bytes.toString("utf8")
  assert(text.startsWith(schemaComment), `${label} has no process schema marker`)
  assert(text.includes(`llvm.target_triple = \"${targetTriple}\"`),
    `${label} has the wrong target triple`)
  assert(text.includes(
    "llvm.func @w_seed_process_entry0_handler(%arguments: !llvm.ptr, %context: !llvm.ptr) -> i32"),
  `${label} has the wrong handler signature`)
  const contextCall = "llvm.call @w_seed_process_entry0_context_drop(%context)"
  const argumentsCall = "llvm.call @w_seed_process_entry0_arguments_drop(%arguments)"
  assert(text.split(contextCall).length - 1 === 1 &&
    text.split(argumentsCall).length - 1 === 1,
  `${label} does not contain exactly one call to each private drop adapter`)
  assert(text.indexOf(contextCall) < text.indexOf(argumentsCall),
    `${label} releases arguments before context`)
  assert(text.includes("llvm.intr.trap") && text.includes("llvm.unreachable") &&
    text.includes("llvm.return %zero : i32"),
  `${label} does not check both adapter statuses before returning`)
  for (const forbidden of [
    "@main",
    "mainCRTStartup",
    "GetStdHandle",
    "WriteFile",
    "ExitProcess",
    "w_seed_process0",
    "root_finalize",
    "entry_invoke",
    "llvm.mlir.global",
  ])
    assert(!text.includes(forbidden), `${label} contains forbidden ${forbidden}`)
  return text
}

function swapCleanupCallLines(text) {
  const contextCall = "llvm.call @w_seed_process_entry0_context_drop(%context)"
  const argumentsCall = "llvm.call @w_seed_process_entry0_arguments_drop(%arguments)"
  assert(text.split(contextCall).length - 1 === 1 &&
    text.split(argumentsCall).length - 1 === 1 &&
    text.indexOf(contextCall) < text.indexOf(argumentsCall),
  "cannot locate cleanup calls for reordered fault trial")
  return text.replace(contextCall, "__PROCESS_ENTRY0_ARGUMENTS_CALL__")
    .replace(argumentsCall, contextCall)
    .replace("__PROCESS_ENTRY0_ARGUMENTS_CALL__", argumentsCall)
}

function sourceWithWrongBody(source) {
  const marker = "return .success"
  assert(source.includes(marker), "canonical fixture has no success body")
  return source.replace(marker, "return .failure")
}

async function runGate(gate, sourcePath, cwd, label) {
  const result = spawn(gate, [sourcePath], cwd)
  assert(result.exitCode === 0 && result.stdout.length > 0 &&
    result.stderr.length === 0,
  `${label} did not emit one successful artifact: ${JSON.stringify({
    exitCode: result.exitCode,
    stdoutBytes: result.stdout.length,
    stderr: result.stderr.toString(),
  })}`)
  return result.stdout
}

async function expectGateReject(gate, sourcePath, cwd, label) {
  const result = spawn(gate, [sourcePath], cwd)
  assert(Number.isInteger(result.exitCode) && result.exitCode === 1 &&
    result.stdout.length === 0,
    `${label} was accepted or emitted an artifact: ${JSON.stringify({
      exitCode: result.exitCode,
      stdoutBytes: result.stdout.length,
      stderr: result.stderr.toString(),
    })}`)
  assert(result.stderr.length === 0, `${label} wrote stderr`)
}

async function compileArtifact(rawBytes, label, tools, workDirectory) {
  const rawPath = join(workDirectory, `${label}.mlir`)
  const verifiedPath = join(workDirectory, `${label}.verified.mlir`)
  const llvmPath = join(workDirectory, `${label}.ll`)
  const objectPath = join(workDirectory, `${label}.obj`)
  await writeFile(rawPath, rawBytes)
  runRequired(`${label} mlir-opt`, tools["mlir-opt.exe"], [
    rawPath, "-o", verifiedPath, "--verify-each",
  ], workDirectory)
  runRequired(`${label} mlir-translate`, tools["mlir-translate.exe"], [
    "--mlir-to-llvmir", verifiedPath, "-o", llvmPath,
  ], workDirectory)
  runRequired(`${label} llc`, tools["llc.exe"], [
    "-filetype=obj", `-mtriple=${targetTriple}`, llvmPath, "-o", objectPath,
  ], workDirectory)
  await regularFile(verifiedPath, `${label} verified MLIR`)
  await regularFile(llvmPath, `${label} LLVM IR`)
  const objectStats = await regularFile(objectPath, `${label} object`)
  const objectBytes = await readFile(objectPath)
  assertCoffX64(objectBytes, `${label} object`)
  return { rawPath, verifiedPath, llvmPath, objectPath, objectStats, objectBytes }
}

function gccFlags() {
  return [
    "-std=c2x",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wconversion",
    "-Wsign-conversion",
    "-Wshadow",
    "-Werror",
  ]
}

function compileC(sourcePath, objectPath, label, cwd) {
  return runRequired(`${label} GCC compile`, gccPath, [
    ...gccFlags(), "-I", includeDirectory, "-c", sourcePath, "-o", objectPath,
  ], cwd)
}

async function linkWithGcc(objects, label, workDirectory) {
  const executablePath = join(workDirectory, `${label}.exe`)
  runRequired(`${label} GCC PE link`, gccPath, [
    "-static", "-static-libgcc", "-o", executablePath, ...objects,
  ], workDirectory)
  const stats = await regularFile(executablePath, `${label} executable`)
  assertPeX64(await readFile(executablePath), `${label} executable`)
  return { executablePath, stats }
}

function expectRuntime(binary, args, label, cwd) {
  const result = spawn(binary, args, cwd)
  assert(Number.isInteger(result.exitCode) && result.exitCode === 0,
    `${label} returned ${String(result.exitCode)}`)
  assertNoOutput(result, label)
}

function runFault(binary, args, environmentName, cwd) {
  const environment = environmentName === undefined ? undefined :
    { ...process.env, W_SEED_PROCESS_ENTRY0_FAULT: environmentName }
  return spawn(binary, args, cwd, environment)
}

function expectHarnessFailure(binary, args, environmentName, label, cwd) {
  const result = runFault(binary, args, environmentName, cwd)
  assert(Number.isInteger(result.exitCode) && result.exitCode === 11,
    `${label} did not expose unreleased owners as harness exit 11: ${JSON.stringify({
      exitCode: result.exitCode,
      stdoutBytes: result.stdout.length,
      stderr: result.stderr.toString(),
    })}`)
  assertNoOutput(result, label)
}

function expectGeneratedTrap(binary, args, environmentName, label, cwd) {
  const result = runFault(binary, args, environmentName, cwd)
  assert(Number.isInteger(result.exitCode) &&
    result.exitCode === generatedTrapExitCode,
    `${label} did not reach the generated trap: ${JSON.stringify({
      exitCode: result.exitCode,
      stdoutBytes: result.stdout.length,
      stderr: result.stderr.toString(),
    })}`)
  assertNoOutput(result, label)
}

async function main() {
  if (process.platform !== "win32" || process.arch !== "x64")
    skip(`native gate requires Windows x86_64, got ${process.platform}/${process.arch}`)
  if (!existsSync(gccPath)) skip(`private GCC compiler is unavailable: ${gccPath}`)
  const cmake = Bun.which("cmake")
  const ninja = Bun.which("ninja")
  if (!cmake || !ninja)
    skip("CMake and Ninja are required for the existing native gate build")

  const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
  const manifestErrors = validateManifest(manifest)
  assert(manifestErrors.length === 0, manifestErrors.join("; "))
  const manifestHash = createHash("sha256")
    .update(await readFile(manifestPath)).digest("hex")
  const cacheDirectory = defaultCacheDirectory()
  assert(!contained(root, cacheDirectory),
    "the MLIR materialization must remain outside the repository")
  const materializedManifestPath = join(cacheDirectory, MATERIALIZED_MANIFEST)
  if (!existsSync(materializedManifestPath))
    skip(`materialized MLIR cache is unavailable: ${cacheDirectory}`)
  const materialized = await validateMaterialized(
    cacheDirectory, manifest, manifestHash)
  const tools = {}
  for (const name of requiredToolNames) {
    const record = materialized.tools?.[name]
    assert(record !== null && typeof record?.relativePath === "string",
      `validated materialization has no required ${name}`)
    const pathValue = resolve(cacheDirectory, record.relativePath)
    assert(contained(cacheDirectory, pathValue), `${name} escapes the cache`)
    await regularFile(pathValue, `${name} in materialized cache`)
    tools[name] = pathValue
    runRequired(`${name} version`, pathValue, ["--version"])
  }

  const compilerVersion = runRequired("GCC version", gccPath, ["--version"])
  assert(/\b13\.2(?:\.\d+)?\b/u.test(compilerVersion.stdout.toString()),
    `unexpected private GCC version: ${shortOutput(compilerVersion.stdout)}`)
  const compilerTarget = runRequired("GCC target", gccPath, ["-dumpmachine"])
    .stdout.toString().trim()
  assert(compilerTarget === "x86_64-w64-mingw32",
    `private compiler is not the verified x64 Windows GCC target: ${compilerTarget}`)

  const canonicalSource = await readFile(canonicalFixture, "utf8")
  const repositoryBuildDirectory = resolve(root, "build")
  if (!existsSync(join(repositoryBuildDirectory, "build.ninja")))
    skip("the existing configured build/build.ninja is unavailable")
  const buildNinja = await readFile(join(repositoryBuildDirectory, "build.ninja"), "utf8")
  assert(buildNinja.includes("w_seed_process_entry0_gate"),
    "the existing build has no process-entry gate target; regenerate it before running this check")
  const workDirectory = await mkdtemp(join(tmpdir(), "w-process-entry0-"))
  const workName = workDirectory.slice(workDirectory.lastIndexOf("\\") + 1)
  assert(workName.startsWith("w-process-entry0-"),
    `temporary gate directory has an unexpected name: ${workDirectory}`)
  assert(contained(tmpdir(), workDirectory),
    `temporary gate directory escaped the system temporary directory: ${workDirectory}`)
  const workStats = await lstat(workDirectory)
  assert(workStats.isDirectory() && !workStats.isSymbolicLink(),
    `temporary gate directory is not a private regular directory: ${workDirectory}`)

  let bodyError
  try {
    runRequired("process gate build", cmake, [
      "--build", repositoryBuildDirectory, "--target",
      "w_seed_process_entry0_gate", "--", "-j", "2",
    ], root)
    const gate = join(repositoryBuildDirectory, "w_seed_process_entry0_gate.exe")
    await regularFile(gate, "process gate executable")

    const canonicalArtifact = await runGate(
      gate, canonicalFixture, root, "canonical process fixture")
    verifyHandlerArtifact(canonicalArtifact, "canonical process artifact")
    const aliasPath = join(workDirectory, "process-entry0-alias.w")
    const aliasSource =
      "// aliases and trivia must not change the private handler artifact\n" +
      "import { Arguments as A, Context as C, ExitCode as E } from std.process\n" +
      "\n" +
      "async fn renamed(args: A, ctx: C): E {\n" +
      "  // body is intentionally the same .success witness\n" +
      "  return .success\n" +
      "}\n" +
      "\n" +
      "entry(renamed)\n"
    await writeFile(aliasPath, aliasSource, "utf8")
    const aliasArtifact = await runGate(
      gate, aliasPath, root, "alias/trivia process fixture")
    verifyHandlerArtifact(aliasArtifact, "alias/trivia process artifact")
    assert(aliasArtifact.equals(canonicalArtifact),
      "aliases/trivia changed the process handler artifact")

    await writeFile(join(workDirectory, "process-entry0-sync.w"),
      canonicalSource.replace("async fn run", "fn run"), "utf8")
    await expectGateReject(gate, join(workDirectory, "process-entry0-sync.w"),
      root, "synchronous process body")
    await writeFile(join(workDirectory, "process-entry0-failure.w"),
      sourceWithWrongBody(canonicalSource), "utf8")
    await expectGateReject(gate,
      join(workDirectory, "process-entry0-failure.w"), root,
      "non-success process body")
    await writeFile(join(workDirectory, "process-entry0-no-entry.w"),
      canonicalSource.replace("entry(run)\n", ""), "utf8")
    await expectGateReject(gate,
      join(workDirectory, "process-entry0-no-entry.w"), root,
      "missing process entry")

    const harnessObject = join(workDirectory, "process-entry0-harness.o")
    const providerObject = join(workDirectory, "process-entry0-provider.o")
    compileC(resolve(seedDirectory, "tests", "process_entry0_harness.c"),
      harnessObject, "process entry harness", root)
    compileC(resolve(seedDirectory, "src", "w_seed_process0.c"),
      providerObject, "PROCESS0 provider", root)
    const canonicalObjects = await compileArtifact(
      canonicalArtifact, "process-entry0", tools, workDirectory)
    const linked = await linkWithGcc([
      canonicalObjects.objectPath, harnessObject, providerObject,
    ], "process-entry0", workDirectory)
    expectRuntime(linked.executablePath, [], "empty selected runtime vector", workDirectory)
    expectRuntime(linked.executablePath, ["alpha", "payload"],
      "nonempty selected runtime vector", workDirectory)

    const harnessFailureCases = [
      ["missing", "missing cleanup adapter forwarding"],
      ["noop-success", "no-op success cleanup adapters"],
    ]
    for (const [fault, label] of harnessFailureCases)
      expectHarnessFailure(linked.executablePath, [], fault, label, workDirectory)
    const trapCases = [
      ["stale-generation", "stale owner generation"],
      ["reversed-arguments", "reversed handler arguments"],
      ["wrong-context", "nonzero context adapter status"],
      ["wrong-arguments", "nonzero arguments adapter status"],
    ]
    for (const [fault, label] of trapCases)
      expectGeneratedTrap(linked.executablePath, [], fault, label, workDirectory)

    const reorderedArtifact = Buffer.from(swapCleanupCallLines(
      canonicalArtifact.toString("utf8")), "utf8")
    const reorderedObjects = await compileArtifact(
      reorderedArtifact, "process-entry0-reordered", tools, workDirectory)
    const reorderedLinked = await linkWithGcc([
      reorderedObjects.objectPath, harnessObject, providerObject,
    ], "process-entry0-reordered", workDirectory)
    expectGeneratedTrap(reorderedLinked.executablePath, [], undefined,
      "reordered generated cleanup", workDirectory)

    console.log(`PROCESS-ENTRY0: passed source-to-HIR16-to-MLIR-to-LLVM-COFF-to-GCC ` +
      `handler execution compiler=${gccPath} target=${compilerTarget} ` +
      `toolchain=23.1.0 cases=empty,nonempty,missing,noop-success,` +
      "stale-generation,reversed-arguments,wrong-context,wrong-arguments,reordered")
  } catch (error) {
    bodyError = error
  }
  let cleanupError
  try {
    const cleanupStats = await lstat(workDirectory)
    assert(cleanupStats.isDirectory() && !cleanupStats.isSymbolicLink(),
      `temporary gate directory changed before cleanup: ${workDirectory}`)
    await rm(workDirectory, { recursive: true, force: false })
    assert(!existsSync(workDirectory),
      `temporary gate directory remains after cleanup: ${workDirectory}`)
  } catch (error) {
    cleanupError = error
  }
  if (bodyError !== undefined && cleanupError !== undefined)
    throw new Error(`${bodyError.message}; cleanup also failed: ${cleanupError.message}`)
  if (bodyError !== undefined) throw bodyError
  if (cleanupError !== undefined) throw cleanupError
}

if (import.meta.main) {
  main().catch((error) => {
    console.error(error.message)
    process.exitCode = 1
  })
}
