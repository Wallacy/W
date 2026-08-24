import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const executableSuffix = process.platform === "win32" ? ".exe" : ""

function fail(message) {
  throw new Error(`seed generic validation: ${message}`)
}

function run(command, args, options = {}) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
    ...options,
  })
  if (result.exitCode !== 0) {
    const stderr = result.stderr.toString().trim()
    fail(`${command} ${args.join(" ")} failed${stderr ? `: ${stderr}` : ""}`)
  }
  return result.stdout.toString()
}

function fragment(source, startMarker, endMarker, label) {
  const start = source.indexOf(startMarker)
  const end = source.indexOf(endMarker, start + startMarker.length)
  const duplicateStart = source.indexOf(startMarker, start + 1)
  const duplicateEnd = source.indexOf(endMarker, end + 1)
  if (start < 0 || end < 0 || end <= start || duplicateStart >= 0 || duplicateEnd >= 0)
    fail(`${label} markers are missing or duplicated`)
  return source.slice(start, end)
}

function uniqueMarker(source, marker, label) {
  const first = source.indexOf(marker)
  if (first < 0 || source.indexOf(marker, first + marker.length) >= 0)
    fail(`${label} marker is missing or duplicated`)
  return marker
}

function parseProbe(output) {
  const resultLine = output.split(/\r?\n/u).find((line) => line.startsWith("GENERIC_RESULT "))
  if (!resultLine) fail("probe has no GENERIC_RESULT line")
  const resultMatch = /^GENERIC_RESULT applications=(\d+) frontend=(\d+) constir=(\d+)$/u.exec(resultLine)
  if (!resultMatch) fail(`invalid result line: ${resultLine}`)
  const lines = output.split(/\r?\n/u).filter((line) => line.startsWith("GENERIC app="))
  const records = lines.map((line) => {
    const match = /^GENERIC app=(\d+) state=(\w+) failure=([a-z:-]+) diagnostic=(\d+) predicates=(\d+) receipts=(\d+) steps=(\d+) module=([^\s]+) head=([^\s]+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) predicate_body_digest=([0-9a-f]{64})$/u.exec(line)
    if (!match) fail(`invalid application line: ${line}`)
    return {
      application: Number(match[1]), state: match[2], failure: match[3],
      diagnostic: Number(match[4]), predicates: Number(match[5]),
      receipts: Number(match[6]), steps: Number(match[7]), module: match[8],
      head: match[9], fingerprintState: match[10],
      fingerprintDigest: match[11], predicateBodyDigest: match[12],
    }
  })
  return {
    applications: Number(resultMatch[1]), frontend: Number(resultMatch[2]),
    constir: Number(resultMatch[3]), records,
  }
}

const textEncoder = new TextEncoder()

function bytes(...parts) {
  const length = parts.reduce((total, part) => total + part.length, 0)
  const result = new Uint8Array(length)
  let offset = 0
  for (const part of parts) {
    result.set(part, offset)
    offset += part.length
  }
  return result
}

function u8(value) {
  return Uint8Array.of(value)
}

function u16(value) {
  return Uint8Array.of((value >>> 8) & 0xff, value & 0xff)
}

function u32(value) {
  return Uint8Array.of(
    (value >>> 24) & 0xff,
    (value >>> 16) & 0xff,
    (value >>> 8) & 0xff,
    value & 0xff,
  )
}

function text(value) {
  const encoded = textEncoder.encode(value)
  return bytes(u32(encoded.length), encoded)
}

function canonicalEnumType() {
  return bytes(u8(0x74), u8(0x09), text("restaurant"), text("ServiceStage"))
}

function canonicalScalarType(kind) {
  return bytes(u8(0x74), u8(kind))
}

function canonicalScalarValue(kind, type, payload) {
  return bytes(u8(0x76), u8(kind), type, payload)
}

