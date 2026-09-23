import { mkdtemp, readFile, realpath, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { isAbsolute, join, relative, resolve, sep } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const cmake = Bun.which("cmake")
const ninja = Bun.which("ninja")
const compiler = ["cc", "gcc", "clang", "cl"].map((name) => Bun.which(name)).find(Boolean)
const options = process.argv.slice(2)
if (options.length !== 0 &&
    (options.length !== 2 || options[0] !== "--build-dir" || !options[1])) {
  throw new Error("HIR0: usage: bun tooling/check-hir0.mjs [--build-dir <existing-repository-build>]")
}
const incremental = options.length === 2

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
  const sourceMatch = cache.match(/^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$/m)
  const generatorMatch = cache.match(/^CMAKE_GENERATOR:INTERNAL=(.*)$/m)
  const normalize = (value) => value.replaceAll("\\", "/").toLowerCase()
  if (!match || normalize(match[1]) !== normalize(compiler)) {
    fail(`${label} did not use the selected compiler`)
  }
  if (!sourceMatch || normalize(sourceMatch[1]) !== normalize(seedDirectory) ||
      !generatorMatch || generatorMatch[1] !== "Ninja") {
    fail(`${label} is not a Ninja build of this seed source`)
  }
}

async function existingBuildDirectory(argument) {
  const physicalRoot = await realpath(root)
  const physical = await realpath(resolve(root, argument))
  const within = relative(physicalRoot, physical)
  if (within === "" || within === ".." || within.startsWith(`..${sep}`) ||
      isAbsolute(within) || physical === seedDirectory) {
    fail("--build-dir must be an existing build inside this repository")
  }
  await assertCompiler(physical, "existing build")
  return physical
}

const buildDirectory = incremental
  ? await existingBuildDirectory(options[1])
  : await mkdtemp(join(tmpdir(), "w-hir0-"))
const toolchainEnvironment = { ...process.env, CC: compiler }
try {
  run(cmake, ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release"], root, toolchainEnvironment)
  await assertCompiler(buildDirectory, "seed configure")
  run(cmake, ["--build", buildDirectory, "--target", "w_seed_hir0_tests",
    "w_seed_hir0_multidoc_tests", "w_seed_product_closure0_tests",
    "w_seed_task_lifecycle0_tests",
    "--parallel", "2"], root,
  toolchainEnvironment)
  const suffix = process.platform === "win32" ? ".exe" : ""
  const output = run(resolve(buildDirectory, `w_seed_hir0_tests${suffix}`), [])
  if (!output.includes("hir0 tests: ok")) fail("unit test witness is missing")
  run(resolve(buildDirectory, `w_seed_hir0_multidoc_tests${suffix}`), [])
  run(resolve(buildDirectory, `w_seed_product_closure0_tests${suffix}`), [])
  const lifecycleOutput = run(
    resolve(buildDirectory, `w_seed_task_lifecycle0_tests${suffix}`), [],
  )
  if (!lifecycleOutput.includes("task_lifecycle0 tests: ok")) {
    fail("task lifecycle unit witness is missing")
  }
  process.stdout.write(
    "HIR0: caller-owned verified single/multi-document HIR, bounded product closure, measured task lifecycle, provider-bound success outcomes, typed Windows completions, and adversarial barriers passed\n",
  )
} finally {
  if (!incremental) await rm(buildDirectory, { recursive: true, force: true })
}
