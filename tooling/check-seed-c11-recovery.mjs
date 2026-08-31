import { mkdtemp, readFile, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")

function fail(message) {
  throw new Error(`seed C11 recovery: ${message}`)
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
if (!cmake || !ninja || !compiler) {
  const missing = [
    ["cmake", cmake],
    ["ninja", ninja],
    ["C compiler", compiler],
  ].filter(([, value]) => !value).map(([name]) => name)
  console.log(`seed C11 recovery: SKIP toolchain unavailable (${missing.join(", ")})`)
  process.exit(0)
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-c11-recovery-"))
const environment = { ...process.env, CC: compiler }
try {
  runRequired("configure", cmake, [
    "-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug", "-DW_SEED_C_STANDARD=11",
  ], root, environment)
  const cache = await readFile(join(buildDirectory, "CMakeCache.txt"), "utf8")
  if (!/^W_SEED_C_STANDARD:STRING=11$/mu.test(cache)) {
    fail("configure did not retain explicit W_SEED_C_STANDARD=11")
  }
  runRequired("build", cmake, [
    "--build", buildDirectory, "--target",
    "w_seed_source_tests", "w_seed_hlo1_tests", "w_seed_hlo1_gate",
    "--parallel", "2",
  ], root, environment)

  const suffix = process.platform === "win32" ? ".exe" : ""
  for (const [name, expected] of [
    ["w_seed_source_tests", "w_seed_source_tests: ok\n"],
    ["w_seed_hlo1_tests", "seed HLO1: deterministic conservative C emitter and all-or-nothing barriers passed\n"],
  ]) {
    const result = run(join(buildDirectory, `${name}${suffix}`), [])
    const output = result.stdout.toString().replaceAll("\r\n", "\n")
    if (result.exitCode !== 0 || result.stderr.length !== 0 || output !== expected) {
      fail(`${name} did not pass the explicit recovery lane: ${JSON.stringify(output)}`)
    }
  }
  console.log("seed C11 recovery: explicit W_SEED_C_STANDARD=11 configure/build passed")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
