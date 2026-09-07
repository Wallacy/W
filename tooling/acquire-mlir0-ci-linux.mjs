import { createHash } from "node:crypto"
import { createReadStream } from "node:fs"
import {
  appendFile,
  lstat,
  mkdir,
  mkdtemp,
  open,
  readFile,
  readlink,
  readdir,
  realpath,
  rm,
  stat,
} from "node:fs/promises"
import { tmpdir } from "node:os"
import { dirname, isAbsolute, join, relative, resolve, sep } from "node:path"

const root = resolve(import.meta.dir, "..")
const manifestPath = resolve(import.meta.dir, "mlir0-ci-toolchain.json")
const expectedVersion = "23.1.0"
const expectedRelease = "2026.08.31"
const expectedFilename =
  "llvm-mlir_llvmorg-23.1.0_x86_64-unknown-linux-gnu.tar.zst"
const releaseBase =
  "https://github.com/munich-quantum-software/portable-mlir-toolchain/releases/download"
const requiredTools = ["mlir-opt", "mlir-translate", "llvm-config", "llc"]

function fail(message) {
  throw new Error(`MLIR0 Linux toolchain: ${message}`)
}

function isContained(parent, candidate) {
  const pathRelative = relative(resolve(parent), resolve(candidate))
  return pathRelative === "" ||
    (pathRelative !== ".." && !pathRelative.startsWith(`..${sep}`) &&
      !isAbsolute(pathRelative))
}

function shortOutput(bytes) {
  const value = Buffer.from(bytes).toString("utf8").trim()
  return value.length > 2000 ? `${value.slice(-2000)}…` : value
}

function directRun(command, args) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
  return {
    exitCode: result.exitCode,
    stdout: Buffer.from(result.stdout),
    stderr: Buffer.from(result.stderr),
  }
}

export function validateManifest(manifest) {
  if (manifest?.$schema !== "w-seed-mlir0-ci-toolchain-1" ||
      manifest.version !== 1 || manifest.status !== "pinned" ||
      manifest.purpose !== "mandatory-native-ci")
    fail("manifest schema, version, status, or purpose is invalid")
  if (manifest.target?.triple !== "x86_64-unknown-linux-gnu" ||
      manifest.target?.os !== "linux" || manifest.target?.abi !== "gnu")
    fail("manifest target is not the pinned Linux GNU target")
  for (const role of ["mlir", "llvm"])
    if (manifest.toolchain?.[role] !== expectedVersion)
      fail(`manifest toolchain ${role} is not ${expectedVersion}`)

  const distribution = manifest.distribution
  if (distribution?.release !== expectedRelease ||
      distribution.filename !== expectedFilename)
    fail("manifest distribution release or filename is not pinned")
  if (!Number.isSafeInteger(distribution.sizeBytes) ||
      distribution.sizeBytes < 1)
    fail("manifest distribution size is invalid")
  if (!/^[0-9a-f]{64}$/u.test(distribution.sha256 ?? ""))
    fail("manifest distribution SHA-256 is invalid")
  if (distribution.source !== "munich-quantum-software/setup-mlir")
    fail("manifest distribution source is invalid")

  const url = `${releaseBase}/${distribution.release}/${distribution.filename}`
  return { distribution, url }
}

export async function hashFile(pathValue) {
  const hash = createHash("sha256")
  let sizeBytes = 0
  for await (const chunk of createReadStream(pathValue)) {
    sizeBytes += chunk.length
    hash.update(chunk)
  }
  return { sizeBytes, sha256: hash.digest("hex") }
}

async function assertRegularFile(pathValue, label) {
  let stats
  try {
    stats = await lstat(pathValue)
  } catch (error) {
    fail(`${label} is unavailable: ${error.message}`)
  }
  if (!stats.isFile() || stats.isSymbolicLink())
    fail(`${label} is not a regular file: ${pathValue}`)
}

export async function verifyArchive(archivePath, expected) {
  await assertRegularFile(archivePath, "archive")
  const actual = await hashFile(archivePath)
  if (actual.sizeBytes !== expected.sizeBytes ||
      actual.sha256 !== expected.sha256)
    fail(`archive does not match pinned size/SHA-256: ${actual.sizeBytes}/${actual.sha256}`)
  return actual
}

