import { createHash, timingSafeEqual } from "node:crypto";

const MAGIC = Uint8Array.from([0x57, 0x4d, 0x65, 0x74, 0x61, 0x31, 0x0d, 0x0a]);
const HEADER_BYTES = 32;
const DIRECTORY_SCHEMA = 1;
const DIGEST_SHA256 = 1;

const DEFAULT_LIMITS = Object.freeze({
  directoryBytes: 64 * 1024 * 1024,
  payloadBytes: 16 * 1024 * 1024 * 1024,
  chunkBytes: 1024 * 1024 * 1024,
  entries: 1_048_576,
  collectionItems: 1_048_576,
  decodedItems: 4_194_304,
  textBytes: 1024 * 1024,
  byteStringBytes: 1024 * 1024 * 1024,
  nesting: 64,
});

const PROFILES = new Map([
  [1, { name: "interface", required: [1, 2], allowed: new Set([1, 2, 3, 4, 5]) }],
  [2, { name: "objectAbi", required: [16, 17, 18, 19], allowed: new Set([16, 17, 18, 19]) }],
]);

const KNOWN_KINDS = new Set([...PROFILES.values()].flatMap((profile) => [...profile.allowed]));
const SUPPORTED_REQUIRED_FEATURES = new Set();

export class WMetaError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function fail(code) {
  throw new WMetaError(code);
}

function bytes(value) {
  if (value instanceof Uint8Array) return value;
  if (ArrayBuffer.isView(value)) {
    return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  }
  if (value instanceof ArrayBuffer) return new Uint8Array(value);
  fail("invalidByteInput");
}

function concatenate(parts) {
  const normalized = parts.map(bytes);
  const length = normalized.reduce((total, part) => total + part.byteLength, 0);
  const result = new Uint8Array(length);
  let offset = 0;
  for (const part of normalized) {
    result.set(part, offset);
    offset += part.byteLength;
  }
  return result;
}

function compareBytes(left, right) {
  const count = Math.min(left.byteLength, right.byteLength);
  for (let index = 0; index < count; index += 1) {
    if (left[index] !== right[index]) return left[index] - right[index];
  }
  return left.byteLength - right.byteLength;
}

function asBigInt(value) {
  if (typeof value === "bigint") return value;
  if (Number.isSafeInteger(value)) return BigInt(value);
  fail("cborIntegerOutOfRange");
}

function encodeHead(major, argument) {
  const value = asBigInt(argument);
  if (value < 0n || value > 0xffff_ffff_ffff_ffffn) {
    fail("cborIntegerOutOfRange");
  }
  if (value < 24n) return Uint8Array.of((major << 5) | Number(value));
  if (value <= 0xffn) return Uint8Array.of((major << 5) | 24, Number(value));
  if (value <= 0xffffn) {
    return Uint8Array.of(
      (major << 5) | 25,
      Number((value >> 8n) & 0xffn),
      Number(value & 0xffn),
    );
  }
  if (value <= 0xffff_ffffn) {
    return Uint8Array.of(
      (major << 5) | 26,
      Number((value >> 24n) & 0xffn),
      Number((value >> 16n) & 0xffn),
      Number((value >> 8n) & 0xffn),
      Number(value & 0xffn),
    );
  }
  const output = new Uint8Array(9);
  output[0] = (major << 5) | 27;
  let remaining = value;
  for (let index = 8; index >= 1; index -= 1) {
    output[index] = Number(remaining & 0xffn);
    remaining >>= 8n;
  }
  return output;
}

export function encodeCborMapEntries(entries, { sort = true, rejectDuplicates = true } = {}) {
  const encoded = entries.map(([key, value]) => {
    const encodedKey = encodeCbor(key);
    return { encodedKey, encodedValue: encodeCbor(value) };
  });
  if (sort) encoded.sort((left, right) => compareBytes(left.encodedKey, right.encodedKey));
  if (rejectDuplicates) {
    for (let index = 1; index < encoded.length; index += 1) {
      if (compareBytes(encoded[index - 1].encodedKey, encoded[index].encodedKey) === 0) {
        fail("duplicateCborMapKey");
      }
    }
  }
  return concatenate([
    encodeHead(5, encoded.length),
    ...encoded.flatMap((entry) => [entry.encodedKey, entry.encodedValue]),
  ]);
}