function staticValuePreimage(typeKind, valueKind, stringValue = "") {
  const type = canonicalScalarType(typeKind)
  const value = valueKind === 1
    ? canonicalScalarValue(1, type, u8(1))
    : canonicalScalarValue(3, type, text(stringValue))
  return bytes(
    textEncoder.encode("w-seed-generic-fingerprint-1"),
    u8(0x47), text("restaurant"), text("StaticValue"), u32(2),
    u8(0x41), u32(0), u8(0x01), u8(0x54), type,
    u8(0x41), u32(1), u8(0x02), u8(0x56), type, value, u8(0x00),
  )
}

function canonicalListType() {
  return bytes(u8(0x74), u8(0x0b), canonicalEnumType())
}

function canonicalEnumValue(name) {
  return bytes(u8(0x76), u8(0x04), canonicalEnumType(), text(name))
}

function canonicalListValue(names) {
  return bytes(
    u8(0x76),
    u8(0x05),
    canonicalListType(),
    u32(names.length),
    ...names.map(canonicalEnumValue),
  )
}

function stagePathPreimage(names, predicateBodyDigestHex) {
  const bodyDigest = Uint8Array.from(
    predicateBodyDigestHex.match(/../gu).map((part) => Number.parseInt(part, 16)),
  )
  return bytes(
    textEncoder.encode("w-seed-generic-fingerprint-1"),
    u8(0x47),
    text("restaurant"),
    text("StagePath"),
    u32(1),
    u8(0x41),
    u32(0),
    u8(0x02),
    u8(0x56),
    canonicalListType(),
    canonicalListValue(names),
    u8(0x01),
    bodyDigest,
  )
}

function sha256Hex(input) {
  const hasher = new Bun.CryptoHasher("sha256")
  hasher.update(input)
  return hasher.digest("hex")
}

const domain = await Bun.file(resolve(root, "reference/last-light/domain.w")).text()
const generics = await Bun.file(resolve(root, "reference/last-light/generics.w")).text()
const staticValueMarker = uniqueMarker(
  generics, "export struct StaticValue<T, _ value: T> {", "StaticValue declaration")
const staticValueBodyMarker = uniqueMarker(
  generics, "export const expected = value", "StaticValue associated const body")
const enabledFeatureMarker = uniqueMarker(
  generics, "export alias EnabledFeature = StaticValue<Bool, true>", "EnabledFeature alias")
const lastCallLabelMarker = uniqueMarker(
  generics, "export alias LastCallLabel = StaticValue<String, \"The final seating\">", "LastCallLabel alias")
const staticValueSignature = staticValueMarker
  .replace(/^export /u, "")
  .replace(/\s*\{$/u, "")
if (!generics.includes(staticValueBodyMarker) ||
    !generics.includes(enabledFeatureMarker) ||
    !generics.includes(lastCallLabelMarker))
  fail("generics.w markers are not present in the extracted source")
const orderId = fragment(domain, "export type OrderId = u64", "export type GuestCount", "OrderId")
const serviceStage = fragment(domain, "export enum ServiceStage {", "export alias CancelledStage", "ServiceStage")
const canMove = fragment(domain, "export const fn canMove", "export const fn isValidStagePath", "canMove")
const isValidStagePath = fragment(domain, "export const fn isValidStagePath", "export enum PartySize", "isValidStagePath")
const stagePath = fragment(domain, "export struct StagePath", "export fn standardStagePath", "StagePath")
const standardStagePath = fragment(domain, "export fn standardStagePath", "export struct Guest", "standardStagePath")
const standardPath = /StagePath<(\[[^\]]+\])>/u.exec(standardStagePath)?.[1]
if (!standardPath) fail("domain.w has no standard StagePath path")
const useSource = `struct Use {
  standard: StagePath<${standardPath}>
  standardAgain: StagePath<${standardPath}>
  cancelled: StagePath<[.accepted, .cancelled]>
  empty: StagePath<[]>
  skipped: StagePath<[.accepted, .completed]>
  duplicate: StagePath<[.accepted, .reserving, .reserving]>
}
`
/* The real associated-const body is verified above. The seed witness uses an
 * empty body because that body is outside this projection's current gate. */