function normalizeArchiveEntry(value) {
  let normalized = value
  while (normalized.startsWith("./")) normalized = normalized.slice(2)
  while (normalized.endsWith("/")) normalized = normalized.slice(0, -1)
  return normalized
}

function validateArchivePath(value, label) {
  if (typeof value !== "string" || value.length === 0 ||
      value.includes("\0") || /[\u0001-\u001f\u007f]/u.test(value) ||
      value.includes("\\") || value.startsWith("/") ||
      /^[A-Za-z]:/u.test(value) || value.split("/").includes(".."))
    fail(`${label} is absolute, traverses, or contains unsafe bytes: ${JSON.stringify(value)}`)
}

function validateLinkTarget(value, label) {
  if (typeof value !== "string" || value.length === 0 ||
      value.includes("\0") || /[\u0001-\u001f\u007f]/u.test(value) ||
      value.includes("\\") || value.startsWith("/") ||
      /^[A-Za-z]:/u.test(value))
    fail(`${label} is absolute or contains unsafe bytes: ${JSON.stringify(value)}`)
}

function resolveArchiveLink(entry, target) {
  validateLinkTarget(target, "archive link target")
  const parts = normalizeArchiveEntry(entry).split("/")
  parts.pop()
  for (const part of target.split("/")) {
    if (part.length === 0 || part === ".") continue
    if (part === "..") {
      if (parts.length === 0)
        fail(`archive link target escapes its root: ${JSON.stringify(target)}`)
      parts.pop()
    } else {
      parts.push(part)
    }
  }
  if (parts.length === 0)
    fail(`archive link target resolves to its root: ${JSON.stringify(target)}`)
  return parts.join("/")
}

function validateArchiveEntries(listing) {
  for (const rawEntry of listing.split(/\r?\n/u).filter(Boolean)) {
    const entry = normalizeArchiveEntry(rawEntry)
    if (entry.length === 0) continue
    validateArchivePath(entry, "archive entry")
  }
}

function validateArchiveLinkTargets(listing) {
  for (const rawLine of listing.split(/\r?\n/u).filter(Boolean)) {
    const type = rawLine[0]
    const linkMarker = type === "l" ? " -> " : " link to "
    const fields = rawLine.match(/^\S+\s+\S+\s+\S+\s+\S+\s+\S+\s+(.+)$/u)
    if (fields === null) continue
    const markerAt = fields[1].lastIndexOf(linkMarker)
    if ((type !== "l" && type !== "h") || markerAt < 0) continue
    const entry = normalizeArchiveEntry(fields[1].slice(0, markerAt))
    const target = fields[1].slice(markerAt + linkMarker.length)
    validateArchivePath(entry, "archive link path")
    resolveArchiveLink(entry, target)
  }
}

async function assertSafeTree(pathValue, treeRoot, ancestors = new Set()) {
  const stats = await lstat(pathValue)
  const identity = resolve(pathValue)
  if (ancestors.has(identity))
    fail(`extracted tree contains a symbolic-link cycle: ${pathValue}`)
  const nextAncestors = new Set(ancestors)
  nextAncestors.add(identity)
  if (stats.isSymbolicLink()) {
    const target = await readlink(pathValue)
    validateLinkTarget(target, "extracted symbolic-link target")
    const resolvedTarget = resolve(dirname(pathValue), target)
    if (!isContained(treeRoot, resolvedTarget))
      fail(`extracted symbolic link escapes its root: ${pathValue}`)
    await assertSafeTree(resolvedTarget, treeRoot, nextAncestors)
    return
  }
  if (stats.isDirectory()) {
    for (const entry of await readdir(pathValue))
      await assertSafeTree(join(pathValue, entry), treeRoot, nextAncestors)
    return
  }
  if (!stats.isFile()) fail(`extracted tree contains an unsupported entry: ${pathValue}`)
}

