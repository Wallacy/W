import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { dialectDisclosure, probeCDialect } from "./c-dialect.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const canonicalFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")

function fail(message) {
  throw new Error(`HLO1: ${message}`)
}

function tool(name) {
  return Bun.which(name)
}

function run(command, args, cwd = root, env = undefined) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    env,
    stdout: "pipe",
    stderr: "pipe",
  })
  return {
    ...result,
    stdoutText: result.stdout.toString(),
    stderrText: result.stderr.toString(),
  }
}

function runChecked(command, args, cwd, label, env = undefined) {
  const result = run(command, args, cwd, env)
  if (result.exitCode !== 0) {
    const details = result.stderrText.trim() || result.stdoutText.trim()
    fail(`${label} failed${details ? `: ${details.slice(-2000)}` : ""}`)
  }
  return result
}

function assert(condition, message) {
  if (!condition) fail(message)
}

async function assertCompiler(cacheDirectory, label) {
  const cache = await readFile(join(cacheDirectory, "CMakeCache.txt"), "utf8")
  const match = cache.match(/^CMAKE_C_COMPILER:FILEPATH=(.*)$/m)
  const normalize = (value) => value.replaceAll("\\", "/").toLowerCase()
  assert(match && normalize(match[1]) === normalize(compiler),
    `${label} did not use the selected compiler`)
  if (label.includes("seed")) {
    assert(/^W_SEED_C_STANDARD:STRING=23$/mu.test(cache),
      `${label} did not retain the C23 seed standard`)
  }
}

const cmake = tool("cmake")
const ninja = tool("ninja")
const compiler = ["cc", "gcc", "clang", "cl"].map(tool).find(Boolean)
const dialect = compiler ? await probeCDialect(compiler) : undefined
if (!cmake || !ninja || !compiler || !dialect) {
  const missing = [
    ["cmake", cmake],
    ["ninja", ninja],
    ["C compiler", compiler],
  ].filter(([, value]) => !value).map(([name]) => name)
  const reason = missing.length > 0
    ? `toolchain unavailable (${missing.join(", ")})`
    : "C23 dialect unavailable (C11 recovery is explicit only)"
  console.log(`HLO1: SKIP ${reason}`)
  process.exit(0)
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-hlo1-seed-"))
const artifactDirectory = await mkdtemp(join(tmpdir(), "w-hlo1-artifact-"))
const toolchainEnvironment = { ...process.env, CC: compiler }
try {
  runChecked(cmake, ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release"], root, "seed configure",
  toolchainEnvironment)
  await assertCompiler(buildDirectory, "seed configure")
  runChecked(cmake, ["--build", buildDirectory, "--target",
    "w_seed_hlo1_tests", "w_seed_hlo1_gate", "--parallel", "2"], root,
  "seed build", toolchainEnvironment)

  const suffix = process.platform === "win32" ? ".exe" : ""
  const unit = run(resolve(buildDirectory, `w_seed_hlo1_tests${suffix}`), [])
  assert(unit.exitCode === 0, `unit tests failed: ${unit.stderrText.trim()}`)
  assert(unit.stderr.length === 0, "unit tests wrote to stderr")
  assert(unit.stdoutText.includes("deterministic conservative C emitter"),
    "unit test witness is missing")
  const seedGate = resolve(buildDirectory, `w_seed_hlo1_gate${suffix}`)
  const canonical = run(seedGate, [canonicalFixture])
  assert(canonical.exitCode === 0,
    `canonical source route failed: ${canonical.stderrText.trim()}`)
  assert(canonical.stderr.length === 0, "canonical route wrote to stderr")
  assert(canonical.stdout.length > 0, "canonical route emitted no C bytes")
  await writeFile(join(artifactDirectory, "generated.c"), canonical.stdout)
  await writeFile(join(artifactDirectory, "CMakeLists.txt"), `cmake_minimum_required(VERSION 3.21)
project(w_hlo1_generated C)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_executable(w_hlo1_generated generated.c)
if(MSVC)
  target_compile_options(w_hlo1_generated PRIVATE /W4 /WX)
else()
  target_compile_options(w_hlo1_generated PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)
endif()
`)
  const artifactBuildDirectory = resolve(artifactDirectory, "build")
  runChecked(cmake, ["-S", artifactDirectory, "-B", artifactBuildDirectory,
    "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"], root,
  "generated C configure", toolchainEnvironment)
  await assertCompiler(artifactBuildDirectory, "generated C configure")
  runChecked(cmake, ["--build", artifactBuildDirectory, "--parallel", "2"],
    root, "generated C build", toolchainEnvironment)
  const generated = resolve(artifactBuildDirectory,
    `w_hlo1_generated${suffix}`)
  const execution = run(generated, [])
  assert(execution.exitCode === 0,
    `generated program exit=${execution.exitCode}`)
  assert(execution.stderr.length === 0, "generated program wrote to stderr")
  assert(Buffer.from(execution.stdout).equals(Buffer.from("Hello, world!\n")),
    "generated program stdout is not exactly Hello, world! LF")

  const commentedPath = resolve(artifactDirectory, "commented-canonical.w")
  await writeFile(commentedPath,
    "// source comment\nfn main() {   print(\"Hello, world!\")   }\n\nentry(main)\n")
  const commented = run(seedGate, [commentedPath])
  assert(commented.exitCode === 0,
    `commented canonical route failed: ${commented.stderrText.trim()}`)
  assert(commented.stderr.length === 0, "commented route wrote to stderr")
  assert(Buffer.from(commented.stdout).equals(Buffer.from(canonical.stdout)),
    "commented canonical route changed the deterministic conservative C artifact")

  const adversarial = [
    ["restaurant-comment.w",
      `fn main() { noop("Other") } // print("Hello, world!")\nentry(main)\n`],
    ["restaurant-payload.w",
      `fn main() { print("Other") } // Hello, world! menu note\nentry(main)\n`],
  ]
  for (const [name, source] of adversarial) {
    const path = resolve(artifactDirectory, name)
    await writeFile(path, source)
    const rejected = run(seedGate, [path])
    assert(rejected.exitCode !== 0, `${name} was accepted`)
    assert(rejected.stdout.length === 0,
      `${name} produced partial C output`)
  }
  console.log(`HLO1: verified-HIR conservative C artifact emission and execution passed (${dialectDisclosure(dialect)})`)
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
  await rm(artifactDirectory, { recursive: true, force: true })
}
