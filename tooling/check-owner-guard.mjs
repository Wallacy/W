import { existsSync } from "node:fs"
import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const expectedTests = [
  "w_seed_owner_guard_unit",
  "w_seed_owner_guard_adapter_unit",
]
const testPattern = "^w_seed_owner_guard_(unit|adapter_unit)$"
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
  throw new Error(`OWN0 owner guard gate: ${message}`)
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

function normalized(execution, label) {
  if (execution.stderr.length !== 0) {
    fail(`${label} wrote to stderr: ${execution.stderr.toString().trim()}`)
  }
  return execution.stdout.toString().replaceAll("\r\n", "\n")
}

function runTwice(label, command, args, expected) {
  const first = runRequired(`${label} first`, command, args)
  const second = runRequired(`${label} second`, command, args)
  if (!Buffer.from(first.stdout).equals(Buffer.from(second.stdout)) ||
      !Buffer.from(first.stderr).equals(Buffer.from(second.stderr))) {
    fail(`${label} output is not byte-identical between runs`)
  }
  const output = normalized(first, `${label} first`)
  normalized(second, `${label} second`)
  if (output !== expected) {
    fail(`${label} stdout is not exact: ${JSON.stringify(output)}`)
  }
}

function wslPath(windowsPath) {
  const argument = windowsPath.replaceAll("\\", "/")
  const probe = runRequired("WSL path conversion", "wsl.exe", [
    "-d",
    "Ubuntu",
    "--",
    "wslpath",
    "-a",
    argument,
  ])
  if (probe.stderr.length !== 0) fail("WSL path conversion wrote to stderr")
  const value = probe.stdout.toString().trim()
  if (!value.startsWith("/") || value.includes("\0") || value.includes("\n")) {
    fail("WSL path conversion returned an invalid absolute path")
  }
  return value
}

function runRequiredWslLinux(buildDirectory) {
  if (process.platform !== "win32") return
  const wslRoot = wslPath(root)
  const wslBuild = wslPath(buildDirectory)
  const executable = `${wslBuild}/w_seed_owner_guard_adapter_wsl`
  const sources = [
    "w_seed_source.c",
    "w_seed_owner_guard.c",
    "w_seed_owner_guard_linux.c",
    "w_seed_owner_guard_windows.c",
  ].map((name) => `${wslRoot}/compiler/seed-c/src/${name}`)
  const compile = runRequired("WSL Linux real compile", "wsl.exe", [
    "-d",
    "Ubuntu",
    "--",
    "gcc",
    ...strictWarnings,
    "-I",
    `${wslRoot}/compiler/seed-c/include`,
    ...sources,
    `${wslRoot}/compiler/seed-c/tests/test_owner_guard_adapters.c`,
    "-o",
    executable,
  ])
  if (compile.stdout.length !== 0 || compile.stderr.length !== 0) {
    fail("WSL Linux real compile produced output")
  }
  runTwice(
    "WSL Linux real and Windows stub",
    "wsl.exe",
    ["-d", "Ubuntu", "--", executable],
    "w_seed_owner_guard_adapters: linux-native ok; windows-stub ok\n",
  )
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-owner-guard-"))
try {
  runRequired("CMake configure", "cmake", [
    "-S",
    seedDirectory,
    "-B",
    buildDirectory,
    "-G",
    "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug",
  ])
  runRequired("explicit OWN0 build", "cmake", [
    "--build",
    buildDirectory,
    "--target",
    "w_seed_owner_guard_tests",
    "w_seed_owner_guard_adapter_tests",
    "--",
    "-j",
    "2",
  ])
  const listing = runRequired("OWN0 CTest listing", "ctest", [
    "--test-dir",
    buildDirectory,
    "--show-only=json-v1",
    "-R",
    testPattern,
  ])
  let names
  try {
    const parsed = JSON.parse(listing.stdout.toString())
    names = Array.isArray(parsed.tests)
      ? parsed.tests.map((test) => test.name)
      : []
  } catch (error) {
    fail(`OWN0 CTest listing is not JSON: ${error.message}`)
  }
  if (JSON.stringify(names) !== JSON.stringify(expectedTests)) {
    fail(`OWN0 CTest membership differs: ${JSON.stringify(names)}`)
  }
  runRequired("anchored OWN0 CTest", "ctest", [
    "--test-dir",
    buildDirectory,
    "--output-on-failure",
    "-R",
    testPattern,
  ])

  const suffix = process.platform === "win32" ? ".exe" : ""
  const core = join(buildDirectory, `w_seed_owner_guard_tests${suffix}`)
  const adapters = join(
    buildDirectory,
    `w_seed_owner_guard_adapter_tests${suffix}`,
  )
  if (!existsSync(core) || !existsSync(adapters)) {
    fail("CMake did not publish both OWN0 executables")
  }
  runTwice(
    "native OWN0 core",
    core,
    [],
    "w_seed_owner_guard_tests: ok\n",
  )
  const nativeExpected =
    process.platform === "win32"
      ? "w_seed_owner_guard_adapters: windows-disabled unsupported; linux-stub ok\n"
      : "w_seed_owner_guard_adapters: linux-native ok; windows-stub ok\n"
  runTwice("native OWN0 adapters", adapters, [], nativeExpected)
  runRequiredWslLinux(buildDirectory)
  console.log(
    "OWN0 owner guard gate: core deterministic; Linux real; Windows fail-closed",
  )
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
