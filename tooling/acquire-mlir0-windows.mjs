import { createHash } from "node:crypto"
import { lstat, mkdir, mkdtemp, open, readdir, readFile, rename, rm, statfs, writeFile } from "node:fs/promises"
import { homedir, tmpdir } from "node:os"
import { basename, dirname, isAbsolute, join, relative, resolve, sep } from "node:path"

const repositoryRoot = resolve(import.meta.dir, "..")
const manifestPath = resolve(import.meta.dir, "mlir0-windows-toolchain.json")
export const MATERIALIZED_SCHEMA = "w-seed-mlir0-windows-materialized-1"
export const MATERIALIZED_MANIFEST = "w-mlir0-windows-materialized.json"
export const STAGING_PREFIX = ".w-mlir0-acquire-"
export const DOWNLOAD_PREFIX = "w-mlir0-download-"
export const INSTALLED_SIZE_DEFINITION =
  "archive payload files; excludes the materialized manifest"

function fail(message) {
  throw new Error(`MLIR0 Windows toolchain: ${message}`)
}

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value)
}

function isContained(parent, candidate) {
  const relativePath = relative(resolve(parent), resolve(candidate))
  return relativePath === "" ||
    (relativePath !== ".." &&
      !relativePath.startsWith(`..${sep}`) &&
      !isAbsolute(relativePath))
}

function pushError(errors, condition, message) {
  if (!condition) errors.push(message)
}

