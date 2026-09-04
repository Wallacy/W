import { existsSync } from "node:fs"
import { mkdtemp, readdir, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const manifestPath = resolve(root, "tooling", "mlir0-toolchain.json")
const helloFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const restaurantLinearFixture = resolve(seedDirectory, "fixtures", "restaurant-linear.w")
const restaurantInterpolationFixture = resolve(seedDirectory, "fixtures", "restaurant-interpolation.w")
const targetTriple = "x86_64-unknown-linux-gnu"
const expectedVersion = "20.1.2"
const expectedHelp =
  "usage: w check <path/file.w> [--json]\n" +
  "usage: w run <path/file.w> [-- <args...>]\n"
const expectedHello = Buffer.from("Hello, world!\n", "utf8")

const isWindows = process.platform === "win32"
const isLinux = process.platform === "linux"
const isMacos = process.platform === "darwin"

function fail(message) {
  throw new Error(`W RUN: ${message}`)
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

function validateManifest(manifest) {
  assert(manifest?.$schema === "w-seed-mlir0-toolchain-1" &&
    manifest.version === 1 && manifest.status === "pinned",
  "toolchain manifest schema or status is not pinned")
  assert(manifest.artifact?.schema === "w-seed-mlir0-6" &&
    manifest.artifact?.scope === "linear-print-and-builtin-display",
  "toolchain manifest MLIR0 artifact scope is invalid")
  assert(manifest.target?.triple === targetTriple &&
    manifest.target?.os === "linux" && manifest.target?.abi === "gnu",
  "toolchain target is not the closed Linux GNU target")
  for (const role of ["mlir", "llvm", "clang"])
    assert(manifest.toolchain?.[role] === expectedVersion,
      `toolchain ${role} version is not ${expectedVersion}`)
  const commands = manifest.commands
  const expectedCommands = {
    mlirOpt: "/usr/bin/mlir-opt-20",
    mlirTranslate: "/usr/bin/mlir-translate-20",
    llvmConfig: "/usr/bin/llvm-config-20",
    clang: "/usr/bin/clang-20",
  }
  for (const [role, expected] of Object.entries(expectedCommands)) {
    const command = commands?.[role]
    assert(command?.linux === expected && command?.wsl === expected &&
      JSON.stringify(command.versionArgs) === JSON.stringify(["--version"]),
    `toolchain command ${role} is not the pinned absolute command`)
  }
  assert(Array.isArray(manifest.pipeline) && manifest.pipeline.length === 3,
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
  assert(pipeline[2]?.tool === "clang" &&
    JSON.stringify(pipeline[2].args) === JSON.stringify([
      "-x", "ir", `--target=${targetTriple}`, "<output.ll>", "-o",
      "<executable>",
    ]), "clang recipe changed")
  assert(manifest.hostModes?.linux === "direct" &&
    manifest.hostModes?.windows === "wsl:Ubuntu" &&
    manifest.windowsNative === false,
  "toolchain host mode is not the pinned Linux/WSL mode")
  assert(manifest.execution?.stdout === "ordered payloads + LF per print" &&
    manifest.execution?.stderr === "empty" && manifest.execution?.exit === 0,
  "toolchain execution contract changed")
  return expectedCommands
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

if (isMacos) {
  console.log("W RUN: SKIP macOS has no pinned MLIR/LLVM/Clang evidence")
  process.exit(0)
}
if (!isWindows && !isLinux) {
  console.log(`W RUN: SKIP unsupported host ${process.platform}`)
  process.exit(0)
}
if (isWindows && !Bun.which("wsl.exe")) {
  console.log("W RUN: SKIP pinned Linux toolchain requires WSL Ubuntu")
  process.exit(0)
}

const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
const commands = validateManifest(manifest)
const runSource = await readFile(resolve(seedDirectory, "cli", "run.c"), "utf8")
for (const [role, symbol] of Object.entries({
  mlirOpt: "MLIR_OPT",
  mlirTranslate: "MLIR_TRANSLATE",
  clang: "CLANG",
})) {
  assert(runSource.includes(`static const char ${symbol}[] = "${commands[role]}";`),
    `cli/run.c ${symbol} path differs from the pinned manifest`)
}
const roles = ["mlirOpt", "mlirTranslate", "llvmConfig", "clang"]
const probes = roles.map((role) => [role,
  versionProbe(commands[role], manifest.commands[role].versionArgs)])
if (!probes.some(([, probe]) => probe.present)) {
  console.log("W RUN: SKIP pinned MLIR/LLVM/Clang toolchain unavailable")
  process.exit(0)
}
for (const [role, probe] of probes)
  if (!probe.present) fail(`pinned toolchain is incomplete: ${role} is absent`)
for (const [role, probe] of probes)
  if (!probe.valid) fail(`${role} version is not ${expectedVersion}: ${probe.output.trim()}`)

const cmake = isWindows ? "cmake" : Bun.which("cmake")
const ninja = isWindows ? "ninja" : Bun.which("ninja")
const compiler = isWindows ? "/usr/bin/gcc" : Bun.which("gcc")
if (!isWindows && (!cmake || !ninja || !compiler))
  fail("Linux CMake/Ninja/GCC build tools are unavailable")
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
let residueBefore
try {
  buildDirectory = await mkdtemp(join(tmpdir(), "w-run-product-build-"))
  fixtureDirectory = await mkdtemp(join(tmpdir(), "w-run-product-fixtures-"))
  const buildPath = isWindows ? wslPath(buildDirectory) : buildDirectory
  const sourcePath = isWindows ? wslPath(seedDirectory) : seedDirectory
  const configureArgs = [
    "-S", sourcePath, "-B", buildPath, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release", `-DCMAKE_C_COMPILER=${compiler}`,
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
  await writeFile(restaurantBinding,
    "fn serve() { let message = \"Table 42 remains open\" print(message) }\n" +
    "entry(serve)\n")
  await writeFile(restaurantLiteral,
    "fn serve() { print(\"Table 42 remains open\") }\nentry(serve)\n")
  await writeFile(restaurantBuiltinDisplay,
    "fn serve() { let state = \"open\" " +
    "print(\"Kitchen ${true}/${false}; table: ${state}\") }\nentry(serve)\n")
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
  const toWsl = (path) => isWindows ? wslPath(path) : path
  residueBefore = await snapshotRunResidue()

  expectSuccess(binary, ["--help"], Buffer.from(expectedHelp), "w --help")
  expectSuccess(binary, ["run", "--help"], Buffer.from(expectedHelp),
    "w run --help")
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
  expectSuccess(binary, ["run", toWsl(restaurantBuiltinDisplay)],
    Buffer.from("Kitchen true/false; table: open\n", "utf8"),
    "Restaurant built-in Display interpolation")
  expectSuccess(binary, ["run", toWsl(empty)], Buffer.from("\n"),
    "empty payload")
  expectSuccess(binary, ["run", toWsl(twoCalls)], Buffer.from("a\nb\n"),
    "two-call sequence")
  expectSuccess(binary, ["run", toWsl(helloFixture), "--", "arbitrary", "--entry", ""],
    expectedHello, "forwarded program arguments")

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
  expectUnsupportedOption(binary, ["run", "--entry", toWsl(helloFixture)],
    "unsupported --entry option")
  expectUnsupportedOption(binary, ["run", "--offline", toWsl(helloFixture)],
    "unsupported --offline option")

  const residueAfter = await snapshotRunResidue()
  assertNoNewResidue(residueBefore, residueAfter)
  console.log("W RUN: public source → verified HIR0 → MLIR0 → MLIR/LLVM/native E2E passed")
} finally {
  if (buildDirectory !== undefined)
    await rm(buildDirectory, { recursive: true, force: true })
  if (fixtureDirectory !== undefined)
    await rm(fixtureDirectory, { recursive: true, force: true })
}
