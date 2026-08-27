import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { basename, join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const platform = resolve(root, "reference", "last-light", "platform.w")

function fail(message) {
  throw new Error("seed check driver: " + message)
}

function run(command, args) {
  const execution = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (execution.exitCode !== 0) {
    fail(command + " " + args.join(" ") + " failed: " +
      execution.stderr.toString().trim())
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

function normalize(text) {
  return text.replace(/\r\n/gu, "\n")
}

function pointAt(bytes, offset) {
  if (offset > bytes.length) fail("diagnostic offset is outside fixture")
  let line = 1
  let column = 1
  for (let index = 0; index < offset; index += 1) {
    if (bytes[index] === 0x0a) {
      line += 1
      column = 1
    } else {
      column += 1
    }
  }
  return { line, column }
}

function expectEmptyOutput(result, label) {
  if (result.stdout.length !== 0) fail(label + " wrote to stdout")
}

function expectRecord(result, source, startByte, endByte, label) {
  if (result.exitCode !== 1 || result.stderr.length !== 0) {
    fail(label + " has the wrong exit or stderr: " + JSON.stringify({
      exitCode: result.exitCode,
      stderr: result.stderr,
    }))
  }
  const text = result.stdout.toString()
  const lines = text.split("\n")
  if (lines.length !== 2 || lines[1] !== "") {
    fail(label + " is not one JSONL record")
  }
  let record
  try {
    record = JSON.parse(lines[0])
  } catch (error) {
    fail(label + " is not JSON: " + error)
  }
  if (JSON.stringify(record) !== lines[0]) {
    fail(label + " changed canonical field order")
  }
  const expected = {
    schemaVersion: 1,
    instance: "D000001",
    code: "W-SEM-0001",
    phase: "semantic.type",
    severity: "error",
    primary: { source, startByte, endByte },
    labels: [],
    facts: { actual: "1", expected: "Bool" },
    notes: [],
    fixes: [],
    root: null,
  }
  if (JSON.stringify(record) !== JSON.stringify(expected)) {
    fail(label + " record differs: " + JSON.stringify(record))
  }
  return lines[0]
}

function expectClosed(result, label) {
  if (result.exitCode !== 2 || result.stdout.length !== 0 ||
      result.stderr.length === 0) {
    fail(label + " did not fail closed: " + JSON.stringify({
      exitCode: result.exitCode,
      stdout: result.stdout.toString(),
      stderr: result.stderr,
    }))
  }
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-check-driver-"))
const invalidSource = join(buildDirectory, "invalid_utf8.w")
const unsupportedSource = join(buildDirectory, "unsupported.w")
const parseSource = join(buildDirectory, "incomplete.w")
const mutationSource = join(buildDirectory, "platform_negative.w")
const missingSource = join(buildDirectory, "missing.w")
try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory, "--target", "w_seed_check_driver",
    "w_seed_diagnostic_tests", "--", "-j", "2"])
  const extension = process.platform === "win32" ? ".exe" : ""
  const driver = join(buildDirectory, "w_seed_check_driver" + extension)

  const platformBytes = await readFile(platform)
  const suffix = "\nfn invalidPlatformCondition() {\n  if 1 { return }\n}"
  const mutationBytes = Buffer.concat([
    platformBytes,
    Buffer.from(suffix, "utf8"),
  ])
  await writeFile(mutationSource, mutationBytes)
  const marker = Buffer.byteLength(
    "\nfn invalidPlatformCondition() {\n  if ", "utf8")
  const startByte = platformBytes.length + marker
  const endByte = startByte + 2
  const logicalSource = basename(mutationSource)

  const clean = invoke(driver, ["--json", platform])
  if (clean.exitCode !== 0) fail("platform clean exit " + clean.exitCode)
  expectEmptyOutput(clean, "platform clean JSON")
  if (clean.stderr.length !== 0) fail("platform clean JSON wrote a diagnostic")

  const first = invoke(driver, ["--json", mutationSource])
  const second = invoke(driver, ["--json", mutationSource])
  const firstRecord = expectRecord(first, logicalSource, startByte, endByte,
    "mutation JSON first run")
  const secondRecord = expectRecord(second, logicalSource, startByte, endByte,
    "mutation JSON second run")
  if (firstRecord !== secondRecord) {
    fail("mutation JSON is not byte-identical across runs")
  }

  const human = invoke(driver, [mutationSource])
  expectEmptyOutput(human, "mutation human")
  const point = pointAt(mutationBytes, startByte)
  const expectedHuman = mutationSource + ":" + point.line + ":" +
    point.column + ":W-SEM-0001: node does not satisfy its expected semantic " +
    "use; facts=actual=1, expected=Bool\n"
  if (human.exitCode !== 1 || normalize(human.stderr) !== expectedHuman) {
    fail("human diagnostic is not stable: " + JSON.stringify(human))
  }

  await writeFile(invalidSource, Buffer.from([0xc3]))
  await writeFile(unsupportedSource,
    Buffer.from("const duration: PhysicalDuration = 10<si.s>\nstruct Use {}\n",
      "utf8"))
  await writeFile(parseSource, Buffer.from("fn broken(\n", "utf8"))
  for (const [label, path] of [
    ["missing", missingSource],
    ["invalid UTF-8", invalidSource],
    ["unsupported frontend", unsupportedSource],
    ["incomplete parse", parseSource],
  ]) {
    expectClosed(invoke(driver, ["--json", path]), label)
  }

  run("ctest", ["--test-dir", buildDirectory, "-R",
    "w_seed_diagnostic_unit", "--output-on-failure"])
  console.log("seed check driver: clean, logical D0 JSON, deterministic human " +
    "diagnostic, invalid UTF-8, parse, unsupported, missing, and empty-output " +
    "failures passed")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
