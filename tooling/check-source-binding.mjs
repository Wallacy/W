import { existsSync } from "node:fs"
import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const expected = "w_seed_source_binding_linux_gate: ok\n"

function fail(message) {
  throw new Error(`BND0 source binding gate: ${message}`)
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

function runGate(command, args) {
  const execution = spawn(command, args)
  if (execution.exitCode !== 0 || execution.stderr.length !== 0 ||
      execution.stdout.toString() !== expected) {
    fail(`gate output was not exact: ${JSON.stringify({
      exitCode: execution.exitCode,
      stdout: execution.stdout.toString(),
      stderr: execution.stderr.toString(),
    })}`)
  }
  return execution.stdout
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-source-binding-gate-"))

try {
  let command = "cmake"
  let configureArgs = [
    "-S",
    seedDirectory,
    "-B",
    buildDirectory,
    "-G",
    "Unix Makefiles",
    "-DCMAKE_BUILD_TYPE=Debug",
  ]
  let buildArgs = [
    "--build",
    buildDirectory,
    "--target",
    "w_seed_source_binding_linux_gate",
    "--",
    "-j",
    "2",
  ]
  let gateCommand = join(buildDirectory, "w_seed_source_binding_linux_gate")
  let gateArgs = []
  if (process.platform === "win32") {
    const wslRoot = wslPath(root)
    const wslBuild = wslPath(buildDirectory)
    command = "wsl.exe"
    configureArgs = [
      "-d",
      "Ubuntu",
      "--",
      "cmake",
      "-S",
      wslRoot + "/compiler/seed-c",
      "-B",
      wslBuild,
      "-G",
      "Unix Makefiles",
      "-DCMAKE_BUILD_TYPE=Debug",
    ]
    buildArgs = [
      "-d",
      "Ubuntu",
      "--",
      "cmake",
      "--build",
      wslBuild,
      "--target",
      "w_seed_source_binding_linux_gate",
      "--",
      "-j",
      "2",
    ]
    gateCommand = "wsl.exe"
    gateArgs = ["-d", "Ubuntu", "--", `${wslBuild}/w_seed_source_binding_linux_gate`]
  }
  runRequired("CMake configure", command, configureArgs)
  runRequired("Linux gate build", command, buildArgs)
  if (process.platform !== "win32" && !existsSync(gateCommand)) {
    fail("CMake did not publish the Linux gate executable")
  }
  runGate(gateCommand, gateArgs)
  runGate(gateCommand, gateArgs)
  console.log("BND0 source binding gate: ok (Linux native/WSL; deterministic)")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
