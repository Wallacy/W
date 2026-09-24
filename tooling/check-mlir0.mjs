import { existsSync } from "node:fs"
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { dialectDisclosure, probeCDialect } from "./c-dialect.mjs"
import { mlir0VersionRequirement } from "./mlir0-version-gate.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const canonicalFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const linearFixture = resolve(seedDirectory, "fixtures", "linear.w")
const interpolationFixture = resolve(seedDirectory, "fixtures", "interpolation.w")
const ifFixture = resolve(seedDirectory, "fixtures", "if.w")
const nestedIfFixture = resolve(seedDirectory, "fixtures", "nested-if.w")
const nestedScalarIfFixture = resolve(seedDirectory,
  "fixtures", "nested-scalar-if.w")
const checkedArithmeticFixture = resolve(seedDirectory,
  "fixtures", "checked-arithmetic.w")
const unaryNegateFixture = resolve(seedDirectory,
  "fixtures", "unary-negate.w")
const unaryInterpolationFixture = resolve(seedDirectory,
  "fixtures", "unary-interpolation.w")
const integerBitwiseFixture = resolve(seedDirectory,
  "fixtures", "integer-bitwise.w")
const unsignedFixture = resolve(seedDirectory,
  "fixtures", "unsigned.w")
const shiftsFixture = resolve(seedDirectory,
  "fixtures", "shifts.w")
const powerFixture = resolve(seedDirectory,
  "fixtures", "power.w")
const powerPrefixFixture = resolve(seedDirectory,
  "fixtures", "power-prefix.w")
const compoundFixture = resolve(seedDirectory,
  "fixtures", "compound.w")
const floatStrictFixture = resolve(seedDirectory,
  "fixtures", "float-strict.w")
const floatBitRepresentationFixture = resolve(seedDirectory,
  "fixtures", "float-bit-representation.w")
const numericWideningFixture = resolve(seedDirectory,
  "fixtures", "numeric-widening.w")
const checkedIntegerArithmeticFixture = resolve(seedDirectory,
  "fixtures", "checked-integer-arithmetic.w")
const integerWrappingFixture = resolve(seedDirectory,
  "fixtures", "integer-wrapping.w")
const integerPrefixFixture = resolve(seedDirectory,
  "fixtures", "integer-prefix.w")
const integerWideningFixture = resolve(seedDirectory,
  "fixtures", "integer-widening.w")
const integerTruncatingBitsFixture = resolve(seedDirectory,
  "fixtures", "integer-truncating-bits.w")
const integerSaturatingConversionFixture = resolve(seedDirectory,
  "fixtures", "integer-saturating-conversion.w")
const uIntWrappingAddFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-add.w")
const uIntWrappingSubtractFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-subtract.w")
const uIntWrappingMultiplyFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-multiply.w")
const uIntWrappingNegateFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-negate.w")
const uIntWrappingPowerFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-power.w")
const uIntOverflowingPowerFixture = resolve(seedDirectory,
  "fixtures", "uint-overflowing-power.w")
const uIntOverflowingFamilyFixture = resolve(seedDirectory,
  "fixtures", "uint-overflowing-family.w")
const uIntWrappingShiftLeftFixture = resolve(seedDirectory,
  "fixtures", "uint-wrapping-shift-left.w")
const uIntRotatedLeftFixture = resolve(seedDirectory,
  "fixtures", "uint-rotated-left.w")
const uIntRotatedRightFixture = resolve(seedDirectory,
  "fixtures", "uint-rotated-right.w")
const uIntCountOnesFixture = resolve(seedDirectory,
  "fixtures", "uint-count-ones.w")
const uIntCountZerosFixture = resolve(seedDirectory,
  "fixtures", "uint-count-zeros.w")
const uIntLeadingZerosFixture = resolve(seedDirectory,
  "fixtures", "uint-leading-zeros.w")
const uIntTrailingZerosFixture = resolve(seedDirectory,
  "fixtures", "uint-trailing-zeros.w")
const uIntReversedBitsFixture = resolve(seedDirectory,
  "fixtures", "uint-reversed-bits.w")
const uIntReversedBytesFixture = resolve(seedDirectory,
  "fixtures", "uint-reversed-bytes.w")
const fixedIntegerBitPrimitivesFixture = resolve(seedDirectory,
  "fixtures", "fixed-integer-bit-primitives.w")
const fixedIntegerShiftPoliciesFixture = resolve(seedDirectory,
  "fixtures", "fixed-integer-shift-policies.w")
const fixedIntegerBitPrimitivesOutput = Buffer.from(
  "i8 3/5/1/1 74/127 82 -92/41\n" +
  "u8 4/4/0/1 105 150 45/75\n" +
  "i16 5/11/3/2 11336/32767 13330 9320/2330\n" +
  "u16 8/8/0/0 54673 43913 4951/50389\n" +
  "i32 13/19/3/3 510274632/2147483647 2018915346 610839792/152709948\n" +
  "u32 20/12/0/0 4155757969 4023233417 324508639/3302352631\n" +
  "i64 30/34/7/1 8553414939923104896/9223372036854775807 7984226321029210881 163971058432973532/40992764608243383\n" +
  "u64 32/32/0/4 597899502893742975 1167088121787636990 18282773015276577825/9182379272246532360\n",
  "utf8")
const fixedIntegerShiftPoliciesOutput = Buffer.from(
  "i8 -128/0/-64/64\n" +
  "u8 128/0/64/64\n" +
  "i16 -32768/0/-16384/16384\n" +
  "u16 32768/0/16384/16384\n" +
  "i32 -2147483648/0/-1073741824/1073741824\n" +
  "u32 2147483648/0/1073741824/1073741824\n" +
  "i64 -9223372036854775808/0/-4611686018427387904/4611686018427387904\n" +
  "u64 9223372036854775808/0/4611686018427387904/4611686018427387904\n" +
  "u64 small-value 2/64/64\n", "utf8")
const uIntSaturatingAddFixture = resolve(seedDirectory,
  "fixtures", "uint-saturating-add.w")
const uIntSaturatingSubtractFixture = resolve(seedDirectory,
  "fixtures", "uint-saturating-subtract.w")
const uIntSaturatingMultiplyFixture = resolve(seedDirectory,
  "fixtures", "uint-saturating-multiply.w")
const uIntSaturatingPolicyFixture = resolve(seedDirectory,
  "fixtures", "uint-saturating-policy.w")
const uIntBitNotFixture = resolve(seedDirectory,
  "fixtures", "uint-bit-not.w")
const uIntBitwiseFixture = resolve(seedDirectory,
  "fixtures", "uint-bitwise.w")
const uIntCompoundFixture = resolve(seedDirectory,
  "fixtures", "uint-compound.w")
const mutationFixture = resolve(seedDirectory,
  "fixtures", "mutation.w")
const conditionalMutationFixture = resolve(seedDirectory,
  "fixtures", "conditional-mutation.w")
const boolMutationFixture = resolve(seedDirectory,
  "fixtures", "bool-mutation.w")
const branchMutationFixture = resolve(seedDirectory,
  "fixtures", "branch-mutation.w")
const multiBranchMutationFixture = resolve(seedDirectory,
  "fixtures", "branch-mutation-multi.w")
const whileFixture = resolve(seedDirectory, "fixtures", "while.w")
const nestedLabeledWhileFixture = resolve(seedDirectory, "fixtures",
  "nested-labeled-while.w")
const wmoFixture = resolve(seedDirectory, "fixtures", "wmo.w")
const asyncJoinFixture = resolve(seedDirectory,
  "fixtures", "async-join.w")
const asyncYieldFixture = resolve(seedDirectory,
  "fixtures", "async-yield.w")
const explicitPanicFixture = resolve(seedDirectory,
  "fixtures", "panic-explicit.w")
