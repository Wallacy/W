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
  const lines = output.split(/\r?\n/u)
    .filter((line) => line.startsWith("GENERIC app=") ||
      line.startsWith("STRING app=") || line.startsWith("D3 app="))
  const records = lines.map((line) => {
    const match = /^(GENERIC|STRING|D3) app=(\d+) state=(\w+) failure=([a-z:-]+) diagnostic=(\d+) predicates=(\d+) computed=(\d+) receipts=(\d+) steps=(\d+) receipt_kinds=([CP]*) receipt_steps=([0-9,]*) receipt_args=([0-9,]*) receipt_typed=([0-9,]*) receipt_values=([ibx0-9,]*) module=([^\s]+) head=([^\s]+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) predicate_body_digest=([0-9a-f]{64})$/u.exec(line)
    if (!match) fail(`invalid application line: ${line}`)
    const parseList = (value) => value === "" ? [] : value.split(",").map((item) => Number(item))
    return {
      kind: match[1], application: Number(match[2]), state: match[3], failure: match[4],
      diagnostic: Number(match[5]), predicates: Number(match[6]),
      computed: Number(match[7]), receipts: Number(match[8]), steps: Number(match[9]),
      receiptKinds: match[10], receiptSteps: parseList(match[11]),
      receiptArgs: parseList(match[12]), receiptTyped: parseList(match[13]),
      receiptValues: match[14] === "" ? [] : match[14].split(","),
      module: match[15], head: match[16], fingerprintState: match[17],
      fingerprintDigest: match[18], predicateBodyDigest: match[19],
    }
  })
  return {
    applications: Number(resultMatch[1]), frontend: Number(resultMatch[2]),
    constir: Number(resultMatch[3]),
    records: records.filter((record) => record.kind === "GENERIC"),
    stringRecords: records.filter((record) => record.kind === "STRING"),
    d3Records: records.filter((record) => record.kind === "D3"),
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

function finalCallPreimage(value, predicateBodyDigestHex) {
  const bodyDigest = Uint8Array.from(
    predicateBodyDigestHex.match(/../gu).map((part) => Number.parseInt(part, 16)),
  )
  const type = canonicalScalarType(3)
  const valueEncoding = canonicalScalarValue(3, type, text(value))
  return bytes(
    textEncoder.encode("w-seed-generic-fingerprint-1"),
    u8(0x47), text("restaurant"), text("FinalCallValue"), u32(1),
    u8(0x41), u32(0), u8(0x02), u8(0x56), type, valueEncoding,
    u8(0x01), bodyDigest,
  )
}

function canonicalIntegerType(signed = true, width = 64) {
  return bytes(u8(0x74), u8(5), u8(signed ? 1 : 0), u16(width))
}

function canonicalIntegerValue(value, type, signed = true, width = 64) {
  const byteCount = Math.ceil(width / 8)
  let remaining = BigInt(value)
  const payload = new Uint8Array(byteCount)
  for (let index = 0; index < byteCount; index += 1) {
    payload[index] = Number(remaining & 0xffn)
    remaining >>= 8n
  }
  return bytes(
    u8(0x76), u8(2), type, u8(signed ? 1 : 0), u16(width),
    u8(byteCount), payload,
  )
}

function ultimateAnswerPreimage(value, predicateBodyDigestHex) {
  const bodyDigest = Uint8Array.from(
    predicateBodyDigestHex.match(/../gu).map((part) => Number.parseInt(part, 16)),
  )
  const type = canonicalIntegerType(true, 64)
  const valueEncoding = canonicalIntegerValue(value, type, true, 64)
  return bytes(
    textEncoder.encode("w-seed-generic-fingerprint-1"),
    u8(0x47), text("restaurant"), text("UltimateAnswer"), u32(1),
    u8(0x41), u32(0), u8(0x02), u8(0x56), type, valueEncoding,
    u8(0x01), bodyDigest,
  )
}

function sha256Hex(input) {
  const hasher = new Bun.CryptoHasher("sha256")
  hasher.update(input)
  return hasher.digest("hex")
}

const fingerprintCorpus = JSON.parse(
  await Bun.file(resolve(root, "tooling/generic-fingerprint-cases.json")).text(),
)
if (fingerprintCorpus.$schema !== "w-generic-fingerprint-cases-1" ||
    !Array.isArray(fingerprintCorpus.cases))
  fail("generic fingerprint corpus has the wrong schema")

function requireCorpusCase(id, decisions, sourcePath, sourceSymbol, witnessKeys = []) {
  const matches = fingerprintCorpus.cases.filter((entry) => entry?.id === id)
  if (matches.length !== 1) fail(`${id} case is missing or duplicated`)
  const entry = matches[0]
  if (JSON.stringify(entry.decisions) !== JSON.stringify(decisions) ||
      entry.source?.path !== sourcePath || entry.source?.symbol !== sourceSymbol ||
      entry.oracle?.runner !== "tooling/check-seed-generic-validation.mjs" ||
      !Array.isArray(entry.oracle?.implementations) ||
      !entry.oracle.implementations.includes("seed C") ||
      !entry.oracle.implementations.some((implementation) =>
        typeof implementation === "string" && implementation.startsWith("Bun ")))
    fail(`${id} case has incomplete decision, source, or runner evidence`)
  for (const key of witnessKeys) {
    if (!(key in (entry.witnesses ?? {}))) fail(`${id} case lacks ${key} witness`)
  }
  return entry
}

const fingerprintD1Case = requireCorpusCase(
  "GPF0-W-1460-current", ["W-1460"],
  "reference/last-light/domain.w", "export struct StagePath<",
)
const fingerprintD2Case = requireCorpusCase(
  "GPF0-W-1461-current", ["W-1461"],
  "reference/last-light/generics.w", "export const fn isFinalCallLabel",
  ["positive", "rejected", "overLimit", "corrupt"],
)
const fingerprintD3Case = requireCorpusCase(
  "GPF0-W-1462-current", ["W-1462"],
  "reference/last-light/generics.w",
  "export alias UltimateAnswerComputed = UltimateAnswer<(6 * 7)>",
  ["immediate", "computed", "duplicateComputed", "rejected", "quota", "overflow", "unsupported", "corrupt"],
)
const d1Witnesses = fingerprintD1Case.witnesses
const d2Witnesses = fingerprintD2Case.witnesses
const d3Witnesses = fingerprintD3Case.witnesses
if (d1Witnesses?.module !== "restaurant" ||
    typeof d1Witnesses?.standard !== "string" ||
    typeof d1Witnesses?.standardAgain !== "string" ||
    typeof d1Witnesses?.cancelled !== "string" ||
    !Array.isArray(d1Witnesses?.rejected) || d1Witnesses.rejected.length !== 3 ||
    d2Witnesses?.module !== "restaurant" ||
    d2Witnesses.positive?.value !== "The final seating" ||
    d2Witnesses.positive?.duplicateCount !== 3 ||
    d2Witnesses.positive?.alias !== "VerifiedFinalCall" ||
    d2Witnesses.rejected?.mostlyHarmless !== "Mostly harmless" ||
    d2Witnesses.rejected?.empty !== "" ||
    d2Witnesses.overLimit?.byteCount !== 4097 ||
    d2Witnesses.overLimit?.state !== "UNSUPPORTED" ||
    d2Witnesses.overLimit?.steps !== 0 ||
    d2Witnesses.corrupt?.field !== "first_byte" ||
    d2Witnesses.corrupt?.relation !== "outside const_bytes" ||
    d2Witnesses.corrupt?.state !== "INVALID" ||
    d2Witnesses.corrupt?.steps !== 0 ||
    d3Witnesses?.module !== "restaurant" ||
    d3Witnesses.immediate?.value !== 42 ||
    d3Witnesses.computed?.expression !== "(6 * 7)" ||
    d3Witnesses.duplicateComputed?.expression !== "(6 * 7)" ||
    d3Witnesses.rejected?.value !== 36 ||
    d3Witnesses.overflow?.state !== "EVALUATION_FAILED" ||
    d3Witnesses.unsupported?.state !== "UNSUPPORTED" ||
    d3Witnesses.corrupt?.state !== "INVALID" ||
    d3Witnesses.quota?.state !== "EVALUATION_FAILED")
  fail("generic fingerprint corpus witnesses do not match the executable contract")

const domain = await Bun.file(resolve(root, "reference/last-light/domain.w")).text()
const generics = await Bun.file(resolve(root, "reference/last-light/generics.w")).text()
const staticValueMarker = uniqueMarker(
  generics, "export struct StaticValue<T, _ value: T> {", "StaticValue declaration")
const staticValueBodyMarker = uniqueMarker(
  generics,
  "export struct StaticValue<T, _ value: T> {\n  export const expected = value\n}",
  "StaticValue associated const body")
const enabledFeatureMarker = uniqueMarker(
  generics, "export alias EnabledFeature = StaticValue<Bool, true>", "EnabledFeature alias")
const lastCallLabelMarker = uniqueMarker(
  generics, "export alias LastCallLabel = StaticValue<String, \"The final seating\">", "LastCallLabel alias")
const finalCallPredicateMarker = uniqueMarker(
  generics, "export const fn isFinalCallLabel(value: String): Bool {", "FinalCallValue predicate")
const finalCallValueMarker = uniqueMarker(
  generics, "export struct FinalCallValue<_ value: String<(isFinalCallLabel(.member))>> {", "FinalCallValue declaration")
const verifiedFinalCallMarker = uniqueMarker(
  generics, "export alias VerifiedFinalCall = FinalCallValue<\"The final seating\">", "VerifiedFinalCall alias")
const ultimateAnswerPredicateMarker = uniqueMarker(
  generics, "export const fn isUltimateAnswer(value: i64): Bool {", "UltimateAnswer predicate")
const ultimateAnswerValueMarker = uniqueMarker(
  generics, "export struct UltimateAnswer<_ value: i64<(isUltimateAnswer(.member))>> {", "UltimateAnswer declaration")
const ultimateAnswerImmediateAliasMarker = uniqueMarker(
  generics, "export alias UltimateAnswerImmediate = UltimateAnswer<42>", "UltimateAnswer immediate alias")
const ultimateAnswerComputedAliasMarker = uniqueMarker(
  generics, "export alias UltimateAnswerComputed = UltimateAnswer<(6 * 7)>", "UltimateAnswer computed alias")
const staticValueSignature = staticValueMarker
  .replace(/^export /u, "")
  .replace(/\s*\{$/u, "")
if (!generics.includes(staticValueBodyMarker) ||
    !generics.includes(enabledFeatureMarker) ||
    !generics.includes(lastCallLabelMarker) ||
    !generics.includes(finalCallPredicateMarker) ||
    !generics.includes(finalCallValueMarker) ||
    !generics.includes(verifiedFinalCallMarker) ||
    !generics.includes(ultimateAnswerPredicateMarker) ||
    !generics.includes(ultimateAnswerValueMarker) ||
    !generics.includes(ultimateAnswerImmediateAliasMarker) ||
    !generics.includes(ultimateAnswerComputedAliasMarker))
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
  finalCall: FinalCallValue<"The final seating">
  finalCallAgain: FinalCallValue<"The final seating">
  mostlyHarmless: FinalCallValue<"Mostly harmless">
  emptyCall: FinalCallValue<"">
}
`
/* The real associated-const body is verified above. The seed witness uses an
 * empty body because that body is outside this projection's current gate. */
const staticValueProjection = `${staticValueSignature} {}\n`
const finalCallPredicate = fragment(
  generics, finalCallPredicateMarker, finalCallValueMarker, "FinalCallValue predicate")
const finalCallValue = fragment(
  generics, finalCallValueMarker, "export struct StaticWindow<", "FinalCallValue")
const finalCallValueSignature = finalCallValueMarker
  .replace(/\s*\{$/u, "")
if (!finalCallValue.includes("export const expected = value"))
  fail("FinalCallValue associated const marker is not present in the extracted source")
const ultimateAnswerPredicate = fragment(
  generics, ultimateAnswerPredicateMarker, ultimateAnswerValueMarker,
  "UltimateAnswer predicate")
const ultimateAnswerValue = fragment(
  generics, ultimateAnswerValueMarker, "export struct StaticWindow<",
  "UltimateAnswer")
const ultimateAnswerValueSignature = ultimateAnswerValueMarker
  .replace(/\s*\{$/u, "")
if (!ultimateAnswerValue.includes("export const expected = value"))
  fail("UltimateAnswer associated const marker is not present in the extracted source")
const ultimateAnswerUse = `struct UltimateAnswerUse {
  immediate: UltimateAnswer<42>
  computed: UltimateAnswer<(6 * 7)>
  duplicateComputed: UltimateAnswer<(6 * 7)>
  rejected: UltimateAnswer<(6 * 6)>
}
`
const witness = `${orderId}\n${serviceStage}\n${canMove}\n${isValidStagePath}\n${stagePath}\n${finalCallPredicate}\n${finalCallValueSignature} {}\n${ultimateAnswerPredicate}\n${ultimateAnswerValueSignature} {}\n${staticValueProjection}${enabledFeatureMarker}\n${lastCallLabelMarker}\n${verifiedFinalCallMarker}\n${useSource}\n${ultimateAnswerUse}`

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
  if (parsed.frontend !== 0 || parsed.constir !== 0 ||
      parsed.records.length !== 3 + d1Witnesses.rejected.length)
    fail("domain witness did not produce six clean StagePath applications")
  const expected = [
    "VERIFIED", "VERIFIED", "VERIFIED",
    ...Array(d1Witnesses.rejected.length).fill("REJECTED"),
  ]
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
  const stringValues = [
    ...Array(d2Witnesses.positive.duplicateCount).fill(d2Witnesses.positive.value),
    d2Witnesses.rejected.mostlyHarmless, d2Witnesses.rejected.empty,
  ]
  if (parsed.stringRecords.length !== stringValues.length)
    fail("FinalCallValue witness did not produce five source-backed applications")
  for (const [index, record] of parsed.stringRecords.entries()) {
    const expectedState = index < 3 ? "VERIFIED" : "REJECTED"
    if (record.module !== "restaurant" || record.head !== "FinalCallValue" ||
        record.state !== expectedState || record.predicates !== 1 ||
        record.receipts !== 1 || record.steps === 0 ||
        record.state === "REJECTED" &&
          (record.failure !== "predicate:false" || record.diagnostic !== 4 ||
           record.fingerprintState !== "NOT_AVAILABLE" ||
           record.fingerprintDigest !== "0".repeat(64)) ||
        record.state === "VERIFIED" &&
          (record.failure !== "none" || record.diagnostic !== 0 ||
           record.fingerprintState !== "AVAILABLE" ||
           record.fingerprintDigest !==
             sha256Hex(finalCallPreimage(stringValues[index], record.predicateBodyDigest))))
      fail(`FinalCallValue application ${index} has wrong state or fingerprint`)
  }
  if (parsed.stringRecords[0].fingerprintDigest !==
        parsed.stringRecords[1].fingerprintDigest ||
      parsed.stringRecords[1].fingerprintDigest !==
        parsed.stringRecords[2].fingerprintDigest ||
      parsed.stringRecords[0].predicateBodyDigest !==
        parsed.stringRecords[1].predicateBodyDigest)
    fail("duplicate positive String fingerprints do not match")

  if (parsed.d3Records.length !== 4)
    fail("UltimateAnswer witness did not produce four source-backed applications")
  const d3Values = [d3Witnesses.immediate.value, 42, 42, d3Witnesses.rejected.value]
  const d3ExpectedStates = ["VERIFIED", "VERIFIED", "VERIFIED", "REJECTED"]
  const d3Digests = []
  for (const [index, record] of parsed.d3Records.entries()) {
    const computed = index !== 0
    const expectedDigest = record.state === "VERIFIED"
      ? sha256Hex(ultimateAnswerPreimage(d3Values[index], record.predicateBodyDigest))
      : null
    if (record.module !== d3Witnesses.module || record.head !== "UltimateAnswer" ||
        record.state !== d3ExpectedStates[index] || record.predicates !== 1 ||
        record.computed !== (computed ? 1 : 0) ||
        record.receipts !== (computed ? 2 : 1) ||
        record.steps === 0 ||
        record.receiptKinds !== (computed ? "CP" : "P") ||
        record.receiptSteps.length !== (computed ? 2 : 1) ||
        record.receiptSteps.some((steps) => steps === 0) ||
        record.receiptArgs.length !== record.receipts ||
        record.receiptArgs.some((argument) => argument !== record.receiptArgs[0]) ||
        record.receiptTyped.length !== record.receipts ||
        record.receiptValues.join(",") !==
          (computed ? `${index === 3 ? "i36" : "i42"},${index === 3 ? "b0" : "b1"}` : "b1") ||
        (computed && record.receiptTyped.some((typed) => typed !== index - 1)) ||
        (!computed && record.receiptTyped.some((typed) => typed !== 4294967295)) ||
        (record.state === "REJECTED" &&
          (record.failure !== "predicate:false" || record.diagnostic !== 4 ||
           record.fingerprintState !== "NOT_AVAILABLE" ||
           record.fingerprintDigest !== "0".repeat(64))) ||
        (record.state === "VERIFIED" &&
          (record.failure !== "none" || record.diagnostic !== 0 ||
           record.fingerprintState !== "AVAILABLE" ||
           record.fingerprintDigest !== expectedDigest)))
      fail(`UltimateAnswer application ${index} has wrong state, order, receipt, or fingerprint evidence`)
    if (record.state === "VERIFIED") d3Digests.push(record.fingerprintDigest)
  }
  if (d3Digests.length !== 3 || d3Digests[0] !== d3Digests[1] ||
      d3Digests[1] !== d3Digests[2] ||
      parsed.d3Records[0].predicateBodyDigest !== parsed.d3Records[1].predicateBodyDigest ||
      parsed.d3Records[1].predicateBodyDigest !== parsed.d3Records[2].predicateBodyDigest)
    fail("immediate, computed, and duplicate UltimateAnswer fingerprints do not match")

  const computedSteps = parsed.d3Records[1].receiptSteps[0]
  const predicateSteps = parsed.d3Records[1].receiptSteps[1]
  if (computedSteps + predicateSteps <= 1)
    fail("UltimateAnswer witness did not expose positive computed and predicate steps")
  const cumulativeQuota = computedSteps + predicateSteps - 1
  const cumulativeQuotaParsed = parseProbe(
    run(executable, ["--domain-witness-quota", witnessPath, String(cumulativeQuota)]),
  )
  const cumulativeQuotaRecord = cumulativeQuotaParsed.d3Records.find(
    (record) => record.application === parsed.d3Records[1].application,
  )
  if (!cumulativeQuotaRecord || cumulativeQuotaRecord.state !== d3Witnesses.quota.state ||
      cumulativeQuotaRecord.failure !== "evaluator-diagnostic" ||
      cumulativeQuotaRecord.diagnostic !== 2 || cumulativeQuotaRecord.computed !== 1 ||
      cumulativeQuotaRecord.predicates !== 1 || cumulativeQuotaRecord.receipts !== 2 ||
      cumulativeQuotaRecord.receiptKinds !== d3Witnesses.quota.receiptKinds ||
      cumulativeQuotaRecord.receiptSteps.length !== 2 ||
      cumulativeQuotaRecord.receiptSteps[0] !== computedSteps ||
      cumulativeQuotaRecord.fingerprintState !== "NOT_AVAILABLE" ||
      cumulativeQuotaRecord.fingerprintDigest !== "0".repeat(64))
    fail("cumulative quota did not preserve computed receipt before predicate failure")

  const overflowWitness = `${ultimateAnswerPredicate}\n${ultimateAnswerValueSignature} {}\n` +
    `struct Narrow<_ value: i8> {}\nstruct Use { overflow: Narrow<(127 + 1)> }\n`
  const overflowPath = join(build, "domain-generic-overflow.w")
  await Bun.write(overflowPath, overflowWitness)
  const overflowParsed = parseProbe(run(executable, ["--domain-witness", overflowPath]))
  const overflowRecord = overflowParsed.d3Records.find((record) => record.head === "Narrow")
  if (!overflowRecord || overflowRecord.state !== d3Witnesses.overflow.state ||
      overflowRecord.failure !== "evaluator-diagnostic" ||
      overflowRecord.diagnostic !== 3 || overflowRecord.steps === 0 ||
      overflowRecord.receipts !== 1 || overflowRecord.receiptKinds !== "C" ||
      overflowRecord.fingerprintState !== "NOT_AVAILABLE" ||
      overflowRecord.fingerprintDigest !== "0".repeat(64))
    fail("overflow witness did not preserve the causal ConstIR receipt")

  const unsupportedWitness = `${ultimateAnswerPredicate}\n${ultimateAnswerValueSignature} {}\n` +
    `const fn helper(value: i64): i64 { return value }\n` +
    `struct Use { unsupported: UltimateAnswer<(helper(42))> }\n`
  const unsupportedPath = join(build, "domain-generic-unsupported.w")
  await Bun.write(unsupportedPath, unsupportedWitness)
  const unsupportedParsed = parseProbe(run(executable, ["--domain-witness", unsupportedPath]))
  const unsupportedRecord = unsupportedParsed.d3Records.find(
    (record) => record.head === "UltimateAnswer",
  )
  if (!unsupportedRecord || unsupportedRecord.state !== d3Witnesses.unsupported.state ||
      unsupportedRecord.steps !== 0 || unsupportedRecord.receipts !== 0 ||
      unsupportedRecord.fingerprintState !== "NOT_AVAILABLE" ||
      unsupportedRecord.fingerprintDigest !== "0".repeat(64))
    fail("unsupported call witness did not stop at the synthetic function boundary")

  const corruptOutput = run(executable, ["--domain-witness-d3-corrupt", witnessPath])
  const corruptRecords = corruptOutput.split(/\r?\n/u)
    .filter((line) => line.startsWith("D3_CORRUPT "))
    .map((line) => {
      const match = /^D3_CORRUPT case=([a-z]+) state=(\w+) failure=([a-z:-]+) diagnostic=(\d+) steps=(\d+) receipts=(\d+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64})$/u.exec(line)
      if (!match) fail(`invalid D3 corruption line: ${line}`)
      return {
        case: match[1], state: match[2], failure: match[3],
        diagnostic: Number(match[4]), steps: Number(match[5]),
        receipts: Number(match[6]), fingerprintState: match[7], fingerprintDigest: match[8],
      }
    })
  if (corruptRecords.length !== d3Witnesses.corrupt.cases.length ||
      corruptRecords.some((record, index) =>
        record.case !== d3Witnesses.corrupt.cases[index] ||
        record.state !== d3Witnesses.corrupt.state || record.steps !== 0 ||
        record.receipts !== 0 || record.fingerprintState !== "NOT_AVAILABLE" ||
        record.fingerprintDigest !== "0".repeat(64)))
    fail("D3 origin/relation/type/application corruption did not fail in zero steps")

  const overLimitWitness = `${finalCallPredicate}\n${finalCallValueSignature} {}\n` +
    `struct Use { over: FinalCallValue<"${"x".repeat(d2Witnesses.overLimit.byteCount)}"> }\n`
  const overLimitPath = join(build, "domain-generic-over-limit.w")
  await Bun.write(overLimitPath, overLimitWitness)
  const overLimitParsed = parseProbe(
    run(executable, ["--domain-witness", overLimitPath]),
  )
  if (overLimitParsed.frontend !== 0 || overLimitParsed.constir !== 0 ||
      overLimitParsed.stringRecords.length !== 1 ||
      overLimitParsed.stringRecords[0].state !== d2Witnesses.overLimit.state ||
      overLimitParsed.stringRecords[0].failure !== "value" ||
      overLimitParsed.stringRecords[0].steps !== d2Witnesses.overLimit.steps ||
      overLimitParsed.stringRecords[0].fingerprintState !== "NOT_AVAILABLE" ||
      overLimitParsed.stringRecords[0].fingerprintDigest !== "0".repeat(64))
    fail("over-limit String application did not stop before evaluation")

  const stringCorruptOutput = run(executable, ["--domain-witness-corrupt", witnessPath])
  const corruptLine = stringCorruptOutput.split(/\r?\n/u)
    .find((line) => line.startsWith("STRING_CORRUPT "))
  const corruptMatch = /^STRING_CORRUPT state=(\w+) failure=([a-z:-]+) diagnostic=(\d+) steps=(\d+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64})$/u
    .exec(corruptLine ?? "")
  if (!corruptMatch || corruptMatch[1] !== d2Witnesses.corrupt.state ||
      corruptMatch[2] !== "invalid-input" || corruptMatch[3] !== "0" ||
      corruptMatch[4] !== String(d2Witnesses.corrupt.steps) ||
      corruptMatch[5] !== "NOT_AVAILABLE" ||
      corruptMatch[6] !== "0".repeat(64))
    fail("corrupt String arena relation was not rejected in preflight")
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