export function encodeCbor(value) {
  if (value === null) return Uint8Array.of(0xf6);
  if (value === false) return Uint8Array.of(0xf4);
  if (value === true) return Uint8Array.of(0xf5);
  if (typeof value === "number" || typeof value === "bigint") {
    const integer = asBigInt(value);
    return integer >= 0n ? encodeHead(0, integer) : encodeHead(1, -1n - integer);
  }
  if (typeof value === "string") {
    const encoded = new TextEncoder().encode(value);
    return concatenate([encodeHead(3, encoded.byteLength), encoded]);
  }
  if (value instanceof Uint8Array || ArrayBuffer.isView(value) || value instanceof ArrayBuffer) {
    const encoded = bytes(value);
    return concatenate([encodeHead(2, encoded.byteLength), encoded]);
  }
  if (Array.isArray(value)) {
    return concatenate([encodeHead(4, value.length), ...value.map(encodeCbor)]);
  }
  if (value instanceof Map) return encodeCborMapEntries([...value.entries()]);
  fail("unsupportedCborValue");
}

function readArgument(state, additional) {
  if (additional < 24) return BigInt(additional);
  const widths = new Map([[24, 1], [25, 2], [26, 4], [27, 8]]);
  const width = widths.get(additional);
  if (width === undefined) fail(additional === 31 ? "indefiniteCbor" : "malformedCbor");
  if (state.offset + width > state.input.byteLength) fail("truncatedCbor");
  let value = 0n;
  for (let index = 0; index < width; index += 1) {
    value = (value << 8n) | BigInt(state.input[state.offset]);
    state.offset += 1;
  }
  const minimum = [24n, 0x100n, 0x1_0000n, 0x1_0000_0000n][width === 1 ? 0 : width === 2 ? 1 : width === 4 ? 2 : 3];
  if (value < minimum) fail("nonCanonicalCborInteger");
  return value;
}

function boundedNumber(value, maximum, code = "cborLimitExceeded") {
  if (value > BigInt(maximum) || value > BigInt(Number.MAX_SAFE_INTEGER)) fail(code);
  return Number(value);
}

function decodeItem(state, depth) {
  if (depth > state.limits.nesting) fail("cborNestingLimitExceeded");
  state.decodedItems += 1;
  if (state.decodedItems > state.limits.decodedItems) fail("cborItemLimitExceeded");
  if (state.offset >= state.input.byteLength) fail("truncatedCbor");

  const initial = state.input[state.offset];
  state.offset += 1;
  const major = initial >> 5;
  const additional = initial & 0x1f;

  if (major === 7) {
    if (additional === 20) return false;
    if (additional === 21) return true;
    if (additional === 22) return null;
    if ([25, 26, 27].includes(additional)) fail("cborFloatForbidden");
    if (additional === 31) fail("indefiniteCbor");
    fail("unsupportedCborValue");
  }

  const argument = readArgument(state, additional);
  if (major === 0) return argument;
  if (major === 1) return -1n - argument;

  if (major === 2 || major === 3) {
    const maximum = major === 2 ? state.limits.byteStringBytes : state.limits.textBytes;
    const length = boundedNumber(argument, maximum);
    if (state.offset + length > state.input.byteLength) fail("truncatedCbor");
    const content = state.input.subarray(state.offset, state.offset + length);
    state.offset += length;
    if (major === 2) return content;
    try {
      return new TextDecoder("utf-8", { fatal: true }).decode(content);
    } catch {
      fail("invalidCborUtf8");
    }
  }

  if (major === 4) {
    const count = boundedNumber(argument, state.limits.collectionItems);
    const result = [];
    for (let index = 0; index < count; index += 1) {
      result.push(decodeItem(state, depth + 1));
    }
    return result;
  }

  if (major === 5) {
    const count = boundedNumber(argument, state.limits.collectionItems);
    const result = new Map();
    let previousKeyBytes;
    for (let index = 0; index < count; index += 1) {
      const keyStart = state.offset;
      const key = decodeItem(state, depth + 1);
      const keyBytes = state.input.subarray(keyStart, state.offset);
      if (typeof key !== "bigint" || key < 0n) fail("cborMapKeyNotUnsigned");
      if (previousKeyBytes !== undefined) {
        const order = compareBytes(previousKeyBytes, keyBytes);
        if (order === 0) fail("duplicateCborMapKey");
        if (order > 0) fail("nonCanonicalCborMapOrder");
      }
      previousKeyBytes = keyBytes;
      result.set(key, decodeItem(state, depth + 1));
    }
    return result;
  }

  if (major === 6) fail("cborTagForbidden");
  fail("malformedCbor");
}

