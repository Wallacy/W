import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

import { ephemeralSourceDigest } from "./ephemeral-module-graph-machine.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const suffix = process.platform === "win32" ? ".exe" : ""

function fail(message) {
  throw new Error(`seed ephemeral graph: ${message}`)
}

function run(command, args, cwd = root) {
  const execution = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (execution.exitCode !== 0) {
    fail(`${command} ${args.join(" ")} failed: ${execution.stderr.toString().trim()}`)
  }
  return execution
}

const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-ephemeral-graph-"))
try {
  run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", buildDirectory, "--target", "w_seed_ephemeral_graph_tests"])
  const test = join(buildDirectory, `w_seed_ephemeral_graph_tests${suffix}`)
  const first = run(test, [])
  const second = run(test, [])
  if (first.stderr.length !== 0 || second.stderr.length !== 0) {
    fail("unit test wrote diagnostics to stderr")
  }
  if (!Buffer.from(first.stdout).equals(Buffer.from(second.stdout))) {
    fail("unit output is not deterministic")
  }
  const output = first.stdout.toString().trim()
  const digest = /^RESULT source-digest=([0-9a-f]{64})$/u.exec(output)
  if (!digest) fail(`unit output has no fixed digest record: ${output}`)
  const expected = ephemeralSourceDigest("")
  if (digest[1] !== expected.slice("sha256:".length)) {
    fail("C source digest does not match the RU0 machine tag")
  }
  console.log("seed ephemeral graph: bounded C graph, frontend projection, deterministic ordering, and RU0 digest tag passed")
} finally {
  await rm(buildDirectory, { recursive: true, force: true })
}