async function prepareDestination(destination) {
  if (isContained(root, destination))
    fail(`extraction destination must be outside the repository: ${destination}`)
  try {
    const stats = await lstat(destination)
    if (!stats.isDirectory() || stats.isSymbolicLink())
      fail(`extraction destination is not a directory: ${destination}`)
    if ((await readdir(destination)).length !== 0)
      fail(`extraction destination is not empty: ${destination}`)
  } catch (error) {
    if (error?.code !== "ENOENT") throw error
    await mkdir(destination)
  }
}

export async function resolveExecutable(pathValue, treeRoot) {
  let canonical
  try {
    canonical = await realpath(pathValue)
  } catch (error) {
    fail(`archive tool cannot be resolved: ${pathValue}: ${error.message}`)
  }
  if (!isContained(treeRoot, canonical))
    fail(`archive tool escapes its extraction root: ${pathValue}`)
  let stats
  try {
    stats = await stat(canonical)
  } catch (error) {
    fail(`archive tool cannot be inspected: ${canonical}: ${error.message}`)
  }
  if (!stats.isFile() || (stats.mode & 0o111) === 0)
    fail(`archive tool is not an executable regular file: ${pathValue}`)
  return canonical
}

export async function extractVerifiedArchive(archivePath, destination, expected) {
  // Keep verification before any decompressor, tar listing, or extraction.
  const verified = await verifyArchive(archivePath, expected)
  await prepareDestination(destination)
  const tar = Bun.which("tar")
  if (!tar) fail("tar is required for Linux archive extraction")
  const tarWorkspace = await mkdtemp(join(runtimeTemp(), "w-mlir0-ci-tar-"))
  const rawTarPath = join(tarWorkspace, "archive.tar")
  try {
    await decompressZstdToTar(archivePath, rawTarPath)
    const listing = directRun(tar, ["-tf", rawTarPath])
    if (listing.exitCode !== 0)
      fail(`archive listing failed: ${shortOutput(listing.stderr) || shortOutput(listing.stdout)}`)
    const listingText = listing.stdout.toString("utf8")
    validateArchiveEntries(listingText)
    const verbose = directRun(tar, ["-tvf", rawTarPath])
    if (verbose.exitCode !== 0)
      fail(`archive type listing failed: ${shortOutput(verbose.stderr) || shortOutput(verbose.stdout)}`)
    validateArchiveLinkTargets(verbose.stdout.toString("utf8"))

    const extraction = directRun(tar, [
      "-xf", rawTarPath, "-C", destination,
      "--no-same-owner", "--no-same-permissions",
    ])
    if (extraction.exitCode !== 0)
      fail(`archive extraction failed: ${shortOutput(extraction.stderr) || shortOutput(extraction.stdout)}`)
    await assertSafeTree(destination, destination)
    return verified
  } finally {
    await rm(tarWorkspace, { recursive: true, force: true })
  }
}

async function writeChunk(file, chunk) {
  // FileHandle.writeFile completes the whole chunk; no partial-write assumption.
  await file.writeFile(chunk)
}

async function decompressZstdToTar(sourcePath, destinationPath) {
  const zstd = Bun.which("zstd")
  if (!zstd) fail("zstd is required for Linux archive extraction")
  // The pinned package exceeds the runtime decompressor's default window.
  const result = directRun(zstd, [
    "--decompress", "--long=31", "--quiet", sourcePath, "-o", destinationPath,
  ])
  if (result.exitCode !== 0)
    fail(`Zstandard decompression failed: ${shortOutput(result.stderr) || shortOutput(result.stdout)}`)
  if ((await stat(destinationPath)).size === 0)
    fail("Zstandard archive decompressed to an empty tar")
}

async function downloadTo(url, destination, expectedSize) {
  let response
  try {
    response = await fetch(url, {
      redirect: "follow",
      signal: AbortSignal.timeout(600_000),
    })
  } catch (error) {
    fail(`archive download failed: ${error.message}`)
  }
  if (!response.ok) fail(`archive download failed: ${response.status} ${response.statusText}`)
  const contentLengthHeader = response.headers.get("content-length")
  if (contentLengthHeader !== null) {
    const contentLength = Number(contentLengthHeader)
    if (!Number.isSafeInteger(contentLength) || contentLength !== expectedSize)
      fail(`archive Content-Length differs from pinned size: ${contentLengthHeader}`)
  }
  if (response.body === null) fail("archive download returned no body")

  const file = await open(destination, "wx")
  const hash = createHash("sha256")
  let sizeBytes = 0
  const reader = response.body.getReader()
  try {
    while (true) {
      const item = await reader.read()
      if (item.done) break
      sizeBytes += item.value.byteLength
      if (sizeBytes > expectedSize)
        fail(`archive exceeds pinned size while downloading: ${sizeBytes}`)
      hash.update(item.value)
      await writeChunk(file, item.value)
    }
  } finally {
    reader.releaseLock()
    await file.close()
  }
  return { sizeBytes, sha256: hash.digest("hex") }
}

