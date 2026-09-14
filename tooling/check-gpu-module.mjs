import { mkdtemp, readFile, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seed = resolve(root, "compiler", "seed-c")
const fixture = resolve(seed, "fixtures", "gpu0-module.w")
const cmake = Bun.which("cmake")
const ninja = Bun.which("ninja")
const compiler = ["cc", "gcc", "clang", "cl"]
  .map((name) => Bun.which(name))
  .find(Boolean)

if (!cmake || !ninja || !compiler) {
  const missing = [
    ["cmake", cmake],
    ["ninja", ninja],
    ["C compiler", compiler],
  ].filter(([, value]) => !value).map(([name]) => name)
  console.log(`GPU module bridge: SKIP toolchain unavailable (${missing.join(", ")})`)
  process.exit(0)
}

function fail(message) {
  throw new Error(`GPU module bridge: ${message}`)
}

function run(command, args, env) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    env,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (result.exitCode !== 0) {
    const detail = result.stderr.toString().trim() || result.stdout.toString().trim()
    fail(`${command} ${args.join(" ")} failed${detail ? `: ${detail}` : ""}`)
  }
  return result
}

async function assertCompiler(buildDirectory) {
  const cache = await readFile(join(buildDirectory, "CMakeCache.txt"), "utf8")
  const match = cache.match(/^CMAKE_C_COMPILER:FILEPATH=(.*)$/mu)
  const normalize = (value) => value.replaceAll("\\", "/").toLowerCase()
  if (!match || normalize(match[1]) !== normalize(compiler)) {
    fail("configure did not retain the selected compiler")
  }
  if (!/^W_SEED_C_STANDARD:STRING=23$/mu.test(cache)) {
    fail("configure did not retain the primary C23 mode")
  }
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-gpu-module-"))
const environment = { ...process.env, CC: compiler }
try {
  run(cmake, ["-S", seed, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release", "-DW_SEED_C_STANDARD=23"], environment)
  await assertCompiler(buildDirectory)
  run(cmake, ["--build", buildDirectory, "--target", "w_seed_frontend_tests",
    "--parallel", "2"], environment)
  const suffix = process.platform === "win32" ? ".exe" : ""
  const result = run(resolve(buildDirectory, `w_seed_frontend_tests${suffix}`),
    [fixture], environment)
  if (result.stdout.length !== 0 || result.stderr.length !== 0) {
    fail("integration witness produced unexpected output")
  }
  process.stdout.write(
    "GPU module bridge: W source -> Frontend27 -> verified gpu-module -> GPU0 projection -> verified device artifact passed\n",
  )
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
