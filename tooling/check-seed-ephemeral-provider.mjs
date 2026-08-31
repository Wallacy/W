import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { dialectArgs, dialectDisclosure, probeCDialect, requireCDialect } from "./c-dialect.mjs"

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
  "w_seed_ephemeral_provider_windows.c",
]
const strictWarningFlags = [
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

function normalizedStdout(execution, label) {
  if (execution.stderr.length !== 0) fail(`${label} wrote to stderr`)
  return execution.stdout.toString().replaceAll("\r\n", "\n")
}

function validateLinuxAdapterOutput(execution, label) {
  const output = normalizedStdout(execution, label)
  const openat2Unsupported =
    output ===
    "SKIP adapter-linux-openat2=unsupported\nSKIP cross-mount=not-created-without-privilege\nRESULT provider-adapter=pass\n"
  if (openat2Unsupported) return "linux-openat2=unsupported"
  if (
    output !==
    "SKIP cross-mount=not-created-without-privilege\nRESULT provider-adapter=pass\n"
  ) {
    fail(`${label} returned unexpected records: ${JSON.stringify(output)}`)
  }
  return "linux-real=passed"
}

function validateLinuxStubOutput(execution, label) {
  const output = normalizedStdout(execution, label)
  if (
    output !==
    "SKIP adapter-linux-real=non-linux-stub\nRESULT provider-adapter=pass\n"
  ) {
    fail(`${label} returned unexpected records: ${JSON.stringify(output)}`)
  }
  return "linux-stub=passed"
}

function validateWindowsAdapterOutput(execution, label, real) {
  const output = normalizedStdout(execution, label)
  if (real) {
    if (
      output !==
        "SKIP symlink=not-created-without-privilege\nRESULT provider-adapter-windows=pass\n" &&
      output !==
        "RESULT symlink=created\nRESULT provider-adapter-windows=pass\n"
    ) {
      fail(`${label} did not produce a real Windows result: ${JSON.stringify(output)}`)
    }
    return "windows-real=passed"
  }
  if (
    output !==
    "SKIP adapter-windows-real=non-windows-stub\nRESULT provider-adapter-windows=pass\n"
  ) {
    fail(`${label} returned unexpected stub records: ${JSON.stringify(output)}`)
  }
  return "windows-stub=passed"
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
    fail("WSL Ubuntu is required for the real Linux adapter proof on Windows")
  }
  const linuxExecutable = `/tmp/w-seed-ephemeral-provider-linux-${process.pid}`
  const windowsExecutable = `/tmp/w-seed-ephemeral-provider-windows-${process.pid}`
  const wslCompiler = ["wsl.exe", "-d", "Ubuntu", "--", "gcc"]
  const dialect = requireCDialect(
    await probeCDialect(wslCompiler),
    "WSL GCC",
  )
  const strictWarnings = [...dialectArgs(dialect), ...strictWarningFlags]
  const linuxSources = compileSources.map((source) => `${wslRoot}/compiler/seed-c/src/${source}`)
  try {
    const linuxCompile = runRequired(
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
    if (linuxCompile.stderr.length !== 0) {
      fail("WSL Linux adapter compile wrote to stderr")
    }
    const windowsCompile = runRequired(
      "WSL Windows adapter stub compile",
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
        `${wslRoot}/compiler/seed-c/tests/test_ephemeral_provider_windows.c`,
        "-o",
        windowsExecutable,
      ],
    )
    if (windowsCompile.stderr.length !== 0) {
      fail("WSL Windows adapter stub compile wrote to stderr")
    }
    const firstLinux = runRequired(
      "WSL Linux adapter first",
      "wsl.exe",
      ["-d", "Ubuntu", "--", linuxExecutable],
    )
    const secondLinux = runRequired(
      "WSL Linux adapter second",
      "wsl.exe",
      ["-d", "Ubuntu", "--", linuxExecutable],
    )
    if (!Buffer.from(firstLinux.stdout).equals(Buffer.from(secondLinux.stdout))) {
      fail("WSL Linux adapter stdout is not deterministic")
    }
    if (!Buffer.from(firstLinux.stderr).equals(Buffer.from(secondLinux.stderr))) {
      fail("WSL Linux adapter stderr is not deterministic")
    }
    const firstLinuxMode = validateLinuxAdapterOutput(
      firstLinux,
      "WSL Linux adapter first",
    )
    const secondLinuxMode = validateLinuxAdapterOutput(
      secondLinux,
      "WSL Linux adapter second",
    )
    if (firstLinuxMode !== secondLinuxMode) {
      fail("WSL Linux adapter status is not deterministic")
    }

    const firstWindows = runRequired(
      "WSL Windows adapter stub first",
      "wsl.exe",
      ["-d", "Ubuntu", "--", windowsExecutable],
    )
    const secondWindows = runRequired(
      "WSL Windows adapter stub second",
      "wsl.exe",
      ["-d", "Ubuntu", "--", windowsExecutable],
    )
    if (!Buffer.from(firstWindows.stdout).equals(Buffer.from(secondWindows.stdout))) {
      fail("WSL Windows adapter stub stdout is not deterministic")
    }
    if (!Buffer.from(firstWindows.stderr).equals(Buffer.from(secondWindows.stderr))) {
      fail("WSL Windows adapter stub stderr is not deterministic")
    }
    const firstWindowsMode = validateWindowsAdapterOutput(
      firstWindows,
      "WSL Windows adapter stub first",
      false,
    )
    const secondWindowsMode = validateWindowsAdapterOutput(
      secondWindows,
      "WSL Windows adapter stub second",
      false,
    )
    if (firstWindowsMode !== secondWindowsMode) {
      fail("WSL Windows adapter stub status is not deterministic")
    }
    return { linux: firstLinuxMode, windows: firstWindowsMode, dialect }
  } finally {
    for (const executable of [linuxExecutable, windowsExecutable]) {
      const cleanup = spawn("wsl.exe", ["-d", "Ubuntu", "--", "rm", "-f", executable])
      if (cleanup.exitCode !== 0) {
        fail(`WSL adapter cleanup failed: ${cleanup.stderr.toString().trim()}`)
      }
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
      "w_seed_ephemeral_provider_windows_tests",
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
      "w_seed_ephemeral_provider_(core|adapter|windows)",
    ],
  )

  const coreExecutable = join(buildDirectory, `w_seed_ephemeral_provider_tests${executableSuffix}`)
  const adapterExecutable = join(
    buildDirectory,
    `w_seed_ephemeral_provider_adapter_tests${executableSuffix}`,
  )
  const windowsExecutable = join(
    buildDirectory,
    `w_seed_ephemeral_provider_windows_tests${executableSuffix}`,
  )
  runDeterministic("native provider core", coreExecutable, validateCoreOutput)
  const nativeLinuxIsReal = process.platform === "linux"
  const nativeLinuxMode = runDeterministic(
    "native provider adapter",
    adapterExecutable,
    nativeLinuxIsReal ? validateLinuxAdapterOutput : validateLinuxStubOutput,
  )
  const nativeWindowsMode = runDeterministic(
    "native Windows provider adapter",
    windowsExecutable,
    (execution, label) =>
      validateWindowsAdapterOutput(execution, label, process.platform === "win32"),
  )
  if (nativeLinuxMode === "linux-stub=passed") {
    console.log("SKIP adapter-linux-real=non-linux-stub")
    console.log("linux-stub=passed")
  } else if (nativeLinuxMode === "linux-openat2=unsupported") {
    console.log("SKIP linux-openat2=unsupported")
    console.log("SKIP cross-mount=not-created-without-privilege")
  } else {
    console.log("SKIP cross-mount=not-created-without-privilege")
    console.log("linux-real=passed")
  }
  console.log(nativeWindowsMode)
  const wslAdapterMode = await runWslAdapter()
  if (wslAdapterMode !== undefined) {
    if (wslAdapterMode.linux === "linux-openat2=unsupported") {
      console.log("SKIP WSL linux-openat2=unsupported")
      console.log("SKIP WSL cross-mount=not-created-without-privilege")
    } else {
      console.log("SKIP WSL cross-mount=not-created-without-privilege")
      console.log("WSL linux-real=passed")
    }
    console.log("WSL windows-stub=passed")
    console.log(`WSL C dialect: ${dialectDisclosure(wslAdapterMode.dialect)}`)
  }
  console.log(
    "seed ephemeral provider: CMake core/Linux/Windows tests and deterministic bounded records passed",
  )
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
