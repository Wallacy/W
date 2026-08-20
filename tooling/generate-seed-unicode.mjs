import { createHash } from "node:crypto"
import { mkdir, readFile, stat } from "node:fs/promises"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const unicodeVersion = "17.0.0"
const unicodeDirectory = resolve(seedDirectory, "unicode", unicodeVersion)
const sourcePath = join(unicodeDirectory, "DerivedCoreProperties.txt")
const licensePath = join(unicodeDirectory, "LICENSE.txt")
const manifestPath = join(unicodeDirectory, "manifest.json")
const generatedPath = resolve(seedDirectory, "src", "w_seed_unicode_data.c")
const sourceUrl = `https://www.unicode.org/Public/${unicodeVersion}/ucd/DerivedCoreProperties.txt`
const licenseUrl = "https://www.unicode.org/license.txt"
const profileId = "W-Identifier-17.0.0-XID-NO-DICP-UNDERSCORE"
const expectedCodePointCounts = {
  XID_Start: 145893,
  XID_Continue: 149221,
  Default_Ignorable_Code_Point: 4174,
}

function fail(message) {
  throw new Error("seed Unicode: " + message)
}

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex")
}

function parseCodePoint(value) {
  const parsed = Number.parseInt(value, 16)
  if (!Number.isSafeInteger(parsed) || parsed < 0 || parsed > 0x10FFFF) {
    fail("invalid code point " + value)
  }
  return parsed
}

function parseRanges(text, property) {
  const ranges = []
  for (const line of text.split(/\r?\n/u)) {
    const data = line.split("#", 1)[0].trim()
    if (data === "") continue
    const separator = data.indexOf(";")
    if (separator < 0) fail("missing property separator in " + line)
    const rangeText = data.slice(0, separator).trim()
    const propertyText = data.slice(separator + 1).trim()
    if (propertyText !== property) continue
    const parts = rangeText.split("..")
    const start = parseCodePoint(parts[0])
    const end = parts.length === 1 ? start : parseCodePoint(parts[1])
    if (parts.length > 2 || start > end) fail("invalid range " + rangeText)
    ranges.push({ start, end })
  }
  ranges.sort((left, right) => left.start - right.start || left.end - right.end)
  const merged = []
  for (const range of ranges) {
    const previous = merged.at(-1)
    if (previous !== undefined && range.start <= previous.end + 1) {
      previous.end = Math.max(previous.end, range.end)
    } else {
      merged.push({ ...range })
    }
  }
  let codePointCount = 0
  for (const range of merged) codePointCount += range.end - range.start + 1
  if (codePointCount !== expectedCodePointCounts[property]) {
    fail(property + " count " + codePointCount + " != expected " + expectedCodePointCounts[property])
  }
  return { ranges: merged, codePointCount }
}

function cCodePoint(value) {
  return `UINT32_C(0x${value.toString(16).padStart(6, "0").toUpperCase()})`
}

export function validateSourceDocument(sourceText) {
  const firstLine = sourceText.split(/\r?\n/u, 1)[0]
  if (firstLine !== `# DerivedCoreProperties-${unicodeVersion}.txt`) {
    fail("source does not identify DerivedCoreProperties " + unicodeVersion)
  }
  if (!sourceText.includes("# Unicode Character Database")) {
    fail("source does not identify the Unicode Character Database")
  }
}

export function validateLicenseDocument(licenseText) {
  if (!licenseText.startsWith("UNICODE LICENSE V3")) {
    fail("Unicode license does not identify UNICODE LICENSE V3")
  }
}

function renderRanges(name, ranges) {
  const rows = ranges.map(({ start, end }) => `  {${cCodePoint(start)}, ${cCodePoint(end)}},`).join("\n")
  return `const w_seed_unicode_range ${name}[] = {\n${rows}\n};\n`
}

function renderGenerated(properties) {
  return [
    "/* GENERATED FILE. Do not edit by hand. */",
    `/* Unicode ${unicodeVersion}; profile ${profileId}. */`,
    `/* UCD terms: compiler/seed-c/unicode/${unicodeVersion}/LICENSE.txt. */`,
    "#include \"w_seed_unicode.h\"",
    "",
    renderRanges("w_seed_unicode_xid_start_ranges", properties.XID_Start.ranges),
    `const size_t w_seed_unicode_xid_start_range_count = ${properties.XID_Start.ranges.length}u;\n`,
    renderRanges("w_seed_unicode_xid_continue_ranges", properties.XID_Continue.ranges),
    `const size_t w_seed_unicode_xid_continue_range_count = ${properties.XID_Continue.ranges.length}u;\n`,
    renderRanges("w_seed_unicode_default_ignorable_ranges", properties.Default_Ignorable_Code_Point.ranges),
    `const size_t w_seed_unicode_default_ignorable_range_count = ${properties.Default_Ignorable_Code_Point.ranges.length}u;\n`,
  ].join("\n")
}

async function readBytes(path, label) {
  try {
    return new Uint8Array(await readFile(path))
  } catch (error) {
    if (error.code === "ENOENT") fail(label + " is missing; run with --update while online")
    throw error
  }
}

