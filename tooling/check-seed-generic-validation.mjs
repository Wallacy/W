import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import {
  registryFixtureInput,
  verifyRegistry,
} from "./authority-registry-machine.mjs"

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
      line.startsWith("STRING app=") || line.startsWith("D3 app=") ||
      line.startsWith("D4 app=") || line.startsWith("D6 app="))
  const records = lines.map((line) => {
    const match = /^(GENERIC|STRING|D3|D4|D6) app=(\d+) state=(\w+) failure=([a-z:-]+) diagnostic=([0-9]+) predicates=(\d+) computed=(\d+) receipts=(\d+) steps=(\d+) cache_hits=(\d+) cache_misses=(\d+) receipt_kinds=([CP]*) receipt_steps=([0-9,]*) receipt_args=([0-9,]*) receipt_typed=([0-9,]*) receipt_values=([ibx0-9,]*) receipt_cache_hits=([0-9,]*) receipt_cache_misses=([0-9,]*) module=([^\s]+) head=([^\s]+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) specialization_state=(\w+) specialization_written=(\d+) specialization_required=(\d+) specialization_digest=([0-9a-f]{64})(?: specialization_preimage=([0-9a-f]+))? predicate_body_digest=([0-9a-f]{64}) cycle_path=([0-9,]*)$/u.exec(line)
    if (!match) fail(`invalid application line: ${line}`)
    const parseList = (value) => value === "" ? [] : value.split(",").map((item) => Number(item))
    return {
      kind: match[1], application: Number(match[2]), state: match[3], failure: match[4],
      diagnostic: Number(match[5]), predicates: Number(match[6]),
      computed: Number(match[7]), receipts: Number(match[8]), steps: Number(match[9]),
      cacheHits: Number(match[10]), cacheMisses: Number(match[11]),
      receiptKinds: match[12], receiptSteps: parseList(match[13]),
      receiptArgs: parseList(match[14]), receiptTyped: parseList(match[15]),
      receiptValues: match[16] === "" ? [] : match[16].split(","),
      receiptCacheHits: parseList(match[17]), receiptCacheMisses: parseList(match[18]),
      module: match[19], head: match[20], fingerprintState: match[21],
      fingerprintDigest: match[22], specializationState: match[23],
      specializationWritten: Number(match[24]),
      specializationRequired: Number(match[25]),
      specializationDigest: match[26], specializationPreimageHex: match[27] ?? null,
      predicateBodyDigest: match[28], cyclePath: parseList(match[29]),
    }
  })
  for (const record of records) {
    if (record.state !== "VERIFIED")
      assertSpecializationNotAvailable(record, `${record.kind} app ${record.application}`)
  }
  return {
    applications: Number(resultMatch[1]), frontend: Number(resultMatch[2]),
    constir: Number(resultMatch[3]),
    records: records.filter((record) => record.kind === "GENERIC"),
    stringRecords: records.filter((record) => record.kind === "STRING"),
    d3Records: records.filter((record) => record.kind === "D3"),
    d4Records: records.filter((record) => record.kind === "D4"),
    d6Records: records.filter((record) => record.kind === "D6"),
  }
}

function parseNominalOriginMatrix(output) {
  const originRecords = new Map()
  const validationRecords = new Map()
  for (const line of output.split(/\r?\n/u)) {
    if (line.startsWith("ORIGIN ")) {
      const match = /^ORIGIN case=([a-z-]+) state=(\w+) written=(\d+) required=(\d+) digest=([0-9a-f]{64})(?: preimage=([0-9a-f]+))?$/u.exec(line)
      if (!match) fail(`invalid nominal-origin line: ${line}`)
      originRecords.set(match[1], {
        state: match[2], written: Number(match[3]), required: Number(match[4]),
        digest: match[5], preimageHex: match[6] ?? null,
      })
    } else if (line.startsWith("NOMINAL_VALIDATION ")) {
      const match = /^NOMINAL_VALIDATION case=([a-z-]+) state=(\w+) specialization_state=(\w+) specialization_written=(\d+) specialization_required=(\d+) specialization_digest=([0-9a-f]{64})(?: specialization_preimage=([0-9a-f]+))? steps=(\d+) receipts=(\d+) predicate_body_digest=([0-9a-f]{64})$/u.exec(line)
      if (!match) fail(`invalid nominal-validation line: ${line}`)
      validationRecords.set(match[1], {
        state: match[2], specializationState: match[3],
        specializationWritten: Number(match[4]), specializationRequired: Number(match[5]),
        specializationDigest: match[6], specializationPreimageHex: match[7] ?? null,
        steps: Number(match[8]), receipts: Number(match[9]), predicateBodyDigest: match[10],
      })
    }
  }
  const collisionLine = output.split(/\r?\n/u).find((line) => line.startsWith("SPECIALIZATION_COLLISION "))
  const collisionMatch = /^SPECIALIZATION_COLLISION equal=(0|1)$/u.exec(collisionLine ?? "")
  if (!collisionMatch) fail("nominal-origin matrix has no valid forced-collision line")
  return {origins: originRecords, validations: validationRecords, collision: Number(collisionMatch[1])}
}

function combineProbeRecords(...parsed) {
  return {
    applications: parsed.reduce((total, item) => total + item.applications, 0),
    frontend: parsed.every((item) => item.frontend === 0) ? 0 : 1,
    constir: parsed.every((item) => item.constir === 0) ? 0 : 1,
    records: parsed.flatMap((item) => item.records),
    stringRecords: parsed.flatMap((item) => item.stringRecords),
    d3Records: parsed.flatMap((item) => item.d3Records),
    d4Records: parsed.flatMap((item) => item.d4Records),
    d6Records: parsed.flatMap((item) => item.d6Records),
  }
}

