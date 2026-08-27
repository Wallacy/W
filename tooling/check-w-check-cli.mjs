import { existsSync } from "node:fs"
import {
  mkdir,
  mkdtemp,
  readFile,
  rm,
  symlink,
  writeFile,
} from "node:fs/promises"
import { tmpdir } from "node:os"
import { basename, join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const fixture = resolve(root, "reference", "last-light", "checker_bootstrap.w")
const expectedHelp = "usage: w check <path/file.w> [--json]\n"
const sourceCapacity = 16 * 1024 * 1024

function fail(message) {
  throw new Error("w check cli: " + message)
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

function invoke(executable, args, cwd = root) {
  const execution = Bun.spawnSync({
    cmd: [executable, ...args],
    cwd,
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

function expectHelp(executable, args, label) {
  const result = invoke(executable, args)
  if (result.exitCode !== 0 || result.stdout.toString() !== expectedHelp ||
      result.stderr.length !== 0) {
    fail(label + " is not exact: " + JSON.stringify(result))
  }
}

function expectInvalidInvocation(executable, args, label) {
  const result = invoke(executable, args)
  if (result.exitCode !== 2 || result.stdout.length !== 0 ||
      normalize(result.stderr) !== expectedHelp) {
    fail(label + " did not fail with usage only: " + JSON.stringify({
      exitCode: result.exitCode,
      stdout: result.stdout.toString(),
      stderr: result.stderr,
    }))
  }
}

function expectBarrier(executable, args, label) {
  const result = invoke(executable, args)
  if (result.exitCode !== 2 || result.stdout.length !== 0 ||
      result.stderr.length === 0) {
    fail(label + " did not fail closed: " + JSON.stringify({
      exitCode: result.exitCode,
      stdout: result.stdout.toString(),
      stderr: result.stderr,
    }))
  }
}

function expectClean(result, label) {
  if (result.exitCode !== 0 || result.stdout.length !== 0 ||
      result.stderr.length !== 0) {
    fail(label + " was not clean: " + JSON.stringify(result))
  }
}

function expectDiagnostic(result, source, startByte, endByte, label) {
  if (result.exitCode !== 1 || result.stderr.length !== 0) {
    fail(label + " has the wrong exit or stderr: " + JSON.stringify({
      exitCode: result.exitCode,
      stderr: result.stderr,
    }))
  }
  const text = result.stdout.toString()
  const lines = text.split("\n")
  if (lines.length !== 2 || lines[1] !== "") {
    fail(label + " is not one JSONL record: " + JSON.stringify(text))
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
    fail(label + " differs: " + JSON.stringify(record))
  }
  return lines[0]
}

function expectMatchDiagnostic(result, source, startByte, endByte, label) {
  if (result.exitCode !== 1 || result.stderr.length !== 0) {
    fail(label + " has the wrong exit or stderr: " + JSON.stringify({
      exitCode: result.exitCode,
      stderr: result.stderr,
    }))
  }
  const text = result.stdout.toString()
  const lines = text.split("\n")
  if (lines.length !== 2 || lines[1] !== "") {
    fail(label + " is not one JSONL record: " + JSON.stringify(text))
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
    code: "W-MATCH-0001",
    phase: "semantic.flow",
    severity: "error",
    primary: { source, startByte, endByte },
    labels: [{
      role: "match-subject",
      span: { source, startByte: startByte + 7, endByte: startByte + 13 },
    }],
    facts: { missingCases: ["accepted", "reserving"], subjectType: "Stage" },
    notes: [],
    fixes: [],
    root: null,
  }
  if (JSON.stringify(record) !== JSON.stringify(expected)) {
    fail(label + " differs: " + JSON.stringify(record))
  }
  return lines[0]
}

function expectEmptyOutput(result, label) {
  if (result.stdout.length !== 0) fail(label + " wrote to stdout")
}

function expectHumanDiagnostic(result, path, bytes, startByte, label) {
  expectEmptyOutput(result, label + " stdout")
  const point = pointAt(bytes, startByte)
  const expected = path + ":" + point.line + ":" + point.column +
    ":W-SEM-0001: node does not satisfy its expected semantic use; " +
    "facts=actual=1, expected=Bool\n"
  if (result.exitCode !== 1 || normalize(result.stderr) !== expected) {
    fail(label + " is not stable: " + JSON.stringify(result))
  }
}

function expectMatchHumanDiagnostic(result, path, bytes, startByte, label) {
  expectEmptyOutput(result, label + " stdout")
  const point = pointAt(bytes, startByte)
  const labelPoint = pointAt(bytes, startByte + 7)
  const expected = path + ":" + point.line + ":" + point.column +
    ":W-MATCH-0001: required switch or catch does not cover its complete " +
    "proven domain; facts=missingCases=[accepted, reserving], " +
    "subjectType=Stage [match-subject " + path + ":" + labelPoint.line +
    ":" + labelPoint.column + "]\n"
  if (result.exitCode !== 1 || normalize(result.stderr) !== expected) {
    fail(label + " is not stable: " + JSON.stringify(result))
  }
}

async function writeUtf8(path, text) {
  await writeFile(path, Buffer.from(text, "utf8"))
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-check-cli-"))
const invalidSource = join(buildDirectory, "invalid_utf8.w")
const incompleteSource = join(buildDirectory, "incomplete.w")
const unsupportedSource = join(buildDirectory, "unsupported.w")
const oversizedSource = join(buildDirectory, "oversized.w")
const missingSource = join(buildDirectory, "missing.w")
  const mutationSource = join(buildDirectory, "checker_bootstrap_negative.w")
  const matchWitnessSource = join(buildDirectory, "match_witness.w")
const nestedDirectory = join(buildDirectory, "nested", "logical")
const headerlessNestedSource = join(nestedDirectory, "headerless.w")
const headerOverrideNestedSource = join(nestedDirectory, "header_override.w")
const emptyStemSource = join(nestedDirectory, ".w")
const hyphenSource = join(nestedDirectory, "bad-name.w")
const multiDotSource = join(nestedDirectory, "a.b.w")
const unicodeBasenameSource = join(nestedDirectory, "rót.w")
const unicodeDirectory = join(buildDirectory, "área")
const unicodeDirectorySource = join(unicodeDirectory, "unicode_root.w")

try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory, "--target", "w", "--", "-j", "2"])
  const extension = process.platform === "win32" ? ".exe" : ""
  const executable = join(buildDirectory, "w" + extension)
  if (!existsSync(executable) || basename(executable) !== "w" + extension) {
    fail("target/executable is not named w" + extension)
  }

  await mkdir(nestedDirectory, { recursive: true })
  await writeUtf8(headerlessNestedSource,
    "export enum State { open }\n")
  await writeUtf8(headerOverrideNestedSource,
    "module explicit_header\nexport enum State { open }\n")
  await writeUtf8(emptyStemSource, "export enum State { open }\n")
  await writeUtf8(hyphenSource, "export enum State { open }\n")
  await writeUtf8(multiDotSource, "export enum State { open }\n")
  await writeUtf8(unicodeBasenameSource, "export enum State { open }\n")
  await mkdir(unicodeDirectory, { recursive: true })
  await writeUtf8(unicodeDirectorySource,
    "export enum State { open }\n")

  expectHelp(executable, ["--help"], "w --help")
  expectHelp(executable, ["help"], "w help")
  expectHelp(executable, ["check", "--help"], "w check --help")

  expectInvalidInvocation(executable, ["unknown"], "unknown command")
  expectInvalidInvocation(executable, ["check", "--json",
    mutationSource], "prefix-json before path")
  expectInvalidInvocation(executable, ["--json", "check",
    mutationSource], "prefix-json before command")
  expectInvalidInvocation(executable, ["check", mutationSource,
    "--json", "--json"], "duplicate --json")
  expectInvalidInvocation(executable, ["check", mutationSource, "extra"],
    "extra argument")
  expectInvalidInvocation(executable, ["check"], "missing path")
  expectInvalidInvocation(executable, ["check", ""], "empty path")

  expectClean(invoke(executable,
    ["check", "reference/last-light/checker_bootstrap.w"]),
    "relative Restaurant fixture")
  expectClean(invoke(executable,
    ["check", "reference/last-light/checker_bootstrap.w", "--json"]),
    "relative Restaurant fixture JSON")
  expectClean(invoke(executable, ["check", headerlessNestedSource]),
    "nested headerless fixture")
  expectClean(invoke(executable, ["check", headerOverrideNestedSource]),
    "nested header override fixture")
  expectClean(invoke(executable, ["check", "unicode_root.w"],
    unicodeDirectory),
    "Unicode directory with ASCII basename")

  for (const [path, label] of [
    [emptyStemSource, "empty logical module stem"],
    [hyphenSource, "hyphen basename"],
    [multiDotSource, "multi-dot basename"],
    [unicodeBasenameSource, "Unicode basename"],
  ]) {
    expectBarrier(executable, ["check", path, "--json"], label)
  }

  const fixtureBytes = await readFile(fixture)
  const mutationSuffix = "\nfn invalidOrderAdmission() {\n  if 1 { return }\n}"
  const mutationBytes = Buffer.concat([
    fixtureBytes,
    Buffer.from(mutationSuffix, "utf8"),
  ])
  const mutationStart = fixtureBytes.length + Buffer.byteLength(
    "\nfn invalidOrderAdmission() {\n  if ", "utf8")
  await writeFile(mutationSource, mutationBytes)
  const mutationSourceId = basename(mutationSource)
  const first = invoke(executable, ["check", mutationSource, "--json"])
  const second = invoke(executable, ["check", mutationSource, "--json"])
  const firstRecord = expectDiagnostic(first, mutationSourceId, mutationStart,
    mutationStart + 2, "root diagnostic JSON first run")
  const secondRecord = expectDiagnostic(second, mutationSourceId, mutationStart,
    mutationStart + 2, "root diagnostic JSON second run")
  if (firstRecord !== secondRecord) {
    fail("root diagnostic JSON is not byte-identical across runs")
  }
  expectHumanDiagnostic(invoke(executable, ["check", mutationSource]),
    mutationSource, mutationBytes, mutationStart, "root physical human diagnostic")

  const matchWitnessText =
    "module match_witness\n" +
    "export enum Stage { accepted reserving preparing }\n" +
    "fn missing(stage: Stage): String { return switch stage { " +
    "case .preparing: \"P\" } }\n"
  const matchWitnessBytes = Buffer.from(matchWitnessText, "utf8")
  await writeFile(matchWitnessSource, matchWitnessBytes)
  const matchWitnessStart = Buffer.byteLength(
    "module match_witness\nexport enum Stage { accepted reserving preparing }\n" +
    "fn missing(stage: Stage): String { return ", "utf8")
  const matchWitnessEnd = matchWitnessStart + Buffer.byteLength(
    "switch stage { case .preparing: \"P\" }", "utf8")
  const matchWitnessSourceId = basename(matchWitnessSource)
  const matchFirst = invoke(executable,
    ["check", matchWitnessSource, "--json"])
  const matchSecond = invoke(executable,
    ["check", matchWitnessSource, "--json"])
  const matchRecordFirst = expectMatchDiagnostic(matchFirst,
    matchWitnessSourceId, matchWitnessStart, matchWitnessEnd,
    "MATCH-0001 JSON first run")
  const matchRecordSecond = expectMatchDiagnostic(matchSecond,
    matchWitnessSourceId, matchWitnessStart, matchWitnessEnd,
    "MATCH-0001 JSON second run")
  if (matchRecordFirst !== matchRecordSecond) {
    fail("MATCH-0001 JSON is not byte-identical across runs")
  }
  expectMatchHumanDiagnostic(invoke(executable,
    ["check", matchWitnessSource]), matchWitnessSource, matchWitnessBytes,
  matchWitnessStart, "MATCH-0001 physical human diagnostic")

  await mkdir(join(buildDirectory, "restaurant"), { recursive: true })
  const restaurantDirectory = join(buildDirectory, "restaurant")
  const restaurantRoot = join(restaurantDirectory, "restaurant.w")
  const restaurantMenu = join(restaurantDirectory, "menu.w")
  const unreachable = join(restaurantDirectory, "ignored.w")
  await writeUtf8(restaurantRoot,
    "module restaurant;\nimport { value } from menu\n" +
    "fn use(): i64 { return value() }\n")
  await writeUtf8(restaurantMenu,
    "module menu;\nexport fn value(): i64 { return 42 }\n")
  await writeFile(unreachable, Buffer.from([0xc3]))
  expectClean(invoke(executable, ["check", restaurantRoot]),
    "Restaurant multifile clean")
  expectClean(invoke(executable, ["check", restaurantRoot, "--json"]),
    "Restaurant multifile clean JSON")

  const diagnosticRoot = join(restaurantDirectory, "restaurant_diagnostic.w")
  const kitchenDirectory = join(restaurantDirectory, "kitchen")
  const kitchenMenu = join(kitchenDirectory, "menu.w")
  await mkdir(kitchenDirectory, { recursive: true })
  const childText = "module menu;\n" +
    "export fn bad(): i64 { if 1 { return 42 } return 0 }\n"
  const childBytes = Buffer.from(childText, "utf8")
  const childStart = Buffer.byteLength(
    "module menu;\nexport fn bad(): i64 { if ", "utf8")
  await writeUtf8(diagnosticRoot,
    "module restaurant_diagnostic;\n" +
    "import { bad } from kitchen.menu\n" +
    "fn use(): i64 { return bad() }\n")
  await writeFile(kitchenMenu, childBytes)
  const diagnosticSourceId = "kitchen/menu.w"
  const diagnosticJsonFirst = invoke(executable,
    ["check", diagnosticRoot, "--json"])
  const diagnosticJsonSecond = invoke(executable,
    ["check", diagnosticRoot, "--json"])
  const childRecordFirst = expectDiagnostic(diagnosticJsonFirst,
    diagnosticSourceId, childStart, childStart + 2,
    "Restaurant child diagnostic JSON first run")
  const childRecordSecond = expectDiagnostic(diagnosticJsonSecond,
    diagnosticSourceId, childStart, childStart + 2,
    "Restaurant child diagnostic JSON second run")
  if (childRecordFirst !== childRecordSecond) {
    fail("Restaurant child diagnostic JSON is not byte-identical")
  }
  const restaurantParent = diagnosticRoot.slice(0,
    diagnosticRoot.length - basename(diagnosticRoot).length)
  const physicalChildDisplay = restaurantParent + diagnosticSourceId
  expectHumanDiagnostic(invoke(executable, ["check", diagnosticRoot]),
    physicalChildDisplay, childBytes, childStart,
    "Restaurant child physical human diagnostic")

  const missingRoot = join(restaurantDirectory, "missing_root.w")
  const stdRoot = join(restaurantDirectory, "std_root.w")
  const cycleRoot = join(restaurantDirectory, "cycle_root.w")
  const cycleChild = join(restaurantDirectory, "cycle_child.w")
  await writeUtf8(missingRoot, "module missing_root;\nimport missing;\n")
  await writeUtf8(stdRoot, "module std_root;\nimport std.io;\n")
  await writeUtf8(cycleRoot, "module cycle_root;\nimport cycle_child;\n")
  await writeUtf8(cycleChild, "module cycle_child;\nimport cycle_root;\n")
  for (const [path, label] of [
    [missingRoot, "missing local module"],
    [stdRoot, "std.io import"],
    [cycleRoot, "root-child import cycle"],
  ]) {
    expectBarrier(executable, ["check", path, "--json"], label)
  }

  await writeFile(invalidSource, Buffer.from([0xc3]))
  await writeUtf8(incompleteSource, "fn broken(\n")
  await writeUtf8(unsupportedSource,
    "const duration: PhysicalDuration = 10<si.s>\nstruct Use {}\n")
  await writeFile(oversizedSource, Buffer.alloc(sourceCapacity + 1, 0x20))
  for (const [path, label] of [
    [invalidSource, "invalid UTF-8"],
    [incompleteSource, "incomplete parse"],
    [unsupportedSource, "unsupported frontend"],
    [oversizedSource, "source capacity"],
  ]) {
    expectBarrier(executable, ["check", path, "--json"], label)
  }

  const chainDirectory = join(buildDirectory, "chain")
  await mkdir(chainDirectory, { recursive: true })
  await writeUtf8(join(chainDirectory, "chain_root.w"),
    "module chain_root;\nimport node01;\n")
  for (let index = 1; index <= 64; index += 1) {
    const name = "node" + String(index).padStart(2, "0")
    const next = index === 64 ? "" : "import node" +
      String(index + 1).padStart(2, "0") + ";\n"
    await writeUtf8(join(chainDirectory, name + ".w"),
      "module " + name + ";\n" + next)
  }
  expectBarrier(executable,
    ["check", join(chainDirectory, "chain_root.w"), "--json"],
    "graph over 64 sources")

  const containmentStatus = "exercised"
  const outsideDirectory = await mkdtemp(join(tmpdir(), "w-check-cli-outside-"))
  const escapeLink = join(buildDirectory, "escape_link")
  const escapeRoot = join(buildDirectory, "escape_root.w")
  try {
    await writeUtf8(join(outsideDirectory, "child.w"),
      "module child;\nexport fn value(): i64 { return 42 }\n")
    await writeUtf8(escapeRoot,
      "module escape_root;\nimport escape_link.child;\n")
    try {
      if (process.platform === "win32") {
        await symlink(outsideDirectory, escapeLink, "junction")
      } else {
        await symlink(outsideDirectory, escapeLink, "dir")
      }
    } catch (error) {
      fail("cannot create " +
        (process.platform === "win32" ? "junction" : "symlink") +
        " containment fixture on " + process.platform +
        "; required capability is unavailable: " + String(error))
    }
    expectBarrier(executable, ["check", escapeRoot, "--json"],
      "symlink or junction directory escape")
  } finally {
    await rm(escapeLink, { force: true })
    await rm(outsideDirectory, { recursive: true, force: true })
  }

  console.log("w check cli: help, single/multifile Restaurant, logical JSON, " +
    "physical human, identity barriers, UTF-8/parse/frontend/source/graph " +
    "limits, and directory containment (" + containmentStatus + ") passed")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
