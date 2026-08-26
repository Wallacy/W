import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const suffix = process.platform === "win32" ? ".exe" : ""

function fail(message) {
  throw new Error(`seed module scan: ${message}`)
}

function run(command, args, cwd = root) {
  const execution = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (execution.exitCode !== 0) {
    fail(`${command} ${args.join(" ")} failed: ${execution.stderr.toString().trim()}`)
  }
  return execution
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-module-scan-"))
try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory, "--target", "w_seed_module_scan_tests"])
  const test = join(buildDirectory, `w_seed_module_scan_tests${suffix}`)
  const result = run(test, [])
  if (result.stdout.length !== 0 || result.stderr.length !== 0) {
    fail("scanner unit test must remain silent")
  }
  console.log("seed module scan: forms, spans, bounded capacity, and fail-closed validation passed")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