export function parseCanonicalCbor(input, limits = {}) {
  const encoded = bytes(input);
  const state = {
    input: encoded,
    offset: 0,
    decodedItems: 0,
    limits: { ...DEFAULT_LIMITS, ...limits },
  };
  const value = decodeItem(state, 0);
  if (state.offset !== encoded.byteLength) fail("trailingCborData");
  return value;
}

function mapValue(map, key, code) {
  if (!(map instanceof Map) || !map.has(BigInt(key))) fail(code);
  return map.get(BigInt(key));
}

function unsigned(value, code) {
  if (typeof value !== "bigint" || value < 0n) fail(code);
  return value;
}

function byteString(value, length, code) {
  if (!(value instanceof Uint8Array) || (length !== undefined && value.byteLength !== length)) {
    fail(code);
  }
  return value;
}

function boolean(value, code) {
  if (typeof value !== "boolean") fail(code);
  return value;
}

function array(value, code) {
  if (!Array.isArray(value)) fail(code);
  return value;
}

function sha256(input) {
  return new Uint8Array(createHash("sha256").update(bytes(input)).digest());
}

export function identityFor(label) {
  return sha256(new TextEncoder().encode(label));
}

function equalDigest(left, right) {
  const leftBuffer = Buffer.from(left);
  const rightBuffer = Buffer.from(right);
  return leftBuffer.byteLength === rightBuffer.byteLength && timingSafeEqual(leftBuffer, rightBuffer);
}

function readU16(input, offset) {
  return (input[offset] << 8) | input[offset + 1];
}

function readU32(input, offset) {
  return (
    input[offset] * 0x1_000000 +
    (input[offset + 1] << 16) +
    (input[offset + 2] << 8) +
    input[offset + 3]
  );
}

function readU64(input, offset) {
  let value = 0n;
  for (let index = 0; index < 8; index += 1) {
    value = (value << 8n) | BigInt(input[offset + index]);
  }
  return value;
}

function writeU16(output, offset, value) {
  output[offset] = (value >>> 8) & 0xff;
  output[offset + 1] = value & 0xff;
}

function writeU32(output, offset, value) {
  output[offset] = (value >>> 24) & 0xff;
  output[offset + 1] = (value >>> 16) & 0xff;
  output[offset + 2] = (value >>> 8) & 0xff;
  output[offset + 3] = value & 0xff;
}

function writeU64(output, offset, value) {
  let remaining = asBigInt(value);
  for (let index = 7; index >= 0; index -= 1) {
    output[offset + index] = Number(remaining & 0xffn);
    remaining >>= 8n;
  }
  if (remaining !== 0n) fail("wmetaLengthOutOfRange");
}

export function assembleWMeta(directory, payload, header = {}) {
  const directoryBytes = bytes(directory);
  const payloadBytes = bytes(payload);
  const output = new Uint8Array(HEADER_BYTES + directoryBytes.byteLength + payloadBytes.byteLength);
  output.set(header.magic ?? MAGIC, 0);
  writeU16(output, 8, header.headerBytes ?? HEADER_BYTES);
  writeU16(output, 10, header.directorySchema ?? DIRECTORY_SCHEMA);
  writeU32(output, 12, header.flags ?? 0);
  writeU64(output, 16, header.directoryBytes ?? directoryBytes.byteLength);
  writeU64(output, 24, header.payloadBytes ?? payloadBytes.byteLength);
  output.set(directoryBytes, HEADER_BYTES);
  output.set(payloadBytes, HEADER_BYTES + directoryBytes.byteLength);
  return output;
}

function entryDirectoryMap(entry) {
  const fields = [
    [0, entry.kind],
    [1, entry.identity],
    [2, entry.schema ?? [1, 0]],
    [3, entry.critical ?? true],
    [4, entry.declaredLength ?? entry.chunk.byteLength],
    [5, [entry.digestAlgorithm ?? DIGEST_SHA256, entry.digest ?? sha256(entry.chunk)]],
  ];
  const omitted = new Set(entry.omitFields ?? []);
  return new Map(fields.filter(([field]) => !omitted.has(field)));
}

function normalizeEntry(entry) {
  const chunk = entry.encoded === true ? bytes(entry.value) : encodeCbor(entry.value);
  return {
    ...entry,
    chunk,
    identity: entry.identity ?? identityFor(`wmeta-kind-${entry.kind}`),
  };
}

