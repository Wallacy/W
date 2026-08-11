import { createHash } from "node:crypto"

const UTF8 = new TextDecoder("utf-8", { fatal: true })
const DIGEST_PATTERN = /^sha256:[0-9a-f]{64}$/
const SCANNER_ABI = "w.foreign-scanner@1"

export class ForeignBodyModelError extends Error {
  constructor(code, facts = {}) {
    super(code)
    this.name = "ForeignBodyModelError"
    this.code = code
    this.facts = facts
  }
}

function fail(code, facts = {}) {
  throw new ForeignBodyModelError(code, facts)
}

function digest(bytes) {
  return `sha256:${createHash("sha256").update(bytes).digest("hex")}`
}

function sourceBytes(operation) {
  let bytes
  if (typeof operation.source === "string") {
    bytes = Buffer.from(operation.source, "utf8")
  } else if (typeof operation.sourceHex === "string") {
    if (!/^(?:[0-9a-fA-F]{2})+$/.test(operation.sourceHex)) fail("W-FOREIGN-0006")
    bytes = Buffer.from(operation.sourceHex, "hex")
  } else {
    fail("W-FOREIGN-0003")
  }

  try {
    UTF8.decode(bytes)
  } catch {
    fail("W-FOREIGN-0006", { reason: "invalidUtf8" })
  }
  if (bytes.includes(0)) fail("W-FOREIGN-0006", { reason: "nulByte" })
  return bytes
}

function advanceEscapedLine(bytes, index) {
  if (bytes[index] !== 0x5c || index + 1 >= bytes.length) return index + 1
  if (bytes[index + 1] === 0x0a) return index + 2
  if (bytes[index + 1] === 0x0d && bytes[index + 2] === 0x0a) return index + 3
  return Math.min(index + 2, bytes.length)
}

