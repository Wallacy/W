import { mkdtemp, readFile, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const cmake = Bun.which("cmake")
const ninja = Bun.which("ninja")
const compiler = ["cc", "gcc", "clang", "cl"].map((name) => Bun.which(name)).find(Boolean)

if (!cmake || !ninja || !compiler) {
  const missing = [
    ["cmake", cmake],
    ["ninja", ninja],
    ["C compiler", compiler],
  ].filter(([, value]) => !value).map(([name]) => name)
  console.log(`HIR0: SKIP toolchain unavailable (${missing.join(", ")})`)
  process.exit(0)
}

function fail(message) {
  throw new Error(`HIR0: ${message}`)
}

function run(command, args, cwd = root, env = undefined) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    env,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (result.exitCode !== 0) {
    const details = result.stderr.toString().trim() || result.stdout.toString().trim()
    fail(`${command} ${args.join(" ")} failed${details ? `: ${details}` : ""}`)
  }
  return result.stdout.toString()
}

async function assertCompiler(buildDirectory, label) {
  const cache = await readFile(join(buildDirectory, "CMakeCache.txt"), "utf8")
  const match = cache.match(/^CMAKE_C_COMPILER:FILEPATH=(.*)$/m)
  const normalize = (value) => value.replaceAll("\\", "/").toLowerCase()
  if (!match || normalize(match[1]) !== normalize(compiler)) {
    fail(`${label} did not use the selected compiler`)
  }
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-hir0-"))
const toolchainEnvironment = { ...process.env, CC: compiler }
try {
  run(cmake, ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release"], root, toolchainEnvironment)
  await assertCompiler(buildDirectory, "seed configure")
  run(cmake, ["--build", buildDirectory, "--target", "w_seed_hir0_tests",
    "--parallel", "2"], root, toolchainEnvironment)
  const suffix = process.platform === "win32" ? ".exe" : ""
  const output = run(resolve(buildDirectory, `w_seed_hir0_tests${suffix}`), [])
  if (!output.includes("hir0 tests: ok")) fail("unit test witness is missing")
  process.stdout.write("HIR0: caller-owned verified HIR and adversarial barriers passed\n")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