export function buildWMeta({
  profile,
  entries,
  requiredFeatures = [],
  sortEntries = true,
  sortFeatures = true,
  rootFields = [],
} = {}) {
  const normalized = entries.map(normalizeEntry);
  if (sortEntries) {
    normalized.sort((left, right) => {
      const kindOrder = left.kind - right.kind;
      return kindOrder !== 0 ? kindOrder : compareBytes(left.identity, right.identity);
    });
  }
  const features = [...requiredFeatures];
  if (sortFeatures) features.sort((left, right) => left - right);
  const directory = new Map([
    [0, DIRECTORY_SCHEMA],
    [1, profile],
    [2, features],
    [3, normalized.map(entryDirectoryMap)],
    ...rootFields,
  ]);
  return assembleWMeta(
    encodeCbor(directory),
    concatenate(normalized.map((entry) => entry.chunk)),
  );
}

function validateSchema(value) {
  const parts = array(value, "malformedChunkSchema");
  if (parts.length !== 2) fail("malformedChunkSchema");
  const major = unsigned(parts[0], "malformedChunkSchema");
  const minor = unsigned(parts[1], "malformedChunkSchema");
  if (major > 0xffffn || minor > 0xffffn) fail("malformedChunkSchema");
  return { major: Number(major), minor: Number(minor) };
}

function validateDirectory(directory, payload, limits) {
  if (!(directory instanceof Map)) fail("malformedWMetaDirectory");
  const schema = unsigned(mapValue(directory, 0, "malformedWMetaDirectory"), "malformedWMetaDirectory");
  if (schema !== BigInt(DIRECTORY_SCHEMA)) fail("unsupportedWMetaDirectorySchema");

  const profileId = Number(unsigned(mapValue(directory, 1, "malformedWMetaDirectory"), "malformedWMetaDirectory"));
  const profile = PROFILES.get(profileId);
  if (!profile) fail("unknownWMetaProfile");

  const features = array(mapValue(directory, 2, "malformedWMetaDirectory"), "malformedWMetaDirectory");
  let previousFeature = -1n;
  const normalizedFeatures = [];
  for (const featureValue of features) {
    const feature = unsigned(featureValue, "malformedWMetaFeature");
    if (feature <= previousFeature) {
      fail(feature === previousFeature ? "duplicateWMetaFeature" : "unsortedWMetaFeatures");
    }
    previousFeature = feature;
    normalizedFeatures.push(Number(feature));
  }
  if (normalizedFeatures.some((feature) => !SUPPORTED_REQUIRED_FEATURES.has(feature))) {
    fail("unknownRequiredWMetaFeature");
  }

  const encodedEntries = array(mapValue(directory, 3, "malformedWMetaDirectory"), "malformedWMetaDirectory");
  if (encodedEntries.length > limits.entries) fail("wmetaEntryLimitExceeded");
  const entries = [];
  const coreCounts = new Map(profile.required.map((kind) => [kind, 0]));
  let previous;
  let payloadOffset = 0n;

  for (const encodedEntry of encodedEntries) {
    if (!(encodedEntry instanceof Map)) fail("malformedWMetaEntry");
    const kindValue = unsigned(mapValue(encodedEntry, 0, "malformedWMetaEntry"), "malformedWMetaEntry");
    if (kindValue === 0n || kindValue > 0xffffn) fail("malformedWMetaEntry");
    const kind = Number(kindValue);
    const identity = byteString(mapValue(encodedEntry, 1, "malformedWMetaEntry"), 32, "malformedWMetaIdentity");
    const schemaValue = validateSchema(mapValue(encodedEntry, 2, "malformedWMetaEntry"));
    const critical = boolean(mapValue(encodedEntry, 3, "malformedWMetaEntry"), "malformedWMetaEntry");
    const length = unsigned(mapValue(encodedEntry, 4, "malformedWMetaEntry"), "malformedWMetaEntry");
    if (length === 0n || length > BigInt(limits.chunkBytes)) fail("wmetaChunkLimitExceeded");

    const digestRecord = array(mapValue(encodedEntry, 5, "malformedWMetaEntry"), "malformedWMetaDigest");
    if (digestRecord.length !== 2) fail("malformedWMetaDigest");
    const digestAlgorithm = unsigned(digestRecord[0], "malformedWMetaDigest");
    const digest = byteString(digestRecord[1], 32, "malformedWMetaDigest");

    if (previous !== undefined) {
      const order = kind === previous.kind
        ? compareBytes(previous.identity, identity)
        : previous.kind - kind;
      if (order === 0) fail("duplicateWMetaEntry");
      if (order > 0) fail("unsortedWMetaEntries");
    }
    previous = { kind, identity };

    const known = KNOWN_KINDS.has(kind);
    if (known && !profile.allowed.has(kind)) fail("wmetaProfileChunkMismatch");
    if (!known && critical) fail("unknownCriticalWMetaChunk");
    if (known && schemaValue.major !== 1 && critical) fail("unsupportedCriticalWMetaSchema");
    if (coreCounts.has(kind)) {
      if (!critical) fail("wmetaCoreChunkMustBeCritical");
      coreCounts.set(kind, coreCounts.get(kind) + 1);
      if (coreCounts.get(kind) > 1) fail("duplicateWMetaCoreChunk");
    }

    entries.push({
      kind,
      identity,
      schema: schemaValue,
      critical,
      length,
      digestAlgorithm,
      digest,
      offset: payloadOffset,
      known,
    });
    payloadOffset += length;
    if (payloadOffset > BigInt(limits.payloadBytes)) fail("wmetaPayloadLimitExceeded");
  }

  for (const count of coreCounts.values()) {
    if (count !== 1) fail("missingWMetaCoreChunk");
  }
  if (payloadOffset !== BigInt(payload.byteLength)) fail("wmetaPayloadLengthMismatch");
  return { profileId, profile, entries };
}