export function scanForeignBody(operation, profile = "c-inline-1") {
  const bytes = sourceBytes(operation)
  const maximumBodyBytes = operation.maximumBodyBytes ?? 64 * 1024
  const maximumBraceDepth = operation.maximumBraceDepth ?? 256
  if (!Number.isSafeInteger(maximumBodyBytes) || maximumBodyBytes < 0) {
    fail("W-FOREIGN-0006", { reason: "invalidByteLimit" })
  }
  if (!Number.isSafeInteger(maximumBraceDepth) || maximumBraceDepth < 0) {
    fail("W-FOREIGN-0006", { reason: "invalidDepthLimit" })
  }
  if (bytes[0] !== 0x7b) fail("W-FOREIGN-0003", { reason: "missingOpen" })

  let index = 1
  let state = "normal"
  let depth = 0
  let maximumDepthObserved = 0
  let lineStart = true
  let sawDirective = false

  while (index < bytes.length) {
    if (index - 1 > maximumBodyBytes) {
      fail("W-FOREIGN-0006", { reason: "bodyBytes", maximumBodyBytes })
    }

    const byte = bytes[index]
    const next = bytes[index + 1]

    if (state === "lineComment" || state === "directive") {
      if (byte === 0x5c) {
        index = advanceEscapedLine(bytes, index)
        continue
      }
      if (byte === 0x0a) {
        state = "normal"
        lineStart = true
      }
      index += 1
      continue
    }

    if (state === "blockComment") {
      if (byte === 0x2a && next === 0x2f) {
        state = "normal"
        index += 2
      } else {
        if (byte === 0x0a) lineStart = true
        index += 1
      }
      continue
    }

    if (state === "singleQuote" || state === "doubleQuote") {
      const delimiter = state === "singleQuote" ? 0x27 : 0x22
      if (byte === 0x5c) {
        index = advanceEscapedLine(bytes, index)
        continue
      }
      if (byte === 0x0a || byte === 0x0d) {
        fail("W-FOREIGN-0004", { reason: "newlineInLiteral", state })
      }
      index += 1
      if (byte === delimiter) state = "normal"
      continue
    }

    if (byte === 0x7d && depth === 0) {
      const body = bytes.subarray(1, index)
      if (body.length > maximumBodyBytes) {
        fail("W-FOREIGN-0006", { reason: "bodyBytes", maximumBodyBytes })
      }
      if (profile === "c-inline-1" && sawDirective) {
        fail("W-FOREIGN-0005", { reason: "preprocessorDirective", closeOffset: index })
      }
      return {
        bodyStart: 1,
        bodyEnd: index,
        closeOffset: index,
        nextOffset: index + 1,
        byteLength: body.length,
        bodyDigest: digest(body),
        maximumDepthObserved,
        sawDirective,
        exactBytesHex: body.toString("hex"),
      }
    }

    if (byte === 0x5c && (next === 0x0a || (next === 0x0d && bytes[index + 2] === 0x0a))) {
      fail("W-FOREIGN-0005", { reason: "lineSplice" })
    }

    if (byte === 0x2f && next === 0x2f) {
      state = "lineComment"
      lineStart = false
      index += 2
      continue
    }
    if (byte === 0x2f && next === 0x2a) {
      state = "blockComment"
      index += 2
      continue
    }
    if (byte === 0x22) {
      state = "doubleQuote"
      lineStart = false
      index += 1
      continue
    }
    if (byte === 0x27) {
      state = "singleQuote"
      lineStart = false
      index += 1
      continue
    }
    if (byte === 0x23 && lineStart) {
      state = "directive"
      sawDirective = true
      index += 1
      continue
    }
    if (byte === 0x25 && next === 0x3a && lineStart) {
      state = "directive"
      sawDirective = true
      index += 2
      continue
    }
    if (byte === 0x3c && next === 0x25) {
      depth += 1
      maximumDepthObserved = Math.max(maximumDepthObserved, depth)
      if (depth > maximumBraceDepth) {
        fail("W-FOREIGN-0006", { reason: "braceDepth", maximumBraceDepth })
      }
      lineStart = false
      index += 2
      continue
    }
    if (byte === 0x25 && next === 0x3e) {
      if (depth > 0) depth -= 1
      lineStart = false
      index += 2
      continue
    }
    if (byte === 0x7b) {
      depth += 1
      maximumDepthObserved = Math.max(maximumDepthObserved, depth)
      if (depth > maximumBraceDepth) {
        fail("W-FOREIGN-0006", { reason: "braceDepth", maximumBraceDepth })
      }
    } else if (byte === 0x7d) {
      depth -= 1
    }

    if (byte === 0x0a) {
      lineStart = true
    } else if (![0x20, 0x09, 0x0b, 0x0c, 0x0d].includes(byte)) {
      lineStart = false
    }
    index += 1
  }

  if (state !== "normal") fail("W-FOREIGN-0004", { reason: state })
  fail("W-FOREIGN-0003", { reason: "missingClose" })
}

function normalizeLanguage(language) {
  if (language === "C" || language === ".c" || language === "c") return "c"
  if (typeof language === "string" && /^[A-Za-z_][A-Za-z0-9_]*$/.test(language)) {
    return language
  }
  fail("W-FOREIGN-0001", { reason: "invalidLanguage" })
}

function validDigest(value) {
  return typeof value === "string" && DIGEST_PATTERN.test(value)
}

