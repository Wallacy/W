import { createHash } from "node:crypto"
import { mkdtemp, readdir, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, relative, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const expectedMaintainedCorpusCount = 294
const expectedMaintainedCorpusAdditions = [
  "reference/last-light/build.w",
  "tooling/studies/aeg0-app-essentials-gate/adversarial.w",
  "tooling/studies/aeg0-app-essentials-gate/current.w",
  "tooling/studies/pfu0-pre-freeze-usability/adversarial.w",
  "tooling/studies/pfu0-pre-freeze-usability/current.w",
]
const retiredMaintainedCorpusPaths = [
  "reference/last-light/package.w",
  "reference/last-light/workspace.w",
]
const opaqueClaims = new Map([
  ["reference/last-light/abi.w", [
    { start: 1446, end: 1607, digest: "sha256:fcdcb287474cc7d876c4330e7206f5ada849d3c44bf18ea039a0e9e17bc24c80", profile: "C" },
    { start: 1703, end: 2353, digest: "sha256:ccf1550449f7aa39f3a03b2ed87b4a268a10978383f0eaeefab591f621d4ee79", profile: "C" },
  ]],
  ["reference/last-light/hardware.w", [
    { start: 1125, end: 1328, digest: "sha256:8aede5643732489afcbb39ebfc4080689736825e9d11aca275a271fe678b5567", profile: "C" },
  ]],
  ["tooling/studies/r1-callable-property-surface/named-language-slot.w", [
    { start: 138, end: 156, digest: "sha256:e7e1fc54e6c1fb4f447c6eb3c3cf9578af05e554f0863f8bc68a242d78fbd1a3", profile: "C" },
  ]],
  ["tooling/studies/r1-callable-property-surface/property-safe-closure-island.w", [
    { start: 290, end: 308, digest: "sha256:e7e1fc54e6c1fb4f447c6eb3c3cf9578af05e554f0863f8bc68a242d78fbd1a3", profile: "C" },
  ]],
  ["reference/last-light/system_escapes.w", [
    { start: 984, end: 1026, digest: "sha256:62040ce2af173703e5b9c14ab6845f217fa984fa2f15d2111cc4c3af0037397e", profile: "Asm" },
  ]],
])

function fail(message) {
  throw new Error("seed lexer: " + message)
}

function run(command, args) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (result.exitCode !== 0) {
    const stderr = result.stderr.toString().trim()
    fail(command + " " + args.join(" ") + " failed" + (stderr ? ": " + stderr : ""))
  }
  return result
}

async function collectW(relativeDirectory, recursive) {
  const result = []
  const base = resolve(root, relativeDirectory)
  async function visit(directory) {
    const entries = await readdir(directory, { withFileTypes: true })
    entries.sort((left, right) => left.name.localeCompare(right.name))
    for (const entry of entries) {
      const absolute = join(directory, entry.name)
      if (entry.isFile() && entry.name.endsWith(".w")) {
        result.push(relative(root, absolute).replaceAll("\\", "/"))
      } else if (recursive && entry.isDirectory()) {
        await visit(absolute)
      }
    }
  }
  await visit(base)
  return result
}

async function corpusPaths() {
  const paths = [
    ...(await collectW("reference/last-light", false)),
    ...(await collectW("std", true)),
    ...(await collectW("tooling/studies", true)),
  ]
  paths.sort()
  if (paths.length !== expectedMaintainedCorpusCount) {
    fail(
      `expected ${expectedMaintainedCorpusCount} maintained .w corpus files after ` +
      `the known corpus additions and retirements, got ${paths.length}`,
    )
  }
  const pathSet = new Set(paths)
  const missingAdditions = expectedMaintainedCorpusAdditions.filter((path) => !pathSet.has(path))
  if (missingAdditions.length > 0) {
    fail("known maintained corpus additions are missing: " + missingAdditions.join(", "))
  }
  const retiredPathsPresent = retiredMaintainedCorpusPaths.filter((path) => pathSet.has(path))
  if (retiredPathsPresent.length > 0) {
    fail("retired maintained corpus paths are still present: " + retiredPathsPresent.join(", "))
  }
  return paths
}