export function validateManifest(manifest) {
  const errors = []
  pushError(errors, isObject(manifest), "manifest must be an object")
  if (!isObject(manifest)) return errors

  pushError(errors, manifest.$schema === "w-seed-mlir0-windows-toolchain-1",
    "manifest.$schema is invalid")
  pushError(errors, manifest.version === 1, "manifest.version must be 1")
  pushError(errors, manifest.status === "external-evaluation",
    "manifest.status must be external-evaluation")
  pushError(errors, manifest.artifact?.schema === "w-seed-mlir0-10" &&
    manifest.artifact?.scope === "unit-cfg-diamond",
  "manifest artifact scope is invalid")
  pushError(errors, manifest.host?.platform === "windows" &&
    manifest.host?.architecture === "x86_64" &&
    manifest.host?.triple === "x86_64-pc-windows-msvc" &&
    manifest.host?.mode === "native",
  "manifest host is not native Windows x86_64")
  pushError(errors, manifest.target?.triple === "x86_64-pc-windows-msvc" &&
    manifest.target?.arch === "x86_64" && manifest.target?.os === "windows" &&
    manifest.target?.abi === "msvc",
  "manifest target is invalid")
  for (const role of ["mlir", "llvm", "lld"])
    pushError(errors, manifest.toolchain?.[role] === "23.1.0",
      `manifest toolchain ${role} must be 23.1.0`)

  const asset = manifest.asset
  pushError(errors, isObject(asset), "manifest asset must be an object")
  if (isObject(asset)) {
    pushError(errors, asset.provider === "portable-mlir-toolchain",
      "manifest asset provider is invalid")
    pushError(errors, asset.release === "2026.08.31",
      "manifest asset release is invalid")
    pushError(errors, asset.llvmTag === "llvmorg-23.1.0",
      "manifest asset LLVM tag is invalid")
    pushError(errors, asset.targetTriple === "x86_64-pc-windows-msvc",
      "manifest asset target is invalid")
    pushError(errors, asset.archiveFormat === "tar.zst",
      "manifest asset archive format is invalid")
    pushError(errors,
      asset.fileName === "llvm-mlir_llvmorg-23.1.0_x86_64-pc-windows-msvc.tar.zst",
      "manifest asset file name is invalid")
    pushError(errors,
      asset.url === "https://github.com/munich-quantum-software/portable-mlir-toolchain/releases/download/2026.08.31/llvm-mlir_llvmorg-23.1.0_x86_64-pc-windows-msvc.tar.zst",
      "manifest asset URL is invalid")
    pushError(errors, Number.isSafeInteger(asset.sizeBytes) && asset.sizeBytes === 415482701,
      "manifest asset size is invalid")
    pushError(errors,
      typeof asset.sha256 === "string" &&
        /^[0-9a-f]{64}$/u.test(asset.sha256) &&
        asset.sha256 === "35244de53a023a3e546e070b34d27ce9f7142ace92b238b5707c1d0cf24fd944",
      "manifest asset SHA-256 is invalid")
  }

  const archivePolicy = manifest.archivePolicy
  pushError(errors, isObject(archivePolicy), "manifest archivePolicy must be an object")
  if (isObject(archivePolicy)) {
    pushError(errors, JSON.stringify(archivePolicy.allowedEntryTypes) ===
      JSON.stringify(["file", "directory"]),
    "archivePolicy allowed entry types are invalid")
    for (const field of ["rejectSymlink", "rejectHardlink",
      "rejectReparsePoint", "rejectAbsolutePath", "rejectTraversal",
      "rejectDuplicatePath"])
      pushError(errors, archivePolicy[field] === true,
        `archivePolicy.${field} must be true`)
  }

  const tools = manifest.tools
  pushError(errors, isObject(tools), "manifest tools must be an object")
  if (isObject(tools)) {
    pushError(errors, JSON.stringify(tools.versionArgs) === JSON.stringify(["--version"]),
      "tools.versionArgs must be [--version]")
    pushError(errors, tools.expectedVersion === "23.1.0",
      "tools.expectedVersion must be 23.1.0")
    pushError(errors, JSON.stringify(tools.required) ===
      JSON.stringify(["mlir-opt.exe", "mlir-translate.exe", "llc.exe", "lld-link.exe"]),
    "tools.required is invalid")
    pushError(errors, Array.isArray(tools.optional) &&
      new Set(tools.optional).size === tools.optional.length,
    "tools.optional must be a duplicate-free array")
    pushError(errors, Array.isArray(tools.optional) &&
      tools.optional.includes("clang.exe") && tools.optional.includes("clang-cl.exe") &&
      !tools.required.includes("clang.exe") && !tools.required.includes("clang-cl.exe"),
    "tools optional Clang policy is invalid")
    pushError(errors,
      tools.discovery === "recursive-exact-basename-after-archive-inspection" &&
        tools.ambiguousBasename === "reject" && tools.missingOptional === "record" &&
        tools.optionalProbeFailure === "record",
      "tools discovery policy is invalid")
  }

  pushError(errors, manifest.materialization?.destination ===
    "explicit-or-default-cache-outside-repository",
  "materialization destination policy is invalid")
  pushError(errors, manifest.materialization?.staging ===
    "same-parent-unique-directory",
  "materialization staging policy is invalid")
  pushError(errors, manifest.materialization?.stagingPrefix === STAGING_PREFIX,
    "materialization staging prefix is invalid")
  pushError(errors, manifest.materialization?.materializedManifest ===
    MATERIALIZED_MANIFEST,
  "materialization manifest name is invalid")
  pushError(errors, manifest.materialization?.sizeDefinition ===
    INSTALLED_SIZE_DEFINITION,
  "materialization installed size definition is invalid")
  pushError(errors, manifest.materialization?.reuse ===
    "validate-existing-materialization",
  "materialization reuse policy is invalid")
  pushError(errors, manifest.materialization?.commit === "atomic-directory-rename",
    "materialization commit policy is invalid")

  pushError(errors, manifest.runtimeBoundary?.consumer === "future-w-run" &&
    manifest.runtimeBoundary?.network === "forbidden" &&
    manifest.runtimeBoundary?.pathSearch === "forbidden" &&
    manifest.runtimeBoundary?.shell === "forbidden" &&
    manifest.runtimeBoundary?.distributionRole === "development-and-release-only" &&
    manifest.runtimeBoundary?.bundledWithW === false &&
    manifest.runtimeBoundary?.extractedSizeIsWBudget === false &&
    manifest.runtimeBoundary?.futureRuntime === "minimal-hermetic-components" &&
    JSON.stringify(manifest.runtimeBoundary?.futureCrossTargetPlatforms) ===
      JSON.stringify(["windows", "linux", "macos"]),
  "runtime boundary is invalid")
  pushError(errors, manifest.provenance?.kind === "external-evaluation" &&
    manifest.provenance?.supportClaim === "none" &&
    manifest.provenance?.platformSupportState === "candidate",
  "provenance boundary is invalid")
  pushError(errors, manifest.linuxWslProfile?.manifest ===
    "tooling/mlir0-toolchain.json" &&
    manifest.linuxWslProfile?.version === "20.1.2" &&
    manifest.linuxWslProfile?.targetTriple === "x86_64-unknown-linux-gnu" &&
    manifest.linuxWslProfile?.status === "pinned-update-required" &&
    manifest.linuxWslProfile?.nativeWindows === false,
  "Linux/WSL profile is not kept separate")
  return errors
}

