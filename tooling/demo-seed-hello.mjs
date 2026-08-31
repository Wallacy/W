import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { probeCDialect } from "./c-dialect.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const fixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")

function fail(message) {
  throw new Error(`seed demo: ${message}`)
}

function run(command, args, cwd = root, env = undefined) {
  return Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    env,
    stdout: "pipe",
    stderr: "pipe",
  })
}

function runRequired(label, command, args, cwd, env) {
  const result = run(command, args, cwd, env)
  if (result.exitCode !== 0) {
    const detail = result.stderr.toString().trim() || result.stdout.toString().trim()
    fail(`${label} failed${detail ? `: ${detail.slice(-2000)}` : ""}`)
  }
  return result
}

const cmake = Bun.which("cmake")
const ninja = Bun.which("ninja")
const compiler = ["cc", "gcc", "clang", "cl"].map((name) => Bun.which(name)).find(Boolean)
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
  console.log(`seed demo: SKIP ${reason}`)
  process.exit(0)
}

const seedBuildDirectory = await mkdtemp(join(tmpdir(), "w-seed-hello-build-"))
const artifactDirectory = await mkdtemp(join(tmpdir(), "w-seed-hello-artifact-"))
const environment = { ...process.env, CC: compiler }
try {
  runRequired("seed configure", cmake, [
    "-S", seedDirectory, "-B", seedBuildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
  ], root, environment)
  const cache = await readFile(join(seedBuildDirectory, "CMakeCache.txt"), "utf8")
  if (!/^W_SEED_C_STANDARD:STRING=23$/mu.test(cache)) {
    fail("seed configure did not retain the C23 default")
  }
  runRequired("seed HLO1 build", cmake, [
    "--build", seedBuildDirectory, "--target", "w_seed_hlo1_gate", "--parallel", "2",
  ], root, environment)

  const suffix = process.platform === "win32" ? ".exe" : ""
  const gate = join(seedBuildDirectory, `w_seed_hlo1_gate${suffix}`)
  const generated = runRequired("source to HLO1 route", gate, [fixture], root)
  if (generated.stderr.length !== 0 || generated.stdout.length === 0) {
    fail("source to HLO1 route did not emit a C artifact without stderr")
  }
  await writeFile(join(artifactDirectory, "generated.c"), generated.stdout)
  await writeFile(join(artifactDirectory, "CMakeLists.txt"), `cmake_minimum_required(VERSION 3.21)
project(w_seed_hello_demo C)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_executable(w_seed_hello_demo generated.c)
if(MSVC)
  target_compile_options(w_seed_hello_demo PRIVATE /W4 /WX)
else()
  target_compile_options(w_seed_hello_demo PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)
endif()
`)
  const artifactBuildDirectory = join(artifactDirectory, "build")
  runRequired("generated C configure", cmake, [
    "-S", artifactDirectory, "-B", artifactBuildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
  ], root, environment)
  runRequired("generated C build", cmake, [
    "--build", artifactBuildDirectory, "--parallel", "2",
  ], root, environment)
  const execution = runRequired("generated seed demo", join(artifactBuildDirectory, `w_seed_hello_demo${suffix}`), [], root)
  if (execution.stderr.length !== 0 || !Buffer.from(execution.stdout).equals(Buffer.from("Hello, world!\n"))) {
    fail("generated seed demo stdout is not exactly Hello, world! LF")
  }
  process.stdout.write(execution.stdout)
} finally {
  await rm(seedBuildDirectory, { recursive: true, force: true })
  await rm(artifactDirectory, { recursive: true, force: true })
}
