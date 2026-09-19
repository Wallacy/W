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
const restaurantIfFixture = resolve(seedDirectory, "fixtures", "restaurant-if.w")
const restaurantNestedIfFixture = resolve(seedDirectory, "fixtures", "restaurant-nested-if.w")
const restaurantNestedScalarIfFixture = resolve(seedDirectory,
  "fixtures", "restaurant-nested-scalar-if.w")
const restaurantCheckedArithmeticFixture = resolve(seedDirectory,
  "fixtures", "restaurant-checked-arithmetic.w")
const restaurantRuntimeDivremFixture = resolve(seedDirectory,
  "fixtures", "restaurant-runtime-divrem.w")
const restaurantUnaryNegateFixture = resolve(seedDirectory,
  "fixtures", "restaurant-unary-negate.w")
const restaurantUnaryInterpolationFixture = resolve(seedDirectory,
  "fixtures", "restaurant-unary-interpolation.w")
const restaurantBitwiseFixture = resolve(seedDirectory,
  "fixtures", "restaurant-bitwise.w")
const restaurantUnsignedFixture = resolve(seedDirectory,
  "fixtures", "restaurant-unsigned.w")
const restaurantShiftsFixture = resolve(seedDirectory,
  "fixtures", "restaurant-shifts.w")
const restaurantPowerFixture = resolve(seedDirectory,
  "fixtures", "restaurant-power.w")
const restaurantPowerPrefixFixture = resolve(seedDirectory,
  "fixtures", "restaurant-power-prefix.w")
const restaurantCompoundFixture = resolve(seedDirectory,
  "fixtures", "restaurant-compound.w")
const restaurantF64StrictFixture = resolve(seedDirectory,
  "fixtures", "restaurant-f64-strict.w")
const restaurantCheckedIntegerArithmeticFixture = resolve(seedDirectory,
  "fixtures", "restaurant-checked-integer-arithmetic.w")
const restaurantIntegerWrappingFixture = resolve(seedDirectory,
  "fixtures", "restaurant-integer-wrapping.w")
const restaurantIntegerWideningFixture = resolve(seedDirectory,
  "fixtures", "restaurant-integer-widening.w")
const restaurantUIntWrappingAddFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-add.w")
const restaurantUIntWrappingSubtractFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-subtract.w")
const restaurantUIntWrappingMultiplyFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-multiply.w")
const restaurantUIntWrappingNegateFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-negate.w")
const restaurantUIntWrappingPowerFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-power.w")
const restaurantUIntOverflowingPowerFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-overflowing-power.w")
const restaurantUIntOverflowingFamilyFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-overflowing-family.w")
const restaurantUIntWrappingShiftLeftFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-wrapping-shift-left.w")
const restaurantUIntMaskedShiftLeftFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-masked-shift-left.w")
const restaurantUIntMaskedShiftRightFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-masked-shift-right.w")
const restaurantUIntLogicalShiftRightFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-logical-shift-right.w")
const restaurantUIntRotatedLeftFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-rotated-left.w")
const restaurantUIntRotatedRightFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-rotated-right.w")
const restaurantUIntCountOnesFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-count-ones.w")
const restaurantUIntCountZerosFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-count-zeros.w")
const restaurantUIntLeadingZerosFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-leading-zeros.w")
const restaurantUIntTrailingZerosFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-trailing-zeros.w")
const restaurantUIntReversedBitsFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-reversed-bits.w")
const restaurantUIntReversedBytesFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-reversed-bytes.w")
const restaurantUIntSaturatingAddFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-saturating-add.w")
const restaurantUIntSaturatingSubtractFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-saturating-subtract.w")
const restaurantUIntSaturatingMultiplyFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-saturating-multiply.w")
const restaurantUIntSaturatingPolicyFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-saturating-policy.w")
const restaurantUIntBitNotFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-bit-not.w")
const restaurantUIntBitwiseFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-bitwise.w")
const restaurantUIntCompoundFixture = resolve(seedDirectory,
  "fixtures", "restaurant-uint-compound.w")
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
const restaurantWhileFixture = resolve(seedDirectory, "fixtures", "restaurant-while.w")
const restaurantWmoFixture = resolve(seedDirectory, "fixtures", "restaurant-wmo.w")
const restaurantAsyncJoinFixture = resolve(seedDirectory,
  "fixtures", "restaurant-async-join.w")
const restaurantAsyncYieldFixture = resolve(seedDirectory,
  "fixtures", "restaurant-async-yield.w")
const explicitPanicFixture = resolve(seedDirectory,
  "fixtures", "panic-explicit.w")
const mlirHeaderPath = resolve(seedDirectory, "include", "w_seed_mlir0.h")
const mlirSourcePath = resolve(seedDirectory, "src", "w_seed_mlir0.c")
const manifestPath = resolve(root, "tooling", "mlir0-toolchain.json")
const targetTriple = "x86_64-unknown-linux-gnu"
const expectedVersion = "23.1.1"
const developmentPatchCompatibility =
  process.env.W_MLIR0_DEVELOPMENT_PATCH_COMPAT !== "0"
const isWindows = process.platform === "win32"

function acceptedVersionPattern() {
  const [major, minor] = expectedVersion.split(".")
  return developmentPatchCompatibility
    ? `\\b${major}\\.${minor}\\.[0-9]+\\b`
    : `\\b${expectedVersion.replaceAll(".", "\\.")}\\b`
}

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
  assert(manifest.artifact?.schema === "w-seed-mlir0-15" &&
    manifest.artifact?.scope === "unit-structured-cfg-natural-loop",
  "toolchain manifest MLIR0 artifact scope is invalid")
  assert(manifest.target?.triple === targetTriple,
    "toolchain manifest target is not the closed MLIR0 target")
  assert(manifest.toolchainDiscovery?.rootEnv === "W_MLIR0_TOOLCHAIN_ROOT" &&
    manifest.toolchainDiscovery?.relativeBin === "bin" &&
    manifest.toolchainDiscovery?.materializedManifest ===
      "w-mlir0-linux-materialized.json" &&
    manifest.toolchainDiscovery?.pathPolicy ===
      "explicit-external-root-or-host-PATH" &&
    manifest.toolchainDiscovery?.repositoryPath === false &&
    manifest.toolchainDiscovery?.archive?.release === "2026.09.11" &&
    manifest.toolchainDiscovery?.archive?.filename ===
      "llvm-mlir_llvmorg-23.1.1_x86_64-unknown-linux-gnu.tar.zst" &&
    manifest.toolchainDiscovery?.archive?.sizeBytes === 400536815 &&
    manifest.toolchainDiscovery?.archive?.sha256 ===
      "cfa94b0c4dfb933e755362468b615ada40e77e6d77b8db609505888de296ca7e" &&
    manifest.toolchainDiscovery?.archive?.source ===
      "munich-quantum-software/setup-mlir",
  "toolchain discovery contract is invalid")
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
      "<input.mlir>", "-o", "<verified.mlir>",
      "--convert-scf-to-cf", "--convert-cf-to-llvm", "--verify-each",
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
    if (!(/^[A-Za-z0-9._+-]+$/u.test(value) ||
      /^\/[A-Za-z0-9._+\-/]+$/u.test(value)))
      fail(`${label} override must be a simple WSL path or command name`)
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
function normalizeExternalToolchainRoot(value) {
  if (typeof value !== "string" || value.length === 0)
    fail("W_MLIR0_TOOLCHAIN_ROOT is empty")
  const normalized = value.replace(/\/+$/u, "")
  const valid = isWindows
    ? /^\/[A-Za-z0-9._+\-/]+$/u.test(normalized)
    : normalized.startsWith("/")
  if (!valid || normalized === "/tmp" || normalized.startsWith("/tmp/"))
    fail("W_MLIR0_TOOLCHAIN_ROOT must be a persistent absolute path outside /tmp")
  return normalized
}

const externalToolchainRoot = process.env.W_MLIR0_TOOLCHAIN_ROOT === undefined
  ? undefined : normalizeExternalToolchainRoot(
    process.env.W_MLIR0_TOOLCHAIN_ROOT)

function compatibleHostCommand(command) {
  if (!developmentPatchCompatibility ||
      !/^[A-Za-z0-9._+-]+$/u.test(command)) return undefined
  const major = expectedVersion.split(".")[0]
  const versioned = `${command}-${major}`
  if (!isWindows) return Bun.which(versioned) ?? undefined
  const probe = run(wsl, ["-d", "Ubuntu", "--", "sh", "-lc",
    `command -v ${versioned}`], root)
  if (probe.exitCode !== 0) return undefined
  const value = probe.stdoutText.trim()
  return /^\/[A-Za-z0-9._+\-/]+$/u.test(value) ? value : undefined
}

