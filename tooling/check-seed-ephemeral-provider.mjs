import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const executableSuffix = process.platform === "win32" ? ".exe" : ""
const compileSources = [
  "w_seed_source.c",
  "w_seed_sha256.c",
  "w_seed_unicode.c",
  "w_seed_unicode_data.c",
  "w_seed_foreign.c",
  "w_seed_lexer.c",
  "w_seed_parser.c",
  "w_seed_module_scan.c",
  "w_seed_ephemeral_graph.c",
  "w_seed_ephemeral_provider.c",
  "w_seed_ephemeral_provider_linux.c",
]
const strictWarnings = [
  "-std=c11",
  "-Wall",
  "-Wextra",
  "-Wpedantic",
  "-Wconversion",
  "-Wsign-conversion",
  "-Wshadow",
  "-Werror",
]

function fail(message) {
  throw new Error(`seed ephemeral provider: ${message}`)
}

function spawn(command, args, cwd = root) {
  return Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  })
}

function runRequired(label, command, args, cwd = root) {
  const execution = spawn(command, args, cwd)
  if (execution.exitCode !== 0) {
    const detail = execution.stderr.toString().trim()
    fail(`${label} failed${detail ? `: ${detail}` : ""}`)
  }
  return execution
}

function validateCoreOutput(execution, label) {
  if (execution.stderr.length !== 0) fail(`${label} wrote to stderr`)
  const output = execution.stdout.toString().replaceAll("\r\n", "\n")
  if (output !== "RESULT provider-core=pass\n") {
    fail(`${label} returned an unexpected record: ${JSON.stringify(output)}`)
  }
  return "core=passed"
}

function validateAdapterOutput(execution, label, linux) {
  if (execution.stderr.length !== 0) fail(`${label} wrote to stderr`)
  const normalizedOutput = execution.stdout.toString().replaceAll("\r\n", "\n")
  const lines = normalizedOutput.trimEnd().split("\n")
  const expected = linux
    ? [
        "SKIP cross-mount=not-created-without-privilege",
        "RESULT provider-adapter=pass",
      ]
    : [
        "SKIP adapter-linux-real=non-linux-stub",
        "RESULT provider-adapter=pass",
      ]
  const openat2Unsupported =
    linux &&
    lines.length === expected.length + 1 &&
    lines[0] === "SKIP adapter-linux-openat2=unsupported"
  if (openat2Unsupported) {
    lines.shift()
  }
  if (lines.length !== expected.length || lines.some((line, index) => line !== expected[index])) {
    fail(`${label} returned unexpected records: ${JSON.stringify(execution.stdout.toString())}`)
  }
  if (!linux) return "windows-stub=passed"
  return openat2Unsupported
    ? "linux-openat2=unsupported"
    : "linux-real=passed"
}

function runDeterministic(label, executable, validator) {
  const first = runRequired(`${label} first`, executable, [])
  const second = runRequired(`${label} second`, executable, [])
  if (!Buffer.from(first.stdout).equals(Buffer.from(second.stdout))) {
    fail(`${label} stdout is not deterministic`)
  }
  if (!Buffer.from(first.stderr).equals(Buffer.from(second.stderr))) {
    fail(`${label} stderr is not deterministic`)
  }
  const firstMode = validator(first, `${label} first`)
  const secondMode = validator(second, `${label} second`)
  if (firstMode !== secondMode) fail(`${label} status is not deterministic`)
  return firstMode
}

function wslRepositoryPath() {
  if (process.platform !== "win32") return undefined
  const windowsRootArgument = root.replaceAll("\\", "/")
  const probe = spawn("wsl.exe", ["-d", "Ubuntu", "--", "wslpath", "-a", windowsRootArgument])
  if (probe.exitCode !== 0 || probe.stderr.length !== 0) return undefined
  const path = probe.stdout.toString().trim()
  if (!path.startsWith("/") || path.includes("\0") || path.includes("\n")) return undefined
  return path
}

