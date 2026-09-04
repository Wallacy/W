import { existsSync } from "node:fs"
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { dialectDisclosure, probeCDialect } from "./c-dialect.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const canonicalFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const restaurantLinearFixture = resolve(seedDirectory, "fixtures", "restaurant-linear.w")
const restaurantInterpolationFixture = resolve(seedDirectory, "fixtures", "restaurant-interpolation.w")
const mlirHeaderPath = resolve(seedDirectory, "include", "w_seed_mlir0.h")
const mlirSourcePath = resolve(seedDirectory, "src", "w_seed_mlir0.c")
const manifestPath = resolve(root, "tooling", "mlir0-toolchain.json")
const targetTriple = "x86_64-unknown-linux-gnu"
const expectedVersion = "20.1.2"
const isWindows = process.platform === "win32"

function fail(message) {
  throw new Error(`MLIR0: ${message}`)
}

function assert(condition, message) {
  if (!condition) fail(message)
}

function run(command, args, cwd = root, env = undefined) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    env,
    stdout: "pipe",
    stderr: "pipe",
  })
  return {
    ...result,
    stdoutText: result.stdout.toString(),
    stderrText: result.stderr.toString(),
  }
}

function runRequired(command, args, cwd, label, env = undefined) {
  const result = run(command, args, cwd, env)
  if (result.exitCode !== 0) {
    const details = result.stderrText.trim() || result.stdoutText.trim()
    fail(`${label} failed${details ? `: ${details.slice(-2000)}` : ""}`)
  }
  return result
}

function validateManifest(manifest) {
  assert(manifest && manifest.$schema === "w-seed-mlir0-toolchain-1" &&
    manifest.version === 1 && manifest.status === "pinned",
  "toolchain manifest schema or status is invalid")
  assert(manifest.artifact?.schema === "w-seed-mlir0-5" &&
    manifest.artifact?.scope === "linear-print-and-internal-i64-display",
  "toolchain manifest MLIR0 artifact scope is invalid")
  assert(manifest.target?.triple === targetTriple,
    "toolchain manifest target is not the closed MLIR0 target")
  for (const role of ["mlir", "llvm", "clang"])
    assert(manifest.toolchain?.[role] === expectedVersion,
      `toolchain manifest ${role} version is not ${expectedVersion}`)
  for (const role of ["mlirOpt", "mlirTranslate", "llvmConfig", "clang"]) {
    const command = manifest.commands?.[role]
    assert(command && typeof command.linux === "string" &&
      typeof command.wsl === "string" &&
      JSON.stringify(command.versionArgs) === JSON.stringify(["--version"]),
    `toolchain manifest command ${role} is invalid`)
  }
  assert(Array.isArray(manifest.pipeline) && manifest.pipeline.length === 3,
    "toolchain manifest pipeline is invalid")
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
    manifest.hostModes?.windows === "wsl:Ubuntu",
  "toolchain host mode is invalid")
  assert(manifest.hostEvidence === "wsl-linux" &&
    manifest.windowsNative === false &&
    manifest.hostMatrix?.linux === "evidence" &&
    manifest.hostMatrix?.windows === "wsl-linux-only" &&
    manifest.hostMatrix?.macos === "gap",
  "toolchain host evidence is invalid")
  assert(JSON.stringify(manifest.emittedTargets?.evidence) ===
    JSON.stringify([targetTriple]) && manifest.emittedTargets?.wideMatrix === "gap",
  "toolchain emitted-target evidence is invalid")
  assert(manifest.distribution?.windowsNativeMlir === "gap" &&
    manifest.distribution?.macosMlir === "gap" &&
    manifest.distribution?.packaging === "gap",
  "toolchain distribution evidence is invalid")
  assert(manifest.execution?.stdout === "ordered payloads + LF per print" &&
    manifest.execution?.stderr === "empty" && manifest.execution?.exit === 0,
  "toolchain execution contract is invalid")
}

function asCommand(value, label, allowMissing = false) {
  if (typeof value !== "string" || value.length === 0)
    fail(`${label} override is empty`)
  if (isWindows) {
    if (!/^\/[A-Za-z0-9._+\-/]+$/u.test(value))
      fail(`${label} override must be a simple absolute WSL path`)
    return value
  }
  const command = value.includes("/") ? value : Bun.which(value)
  if (!command || (value.includes("/") && !existsSync(command))) {
    if (allowMissing) return undefined
    fail(`${label} override does not name an executable`)
  }
  return command
}

function wslPath(value) {
  const absolute = resolve(value).replaceAll("\\", "/")
  const match = absolute.match(/^([A-Za-z]):\/(.*)$/u)
  if (!match) fail(`path is not a Windows drive path: ${value}`)
  return `/mnt/${match[1].toLowerCase()}/${match[2]}`
}

