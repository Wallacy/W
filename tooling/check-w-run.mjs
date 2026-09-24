import { existsSync } from "node:fs"
import { chmod, lstat, mkdir, mkdtemp, readdir, readFile, rm, symlink, unlink, writeFile } from "node:fs/promises"
import { createHash } from "node:crypto"
import { tmpdir } from "node:os"
import { isAbsolute, join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const localManifestPath = resolve(root, "tooling", "mlir0-toolchain.json")
const ciManifestPath = resolve(root, "tooling", "mlir0-ci-toolchain.json")
const helloFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const linearFixture = resolve(seedDirectory, "fixtures", "linear.w")
const interpolationFixture = resolve(seedDirectory, "fixtures", "interpolation.w")
const ifFixture = resolve(seedDirectory, "fixtures", "if.w")
const terminalReturnsFixture = resolve(seedDirectory, "fixtures", "terminal-returns.w")
const comparisonsFixture = resolve(seedDirectory, "fixtures", "comparisons.w")
const enumFixture = resolve(seedDirectory, "fixtures", "enum.w")
const enumSubsetFixture = resolve(seedDirectory, "fixtures", "enum-subset.w")
const enumPayloadFixture = resolve(seedDirectory,
  "fixtures", "enum-payload.w")
const enumBoolPayloadFixture = resolve(seedDirectory,
  "fixtures", "enum-bool-payload.w")
const comparisonCompositionFixture = resolve(seedDirectory, "fixtures", "comparison-composition.w")
const integerComparisonFixture = resolve(seedDirectory,
  "fixtures", "integer-comparison.w")
const boolShortCircuitFixture = resolve(seedDirectory, "fixtures", "bool-short-circuit.w")
const nestedIfFixture = resolve(seedDirectory, "fixtures", "nested-if.w")
const whileFixture = resolve(seedDirectory, "fixtures", "while.w")
const whileMultiFixture = resolve(seedDirectory,
  "fixtures", "while-multi.w")
const whilePostFixture = resolve(seedDirectory,
  "fixtures", "while-post.w")
const whileBreakContinueFixture = resolve(seedDirectory,
  "fixtures", "while-break-continue.w")
const nestedLabeledWhileFixture = resolve(seedDirectory,
  "fixtures", "nested-labeled-while.w")
const nestedLoopTerminalReturnsFixture = resolve(seedDirectory,
  "fixtures", "nested-loop-terminal-returns.w")
const repeatFixture = resolve(seedDirectory,
  "fixtures", "repeat.w")
const wmoFixture = resolve(seedDirectory, "fixtures", "wmo.w")
const asyncJoinFixture = resolve(seedDirectory,
  "fixtures", "async-join.w")
const asyncYieldFixture = resolve(seedDirectory,
  "fixtures", "async-yield.w")
const explicitPanicFixture = resolve(seedDirectory,
  "fixtures", "panic-explicit.w")
const mainDispatchFixture = resolve(seedDirectory,
  "fixtures", "main-dispatch0.w")
const mainCardinalityFixture = resolve(seedDirectory,
  "fixtures", "main-cardinality0.w")
const processArgumentsCountFixture = resolve(seedDirectory,
  "fixtures", "process-arguments-count.w")
const processArgumentsOrderingFixture = resolve(seedDirectory,
  "fixtures", "process-arguments-ordering.w")
const processInputFixture = resolve(seedDirectory, "fixtures", "process-input0.w")
const processEnumPayloadFixture = resolve(seedDirectory,
  "fixtures", "process-enum-payload.w")
const processIntegerExactSuccessFixture = resolve(seedDirectory, "fixtures",
  "process-integer-exact-success.w")
const processIntegerExactErrorFixture = resolve(seedDirectory, "fixtures",
  "process-integer-exact-error.w")
const processFloatRoundingSuccessFixture = resolve(seedDirectory, "fixtures",
  "process-float-rounding-success.w")
const processFloatRoundingRuntimeIfFixture = resolve(seedDirectory, "fixtures",
  "process-float-rounding-runtime-if.w")
const processFloatRoundingErrorFixture = resolve(seedDirectory, "fixtures",
  "process-float-rounding-error.w")
const processIntegerExactRuntimeFixture = resolve(seedDirectory, "fixtures",
  "process-fixed-integer-arithmetic.w")
const localGraphFixture = resolve(seedDirectory, "fixtures", "local-graph",
  "app.w")
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
const fixtureIntegerSaturatingConversionOutput = Buffer.from(
  "ss -128/7/127; us 7/127/127; su 0/200/255; " +
  "uu 7/255/255; UInt->Int 9223372036854775807\n", "utf8")
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
  "i8 -128/0/-64/64\nu8 128/0/64/64\n" +
  "i16 -32768/0/-16384/16384\nu16 32768/0/16384/16384\n" +
  "i32 -2147483648/0/-1073741824/1073741824\n" +
  "u32 2147483648/0/1073741824/1073741824\n" +
  "i64 -9223372036854775808/0/-4611686018427387904/4611686018427387904\n" +
  "u64 9223372036854775808/0/4611686018427387904/4611686018427387904\n" +
  "u64 small-value 2/64/64\n", "utf8")
const uIntOverflowingAddFixture = resolve(seedDirectory,
  "fixtures", "uint-overflowing-add.w")
const uIntOverflowingPowerFixture = resolve(seedDirectory,
  "fixtures", "uint-overflowing-power.w")
const uIntOverflowingFamilyFixture = resolve(seedDirectory,
  "fixtures", "uint-overflowing-family.w")
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
const w1531MinimalFixture = resolve(seedDirectory, "fixtures", "w1531-if-minimal.w")
const w1531NoElseFixture = resolve(seedDirectory, "fixtures", "w1531-if-no-else.w")
const w1531LearnerFixture = resolve(seedDirectory, "fixtures", "w1531-if-learner.w")
const w1531IdiomaticFixture = resolve(seedDirectory, "fixtures", "w1531-if-idiomatic.w")
const w1531FrontierFixture = resolve(seedDirectory, "fixtures", "w1531-if-frontier.w")
const targetTriple = "x86_64-unknown-linux-gnu"
const expectedHelp =
  "usage: w check <path/file.w> [--json]\n" +
  "usage: w run <path/file.w> [-- <args...>]\n" +
  "usage: w build <path/file.w> --target <target> --output <artifact> " +
  "[--pie <on|off>] [--audit-dir <new-directory>]\n" +
  "  --pie is a temporary Linux x86_64 seed option (default: on); " +
  "--audit-dir is development-only and non-ranking\n" +
  "usage: w bench process --exe <absolute-path> [options]\n" +
  "  options: --cwd <absolute-dir> --arg <value> --warmup <n> " +
  "--samples <n> --timeout-ms <n> --expect-exit <n> " +
  "--expect-stdout-hex <bytes> --expect-stderr-hex <bytes>\n" +
  (process.platform === "win32"
    ? "  Windows-native cold process measurement only; does not compile or " +
      "execute .w source\n"
    : "")
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
const expectedVersion = "23.1.1"
const developmentPatchCompatibility =
  !ciMode && process.env.W_MLIR0_DEVELOPMENT_PATCH_COMPAT !== "0"
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

export function resolveToolCommand(command, {
  role = command,
  versionedMajor,
  findExecutable,
  required = false,
}) {
  assert(typeof findExecutable === "function",
    "tool resolver requires an executable lookup")
  const candidates = versionedMajor === undefined
    ? [command] : [`${command}-${versionedMajor}`, command]
  for (const candidate of candidates) {
    const resolved = findExecutable(candidate)
    if (resolved) return resolved
  }
  if (required) throw new Error(`required tool ${role} is unavailable: ${command}`)
  return command
}

export function incompatibleToolVersions(outputs, expectedVersion,
  patchCompatible) {
  const parts = expectedVersion.split(".")
  const acceptedVersion = patchCompatible
    ? `${escapedVersion(parts[0])}\\.${escapedVersion(parts[1])}\\.[0-9]+`
    : escapedVersion(expectedVersion)
  const pattern = new RegExp(`(?<![\\d.])${acceptedVersion}(?![\\d.])`, "u")
  return Object.entries(outputs)
    .filter(([, output]) => !pattern.test(output))
    .map(([role]) => role)
}

