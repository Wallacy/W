import { existsSync } from "node:fs"
import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { basename, join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const fixture = resolve(root, "reference", "last-light", "checker_bootstrap.w")
const expectedHelp = "usage: w check <path/file.w> [--json]\n"
const sourceCapacity = 16 * 1024 * 1024

function fail(message) {
  throw new Error(`w check cli: ${message}`)
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

function invoke(executable, args) {
  const execution = Bun.spawnSync({
    cmd: [executable, ...args],
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

function expectHelp(executable, args, label) {
  const result = invoke(executable, args)
  if (result.exitCode !== 0 || result.stdout.toString() !== expectedHelp ||
      result.stderr.length !== 0) {
    fail(`${label} is not exact: ${JSON.stringify(result)}`)
  }
}

function expectInvalidInvocation(executable, args, label) {
  const result = invoke(executable, args)
  if (result.exitCode !== 2 || result.stdout.length !== 0 ||
      result.stderr.replace(/\r\n/gu, "\n") !== expectedHelp) {
    fail(`${label} did not fail with usage only: ${JSON.stringify({
      exitCode: result.exitCode,
      stdout: result.stdout.toString(),
      stderr: result.stderr,
    })}`)
  }
}

function expectBarrier(executable, args, label, expectedMessage) {
  const result = invoke(executable, args)
  if (result.exitCode !== 2 || result.stdout.length !== 0 ||
      result.stderr.length === 0 ||
      (expectedMessage !== undefined && !result.stderr.includes(expectedMessage))) {
    fail(`${label} did not fail closed: ${JSON.stringify({
      exitCode: result.exitCode,
      stdout: result.stdout.toString(),
      stderr: result.stderr,
    })}`)
  }
}

function expectClean(result, label) {
  if (result.exitCode !== 0 || result.stdout.length !== 0 ||
      result.stderr.length !== 0) {
    fail(`${label} was not clean: ${JSON.stringify(result)}`)
  }
}

function expectDiagnostic(result, source, label) {
  if (result.exitCode !== 1 || result.stderr.length !== 0) {
    fail(`${label} has the wrong exit or stderr: ${JSON.stringify({
      exitCode: result.exitCode,
      stderr: result.stderr,
    })}`)
  }
  const lines = result.stdout.toString().split("\n")
  if (lines.length !== 2 || lines[1] !== "") {
    fail(`${label} is not one JSONL record: ${JSON.stringify(result.stdout.toString())}`)
  }
  let record
  try {
    record = JSON.parse(lines[0])
  } catch (error) {
    fail(`${label} is not JSON: ${error}`)
  }
  if (JSON.stringify(record) !== lines[0]) {
    fail(`${label} changed canonical field order`)
  }
  const expected = {
    schemaVersion: 1,
    instance: "D000001",
    code: "W-SEM-0001",
    phase: "semantic.type",
    severity: "error",
    primary: { source, startByte: 300, endByte: 302 },
    labels: [],
    facts: { actual: "1", expected: "Bool" },
    notes: [],
    fixes: [],
    root: null,
  }
  if (JSON.stringify(record) !== JSON.stringify(expected)) {
    fail(`${label} differs: ${JSON.stringify(record)}`)
  }
  return lines[0]
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-check-cli-"))
const invalidSource = join(buildDirectory, "invalid-utf8.w")
const incompleteSource = join(buildDirectory, "incomplete.w")
const importSource = join(buildDirectory, "import.w")
const unsupportedSource = join(buildDirectory, "unsupported.w")
const oversizedSource = join(buildDirectory, "oversized.w")
const missingSource = join(buildDirectory, "missing.w")
const mutationSource = join(buildDirectory, "checker-bootstrap-negative.w")
const nestedDirectory = join(buildDirectory, "nested", "logical")
const headerlessNestedSource = join(nestedDirectory, "headerless.w")
const headerOverrideNestedSource = join(nestedDirectory, "header-override.w")
const emptyStemSource = join(nestedDirectory, ".w")
try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory, "--target", "w", "--", "-j", "2"])
  const extension = process.platform === "win32" ? ".exe" : ""
  const executable = join(buildDirectory, `w${extension}`)
  if (!existsSync(executable) || basename(executable) !== `w${extension}`) {
    fail(`target/executable is not named w${extension}`)
  }

  await mkdir(nestedDirectory, { recursive: true })
  await writeFile(headerlessNestedSource,
    Buffer.from("export enum State { open }\n", "utf8"))
  await writeFile(headerOverrideNestedSource,
    Buffer.from("module explicit_header\nexport enum State { open }\n", "utf8"))
  await writeFile(emptyStemSource,
    Buffer.from("export enum State { open }\n", "utf8"))

  expectHelp(executable, ["--help"], "w --help")
  expectHelp(executable, ["help"], "w help")
  expectHelp(executable, ["check", "--help"], "w check --help")

  const clean = invoke(executable, ["check", "reference/last-light/checker_bootstrap.w"])
  expectClean(clean, "relative Restaurant fixture")
  const cleanJson = invoke(executable, ["check", "reference/last-light/checker_bootstrap.w", "--json"])
  expectClean(cleanJson, "relative Restaurant fixture JSON")
  expectClean(invoke(executable, ["check", headerlessNestedSource]),
              "nested headerless fixture")
  const headerOverridePath = headerOverrideNestedSource.replaceAll("\\", "/")
  expectClean(invoke(executable, ["check", headerOverridePath]),
              "nested header override fixture")
  expectBarrier(executable, ["check", emptyStemSource, "--json"],
                "empty logical module stem", "logical module name is empty")
  expectBarrier(executable, ["check", missingSource, "--json"], "missing source file")

  const fixtureBytes = Buffer.from(await Bun.file(fixture).arrayBuffer())
  const mutationBytes = Buffer.concat([
    fixtureBytes,
    Buffer.from("\nfn invalidOrderAdmission() {\n  if 1 { return }\n}", "utf8"),
  ])
  await writeFile(mutationSource, mutationBytes)
  const first = invoke(executable, ["check", mutationSource, "--json"])
  const second = invoke(executable, ["check", mutationSource, "--json"])
  const firstRecord = expectDiagnostic(first, mutationSource, "mutation JSON first run")
  const secondRecord = expectDiagnostic(second, mutationSource, "mutation JSON second run")
  if (firstRecord !== secondRecord) fail("mutation JSON is not byte-identical across runs")

  const human = invoke(executable, ["check", mutationSource])
  if (human.exitCode !== 1 || human.stdout.length !== 0 ||
      human.stderr.replace(/\r\n/gu, "\n") !==
        `${mutationSource}:20:6:W-SEM-0001: actual=1 expected=Bool\n`) {
    fail(`human diagnostic is not stable: ${JSON.stringify(human)}`)
  }

  for (const [args, label] of [
    [["unknown"], "unknown command"],
    [["check", "--json", mutationSource], "prefix-json before path"],
    [["--json", "check", mutationSource], "prefix-json before command"],
    [["check", mutationSource, "--json", "--json"], "duplicate --json"],
    [["check", mutationSource, "extra"], "extra argument"],
    [["check"], "missing path"],
    [["check", ""], "empty path"],
  ]) {
    expectInvalidInvocation(executable, args, label)
  }

  await writeFile(invalidSource, Buffer.from([0xc3]))
  await writeFile(incompleteSource, Buffer.from("fn broken(\n", "utf8"))
  await writeFile(importSource,
    Buffer.from("module import_gate\nimport text from std\nexport enum State { open }\n", "utf8"))
  await writeFile(unsupportedSource,
    Buffer.from("const duration: PhysicalDuration = 10<si.s>\nstruct Use {}\n", "utf8"))
  await writeFile(oversizedSource, Buffer.alloc(sourceCapacity + 1, 0x20))
  for (const [path, label] of [
    [invalidSource, "invalid UTF-8"],
    [incompleteSource, "incomplete parse"],
    [importSource, "import or module graph"],
    [unsupportedSource, "unsupported frontend"],
    [oversizedSource, "source capacity"],
  ]) {
    const expectedMessage = label === "import or module graph"
      ? "imports require module graph resolution"
      : undefined
    expectBarrier(executable, ["check", path, "--json"], label, expectedMessage)
  }
  console.log("w check cli: help, bounded Restaurant check, deterministic D0, human renderer, and fail-closed barriers passed")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
