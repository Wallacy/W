import { createHash } from "node:crypto"
import { readFileSync } from "node:fs"
import {
  copyFile,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rename,
  rm,
  writeFile,
} from "node:fs/promises"
import { tmpdir } from "node:os"
import {
  dirname,
  isAbsolute,
  join,
  relative,
  resolve,
  sep,
} from "node:path"
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
const binaryName = "w.exe"
const receiptName = "receipt.json"
const binaryPath = join(outputDirectory, binaryName)
const receiptPath = join(outputDirectory, receiptName)
const WINDOWS_REQUIRED_TOOL_NAMES = Object.freeze([
  "mlir-opt.exe",
  "mlir-translate.exe",
  "llc.exe",
  "lld-link.exe",
])

export const RECEIPT_SCHEMA = "w-seed-windows-build-receipt-1"
export const RECEIPT_NAME = receiptName
export const PROFILE_RECIPES = Object.freeze({
  development: Object.freeze({
    cmakeBuildType: "Debug",
    purpose: "toolchain-iteration-and-diagnostics",
    reproducible: false,
  }),
  release: Object.freeze({
    cmakeBuildType: "Release",
    purpose: "performance-first",
    reproducible: false,
  }),
  benchmark: Object.freeze({
    cmakeBuildType: "Release",
    purpose: "reproducible-pinned",
    reproducible: true,
  }),
  "size-experimental": Object.freeze({
    cmakeBuildType: "MinSizeRel",
    purpose: "size-comparison-only",
    reproducible: false,
  }),
})
export const SMOKE_CASES = Object.freeze([
  Object.freeze({
    id: "hello",
    fixture: "compiler/seed-c/fixtures/hlo0-hello.w",
    absoluteFixture: resolve(seedDirectory, "fixtures", "hlo0-hello.w"),
    expectedStdout: "Hello, world!\n",
  }),
  Object.freeze({
    id: "restaurant",
    fixture: "compiler/seed-c/fixtures/restaurant-if.w",
    absoluteFixture: resolve(seedDirectory, "fixtures", "restaurant-if.w"),
    expectedStdout: "Kitchen open\nAfter service\nKitchen closed\nAfter service\n",
  }),
])

function fail(message) {
  throw new Error(`W Windows build: ${message}`)
}

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value)
}

function hasExactKeys(value, keys) {
  return isObject(value) && JSON.stringify(Object.keys(value).sort()) ===
    JSON.stringify([...keys].sort())
}

function hasCanonicalKeys(value, keys) {
  return hasExactKeys(value, keys) && JSON.stringify(Object.keys(value)) ===
    JSON.stringify([...keys])
}

function isContained(parent, candidate) {
  const pathRelative = relative(resolve(parent), resolve(candidate))
  return pathRelative === "" || (pathRelative !== ".." &&
    !pathRelative.startsWith(`..${sep}`) && !pathRelative.startsWith("../") &&
    !pathRelative.includes(":") && !pathRelative.startsWith("/"))
}

function isReparsePoint(stats) {
  return stats.isSymbolicLink() || (stats.mode & 0xf000) === 0xa000
}

async function physicalDirectory(pathValue, label) {
  let stats
  try {
    stats = await lstat(pathValue)
  } catch (error) {
    if (error?.code === "ENOENT") return false
    throw error
  }
  if (!stats.isDirectory() || isReparsePoint(stats))
    fail(`${label} is not a physical directory`)
  return true
}

function requireCommand(name) {
  const command = Bun.which(name)
  if (command === null) fail(`${name} is unavailable; install it before running the offline build`)
  return command
}

function outputText(result) {
  return `${Buffer.from(result.stderr ?? "").toString()}${Buffer.from(result.stdout ?? "").toString()}`
    .trim()
}

function runWithVisualStudioChecked(label, vsDevCmd, command, args) {
  const result = runWithVisualStudio(vsDevCmd, command, args, { cwd: root })
  if (result.exitCode !== 0) {
    const output = outputText(result).slice(-2000)
    fail(`${label} failed${output.length > 0 ? `: ${output}` : ""}`)
  }
  return result
}

