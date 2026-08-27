import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const suffix = process.platform === "win32" ? ".exe" : ""
const nativeLinux = process.platform === "linux"

function fail(message) {
  throw new Error(`seed ephemeral driver: ${message}`)
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

function invoke(executable) {
  const execution = spawn(executable, [])
  return {
    exitCode: execution.exitCode,
    stdout: Buffer.from(execution.stdout),
    stderr: Buffer.from(execution.stderr),
  }
}

function normalized(buffer) {
  return buffer.toString().replaceAll("\r\n", "\n")
}

function checkDeterministic(label, executable, expectedLines) {
  const first = invoke(executable)
  const second = invoke(executable)
  if (first.exitCode !== 0 || second.exitCode !== 0) {
    fail(`${label} returned ${first.exitCode}/${second.exitCode}`)
  }
  if (!first.stdout.equals(second.stdout) || !first.stderr.equals(second.stderr)) {
    fail(`${label} output is not deterministic`)
  }
  if (first.stderr.length !== 0) {
    fail(`${label} wrote to stderr: ${JSON.stringify(normalized(first.stderr))}`)
  }
  const output = normalized(first.stdout)
  const candidates = Array.isArray(expectedLines[0]) ? expectedLines : [expectedLines]
  const expected = candidates.map((lines) => `${lines.join("\n")}\n`)
  if (!expected.includes(output)) {
    fail(`${label} output differs: ${JSON.stringify(output)}`)
  }
  return output
}

function adapterExpectedLines() {
  if (!nativeLinux) {
    return [["SKIP adapter-linux-real=non-linux-stub", "w_seed_ephemeral_driver_linux_tests: ok"]]
  }
  return [
    ["w_seed_ephemeral_driver_linux_tests: ok"],
    ["SKIP adapter-linux-openat2=unsupported", "w_seed_ephemeral_driver_linux_tests: ok"],
  ]
}

function wslRepositoryPath() {
  if (process.platform !== "win32") return undefined
  const windowsRootArgument = root.replaceAll("\\", "/")
  const probe = spawn("wsl.exe", ["-d", "Ubuntu", "--", "wslpath", "-a", windowsRootArgument])
  if (probe.exitCode !== 0 || probe.stderr.length !== 0) return undefined
  const path = probe.stdout.toString().trim()
  if (!path.startsWith("/") || path.includes("\0") || /[\r\n]/u.test(path)) return undefined
  return path
}

function wslSourceArguments(wslRoot) {
  return [
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
    "w_seed_ephemeral_driver.c",
  ].map((source) => `${wslRoot}/compiler/seed-c/src/${source}`)
}

function checkWslDeterministic(label, executable, expectedLines) {
  const first = runRequired(`${label} first`, "wsl.exe", ["-d", "Ubuntu", "--", executable])
  const second = runRequired(`${label} second`, "wsl.exe", ["-d", "Ubuntu", "--", executable])
  if (!Buffer.from(first.stdout).equals(Buffer.from(second.stdout)) ||
      !Buffer.from(first.stderr).equals(Buffer.from(second.stderr))) {
    fail(`${label} output is not deterministic`)
  }
  if (first.stderr.length !== 0) fail(`${label} wrote to stderr`)
  const output = normalized(Buffer.from(first.stdout))
  const candidates = Array.isArray(expectedLines[0]) ? expectedLines : [expectedLines]
  const expected = candidates.map((lines) => `${lines.join("\n")}\n`)
  if (!expected.includes(output)) {
    fail(`${label} output differs: ${JSON.stringify(output)}`)
  }
  return output
}

async function runWslLinuxAdapter() {
  if (process.platform !== "win32") return "not-windows"
  const wslRoot = wslRepositoryPath()
  if (wslRoot === undefined) {
    console.log("SKIP adapter-linux-wsl=unavailable")
    return "unavailable"
  }
  const executable = `/tmp/w-seed-ephemeral-driver-${process.pid}`
  const sources = wslSourceArguments(wslRoot)
  try {
    runRequired(
      "WSL Linux adapter compile",
      "wsl.exe",
      [
        "-d",
        "Ubuntu",
        "--",
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wconversion",
        "-Wsign-conversion",
        "-Wshadow",
        "-Werror",
        "-I",
        `${wslRoot}/compiler/seed-c/include`,
        ...sources,
        `${wslRoot}/compiler/seed-c/tests/test_ephemeral_driver_linux.c`,
        "-o",
        executable,
      ],
    )
    const output = checkWslDeterministic("WSL Linux adapter", executable, [
      "w_seed_ephemeral_driver_linux_tests: ok\n",
      "SKIP adapter-linux-openat2=unsupported\nw_seed_ephemeral_driver_linux_tests: ok\n",
    ].map((line) => line.endsWith("\n") ? line.trimEnd().split("\n") : [line]))
    console.log(output.trimEnd())
    return "passed"
  } finally {
    const cleanup = spawn("wsl.exe", ["-d", "Ubuntu", "--", "rm", "-f", executable])
    if (cleanup.exitCode !== 0) {
      fail(`WSL Linux adapter cleanup failed: ${cleanup.stderr.toString().trim()}`)
    }
  }
}

async function runWslSanitizer() {
  if (process.platform !== "win32") return "not-windows"
  const wslRoot = wslRepositoryPath()
  if (wslRoot === undefined) {
    console.log("SKIP sanitizer-wsl=unavailable")
    return "unavailable"
  }
  const coreExecutable = `/tmp/w-seed-ephemeral-driver-asan-core-${process.pid}`
  const adapterExecutable = `/tmp/w-seed-ephemeral-driver-asan-adapter-${process.pid}`
  const flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
  const common = [
    "-d",
    "Ubuntu",
    "--",
    "gcc",
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wconversion",
    "-Wsign-conversion",
    "-Wshadow",
    "-Werror",
    ...flags,
    "-I",
    `${wslRoot}/compiler/seed-c/include`,
    ...wslSourceArguments(wslRoot),
  ]
  try {
    const coreCompile = spawn("wsl.exe", [
      ...common,
      `${wslRoot}/compiler/seed-c/tests/test_ephemeral_driver.c`,
      "-o",
      coreExecutable,
    ])
    if (coreCompile.exitCode !== 0) {
      console.log("SKIP sanitizer-wsl=unavailable")
      return "unavailable"
    }
    const adapterCompile = spawn("wsl.exe", [
      ...common,
      `${wslRoot}/compiler/seed-c/tests/test_ephemeral_driver_linux.c`,
      "-o",
      adapterExecutable,
    ])
    if (adapterCompile.exitCode !== 0) {
      console.log("SKIP sanitizer-wsl=unavailable")
      return "unavailable"
    }
    if (coreCompile.stderr.length !== 0 || adapterCompile.stderr.length !== 0) {
      fail("WSL ASan+UBSan compile wrote to stderr")
    }
    checkWslDeterministic("WSL ASan+UBSan core", coreExecutable, [
      ["w_seed_ephemeral_driver_tests: ok"],
    ])
    checkWslDeterministic("WSL ASan+UBSan adapter", adapterExecutable, [
      ["w_seed_ephemeral_driver_linux_tests: ok"],
      ["SKIP adapter-linux-openat2=unsupported", "w_seed_ephemeral_driver_linux_tests: ok"],
    ])
    console.log("sanitizer-wsl=passed")
    return "passed"
  } finally {
    const coreCleanup = spawn("wsl.exe", ["-d", "Ubuntu", "--", "rm", "-f", coreExecutable])
    const adapterCleanup = spawn("wsl.exe", ["-d", "Ubuntu", "--", "rm", "-f", adapterExecutable])
    if (coreCleanup.exitCode !== 0 || adapterCleanup.exitCode !== 0) {
      fail("WSL ASan+UBSan cleanup failed")
    }
  }
}

async function runSanitizer() {
  const directory = await mkdtemp(join(tmpdir(), "w-seed-ephemeral-driver-asan-"))
  try {
    const flags = "-fsanitize=address,undefined -fno-omit-frame-pointer"
    const configured = spawn("cmake", [
      "-S",
      seedDirectory,
      "-B",
      directory,
      "-G",
      "Ninja",
      "-DCMAKE_BUILD_TYPE=Debug",
      `-DCMAKE_C_FLAGS=${flags}`,
      `-DCMAKE_EXE_LINKER_FLAGS=${flags}`,
    ])
    if (configured.exitCode !== 0) {
      console.log("SKIP sanitizer-native=unavailable")
      return "unavailable"
    }
    const built = spawn("cmake", [
      "--build",
      directory,
      "--target",
      "w_seed_ephemeral_driver_tests",
      "w_seed_ephemeral_driver_linux_tests",
      "--",
      "-j",
      "2",
    ])
    if (built.exitCode !== 0) {
      console.log("SKIP sanitizer-native=unavailable")
      return "unavailable"
    }
    checkDeterministic(
      "sanitized driver core",
      join(directory, `w_seed_ephemeral_driver_tests${suffix}`),
      ["w_seed_ephemeral_driver_tests: ok"],
    )
    checkDeterministic(
      "sanitized driver adapter",
      join(directory, `w_seed_ephemeral_driver_linux_tests${suffix}`),
      adapterExpectedLines(),
    )
    console.log("sanitizer=passed")
    return "passed"
  } finally {
    await rm(directory, { recursive: true, force: true })
  }
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-ephemeral-driver-"))
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
      "w_seed_ephemeral_driver_tests",
      "w_seed_ephemeral_driver_linux_tests",
      "--",
      "-j",
      "2",
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
      "w_seed_ephemeral_driver(_linux)?_unit",
    ],
  )

  checkDeterministic(
    "driver core",
    join(buildDirectory, `w_seed_ephemeral_driver_tests${suffix}`),
    ["w_seed_ephemeral_driver_tests: ok"],
  )
  checkDeterministic(
    "driver adapter",
    join(buildDirectory, `w_seed_ephemeral_driver_linux_tests${suffix}`),
    adapterExpectedLines(),
  )
  await runWslLinuxAdapter()
  const sanitizerMode = await runSanitizer()
  if (sanitizerMode === "unavailable") await runWslSanitizer()
  console.log("seed ephemeral driver: bounded core/adapter tests, deterministic output, fail-closed publication, and optional sanitizer/WSL evidence passed")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
