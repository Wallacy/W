import { createHash } from "node:crypto"
import { mkdtemp, realpath, rm, stat } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, relative, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const PROBE_CAPACITY = 16 * 1024 * 1024

function fail(message) {
  throw new Error("seed source reader: " + message)
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

export function formatterInputText(input, owner = "formatter input") {
  if (!input || !Array.isArray(input.lines) || input.lines.length === 0) {
    fail(owner + " has no input lines")
  }
  if (input.lines.some((line) => typeof line !== "string" || /[\r\n]/u.test(line))) {
    fail(owner + " input lines contain a newline")
  }
  if (Object.hasOwn(input, "bom") && typeof input.bom !== "boolean") {
    fail(owner + " bom must be boolean when present")
  }
  if (Object.hasOwn(input, "finalNewline") && typeof input.finalNewline !== "boolean") {
    fail(owner + " finalNewline must be boolean when present")
  }
  const newline = input.newline === "crlf" ? "\r\n" : "\n"
  if (input.newline !== undefined && input.newline !== "lf" && input.newline !== "crlf") {
    fail(owner + " has an invalid newline mode")
  }
  let source = input.lines.join(newline)
  if (input.finalNewline !== false) source += newline
  if (input.bom === true) source = "\uFEFF" + source
  return source
}

export function formatterOutputText(output, owner = "formatter output") {
  if (!Array.isArray(output) || output.length === 0) {
    fail(owner + " has no output lines")
  }
  if (output.some((line) => typeof line !== "string" || /[\r\n]/u.test(line))) {
    fail(owner + " output lines contain a newline")
  }
  return output.join("\n") + "\n"
}

export function sourceFacts(bytes) {
  let lines = 1
  for (const byte of bytes) if (byte === 0x0a) lines += 1
  return {
    length: bytes.length,
    bom: bytes.length >= 3 && bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf ? 1 : 0,
    lines,
  }
}

export function validateSourceRefPath(sourcePath, owner = "sourceRef") {
  if (typeof sourcePath !== "string" || sourcePath.trim() === "") {
    fail(owner + ".path must be a non-empty relative .w path")
  }
  const normalized = sourcePath.replaceAll("\\", "/")
  const segments = normalized.split("/")
  if (
    normalized.startsWith("/") ||
    /^[A-Za-z][A-Za-z0-9+.-]*:/u.test(normalized) ||
    segments.some((segment) => segment === "" || segment === ".") ||
    segments.includes("..")
  ) {
    fail(owner + ".path must not be absolute, a scheme, or contain empty/./.. segments")
  }
  if (!normalized.endsWith(".w")) {
    fail(owner + ".path must end with .w")
  }
  if (segments.some((segment) => segment === "history" || segment === "generated")) {
    fail(owner + ".path must not reference history or generated sources")
  }
  if (
    segments[0] === "tooling" &&
    segments[1] === "tree-sitter-w" &&
    segments[2] === "src"
  ) {
    fail(owner + ".path must not reference generated Tree-sitter sources")
  }
  return normalized
}

async function resolveSourceRefPath(sourcePath, owner, realRoot) {
  const normalized = validateSourceRefPath(sourcePath, owner)
  const resolved = resolve(root, normalized)
  const lexical = relative(root, resolved).replaceAll("\\", "/")
  if (!lexical || lexical === ".." || lexical.startsWith("../") || /^[A-Za-z]:/u.test(lexical)) {
    fail(owner + ".path must stay inside the repository")
  }

  let actual
  try {
    actual = await realpath(resolved)
  } catch {
    fail(owner + ".path references a missing file")
    return undefined
  }
  const actualRelative = relative(realRoot, actual).replaceAll("\\", "/")
  if (
    !actualRelative ||
    actualRelative === ".." ||
    actualRelative.startsWith("../") ||
    /^[A-Za-z]:/u.test(actualRelative)
  ) {
    fail(owner + ".path realpath must stay inside the repository")
  }
  const actualSegments = actualRelative.split("/")
  if (
    !actualRelative.endsWith(".w") ||
    actualSegments.some((segment) => segment === "history" || segment === "generated") ||
    (actualSegments[0] === "tooling" &&
      actualSegments[1] === "tree-sitter-w" &&
      actualSegments[2] === "src")
  ) {
    fail(owner + ".path realpath must reference a maintained .w source")
  }
  let info
  try {
    info = await stat(actual)
  } catch {
    fail(owner + ".path references a missing file")
    return undefined
  }
  if (!info.isFile()) {
    fail(owner + ".path references a non-file")
  }
  return actual
}

export function parseProbeSummary(summary) {
  const match = /^ok length=(\d+) bom=(0|1) lines=(\d+)\r?$/u.exec(summary)
  if (!match) throw new Error("invalid probe summary " + JSON.stringify(summary))
  return {
    length: Number(match[1]),
    bom: Number(match[2]),
    lines: Number(match[3]),
  }
}

export function validateProbeExecution(label, bytes, execution) {
  if (execution.exitCode !== 0) {
    throw new Error(label + ": probe failed: " + execution.stderr.toString().trim())
  }
  const output = Buffer.from(execution.stdout)
  if (!output.equals(bytes)) throw new Error(label + ": probe did not round-trip bytes")
  const actual = parseProbeSummary(execution.stderr.toString().trim())
  const expected = sourceFacts(bytes)
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(label + ": facts " + JSON.stringify(actual) + " != " + JSON.stringify(expected))
  }
}