export function parseBuildArguments(argv) {
  if (!Array.isArray(argv)) throw new Error("build arguments must be an array")
  let profile
  let c11Recovery = false
  let help = false

  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index]
    if (argument === "--profile") {
      if (profile !== undefined) throw new Error("--profile may be used only once")
      const value = argv[index + 1]
      if (typeof value !== "string" || value.length === 0 || value.startsWith("--"))
        throw new Error("--profile requires exactly one value")
      index += 1
      if (!Object.hasOwn(PROFILE_RECIPES, value))
        throw new Error(`unknown --profile value: ${value}`)
      profile = value
    } else if (argument === "--c11-recovery") {
      if (c11Recovery) throw new Error("--c11-recovery may be used only once")
      c11Recovery = true
    } else if (argument === "--help") {
      if (help) throw new Error("--help may be used only once")
      help = true
    } else {
      throw new Error(`unknown option: ${String(argument)}`)
    }
  }

  const selectedProfile = profile ?? "release"
  return {
    help,
    profile: selectedProfile,
    selectedProfile,
    c11Recovery,
    cStandard: c11Recovery ? "11" : "23",
  }
}

export function profileRecipe(profile) {
  if (!Object.hasOwn(PROFILE_RECIPES, profile))
    throw new Error(`unknown toolchain profile: ${profile}`)
  const recipe = PROFILE_RECIPES[profile]
  return { profile, ...recipe }
}

