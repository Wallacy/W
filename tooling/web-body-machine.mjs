const supportedFormMediaTypes = new Set([
  "application/x-www-form-urlencoded",
  "multipart/form-data",
])

function safeAdd(left, right) {
  const result = left + right
  return Number.isSafeInteger(result) ? result : null
}

function byteCount(value) {
  return new TextEncoder().encode(value).byteLength
}

export function normalizeBlobType(value) {
  for (const byte of new TextEncoder().encode(value)) {
    if (byte < 0x20 || byte > 0x7e) return ""
  }
  return value.replace(/[A-Z]/g, (letter) => letter.toLowerCase())
}

export function deriveBlob(input) {
  const size = input.size ?? 0
  if (!Number.isSafeInteger(size) || size < 0) {
    return { accepted: false, reason: "invalidSize" }
  }

  if (input.operation === "normalize") {
    return { accepted: true, type: normalizeBlobType(input.type ?? "") }
  }
  if (input.operation === "duplicate") {
    return {
      accepted: true,
      logicalOwners: 2,
      payloadCopies: 0,
      retainVisible: input.explicitCopy === true,
    }
  }
  if (input.operation === "slice") {
    const start = input.start ?? 0
    const end = input.end ?? size
    if (![start, end].every((value) => Number.isSafeInteger(value) && value >= 0)
      || start > end
      || end > size) {
      return { accepted: false, reason: "invalidRange", preservedSize: size }
    }
    return {
      accepted: true,
      start,
      size: end - start,
      payloadCopies: 0,
      type: normalizeBlobType(input.type ?? ""),
    }
  }
  if (input.operation === "materialize") {
    const maximumBytes = input.maximumBytes
    if (!Number.isSafeInteger(maximumBytes) || maximumBytes < 1) {
      return { accepted: false, reason: "invalidLimit" }
    }
    if (size > maximumBytes) {
      return { accepted: false, reason: "limitExceeded", preservedSize: size }
    }
    return { accepted: true, copiedBytes: size, size }
  }
  if (input.operation === "stream") {
    const chunkBytes = input.chunkBytes
    if (!Number.isSafeInteger(chunkBytes) || chunkBytes < 1) {
      return { accepted: false, reason: "invalidChunkBound" }
    }
    return {
      accepted: true,
      independentCursor: true,
      maximumChunkBytes: chunkBytes,
      chunks: size === 0 ? 0 : Math.ceil(size / chunkBytes),
      payloadCopies: size,
    }
  }
  return { accepted: false, reason: "unknownBlobOperation" }
}

function entryPayloadBytes(entry) {
  return entry.kind === "text" ? byteCount(entry.value ?? "") : entry.size ?? 0
}

function entryRetainedBytes(entry) {
  let total = safeAdd(byteCount(entry.name ?? ""), entryPayloadBytes(entry))
  if (total === null) return null
  if (entry.kind === "blob") total = safeAdd(total, byteCount(entry.filename ?? "blob"))
  return total
}

function validateEntry(entry, limits) {
  if (!entry || !["text", "blob"].includes(entry.kind)) return "invalidEntry"
  if (byteCount(entry.name ?? "") > limits.maximumNameBytes) return "nameBytes"
  if (entry.kind === "text") {
    if (byteCount(entry.value ?? "") > limits.maximumTextBytes) return "textBytes"
  } else {
    if (!Number.isSafeInteger(entry.size) || entry.size < 0) return "invalidBlobSize"
    if (entry.size > limits.maximumBlobBytes) return "blobBytes"
    if (byteCount(entry.filename ?? "blob") > limits.maximumFilenameBytes) return "filenameBytes"
  }
  return null
}

function payloadBytes(entries) {
  let total = 0
  for (const entry of entries) {
    total = safeAdd(total, entryRetainedBytes(entry))
    if (total === null) return null
  }
  return total
}

function validateForm(entries, limits) {
  if (entries.length > limits.maximumEntries) return "entries"
  for (const entry of entries) {
    const error = validateEntry(entry, limits)
    if (error) return error
  }
  const payload = payloadBytes(entries)
  if (payload === null || payload > limits.maximumPayloadBytes) return "payloadBytes"
  return null
}

function copyEntries(entries) {
  return entries.map((entry) => ({ ...entry }))
}

