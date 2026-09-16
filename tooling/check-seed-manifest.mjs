import { access, constants, mkdtemp, rm, stat } from "node:fs/promises"
import { homedir, tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { dialectArgs, dialectDisclosure, probeCDialect, requireCDialect } from "./c-dialect.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const strictWarningFlags = [
  "-Wall",
  "-Wextra",
  "-Wpedantic",
  "-Wconversion",
  "-Wsign-conversion",
  "-Wshadow",
  "-Werror",
]
const gateSources = [
  "src/w_seed_source.c",
  "src/w_seed_unicode.c",
  "src/w_seed_unicode_data.c",
  "src/w_seed_sha256.c",
  "src/w_seed_owner_guard.c",
  "src/w_seed_owner_guard_linux.c",
  "src/w_seed_owner_guard_windows.c",
  "src/w_seed_manifest.c",
  "src/w_seed_manifest_linux.c",
  "src/w_seed_manifest_windows.c",
  "tests/test_manifest_linux_gate.c",
]

function fail(message) {
  throw new Error("MAN0 Linux gate: " + message)
}

function shortOutput(value) {
  const text = value.toString().trim()
  return text.length > 2000 ? text.slice(0, 2000) + "…" : text
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
    const stderr = shortOutput(execution.stderr)
    fail(label + " failed" + (stderr ? ": " + stderr : ""))
  }
  return execution
}

function normalized(execution, label) {
  if (execution.stderr.length !== 0) {
    fail(label + " wrote stderr: " + shortOutput(execution.stderr))
  }
  return execution.stdout.toString().replaceAll("\r\n", "\n")
}

function wslPath(windowsPath) {
  const argument = windowsPath.replaceAll("\\", "/")
  const execution = runRequired(
    "WSL path conversion",
    "wsl.exe",
    ["-d", "Ubuntu", "--", "wslpath", "-a", argument],
  )
  const value = execution.stdout.toString().trim()
  if (!value.startsWith("/") || value.includes("\0") || value.includes("\n")) {
    fail("WSL path conversion returned a non-absolute path")
  }
  return value
}

function wslRootFilesystemTemporaryBase() {
  const homeExecution = runRequired(
    "WSL home discovery",
    "wsl.exe",
    ["-d", "Ubuntu", "--", "printenv", "HOME"],
  )
  const home = homeExecution.stdout.toString().trim()
  const rootDevice = runRequired(
    "WSL root device discovery",
    "wsl.exe",
    ["-d", "Ubuntu", "--", "stat", "-c", "%d", "--", "/"],
  ).stdout.toString().trim()
  for (const candidate of ["/var/tmp", home, "/tmp"]) {
    if (!candidate.startsWith("/") || candidate.includes("\0") ||
        candidate.includes("\n")) continue
    const probe = spawn("wsl.exe", [
      "-d", "Ubuntu", "--", "stat", "-c", "%d", "--", candidate,
    ])
    const writable = spawn("wsl.exe", [
      "-d", "Ubuntu", "--", "test", "-d", candidate, "-a", "-w", candidate,
      "-a", "-x", candidate,
    ])
    if (probe.exitCode === 0 && probe.stdout.toString().trim() === rootDevice &&
        writable.exitCode === 0) return candidate
  }
  fail("no writable temporary directory exists on the WSL root filesystem")
}

function wslTemporaryFixture() {
  const base = wslRootFilesystemTemporaryBase()
  const execution = runRequired(
    "WSL fixture creation",
    "wsl.exe",
    ["-d", "Ubuntu", "--", "mktemp", "-d", "-p", base,
      "w-manifest-fixture-XXXXXX"],
  )
  const value = execution.stdout.toString().trim()
  if (!value.startsWith(`${base}/`) ||
      !/^w-manifest-fixture-[A-Za-z0-9]+$/u.test(value.slice(base.length + 1))) {
    fail("WSL fixture creation returned an unallowlisted path")
  }
  return { path: value, base }
}

function removeWslTemporaryFixture(fixture) {
  if (fixture === undefined) return
  if (!fixture.path.startsWith(`${fixture.base}/`) ||
      !/^w-manifest-fixture-[A-Za-z0-9]+$/u.test(
        fixture.path.slice(fixture.base.length + 1),
      )) {
    fail("refusing to remove an unallowlisted WSL fixture")
  }
  runRequired("WSL fixture cleanup", "wsl.exe", [
    "-d",
    "Ubuntu",
    "--",
    "rm",
    "-rf",
    "--",
    fixture.path,
  ])
}