export function normalizeArchivePath(value) {
  if (typeof value !== "string" || value.length === 0 || value.includes("\0"))
    throw new Error("archive entry is empty or contains NUL")
  if (value.includes("\\") || value.startsWith("/") ||
      /^[A-Za-z]:/u.test(value))
    throw new Error(`archive entry is absolute or uses a backslash: ${JSON.stringify(value)}`)
  let pathValue = value
  while (pathValue.startsWith("./")) pathValue = pathValue.slice(2)
  while (pathValue.endsWith("/")) pathValue = pathValue.slice(0, -1)
  if (pathValue.length === 0) throw new Error("archive entry has no path")
  const parts = pathValue.split("/")
  if (parts.some((part) => part.length === 0 || part === "." || part === ".."))
    throw new Error(`archive entry contains an empty, dot, or traversal component: ${JSON.stringify(value)}`)
  return parts.join("/")
}

export function validateArchiveEntries(entries) {
  if (!Array.isArray(entries) || entries.length === 0)
    throw new Error("archive has no entries")
  const seen = new Set()
  const types = new Map()
  for (const entry of entries) {
    if (!isObject(entry) || !["file", "directory"].includes(entry.type))
      throw new Error("archive contains a non-file or non-directory entry")
    const normalized = normalizeArchivePath(entry.path)
    if (normalized === MATERIALIZED_MANIFEST)
      throw new Error(`archive entry is reserved for the materialized manifest: ${normalized}`)
    if (seen.has(normalized))
      throw new Error(`archive contains a duplicate entry: ${normalized}`)
    seen.add(normalized)
    types.set(normalized, entry.type)
  }
  for (const entry of seen) {
    const parts = entry.split("/")
    for (let count = 1; count < parts.length; count += 1) {
      const parent = parts.slice(0, count).join("/")
      if (types.get(parent) === "file")
        throw new Error(`archive file is also a directory parent: ${parent}`)
    }
  }
  return entries.map((entry) => ({
    path: normalizeArchivePath(entry.path),
    type: entry.type,
  }))
}

export function defaultCacheDirectory() {
  const localAppData = process.env.LOCALAPPDATA ||
    join(homedir(), "AppData", "Local")
  return resolve(localAppData, "W", "toolchains", "portable-mlir-toolchain",
    "2026.08.31", "x86_64-pc-windows-msvc")
}

async function pathExists(pathValue) {
  try {
    await lstat(pathValue)
    return true
  } catch (error) {
    if (error?.code === "ENOENT") return false
    throw error
  }
}

function isReparsePoint(stats) {
  return stats.isSymbolicLink() || (stats.mode & 0xf000) === 0xa000
}

async function assertNoReparseAncestors(pathValue) {
  let current = resolve(pathValue)
  while (true) {
    try {
      const stats = await lstat(current)
      if (isReparsePoint(stats))
        fail(`refuses a symbolic link or reparse point in path ${current}`)
    } catch (error) {
      if (error?.code !== "ENOENT") throw error
    }
    const parent = dirname(current)
    if (parent === current) return
    current = parent
  }
}

function assertOwnedDownloadPath(pathValue) {
  const resolved = resolve(pathValue)
  if (!isContained(tmpdir(), resolved) ||
      !basename(resolved).startsWith(DOWNLOAD_PREFIX))
    fail(`refuses to remove an unowned download path: ${resolved}`)
}

function assertOwnedStagingPath(pathValue) {
  const resolved = resolve(pathValue)
  if (!basename(resolved).startsWith(STAGING_PREFIX))
    fail(`refuses to remove an unowned staging path: ${resolved}`)
}

async function removeOwnedDownload(pathValue) {
  if (pathValue === undefined) return
  assertOwnedDownloadPath(pathValue)
  await rm(pathValue, { recursive: true, force: true })
}

async function removeOwnedStaging(pathValue) {
  if (pathValue === undefined) return
  assertOwnedStagingPath(pathValue)
  await rm(pathValue, { recursive: true, force: true })
}

function directRun(command, args, cwd = repositoryRoot) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  })
  return {
    ...result,
    stdoutText: result.stdout.toString(),
    stderrText: result.stderr.toString(),
  }
}

async function writeAll(file, bytes) {
  let offset = 0
  while (offset < bytes.byteLength) {
    const result = await file.write(bytes, offset, bytes.byteLength - offset)
    if (!Number.isInteger(result.bytesWritten) || result.bytesWritten <= 0)
      fail("archive staging write made no progress")
    offset += result.bytesWritten
  }
}