function initialState() {
  return {
    phase: "unresolved",
    revision: 0,
    language: null,
    adapterDigest: null,
    scannerDigest: null,
    scannerAbi: null,
    scannerProfile: null,
    authoritative: false,
    scan: null,
    mappedDiagnostics: [],
    recipe: null,
  }
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "resolve": {
      if (state.phase !== "unresolved") fail("W-FOREIGN-0001", { reason: "alreadyResolved" })
      const language = normalizeLanguage(operation.language)
      const available = operation.available === true
      if (operation.mode !== "build" && operation.mode !== "tooling") {
        fail("W-FOREIGN-0001", { reason: "invalidMode" })
      }
      if (operation.mode === "build" && !available) fail("W-FOREIGN-0001", { reason: "adapterMissing" })
      if (available && operation.locked !== true) fail("W-FOREIGN-0001", { reason: "adapterUnlocked" })
      if (
        available &&
        (!operation.adapterDigest || !operation.scannerDigest || !operation.scannerAbi)
      ) {
        fail("W-FOREIGN-0002", { reason: "scannerIdentityMissing" })
      }
      if (available && (!validDigest(operation.adapterDigest) || !validDigest(operation.scannerDigest))) {
        fail("W-FOREIGN-0002", { reason: "scannerDigestInvalid" })
      }
      if (available && operation.scannerAbi !== SCANNER_ABI) {
        fail("W-FOREIGN-0002", { reason: "scannerAbi" })
      }
      if (available && typeof operation.scannerProfile !== "string") {
        fail("W-FOREIGN-0002", { reason: "scannerProfileMissing" })
      }
      if (available && language === "c" && operation.scannerProfile !== "c-inline-1") {
        fail("W-FOREIGN-0002", { reason: "scannerProfile" })
      }
      state.language = language
      state.adapterDigest = operation.adapterDigest ?? null
      state.scannerDigest = operation.scannerDigest ?? null
      state.scannerAbi = operation.scannerAbi ?? null
      state.scannerProfile = available ? operation.scannerProfile : "structural-fallback-1"
      state.authoritative = available
      state.phase = "resolved"
      return
    }

    case "scan": {
      if (state.phase !== "resolved") fail("W-FOREIGN-0003", { reason: "scanPhase" })
      state.scan = scanForeignBody(operation, state.scannerProfile)
      state.revision += 1
      state.phase = state.authoritative ? "scanned" : "preservedUnvalidated"
      return
    }

    case "mapDiagnostic": {
      if (!state.scan) fail("W-FOREIGN-0009", { reason: "scanMissing" })
      if (!Number.isSafeInteger(operation.start) || !Number.isSafeInteger(operation.end)) {
        fail("W-FOREIGN-0009", { reason: "spanInvalid" })
      }
      if (operation.start < 0 || operation.end < operation.start || operation.end > state.scan.byteLength) {
        fail("W-FOREIGN-0009", { reason: "spanOutsideBody" })
      }
      state.mappedDiagnostics.push({
        code: operation.code,
        start: state.scan.bodyStart + operation.start,
        end: state.scan.bodyStart + operation.end,
      })
      return
    }

    case "replaceSource": {
      if (!state.scan) fail("W-FOREIGN-0003", { reason: "replaceBeforeScan" })
      state.scan = scanForeignBody(operation, state.scannerProfile)
      state.revision += 1
      state.mappedDiagnostics = []
      state.recipe = null
      state.phase = state.authoritative ? "scanned" : "preservedUnvalidated"
      return
    }

    case "publish": {
      if (!state.authoritative) fail("W-FOREIGN-0008", { reason: "fallbackNotBuildEvidence" })
      if (state.phase !== "scanned") fail("W-FOREIGN-0008", { reason: "scanNotReady" })
      const scannerDigest = operation.scannerDigest ?? state.scannerDigest
      const bodyDigest = operation.bodyDigest ?? state.scan.bodyDigest
      if (scannerDigest !== state.scannerDigest) {
        fail("W-FOREIGN-0007", { reason: "scannerDigest" })
      }
      if (bodyDigest !== state.scan.bodyDigest) {
        fail("W-FOREIGN-0007", { reason: "bodyDigest" })
      }
      state.recipe = {
        language: state.language,
        adapterDigest: state.adapterDigest,
        scannerDigest: state.scannerDigest,
        scannerAbi: state.scannerAbi,
        scannerProfile: state.scannerProfile,
        bodyDigest: state.scan.bodyDigest,
      }
      state.phase = "published"
      return
    }

    default:
      fail("W-FOREIGN-0002", { reason: "operation", operation: operation.op })
  }
}

export function runForeignBodyOperations(operations) {
  const state = initialState()
  try {
    for (const operation of operations) applyOperation(state, operation)
    return {
      status: state.phase === "preservedUnvalidated" ? "info" : "accepted",
      error: null,
      errorFacts: null,
      state,
    }
  } catch (error) {
    if (!(error instanceof ForeignBodyModelError)) throw error
    return {
      status: "rejected",
      error: error.code,
      errorFacts: error.facts,
      state,
    }
  }
}