async function nativeRootFilesystemTemporaryBase() {
  const rootDevice = (await stat("/")).dev
  for (const candidate of ["/var/tmp", homedir(), tmpdir()]) {
    try {
      const value = await stat(candidate)
      await access(candidate, constants.W_OK | constants.X_OK)
      if (value.isDirectory() && value.dev === rootDevice) return candidate
    } catch {
      // Try the next closed candidate.
    }
  }
  fail("no writable temporary directory exists on the native root filesystem")
}

function canonicalOutput(output, label) {
  const match = /^w_seed_manifest_linux_gate: candidates=2 order=0,1 semantic=([0-9a-f]{64}) provenance=([0-9a-f]{64}) receipt=([0-9a-f]{64})\n$/u.exec(output)
  if (!match) fail(label + " returned non-canonical success output")
  return match
}

function runTwice(label, command, args, cwd, expected) {
  const first = normalized(runRequired(label + " first run", command, args, cwd), label)
  const second = normalized(runRequired(label + " second run", command, args, cwd), label)
  if (first !== second) fail(label + " stdout was not byte-identical")
  const firstMatch = canonicalOutput(first, label)
  const secondMatch = canonicalOutput(second, label)
  if (firstMatch[0] !== secondMatch[0]) fail(label + " canonical outputs differ")
  if (expected !== undefined && first !== expected) {
    fail(label + " returned an unexpected success line")
  }
  return first
}

let buildDirectory
let fixtureDirectory
let wslFixtureDirectory
try {
  buildDirectory = await mkdtemp(join(tmpdir(), "w-manifest-linux-"))
  if (process.platform === "win32") {
    wslFixtureDirectory = wslTemporaryFixture()
  } else {
    fixtureDirectory = await mkdtemp(join(
      await nativeRootFilesystemTemporaryBase(),
      "w-manifest-fixture-",
    ))
  }
  const binaryName = "w_seed_manifest_linux_gate"
  const binaryPath = join(buildDirectory, binaryName)

  if (process.platform === "win32") {
    const wslRoot = wslPath(root)
    const wslBuild = wslPath(buildDirectory)
    const wslFixture = wslFixtureDirectory.path
    const sources = gateSources.map(
      (source) => `${wslRoot}/compiler/seed-c/${source}`,
    )
    const dialect = requireCDialect(
      await probeCDialect(["wsl.exe", "-d", "Ubuntu", "--", "gcc"]),
      "WSL GCC",
    )
    runRequired(
      "WSL Linux MAN0 real compile",
      "wsl.exe",
      [
        "-d",
        "Ubuntu",
        "--",
        "gcc",
        ...dialectArgs(dialect),
        ...strictWarningFlags,
        "-I",
        `${wslRoot}/compiler/seed-c/include`,
        ...sources,
        "-o",
        `${wslBuild}/${binaryName}`,
      ],
    )
    const output = runTwice(
      "WSL Linux MAN0 real gate",
      "wsl.exe",
      ["-d", "Ubuntu", "--", `${wslBuild}/${binaryName}`, wslFixture],
      root,
    )
    canonicalOutput(output, "WSL Linux MAN0 real gate")
    console.log(`MAN0 C dialect: ${dialectDisclosure(dialect)}`)
  } else {
    const sources = gateSources.map((source) => resolve(seedDirectory, source))
    const compiler = Bun.which(process.env.MANIFEST_CC ?? "gcc")
    const dialect = requireCDialect(
      await probeCDialect(compiler),
      "MAN0 C compiler",
    )
    runRequired(
      "Linux MAN0 real compile",
      compiler,
      [
        ...dialectArgs(dialect),
        ...strictWarningFlags,
        "-I",
        resolve(seedDirectory, "include"),
        ...sources,
        "-o",
        binaryPath,
      ],
    )
    const output = runTwice(
      "Linux MAN0 real gate",
      binaryPath,
      [fixtureDirectory],
      root,
    )
    canonicalOutput(output, "Linux MAN0 real gate")
    console.log(`MAN0 C dialect: ${dialectDisclosure(dialect)}`)
  }
  console.log("MAN0 Linux gate: real OWN0+MAN0 two-wave; deterministic")
} finally {
  if (wslFixtureDirectory !== undefined) {
    removeWslTemporaryFixture(wslFixtureDirectory)
  }
  if (fixtureDirectory !== undefined) {
    await rm(fixtureDirectory, { recursive: true, force: true })
  }
  if (buildDirectory !== undefined) {
    await rm(buildDirectory, { recursive: true, force: true })
  }
}
