import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const fixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const suffix = process.platform === "win32" ? ".exe" : ""

function fail(message) {
  throw new Error(`HLO0: ${message}`)
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
  console.log(`HLO0: SKIP toolchain unavailable (${missing.join(", ")})`)
  process.exit(0)
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-hlo0-"))

async function assertCompiler(buildDirectory, label) {
  const cache = await readFile(join(buildDirectory, "CMakeCache.txt"), "utf8")
  const match = cache.match(/^CMAKE_C_COMPILER:FILEPATH=(.*)$/m)
  const normalize = (value) => value.replaceAll("\\", "/").toLowerCase()
  if (!match || normalize(match[1]) !== normalize(compiler)) {
    fail(`${label} did not use the selected compiler`)
  }
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
    const stderr = result.stderr.toString().trim()
    fail(`${command} ${args.join(" ")} failed${stderr ? `: ${stderr}` : ""}`)
  }
  return result.stdout.toString()
}

function runRaw(command, args, cwd = root) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  })
  return {
    exitCode: result.exitCode,
    stdout: result.stdout,
    stderr: result.stderr,
  }
}

const caseDirectory = await mkdtemp(join(tmpdir(), "w-hlo0-cases-"))
const toolchainEnvironment = { ...process.env, CC: compiler }
try {
  run(cmake, ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug"], root, toolchainEnvironment)
  await assertCompiler(buildDirectory, "seed configure")
  run(cmake, ["--build", buildDirectory, "--target", "w_seed_hlo0_gate",
    "w_seed_hlo0_tests", "--parallel", "2"], root, toolchainEnvironment)
  const tests = run(resolve(buildDirectory, `w_seed_hlo0_tests${suffix}`), [])
  if (!tests.includes("verified-HIR Hello plan")) {
    fail("adversarial C test witness is missing")
  }
  const gate = run(resolve(buildDirectory, `w_seed_hlo0_gate${suffix}`), [fixture])
  if (!gate.includes("verified-HIR plan only; W execution unavailable")) {
    fail("gate did not state the execution boundary")
  }
  if (gate.toLowerCase().includes("timing") || gate.toLowerCase().includes("performance")) {
    fail("gate reported timing/performance data")
  }
  if (!gate.includes("payload=13") || !gate.includes("stdout=14") ||
      !gate.includes("sha256=d9014c4624844aa5bac314773d6b689ad467fa4e1d1a50a1b8a99d5a95f72ff5")) {
    fail("gate output does not contain the canonical payload identity")
  }
  const adversarial = [
    ["comment-noop.w", "fn main() { noop(\"Other\") } // print(\"Hello, world!\")\nentry(main)\n"],
    ["wrong-payload.w", "fn main() { print(\"Other\") } // Hello, world!\nentry(main)\n"],
  ]
  for (const [name, source] of adversarial) {
    const path = resolve(caseDirectory, name)
    await writeFile(path, source)
    const rejected = runRaw(resolve(buildDirectory, `w_seed_hlo0_gate${suffix}`), [path])
    if (rejected.exitCode === 0) fail(`${name} was accepted`)
    if (rejected.stdout.length !== 0) fail(`${name} produced partial stdout`)
  }
  process.stdout.write(`${gate.trim()}\n`)
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
  await rm(caseDirectory, { recursive: true, force: true })
}
