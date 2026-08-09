import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  WMetaError,
  assembleWMeta,
  buildWMeta,
  encodeCbor,
  encodeCborMapEntries,
  identityFor,
  readWMeta,
} from "./wmeta-reference.mjs";
import { ledgerIdSet as designDecisionIds } from "./design-ledger.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "wmeta-cases.json");
const snapshotPath = path.join(toolingDirectory, "wmeta-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const errors = [];
const caseIds = new Set();
const results = [];

function concat(parts) {
  const size = parts.reduce((total, part) => total + part.byteLength, 0);
  const output = new Uint8Array(size);
  let offset = 0;
  for (const part of parts) {
    output.set(part, offset);
    offset += part.byteLength;
  }
  return output;
}

function sha256(value) {
  return new Uint8Array(createHash("sha256").update(value).digest());
}

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

function resolveReference(reference, location) {
  if (!requireString(reference.path, `${location}.path`)) return;
  const resolved = path.resolve(toolingDirectory, reference.path);
  const relative = path.relative(wDirectory, resolved);
  if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
    errors.push(`${location}.path must stay inside the W repository.`);
    return;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${location}.path references a missing file.`);
    return;
  }
  if (
    requireString(reference.symbol, `${location}.symbol`) &&
    !fs.readFileSync(resolved, "utf8").includes(reference.symbol)
  ) {
    errors.push(`${location}.symbol is absent from ${reference.path}.`);
  }
}

function chunk(kind, label = `kind-${kind}`, options = {}) {
  return {
    kind,
    identity: options.identity ?? identityFor(`last-light.${label}`),
    critical: options.critical ?? true,
    schema: options.schema ?? [1, 0],
    digestAlgorithm: options.digestAlgorithm,
    digest: options.digest,
    declaredLength: options.declaredLength,
    omitFields: options.omitFields,
    encoded: options.encoded ?? false,
    value: options.value ?? new Map([
      [0, 1],
      [1, label],
    ]),
  };
}

function fixtureEntries(fixture) {
  if (fixture === "interface" || fixture === "interfaceOptional") {
    const entries = [
      chunk(1, "interface-index"),
      chunk(2, "semantic-interface"),
    ];
    if (fixture === "interfaceOptional") {
      entries.push(chunk(3, "documentation", { critical: false }));
    }
    return { profile: 1, entries };
  }
  if (fixture === "objectAbi") {
    return {
      profile: 2,
      entries: [
        chunk(16, "abi-note"),
        chunk(17, "representation-map"),
        chunk(18, "symbol-manifest"),
        chunk(19, "runtime-requirements"),
      ],
    };
  }
  throw new Error(`Unknown WMeta fixture ${fixture}.`);
}

function readU64(input, offset) {
  let value = 0n;
  for (let index = 0; index < 8; index += 1) {
    value = (value << 8n) | BigInt(input[offset + index]);
  }
  return Number(value);
}

function writeU16(input, offset, value) {
  input[offset] = (value >>> 8) & 0xff;
  input[offset + 1] = value & 0xff;
}

function writeU32(input, offset, value) {
  input[offset] = (value >>> 24) & 0xff;
  input[offset + 1] = (value >>> 16) & 0xff;
  input[offset + 2] = (value >>> 8) & 0xff;
  input[offset + 3] = value & 0xff;
}

function writeU64(input, offset, value) {
  let remaining = BigInt(value);
  for (let index = 7; index >= 0; index -= 1) {
    input[offset + index] = Number(remaining & 0xffn);
    remaining >>= 8n;
  }
}

function splitContainer(container) {
  const directoryLength = readU64(container, 16);
  const directoryEnd = 32 + directoryLength;
  return {
    directory: container.subarray(32, directoryEnd),
    payload: container.subarray(directoryEnd),
  };
}

function clone(value) {
  return new Uint8Array(value);
}

function rawEntry(entry) {
  const encodedChunk = entry.encoded ? entry.value : encodeCbor(entry.value);
  const digest = entry.digest ?? sha256(encodedChunk);
  const fields = [
    [0, entry.kind],
    [1, entry.identity],
    [2, entry.schema ?? [1, 0]],
    [3, entry.critical ?? true],
    [4, entry.declaredLength ?? encodedChunk.byteLength],
    [5, [entry.digestAlgorithm ?? 1, digest]],
  ];
  return {
    chunk: encodedChunk,
    map: new Map(fields.filter(([field]) => !(entry.omitFields ?? []).includes(field))),
  };
}

function buildRaw({
  profile,
  entries,
  features = [],
  rootOrder = [0, 1, 2, 3],
  duplicateRootSchema = false,
}) {
  const materialized = entries.map(rawEntry);
  const values = new Map([
    [0, 1],
    [1, profile],
    [2, features],
    [3, materialized.map((entry) => entry.map)],
  ]);
  const pairs = rootOrder.map((field) => [field, values.get(field)]);
  if (duplicateRootSchema) pairs.splice(1, 0, [0, 1]);
  const directory = encodeCborMapEntries(pairs, {
    sort: false,
    rejectDuplicates: false,
  });
  return assembleWMeta(directory, concat(materialized.map((entry) => entry.chunk)));
}

function replaceFirstCore(entries, value) {
  return entries.map((entry, index) => index === 0
    ? { ...entry, value, encoded: true }
    : entry);
}

function mutate(fixture, mutation) {
  const base = fixtureEntries(fixture);
  const standard = () => buildWMeta(base);

  switch (mutation) {
    case "none":
      return standard();
    case "badMagic": {
      const output = clone(standard());
      output[0] ^= 0xff;
      return output;
    }
    case "badHeaderSize": {
      const output = clone(standard());
      writeU16(output, 8, 31);
      return output;
    }
    case "badDirectorySchema": {
      const output = clone(standard());
      writeU16(output, 10, 2);
      return output;
    }
    case "headerFlags": {
      const output = clone(standard());
      writeU32(output, 12, 1);
      return output;
    }
    case "directoryLimit": {
      const output = clone(standard());
      writeU64(output, 16, 64n * 1024n * 1024n + 1n);
      return output;
    }
    case "payloadLimit": {
      const output = clone(standard());
      writeU64(output, 24, 16n * 1024n * 1024n * 1024n + 1n);
      return output;
    }
    case "trailingContainer":
      return concat([standard(), Uint8Array.of(0)]);
    case "nonCanonicalDirectoryInteger": {
      const { directory, payload } = splitContainer(standard());
      return assembleWMeta(
        concat([directory.subarray(0, 2), Uint8Array.of(0x18, 0x01), directory.subarray(3)]),
        payload,
      );
    }
    case "indefiniteDirectory": {
      const { directory, payload } = splitContainer(standard());
      const changed = clone(directory);
      changed[0] = 0xbf;
      return assembleWMeta(changed, payload);
    }
    case "unsortedDirectoryMap":
      return buildRaw({ ...base, rootOrder: [1, 0, 2, 3] });
    case "duplicateDirectoryKey":
      return buildRaw({ ...base, duplicateRootSchema: true });
    case "unknownProfile":
      return buildWMeta({ ...base, profile: 99 });
    case "unknownFeature":
      return buildWMeta({ ...base, requiredFeatures: [99] });
    case "duplicateFeature":
      return buildWMeta({ ...base, requiredFeatures: [1, 1], sortFeatures: false });
    case "unsortedFeatures":
      return buildWMeta({ ...base, requiredFeatures: [2, 1], sortFeatures: false });
    case "unsortedEntries":
      return buildWMeta({ ...base, entries: [...base.entries].reverse(), sortEntries: false });
    case "duplicateEntry":
      return buildWMeta({ ...base, entries: [base.entries[0], base.entries[0], ...base.entries.slice(1)] });
    case "unknownCriticalChunk":
      return buildWMeta({ ...base, entries: [...base.entries, chunk(500, "critical-extension")] });
    case "unknownOptionalChunk":
      return buildWMeta({ ...base, entries: [...base.entries, chunk(500, "optional-extension", { critical: false })] });
    case "missingCoreChunk":
      return buildWMeta({ ...base, entries: base.entries.slice(0, 1) });
    case "noncriticalCoreChunk":
      return buildWMeta({ ...base, entries: [{ ...base.entries[0], critical: false }, ...base.entries.slice(1)] });
    case "profileChunkMismatch":
      return buildWMeta({ ...base, entries: [...base.entries, chunk(16, "abi-note")] });
    case "criticalSchemaMajor":
      return buildWMeta({ ...base, entries: [{ ...base.entries[0], schema: [2, 0] }, ...base.entries.slice(1)] });
    case "criticalDigestAlgorithm":
      return buildWMeta({ ...base, entries: [{ ...base.entries[0], digestAlgorithm: 2 }, ...base.entries.slice(1)] });
    case "malformedDigest":
      return buildWMeta({ ...base, entries: [{ ...base.entries[0], digest: new Uint8Array(31) }, ...base.entries.slice(1)] });
    case "coreDigestMismatch": {
      const output = clone(standard());
      const directoryLength = readU64(output, 16);
      output[32 + directoryLength] ^= 0x01;
      return output;
    }
    case "optionalDigestMismatch": {
      const output = clone(standard());
      output[output.byteLength - 1] ^= 0x01;
      return output;
    }
    case "invalidIdentity":
      return buildWMeta({ ...base, entries: [{ ...base.entries[0], identity: new Uint8Array(31) }, ...base.entries.slice(1)] });
    case "zeroLengthChunk":
      return buildWMeta({ ...base, entries: [{ ...base.entries[0], value: new Uint8Array(), encoded: true }, ...base.entries.slice(1)] });
    case "payloadSumMismatch": {
      const first = base.entries[0];
      const length = encodeCbor(first.value).byteLength;
      return buildWMeta({ ...base, entries: [{ ...first, declaredLength: length + 1 }, ...base.entries.slice(1)] });
    }
    case "floatChunk":
      return buildWMeta({ ...base, entries: replaceFirstCore(base.entries, Uint8Array.of(0xf9, 0x3c, 0x00)) });
    case "tagChunk":
      return buildWMeta({ ...base, entries: replaceFirstCore(base.entries, Uint8Array.of(0xc0, 0x00)) });
    case "invalidUtf8Chunk":
      return buildWMeta({ ...base, entries: replaceFirstCore(base.entries, Uint8Array.of(0x61, 0xff)) });
    case "duplicateChunkMapKey":
      return buildWMeta({ ...base, entries: replaceFirstCore(base.entries, Uint8Array.of(0xa2, 0x00, 0x00, 0x00, 0x01)) });
    case "nestingLimit":
      return buildWMeta({ ...base, entries: replaceFirstCore(base.entries, concat([new Uint8Array(65).fill(0x81), Uint8Array.of(0x00)])) });
    case "trailingChunkCbor":
      return buildWMeta({ ...base, entries: replaceFirstCore(base.entries, Uint8Array.of(0xa0, 0x00)) });
    case "truncatedChunkCbor":
      return buildWMeta({ ...base, entries: replaceFirstCore(base.entries, Uint8Array.of(0x50)) });
    case "trailingDirectoryCbor": {
      const { directory, payload } = splitContainer(standard());
      return assembleWMeta(concat([directory, Uint8Array.of(0)]), payload);
    }
    default:
      throw new Error(`Unknown WMeta mutation ${mutation}.`);
  }
}

if (corpus.$schema !== "w-wmeta-cases-w0") errors.push("wmeta-cases.json has the wrong schema.");
if (corpus.status !== "design-oracle-input-w0") errors.push("wmeta-cases.json has the wrong status.");
if (corpus.machine !== "wmeta-reference-w0") errors.push("wmeta-cases.json has the wrong machine.");
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) errors.push("wmeta-cases.json must contain cases.");

for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`;
  if (!/^W0-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase.id ?? "")) {
    errors.push(`${location}.id must use W0-kebab-case.`);
  } else if (caseIds.has(testCase.id)) {
    errors.push(`${location}.id duplicates ${testCase.id}.`);
  } else {
    caseIds.add(testCase.id);
  }
  for (const field of ["fixture", "mutation", "mode"]) {
    requireString(testCase[field], `${location}.${field}`);
  }
  if (!Array.isArray(testCase.references) || testCase.references.length === 0) {
    errors.push(`${location}.references must link to Last Light.`);
  } else {
    testCase.references.forEach((reference, referenceIndex) =>
      resolveReference(reference, `${location}.references[${referenceIndex}]`),
    );
  }
  if (!Array.isArray(testCase.decisions) || testCase.decisions.length === 0) {
    errors.push(`${location}.decisions must not be empty.`);
  } else {
    for (const decision of testCase.decisions) {
      if (!designDecisionIds.has(decision)) errors.push(`${location} references missing ${decision}.`);
    }
  }
  if (!["accepted", "rejected"].includes(testCase.expected?.status)) {
    errors.push(`${location}.expected.status is invalid.`);
  }
  if (testCase.expected?.status === "rejected") {
    requireString(testCase.expected.code, `${location}.expected.code`);
  }

  let encoded;
  let actual;
  try {
    encoded = mutate(testCase.fixture, testCase.mutation);
    const summary = readWMeta(encoded, { mode: testCase.mode });
    actual = { status: "accepted", summary };
  } catch (error) {
    if (!(error instanceof WMetaError)) throw error;
    actual = { status: "rejected", code: error.code };
  }
  if (actual.status !== testCase.expected?.status || actual.code !== testCase.expected?.code) {
    errors.push(
      `${location} expected ${JSON.stringify(testCase.expected)}; ` +
      `actual is ${JSON.stringify({ status: actual.status, code: actual.code })}.`,
    );
  }
  results.push({
    caseId: testCase.id,
    mode: testCase.mode,
    status: actual.status,
    ...(actual.code ? { code: actual.code } : {}),
    hex: encoded ? Buffer.from(encoded).toString("hex") : "",
    ...(actual.summary ? { summary: actual.summary } : {}),
  });
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const snapshot = [
  JSON.stringify({ schema: "w-wmeta-results-w0", status: "design-oracle-output-w0" }),
  ...results.map((result) => JSON.stringify(result)),
].join("\n") + "\n";
const accepted = results.filter((result) => result.status === "accepted").length;
const summary = `WMeta W0: ${results.length} cases, ${accepted} accepted, ${results.length - accepted} rejected.`;

if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, snapshot);
  process.stdout.write(`${summary}\nUpdated ${path.basename(snapshotPath)}.\n`);
  process.exit(0);
}

if (!fs.existsSync(snapshotPath)) {
  process.stderr.write(`${path.basename(snapshotPath)} is missing; run with --write.\n`);
  process.exit(1);
}
if (fs.readFileSync(snapshotPath, "utf8") !== snapshot) {
  process.stderr.write(`${path.basename(snapshotPath)} is stale; run with --write.\n`);
  process.exit(1);
}

process.stdout.write(`${summary}\n`);