async function downloadTo(url, destination, expectedSize, expectedSha256) {
  let response
  try {
    response = await fetch(url, { redirect: "follow" })
  } catch (error) {
    fail(`download request failed: ${error.message}`)
  }
  if (!response.ok || response.body === null)
    fail(`download returned HTTP ${response.status}`)
  const contentLength = response.headers.get("content-length")
  if (contentLength !== null && Number(contentLength) !== expectedSize)
    fail(`download Content-Length differs from pinned size: ${contentLength}`)

  const file = await open(destination, "wx")
  const digest = createHash("sha256")
  let byteCount = 0
  try {
    const reader = response.body.getReader()
    while (true) {
      const chunkResult = await reader.read()
      if (chunkResult.done) break
      const chunk = chunkResult.value instanceof Uint8Array
        ? chunkResult.value
        : new Uint8Array(chunkResult.value)
      byteCount += chunk.byteLength
      if (byteCount > expectedSize)
        fail(`download exceeds pinned size at ${byteCount} bytes`)
      digest.update(chunk)
      await writeAll(file, chunk)
    }
    await file.sync()
  } finally {
    await file.close()
  }
  const sha256 = digest.digest("hex")
  if (byteCount !== expectedSize)
    fail(`download size is ${byteCount}, expected ${expectedSize}`)
  if (sha256 !== expectedSha256)
    fail(`download SHA-256 is ${sha256}, expected ${expectedSha256}`)
  return { sizeBytes: byteCount, sha256 }
}

async function hashFile(pathValue) {
  const stream = Bun.file(pathValue).stream()
  const reader = stream.getReader()
  const digest = createHash("sha256")
  let sizeBytes = 0
  while (true) {
    const chunkResult = await reader.read()
    if (chunkResult.done) break
    const chunk = chunkResult.value instanceof Uint8Array
      ? chunkResult.value
      : new Uint8Array(chunkResult.value)
    digest.update(chunk)
    sizeBytes += chunk.byteLength
  }
  return { sizeBytes, sha256: digest.digest("hex") }
}

async function decompressZstdToTar(sourcePath, destinationPath) {
  const compressed = Bun.file(sourcePath).stream()
  const stream = compressed.pipeThrough(new DecompressionStream("zstd"))
  const reader = stream.getReader()
  const file = await open(destinationPath, "wx")
  let sizeBytes = 0
  try {
    while (true) {
      const chunkResult = await reader.read()
      if (chunkResult.done) break
      const chunk = chunkResult.value instanceof Uint8Array
        ? chunkResult.value
        : new Uint8Array(chunkResult.value)
      sizeBytes += chunk.byteLength
      await writeAll(file, chunk)
    }
    await file.sync()
  } finally {
    await file.close()
  }
  if (sizeBytes === 0) fail("Zstandard archive decompressed to an empty tar")
  return { sizeBytes }
}

async function decompressZstdWithCommand(command, sourcePath, destinationPath) {
  const file = await open(destinationPath, "wx")
  let child
  let stderrPromise
  try {
    child = Bun.spawn({
      cmd: [command, "x", "-so", "-tZstd", "-bsp0", sourcePath],
      cwd: repositoryRoot,
      stdout: "pipe",
      stderr: "pipe",
    })
    if (child.stdout === null || child.stderr === null)
      fail("Zstandard decompressor did not expose pipes")
    stderrPromise = new Response(child.stderr).text()
    const reader = child.stdout.getReader()
    let sizeBytes = 0
    try {
      while (true) {
        const chunkResult = await reader.read()
        if (chunkResult.done) break
        const chunk = chunkResult.value instanceof Uint8Array
          ? chunkResult.value
          : new Uint8Array(chunkResult.value)
        sizeBytes += chunk.byteLength
        await writeAll(file, chunk)
      }
      await file.sync()
    } catch (error) {
      child.kill()
      throw error
    }
    const exitCode = await child.exited
    const stderrText = await stderrPromise
    if (exitCode !== 0)
      fail(`Zstandard decompressor failed: ${(stderrText || "").trim()}`)
    if (sizeBytes === 0) fail("Zstandard decompressor produced an empty tar")
    return { sizeBytes }
  } finally {
    await file.close()
    if (child !== undefined && child.exitCode === null) child.kill()
  }
}

async function resolveSevenZip() {
  const candidates = []
  if (process.env.W_MLIR0_7Z !== undefined)
    candidates.push(resolve(process.env.W_MLIR0_7Z))
  const pathCommand = Bun.which("7z.exe") || Bun.which("7z") ||
    Bun.which("7zz.exe") || Bun.which("7zz")
  if (pathCommand) candidates.push(pathCommand)
  if (process.env.ProgramFiles !== undefined)
    candidates.push(join(process.env.ProgramFiles, "7-Zip", "7z.exe"))
  if (process.env.ProgramW6432 !== undefined)
    candidates.push(join(process.env.ProgramW6432, "7-Zip", "7z.exe"))
  for (const candidate of candidates) {
    if (!candidate || !(await pathExists(candidate))) continue
    const stats = await lstat(candidate)
    if (stats.isFile() && !isReparsePoint(stats)) return resolve(candidate)
  }
  return undefined
}