function parseProbeOutput(text, bytes, expectedOpaque) {
  const lines = text.trim() === "" ? [] : text.trim().split(/\r?\n/u)
  const spans = []
  let previous = 0
  for (const line of lines) {
    const match = /^item kind=(\d+) start=(\d+) end=(\d+) flags=(\d+)$/u.exec(line)
    if (!match) fail("invalid probe line " + JSON.stringify(line))
    const kind = Number(match[1])
    const start = Number(match[2])
    const end = Number(match[3])
    if (start !== previous || end < start || end > bytes.length) {
      fail("C partition gap/overlap at " + start + ":" + end + " after " + previous)
    }
    spans.push({ kind, start, end })
    previous = end
  }
  if (previous !== bytes.length) fail("C partition stops at " + previous + " of " + bytes.length)

  const foreign = spans.filter((span) => span.kind === 6).map(({ start, end }) => ({ start, end }))
  if (JSON.stringify(foreign) !== JSON.stringify(expectedOpaque)) {
    fail("pinned opaque partition " + JSON.stringify(foreign) + " != " + JSON.stringify(expectedOpaque))
  }
  return spans
}

function compareBunPartition(bytes, spans, expectedOpaque) {
  // Independent Bun evidence: every C interval maps to original bytes, and the
  // pinned intervals are the only foreign intervals.
  const reconstructed = Buffer.alloc(bytes.length)
  let cursor = 0
  for (const span of spans) {
    const fragment = bytes.subarray(span.start, span.end)
    fragment.copy(reconstructed, span.start)
    if (span.start !== cursor) fail("Bun partition cursor mismatch at " + span.start)
    cursor = span.end
  }
  if (!reconstructed.equals(bytes)) fail("Bun byte reconstruction differs from source")
  const expected = expectedOpaque.map(({ start, end }) => start + ":" + end).join(",")
  const actual = spans.filter((span) => span.kind === 6).map(({ start, end }) => start + ":" + end).join(",")
  if (actual !== expected) fail("Bun foreign evidence differs from pinned spans")
}

function validatePinnedOpaque(path, bytes) {
  const claims = opaqueClaims.get(path) ?? []
  for (const claim of claims) {
    if (claim.end > bytes.length || claim.start > claim.end) fail(path + " opaque span is outside source")
    const digest = "sha256:" + createHash("sha256").update(bytes.subarray(claim.start, claim.end)).digest("hex")
    if (digest !== claim.digest) fail(path + " pinned opaque digest changed at " + claim.start + ":" + claim.end)
  }
  return claims.map(({ start, end }) => ({ start, end }))
}

function validatePinnedProfiles() {
  const profiles = [...opaqueClaims.values()].flat().map((claim) => claim.profile)
  const cCount = profiles.filter((profile) => profile === "C").length
  const asmCount = profiles.filter((profile) => profile === "Asm").length
  if (profiles.length !== 6 || cCount !== 5 || asmCount !== 1) {
    fail("pinned opaque profile counts are not C×5/Asm×1")
  }
}

function expectFailure(label, callback) {
  try {
    callback()
  } catch {
    return
  }
  fail(label + " mutation was accepted")
}

async function runMutationChecks() {
  const bytes = Buffer.from("abc", "utf8")
  expectFailure("gap", () => parseProbeOutput("item kind=2 start=1 end=3 flags=0", bytes, []))
  expectFailure("overlap", () => parseProbeOutput(
    "item kind=2 start=0 end=2 flags=0\nitem kind=2 start=1 end=3 flags=0",
    bytes,
    [],
  ))
  expectFailure("wrong foreign set", () => parseProbeOutput("item kind=6 start=0 end=3 flags=0", bytes, []))

  const pinnedPath = "reference/last-light/abi.w"
  const pinnedBytes = Buffer.from(await Bun.file(resolve(root, pinnedPath)).arrayBuffer())
  pinnedBytes[1446] ^= 0x01
  expectFailure("stale body digest", () => validatePinnedOpaque(pinnedPath, pinnedBytes))
}