function invokeTool(command, args, label, environment = undefined) {
  if (isWindows) {
    const result = run(wsl, ["-d", "Ubuntu", "--", command, ...args], root,
      environment)
    if (result.exitCode !== 0)
      fail(`${label} failed: ${(result.stderrText || result.stdoutText).trim()}`)
    return result
  }
  return runRequired(command, args, root, label, environment)
}

function invokeProgram(command, args, label) {
  if (isWindows)
    return run(wsl, ["-d", "Ubuntu", "--", command, ...args], root)
  return run(command, args, root)
}

const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
validateManifest(manifest)

const mlirSourceContract = `${await readFile(mlirHeaderPath, "utf8")}\n` +
  await readFile(mlirSourcePath, "utf8")
assert(!/\bw_seed_hlo0(?:_[A-Za-z0-9_]+)?\b/u.test(mlirSourceContract),
  "MLIR0 source/API still includes or calls HLO0")

const cmake = Bun.which("cmake")
const ninja = Bun.which("ninja")
const compiler = ["cc", "gcc", "clang", "cl"].map((name) => Bun.which(name))
  .find(Boolean)
const dialect = compiler ? await probeCDialect(compiler) : undefined
const wsl = isWindows ? Bun.which("wsl.exe") : undefined
function resolveToolCommand(role, environmentName) {
  const override = process.env[environmentName]
  const value = override !== undefined ? override :
    (isWindows ? manifest.commands[role].wsl : manifest.commands[role].linux)
  return asCommand(value, environmentName, override === undefined)
}

const mlirCommands = {
  mlirOpt: resolveToolCommand("mlirOpt", "W_MLIR0_MLIR_OPT"),
  mlirTranslate: resolveToolCommand("mlirTranslate", "W_MLIR0_MLIR_TRANSLATE"),
  llvmConfig: resolveToolCommand("llvmConfig", "W_MLIR0_LLVM_CONFIG"),
  clang: resolveToolCommand("clang", "W_MLIR0_CLANG"),
}

function versionProbe(role, command) {
  if (isWindows) {
    if (!wsl) return { present: false, valid: false, output: "" }
    const result = run(wsl, ["-d", "Ubuntu", "--", command, "--version"], root)
    return {
      present: result.exitCode !== 127,
      valid: result.exitCode === 0 &&
        new RegExp(`\\b${expectedVersion.replaceAll(".", "\\.")}\\b`, "u")
          .test(`${result.stdoutText}\n${result.stderrText}`),
      output: `${result.stdoutText}\n${result.stderrText}`,
    }
  }
  if (!command || !existsSync(command))
    return { present: false, valid: false, output: "" }
  const result = run(command, ["--version"], root)
  return {
    present: true,
    valid: result.exitCode === 0 &&
      new RegExp(`\\b${expectedVersion.replaceAll(".", "\\.")}\\b`, "u")
        .test(`${result.stdoutText}\n${result.stderrText}`),
    output: `${result.stdoutText}\n${result.stderrText}`,
  }
}

const versionProbes = Object.entries(mlirCommands).map(([role, command]) =>
  [role, versionProbe(role, command)])
const mlirToolchainPresent = versionProbes.some(([, probe]) => probe.present)
if (!mlirToolchainPresent) {
  console.log("MLIR0: SKIP MLIR/LLVM/Clang/llvm-config toolchain unavailable")
  process.exit(0)
}
if (versionProbes.some(([, probe]) => !probe.present))
  fail("MLIR/LLVM/Clang toolchain is incomplete")
for (const [role, probe] of versionProbes) {
  if (!probe.valid)
    fail(`${role} tool version is not ${expectedVersion}: ${probe.output.trim()}`)
}
if (!cmake || !ninja || !compiler || !dialect)
  fail("seed C build toolchain is incomplete or lacks C23")

const buildDirectory = await mkdtemp(join(tmpdir(), "w-mlir0-seed-"))
const artifactDirectory = await mkdtemp(join(tmpdir(), "w-mlir0-artifact-"))
const suffix = process.platform === "win32" ? ".exe" : ""
const toolchainEnvironment = { ...process.env, CC: compiler }
const tool = (role) => mlirCommands[role]