async function diskFree(pathValue) {
  const value = await statfs(pathValue)
  return {
    availableBytes: Number(value.bavail) * Number(value.bsize),
    totalBytes: Number(value.blocks) * Number(value.bsize),
  }
}

function resolveTar() {
  const command = Bun.which("tar.exe") || Bun.which("tar")
  if (!command) fail("tar.exe is required for tar.zst extraction")
  return command
}

function parseTarList(text) {
  return text.split(/\r?\n/u).filter((line) => line.length > 0)
}

async function inspectArchive(archivePath, tarCommand) {
  const namesResult = directRun(tarCommand, ["-tf", archivePath])
  if (namesResult.exitCode !== 0)
    fail(`archive listing failed: ${(namesResult.stderrText || namesResult.stdoutText).trim()}`)
  const verboseResult = directRun(tarCommand, ["-tvf", archivePath])
  if (verboseResult.exitCode !== 0)
    fail(`archive type listing failed: ${(verboseResult.stderrText || verboseResult.stdoutText).trim()}`)
  const names = parseTarList(namesResult.stdoutText)
  const typeLines = parseTarList(verboseResult.stdoutText)
  if (names.length !== typeLines.length)
    fail(`archive list/type count differs: ${names.length}/${typeLines.length}`)
  const entries = []
  for (let index = 0; index < names.length; index += 1) {
    const mode = typeLines[index][0]
    if (mode !== "-" && mode !== "d") {
      const kind = mode === "l" ? "symlink" : mode === "h" ? "hardlink" : "special entry"
      fail(`archive contains a ${kind}: ${JSON.stringify(names[index])}`)
    }
    if (/^(?:\.\/)+$/u.test(names[index]) || names[index] === ".") continue
    entries.push({ path: names[index], type: mode === "d" ? "directory" : "file" })
  }
  try {
    return validateArchiveEntries(entries)
  } catch (error) {
    fail(error.message)
  }
}

function isZstdMemoryLimitError(error) {
  return /Frame requires too much memory for decoding/u.test(error?.message || "")
}

function archiveAllowedPaths(entries) {
  const allowed = new Set()
  for (const entry of entries) {
    const parts = entry.path.split("/")
    for (let count = 1; count <= parts.length; count += 1)
      allowed.add(parts.slice(0, count).join("/"))
  }
  return allowed
}

async function scanTree(root, expectedEntries, extraExpectedPaths = []) {
  const expected = archiveAllowedPaths([
    ...expectedEntries,
    ...extraExpectedPaths.map((pathValue) => ({ path: pathValue, type: "file" })),
  ])
  const files = []
  const actual = []
  async function visit(directory) {
    const children = await readdir(directory, { withFileTypes: true })
    for (const child of children) {
      const absolutePath = join(directory, child.name)
      const stats = await lstat(absolutePath)
      if (isReparsePoint(stats))
        fail(`extracted tree contains a symbolic link or reparse point: ${absolutePath}`)
      const relativePath = relative(root, absolutePath).replaceAll("\\", "/")
      const normalized = normalizeArchivePath(relativePath)
      if (!expected.has(normalized))
        fail(`extracted tree contains an unlisted path: ${normalized}`)
      actual.push({ path: normalized, type: stats.isDirectory() ? "directory" : "file" })
      if (stats.isDirectory()) await visit(absolutePath)
      else if (stats.isFile()) files.push({ path: normalized, absolutePath, stats })
      else fail(`extracted tree contains a non-regular path: ${absolutePath}`)
    }
  }
  await visit(root)
  return { files, entries: actual }
}

export function installedSizeForTree(files) {
  return files
    .filter((file) => file.path !== MATERIALIZED_MANIFEST)
    .reduce((total, file) => total + Number(file.stats.size), 0)
}

async function findToolPaths(root, files, toolManifest) {
  const expectedNames = [...toolManifest.required, ...toolManifest.optional]
  const result = {}
  const missingRequired = []
  for (const name of expectedNames) {
    const matches = files.filter((file) => basename(file.absolutePath) === name)
    if (matches.length > 1)
      fail(`tool basename is ambiguous for ${name}`)
    if (matches.length === 1)
      result[name] = matches[0]
    else if (toolManifest.required.includes(name))
      missingRequired.push(name)
  }
  if (missingRequired.length > 0) {
    const inventory = files
      .map((file) => basename(file.absolutePath))
      .filter((name) => /^(?:clang|llc|mlir-|llvm-config|lld|ld\.lld)/u.test(name))
      .sort()
    fail(`required tool(s) are absent from the inspected archive: ${missingRequired.join(", ")}; ` +
      `candidate executable inventory: ${inventory.join(", ") || "none"}`)
  }
  return result
}