async function fetchBytes(url) {
  const response = await fetch(url)
  if (!response.ok) fail(`download ${url} failed with HTTP ${response.status}`)
  return new Uint8Array(await response.arrayBuffer())
}

function manifestFor(sourceBytes, licenseBytes, generatedBytes, properties) {
  return {
    schema: "w-seed-unicode-manifest-1",
    unicodeVersion,
    profileId,
    source: {
      file: "DerivedCoreProperties.txt",
      url: sourceUrl,
      sha256: sha256(sourceBytes),
      bytes: sourceBytes.length,
    },
    license: {
      file: "LICENSE.txt",
      url: licenseUrl,
      sha256: sha256(licenseBytes),
      bytes: licenseBytes.length,
    },
    properties: Object.fromEntries(Object.entries(properties).map(([name, value]) => [name, {
      codePoints: value.codePointCount,
      ranges: value.ranges.length,
    }])),
    generated: {
      file: "../../src/w_seed_unicode_data.c",
      sha256: sha256(generatedBytes),
      bytes: generatedBytes.length,
    },
  }
}

async function buildArtifacts(sourceBytes, licenseBytes) {
  const sourceText = new TextDecoder("utf-8", { fatal: true }).decode(sourceBytes)
  const licenseText = new TextDecoder("utf-8", { fatal: true }).decode(licenseBytes)
  validateSourceDocument(sourceText)
  validateLicenseDocument(licenseText)
  const properties = Object.fromEntries([
    "XID_Start",
    "XID_Continue",
    "Default_Ignorable_Code_Point",
  ].map((property) => [property, parseRanges(sourceText, property)]))
  const generatedText = renderGenerated(properties)
  const generatedBytes = new TextEncoder().encode(generatedText)
  const manifest = manifestFor(sourceBytes, licenseBytes, generatedBytes, properties)
  const manifestText = JSON.stringify(manifest, null, 2) + "\n"
  return { generatedText, generatedBytes, manifestText }
}

async function writeUpdated() {
  const sourceBytes = await fetchBytes(sourceUrl)
  const licenseBytes = await fetchBytes(licenseUrl)
  const artifacts = await buildArtifacts(sourceBytes, licenseBytes)
  await mkdir(unicodeDirectory, { recursive: true })
  await Bun.write(sourcePath, sourceBytes)
  await Bun.write(licensePath, licenseBytes)
  await Bun.write(generatedPath, artifacts.generatedBytes)
  await Bun.write(manifestPath, artifacts.manifestText)
  console.log(`seed Unicode: updated Unicode ${unicodeVersion} data, generated ranges, and manifest`)
}

async function checkOffline() {
  const sourceBytes = await readBytes(sourcePath, "DerivedCoreProperties.txt")
  const licenseBytes = await readBytes(licensePath, "LICENSE.txt")
  const generatedBytes = await readBytes(generatedPath, "generated classifier data")
  const manifestBytes = await readBytes(manifestPath, "manifest")
  let manifest
  try {
    manifest = JSON.parse(new TextDecoder().decode(manifestBytes))
  } catch {
    fail("manifest is not valid JSON")
  }
  if (manifest.unicodeVersion !== unicodeVersion || manifest.profileId !== profileId) {
    fail("manifest version/profile does not match pinned seed contract")
  }
  if (manifest.source.url !== sourceUrl || manifest.source.file !== "DerivedCoreProperties.txt" ||
      manifest.source.sha256 !== sha256(sourceBytes) || manifest.source.bytes !== sourceBytes.length) {
    fail("source URL, digest, or size does not match manifest")
  }
  if (manifest.license.url !== licenseUrl || manifest.license.file !== "LICENSE.txt" ||
      manifest.license.sha256 !== sha256(licenseBytes) || manifest.license.bytes !== licenseBytes.length) {
    fail("license URL, digest, or size does not match manifest")
  }
  const artifacts = await buildArtifacts(sourceBytes, licenseBytes)
  if (sha256(generatedBytes) !== manifest.generated.sha256 || generatedBytes.length !== manifest.generated.bytes) {
    fail("generated classifier digest or size does not match manifest")
  }
  if (new TextDecoder().decode(generatedBytes) !== artifacts.generatedText) {
    fail("generated classifier is stale; run generator --update")
  }
  if (JSON.stringify(manifest) !== JSON.stringify({
    ...manifestFor(sourceBytes, licenseBytes, artifacts.generatedBytes,
      Object.fromEntries(["XID_Start", "XID_Continue", "Default_Ignorable_Code_Point"].map((property) => [
        property,
        parseRanges(new TextDecoder("utf-8", { fatal: true }).decode(sourceBytes), property),
      ]))),
  })) {
    fail("manifest metadata is stale")
  }
  const sourceStat = await stat(sourcePath)
  const licenseStat = await stat(licensePath)
  if (sourceStat.size !== sourceBytes.length || licenseStat.size !== licenseBytes.length) {
    fail("manifest file sizes differ from on-disk sizes")
  }
  console.log(`seed Unicode: offline Unicode ${unicodeVersion} data/generator check passed`)
}

if (import.meta.main) {
  if (Bun.argv.includes("--update")) {
    await writeUpdated()
  } else {
    await checkOffline()
  }
}