try {
  runRequired(cmake, ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release"], root, "seed configure", toolchainEnvironment)
  runRequired(cmake, ["--build", buildDirectory, "--target",
    "w_seed_mlir0_tests", "w_seed_mlir0_gate", "--parallel", "2"], root,
  "seed build", toolchainEnvironment)

  const unit = run(resolve(buildDirectory, `w_seed_mlir0_tests${suffix}`), [])
  assert(unit.exitCode === 0, `unit tests failed: ${unit.stderrText}`)
  assert(unit.stderr.length === 0 &&
    unit.stdoutText.includes("verified HIR0 native subset"),
  "unit witness is missing or wrote to stderr")

  const seedGate = resolve(buildDirectory, `w_seed_mlir0_gate${suffix}`)
  const restaurantPath = resolve(artifactDirectory, "restaurant.w")
  const restaurantLiteralPath = resolve(artifactDirectory, "restaurant-literal.w")
  const restaurantLinearLiteralPath = resolve(artifactDirectory, "restaurant-linear-literal.w")
  const twoCallsPath = resolve(artifactDirectory, "two-calls.w")
  const arithmeticPath = resolve(artifactDirectory, "typed-arithmetic.w")
  const percentPath = resolve(artifactDirectory, "percent-interpolation.w")
  const interpolationNulPath = resolve(artifactDirectory,
    "nul-interpolation.w")
  const minimumI64Path = resolve(artifactDirectory, "minimum-i64.w")
  const emptyPath = resolve(artifactDirectory, "empty.w")
  await writeFile(restaurantPath,
    `fn serve() { let message = "Table 42 remains open" print(message) }\nentry(serve)\n`)
  await writeFile(restaurantLiteralPath,
    `fn serve() { print("Table 42 remains open") }\nentry(serve)\n`)
  await writeFile(restaurantLinearLiteralPath,
    `fn serve() {\nprint("Table 42 remains open")\n` +
    `print("Kitchen is ready")\n}\nentry(serve)\n`)
  await writeFile(twoCallsPath,
    `fn main() { print("a")\nprint("b") }\nentry(main)\n`)
  await writeFile(arithmeticPath,
    'fn main() { print("${8 + 2} ${2 - 8} ${8 * 2} ${8 / 2} ${8 % 3}") }\n' +
    'entry(main)\n')
  await writeFile(percentPath,
    'fn main() { print("Load ${6 * 7}%") }\nentry(main)\n')
  await writeFile(interpolationNulPath,
    Buffer.from('fn main() { print("A\u0000${6 * 7}") }\nentry(main)\n'))
  await writeFile(minimumI64Path,
    'fn main() { print("${0 - 9223372036854775807 - 1}") }\nentry(main)\n')
  await writeFile(emptyPath, `fn main() { print("") }\nentry(main)\n`)
  const products = [
    { name: "hello", source: canonicalFixture,
      expected: Buffer.from("Hello, world!\n", "utf8") },
    { name: "restaurant-binding", source: restaurantPath,
      expected: Buffer.from("Table 42 remains open\n", "utf8") },
    { name: "restaurant-literal", source: restaurantLiteralPath,
      expected: Buffer.from("Table 42 remains open\n", "utf8") },
    { name: "restaurant-linear", source: restaurantLinearFixture,
      expected: Buffer.from("Table 42 remains open\nKitchen is ready\n", "utf8") },
    { name: "restaurant-interpolation", source: restaurantInterpolationFixture,
      expected: Buffer.from("Table 42 remains open\n", "utf8") },
    { name: "restaurant-linear-literal", source: restaurantLinearLiteralPath,
      expected: Buffer.from("Table 42 remains open\nKitchen is ready\n", "utf8") },
    { name: "two-calls", source: twoCallsPath,
      expected: Buffer.from("a\nb\n", "utf8") },
    { name: "typed-arithmetic", source: arithmeticPath,
      expected: Buffer.from("10 -6 16 4 2\n", "utf8") },
    { name: "percent-interpolation", source: percentPath,
      expected: Buffer.from("Load 42%\n", "utf8") },
    { name: "nul-interpolation", source: interpolationNulPath,
      expected: Buffer.from([0x41, 0x00, 0x34, 0x32, 0x0a]) },
    { name: "minimum-i64", source: minimumI64Path,
      expected: Buffer.from("-9223372036854775808\n", "utf8") },
    { name: "empty", source: emptyPath, expected: Buffer.from("\n", "utf8") },
  ]
  const artifacts = new Map()
  for (const product of products) {
    const generated = run(seedGate, [product.source])
    assert(generated.exitCode === 0,
      `${product.name} source route failed with ${generated.exitCode}: ` +
      generated.stderrText.trim())
    assert(generated.stderr.length === 0 && generated.stdout.length > 0,
      `${product.name} route did not emit one MLIR artifact`)
    artifacts.set(product.name, Buffer.from(generated.stdout))
    const input = resolve(artifactDirectory, `${product.name}.mlir`)
    const verified = resolve(artifactDirectory, `${product.name}.verified.mlir`)
    const llvm = resolve(artifactDirectory, `${product.name}.ll`)
    const executable = resolve(artifactDirectory, `${product.name}.native`)
    await writeFile(input, generated.stdout)
    const inputForTool = isWindows ? wslPath(input) : input
    const verifiedForTool = isWindows ? wslPath(verified) : verified
    const llvmForTool = isWindows ? wslPath(llvm) : llvm
    const executableForTool = isWindows ? wslPath(executable) : executable
    invokeTool(tool("mlirOpt"), [inputForTool, "-o", verifiedForTool,
      "--verify-each"], `${product.name} mlir-opt`)
    invokeTool(tool("mlirTranslate"), ["--mlir-to-llvmir", verifiedForTool,
      "-o", llvmForTool], `${product.name} mlir-translate`)
    invokeTool(tool("clang"), ["-x", "ir", `--target=${targetTriple}`,
      llvmForTool, "-o", executableForTool], `${product.name} clang LLVM IR`)
    const execution = invokeProgram(executableForTool, [],
      `${product.name} generated executable`)
    assert(execution.exitCode === 0,
      `${product.name} generated executable returned ${execution.exitCode}`)
    assert(execution.stderr.length === 0,
      `${product.name} generated executable wrote to stderr`)
    assert(Buffer.from(execution.stdout).equals(product.expected),
      `${product.name} stdout is not exact payload plus LF`)
  }
  assert(artifacts.get("restaurant-binding").equals(
    artifacts.get("restaurant-literal")),
  "Restaurant literal and binding MLIR artifacts differ")
  assert(artifacts.get("restaurant-linear").equals(
    artifacts.get("restaurant-linear-literal")),
  "Restaurant linear literal and binding MLIR artifacts differ")
  assert(!artifacts.get("hello").equals(artifacts.get("restaurant-binding")),
    "Restaurant payload did not change MLIR")
  assert(!artifacts.get("hello").equals(artifacts.get("empty")),
    "empty payload did not change MLIR")
  for (const name of ["restaurant-interpolation", "typed-arithmetic",
    "percent-interpolation", "nul-interpolation", "minimum-i64"]) {
    const artifact = artifacts.get(name)
    assert(artifact.includes("@w_seed_append_i64") &&
      !artifact.includes("snprintf") && !artifact.includes("%ld") &&
      !artifact.includes("vararg"),
    `${name} did not use the internal bounded i64 writer`)
  }

  const commentedPath = resolve(artifactDirectory, "commented.w")
  await writeFile(commentedPath,
    `// source comment\nfn main() {   print("Hello, world!")   }\n\nentry(main)\n`)
  const commented = run(seedGate, [commentedPath])
  assert(commented.exitCode === 0 && commented.stderr.length === 0 &&
    Buffer.from(commented.stdout).equals(artifacts.get("hello")),
  "trivia changed the deterministic MLIR artifact")

  const tooManyInstructions =
    `fn main() {\n${Array.from({ length: 33 }, () => "print(\"x\")").join("\n")}\n` +
    `}\nentry(main)\n`
  const totalOutputOverflow =
    `fn main() {\nlet message = "${"x".repeat(256)}"\n` +
    `${Array.from({ length: 17 }, () => "print(message)").join("\n")}\n` +
    `}\nentry(main)\n`
  const adversarial = [
    ["comment-with-print.w",
      `fn main() { noop("Other") } // print("Hello, world!")\nentry(main)\n`],
    ["noop.w", `fn main() { noop("Other") }\nentry(main)\n`],
    ["outside-subset.w",
      `fn main(value: String) { print(value) }\nentry(main)\n`],
    ["var-binding.w",
      `fn main() { var message = "Hello, world!" print(message) }\nentry(main)\n`],
    ["unused-binding.w",
      `fn main() { let message = "unused" print("kept") }\nentry(main)\n`],
    ["too-many-instructions.w", tooManyInstructions],
    ["total-output-overflow.w", totalOutputOverflow],
  ]
  for (const [name, source] of adversarial) {
    const path = resolve(artifactDirectory, name)
    await writeFile(path, source)
    const rejected = run(seedGate, [path])
    assert(rejected.exitCode !== 0 && rejected.stdout.length === 0,
      `${name} was accepted or emitted partial MLIR`)
  }
  console.log(`MLIR0: verified HIR0 → LLVM dialect → mlir-opt → mlir-translate → clang IR/native passed (${dialectDisclosure(dialect)})`)
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
  await rm(artifactDirectory, { recursive: true, force: true })
}