function checkStop(probe, bytes, expectedKind, label, expectedCodePoint = null) {
  const execution = Bun.spawnSync({
    cmd: [probe],
    cwd: root,
    stdin: bytes,
    stdout: "pipe",
    stderr: "pipe",
  })
  const stderr = execution.stderr.toString()
  if (execution.exitCode === 0 || !new RegExp("kind=" + expectedKind + "\\b", "u").test(stderr)) {
    fail(label + " did not stop with internal error kind " + expectedKind)
  }
  if (expectedCodePoint !== null) {
    const codePoint = expectedCodePoint.toString(16).toUpperCase().padStart(4, "0")
    if (!new RegExp("code_point=U\\+" + codePoint + "\\b", "u").test(stderr)) {
      fail(label + " did not report code point U+" + codePoint)
    }
  }
}

function checkPass(probe, bytes, label) {
  const execution = Bun.spawnSync({
    cmd: [probe],
    cwd: root,
    stdin: bytes,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (execution.exitCode !== 0) {
    fail(label + " unexpectedly stopped: " + execution.stderr.toString().trim())
  }
  parseProbeOutput(execution.stdout.toString(), bytes, [])
}

async function main() {
  const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-lexer-"))
  try {
    validatePinnedProfiles()
    await runMutationChecks()
    run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
    run("cmake", ["--build", buildDirectory])
    run("ctest", ["--test-dir", buildDirectory, "--output-on-failure"])
    const probeName = process.platform === "win32" ? "w_seed_lexer_probe.exe" : "w_seed_lexer_probe"
    const probe = join(buildDirectory, probeName)
    if (!(await Bun.file(probe).exists())) fail("probe is missing at " + probe)

    const paths = await corpusPaths()
    let itemCount = 0
    for (const path of paths) {
      const bytes = Buffer.from(await Bun.file(resolve(root, path)).arrayBuffer())
      const claims = validatePinnedOpaque(path, bytes)
      const args = claims.flatMap(({ start, end }) => ["--opaque=" + start + ":" + end])
      const execution = Bun.spawnSync({
        cmd: [probe, ...args],
        cwd: root,
        stdin: bytes,
        stdout: "pipe",
        stderr: "pipe",
      })
      if (execution.exitCode !== 0) {
        fail(path + " failed: " + execution.stderr.toString().trim())
      }
      const spans = parseProbeOutput(execution.stdout.toString(), bytes, claims)
      compareBunPartition(bytes, spans, claims)
      itemCount += spans.length
    }
    checkPass(probe, Buffer.from("let café = 1", "utf8"), "Unicode identifier")
    checkPass(probe, Buffer.from("cafe\u0301", "utf8"), "decomposed Unicode identifier")
    checkPass(probe, Buffer.from("paypa\u043b", "utf8"), "mixed-script raw identifier")
    checkStop(probe, Buffer.from("a\u200Cb", "utf8"), 5, "ZWNJ identifier", 0x200C)
    checkStop(probe, Buffer.from("a\u200Db", "utf8"), 5, "ZWJ identifier", 0x200D)
    checkStop(probe, Buffer.from("a\uFE0F", "utf8"), 5, "variation selector identifier", 0xFE0F)
    checkStop(probe, Buffer.from("a\u202Eb", "utf8"), 5, "bidi control identifier", 0x202E)
    checkStop(probe, Buffer.from("a\uFEFF", "utf8"), 5, "internal BOM identifier", 0xFEFF)
    checkStop(probe, Buffer.from("\u0301", "utf8"), 5, "combining mark at identifier start", 0x0301)
    checkStop(probe, Buffer.from("a\u{1F600}b", "utf8"), 5, "non-XID emoji identifier", 0x1F600)
    checkStop(probe, Buffer.from("x\ry", "utf8"), 6, "bare CR control")
    console.log("Seed C lexer: " + paths.length + " .w byte partitions, " + itemCount + " items, C11/Bun partition-integrity checks passed (pinned opaque C×5/Asm×1).")
  } finally {
    await rm(buildDirectory, { recursive: true, force: true })
  }
}

if (import.meta.main) await main()