function versionMatches(output, expected) {
  const escaped = expected.replaceAll(/[.*+?^${}()|[\]\\]/gu, "\\$&")
  return new RegExp(`\\b${escaped}\\b`, "u").test(output)
}

function probeTool(toolPath, versionArgs, expectedVersion) {
  const result = directRun(toolPath, versionArgs)
  const output = `${result.stdoutText}\n${result.stderrText}`
  if (result.exitCode !== 0 || !versionMatches(output, expectedVersion))
    fail(`tool version probe failed for ${toolPath}: ${output.trim().slice(-1000)}`)
  return expectedVersion
}

async function collectToolRecords(root, files, toolPaths, toolManifest) {
  const records = {}
  for (const name of [...toolManifest.required, ...toolManifest.optional]) {
    const tool = toolPaths[name]
    if (tool === undefined) {
      records[name] = null
      continue
    }
    const hash = await hashFile(tool.absolutePath)
    let version = null
    let versionStatus = "unverified"
    try {
      version = probeTool(tool.absolutePath, toolManifest.versionArgs,
        toolManifest.expectedVersion)
      versionStatus = "verified"
    } catch (error) {
      if (toolManifest.required.includes(name)) throw error
    }
    records[name] = {
      relativePath: tool.path,
      sizeBytes: hash.sizeBytes,
      sha256: hash.sha256,
      version,
      versionStatus,
    }
  }
  return records
}

function safeRelativePath(value) {
  return typeof value === "string" && value.length > 0 &&
    !isAbsolute(value) && !value.includes("\\") && !value.includes("\0") &&
    !value.split("/").some((part) => part.length === 0 || part === "." || part === "..")
}

async function readMaterialized(destination) {
  const pathValue = join(destination, MATERIALIZED_MANIFEST)
  const stats = await lstat(pathValue)
  if (!stats.isFile() || isReparsePoint(stats))
    fail(`materialized manifest is not a regular file: ${pathValue}`)
  const source = await readFile(pathValue, "utf8")
  try {
    return JSON.parse(source)
  } catch (error) {
    fail(`materialized manifest is not valid JSON: ${error.message}`)
  }
}

export async function validateMaterialized(destination, manifest, pinSha256) {
  await assertNoReparseAncestors(destination)
  const document = await readMaterialized(destination)
  const errors = []
  const add = (condition, message) => { if (!condition) errors.push(message) }
  add(document?.$schema === MATERIALIZED_SCHEMA, "materialized schema is invalid")
  add(document?.version === 1, "materialized version is invalid")
  add(document?.installedSizeDefinition === INSTALLED_SIZE_DEFINITION,
    "materialized installed size definition is invalid")
  add(document?.distributionRole === "development-and-release-only" &&
    document?.bundledWithW === false &&
    document?.extractedSizeIsWBudget === false,
  "materialized distribution boundary is invalid")
  add(document?.manifest?.sha256 === pinSha256,
    "materialized pin digest differs from the checked-in manifest")
  add(document?.asset?.sha256 === manifest.asset.sha256 &&
    document?.asset?.sizeBytes === manifest.asset.sizeBytes,
  "materialized asset identity differs from the pinned asset")
  add(document?.target?.triple === manifest.target.triple,
    "materialized target differs from the pinned target")
  add(resolve(document?.destination || "") === resolve(destination),
    "materialized destination differs from the requested destination")
  const entries = Array.isArray(document?.archiveEntries)
    ? document.archiveEntries : []
  try {
    validateArchiveEntries(entries)
  } catch (error) {
    errors.push(`materialized archive entries are invalid: ${error.message}`)
  }
  if (!isObject(document?.tools)) errors.push("materialized tools are missing")
  if (!Number.isSafeInteger(document?.installedSizeBytes) ||
      document.installedSizeBytes < 1)
    errors.push("materialized installed size is invalid")
  if (errors.length > 0) fail(errors.join("; "))

  const tree = await scanTree(destination, entries, [MATERIALIZED_MANIFEST])
  const installedSizeBytes = installedSizeForTree(tree.files)
  if (installedSizeBytes !== document.installedSizeBytes)
    fail(`installed size changed from ${document.installedSizeBytes} to ${installedSizeBytes}`)
  const expectedNames = [...manifest.tools.required, ...manifest.tools.optional]
  for (const name of expectedNames) {
    const record = document.tools[name]
    if (record === null) {
      if (manifest.tools.required.includes(name)) fail(`required materialized tool is missing: ${name}`)
      continue
    }
    if (!isObject(record) || !safeRelativePath(record.relativePath))
      fail(`materialized tool path is invalid for ${name}`)
    const absolutePath = resolve(destination, record.relativePath)
    if (!isContained(destination, absolutePath))
      fail(`materialized tool escapes destination: ${name}`)
    const stats = await lstat(absolutePath)
    if (!stats.isFile() || isReparsePoint(stats))
      fail(`materialized tool is not a regular file: ${name}`)
    const hash = await hashFile(absolutePath)
    if (hash.sizeBytes !== record.sizeBytes || hash.sha256 !== record.sha256)
      fail(`materialized tool bytes changed: ${name}`)
    if (record.version === null) {
      if (manifest.tools.required.includes(name) ||
          record.versionStatus !== "unverified")
        fail(`materialized required or invalid-version tool record: ${name}`)
      continue
    }
    probeTool(absolutePath, manifest.tools.versionArgs, manifest.tools.expectedVersion)
    if (record.version !== manifest.tools.expectedVersion ||
        record.versionStatus !== "verified")
      fail(`materialized tool version changed: ${name}`)
  }
  const disk = await diskFree(dirname(destination))
  return {
    status: "reused",
    destination: resolve(destination),
    installedSizeBytes,
    diskFreeBytesBefore: document.disk?.freeBytesBefore ?? null,
    diskFreeBytesAfter: disk.availableBytes,
    tools: document.tools,
  }
}

