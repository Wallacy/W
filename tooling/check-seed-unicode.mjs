import { createHash } from "node:crypto"
import { readdir, readFile } from "node:fs/promises"
import { join, resolve } from "node:path"

import {
  validateLicenseDocument,
  validateSourceDocument,
} from "./generate-seed-unicode.mjs"

const root = resolve(import.meta.dir, "..")
const unicodeDirectory = resolve(root, "compiler", "seed-c", "unicode", "17.0.0")
const sourcePath = join(unicodeDirectory, "DerivedCoreProperties.txt")
const licensePath = join(unicodeDirectory, "LICENSE.txt")
const manifestPath = join(unicodeDirectory, "manifest.json")
const generatedPath = resolve(root, "compiler", "seed-c", "src", "w_seed_unicode_data.c")

function fail(message) {
  throw new Error("seed Unicode check: " + message)
}

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex")
}

function expectFailure(label, callback) {
  try {
    callback()
  } catch {
    return
  }
  fail(label + " mutation was accepted")
}

function verifyManifest(manifest, sourceBytes, licenseBytes, generatedBytes) {
  if (manifest.schema !== "w-seed-unicode-manifest-1" ||
      manifest.unicodeVersion !== "17.0.0" ||
      manifest.profileId !== "W-Identifier-17.0.0-XID-NO-DICP-UNDERSCORE") {
    fail("manifest schema/version/profile mismatch")
  }
  if (manifest.source.url !== "https://www.unicode.org/Public/17.0.0/ucd/DerivedCoreProperties.txt" ||
      manifest.source.sha256 !== sha256(sourceBytes) ||
      manifest.source.bytes !== sourceBytes.length) {
    fail("source URL, digest, or byte size mismatch")
  }
  if (manifest.license.url !== "https://www.unicode.org/license.txt" ||
      manifest.license.sha256 !== sha256(licenseBytes) ||
      manifest.license.bytes !== licenseBytes.length) {
    fail("license URL, digest, or byte size mismatch")
  }
  if (manifest.generated.file !== "../../src/w_seed_unicode_data.c" ||
      manifest.generated.sha256 !== sha256(generatedBytes) ||
      manifest.generated.bytes !== generatedBytes.length) {
    fail("generated file digest or byte size mismatch")
  }
  const expectedProperties = {
    XID_Start: { codePoints: 145893, ranges: 691 },
    XID_Continue: { codePoints: 149221, ranges: 806 },
    Default_Ignorable_Code_Point: { codePoints: 4174, ranges: 17 },
  }
  if (JSON.stringify(manifest.properties) !== JSON.stringify(expectedProperties)) {
    fail("generated property counts differ from pinned Unicode 17 profile")
  }
}

function runGenerator() {
  const result = Bun.spawnSync({
    cmd: ["bun", "tooling/generate-seed-unicode.mjs"],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (result.exitCode !== 0) {
    fail("offline generator failed: " + result.stderr.toString().trim())
  }
  return result.stdout.toString()
}

async function main() {
  const sourceBytes = new Uint8Array(await readFile(sourcePath))
  const licenseBytes = new Uint8Array(await readFile(licensePath))
  const generatedBytes = new Uint8Array(await readFile(generatedPath))
  const sourceText = new TextDecoder("utf-8", { fatal: true }).decode(sourceBytes)
  const licenseText = new TextDecoder("utf-8", { fatal: true }).decode(licenseBytes)
  const manifest = JSON.parse(await Bun.file(manifestPath).text())
  verifyManifest(manifest, sourceBytes, licenseBytes, generatedBytes)
  validateSourceDocument(sourceText)
  validateLicenseDocument(licenseText)
  const entries = (await readdir(unicodeDirectory)).sort()
  const expectedEntries = ["DerivedCoreProperties.txt", "LICENSE.txt", "manifest.json"]
  if (JSON.stringify(entries) !== JSON.stringify(expectedEntries)) {
    fail("unexpected vendored Unicode files: " + entries.join(", "))
  }
  if (!new TextDecoder().decode(generatedBytes).startsWith("/* GENERATED FILE. Do not edit by hand. */")) {
    fail("generated classifier is not marked as generated")
  }

  const firstOutput = runGenerator()
  const secondOutput = runGenerator()
  if (firstOutput !== secondOutput) fail("offline generator output is not deterministic")

  const mutatedSource = { ...manifest.source, sha256: "0".repeat(64) }
  expectFailure("source digest", () => verifyManifest(
    { ...manifest, source: mutatedSource }, sourceBytes, licenseBytes, generatedBytes,
  ))
  const mutatedGenerated = { ...manifest.generated, bytes: manifest.generated.bytes + 1 }
  expectFailure("generated size", () => verifyManifest(
    { ...manifest, generated: mutatedGenerated }, sourceBytes, licenseBytes, generatedBytes,
  ))
  expectFailure("Unicode version header", () => validateSourceDocument(
    sourceText.replace("DerivedCoreProperties-17.0.0.txt", "DerivedCoreProperties-16.0.0.txt"),
  ))
  expectFailure("Unicode database header", () => validateSourceDocument(
    sourceText.replace("# Unicode Character Database", "# Not a Unicode data file"),
  ))
  expectFailure("Unicode license version", () => validateLicenseDocument(
    licenseText.replace("UNICODE LICENSE V3", "UNICODE LICENSE V2"),
  ))

  console.log("Seed Unicode: pinned Unicode 17.0.0 data, license, manifest, deterministic generator, and mutation checks passed.")
}

if (import.meta.main) await main()
