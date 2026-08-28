import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const fixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const buildDirectory = await mkdtemp(join(tmpdir(), "w-hlo0-"))
const suffix = process.platform === "win32" ? ".exe" : ""

function fail(message) {
  throw new Error(`HLO0: ${message}`)
}

function run(command, args, cwd = root) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (result.exitCode !== 0) {
    const stderr = result.stderr.toString().trim()
    fail(`${command} ${args.join(" ")} failed${stderr ? `: ${stderr}` : ""}`)
  }
  return result.stdout.toString()
}

try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory, "--target", "w_seed_hlo0_gate",
    "w_seed_hlo0_tests", "--parallel", "2"])
  const tests = run(resolve(buildDirectory, `w_seed_hlo0_tests${suffix}`), [])
  if (!tests.includes("source-backed bounded Hello plan")) {
    fail("adversarial C test witness is missing")
  }
  const gate = run(resolve(buildDirectory, `w_seed_hlo0_gate${suffix}`), [fixture])
  if (!gate.includes("source-backed plan only; W execution unavailable")) {
    fail("gate did not state the execution boundary")
  }
  if (gate.toLowerCase().includes("timing") || gate.toLowerCase().includes("performance")) {
    fail("gate reported timing/performance data")
  }
  if (!gate.includes("payload=13") || !gate.includes("stdout=14") ||
      !gate.includes("sha256=d9014c4624844aa5bac314773d6b689ad467fa4e1d1a50a1b8a99d5a95f72ff5")) {
    fail("gate output does not contain the canonical payload identity")
  }
  process.stdout.write(`${gate.trim()}\n`)
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
