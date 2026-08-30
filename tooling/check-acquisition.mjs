import { existsSync } from "node:fs"
import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { basename, join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const expectedTests = [
  "w_seed_acquisition_unit",
  "w_seed_check_storage_unit",
  "w_seed_check_retry_unit",
  "w_seed_check_pipeline_unit",
  "w_seed_check_host_unit",
]
const testPattern =
  "^w_seed_(acquisition|check_storage|check_retry|check_pipeline|check_host)_unit$"
const targetNames = [
  "w_seed_acquisition_tests",
  "w_seed_check_storage_tests",
  "w_seed_check_retry_tests",
  "w_seed_check_pipeline_tests",
  "w_seed_check_host_tests",
]

function fail(message) {
  throw new Error(`ACQ0 acquisition gate: ${message}`)
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

function summary(result) {
  return JSON.stringify({
    exitCode: result.exitCode,
    stdout: result.stdout.toString(),
    stderr: result.stderr.toString(),
  })
}

function checkAcquisitionTwice(executable) {
  const first = invoke(executable)
  const second = invoke(executable)
  if (first.exitCode !== 0 || second.exitCode !== 0) {
    fail(`acquisition unit returned ${first.exitCode}/${second.exitCode}`)
  }
  if (!first.stdout.equals(second.stdout) ||
      !first.stderr.equals(second.stderr)) {
    fail("acquisition unit output is not byte-identical between runs")
  }
  if (first.stderr.length !== 0) {
    fail(`acquisition unit wrote to stderr: ${summary(first)}`)
  }
  const newline = process.platform === "win32" ? "\r\n" : "\n"
  const expected = Buffer.from(`w_seed_acquisition_tests: ok${newline}`)
  if (!first.stdout.equals(expected)) {
    fail(`acquisition unit stdout is not exact: ${summary(first)}`)
  }
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-acquisition-gate-"))

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
  runRequired("explicit acquisition build", "cmake", [
    "--build",
    buildDirectory,
    "--target",
    ...targetNames,
    "--",
    "-j",
    "2",
  ])

  const listing = runRequired("focal CTest listing", "ctest", [
    "--test-dir",
    buildDirectory,
    "--show-only=json-v1",
    "-R",
    testPattern,
  ])
  let tests
  try {
    tests = JSON.parse(listing.stdout.toString()).tests
  } catch (error) {
    fail(`focal CTest listing is not JSON: ${error.message}`)
  }
  const names = Array.isArray(tests) ? tests.map((test) => test.name) : []
  if (JSON.stringify(names) !== JSON.stringify(expectedTests)) {
    fail(`focal CTest membership differs: ${JSON.stringify(names)}`)
  }
  runRequired("anchored focal CTest", "ctest", [
    "--test-dir",
    buildDirectory,
    "--output-on-failure",
    "-R",
    testPattern,
  ])

  const suffix = process.platform === "win32" ? ".exe" : ""
  const acquisition = join(
    buildDirectory,
    `w_seed_acquisition_tests${suffix}`,
  )
  if (!existsSync(acquisition) ||
      basename(acquisition) !== `w_seed_acquisition_tests${suffix}`) {
    fail("acquisition target did not publish the expected executable")
  }
  checkAcquisitionTwice(acquisition)
  console.log("ACQ0 acquisition gate: ok")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
