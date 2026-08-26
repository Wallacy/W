import { mkdtemp, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const platform = resolve(root, "reference", "last-light", "platform.w")

function fail(message) {
  throw new Error(`seed check driver: ${message}`)
}

function run(command, args) {
  const execution = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (execution.exitCode !== 0) {
    fail(`${command} ${args.join(" ")} failed: ${execution.stderr.toString().trim()}`)
  }
  return execution
}

function invoke(driver, args) {
  const execution = Bun.spawnSync({
    cmd: [driver, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
  return {
    exitCode: execution.exitCode,
    stdout: Buffer.from(execution.stdout),
    stderr: execution.stderr.toString(),
  }
}

function expectEmptyOutput(result, label) {
  if (result.stdout.length !== 0) fail(`${label} wrote to stdout`)
}

function expectRecord(result, path, label) {
  if (result.exitCode !== 1) fail(`${label} exit ${result.exitCode}, expected 1`)
  const text = result.stdout.toString()
  const lines = text.split("\n")
  if (lines.length !== 2 || lines[1] !== "") fail(`${label} is not one JSONL record`)
  let record
  try {
    record = JSON.parse(lines[0])
  } catch (error) {
    fail(`${label} is not JSON: ${error}`)
  }
  if (JSON.stringify(record) !== lines[0]) fail(`${label} changed canonical field order`)
  const expected = {
    schemaVersion: 1,
    instance: "D000001",
    code: "W-SEM-0001",
    phase: "semantic.type",
    severity: "error",
    primary: { source: path, startByte: 171, endByte: 173 },
    labels: [],
    facts: { actual: "1", expected: "Bool" },
    notes: [],
    fixes: [],
    root: null,
  }
  if (JSON.stringify(record) !== JSON.stringify(expected)) {
    fail(`${label} record differs: ${JSON.stringify(record)}`)
  }
  return lines[0]
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-check-driver-"))
const invalidSource = join(buildDirectory, "invalid-utf8.w")
const unsupportedSource = join(buildDirectory, "unsupported.w")
const parseSource = join(buildDirectory, "incomplete.w")
const mutationSource = join(buildDirectory, "platform-negative.w")
try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory, "--target", "w_seed_check_driver", "w_seed_diagnostic_tests", "--", "-j", "2"])
  const extension = process.platform === "win32" ? ".exe" : ""
  const driver = join(buildDirectory, `w_seed_check_driver${extension}`)

  const platformBytes = await Bun.file(platform).arrayBuffer()
  const mutationBytes = Buffer.concat([
    Buffer.from(platformBytes),
    Buffer.from("\nfn invalidPlatformCondition() {\n  if 1 { return }\n}", "utf8"),
  ])
  await writeFile(mutationSource, mutationBytes)

  const clean = invoke(driver, ["--json", platform])
  if (clean.exitCode !== 0) fail(`platform clean exit ${clean.exitCode}`)
  expectEmptyOutput(clean, "platform clean JSON")
  if (clean.stderr.length !== 0) fail("platform clean JSON wrote a diagnostic")

  const first = invoke(driver, ["--json", mutationSource])
  const second = invoke(driver, ["--json", mutationSource])
  const firstRecord = expectRecord(first, mutationSource, "mutation JSON first run")
  const secondRecord = expectRecord(second, mutationSource, "mutation JSON second run")
  if (firstRecord !== secondRecord) fail("mutation JSON is not byte-identical across runs")
  if (first.stderr.length !== 0 || second.stderr.length !== 0) {
    fail("mutation JSON wrote human diagnostics")
  }

  const human = invoke(driver, [mutationSource])
  expectEmptyOutput(human, "mutation human")
  if (human.exitCode !== 1 ||
      !human.stderr.includes(`${mutationSource}:9:6:W-SEM-0001: actual=1 expected=Bool`)) {
    fail(`human diagnostic is not stable: ${JSON.stringify(human)}`)
  }

  await writeFile(invalidSource, Buffer.from([0xc3]))
  await writeFile(unsupportedSource,
                  Buffer.from("const duration: PhysicalDuration = 10<si.s>\nstruct Use {}\n"))
  await writeFile(parseSource, Buffer.from("fn broken(\n"))
  const missing = invoke(driver, ["--json", join(buildDirectory, "missing.w")])
  const invalid = invoke(driver, ["--json", invalidSource])
  const unsupported = invoke(driver, ["--json", unsupportedSource])
  const incomplete = invoke(driver, ["--json", parseSource])
  for (const [label, result] of [
    ["missing", missing],
    ["invalid UTF-8", invalid],
    ["unsupported frontend", unsupported],
    ["incomplete parse", incomplete],
  ]) {
    if (result.exitCode !== 2) fail(`${label} exit ${result.exitCode}, expected 2`)
    expectEmptyOutput(result, label)
  }
  run("ctest", ["--test-dir", buildDirectory, "-R", "w_seed_diagnostic_unit", "--output-on-failure"])
  console.log("seed check driver: platform clean, semantic JSON/human error, and fail-closed cases passed")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