const staticValueProjection = `${staticValueSignature} {}\n`
const witness = `${orderId}\n${serviceStage}\n${canMove}\n${isValidStagePath}\n${stagePath}\n${staticValueProjection}${enabledFeatureMarker}\n${lastCallLabelMarker}\n${useSource}`

const build = await mkdtemp(join(tmpdir(), "w-seed-generic-validation-check-"))
const witnessPath = join(build, "domain-generic-witness.w")
try {
  await Bun.write(witnessPath, witness)
  run("cmake", ["-S", seedDirectory, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", build, "--target", "w_seed_generic_validation_tests", "--", "-j", "2"])
  run(join(build, `w_seed_generic_validation_tests${executableSuffix}`), [])
  const executable = join(build, `w_seed_generic_validation_tests${executableSuffix}`)
  const firstOutput = run(executable, ["--domain-witness", witnessPath])
  const secondOutput = run(executable, ["--domain-witness", witnessPath])
  if (firstOutput !== secondOutput) fail("domain witness output is not deterministic")
  const parsed = parseProbe(firstOutput)
  if (parsed.frontend !== 0 || parsed.constir !== 0 || parsed.records.length !== 6)
    fail("domain witness did not produce six clean StagePath applications")
  const expected = ["VERIFIED", "VERIFIED", "VERIFIED", "REJECTED", "REJECTED", "REJECTED"]
  for (const [index, record] of parsed.records.entries()) {
    if (record.module !== "restaurant" || record.head !== "StagePath" ||
        record.state !== expected[index] || record.predicates !== 1 || record.receipts !== 1 ||
        record.steps === 0 || (record.state === "REJECTED" &&
          (record.failure !== "predicate:false" || record.diagnostic !== 4 ||
           record.fingerprintState !== "NOT_AVAILABLE" ||
           record.fingerprintDigest !== "0".repeat(64))) ||
        (record.state === "VERIFIED" && (record.failure !== "none" || record.diagnostic !== 0 ||
          record.fingerprintState !== "AVAILABLE" ||
          record.fingerprintDigest !==
            sha256Hex(stagePathPreimage(
              index === 2 ? ["accepted", "cancelled"] :
                ["accepted", "reserving", "preparing", "serving", "completed"],
              record.predicateBodyDigest,
            )))))
      fail(`domain witness application ${index} has wrong state, facts, or counters`)
  }
  if (parsed.records[0].fingerprintDigest !== parsed.records[1].fingerprintDigest ||
      parsed.records[0].fingerprintDigest === parsed.records[2].fingerprintDigest ||
      parsed.records[0].predicateBodyDigest !== parsed.records[1].predicateBodyDigest ||
      parsed.records[0].predicateBodyDigest !== parsed.records[2].predicateBodyDigest)
    fail("standard and cancelled fingerprint evidence does not match the contract")
  const staticRecords = firstOutput.split(/\r?\n/u)
    .filter((line) => line.startsWith("STATIC "))
    .map((line) => {
      const match = /^STATIC app=(\d+) state=(\w+) failure=([a-z:-]+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64})$/u.exec(line)
      if (!match) fail(`invalid StaticValue line: ${line}`)
      return { application: Number(match[1]), state: match[2], failure: match[3], fingerprintState: match[4], fingerprintDigest: match[5] }
    })
  if (staticRecords.length !== 2 ||
      staticRecords.some((record) => record.state !== "VERIFIED" ||
        record.failure !== "none" || record.fingerprintState !== "AVAILABLE"))
    fail("StaticValue witness did not produce two verified fingerprints")
  const expectedStatic = [
    sha256Hex(staticValuePreimage(2, 1)),
    sha256Hex(staticValuePreimage(3, 3, "The final seating")),
  ]
  for (const [index, record] of staticRecords.entries()) {
    if (record.fingerprintDigest !== expectedStatic[index])
      fail(`StaticValue application ${index} disagrees with independent preimage`)
  }
} finally {
  await rm(build, { recursive: true, force: true })
}

console.log("seed generic validation: ok")