export function readWMeta(input, { mode = "core", limits = {} } = {}) {
  const encoded = bytes(input);
  const effectiveLimits = { ...DEFAULT_LIMITS, ...limits };
  if (!["directory", "core", "full"].includes(mode)) fail("unknownWMetaOpenMode");
  if (encoded.byteLength < HEADER_BYTES) fail("truncatedWMetaHeader");
  if (compareBytes(encoded.subarray(0, MAGIC.byteLength), MAGIC) !== 0) fail("invalidWMetaMagic");
  if (readU16(encoded, 8) !== HEADER_BYTES) fail("unsupportedWMetaHeader");
  if (readU16(encoded, 10) !== DIRECTORY_SCHEMA) fail("unsupportedWMetaDirectorySchema");
  if (readU32(encoded, 12) !== 0) fail("unsupportedWMetaHeaderFlags");

  const directoryLength = readU64(encoded, 16);
  const payloadLength = readU64(encoded, 24);
  if (directoryLength === 0n || directoryLength > BigInt(effectiveLimits.directoryBytes)) {
    fail("wmetaDirectoryLimitExceeded");
  }
  if (payloadLength > BigInt(effectiveLimits.payloadBytes)) fail("wmetaPayloadLimitExceeded");
  const totalLength = BigInt(HEADER_BYTES) + directoryLength + payloadLength;
  if (totalLength !== BigInt(encoded.byteLength)) fail("wmetaContainerLengthMismatch");

  const directoryEnd = HEADER_BYTES + Number(directoryLength);
  const directoryBytes = encoded.subarray(HEADER_BYTES, directoryEnd);
  const payload = encoded.subarray(directoryEnd);
  const directory = parseCanonicalCbor(directoryBytes, effectiveLimits);
  const validated = validateDirectory(directory, payload, effectiveLimits);

  if (mode !== "directory") {
    for (const entry of validated.entries) {
      const mustRead = mode === "full" || entry.critical;
      if (!mustRead) continue;
      if (entry.digestAlgorithm !== BigInt(DIGEST_SHA256)) {
        fail(entry.critical ? "unsupportedCriticalWMetaDigest" : "unsupportedWMetaDigest");
      }
      const start = Number(entry.offset);
      const end = start + Number(entry.length);
      const chunk = payload.subarray(start, end);
      if (!equalDigest(sha256(chunk), entry.digest)) fail("wmetaChunkDigestMismatch");
      parseCanonicalCbor(chunk, effectiveLimits);
    }
  }

  return {
    schema: "wmeta1-summary-v1",
    profile: validated.profile.name,
    mode,
    directoryBytes: Number(directoryLength),
    payloadBytes: Number(payloadLength),
    entries: validated.entries.map((entry) => ({
      kind: entry.kind,
      identity: Buffer.from(entry.identity).toString("hex"),
      schema: `${entry.schema.major}.${entry.schema.minor}`,
      critical: entry.critical,
      offset: Number(entry.offset),
      length: Number(entry.length),
    })),
  };
}

export const wmetaConstants = Object.freeze({
  magic: MAGIC,
  headerBytes: HEADER_BYTES,
  directorySchema: DIRECTORY_SCHEMA,
  digestSha256: DIGEST_SHA256,
  defaultLimits: DEFAULT_LIMITS,
  profiles: PROFILES,
});