function runtimeTemp() {
  return resolve(process.env.RUNNER_TEMP || tmpdir())
}

export async function acquire({ archivePath, destination } = {}) {
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
  const { distribution, url } = validateManifest(manifest)
  let workspace
  let ownedArchive = false
  let extractionDirectory = destination === undefined
    ? undefined : resolve(destination)
  let success = false
  try {
    if (archivePath === undefined) {
      workspace = await mkdtemp(join(runtimeTemp(), "w-mlir0-ci-"))
      archivePath = join(workspace, distribution.filename)
      ownedArchive = true
      const downloaded = await downloadTo(url, archivePath, distribution.sizeBytes)
      if (downloaded.sizeBytes !== distribution.sizeBytes ||
          downloaded.sha256 !== distribution.sha256)
        fail(`download does not match pinned size/SHA-256: ${downloaded.sizeBytes}/${downloaded.sha256}`)
    } else {
      archivePath = resolve(archivePath)
    }

    if (extractionDirectory === undefined) {
      workspace ??= await mkdtemp(join(runtimeTemp(), "w-mlir0-ci-"))
      extractionDirectory = join(workspace, "toolchain")
    }
    await extractVerifiedArchive(archivePath, extractionDirectory, distribution)

    const top = await readdir(extractionDirectory, { withFileTypes: true })
    const directories = top.filter((entry) => entry.isDirectory())
    const files = top.filter((entry) => !entry.isDirectory())
    let toolRoot = extractionDirectory
    if (directories.length === 1 && files.length === 0)
      toolRoot = join(extractionDirectory, directories[0].name)
    const bin = join(toolRoot, "bin")
    const binStats = await lstat(bin)
    if (!binStats.isDirectory() || binStats.isSymbolicLink())
      fail(`archive does not contain a regular bin directory: ${bin}`)
    for (const name of requiredTools) {
      const pathValue = join(bin, name)
      await resolveExecutable(pathValue, extractionDirectory)
    }

    // Export only after download, digest, archive paths, and tool paths passed.
    if (process.env.GITHUB_PATH !== undefined)
      await appendFile(process.env.GITHUB_PATH, `${bin}\n`, "utf8")
    console.log(`MLIR0 Linux toolchain: verified ${distribution.filename} sha256=${distribution.sha256} bin=${bin}`)
    success = true
    return { bin, extractionDirectory, distribution }
  } finally {
    if (ownedArchive) await rm(archivePath, { force: true })
    if (!success && workspace !== undefined)
      await rm(workspace, { recursive: true, force: true })
  }
}

function parseArguments(argv) {
  let archivePath
  let destination
  let download = false
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index]
    if (argument === "--download" && !download && archivePath === undefined) {
      download = true
    } else if (argument === "--archive" && archivePath === undefined && !download) {
      archivePath = argv[++index]
      if (archivePath === undefined) fail("--archive requires a path")
    } else if (argument === "--destination" && destination === undefined) {
      destination = argv[++index]
      if (destination === undefined) fail("--destination requires a path")
    } else {
      fail(`unknown or repeated option: ${String(argument)}`)
    }
  }
  if (!download && archivePath === undefined)
    fail("choose --download or --archive <path>")
  return { archivePath, destination }
}

if (import.meta.main) {
  try {
    if (process.platform !== "linux" || process.arch !== "x64")
      fail(`native Linux acquisition requires linux/x64, got ${process.platform}/${process.arch}`)
    await acquire(parseArguments(process.argv.slice(2)))
  } catch (error) {
    console.error(error.message)
    process.exitCode = 1
  }
}
