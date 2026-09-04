import { readFile } from "node:fs/promises"
import { isAbsolute, relative, resolve, sep } from "node:path"
import {
  MATERIALIZED_MANIFEST,
  STAGING_PREFIX,
  defaultCacheDirectory,
  validateManifest,
} from "./acquire-mlir0-windows.mjs"

const root = resolve(import.meta.dir, "..")
const manifestPath = resolve(import.meta.dir, "mlir0-windows-toolchain.json")
const linuxManifestPath = resolve(import.meta.dir, "mlir0-toolchain.json")
const acquisitionPath = resolve(import.meta.dir, "acquire-mlir0-windows.mjs")
const smokePath = resolve(import.meta.dir, "smoke-mlir0-windows.mjs")

function fail(message) {
  throw new Error(`MLIR0 Windows acquisition: ${message}`)
}

function assert(condition, message) {
  if (!condition) fail(message)
}

function outsideRepository(pathValue) {
  const relativePath = relative(root, resolve(pathValue))
  return isAbsolute(relativePath) || relativePath === ".." ||
    relativePath.startsWith(`..${sep}`)
}

const manifest = JSON.parse(await readFile(manifestPath, "utf8"))
const errors = validateManifest(manifest)
assert(errors.length === 0, errors.join("; "))
assert(outsideRepository(defaultCacheDirectory()),
  "default cache template must be outside the repository")
assert(!outsideRepository(resolve(root, "tooling")),
  "repository-internal paths must not be classified as outside")
assert(manifest.materializedManifest === undefined,
  "materialized product data must not be added to the checked-in pin")
assert(manifest.materialization.materializedManifest === MATERIALIZED_MANIFEST,
  "materialized manifest name differs from the acquisition contract")
assert(manifest.materialization.stagingPrefix === STAGING_PREFIX,
  "staging prefix differs from the acquisition contract")

const linuxManifest = JSON.parse(await readFile(linuxManifestPath, "utf8"))
assert(linuxManifest.status === "pinned" &&
  linuxManifest.toolchain?.mlir === "20.1.2" &&
  linuxManifest.target?.triple === "x86_64-unknown-linux-gnu" &&
  linuxManifest.windowsNative === false,
"Linux/WSL 20.1.2 profile was not preserved")

const source = await readFile(acquisitionPath, "utf8")
for (const marker of [
  'fetch(url, { redirect: "follow" })',
  'open(destination, "wx")',
  'digest.update(chunk)',
  '"--no-same-owner", "--no-same-permissions"',
  "isReparsePoint",
  "validateMaterialized",
  "await rename(stage, destination)",
  "await rm(pathValue, { recursive: true, force: true })",
])
  assert(source.includes(marker), `acquisition guard is missing: ${marker}`)
for (const forbidden of [
  "process.env.PATH",
  "child_process.exec(",
  "child_process.execSync(",
  "shell: true",
])
  assert(!source.includes(forbidden),
    `acquisition script contains a forbidden runtime boundary: ${forbidden}`)

const smokeSource = await readFile(smokePath, "utf8")
for (const marker of [
  '"-filetype=obj"',
  '"-mtriple=x86_64-pc-windows-msvc"',
  '"/entry:mainCRTStartup"',
  '"/subsystem:console"',
  '"/nodefaultlib"',
  "GetStdHandle",
  "WriteFile",
  "ExitProcess",
  "kernel32.lib",
  "WindowsSdkDir",
])
  assert(smokeSource.includes(marker), `Windows smoke guard is missing: ${marker}`)
assert(!smokeSource.includes("process.env.PATH"),
  "Windows smoke must not depend on PATH")

console.log("MLIR0 Windows acquisition: offline structural checks passed")