function assertCycleRecord(record, witness, label) {
  const diagnosticCodes = {"W-CONST-0002": 2}
  const expectedDiagnostic = diagnosticCodes[witness?.diagnostic]
  if (!record || expectedDiagnostic === undefined ||
      record.state !== witness.state || record.failure !== witness.failure ||
      record.diagnostic !== expectedDiagnostic || record.computed !== witness.computed ||
      record.receipts !== witness.receipts || record.steps !== witness.steps ||
      record.cacheHits !== witness.hits || record.cacheMisses !== witness.misses ||
      (witness.receiptKinds !== undefined &&
       record.receiptKinds !== witness.receiptKinds) ||
      record.fingerprintState !== witness.fingerprintState ||
      record.fingerprintDigest !== "0".repeat(64) ||
      record.cyclePath.join(",") !== witness.path)
    fail(`${label} C witness disagrees with GPF0-W-1466-current`)
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

function authorityBytes(value) {
  if (value instanceof Uint8Array) return value
  return textEncoder.encode(value)
}

function canonicalEnumType(module = "restaurant") {
  return bytes(u8(0x74), u8(0x09), text(module), text("ServiceStage"))
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

function staticValueSpecializationPreimage(
  typeKind, valueKind, stringValue = "", module = "restaurant", authorityValue,
) {
  const type = canonicalScalarType(typeKind)
  const value = valueKind === 1
    ? canonicalScalarValue(1, type, u8(1))
    : canonicalScalarValue(3, type, text(stringValue))
  return specializationPreimage(
    module, "StaticValue",
    [specializationType(), specializationDependentParameter()],
    [specializationType(type), specializationValue(type, value)],
    authorityValue === undefined ? {} : { authority: authorityValue },
  )
}

function canonicalListType(module = "restaurant") {
  return bytes(u8(0x74), u8(0x0b), canonicalEnumType(module))
}

function canonicalEnumValue(name, module = "restaurant") {
  return bytes(u8(0x76), u8(0x04), canonicalEnumType(module), text(name))
}

function canonicalListValue(names, module = "restaurant") {
  return bytes(
    u8(0x76),
    u8(0x05),
    canonicalListType(module),
    u32(names.length),
    ...names.map((name) => canonicalEnumValue(name, module)),
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

function answerPairPreimage(left, right) {
  const type = canonicalIntegerType(true, 64)
  const argument = (ordinal, value) => bytes(
    u8(0x41), u32(ordinal), u8(0x02), u8(0x56), type,
    canonicalIntegerValue(value, type, true, 64), u8(0x00),
  )
  return bytes(
    textEncoder.encode("w-seed-generic-fingerprint-1"),
    u8(0x47), text("restaurant"), text("AnswerPair"), u32(2),
    argument(0, left), argument(1, right),
  )
}

function nominalOriginPreimage(
  authorityValue, packageName, moduleSegments, declarationKind, owners, declaredName,
) {
  const authority = authorityBytes(authorityValue)
  return bytes(
    textEncoder.encode("w-seed-nominal-origin-1"),
    u8(0x4f),
    u8(0x41), u32(authority.length), authority,
    u8(0x50), text(packageName),
    u8(0x4d), u32(moduleSegments.length),
    ...moduleSegments.map((segment) => bytes(u8(0x49), text(segment))),
    u8(0x44), u8(declarationKind), u32(owners.length),
    ...owners.map((owner) => bytes(u8(owner.kind), text(owner.name))),
    text(declaredName),
  )
}

function specializationPreimage(module, head, parameters, substitutions, originOptions = {}) {
  const origin = nominalOriginPreimage(
    originOptions.authority ?? "w-authority-fixture-1|registry=w",
    originOptions.packageName ?? "last-light/restaurant",
    originOptions.moduleSegments ?? [module],
    originOptions.declarationKind ?? 1,
    originOptions.owners ?? [],
    originOptions.declaredName ?? head,
  )
  const declaration = bytes(
    u8(0x44), u32(parameters.length),
    ...parameters.map((parameter, ordinal) => bytes(
      u8(0x50), u32(ordinal), u8(parameter.kind), u8(parameter.domainKind),
      parameter.domainKind === 0
        ? new Uint8Array(0)
        : parameter.domainKind === 1
          ? parameter.domain
          : u32(parameter.dependentOrdinal ?? 0),
      u8(parameter.refinement ? 1 : 0),
      ...(parameter.refinement ? [Uint8Array.from(
        parameter.refinement.match(/../gu).map((part) => Number.parseInt(part, 16)),
      )] : []),
    )),
  )
  const substitution = bytes(
    u8(0x53), u32(substitutions.length),
    ...substitutions.map((argument, ordinal) => bytes(
      u8(0x41), u32(ordinal), u8(argument.kind),
      argument.kind === 1
        ? bytes(u8(0x54), argument.type)
        : bytes(u8(0x56), argument.domain, argument.value),
    )),
  )
  return bytes(
    textEncoder.encode("w-seed-generic-specialization-2"),
    u8(0x49), u8(0x4f), u32(origin.length), origin,
    declaration, substitution, u8(0x57), u32(0),
  )
}

function specializationParameter(domain, refinement = null) {
  return {kind: 2, domainKind: 1, domain, refinement}
}

function specializationDependentParameter(refinement = null, dependentOrdinal = 0) {
  return {kind: 2, domainKind: 2, dependentOrdinal, refinement}
}

function specializationValue(domain, value) {
  return {kind: 2, domain, value}
}

function specializationType(type = null) {
  return type === null ? {kind: 1, domainKind: 0} : {kind: 1, type}
}

function sha256Hex(input) {
  const hasher = new Bun.CryptoHasher("sha256")
  hasher.update(input)
  return hasher.digest("hex")
}

function bytesHex(input) {
  return Array.from(input, (value) => value.toString(16).padStart(2, "0")).join("")
}

function assertSpecializationAvailable(record, expected, label) {
  const expectedHex = bytesHex(expected)
  if (!record || record.specializationState !== "AVAILABLE" ||
      record.specializationWritten !== expected.length ||
      record.specializationRequired !== expected.length ||
      record.specializationDigest !== sha256Hex(expected) ||
      record.specializationPreimageHex === null ||
      record.specializationPreimageHex.length !== expected.length * 2 ||
      record.specializationPreimageHex !== expectedHex)
    fail(`${label} C specialization bytes/digest disagree with independent preimage`)
}

function assertSpecializationNotAvailable(record, label) {
  if (!record || record.specializationState !== "NOT_AVAILABLE" ||
      record.specializationWritten !== 0 || record.specializationRequired !== 0 ||
      record.specializationDigest !== "0".repeat(64) ||
      record.specializationPreimageHex !== null)
    fail(`${label} published an invalid specialization projection`)
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
const fingerprintD4Case = requireCorpusCase(
  "GPF0-W-1463-current", ["W-1463"],
  "reference/last-light/generics.w",
  "export alias UltimateAnswerNamed = UltimateAnswer<(ultimateAnswer)>",
  ["forward", "duplicate", "rejected", "selfCycle", "twoCycle", "threeCycle",
  "unreachable", "typeMismatch", "unresolved", "call", "string", "untyped",
    "imported", "quantity", "size", "corrupt", "zeroCapacity", "quota",
    "dependencyLimit", "arithmeticOverflow", "repeated"],
)
const fingerprintD5Case = requireCorpusCase(
  "GPF0-W-1464-current", ["W-1464"],
  "reference/last-light/generics.w",
  "export alias UltimateAnswerShared = UltimateAnswer<(assembledUltimateAnswer)>",
  ["diamond", "duplicate", "linear", "quota", "failure", "fingerprint", "repeated",
    "preflight"],
)
const fingerprintD6Case = requireCorpusCase(
  "GPF0-W-1465-current", ["W-1465"],
  "reference/last-light/generics.w",
  "export alias ConsistentUltimateAnswer = AnswerPair<(assembledUltimateAnswer), (assembledUltimateAnswer)>",
  ["head", "root", "sourceOrder", "first", "second", "application", "duplicate",
    "quota", "newRun", "failureFirst", "preflight"],
)
const fingerprintD7Case = requireCorpusCase(
  "GPF0-W-1466-current", ["W-1466"],
  "reference/last-light/generics.w", "const answerSeed = 21",
  ["diamond", "integerDefault", "boolean", "suffix", "propagation", "forward",
    "reordered", "equivalent", "cycles", "negative"],
)
const specializationD8Case = requireCorpusCase(
  "GPF0-W-1467-current", ["W-1467"],
  "reference/last-light/generics.w",
  "export alias UltimateAnswerImmediate = UltimateAnswer<42>",
  ["equivalent", "differentHead", "differentModule", "differentRefinement",
    "rejected", "capacity", "ignored", "collision"],
)
const authorityCorpus = JSON.parse(
  await Bun.file(resolve(root, "tooling/authority-registry-cases.json")).text(),
)
const authorityCurrentCase = authorityCorpus.cases.find(
  (entry) => entry?.id === "AUL0-W-1469-current",
)
if (!authorityCurrentCase || JSON.stringify(authorityCurrentCase.decisions) !==
      JSON.stringify(["W-1469"]) ||
    authorityCurrentCase.sourceRef?.path !== "reference/last-light/build.w")
  fail("AUL0 current case is not source-backed by the Last Light origin marker")
const authorityResult = verifyRegistry(
  registryFixtureInput(authorityCorpus.fixtures.registry),
)
const differentAuthorityResult = verifyRegistry(
  registryFixtureInput(authorityCorpus.fixtures["registry-alt"]),
)
const authorityOriginMarker =
  `origin: { object: "${authorityResult.originDigest}", length: ${authorityResult.origin.length} }`
if (authorityCurrentCase.sourceRef?.symbol !== authorityOriginMarker)
  fail("AUL0 current case does not name the full AuthorityOrigin lock marker")
if (authorityResult.status !== "accepted" ||
    authorityResult.code !== "authorityLineageVerified" ||
    authorityResult.continuity.observedRootVersion !== 2 ||
    differentAuthorityResult.status !== "accepted" ||
    differentAuthorityResult.code !== "authorityLineageVerified")
  fail("AUL0 source-backed authority fixtures did not verify")
const sourceAuthorityBytes = new Uint8Array(authorityResult.origin)
const differentAuthorityBytes = new Uint8Array(differentAuthorityResult.origin)
if (bytesHex(sourceAuthorityBytes) === bytesHex(differentAuthorityBytes) ||
    authorityResult.originDigest === differentAuthorityResult.originDigest)
  fail("AUL0 different authority collapsed to the same full-byte origin")
const nominalOriginD9Case = requireCorpusCase(
  "GPF0-W-1468-current", ["W-1468"],
  "reference/last-light/build.w", authorityOriginMarker,
  ["package", "authorityBinding", "modules", "equivalent", "differentAuthority",
    "differentPackage", "differentModule", "differentKind", "differentOwner",
    "differentBody", "excluded", "missing", "corrupt", "capacity", "collision"],
)
const d1Witnesses = fingerprintD1Case.witnesses
const d2Witnesses = fingerprintD2Case.witnesses
const d3Witnesses = fingerprintD3Case.witnesses
const d4Witnesses = fingerprintD4Case.witnesses
const d5Witnesses = fingerprintD5Case.witnesses
const d6Witnesses = fingerprintD6Case.witnesses
const d7Witnesses = fingerprintD7Case.witnesses
const d8Witnesses = specializationD8Case.witnesses
const d9Witnesses = nominalOriginD9Case.witnesses
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
    d3Witnesses.quota?.state !== "EVALUATION_FAILED" ||
    d4Witnesses?.module !== "restaurant" ||
    d4Witnesses.forward?.state !== "VERIFIED" ||
    d4Witnesses.duplicate?.state !== "VERIFIED" ||
    d4Witnesses.rejected?.state !== "REJECTED" ||
    d4Witnesses.selfCycle?.state !== "EVALUATION_FAILED" ||
    d4Witnesses.twoCycle?.path !== "1,2,1" ||
    d4Witnesses.threeCycle?.path !== "1,2,3,1" ||
    d4Witnesses.unreachable?.state !== "VERIFIED" ||
    d4Witnesses.typeMismatch?.state !== "INVALID" ||
    d4Witnesses.unresolved?.state !== "INVALID" ||
    d4Witnesses.call?.state !== "UNSUPPORTED" ||
    d4Witnesses.call?.failure !== "function" ||
    d4Witnesses.string?.state !== "UNSUPPORTED" ||
    d4Witnesses.untyped?.state !== "UNSUPPORTED" ||
    d4Witnesses.imported?.state !== "UNSUPPORTED" ||
    d4Witnesses.quantity?.state !== "UNSUPPORTED" ||
    d4Witnesses.size?.state !== "UNSUPPORTED" ||
    d4Witnesses.corrupt?.state !== "INVALID" ||
    d4Witnesses.zeroCapacity?.state !== "EVALUATION_FAILED" ||
    d4Witnesses.quota?.state !== "EVALUATION_FAILED" ||
    d4Witnesses.dependencyLimit?.state !== "UNSUPPORTED" ||
    d4Witnesses.dependencyLimit?.failure !== "dependency-limit" ||
    d4Witnesses.arithmeticOverflow?.state !== "EVALUATION_FAILED" ||
    d4Witnesses.arithmeticOverflow?.diagnostic !== "W-CONST-0006" ||
    d4Witnesses.arithmeticOverflow?.receiptKinds !== "C" ||
    d4Witnesses.arithmeticOverflow?.receipts !== 1 ||
    d4Witnesses.arithmeticOverflow?.predicates !== 1 ||
    d4Witnesses.repeated?.count !== 2 ||
    d5Witnesses?.module !== "restaurant" ||
    d5Witnesses.diamond?.root !== "assembledUltimateAnswer" ||
    JSON.stringify(d5Witnesses.diamond?.sourceOrder) !==
      JSON.stringify(["answerSeed", "firstAnswerHalf", "secondAnswerHalf", "assembledUltimateAnswer"]) ||
    d5Witnesses.diamond?.value !== 42 ||
    d5Witnesses.diamond?.misses !== 4 || d5Witnesses.diamond?.hits !== 1 ||
    d5Witnesses.diamond?.steps !== 7 || d5Witnesses.duplicate?.count !== 2 ||
    d5Witnesses.duplicate?.misses !== 4 || d5Witnesses.duplicate?.hits !== 1 ||
    d5Witnesses.duplicate?.steps !== 7 || d5Witnesses.linear?.hits !== 0 ||
    d5Witnesses.quota?.allowed !== 7 || d5Witnesses.quota?.rejected !== 6 ||
    d5Witnesses.quota?.diagnostic !== "W-CONST-0003" ||
    d5Witnesses.failure?.diagnostic !== "W-CONST-0006" ||
    d5Witnesses.failure?.cached !== false || d5Witnesses.repeated?.count !== 2 ||
    d5Witnesses.preflight?.selfCycle?.hits !== 0 ||
    d5Witnesses.preflight?.selfCycle?.misses !== 0 ||
    d5Witnesses.preflight?.selfCycle?.steps !== 0 ||
    d5Witnesses.preflight?.selfCycle?.receipts !== 1 ||
    d5Witnesses.preflight?.selfCycle?.receiptHits !== 0 ||
    d5Witnesses.preflight?.selfCycle?.receiptMisses !== 0 ||
    d5Witnesses.preflight?.twoCycle?.hits !== 0 ||
    d5Witnesses.preflight?.twoCycle?.misses !== 0 ||
    d5Witnesses.preflight?.twoCycle?.steps !== 0 ||
    d5Witnesses.preflight?.twoCycle?.receipts !== 1 ||
    d5Witnesses.preflight?.twoCycle?.receiptHits !== 0 ||
    d5Witnesses.preflight?.twoCycle?.receiptMisses !== 0 ||
    d5Witnesses.preflight?.threeCycle?.hits !== 0 ||
    d5Witnesses.preflight?.threeCycle?.misses !== 0 ||
    d5Witnesses.preflight?.threeCycle?.steps !== 0 ||
    d5Witnesses.preflight?.threeCycle?.receipts !== 1 ||
    d5Witnesses.preflight?.threeCycle?.receiptHits !== 0 ||
    d5Witnesses.preflight?.threeCycle?.receiptMisses !== 0 ||
    d5Witnesses.preflight?.zeroCapacity?.hits !== 0 ||
    d5Witnesses.preflight?.zeroCapacity?.misses !== 0 ||
    d5Witnesses.preflight?.zeroCapacity?.steps !== 0 ||
    d5Witnesses.preflight?.zeroCapacity?.receipts !== 0 ||
    d5Witnesses.preflight?.dependencyLimit?.hits !== 0 ||
    d5Witnesses.preflight?.dependencyLimit?.misses !== 0 ||
    d5Witnesses.preflight?.dependencyLimit?.steps !== 0 ||
    d5Witnesses.preflight?.dependencyLimit?.receipts !== 0 ||
    d5Witnesses.preflight?.corruption?.hits !== 0 ||
    d5Witnesses.preflight?.corruption?.misses !== 0 ||
    d5Witnesses.preflight?.corruption?.steps !== 0 ||
    d5Witnesses.preflight?.corruption?.receipts !== 0 ||
    d6Witnesses?.module !== "restaurant" || d6Witnesses.head !== "AnswerPair" ||
    d6Witnesses.member !== "agrees" ||
    d6Witnesses.root !== "assembledUltimateAnswer" ||
    JSON.stringify(d6Witnesses.sourceOrder) !==
      JSON.stringify(["answerSeed", "firstAnswerHalf", "secondAnswerHalf", "assembledUltimateAnswer"]) ||
    d6Witnesses.value !== 42 || d6Witnesses.first?.steps !== 7 ||
    d6Witnesses.first?.misses !== 4 || d6Witnesses.first?.hits !== 1 ||
    d6Witnesses.second?.steps !== 1 || d6Witnesses.second?.misses !== 0 ||
    d6Witnesses.second?.hits !== 1 || d6Witnesses.application?.arguments !== 2 ||
    d6Witnesses.application?.quota !== 8 || d6Witnesses.duplicate?.aliases !== 2 ||
    d6Witnesses.duplicate?.arguments !== 2 || d6Witnesses.quota?.accepted !== 8 ||
    d6Witnesses.quota?.secondRejected !== 7 ||
    d6Witnesses.quota?.diagnostic !== "W-CONST-0003" ||
    d6Witnesses.quota?.secondSteps !== 0 || d6Witnesses.quota?.secondMisses !== 0 ||
    d6Witnesses.quota?.secondHits !== 0 || d6Witnesses.newRun?.firstSteps !== 7 ||
    d6Witnesses.newRun?.secondSteps !== 1 || d6Witnesses.newRun?.firstMisses !== 4 ||
    d6Witnesses.newRun?.secondMisses !== 0 || d6Witnesses.newRun?.firstHits !== 1 ||
    d6Witnesses.newRun?.secondHits !== 1 ||
    d7Witnesses?.module !== "restaurant" ||
    d7Witnesses.diamond?.root !== "assembledUltimateAnswer" ||
    d7Witnesses.diamond?.effectiveType !== "i64" ||
    d7Witnesses.diamond?.declaredType !== null ||
    d7Witnesses.diamond?.explicit !== false ||
    JSON.stringify(d7Witnesses.diamond?.sourceOrder) !==
      JSON.stringify(["answerSeed", "firstAnswerHalf", "secondAnswerHalf", "assembledUltimateAnswer"]) ||
    d7Witnesses.integerDefault?.effectiveType !== "i64" ||
    d7Witnesses.boolean?.effectiveType !== "Bool" ||
    d7Witnesses.suffix?.effectiveType !== "u16" ||
    d7Witnesses.propagation?.effectiveType !== "u16" ||
    d7Witnesses.forward?.value !== d7Witnesses.reordered?.value ||
    d7Witnesses.forward?.effectiveType !== d7Witnesses.reordered?.effectiveType ||
    d7Witnesses.equivalent?.fingerprint !== "same" ||
    d7Witnesses.cycles?.anchored?.diagnostic !== "W-CONST-0002" ||
    d7Witnesses.cycles?.unanchored?.diagnostic !== "W-CONST-0002" ||
    d7Witnesses.cycles?.anchored?.steps !== 0 || d7Witnesses.cycles?.unanchored?.steps !== 0 ||
    d7Witnesses.cycles?.incompatible?.diagnostic !== "W-CONST-0002" ||
    d7Witnesses.cycles?.incompatible?.failure !== "evaluator-diagnostic" ||
    d7Witnesses.cycles?.incompatible?.path !== "0,1,0" ||
    d7Witnesses.cycles?.incompatible?.names !== "left,right,left" ||
    JSON.stringify(d7Witnesses.cycles?.incompatible?.constraints) !==
      JSON.stringify(["Bool", "integer"]) ||
    d7Witnesses.cycles?.incompatible?.computed !== 1 ||
    d7Witnesses.cycles?.incompatible?.receipts !== 1 ||
    d7Witnesses.cycles?.incompatible?.receiptKinds !== "C" ||
    d7Witnesses.cycles?.incompatible?.steps !== 0 ||
    d7Witnesses.cycles?.incompatible?.hits !== 0 ||
    d7Witnesses.cycles?.incompatible?.misses !== 0 ||
    d7Witnesses.cycles?.incompatible?.fingerprintState !== "NOT_AVAILABLE" ||
    d7Witnesses.cycles?.incompatible?.fingerprint !== "zero" ||
    d7Witnesses.cycles?.incompatibleMultiSlot?.computed !== 2 ||
    d7Witnesses.cycles?.incompatibleMultiSlot?.receipts !== 1 ||
    d7Witnesses.cycles?.incompatibleMultiSlotZeroCapacity?.computed !== 2 ||
    d7Witnesses.cycles?.incompatibleMultiSlotZeroCapacity?.receipts !== 0 ||
    d7Witnesses.negative?.unsupported?.state !== "UNSUPPORTED" ||
    d7Witnesses.negative?.mismatch?.state !== "INVALID" ||
    d7Witnesses.negative?.unresolved?.state !== "INVALID" ||
    d6Witnesses.failureFirst?.diagnostic !== "W-CONST-0006" ||
    d6Witnesses.failureFirst?.receipts !== 1 ||
    d6Witnesses.failureFirst?.secondEvaluated !== false ||
    d6Witnesses.preflight?.cycles?.hits !== 0 ||
    d6Witnesses.preflight?.cycles?.misses !== 0 ||
    d6Witnesses.preflight?.cycles?.steps !== 0 ||
    d6Witnesses.preflight?.dependencyLimit?.hits !== 0 ||
    d6Witnesses.preflight?.dependencyLimit?.misses !== 0 ||
    d6Witnesses.preflight?.dependencyLimit?.steps !== 0 ||
    d6Witnesses.preflight?.corruption?.hits !== 0 ||
    d6Witnesses.preflight?.corruption?.misses !== 0 ||
    d6Witnesses.preflight?.corruption?.steps !== 0 ||
    d8Witnesses?.module !== "restaurant" ||
    JSON.stringify(d8Witnesses.heads) !==
      JSON.stringify(["StagePath", "FinalCallValue", "UltimateAnswer", "AnswerPair", "StaticValue"]) ||
    JSON.stringify(d8Witnesses.staticValues) !==
      JSON.stringify(["StaticValue<Bool,true>", "StaticValue<String,\"The final seating\">"]) ||
    d8Witnesses.collision !== "digest igual forçado com preimages diferentes não compara como igual" ||
     d9Witnesses?.authority !== "AUL0-W-1469-current" ||
     d9Witnesses?.package !== "last-light/restaurant" ||
    d9Witnesses.authorityOrigin?.object !== authorityResult.originDigest ||
    d9Witnesses.authorityOrigin?.length !== authorityResult.origin.length ||
    d9Witnesses?.authorityBinding !==
      "AUL0-W-1469-current accepted source-backed AuthorityOrigin" ||
    d9Witnesses?.differentAuthority !==
      "AUL0-W-1469-genesis-different registry-alt full-byte origin" ||
    d9Witnesses.modules?.domain !== "reference/last-light/domain.w" ||
    d9Witnesses.modules?.generics !== "reference/last-light/generics.w" ||
    JSON.stringify(d9Witnesses.domainHeads) !== JSON.stringify(["StagePath"]) ||
     JSON.stringify(d9Witnesses.genericHeads) !==
       JSON.stringify(["FinalCallValue", "UltimateAnswer", "AnswerPair", "StaticValue"]) ||
     JSON.stringify(d9Witnesses.equivalent) !==
       JSON.stringify(["immediate", "computed", "named", "diamond", "duplicate"]) ||
     JSON.stringify(d9Witnesses.excluded) !==
       JSON.stringify(["alias", "version", "revision", "workspace", "checkout/file path",
         "source spelling", "target", "feature", "profile"]) ||
     JSON.stringify(d9Witnesses.corrupt) !==
       JSON.stringify(["truncated", "trailing", "digest", "module", "head", "kind", "owner", "process"]) ||
     JSON.stringify(d9Witnesses.capacity) !==
       JSON.stringify(["zero", "exact", "short-by-one"]) ||
     d9Witnesses.missing !== "VERIFIED + IDENTITY_REQUIRED + 0/0 + digest zero" ||
    !Array.isArray(d9Witnesses.corrupt) ||
    d9Witnesses.collision !== "forced digest collision não iguala preimages distintos")
  fail("generic fingerprint corpus witnesses do not match the executable contract")

const domain = await Bun.file(resolve(root, "reference/last-light/domain.w")).text()
const generics = await Bun.file(resolve(root, "reference/last-light/generics.w")).text()
const buildManifest = await Bun.file(resolve(root, "reference/last-light/build.w")).text()
const buildAuthorityMarker = uniqueMarker(
  buildManifest, 'authority: .registry("w")', "Last Light registry authority marker")
const buildAuthorityOriginMarker = uniqueMarker(
  buildManifest, authorityOriginMarker, "Last Light AuthorityOrigin lock marker")
const buildPackageMarker = uniqueMarker(
  buildManifest,
  'package {\n  schema: "w.package/1"\n  authority: .registry("w")\n  name: "last-light/restaurant"',
  "Last Light package identity marker")
const buildModuleSetMarker = uniqueMarker(
  buildManifest,
  'name: "restaurant-modules"\n      activation: .always\n      root: "."\n      include: ["*.w"]\n      exclude: ["build.w"]\n      layout: .fileStem',
  "Last Light root moduleSet marker")
if (!buildAuthorityMarker || !buildAuthorityOriginMarker || !buildPackageMarker || !buildModuleSetMarker ||
    !buildManifest.includes('name: "last-light/restaurant"'))
  fail("build.w D9 authority/package/moduleSet markers are not source-backed")
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
  generics, "export struct FinalCallValue<_ value: String<(isFinalCallLabel(value: .member))>> {", "FinalCallValue declaration")
const verifiedFinalCallMarker = uniqueMarker(
  generics, "export alias VerifiedFinalCall = FinalCallValue<\"The final seating\">", "VerifiedFinalCall alias")
const ultimateAnswerConstMarker = uniqueMarker(
  generics, "export const ultimateAnswer: i64 = 6 * 7", "ultimateAnswer module const")
const ultimateAnswerPredicateMarker = uniqueMarker(
  generics, "export const fn isUltimateAnswer(value: i64): Bool {", "UltimateAnswer predicate")
const ultimateAnswerValueMarker = uniqueMarker(
  generics, "export struct UltimateAnswer<_ value: i64<(isUltimateAnswer(value: .member))>> {", "UltimateAnswer declaration")
const ultimateAnswerImmediateAliasMarker = uniqueMarker(
  generics, "export alias UltimateAnswerImmediate = UltimateAnswer<42>", "UltimateAnswer immediate alias")
const ultimateAnswerComputedAliasMarker = uniqueMarker(
  generics, "export alias UltimateAnswerComputed = UltimateAnswer<(6 * 7)>", "UltimateAnswer computed alias")
const ultimateAnswerNamedAliasMarker = uniqueMarker(
  generics, "export alias UltimateAnswerNamed = UltimateAnswer<(ultimateAnswer)>", "UltimateAnswer named alias")
const answerSeedMarker = uniqueMarker(
  generics, "const answerSeed = 21", "D7 answerSeed declaration")
const firstAnswerHalfMarker = uniqueMarker(
  generics, "const firstAnswerHalf = answerSeed", "D7 firstAnswerHalf declaration")
const secondAnswerHalfMarker = uniqueMarker(
  generics, "const secondAnswerHalf = answerSeed", "D7 secondAnswerHalf declaration")
const assembledUltimateAnswerMarker = uniqueMarker(
  generics,
  "export const assembledUltimateAnswer = firstAnswerHalf + secondAnswerHalf",
  "D7 assembledUltimateAnswer declaration")
const ultimateAnswerSharedAliasMarker = uniqueMarker(
  generics,
  "export alias UltimateAnswerShared = UltimateAnswer<(assembledUltimateAnswer)>",
  "D5 shared alias")
const ultimateAnswerSharedDuplicateAliasMarker = uniqueMarker(
  generics,
  "export alias UltimateAnswerSharedDuplicate = UltimateAnswer<(assembledUltimateAnswer)>",
  "D5 duplicate shared alias")
const answerPairMarker = uniqueMarker(
  generics, "export struct AnswerPair<_ left: i64, _ right: i64> {",
  "D6 AnswerPair declaration")
const answerPairAgreesMarker = uniqueMarker(
  generics, "  export const agrees = left == right", "D6 AnswerPair agrees member")
const restaurantGenericContractMarker = uniqueMarker(
  generics, "test \"restaurantGenericContractHolds\"", "D6 restaurant contract witness")
const consistentUltimateAnswerAliasMarker = uniqueMarker(
  generics,
  "export alias ConsistentUltimateAnswer = AnswerPair<(assembledUltimateAnswer), (assembledUltimateAnswer)>",
  "D6 shared alias")
const consistentUltimateAnswerDuplicateAliasMarker = uniqueMarker(
  generics,
  "export alias ConsistentUltimateAnswerDuplicate = AnswerPair<(assembledUltimateAnswer), (assembledUltimateAnswer)>",
  "D6 duplicate shared alias")
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
    !generics.includes(ultimateAnswerComputedAliasMarker) ||
    !generics.includes(ultimateAnswerNamedAliasMarker) ||
    !generics.includes(answerSeedMarker) ||
    !generics.includes(firstAnswerHalfMarker) ||
    !generics.includes(secondAnswerHalfMarker) ||
    !generics.includes(assembledUltimateAnswerMarker) ||
    !generics.includes(ultimateAnswerSharedAliasMarker) ||
    !generics.includes(ultimateAnswerSharedDuplicateAliasMarker) ||
    !generics.includes(answerPairMarker) ||
    !generics.includes(answerPairAgreesMarker) ||
    !generics.includes(restaurantGenericContractMarker) ||
    !generics.includes(consistentUltimateAnswerAliasMarker) ||
    !generics.includes(consistentUltimateAnswerDuplicateAliasMarker))
  fail("generics.w markers are not present in the extracted source")
const orderId = fragment(domain, "export type OrderId = u64", "export type GuestCount", "OrderId")
const serviceStage = fragment(domain, "export enum ServiceStage {", "export alias CancelledStage", "ServiceStage")
const canMove = fragment(domain, "export const fn canMove", "export const fn isValidStagePath", "canMove")
const isValidStagePath = fragment(domain, "export const fn isValidStagePath", "export enum PartySize", "isValidStagePath")
const stagePath = fragment(domain, "export struct StagePath", "export fn standardStagePath", "StagePath")
const standardStagePath = fragment(domain, "export fn standardStagePath", "export struct Guest", "standardStagePath")
const standardPath = /StagePath<(\[[^\]]+\])>/u.exec(standardStagePath)?.[1]
if (!standardPath) fail("domain.w has no standard StagePath path")
const stageUseSource = `struct Use {
  let standard: StagePath<${standardPath}>
  let standardAgain: StagePath<${standardPath}>
  let cancelled: StagePath<[.accepted, .cancelled]>
  let empty: StagePath<[]>
  let skipped: StagePath<[.accepted, .completed]>
  let duplicate: StagePath<[.accepted, .reserving, .reserving]>
}
`
const genericUseSource = `struct GenericUse {
  let finalCall: FinalCallValue<"The final seating">
  let finalCallAgain: FinalCallValue<"The final seating">
  let mostlyHarmless: FinalCallValue<"Mostly harmless">
  let emptyCall: FinalCallValue<"">
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
  let immediate: UltimateAnswer<42>
  let computed: UltimateAnswer<(6 * 7)>
  let duplicateComputed: UltimateAnswer<(6 * 7)>
  let rejected: UltimateAnswer<(6 * 6)>
}
`
const ultimateAnswerNamedUse = `struct UltimateAnswerNamedUse {
  let forward: UltimateAnswer<(forwardAnswer)>
  let duplicate: UltimateAnswer<(ultimateAnswer)>
  let rejected: UltimateAnswer<(rejectedAnswer)>
}
`
const domainWitness = `${orderId}\n${serviceStage}\n${canMove}\n${isValidStagePath}\n${stagePath}\n${stageUseSource}`
const genericsWitness = `${finalCallPredicate}\n${finalCallValueSignature} {}\n${ultimateAnswerPredicate}\n${ultimateAnswerValueSignature} {}\n${staticValueProjection}${enabledFeatureMarker}\n${lastCallLabelMarker}\n${verifiedFinalCallMarker}\n${genericUseSource}\n${ultimateAnswerUse}`
const witness = `${domainWitness}\n${genericsWitness}`
const d4Witness = `${ultimateAnswerConstMarker}\n` +
  "const forwardAnswer: i64 = laterAnswer\n" +
  "const laterAnswer: i64 = 42\n" +
  "const rejectedAnswer: i64 = 6 * 6\n" +
  `${ultimateAnswerPredicate}\n${ultimateAnswerValueSignature} {}\n` +
  ultimateAnswerNamedUse
const d5Declarations = `${answerSeedMarker}\n${firstAnswerHalfMarker}\n` +
  `${secondAnswerHalfMarker}\n${assembledUltimateAnswerMarker}\n`
const explicitD5Declarations =
  "const answerSeed: i64 = 21\n" +
  "const firstAnswerHalf: i64 = answerSeed\n" +
  "const secondAnswerHalf: i64 = answerSeed\n" +
  "export const assembledUltimateAnswer: i64 = firstAnswerHalf + secondAnswerHalf\n"
const d5Use = `struct UltimateAnswerSharedUse {
  let shared: UltimateAnswer<(assembledUltimateAnswer)>
  let sharedAgain: UltimateAnswer<(assembledUltimateAnswer)>
}
`
const d5Witness = `${d5Declarations}${ultimateAnswerPredicate}\n` +
  `${ultimateAnswerValueSignature} {}\n${d5Use}`
/* The Last Light associated member is marker-checked above.  The seed
 * witness keeps the body empty because this frontend subset does not lower
 * associated members in this generic evidence fixture. */
const answerPairSeedDeclaration = answerPairMarker.replace(/\s*\{$/u, "{}")
const answerPairSignature = answerPairSeedDeclaration.replace(/^export /u, "")
const d6Use = `struct ConsistentUltimateAnswerUse {
  let first: ConsistentUltimateAnswer
  let second: ConsistentUltimateAnswerDuplicate
}
`
const d6Witness = `${d5Declarations}${answerPairSeedDeclaration}\n` +
  `${consistentUltimateAnswerAliasMarker}\n` +
  `${consistentUltimateAnswerDuplicateAliasMarker}\n${d6Use}`
const d9AnswerPairSource = `${answerPairSeedDeclaration}\n` +
  "struct D9AnswerPairUse {\n" +
  "  let first: AnswerPair<42, 42>\n" +
  "  let second: AnswerPair<42, 42>\n" +
  "}\n"
const genericsSourceWitness = `${genericsWitness}\n${d9AnswerPairSource}`

/* Independent host reconstruction of the D5/D7 diamond. This parser uses only
 * declaration source, accepts an optional annotation, infers effective types,
 * and counts the same ConstIR node classes. It does not read corpus expected
 * values or the C memo counters. */
function reconstructDiamond(source) {
  const declarations = [...source.matchAll(
    /^(?:export )?const ([A-Za-z_][A-Za-z0-9_]*)(?:\s*:\s*([A-Za-z_][A-Za-z0-9_]*))?\s*=\s*(.+)$/gmu,
  )].map((match) => ({
    name: match[1], declaredType: match[2] ?? null, expression: match[3].trim(),
  }))
  const expectedOrder = [
    "answerSeed", "firstAnswerHalf", "secondAnswerHalf", "assembledUltimateAnswer",
  ]
  const sourceOrder = declarations.map((declaration) => declaration.name)
  if (JSON.stringify(sourceOrder) !== JSON.stringify(expectedOrder))
    fail(`D5 source order changed: ${sourceOrder.join(",")}`)
  if (declarations.length !== expectedOrder.length)
    fail(`D7 diamond declaration count changed: ${declarations.length}`)
  const byName = new Map(declarations.map((declaration) => [declaration.name, declaration]))
  const active = new Set()
  const ready = new Map()
  let hits = 0
  let misses = 0
  let steps = 1 // Typed generic expression root CALL.
  function evaluateDeclaration(name) {
    if (ready.has(name)) {
      hits += 1
      return ready.get(name)
    }
    if (active.has(name)) fail(`D5 reconstructed a cycle at ${name}`)
    const declaration = byName.get(name)
    if (!declaration) fail(`D5 has no declaration for ${name}`)
    active.add(name)
    misses += 1
    const expression = declaration.expression
    let result
    const binary = /^([A-Za-z_][A-Za-z0-9_]*)\s*\+\s*([A-Za-z_][A-Za-z0-9_]*)$/u.exec(expression)
    if (binary) {
      steps += 1 // binary node
      const left = evaluateExpression(binary[1])
      const right = evaluateExpression(binary[2])
      if (left.effectiveType !== right.effectiveType || left.effectiveType === "Bool")
        fail(`D7 reconstructed binary has no unique type: ${expression}`)
      result = {value: left.value + right.value, effectiveType: left.effectiveType}
    } else {
      result = evaluateExpression(expression)
    }
    const effectiveType = declaration.declaredType ?? result.effectiveType
    if (declaration.declaredType !== null && declaration.declaredType !== result.effectiveType)
      fail(`D7 annotation disagrees with inferred type for ${name}`)
    if (effectiveType !== "i64")
      fail(`D7 diamond declaration ${name} did not resolve to i64`)
    active.delete(name)
    const value = {
      value: result.value, effectiveType, declaredType: declaration.declaredType,
    }
    declaration.effectiveType = effectiveType
    ready.set(name, value)
    return value
  }
  function evaluateExpression(expression) {
    const integer = /^(\d+)(?:_([iu]\d+))?$/u.exec(expression)
    if (integer) {
      steps += 1 // integer literal node
      return {
        value: Number(integer[1]), effectiveType: integer[2] ?? "i64",
      }
    }
    if (expression === "true" || expression === "false") {
      steps += 1 // boolean literal node
      return {value: expression === "true", effectiveType: "Bool"}
    }
    if (/^[A-Za-z_][A-Za-z0-9_]*$/u.test(expression)) {
      steps += 1 // CALL node
      return evaluateDeclaration(expression)
    }
    fail(`D5 has an unsupported reconstructed expression: ${expression}`)
  }
  const result = evaluateDeclaration("assembledUltimateAnswer")
  return {
    value: result.value, effectiveType: result.effectiveType, sourceOrder,
    declarations: declarations.map(({name, declaredType, effectiveType, expression}) =>
      ({name, declaredType, effectiveType, expression})),
    misses, hits, steps,
  }
}

function reconstructSiblingPair(source) {
  const first = reconstructDiamond(source)
  /* The second typed expression has one root CALL. The root declaration is
   * READY from the first expression, so its CALL is the only new node step. */
  return {
    value: first.value,
    sourceOrder: first.sourceOrder,
    first: {
      steps: first.steps,
      misses: first.misses,
      hits: first.hits,
    },
    second: {steps: 1, misses: 0, hits: 1},
  }
}

/* Small independent D7 solver used for the adversarial scalar witnesses. It
 * deliberately owns its source parser and never consumes C records, receipts,
 * or corpus values. */
function reconstructScalarGraph(source, root) {
  const declarations = [...source.matchAll(
    /^(?:export )?const ([A-Za-z_][A-Za-z0-9_]*)(?:\s*:\s*([A-Za-z_][A-Za-z0-9_]*))?\s*=\s*(.+)$/gmu,
  )].map((match) => ({
    name: match[1], declaredType: match[2] ?? null, expression: match[3].trim(),
  }))
  const byName = new Map(declarations.map((declaration) => [declaration.name, declaration]))
  const active = new Set()
  const ready = new Map()
  function evaluate(name) {
    if (ready.has(name)) return ready.get(name)
    if (active.has(name)) fail(`D7 scalar reconstruction found a cycle at ${name}`)
    const declaration = byName.get(name)
    if (!declaration) fail(`D7 scalar reconstruction has no declaration for ${name}`)
    active.add(name)
    const inferred = evaluateExpression(declaration.expression)
    const effectiveType = declaration.declaredType ?? inferred.effectiveType
    if (declaration.declaredType !== null && declaration.declaredType !== inferred.effectiveType)
      fail(`D7 scalar annotation disagrees for ${name}`)
    const result = {value: inferred.value, effectiveType, declaredType: declaration.declaredType}
    declaration.effectiveType = effectiveType
    active.delete(name)
    ready.set(name, result)
    return result
  }
  function evaluateExpression(expression) {
    const integer = /^(\d+)(?:_([iu]\d+))?$/u.exec(expression)
    if (integer) return {value: Number(integer[1]), effectiveType: integer[2] ?? "i64"}
    if (expression === "true" || expression === "false")
      return {value: expression === "true", effectiveType: "Bool"}
    if (/^[A-Za-z_][A-Za-z0-9_]*$/u.test(expression)) return evaluate(expression)
    const comparison = /^(.+?)\s*(==|!=|<=|>=|<|>)\s*(.+)$/u.exec(expression)
    if (comparison) {
      const left = evaluateExpression(comparison[1].trim())
      const right = evaluateExpression(comparison[3].trim())
      if (left.effectiveType !== right.effectiveType)
        fail(`D7 comparison has no unique type: ${expression}`)
      const values = {
        "==": left.value === right.value, "!=": left.value !== right.value,
        "<": left.value < right.value, "<=": left.value <= right.value,
        ">": left.value > right.value, ">=": left.value >= right.value,
      }
      return {value: values[comparison[2]], effectiveType: "Bool"}
    }
    const binary = /^(.+?)\s*([+\-*/%])\s*(.+)$/u.exec(expression)
    if (binary) {
      const left = evaluateExpression(binary[1].trim())
      const right = evaluateExpression(binary[3].trim())
      if (left.effectiveType !== right.effectiveType || left.effectiveType === "Bool")
        fail(`D7 binary has no unique type: ${expression}`)
      const operations = {
        "+": (a, b) => a + b, "-": (a, b) => a - b, "*": (a, b) => a * b,
        "/": (a, b) => a / b, "%": (a, b) => a % b,
      }
      return {value: operations[binary[2]](left.value, right.value), effectiveType: left.effectiveType}
    }
    fail(`D7 scalar reconstruction has an unsupported expression: ${expression}`)
  }
  const result = evaluate(root)
  return {
    value: result.value,
    effectiveType: result.effectiveType,
    declarations: declarations.map(({name, declaredType, effectiveType, expression}) =>
      ({name, declaredType, effectiveType, expression})),
  }
}

/* Reconstruct only the dependency/constraint witness for a reachable cycle.
 * This parser is intentionally separate from reconstructScalarGraph: an
 * incompatible cycle must remain observable even though no scalar value can
 * be evaluated. */
function reconstructReachableCycle(source, root) {
  const declarations = [...source.matchAll(
    /^(?:export )?const ([A-Za-z_][A-Za-z0-9_]*)(?:\s*:\s*([A-Za-z_][A-Za-z0-9_]*))?\s*=\s*(.+)$/gmu,
  )].map((match) => ({name: match[1], expression: match[3].trim()}))
  const byName = new Map(declarations.map((declaration) => [declaration.name, declaration]))
  const active = []
  const constraints = []
  let cycle = null
  const balancedOuter = (expression) => {
    if (!expression.startsWith("(") || !expression.endsWith(")")) return false
    let depth = 0
    for (let index = 0; index < expression.length; index += 1) {
      if (expression[index] === "(") depth += 1
      if (expression[index] === ")") depth -= 1
      if (depth === 0 && index !== expression.length - 1) return false
    }
    return depth === 0
  }
  const visitExpression = (rawExpression) => {
    if (cycle) return
    let expression = rawExpression.trim()
    while (balancedOuter(expression)) expression = expression.slice(1, -1).trim()
    if (/^[A-Za-z_][A-Za-z0-9_]*$/u.test(expression)) {
      visitDeclaration(expression)
      return
    }
    if (/^(?:true|false|\d+(?:_[iu]\d+)?)$/u.test(expression)) return
    const binary = /^(.+?)\s*(&&|\|\||==|!=|<=|>=|<|>|[+\-*/%])\s*(.+)$/u.exec(expression)
    if (!binary) fail(`D7 cycle reconstruction has an unsupported expression: ${expression}`)
    constraints.push(binary[2] === "&&" || binary[2] === "||" ? "Bool" :
      binary[2] === "==" || binary[2] === "!=" || binary[2] === "<" ||
      binary[2] === "<=" || binary[2] === ">" || binary[2] === ">=" ? "comparison" : "integer")
    visitExpression(binary[1])
    visitExpression(binary[3])
  }
  function visitDeclaration(name) {
    if (cycle) return
    const activeIndex = active.indexOf(name)
    if (activeIndex >= 0) {
      cycle = [...active.slice(activeIndex), name]
      return
    }
    const declaration = byName.get(name)
    if (!declaration) fail(`D7 cycle reconstruction has no declaration for ${name}`)
    active.push(name)
    visitExpression(declaration.expression)
    active.pop()
  }
  visitDeclaration(root)
  return {cycle: cycle ?? [], constraints}
}

const reconstructedDiamond = reconstructDiamond(d5Declarations)
const reconstructedExplicitDiamond = reconstructDiamond(explicitD5Declarations)
if (reconstructedDiamond.value !== d7Witnesses.diamond.value ||
    reconstructedDiamond.effectiveType !== d7Witnesses.diamond.effectiveType ||
    reconstructedDiamond.declarations.some((declaration) =>
      declaration.declaredType !== null || declaration.effectiveType !== "i64") ||
    reconstructedExplicitDiamond.value !== reconstructedDiamond.value ||
    reconstructedExplicitDiamond.effectiveType !== reconstructedDiamond.effectiveType ||
    reconstructedExplicitDiamond.declarations.some((declaration) =>
      declaration.declaredType !== "i64" || declaration.effectiveType !== "i64"))
  fail("D7 diamond annotations or effective types do not reconstruct independently")

const d7IntegerDefault = reconstructScalarGraph(
  "const integerDefault = 4096\n", "integerDefault")
const d7Boolean = reconstructScalarGraph(
  "const booleanValue = true == true\n", "booleanValue")
const d7Suffix = reconstructScalarGraph(
  "const suffixValue = 7_u16\n", "suffixValue")
const d7Propagation = reconstructScalarGraph(
  "const suffixValue = 7_u16\nconst propagatedValue = suffixValue\n", "propagatedValue")
const d7Forward = reconstructScalarGraph(
  "const forwardValue = laterValue\nconst laterValue = 42\n", "forwardValue")
const d7Reordered = reconstructScalarGraph(
  "const laterValue = 42\nconst forwardValue = laterValue\n", "forwardValue")
const d7Inferred = reconstructScalarGraph("const equivalentValue = 42\n", "equivalentValue")
const d7Explicit = reconstructScalarGraph(
  "const equivalentValue: i64 = 42\n", "equivalentValue")
const d7IncompatibleCycle = reconstructReachableCycle(
  "const left = right && true\nconst right = left + 1\n", "left")
if (d7IntegerDefault.effectiveType !== d7Witnesses.integerDefault.effectiveType ||
    d7Boolean.effectiveType !== d7Witnesses.boolean.effectiveType ||
    d7Suffix.effectiveType !== d7Witnesses.suffix.effectiveType ||
    d7Propagation.effectiveType !== d7Witnesses.propagation.effectiveType ||
    d7Forward.value !== d7Reordered.value ||
    d7Forward.effectiveType !== d7Reordered.effectiveType ||
    d7Forward.effectiveType !== d7Witnesses.forward.effectiveType ||
    d7Inferred.value !== d7Explicit.value ||
    d7Inferred.effectiveType !== d7Explicit.effectiveType ||
    d7Inferred.declarations[0].declaredType !== null ||
    d7Explicit.declarations[0].declaredType !== "i64" ||
    d7IncompatibleCycle.cycle.join(",") !== d7Witnesses.cycles.incompatible.names ||
    JSON.stringify(d7IncompatibleCycle.constraints) !==
      JSON.stringify(d7Witnesses.cycles.incompatible.constraints))
  fail("D7 scalar inference witnesses disagree with the independent Bun solver")

const build = await mkdtemp(join(tmpdir(), "w-seed-generic-validation-check-"))
const witnessPath = join(build, "domain-generic-witness.w")
const domainWitnessPath = join(build, "domain-witness.w")
const genericsWitnessPath = join(build, "generics-witness.w")
try {
  await Bun.write(witnessPath, witness)
  await Bun.write(domainWitnessPath, domainWitness)
  await Bun.write(genericsWitnessPath, genericsSourceWitness)
  run("cmake", ["-S", seedDirectory, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", build, "--target", "w_seed_generic_validation_tests", "--", "-j", "2"])
  run(join(build, `w_seed_generic_validation_tests${executableSuffix}`), [])
  const executable = join(build, `w_seed_generic_validation_tests${executableSuffix}`)
  const nominalMatrix = parseNominalOriginMatrix(
    run(executable, ["--nominal-origin-matrix", bytesHex(sourceAuthorityBytes),
      bytesHex(differentAuthorityBytes)]),
  )
  const expectedOriginCases = {
    base: [sourceAuthorityBytes, "last-light/restaurant", ["domain"], 1, [], "Box"],
    authority: [differentAuthorityBytes, "last-light/restaurant", ["domain"], 1, [], "Box"],
    package: [sourceAuthorityBytes, "other/restaurant", ["domain"], 1, [], "Box"],
    module: [sourceAuthorityBytes, "last-light/restaurant", ["generics"], 1, [], "Box"],
    kind: [sourceAuthorityBytes, "last-light/restaurant", ["domain"], 2, [], "Box"],
    owner: [sourceAuthorityBytes, "last-light/restaurant", ["domain"], 1, [{kind: 1, name: "Outer"}], "Box"],
  }
  const expectedOriginLengths = new Map()
  for (const [caseName, fields] of Object.entries(expectedOriginCases)) {
    const expected = nominalOriginPreimage(...fields)
    expectedOriginLengths.set(caseName, expected.length)
    const record = nominalMatrix.origins.get(caseName)
    if (!record || record.state !== "AVAILABLE" || record.written !== expected.length ||
        record.required !== expected.length || record.digest !== sha256Hex(expected) ||
        record.preimageHex !== bytesHex(expected))
      fail(`nominal-origin ${caseName} C bytes disagree with independent Bun preimage`)
  }
  const baseOriginLength = expectedOriginLengths.get("base")
  for (const caseName of ["short", "zero"]) {
    const record = nominalMatrix.origins.get(caseName)
    if (!record || record.state !== "CAPACITY" || record.written !== 0 ||
        record.required !== baseOriginLength || record.digest !== "0".repeat(64) ||
        record.preimageHex !== null)
      fail(`nominal-origin ${caseName} did not preserve the no-partial-write contract`)
  }
  for (const [caseName, state] of [["null", "INVALID"], ["unicode", "UNSUPPORTED"]]) {
    const record = nominalMatrix.origins.get(caseName)
    if (!record || record.state !== state || record.written !== 0 || record.required !== 0 ||
        record.digest !== "0".repeat(64) || record.preimageHex !== null)
      fail(`nominal-origin ${caseName} did not preserve its lifecycle state`)
  }
  if (nominalMatrix.collision !== 0)
    fail("forced SHA-256 collision incorrectly made distinct specializations equal")

  const availableValidation = nominalMatrix.validations.get("available")
  const expectedAvailableSpecialization = specializationPreimage(
    "domain", "Box",
    [specializationParameter(
      canonicalIntegerType(true, 64), availableValidation?.predicateBodyDigest,
    )],
    [specializationValue(
      canonicalIntegerType(true, 64),
      canonicalIntegerValue(42, canonicalIntegerType(true, 64), true, 64),
    )],
    { authority: sourceAuthorityBytes },
  )
  if (!availableValidation || availableValidation.state !== "VERIFIED" ||
      availableValidation.steps !== 1 || availableValidation.receipts !== 1)
    fail("nominal-origin available validation did not verify its source-backed application")
  assertSpecializationAvailable(
    {
      specializationState: availableValidation.specializationState,
      specializationWritten: availableValidation.specializationWritten,
      specializationRequired: availableValidation.specializationRequired,
      specializationDigest: availableValidation.specializationDigest,
      specializationPreimageHex: availableValidation.specializationPreimageHex,
    },
    expectedAvailableSpecialization,
    "nominal-origin available",
  )
  const bodyValidation = nominalMatrix.validations.get("body")
  const expectedBodySpecialization = specializationPreimage(
    "domain", "Box",
    [specializationParameter(
      canonicalIntegerType(true, 64), bodyValidation?.predicateBodyDigest,
    )],
    [specializationValue(
      canonicalIntegerType(true, 64),
      canonicalIntegerValue(42, canonicalIntegerType(true, 64), true, 64),
    )],
    { authority: sourceAuthorityBytes },
  )
  if (!bodyValidation || bodyValidation.state !== "VERIFIED" ||
      bodyValidation.steps === 0 || bodyValidation.receipts !== 1 ||
      bodyValidation.predicateBodyDigest === availableValidation.predicateBodyDigest)
    fail("predicate/refinement body variant did not preserve origin and change specialization")
  assertSpecializationAvailable(
    {
      specializationState: bodyValidation.specializationState,
      specializationWritten: bodyValidation.specializationWritten,
      specializationRequired: bodyValidation.specializationRequired,
      specializationDigest: bodyValidation.specializationDigest,
      specializationPreimageHex: bodyValidation.specializationPreimageHex,
    },
    expectedBodySpecialization,
    "nominal-origin predicate body variant",
  )
  const specializationOriginPrefixLength =
    textEncoder.encode("w-seed-generic-specialization-2").length + 2 + 4
  const expectedOriginBytes = nominalOriginPreimage(
    sourceAuthorityBytes, "last-light/restaurant", ["domain"],
    1, [], "Box",
  )
  if (bytesHex(expectedAvailableSpecialization.slice(
        specializationOriginPrefixLength,
        specializationOriginPrefixLength + expectedOriginBytes.length,
      )) !== bytesHex(expectedBodySpecialization.slice(
        specializationOriginPrefixLength,
        specializationOriginPrefixLength + expectedOriginBytes.length,
      )))
    fail("predicate body variant changed nominal origin bytes")
  const missingValidation = nominalMatrix.validations.get("missing")
  if (!missingValidation || missingValidation.state !== "VERIFIED" ||
      missingValidation.specializationState !== "IDENTITY_REQUIRED" ||
      missingValidation.specializationWritten !== 0 || missingValidation.specializationRequired !== 0 ||
      missingValidation.specializationDigest !== "0".repeat(64) ||
      missingValidation.specializationPreimageHex !== null ||
      missingValidation.steps !== 1 || missingValidation.receipts !== 1)
    fail("missing nominal origin did not publish VERIFIED + IDENTITY_REQUIRED with zero bytes")
  for (const caseName of [
    "truncated", "trailing", "digest", "module", "head", "kind", "owner", "process",
  ]) {
    const record = nominalMatrix.validations.get(caseName)
    if (!record || record.state !== "INVALID" || record.steps !== 0 || record.receipts !== 0)
      fail(`malformed nominal-origin ${caseName} was evaluated before rejection`)
    assertSpecializationNotAvailable({
      specializationState: record.specializationState,
      specializationWritten: record.specializationWritten,
      specializationRequired: record.specializationRequired,
      specializationDigest: record.specializationDigest,
      specializationPreimageHex: record.specializationPreimageHex,
    }, `nominal-origin ${caseName}`)
  }
  for (const caseName of ["short", "zero"]) {
    const record = nominalMatrix.validations.get(caseName)
    if (!record || record.state !== "VERIFIED" || record.steps !== 1 || record.receipts !== 1 ||
        record.specializationState !== "CAPACITY" || record.specializationWritten !== 0 ||
        record.specializationRequired !== expectedAvailableSpecialization.length ||
        record.specializationDigest !== "0".repeat(64) || record.specializationPreimageHex !== null)
      fail(`specialization capacity ${caseName} did not preserve buffers or counters`)
  }
  const sourceAuthorityHex = bytesHex(sourceAuthorityBytes)
  const firstOutput = run(executable, ["--domain-witness", witnessPath,
    sourceAuthorityHex])
  const secondOutput = run(executable, ["--domain-witness", witnessPath,
    sourceAuthorityHex])
  if (firstOutput !== secondOutput) fail("domain witness output is not deterministic")
  const parsed = parseProbe(firstOutput)
  const firstDomainOutput = run(executable, ["--domain-witness-module", domainWitnessPath,
    "domain", sourceAuthorityHex])
  const secondDomainOutput = run(executable, ["--domain-witness-module", domainWitnessPath,
    "domain", sourceAuthorityHex])
  const firstGenericsOutput = run(executable, ["--domain-witness-module", genericsWitnessPath,
    "generics", sourceAuthorityHex])
  const secondGenericsOutput = run(executable, ["--domain-witness-module", genericsWitnessPath,
    "generics", sourceAuthorityHex])
  if (firstDomainOutput !== secondDomainOutput ||
      firstGenericsOutput !== secondGenericsOutput)
    fail("domain/generics witness output is not deterministic")
  const domainParsed = parseProbe(firstDomainOutput)
  const genericsParsed = parseProbe(firstGenericsOutput)
  if (domainParsed.records.some((record) => record.module !== "domain") ||
      genericsParsed.stringRecords.some((record) => record.module !== "generics") ||
      genericsParsed.d3Records.some((record) => record.module !== "generics"))
    fail("D9 source-backed witnesses did not bind to domain/generics module origins")
  const d9StringValues = [
    ...Array(d2Witnesses.positive.duplicateCount).fill(d2Witnesses.positive.value),
    d2Witnesses.rejected.mostlyHarmless, d2Witnesses.rejected.empty,
  ]
  if (domainParsed.records.length !== 3 + d1Witnesses.rejected.length ||
      genericsParsed.stringRecords.length !== d9StringValues.length ||
      genericsParsed.d3Records.length !== 4 || genericsParsed.d6Records.length !== 2)
    fail(`D9 source-backed domain/generics witness split changed application coverage: domain=${domainParsed.records.length}, strings=${genericsParsed.stringRecords.length}, d3=${genericsParsed.d3Records.length}, d6=${genericsParsed.d6Records.length}`)
  for (const [index, record] of domainParsed.records.entries()) {
    if (record.state !== "VERIFIED") continue
    const stageNames = index === 2
      ? ["accepted", "cancelled"]
      : ["accepted", "reserving", "preparing", "serving", "completed"]
    const expected = specializationPreimage(
      "domain", "StagePath",
      [specializationParameter(canonicalListType("domain"), record.predicateBodyDigest)],
      [specializationValue(canonicalListType("domain"), canonicalListValue(stageNames, "domain"))],
      { authority: sourceAuthorityBytes },
    )
    assertSpecializationAvailable(record, expected, `D9 domain StagePath ${index}`)
  }
  for (const [index, record] of genericsParsed.stringRecords.entries()) {
    if (record.state !== "VERIFIED") continue
    const type = canonicalScalarType(3)
    const expected = specializationPreimage(
      "generics", "FinalCallValue",
      [specializationParameter(type, record.predicateBodyDigest)],
      [specializationValue(type, canonicalScalarValue(3, type, text(d9StringValues[index])))],
      { authority: sourceAuthorityBytes },
    )
    assertSpecializationAvailable(record, expected, `D9 generics FinalCallValue ${index}`)
  }
  const sourceD3Values = [d3Witnesses.immediate.value, 42, 42, d3Witnesses.rejected.value]
  for (const [index, record] of genericsParsed.d3Records.entries()) {
    if (record.state !== "VERIFIED") continue
    const type = canonicalIntegerType(true, 64)
    const expected = specializationPreimage(
      "generics", "UltimateAnswer",
      [specializationParameter(type, record.predicateBodyDigest)],
      [specializationValue(type, canonicalIntegerValue(sourceD3Values[index], type, true, 64))],
      { authority: sourceAuthorityBytes },
    )
    assertSpecializationAvailable(record, expected, `D9 generics UltimateAnswer ${index}`)
  }
  const sourcePairType = canonicalIntegerType(true, 64)
  const sourcePairExpected = specializationPreimage(
    "generics", "AnswerPair",
    [specializationParameter(sourcePairType), specializationParameter(sourcePairType)],
    [specializationValue(sourcePairType, canonicalIntegerValue(42, sourcePairType, true, 64)),
      specializationValue(sourcePairType, canonicalIntegerValue(42, sourcePairType, true, 64))],
    { authority: sourceAuthorityBytes },
  )
  for (const record of genericsParsed.d6Records) {
    assertSpecializationAvailable(record, sourcePairExpected, "D9 generics AnswerPair")
  }
  const sourceStaticLines = firstGenericsOutput.split(/\r?\n/u)
    .filter((line) => line.startsWith("STATIC "))
  if (sourceStaticLines.length !== 2)
    fail("D9 generics witness did not produce both StaticValue applications")
  for (const [index, line] of sourceStaticLines.entries()) {
    const match = /^STATIC app=(\d+) state=(\w+) failure=([a-z:-]+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) specialization_state=(\w+) specialization_written=(\d+) specialization_required=(\d+) specialization_digest=([0-9a-f]{64})(?: specialization_preimage=([0-9a-f]+))?$/.exec(line)
    if (!match) fail(`invalid D9 StaticValue line: ${line}`)
    const record = {
      specializationState: match[6], specializationWritten: Number(match[7]),
      specializationRequired: Number(match[8]), specializationDigest: match[9],
      specializationPreimageHex: match[10] ?? null,
    }
    assertSpecializationAvailable(
      record,
      staticValueSpecializationPreimage(
        index === 0 ? 2 : 3, index === 0 ? 1 : 3,
        index === 0 ? "" : "The final seating", "generics", sourceAuthorityBytes,
      ),
      `D9 generics StaticValue ${index}`,
    )
  }
  if (parsed.frontend !== 0 || parsed.constir !== 0 ||
      parsed.records.length !== 3 + d1Witnesses.rejected.length)
    fail("domain witness did not produce six clean StagePath applications")
  const expected = [
    "VERIFIED", "VERIFIED", "VERIFIED",
    ...Array(d1Witnesses.rejected.length).fill("REJECTED"),
  ]
  for (const [index, record] of parsed.records.entries()) {
    const stageNames = index === 2
      ? ["accepted", "cancelled"]
      : ["accepted", "reserving", "preparing", "serving", "completed"]
    const expectedSpecialization = specializationPreimage(
      "restaurant", "StagePath",
      [specializationParameter(canonicalListType(), record.predicateBodyDigest)],
      [specializationValue(canonicalListType(), canonicalListValue(stageNames))],
      { authority: sourceAuthorityBytes },
    )
    if (record.module !== "restaurant" || record.head !== "StagePath" ||
        record.state !== expected[index] || record.predicates !== 1 || record.receipts !== 1 ||
        record.cacheHits !== 0 || record.cacheMisses !== 0 ||
        record.receiptCacheHits.join(",") !== "0" || record.receiptCacheMisses.join(",") !== "0" ||
        record.steps === 0 || (record.state === "REJECTED" &&
          (record.failure !== "predicate:false" || record.diagnostic !== 5 ||
           record.fingerprintState !== "NOT_AVAILABLE" ||
           record.fingerprintDigest !== "0".repeat(64))) ||
        (record.state === "VERIFIED" && (record.failure !== "none" || record.diagnostic !== 0 ||
          record.fingerprintState !== "AVAILABLE" ||
          record.fingerprintDigest !==
            sha256Hex(stagePathPreimage(
              stageNames,
              record.predicateBodyDigest,
            )) || record.specializationState !== "AVAILABLE" ||
          record.specializationWritten !== expectedSpecialization.length ||
          record.specializationRequired !== expectedSpecialization.length ||
          record.specializationDigest !== sha256Hex(expectedSpecialization))) ||
        (record.state !== "VERIFIED" &&
          (record.specializationState !== "NOT_AVAILABLE" ||
           record.specializationWritten !== 0 || record.specializationRequired !== 0 ||
           record.specializationDigest !== "0".repeat(64))))
      fail(`domain witness application ${index} has wrong state, facts, or counters`)
    if (record.state === "VERIFIED")
      assertSpecializationAvailable(record, expectedSpecialization, `StagePath ${index}`)
    else
      assertSpecializationNotAvailable(record, `StagePath ${index}`)
  }
  if (parsed.records[0].fingerprintDigest !== parsed.records[1].fingerprintDigest ||
      parsed.records[0].fingerprintDigest === parsed.records[2].fingerprintDigest ||
      parsed.records[0].specializationDigest !== parsed.records[1].specializationDigest ||
      parsed.records[0].specializationDigest === parsed.records[2].specializationDigest ||
      parsed.records[0].specializationPreimageHex !== parsed.records[1].specializationPreimageHex ||
      parsed.records[0].specializationPreimageHex === parsed.records[2].specializationPreimageHex ||
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
    const expectedSpecialization = specializationPreimage(
      "restaurant", "FinalCallValue",
      [specializationParameter(canonicalScalarType(3), record.predicateBodyDigest)],
      [specializationValue(
        canonicalScalarType(3),
        canonicalScalarValue(3, canonicalScalarType(3), text(stringValues[index])),
      )],
      { authority: sourceAuthorityBytes },
    )
    if (record.module !== "restaurant" || record.head !== "FinalCallValue" ||
        record.state !== expectedState || record.predicates !== 1 ||
        record.receipts !== 1 || record.cacheHits !== 0 || record.cacheMisses !== 0 ||
        record.receiptCacheHits.join(",") !== "0" || record.receiptCacheMisses.join(",") !== "0" ||
        record.steps === 0 ||
        record.state === "REJECTED" &&
          (record.failure !== "predicate:false" || record.diagnostic !== 5 ||
           record.fingerprintState !== "NOT_AVAILABLE" ||
           record.fingerprintDigest !== "0".repeat(64)) ||
        record.state === "VERIFIED" &&
          (record.failure !== "none" || record.diagnostic !== 0 ||
           record.fingerprintState !== "AVAILABLE" ||
           record.fingerprintDigest !==
             sha256Hex(finalCallPreimage(stringValues[index], record.predicateBodyDigest)) ||
           record.specializationState !== "AVAILABLE" ||
           record.specializationWritten !== expectedSpecialization.length ||
           record.specializationRequired !== expectedSpecialization.length ||
           record.specializationDigest !== sha256Hex(expectedSpecialization)) ||
        record.state !== "VERIFIED" &&
          (record.specializationState !== "NOT_AVAILABLE" ||
           record.specializationWritten !== 0 || record.specializationRequired !== 0 ||
           record.specializationDigest !== "0".repeat(64)))
      fail(`FinalCallValue application ${index} has wrong state or fingerprint`)
    if (record.state === "VERIFIED")
      assertSpecializationAvailable(record, expectedSpecialization, `FinalCallValue ${index}`)
    else
      assertSpecializationNotAvailable(record, `FinalCallValue ${index}`)
  }
  if (parsed.stringRecords[0].fingerprintDigest !==
        parsed.stringRecords[1].fingerprintDigest ||
      parsed.stringRecords[1].fingerprintDigest !==
        parsed.stringRecords[2].fingerprintDigest ||
      parsed.stringRecords[0].specializationDigest !==
        parsed.stringRecords[1].specializationDigest ||
      parsed.stringRecords[1].specializationDigest !==
        parsed.stringRecords[2].specializationDigest ||
      parsed.stringRecords[0].specializationPreimageHex !==
        parsed.stringRecords[1].specializationPreimageHex ||
      parsed.stringRecords[1].specializationPreimageHex !==
        parsed.stringRecords[2].specializationPreimageHex ||
      parsed.stringRecords[0].predicateBodyDigest !==
        parsed.stringRecords[1].predicateBodyDigest)
    fail("duplicate positive String fingerprints do not match")

  if (parsed.d3Records.length !== 4)
    fail("UltimateAnswer witness did not produce four source-backed applications")
  const d3Values = [d3Witnesses.immediate.value, 42, 42, d3Witnesses.rejected.value]
  const d3ExpectedStates = ["VERIFIED", "VERIFIED", "VERIFIED", "REJECTED"]
  const d3Digests = []
  const d3SpecializationDigests = []
  const d3SpecializationPreimages = []
  for (const [index, record] of parsed.d3Records.entries()) {
    const computed = index !== 0
    const ultimateType = canonicalIntegerType(true, 64)
    const expectedSpecialization = specializationPreimage(
      "restaurant", "UltimateAnswer",
      [specializationParameter(ultimateType, record.predicateBodyDigest)],
      [specializationValue(
        ultimateType,
        canonicalIntegerValue(d3Values[index], ultimateType, true, 64),
      )],
      { authority: sourceAuthorityBytes },
    )
    const expectedDigest = record.state === "VERIFIED"
      ? sha256Hex(ultimateAnswerPreimage(d3Values[index], record.predicateBodyDigest))
      : null
    if (record.module !== d3Witnesses.module || record.head !== "UltimateAnswer" ||
        record.state !== d3ExpectedStates[index] || record.predicates !== 1 ||
        record.computed !== (computed ? 1 : 0) ||
        record.receipts !== (computed ? 2 : 1) ||
        record.cacheHits !== 0 || record.cacheMisses !== 0 ||
        record.steps === 0 ||
        record.receiptKinds !== (computed ? "CP" : "P") ||
        record.receiptSteps.length !== (computed ? 2 : 1) ||
        record.receiptSteps.some((steps) => steps === 0) ||
        record.receiptArgs.length !== record.receipts ||
        record.receiptArgs.some((argument) => argument !== record.receiptArgs[0]) ||
        record.receiptTyped.length !== record.receipts ||
        record.receiptValues.join(",") !==
          (computed ? `${index === 3 ? "i36" : "i42"},${index === 3 ? "b0" : "b1"}` : "b1") ||
        record.receiptCacheHits.join(",") !== (computed ? "0,0" : "0") ||
        record.receiptCacheMisses.join(",") !== (computed ? "0,0" : "0") ||
        (computed && record.receiptTyped.some((typed) => typed !== index - 1)) ||
        (!computed && record.receiptTyped.some((typed) => typed !== 4294967295)) ||
        (record.state === "REJECTED" &&
          (record.failure !== "predicate:false" || record.diagnostic !== 5 ||
           record.fingerprintState !== "NOT_AVAILABLE" ||
           record.fingerprintDigest !== "0".repeat(64) ||
           record.specializationState !== "NOT_AVAILABLE" ||
           record.specializationWritten !== 0 || record.specializationRequired !== 0 ||
           record.specializationDigest !== "0".repeat(64))) ||
        (record.state === "VERIFIED" &&
          (record.failure !== "none" || record.diagnostic !== 0 ||
           record.fingerprintState !== "AVAILABLE" ||
           record.fingerprintDigest !== expectedDigest ||
           record.specializationState !== "AVAILABLE" ||
           record.specializationWritten !== expectedSpecialization.length ||
           record.specializationRequired !== expectedSpecialization.length ||
           record.specializationDigest !== sha256Hex(expectedSpecialization))) )
      fail(`UltimateAnswer application ${index} has wrong state, order, receipt, or fingerprint evidence`)
    if (record.state === "VERIFIED") {
      assertSpecializationAvailable(record, expectedSpecialization, `UltimateAnswer ${index}`)
      d3Digests.push(record.fingerprintDigest)
      d3SpecializationDigests.push(record.specializationDigest)
      d3SpecializationPreimages.push(record.specializationPreimageHex)
    } else {
      assertSpecializationNotAvailable(record, `UltimateAnswer ${index}`)
    }
  }
  if (d3Digests.length !== 3 || d3Digests[0] !== d3Digests[1] ||
      d3Digests[1] !== d3Digests[2] ||
      d3SpecializationDigests.length !== 3 ||
      d3SpecializationDigests[0] !== d3SpecializationDigests[1] ||
      d3SpecializationDigests[1] !== d3SpecializationDigests[2] ||
      d3SpecializationPreimages[0] !== d3SpecializationPreimages[1] ||
      d3SpecializationPreimages[1] !== d3SpecializationPreimages[2] ||
      parsed.d3Records[0].predicateBodyDigest !== parsed.d3Records[1].predicateBodyDigest ||
      parsed.d3Records[1].predicateBodyDigest !== parsed.d3Records[2].predicateBodyDigest)
    fail("immediate, computed, and duplicate UltimateAnswer fingerprints do not match")

  const computedSteps = parsed.d3Records[1].receiptSteps[0]
  const predicateSteps = parsed.d3Records[1].receiptSteps[1]
  if (computedSteps + predicateSteps <= 1)
    fail("UltimateAnswer witness did not expose positive computed and predicate steps")
  const cumulativeQuota = computedSteps + predicateSteps - 1
  const cumulativeQuotaParsed = parseProbe(
    run(executable, ["--domain-witness-quota", witnessPath,
      String(cumulativeQuota)]),
  )
  const cumulativeQuotaRecord = cumulativeQuotaParsed.d3Records.find(
    (record) => record.application === parsed.d3Records[1].application,
  )
  if (!cumulativeQuotaRecord || cumulativeQuotaRecord.state !== d3Witnesses.quota.state ||
      cumulativeQuotaRecord.failure !== "evaluator-diagnostic" ||
       cumulativeQuotaRecord.diagnostic !== 3 || cumulativeQuotaRecord.computed !== 1 ||
      cumulativeQuotaRecord.predicates !== 1 || cumulativeQuotaRecord.receipts !== 2 ||
      cumulativeQuotaRecord.receiptKinds !== d3Witnesses.quota.receiptKinds ||
      cumulativeQuotaRecord.receiptSteps.length !== 2 ||
      cumulativeQuotaRecord.receiptSteps[0] !== computedSteps ||
       cumulativeQuotaRecord.fingerprintState !== "NOT_AVAILABLE" ||
       cumulativeQuotaRecord.fingerprintDigest !== "0".repeat(64))
     fail("cumulative quota did not preserve computed receipt before predicate failure")
  assertSpecializationNotAvailable(cumulativeQuotaRecord, "cumulative quota")

  await Bun.write(join(build, "domain-generic-d4.w"), d4Witness)
  const d4Path = join(build, "domain-generic-d4.w")
  const d4FirstOutput = run(executable, ["--domain-witness", d4Path])
  const d4SecondOutput = run(executable, ["--domain-witness", d4Path])
  const d4Parsed = parseProbe(d4SecondOutput)
  if (d4FirstOutput !== d4SecondOutput || d4Parsed.d4Records.length !== 3)
    fail("D4 witness output is not deterministic or had the wrong application count")
  const d4Values = [d4Witnesses.forward.value, d4Witnesses.duplicate.value,
    d4Witnesses.rejected.value]
  const d4ExpectedStates = ["VERIFIED", "VERIFIED", "REJECTED"]
  const d4Digests = []
  const d4SpecializationDigests = []
  const d4SpecializationPreimages = []
  for (const [index, record] of d4Parsed.d4Records.entries()) {
    const ultimateType = canonicalIntegerType(true, 64)
    const expectedSpecialization = specializationPreimage(
      "restaurant", "UltimateAnswer",
      [specializationParameter(ultimateType, record.predicateBodyDigest)],
      [specializationValue(
        ultimateType,
        canonicalIntegerValue(d4Values[index], ultimateType, true, 64),
      )],
    )
    const expectedDigest = record.state === "VERIFIED"
      ? sha256Hex(ultimateAnswerPreimage(d4Values[index], record.predicateBodyDigest))
      : null
    if (record.module !== d4Witnesses.module || record.head !== "UltimateAnswer" ||
        record.state !== d4ExpectedStates[index] || record.predicates !== 1 ||
        record.computed !== 1 || record.receipts !== 2 || record.steps === 0 ||
        record.cacheHits !== 0 || record.cacheMisses !== 0 ||
        record.receiptKinds !== "CP" || record.receiptSteps.length !== 2 ||
        record.receiptSteps.some((steps) => steps === 0) ||
        record.receiptArgs.length !== 2 || record.receiptArgs.some((argument) => argument !== index) ||
        record.receiptTyped.length !== 2 || record.receiptTyped.some((typed) => typed !== index) ||
        record.receiptCacheHits.join(",") !== "0,0" ||
        record.receiptCacheMisses.join(",") !== (index === 0 ? "2,0" : "1,0") ||
        record.cyclePath.length !== 0 ||
        record.receiptValues.join(",") !==
          (index === 2 ? "i36,b0" : "i42,b1") ||
        (record.state === "REJECTED" &&
          (record.failure !== "predicate:false" || record.diagnostic !== 5 ||
           record.fingerprintState !== "NOT_AVAILABLE" ||
           record.fingerprintDigest !== "0".repeat(64) ||
           record.specializationState !== "NOT_AVAILABLE" ||
           record.specializationWritten !== 0 || record.specializationRequired !== 0 ||
           record.specializationDigest !== "0".repeat(64))) ||
        (record.state === "VERIFIED" &&
          (record.failure !== "none" || record.diagnostic !== 0 ||
           record.fingerprintState !== "AVAILABLE" ||
           record.fingerprintDigest !== expectedDigest ||
           record.specializationState !== "AVAILABLE" ||
           record.specializationWritten !== expectedSpecialization.length ||
           record.specializationRequired !== expectedSpecialization.length ||
           record.specializationDigest !== sha256Hex(expectedSpecialization))) )
      fail(`D4 application ${index} has wrong source dependency, receipt, or fingerprint evidence`)
    if (record.state === "VERIFIED") {
      assertSpecializationAvailable(record, expectedSpecialization, `D4 UltimateAnswer ${index}`)
      d4Digests.push(record.fingerprintDigest)
      d4SpecializationDigests.push(record.specializationDigest)
      d4SpecializationPreimages.push(record.specializationPreimageHex)
    } else {
      assertSpecializationNotAvailable(record, `D4 UltimateAnswer ${index}`)
    }
  }
  if (d4Digests.length !== 2 || d4Digests[0] !== d4Digests[1] ||
      d4SpecializationDigests.length !== 2 ||
      d4SpecializationDigests[0] !== d4SpecializationDigests[1] ||
      d4SpecializationPreimages[0] !== d4SpecializationPreimages[1] ||
      d4Digests[0] !== parsed.d3Records[1].fingerprintDigest ||
      d4Parsed.d4Records[0].predicateBodyDigest !== d4Parsed.d4Records[1].predicateBodyDigest ||
      d4Parsed.d4Records[1].predicateBodyDigest !== d4Parsed.d4Records[2].predicateBodyDigest ||
      d4Parsed.d4Records.filter((record) => record.fingerprintDigest === d4Digests[0]).length !==
        d4Witnesses.repeated.count)
    fail("named D4 and duplicate D4 fingerprints do not match the immediate/computed preimage")
  const d4QuotaRecord = parseProbe(
    run(executable, ["--domain-witness-quota", d4Path,
      String(d4Parsed.d4Records[0].receiptSteps[0] + d4Parsed.d4Records[0].receiptSteps[1] - 1)]),
  ).d4Records[0]
  if (!d4QuotaRecord || d4QuotaRecord.state !== d4Witnesses.quota.state ||
      d4QuotaRecord.failure !== "evaluator-diagnostic" || d4QuotaRecord.diagnostic !== 3 ||
      d4QuotaRecord.computed !== 1 || d4QuotaRecord.predicates !== 1 ||
      d4QuotaRecord.receipts !== 2 || d4QuotaRecord.receiptKinds !== "CP" ||
      d4QuotaRecord.receiptSteps.length !== 2 ||
      d4QuotaRecord.receiptSteps[0] !== d4Parsed.d4Records[0].receiptSteps[0] ||
      d4QuotaRecord.fingerprintState !== "NOT_AVAILABLE" ||
      d4QuotaRecord.fingerprintDigest !== "0".repeat(64))
    fail("D4 cumulative quota did not preserve the const dependency receipt")

  await Bun.write(join(build, "domain-generic-d5.w"), d5Witness)
  const d5Path = join(build, "domain-generic-d5.w")
  const d5FirstOutput = run(executable, ["--domain-witness", d5Path])
  const d5SecondOutput = run(executable, ["--domain-witness", d5Path])
  const d5Parsed = parseProbe(d5SecondOutput)
  const d5ExplicitPath = join(build, "domain-generic-d5-explicit.w")
  await Bun.write(d5ExplicitPath, `${explicitD5Declarations}${ultimateAnswerPredicate}\n` +
    `${ultimateAnswerValueSignature} {}\n${d5Use}`)
  const d5ExplicitParsed = parseProbe(run(executable, ["--domain-witness", d5ExplicitPath]))
  if (d5ExplicitParsed.d4Records.length !== d5Parsed.d4Records.length ||
      d5ExplicitParsed.d4Records.some((record, index) =>
        record.state !== d5Parsed.d4Records[index].state ||
        record.receiptValues.join(",") !== d5Parsed.d4Records[index].receiptValues.join(",") ||
        record.fingerprintDigest !== d5Parsed.d4Records[index].fingerprintDigest ||
        record.specializationDigest !== d5Parsed.d4Records[index].specializationDigest ||
        record.specializationPreimageHex !== d5Parsed.d4Records[index].specializationPreimageHex))
    fail("explicit and inferred D7 diamonds did not preserve value or fingerprint")
  const reconstructedDiamondRepeat = reconstructDiamond(d5Declarations)
  if (d5FirstOutput !== d5SecondOutput ||
      d5Parsed.d4Records.length !== d5Witnesses.duplicate.count ||
      reconstructedDiamond.value !== d5Witnesses.diamond.value ||
      reconstructedDiamond.sourceOrder.join(",") !==
        d5Witnesses.diamond.sourceOrder.join(",") ||
      reconstructedDiamond.misses !== d5Witnesses.diamond.misses ||
      reconstructedDiamond.hits !== d5Witnesses.diamond.hits ||
      reconstructedDiamond.steps !== d5Witnesses.diamond.steps ||
      JSON.stringify(reconstructedDiamond) !== JSON.stringify(reconstructedDiamondRepeat))
    fail("D5 Bun reconstruction is not deterministic or disagrees with the diamond source")
  for (const [index, record] of d5Parsed.d4Records.entries()) {
    const expectedDigest = sha256Hex(
      ultimateAnswerPreimage(reconstructedDiamond.value, record.predicateBodyDigest),
    )
    const ultimateType = canonicalIntegerType(true, 64)
    const expectedSpecialization = specializationPreimage(
      "restaurant", "UltimateAnswer",
      [specializationParameter(ultimateType, record.predicateBodyDigest)],
      [specializationValue(
        ultimateType,
        canonicalIntegerValue(reconstructedDiamond.value, ultimateType, true, 64),
      )],
    )
    if (record.module !== d5Witnesses.module || record.head !== "UltimateAnswer" ||
        record.state !== "VERIFIED" || record.failure !== "none" ||
        record.diagnostic !== 0 || record.predicates !== 1 || record.computed !== 1 ||
        record.receipts !== 2 || record.steps === 0 || record.cacheHits !== 0 ||
        record.cacheMisses !== 0 || record.receiptKinds !== "CP" ||
        record.receiptSteps.length !== 2 || record.receiptSteps[0] !== reconstructedDiamond.steps ||
        record.receiptCacheHits.join(",") !== "1,0" ||
        record.receiptCacheMisses.join(",") !== "4,0" || record.receiptValues.join(",") !== "i42,b1" ||
        record.fingerprintState !== "AVAILABLE" || record.fingerprintDigest !== expectedDigest ||
        record.specializationState !== "AVAILABLE" ||
        record.specializationWritten !== expectedSpecialization.length ||
        record.specializationRequired !== expectedSpecialization.length ||
        record.specializationDigest !== sha256Hex(expectedSpecialization) ||
        record.fingerprintDigest !== parsed.d3Records[0].fingerprintDigest ||
        record.cyclePath.length !== 0)
      fail(`D5 diamond application ${index} has wrong memo receipt or fingerprint evidence`)
    assertSpecializationAvailable(record, expectedSpecialization, `D5 UltimateAnswer ${index}`)
  }
  if (d5Parsed.d4Records[0].fingerprintDigest !== d5Parsed.d4Records[1].fingerprintDigest ||
      d5Parsed.d4Records[0].specializationDigest !== d5Parsed.d4Records[1].specializationDigest ||
      d5Parsed.d4Records[0].specializationPreimageHex !== d5Parsed.d4Records[1].specializationPreimageHex ||
      d5Parsed.d4Records[0].predicateBodyDigest !== d5Parsed.d4Records[1].predicateBodyDigest)
    fail("D5 duplicate application changed fingerprint or predicate digest")

  const d5QuotaSource = `${d5Declarations}struct Box<_ value: i64> {}
struct Use { let shared: Box<(assembledUltimateAnswer)> }
`
  const d5QuotaPath = join(build, "domain-generic-d5-quota.w")
  await Bun.write(d5QuotaPath, d5QuotaSource)
  const d5QuotaParsed = parseProbe(run(executable, ["--domain-witness", d5QuotaPath]))
  const d5QuotaRecord = d5QuotaParsed.d3Records.find((record) => record.head === "Box")
  if (!d5QuotaRecord || d5QuotaRecord.state !== "VERIFIED" || d5QuotaRecord.predicates !== 0 ||
      d5QuotaRecord.computed !== 1 || d5QuotaRecord.receipts !== 1 ||
      d5QuotaRecord.receiptSteps.join(",") !== String(reconstructedDiamond.steps) ||
      d5QuotaRecord.receiptCacheHits.join(",") !== "1" ||
      d5QuotaRecord.receiptCacheMisses.join(",") !== "4")
    fail("D5 quota witness did not expose the isolated diamond receipt")
  const d5QuotaRejected = parseProbe(
    run(executable, ["--domain-witness-quota", d5QuotaPath,
      String(d5Witnesses.quota.rejected)]),
  ).d3Records.find((record) => record.head === "Box")
  if (!d5QuotaRejected || d5QuotaRejected.state !== "EVALUATION_FAILED" ||
      d5QuotaRejected.failure !== "evaluator-diagnostic" ||
      d5QuotaRejected.diagnostic !== 3 || d5QuotaRejected.receiptSteps.join(",") !== "6" ||
      d5QuotaRejected.receiptCacheHits.join(",") !== "0" ||
      d5QuotaRejected.receiptCacheMisses.join(",") !== "4")
    fail("D5 quota six did not fail with the expected partial receipt and counters")

  const d5FailureSource = `${d5Declarations}const broken: i8 = 127 + 1
struct Narrow<_ value: i8> {}
struct Use { let broken: Narrow<(broken)> }
`
  const d5FailurePath = join(build, "domain-generic-d5-failure.w")
  await Bun.write(d5FailurePath, d5FailureSource)
  const d5FailureFirst = run(executable, ["--domain-witness", d5FailurePath])
  const d5FailureSecond = run(executable, ["--domain-witness", d5FailurePath])
  const d5FailureRecord = parseProbe(d5FailureSecond).d3Records.find(
    (record) => record.head === "Narrow",
  )
  if (d5FailureFirst !== d5FailureSecond || !d5FailureRecord ||
      d5FailureRecord.state !== "EVALUATION_FAILED" || d5FailureRecord.diagnostic !== 4 ||
      d5FailureRecord.receiptCacheHits.join(",") !== "0" ||
      d5FailureRecord.receiptCacheMisses.join(",") !== "1")
    fail("D5 arithmetic failure was cached or changed on repetition")

  await Bun.write(join(build, "domain-generic-d6.w"), d6Witness)
  const d6Path = join(build, "domain-generic-d6.w")
  const d6FirstOutput = run(executable, ["--domain-witness", d6Path])
  const d6SecondOutput = run(executable, ["--domain-witness", d6Path])
  const d6Parsed = parseProbe(d6SecondOutput)
  const reconstructedPair = reconstructSiblingPair(d5Declarations)
  const reconstructedPairRepeat = reconstructSiblingPair(d5Declarations)
  const d6Digest = sha256Hex(answerPairPreimage(
    reconstructedPair.value, reconstructedPair.value,
  ))
  const d6Specialization = specializationPreimage(
    "restaurant", "AnswerPair",
    [specializationParameter(canonicalIntegerType(true, 64)),
      specializationParameter(canonicalIntegerType(true, 64))],
    [specializationValue(canonicalIntegerType(true, 64),
      canonicalIntegerValue(reconstructedPair.value, canonicalIntegerType(true, 64), true, 64)),
      specializationValue(canonicalIntegerType(true, 64),
        canonicalIntegerValue(reconstructedPair.value, canonicalIntegerType(true, 64), true, 64))],
  )
  if (d6FirstOutput !== d6SecondOutput ||
      d6Parsed.d6Records.length !== d6Witnesses.duplicate.aliases ||
      reconstructedPair.value !== d6Witnesses.value ||
      reconstructedPair.sourceOrder.join(",") !== d6Witnesses.sourceOrder.join(",") ||
      JSON.stringify(reconstructedPair) !== JSON.stringify(reconstructedPairRepeat))
    fail("D6 Bun reconstruction is not deterministic or disagrees with the sibling source")
  for (const record of d6Parsed.d6Records) {
    if (record.module !== d6Witnesses.module || record.head !== d6Witnesses.head ||
        record.state !== "VERIFIED" || record.failure !== "none" ||
        record.diagnostic !== 0 || record.predicates !== 0 || record.computed !== 2 ||
        record.receipts !== 2 || record.steps !== d6Witnesses.second.steps ||
        record.cacheHits !== d6Witnesses.second.hits ||
        record.cacheMisses !== d6Witnesses.second.misses ||
        record.receiptKinds !== "CC" || record.receiptSteps.join(",") !== "7,1" ||
        record.receiptCacheHits.join(",") !== "1,1" ||
        record.receiptCacheMisses.join(",") !== "4,0" ||
        record.receiptValues.join(",") !== "i42,i42" ||
        record.fingerprintState !== "AVAILABLE" ||
        record.fingerprintDigest !== d6Digest ||
        record.specializationState !== "AVAILABLE" ||
        record.specializationWritten !== d6Specialization.length ||
        record.specializationRequired !== d6Specialization.length ||
        record.specializationDigest !== sha256Hex(d6Specialization) ||
        record.cyclePath.length !== 0)
      fail("D6 sibling application has wrong session, receipt, or fingerprint evidence")
    assertSpecializationAvailable(record, d6Specialization, "D6 AnswerPair")
  }

  const d6QuotaSource = `${d5Declarations}${answerPairSignature}
struct Use { let pair: AnswerPair<(assembledUltimateAnswer), (assembledUltimateAnswer)> }
`
  const d6QuotaPath = join(build, "domain-generic-d6-quota.w")
  await Bun.write(d6QuotaPath, d6QuotaSource)
  const d6QuotaParsed = parseProbe(
    run(executable, ["--domain-witness-quota", d6QuotaPath,
      String(d6Witnesses.application.quota)]),
  )
  const d6QuotaRecord = d6QuotaParsed.d6Records.find(
    (record) => record.head === d6Witnesses.head,
  )
  if (!d6QuotaRecord || d6QuotaRecord.state !== "VERIFIED" ||
      d6QuotaRecord.computed !== 2 || d6QuotaRecord.receipts !== 2 ||
      d6QuotaRecord.receiptSteps.join(",") !== "7,1" ||
      d6QuotaRecord.receiptCacheHits.join(",") !== "1,1" ||
      d6QuotaRecord.receiptCacheMisses.join(",") !== "4,0")
    fail("D6 quota-8 witness did not expose both sibling evaluations")
  const d6QuotaRejected = parseProbe(
    run(executable, ["--domain-witness-quota", d6QuotaPath,
      String(d6Witnesses.quota.secondRejected)]),
  ).d6Records.find((record) => record.head === d6Witnesses.head)
  if (!d6QuotaRejected || d6QuotaRejected.state !== "EVALUATION_FAILED" ||
      d6QuotaRejected.failure !== "evaluator-diagnostic" ||
      d6QuotaRejected.diagnostic !== 3 || d6QuotaRejected.computed !== 2 ||
      d6QuotaRejected.receipts !== 2 || d6QuotaRejected.receiptSteps.join(",") !== "7,0" ||
      d6QuotaRejected.receiptCacheHits.join(",") !== "1,0" ||
      d6QuotaRejected.receiptCacheMisses.join(",") !== "4,0")
    fail("D6 quota-7 witness did not fail before the second session lookup")

  const d6FailureSource = `${d5Declarations}const broken: i8 = 127 + 1
struct FailurePair<_ left: i8, _ right: i64> {}
struct Use { let pair: FailurePair<(broken), (assembledUltimateAnswer)> }
`
  const d6FailurePath = join(build, "domain-generic-d6-failure.w")
  await Bun.write(d6FailurePath, d6FailureSource)
  const d6FailureFirst = run(executable, ["--domain-witness", d6FailurePath])
  const d6FailureSecond = run(executable, ["--domain-witness", d6FailurePath])
  const d6FailureRecord = parseProbe(d6FailureSecond).d6Records.find(
    (record) => record.head === "FailurePair",
  )
  if (d6FailureFirst !== d6FailureSecond || !d6FailureRecord ||
      d6FailureRecord.state !== "EVALUATION_FAILED" ||
      d6FailureRecord.diagnostic !== 4 || d6FailureRecord.computed !== 2 ||
      d6FailureRecord.receipts !== d6Witnesses.failureFirst.receipts ||
      d6FailureRecord.receiptCacheHits.join(",") !== "0" ||
      d6FailureRecord.receiptCacheMisses.join(",") !== "1")
    fail("D6 first-argument failure allowed a sibling evaluation or changed on repetition")

  const runD4Case = async (name, source) => {
    const path = join(build, `domain-generic-d4-${name}.w`)
    await Bun.write(path, source)
    const parsedCase = parseProbe(run(executable, ["--domain-witness", path]))
    const record = [...parsedCase.d4Records, ...parsedCase.d3Records]
      .find((candidate) => candidate.head === "UltimateAnswer")
    if (!record) fail(`D4 ${name} witness produced no UltimateAnswer record`)
    return { path, parsed: parsedCase, record }
  }
  const d4PredicateAndValue = `${ultimateAnswerPredicate}\n${ultimateAnswerValueSignature} {}\n`
  const forwardCase = await runD4Case(
    "forward", "const forwardAnswer: i64 = laterAnswer\nconst laterAnswer: i64 = 42\n" +
      `${d4PredicateAndValue}struct Use { let forward: UltimateAnswer<(forwardAnswer)> }\n`)
  if (forwardCase.record.state !== d4Witnesses.forward.state ||
      forwardCase.record.receiptKinds !== "CP" || forwardCase.record.receipts !== 2 ||
      forwardCase.record.receiptValues.join(",") !== "i42,b1" ||
      forwardCase.record.cyclePath.length !== 0)
    fail("forward named-const chain did not verify with causal receipts")
  const selfCase = await runD4Case(
    "self", "const self: i64 = self\n" +
      `${d4PredicateAndValue}struct Use { let self: UltimateAnswer<(self)> }\n`)
  if (selfCase.record.state !== d4Witnesses.selfCycle.state ||
      selfCase.record.failure !== "evaluator-diagnostic" || selfCase.record.diagnostic !== 2 ||
      selfCase.record.steps !== 0 || selfCase.record.receipts !== 1 ||
      selfCase.record.cacheHits !== d5Witnesses.preflight.selfCycle.hits ||
      selfCase.record.cacheMisses !== d5Witnesses.preflight.selfCycle.misses ||
      selfCase.record.receiptKinds !== "C" ||
      selfCase.record.receiptCacheHits.join(",") !==
        String(d5Witnesses.preflight.selfCycle.receiptHits) ||
      selfCase.record.receiptCacheMisses.join(",") !==
        String(d5Witnesses.preflight.selfCycle.receiptMisses) ||
      selfCase.record.cyclePath.join(",") !== "1,1")
    fail("self-cycle did not fail before evaluation with the closed path")
  const twoCycleCase = await runD4Case(
    "two-cycle", "const left: i64 = right\nconst right: i64 = left\n" +
      `${d4PredicateAndValue}struct Use { let cycle: UltimateAnswer<(left)> }\n`)
  if (twoCycleCase.record.state !== d4Witnesses.twoCycle.state ||
      twoCycleCase.record.diagnostic !== 2 || twoCycleCase.record.steps !== 0 ||
      twoCycleCase.record.receipts !== d5Witnesses.preflight.twoCycle.receipts ||
      twoCycleCase.record.cacheHits !== d5Witnesses.preflight.twoCycle.hits ||
      twoCycleCase.record.cacheMisses !== d5Witnesses.preflight.twoCycle.misses ||
      twoCycleCase.record.receiptKinds !== "C" ||
      twoCycleCase.record.receiptCacheHits.join(",") !==
        String(d5Witnesses.preflight.twoCycle.receiptHits) ||
      twoCycleCase.record.receiptCacheMisses.join(",") !==
        String(d5Witnesses.preflight.twoCycle.receiptMisses) ||
      twoCycleCase.record.cyclePath.join(",") !== d4Witnesses.twoCycle.path)
    fail("two-member cycle path was not deterministic")
  const zeroCapacityOutput = run(
    executable, ["--domain-witness-d4-zero-capacity", twoCycleCase.path],
  )
  const zeroCapacityMatch = /^D4_ZERO state=(\w+) failure=([a-z:-]+) diagnostic=(\d+) computed=(\d+) steps=(\d+) receipts=(\d+) cache_hits=(\d+) cache_misses=(\d+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) specialization_state=(\w+) specialization_written=(\d+) specialization_required=(\d+) specialization_digest=([0-9a-f]{64}) cycle_path=([0-9,]*)$/u
    .exec(zeroCapacityOutput.trim())
  if (!zeroCapacityMatch || zeroCapacityMatch[1] !== d4Witnesses.zeroCapacity.state ||
      zeroCapacityMatch[2] !== "evaluator-diagnostic" || zeroCapacityMatch[3] !== "2" ||
      zeroCapacityMatch[4] !== "1" || zeroCapacityMatch[5] !== "0" ||
      zeroCapacityMatch[6] !== "0" ||
      zeroCapacityMatch[7] !== String(d5Witnesses.preflight.zeroCapacity.hits) ||
      zeroCapacityMatch[8] !== String(d5Witnesses.preflight.zeroCapacity.misses) ||
      zeroCapacityMatch[9] !== "NOT_AVAILABLE" ||
      zeroCapacityMatch[10] !== "0".repeat(64) ||
      zeroCapacityMatch[11] !== "NOT_AVAILABLE" || zeroCapacityMatch[12] !== "0" ||
      zeroCapacityMatch[13] !== "0" || zeroCapacityMatch[14] !== "0".repeat(64) ||
      zeroCapacityMatch[15] !== "1,2,1")
    fail("zero-capacity cycle preflight did not preserve buffers or the closed path")
  const threeCycleCase = await runD4Case(
    "three-cycle", "const first: i64 = second\nconst second: i64 = third\n" +
      "const third: i64 = first\n" +
      `${d4PredicateAndValue}struct Use { let cycle: UltimateAnswer<(first)> }\n`)
  if (threeCycleCase.record.state !== d4Witnesses.threeCycle.state ||
      threeCycleCase.record.diagnostic !== 2 || threeCycleCase.record.steps !== 0 ||
      threeCycleCase.record.receipts !== d5Witnesses.preflight.threeCycle.receipts ||
      threeCycleCase.record.cacheHits !== d5Witnesses.preflight.threeCycle.hits ||
      threeCycleCase.record.cacheMisses !== d5Witnesses.preflight.threeCycle.misses ||
      threeCycleCase.record.receiptKinds !== "C" ||
      threeCycleCase.record.receiptCacheHits.join(",") !==
        String(d5Witnesses.preflight.threeCycle.receiptHits) ||
      threeCycleCase.record.receiptCacheMisses.join(",") !==
        String(d5Witnesses.preflight.threeCycle.receiptMisses) ||
      threeCycleCase.record.cyclePath.join(",") !== d4Witnesses.threeCycle.path)
    fail("three-member cycle path was not deterministic")
  const incompatibleCycleCase = await runD4Case(
    "incompatible-cycle",
    "const left = right && true\nconst right = left + 1\n" +
      "struct UltimateAnswer<_ value: Bool> {}\n" +
      "struct Use { let cycle: UltimateAnswer<(left)> }\n",
  )
  assertCycleRecord(
    incompatibleCycleCase.record, d7Witnesses.cycles.incompatible,
    "incompatible reachable cycle",
  )
  const incompatibleMultiSlotCase = await runD4Case(
    "incompatible-multi-slot",
    "const left = right && true\nconst right = left + 1\n" +
      "struct UltimateAnswer<_ first: Bool, _ second: Bool> {}\n" +
      "struct Use { let cycle: UltimateAnswer<(left), (left)> }\n",
  )
  assertCycleRecord(
    incompatibleMultiSlotCase.record, d7Witnesses.cycles.incompatibleMultiSlot,
    "incompatible multi-slot cycle",
  )
  const incompatibleMultiSlotZeroOutput = run(
    executable, ["--domain-witness-d4-zero-capacity", incompatibleMultiSlotCase.path],
  )
  const incompatibleMultiSlotZeroMatch = /^D4_ZERO state=(\w+) failure=([a-z:-]+) diagnostic=([0-9]+) computed=(\d+) steps=(\d+) receipts=(\d+) cache_hits=(\d+) cache_misses=(\d+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) specialization_state=(\w+) specialization_written=(\d+) specialization_required=(\d+) specialization_digest=([0-9a-f]{64}) cycle_path=([0-9,]*)$/u
    .exec(incompatibleMultiSlotZeroOutput.trim())
  assertCycleRecord({
    state: incompatibleMultiSlotZeroMatch?.[1],
    failure: incompatibleMultiSlotZeroMatch?.[2],
    diagnostic: Number(incompatibleMultiSlotZeroMatch?.[3]),
    computed: Number(incompatibleMultiSlotZeroMatch?.[4]),
    steps: Number(incompatibleMultiSlotZeroMatch?.[5]),
    receipts: Number(incompatibleMultiSlotZeroMatch?.[6]),
    cacheHits: Number(incompatibleMultiSlotZeroMatch?.[7]),
    cacheMisses: Number(incompatibleMultiSlotZeroMatch?.[8]),
    receiptKinds: "",
    fingerprintState: incompatibleMultiSlotZeroMatch?.[9],
    fingerprintDigest: incompatibleMultiSlotZeroMatch?.[10],
    cyclePath: incompatibleMultiSlotZeroMatch?.[15]?.split(",").filter(Boolean).map(Number) ?? [],
  }, d7Witnesses.cycles.incompatibleMultiSlotZeroCapacity,
  "incompatible multi-slot zero-capacity cycle")
  if (!incompatibleMultiSlotZeroMatch ||
      incompatibleMultiSlotZeroMatch[11] !== "NOT_AVAILABLE" ||
      incompatibleMultiSlotZeroMatch[12] !== "0" || incompatibleMultiSlotZeroMatch[13] !== "0" ||
      incompatibleMultiSlotZeroMatch[14] !== "0".repeat(64))
    fail("incompatible multi-slot zero-capacity cycle published specialization metadata")
  const dependencyLimitSource = Array.from({ length: 257 }, (_, index) =>
    `const c${index}: i64 = ${index + 1 < 257 ? `c${index + 1}` : "42"}\n`).join("") +
    `${d4PredicateAndValue}struct Use { let dependencyLimit: UltimateAnswer<(c0)> }\n`
  const dependencyLimitCase = await runD4Case("dependency-limit", dependencyLimitSource)
  if (dependencyLimitCase.record.state !== d4Witnesses.dependencyLimit.state ||
      dependencyLimitCase.record.failure !== "dependency-limit" ||
      dependencyLimitCase.record.computed !== 1 || dependencyLimitCase.record.steps !== 0 ||
      dependencyLimitCase.record.receipts !== 0 ||
      dependencyLimitCase.record.cacheHits !== d5Witnesses.preflight.dependencyLimit.hits ||
      dependencyLimitCase.record.cacheMisses !== d5Witnesses.preflight.dependencyLimit.misses ||
      dependencyLimitCase.record.fingerprintState !== "NOT_AVAILABLE")
    fail("D4 dependency ceiling did not stop before conversion or evaluation")
  const unreachableCase = await runD4Case(
    "unreachable", "const deadLeft: i64 = deadRight\nconst deadRight: i64 = deadLeft\n" +
      "const good: i64 = 42\n" +
      `${d4PredicateAndValue}struct Use { let independent: UltimateAnswer<(good)> }\n`)
  if (unreachableCase.record.state !== d4Witnesses.unreachable.state ||
      unreachableCase.record.fingerprintState !== "AVAILABLE" ||
      unreachableCase.record.receiptValues.join(",") !== "i42,b1" ||
      unreachableCase.record.cyclePath.length !== 0)
    fail("unreachable cycle blocked an independent named-const application")
  const typeMismatchCase = await runD4Case(
    "type-mismatch", "const wrong: i64 = true\n" +
      `${d4PredicateAndValue}struct Use { let mismatch: UltimateAnswer<(wrong)> }\n`)
  if (typeMismatchCase.record.state !== d4Witnesses.typeMismatch.state ||
      typeMismatchCase.record.failure !== "invalid-input" || typeMismatchCase.record.steps !== 0 ||
      typeMismatchCase.record.receipts !== 0)
    fail("named-const type mismatch was not rejected in frontend preflight")
  const unresolvedCase = await runD4Case(
    "unresolved", "const anchor: i64 = 42\n" +
      `${d4PredicateAndValue}struct Use { let missing: UltimateAnswer<(missing)> }\n`)
  if (unresolvedCase.record.state !== d4Witnesses.unresolved.state ||
      unresolvedCase.record.failure !== "invalid-input" ||
      unresolvedCase.record.steps !== 0 || unresolvedCase.record.receipts !== 0 ||
      unresolvedCase.record.fingerprintState !== "NOT_AVAILABLE")
    fail("unresolved named-const reference did not stop before evaluation")
  const callCase = await runD4Case(
    "call", "const fn helper(_ value: i64): i64 { return value }\n" +
      "const called: i64 = helper(42)\n" +
      `${d4PredicateAndValue}struct Use { let unsupported: UltimateAnswer<(called)> }\n`)
  if (callCase.record.state !== d4Witnesses.call.state ||
      callCase.record.failure !== "function" || callCase.record.steps !== 0 ||
      callCase.record.receipts !== 0)
    fail("unsupported named-const call was not stopped at the frontend boundary")
  const stringCase = await runD4Case(
    "string", "const textValue: String = \"42\"\n" +
      `${d4PredicateAndValue}struct Use { let unsupported: UltimateAnswer<(textValue)> }\n`)
  if (stringCase.record.state !== d4Witnesses.string.state || stringCase.record.steps !== 0 ||
      stringCase.record.receipts !== 0)
    fail("unsupported String named const was not stopped before execution")
  const untypedCase = await runD4Case(
    "untyped", "const untyped = \"42\"\n" +
      `${d4PredicateAndValue}struct Use { let unsupported: UltimateAnswer<(untyped)> }\n`)
  if (untypedCase.record.state !== d4Witnesses.untyped.state || untypedCase.record.steps !== 0 ||
      untypedCase.record.receipts !== 0)
    fail("unsupported untyped named const escaped the D4 boundary")
  const importedCase = await runD4Case(
    "imported", "import { answer } from other\n" +
      `${d4PredicateAndValue}struct Use { let unsupported: UltimateAnswer<(answer)> }\n`)
  if (importedCase.record.state !== d4Witnesses.imported.state ||
      importedCase.record.steps !== 0 || importedCase.record.receipts !== 0)
    fail("imported named const was not rejected as outside D4")
  const quantityCase = await runD4Case(
    "quantity", "const duration: PhysicalDuration = 10<si.s>\n" +
      `${d4PredicateAndValue}struct Use { let unsupported: UltimateAnswer<(duration)> }\n`)
  if (quantityCase.record.state !== d4Witnesses.quantity.state ||
      quantityCase.record.steps !== 0 || quantityCase.record.receipts !== 0 ||
      quantityCase.record.fingerprintState !== "NOT_AVAILABLE")
    fail("quantity named const escaped the D4 unsupported boundary")
  const sizeCase = await runD4Case(
    "size", "const sizeValue: usize = 1<iec.MiB>\n" +
      `${d4PredicateAndValue}struct Use { let unsupported: UltimateAnswer<(sizeValue)> }\n`)
  if (sizeCase.record.state !== d4Witnesses.size.state ||
      sizeCase.record.steps !== 0 || sizeCase.record.receipts !== 0 ||
      sizeCase.record.fingerprintState !== "NOT_AVAILABLE")
    fail("size named const escaped the D4 unsupported boundary")

  const arithmeticOverflowCase = await runD4Case(
    "arithmetic-overflow",
    "const overflowValue: i8 = 127 + 1\n" +
      "const fn isUltimateAnswer8(value: i8): Bool { return value == 42 }\n" +
      "struct UltimateAnswer<_ value: i8<(isUltimateAnswer8(.member))>> {}\n" +
      "struct Use { let arithmeticOverflow: UltimateAnswer<(overflowValue)> }\n",
  )
  if (arithmeticOverflowCase.record.state !== d4Witnesses.arithmeticOverflow.state ||
      arithmeticOverflowCase.record.failure !== "evaluator-diagnostic" ||
      arithmeticOverflowCase.record.diagnostic !== 4 ||
      arithmeticOverflowCase.record.computed !== 1 ||
      arithmeticOverflowCase.record.predicates !== 1 ||
      arithmeticOverflowCase.record.steps === 0 ||
      arithmeticOverflowCase.record.receipts !== d4Witnesses.arithmeticOverflow.receipts ||
      arithmeticOverflowCase.record.receiptKinds !== d4Witnesses.arithmeticOverflow.receiptKinds ||
      arithmeticOverflowCase.record.receiptKinds.includes("P") ||
      arithmeticOverflowCase.record.fingerprintState !== "NOT_AVAILABLE" ||
      arithmeticOverflowCase.record.fingerprintDigest !== "0".repeat(64))
    fail("D4 named-const arithmetic overflow did not preserve only its causal receipt")

  const overflowWitness = `${ultimateAnswerPredicate}\n${ultimateAnswerValueSignature} {}\n` +
    `struct Narrow<_ value: i8> {}\nstruct Use { let overflow: Narrow<(127 + 1)> }\n`
  const overflowPath = join(build, "domain-generic-overflow.w")
  await Bun.write(overflowPath, overflowWitness)
  const overflowParsed = parseProbe(run(executable, ["--domain-witness", overflowPath]))
  const overflowRecord = overflowParsed.d3Records.find((record) => record.head === "Narrow")
  if (!overflowRecord || overflowRecord.state !== d3Witnesses.overflow.state ||
      overflowRecord.failure !== "evaluator-diagnostic" ||
      overflowRecord.diagnostic !== 4 || overflowRecord.steps === 0 ||
      overflowRecord.receipts !== 1 || overflowRecord.receiptKinds !== "C" ||
      overflowRecord.fingerprintState !== "NOT_AVAILABLE" ||
      overflowRecord.fingerprintDigest !== "0".repeat(64))
    fail("overflow witness did not preserve the causal ConstIR receipt")

  const unsupportedWitness = `${ultimateAnswerPredicate}\n${ultimateAnswerValueSignature} {}\n` +
    `const fn helper(_ value: i64): i64 { return value }\n` +
    `struct Use { let unsupported: UltimateAnswer<(helper(42))> }\n`
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

  const corruptOutput = run(executable, ["--domain-witness-d3-corrupt", genericsWitnessPath])
  const corruptRecords = corruptOutput.split(/\r?\n/u)
    .filter((line) => line.startsWith("D3_CORRUPT "))
    .map((line) => {
      const match = /^D3_CORRUPT case=([a-z]+) state=(\w+) failure=([a-z:-]+) diagnostic=(\d+) steps=(\d+) receipts=(\d+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) specialization_state=(\w+) specialization_written=(\d+) specialization_required=(\d+) specialization_digest=([0-9a-f]{64})$/u.exec(line)
      if (!match) fail(`invalid D3 corruption line: ${line}`)
      return {
        case: match[1], state: match[2], failure: match[3],
        diagnostic: Number(match[4]), steps: Number(match[5]),
        receipts: Number(match[6]), fingerprintState: match[7], fingerprintDigest: match[8],
        specializationState: match[9], specializationWritten: Number(match[10]),
        specializationRequired: Number(match[11]), specializationDigest: match[12],
      }
    })
  if (corruptRecords.length !== d3Witnesses.corrupt.cases.length ||
      corruptRecords.some((record, index) =>
        record.case !== d3Witnesses.corrupt.cases[index] ||
        record.state !== d3Witnesses.corrupt.state || record.steps !== 0 ||
        record.receipts !== 0 || record.fingerprintState !== "NOT_AVAILABLE" ||
        record.fingerprintDigest !== "0".repeat(64) ||
        record.specializationState !== "NOT_AVAILABLE" || record.specializationWritten !== 0 ||
        record.specializationRequired !== 0 || record.specializationDigest !== "0".repeat(64)))
    fail("D3 origin/relation/type/application corruption did not fail in zero steps")

  const d4CorruptOutput = run(executable, ["--domain-witness-d4-corrupt", d4Path])
  const d4CorruptRecords = d4CorruptOutput.split(/\r?\n/u)
    .filter((line) => line.startsWith("D4_CORRUPT "))
    .map((line) => {
      const match = /^D4_CORRUPT case=([a-z]+) state=(\w+) failure=([a-z:-]+) diagnostic=(\d+) steps=(\d+) receipts=(\d+) cache_hits=(\d+) cache_misses=(\d+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) specialization_state=(\w+) specialization_written=(\d+) specialization_required=(\d+) specialization_digest=([0-9a-f]{64})$/u.exec(line)
      if (!match) fail(`invalid D4 corruption line: ${line}`)
      return {
        case: match[1], state: match[2], failure: match[3],
        diagnostic: Number(match[4]), steps: Number(match[5]),
        receipts: Number(match[6]), cacheHits: Number(match[7]), cacheMisses: Number(match[8]),
        fingerprintState: match[9], fingerprintDigest: match[10],
        specializationState: match[11], specializationWritten: Number(match[12]),
        specializationRequired: Number(match[13]), specializationDigest: match[14],
      }
    })
  if (d4CorruptRecords.length !== d4Witnesses.corrupt.cases.length ||
      d4CorruptRecords.some((record, index) =>
        record.case !== d4Witnesses.corrupt.cases[index] ||
        record.state !== d4Witnesses.corrupt.state || record.steps !== 0 ||
        record.receipts !== 0 || record.cacheHits !== d5Witnesses.preflight.corruption.hits ||
        record.cacheMisses !== d5Witnesses.preflight.corruption.misses ||
        record.fingerprintState !== "NOT_AVAILABLE" ||
        record.fingerprintDigest !== "0".repeat(64) ||
        record.specializationState !== "NOT_AVAILABLE" || record.specializationWritten !== 0 ||
        record.specializationRequired !== 0 || record.specializationDigest !== "0".repeat(64)))
    fail("D4 origin/mapping/dependency/type/application corruption did not fail in zero steps")

  const overLimitWitness = `${finalCallPredicate}\n${finalCallValueSignature} {}\n` +
    `struct Use { let over: FinalCallValue<"${"x".repeat(d2Witnesses.overLimit.byteCount)}"> }\n`
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

  const stringCorruptOutput = run(executable, ["--domain-witness-corrupt", genericsWitnessPath])
  const corruptLine = stringCorruptOutput.split(/\r?\n/u)
    .find((line) => line.startsWith("STRING_CORRUPT "))
  const corruptMatch = /^STRING_CORRUPT state=(\w+) failure=([a-z:-]+) diagnostic=(\d+) steps=(\d+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) specialization_state=(\w+) specialization_written=(\d+) specialization_required=(\d+) specialization_digest=([0-9a-f]{64})$/u
    .exec(corruptLine ?? "")
  if (!corruptMatch || corruptMatch[1] !== d2Witnesses.corrupt.state ||
      corruptMatch[2] !== "invalid-input" || corruptMatch[3] !== "0" ||
      corruptMatch[4] !== String(d2Witnesses.corrupt.steps) ||
      corruptMatch[5] !== "NOT_AVAILABLE" ||
      corruptMatch[6] !== "0".repeat(64) || corruptMatch[7] !== "NOT_AVAILABLE" ||
      corruptMatch[8] !== "0" || corruptMatch[9] !== "0" ||
      corruptMatch[10] !== "0".repeat(64))
    fail("corrupt String arena relation was not rejected in preflight")
  const staticRecords = firstOutput.split(/\r?\n/u)
    .filter((line) => line.startsWith("STATIC "))
    .map((line) => {
      const match = /^STATIC app=(\d+) state=(\w+) failure=([a-z:-]+) fingerprint_state=(\w+) fingerprint_digest=([0-9a-f]{64}) specialization_state=(\w+) specialization_written=(\d+) specialization_required=(\d+) specialization_digest=([0-9a-f]{64})(?: specialization_preimage=([0-9a-f]+))?$/u.exec(line)
      if (!match) fail(`invalid StaticValue line: ${line}`)
      return {
        application: Number(match[1]), state: match[2], failure: match[3],
        fingerprintState: match[4], fingerprintDigest: match[5],
        specializationState: match[6], specializationWritten: Number(match[7]),
        specializationRequired: Number(match[8]), specializationDigest: match[9],
        specializationPreimageHex: match[10] ?? null,
      }
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
    const expectedSpecialization = staticValueSpecializationPreimage(
      index === 0 ? 2 : 3, index === 0 ? 1 : 3,
      index === 0 ? "" : "The final seating", "restaurant", sourceAuthorityBytes,
    )
    if (record.fingerprintDigest !== expectedStatic[index])
      fail(`StaticValue application ${index} disagrees with independent preimage`)
    assertSpecializationAvailable(record, expectedSpecialization, `StaticValue ${index}`)
  }
  if (staticRecords[0].specializationDigest === staticRecords[1].specializationDigest ||
      staticRecords[0].specializationPreimageHex === staticRecords[1].specializationPreimageHex)
    fail("Bool and String StaticValue witnesses collapsed to one specialization identity")
} finally {
  await rm(build, { recursive: true, force: true })
}

console.log("seed generic validation: ok")