function resolveToolCommand(role, environmentName) {
  const override = process.env[environmentName]
  let value = override !== undefined ? override :
    (isWindows ? manifest.commands[role].wsl : manifest.commands[role].linux)
  if (override === undefined && externalToolchainRoot !== undefined) {
    assert(/^[A-Za-z0-9._+-]+$/u.test(value),
      `manifest command ${role} is not a simple command name`)
    value = `${externalToolchainRoot}/bin/${value}`
  } else if (override === undefined && externalToolchainRoot === undefined) {
    value = compatibleHostCommand(value) ?? value
  }
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
        new RegExp(acceptedVersionPattern(), "u")
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
      new RegExp(acceptedVersionPattern(), "u")
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
    fail(`${role} tool version is not ${developmentPatchCompatibility
      ? "in the 23.1.x development line" : expectedVersion}: ${probe.output.trim()}`)
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

  const unitPath = resolve(buildDirectory, `w_seed_mlir0_tests${suffix}`)
  const unit = run(unitPath, [])
  assert(unit.exitCode === 0, `unit tests failed: ${unit.stderrText}`)
  assert(unit.stderr.length === 0 &&
    unit.stdoutText.includes("verified HIR0 native subset"),
  "unit witness is missing or wrote to stderr")

  const typedPropagation = run(unitPath, ["--emit-typed-propagation"])
  assert(typedPropagation.exitCode === 0,
    `typed propagation probe failed: ${typedPropagation.stderrText}`)
  assert(typedPropagation.stderr.length === 0 && typedPropagation.stdout.length > 0,
    "typed propagation probe did not emit one silent MLIR artifact")
  const typedInput = resolve(artifactDirectory, "typed-propagation.mlir")
  const typedVerified = resolve(artifactDirectory,
    "typed-propagation.verified.mlir")
  const typedLlvm = resolve(artifactDirectory, "typed-propagation.ll")
  await writeFile(typedInput, typedPropagation.stdout)
  const typedInputForTool = isWindows ? wslPath(typedInput) : typedInput
  const typedVerifiedForTool = isWindows ? wslPath(typedVerified) : typedVerified
  const typedLlvmForTool = isWindows ? wslPath(typedLlvm) : typedLlvm
  invokeTool(tool("mlirOpt"), [typedInputForTool, "-o", typedVerifiedForTool,
    "--verify-each"], "typed propagation mlir-opt")
  invokeTool(tool("mlirTranslate"), ["--mlir-to-llvmir", typedVerifiedForTool,
    "-o", typedLlvmForTool], "typed propagation mlir-translate")
  const typedLlvmText = await readFile(typedLlvm, "utf8")
  assert(typedLlvmText.includes("define internal { i1, i64 } @w_seed_typed_leaf") &&
    typedLlvmText.includes("call { i1, i64 } @w_seed_typed_leaf") &&
    typedLlvmText.includes("br i1") && !typedLlvmText.includes("invoke "),
  "typed propagation LLVM translation lost the compact call/branch carrier")

  const typedCleanup = run(unitPath, ["--emit-typed-cleanup"])
  assert(typedCleanup.exitCode === 0,
    `typed cleanup probe failed: ${typedCleanup.stderrText}`)
  assert(typedCleanup.stderr.length === 0 && typedCleanup.stdout.length > 0,
    "typed cleanup probe did not emit one silent MLIR artifact")
  const cleanupInput = resolve(artifactDirectory, "typed-cleanup.mlir")
  const cleanupVerified = resolve(artifactDirectory, "typed-cleanup.verified.mlir")
  const cleanupLlvm = resolve(artifactDirectory, "typed-cleanup.ll")
  await writeFile(cleanupInput, typedCleanup.stdout)
  const cleanupInputForTool = isWindows ? wslPath(cleanupInput) : cleanupInput
  const cleanupVerifiedForTool = isWindows ? wslPath(cleanupVerified) : cleanupVerified
  const cleanupLlvmForTool = isWindows ? wslPath(cleanupLlvm) : cleanupLlvm
  invokeTool(tool("mlirOpt"), [cleanupInputForTool, "-o", cleanupVerifiedForTool,
    "--verify-each"], "typed cleanup mlir-opt")
  invokeTool(tool("mlirTranslate"), ["--mlir-to-llvmir", cleanupVerifiedForTool,
    "-o", cleanupLlvmForTool], "typed cleanup mlir-translate")
  const cleanupLlvmText = await readFile(cleanupLlvm, "utf8")
  const cleanupCalls = cleanupLlvmText.match(/call void @w_seed_typed_clean\(\)/g) ?? []
  assert(cleanupLlvmText.includes("define internal void @w_seed_typed_clean") &&
    cleanupLlvmText.includes("define internal { i1, i64 } @w_seed_typed_leaf") &&
    cleanupLlvmText.includes("call { i1, i64 } @w_seed_typed_leaf") &&
    cleanupLlvmText.includes("br i1") && cleanupCalls.length === 2 &&
    !cleanupLlvmText.includes("invoke "),
  "typed cleanup LLVM translation lost either successor cleanup or compact carrier")

  const seedGate = resolve(buildDirectory, `w_seed_mlir0_gate${suffix}`)
  const restaurantPath = resolve(artifactDirectory, "restaurant.w")
  const restaurantLiteralPath = resolve(artifactDirectory, "restaurant-literal.w")
  const restaurantLinearLiteralPath = resolve(artifactDirectory, "restaurant-linear-literal.w")
  const restaurantIfPath = resolve(artifactDirectory, "restaurant-if.w")
  const restaurantNestedIfPath = resolve(artifactDirectory, "restaurant-nested-if.w")
  const restaurantNestedScalarIfPath = resolve(artifactDirectory,
    "restaurant-nested-scalar-if.w")
  const twoCallsPath = resolve(artifactDirectory, "two-calls.w")
  const arithmeticPath = resolve(artifactDirectory, "typed-arithmetic.w")
  const percentPath = resolve(artifactDirectory, "percent-interpolation.w")
  const interpolationNulPath = resolve(artifactDirectory,
    "nul-interpolation.w")
  const stringValueNulPath = resolve(artifactDirectory,
    "nul-string-value.w")
  const minimumI64Path = resolve(artifactDirectory, "minimum-i64.w")
  const builtinDisplayPath = resolve(artifactDirectory, "builtin-display.w")
  const typedBindingsPath = resolve(artifactDirectory, "typed-bindings.w")
  const directCallPath = resolve(artifactDirectory, "direct-call.w")
  const scalarReturnPath = resolve(artifactDirectory, "scalar-return.w")
  const boolReturnPath = resolve(artifactDirectory, "bool-return.w")
  const checkedOverflowPath = resolve(artifactDirectory, "checked-overflow.w")
  const runtimeMinimumRemainderPath = resolve(artifactDirectory,
    "runtime-minimum-remainder.w")
  const runtimeDivisionZeroPath = resolve(artifactDirectory,
    "runtime-division-zero.w")
  const runtimeDivisionOverflowPath = resolve(artifactDirectory,
    "runtime-division-overflow.w")
  const runtimeRemainderZeroPath = resolve(artifactDirectory,
    "runtime-remainder-zero.w")
  const runtimeNegationOverflowPath = resolve(artifactDirectory,
    "runtime-negation-overflow.w")
  const runtimeShiftCountPath = resolve(artifactDirectory,
    "runtime-shift-count.w")
  const runtimeUnsignedShiftOverflowPath = resolve(artifactDirectory,
    "runtime-unsigned-shift-overflow.w")
  const runtimeSignedShiftOverflowPath = resolve(artifactDirectory,
    "runtime-signed-shift-overflow.w")
  const runtimeSignedPowerOverflowPath = resolve(artifactDirectory,
    "runtime-signed-power-overflow.w")
  const runtimeUnsignedPowerOverflowPath = resolve(artifactDirectory,
    "runtime-unsigned-power-overflow.w")
  const runtimeUIntAddOverflowPath = resolve(artifactDirectory,
    "runtime-uint-add-overflow.w")
  const runtimeUIntSubtractUnderflowPath = resolve(artifactDirectory,
    "runtime-uint-subtract-underflow.w")
  const runtimeUIntMultiplyOverflowPath = resolve(artifactDirectory,
    "runtime-uint-multiply-overflow.w")
  const runtimeUIntDivisionZeroPath = resolve(artifactDirectory,
    "runtime-uint-division-zero.w")
  const runtimeUIntRemainderZeroPath = resolve(artifactDirectory,
    "runtime-uint-remainder-zero.w")
  const emptyPath = resolve(artifactDirectory, "empty.w")
  await writeFile(restaurantPath,
    `fn serve() { let message = "Table 42 remains open" print(message) }\nentry(serve)\n`)
  await writeFile(restaurantLiteralPath,
    `fn serve() { print("Table 42 remains open") }\nentry(serve)\n`)
  await writeFile(restaurantLinearLiteralPath,
    `fn serve() {\nprint("Table 42 remains open")\n` +
    `print("Kitchen is ready")\n}\nentry(serve)\n`)
  await writeFile(restaurantIfPath,
    `fn serve(isOpen: Bool) {\n` +
    `  if isOpen { print("Kitchen open") } else { print("Kitchen closed") }\n` +
    `  print("After service")\n` +
    `}\n` +
    `fn main() { serve(isOpen: true) serve(isOpen: false) }\n` +
    `entry(main)\n`)
  await writeFile(restaurantNestedIfPath,
    await readFile(restaurantNestedIfFixture))
  await writeFile(restaurantNestedScalarIfPath,
    await readFile(restaurantNestedScalarIfFixture))
  await writeFile(twoCallsPath,
    `fn main() { print("a")\nprint("b") }\nentry(main)\n`)
  await writeFile(arithmeticPath,
    'fn main() { print("${8 + 2} ${2 - 8} ${8 * 2} ${8 / 2} ${8 % 3}") }\n' +
    'entry(main)\n')
  await writeFile(percentPath,
    'fn main() { print("Load ${6 * 7}%") }\nentry(main)\n')
  await writeFile(interpolationNulPath,
    Buffer.from('fn main() { print("A\u0000${6 * 7}") }\nentry(main)\n'))
  await writeFile(stringValueNulPath,
    Buffer.from('fn main() { let state = "A\u0000B" print("${state}") }\nentry(main)\n'))
  await writeFile(minimumI64Path,
    'fn main() { print("${0 - 9223372036854775807 - 1}") }\nentry(main)\n')
  await writeFile(builtinDisplayPath,
    'fn serve() { let state = "open" print("Kitchen ${true}/${false}; table: ${state}") }\nentry(serve)\n')
  await writeFile(typedBindingsPath,
    'fn serve() { let table = 6 * 7 let isOpen = true let state = "open" ' +
    'print("Table ${table}; open: ${isOpen}; state: ${state}") }\nentry(serve)\n')
  await writeFile(directCallPath,
    'fn announce(table: i64, isOpen: Bool) {\n' +
    '  print("Table ${table}; open: ${isOpen}")\n}\n' +
    'fn main() { announce(isOpen: true, table: 6 * 7) }\nentry(main)\n')
  await writeFile(scalarReturnPath,
    'fn tableNumber(): i64 { return 6 * 7 }\n' +
    'fn main() { let table = tableNumber() ' +
    'print("Table ${table}") }\nentry(main)\n')
  await writeFile(boolReturnPath,
    'fn kitchenOpen(): Bool { return true }\n' +
    'fn main() { let open = kitchenOpen() ' +
    'print("Open ${open}") }\nentry(main)\n')
  await writeFile(checkedOverflowPath,
    'fn addOne(value: i64): i64 { return value + 1 }\n' +
    'fn main() { let result = addOne(value: 9223372036854775807) ' +
    'print("success ${result}") }\nentry(main)\n')
  await writeFile(runtimeMinimumRemainderPath,
    'fn remainder(value: i64, by divisor: i64): i64 { return value % divisor }\n' +
    'fn main() { let result = remainder(' +
    'value: 0 - 9223372036854775807 - 1, by: 0 - 1) ' +
    'print("${result}") }\nentry(main)\n')
  await writeFile(runtimeDivisionZeroPath,
    'fn divide(value: i64, by divisor: i64): i64 { return value / divisor }\n' +
    'fn main() { let result = divide(value: 8, by: 0) ' +
    'print("success ${result}") }\nentry(main)\n')
  await writeFile(runtimeDivisionOverflowPath,
    'fn divide(value: i64, by divisor: i64): i64 { return value / divisor }\n' +
    'fn main() { let result = divide(' +
    'value: 0 - 9223372036854775807 - 1, by: 0 - 1) ' +
    'print("success ${result}") }\nentry(main)\n')
  await writeFile(runtimeRemainderZeroPath,
    'fn remainder(value: i64, by divisor: i64): i64 { return value % divisor }\n' +
    'fn main() { let result = remainder(value: 8, by: 0) ' +
    'print("success ${result}") }\nentry(main)\n')
  await writeFile(runtimeNegationOverflowPath,
    'fn negate(value: i64): i64 { return -value }\n' +
    'fn main() { let result = negate(' +
    'value: 0 - 9223372036854775807 - 1) ' +
    'print("success ${result}") }\nentry(main)\n')
  await writeFile(runtimeShiftCountPath,
    'fn shift(value: UInt, count: UInt): UInt { return value >> count }\n' +
    'entry { let result = shift(value: 1_u64, count: 64_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeUnsignedShiftOverflowPath,
    'fn shift(value: UInt, count: UInt): UInt { return value << count }\n' +
    'entry { let result = shift(' +
    'value: 18446744073709551615_u64, count: 1_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeSignedShiftOverflowPath,
    'fn shift(value: Int, count: UInt): Int { return value << count }\n' +
    'entry { let result = shift(' +
    'value: 9223372036854775807, count: 1_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeSignedPowerOverflowPath,
    'fn power(base: Int, exponent: UInt): Int { return base ** exponent }\n' +
    'entry { let result = power(' +
    'base: 9223372036854775807, exponent: 2_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeUnsignedPowerOverflowPath,
    'fn power(base: UInt, exponent: UInt): UInt { return base ** exponent }\n' +
    'entry { let result = power(' +
    'base: 18446744073709551615_u64, exponent: 2_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeUIntAddOverflowPath,
    'fn add(left: UInt, right: UInt): UInt { return left + right }\n' +
    'entry { let result = add(left: 18446744073709551615_u64, right: 1_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeUIntSubtractUnderflowPath,
    'fn subtract(left: UInt, right: UInt): UInt { return left - right }\n' +
    'entry { let result = subtract(left: 0_u64, right: 1_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeUIntMultiplyOverflowPath,
    'fn multiply(left: UInt, right: UInt): UInt { return left * right }\n' +
    'entry { let result = multiply(' +
    'left: 18446744073709551615_u64, right: 2_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeUIntDivisionZeroPath,
    'fn divide(left: UInt, right: UInt): UInt { return left / right }\n' +
    'entry { let result = divide(left: 8_u64, right: 0_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeUIntRemainderZeroPath,
    'fn remainder(left: UInt, right: UInt): UInt { return left % right }\n' +
    'entry { let result = remainder(left: 8_u64, right: 0_u64) ' +
    'print("success ${result}") }\n')
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
    { name: "restaurant-if", source: restaurantIfPath,
      expected: Buffer.from(
        "Kitchen open\nAfter service\nKitchen closed\nAfter service\n", "utf8") },
    { name: "restaurant-nested-if", source: restaurantNestedIfPath,
      expected: Buffer.from(
        "Restaurant open\nKitchen ready\nOpen branch joined\nPost-join service\n" +
        "Restaurant open\nKitchen closed\nOpen branch joined\nPost-join service\n" +
        "Restaurant closed\nKitchen ready\nClosed branch joined\nPost-join service\n" +
        "Restaurant closed\nKitchen closed\nClosed branch joined\nPost-join service\n",
        "utf8") },
    { name: "restaurant-nested-scalar-if", source: restaurantNestedScalarIfPath,
      expected: Buffer.from("1,2,3\n", "utf8") },
    { name: "restaurant-checked-arithmetic", source: restaurantCheckedArithmeticFixture,
      expected: Buffer.from("Open 6; closed 1\n", "utf8") },
    { name: "two-calls", source: twoCallsPath,
      expected: Buffer.from("a\nb\n", "utf8") },
    { name: "typed-arithmetic", source: arithmeticPath,
      expected: Buffer.from("10 -6 16 4 2\n", "utf8") },
    { name: "percent-interpolation", source: percentPath,
      expected: Buffer.from("Load 42%\n", "utf8") },
    { name: "nul-interpolation", source: interpolationNulPath,
      expected: Buffer.from([0x41, 0x00, 0x34, 0x32, 0x0a]) },
    { name: "nul-string-value", source: stringValueNulPath,
      expected: Buffer.from([0x41, 0x00, 0x42, 0x0a]) },
    { name: "minimum-i64", source: minimumI64Path,
      expected: Buffer.from("-9223372036854775808\n", "utf8") },
    { name: "builtin-display", source: builtinDisplayPath,
      expected: Buffer.from("Kitchen true/false; table: open\n", "utf8") },
    { name: "typed-bindings", source: typedBindingsPath,
      expected: Buffer.from("Table 42; open: true; state: open\n", "utf8") },
    { name: "direct-call", source: directCallPath,
      expected: Buffer.from("Table 42; open: true\n", "utf8") },
    { name: "scalar-return", source: scalarReturnPath,
      expected: Buffer.from("Table 42\n", "utf8") },
    { name: "bool-return", source: boolReturnPath,
      expected: Buffer.from("Open true\n", "utf8") },
    { name: "restaurant-runtime-divrem", source: restaurantRuntimeDivremFixture,
      expected: Buffer.from("Each 7; left 2\n", "utf8") },
    { name: "runtime-minimum-remainder", source: runtimeMinimumRemainderPath,
      expected: Buffer.from("0\n", "utf8") },
    { name: "restaurant-unary-negate", source: restaurantUnaryNegateFixture,
      expected: Buffer.from("Balance -7\n", "utf8") },
    { name: "direct-unary-interpolation",
      source: restaurantUnaryInterpolationFixture,
      expected: Buffer.from("Balance -7\n", "utf8") },
    { name: "restaurant-mutation", source: restaurantMutationFixture,
      expected: Buffer.from("Open 6\n", "utf8") },
    { name: "restaurant-conditional-mutation",
      source: restaurantConditionalMutationFixture,
      expected: Buffer.from("Open 6; closed 4\n", "utf8") },
    { name: "restaurant-bool-mutation", source: restaurantBoolMutationFixture,
      expected: Buffer.from("Open true; closed false\n", "utf8") },
    { name: "restaurant-branch-mutation",
      source: restaurantBranchMutationFixture,
      expected: Buffer.from("Open 6; closed 4\n", "utf8") },
    { name: "restaurant-branch-mutation-multi",
      source: restaurantMultiBranchMutationFixture,
      expected: Buffer.from("Open 18; closed -4\n", "utf8") },
    { name: "restaurant-while", source: restaurantWhileFixture,
      expected: Buffer.from("Served 3\n", "utf8") },
    { name: "restaurant-wmo", source: restaurantWmoFixture,
      expected: Buffer.from("Bill 42\n", "utf8") },
    { name: "restaurant-async-join", source: restaurantAsyncJoinFixture,
      expected: Buffer.from("Prepared 42\n", "utf8") },
    { name: "restaurant-async-yield", source: restaurantAsyncYieldFixture,
      expected: Buffer.from("Prepared 88\n", "utf8") },
    { name: "restaurant-bitwise", source: restaurantBitwiseFixture,
      expected: Buffer.from("Flags 14/-15\n", "utf8") },
    { name: "restaurant-unsigned", source: restaurantUnsignedFixture,
      expected: Buffer.from("Unsigned 18446744073709551615\n", "utf8") },
    { name: "restaurant-shifts", source: restaurantShiftsFixture,
      expected: Buffer.from("Shifts -4/15/-48/48\n", "utf8") },
    { name: "restaurant-power", source: restaurantPowerFixture,
      expected: Buffer.from("Power -27/1024/1/512\n", "utf8") },
    { name: "restaurant-power-prefix", source: restaurantPowerPrefixFixture,
      expected: Buffer.from("Power prefix -4/4/512/-9/-27\n", "utf8") },
    { name: "restaurant-compound", source: restaurantCompoundFixture,
      expected: Buffer.from("Compound 11\n", "utf8") },
    { name: "restaurant-f64-strict", source: restaurantF64StrictFixture,
      expected: Buffer.from("Float strict ok\n", "utf8") },
    { name: "restaurant-checked-integer-arithmetic",
      source: restaurantCheckedIntegerArithmeticFixture,
      expected: Buffer.from(
        "i8 -9/-15/-36; compound -22\nu8 43/37/120; compound 82\n" +
        "i16 -970/-1030/-30000; compound -1944\n" +
        "u16 1030/970/30000; compound 2056\n" +
        "i32 -117000/-123000/-360000000; compound -234004\n" +
        "u32 100300/99700/30000000; compound 200596\n" +
        "i64 -600000/-1200000/-270000000000; compound -1200004\n" +
        "u64 6000000000/4000000000/5000000000000000000; compound 11999999996\n" +
        "Int -4000000000/-6000000000/-5000000000000000000; " +
        "compound -8000000004\n" +
        "UInt 9000000000/3000000000/18000000000000000000; " +
        "compound 17999999996\n", "utf8") },
    { name: "restaurant-integer-wrapping", source: restaurantIntegerWrappingFixture,
      expected: Buffer.from(
        "i8/u8 -128/0\ni16/u16 32767/2\ni32/u32 -2/4294967295\n" +
        "i64/u64 -9223372036854775808/0\n" +
        "Int/UInt -9223372036854775808/18446744073709551615\n", "utf8") },
    { name: "restaurant-integer-widening", source: restaurantIntegerWideningFixture,
      expected: Buffer.from("Widen -7/200/202/203\n", "utf8") },
    { name: "restaurant-uint-wrapping-add",
      source: restaurantUIntWrappingAddFixture,
      expected: Buffer.from("Wrapped 0\n", "utf8") },
    { name: "restaurant-uint-wrapping-subtract",
      source: restaurantUIntWrappingSubtractFixture,
      expected: Buffer.from("Wrapped 18446744073709551615\n", "utf8") },
    { name: "restaurant-uint-wrapping-multiply",
      source: restaurantUIntWrappingMultiplyFixture,
      expected: Buffer.from("Wrapped 18446744073709551614\n", "utf8") },
    { name: "restaurant-uint-wrapping-negate",
      source: restaurantUIntWrappingNegateFixture,
      expected: Buffer.from("Wrapped 18446744073709551615\n", "utf8") },
    { name: "restaurant-uint-wrapping-power",
      source: restaurantUIntWrappingPowerFixture,
      expected: Buffer.from("Wrapped 12157665459056928801\n", "utf8") },
    { name: "restaurant-uint-overflowing-power",
      source: restaurantUIntOverflowingPowerFixture,
      expected: Buffer.from(
        "Overflowing power 9223372036854775808/false; 0/true; 1/true; " +
        "1/false\n", "utf8") },
    { name: "restaurant-uint-overflowing-family",
      source: restaurantUIntOverflowingFamilyFixture,
      expected: Buffer.from(
        "Overflowing family add 0/true,11/false; subtract 41/false," +
        "18446744073709551615/true; multiply 42/false," +
        "18446744073709551614/true; negate 0/false," +
        "18446744073709551615/true; power 9223372036854775808/false," +
        "0/true,1/true,1/false\n", "utf8") },
    { name: "restaurant-uint-wrapping-shift-left",
      source: restaurantUIntWrappingShiftLeftFixture,
      expected: Buffer.from("Wrapped 18446744073709551614\n", "utf8") },
    { name: "restaurant-uint-masked-shift-left",
      source: restaurantUIntMaskedShiftLeftFixture,
      expected: Buffer.from("Masked 2\n", "utf8") },
    { name: "restaurant-uint-masked-shift-right",
      source: restaurantUIntMaskedShiftRightFixture,
      expected: Buffer.from("Masked 64\n", "utf8") },
    { name: "restaurant-uint-logical-shift-right",
      source: restaurantUIntLogicalShiftRightFixture,
      expected: Buffer.from("Logical 64\n", "utf8") },
    { name: "restaurant-uint-rotated-left",
      source: restaurantUIntRotatedLeftFixture,
      expected: Buffer.from("Rotated 3\n", "utf8") },
    { name: "restaurant-uint-rotated-right",
      source: restaurantUIntRotatedRightFixture,
      expected: Buffer.from("Rotated 9223372036854775809\n", "utf8") },
    { name: "restaurant-uint-count-ones",
      source: restaurantUIntCountOnesFixture,
      expected: Buffer.from("Ones 32\n", "utf8") },
    { name: "restaurant-uint-count-zeros",
      source: restaurantUIntCountZerosFixture,
      expected: Buffer.from("Zeros 32\n", "utf8") },
    { name: "restaurant-uint-leading-zeros",
      source: restaurantUIntLeadingZerosFixture,
      expected: Buffer.from("Leading 56/64\n", "utf8") },
    { name: "restaurant-uint-trailing-zeros",
      source: restaurantUIntTrailingZerosFixture,
      expected: Buffer.from("Trailing 12/64\n", "utf8") },
    { name: "restaurant-uint-reversed-bits",
      source: restaurantUIntReversedBitsFixture,
      expected: Buffer.from("Bits 17848844570815808640\n", "utf8") },
    { name: "restaurant-uint-reversed-bytes",
      source: restaurantUIntReversedBytesFixture,
      expected: Buffer.from("Bytes 17279655951921914625\n", "utf8") },
    { name: "restaurant-uint-saturating-add",
      source: restaurantUIntSaturatingAddFixture,
      expected: Buffer.from("Saturated 18446744073709551615/11\n", "utf8") },
    { name: "restaurant-uint-saturating-subtract",
      source: restaurantUIntSaturatingSubtractFixture,
      expected: Buffer.from("Saturated subtract 0/10\n", "utf8") },
    { name: "restaurant-uint-saturating-multiply",
      source: restaurantUIntSaturatingMultiplyFixture,
      expected: Buffer.from("Saturated multiply 18446744073709551615/42\n", "utf8") },
    { name: "restaurant-uint-saturating-policy",
      source: restaurantUIntSaturatingPolicyFixture,
      expected: Buffer.from(
        "Saturating policy add 18446744073709551615/11; subtract 0/10; " +
        "multiply 18446744073709551615/42; negate 0/0; power " +
        "8/18446744073709551615/1\n", "utf8") },
    { name: "restaurant-uint-bit-not", source: restaurantUIntBitNotFixture,
      expected: Buffer.from("UInt not 18446744073709551615\n", "utf8") },
    { name: "restaurant-uint-bitwise", source: restaurantUIntBitwiseFixture,
      expected: Buffer.from(
        "Not 18446744073709551615\nAnd 0\nOr 18446744073709551615\n" +
        "Xor 18446744073709551615\nOnes 32\nZeros 32\nLeading 56\n" +
        "Leading zero 64\nTrailing 12\nTrailing zero 64\n", "utf8") },
    { name: "restaurant-uint-compound", source: restaurantUIntCompoundFixture,
      expected: Buffer.from(
        "UInt compound 4611686018427387907/4611686018427387906/" +
        "9223372036854775812/4611686018427387906/4611686018427387906/" +
        "4611686018427387906/9223372036854775812/4611686018427387906/" +
        "2/87/95\n", "utf8") },
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
      "--convert-scf-to-cf", "--convert-cf-to-llvm", "--verify-each"],
    `${product.name} mlir-opt`)
    if (product.name === "restaurant-while") {
      const lowered = await readFile(verified)
      assert(!lowered.includes("scf.") && lowered.includes("llvm.cond_br") &&
        lowered.includes("llvm.br"),
      "natural loop was not lowered from SCF to the LLVM dialect CFG")
    }
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
  const overflowGenerated = run(seedGate, [checkedOverflowPath])
  assert(overflowGenerated.exitCode === 0 && overflowGenerated.stderr.length === 0 &&
    overflowGenerated.stdout.length > 0,
  `checked-overflow source route failed with ${overflowGenerated.exitCode}: ` +
    overflowGenerated.stderrText.trim())
  const overflowInput = resolve(artifactDirectory, "checked-overflow.mlir")
  const overflowVerified = resolve(artifactDirectory, "checked-overflow.verified.mlir")
  const overflowLlvm = resolve(artifactDirectory, "checked-overflow.ll")
  const overflowExecutable = resolve(artifactDirectory, "checked-overflow.native")
  await writeFile(overflowInput, overflowGenerated.stdout)
  const overflowInputForTool = isWindows ? wslPath(overflowInput) : overflowInput
  const overflowVerifiedForTool = isWindows ? wslPath(overflowVerified) : overflowVerified
  const overflowLlvmForTool = isWindows ? wslPath(overflowLlvm) : overflowLlvm
  const overflowExecutableForTool = isWindows ? wslPath(overflowExecutable) : overflowExecutable
  invokeTool(tool("mlirOpt"), [overflowInputForTool, "-o", overflowVerifiedForTool,
    "--convert-scf-to-cf", "--convert-cf-to-llvm", "--verify-each"],
  "checked-overflow mlir-opt")
  invokeTool(tool("mlirTranslate"), ["--mlir-to-llvmir", overflowVerifiedForTool,
    "-o", overflowLlvmForTool], "checked-overflow mlir-translate")
  invokeTool(tool("clang"), ["-x", "ir", `--target=${targetTriple}`,
    overflowLlvmForTool, "-o", overflowExecutableForTool],
  "checked-overflow clang LLVM IR")
  const overflowExecution = invokeProgram(overflowExecutableForTool, [],
    "checked-overflow generated executable")
  assert(overflowExecution.exitCode !== 0,
    "checked-overflow generated executable returned success")
  assert(overflowExecution.stdout.length === 0 &&
    !Buffer.from(overflowExecution.stdout).includes(Buffer.from("success")),
  "checked-overflow published output after the trap")
  for (const fault of [
    { name: "runtime-division-zero", source: runtimeDivisionZeroPath },
    { name: "runtime-division-overflow", source: runtimeDivisionOverflowPath },
    { name: "runtime-remainder-zero", source: runtimeRemainderZeroPath },
    { name: "runtime-negation-overflow", source: runtimeNegationOverflowPath },
    { name: "runtime-shift-count", source: runtimeShiftCountPath },
    { name: "runtime-unsigned-shift-overflow",
      source: runtimeUnsignedShiftOverflowPath },
    { name: "runtime-signed-shift-overflow",
      source: runtimeSignedShiftOverflowPath },
    { name: "runtime-signed-power-overflow",
      source: runtimeSignedPowerOverflowPath },
    { name: "runtime-unsigned-power-overflow",
      source: runtimeUnsignedPowerOverflowPath },
    { name: "runtime-uint-add-overflow", source: runtimeUIntAddOverflowPath },
    { name: "runtime-uint-subtract-underflow",
      source: runtimeUIntSubtractUnderflowPath },
    { name: "runtime-uint-multiply-overflow",
      source: runtimeUIntMultiplyOverflowPath },
    { name: "runtime-uint-division-zero", source: runtimeUIntDivisionZeroPath },
    { name: "runtime-uint-remainder-zero", source: runtimeUIntRemainderZeroPath },
    { name: "explicit-panic", source: explicitPanicFixture },
  ]) {
    const generated = run(seedGate, [fault.source])
    assert(generated.exitCode === 0 && generated.stderr.length === 0 &&
      generated.stdout.length > 0,
    `${fault.name} source route did not emit checked MLIR`)
    const input = resolve(artifactDirectory, `${fault.name}.mlir`)
    const verified = resolve(artifactDirectory, `${fault.name}.verified.mlir`)
    const llvm = resolve(artifactDirectory, `${fault.name}.ll`)
    const executable = resolve(artifactDirectory, `${fault.name}.native`)
    await writeFile(input, generated.stdout)
    const inputForTool = isWindows ? wslPath(input) : input
    const verifiedForTool = isWindows ? wslPath(verified) : verified
    const llvmForTool = isWindows ? wslPath(llvm) : llvm
    const executableForTool = isWindows ? wslPath(executable) : executable
    invokeTool(tool("mlirOpt"), [inputForTool, "-o", verifiedForTool,
      "--convert-scf-to-cf", "--convert-cf-to-llvm", "--verify-each"],
    `${fault.name} mlir-opt`)
    invokeTool(tool("mlirTranslate"), ["--mlir-to-llvmir", verifiedForTool,
      "-o", llvmForTool], `${fault.name} mlir-translate`)
    invokeTool(tool("clang"), ["-x", "ir", `--target=${targetTriple}`,
      llvmForTool, "-o", executableForTool], `${fault.name} clang LLVM IR`)
    const execution = invokeProgram(executableForTool, [],
      `${fault.name} generated executable`)
    assert(execution.exitCode !== 0 && execution.stdout.length === 0,
      `${fault.name} did not trap before publishing output`)
    if (fault.name === "explicit-panic") {
      const artifact = await readFile(input)
      assert(artifact.includes("llvm.intr.trap") &&
        artifact.includes("llvm.unreachable") &&
        !artifact.includes("explicit invariant failure"),
      "explicit panic did not lower to a stripped terminal trap")
    }
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
  for (const name of ["restaurant-interpolation", "restaurant-checked-arithmetic",
    "typed-arithmetic", "percent-interpolation", "nul-interpolation",
    "minimum-i64"]) {
    const artifact = artifacts.get(name)
    assert(artifact.includes("@w_seed_append_i64") &&
      !artifact.includes("snprintf") && !artifact.includes("%ld") &&
      !artifact.includes("vararg"),
    `${name} did not use the internal bounded i64 writer`)
  }
  for (const name of ["builtin-display", "typed-bindings"]) {
    const artifact = artifacts.get(name)
    assert(artifact.includes("@w_seed_append_bool") &&
      !artifact.includes("snprintf") && !artifact.includes("vararg"),
    `${name} did not retain typed Boolean lowering`)
  }
  const checkedArithmeticArtifact = artifacts.get("restaurant-checked-arithmetic")
    assert(checkedArithmeticArtifact.includes(
    "llvm.call @w_seed_checked_add_i64") &&
    checkedArithmeticArtifact.includes("llvm.call @w_seed_checked_subtract_i64") &&
    checkedArithmeticArtifact.includes("llvm.intr.sadd.with.overflow") &&
    checkedArithmeticArtifact.includes("llvm.intr.ssub.with.overflow") &&
    !checkedArithmeticArtifact.includes("llvm.add %v") &&
      !checkedArithmeticArtifact.includes("llvm.sub %v"),
  "Restaurant checked arithmetic did not retain checked add/sub lowering")
  const typedArithmeticArtifact = artifacts.get("typed-arithmetic")
  assert(typedArithmeticArtifact.includes("llvm.sdiv %v") &&
    typedArithmeticArtifact.includes("llvm.srem %v"),
  "constant division and remainder did not retain LLVM arithmetic lowering")
  const signedBitwiseArtifact = artifacts.get("restaurant-bitwise")
  assert(signedBitwiseArtifact.includes("llvm.and ") &&
    signedBitwiseArtifact.includes("llvm.xor ") &&
    signedBitwiseArtifact.includes("llvm.or ") &&
    signedBitwiseArtifact.includes(
      "_bit_not_mask = llvm.mlir.constant(-1 : i64)") &&
    signedBitwiseArtifact.includes("llvm.call @w_fn_0") &&
    !signedBitwiseArtifact.includes("w_seed_checked_bit"),
  "signed bitwise operations were folded, reordered, or helper-lowered")
  const unsignedArtifact = artifacts.get("restaurant-unsigned")
  assert(unsignedArtifact.includes("llvm.mlir.constant(-1 : i64) : i64") &&
    unsignedArtifact.includes("llvm.func internal @w_seed_append_u64") &&
    unsignedArtifact.includes("llvm.call @w_seed_append_u64") &&
    unsignedArtifact.includes("llvm.udiv") &&
    unsignedArtifact.includes("llvm.urem") &&
    unsignedArtifact.includes("llvm.call @w_fn_0") &&
    !unsignedArtifact.includes("llvm.call @w_seed_append_i64"),
  "UInt did not retain its unsigned full-width lowering and formatter")
  const shiftsArtifact = artifacts.get("restaurant-shifts")
  assert(shiftsArtifact.includes(
    "llvm.func internal @w_seed_checked_shift_left_i64") &&
    shiftsArtifact.includes(
      "llvm.func internal @w_seed_checked_shift_left_u64") &&
    shiftsArtifact.includes(
      "llvm.func internal @w_seed_checked_shift_right_i64") &&
    shiftsArtifact.includes(
      "llvm.func internal @w_seed_checked_shift_right_u64") &&
    shiftsArtifact.includes('llvm.icmp "uge" %count, %width : i64') &&
    shiftsArtifact.includes("llvm.shl %left, %count : i64") &&
    shiftsArtifact.includes("llvm.ashr %left, %count : i64") &&
    shiftsArtifact.includes("llvm.lshr %left, %count : i64") &&
    shiftsArtifact.includes(
      "llvm.call @w_seed_checked_shift_right_i64") &&
    shiftsArtifact.includes(
      "llvm.call @w_seed_checked_shift_right_u64") &&
    shiftsArtifact.includes(
      "llvm.call @w_seed_checked_shift_left_i64") &&
    shiftsArtifact.includes(
      "llvm.call @w_seed_checked_shift_left_u64"),
  "checked shifts lost width guards, signedness, or runtime lowering")
  const powerArtifact = artifacts.get("restaurant-power")
  assert(powerArtifact.includes(
    "llvm.func internal @w_seed_checked_power_i64") &&
    powerArtifact.includes(
      "llvm.func internal @w_seed_checked_power_u64") &&
    powerArtifact.includes("llvm.intr.smul.with.overflow") &&
    powerArtifact.includes("llvm.intr.umul.with.overflow") &&
    powerArtifact.includes("llvm.lshr %remaining, %one : i64") &&
    powerArtifact.includes("llvm.call @w_seed_checked_power_i64") &&
    powerArtifact.includes("llvm.call @w_seed_checked_power_u64"),
  "checked power lost exponentiation-by-squaring or signedness")
  const f64Artifact = artifacts.get("restaurant-f64-strict").toString("utf8")
  assert(f64Artifact.includes("llvm.fadd") &&
    f64Artifact.includes("llvm.fsub") &&
    f64Artifact.includes("llvm.fmul") &&
    f64Artifact.includes("llvm.fdiv") &&
    f64Artifact.includes("llvm.fneg") &&
    f64Artifact.includes('llvm.fcmp "oeq"') &&
    f64Artifact.includes('llvm.fcmp "une"') &&
    f64Artifact.includes('llvm.fcmp "olt"') &&
    f64Artifact.includes('llvm.fcmp "ole"') &&
    f64Artifact.includes('llvm.fcmp "ogt"') &&
    f64Artifact.includes('llvm.fcmp "oge"') &&
    f64Artifact.includes("0x3ff8000000000000 : f64") &&
    !f64Artifact.includes("fastmath"),
  "strict f64 lowering lost an operator, predicate, bit pattern, or strict mode")
  const checkedIntegerArithmeticArtifact =
    artifacts.get("restaurant-checked-integer-arithmetic").toString("utf8")
  assert(checkedIntegerArithmeticArtifact.includes("@w_seed_checked_add_i64") &&
    checkedIntegerArithmeticArtifact.includes("@w_seed_checked_subtract_i64") &&
    checkedIntegerArithmeticArtifact.includes("@w_seed_checked_multiply_i64") &&
    checkedIntegerArithmeticArtifact.includes("@w_seed_checked_add_u64") &&
    checkedIntegerArithmeticArtifact.includes("@w_seed_checked_subtract_u64") &&
    checkedIntegerArithmeticArtifact.includes("@w_seed_checked_multiply_u64") &&
    checkedIntegerArithmeticArtifact.includes("llvm.intr.sadd.with.overflow") &&
    checkedIntegerArithmeticArtifact.includes("llvm.intr.usub.with.overflow") &&
    checkedIntegerArithmeticArtifact.includes("llvm.intr.smul.with.overflow") &&
    checkedIntegerArithmeticArtifact.includes("llvm.intr.uadd.with.overflow") &&
    checkedIntegerArithmeticArtifact.includes("llvm.intr.umul.with.overflow") &&
    checkedIntegerArithmeticArtifact.includes("%narrow_overflow = llvm.icmp \"ne\"") &&
    checkedIntegerArithmeticArtifact.includes("llvm.cond_br %invalid, ^checked_overflow, ^checked_ok") &&
    checkedIntegerArithmeticArtifact.includes("llvm.mlir.constant(8 : i64)") &&
    checkedIntegerArithmeticArtifact.includes("llvm.mlir.constant(16 : i64)") &&
    checkedIntegerArithmeticArtifact.includes("llvm.mlir.constant(32 : i64)"),
  "checked integer arithmetic lost signedness, overflow, logical width, or trapping")
  const uintWrappingAddArtifact =
    artifacts.get("restaurant-uint-wrapping-add").toString("utf8")
  assert(uintWrappingAddArtifact.includes("llvm.add %p0,") &&
    uintWrappingAddArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingAddArtifact.includes("@w_seed_checked_add_u64") &&
    !uintWrappingAddArtifact.includes("llvm.intr.uadd.with.overflow"),
  "u64.wrappingAdd did not retain direct wrapping u64 lowering")
  const uintWrappingSubtractArtifact =
    artifacts.get("restaurant-uint-wrapping-subtract").toString("utf8")
  assert(uintWrappingSubtractArtifact.includes("llvm.sub %p0,") &&
    uintWrappingSubtractArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingSubtractArtifact.includes("@w_seed_checked_subtract_u64") &&
    !uintWrappingSubtractArtifact.includes("llvm.intr.usub.with.overflow"),
  "u64.wrappingSubtract did not retain direct wrapping u64 lowering")
  const uintWrappingMultiplyArtifact =
    artifacts.get("restaurant-uint-wrapping-multiply").toString("utf8")
  assert(uintWrappingMultiplyArtifact.includes("llvm.mul %p0,") &&
    uintWrappingMultiplyArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingMultiplyArtifact.includes("@w_seed_checked_multiply_u64") &&
    !uintWrappingMultiplyArtifact.includes("llvm.intr.umul.with.overflow"),
  "u64.wrappingMultiply did not retain direct wrapping u64 lowering")
  const uintWrappingNegateArtifact =
    artifacts.get("restaurant-uint-wrapping-negate").toString("utf8")
  assert(uintWrappingNegateArtifact.includes("llvm.mlir.constant(0 : i64)") &&
    uintWrappingNegateArtifact.includes("llvm.sub") &&
    uintWrappingNegateArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingNegateArtifact.includes("@w_seed_checked_subtract_u64") &&
    !uintWrappingNegateArtifact.includes("llvm.intr.usub.with.overflow"),
  "u64.wrappingNegate did not retain direct wrapping u64 lowering")
  const uintWrappingPowerArtifact =
    artifacts.get("restaurant-uint-wrapping-power").toString("utf8")
  assert(uintWrappingPowerArtifact.includes(
    "llvm.func internal @w_seed_wrapping_power_u64") &&
    uintWrappingPowerArtifact.includes(
      "llvm.call @w_seed_wrapping_power_u64") &&
    uintWrappingPowerArtifact.includes("llvm.mul") &&
    uintWrappingPowerArtifact.includes("llvm.lshr") &&
    uintWrappingPowerArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingPowerArtifact.includes("@w_seed_checked_power_u64") &&
    !uintWrappingPowerArtifact.includes("llvm.intr.umul.with.overflow"),
  "u64.wrappingPower did not retain runtime modulo exponentiation lowering")
  const uintOverflowingPowerArtifact =
    artifacts.get("restaurant-uint-overflowing-power").toString("utf8")
  assert((uintOverflowingPowerArtifact.match(
    /llvm\.func internal @w_seed_overflowing_power_u64/g) ?? []).length === 1 &&
    uintOverflowingPowerArtifact.includes(
      "llvm.call @w_seed_overflowing_power_u64") &&
    uintOverflowingPowerArtifact.includes("llvm.intr.umul.with.overflow") &&
    uintOverflowingPowerArtifact.includes("llvm.or") &&
    uintOverflowingPowerArtifact.includes("llvm.lshr") &&
    uintOverflowingPowerArtifact.includes("llvm.call @w_seed_append_u64") &&
    uintOverflowingPowerArtifact.includes("llvm.call @w_seed_append_bool") &&
    !uintOverflowingPowerArtifact.includes("@w_seed_checked_power_u64"),
  "u64.overflowingPower lost sticky-overflow exponentiation lowering")
  const uintOverflowingFamilyArtifact =
    artifacts.get("restaurant-uint-overflowing-family").toString("utf8")
  assert((uintOverflowingFamilyArtifact.match(
    /llvm\.intr\.uadd\.with\.overflow/g) ?? []).length === 2 &&
    (uintOverflowingFamilyArtifact.match(
    /llvm\.intr\.usub\.with\.overflow/g) ?? []).length === 4 &&
    (uintOverflowingFamilyArtifact.match(
      /llvm\.intr\.umul\.with\.overflow/g) ?? []).length === 4 &&
    (uintOverflowingFamilyArtifact.match(/llvm\.extractvalue/g) ?? []).length >= 16 &&
    (uintOverflowingFamilyArtifact.match(
      /llvm\.func internal @w_seed_overflowing_power_u64/g) ?? []).length === 1 &&
    (uintOverflowingFamilyArtifact.match(
      /llvm\.call @w_seed_overflowing_power_u64/g) ?? []).length === 4 &&
    uintOverflowingFamilyArtifact.includes("llvm.call @w_seed_append_u64") &&
    uintOverflowingFamilyArtifact.includes("llvm.call @w_seed_append_bool") &&
    !uintOverflowingFamilyArtifact.includes("@w_seed_checked_subtract_u64") &&
    !uintOverflowingFamilyArtifact.includes("@w_seed_checked_multiply_u64") &&
    !uintOverflowingFamilyArtifact.includes("@w_seed_checked_negate_u64"),
  "u64 overflowing arithmetic family lost direct tuple or power lowering")
  const uintWrappingShiftLeftArtifact =
    artifacts.get("restaurant-uint-wrapping-shift-left").toString("utf8")
  assert((uintWrappingShiftLeftArtifact.match(
    /llvm\.func internal @w_seed_wrapping_shift_left_u64/g) ?? []).length === 1 &&
    uintWrappingShiftLeftArtifact.includes(
      "llvm.call @w_seed_wrapping_shift_left_u64") &&
    uintWrappingShiftLeftArtifact.includes('llvm.icmp "uge"') &&
    uintWrappingShiftLeftArtifact.includes("llvm.shl") &&
    uintWrappingShiftLeftArtifact.includes('"llvm.intr.trap"()') &&
    uintWrappingShiftLeftArtifact.includes("llvm.unreachable") &&
    uintWrappingShiftLeftArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingShiftLeftArtifact.includes("@w_seed_checked_shift_left_u64") &&
    !uintWrappingShiftLeftArtifact.includes("llvm.intr.ushl.with.overflow"),
  "u64.wrappingShiftLeft lost its count guard or gained checked-shift semantics")
  const uintMaskedShiftLeftArtifact =
    artifacts.get("restaurant-uint-masked-shift-left").toString("utf8")
  assert((uintMaskedShiftLeftArtifact.match(
    /llvm\.func internal @w_seed_masked_shift_left_u64/g) ?? []).length === 1 &&
    uintMaskedShiftLeftArtifact.includes(
      "llvm.call @w_seed_masked_shift_left_u64") &&
    uintMaskedShiftLeftArtifact.includes("llvm.and %count, %mask : i64") &&
    uintMaskedShiftLeftArtifact.includes("llvm.shl %left, %masked_count : i64") &&
    uintMaskedShiftLeftArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintMaskedShiftLeftArtifact.includes("@w_seed_checked_shift_left_u64") &&
    !uintMaskedShiftLeftArtifact.includes("llvm.intr.ushl.with.overflow"),
  "u64.maskedShiftLeft lost count masking or gained checked-shift semantics")
  const uintMaskedShiftRightArtifact =
    artifacts.get("restaurant-uint-masked-shift-right").toString("utf8")
  assert((uintMaskedShiftRightArtifact.match(
    /llvm\.func internal @w_seed_masked_shift_right_u64/g) ?? []).length === 1 &&
    uintMaskedShiftRightArtifact.includes(
      "llvm.call @w_seed_masked_shift_right_u64") &&
    uintMaskedShiftRightArtifact.includes("llvm.and %count, %mask : i64") &&
    uintMaskedShiftRightArtifact.includes("llvm.lshr %left, %masked_count : i64") &&
    uintMaskedShiftRightArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintMaskedShiftRightArtifact.includes("@w_seed_checked_shift_right_u64"),
  "u64.maskedShiftRight lost count masking or unsigned logical shift semantics")
  const uintLogicalShiftRightArtifact =
    artifacts.get("restaurant-uint-logical-shift-right").toString("utf8")
  assert((uintLogicalShiftRightArtifact.match(
    /llvm\.func internal @w_seed_logical_shift_right_u64/g) ?? []).length === 1 &&
    uintLogicalShiftRightArtifact.includes(
      "llvm.call @w_seed_logical_shift_right_u64") &&
    uintLogicalShiftRightArtifact.includes(
      "llvm.icmp \"uge\" %count, %width : i64") &&
    uintLogicalShiftRightArtifact.includes("llvm.lshr %left, %count : i64") &&
    uintLogicalShiftRightArtifact.includes("\"llvm.intr.trap\"() : () -> ()") &&
    uintLogicalShiftRightArtifact.includes("llvm.unreachable") &&
    uintLogicalShiftRightArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintLogicalShiftRightArtifact.includes("llvm.and %count, %mask : i64") &&
    !uintLogicalShiftRightArtifact.includes("@w_seed_checked_shift_right_u64"),
  "u64.logicalShiftRight lost zero fill or invalid-count trap semantics")
  const uintRotatedLeftArtifact =
    artifacts.get("restaurant-uint-rotated-left").toString("utf8")
  assert((uintRotatedLeftArtifact.match(
    /llvm\.func internal @w_seed_rotated_left_u64/g) ?? []).length === 1 &&
    uintRotatedLeftArtifact.includes("llvm.call @w_seed_rotated_left_u64") &&
    (uintRotatedLeftArtifact.match(/llvm\.intr\.fshl/g) ?? []).length === 1 &&
    uintRotatedLeftArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintRotatedLeftArtifact.includes("llvm.and %count, %mask : i64") &&
    !uintRotatedLeftArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.rotatedLeft lost funnel-shift or modulo-width semantics")
  const uintRotatedRightArtifact =
    artifacts.get("restaurant-uint-rotated-right").toString("utf8")
  assert((uintRotatedRightArtifact.match(
    /llvm\.func internal @w_seed_rotated_right_u64/g) ?? []).length === 1 &&
    uintRotatedRightArtifact.includes("llvm.call @w_seed_rotated_right_u64") &&
    (uintRotatedRightArtifact.match(/llvm\.intr\.fshr/g) ?? []).length === 1 &&
    uintRotatedRightArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintRotatedRightArtifact.includes("llvm.and %count, %mask : i64") &&
    !uintRotatedRightArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.rotatedRight lost funnel-shift or modulo-width semantics")
  const uintCountOnesArtifact =
    artifacts.get("restaurant-uint-count-ones").toString("utf8")
  assert((uintCountOnesArtifact.match(/llvm\.intr\.ctpop/g) ?? []).length === 1 &&
    uintCountOnesArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintCountOnesArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.countOnes lost population-count or total-operation semantics")
  const uintCountZerosArtifact =
    artifacts.get("restaurant-uint-count-zeros").toString("utf8")
  assert((uintCountZerosArtifact.match(/llvm\.intr\.ctpop/g) ?? []).length === 1 &&
    uintCountZerosArtifact.includes(
      "_count_width = llvm.mlir.constant(64 : i64)") &&
    uintCountZerosArtifact.includes("_count_ones : i64") &&
    uintCountZerosArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintCountZerosArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.countZeros lost width-minus-popcount or total-operation semantics")
  const uintLeadingZerosArtifact =
    artifacts.get("restaurant-uint-leading-zeros").toString("utf8")
  assert((uintLeadingZerosArtifact.match(/llvm\.intr\.ctlz/g) ?? []).length === 1 &&
    uintLeadingZerosArtifact.includes("is_zero_poison = false") &&
    uintLeadingZerosArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintLeadingZerosArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.countLeadingZeros lost non-poison ctlz or total-operation semantics")
  const uintTrailingZerosArtifact =
    artifacts.get("restaurant-uint-trailing-zeros").toString("utf8")
  assert((uintTrailingZerosArtifact.match(/llvm\.intr\.cttz/g) ?? []).length === 1 &&
    uintTrailingZerosArtifact.includes("is_zero_poison = false") &&
    uintTrailingZerosArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintTrailingZerosArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.countTrailingZeros lost non-poison cttz or total-operation semantics")
  const uintReversedBitsArtifact =
    artifacts.get("restaurant-uint-reversed-bits").toString("utf8")
  assert((uintReversedBitsArtifact.match(/llvm\.intr\.bitreverse/g) ?? []).length === 1 &&
    uintReversedBitsArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintReversedBitsArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.reversedBits lost direct bit-reverse or total-operation semantics")
  const uintReversedBytesArtifact =
    artifacts.get("restaurant-uint-reversed-bytes").toString("utf8")
  assert((uintReversedBytesArtifact.match(/llvm\.intr\.bswap/g) ?? []).length === 1 &&
    uintReversedBytesArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintReversedBytesArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.reversedBytes lost direct byte-swap or total-operation semantics")
  const uintSaturatingAddArtifact =
    artifacts.get("restaurant-uint-saturating-add").toString("utf8")
  assert((uintSaturatingAddArtifact.match(/llvm\.intr\.uadd\.sat/g) ?? []).length === 1 &&
    uintSaturatingAddArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintSaturatingAddArtifact.includes("@w_seed_checked_add_u64") &&
    !uintSaturatingAddArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.saturatingAdd lost direct saturation or total-operation semantics")
  const uintSaturatingSubtractArtifact =
    artifacts.get("restaurant-uint-saturating-subtract").toString("utf8")
  assert((uintSaturatingSubtractArtifact.match(/llvm\.intr\.usub\.sat/g) ?? []).length === 1 &&
    uintSaturatingSubtractArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintSaturatingSubtractArtifact.includes("@w_seed_checked_subtract_u64") &&
    !uintSaturatingSubtractArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.saturatingSubtract lost direct saturation or total-operation semantics")
  const uintSaturatingMultiplyArtifact =
    artifacts.get("restaurant-uint-saturating-multiply").toString("utf8")
  assert((uintSaturatingMultiplyArtifact.match(/llvm\.intr\.umul\.with\.overflow/g) ?? []).length === 1 &&
    uintSaturatingMultiplyArtifact.includes("llvm.extractvalue") &&
    uintSaturatingMultiplyArtifact.includes("llvm.select") &&
    uintSaturatingMultiplyArtifact.includes("llvm.mlir.constant(-1 : i64)") &&
    uintSaturatingMultiplyArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintSaturatingMultiplyArtifact.includes("@w_seed_checked_multiply_u64") &&
    !uintSaturatingMultiplyArtifact.includes("llvm.intr.umul.sat") &&
    !uintSaturatingMultiplyArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.saturatingMultiply lost inline saturation or total-operation semantics")
  const uintSaturatingPolicyArtifact =
    artifacts.get("restaurant-uint-saturating-policy").toString("utf8")
  assert((uintSaturatingPolicyArtifact.match(/llvm\.intr\.uadd\.sat/g) ?? []).length === 1 &&
    (uintSaturatingPolicyArtifact.match(/llvm\.intr\.usub\.sat/g) ?? []).length === 2 &&
    (uintSaturatingPolicyArtifact.match(/llvm\.func internal @w_seed_saturating_power_u64/g) ?? []).length === 1 &&
    (uintSaturatingPolicyArtifact.match(/llvm\.call @w_seed_saturating_power_u64/g) ?? []).length === 1 &&
    (uintSaturatingPolicyArtifact.match(/llvm\.intr\.umul\.with\.overflow/g) ?? []).length === 3 &&
    uintSaturatingPolicyArtifact.includes("llvm.select %acc_overflow, %max") &&
    uintSaturatingPolicyArtifact.includes("llvm.cond_br %last, ^saturating_power_done") &&
    uintSaturatingPolicyArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintSaturatingPolicyArtifact.includes("@w_seed_checked_power_u64"),
  "u64 saturating policy lost exact power saturation, negate, or final-square reachability evidence")
  const uintBitNotArtifact =
    artifacts.get("restaurant-uint-bit-not").toString("utf8")
  assert(uintBitNotArtifact.includes("llvm.xor") &&
    uintBitNotArtifact.includes("-1 : i64") &&
    !uintBitNotArtifact.includes("@w_seed_checked_negate_i64"),
  "UInt bitwise complement lost direct all-ones xor lowering")
  const uintBitwiseArtifact =
    artifacts.get("restaurant-uint-bitwise").toString("utf8")
  assert(uintBitwiseArtifact.includes("llvm.and ") &&
    uintBitwiseArtifact.includes("llvm.or ") &&
    uintBitwiseArtifact.includes("llvm.xor ") &&
    uintBitwiseArtifact.includes("llvm.intr.ctpop") &&
    uintBitwiseArtifact.includes("llvm.intr.ctlz") &&
    uintBitwiseArtifact.includes("llvm.intr.cttz") &&
    uintBitwiseArtifact.includes("llvm.call @w_fn_0") &&
    !uintBitwiseArtifact.includes("@w_seed_checked_"),
  "UInt binary bitwise lowering lost a direct operation or gained a signed helper")
  const uintCompoundArtifact =
    artifacts.get("restaurant-uint-compound").toString("utf8")
  for (const helper of [
    "@w_seed_checked_add_u64",
    "@w_seed_checked_subtract_u64",
    "@w_seed_checked_multiply_u64",
    "@w_seed_checked_divide_u64",
    "@w_seed_checked_remainder_u64",
    "@w_seed_checked_power_u64",
    "@w_seed_checked_shift_left_u64",
    "@w_seed_checked_shift_right_u64",
  ]) {
    assert(uintCompoundArtifact.includes(helper),
      `UInt compound mutation lost unsigned helper ${helper}`)
  }
  assert(uintCompoundArtifact.includes("llvm.udiv ") &&
    uintCompoundArtifact.includes("llvm.urem ") &&
    uintCompoundArtifact.includes("llvm.shl ") &&
    uintCompoundArtifact.includes("llvm.lshr ") &&
    uintCompoundArtifact.includes("llvm.and ") &&
    uintCompoundArtifact.includes("llvm.xor ") &&
    uintCompoundArtifact.includes("llvm.or "),
  "UInt compound mutation lost unsigned division, shifts, or bitwise lowering")
  assert(uintCompoundArtifact.includes("llvm.call @w_seed_append_u64"),
    "UInt compound mutation lost unsigned decimal output")
  const uintCompoundFunctionStart = uintCompoundArtifact.indexOf(
    "llvm.func internal @w_fn_0(")
  const uintCompoundFunctionEnd = uintCompoundArtifact.indexOf(
    "\n  llvm.func ", uintCompoundFunctionStart + 1)
  assert(uintCompoundFunctionStart >= 0 && uintCompoundFunctionEnd >
    uintCompoundFunctionStart,
  "UInt compound mutation function boundary is missing")
  const uintCompoundFunction = uintCompoundArtifact.slice(
    uintCompoundFunctionStart, uintCompoundFunctionEnd)
  assert(!uintCompoundFunction.includes("@w_seed_checked_add_i64") &&
    !uintCompoundFunction.includes("@w_seed_checked_subtract_i64") &&
    !uintCompoundFunction.includes("@w_seed_checked_multiply_i64") &&
    !uintCompoundFunction.includes("@w_seed_checked_divide_i64") &&
    !uintCompoundFunction.includes("@w_seed_checked_remainder_i64") &&
    !uintCompoundFunction.includes("@w_seed_checked_power_i64") &&
    !uintCompoundFunction.includes("@w_seed_checked_shift_left_i64") &&
    !uintCompoundFunction.includes("@w_seed_checked_shift_right_i64") &&
    !uintCompoundFunction.includes("llvm.sdiv ") &&
    !uintCompoundFunction.includes("llvm.srem ") &&
    !uintCompoundFunction.includes("llvm.ashr ") &&
    !uintCompoundFunction.includes("llvm.intr.smul.with.overflow") &&
    !uintCompoundFunction.includes("llvm.alloca"),
  "UInt compound mutation gained a signed helper or operation")
  const wmoArtifact = artifacts.get("restaurant-wmo")
  assert(!artifacts.get("hello").includes("@w_seed_checked_") &&
    !artifacts.get("hello").includes("llvm.intr.usub.with.overflow") &&
    !artifacts.get("hello").includes("llvm.intr.umul.with.overflow") &&
    !artifacts.get("hello").includes("@w_seed_overflowing_power_u64") &&
    !artifacts.get("hello").includes("@w_seed_saturating_power_u64") &&
    wmoArtifact.includes("@w_fn_0(") &&
    !wmoArtifact.includes("@w_fn_1(") &&
    !wmoArtifact.includes("@w_fn_2(") &&
    !wmoArtifact.includes("@w_seed_checked_divide_i64") &&
    !wmoArtifact.includes("\\4E\\65\\76\\65\\72\\20\\73\\65\\72\\76\\65\\64"),
  "whole-module reachability did not retain only the selected product closure")
  const asyncYieldArtifact = artifacts.get("restaurant-async-yield")
    .toString("utf8")
  assert(asyncYieldArtifact.includes("llvm.call @w_fn_0") &&
    asyncYieldArtifact.includes("@w_seed_checked_add_i64") &&
    asyncYieldArtifact.includes("@w_seed_checked_multiply_i64") &&
    !/task|yield|async|wrt/i.test(asyncYieldArtifact),
  "static-yield product retained Task/frame/runtime surface or lost scalar work")
  assert(artifacts.get("typed-bindings").includes(
    "llvm.call @w_seed_checked_multiply_i64(%v0, %v1, %v2_checked_width) : " +
    "(i64, i64, i64) -> i64"),
  "typed binding arithmetic was precomputed before MLIR")
  const runtimeDivremArtifact = artifacts.get("restaurant-runtime-divrem")
  assert(runtimeDivremArtifact.includes(
    "llvm.call @w_seed_checked_divide_i64") &&
    runtimeDivremArtifact.includes("llvm.sdiv %left, %right") &&
    runtimeDivremArtifact.includes(
      "llvm.call @w_seed_checked_remainder_i64") &&
    runtimeDivremArtifact.includes("llvm.srem %left, %right") &&
    artifacts.get("runtime-minimum-remainder").includes(
      "llvm.cond_br %overflow_pair, ^checked_minimum, ^checked_ok"),
  "runtime division/remainder checks were omitted or precomputed")
  const runtimeNegateArtifact = artifacts.get("restaurant-unary-negate")
  assert(runtimeNegateArtifact.includes(
    "llvm.call @w_seed_checked_subtract_i64") &&
    runtimeNegateArtifact.includes("_neg_zero") &&
    !artifacts.get("direct-unary-interpolation").includes(
      "@w_seed_checked_subtract_i64") &&
    artifacts.get("direct-unary-interpolation").includes(" = llvm.sub "),
  "checked runtime or direct constant unary negation was not retained")
  assert(artifacts.get("direct-call").includes("llvm.call @w_fn_0") &&
    artifacts.get("direct-call").includes(
      "llvm.call @w_seed_checked_multiply_i64(%v4, %v5, %v6_checked_width) : " +
      "(i64, i64, i64) -> i64") &&
    artifacts.get("direct-call").includes("%p0") &&
    artifacts.get("direct-call").includes("%p1"),
  "direct W call was flattened, reordered during evaluation, or precomputed")
  assert(artifacts.get("direct-call").indexOf("llvm.mlir.constant(true)") <
    artifacts.get("direct-call").indexOf(
      "llvm.call @w_seed_checked_multiply_i64(%v4"),
  "named argument evaluation did not preserve source order")
  assert(artifacts.get("scalar-return").includes(
    "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, %cursor_address: !llvm.ptr) -> i64") &&
    artifacts.get("scalar-return").includes("%call0 = llvm.call @w_fn_0") &&
    artifacts.get("scalar-return").includes("llvm.return %v") &&
    artifacts.get("scalar-return").includes("@w_seed_append_i64") &&
    artifacts.get("scalar-return").includes("w_seed_checked_multiply_i64"),
  "scalar return was flattened, precomputed, or disconnected from interpolation")
  assert(artifacts.get("bool-return").includes(
    "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, %cursor_address: !llvm.ptr) -> i1") &&
    artifacts.get("bool-return").includes("%call0 = llvm.call @w_fn_0") &&
    artifacts.get("bool-return").includes("llvm.return %v") &&
    artifacts.get("bool-return").includes("@w_seed_append_bool"),
  "Bool return was flattened or disconnected from interpolation")
  const conditionalMutationArtifact = artifacts.get(
    "restaurant-conditional-mutation").toString("utf8")
  const conditionalMutationStart = conditionalMutationArtifact.indexOf(
    "llvm.func internal @w_fn_0(")
  const conditionalMutationEntry = conditionalMutationArtifact.indexOf(
    "llvm.func internal @w_fn_1(", conditionalMutationStart + 1)
  assert(conditionalMutationStart >= 0 &&
    conditionalMutationEntry > conditionalMutationStart,
  "conditional mutation function boundaries are missing")
  const conditionalMutationFunction = conditionalMutationArtifact.slice(
    conditionalMutationStart, conditionalMutationEntry)
  assert(conditionalMutationFunction.includes(
    "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2") &&
    conditionalMutationFunction.includes("^w_fn_0_b_3(%arg0: i64):") &&
    conditionalMutationFunction.includes("@w_seed_checked_add_i64") &&
    conditionalMutationFunction.includes("@w_seed_checked_subtract_i64") &&
    conditionalMutationFunction.includes("llvm.return %") &&
    !conditionalMutationFunction.includes("llvm.alloca") &&
    (conditionalMutationArtifact.match(/llvm\.call @w_fn_0/gu) || []).length === 2,
  "conditional mutation did not retain one SSA value join and two runtime calls")
  const boolMutationArtifact = artifacts.get("restaurant-bool-mutation")
    .toString("utf8")
  const boolMutationStart = boolMutationArtifact.indexOf(
    "llvm.func internal @w_fn_0(")
  const boolMutationEntry = boolMutationArtifact.indexOf(
    "llvm.func internal @w_fn_1(", boolMutationStart + 1)
  assert(boolMutationStart >= 0 && boolMutationEntry > boolMutationStart,
  "Boolean mutation function boundaries are missing")
  const boolMutationFunction = boolMutationArtifact.slice(
    boolMutationStart, boolMutationEntry)
  assert(boolMutationFunction.includes("%p0: i1) -> i1") &&
    boolMutationFunction.includes("llvm.return %p0 : i1") &&
    !boolMutationFunction.includes("llvm.alloca") &&
    boolMutationArtifact.includes("@w_seed_append_bool") &&
    (boolMutationArtifact.match(/llvm\.call @w_fn_0/gu) || []).length === 2,
  "Boolean mutation was stored, flattened, or disconnected from display")
  const branchMutationArtifact = artifacts.get("restaurant-branch-mutation")
    .toString("utf8")
  const branchMutationStart = branchMutationArtifact.indexOf(
    "llvm.func internal @w_fn_0(")
  const branchMutationEntry = branchMutationArtifact.indexOf(
    "llvm.func internal @w_fn_1(", branchMutationStart + 1)
  assert(branchMutationStart >= 0 && branchMutationEntry > branchMutationStart,
  "branch-local mutation function boundaries are missing")
  const branchMutationFunction = branchMutationArtifact.slice(
    branchMutationStart, branchMutationEntry)
  assert(branchMutationFunction.includes(
    "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2") &&
    branchMutationFunction.includes("^w_fn_0_b_3(%arg0: i64):") &&
    branchMutationFunction.includes("llvm.return %arg0 : i64") &&
    !branchMutationFunction.includes("llvm.alloca") &&
    (branchMutationArtifact.match(/llvm\.call @w_fn_0/gu) || []).length === 2,
  "branch-local mutation did not retain one SSA join and two runtime calls")
  const multiBranchMutationArtifact = artifacts.get(
    "restaurant-branch-mutation-multi").toString("utf8")
  const multiBranchMutationStart = multiBranchMutationArtifact.indexOf(
    "llvm.func internal @w_fn_0(")
  const multiBranchMutationEntry = multiBranchMutationArtifact.indexOf(
    "llvm.func internal @w_fn_1(", multiBranchMutationStart + 1)
  assert(multiBranchMutationStart >= 0 &&
    multiBranchMutationEntry > multiBranchMutationStart,
  "multi branch mutation function boundaries are missing")
  const multiBranchMutationFunction = multiBranchMutationArtifact.slice(
    multiBranchMutationStart, multiBranchMutationEntry)
  assert(multiBranchMutationFunction.includes(
    "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2") &&
    multiBranchMutationFunction.includes(
      "^w_fn_0_b_3(%arg0: i64, %arg1: i64):") &&
    (multiBranchMutationFunction.match(
      /llvm\.br \^w_fn_0_b_3\(%v\d+, %v\d+ : i64, i64\)/gu) || []).length === 2 &&
    multiBranchMutationFunction.includes("llvm.return %v") &&
    !multiBranchMutationFunction.includes("llvm.alloca"),
  "multi branch mutation did not retain two typed SSA join values")
  const naturalLoopArtifact = artifacts.get("restaurant-while").toString("utf8")
  const naturalLoopStart = naturalLoopArtifact.indexOf(
    "llvm.func internal @w_fn_0(")
  const naturalLoopEntry = naturalLoopArtifact.indexOf(
    "llvm.func internal @w_fn_1(", naturalLoopStart + 1)
  assert(naturalLoopStart >= 0 && naturalLoopEntry > naturalLoopStart,
    "natural-loop function boundaries are missing")
  const naturalLoopFunction = naturalLoopArtifact.slice(
    naturalLoopStart, naturalLoopEntry)
  assert(naturalLoopFunction.includes(" = scf.while (") &&
    naturalLoopFunction.includes("scf.condition(") &&
    naturalLoopFunction.includes("scf.yield ") &&
    naturalLoopFunction.includes("llvm.return %loop0 : i64") &&
    !naturalLoopFunction.includes("llvm.br ^w_fn_0_b_1") &&
    !naturalLoopFunction.includes("llvm.alloca"),
  "natural loop did not preserve structured SCF and SSA storage elimination")
  const cfgArtifact = artifacts.get("restaurant-if").toString("utf8")
  const joinBranches = cfgArtifact.match(/llvm\.br \^w_fn_0_b_3\n/gu) || []
  const cfgSignature = cfgArtifact.match(
    /llvm\.func internal @w_fn_0\([^\n]*%p0: i1\)/u)
  assert(cfgSignature && cfgArtifact.includes("llvm.cond_br %p0") &&
    joinBranches.length === 2 &&
    cfgArtifact.includes("\\4b\\69\\74\\63\\68\\65\\6e\\20\\6f\\70\\65\\6e\\0a") &&
    cfgArtifact.includes("\\4b\\69\\74\\63\\68\\65\\6e\\20\\63\\6c\\6f\\73\\65\\64\\0a") &&
    (cfgArtifact.match(/\\41\\66\\74\\65\\72\\20\\73\\65\\72\\76\\69\\63\\65\\0a/gu) || []).length === 1 &&
    (cfgArtifact.match(/llvm\.call @w_fn_0/gu) || []).length === 2,
  "if diamond did not retain typed cond_br, two join branches, both payloads, and one post-join body")

  const nestedCfgArtifact = artifacts.get("restaurant-nested-if").toString("utf8")
  const nestedJoinBranches = nestedCfgArtifact.match(
    /llvm\.br \^w_fn_0_b_(?:4|8|9)\n/gu) || []
  assert(nestedCfgArtifact.includes("llvm.cond_br %p0") &&
    (nestedCfgArtifact.match(/llvm\.cond_br %p1/gu) || []).length >= 2 &&
    nestedJoinBranches.length >= 6 &&
    nestedCfgArtifact.includes("\\50\\6f\\73\\74\\2d\\6a\\6f\\69\\6e\\20\\73\\65\\72\\76\\69\\63\\65\\0a") &&
    (nestedCfgArtifact.match(/llvm\.call @w_fn_0/gu) || []).length === 4,
  "nested Restaurant did not retain both inner diamonds and one post-join call per invocation")

  const nestedScalarArtifact = artifacts.get("restaurant-nested-scalar-if")
    .toString("utf8")
  const nestedScalarChooseStart = nestedScalarArtifact.indexOf(
    "llvm.func internal @w_fn_0(")
  const nestedScalarMainStart = nestedScalarArtifact.indexOf(
    "llvm.func internal @w_fn_1(", nestedScalarChooseStart + 1)
  assert(nestedScalarChooseStart >= 0 && nestedScalarMainStart > nestedScalarChooseStart,
    "nested scalar Restaurant function boundaries are missing")
  const nestedScalarChooseArtifact = nestedScalarArtifact.slice(
    nestedScalarChooseStart, nestedScalarMainStart)
  assert((nestedScalarChooseArtifact.match(/llvm\.cond_br/gu) || []).length === 2 &&
    nestedScalarChooseArtifact.includes(
      "llvm.br ^w_fn_0_b_4(%p2 : i64)") &&
    nestedScalarChooseArtifact.includes(
      "llvm.br ^w_fn_0_b_4(%p3 : i64)") &&
    nestedScalarChooseArtifact.includes(
      "llvm.br ^w_fn_0_b_6(%arg0 : i64)") &&
    nestedScalarChooseArtifact.includes(
      "llvm.br ^w_fn_0_b_6(%p4 : i64)") &&
    nestedScalarChooseArtifact.includes("^w_fn_0_b_4(%arg0: i64):") &&
    nestedScalarChooseArtifact.includes("^w_fn_0_b_6(%arg1: i64):") &&
    nestedScalarChooseArtifact.includes("llvm.return %arg1 : i64") &&
    !nestedScalarChooseArtifact.includes("llvm.select") &&
    (nestedScalarArtifact.match(/llvm\.call @w_fn_0/gu) || []).length === 3 &&
    !nestedScalarArtifact.includes("1,2,3") &&
    !nestedScalarArtifact.includes("\\31\\2c\\32\\2c\\33\\0a"),
  "nested scalar Restaurant did not retain two typed value diamonds")

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
  const nestedIfTooDeep =
    `fn main() { ${"if true { ".repeat(65)}print("x") ` +
    `${"} ".repeat(65)}}\nentry(main)\n`
  const adversarial = [
    ["comment-with-print.w",
      `fn main() { noop("Other") } // print("Hello, world!")\nentry(main)\n`],
    ["noop.w", `fn main() { noop("Other") }\nentry(main)\n`],
    ["outside-subset.w",
      `fn main(value: String) { print(value) }\nentry(main)\n`],
    ["immutable-assignment.w",
      `fn main() { let seats = 5 seats = seats + 1 print("\${seats}") }\nentry(main)\n`],
    ["unused-binding.w",
      `fn main() { let message = "unused" print("kept") }\nentry(main)\n`],
    ["recursive-call.w",
      `fn again() { again() }\nfn main() { again() }\nentry(main)\n`],
    ["runtime-string-parameter.w",
      `fn show(value: String) { print(value) }\n` +
      `fn main() { show(value: "x") }\nentry(main)\n`],
    ["runtime-string-result.w",
      `fn state(): String { return "open" }\n` +
      `fn main() { let value = state() print("\${value}") }\nentry(main)\n`],
    ["missing-scalar-return.w",
      `fn value(): i64 { print("no value") }\n` +
      `fn main() { let result = value() print("\${result}") }\nentry(main)\n`],
    ["nested-return-call.w",
      `fn value(): i64 { return 42 }\nfn relay(): i64 { return value() }\n` +
      `fn main() { let result = relay() print("\${result}") }\nentry(main)\n`],
    ["nested-if-too-deep.w", nestedIfTooDeep],
    ["constant-arithmetic-overflow.w",
      `fn main() { let value = 9223372036854775807 + 1 ` +
      `print("\${value}") }\nentry(main)\n`],
    ["scalar-cfg.w",
      `fn main(): i64 { if true { print("x") } return 1 }\nentry(main)\n`],
    ["scalar-entry.w", `fn main(): i64 { return 42 }\nentry(main)\n`],
    ["non-carried-while.w",
      `fn countTo(limit: i64): i64 { var count = 0 ` +
      `while limit > 0 { count = count + 1 } return count }\n` +
      `entry { let value = countTo(limit: 3) print("\${value}") }\n`],
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
  console.log(`MLIR0: verified HIR0 → SCF/LLVM dialects → mlir-opt → mlir-translate → clang IR/native passed (${dialectDisclosure(dialect)})`)
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
  await rm(artifactDirectory, { recursive: true, force: true })
}