const mlirHeaderPath = resolve(seedDirectory, "include", "w_seed_mlir0.h")
const mlirSourcePath = resolve(seedDirectory, "src", "w_seed_mlir0.c")
const manifestPath = resolve(root, "tooling", "mlir0-toolchain.json")
const targetTriple = "x86_64-unknown-linux-gnu"
const expectedVersion = "23.1.1"
const developmentPatchCompatibility =
  process.env.W_MLIR0_DEVELOPMENT_PATCH_COMPAT !== "0"
const acceptedVersion = mlir0VersionRequirement({
  pinnedVersion: expectedVersion,
  candidateVersion: process.env.W_MLIR0_ACCEPT_VERSION,
  developmentPatchCompatibility,
})
const isWindows = process.platform === "win32"

function fail(message) {
  throw new Error(`MLIR0: ${message}`)
}

function assert(condition, message) {
  if (!condition) fail(message)
}

function floatRoundingTranslationIsGuarded(llvmText, sourceType,
  llvmFloatType, intrinsic, conversionOpcode) {
  const classification = llvmText.indexOf(`llvm.is.fpclass.${sourceType}`)
  const rounding = llvmText.indexOf(`llvm.${intrinsic}.${sourceType}`)
  const lowerCompare = llvmText.indexOf(`fcmp oge ${llvmFloatType}`)
  const upperCompare = llvmText.indexOf(`fcmp olt ${llvmFloatType}`)
  const rangeValue = llvmText.match(/(%[-a-zA-Z$._0-9]+) = and i1[^\n]+/u)?.[1]
  const rangeBranch = rangeValue === undefined
    ? -1 : llvmText.indexOf(`br i1 ${rangeValue}`, upperCompare)
  const conversion = llvmText.indexOf(
    `${conversionOpcode} ${llvmFloatType}`, rounding)
  return classification >= 0 && rounding > classification &&
    lowerCompare > rounding && upperCompare > lowerCompare &&
    rangeBranch > upperCompare && conversion > rangeBranch
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
        acceptedVersion.pattern
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
      acceptedVersion.pattern
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
    fail(`${role} tool version is not ${acceptedVersion.description}: ${probe.output.trim()}`)
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

  const integerExactly = run(unitPath, ["--emit-integer-exactly"])
  assert(integerExactly.exitCode === 0,
    `integer exactly probe failed: ${integerExactly.stderrText}`)
  assert(integerExactly.stderr.length === 0 && integerExactly.stdout.length > 0,
    "integer exactly probe did not emit one silent MLIR artifact")
  const integerExactlyInput = resolve(artifactDirectory, "integer-exactly.mlir")
  const integerExactlyVerified = resolve(artifactDirectory,
    "integer-exactly.verified.mlir")
  const integerExactlyLlvm = resolve(artifactDirectory, "integer-exactly.ll")
  await writeFile(integerExactlyInput, integerExactly.stdout)
  const integerExactlyInputForTool = isWindows
    ? wslPath(integerExactlyInput) : integerExactlyInput
  const integerExactlyVerifiedForTool = isWindows
    ? wslPath(integerExactlyVerified) : integerExactlyVerified
  const integerExactlyLlvmForTool = isWindows
    ? wslPath(integerExactlyLlvm) : integerExactlyLlvm
  invokeTool(tool("mlirOpt"), [integerExactlyInputForTool, "-o",
    integerExactlyVerifiedForTool, "--verify-each"], "integer exactly mlir-opt")
  invokeTool(tool("mlirTranslate"), ["--mlir-to-llvmir",
    integerExactlyVerifiedForTool, "-o", integerExactlyLlvmForTool],
  "integer exactly mlir-translate")
  const integerExactlyLlvmText = await readFile(integerExactlyLlvm, "utf8")
  assert(integerExactlyLlvmText.includes(
    "define internal { i1, i64 } @w_seed_exact_integer_convert") &&
    integerExactlyLlvmText.includes("icmp ule i64") &&
    integerExactlyLlvmText.includes("9223372036854775807") &&
    integerExactlyLlvmText.includes("br i1") &&
    !integerExactlyLlvmText.includes("invoke "),
  "integer exactly LLVM translation lost the checked typed branch")

  const floatRoundingModes = [
    ["nearestEven", "roundeven"],
    ["nearestAwayFromZero", "round"],
    ["towardZero", "trunc"],
    ["towardPositive", "ceil"],
    ["towardNegative", "floor"],
  ]
  const unguardedRoundingMutation = [
    "%finite = call i1 @llvm.is.fpclass.f64(double %source, i32 519)",
    "br i1 %finite, label %non_finite, label %round",
    "%rounded = call double @llvm.roundeven.f64(double %source)",
    "%lower = fcmp oge double %rounded, -9.22e18",
    "%upper = fcmp olt double %rounded, 9.22e18",
    "%converted = fptosi double %rounded to i64",
    "%fits = and i1 %lower, %upper",
    "br i1 %fits, label %success, label %out_of_range",
  ].join("\n")
  assert(!floatRoundingTranslationIsGuarded(unguardedRoundingMutation,
    "f64", "double", "roundeven", "fptosi"),
  "float-to-integer guard checker accepted conversion before range proof")
  for (const [sourceType, llvmFloatType] of [["f32", "float"], ["f64", "double"]]) {
    for (let modeIndex = 0; modeIndex < floatRoundingModes.length; modeIndex += 1) {
      const [mode, intrinsic] = floatRoundingModes[modeIndex]
      const destinationType = modeIndex % 2 === 0 ? "i64" : "u64"
      const conversionOpcode = destinationType === "i64" ? "fptosi" : "fptoui"
      const stem = `float-to-integer-rounding-${sourceType}-${destinationType}-${mode}`
      const floatRounding = run(unitPath,
        ["--emit-float-to-integer-rounding", sourceType, destinationType, mode])
      assert(floatRounding.exitCode === 0,
        `${stem} probe failed: ${floatRounding.stderrText}`)
      assert(floatRounding.stderr.length === 0 && floatRounding.stdout.length > 0,
        `${stem} did not emit one silent MLIR artifact`)
      const floatRoundingInput = resolve(artifactDirectory, `${stem}.mlir`)
      const floatRoundingVerified = resolve(artifactDirectory, `${stem}.verified.mlir`)
      const floatRoundingLlvm = resolve(artifactDirectory, `${stem}.ll`)
      await writeFile(floatRoundingInput, floatRounding.stdout)
      const floatRoundingInputForTool = isWindows
        ? wslPath(floatRoundingInput) : floatRoundingInput
      const floatRoundingVerifiedForTool = isWindows
        ? wslPath(floatRoundingVerified) : floatRoundingVerified
      const floatRoundingLlvmForTool = isWindows
        ? wslPath(floatRoundingLlvm) : floatRoundingLlvm
      invokeTool(tool("mlirOpt"), [floatRoundingInputForTool, "-o",
        floatRoundingVerifiedForTool, "--verify-each"], `${stem} mlir-opt`)
      invokeTool(tool("mlirTranslate"), ["--mlir-to-llvmir",
        floatRoundingVerifiedForTool, "-o", floatRoundingLlvmForTool],
      `${stem} mlir-translate`)
      const llvmText = await readFile(floatRoundingLlvm, "utf8")
      assert(llvmText.includes(
        `define internal { i2, i64 } @w_seed_float_to_integer_round`) &&
        floatRoundingTranslationIsGuarded(llvmText, sourceType,
          llvmFloatType, intrinsic, conversionOpcode) &&
        !llvmText.includes("invoke "),
      `${stem} LLVM translation lost classification, bounds, or guarded conversion`)
    }
  }

  const seedGate = resolve(buildDirectory, `w_seed_mlir0_gate${suffix}`)
  const fixturePath = resolve(artifactDirectory, "service-example.w")
  const fixtureLiteralPath = resolve(artifactDirectory, "literal.w")
  const fixtureLinearLiteralPath = resolve(artifactDirectory, "linear-literal.w")
  const fixtureIfPath = resolve(artifactDirectory, "if.w")
  const fixtureNestedIfPath = resolve(artifactDirectory, "nested-if.w")
  const fixtureNestedScalarIfPath = resolve(artifactDirectory,
    "nested-scalar-if.w")
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
  const runtimeIntegerNegationMinimumPaths = []
  for (const [type, maximum, suffix] of [
    ["i8", "127", "_i8"],
    ["i16", "32767", "_i16"],
    ["i32", "2147483647", "_i32"],
    ["i64", "9223372036854775807", "_i64"],
    ["Int", "9223372036854775807", "_i64"],
  ]) {
    const name = `runtime-negation-minimum-${type.toLowerCase()}`
    const source = resolve(artifactDirectory, `${name}.w`)
    await writeFile(source,
      `fn negate(value: ${type}): ${type} { return -value }\n` +
      `entry { print("must not commit") ` +
      `let result = negate(value: ~${maximum}${suffix}) ` +
      `print("\${result}") }\n`)
    runtimeIntegerNegationMinimumPaths.push({ name, source })
  }
  const runtimeShiftCountPath = resolve(artifactDirectory,
    "runtime-shift-count.w")
  const runtimeUnsignedShiftCountPath = resolve(artifactDirectory,
    "runtime-unsigned-shift-count.w")
  const runtimeUnsignedShiftOverflowPath = resolve(artifactDirectory,
    "runtime-unsigned-shift-overflow.w")
  const runtimeSignedShiftOverflowPath = resolve(artifactDirectory,
    "runtime-signed-shift-overflow.w")
  const runtimeNarrowUnsignedShiftOverflowPath = resolve(artifactDirectory,
    "runtime-narrow-unsigned-shift-overflow.w")
  const runtimeNarrowSignedShiftOverflowPath = resolve(artifactDirectory,
    "runtime-narrow-signed-shift-overflow.w")
  const runtimeNarrowNegativeShiftOverflowPath = resolve(artifactDirectory,
    "runtime-narrow-negative-shift-overflow.w")
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
  await writeFile(fixturePath,
    `fn serve() { let message = "Table 42 remains open" print(message) }\nentry(serve)\n`)
  await writeFile(fixtureLiteralPath,
    `fn serve() { print("Table 42 remains open") }\nentry(serve)\n`)
  await writeFile(fixtureLinearLiteralPath,
    `fn serve() {\nprint("Table 42 remains open")\n` +
    `print("Kitchen is ready")\n}\nentry(serve)\n`)
  await writeFile(fixtureIfPath,
    `fn serve(isOpen: Bool) {\n` +
    `  if isOpen { print("Kitchen open") } else { print("Kitchen closed") }\n` +
    `  print("After service")\n` +
    `}\n` +
    `fn main() { serve(isOpen: true) serve(isOpen: false) }\n` +
    `entry(main)\n`)
  await writeFile(fixtureNestedIfPath,
    await readFile(nestedIfFixture))
  await writeFile(fixtureNestedScalarIfPath,
    await readFile(nestedScalarIfFixture))
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
    '// Expected exit: nonzero (trap)\n// Expected stdout: <empty>\n' +
    'fn shift(value: i8, count: UInt): i8 { return value >> count }\n' +
    'entry { print("must not commit") let result = shift(value: 1_i8, count: 8_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeUnsignedShiftCountPath,
    '// Expected exit: nonzero (trap)\n// Expected stdout: <empty>\n' +
    'fn shift(value: u8, count: UInt): u8 { return value >> count }\n' +
    'entry { print("must not commit") let result = shift(value: 1_u8, count: 8_u64) ' +
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
  await writeFile(runtimeNarrowUnsignedShiftOverflowPath,
    '// Expected exit: nonzero (trap)\n// Expected stdout: <empty>\n' +
    'fn shift(value: u8, count: UInt): u8 { return value << count }\n' +
    'entry { print("must not commit") let result = shift(value: 128_u8, count: 1_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeNarrowSignedShiftOverflowPath,
    '// Expected exit: nonzero (trap)\n// Expected stdout: <empty>\n' +
    'fn shift(value: i8, count: UInt): i8 { return value << count }\n' +
    'entry { print("must not commit") let result = shift(value: 64_i8, count: 1_u64) ' +
    'print("success ${result}") }\n')
  await writeFile(runtimeNarrowNegativeShiftOverflowPath,
    '// Expected exit: nonzero (trap)\n// Expected stdout: <empty>\n' +
    'fn shift(value: i8, count: UInt): i8 { return value << count }\n' +
    'entry { print("must not commit") let result = shift(value: -64_i8, count: 2_u64) ' +
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
    { name: "binding", source: fixturePath,
      expected: Buffer.from("Table 42 remains open\n", "utf8") },
    { name: "literal", source: fixtureLiteralPath,
      expected: Buffer.from("Table 42 remains open\n", "utf8") },
    { name: "linear", source: linearFixture,
      expected: Buffer.from("Table 42 remains open\nKitchen is ready\n", "utf8") },
    { name: "interpolation", source: interpolationFixture,
      expected: Buffer.from("Table 42 remains open\n", "utf8") },
    { name: "linear-literal", source: fixtureLinearLiteralPath,
      expected: Buffer.from("Table 42 remains open\nKitchen is ready\n", "utf8") },
    { name: "if", source: fixtureIfPath,
      expected: Buffer.from(
        "Kitchen open\nAfter service\nKitchen closed\nAfter service\n", "utf8") },
    { name: "nested-if", source: fixtureNestedIfPath,
      expected: Buffer.from(
        "Restaurant open\nKitchen ready\nOpen branch joined\nPost-join service\n" +
        "Restaurant open\nKitchen closed\nOpen branch joined\nPost-join service\n" +
        "Restaurant closed\nKitchen ready\nClosed branch joined\nPost-join service\n" +
        "Restaurant closed\nKitchen closed\nClosed branch joined\nPost-join service\n",
        "utf8") },
    { name: "nested-scalar-if", source: fixtureNestedScalarIfPath,
      expected: Buffer.from("1,2,3\n", "utf8") },
    { name: "checked-arithmetic", source: checkedArithmeticFixture,
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
    { name: "runtime-minimum-remainder", source: runtimeMinimumRemainderPath,
      expected: Buffer.from("0\n", "utf8") },
    { name: "unary-negate", source: unaryNegateFixture,
      expected: Buffer.from("Balance -7\n", "utf8") },
    { name: "direct-unary-interpolation",
      source: unaryInterpolationFixture,
      expected: Buffer.from("Balance -7\n", "utf8") },
    { name: "mutation", source: mutationFixture,
      expected: Buffer.from("Open 6\n", "utf8") },
    { name: "conditional-mutation",
      source: conditionalMutationFixture,
      expected: Buffer.from("Open 6; closed 4\n", "utf8") },
    { name: "bool-mutation", source: boolMutationFixture,
      expected: Buffer.from("Open true; closed false\n", "utf8") },
    { name: "branch-mutation",
      source: branchMutationFixture,
      expected: Buffer.from("Open 6; closed 4\n", "utf8") },
    { name: "branch-mutation-multi",
      source: multiBranchMutationFixture,
      expected: Buffer.from("Open 18; closed -4\n", "utf8") },
    { name: "while", source: whileFixture,
      expected: Buffer.from("Served 3\n", "utf8") },
    { name: "nested-labeled-while", source: nestedLabeledWhileFixture,
      expected: Buffer.from("0,1,3\n", "utf8") },
    { name: "wmo", source: wmoFixture,
      expected: Buffer.from("Bill 42\n", "utf8") },
    { name: "async-join", source: asyncJoinFixture,
      expected: Buffer.from("Prepared 42\n", "utf8") },
    { name: "async-yield", source: asyncYieldFixture,
      expected: Buffer.from("Prepared 88\n", "utf8") },
    { name: "integer-bitwise",
      source: integerBitwiseFixture,
      expected: Buffer.from(
        "i8 10/-81/-91\n" +
        "u8 10/175/165\n" +
        "i16 2570/-20561/-23131\n" +
        "u16 2570/44975/42405\n" +
        "i32 168430090/-1347440721/-1515870811\n" +
        "u32 168430090/2947526575/2779096485\n" +
        "i64 723401728380766730/-5787213827046133841/-6510615555426900571\n" +
        "u64 723401728380766730/12659530246663417775/11936128518282651045\n" +
        "Int 723401728380766730/-5787213827046133841/-6510615555426900571\n" +
        "UInt 723401728380766730/12659530246663417775/11936128518282651045\n" +
        "Widened -13\n" +
        "Mixed 255\n",
        "utf8") },
    { name: "unsigned", source: unsignedFixture,
      expected: Buffer.from("Unsigned 18446744073709551615\n", "utf8") },
    { name: "shifts", source: shiftsFixture,
      expected: Buffer.from(
        "i8 -16/-128\nu8 32/128\ni16 -4096/-32768\nu16 8192/32768\n" +
        "i32 -268435456/-2147483648\nu32 536870912/2147483648\n" +
        "i64 -1152921504606846976/-9223372036854775808\n" +
        "u64 2305843009213693952/9223372036854775808\n" +
        "Int -1152921504606846976/-9223372036854775808\n" +
        "UInt 2305843009213693952/9223372036854775808\n", "utf8") },
    { name: "power", source: powerFixture,
      expected: Buffer.from("Power -27/1024/1/512\n", "utf8") },
    { name: "power-prefix", source: powerPrefixFixture,
      expected: Buffer.from("Power prefix -4/4/512/-9/-27\n", "utf8") },
    { name: "compound", source: compoundFixture,
      expected: Buffer.from("Compound 11\n", "utf8") },
    { name: "float-strict", source: floatStrictFixture,
      expected: Buffer.from("Float strict ok\n", "utf8") },
    { name: "float-bit-representation",
      source: floatBitRepresentationFixture,
      expected: Buffer.from(
        "Float bits f32 2147483648/2139095040/2143363909 " +
        "f64 9223372036854775808/9218868437227405312/9221140253039434428\n",
        "utf8") },
    { name: "numeric-widening",
      source: numericWideningFixture,
      expected: Buffer.from("Numeric widen ok\n", "utf8") },
    { name: "checked-integer-arithmetic",
      source: checkedIntegerArithmeticFixture,
      expected: Buffer.from(
        "i8 -9/-15/-36; divrem -4/0; compound -2\nu8 43/37/120; divrem 13/1; compound 2\n" +
        "i16 -970/-1030/-30000; divrem -33/-10; compound -12\n" +
        "u16 1030/970/30000; divrem 33/10; compound 8\n" +
        "i32 -117000/-123000/-360000000; divrem -40/0; compound -2\n" +
        "u32 100300/99700/30000000; divrem 333/100; compound 98\n" +
        "i64 -600000/-1200000/-270000000000; divrem -3/0; compound -2\n" +
        "u64 6000000000/4000000000/5000000000000000000; divrem 5/0; compound 999999998\n" +
        "Int -4000000000/-6000000000/-5000000000000000000; divrem -5/0; compound -2\n" +
        "UInt 9000000000/3000000000/18000000000000000000; divrem 2/0; compound 2999999998\n", "utf8") },
    { name: "integer-wrapping", source: integerWrappingFixture,
      expected: Buffer.from(
        "i8/u8 -128/0\ni16/u16 32767/2\ni32/u32 -2/4294967295\n" +
        "i64/u64 -9223372036854775808/0\n" +
        "Int/UInt -9223372036854775808/18446744073709551615\n", "utf8") },
    { name: "integer-prefix", source: integerPrefixFixture,
      expected: Buffer.from(
        "i8 -7/-43\ni16 -7/-43\ni32 -7/-43\ni64 -7/-43\n" +
        "Int -7/-43\nu8 170\nu16 65450\nu32 4294967210\n" +
        "u64 18446744073709551530\nUInt 18446744073709551530\n" +
        "literal -7\n", "utf8") },
    { name: "integer-widening", source: integerWideningFixture,
      expected: Buffer.from("Widen -7/200/202/203\n", "utf8") },
    { name: "integer-truncating-bits",
      source: integerTruncatingBitsFixture,
      expected: Buffer.from(
        "Trunc 2/-7/-6/18446744073709551609/-1\n", "utf8") },
    { name: "integer-saturating-conversion",
      source: integerSaturatingConversionFixture,
      expected: Buffer.from(
        "ss -128/7/127; us 7/127/127; su 0/200/255; " +
        "uu 7/255/255; UInt->Int 9223372036854775807\n", "utf8") },
    { name: "uint-wrapping-add",
      source: uIntWrappingAddFixture,
      expected: Buffer.from("Wrapped 0\n", "utf8") },
    { name: "uint-wrapping-subtract",
      source: uIntWrappingSubtractFixture,
      expected: Buffer.from("Wrapped 18446744073709551615\n", "utf8") },
    { name: "uint-wrapping-multiply",
      source: uIntWrappingMultiplyFixture,
      expected: Buffer.from("Wrapped 18446744073709551614\n", "utf8") },
    { name: "uint-wrapping-negate",
      source: uIntWrappingNegateFixture,
      expected: Buffer.from("Wrapped 18446744073709551615\n", "utf8") },
    { name: "uint-wrapping-power",
      source: uIntWrappingPowerFixture,
      expected: Buffer.from("Wrapped 12157665459056928801\n", "utf8") },
    { name: "uint-overflowing-power",
      source: uIntOverflowingPowerFixture,
      expected: Buffer.from(
        "Overflowing power 9223372036854775808/false; 0/true; 1/true; " +
        "1/false\n", "utf8") },
    { name: "uint-overflowing-family",
      source: uIntOverflowingFamilyFixture,
      expected: Buffer.from(
        "Overflowing family add 0/true,11/false; subtract 41/false," +
        "18446744073709551615/true; multiply 42/false," +
        "18446744073709551614/true; negate 0/false," +
        "18446744073709551615/true; power 9223372036854775808/false," +
        "0/true,1/true,1/false\n", "utf8") },
    { name: "uint-wrapping-shift-left",
      source: uIntWrappingShiftLeftFixture,
      expected: Buffer.from("Wrapped 18446744073709551614\n", "utf8") },
    { name: "uint-rotated-left",
      source: uIntRotatedLeftFixture,
      expected: Buffer.from("Rotated 3\n", "utf8") },
    { name: "uint-rotated-right",
      source: uIntRotatedRightFixture,
      expected: Buffer.from("Rotated 9223372036854775809\n", "utf8") },
    { name: "uint-count-ones",
      source: uIntCountOnesFixture,
      expected: Buffer.from("Ones 32\n", "utf8") },
    { name: "uint-count-zeros",
      source: uIntCountZerosFixture,
      expected: Buffer.from("Zeros 32\n", "utf8") },
    { name: "uint-leading-zeros",
      source: uIntLeadingZerosFixture,
      expected: Buffer.from("Leading 56/64\n", "utf8") },
    { name: "uint-trailing-zeros",
      source: uIntTrailingZerosFixture,
      expected: Buffer.from("Trailing 12/64\n", "utf8") },
    { name: "uint-reversed-bits",
      source: uIntReversedBitsFixture,
      expected: Buffer.from("Bits 17848844570815808640\n", "utf8") },
    { name: "uint-reversed-bytes",
      source: uIntReversedBytesFixture,
      expected: Buffer.from("Bytes 17279655951921914625\n", "utf8") },
    { name: "fixed-integer-bit-primitives",
      source: fixedIntegerBitPrimitivesFixture,
      expected: fixedIntegerBitPrimitivesOutput },
    { name: "fixed-integer-shift-policies",
      source: fixedIntegerShiftPoliciesFixture,
      expected: fixedIntegerShiftPoliciesOutput },
    { name: "uint-saturating-add",
      source: uIntSaturatingAddFixture,
      expected: Buffer.from("Saturated 18446744073709551615/11\n", "utf8") },
    { name: "uint-saturating-subtract",
      source: uIntSaturatingSubtractFixture,
      expected: Buffer.from("Saturated subtract 0/10\n", "utf8") },
    { name: "uint-saturating-multiply",
      source: uIntSaturatingMultiplyFixture,
      expected: Buffer.from("Saturated multiply 18446744073709551615/42\n", "utf8") },
    { name: "uint-saturating-policy",
      source: uIntSaturatingPolicyFixture,
      expected: Buffer.from(
        "Saturating policy add 18446744073709551615/11; subtract 0/10; " +
        "multiply 18446744073709551615/42; negate 0/0; power " +
        "8/18446744073709551615/1\n", "utf8") },
    { name: "uint-bit-not", source: uIntBitNotFixture,
      expected: Buffer.from("UInt not 18446744073709551615\n", "utf8") },
    { name: "uint-bitwise", source: uIntBitwiseFixture,
      expected: Buffer.from(
        "Not 18446744073709551615\nAnd 0\nOr 18446744073709551615\n" +
        "Xor 18446744073709551615\nOnes 32\nZeros 32\nLeading 56\n" +
        "Leading zero 64\nTrailing 12\nTrailing zero 64\n", "utf8") },
    { name: "uint-compound", source: uIntCompoundFixture,
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
    if (product.name === "while" ||
        product.name === "nested-labeled-while") {
      const lowered = await readFile(verified)
      assert(!lowered.includes("scf.") && lowered.includes("llvm.cond_br") &&
        lowered.includes("llvm.br"),
      `${product.name} was not lowered to the LLVM dialect CFG`)
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
  const floatBitRepresentationArtifact = artifacts.get(
    "float-bit-representation").toString("utf8")
  const floatBitRepresentationEvidence = {
    i32ToF32: (floatBitRepresentationArtifact.match(
      /llvm\.bitcast [^\n]* : i32 to f32\n/gu) ?? []).length,
    i64ToF64: (floatBitRepresentationArtifact.match(
      /llvm\.bitcast [^\n]* : i64 to f64\n/gu) ?? []).length,
    f32ToI32: (floatBitRepresentationArtifact.match(
      /llvm\.bitcast [^\n]* : f32 to i32\n/gu) ?? []).length,
    f64ToI64: (floatBitRepresentationArtifact.match(
      /llvm\.bitcast [^\n]* : f64 to i64\n/gu) ?? []).length,
    f32Narrow: (floatBitRepresentationArtifact.match(
      /_float_bits_narrow = llvm\.trunc [^\n]* : i64 to i32\n/gu) ?? []).length,
    f32Bitcast: (floatBitRepresentationArtifact.match(
      /_float_bits_i32 = llvm\.bitcast [^\n]* : f32 to i32\n/gu) ?? []).length,
    f32Extend: (floatBitRepresentationArtifact.match(
      /llvm\.zext [^\n]* : i32 to i64\n/gu) ?? []).length,
    floatBitsHelper: floatBitRepresentationArtifact.includes(
      "@w_seed_float_bits"),
    numericConversions: ["llvm.fptosi", "llvm.fptoui", "llvm.sitofp",
      "llvm.uitofp", "llvm.fpext", "llvm.fptrunc"].some((operation) =>
      floatBitRepresentationArtifact.includes(operation)),
    fastMath: floatBitRepresentationArtifact.includes("fastmath"),
  }
  assert(floatBitRepresentationEvidence.i32ToF32 === 3 &&
    floatBitRepresentationEvidence.i64ToF64 === 3 &&
    floatBitRepresentationEvidence.f32ToI32 === 3 &&
    floatBitRepresentationEvidence.f64ToI64 === 3 &&
    floatBitRepresentationEvidence.f32Narrow === 3 &&
    floatBitRepresentationEvidence.f32Bitcast === 3 &&
    floatBitRepresentationEvidence.f32Extend === 3 &&
    !floatBitRepresentationEvidence.floatBitsHelper &&
    !floatBitRepresentationEvidence.numericConversions &&
    !floatBitRepresentationEvidence.fastMath,
  `float bit representation must lower through direct width-correct bitcasts without numeric FP conversions, float-bit helpers, or fast-math: ${JSON.stringify(floatBitRepresentationEvidence)}`)
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
    ...runtimeIntegerNegationMinimumPaths,
    { name: "runtime-shift-count", source: runtimeShiftCountPath },
    { name: "runtime-unsigned-shift-count",
      source: runtimeUnsignedShiftCountPath },
    { name: "runtime-unsigned-shift-overflow",
      source: runtimeUnsignedShiftOverflowPath },
    { name: "runtime-signed-shift-overflow",
      source: runtimeSignedShiftOverflowPath },
    { name: "runtime-narrow-unsigned-shift-overflow",
      source: runtimeNarrowUnsignedShiftOverflowPath },
    { name: "runtime-narrow-signed-shift-overflow",
      source: runtimeNarrowSignedShiftOverflowPath },
    { name: "runtime-narrow-negative-shift-overflow",
      source: runtimeNarrowNegativeShiftOverflowPath },
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
  assert(artifacts.get("binding").equals(
    artifacts.get("literal")),
  "Restaurant literal and binding MLIR artifacts differ")
  assert(artifacts.get("linear").equals(
    artifacts.get("linear-literal")),
  "Restaurant linear literal and binding MLIR artifacts differ")
  assert(!artifacts.get("hello").equals(artifacts.get("binding")),
    "Restaurant payload did not change MLIR")
  assert(!artifacts.get("hello").equals(artifacts.get("empty")),
    "empty payload did not change MLIR")
  for (const name of ["interpolation", "checked-arithmetic",
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
  const checkedArithmeticArtifact = artifacts.get("checked-arithmetic")
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
  const integerBitwiseArtifact = artifacts.get("integer-bitwise")
  assert(integerBitwiseArtifact.includes("llvm.and ") &&
    integerBitwiseArtifact.includes("llvm.xor ") &&
    integerBitwiseArtifact.includes("llvm.or ") &&
    integerBitwiseArtifact.includes("_bitwise_left = llvm.trunc") &&
    integerBitwiseArtifact.includes("_bitwise_right = llvm.trunc") &&
    integerBitwiseArtifact.includes("_bitwise_raw = llvm.or") &&
    integerBitwiseArtifact.includes("llvm.sext %v") &&
    integerBitwiseArtifact.includes("llvm.zext %v") &&
    integerBitwiseArtifact.includes("llvm.call @w_fn_0") &&
    !integerBitwiseArtifact.includes("w_seed_checked_bit"),
  "fixed-width integer bitwise lowering did not normalize logical widths")
  const widenedFunctionStart = integerBitwiseArtifact.indexOf(
    "  llvm.func internal @w_fn_10(")
  const widenedFunctionEnd = integerBitwiseArtifact.indexOf(
    "  llvm.func internal @w_fn_", widenedFunctionStart + 1)
  const widenedFunctionArtifact = widenedFunctionStart < 0 ? "" :
    integerBitwiseArtifact.slice(widenedFunctionStart,
      widenedFunctionEnd < 0 ? undefined : widenedFunctionEnd)
  assert(widenedFunctionArtifact.includes("_widen_trunc = llvm.trunc") &&
    widenedFunctionArtifact.includes("_bitwise_left = llvm.trunc") &&
    widenedFunctionArtifact.includes("_bitwise_right = llvm.trunc") &&
    widenedFunctionArtifact.includes("_bitwise_raw = llvm.or") &&
    widenedFunctionArtifact.includes("_bitwise_raw : i32 to i64") &&
    !widenedFunctionArtifact.includes("llvm.call @w_seed_"),
  "i8-to-i32 bitwise widening did not lower directly at logical width")
  const mixedFunctionStart = integerBitwiseArtifact.indexOf(
    "  llvm.func internal @w_fn_11(")
  const mixedFunctionEnd = integerBitwiseArtifact.indexOf(
    "  llvm.func internal @w_fn_", mixedFunctionStart + 1)
  const mixedFunctionArtifact = mixedFunctionStart < 0 ? "" :
    integerBitwiseArtifact.slice(mixedFunctionStart,
      mixedFunctionEnd < 0 ? undefined : mixedFunctionEnd)
  const mixedSourceWidening = mixedFunctionArtifact.indexOf("llvm.zext %v")
  const mixedDirectOr = mixedFunctionArtifact.indexOf(
    "_bitwise_raw = llvm.or")
  assert(mixedFunctionArtifact.includes("_widen_trunc = llvm.trunc") &&
    mixedSourceWidening >= 0 && mixedDirectOr > mixedSourceWidening &&
    mixedFunctionArtifact.includes("_bitwise_left = llvm.trunc") &&
    mixedFunctionArtifact.includes("_bitwise_right = llvm.trunc") &&
    mixedFunctionArtifact.includes("_bitwise_raw : i16 to i64") &&
    !mixedFunctionArtifact.includes("llvm.call @w_seed_"),
  "u8-to-i16 bitwise widening did not zext before direct logical-width OR")
  const unsignedArtifact = artifacts.get("unsigned")
  assert(unsignedArtifact.includes("llvm.mlir.constant(-1 : i64) : i64") &&
    unsignedArtifact.includes("llvm.func internal @w_seed_append_u64") &&
    unsignedArtifact.includes("llvm.call @w_seed_append_u64") &&
    unsignedArtifact.includes("llvm.udiv") &&
    unsignedArtifact.includes("llvm.urem") &&
    unsignedArtifact.includes("llvm.call @w_fn_0") &&
    !unsignedArtifact.includes("llvm.call @w_seed_append_i64"),
  "UInt did not retain its unsigned full-width lowering and formatter")
  const shiftsArtifact = artifacts.get("shifts")
  const shiftsArtifactText = shiftsArtifact.toString("utf8")
  const shiftEvidence = {
    width8: (shiftsArtifactText.match(/_checked_width = llvm\.mlir\.constant\(8 : i64\)/gu) ?? []).length,
    width16: (shiftsArtifactText.match(/_checked_width = llvm\.mlir\.constant\(16 : i64\)/gu) ?? []).length,
    width32: (shiftsArtifactText.match(/_checked_width = llvm\.mlir\.constant\(32 : i64\)/gu) ?? []).length,
    width64: (shiftsArtifactText.match(/_checked_width = llvm\.mlir\.constant\(64 : i64\)/gu) ?? []).length,
    signed: (shiftsArtifactText.match(/_checked_signed = llvm\.mlir\.constant\(true\)/gu) ?? []).length,
    unsigned: (shiftsArtifactText.match(/_checked_signed = llvm\.mlir\.constant\(false\)/gu) ?? []).length,
    left: (shiftsArtifactText.match(/llvm\.call @w_seed_checked_shift_left\(/gu) ?? []).length,
    right: (shiftsArtifactText.match(/llvm\.call @w_seed_checked_shift_right\(/gu) ?? []).length,
  }
  assert(shiftsArtifact.includes(
    "llvm.func internal @w_seed_checked_shift_left(%left: i64, %count: i64, %width: i64, %is_signed: i1)") &&
    shiftsArtifact.includes(
      "llvm.func internal @w_seed_checked_shift_right(%left: i64, %count: i64, %width: i64, %is_signed: i1)") &&
    shiftsArtifact.includes('llvm.icmp "uge" %count, %width : i64') &&
    shiftsArtifact.includes("llvm.shl %left, %count : i64") &&
    shiftsArtifact.includes("llvm.ashr %left, %count : i64") &&
    shiftsArtifact.includes("llvm.lshr %left, %count : i64") &&
    /* Four shifts per narrow width plus two checked negative-literal
       materializations; full-width negation lowers directly and adds none. */
    shiftEvidence.width8 === 6 && shiftEvidence.width16 === 6 &&
    shiftEvidence.width32 === 6 && shiftEvidence.width64 === 8 &&
    shiftEvidence.signed === 10 && shiftEvidence.unsigned === 10 &&
    shiftEvidence.left === 10 && shiftEvidence.right === 10,
  "checked shifts lost verified logical widths, signedness, or runtime lowering")
  const powerArtifact = artifacts.get("power")
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
  const floatArtifact = artifacts.get("float-strict").toString("utf8")
  assert(floatArtifact.includes("llvm.fadd") &&
    floatArtifact.includes("llvm.fsub") &&
    floatArtifact.includes("llvm.fmul") &&
    floatArtifact.includes("llvm.fdiv") &&
    floatArtifact.includes("llvm.fneg") &&
    floatArtifact.includes('llvm.fcmp "oeq"') &&
    floatArtifact.includes('llvm.fcmp "une"') &&
    floatArtifact.includes('llvm.fcmp "olt"') &&
    floatArtifact.includes('llvm.fcmp "ole"') &&
    floatArtifact.includes('llvm.fcmp "ogt"') &&
    floatArtifact.includes('llvm.fcmp "oge"') &&
    floatArtifact.includes("0x3fc00000 : f32") &&
    floatArtifact.includes("0x3ff8000000000000 : f64") &&
    !floatArtifact.includes("fastmath"),
  "strict float lowering lost a width, operator, predicate, bit pattern, or strict mode")
  const checkedIntegerArithmeticArtifact =
    artifacts.get("checked-integer-arithmetic").toString("utf8")
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
    artifacts.get("uint-wrapping-add").toString("utf8")
  assert(uintWrappingAddArtifact.includes("llvm.add %p0,") &&
    uintWrappingAddArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingAddArtifact.includes("@w_seed_checked_add_u64") &&
    !uintWrappingAddArtifact.includes("llvm.intr.uadd.with.overflow"),
  "u64.wrappingAdd did not retain direct wrapping u64 lowering")
  const uintWrappingSubtractArtifact =
    artifacts.get("uint-wrapping-subtract").toString("utf8")
  assert(uintWrappingSubtractArtifact.includes("llvm.sub %p0,") &&
    uintWrappingSubtractArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingSubtractArtifact.includes("@w_seed_checked_subtract_u64") &&
    !uintWrappingSubtractArtifact.includes("llvm.intr.usub.with.overflow"),
  "u64.wrappingSubtract did not retain direct wrapping u64 lowering")
  const uintWrappingMultiplyArtifact =
    artifacts.get("uint-wrapping-multiply").toString("utf8")
  assert(uintWrappingMultiplyArtifact.includes("llvm.mul %p0,") &&
    uintWrappingMultiplyArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingMultiplyArtifact.includes("@w_seed_checked_multiply_u64") &&
    !uintWrappingMultiplyArtifact.includes("llvm.intr.umul.with.overflow"),
  "u64.wrappingMultiply did not retain direct wrapping u64 lowering")
  const uintWrappingNegateArtifact =
    artifacts.get("uint-wrapping-negate").toString("utf8")
  assert(uintWrappingNegateArtifact.includes("llvm.mlir.constant(0 : i64)") &&
    uintWrappingNegateArtifact.includes("llvm.sub") &&
    uintWrappingNegateArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingNegateArtifact.includes("@w_seed_checked_subtract_u64") &&
    !uintWrappingNegateArtifact.includes("llvm.intr.usub.with.overflow"),
  "u64.wrappingNegate did not retain direct wrapping u64 lowering")
  const uintWrappingPowerArtifact =
    artifacts.get("uint-wrapping-power").toString("utf8")
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
    artifacts.get("uint-overflowing-power").toString("utf8")
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
    artifacts.get("uint-overflowing-family").toString("utf8")
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
    artifacts.get("uint-wrapping-shift-left").toString("utf8")
  assert((uintWrappingShiftLeftArtifact.match(
    /llvm\.func internal @w_seed_wrapping_shift_left_u64/g) ?? []).length === 1 &&
    uintWrappingShiftLeftArtifact.includes(
      "llvm.call @w_seed_wrapping_shift_left_u64") &&
    uintWrappingShiftLeftArtifact.includes('llvm.icmp "uge"') &&
    uintWrappingShiftLeftArtifact.includes("llvm.shl") &&
    uintWrappingShiftLeftArtifact.includes('"llvm.intr.trap"()') &&
    uintWrappingShiftLeftArtifact.includes("llvm.unreachable") &&
    uintWrappingShiftLeftArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintWrappingShiftLeftArtifact.includes("@w_seed_checked_shift_left") &&
    !uintWrappingShiftLeftArtifact.includes("llvm.intr.ushl.with.overflow"),
  "u64.wrappingShiftLeft lost its count guard or gained checked-shift semantics")
  const fixedIntegerShiftPoliciesArtifact =
    artifacts.get("fixed-integer-shift-policies").toString("utf8")
  assert(!fixedIntegerShiftPoliciesArtifact.includes(
      "@w_seed_masked_shift_left_u64") &&
    !fixedIntegerShiftPoliciesArtifact.includes(
      "@w_seed_masked_shift_right_u64") &&
    (fixedIntegerShiftPoliciesArtifact.match(
      /_shift_count_mod = llvm\.and /g) ?? []).length >= 25 &&
    fixedIntegerShiftPoliciesArtifact.includes("llvm.shl ") &&
    fixedIntegerShiftPoliciesArtifact.includes("llvm.ashr ") &&
    fixedIntegerShiftPoliciesArtifact.includes("llvm.lshr ") &&
    [7, 15, 31, 63].every((mask) => fixedIntegerShiftPoliciesArtifact.includes(
      `_shift_mask = llvm.mlir.constant(${mask} : i64)`)) &&
    !fixedIntegerShiftPoliciesArtifact.includes("@w_seed_checked_shift_left") &&
    !fixedIntegerShiftPoliciesArtifact.includes("@w_seed_checked_shift_right") &&
    !fixedIntegerShiftPoliciesArtifact.includes("llvm.intr.ushl.with.overflow") &&
    fixedIntegerShiftPoliciesArtifact.includes(
      "llvm.func internal @w_seed_logical_shift_right_integer") &&
    fixedIntegerShiftPoliciesArtifact.includes(
      "llvm.call @w_seed_logical_shift_right_integer") &&
    fixedIntegerShiftPoliciesArtifact.includes(
      "llvm.icmp \"uge\" %count, %width : i64") &&
    fixedIntegerShiftPoliciesArtifact.includes(
      "llvm.lshr %normalized, %count : i64") &&
    fixedIntegerShiftPoliciesArtifact.includes("\"llvm.intr.trap\"() : () -> ()") &&
    fixedIntegerShiftPoliciesArtifact.includes("llvm.unreachable") &&
    fixedIntegerShiftPoliciesArtifact.includes("llvm.call @w_seed_append_u64"),
  "fixed-width named shifts lost count masking, signedness, logical zero-fill, or invalid-count trap semantics")
  const uintRotatedLeftArtifact =
    artifacts.get("uint-rotated-left").toString("utf8")
  assert(!uintRotatedLeftArtifact.includes("@w_seed_rotated_left_u64") &&
    (uintRotatedLeftArtifact.match(/llvm\.intr\.fshl/g) ?? []).length === 1 &&
    uintRotatedLeftArtifact.includes(
      "_rotate_mask = llvm.mlir.constant(63 : i64)") &&
    uintRotatedLeftArtifact.includes("_rotate_mod = llvm.and ") &&
    uintRotatedLeftArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintRotatedLeftArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.rotatedLeft lost funnel-shift or modulo-width semantics")
  const uintRotatedRightArtifact =
    artifacts.get("uint-rotated-right").toString("utf8")
  assert(!uintRotatedRightArtifact.includes("@w_seed_rotated_right_u64") &&
    (uintRotatedRightArtifact.match(/llvm\.intr\.fshr/g) ?? []).length === 1 &&
    uintRotatedRightArtifact.includes(
      "_rotate_mask = llvm.mlir.constant(63 : i64)") &&
    uintRotatedRightArtifact.includes("_rotate_mod = llvm.and ") &&
    uintRotatedRightArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintRotatedRightArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.rotatedRight lost funnel-shift or modulo-width semantics")
  const uintCountOnesArtifact =
    artifacts.get("uint-count-ones").toString("utf8")
  assert((uintCountOnesArtifact.match(/llvm\.intr\.ctpop/g) ?? []).length === 1 &&
    uintCountOnesArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintCountOnesArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.countOnes lost population-count or total-operation semantics")
  const uintCountZerosArtifact =
    artifacts.get("uint-count-zeros").toString("utf8")
  assert((uintCountZerosArtifact.match(/llvm\.intr\.ctpop/g) ?? []).length === 1 &&
    uintCountZerosArtifact.includes(
      "_bit_width = llvm.mlir.constant(64 : i64)") &&
    uintCountZerosArtifact.includes("_bit_ones : i64") &&
    uintCountZerosArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintCountZerosArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.countZeros lost width-minus-popcount or total-operation semantics")
  const uintLeadingZerosArtifact =
    artifacts.get("uint-leading-zeros").toString("utf8")
  assert((uintLeadingZerosArtifact.match(/llvm\.intr\.ctlz/g) ?? []).length === 1 &&
    uintLeadingZerosArtifact.includes("is_zero_poison = false") &&
    uintLeadingZerosArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintLeadingZerosArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.countLeadingZeros lost non-poison ctlz or total-operation semantics")
  const uintTrailingZerosArtifact =
    artifacts.get("uint-trailing-zeros").toString("utf8")
  assert((uintTrailingZerosArtifact.match(/llvm\.intr\.cttz/g) ?? []).length === 1 &&
    uintTrailingZerosArtifact.includes("is_zero_poison = false") &&
    uintTrailingZerosArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintTrailingZerosArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.countTrailingZeros lost non-poison cttz or total-operation semantics")
  const uintReversedBitsArtifact =
    artifacts.get("uint-reversed-bits").toString("utf8")
  assert((uintReversedBitsArtifact.match(/llvm\.intr\.bitreverse/g) ?? []).length === 1 &&
    uintReversedBitsArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintReversedBitsArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.reversedBits lost direct bit-reverse or total-operation semantics")
  const uintReversedBytesArtifact =
    artifacts.get("uint-reversed-bytes").toString("utf8")
  assert((uintReversedBytesArtifact.match(/llvm\.intr\.bswap/g) ?? []).length === 1 &&
    uintReversedBytesArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintReversedBytesArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.reversedBytes lost direct byte-swap or total-operation semantics")
  const uintSaturatingAddArtifact =
    artifacts.get("uint-saturating-add").toString("utf8")
  assert((uintSaturatingAddArtifact.match(/llvm\.intr\.uadd\.sat/g) ?? []).length === 1 &&
    uintSaturatingAddArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintSaturatingAddArtifact.includes("@w_seed_checked_add_u64") &&
    !uintSaturatingAddArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.saturatingAdd lost direct saturation or total-operation semantics")
  const uintSaturatingSubtractArtifact =
    artifacts.get("uint-saturating-subtract").toString("utf8")
  assert((uintSaturatingSubtractArtifact.match(/llvm\.intr\.usub\.sat/g) ?? []).length === 1 &&
    uintSaturatingSubtractArtifact.includes("llvm.call @w_seed_append_u64") &&
    !uintSaturatingSubtractArtifact.includes("@w_seed_checked_subtract_u64") &&
    !uintSaturatingSubtractArtifact.includes("\"llvm.intr.trap\"() : () -> ()"),
  "u64.saturatingSubtract lost direct saturation or total-operation semantics")
  const uintSaturatingMultiplyArtifact =
    artifacts.get("uint-saturating-multiply").toString("utf8")
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
    artifacts.get("uint-saturating-policy").toString("utf8")
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
    artifacts.get("uint-bit-not").toString("utf8")
  assert(uintBitNotArtifact.includes("llvm.xor") &&
    uintBitNotArtifact.includes("-1 : i64") &&
    !uintBitNotArtifact.includes("@w_seed_checked_negate_i64"),
  "UInt bitwise complement lost direct all-ones xor lowering")
  const uintBitwiseArtifact =
    artifacts.get("uint-bitwise").toString("utf8")
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
    artifacts.get("uint-compound").toString("utf8")
  for (const helper of [
    "@w_seed_checked_add_u64",
    "@w_seed_checked_subtract_u64",
    "@w_seed_checked_multiply_u64",
    "@w_seed_checked_divide_u64",
    "@w_seed_checked_remainder_u64",
    "@w_seed_checked_power_u64",
    "@w_seed_checked_shift_left",
    "@w_seed_checked_shift_right",
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
    uintCompoundFunction.includes(
      "_checked_signed = llvm.mlir.constant(false) : i1") &&
    !uintCompoundFunction.includes(
      "_checked_signed = llvm.mlir.constant(true) : i1") &&
    !uintCompoundFunction.includes("llvm.sdiv ") &&
    !uintCompoundFunction.includes("llvm.srem ") &&
    !uintCompoundFunction.includes("llvm.ashr ") &&
    !uintCompoundFunction.includes("llvm.intr.smul.with.overflow") &&
    !uintCompoundFunction.includes("llvm.alloca"),
  "UInt compound mutation gained a signed helper or operation")
  const wmoArtifact = artifacts.get("wmo")
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
  const asyncYieldArtifact = artifacts.get("async-yield")
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
  const runtimeDivremArtifact = artifacts.get(
    "checked-integer-arithmetic").toString("utf8")
  assert(runtimeDivremArtifact.includes(
    "llvm.call @w_seed_checked_divide_i64") &&
    runtimeDivremArtifact.includes("llvm.sdiv %left, %right") &&
    runtimeDivremArtifact.includes(
      "llvm.call @w_seed_checked_remainder_i64") &&
    runtimeDivremArtifact.includes("llvm.srem %left, %right") &&
    artifacts.get("runtime-minimum-remainder").includes(
      "llvm.cond_br %overflow_pair, ^checked_minimum, ^checked_ok"),
  "runtime division/remainder checks were omitted or precomputed")
  const runtimeNegateArtifact = artifacts.get("unary-negate")
  assert(runtimeNegateArtifact.includes(
    "llvm.call @w_seed_checked_subtract_i64") &&
    runtimeNegateArtifact.includes("_neg_zero") &&
    !artifacts.get("direct-unary-interpolation").includes(
      "@w_seed_checked_subtract_i64") &&
    artifacts.get("direct-unary-interpolation").includes(" = llvm.sub "),
  "checked runtime or direct constant unary negation was not retained")
  const integerPrefixArtifact = artifacts.get("integer-prefix")
    .toString("utf8")
  assert((integerPrefixArtifact.match(
    /llvm\.call @w_seed_checked_subtract_i64\(/gu) ?? []).length === 5 &&
    (integerPrefixArtifact.match(/llvm\.xor /gu) ?? []).length >= 10 &&
    integerPrefixArtifact.includes("llvm.trunc") &&
    integerPrefixArtifact.includes("llvm.sext") &&
    integerPrefixArtifact.includes("llvm.zext") &&
    integerPrefixArtifact.includes("llvm.call @w_fn_0") &&
    !integerPrefixArtifact.includes("w_seed_checked_bit"),
  "integer prefix family lost runtime-shaped checked negation or logical-width bitwise lowering")
  const integerTruncatingBitsArtifact = artifacts.get(
    "integer-truncating-bits").toString("utf8")
  assert((integerTruncatingBitsArtifact.match(
    /_truncating_source_bits = llvm\.trunc %p0 : i64 to i(?:8|16|32)\n/gu) ?? []).length === 3 &&
    (integerTruncatingBitsArtifact.match(
      /_truncating_source = llvm\.(?:sext|zext) %v\d+_truncating_source_bits : i(?:8|16|32) to i64\n/gu) ?? []).length === 3 &&
    (integerTruncatingBitsArtifact.match(
      /_truncating_destination_bits = llvm\.trunc %v\d+_truncating_source : i64 to i(?:8|16)\n/gu) ?? []).length === 3 &&
    (integerTruncatingBitsArtifact.match(
      / = llvm\.(?:sext|zext) %v\d+_truncating_destination_bits : i(?:8|16) to i64\n/gu) ?? []).length === 3 &&
    (integerTruncatingBitsArtifact.match(
      /_truncating_zero = llvm\.mlir\.constant\(0 : i64\) : i64\n/gu) ?? []).length === 2 &&
    (integerTruncatingBitsArtifact.match(
      / = llvm\.or %p0, %v\d+_truncating_zero : i64\n/gu) ?? []).length === 2,
  "truncatingBits must canonicalize each narrow source, truncate destination bits, and extend by destination signedness")
  const integerSaturatingConversionArtifact = artifacts.get(
    "integer-saturating-conversion").toString("utf8")
  const saturatingCompareCount = (integerSaturatingConversionArtifact.match(
    /llvm\.icmp "(?:slt|sgt|ugt)" %v\d+_saturating_/gu) ?? []).length
  const saturatingSelectCount = (integerSaturatingConversionArtifact.match(
    /llvm\.select %v\d+_saturating_/gu) ?? []).length
  assert(saturatingCompareCount > 0 &&
    saturatingSelectCount === saturatingCompareCount &&
    integerSaturatingConversionArtifact.includes(
      'llvm.icmp "slt" %v') &&
    integerSaturatingConversionArtifact.includes(
      'llvm.icmp "sgt" %v') &&
    integerSaturatingConversionArtifact.includes(
      'llvm.icmp "ugt" %v') &&
    integerSaturatingConversionArtifact.includes("_saturating_source_bits = llvm.trunc") &&
    integerSaturatingConversionArtifact.includes("_saturating_source = llvm.sext") &&
    integerSaturatingConversionArtifact.includes("_saturating_source = llvm.zext"),
  "saturating conversions must normalize their source and clamp with compare/select")
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
    "conditional-mutation").toString("utf8")
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
  const boolMutationArtifact = artifacts.get("bool-mutation")
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
  const branchMutationArtifact = artifacts.get("branch-mutation")
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
    "branch-mutation-multi").toString("utf8")
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
  const naturalLoopArtifact = artifacts.get("while").toString("utf8")
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
  const cfgArtifact = artifacts.get("if").toString("utf8")
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

  const nestedCfgArtifact = artifacts.get("nested-if").toString("utf8")
  const nestedJoinBranches = nestedCfgArtifact.match(
    /llvm\.br \^w_fn_0_b_(?:4|8|9)\n/gu) || []
  assert(nestedCfgArtifact.includes("llvm.cond_br %p0") &&
    (nestedCfgArtifact.match(/llvm\.cond_br %p1/gu) || []).length >= 2 &&
    nestedJoinBranches.length >= 6 &&
    nestedCfgArtifact.includes("\\50\\6f\\73\\74\\2d\\6a\\6f\\69\\6e\\20\\73\\65\\72\\76\\69\\63\\65\\0a") &&
    (nestedCfgArtifact.match(/llvm\.call @w_fn_0/gu) || []).length === 4,
  "nested Restaurant did not retain both inner diamonds and one post-join call per invocation")

  const nestedScalarArtifact = artifacts.get("nested-scalar-if")
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
    ["integer-saturating-wrong-label.w",
      `entry { print("must not commit") ` +
      `let value = i8(saturating: 128_i16, other: 0_i16) }\n`],
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
  console.log(`MLIR0: verified HIR0 → SCF/LLVM dialects → mlir-opt → mlir-translate → clang IR/native passed (${dialectDisclosure(dialect)}; ${acceptedVersion.description})`)
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
  await rm(artifactDirectory, { recursive: true, force: true })
}
