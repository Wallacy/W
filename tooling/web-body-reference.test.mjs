import assert from "node:assert/strict"
import test from "node:test"
import {
  attachFormData,
  deriveBlob,
  mutateFormData,
  normalizeBlobType,
  parseFormData,
} from "./web-body-machine.mjs"

const limits = {
  maximumEntries: 4,
  maximumNameBytes: 32,
  maximumFilenameBytes: 64,
  maximumTextBytes: 64,
  maximumBlobBytes: 1024,
  maximumPayloadBytes: 1024,
  maximumEncodedBytes: 4096,
}

test("Blob normalization and slicing derive value facts", () => {
  assert.equal(normalizeBlobType("IMAGE/JPEG"), "image/jpeg")
  assert.equal(normalizeBlobType("text/🪐"), "")
  assert.deepEqual(
    deriveBlob({ subject: "blob", operation: "slice", size: 10, start: 2, end: 6 }),
    { accepted: true, start: 2, size: 4, payloadCopies: 0, type: "" },
  )
})

test("Blob limits reject without consuming the logical value", () => {
  assert.deepEqual(
    deriveBlob({ subject: "blob", operation: "materialize", size: 65, maximumBytes: 64 }),
    { accepted: false, reason: "limitExceeded", preservedSize: 65 },
  )
})

test("FormData set preserves the first position and removes duplicates", () => {
  const result = mutateFormData({
    operation: "set",
    limits,
    entries: [
      { name: "a", kind: "text", value: "1" },
      { name: "b", kind: "text", value: "2" },
      { name: "a", kind: "text", value: "3" },
    ],
    entry: { name: "a", kind: "text", value: "4" },
  })
  assert.equal(result.accepted, true)
  assert.deepEqual(result.entries.map(({ name, value }) => ({ name, value })), [
    { name: "a", value: "4" },
    { name: "b", value: "2" },
  ])
})

test("FormData rejection keeps the previous entries", () => {
  const entries = [{ name: "a", kind: "blob", size: 900, filename: "a.bin" }]
  const result = mutateFormData({
    operation: "append",
    limits,
    entries,
    entry: { name: "b", kind: "blob", size: 200, filename: "b.bin" },
  })
  assert.equal(result.accepted, false)
  assert.equal(result.reason, "payloadBytes")
  assert.deepEqual(result.entries, entries)

  const metadataBound = mutateFormData({
    operation: "append",
    limits: { ...limits, maximumPayloadBytes: 4 },
    entries: [],
    entry: { name: "abc", kind: "text", value: "xy" },
  })
  assert.equal(metadataBound.accepted, false)
  assert.equal(metadataBound.reason, "payloadBytes")
})

test("multipart attachment requires host boundary and streams Blob parts", () => {
  const entries = [
    { name: "station", kind: "text", value: "Milliways" },
    { name: "image", kind: "blob", size: 100, filename: "horizon.jpg" },
  ]
  const result = attachFormData({
    entries,
    limits,
    headers: [],
    boundary: { generatedByHost: true, length: 48, entropyBits: 128 },
    messageMaximumBytes: 4096,
  })
  assert.equal(result.accepted, true)
  assert.equal(result.streamedBlobParts, 1)
  assert.equal(result.materializedMultipart, false)

  const callerBoundary = attachFormData({
    entries,
    limits,
    headers: [],
    boundary: { generatedByHost: false, length: 48, entropyBits: 128 },
    messageMaximumBytes: 4096,
  })
  assert.equal(callerBoundary.reason, "invalidBoundaryEvidence")
})

test("FormData parsing consumes supported bodies without path authority", () => {
  const result = parseFormData({
    mediaType: "multipart/form-data; boundary=host",
    encodedBytes: 128,
    limits,
    entries: [{ name: "image", kind: "blob", size: 10, filename: "../horizon.jpg" }],
  })
  assert.equal(result.accepted, true)
  assert.equal(result.consumed, true)
  assert.equal(result.pathAuthority, false)

  const missingBoundary = parseFormData({
    mediaType: "multipart/form-data",
    encodedBytes: 0,
    limits,
    entries: [],
  })
  assert.equal(missingBoundary.reason, "malformed")

  const urlEncodedBlob = parseFormData({
    mediaType: "application/x-www-form-urlencoded",
    encodedBytes: 8,
    limits,
    entries: [{ name: "image", kind: "blob", size: 1, filename: "a.bin" }],
  })
  assert.equal(urlEncodedBlob.reason, "malformed")
})
