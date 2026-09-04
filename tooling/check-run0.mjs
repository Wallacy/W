import { existsSync } from "node:fs"
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { basename, join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const canonicalFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const expectedOutput = Buffer.from("Hello, world!\n", "utf8")
const expectedPublicHelp =
  "usage: w check <path/file.w> [--json]\n" +
  "usage: w run <path/file.w> [-- <args...>]\n"
const expectedGateUsage = "usage: w_seed_run0_gate <path/file.w>\n"
const acquisitionFailure =
  "w_seed_run0_gate: source is missing, unreadable, empty, or over 4096 bytes\n"
const parseFailure =
  "w_seed_run0_gate: source was rejected by UTF-8 or parser validation\n"
const frontendFailure =
  "w_seed_run0_gate: source was rejected by frontend validation\n"
const hir0Failure =
  "w_seed_run0_gate: source was rejected by HIR0 validation\n"
const hlo0Unsupported =
  "w_seed_run0_gate: source is unsupported by the HLO0 seed subset\n"
const outputFailure = "w_seed_run0_gate: output write or flush failed\n"

function fail(message) {
  throw new Error("RUN0 gate: " + message)
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

function invoke(executable, args, environment = {}) {
  const execution = Bun.spawnSync({
    cmd: [executable, ...args],
    cwd: root,
    env: {
      ...process.env,
      W_SEED_RUN0_GATE_FAULT: "",
      ...environment,
    },
    stdout: "pipe",
    stderr: "pipe",
  })
  return {
    exitCode: execution.exitCode,
    stdout: Buffer.from(execution.stdout),
    stderr: Buffer.from(execution.stderr),
  }
}

function normalize(bytes) {
  return bytes.toString().replace(/\r\n/gu, "\n")
}

function resultSummary(result) {
  return JSON.stringify({
    exitCode: result.exitCode,
    stdout: result.stdout.toString(),
    stderr: result.stderr.toString(),
  })
}

function expectHelp(executable, args, expected, label) {
  const result = invoke(executable, args)
  if (result.exitCode !== 0 || result.stdout.toString() !== expected ||
      result.stderr.length !== 0) {
    fail(label + " is not exact: " + resultSummary(result))
  }
}

function expectUsageFailure(executable, args, expected, label) {
  const result = invoke(executable, args)
  if (result.exitCode !== 2 || result.stdout.length !== 0 ||
      normalize(result.stderr) !== expected) {
    fail(label + " did not fail with exact usage: " + resultSummary(result))
  }
}

function expectSourceFailure(executable, path, expected, label) {
  const result = invoke(executable, [path])
  if (result.exitCode !== 2 || result.stdout.length !== 0 ||
      normalize(result.stderr) !== expected) {
    fail(label + " did not fail at the expected stage: " +
      resultSummary(result))
  }
}

function expectExactGate(executable, path, expected, label) {
  const result = invoke(executable, [path])
  if (result.exitCode !== 0 || !result.stdout.equals(expected) ||
      result.stderr.length !== 0) {
    fail(label + " is not exact: " + resultSummary(result))
  }
  return result.stdout
}

function expectInternalFailure(executable, path, fault, label) {
  const result = invoke(executable, [path], {
    W_SEED_RUN0_GATE_FAULT: fault,
  })
  if (result.exitCode !== 3 || result.stdout.length !== 0 ||
      result.stderr.length === 0) {
    fail(label + " did not report an internal fault: " + resultSummary(result))
  }
}

function expectInternalEffect(executable, path, fault, expectedStdout, label) {
  const result = invoke(executable, [path], {
    W_SEED_RUN0_GATE_FAULT: fault,
  })
  if (result.exitCode !== 3 || !result.stdout.equals(expectedStdout) ||
      normalize(result.stderr) !== outputFailure) {
    fail(label + " did not preserve the reported output effect: " +
      resultSummary(result))
  }
}

function expectUnit(executable, label) {
  const result = invoke(executable, [])
  if (result.exitCode !== 0 || result.stderr.length !== 0) {
    fail(label + " failed: " + resultSummary(result))
  }
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-run0-gate-"))

try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory, "--target", "w",
    "w_seed_run0_gate", "w_seed_run0_tests", "w_seed_cli_io_tests", "--",
    "-j", "2"])

  const extension = process.platform === "win32" ? ".exe" : ""
  const executable = join(buildDirectory, "w" + extension)
  const run0Gate = join(buildDirectory, "w_seed_run0_gate" + extension)
  const run0Unit = join(buildDirectory, "w_seed_run0_tests" + extension)
  const cliIoUnit = join(buildDirectory, "w_seed_cli_io_tests" + extension)
  if (!existsSync(executable) || basename(executable) !== "w" + extension) {
    fail("target/executable is not named w" + extension)
  }
  if (!existsSync(run0Gate) ||
      basename(run0Gate) !== "w_seed_run0_gate" + extension) {
    fail("internal target/executable is not named w_seed_run0_gate" + extension)
  }
  expectUnit(run0Unit, "RUN0 unit")
  expectUnit(cliIoUnit, "CLI I/O unit")

  expectHelp(executable, ["--help"], expectedPublicHelp, "w --help")
  expectHelp(executable, ["help"], expectedPublicHelp, "w help")
  expectHelp(executable, ["check", "--help"], expectedPublicHelp,
    "w check --help")
  expectHelp(executable, ["run", "--help"], expectedPublicHelp,
    "w run --help")
  expectUsageFailure(executable, ["run", "--entry", canonicalFixture],
    expectedPublicHelp, "w run --entry remains unavailable")
  expectUsageFailure(executable, ["run", "--offline", canonicalFixture],
    expectedPublicHelp, "w run --offline remains unavailable")
  expectUsageFailure(executable, ["run", canonicalFixture, "arg"],
    expectedPublicHelp, "w run requires -- before program arguments")

  const canonicalFirst = expectExactGate(run0Gate, canonicalFixture,
    expectedOutput, "canonical source first run")
  const canonicalSecond = expectExactGate(run0Gate, canonicalFixture,
    expectedOutput, "canonical source second run")
  if (!canonicalFirst.equals(canonicalSecond)) {
    fail("canonical output changed between runs")
  }
  for (const [fault, label] of [
    ["hlo-forgery", "forged HLO0 after successful lowering"],
    ["run0-invalid", "RUN0 invalid after shared verification"],
    ["run0-alias", "RUN0 alias after shared verification"],
    ["sink-reject", "RUN0 sink rejection"],
  ]) {
    expectInternalFailure(run0Gate, canonicalFixture, fault, label)
  }
  expectInternalEffect(run0Gate, canonicalFixture, "sink-short-write",
    Buffer.from("Hello", "utf8"), "RUN0 stdout short write")
  expectInternalEffect(run0Gate, canonicalFixture, "sink-flush-failure",
    expectedOutput, "RUN0 stdout flush failure after full acceptance")

  const restaurantPath = join(buildDirectory, "restaurant.w")
  const emptyPath = join(buildDirectory, "empty.w")
  await writeFile(restaurantPath,
    "fn serve() { let message = \"Table 42 remains open\" print(message) }\nentry(serve)\n")
  await writeFile(emptyPath, "fn main() { print(\"\") }\nentry(main)\n")
  expectExactGate(run0Gate, restaurantPath,
    Buffer.from("Table 42 remains open\n", "utf8"),
    "Restaurant payload source")
  expectExactGate(run0Gate, emptyPath, Buffer.from("\n", "utf8"),
    "empty payload source")

  const sources = new Map([
    ["whitespace_comments.w",
      "// leading comment\n" +
      "fn  main ( ) {\n  // call comment\n" +
      "  print ( \"Hello, world!\" )\n}\n\nentry ( main )\n"],
    ["comment_only.w",
      "// fn main() { print(\"Hello, world!\") }\n" +
      "/* entry(main) */\n"],
    ["shadow_print.w",
      "fn print(message: String) {}\n" +
      "fn main() { print(\"Hello, world!\") }\nentry(main)\n"],
    ["noop_payload.w", "fn main() { noop() }\nentry(main)\n"],
    ["comment_with_print.w",
      "fn main() { noop(\"Other\") } // print(\"Hello, world!\")\nentry(main)\n"],
    ["two_calls.w",
      "fn main() { print(\"a\")\nprint(\"b\") }\nentry(main)\n"],
    ["outside_subset.w",
      "fn main(value: String) { print(value) }\nentry(main)\n"],
    ["var_binding.w",
      "fn main() { var message = \"Hello, world!\" print(message) }\nentry(main)\n"],
    ["qualified_call.w",
      "fn main() { console.print(\"Hello, world!\") }\nentry(main)\n"],
    ["imported_call.w",
      "import { print } from console\n" +
      "fn main() { print(\"Hello, world!\") }\nentry(main)\n"],
    ["extra_entry.w",
      "fn main() { print(\"Hello, world!\") }\n" +
      "entry(main)\nentry(main)\n"],
    ["missing_entry.w",
      "fn main() { print(\"Hello, world!\") }\n"],
    ["extra_function.w",
      "fn helper() {}\n" +
      "fn main() { print(\"Hello, world!\") }\nentry(main)\n"],
    ["async_main.w",
      "async fn main() { print(\"Hello, world!\") }\nentry(main)\n"],
    ["throws_main.w",
      "fn main(): () throws String { print(\"Hello, world!\") }\n" +
      "entry(main)\n"],
    ["incomplete.w", "fn main(\n"],
  ])

  for (const [name, source] of sources) {
    await writeFile(join(buildDirectory, name), Buffer.from(source, "utf8"))
  }
  await writeFile(join(buildDirectory, "invalid_utf8.w"), Buffer.from([0xc3]))
  await writeFile(join(buildDirectory, "empty.w"), Buffer.alloc(0))

  const canonicalBytes = await readFile(canonicalFixture)
  const paddingPrefix = Buffer.concat([
    canonicalBytes,
    Buffer.from("//", "utf8"),
  ])
  if (paddingPrefix.length > 4096) fail("canonical padding prefix is too large")
  const exactLimit = Buffer.concat([
    paddingPrefix,
    Buffer.alloc(4096 - paddingPrefix.length, 0x70),
  ])
  const exactLimitPath = join(buildDirectory, "exact_4096.w")
  const overLimitPath = join(buildDirectory, "over_4096.w")
  await writeFile(exactLimitPath, exactLimit)
  await writeFile(overLimitPath,
    Buffer.concat([exactLimit, Buffer.from("p", "utf8")]))
  if ((await readFile(exactLimitPath)).length !== 4096 ||
      (await readFile(overLimitPath)).length !== 4097) {
    fail("source limit fixtures have the wrong byte count")
  }

  expectExactGate(run0Gate, join(buildDirectory, "whitespace_comments.w"),
    expectedOutput, "whitespace and comments")
  expectExactGate(run0Gate, exactLimitPath, expectedOutput,
    "exact 4096-byte source")

  for (const [name, expected, label] of [
    ["comment_only.w", hir0Failure, "comment-only source"],
    ["shadow_print.w", frontendFailure, "shadowed print"],
    ["noop_payload.w", hlo0Unsupported, "noop payload"],
    ["comment_with_print.w", frontendFailure, "comment with print"],
    ["two_calls.w", hlo0Unsupported, "two calls outside HLO0 subset"],
    ["outside_subset.w", hir0Failure, "outside HLO0 subset"],
    ["var_binding.w", hir0Failure, "mutable binding"],
    ["qualified_call.w", frontendFailure, "qualified call"],
    ["imported_call.w", frontendFailure, "imported call"],
    ["extra_entry.w", parseFailure, "extra entry"],
    ["missing_entry.w", hir0Failure, "missing entry"],
    ["extra_function.w", hlo0Unsupported, "extra function"],
    ["async_main.w", hlo0Unsupported, "async main"],
    ["throws_main.w", hlo0Unsupported, "throws main"],
    ["invalid_utf8.w", parseFailure, "invalid UTF-8"],
    ["incomplete.w", parseFailure, "incomplete parse"],
    ["empty.w", acquisitionFailure, "empty source"],
  ]) {
    expectSourceFailure(run0Gate, join(buildDirectory, name), expected, label)
  }
  expectSourceFailure(run0Gate, join(buildDirectory, "missing.w"),
    acquisitionFailure, "missing source")
  expectSourceFailure(run0Gate, overLimitPath, acquisitionFailure,
    "4097-byte source")

  for (const [args, label] of [
    [[], "missing gate path"],
    [[canonicalFixture, "extra"], "extra gate argument"],
    [[canonicalFixture, "--json"], "gate JSON suffix"],
    [["--json", canonicalFixture], "gate JSON prefix"],
  ]) {
    expectUsageFailure(run0Gate, args, expectedGateUsage, label)
  }

  console.log("RUN0 gate: test-only source pipeline, bounded failures, " +
    "public run argument surface, RUN0 effects, and I/O helper passed")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