export function validateManifest(manifest, mode = ciMode) {
  const manifestVersion = "23.1.1"
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
    manifest.artifact?.scope === "unit-structured-cfg-natural-loop",
  "toolchain manifest MLIR0 artifact scope is invalid")
  assert(manifest.target?.triple === targetTriple &&
    manifest.target?.os === "linux" && manifest.target?.abi === "gnu",
  "toolchain target is not the closed Linux GNU target")
  if (!mode) assert(manifest.toolchainDiscovery?.rootEnv ===
    "W_MLIR0_TOOLCHAIN_ROOT" &&
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
  "local toolchain discovery contract is invalid")
  for (const role of mode ? ["mlir", "llvm"] : ["mlir", "llvm", "clang"])
    assert(manifest.toolchain?.[role] === manifestVersion,
      `toolchain ${role} version is not ${manifestVersion}`)
  const commands = manifest.commands
  const expectedCommands = mode
    ? { mlirOpt: "mlir-opt", mlirTranslate: "mlir-translate",
        llvmConfig: "llvm-config", llvmOpt: "opt", llc: "llc",
        linkDriver: "/usr/bin/ld" }
    : { mlirOpt: "mlir-opt", mlirTranslate: "mlir-translate",
        llvmConfig: "llvm-config", llvmOpt: "opt", clang: "clang", llc: "llc",
        linkDriver: "/usr/bin/ld" }
  for (const [role, expected] of Object.entries(expectedCommands)) {
    const command = commands?.[role]
    assert(command?.linux === expected && (mode || command?.wsl === expected) &&
      JSON.stringify(command.versionArgs) === JSON.stringify(["--version"]),
    `toolchain command ${role} is not the pinned absolute command`)
  }
  assert(Array.isArray(manifest.pipeline) && manifest.pipeline.length === (mode ? 6 : 3),
    "toolchain pipeline is invalid")
  const pipeline = manifest.pipeline
  assert(pipeline[0]?.tool === "mlir-opt" &&
    JSON.stringify(pipeline[0].args) === JSON.stringify([
      "<input.mlir>", "-o", "<verified.mlir>", "--convert-scf-to-cf",
      "--convert-cf-to-llvm", "--verify-each",
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
    assert(pipeline[2]?.tool === "opt" &&
      JSON.stringify(pipeline[2].args) === JSON.stringify([
        "-passes=instcombine,simplifycfg",
        "-S", "<output.ll>", "-o",
        "<optimized.ll>",
      ]), "LLVM optimization recipe changed")
    assert(pipeline[3]?.tool === "llc" &&
      JSON.stringify(pipeline[3].args) === JSON.stringify([
        `-mtriple=${targetTriple}`, "-filetype=obj", "-relocation-model=pic",
        "<optimized.ll>", "-o", "<output.o>",
      ]), "llc recipe changed")
    assert(pipeline[4]?.tool === "llc" &&
      JSON.stringify(pipeline[4].args) === JSON.stringify([
        `-mtriple=${targetTriple}`, "-filetype=obj", "-relocation-model=pic",
        "<wrt0.ll>", "-o", "<wrt0.o>",
      ]), "WRT0 object recipe changed")
    assert(pipeline[5]?.tool === "link-driver" &&
      JSON.stringify(pipeline[5].args) === JSON.stringify([
        "-pie", "--no-dynamic-linker", "--hash-style=gnu", "-e", "_start", "--gc-sections",
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
  // The shared manifest retains the check:mlir0 clang recipe, not this runner.
  return { mlirOpt: expectedCommands.mlirOpt,
    mlirTranslate: expectedCommands.mlirTranslate,
    llvmConfig: expectedCommands.llvmConfig,
    llvmOpt: expectedCommands.llvmOpt, llc: expectedCommands.llc,
    linkDriver: expectedCommands.linkDriver }
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

function normalizeExternalToolchainRoot(value) {
  if (typeof value !== "string" || value.length === 0)
    fail("W_MLIR0_TOOLCHAIN_ROOT is empty")
  const normalized = value.replace(/\/+$/u, "")
  const valid = isWindows
    ? /^\/[A-Za-z0-9._+\-/]+$/u.test(normalized)
    : isAbsolute(normalized)
  if (!valid || normalized === "/tmp" || normalized.startsWith("/tmp/"))
    fail("W_MLIR0_TOOLCHAIN_ROOT must be a persistent absolute path outside /tmp")
  return normalized
}

function wslWhich(command) {
  const result = wslRun("sh", ["-lc", `command -v ${command}`])
  if (result.exitCode !== 0) return undefined
  const value = result.stdoutBytes.toString().trim()
  return /^\/[A-Za-z0-9._+\-/]+$/u.test(value) ? value : undefined
}

function findHostExecutable(command) {
  return isWindows
    ? wslWhich(command)
    : Bun.which(command) ??
      (isAbsolute(command) && existsSync(command) ? command : undefined)
}

function resolveLocalTool(command, role, externalRoot) {
  if (role === "linkDriver")
    return resolveToolCommand(command, {
      role: "native link driver",
      findExecutable: findHostExecutable,
      required: true,
    })
  assert(/^[A-Za-z0-9._+-]+$/u.test(command),
    `local tool ${role} is not a simple command name`)
  if (externalRoot) return `${externalRoot}/bin/${command}`
  // The suffix is only a discovery hint; versionProbe below enforces the
  // exact pin or the separately authorized 23.1.x development allowance.
  const [major] = expectedVersion.split(".")
  return resolveToolCommand(command, {
    role,
    versionedMajor: major,
    findExecutable: findHostExecutable,
  })
}

function versionProbe(command, versionArgs) {
  if (!isWindows && !existsSync(command))
    return { present: false, succeeded: false, output: "" }
  const result = isWindows
    ? wslRun(command, versionArgs)
    : spawn(command, versionArgs)
  const present = result.exitCode !== 127
  const output = `${result.stdoutBytes}\n${result.stderrBytes}`
  return {
    present,
    succeeded: present && result.exitCode === 0,
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

async function validateExternalMaterialization(toolchainRoot, manifest) {
  const receiptPath = `${toolchainRoot}/w-mlir0-linux-materialized.json`
  let source
  try {
    if (isWindows) {
      const result = wslRun("cat", [receiptPath])
      if (result.exitCode !== 0)
        fail(`external MLIR0 materialized receipt is unavailable: ${receiptPath}`)
      source = result.stdoutBytes.toString()
    } else source = await readFile(receiptPath, "utf8")
  } catch (error) {
    fail(`external MLIR0 materialized receipt is unavailable: ${error.message}`)
  }
  let receipt
  try {
    receipt = JSON.parse(source)
  } catch (error) {
    fail(`external MLIR0 materialized receipt is invalid: ${error.message}`)
  }
  const archive = manifest.toolchainDiscovery.archive
  assert(receipt?.$schema === "w-seed-mlir0-linux-materialized-1" &&
    receipt.version === 1 && receipt.bin === "bin" &&
    JSON.stringify(receipt.requiredTools) ===
      JSON.stringify(["mlir-opt", "mlir-translate", "llvm-config", "llc"]) &&
    receipt.toolchain?.mlir === "23.1.1" &&
    receipt.toolchain?.llvm === "23.1.1" &&
    receipt.target?.triple === "x86_64-unknown-linux-gnu" &&
    receipt.distribution?.release === archive.release &&
    receipt.distribution?.filename === archive.filename &&
    receipt.distribution?.sizeBytes === archive.sizeBytes &&
    receipt.distribution?.sha256 === archive.sha256 &&
    receipt.distribution?.source === archive.source,
  "external MLIR0 materialization does not match the pinned 23.1.1 archive")
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

function expectExact(binary, args, exitCode, expected, label) {
  const result = invoke(binary, args)
  assert(result.exitCode === exitCode && result.stdout.equals(expected) &&
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
    const residue = []
    for (const pattern of [".w-build-*", ".w-audit-*"]) {
      const result = runRequired(`${label} WSL residue check`, "wsl.exe", [
        "-d", "Ubuntu", "--", "find", directory, "-mindepth", "1",
        "-maxdepth", "1", "-type", "d", "-name", pattern, "-print",
      ])
      assert(result.stderrBytes.length === 0,
        `${label} residue check wrote stderr`)
      residue.push(...result.stdoutBytes.toString()
        .split(/\r?\n/u).filter(Boolean))
    }
    assert(residue.length === 0,
      `${label} left staging entries: ${JSON.stringify(residue)}`)
    return
  }
  const entries = await readdir(directory, { withFileTypes: true })
  const residue = entries.filter((entry) => entry.name.startsWith(".w-build-") ||
    entry.name.startsWith(".w-audit-"))
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

function auditChild(directory, name) {
  return isWindows ? `${directory}/${name}` : join(directory, name)
}

async function readBuildAuditFile(directory, name) {
  const path = auditChild(directory, name)
  if (!isWindows) return readFile(path)
  return runRequired(`WSL audit trace read ${name}`, "wsl.exe", [
    "-d", "Ubuntu", "--", "cat", path,
  ]).stdoutBytes
}

async function listBuildAuditFiles(directory) {
  if (!isWindows)
    return (await readdir(directory, { withFileTypes: true }))
      .filter((entry) => entry.isFile()).map((entry) => entry.name).sort()
  const result = runRequired("WSL audit trace inventory", "wsl.exe", [
    "-d", "Ubuntu", "--", "find", directory, "-mindepth", "1",
    "-maxdepth", "1", "-type", "f", "-printf", "%f,",
  ])
  assert(result.stderrBytes.length === 0, "WSL audit trace inventory wrote stderr")
  return result.stdoutBytes.toString().split(",").filter(Boolean).sort()
}

async function verifyAuditTrace(directory, finalArtifact,
                                buildArtifactDirectory) {
  const expectedFiles = ["final-artifact", "input.mlir", "manifest.json",
    "optimized.ll", "output.ll", "output.o", "verified.mlir", "wrt0.ll",
    "wrt0.o"].sort()
  const actualFiles = await listBuildAuditFiles(directory)
  assert(JSON.stringify(actualFiles) === JSON.stringify(expectedFiles),
    `audit trace file inventory is not exact: ${JSON.stringify({
      expectedFiles, actualFiles,
    })}`)
  const manifestBytes = await readBuildAuditFile(directory, "manifest.json")
  const manifestText = manifestBytes.toString("utf8")
  const manifest = JSON.parse(manifestText)
  assert(manifest.schema === "w-seed-audit-trace-1" &&
    manifest.purpose === "development-only/non-ranking" &&
    manifest.closure_status === "inspection-required",
  "audit manifest does not identify its non-ranking inspection status")
  assert(manifest.product?.target === targetTriple &&
    manifest.product?.abi === "linux-gnu" &&
    manifest.product?.profile === "release" &&
    manifest.product?.final_artifact === "final-artifact" &&
    JSON.stringify(manifest.link?.inputs) ===
      JSON.stringify(["output.o", "wrt0.o"]),
  "audit manifest does not bind the Linux multi-object release product")
  assert(/^[0-9a-f]{64}$/u.test(manifest.compiler_binary_sha256) &&
    /^[0-9a-f]{64}$/u.test(manifest.source_id_sha256) &&
    Array.isArray(manifest.tools) && manifest.tools.length === 5 &&
    manifest.tools.every((tool) => /^[0-9a-f]{64}$/u.test(tool.sha256)),
  "audit manifest compiler/source/tool hashes are incomplete")
  assert(!manifestText.includes(buildArtifactDirectory) &&
    !manifestText.includes("W_MLIR0_TOOLCHAIN_ROOT") &&
    !manifestText.includes("argv") && !manifestText.includes("environment"),
  "audit manifest includes a local path, environment, or invocation history")
  const records = manifest.artifacts
  assert(Array.isArray(records) && records.length === 8 &&
    JSON.stringify(records.map((record) => record.name)) ===
      JSON.stringify(["input.mlir", "verified.mlir", "output.ll",
        "optimized.ll", "output.o", "wrt0.ll", "wrt0.o", "final-artifact"]),
  "audit manifest artifact order does not bind every product and WRT0 input")
  for (const record of records) {
    const bytes = await readBuildAuditFile(directory, record.name)
    assert(record.bytes === bytes.length &&
      record.sha256 === createHash("sha256").update(bytes).digest("hex"),
    `audit manifest digest/size does not match ${record.name}`)
  }
  assert((await readBuildAuditFile(directory, "final-artifact")).equals(
    await readBuildArtifact(finalArtifact)),
  "audit final artifact is not byte-identical to the published product")
}

export function assertCrtFreeElf(bytes) {
  return assertCrtFreeElfType(bytes, 3, "static PIE")
}

export function assertCrtFreeExecElf(bytes) {
  return assertCrtFreeElfType(bytes, 2, "static executable")
}

function assertCrtFreeElfType(bytes, expectedType, expectedKind) {
  assert(Buffer.isBuffer(bytes) && bytes.length >= 64 &&
    bytes.subarray(0, 4).equals(Buffer.from([0x7f, 0x45, 0x4c, 0x46])) &&
    bytes[4] === 2 && bytes[5] === 1,
  "built artifact is not little-endian ELF64")
  assert(bytes.readUInt16LE(16) === expectedType && bytes.readUInt16LE(18) === 62,
    `built artifact is not an x86_64 ${expectedKind}`)
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

export function assertElfNoExecutableStack(bytes) {
  assert(Buffer.isBuffer(bytes) && bytes.length >= 64 &&
    bytes.subarray(0, 4).equals(Buffer.from([0x7f, 0x45, 0x4c, 0x46])) &&
    bytes[4] === 2 && bytes[5] === 1,
  "built artifact is not little-endian ELF64")
  const headerOffset = Number(bytes.readBigUInt64LE(32))
  const headerSize = bytes.readUInt16LE(54)
  const headerCount = bytes.readUInt16LE(56)
  assert(Number.isSafeInteger(headerOffset) && headerSize >= 56 &&
    headerCount > 0 && headerOffset + headerSize * headerCount <= bytes.length,
  "ELF program-header table is invalid")
  let foundStack = false
  for (let index = 0; index < headerCount; index += 1) {
    const offset = headerOffset + index * headerSize
    if (bytes.readUInt32LE(offset) !== 0x6474e551) continue
    foundStack = true
    assert((bytes.readUInt32LE(offset + 4) & 1) === 0,
      "built ELF requests an executable stack")
  }
  assert(foundStack, "built ELF has no GNU_STACK program header")
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
const emitterSource = await readFile(resolve(seedDirectory, "src", "w_seed_mlir0.c"),
  "utf8")
for (const marker of ["W_SEED_LINUX_MLIR_OPT_PATH",
  "W_SEED_LINUX_MLIR_TRANSLATE_PATH", "W_SEED_LINUX_LLVM_OPT_PATH",
  "W_SEED_LINUX_LLC_PATH",
  "W_SEED_LINUX_LINK_DRIVER_PATH"])
  assert(runSource.includes(marker), `cli/run.c does not use ${marker}`)
for (const marker of ["--convert-scf-to-cf", "--convert-arith-to-llvm",
  "--convert-func-to-llvm", "--convert-cf-to-llvm",
  "--canonicalize", "--cse", "-O3", "-s", "-no-pie",
  "-relocation-model=static",
  "--no-dynamic-linker", "--gc-sections", "_start", "w_seed_wrt0_get"])
  assert(runSource.includes(marker),
    `cli/run.c is missing the release build flag ${marker}`)
assert(!runSource.includes("--disable-simplify-libcalls"),
  "cli/run.c must preserve LLVM libcall simplification")
assert(emitterSource.includes("no-builtin-strlen"),
  "Linux argv scan must suppress only strlen builtin synthesis")
assert(runSource.includes("W_SEED_RUN_COMPILE_PROFILE_DEV"),
  "cli/run.c does not select the development compile profile for w run")
assert(buildSource.includes("W_SEED_RUN_COMPILE_PROFILE_RELEASE"),
  "cli/build.c does not select the release compile profile for w build")
const llvmRoles = ["mlirOpt", "mlirTranslate", "llvmConfig", "llvmOpt", "llc"]
const roles = [...llvmRoles, "linkDriver"]
const externalToolchainRoot = !ciMode &&
  process.env.W_MLIR0_TOOLCHAIN_ROOT !== undefined
  ? normalizeExternalToolchainRoot(process.env.W_MLIR0_TOOLCHAIN_ROOT)
  : undefined
if (externalToolchainRoot !== undefined)
  await validateExternalMaterialization(externalToolchainRoot, manifest)
const resolvedCommands = Object.fromEntries(roles.map((role) => {
  if (!ciMode) return [role,
    resolveLocalTool(commands[role], role, externalToolchainRoot)]
  if (role === "linkDriver") return [role, resolveToolCommand(
    commands[role], {
      role: "native link driver",
      findExecutable: findHostExecutable,
      required: true,
    })]
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
const invalidVersionRoles = incompatibleToolVersions(
  Object.fromEntries(probes.map(([role, probe]) => [role, probe.output])),
  expectedVersion, developmentPatchCompatibility)
for (const [role, probe] of probes) {
  if (probe.succeeded && !invalidVersionRoles.includes(role)) continue
  fail(`${role} version is not ${developmentPatchCompatibility
    ? "in the 23.1.x development line" : expectedVersion}: ${probe.output.trim()}`)
}

const hostProbe = (command, args) => isWindows ? wslRun(command, args) : spawn(command, args)
const linkTarget = hostProbe(resolvedCommands.linkDriver, ["-V"])
const linkTargetOutput = `${linkTarget.stdoutBytes.toString()}\n${linkTarget.stderrBytes.toString()}`
assert(linkTarget.exitCode === 0 && /(?:^|\s)elf_x86_64(?:\s|$)/u.test(linkTargetOutput),
"native linker does not report elf_x86_64 support")
const linkVersion = hostProbe(resolvedCommands.linkDriver, ["--version"])
assert(linkVersion.exitCode === 0, "native linker version probe failed")
console.log(`W RUN: LLVM tools ${developmentPatchCompatibility
  ? "23.1.x development-compatible" : expectedVersion}; native linker ${resolvedCommands.linkDriver}: ` +
  `${linkVersion.stdoutBytes.toString().split(/\r?\n/u)[0]}; target elf_x86_64`)
console.log("W RUN: stages MLIR → LLVM IR → LLVM opt (libcall simplification enabled; argv length scans are function-scoped) → llc PIC objects → WRT0 + direct static-PIE link (no CRT/libc)")

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
  const failureTool = join(fixtureDirectory, "fail-tool")
  await writeFile(failureTool, "#!/bin/sh\nexit 1\n")
  if (isWindows) {
    runRequired("WSL private failure-tool mode", "wsl.exe", [
      "-d", "Ubuntu", "--", "chmod", "755", wslPath(failureTool),
    ])
  } else await chmod(failureTool, 0o755)
  const failureToolForHost = isWindows ? wslPath(failureTool) : failureTool
  const toolNames = {
    mlirOpt: "mlir-opt",
    mlirTranslate: "mlir-translate",
    llvmConfig: "llvm-config",
    llvmOpt: "opt",
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
    `-DW_MLIR0_LINUX_LLVM_OPT:FILEPATH=${configuredCommands.llvmOpt}`,
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
  const fixtureBinding = join(fixtureDirectory, "binding.w")
  const fixtureLiteral = join(fixtureDirectory, "literal.w")
  const fixtureBuiltinDisplay = join(fixtureDirectory,
    "builtin_display.w")
  const fixtureTypedBindings = join(fixtureDirectory,
    "typed_bindings.w")
  const fixtureDirectCall = join(fixtureDirectory,
    "direct_call.w")
  const fixtureScalarReturn = join(fixtureDirectory,
    "scalar_return.w")
  const invalidTruncatingBits = join(fixtureDirectory,
    "invalid_truncating_bits.w")
  const invalidSaturatingLabel = join(fixtureDirectory,
    "invalid_saturating_label.w")
  const invalidNumericWidening = join(fixtureDirectory,
    "invalid_numeric_widening.w")
  const runtimeDivisionZero = join(fixtureDirectory,
    "runtime_division_zero.w")
  const runtimeDivisionOverflow = join(fixtureDirectory,
    "runtime_division_overflow.w")
  const runtimeRemainderZero = join(fixtureDirectory,
    "runtime_remainder_zero.w")
  const runtimeUnsignedDivisionZero = join(fixtureDirectory,
    "runtime_unsigned_division_zero.w")
  const runtimeUnsignedRemainderZero = join(fixtureDirectory,
    "runtime_unsigned_remainder_zero.w")
  const runtimeMinimumRemainder = join(fixtureDirectory,
    "runtime_minimum_remainder.w")
  const runtimeCheckedI8Overflow = join(fixtureDirectory,
    "runtime_checked_i8_overflow.w")
  const runtimeCheckedU16Underflow = join(fixtureDirectory,
    "runtime_checked_u16_underflow.w")
  const runtimeCheckedI16MultiplyOverflow = join(fixtureDirectory,
    "runtime_checked_i16_multiply_overflow.w")
  const runtimeShiftFaults = []
  const runtimeNegationMinimums = []
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
  const privateGraphDirectory = join(fixtureDirectory, "private-graph")
  const privateGraphRoot = join(privateGraphDirectory, "app.w")
  const privateGraphLibrary = join(privateGraphDirectory, "lib.w")
  const invalidComparisons = [
    ["Bool operands", "true == false"],
    ["String operands", '"a" != "b"'],
    ["mixed operands", "1 <= true"],
    ["comparison used as i64", "(1 < 2) + 3"],
  ]
  await writeFile(fixtureBinding,
    "fn serve() { let message = \"Table 42 remains open\" print(message) }\n" +
    "entry(serve)\n")
  await writeFile(fixtureLiteral,
    "fn serve() { print(\"Table 42 remains open\") }\nentry(serve)\n")
  await writeFile(fixtureBuiltinDisplay,
    "fn serve() { let state = \"open\" " +
    "print(\"Kitchen ${true}/${false}; table: ${state}\") }\nentry(serve)\n")
  await writeFile(fixtureTypedBindings,
    "fn serve() { let table = 6 * 7 let isOpen = true let state = \"open\" " +
    "print(\"Table ${table}; open: ${isOpen}; state: ${state}\") }\n" +
    "entry(serve)\n")
  await writeFile(fixtureDirectCall,
    "fn announce(table: i64, isOpen: Bool) {\n" +
    "  print(\"Table ${table}; open: ${isOpen}\")\n}\n" +
    "fn main() { announce(isOpen: true, table: 6 * 7) }\n" +
    "entry(main)\n")
  await writeFile(fixtureScalarReturn,
    "fn tableNumber(): i64 { return 6 * 7 }\n" +
    "fn main() { let table = tableNumber() " +
    "print(\"Table ${table}\") }\nentry(main)\n")
  await writeFile(invalidTruncatingBits,
    "fn main() { print(\"must not commit\") " +
    "let result = i8(exactly: 258_i16) }\nentry(main)\n")
  await writeFile(invalidSaturatingLabel,
    "entry { print(\"must not commit\") " +
    "let value = i8(saturating: 128_i16, other: 0_i16) }\n")
  await writeFile(invalidNumericWidening,
    "entry { print(\"must not commit\") let value: f32 = 1_i32 }\n")
  await writeFile(runtimeDivisionZero,
    "fn divide(value: i64, by divisor: i64): i64 { return value / divisor }\n" +
    "entry { print(\"must not commit\") " +
    "let result = divide(value: 8, by: 0) print(\"${result}\") }\n")
  await writeFile(runtimeDivisionOverflow,
    "fn divide(value: i64, by divisor: i64): i64 { return value / divisor }\n" +
    "entry { print(\"must not commit\") let result = divide(" +
    "value: 0 - 9223372036854775807 - 1, by: 0 - 1) " +
    "print(\"${result}\") }\n")
  await writeFile(runtimeRemainderZero,
    "fn remainder(value: i64, by divisor: i64): i64 { return value % divisor }\n" +
    "entry { print(\"must not commit\") " +
    "let result = remainder(value: 8, by: 0) print(\"${result}\") }\n")
  await writeFile(runtimeUnsignedDivisionZero,
    "fn divide(value: u64, by divisor: u64): u64 { return value / divisor }\n" +
    "entry { print(\"must not commit\") " +
    "let result = divide(value: 8_u64, by: 0_u64) print(\"${result}\") }\n")
  await writeFile(runtimeUnsignedRemainderZero,
    "fn remainder(value: u64, by divisor: u64): u64 { return value % divisor }\n" +
    "entry { print(\"must not commit\") " +
    "let result = remainder(value: 8_u64, by: 0_u64) print(\"${result}\") }\n")
  await writeFile(runtimeMinimumRemainder,
    "fn remainder(value: i64, by divisor: i64): i64 { return value % divisor }\n" +
    "entry { let result = remainder(" +
    "value: 0 - 9223372036854775807 - 1, by: 0 - 1) " +
    "print(\"${result}\") }\n")
  await writeFile(runtimeCheckedI8Overflow,
    "fn add(left: i8, right: i8): i8 { return left + right }\n" +
    "entry { print(\"must not commit\") " +
    "let result = add(left: 127_i8, right: 1_i8) print(\"${result}\") }\n")
  await writeFile(runtimeCheckedU16Underflow,
    "fn subtract(left: u16, right: u16): u16 { var result = left " +
    "result -= right return result }\n" +
    "entry { print(\"must not commit\") " +
    "let result = subtract(left: 0_u16, right: 1_u16) print(\"${result}\") }\n")
  await writeFile(runtimeCheckedI16MultiplyOverflow,
    "fn multiply(left: i16, right: i16): i16 { return left * right }\n" +
    "entry { print(\"must not commit\") " +
    "let result = multiply(left: 200_i16, right: 200_i16) " +
    "print(\"${result}\") }\n")
  for (const [name, type, value, count, operator, label] of [
    ["runtime_shift_count_i8", "i8", "1_i8", "8_u64", ">>", "signed i8 shift count at logical width"],
    ["runtime_shift_count_u8", "u8", "1_u8", "8_u64", ">>", "unsigned u8 shift count at logical width"],
    ["runtime_signed_shift_loss_i8", "i8", "64_i8", "1_u64", "<<", "signed i8 shift loses a sign bit"],
    ["runtime_negative_shift_loss_i8", "i8", "-64_i8", "2_u64", "<<", "negative signed i8 shift loses high bits"],
    ["runtime_unsigned_shift_loss_u8", "u8", "128_u8", "1_u64", "<<", "unsigned u8 shift loses a high bit"],
  ]) {
    const path = join(fixtureDirectory, `${name}.w`)
    await writeFile(path,
      "// Expected exit: nonzero (trap)\n// Expected stdout: <empty>\n" +
      `fn shift(value: ${type}, count: UInt): ${type} { return value ${operator} count }\n` +
      "entry { print(\"must not commit\") " +
      `let result = shift(value: ${value}, count: ${count}) ` +
      "print(\"${result}\") }\n")
    runtimeShiftFaults.push([path, label])
  }
  for (const [name, type, value, count] of [
    ["runtime_logical_shift_count_i8", "i8", "1_i8", "8_u64"],
    ["runtime_logical_shift_count_u64", "u64", "1_u64", "64_u64"],
  ]) {
    const path = join(fixtureDirectory, `${name}.w`)
    await writeFile(path,
      "// Expected exit: nonzero (trap)\n// Expected stdout: <empty>\n" +
      `fn shift(value: ${type}, count: UInt): ${type} { ` +
      `return ${type}.logicalShiftRight(value, count) }\n` +
      "entry { print(\"must not commit\") " +
      `let result = shift(value: ${value}, count: ${count}) ` +
      "print(\"${result}\") }\n")
    runtimeShiftFaults.push([path,
      `${type}.logicalShiftRight rejects a count at logical width`])
  }
  for (const [type, maximum, suffix] of [
    ["i8", "127", "_i8"],
    ["i16", "32767", "_i16"],
    ["i32", "2147483647", "_i32"],
    ["i64", "9223372036854775807", "_i64"],
    ["Int", "9223372036854775807", "_i64"],
  ]) {
    const path = join(fixtureDirectory,
      `runtime_${type.toLowerCase()}_negation_minimum.w`)
    await writeFile(path,
      "fn negate(value: " + type + "): " + type + " { return -value }\n" +
      "entry { print(\"must not commit\") " +
      "let result = negate(value: ~" + maximum + suffix + ") " +
      "print(\"${result}\") }\n")
    runtimeNegationMinimums.push([path, `${type} unary negation of logical minimum`])
  }
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
  await mkdir(privateGraphDirectory)
  await writeFile(privateGraphRoot,
    "import { helper as h } from lib\n" +
    "fn run() { let value = h() print(\"answer ${value}\") }\n" +
    "entry(run)\n")
  await writeFile(privateGraphLibrary,
    "module lib\nfn helper(): i64 { return 42 }\n")
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
  expectSuccess(binary, ["run", toWsl(localGraphFixture)],
    Buffer.from("answer 42\n", "utf8"), "resolved local-module graph")
  expectSourceFailure(binary, toWsl(privateGraphRoot),
    "private cross-module symbol")
  expectSuccess(binary, ["run", toWsl(fixtureBinding)],
    Buffer.from("Table 42 remains open\n"), "Restaurant binding")
  expectSuccess(binary, ["run", toWsl(fixtureLiteral)],
    Buffer.from("Table 42 remains open\n"), "Restaurant literal")
  expectSuccess(binary, ["run", toWsl(linearFixture)],
    Buffer.from("Table 42 remains open\nKitchen is ready\n", "utf8"),
    "Restaurant linear sequence")
  expectSuccess(binary, ["run", toWsl(enumFixture)],
    Buffer.from("Courses 10/30/20\n", "utf8"),
    "Restaurant payloadless enum exhaustive switch")
  expectSuccess(binary, ["run", toWsl(enumSubsetFixture)],
    Buffer.from("Work 1/2\n", "utf8"),
    "Restaurant payloadless enum subset switch")
  expectSuccess(binary, ["run", toWsl(enumPayloadFixture)],
    Buffer.from("Bills 32/44/10/7\n", "utf8"),
    "Restaurant enum payload return and reordered captures")
  expectSuccess(binary, ["run", toWsl(enumBoolPayloadFixture)],
    Buffer.from(
      "States true/false/false/true; charges 17/31; licensed true\n", "utf8"),
    "Restaurant mixed Bool and i64 enum payloads")
  expectSuccess(binary, ["run", toWsl(interpolationFixture)],
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
  expectSuccess(binary, ["run", toWsl(ifFixture)],
    expectedRestaurantIf,
    "Restaurant if diamond")
  expectSuccess(binary, ["run", toWsl(terminalReturnsFixture)],
    Buffer.from("-1,0,1\n", "utf8"),
    "terminal scalar branch returns")
  expectSuccess(binary, ["run", toWsl(comparisonsFixture)],
    Buffer.from("Seat party\nSeat party\nWaitlist\n", "utf8"),
    "Restaurant signed-i64 admission comparison")
  expectSuccess(binary, ["run", toWsl(integerComparisonFixture)],
    Buffer.from(
      "i8 false/true/true/true/false/false\n" +
      "u8 false/true/false/false/true/true\n" +
      "i16 true/false/false/true/false/true\n" +
      "u16 false/true/true/true/false/false\n" +
      "i32 false/true/true/true/false/false\n" +
      "u32 false/true/false/false/true/true\n" +
      "i64 false/true/true/true/false/false\n" +
      "u64 false/true/false/false/true/true\n" +
      "Int false/true/true/true/false/false\n" +
      "UInt true/false/false/true/false/true\n" +
      "widen u8->i16 true\n", "utf8"),
    "Restaurant fixed-width integer comparison family")
  expectSuccess(binary, ["run", toWsl(comparisonCompositionFixture)],
    Buffer.from(
      "false/true/true/true/false/false\n" +
      "true/false/false/true/false/true\n" +
      "false/true/false/false/true/true\n" +
      "false/true/true/true/false/false\n" +
      "false/true/false/false/true/true\nAllowed true\nAllowed false\n", "utf8"),
    "Restaurant comparison operators, signed endpoints, and Bool composition")
  expectSuccess(binary, ["run", toWsl(boolShortCircuitFixture)],
    Buffer.from(
      "Override checked\nClosed allowed true\nCapacity checked\n" +
      "Open allowed true\n", "utf8"),
    "Restaurant Bool short-circuit")
  expectSuccess(binary, ["run", toWsl(nestedIfFixture)],
    expectedRestaurantNestedIf,
    "Restaurant nested if")
  expectSuccess(binary, ["run", toWsl(whileFixture)],
    Buffer.from("Served 3\n", "utf8"),
    "Restaurant natural while lowered through structured MLIR")
  expectSuccess(binary, ["run", toWsl(whileMultiFixture)],
    Buffer.from("Served 9\n", "utf8"),
    "Restaurant multi-carrier natural while lowered through structured MLIR")
  expectSuccess(binary, ["run", toWsl(whilePostFixture)],
    Buffer.from("Final 9\n", "utf8"),
    "Restaurant post-loop SSA continuation after structured natural while")
  expectSuccess(binary, ["run", toWsl(whileBreakContinueFixture)],
    Buffer.from("0,4,8\n", "utf8"),
    "verified loop CFG with break and continue")
  expectSuccess(binary, ["run", toWsl(nestedLabeledWhileFixture)],
    Buffer.from("0,1,3\n", "utf8"),
    "verified nested labeled loop CFG")
  expectSuccess(binary, ["run", toWsl(nestedLoopTerminalReturnsFixture)],
    Buffer.from("-1,1,3\n", "utf8"),
    "verified nested loop CFG with terminal return branches")
  expectSuccess(binary, ["run", toWsl(repeatFixture)],
    Buffer.from("Receipt digits 1/5\n", "utf8"),
    "Restaurant post-test repeat lowered through structured MLIR")
  expectSuccess(binary, ["run", toWsl(wmoFixture)],
    Buffer.from("Bill 42\n", "utf8"),
    "Restaurant whole-module product closure")
  expectSuccess(binary, ["run", toWsl(asyncJoinFixture)],
    Buffer.from("Prepared 42\n", "utf8"),
    "Restaurant virtual structured-task elision")
  expectSuccess(binary, ["run", toWsl(asyncYieldFixture)],
    Buffer.from("Prepared 88\n", "utf8"),
    "Restaurant virtual Task with statically discharged yields")
  expectSuccess(binary, ["run", toWsl(mainDispatchFixture)],
    Buffer.from("Dispatched 88\n", "utf8"),
    "Restaurant physical main-domain dispatch")
  expectSuccess(binary, ["run", toWsl(mainCardinalityFixture)],
    Buffer.from("Dispatched 92\n", "utf8"),
    "Restaurant bounded main-domain cardinality")
  expectSuccess(binary, ["run", toWsl(w1531MinimalFixture)],
    Buffer.from("then\n", "utf8"), "W-1531 minimal if/else")
  expectSuccess(binary, ["run", toWsl(w1531NoElseFixture)],
    Buffer.from("Kitchen open\nAfter service\n", "utf8"),
    "W-1531 no-else")
  const styleFixtures = [
    [w1531LearnerFixture, "W-1531 learner source-style candidate"],
    [w1531IdiomaticFixture, "W-1531 idiomatic source-style candidate"],
    [w1531FrontierFixture, "W-1531 frontier exploration source-style candidate"],
  ]
  for (const [fixture, label] of styleFixtures)
    expectSuccess(binary, ["run", toWsl(fixture)], expectedRestaurantIf, label)
  expectSuccess(binary, ["run", toWsl(fixtureBuiltinDisplay)],
    Buffer.from("Kitchen true/false; table: open\n", "utf8"),
    "Restaurant built-in Display interpolation")
  expectSuccess(binary, ["run", toWsl(fixtureTypedBindings)],
    Buffer.from("Table 42; open: true; state: open\n", "utf8"),
    "Restaurant typed immutable bindings")
  expectSuccess(binary, ["run", toWsl(fixtureDirectCall)],
    Buffer.from("Table 42; open: true\n", "utf8"),
    "Restaurant direct W call")
  expectSuccess(binary, ["run", toWsl(fixtureScalarReturn)],
    Buffer.from("Table 42\n", "utf8"),
    "Restaurant scalar return")
  expectSuccess(binary, ["run", toWsl(unaryNegateFixture)],
    Buffer.from("Balance -7\n", "utf8"),
    "Restaurant checked runtime unary negation")
  expectSuccess(binary, ["run", toWsl(unaryInterpolationFixture)],
    Buffer.from("Balance -7\n", "utf8"),
    "Restaurant direct unary interpolation")
  expectSuccess(binary, ["run", toWsl(unsignedFixture)],
    Buffer.from("Unsigned 18446744073709551615\n", "utf8"),
    "Restaurant full-width UInt parameter, return, and interpolation")
  expectSuccess(binary, ["run", toWsl(integerBitwiseFixture)],
    Buffer.from(
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
      "utf8"),
    "Restaurant fixed-width signed/unsigned bitwise family")
  expectSuccess(binary, ["run", toWsl(shiftsFixture)],
    Buffer.from(
      "i8 -16/-128\nu8 32/128\ni16 -4096/-32768\nu16 8192/32768\n" +
      "i32 -268435456/-2147483648\nu32 536870912/2147483648\n" +
      "i64 -1152921504606846976/-9223372036854775808\n" +
      "u64 2305843009213693952/9223372036854775808\n" +
      "Int -1152921504606846976/-9223372036854775808\n" +
      "UInt 2305843009213693952/9223372036854775808\n", "utf8"),
    "Restaurant checked signed and unsigned shifts across logical widths")
  expectSuccess(binary, ["run", toWsl(powerFixture)],
    Buffer.from("Power -27/1024/1/512\n", "utf8"),
    "Restaurant checked signed and unsigned power")
  expectSuccess(binary, ["run", toWsl(powerPrefixFixture)],
    Buffer.from("Power prefix -4/4/512/-9/-27\n", "utf8"),
    "Restaurant prefix and power precedence")
  expectSuccess(binary, ["run", toWsl(compoundFixture)],
    Buffer.from("Compound 11\n", "utf8"),
    "Restaurant checked compound assignment")
  expectSuccess(binary, ["run", toWsl(floatStrictFixture)],
    Buffer.from("Float strict ok\n", "utf8"),
    "Restaurant strict f32/f64 arithmetic and IEEE comparisons")
  expectSuccess(binary,
    ["run", toWsl(floatBitRepresentationFixture)],
    Buffer.from(
      "Float bits f32 2147483648/2139095040/2143363909 " +
      "f64 9223372036854775808/9218868437227405312/9221140253039434428\n",
      "utf8"),
    "Restaurant exact f32/f64 bit representation round trips")
  expectSuccess(binary, ["run", toWsl(numericWideningFixture)],
    Buffer.from("Numeric widen ok\n", "utf8"),
    "Restaurant exact implicit integer/float widening")
  expectSuccess(binary, ["run", toWsl(checkedIntegerArithmeticFixture)],
    Buffer.from(
      "i8 -9/-15/-36; divrem -4/0; compound -2\n" +
      "u8 43/37/120; divrem 13/1; compound 2\n" +
      "i16 -970/-1030/-30000; divrem -33/-10; compound -12\n" +
      "u16 1030/970/30000; divrem 33/10; compound 8\n" +
      "i32 -117000/-123000/-360000000; divrem -40/0; compound -2\n" +
      "u32 100300/99700/30000000; divrem 333/100; compound 98\n" +
      "i64 -600000/-1200000/-270000000000; divrem -3/0; compound -2\n" +
      "u64 6000000000/4000000000/5000000000000000000; divrem 5/0; compound 999999998\n" +
      "Int -4000000000/-6000000000/-5000000000000000000; divrem -5/0; compound -2\n" +
      "UInt 9000000000/3000000000/18000000000000000000; divrem 2/0; compound 2999999998\n", "utf8"),
    "Restaurant checked signed/unsigned integer arithmetic family")
  expectSuccess(binary, ["run", toWsl(integerPrefixFixture)],
    Buffer.from(
      "i8 -7/-43\ni16 -7/-43\ni32 -7/-43\ni64 -7/-43\n" +
      "Int -7/-43\nu8 170\nu16 65450\nu32 4294967210\n" +
      "u64 18446744073709551530\nUInt 18446744073709551530\n" +
      "literal -7\n", "utf8"),
    "Restaurant integer prefix width and signedness family")
  expectSuccess(binary, ["run", toWsl(integerWrappingFixture)],
    Buffer.from(
      "i8/u8 -128/0\ni16/u16 32767/2\ni32/u32 -2/4294967295\n" +
      "i64/u64 -9223372036854775808/0\n" +
      "Int/UInt -9223372036854775808/18446744073709551615\n", "utf8"),
    "Restaurant fixed-width integer wrapping policy")
  expectSuccess(binary, ["run", toWsl(integerWideningFixture)],
    Buffer.from("Widen -7/200/202/203\n", "utf8"),
    "Restaurant implicit integer widening policy")
  expectSuccess(binary, ["run", toWsl(integerTruncatingBitsFixture)],
    Buffer.from("Trunc 2/-7/-6/18446744073709551609/-1\n", "utf8"),
    "Restaurant explicit fixed-width truncatingBits family")
  expectSuccess(binary,
    ["run", toWsl(integerSaturatingConversionFixture)],
    fixtureIntegerSaturatingConversionOutput,
    "Restaurant four-quadrant saturating conversions and UInt-to-Int alias")
  expectSuccess(binary, ["run", toWsl(uIntWrappingAddFixture)],
    Buffer.from("Wrapped 0\n", "utf8"),
    "Restaurant UInt wrappingAdd at the unsigned maximum")
  expectSuccess(binary, ["run", toWsl(uIntWrappingSubtractFixture)],
    Buffer.from("Wrapped 18446744073709551615\n", "utf8"),
    "Restaurant UInt wrappingSubtract below zero")
  expectSuccess(binary, ["run", toWsl(uIntWrappingMultiplyFixture)],
    Buffer.from("Wrapped 18446744073709551614\n", "utf8"),
    "Restaurant UInt wrappingMultiply at the unsigned maximum")
  expectSuccess(binary, ["run", toWsl(uIntWrappingNegateFixture)],
    Buffer.from("Wrapped 18446744073709551615\n", "utf8"),
    "Restaurant UInt wrappingNegate of one")
  expectSuccess(binary, ["run", toWsl(uIntWrappingPowerFixture)],
    Buffer.from("Wrapped 12157665459056928801\n", "utf8"),
    "Restaurant UInt wrappingPower of three to forty")
  expectSuccess(binary,
    ["run", toWsl(uIntWrappingShiftLeftFixture)],
    Buffer.from("Wrapped 18446744073709551614\n", "utf8"),
    "Restaurant UInt wrappingShiftLeft with a valid count")
  expectSuccess(binary, ["run", toWsl(fixedIntegerShiftPoliciesFixture)],
    fixedIntegerShiftPoliciesOutput,
    "Fixed-width named shift policies and signed/unsigned edge cases")
  expectSuccess(binary,
    ["run", toWsl(uIntRotatedLeftFixture)],
    Buffer.from("Rotated 3\n", "utf8"),
    "Restaurant UInt rotatedLeft reduces count modulo bit width")
  expectSuccess(binary,
    ["run", toWsl(uIntRotatedRightFixture)],
    Buffer.from("Rotated 9223372036854775809\n", "utf8"),
    "Restaurant UInt rotatedRight reduces count modulo bit width")
  expectSuccess(binary, ["run", toWsl(uIntCountOnesFixture)],
    Buffer.from("Ones 32\n", "utf8"),
    "Restaurant UInt countOnes uses full-width population count")
  expectSuccess(binary, ["run", toWsl(uIntCountZerosFixture)],
    Buffer.from("Zeros 32\n", "utf8"),
    "Restaurant UInt countZeros derives the full-width complement count")
  expectSuccess(binary, ["run", toWsl(uIntLeadingZerosFixture)],
    Buffer.from("Leading 56/64\n", "utf8"),
    "Restaurant UInt countLeadingZeros preserves the zero boundary")
  expectSuccess(binary, ["run", toWsl(uIntTrailingZerosFixture)],
    Buffer.from("Trailing 12/64\n", "utf8"),
    "Restaurant UInt countTrailingZeros preserves the zero boundary")
  expectSuccess(binary, ["run", toWsl(uIntReversedBitsFixture)],
    Buffer.from("Bits 17848844570815808640\n", "utf8"),
    "Restaurant UInt reversedBits preserves the complete logical width")
  expectSuccess(binary, ["run", toWsl(uIntReversedBytesFixture)],
    Buffer.from("Bytes 17279655951921914625\n", "utf8"),
    "Restaurant UInt reversedBytes is independent of host endianness")
  expectSuccess(binary, ["run", toWsl(fixedIntegerBitPrimitivesFixture)],
    fixedIntegerBitPrimitivesOutput,
    "Fixed-width signed and unsigned bit-primitives family")
  expectSuccess(binary, ["run", toWsl(uIntOverflowingAddFixture)],
    Buffer.from("Overflowing 0/true/11/false\n", "utf8"),
    "Restaurant UInt overflowingAdd returns wrapped value and overflow flag")
  expectSuccess(binary, ["run", toWsl(uIntOverflowingPowerFixture)],
    Buffer.from(
      "Overflowing power 9223372036854775808/false; 0/true; 1/true; " +
      "1/false\n", "utf8"),
    "Restaurant UInt overflowingPower preserves sticky overflow")
  expectSuccess(binary, ["run", toWsl(uIntOverflowingFamilyFixture)],
    Buffer.from(
      "Overflowing family add 0/true,11/false; subtract 41/false," +
      "18446744073709551615/true; multiply 42/false," +
      "18446744073709551614/true; negate 0/false," +
      "18446744073709551615/true; power 9223372036854775808/false," +
      "0/true,1/true,1/false\n",
      "utf8"),
    "Restaurant UInt overflowing family preserves all operation flags")
  expectSuccess(binary, ["run", toWsl(uIntSaturatingAddFixture)],
    Buffer.from("Saturated 18446744073709551615/11\n", "utf8"),
    "Restaurant UInt saturatingAdd clamps overflow without trapping")
  expectSuccess(binary, ["run", toWsl(uIntSaturatingSubtractFixture)],
    Buffer.from("Saturated subtract 0/10\n", "utf8"),
    "Restaurant UInt saturatingSubtract clamps underflow without trapping")
  expectSuccess(binary, ["run", toWsl(uIntSaturatingMultiplyFixture)],
    Buffer.from("Saturated multiply 18446744073709551615/42\n", "utf8"),
    "Restaurant UInt saturatingMultiply clamps overflow without trapping")
  expectSuccess(binary, ["run", toWsl(uIntSaturatingPolicyFixture)],
    Buffer.from(
      "Saturating policy add 18446744073709551615/11; subtract 0/10; " +
      "multiply 18446744073709551615/42; negate 0/0; power " +
      "8/18446744073709551615/1\n", "utf8"),
    "Restaurant UInt saturating policy covers the complete family")
  expectSuccess(binary, ["run", toWsl(uIntBitNotFixture)],
    Buffer.from("UInt not 18446744073709551615\n", "utf8"),
    "Restaurant UInt bitwise complement")
  expectSuccess(binary, ["run", toWsl(uIntBitwiseFixture)],
    Buffer.from(
      "Not 18446744073709551615\nAnd 0\nOr 18446744073709551615\n" +
      "Xor 18446744073709551615\nOnes 32\nZeros 32\nLeading 56\n" +
      "Leading zero 64\nTrailing 12\nTrailing zero 64\n", "utf8"),
    "Restaurant UInt bit-primitives family")
  expectSuccess(binary, ["run", toWsl(uIntCompoundFixture)],
    Buffer.from(
      "UInt compound 4611686018427387907/4611686018427387906/" +
      "9223372036854775812/4611686018427387906/4611686018427387906/" +
      "4611686018427387906/9223372036854775812/4611686018427387906/" +
      "2/87/95\n", "utf8"),
    "Restaurant UInt compound assignment")
  expectSuccess(binary, ["run", toWsl(mutationFixture)],
    Buffer.from("Open 6\n", "utf8"),
    "Restaurant straight-line local mutation")
  expectSuccess(binary, ["run", toWsl(conditionalMutationFixture)],
    Buffer.from("Open 6; closed 4\n", "utf8"),
    "Restaurant conditional mutation merged through SSA")
  expectSuccess(binary, ["run", toWsl(boolMutationFixture)],
    Buffer.from("Open true; closed false\n", "utf8"),
    "Restaurant Boolean local mutation")
  expectSuccess(binary, ["run", toWsl(branchMutationFixture)],
    Buffer.from("Open 6; closed 4\n", "utf8"),
    "Restaurant branch-local mutation merge")
  expectSuccess(binary, ["run", toWsl(multiBranchMutationFixture)],
    Buffer.from("Open 18; closed -4\n", "utf8"),
    "Restaurant multi-branch mutation merge")
  expectSuccess(binary, ["run", toWsl(runtimeMinimumRemainder)],
    Buffer.from("0\n", "utf8"),
    "runtime signed minimum remainder by negative one")
  for (const [path, label] of [
    [runtimeDivisionZero, "runtime signed division by zero"],
    [runtimeDivisionOverflow, "runtime signed division overflow"],
    [runtimeRemainderZero, "runtime signed remainder by zero"],
    [runtimeUnsignedDivisionZero, "runtime unsigned division by zero"],
    [runtimeUnsignedRemainderZero, "runtime unsigned remainder by zero"],
  ]) {
    const fault = invoke(binary, ["run", toWsl(path)])
    assert(fault.exitCode !== 0 && fault.stdout.length === 0 &&
      fault.stderr.length === 0,
    `${label} did not fail silently before output commit: ${resultSummary(fault)}`)
  }
  expectSourceFailure(binary, toWsl(invalidTruncatingBits),
    "wrong truncatingBits label fails before output commit")
  expectSourceFailure(binary, toWsl(invalidSaturatingLabel),
    "wrong saturating label fails before output commit")
  expectSourceFailure(binary, toWsl(invalidNumericWidening),
    "inexact implicit i32-to-f32 conversion fails before output commit")
  for (const [path, label] of [
    [runtimeCheckedI8Overflow, "signed i8 checked addition overflow"],
    [runtimeCheckedU16Underflow, "unsigned u16 checked compound subtraction underflow"],
    [runtimeCheckedI16MultiplyOverflow, "signed i16 checked multiplication overflow"],
    ...runtimeShiftFaults,
  ]) {
    const fault = invoke(binary, ["run", toWsl(path)])
    assert(fault.exitCode !== 0 && fault.stdout.length === 0 &&
      fault.stderr.length === 0,
    `${label} did not trap before committing output: ${resultSummary(fault)}`)
  }
  for (const [path, label] of runtimeNegationMinimums) {
    const fault = invoke(binary, ["run", toWsl(path)])
    assert(fault.exitCode !== 0 && fault.stdout.length === 0 &&
      fault.stderr.length === 0,
    `${label} did not trap before committing output: ${resultSummary(fault)}`)
  }
  const panicFault = invoke(binary, ["run", toWsl(explicitPanicFixture)])
  assert(panicFault.exitCode !== 0 && panicFault.stdout.length === 0 &&
    panicFault.stderr.length === 0,
  `explicit panic did not terminate silently: ${resultSummary(panicFault)}`)
  expectSuccess(binary, ["run", toWsl(empty)], Buffer.from("\n"),
    "empty payload")
  expectSuccess(binary, ["run", toWsl(twoCalls)], Buffer.from("a\nb\n"),
    "two-call sequence")
  expectSuccess(binary, ["run", toWsl(helloFixture), "--", "arbitrary", "--entry", ""],
    expectedHello, "forwarded program arguments")
  expectExact(binary, ["run", toWsl(processInputFixture)], 2,
    Buffer.from("missing\n", "utf8"),
    "Linux public process input without arguments")
  expectExact(binary, ["run", toWsl(processInputFixture), "--", ""], 0,
    Buffer.from("received\n", "utf8"),
    "Linux public process input with empty argument")
  expectExact(binary, ["run", toWsl(processInputFixture), "--", "payload"], 0,
    Buffer.from("received\n", "utf8"),
    "Linux public process input with one argument")
  expectExact(binary, ["run", toWsl(processIntegerExactSuccessFixture)], 0,
    Buffer.alloc(0), "Linux public exact integer conversion success")
  expectExact(binary, ["run", toWsl(processIntegerExactErrorFixture)], 1,
    Buffer.alloc(0), "Linux public exact integer conversion typed error")
  expectExact(binary, ["run", toWsl(processFloatRoundingSuccessFixture)], 0,
    Buffer.from("Rounded 2\n", "utf8"),
    "Linux public constant float rounding success")
  expectExact(binary, ["run", toWsl(processFloatRoundingRuntimeIfFixture)], 0,
    Buffer.from("Rounded 2\n", "utf8"),
    "Linux public runtime conditional float rounding without arguments")
  expectExact(binary, ["run", toWsl(processFloatRoundingRuntimeIfFixture), "--",
    "x"], 0, Buffer.from("Rounded 4\n", "utf8"),
    "Linux public runtime conditional float rounding with one argument")
  expectExact(binary, ["run", toWsl(processFloatRoundingErrorFixture)], 1,
    Buffer.alloc(0), "Linux public constant float rounding typed error")
  expectExact(binary, ["run", toWsl(processIntegerExactRuntimeFixture)], 0,
    Buffer.from("Arithmetic 0/4/1\n", "utf8"),
    "Linux public runtime fixed-integer arithmetic success")
  expectExact(binary, ["run", toWsl(processIntegerExactRuntimeFixture), "--",
    ...Array.from({ length: 126 }, () => "x")], 0,
    Buffer.from("Arithmetic 126/9/127\n", "utf8"),
    "Linux public runtime fixed-integer arithmetic success boundary")
  expectExact(binary, ["run", toWsl(processIntegerExactRuntimeFixture), "--",
    ...Array.from({ length: 127 }, () => "x")], 2, Buffer.alloc(0),
    "Linux public runtime fixed-integer arithmetic structured fault")
  expectExact(binary, ["run", toWsl(processIntegerExactRuntimeFixture), "--",
    ...Array.from({ length: 128 }, () => "x")], 1, Buffer.alloc(0),
  "Linux public runtime exact integer conversion out of range")
  expectExact(binary, ["run", toWsl(processEnumPayloadFixture)], 7,
    Buffer.from("arguments-missing count=0 amount=17 over-limit=false\n", "utf8"),
    "Linux public enum payload process input without arguments")
  expectExact(binary, ["run", toWsl(processEnumPayloadFixture), "--", ""], 0,
    Buffer.from("arguments-present count=1 amount=17 over-limit=false\n", "utf8"),
    "Linux public enum payload process input with empty argument")
  expectExact(binary, ["run", toWsl(processEnumPayloadFixture), "--",
    "alpha", "beta"], 0,
    Buffer.from("arguments-present count=2 amount=17 over-limit=false\n", "utf8"),
    "Linux public enum payload process input with two arguments")
  expectExact(binary, ["run", toWsl(processEnumPayloadFixture), "--",
    "alpha", "beta", "gamma"], 0,
    Buffer.from("arguments-present count=3 amount=17 over-limit=true\n", "utf8"),
    "Linux public enum payload process input with three arguments")
  expectExact(binary, ["run", toWsl(processArgumentsOrderingFixture)], 0,
    Buffer.from("Argument mode compact: count=0\n", "utf8"),
    "Linux ordered process count input without arguments")
  expectExact(binary, ["run", toWsl(processArgumentsOrderingFixture), "--", ""], 0,
    Buffer.from("Argument mode compact: count=1\n", "utf8"),
    "Linux ordered process count input with empty argument")
  expectExact(binary,
    ["run", toWsl(processArgumentsOrderingFixture), "--", "alpha", "beta"], 0,
    Buffer.from("Argument mode extended: count=2\n", "utf8"),
    "Linux ordered process count input with two arguments")
  expectExact(binary, ["run", toWsl(processArgumentsOrderingFixture), "--",
    "alpha", "beta", "gamma"], 0,
    Buffer.from("Argument mode extended: count=3\n", "utf8"),
    "Linux ordered process count input with three arguments")

  const buildOutput = (name) => isWindows
    ? `${buildArtifactDirectory}/${name}`
    : join(buildArtifactDirectory, name)
  const buildHello = buildOutput("hello-build")
  const buildHelloAudit = buildOutput("hello-audit-build")
  const helloAuditTrace = buildOutput("hello-audit-trace")
  const failedAuditTrace = buildOutput("failed-build-audit-trace")
  const failedAuditProduct = buildOutput("failed-build-audit-product")
  const existingAuditTrace = buildOutput("existing-audit-trace")
  const existingAuditMarker = auditChild(existingAuditTrace, "keep")
  const symlinkAuditTrace = buildOutput("symlink-audit-trace")
  const symlinkAuditTarget = buildOutput("symlink-audit-target")
  const symlinkAuditParent = buildOutput("symlink-audit-parent")
  const symlinkAuditChild = auditChild(symlinkAuditParent, "trace")
  const escapeAuditTrace = isWindows
    ? `${buildArtifactDirectory}/../w-run-audit-escape`
    : `${buildArtifactDirectory}/../w-run-audit-escape`
  const buildHelloNoPie = buildOutput("hello-build-no-pie")
  const buildWindowsTargetPie = buildOutput("pie-on-windows-target-build")
  const buildWindowsTargetNoPie = buildOutput("pie-off-windows-target-build")
  const buildLocalGraph = buildOutput("local-graph-build")
  const buildPrivateGraph = buildOutput("private-graph-build")
  const buildRestaurantIf = buildOutput("if-build")
  const buildRestaurantRepeat = buildOutput("repeat-build")
  const buildWhileBreakContinue = buildOutput("while-break-continue-build")
  const buildNestedLabeledWhile = buildOutput("nested-labeled-while-build")
  const buildNestedLoopTerminalReturns = buildOutput(
    "nested-loop-terminal-returns-build")
  const buildRestaurantMainDispatch = buildOutput(
    "main-dispatch-build")
  const buildRestaurantMainCardinality = buildOutput(
    "main-cardinality-build")
  const buildProcessInput = buildOutput("process-input-build")
  const buildProcessIntegerExactSuccess = buildOutput(
    "process-integer-exact-success-build")
  const buildProcessIntegerExactError = buildOutput(
    "process-integer-exact-error-build")
  const buildProcessFloatRoundingSuccess = buildOutput(
    "process-float-rounding-success-build")
  const buildProcessFloatRoundingRuntimeIf = buildOutput(
    "process-float-rounding-runtime-if-build")
  const buildProcessFloatRoundingError = buildOutput(
    "process-float-rounding-error-build")
  const buildProcessIntegerExactRuntime = buildOutput(
    "process-fixed-integer-arithmetic-build")
  const buildProcessArgumentsCount = buildOutput("process-arguments-count-build")
  const buildProcessArgumentsOrdering = buildOutput(
    "process-arguments-ordering-build")
  const buildProcessEnumPayload = buildOutput("process-enum-payload-build")
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
  expectSuccess(binary, ["build", toWsl(helloFixture), "--target", targetTriple,
    "--output", buildHelloAudit, "--audit-dir", helloAuditTrace],
  Buffer.alloc(0), "build one Hello product with the audit-only trace")
  expectSuccess(buildHelloAudit, [], expectedHello,
    "execute audited Hello product")
  assertCrtFreeElf(await readBuildArtifact(buildHelloAudit))
  await verifyAuditTrace(helloAuditTrace, buildHelloAudit,
    buildArtifactDirectory)

  if (isWindows) {
    runRequired("WSL existing audit target directory", "wsl.exe", [
      "-d", "Ubuntu", "--", "mkdir", "--", existingAuditTrace,
    ])
    runRequired("WSL existing audit target marker", "wsl.exe", [
      "-d", "Ubuntu", "--", "touch", "--", existingAuditMarker,
    ])
    runRequired("WSL audit target symlink", "wsl.exe", [
      "-d", "Ubuntu", "--", "ln", "-s", symlinkAuditTarget,
      symlinkAuditTrace,
    ])
    runRequired("WSL audit parent symlink", "wsl.exe", [
      "-d", "Ubuntu", "--", "ln", "-s", buildArtifactDirectory,
      symlinkAuditParent,
    ])
  } else {
    await mkdir(existingAuditTrace)
    await writeFile(existingAuditMarker, "preserve existing audit data\n")
    await symlink(symlinkAuditTarget, symlinkAuditTrace)
    await symlink(buildArtifactDirectory, symlinkAuditParent)
  }
  try {
    expectBuildFailure(binary, ["build", toWsl(join(fixtureDirectory,
      "missing.w")), "--target", targetTriple, "--output", failedAuditProduct,
    "--audit-dir", existingAuditTrace],
    "reject existing audit directory before source/product work")
    assert(!artifactExists(failedAuditProduct) &&
      artifactExists(existingAuditMarker),
    "existing audit target was replaced or output was published")
    expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
      targetTriple, "--output", failedAuditProduct, "--audit-dir",
      symlinkAuditTrace], "reject symlink audit directory")
    assert(!artifactExists(failedAuditProduct),
      "symlink audit rejection left an executable")
    expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
      targetTriple, "--output", failedAuditProduct, "--audit-dir",
      symlinkAuditChild], "reject symlink audit parent")
    expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
      targetTriple, "--output", failedAuditProduct, "--audit-dir",
      escapeAuditTrace], "reject escaping audit path")
    assert(!artifactExists(failedAuditProduct) &&
      !artifactExists(escapeAuditTrace),
    "invalid audit target left a product or escaped trace")
    expectBuildFailure(binary, ["build", toWsl(privateGraphRoot), "--target",
      targetTriple, "--output", failedAuditProduct, "--audit-dir",
      failedAuditTrace], "remove failed compile audit staging")
    assert(!artifactExists(failedAuditProduct) &&
      !artifactExists(failedAuditTrace),
    "failed compilation published a partial audit trace")
    await assertNoBuildResidue(buildArtifactDirectory,
      "audit target and compilation failures")
  } finally {
    if (isWindows) {
      runRequired("WSL audit symlink cleanup", "wsl.exe", [
        "-d", "Ubuntu", "--", "rm", "-f", "--", symlinkAuditTrace,
        symlinkAuditParent,
      ])
    } else {
      await unlink(symlinkAuditTrace)
      await unlink(symlinkAuditParent)
    }
  }
  expectSuccess(binary, ["build", toWsl(helloFixture), "--target",
    targetTriple, "--output", buildHelloNoPie, "--pie", "off"],
  Buffer.alloc(0), "build Hello as a non-PIE Linux executable")
  expectSuccess(buildHelloNoPie, [], expectedHello,
    "execute built non-PIE Hello artifact")
  const noPieHelloBytes = await readBuildArtifact(buildHelloNoPie)
  assertCrtFreeExecElf(noPieHelloBytes)
  assertElfNoExecutableStack(noPieHelloBytes)
  for (const [mode, output] of [["on", buildWindowsTargetPie],
    ["off", buildWindowsTargetNoPie]]) {
    expectUnsupportedOption(binary, ["build", toWsl(helloFixture), "--target",
      "x86_64-pc-windows-msvc", "--output", output, "--pie", mode],
    `reject explicit PIE mode ${mode} on a Windows target`)
    assert(!artifactExists(output),
      `Windows-target PIE mode ${mode} left an artifact`)
  }
  expectSuccess(binary, ["build", toWsl(localGraphFixture), "--target",
    targetTriple, "--output", buildLocalGraph], Buffer.alloc(0),
    "build resolved local-module graph")
  expectSuccess(buildLocalGraph, [], Buffer.from("answer 42\n", "utf8"),
    "execute built local-module graph artifact")
  expectBuildFailure(binary, ["build", toWsl(privateGraphRoot), "--target",
    targetTriple, "--output", buildPrivateGraph],
  "reject private cross-module build")
  assert(!artifactExists(buildPrivateGraph),
    "private cross-module build left an artifact")
  const helloBytes = await readBuildArtifact(buildHello)
  assertCrtFreeElf(helloBytes)
  assertElfNoExecutableStack(helloBytes)
  expectBuildFailure(binary, ["build", toWsl(helloFixture), "--target",
    targetTriple, "--output", buildHello],
  "reject existing build output")
  assert((await readBuildArtifact(buildHello)).equals(helloBytes),
    "existing build output was modified")
  expectSuccess(binary, ["build", toWsl(ifFixture), "--target",
    targetTriple, "--output", buildRestaurantIf], Buffer.alloc(0),
    "build if fixture")
  expectSuccess(buildRestaurantIf, [], expectedRestaurantIf,
    "execute built if artifact")
  expectSuccess(binary, ["build", toWsl(repeatFixture), "--target",
    targetTriple, "--output", buildRestaurantRepeat], Buffer.alloc(0),
    "build repeat fixture")
  expectSuccess(buildRestaurantRepeat, [],
    Buffer.from("Receipt digits 1/5\n", "utf8"),
    "execute built repeat artifact")
  assertCrtFreeElf(await readBuildArtifact(buildRestaurantRepeat))
  expectSuccess(binary, ["build", toWsl(whileBreakContinueFixture),
    "--target", targetTriple, "--output", buildWhileBreakContinue],
    Buffer.alloc(0), "build verified loop CFG fixture")
  expectSuccess(buildWhileBreakContinue, [],
    Buffer.from("0,4,8\n", "utf8"),
    "execute built verified loop CFG artifact")
  assertCrtFreeElf(await readBuildArtifact(buildWhileBreakContinue))
  expectSuccess(binary, ["build", toWsl(nestedLabeledWhileFixture),
    "--target", targetTriple, "--output", buildNestedLabeledWhile],
  Buffer.alloc(0), "build verified nested labeled loop CFG fixture")
  expectSuccess(buildNestedLabeledWhile, [],
    Buffer.from("0,1,3\n", "utf8"),
    "execute built verified nested labeled loop CFG artifact")
  assertCrtFreeElf(await readBuildArtifact(buildNestedLabeledWhile))
  expectSuccess(binary, ["build", toWsl(nestedLoopTerminalReturnsFixture),
    "--target", targetTriple, "--output", buildNestedLoopTerminalReturns],
    Buffer.alloc(0), "build verified nested loop terminal-return CFG fixture")
  expectSuccess(buildNestedLoopTerminalReturns, [],
    Buffer.from("-1,1,3\n", "utf8"),
    "execute built verified nested loop terminal-return CFG artifact")
  assertCrtFreeElf(await readBuildArtifact(buildNestedLoopTerminalReturns))
  expectSuccess(binary, ["build", toWsl(mainDispatchFixture),
    "--target", targetTriple, "--output", buildRestaurantMainDispatch],
  Buffer.alloc(0), "build restaurant main-domain dispatch fixture")
  expectSuccess(buildRestaurantMainDispatch, [],
    Buffer.from("Dispatched 88\n", "utf8"),
    "execute built restaurant main-domain dispatch artifact")
  assertCrtFreeElf(await readBuildArtifact(buildRestaurantMainDispatch))
  expectSuccess(binary, ["build", toWsl(mainCardinalityFixture),
    "--target", targetTriple, "--output", buildRestaurantMainCardinality],
  Buffer.alloc(0), "build restaurant main-domain cardinality fixture")
  expectSuccess(buildRestaurantMainCardinality, [],
    Buffer.from("Dispatched 92\n", "utf8"),
    "execute built restaurant main-domain cardinality artifact")
  assertCrtFreeElf(await readBuildArtifact(buildRestaurantMainCardinality))
  expectSuccess(binary, ["build", toWsl(processInputFixture), "--target",
    targetTriple, "--output", buildProcessInput], Buffer.alloc(0),
  "build Linux public process-input fixture")
  const processInputBytes = await readBuildArtifact(buildProcessInput)
  assertCrtFreeElf(processInputBytes)
  expectExact(buildProcessInput, [], 2, Buffer.from("missing\n", "utf8"),
    "execute built Linux process-input artifact without arguments")
  expectExact(buildProcessInput, [""], 0, Buffer.from("received\n", "utf8"),
    "execute built Linux process-input artifact with empty argument")
  expectExact(buildProcessInput, ["payload"], 0,
    Buffer.from("received\n", "utf8"),
    "execute built Linux process-input artifact with one argument")
  expectSuccess(binary, ["build", toWsl(processIntegerExactSuccessFixture),
    "--target", targetTriple, "--output", buildProcessIntegerExactSuccess],
  Buffer.alloc(0), "build Linux exact integer conversion success fixture")
  assertCrtFreeElf(await readBuildArtifact(buildProcessIntegerExactSuccess))
  expectExact(buildProcessIntegerExactSuccess, [], 0, Buffer.alloc(0),
    "execute built Linux exact integer conversion success artifact")
  expectSuccess(binary, ["build", toWsl(processIntegerExactErrorFixture),
    "--target", targetTriple, "--output", buildProcessIntegerExactError],
  Buffer.alloc(0), "build Linux exact integer conversion typed-error fixture")
  assertCrtFreeElf(await readBuildArtifact(buildProcessIntegerExactError))
  expectExact(buildProcessIntegerExactError, [], 1, Buffer.alloc(0),
    "execute built Linux exact integer conversion typed-error artifact")
  expectSuccess(binary, ["build", toWsl(processFloatRoundingSuccessFixture),
    "--target", targetTriple, "--output", buildProcessFloatRoundingSuccess],
  Buffer.alloc(0), "build Linux constant float rounding success fixture")
  assertCrtFreeElf(await readBuildArtifact(buildProcessFloatRoundingSuccess))
  expectExact(buildProcessFloatRoundingSuccess, [], 0,
    Buffer.from("Rounded 2\n", "utf8"),
    "execute built Linux constant float rounding success artifact")
  expectSuccess(binary, ["build", toWsl(processFloatRoundingRuntimeIfFixture),
    "--target", targetTriple, "--output", buildProcessFloatRoundingRuntimeIf],
  Buffer.alloc(0), "build Linux runtime conditional float rounding fixture")
  assertCrtFreeElf(await readBuildArtifact(buildProcessFloatRoundingRuntimeIf))
  expectExact(buildProcessFloatRoundingRuntimeIf, [], 0,
    Buffer.from("Rounded 2\n", "utf8"),
    "execute built Linux runtime conditional float rounding without arguments")
  expectExact(buildProcessFloatRoundingRuntimeIf, ["x"], 0,
    Buffer.from("Rounded 4\n", "utf8"),
    "execute built Linux runtime conditional float rounding with one argument")
  expectSuccess(binary, ["build", toWsl(processFloatRoundingErrorFixture),
    "--target", targetTriple, "--output", buildProcessFloatRoundingError],
  Buffer.alloc(0), "build Linux constant float rounding typed-error fixture")
  assertCrtFreeElf(await readBuildArtifact(buildProcessFloatRoundingError))
  expectExact(buildProcessFloatRoundingError, [], 1, Buffer.alloc(0),
    "execute built Linux constant float rounding typed-error artifact")
  expectSuccess(binary, ["build", toWsl(processIntegerExactRuntimeFixture),
    "--target", targetTriple, "--output", buildProcessIntegerExactRuntime],
  Buffer.alloc(0), "build Linux runtime fixed-integer arithmetic fixture")
  assertCrtFreeElf(await readBuildArtifact(buildProcessIntegerExactRuntime))
  expectExact(buildProcessIntegerExactRuntime, [], 0,
    Buffer.from("Arithmetic 0/4/1\n", "utf8"),
    "execute built Linux runtime fixed-integer arithmetic success")
  expectExact(buildProcessIntegerExactRuntime,
    Array.from({ length: 126 }, () => "x"), 0,
    Buffer.from("Arithmetic 126/9/127\n", "utf8"),
    "execute built Linux runtime fixed-integer arithmetic success boundary")
  expectExact(buildProcessIntegerExactRuntime,
    Array.from({ length: 127 }, () => "x"), 2, Buffer.alloc(0),
    "execute built Linux runtime fixed-integer arithmetic structured fault")
  expectExact(buildProcessIntegerExactRuntime,
    Array.from({ length: 128 }, () => "x"), 1, Buffer.alloc(0),
  "execute built Linux runtime exact integer conversion out of range")
  expectSuccess(binary, ["build", toWsl(processArgumentsCountFixture), "--target",
    targetTriple, "--output", buildProcessArgumentsCount], Buffer.alloc(0),
  "build Linux public process-arguments-count fixture")
  const processArgumentsCountBytes = await readBuildArtifact(buildProcessArgumentsCount)
  assertCrtFreeElf(processArgumentsCountBytes)
  const argumentCountCases = [
    ["without user arguments", []],
    ["with one empty argument", [""]],
    ["with two ordinary arguments", ["alpha", "beta"]],
    ["with exactly 256 user arguments", Array.from({ length: 256 }, () => "x")],
  ]
  for (const [label, argumentsList] of argumentCountCases) {
    const expectedOutput = argumentsList.length === 2
      ? "Exactly two arguments\n"
      : `Argument count ${argumentsList.length}\n`
    expectExact(buildProcessArgumentsCount, argumentsList, 0,
      Buffer.from(expectedOutput, "utf8"),
      `execute Linux process-arguments-count artifact ${label}`)
  }
  expectExact(buildProcessArgumentsCount,
    Array.from({ length: 257 }, () => "x"), 3, Buffer.alloc(0),
    "reject Linux process descriptor overflow without partial output")
  expectSuccess(binary, ["build", toWsl(processArgumentsOrderingFixture), "--target",
    targetTriple, "--output", buildProcessArgumentsOrdering], Buffer.alloc(0),
  "build Linux ordered process-arguments fixture")
  const processArgumentsOrderingBytes = await readBuildArtifact(
    buildProcessArgumentsOrdering)
  assertCrtFreeElf(processArgumentsOrderingBytes)
  const orderedArgumentCountCases = [
    ["without user arguments", [], "Argument mode compact: count=0\n"],
    ["with one empty argument", [""], "Argument mode compact: count=1\n"],
    ["with two ordinary arguments", ["alpha", "beta"],
      "Argument mode extended: count=2\n"],
    ["with three ordinary arguments", ["alpha", "beta", "gamma"],
      "Argument mode extended: count=3\n"],
  ]
  for (const [label, argumentsList, expectedOutput] of orderedArgumentCountCases)
    expectExact(buildProcessArgumentsOrdering, argumentsList, 0,
      Buffer.from(expectedOutput, "utf8"),
      `execute Linux ordered process-arguments artifact ${label}`)
  expectSuccess(binary, ["build", toWsl(processEnumPayloadFixture), "--target",
    targetTriple, "--output", buildProcessEnumPayload], Buffer.alloc(0),
  "build Linux public enum payload process fixture")
  const processEnumPayloadBytes = await readBuildArtifact(buildProcessEnumPayload)
  assertCrtFreeElf(processEnumPayloadBytes)
  expectExact(buildProcessEnumPayload, [], 7,
    Buffer.from("arguments-missing count=0 amount=17 over-limit=false\n", "utf8"),
    "execute built Linux enum payload process artifact without arguments")
  expectExact(buildProcessEnumPayload, [""], 0,
    Buffer.from("arguments-present count=1 amount=17 over-limit=false\n", "utf8"),
    "execute built Linux enum payload process artifact with empty argument")
  expectExact(buildProcessEnumPayload, ["alpha", "beta"], 0,
    Buffer.from("arguments-present count=2 amount=17 over-limit=false\n", "utf8"),
    "execute built Linux enum payload process artifact with two arguments")
  expectExact(buildProcessEnumPayload, ["alpha", "beta", "gamma"], 0,
    Buffer.from("arguments-present count=3 amount=17 over-limit=true\n", "utf8"),
    "execute built Linux enum payload process artifact with three arguments")
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
  for (const role of ["mlirOpt", "mlirTranslate", "llvmOpt", "llc", "linkDriver"]) {
    try {
      await replaceToolLink(role, failureToolForHost)
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
