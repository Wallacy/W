import { existsSync } from "node:fs"
import { lstat, mkdir, mkdtemp, readdir, readFile, rm, symlink, unlink, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { isAbsolute, join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const localManifestPath = resolve(root, "tooling", "mlir0-toolchain.json")
const ciManifestPath = resolve(root, "tooling", "mlir0-ci-toolchain.json")
const helloFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const restaurantLinearFixture = resolve(seedDirectory, "fixtures", "restaurant-linear.w")
const restaurantInterpolationFixture = resolve(seedDirectory, "fixtures", "restaurant-interpolation.w")
const restaurantIfFixture = resolve(seedDirectory, "fixtures", "restaurant-if.w")
const restaurantComparisonsFixture = resolve(seedDirectory, "fixtures", "restaurant-comparisons.w")
const restaurantComparisonCompositionFixture = resolve(seedDirectory, "fixtures", "restaurant-comparison-composition.w")
const restaurantBoolShortCircuitFixture = resolve(seedDirectory, "fixtures", "restaurant-bool-short-circuit.w")
const restaurantNestedIfFixture = resolve(seedDirectory, "fixtures", "restaurant-nested-if.w")
const restaurantRuntimeDivremFixture = resolve(seedDirectory,
  "fixtures", "restaurant-runtime-divrem.w")
const w1531MinimalFixture = resolve(seedDirectory, "fixtures", "w1531-if-minimal.w")
const w1531NoElseFixture = resolve(seedDirectory, "fixtures", "w1531-if-no-else.w")
const w1531LearnerFixture = resolve(seedDirectory, "fixtures", "w1531-if-learner.w")
const w1531IdiomaticFixture = resolve(seedDirectory, "fixtures", "w1531-if-idiomatic.w")
const w1531FrontierFixture = resolve(seedDirectory, "fixtures", "w1531-if-frontier.w")
const targetTriple = "x86_64-unknown-linux-gnu"
const expectedHelp =
  "usage: w check <path/file.w> [--json]\n" +
  "usage: w run <path/file.w> [-- <args...>]\n" +
  "usage: w build <path/file.w> --target <target> --output <artifact>\n"
const expectedHello = Buffer.from("Hello, world!\n", "utf8")

const isWindows = process.platform === "win32"
const isLinux = process.platform === "linux"
const isMacos = process.platform === "darwin"

function fail(message) {
  throw new Error(`W RUN: ${message}`)
}

export function parseArguments(argv) {
  let ci = false
  for (const argument of argv) {
    if (argument === "--ci" && !ci) ci = true
    else throw new Error(`unknown option: ${String(argument)}`)
  }
  return { ci }
}

const { ci: ciMode } = import.meta.main
  ? parseArguments(process.argv.slice(2))
  : { ci: false }
const expectedVersion = ciMode ? "23.1.0" : "20.1.2"
const manifestPath = ciMode ? ciManifestPath : localManifestPath

function assert(condition, message) {
  if (!condition) fail(message)
}

function unavailable(message) {
  if (ciMode) fail(`${message}; mandatory native CI prerequisite is unavailable`)
  console.log(`W RUN: SKIP ${message}`)
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
    stdoutBytes: Buffer.from(result.stdout),
    stderrBytes: Buffer.from(result.stderr),
  }
}

function shortOutput(bytes) {
  const text = bytes.toString().trim()
  return text.length > 2000 ? `${text.slice(-2000)}…` : text
}

function runRequired(label, command, args, cwd = root) {
  const result = spawn(command, args, cwd)
  if (result.exitCode !== 0) {
    const detail = shortOutput(result.stderrBytes) || shortOutput(result.stdoutBytes)
    fail(`${label} failed${detail ? `: ${detail}` : ""}`)
  }
  return result
}

function escapedVersion(value) {
  return value.replaceAll(/[.*+?^${}()|[\]\\]/gu, "\\$&")
}

export function validateManifest(manifest, mode = ciMode) {
  const manifestVersion = mode ? "23.1.0" : "20.1.2"
  const expectedSchema = mode
    ? "w-seed-mlir0-ci-toolchain-1"
    : "w-seed-mlir0-toolchain-1"
  assert(manifest?.$schema === expectedSchema &&
    manifest.version === 1 && manifest.status === "pinned",
  "toolchain manifest schema or status is not pinned")
  if (mode)
    assert(manifest.purpose === "mandatory-native-ci",
      "CI toolchain manifest purpose is invalid")
  assert(manifest.artifact?.schema === "w-seed-mlir0-15" &&
    manifest.artifact?.scope === "unit-cfg-nested-diamond",
  "toolchain manifest MLIR0 artifact scope is invalid")
  assert(manifest.target?.triple === targetTriple &&
    manifest.target?.os === "linux" && manifest.target?.abi === "gnu",
  "toolchain target is not the closed Linux GNU target")
  for (const role of mode ? ["mlir", "llvm"] : ["mlir", "llvm", "clang"])
    assert(manifest.toolchain?.[role] === manifestVersion,
      `toolchain ${role} version is not ${manifestVersion}`)
  const commands = manifest.commands
  const expectedCommands = mode
    ? { mlirOpt: "mlir-opt", mlirTranslate: "mlir-translate",
        llvmConfig: "llvm-config", llc: "llc", linkDriver: "/usr/bin/ld" }
    : { mlirOpt: "/usr/bin/mlir-opt-20",
        mlirTranslate: "/usr/bin/mlir-translate-20",
        llvmConfig: "/usr/bin/llvm-config-20", clang: "/usr/bin/clang-20" }
  for (const [role, expected] of Object.entries(expectedCommands)) {
    const command = commands?.[role]
    assert(command?.linux === expected && (mode || command?.wsl === expected) &&
      JSON.stringify(command.versionArgs) === JSON.stringify(["--version"]),
    `toolchain command ${role} is not the pinned absolute command`)
  }
  assert(Array.isArray(manifest.pipeline) && manifest.pipeline.length === (mode ? 5 : 3),
    "toolchain pipeline is invalid")
  const pipeline = manifest.pipeline
  assert(pipeline[0]?.tool === "mlir-opt" &&
    JSON.stringify(pipeline[0].args) === JSON.stringify([
      "<input.mlir>", "-o", "<verified.mlir>", "--verify-each",
    ]), "mlir-opt recipe changed")
  assert(pipeline[1]?.tool === "mlir-translate" &&
    JSON.stringify(pipeline[1].args) === JSON.stringify([
      "--mlir-to-llvmir", "<verified.mlir>", "-o", "<output.ll>",
    ]), "mlir-translate recipe changed")
  if (mode) {
    assert(manifest.hostLink?.driver === "/usr/bin/ld" &&
      manifest.hostLink?.targetFamily === "elf_x86_64" &&
      JSON.stringify(manifest.hostLink?.targetProbe) === JSON.stringify(["-V"]),
    "native linker contract changed")
    assert(pipeline[2]?.tool === "llc" &&
      JSON.stringify(pipeline[2].args) === JSON.stringify([
        `-mtriple=${targetTriple}`, "-filetype=obj", "-relocation-model=pic",
        "<output.ll>", "-o", "<output.o>",
      ]), "llc recipe changed")
    assert(pipeline[3]?.tool === "llc" &&
      JSON.stringify(pipeline[3].args) === JSON.stringify([
        `-mtriple=${targetTriple}`, "-filetype=obj", "-relocation-model=pic",
        "<wrt0.ll>", "-o", "<wrt0.o>",
      ]), "WRT0 object recipe changed")
    assert(pipeline[4]?.tool === "link-driver" &&
      JSON.stringify(pipeline[4].args) === JSON.stringify([
        "-pie", "--no-dynamic-linker", "-e", "_start", "--gc-sections",
        "-z", "noexecstack", "<output.o>", "<wrt0.o>", "-o",
        "<executable>",
      ]), "native CRT-free link recipe changed")
  } else assert(pipeline[2]?.tool === "clang" &&
    JSON.stringify(pipeline[2].args) === JSON.stringify([
      "-x", "ir", `--target=${targetTriple}`, "<output.ll>", "-o",
      "<executable>",
    ]), "clang recipe changed")
  assert(manifest.hostModes?.linux === "direct" &&
    (mode ? manifest.hostModes?.windows === "unsupported"
      : manifest.hostModes?.windows === "wsl:Ubuntu") &&
    manifest.windowsNative === false,
  "toolchain host mode is not the pinned Linux/WSL mode")
  assert(manifest.execution?.stdout === "ordered payloads + LF per print" &&
    manifest.execution?.stderr === "empty" && manifest.execution?.exit === 0,
  "toolchain execution contract changed")
  if (mode) return expectedCommands
  // The shared manifest retains the older check:mlir0 recipe, not this runner.
  return { mlirOpt: expectedCommands.mlirOpt,
    mlirTranslate: expectedCommands.mlirTranslate,
    llvmConfig: expectedCommands.llvmConfig,
    llc: "/usr/bin/llc-20", linkDriver: "/usr/bin/ld" }
}

function wslRun(command, args) {
  return spawn("wsl.exe", ["-d", "Ubuntu", "--", command, ...args])
}

function wslPath(windowsPath) {
  const result = runRequired("WSL path conversion", "wsl.exe", [
    "-d", "Ubuntu", "--", "wslpath", "-a", windowsPath.replaceAll("\\", "/"),
  ])
  assert(result.stderrBytes.length === 0, "wslpath wrote stderr")
  const value = result.stdoutBytes.toString().trim()
  assert(value.startsWith("/") && !value.includes("\0") &&
    !/[\r\n]/u.test(value), "wslpath did not return one absolute path")
  return value
}

function versionProbe(command, versionArgs) {
  if (!isWindows && !existsSync(command))
    return { present: false, valid: false, output: "" }
  const result = isWindows
    ? wslRun(command, versionArgs)
    : spawn(command, versionArgs)
  const present = result.exitCode !== 127
  const output = `${result.stdoutBytes}\n${result.stderrBytes}`
  return {
    present,
    valid: present && result.exitCode === 0 &&
      new RegExp(`\\b${escapedVersion(expectedVersion)}\\b`, "u").test(output),
    output,
  }
}

async function snapshotRunResidue() {
  if (isWindows) {
    const result = runRequired("WSL /tmp residue snapshot", "wsl.exe", [
      "-d", "Ubuntu", "--", "find", "/tmp", "-mindepth", "1",
      "-maxdepth", "1", "-type", "d", "-name", "w-run-*", "-printf",
      "%f\\n",
    ])
    assert(result.stderrBytes.length === 0, "residue snapshot wrote stderr")
    return new Set(result.stdoutBytes.toString().split(/\r?\n/u).filter(Boolean))
  }
  const entries = await readdir("/tmp", { withFileTypes: true })
  return new Set(entries.filter((entry) => entry.isDirectory() &&
    entry.name.startsWith("w-run-")).map((entry) => entry.name))
}

function assertNoNewResidue(before, after) {
  const added = [...after].filter((name) => !before.has(name))
  assert(added.length === 0,
    `public run left /tmp residue: ${JSON.stringify(added)}`)
}

function invoke(binary, args) {
  const result = isWindows
    ? wslRun(binary, args)
    : spawn(binary, args)
  return {
    exitCode: result.exitCode,
    stdout: result.stdoutBytes,
    stderr: result.stderrBytes,
  }
}

function resultSummary(result) {
  return JSON.stringify({
    exitCode: result.exitCode,
    stdout: result.stdout.toString(),
    stderr: result.stderr.toString(),
  })
}

function expectSuccess(binary, args, expected, label) {
  const result = invoke(binary, args)
  assert(result.exitCode === 0 && result.stdout.equals(expected) &&
    result.stderr.length === 0,
  `${label} was not exact: ${resultSummary(result)}`)
}

function expectSourceFailure(binary, path, label) {
  const result = invoke(binary, ["run", path])
  assert(result.exitCode === 2 && result.stdout.length === 0 &&
    result.stderr.length === 0,
  `${label} did not fail cleanly: ${resultSummary(result)}`)
}

function expectUnsupportedOption(binary, args, label) {
  const result = invoke(binary, args)
  assert(result.exitCode === 2 && result.stdout.length === 0 &&
    result.stderr.toString() === expectedHelp,
  `${label} did not reject with exact usage: ${resultSummary(result)}`)
}

function expectBuildFailure(binary, args, label) {
  const result = invoke(binary, args)
  assert(result.exitCode === 2 && result.stdout.length === 0 &&
    result.stderr.length === 0,
  `${label} did not fail cleanly: ${resultSummary(result)}`)
}

async function assertNoBuildResidue(directory, label = "public build") {
  if (isWindows) {
    const result = runRequired(`${label} WSL residue check`, "wsl.exe", [
      "-d", "Ubuntu", "--", "find", directory, "-mindepth", "1",
      "-maxdepth", "1", "-name", ".w-build-*", "-printf", "%f\\n",
    ])
    assert(result.stderrBytes.length === 0, `${label} residue check wrote stderr`)
    const residue = result.stdoutBytes.toString().split(/\r?\n/u).filter(Boolean)
    assert(residue.length === 0,
      `${label} left staging entries: ${JSON.stringify(residue)}`)
    return
  }
  const entries = await readdir(directory, { withFileTypes: true })
  const residue = entries.filter((entry) => entry.name.startsWith(".w-build-"))
  assert(residue.length === 0,
    `${label} left staging entries: ${JSON.stringify(residue)}`)
}

function artifactExists(path) {
  if (!isWindows) return existsSync(path)
  return wslRun("test", ["-e", path]).exitCode === 0
}

function readBuildArtifact(path) {
  if (!isWindows) return readFile(path)
  return Promise.resolve(runRequired("WSL artifact read", "wsl.exe", [
    "-d", "Ubuntu", "--", "cat", path,
  ]).stdoutBytes)
}

export function assertCrtFreeElf(bytes) {
  assert(Buffer.isBuffer(bytes) && bytes.length >= 64 &&
    bytes.subarray(0, 4).equals(Buffer.from([0x7f, 0x45, 0x4c, 0x46])) &&
    bytes[4] === 2 && bytes[5] === 1,
  "built artifact is not little-endian ELF64")
  assert(bytes.readUInt16LE(16) === 3 && bytes.readUInt16LE(18) === 62,
    "built artifact is not an x86_64 static PIE")
  const headerOffset = Number(bytes.readBigUInt64LE(32))
  const headerSize = bytes.readUInt16LE(54)
  const headerCount = bytes.readUInt16LE(56)
  assert(Number.isSafeInteger(headerOffset) && headerSize >= 56 &&
    headerCount > 0 && headerOffset + headerSize * headerCount <= bytes.length,
  "ELF program-header table is invalid")
  for (let index = 0; index < headerCount; index += 1) {
    const offset = headerOffset + index * headerSize
    const type = bytes.readUInt32LE(offset)
    assert(type !== 3, "CRT-free ELF unexpectedly names a dynamic interpreter")
    if (type !== 2) continue
    const dynamicOffset = Number(bytes.readBigUInt64LE(offset + 8))
    const dynamicSize = Number(bytes.readBigUInt64LE(offset + 32))
    assert(Number.isSafeInteger(dynamicOffset) &&
      Number.isSafeInteger(dynamicSize) && dynamicSize % 16 === 0 &&
      dynamicOffset + dynamicSize <= bytes.length,
    "ELF dynamic table is invalid")
    let terminated = false
    for (let entry = dynamicOffset; entry < dynamicOffset + dynamicSize;
      entry += 16) {
      const tag = bytes.readBigInt64LE(entry)
      assert(tag !== 1n, "CRT-free ELF unexpectedly has a DT_NEEDED dependency")
      if (tag === 0n) {
        terminated = true
        break
      }
    }
    assert(terminated, "ELF dynamic table has no terminator")
  }
}

if (import.meta.main) {
if (isMacos) unavailable("macOS has no pinned MLIR/LLVM/native-link evidence")
if (!isWindows && !isLinux) unavailable(`unsupported host ${process.platform}`)
if (ciMode && (!isLinux || process.arch !== "x64"))
  unavailable(`Linux native CI requires linux/x64, got ${process.platform}/${process.arch}`)
if (isWindows && !Bun.which("wsl.exe"))
  unavailable("pinned Linux toolchain requires WSL Ubuntu")

const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
const commands = validateManifest(manifest)
const runSource = await readFile(resolve(seedDirectory, "cli", "run.c"), "utf8")
const buildSource = await readFile(resolve(seedDirectory, "cli", "build.c"), "utf8")
for (const marker of ["W_SEED_LINUX_MLIR_OPT_PATH",
  "W_SEED_LINUX_MLIR_TRANSLATE_PATH", "W_SEED_LINUX_LLC_PATH",
  "W_SEED_LINUX_LINK_DRIVER_PATH"])
  assert(runSource.includes(marker), `cli/run.c does not use ${marker}`)
for (const marker of ["--canonicalize", "--cse", "-O3", "-s",
  "--no-dynamic-linker", "--gc-sections", "_start", "WRT0_LL"])
  assert(runSource.includes(marker),
    `cli/run.c is missing the release build flag ${marker}`)
assert(runSource.includes("W_SEED_RUN_COMPILE_PROFILE_DEV"),
  "cli/run.c does not select the development compile profile for w run")
assert(buildSource.includes("W_SEED_RUN_COMPILE_PROFILE_RELEASE"),
  "cli/build.c does not select the release compile profile for w build")
const llvmRoles = ["mlirOpt", "mlirTranslate", "llvmConfig", "llc"]
const roles = [...llvmRoles, "linkDriver"]
const resolvedCommands = Object.fromEntries(roles.map((role) => {
  if (!ciMode || role === "linkDriver") return [role, commands[role]]
  const resolved = Bun.which(commands[role])
  assert(resolved && isAbsolute(resolved) && existsSync(resolved),
    `CI tool ${role} basename did not resolve to an absolute executable`)
  return [role, resolved]
}))
const probes = llvmRoles.map((role) => [role,
  versionProbe(resolvedCommands[role] ?? commands[role],
    manifest.commands[role]?.versionArgs ?? ["--version"])])
if (!probes.some(([, probe]) => probe.present)) {
  unavailable("pinned MLIR/LLVM toolchain unavailable")
}
for (const [role, probe] of probes)
  if (!probe.present) fail(`pinned toolchain is incomplete: ${role} is absent`)
for (const [role, probe] of probes)
  if (!probe.valid) fail(`${role} version is not ${expectedVersion}: ${probe.output.trim()}`)

const hostProbe = (command, args) => isWindows ? wslRun(command, args) : spawn(command, args)
const linkTarget = hostProbe(resolvedCommands.linkDriver, ["-V"])
const linkTargetOutput = `${linkTarget.stdoutBytes.toString()}\n${linkTarget.stderrBytes.toString()}`
assert(linkTarget.exitCode === 0 && /(?:^|\s)elf_x86_64(?:\s|$)/u.test(linkTargetOutput),
"native linker does not report elf_x86_64 support")
const linkVersion = hostProbe(resolvedCommands.linkDriver, ["--version"])
assert(linkVersion.exitCode === 0, "native linker version probe failed")
console.log(`W RUN: LLVM tools ${expectedVersion}; native linker ${resolvedCommands.linkDriver}: ` +
  `${linkVersion.stdoutBytes.toString().split(/\r?\n/u)[0]}; target elf_x86_64`)
console.log("W RUN: stages MLIR → LLVM IR → llc PIC objects → WRT0 + direct static-PIE link (no CRT/libc)")

const cmake = isWindows ? "cmake" : Bun.which("cmake")
const ninja = isWindows ? "ninja" : Bun.which("ninja")
const compiler = "/usr/bin/gcc"
if (!isWindows && (!cmake || !ninja || !existsSync(compiler)))
  fail("Linux CMake/Ninja/GCC build tools are unavailable")
const bootstrapVersion = hostProbe(compiler, ["--version"])
assert(bootstrapVersion.exitCode === 0, "seed bootstrap compiler probe failed")
console.log(`W RUN: seed bootstrap ${compiler}: ` +
  bootstrapVersion.stdoutBytes.toString().split(/\r?\n/u)[0])
if (isWindows) {
  for (const [label, command, args] of [
    ["WSL CMake", "cmake", ["--version"]],
    ["WSL Ninja", "ninja", ["--version"]],
    ["WSL GCC", "/usr/bin/gcc", ["--version"]],
  ]) {
    const result = wslRun(command, args)
    if (result.exitCode === 127)
      fail(`${label} is unavailable`)
    if (result.exitCode !== 0)
      fail(`${label} probe failed: ${shortOutput(result.stderrBytes)}`)
  }
}

let buildDirectory
let fixtureDirectory
let buildArtifactDirectory
let residueBefore
try {
  buildDirectory = await mkdtemp(join(tmpdir(), "w-run-product-build-"))
  fixtureDirectory = await mkdtemp(join(tmpdir(), "w-run-product-fixtures-"))
  if (isWindows) {
    const result = runRequired("WSL build artifact directory", "wsl.exe", [
      "-d", "Ubuntu", "--", "mktemp", "-d",
      "/tmp/w-run-build-artifacts-XXXXXX",
    ])
    buildArtifactDirectory = result.stdoutBytes.toString().trim()
    assert(/^\/tmp\/w-run-build-artifacts-[A-Za-z0-9]+$/u.test(buildArtifactDirectory),
      "WSL build artifact directory is not an isolated /tmp path")
  } else buildArtifactDirectory = fixtureDirectory
  const buildPath = isWindows ? wslPath(buildDirectory) : buildDirectory
  const sourcePath = isWindows ? wslPath(seedDirectory) : seedDirectory
  const toolDirectory = join(fixtureDirectory, "tool links")
  const toolPath = isWindows ? wslPath(toolDirectory) : toolDirectory
  const toolNames = {
    mlirOpt: "mlir-opt",
    mlirTranslate: "mlir-translate",
    llvmConfig: "llvm-config",
    llc: "llc",
    linkDriver: "ld",
  }
  if (isWindows) {
    runRequired("WSL tool-link directory", "wsl.exe", [
      "-d", "Ubuntu", "--", "mkdir", "-p", toolPath,
    ])
    for (const role of roles)
      runRequired(`WSL ${role} tool link`, "wsl.exe", [
        "-d", "Ubuntu", "--", "ln", "-s", resolvedCommands[role],
        `${toolPath}/${toolNames[role]}`,
      ])
  } else {
    await mkdir(toolDirectory)
    for (const role of roles)
      await symlink(resolvedCommands[role],
        join(toolDirectory, toolNames[role]))
  }
  const configuredCommands = Object.fromEntries(roles.map((role) => [
    role, `${toolPath}/${toolNames[role]}`,
  ]))
  const configureArgs = [
    "-S", sourcePath, "-B", buildPath, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release", `-DCMAKE_C_COMPILER:FILEPATH=${compiler}`,
    "-DW_SEED_ENABLE_LINUX_NATIVE_RUN=ON",
    `-DW_MLIR0_LINUX_MLIR_OPT:FILEPATH=${configuredCommands.mlirOpt}`,
    `-DW_MLIR0_LINUX_MLIR_TRANSLATE:FILEPATH=${configuredCommands.mlirTranslate}`,
    `-DW_MLIR0_LINUX_LLVM_CONFIG:FILEPATH=${configuredCommands.llvmConfig}`,
    `-DW_MLIR0_LINUX_LLC:FILEPATH=${configuredCommands.llc}`,
    `-DW_MLIR0_LINUX_LINK_DRIVER:FILEPATH=${configuredCommands.linkDriver}`,
  ]
  if (isWindows) {
    runRequired("WSL seed configure", "wsl.exe", [
      "-d", "Ubuntu", "--", "cmake", ...configureArgs,
    ])
    runRequired("WSL public w build", "wsl.exe", [
      "-d", "Ubuntu", "--", "cmake", "--build", buildPath,
      "--target", "w", "--", "-j", "2",
    ])
  } else {
    runRequired("Linux seed configure", cmake, configureArgs)
    runRequired("Linux public w build", cmake, [
      "--build", buildDirectory, "--target", "w", "--", "-j", "2",
    ])
  }
  const binary = isWindows ? `${buildPath}/w` : join(buildDirectory, "w")
  const restaurantBinding = join(fixtureDirectory, "restaurant_binding.w")
  const restaurantLiteral = join(fixtureDirectory, "restaurant_literal.w")
  const restaurantBuiltinDisplay = join(fixtureDirectory,
    "restaurant_builtin_display.w")
  const restaurantTypedBindings = join(fixtureDirectory,
    "restaurant_typed_bindings.w")
  const restaurantDirectCall = join(fixtureDirectory,
    "restaurant_direct_call.w")
  const restaurantScalarReturn = join(fixtureDirectory,
    "restaurant_scalar_return.w")
  const runtimeDivisionZero = join(fixtureDirectory,
    "runtime_division_zero.w")
  const empty = join(fixtureDirectory, "empty.w")
  const zero = join(fixtureDirectory, "zero.w")
  const oversize = join(fixtureDirectory, "oversize.w")
  const invalidUtf8 = join(fixtureDirectory, "invalid_utf8.w")
  const incomplete = join(fixtureDirectory, "incomplete.w")
  const noop = join(fixtureDirectory, "noop.w")
  const twoCalls = join(fixtureDirectory, "two_calls.w")
  const unusedBinding = join(fixtureDirectory, "unused_binding.w")
  const tooManyInstructions = join(fixtureDirectory, "too_many_instructions.w")
  const totalOutputOverflow = join(fixtureDirectory, "total_output_overflow.w")
  const recursiveCall = join(fixtureDirectory, "recursive_call.w")
  const runtimeStringParameter = join(fixtureDirectory,
    "runtime_string_parameter.w")
  const runtimeStringResult = join(fixtureDirectory,
    "runtime_string_result.w")
  const missingScalarReturn = join(fixtureDirectory,
    "missing_scalar_return.w")
  const nestedReturnCall = join(fixtureDirectory, "nested_return_call.w")
  const invalidComparisons = [
    ["Bool operands", "true == false"],
    ["String operands", '"a" != "b"'],
    ["mixed operands", "1 <= true"],
    ["comparison used as i64", "(1 < 2) + 3"],
  ]
  await writeFile(restaurantBinding,
    "fn serve() { let message = \"Table 42 remains open\" print(message) }\n" +
    "entry(serve)\n")
  await writeFile(restaurantLiteral,
    "fn serve() { print(\"Table 42 remains open\") }\nentry(serve)\n")
  await writeFile(restaurantBuiltinDisplay,
    "fn serve() { let state = \"open\" " +
    "print(\"Kitchen ${true}/${false}; table: ${state}\") }\nentry(serve)\n")
  await writeFile(restaurantTypedBindings,
    "fn serve() { let table = 6 * 7 let isOpen = true let state = \"open\" " +
    "print(\"Table ${table}; open: ${isOpen}; state: ${state}\") }\n" +
    "entry(serve)\n")
  await writeFile(restaurantDirectCall,
    "fn announce(table: i64, isOpen: Bool) {\n" +
    "  print(\"Table ${table}; open: ${isOpen}\")\n}\n" +
    "fn main() { announce(isOpen: true, table: 6 * 7) }\n" +
    "entry(main)\n")
  await writeFile(restaurantScalarReturn,
    "fn tableNumber(): i64 { return 6 * 7 }\n" +
    "fn main() { let table = tableNumber() " +
    "print(\"Table ${table}\") }\nentry(main)\n")
  await writeFile(runtimeDivisionZero,
    "fn divide(value: i64, by divisor: i64): i64 { return value / divisor }\n" +
    "fn main() { let result = divide(value: 8, by: 0) " +
    "print(\"success ${result}\") }\nentry(main)\n")
  await writeFile(empty, "fn main() { print(\"\") }\nentry(main)\n")
  await writeFile(zero, Buffer.alloc(0))
  await writeFile(oversize, Buffer.alloc(4097, 0x70))
  await writeFile(invalidUtf8, Buffer.from([0xc3]))
  await writeFile(incomplete, "fn main(\n")
  await writeFile(noop, "fn main() { noop(\"Other\") }\nentry(main)\n")
  await writeFile(twoCalls,
    "fn main() { print(\"a\")\nprint(\"b\") }\nentry(main)\n")
  await writeFile(unusedBinding,
    "fn main() { let message = \"unused\"\nprint(\"kept\") }\nentry(main)\n")
  await writeFile(tooManyInstructions,
    `fn main() {\n${Array.from({ length: 33 }, () => "print(\"x\")").join("\n")}\n` +
    `}\nentry(main)\n`)
  await writeFile(totalOutputOverflow,
    `fn main() {\nlet message = "${"x".repeat(256)}"\n` +
    `${Array.from({ length: 17 }, () => "print(message)").join("\n")}\n` +
    `}\nentry(main)\n`)
  await writeFile(recursiveCall,
    "fn again() { again() }\nfn main() { again() }\nentry(main)\n")
  await writeFile(runtimeStringParameter,
    "fn show(value: String) { print(value) }\n" +
    "fn main() { show(value: \"x\") }\nentry(main)\n")
  await writeFile(runtimeStringResult,
    "fn state(): String { return \"open\" }\n" +
    "fn main() { let value = state() print(\"${value}\") }\nentry(main)\n")
  await writeFile(missingScalarReturn,
    "fn value(): i64 { print(\"no value\") }\n" +
    "fn main() { let result = value() print(\"${result}\") }\nentry(main)\n")
  await writeFile(nestedReturnCall,
    "fn value(): i64 { return 42 }\n" +
    "fn relay(): i64 { return value() }\n" +
    "fn main() { let result = relay() print(\"${result}\") }\nentry(main)\n")
  for (const [index, [label, expression]] of invalidComparisons.entries()) {
    const path = join(fixtureDirectory, `invalid_comparison_${index}.w`)
    await writeFile(path,
      `fn main() { let result = ${expression} print("\${result}") }\nentry(main)\n`)
    invalidComparisons[index] = [label, path]
  }
  const toWsl = (path) => isWindows ? wslPath(path) : path
  residueBefore = await snapshotRunResidue()

  expectSuccess(binary, ["--help"], Buffer.from(expectedHelp), "w --help")
  expectSuccess(binary, ["run", "--help"], Buffer.from(expectedHelp),
    "w run --help")
  expectSuccess(binary, ["build", "--help"], Buffer.from(expectedHelp),
    "w build --help")
  expectSuccess(binary, ["run", toWsl(helloFixture)], expectedHello,
    "Hello fixture")
  expectSuccess(binary, ["run", toWsl(restaurantBinding)],
    Buffer.from("Table 42 remains open\n"), "Restaurant binding")
  expectSuccess(binary, ["run", toWsl(restaurantLiteral)],
    Buffer.from("Table 42 remains open\n"), "Restaurant literal")
  expectSuccess(binary, ["run", toWsl(restaurantLinearFixture)],
    Buffer.from("Table 42 remains open\nKitchen is ready\n", "utf8"),
    "Restaurant linear sequence")
  expectSuccess(binary, ["run", toWsl(restaurantInterpolationFixture)],
    Buffer.from("Table 42 remains open\n", "utf8"),
    "Restaurant typed interpolation")
  const expectedRestaurantIf = Buffer.from(
    "Kitchen open\nAfter service\nKitchen closed\nAfter service\n", "utf8")
  const expectedRestaurantNestedIf = Buffer.from(
    "Restaurant open\nKitchen ready\nOpen branch joined\nPost-join service\n" +
    "Restaurant open\nKitchen closed\nOpen branch joined\nPost-join service\n" +
    "Restaurant closed\nKitchen ready\nClosed branch joined\nPost-join service\n" +
    "Restaurant closed\nKitchen closed\nClosed branch joined\nPost-join service\n",
    "utf8")
  expectSuccess(binary, ["run", toWsl(restaurantIfFixture)],
    expectedRestaurantIf,
    "Restaurant if diamond")
  expectSuccess(binary, ["run", toWsl(restaurantComparisonsFixture)],
    Buffer.from("Seat party\nSeat party\nWaitlist\n", "utf8"),
    "Restaurant signed-i64 admission comparison")
  expectSuccess(binary, ["run", toWsl(restaurantComparisonCompositionFixture)],
    Buffer.from(
      "false/true/true/true/false/false\n" +
      "true/false/false/true/false/true\n" +
      "false/true/false/false/true/true\n" +
      "false/true/true/true/false/false\n" +
      "false/true/false/false/true/true\nAllowed true\nAllowed false\n", "utf8"),
    "Restaurant comparison operators, signed endpoints, and Bool composition")
  expectSuccess(binary, ["run", toWsl(restaurantBoolShortCircuitFixture)],
    Buffer.from(
      "Override checked\nClosed allowed true\nCapacity checked\n" +
      "Open allowed true\n", "utf8"),
    "Restaurant Bool short-circuit")
  expectSuccess(binary, ["run", toWsl(restaurantNestedIfFixture)],
    expectedRestaurantNestedIf,
    "Restaurant nested if")
  expectSuccess(binary, ["run", toWsl(w1531MinimalFixture)],
    Buffer.from("then\n", "utf8"), "W-1531 minimal if/else")
  expectSuccess(binary, ["run", toWsl(w1531NoElseFixture)],
    Buffer.from("Kitchen open\nAfter service\n", "utf8"),
    "W-1531 no-else")
  const restaurantStyleFixtures = [
    [w1531LearnerFixture, "W-1531 learner source-style candidate"],
    [w1531IdiomaticFixture, "W-1531 idiomatic source-style candidate"],
    [w1531FrontierFixture, "W-1531 frontier exploration source-style candidate"],
  ]
  for (const [fixture, label] of restaurantStyleFixtures)
    expectSuccess(binary, ["run", toWsl(fixture)], expectedRestaurantIf, label)
  expectSuccess(binary, ["run", toWsl(restaurantBuiltinDisplay)],
    Buffer.from("Kitchen true/false; table: open\n", "utf8"),
    "Restaurant built-in Display interpolation")
  expectSuccess(binary, ["run", toWsl(restaurantTypedBindings)],
    Buffer.from("Table 42; open: true; state: open\n", "utf8"),
    "Restaurant typed immutable bindings")
  expectSuccess(binary, ["run", toWsl(restaurantDirectCall)],
    Buffer.from("Table 42; open: true\n", "utf8"),
    "Restaurant direct W call")
  expectSuccess(binary, ["run", toWsl(restaurantScalarReturn)],
    Buffer.from("Table 42\n", "utf8"),
    "Restaurant scalar return")
  expectSuccess(binary, ["run", toWsl(restaurantRuntimeDivremFixture)],
    Buffer.from("Each 7; left 2\n", "utf8"),
    "Restaurant checked runtime division/remainder")
  const divisionFault = invoke(binary, ["run", toWsl(runtimeDivisionZero)])
  assert(divisionFault.exitCode !== 0 && divisionFault.stdout.length === 0 &&
    divisionFault.stderr.length === 0,
  `runtime division by zero did not fail before output: ${resultSummary(divisionFault)}`)
  expectSuccess(binary, ["run", toWsl(empty)], Buffer.from("\n"),
    "empty payload")
  expectSuccess(binary, ["run", toWsl(twoCalls)], Buffer.from("a\nb\n"),
    "two-call sequence")
  expectSuccess(binary, ["run", toWsl(helloFixture), "--", "arbitrary", "--entry", ""],
    expectedHello, "forwarded program arguments")

  const buildOutput = (name) => isWindows
    ? `${buildArtifactDirectory}/${name}`
    : join(buildArtifactDirectory, name)
  const buildHello = buildOutput("hello-build")
  const buildRestaurantIf = buildOutput("restaurant-if-build")
  const buildMounted = join(fixtureDirectory, "mounted-build")
  const buildWrongTarget = buildOutput("wrong-target-build")
  const buildMissingParent = buildOutput("missing/artifact")
  const buildSymlinkOutput = buildOutput("symlink-output")
  const buildSymlinkTarget = buildOutput("symlink-target")
  expectSuccess(binary, ["build", toWsl(helloFixture), "--target", targetTriple,
    "--output", buildHello], Buffer.alloc(0),
    "build Hello fixture")
  expectSuccess(buildHello, [], expectedHello,
    "execute built Hello artifact")
  const helloBytes = await readBuildArtifact(buildHello)
  assertCrtFreeElf(helloBytes)
  expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
    targetTriple, "--output", buildHello],
  "reject existing build output")
  assert((await readBuildArtifact(buildHello)).equals(helloBytes),
    "existing build output was modified")
  expectSuccess(binary, ["build", toWsl(restaurantIfFixture), "--target",
    targetTriple, "--output", buildRestaurantIf], Buffer.alloc(0),
    "build restaurant-if fixture")
  expectSuccess(buildRestaurantIf, [], expectedRestaurantIf,
    "execute built restaurant-if artifact")
  if (isWindows) {
    expectSuccess(binary, ["build", toWsl(helloFixture), "--target", targetTriple,
      "--output", toWsl(buildMounted)], Buffer.alloc(0),
    "build mounted-filesystem fallback")
    expectSuccess(toWsl(buildMounted), [], expectedHello,
      "execute mounted-filesystem fallback artifact")
    await assertNoBuildResidue(wslPath(fixtureDirectory),
      "mounted-filesystem fallback")
  }
  expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
    "x86_64-pc-windows-msvc", "--output", buildWrongTarget],
  "reject unsupported build target")
  assert(!artifactExists(buildWrongTarget), "wrong-target build left an artifact")
  expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
    targetTriple, "--output", buildMissingParent],
  "reject missing build output parent")
  assert(!artifactExists(buildMissingParent), "missing-parent build left an artifact")
  if (isWindows) {
    runRequired("WSL output symlink", "wsl.exe", [
      "-d", "Ubuntu", "--", "ln", "-s", buildSymlinkTarget,
      buildSymlinkOutput,
    ])
    try {
      expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
        targetTriple, "--output", buildSymlinkOutput],
      "reject symlink build output")
      const symlinkCheck = wslRun("test", ["-L", buildSymlinkOutput])
      assert(symlinkCheck.exitCode === 0,
        "rejected symlink build output was modified")
    } finally {
      runRequired("WSL output symlink cleanup", "wsl.exe", [
        "-d", "Ubuntu", "--", "rm", "-f", "--", buildSymlinkOutput,
      ])
    }
  } else {
    await symlink("symlink-target", buildSymlinkOutput)
    try {
      expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
        targetTriple, "--output", buildSymlinkOutput],
      "reject symlink build output")
      const symlinkStats = await lstat(buildSymlinkOutput)
      assert(symlinkStats.isSymbolicLink(),
        "rejected symlink build output was modified")
    } finally {
      await unlink(buildSymlinkOutput)
    }
  }
  await assertNoBuildResidue(buildArtifactDirectory)

  expectSourceFailure(binary, toWsl(join(fixtureDirectory, "missing.w")),
    "missing source")
  expectSourceFailure(binary, toWsl(zero), "zero-byte source")
  expectSourceFailure(binary, toWsl(oversize), "oversize source")
  expectSourceFailure(binary, toWsl(invalidUtf8), "invalid UTF-8 source")
  expectSourceFailure(binary, toWsl(incomplete), "incomplete source")
  expectSourceFailure(binary, toWsl(noop), "unsupported noop source")
  expectSourceFailure(binary, toWsl(unusedBinding), "unused binding source")
  expectSourceFailure(binary, toWsl(tooManyInstructions),
    "too-many-instructions source")
  expectSourceFailure(binary, toWsl(totalOutputOverflow),
    "total-output-overflow source")
  expectSourceFailure(binary, toWsl(recursiveCall), "recursive call source")
  expectSourceFailure(binary, toWsl(runtimeStringParameter),
    "runtime String parameter source")
  expectSourceFailure(binary, toWsl(runtimeStringResult),
    "runtime String result source")
  expectSourceFailure(binary, toWsl(missingScalarReturn),
    "missing scalar return source")
  expectSourceFailure(binary, toWsl(nestedReturnCall),
    "nested return call source")
  for (const [label, path] of invalidComparisons)
    expectSourceFailure(binary, toWsl(path), `comparison rejects ${label}`)
  expectUnsupportedOption(binary, ["run", "--entry", toWsl(helloFixture)],
    "unsupported --entry option")
  expectUnsupportedOption(binary, ["run", "--offline", toWsl(helloFixture)],
    "unsupported --offline option")

  // Replace only this gate's private links. Failures must clean every stage.
  async function replaceToolLink(role, destination) {
    const linkPath = configuredCommands[role]
    if (isWindows) {
      runRequired("WSL private tool-link removal", "wsl.exe", [
        "-d", "Ubuntu", "--", "rm", "--", linkPath,
      ])
      runRequired("WSL private tool-link replacement", "wsl.exe", [
        "-d", "Ubuntu", "--", "ln", "-s", destination, linkPath,
      ])
    } else {
      await rm(linkPath)
      await symlink(destination, linkPath)
    }
  }
  for (const role of ["mlirOpt", "mlirTranslate", "llc", "linkDriver"]) {
    try {
      await replaceToolLink(role, "/usr/bin/false")
      expectSourceFailure(binary, toWsl(helloFixture), `${role} stage failure`)
      expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
        targetTriple, "--output", buildOutput(`${role}-failure`)],
      `${role} build stage failure`)
      await assertNoBuildResidue(buildArtifactDirectory,
        `${role} build stage failure`)
      await replaceToolLink(role, `${toolPath}/missing-executable`)
      expectSourceFailure(binary, toWsl(helloFixture), `${role} missing at runtime`)
      expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
        targetTriple, "--output", buildOutput(`${role}-missing`)],
      `${role} build missing at runtime`)
      await assertNoBuildResidue(buildArtifactDirectory,
        `${role} build missing at runtime`)
    } finally {
      await replaceToolLink(role, resolvedCommands[role])
    }
  }
  expectSuccess(binary, ["run", toWsl(helloFixture)], expectedHello,
    "native pipeline after restoring tools")

  const residueAfter = await snapshotRunResidue()
  assertNoNewResidue(residueBefore, residueAfter)
  await assertNoBuildResidue(buildArtifactDirectory)
  console.log("W RUN: public source → verified HIR0 → MLIR0 → MLIR/LLVM/native E2E passed")
} finally {
  if (buildDirectory !== undefined)
    await rm(buildDirectory, { recursive: true, force: true })
  if (fixtureDirectory !== undefined)
    await rm(fixtureDirectory, { recursive: true, force: true })
  if (isWindows && buildArtifactDirectory !== undefined)
    runRequired("WSL build artifact cleanup", "wsl.exe", [
      "-d", "Ubuntu", "--", "rm", "-rf", "--", buildArtifactDirectory,
    ])
}
}