export function mutateFormData(input) {
  const original = copyEntries(input.entries ?? [])
  const candidate = copyEntries(original)
  const operation = input.operation
  const name = input.entry?.name ?? input.name

  if (operation === "append") candidate.push({ ...input.entry })
  else if (operation === "delete") {
    candidate.splice(0, candidate.length, ...candidate.filter((entry) => entry.name !== name))
  } else if (operation === "set") {
    const first = candidate.findIndex((entry) => entry.name === name)
    const filtered = candidate.filter((entry) => entry.name !== name)
    const index = first < 0 ? filtered.length : first
    filtered.splice(index, 0, { ...input.entry })
    candidate.splice(0, candidate.length, ...filtered)
  } else {
    return { accepted: false, reason: "unknownMutation", entries: original }
  }

  const reason = validateForm(candidate, input.limits)
  if (reason) return { accepted: false, reason, entries: original }
  return {
    accepted: true,
    entries: candidate,
    payloadBytes: payloadBytes(candidate),
  }
}

function multipartUpperBound(entries, boundaryLength) {
  let total = boundaryLength + 6
  for (const entry of entries) {
    const nameBytes = byteCount(entry.name ?? "")
    const filenameBytes = entry.kind === "blob" ? byteCount(entry.filename ?? "blob") : 0
    const overhead = boundaryLength + 128 + (nameBytes * 3) + (filenameBytes * 3)
    total = safeAdd(total, overhead)
    if (total === null) return null
    total = safeAdd(total, entryPayloadBytes(entry))
    if (total === null) return null
  }
  return total
}

export function attachFormData(input) {
  const entries = copyEntries(input.entries ?? [])
  const logicalError = validateForm(entries, input.limits)
  if (logicalError) return { accepted: false, reason: logicalError, published: false }
  if ((input.headers ?? []).some((name) => name.toLowerCase() === "content-type")) {
    return { accepted: false, reason: "contentTypeControlled", published: false }
  }

  const boundary = input.boundary ?? {}
  if (boundary.generatedByHost !== true
    || !Number.isSafeInteger(boundary.length)
    || boundary.length < 27
    || boundary.length > 70
    || !Number.isSafeInteger(boundary.entropyBits)
    || boundary.entropyBits < 95) {
    return { accepted: false, reason: "invalidBoundaryEvidence", published: false }
  }

  const encodedBytes = multipartUpperBound(entries, boundary.length)
  const maximum = Math.min(input.limits.maximumEncodedBytes, input.messageMaximumBytes)
  if (encodedBytes === null || encodedBytes > maximum) {
    return { accepted: false, reason: "encodedBytes", published: false }
  }

  return {
    accepted: true,
    published: true,
    contentType: "multipart/form-data; boundary=<host>",
    encodedBytes,
    entryOrder: entries.map((entry) => entry.name),
    streamedBlobParts: entries.filter((entry) => entry.kind === "blob").length,
    materializedMultipart: false,
  }
}

export function parseFormData(input) {
  const mediaTypeParts = (input.mediaType ?? "").split(";")
  const mediaType = mediaTypeParts[0].trim().toLowerCase()
  if (!supportedFormMediaTypes.has(mediaType)) {
    return { accepted: false, reason: "unsupportedMediaType", consumed: true }
  }
  if (mediaType === "multipart/form-data") {
    const boundary = mediaTypeParts.slice(1).find((part) => {
      const separator = part.indexOf("=")
      return separator >= 0
        && part.slice(0, separator).trim().toLowerCase() === "boundary"
        && part.slice(separator + 1).trim().length > 0
    })
    if (!boundary) return { accepted: false, reason: "malformed", consumed: true }
  }
  if (input.malformed === true) {
    return { accepted: false, reason: "malformed", consumed: true }
  }
  if (!Number.isSafeInteger(input.encodedBytes)
    || input.encodedBytes < 0
    || input.encodedBytes > input.limits.maximumEncodedBytes) {
    return { accepted: false, reason: "encodedBytes", consumed: true }
  }
  if (mediaType === "application/x-www-form-urlencoded"
    && (input.entries ?? []).some((entry) => entry.kind === "blob")) {
    return { accepted: false, reason: "malformed", consumed: true }
  }
  const error = validateForm(input.entries ?? [], input.limits)
  if (error) return { accepted: false, reason: error, consumed: true }
  return {
    accepted: true,
    consumed: true,
    entries: copyEntries(input.entries ?? []),
    filenamesAreData: true,
    pathAuthority: false,
  }
}

export function deriveWebBody(input) {
  if (input.subject === "blob") return deriveBlob(input)
  if (input.subject === "formMutation") return mutateFormData(input)
  if (input.subject === "formAttachment") return attachFormData(input)
  if (input.subject === "formParse") return parseFormData(input)
  return { accepted: false, reason: "unknownSubject" }
}