function gitResult(args, cwd = root) {
  const git = Bun.which("git")
  if (git === null) fail("git is unavailable; a source identity is required for the receipt")
  return Bun.spawnSync({
    cmd: [git, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
  })
}

export function readGitState(cwd = root) {
  const headResult = gitResult(["rev-parse", "--verify", "HEAD"], cwd)
  if (headResult.exitCode !== 0)
    fail(`cannot read Git HEAD: ${outputText(headResult).slice(-1000)}`)
  const head = Buffer.from(headResult.stdout).toString("utf8").trim()
  if (!/^[0-9a-f]{7,64}$/u.test(head)) fail("Git HEAD is not a hexadecimal commit identity")

  const statusResult = gitResult(["status", "--porcelain=v1", "--untracked-files=all"], cwd)
  if (statusResult.exitCode !== 0)
    fail(`cannot read Git status: ${outputText(statusResult).slice(-1000)}`)
  return {
    head,
    dirty: Buffer.from(statusResult.stdout).toString("utf8").length !== 0,
  }
}

export function assertStableGitHead(startingState, endingState) {
  if (startingState?.head !== endingState?.head)
    throw new Error("source HEAD changed during build")
}

async function hashFile(pathValue) {
  const bytes = await readFile(pathValue)
  return {
    sizeBytes: bytes.byteLength,
    sha256: createHash("sha256").update(bytes).digest("hex"),
  }
}

export function parseMsvcCompilerVersion(output) {
  if (typeof output !== "string") throw new Error("MSVC compiler probe output must be text")
  const candidates = [...output.matchAll(/\b((?:1[0-9])(?:\.[0-9]+){2,3})\b/gu)]
    .map((match) => match[1])
    .sort((left, right) => right.split(".").length - left.split(".").length)
  if (candidates.length > 0) return candidates[0]
  const labeled = output.match(/(?:Compiler\s+Version|Version)\s+((?:[0-9]+\.){2,3}[0-9]+)/iu)
  if (labeled === null) throw new Error("MSVC compiler probe did not report a version")
  return labeled[1]
}

function probeCompilerIdentity(vsDevCmd) {
  const result = runWithVisualStudio(vsDevCmd, "cl.exe", ["/Bv"], { cwd: root })
  const output = outputText(result)
  let version
  try {
    version = parseMsvcCompilerVersion(output)
  } catch (error) {
    fail(`${error.message}: ${output.slice(-1000)}`)
  }
  return {
    identity: "Microsoft Visual C++ compiler",
    executable: "cl.exe",
    version,
  }
}

async function probeMsvcReproducibility(vsDevCmd, probeDirectory) {
  const sourcePath = join(probeDirectory, "w-repro-probe.c")
  const objectPath = join(probeDirectory, "w-repro-probe.obj")
  const executablePath = join(probeDirectory, "w-repro-probe.exe")
  await writeFile(sourcePath, "int mainCRTStartup(void) { return 0; }\n", "utf8")
  const compilerFlags = [
    "/nologo",
    "/Brepro",
    `/pathmap:${root}=W`,
  ]
  const compilerResult = runWithVisualStudio(vsDevCmd, "cl.exe", [
    ...compilerFlags,
    "/c",
    `/Fo${objectPath}`,
    sourcePath,
  ], { cwd: probeDirectory })
  if (compilerResult.exitCode !== 0)
    fail(`benchmark reproducibility compiler probe failed: ${outputText(compilerResult).slice(-1500)}`)

  const linkerFlags = [
    "/nologo",
    "/Brepro",
    `/out:${executablePath}`,
    "/entry:mainCRTStartup",
    "/subsystem:console",
    "/nodefaultlib",
    objectPath,
  ]
  const linkerResult = runWithVisualStudio(vsDevCmd, "link.exe", linkerFlags,
    { cwd: probeDirectory })
  if (linkerResult.exitCode !== 0)
    fail(`benchmark reproducibility linker probe failed: ${outputText(linkerResult).slice(-1500)}`)

  return {
    required: true,
    compilerFlags: ["/Brepro", "/pathmap:<workspace>=W"],
    linkerFlags: ["/Brepro"],
    pathMapping: "workspace-source-to-W",
    probes: { compiler: "passed", linker: "passed" },
  }
}

function emptyReproducibilityRecipe() {
  return {
    required: false,
    compilerFlags: [],
    linkerFlags: [],
    pathMapping: null,
    probes: null,
  }
}

function cmakeProfileArguments(recipe, buildDirectory) {
  const args = [
    `-DCMAKE_BUILD_TYPE=${recipe.cmakeBuildType}`,
  ]
  if (recipe.reproducible) {
    args.push(`-DCMAKE_C_FLAGS=/Brepro;/pathmap:${root}=W;/pathmap:${buildDirectory}=B`)
    args.push("-DCMAKE_EXE_LINKER_FLAGS=/Brepro")
  }
  return args
}

function toolchainReceiptIdentity(manifest, materialized, manifestSha256) {
  const requiredTools = {}
  for (const name of manifest.tools.required) {
    const record = materialized.tools?.[name]
    if (!isObject(record) || typeof record.sha256 !== "string" ||
        !Number.isSafeInteger(record.sizeBytes) || typeof record.version !== "string")
      fail(`validated materialization has no complete identity for ${name}`)
    requiredTools[name] = {
      version: record.version,
      sizeBytes: record.sizeBytes,
      sha256: record.sha256,
    }
  }
  return {
    asset: {
      provider: manifest.asset.provider,
      release: manifest.asset.release,
      fileName: manifest.asset.fileName,
      targetTriple: manifest.asset.targetTriple,
      sizeBytes: manifest.asset.sizeBytes,
      sha256: manifest.asset.sha256,
    },
    llvm: {
      tag: manifest.asset.llvmTag,
      version: manifest.toolchain.llvm,
      digest: manifest.asset.sha256,
    },
    manifestSha256,
    requiredTools,
  }
}

function smokeRecord(smokeCase, result) {
  const stdout = Buffer.from(result.stdout)
  const stderr = Buffer.from(result.stderr)
  return {
    id: smokeCase.id,
    fixture: smokeCase.fixture,
    fixtureSha256: smokeCase.fixtureSha256,
    outcome: "pass",
    exitCode: result.exitCode,
    stdoutBytes: stdout.byteLength,
    stdoutSha256: createHash("sha256").update(stdout).digest("hex"),
    stderrBytes: stderr.byteLength,
  }
}

async function hashSmokeFixtures() {
  return Promise.all(SMOKE_CASES.map(async (smokeCase) => ({
    ...smokeCase,
    fixtureSha256: (await hashFile(smokeCase.absoluteFixture)).sha256,
  })))
}

function expectProgram(executablePath, smokeCase) {
  const result = Bun.spawnSync({
    cmd: [executablePath, "run", smokeCase.absoluteFixture],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
  })
  const stdout = Buffer.from(result.stdout)
  const stderr = Buffer.from(result.stderr)
  if (result.exitCode !== 0 || !stdout.equals(Buffer.from(smokeCase.expectedStdout, "utf8")) ||
      stderr.length !== 0) {
    fail(`${smokeCase.id} smoke was not exact: ${JSON.stringify({
      exitCode: result.exitCode,
      stdout: stdout.toString(),
      stderr: stderr.toString(),
    })}`)
  }
  return smokeRecord(smokeCase, result)
}

function hasAbsolutePathString(value) {
  return isAbsolute(value) || /^[A-Za-z]:[\\/]/u.test(value) || value.startsWith("\\\\")
}

function receiptContainsAbsolutePath(value) {
  if (typeof value === "string") return hasAbsolutePathString(value)
  if (Array.isArray(value)) return value.some((item) => receiptContainsAbsolutePath(item))
  if (isObject(value)) return Object.values(value).some((item) => receiptContainsAbsolutePath(item))
  return false
}

export function validateReceipt(receipt) {
  const errors = []
  const add = (condition, message) => { if (!condition) errors.push(message) }
  add(isObject(receipt), "receipt must be an object")
  if (!isObject(receipt)) return errors
  add(receipt.$schema === RECEIPT_SCHEMA, "receipt schema is invalid")
  add(receipt.version === 1, "receipt version must be 1")
  add(receipt.status === "local-evidence-only", "receipt status must be local-evidence-only")
  add(hasExactKeys(receipt, [
    "$schema", "version", "status", "claimBoundary", "profile", "cStandard", "cLane",
    "source", "toolchain", "windowsSdk", "compiler", "reproducibility", "artifact", "smoke", "runner",
  ]), "receipt keys are invalid")
  add(hasExactKeys(receipt.claimBoundary, ["package", "budget", "performance", "statement"]) &&
    receipt.claimBoundary.package === false &&
    receipt.claimBoundary.budget === false && receipt.claimBoundary.performance === false &&
    receipt.claimBoundary.statement ===
      "This receipt is not a package, budget, or performance proof.",
  "receipt claim boundary is invalid")

  const selectedProfile = receipt.profile?.selected
  const recipe = Object.hasOwn(PROFILE_RECIPES, selectedProfile)
    ? PROFILE_RECIPES[selectedProfile]
    : undefined
  add(recipe !== undefined, "receipt selected profile is invalid")
  add(hasExactKeys(receipt.profile, ["selected", "cmakeBuildType"]), "receipt profile keys are invalid")
  if (recipe !== undefined)
    add(receipt.profile.cmakeBuildType === recipe.cmakeBuildType,
      "receipt CMake build type does not match the selected profile")
  add(receipt.cStandard === "23" || receipt.cStandard === "11",
    "receipt C standard is invalid")
  add(receipt.cLane === (receipt.cStandard === "11" ? "c11-recovery" : "c23-primary"),
    "receipt C lane is invalid")

  add(hasExactKeys(receipt.source, ["head", "dirty"]) &&
    /^[0-9a-f]{7,64}$/u.test(receipt.source.head ?? "") &&
    typeof receipt.source.dirty === "boolean", "receipt source identity is invalid")
  const toolchain = receipt.toolchain
  const asset = toolchain?.asset
  const llvm = toolchain?.llvm
  const requiredTools = toolchain?.requiredTools
  add(hasExactKeys(toolchain, ["asset", "llvm", "manifestSha256", "requiredTools"]),
    "receipt toolchain keys are invalid")
  add(hasExactKeys(asset,
    ["provider", "release", "fileName", "targetTriple", "sizeBytes", "sha256"]) &&
    [asset?.provider, asset?.release, asset?.fileName, asset?.targetTriple]
      .every((value) => typeof value === "string" && value.length > 0) &&
    Number.isSafeInteger(asset?.sizeBytes) && asset.sizeBytes > 0 &&
    typeof asset?.sha256 === "string" && /^[0-9a-f]{64}$/u.test(asset.sha256),
  "receipt asset identity is invalid")
  add(hasExactKeys(llvm, ["tag", "version", "digest"]) &&
    typeof llvm?.tag === "string" && llvm.tag.length > 0 &&
    typeof llvm?.version === "string" && llvm.version.length > 0 &&
    typeof llvm?.digest === "string" && /^[0-9a-f]{64}$/u.test(llvm.digest) &&
    llvm.digest === asset?.sha256,
  "receipt LLVM identity is invalid")
  add(typeof toolchain?.manifestSha256 === "string" &&
    /^[0-9a-f]{64}$/u.test(toolchain.manifestSha256),
  "receipt manifest hash is invalid")
  add(hasCanonicalKeys(requiredTools, WINDOWS_REQUIRED_TOOL_NAMES) &&
    WINDOWS_REQUIRED_TOOL_NAMES.every((name) => {
      const tool = requiredTools?.[name]
      return hasCanonicalKeys(tool, ["version", "sizeBytes", "sha256"]) &&
        typeof tool.version === "string" && tool.version.length > 0 &&
        tool.version === llvm?.version && Number.isSafeInteger(tool.sizeBytes) &&
        tool.sizeBytes > 0 && typeof tool.sha256 === "string" &&
        /^[0-9a-f]{64}$/u.test(tool.sha256)
    }),
  "receipt required tool identities are invalid")
  add(hasExactKeys(receipt.windowsSdk, ["version"]) && typeof receipt.windowsSdk.version === "string" &&
    receipt.windowsSdk.version.length > 0, "receipt Windows SDK identity is invalid")
  add(hasExactKeys(receipt.compiler, ["identity", "executable", "version"]) &&
    receipt.compiler.identity === "Microsoft Visual C++ compiler" &&
    receipt.compiler.executable === "cl.exe" &&
    /^\d+\.\d+\.\d+(?:\.\d+)?$/u.test(receipt.compiler.version ?? ""),
  "receipt compiler identity is invalid")

  const repro = receipt.reproducibility
  add(hasExactKeys(repro, ["required", "compilerFlags", "linkerFlags", "pathMapping", "probes"]) &&
    typeof repro.required === "boolean" &&
    Array.isArray(repro.compilerFlags) && Array.isArray(repro.linkerFlags),
  "receipt reproducibility recipe is invalid")
  if (selectedProfile === "benchmark" && isObject(repro)) {
    add(receipt.source?.dirty === false,
      "benchmark receipt must record a clean source tree")
    add(repro.required === true &&
      JSON.stringify(repro.compilerFlags) === JSON.stringify([
        "/Brepro", "/pathmap:<workspace>=W",
      ]) && JSON.stringify(repro.linkerFlags) === JSON.stringify(["/Brepro"]) &&
      repro.pathMapping === "workspace-source-to-W" &&
      hasCanonicalKeys(repro.probes, ["compiler", "linker"]) &&
      repro.probes.compiler === "passed" && repro.probes.linker === "passed",
    "benchmark receipt does not prove its reproducibility recipe")
  } else if (selectedProfile !== "benchmark" && isObject(repro)) {
    add(repro.required === false && repro.compilerFlags.length === 0 &&
      repro.linkerFlags.length === 0 && repro.pathMapping === null &&
      repro.probes === null,
    "nonbenchmark receipt contains a reproducibility recipe")
  }

  add(hasExactKeys(receipt.artifact, ["name", "byteSize", "sha256"]) &&
    receipt.artifact.name === binaryName &&
    Number.isSafeInteger(receipt.artifact.byteSize) && receipt.artifact.byteSize > 0 &&
    /^[0-9a-f]{64}$/u.test(receipt.artifact.sha256 ?? ""),
  "receipt artifact identity is invalid")
  add(Array.isArray(receipt.smoke) && receipt.smoke.length === SMOKE_CASES.length,
    "receipt smoke records are incomplete")
  if (Array.isArray(receipt.smoke)) {
    const smokeIds = receipt.smoke.map((item) => item?.id)
    add(JSON.stringify(smokeIds) === JSON.stringify(SMOKE_CASES.map((item) => item.id)),
      "receipt smoke identities are invalid")
    for (const [index, item] of receipt.smoke.entries()) {
      const smokeCase = SMOKE_CASES[index]
    add(isObject(item) && item.outcome === "pass" && item.exitCode === 0 &&
        hasExactKeys(item, ["id", "fixture", "fixtureSha256", "outcome", "exitCode", "stdoutBytes", "stdoutSha256", "stderrBytes"]) &&
        typeof item.fixture === "string" && Number.isSafeInteger(item.stdoutBytes) &&
        typeof item.fixtureSha256 === "string" && /^[0-9a-f]{64}$/u.test(item.fixtureSha256) &&
        item.stdoutBytes > 0 && /^[0-9a-f]{64}$/u.test(item.stdoutSha256 ?? "") &&
        item.stderrBytes === 0 && item.id === smokeCase?.id &&
        item.fixture === smokeCase?.fixture &&
        item.fixtureSha256 === (() => {
          try {
            return createHash("sha256").update(readFileSync(smokeCase.absoluteFixture)).digest("hex")
          } catch {
            return ""
          }
        })() &&
        item.stdoutBytes === Buffer.byteLength(smokeCase?.expectedStdout ?? "", "utf8") &&
        item.stdoutSha256 === createHash("sha256")
          .update(Buffer.from(smokeCase?.expectedStdout ?? "", "utf8"))
          .digest("hex"),
      `receipt smoke outcome is invalid for ${item?.id ?? "unknown"}`)
    }
  }
  add(receipt.runner === "staged-w.exe", "receipt runner identity is invalid")
  add(!receiptContainsAbsolutePath(receipt), "receipt must not contain absolute paths")
  return errors
}

export function serializeReceipt(receipt) {
  const errors = validateReceipt(receipt)
  if (errors.length > 0) throw new Error(`invalid receipt: ${errors.join("; ")}`)
  return `${JSON.stringify(receipt, null, 2)}\n`
}

function makeReceipt({
  profile,
  cStandard,
  gitState,
  manifest,
  materialized,
  manifestSha256,
  sdk,
  compiler,
  artifact,
  smoke,
  reproducibility,
}) {
  const recipe = profileRecipe(profile)
  return {
    $schema: RECEIPT_SCHEMA,
    version: 1,
    status: "local-evidence-only",
    claimBoundary: {
      package: false,
      budget: false,
      performance: false,
      statement: "This receipt is not a package, budget, or performance proof.",
    },
    profile: {
      selected: profile,
      cmakeBuildType: recipe.cmakeBuildType,
    },
    cStandard,
    cLane: cStandard === "11" ? "c11-recovery" : "c23-primary",
    source: {
      head: gitState.head,
      dirty: gitState.dirty,
    },
    toolchain: toolchainReceiptIdentity(manifest, materialized, manifestSha256),
    windowsSdk: {
      version: sdk.version,
    },
    compiler,
    reproducibility,
    artifact: {
      name: binaryName,
      byteSize: artifact.sizeBytes,
      sha256: artifact.sha256,
    },
    smoke,
    runner: "staged-w.exe",
  }
}

export function createBuildReceipt(input) {
  return makeReceipt(input)
}

async function writeAndValidateReceipt(pathValue, receipt) {
  const serialized = serializeReceipt(receipt)
  await writeFile(pathValue, serialized, "utf8")
  const actual = await readFile(pathValue, "utf8")
  if (actual !== serialized) fail("receipt bytes are not deterministic")
  let parsed
  try {
    parsed = JSON.parse(actual)
  } catch (error) {
    fail(`receipt is not valid JSON: ${error.message}`)
  }
  const errors = validateReceipt(parsed)
  if (errors.length > 0) fail(errors.join("; "))
}

export async function validateOutputDirectory(directory) {
  const entries = (await readdir(directory)).sort()
  const expected = [binaryName, receiptName].sort()
  if (JSON.stringify(entries) !== JSON.stringify(expected))
    fail(`persistent output must contain only ${binaryName} and ${receiptName}, found: ${entries.join(", ")}`)
  for (const entry of expected) {
    const pathValue = join(directory, entry)
    const stats = await lstat(pathValue)
    if (!stats.isFile() || isReparsePoint(stats))
      fail(`output entry is not a regular file: ${entry}`)
  }
  const receiptSource = await readFile(join(directory, receiptName), "utf8")
  let receipt
  try {
    receipt = JSON.parse(receiptSource)
  } catch (error) {
    fail(`output receipt is not valid JSON: ${error.message}`)
  }
  const errors = validateReceipt(receipt)
  if (errors.length > 0) fail(errors.join("; "))
  if (serializeReceipt(receipt) !== receiptSource)
    fail("output receipt is not deterministic")
  const artifact = await hashFile(join(directory, binaryName))
  if (receipt.artifact?.byteSize !== artifact.sizeBytes || receipt.artifact?.sha256 !== artifact.sha256)
    fail("output w.exe does not match the receipt artifact identity")
  return receipt
}

export async function stageBinary({ builtBinary, stageParent }) {
  if (!await physicalDirectory(dirname(stageParent), "stage parent") &&
      dirname(stageParent) !== stageParent)
    await mkdir(dirname(stageParent), { recursive: true })
  await mkdir(stageParent, { recursive: true })
  const stageDirectory = await mkdtemp(join(stageParent, ".w-windows-stage-"))
  try {
    const builtStats = await lstat(builtBinary)
    if (!builtStats.isFile() || isReparsePoint(builtStats) || builtStats.size === 0)
      fail("built w.exe is not a non-empty regular file")
    await copyFile(builtBinary, join(stageDirectory, binaryName))
    return stageDirectory
  } catch (error) {
    await rm(stageDirectory, { recursive: true, force: true })
    throw error
  }
}

export async function stageOutput({ builtBinary, stageParent, receipt }) {
  const stageDirectory = await stageBinary({ builtBinary, stageParent })
  try {
    await writeAndValidateReceipt(join(stageDirectory, receiptName), receipt)
    await validateOutputDirectory(stageDirectory)
    return stageDirectory
  } catch (error) {
    await rm(stageDirectory, { recursive: true, force: true })
    throw error
  }
}

async function uniqueBackupPath(parent) {
  const prefix = join(parent, `.w-windows-backup-${process.pid}-${Date.now().toString(36)}`)
  let candidate = prefix
  for (let index = 0; index < 100; index += 1) {
    try {
      await lstat(candidate)
      candidate = `${prefix}-${index + 1}`
    } catch (error) {
      if (error?.code === "ENOENT") return candidate
      throw error
    }
  }
  fail("cannot reserve a recoverable output backup path")
}

export async function atomicInstallOutput(stageDirectory, targetDirectory) {
  await validateOutputDirectory(stageDirectory)
  const parent = dirname(targetDirectory)
  await mkdir(parent, { recursive: true })
  const targetExists = await physicalDirectory(targetDirectory, "existing output")
  const backupDirectory = targetExists ? await uniqueBackupPath(parent) : null
  if (targetExists) await rename(targetDirectory, backupDirectory)
  try {
    await rename(stageDirectory, targetDirectory)
  } catch (error) {
    if (backupDirectory !== null) {
      try {
        await rename(backupDirectory, targetDirectory)
      } catch (restoreError) {
        throw new Error(`output swap failed and restore failed: ${error.message}; ${restoreError.message}`)
      }
    }
    throw error
  }

  if (backupDirectory !== null) {
    try {
      await rm(backupDirectory, { recursive: true, force: false })
    } catch (error) {
      const failedDirectory = await uniqueBackupPath(parent)
      try {
        await rename(targetDirectory, failedDirectory)
        await rename(backupDirectory, targetDirectory)
        await rm(failedDirectory, { recursive: true, force: false })
      } catch (restoreError) {
        throw new Error(`output backup cleanup failed and restore failed: ${error.message}; ${restoreError.message}`)
      }
      throw error
    }
  }
  return targetDirectory
}

async function main() {
  let options
  try {
    options = parseBuildArguments(process.argv.slice(2))
  } catch (error) {
    fail(error.message)
  }
  if (options.help) {
    console.log("usage: bun tooling/build-w-windows.mjs [--profile <development|release|benchmark|size-experimental>] [--c11-recovery]")
    console.log("default profile: release; --c11-recovery is explicit and at most once")
    return
  }
  if (process.platform !== "win32" || process.arch !== "x64")
    fail(`requires native Windows x86_64, got ${process.platform}/${process.arch}`)

  const recipe = profileRecipe(options.profile)
  const startingGitState = readGitState()
  if (recipe.reproducible && startingGitState.dirty)
    fail("benchmark profile requires a clean Git worktree before build")
  if (options.cStandard === "11")
    console.log("W Windows build: C23 is the primary standard; using explicit C11 recovery")

  const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
  const manifestErrors = validateManifest(manifest)
  if (manifestErrors.length > 0) fail(manifestErrors.join("; "))
  const manifestSha256 = createHash("sha256")
    .update(await readFile(manifestPath)).digest("hex")
  const cacheDirectory = defaultCacheDirectory()
  if (isContained(root, cacheDirectory))
    fail(`materialized cache must be outside the repository: ${cacheDirectory}`)
  const materializedPath = join(cacheDirectory, MATERIALIZED_MANIFEST)
  const materialized = await validateMaterialized(cacheDirectory, manifest, manifestSha256)
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
  const compiler = probeCompilerIdentity(visualStudio.devCommand)
  const cmake = requireCommand("cmake")
  const ninja = requireCommand("ninja")
  const buildDirectory = await mkdtemp(join(tmpdir(), "w-build-windows-"))
  const outputParent = dirname(outputDirectory)
  let stageDirectory
  try {
    const reproducibility = recipe.reproducible
      ? await probeMsvcReproducibility(visualStudio.devCommand, buildDirectory)
      : emptyReproducibilityRecipe()
    const cmakeArguments = [
      "-S", seedDirectory,
      "-B", buildDirectory,
      "-G", "Ninja",
      `-DCMAKE_MAKE_PROGRAM=${ninja}`,
      ...cmakeProfileArguments(recipe, buildDirectory),
      "-DCMAKE_C_COMPILER=cl",
      `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${buildDirectory}`,
      `-DW_SEED_C_STANDARD=${options.cStandard}`,
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
    const builtBinary = join(buildDirectory, binaryName)
    const builtStats = await lstat(builtBinary)
    if (!builtStats.isFile() || isReparsePoint(builtStats) || builtStats.size === 0)
      fail(`build did not produce a non-empty regular ${builtBinary}`)
    if (await lstat(materializedPath).catch(() => null) === null)
      fail("materialized manifest disappeared during the build")
    if (await lstat(join(buildDirectory, "bin", "mlir-opt.exe")).catch(() => null) !== null)
      fail("build output copied MLIR tools into the W build directory")

    const stagedArtifact = await hashFile(builtBinary)
    const endingGitState = readGitState()
    try {
      assertStableGitHead(startingGitState, endingGitState)
    } catch (error) {
      fail(error.message)
    }
    if (recipe.reproducible && endingGitState.dirty)
      fail("benchmark worktree became dirty during build")
    await mkdir(outputParent, { recursive: true })
    stageDirectory = await stageBinary({
      builtBinary,
      stageParent: outputParent,
    })
    const smokeCases = await hashSmokeFixtures()
    const smoke = smokeCases.map((smokeCase) =>
      expectProgram(join(stageDirectory, binaryName), smokeCase))
    const receipt = makeReceipt({
      profile: options.profile,
      cStandard: options.cStandard,
      gitState: startingGitState,
      manifest,
      materialized,
      manifestSha256,
      sdk,
      compiler,
      artifact: stagedArtifact,
      smoke,
      reproducibility,
    })
    await writeAndValidateReceipt(join(stageDirectory, receiptName), receipt)
    await validateOutputDirectory(stageDirectory)
    await atomicInstallOutput(stageDirectory, outputDirectory)
    stageDirectory = undefined
    const installedReceipt = await validateOutputDirectory(outputDirectory)
    const installedStats = await lstat(binaryPath)
    console.log(`W Windows build: profile=${options.profile} cmakeBuildType=${recipe.cmakeBuildType} ` +
      `wExe=${binaryPath} bytes=${installedStats.size} receipt=${receiptPath} ` +
      `sdk=${sdk.version} compiler=${compiler.version} cStandard=${options.cStandard}`)
    console.log(`W Windows build: staged smoke passed=${installedReceipt.smoke.map((item) => item.id).join(",")}`)
  } finally {
    if (stageDirectory !== undefined)
      await rm(stageDirectory, { recursive: true, force: true })
    await rm(buildDirectory, { recursive: true, force: true })
  }
}

if (import.meta.main) {
  main().catch((error) => {
    console.error(error.message)
    process.exitCode = 1
  })
}