async function runWslAdapter() {
  if (process.platform !== "win32") return undefined
  const wslRoot = wslRepositoryPath()
  if (wslRoot === undefined) {
    console.log("SKIP provider-linux-wsl=unavailable")
    return "wsl-unavailable"
  }
  const linuxExecutable = `/tmp/w-seed-ephemeral-provider-${process.pid}`
  const linuxSources = compileSources.map((source) => `${wslRoot}/compiler/seed-c/src/${source}`)
  try {
    const compile = runRequired(
      "WSL Linux adapter compile",
      "wsl.exe",
      [
        "-d",
        "Ubuntu",
        "--",
        "gcc",
        ...strictWarnings,
        "-I",
        `${wslRoot}/compiler/seed-c/include`,
        ...linuxSources,
        `${wslRoot}/compiler/seed-c/tests/test_ephemeral_provider_linux.c`,
        "-o",
        linuxExecutable,
      ],
    )
    if (compile.stderr.length !== 0) fail("WSL Linux adapter compile wrote to stderr")
    const first = runRequired(
      "WSL Linux adapter first",
      "wsl.exe",
      ["-d", "Ubuntu", "--", linuxExecutable],
    )
    const second = runRequired(
      "WSL Linux adapter second",
      "wsl.exe",
      ["-d", "Ubuntu", "--", linuxExecutable],
    )
    if (!Buffer.from(first.stdout).equals(Buffer.from(second.stdout))) {
      fail("WSL Linux adapter stdout is not deterministic")
    }
    if (!Buffer.from(first.stderr).equals(Buffer.from(second.stderr))) {
      fail("WSL Linux adapter stderr is not deterministic")
    }
    const firstMode = validateAdapterOutput(first, "WSL Linux adapter first", true)
    const secondMode = validateAdapterOutput(second, "WSL Linux adapter second", true)
    if (firstMode !== secondMode) fail("WSL Linux adapter status is not deterministic")
    return firstMode
  } finally {
    const cleanup = spawn("wsl.exe", ["-d", "Ubuntu", "--", "rm", "-f", linuxExecutable])
    if (cleanup.exitCode !== 0) {
      fail(`WSL Linux adapter cleanup failed: ${cleanup.stderr.toString().trim()}`)
    }
  }
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-ephemeral-provider-"))
try {
  runRequired(
    "CMake configure",
    "cmake",
    ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"],
  )
  runRequired(
    "CMake build",
    "cmake",
    [
      "--build",
      buildDirectory,
      "--target",
      "w_seed_ephemeral_provider_tests",
      "w_seed_ephemeral_provider_adapter_tests",
    ],
  )
  runRequired(
    "scoped CTest",
    "ctest",
    [
      "--test-dir",
      buildDirectory,
      "--output-on-failure",
      "-R",
      "w_seed_ephemeral_provider_(core|adapter)",
    ],
  )

  const coreExecutable = join(buildDirectory, `w_seed_ephemeral_provider_tests${executableSuffix}`)
  const adapterExecutable = join(
    buildDirectory,
    `w_seed_ephemeral_provider_adapter_tests${executableSuffix}`,
  )
  runDeterministic("native provider core", coreExecutable, validateCoreOutput)
  const nativeAdapterMode = runDeterministic(
    "native provider adapter",
    adapterExecutable,
    (execution, label) => validateAdapterOutput(execution, label, process.platform !== "win32"),
  )
  if (nativeAdapterMode === "windows-stub=passed") {
    console.log("SKIP adapter-linux-real=non-linux-stub")
    console.log("windows-stub=passed")
  } else if (nativeAdapterMode === "linux-openat2=unsupported") {
    console.log("SKIP linux-openat2=unsupported")
    console.log("SKIP cross-mount=not-created-without-privilege")
  } else {
    console.log("SKIP cross-mount=not-created-without-privilege")
    console.log("linux-real=passed")
  }
  const wslAdapterMode = await runWslAdapter()
  if (wslAdapterMode === "linux-openat2=unsupported") {
    console.log("SKIP linux-openat2=unsupported")
    console.log("SKIP cross-mount=not-created-without-privilege")
  } else if (wslAdapterMode === "linux-real=passed") {
    console.log("SKIP cross-mount=not-created-without-privilege")
    console.log("linux-real=passed")
  }
  console.log("seed ephemeral provider: CMake core/adapter tests and deterministic bounded records passed; optional Linux evidence is reported above")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