function bytesEqual(left, right) {
  if (left.length !== right.length) return false
  for (let index = 0; index < left.length; index += 1) {
    if (left[index] !== right[index]) return false
  }
  return true
}

function validateProbeCapacity(probe) {
  const overCapacity = Buffer.alloc(PROBE_CAPACITY + 1, 0x41)
  const exactInput = overCapacity.subarray(0, PROBE_CAPACITY)
  const accepted = Bun.spawnSync({
    cmd: [probe],
    cwd: root,
    stdin: exactInput,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (accepted.exitCode !== 0 || !bytesEqual(accepted.stdout, exactInput)) {
    fail("probe exact-capacity input was not accepted and round-tripped")
  }
  const acceptedFacts = parseProbeSummary(accepted.stderr.toString().trim())
  if (
    acceptedFacts.length !== PROBE_CAPACITY ||
    acceptedFacts.bom !== 0 ||
    acceptedFacts.lines !== 1
  ) {
    fail("probe exact-capacity facts are incorrect")
  }

  const rejected = Bun.spawnSync({
    cmd: [probe],
    cwd: root,
    stdin: overCapacity,
    stdout: "pipe",
    stderr: "pipe",
  })
  const expectedError = "error operation=read kind=source-too-large offset=" + PROBE_CAPACITY
  if (
    rejected.exitCode !== 2 ||
    rejected.stdout.length !== 0 ||
    rejected.stderr.toString().trim() !== expectedError
  ) {
    fail("probe over-capacity input did not fail with the closed source-too-large result")
  }
}

async function readFixtures() {
  const formatter = await Bun.file(resolve(root, "tooling", "formatter-cases.json")).json()
  if (formatter.status !== "design-oracle-input" || !Array.isArray(formatter.cases)) {
    fail("formatter corpus is not a design-oracle input")
  }

  const fixtures = []
  for (const testCase of formatter.cases) {
    fixtures.push({
      label: testCase.id + ":input",
      bytes: Buffer.from(formatterInputText(testCase.input, testCase.id), "utf8"),
    })
    fixtures.push({
      label: testCase.id + ":output",
      bytes: Buffer.from(formatterOutputText(testCase.output, testCase.id), "utf8"),
    })
  }
  if (fixtures.length !== 56) fail("expected 56 F0 texts, got " + fixtures.length)

  fixtures.push({
    label: "last-light/formatting.w",
    bytes: Buffer.from(await Bun.file(resolve(root, "reference", "last-light", "formatting.w")).arrayBuffer()),
  })

  const freeze = await Bun.file(resolve(root, "tooling", "frontend-freeze-cases.json")).json()
  if (freeze.status !== "design-oracle-input" || !Array.isArray(freeze.families)) {
    fail("FZ0 corpus is not a design-oracle input")
  }
  const realRoot = await realpath(root)
  const seenPaths = new Set()
  for (const family of freeze.families) {
    const sourceRef = family.sourceRef
    const owner = "FZ0/" + family.id + ".sourceRef"
    const normalizedPath = validateSourceRefPath(sourceRef?.path, owner)
    if (seenPaths.has(normalizedPath)) {
      fail("FZ0 source ref is missing or duplicated in " + family.id)
    }
    seenPaths.add(normalizedPath)
    const sourcePath = await resolveSourceRefPath(normalizedPath, owner, realRoot)
    const bytes = Buffer.from(await Bun.file(sourcePath).arrayBuffer())
    const digest = "sha256:" + createHash("sha256").update(bytes).digest("hex")
    if (digest !== sourceRef.digest) fail(family.id + ": source digest is stale")
    fixtures.push({ label: "FZ0/" + family.id + ":" + normalizedPath, bytes })
  }
  if (seenPaths.size !== 6) fail("expected six FZ0 source refs, got " + seenPaths.size)
  return fixtures
}

async function main() {
  const buildDirectory = await mkdtemp(join(tmpdir(), "w-seed-source-reader-"))
  try {
    run("cmake", ["-S", seedDirectory, "-B", buildDirectory, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
    run("cmake", ["--build", buildDirectory])
    run("ctest", ["--test-dir", buildDirectory, "--output-on-failure"])

    const probeName = process.platform === "win32" ? "w_seed_source_probe.exe" : "w_seed_source_probe"
    const probe = join(buildDirectory, probeName)
    if (!(await Bun.file(probe).exists())) fail("probe is missing at " + probe)

    const fixtures = await readFixtures()
    for (const fixture of fixtures) {
      const execution = Bun.spawnSync({
        cmd: [probe],
        cwd: root,
        stdin: fixture.bytes,
        stdout: "pipe",
        stderr: "pipe",
      })
      try {
        validateProbeExecution(fixture.label, fixture.bytes, execution)
      } catch (error) {
        fail(error instanceof Error ? error.message : String(error))
      }
    }
    validateProbeCapacity(probe)
    console.log("Seed C source reader: " + fixtures.length + " byte-preserving probes, C11 unit tests passed.")
  } finally {
    await rm(buildDirectory, { recursive: true, force: true })
  }
}

if (import.meta.main) await main()