async function materialize(archivePath, destination, manifest, pinSha256) {
  const parent = dirname(destination)
  await assertNoReparseAncestors(parent)
  await mkdir(parent, { recursive: true })
  await assertNoReparseAncestors(parent)
  if (await pathExists(destination)) {
    const stats = await lstat(destination)
    if (isReparsePoint(stats)) fail(`destination is a symbolic link or reparse point: ${destination}`)
    if (stats.isDirectory())
      return validateMaterialized(destination, manifest, pinSha256)
    fail(`destination exists and is not a directory: ${destination}`)
  }

  const diskBefore = await diskFree(parent)
  const tarCommand = resolveTar()
  let archiveForTar = archivePath
  let archiveWorkspace
  let ownedStage
  try {
    let archiveEntries
    try {
      archiveEntries = await inspectArchive(archiveForTar, tarCommand)
    } catch (error) {
      if (!isZstdMemoryLimitError(error)) throw error
      archiveWorkspace = await mkdtemp(join(parent, STAGING_PREFIX))
      archiveForTar = join(archiveWorkspace, "archive.tar")
      let decompressed
      const sevenZip = await resolveSevenZip()
      if (sevenZip !== undefined) {
        decompressed = await decompressZstdWithCommand(sevenZip, archivePath,
          archiveForTar)
        console.log(`MLIR0 Windows toolchain: using explicit native 7-Zip decompressor=${sevenZip}`)
      } else {
        decompressed = await decompressZstdToTar(archivePath, archiveForTar)
      }
      archiveEntries = await inspectArchive(archiveForTar, tarCommand)
      console.log(`MLIR0 Windows toolchain: native tar zstd limit; staged uncompressed tarBytes=${decompressed.sizeBytes}`)
    }

    const stage = await mkdtemp(join(parent, STAGING_PREFIX))
    ownedStage = stage
    const extraction = directRun(tarCommand, ["-xf", archiveForTar, "-C", stage,
      "--no-same-owner", "--no-same-permissions"])
    if (extraction.exitCode !== 0)
      fail(`archive extraction failed: ${(extraction.stderrText || extraction.stdoutText).trim()}`)
    const tree = await scanTree(stage, archiveEntries)
    const tools = await findToolPaths(stage, tree.files, manifest.tools)
    const toolRecords = await collectToolRecords(stage, tree.files, tools, manifest.tools)
    const installedSizeBytes = installedSizeForTree(tree.files)
    const diskAfter = await diskFree(parent)
    const document = {
      $schema: MATERIALIZED_SCHEMA,
      version: 1,
      manifest: {
        path: "tooling/mlir0-windows-toolchain.json",
        sha256: pinSha256,
      },
      asset: {
        provider: manifest.asset.provider,
        release: manifest.asset.release,
        llvmTag: manifest.asset.llvmTag,
        targetTriple: manifest.asset.targetTriple,
        fileName: manifest.asset.fileName,
        sizeBytes: manifest.asset.sizeBytes,
        sha256: manifest.asset.sha256,
      },
      host: manifest.host,
      target: manifest.target,
      destination: resolve(destination),
      distributionRole: manifest.runtimeBoundary.distributionRole,
      bundledWithW: manifest.runtimeBoundary.bundledWithW,
      extractedSizeIsWBudget: manifest.runtimeBoundary.extractedSizeIsWBudget,
      archiveEntries,
      tools: toolRecords,
      installedSizeDefinition: INSTALLED_SIZE_DEFINITION,
      installedSizeBytes,
      disk: {
        freeBytesBefore: diskBefore.availableBytes,
        freeBytesAfter: diskAfter.availableBytes,
        totalBytes: diskAfter.totalBytes,
      },
      trustBoundary: manifest.provenance.trustBoundary,
    }
    await writeFile(join(stage, MATERIALIZED_MANIFEST),
      `${JSON.stringify(document, null, 2)}\n`, { encoding: "utf8", mode: 0o600 })
    await rename(stage, destination)
    ownedStage = undefined
    return validateMaterialized(destination, manifest, pinSha256)
  } finally {
    if (ownedStage !== undefined) await removeOwnedStaging(ownedStage)
    if (archiveWorkspace !== undefined) await removeOwnedStaging(archiveWorkspace)
  }
}

function parseArguments(argumentsList) {
  const options = { archive: undefined, destination: undefined, download: false,
    help: false }
  for (let index = 0; index < argumentsList.length; index += 1) {
    const argument = argumentsList[index]
    if (argument === "--help" || argument === "-h") options.help = true
    else if (argument === "--download") options.download = true
    else if (argument === "--archive" || argument === "--destination") {
      const value = argumentsList[index + 1]
      if (typeof value !== "string" || value.length === 0)
        fail(`${argument} requires a path`)
      options[argument === "--archive" ? "archive" : "destination"] = value
      index += 1
    } else fail(`unknown option: ${argument}`)
  }
  if (options.archive !== undefined && options.download)
    fail("--archive and --download are mutually exclusive")
  return options
}

function usage() {
  return "usage: bun tooling/acquire-mlir0-windows.mjs [--archive <path> | --download] [--destination <path>]"
}

async function main() {
  const options = parseArguments(process.argv.slice(2))
  if (options.help) {
    console.log(usage())
    return
  }
  if (process.platform !== "win32" || process.arch !== "x64")
    fail(`native acquisition requires Windows x86_64, got ${process.platform}/${process.arch}`)
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
  const manifestErrors = validateManifest(manifest)
  if (manifestErrors.length > 0) fail(manifestErrors.join("; "))
  const pinHash = await hashFile(manifestPath)
  const destination = resolve(options.destination || defaultCacheDirectory())
  if (isContained(repositoryRoot, destination))
    fail(`destination must be outside the repository: ${destination}`)
  await assertNoReparseAncestors(dirname(destination))

  if (await pathExists(destination)) {
    const stats = await lstat(destination)
    if (stats.isDirectory()) {
      const reused = await validateMaterialized(destination, manifest, pinHash.sha256)
      console.log(`MLIR0 Windows toolchain: reused destination=${reused.destination} installedBytes=${reused.installedSizeBytes} diskFreeAfter=${reused.diskFreeBytesAfter}`)
      return
    }
    fail(`destination exists and is not a directory: ${destination}`)
  }
  if (options.archive === undefined && !options.download)
    fail(`destination is not materialized: ${destination}; choose --archive <path> or explicit --download`)

  let downloadedWorkspace
  let archivePath
  try {
    if (options.download) {
      downloadedWorkspace = await mkdtemp(join(tmpdir(), DOWNLOAD_PREFIX))
      archivePath = join(downloadedWorkspace, manifest.asset.fileName)
      const downloaded = await downloadTo(manifest.asset.url, archivePath,
        manifest.asset.sizeBytes, manifest.asset.sha256)
      if (downloaded.sizeBytes !== manifest.asset.sizeBytes ||
          downloaded.sha256 !== manifest.asset.sha256)
        fail("download verification did not match the pinned asset")
    } else {
      archivePath = resolve(options.archive)
      const archiveStats = await lstat(archivePath)
      if (!archiveStats.isFile() || isReparsePoint(archiveStats))
        fail(`offline archive is not a regular file: ${archivePath}`)
      const archive = await hashFile(archivePath)
      if (archive.sizeBytes !== manifest.asset.sizeBytes ||
          archive.sha256 !== manifest.asset.sha256)
        fail(`offline archive does not match pinned size/SHA-256: ${archive.sizeBytes}/${archive.sha256}`)
    }
    const result = await materialize(archivePath, destination, manifest, pinHash.sha256)
    console.log(`MLIR0 Windows toolchain: materialized destination=${result.destination} installedBytes=${result.installedSizeBytes} diskFreeBefore=${result.diskFreeBytesBefore} diskFreeAfter=${result.diskFreeBytesAfter}`)
  } finally {
    if (downloadedWorkspace !== undefined) await removeOwnedDownload(downloadedWorkspace)
  }
}

if (import.meta.main) {
  main().catch((error) => {
    console.error(error.message)
    process.exitCode = 1
  })
}
