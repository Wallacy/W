import crypto from "node:crypto";

export const AUTHORITY_ORIGIN_SCHEMA = "w-authority-origin-1";
export const AUTHORITY_EVIDENCE_SCHEMA = "w-authority-evidence-1";
export const AUTHORITY_RECORD_SCHEMA = "w-authority-record-1";
export const AUTHORITY_CHECKPOINT_SCHEMA = "w-authority-checkpoint-1";
export const REGISTRY_PROVIDER = "w.registry.tuf-lineage/1";

export const LIMITS = Object.freeze({
  maxUpdates: 8,
  maxKeys: 8,
  maxSignatures: 8,
  maxRootBytes: 8192,
  maxEvidenceBytes: 65536,
  maxCheckpointBytes: 32768,
  maxOriginBytes: 16384,
  maxLineageBytes: 128,
  maxProviderBytes: 128,
  maxKeyIdBytes: 71,
  maxLocatorBytes: 256,
  maxDisplayBytes: 256,
  maxMirrorCount: 4,
  maxMetadataEntries: 16,
  maxMetadataKeyBytes: 64,
  maxMetadataValueBytes: 256,
});

const textEncoder = new TextEncoder();
const SPKI_ED25519_PREFIX = Buffer.from("302a300506032b6570032100", "hex");
const RESULT_KEYS = new Set(["expected", "status", "result", "outcome"]);
const SHA256_KEY_ID_PATTERN = /^sha256:[0-9a-f]{64}$/u;
const LINEAGE_PATTERN = /^[a-z][a-z0-9-]{0,127}$/u;

export class AuthorityRegistryError extends Error {
  constructor(code, details = {}) {
    super(code);
    this.name = "AuthorityRegistryError";
    this.code = code;
    this.details = details;
  }
}

function fail(code, details = {}) {
  throw new AuthorityRegistryError(code, details);
}

function object(value, code) {
  if (!value || typeof value !== "object" || Array.isArray(value)) fail(code);
  return value;
}

function closed(value, allowed, code) {
  object(value, code);
  for (const key of Object.keys(value)) if (!allowed.has(key)) fail(code, { key });
  return value;
}

function safeInteger(value, minimum, maximum, code) {
  if (!Number.isSafeInteger(value) || value < minimum || value > maximum) fail(code);
  return value;
}

function u8(value) {
  safeInteger(value, 0, 0xff, "authorityEncodingOverflow");
  return Uint8Array.of(value);
}

function u32(value) {
  safeInteger(value, 0, 0xffffffff, "authorityEncodingOverflow");
  const result = new Uint8Array(4);
  new DataView(result.buffer).setUint32(0, value, false);
  return result;
}

function concat(...parts) {
  const arrays = parts.map((part) => part instanceof Uint8Array ? part : Uint8Array.from(part));
  const length = arrays.reduce((total, part) => total + part.length, 0);
  const result = new Uint8Array(length);
  let offset = 0;
  for (const part of arrays) {
    result.set(part, offset);
    offset += part.length;
  }
  return result;
}

function textBytes(value, maximum, code = "authorityTextInvalid") {
  if (typeof value !== "string") fail(code);
  for (let index = 0; index < value.length; index += 1) {
    const unit = value.charCodeAt(index);
    if (unit >= 0xd800 && unit <= 0xdbff) {
      const next = value.charCodeAt(index + 1);
      if (Number.isNaN(next) || next < 0xdc00 || next > 0xdfff) fail(code);
      index += 1;
    } else if (unit >= 0xdc00 && unit <= 0xdfff) {
      fail(code);
    }
  }
  const bytes = textEncoder.encode(value);
  if (bytes.length > maximum) fail("authorityLimitExceeded", { field: code, maximum });
  return bytes;
}

function lineageBytes(value) {
  const bytes = textBytes(value, LIMITS.maxLineageBytes, "authorityLineageInvalid");
  if (!LINEAGE_PATTERN.test(value)) fail("authorityLineageInvalid");
  return bytes;
}

function framedBytes(value, maximum, code = "authorityBytesInvalid") {
  const bytes = value instanceof Uint8Array ? value : Uint8Array.from(value ?? []);
  if (bytes.length > maximum) fail("authorityLimitExceeded", { field: code, maximum });
  return concat(u32(bytes.length), bytes);
}

function framedText(value, maximum, code = "authorityTextInvalid") {
  return framedBytes(textBytes(value, maximum, code), maximum, code);
}

function hexBytes(value, code = "authorityHexInvalid") {
  if (typeof value !== "string" || value.length % 2 !== 0 || !/^[0-9a-f]*$/u.test(value)) fail(code);
  return Uint8Array.from(Buffer.from(value, "hex"));
}

function cloneBytes(value) {
  return new Uint8Array(value);
}

function equalBytes(left, right) {
  return left.length === right.length && left.every((value, index) => value === right[index]);
}

function compareBytes(left, right) {
  const length = Math.min(left.length, right.length);
  for (let index = 0; index < length; index += 1) {
    if (left[index] !== right[index]) return left[index] - right[index];
  }
  return left.length - right.length;
}

function compareTextBytes(left, right) {
  return compareBytes(textEncoder.encode(left), textEncoder.encode(right));
}

export function sha256Bytes(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

function digestHex(value) {
  return sha256Bytes(value).slice("sha256:".length);
}

function algorithmCode(value) {
  if (value !== "ed25519") fail(value === undefined ? "authorityAlgorithmMissing" : "authorityAlgorithmUnsupported", { algorithm: value });
  return 1;
}

function authorityKeyIdPreimage(algorithm, publicKey) {
  return concat(
    textEncoder.encode("w-authority-key-1"),
    u8(algorithm),
    framedBytes(publicKey, 32, "authorityPublicKeyMalformed"),
  );
}

export function deriveAuthorityKeyId({ algorithm, publicKey }) {
  const algorithmNumber = algorithmCode(algorithm);
  const keyBytes = publicKey instanceof Uint8Array
    ? cloneBytes(publicKey)
    : hexBytes(publicKey, "authorityPublicKeyMalformed");
  if (keyBytes.length !== 32) fail("authorityPublicKeyMalformed");
  return `sha256:${digestHex(authorityKeyIdPreimage(algorithmNumber, keyBytes))}`;
}

function canonicalKey(value) {
  closed(value, new Set(["id", "algorithm", "publicKey"]), "authorityKeyInvalid");
  const algorithm = algorithmCode(value.algorithm);
  const publicKey = hexBytes(value.publicKey, "authorityPublicKeyMalformed");
  if (publicKey.length !== 32) fail("authorityPublicKeyMalformed");
  const derivedId = deriveAuthorityKeyId({ algorithm: value.algorithm, publicKey });
  if (value.id !== derivedId) fail("authorityKeyIdMismatch", { expected: derivedId, actual: value.id });
  const id = textBytes(derivedId, LIMITS.maxKeyIdBytes, "authorityKeyIdInvalid");
  return { idText: derivedId, id, algorithm, publicKey };
}

function canonicalSignature(value) {
  closed(value, new Set(["signer", "algorithm", "signature"]), "authoritySignatureInvalid");
  const signer = textBytes(value.signer, LIMITS.maxKeyIdBytes, "authoritySignerInvalid");
  if (!SHA256_KEY_ID_PATTERN.test(value.signer)) fail("authoritySignerInvalid");
  const algorithm = algorithmCode(value.algorithm);
  const signature = hexBytes(value.signature, "authoritySignatureMalformed");
  if (signature.length !== 64) fail("authoritySignatureMalformed");
  return { signerText: value.signer, signer, algorithm, signature };
}

function sortedKeys(root) {
  if (!Array.isArray(root.keys) || root.keys.length === 0 || root.keys.length > LIMITS.maxKeys)
    fail("authorityKeyCountExceeded", { count: root.keys?.length });
  const keys = root.keys.map(canonicalKey);
  if (new Set(keys.map((key) => key.idText)).size !== keys.length) fail("authorityDuplicateKey");
  return [...keys].sort((left, right) => compareBytes(left.id, right.id));
}

function sortedSignatures(root) {
  if (!Array.isArray(root.signatures) || root.signatures.length === 0 || root.signatures.length > LIMITS.maxSignatures)
    fail("authoritySignatureCountExceeded", { count: root.signatures?.length });
  const signatures = root.signatures.map(canonicalSignature);
  if (new Set(signatures.map((signature) => signature.signerText)).size !== signatures.length)
    fail("authorityDuplicateSigner");
  return [...signatures].sort((left, right) => compareBytes(left.signer, right.signer));
}

function rootPublicFields(root) {
  closed(root, new Set(["version", "lineage", "threshold", "keys", "signatures"]), "authorityRootInvalid");
  const version = safeInteger(root.version, 1, 0xffffffff, "authorityRootVersionInvalid");
  const lineage = lineageBytes(root.lineage);
  if (lineage.length === 0) fail("authorityLineageInvalid");
  const keys = sortedKeys(root);
  const threshold = safeInteger(root.threshold, 1, keys.length, "authorityThresholdInvalid");
  return {
    version,
    lineage,
    lineageText: root.lineage,
    threshold,
    keys,
  };
}

function encodeKey(key) {
  return concat(Uint8Array.of(0x4b), framedBytes(key.id, LIMITS.maxKeyIdBytes), u8(key.algorithm), framedBytes(key.publicKey, 32));
}

function encodePublicFields(root) {
  const fields = rootPublicFields(root);
  return concat(
    u8(0x52),
    framedText(fields.lineageText, LIMITS.maxLineageBytes, "authorityLineageInvalid"),
    u32(fields.version),
    u8(fields.threshold),
    u32(fields.keys.length),
    ...fields.keys.map(encodeKey),
  );
}

export function encodeRootPayload(root) {
  return concat(textEncoder.encode("w-authority-root-payload-1"), u8(0x50), encodePublicFields(root));
}

export function encodeRoot(root) {
  const signatures = sortedSignatures(root);
  return concat(
    textEncoder.encode("w-authority-root-1"),
    encodePublicFields(root),
    u32(signatures.length),
    ...signatures.map((signature) => concat(
      Uint8Array.of(0x53),
      framedText(signature.signerText, LIMITS.maxKeyIdBytes, "authoritySignerInvalid"),
      u8(signature.algorithm),
      framedBytes(signature.signature, 64, "authoritySignatureMalformed"),
    )),
  );
}

function publicKeyObject(raw) {
  return crypto.createPublicKey({
    key: Buffer.concat([SPKI_ED25519_PREFIX, Buffer.from(raw)]),
    format: "der",
    type: "spki",
  });
}

function verifySignature(payload, signature, key) {
  try {
    return crypto.verify(null, Buffer.from(payload), publicKeyObject(key.publicKey), Buffer.from(signature.signature));
  } catch {
    fail("authoritySignatureMalformed");
  }
}

function keyById(keys) {
  return new Map(keys.map((key) => [key.idText, key]));
}

function validSignerSet(payload, signatures, keys) {
  const byId = keyById(keys);
  const valid = new Set();
  for (const signature of signatures) {
    const key = byId.get(signature.signerText);
    if (!key || key.algorithm !== signature.algorithm || !verifySignature(payload, signature, key)) continue;
    valid.add(signature.signerText);
  }
  return valid;
}

function validateRoot(root, expectedVersion, previousRoot = null) {
  const fields = rootPublicFields(root);
  if (fields.version !== expectedVersion) {
    fail(fields.version < expectedVersion ? "authorityRootRollback" : "authorityRootGap", {
      expectedVersion,
      actualVersion: fields.version,
    });
  }
  const signatures = sortedSignatures(root);
  const payload = encodeRootPayload(root);
  if (payload.length > LIMITS.maxRootBytes) fail("authorityRootBytesExceeded");
  const currentValid = validSignerSet(payload, signatures, fields.keys);
  if (previousRoot === null) {
    if (currentValid.size < fields.threshold) fail("authorityGenesisThresholdInsufficient");
  } else {
    if (fields.lineageText !== previousRoot.lineageText) fail("authorityWrongLineage");
    const previousValid = validSignerSet(payload, signatures, previousRoot.keys);
    if (previousValid.size < previousRoot.threshold) fail("authorityOldThresholdInsufficient");
    if (currentValid.size < fields.threshold) fail("authorityNewThresholdInsufficient");
  }
  return { ...fields, signatures, payload, canonical: encodeRoot(root), digest: sha256Bytes(encodeRoot(root)) };
}

function validateManifest(manifest) {
  closed(manifest, new Set(["kind", "locator", "display"]), "authorityManifestInvalid");
  if (manifest.kind !== "registry") fail("authorityManifestKindRejected", { kind: manifest.kind });
  textBytes(manifest.locator, LIMITS.maxLocatorBytes, "authorityLocatorInvalid");
  textBytes(manifest.display, LIMITS.maxDisplayBytes, "authorityDisplayInvalid");
  return manifest;
}

function validateByteView(view, maximum, invalidCode, malformedCode) {
  closed(view, new Set(["bytes", "digest", "length"]), invalidCode);
  const bytes = hexBytes(view.bytes, malformedCode);
  safeInteger(view.length, 0, maximum, invalidCode);
  const mismatchPrefix = invalidCode.endsWith("Invalid")
    ? invalidCode.slice(0, -"Invalid".length)
    : invalidCode;
  if (view.length !== bytes.length) fail(`${mismatchPrefix}LengthMismatch`);
  if (view.digest !== sha256Bytes(bytes)) fail(`${mismatchPrefix}DigestMismatch`);
  return { bytes, digest: view.digest, length: view.length };
}

function validateTrustedGenesis(anchor) {
  return validateByteView(anchor, LIMITS.maxRootBytes,
    "authorityTrustedGenesisInvalid", "authorityTrustedGenesisMalformed");
}

function validateCheckpoint(checkpoint, trustedGenesis, origin, lineage) {
  closed(checkpoint, new Set(["root", "view"]), "authorityTrustedCheckpointInvalid");
  closed(checkpoint.root, new Set(["version", "lineage", "threshold", "keys", "signatures"]), "authorityTrustedCheckpointInvalid");
  const root = validateRoot(checkpoint.root, checkpoint.root.version, null);
  if (root.lineageText !== lineage) fail("authorityWrongLineage");
  const checkpointBytes = encodeAuthorityCheckpoint({ origin, rootPayload: root.payload });
  const checkpointView = validateByteView(checkpoint.view, LIMITS.maxCheckpointBytes,
    "authorityCheckpointInvalid", "authorityCheckpointMalformed");
  if (!equalBytes(checkpointBytes, checkpointView.bytes)) fail("authorityCheckpointBytesMismatch");
  if (root.version === 1 && !equalBytes(root.payload, trustedGenesis.bytes))
    fail("authorityCheckpointGenesisMismatch");
  return { root, view: checkpointView };
}

function validateContinuity(continuity, checkpoint, lineage) {
  closed(continuity, new Set(["checkpointVersion", "checkpointDigest", "updates", "mirrors", "transport", "alias", "metadata", "observedRootVersion", "observedRootDigest"]), "authorityContinuityInvalid");
  if (continuity.checkpointVersion !== checkpoint.root.version || continuity.checkpointDigest !== checkpoint.view.digest)
    fail("authorityCheckpointBindingMismatch");
  if (!Array.isArray(continuity.updates) || continuity.updates.length > LIMITS.maxUpdates)
    fail("authorityUpdateCountExceeded", { count: continuity.updates?.length });
  const updates = [];
  let previousRoot = checkpoint.root;
  for (let index = 0; index < continuity.updates.length; index += 1) {
    const root = validateRoot(continuity.updates[index], checkpoint.root.version + index + 1, previousRoot);
    if (root.lineageText !== lineage) fail("authorityWrongLineage");
    updates.push(root);
    previousRoot = root;
  }
  const observedRoot = updates.at(-1) ?? checkpoint.root;
  if (!Array.isArray(continuity.mirrors) || continuity.mirrors.length > LIMITS.maxMirrorCount)
    fail("authorityMirrorLimitExceeded");
  const mirrors = continuity.mirrors.map((mirror) => {
    closed(mirror, new Set(["locator", "rootDigest"]), "authorityMirrorInvalid");
    textBytes(mirror.locator, LIMITS.maxLocatorBytes, "authorityMirrorInvalid");
    if (typeof mirror.rootDigest !== "string" || !/^sha256:[0-9a-f]{64}$/u.test(mirror.rootDigest)) fail("authorityMirrorInvalid");
    return { locator: mirror.locator, rootDigest: mirror.rootDigest };
  });
  textBytes(continuity.transport, LIMITS.maxLocatorBytes, "authorityTransportInvalid");
  textBytes(continuity.alias, LIMITS.maxDisplayBytes, "authorityAliasInvalid");
  object(continuity.metadata, "authorityMetadataInvalid");
  const metadataEntries = Object.entries(continuity.metadata);
  if (metadataEntries.length > LIMITS.maxMetadataEntries) fail("authorityMetadataLimitExceeded");
  const metadata = {};
  for (const [key, value] of metadataEntries.sort(([left], [right]) => compareTextBytes(left, right))) {
    textBytes(key, LIMITS.maxMetadataKeyBytes, "authorityMetadataInvalid");
    textBytes(value, LIMITS.maxMetadataValueBytes, "authorityMetadataInvalid");
    metadata[key] = value;
  }
  if (continuity.observedRootVersion !== observedRoot.version || continuity.observedRootDigest !== observedRoot.digest)
    fail("authorityObservedRootMismatch");
  return {
    checkpointVersion: continuity.checkpointVersion,
    checkpointDigest: continuity.checkpointDigest,
    updates,
    mirrors,
    transport: continuity.transport,
    alias: continuity.alias,
    metadata,
    observedRootVersion: continuity.observedRootVersion,
    observedRootDigest: continuity.observedRootDigest,
    observedRoot,
  };
}

function encodeMetadata(metadata) {
  const entries = Object.entries(metadata).sort(([left], [right]) => compareTextBytes(left, right));
  return concat(u32(entries.length), ...entries.map(([key, value]) => concat(
    framedText(key, LIMITS.maxMetadataKeyBytes, "authorityMetadataInvalid"),
    framedText(value, LIMITS.maxMetadataValueBytes, "authorityMetadataInvalid"),
  )));
}

export function encodeAuthorityOrigin({ provider, lineage, genesisPayload }) {
  const providerBytes = textBytes(provider, LIMITS.maxProviderBytes, "authorityProviderInvalid");
  const lineageValueBytes = lineageBytes(lineage);
  if (!(genesisPayload instanceof Uint8Array)) fail("authorityGenesisInvalid");
  if (genesisPayload.length > LIMITS.maxRootBytes) fail("authorityRootBytesExceeded");
  const origin = concat(
    textEncoder.encode(AUTHORITY_ORIGIN_SCHEMA),
    u8(0x41),
    u8(1),
    u8(0x50),
    framedBytes(providerBytes, LIMITS.maxProviderBytes, "authorityProviderInvalid"),
    u8(0x4c),
    framedBytes(lineageValueBytes, LIMITS.maxLineageBytes, "authorityLineageInvalid"),
    u8(0x47),
    framedBytes(genesisPayload, LIMITS.maxRootBytes, "authorityGenesisInvalid"),
  );
  if (origin.length > LIMITS.maxOriginBytes) fail("authorityOriginBytesExceeded");
  return origin;
}

export function encodeAuthorityCheckpoint({ origin, rootPayload }) {
  if (!(origin instanceof Uint8Array) || !(rootPayload instanceof Uint8Array)) fail("authorityCheckpointMalformed");
  const result = concat(
    textEncoder.encode(AUTHORITY_CHECKPOINT_SCHEMA),
    u8(0x43),
    u8(1),
    framedBytes(origin, LIMITS.maxOriginBytes, "authorityCheckpointOriginInvalid"),
    framedBytes(rootPayload, LIMITS.maxRootBytes, "authorityCheckpointPayloadInvalid"),
  );
  if (result.length > LIMITS.maxCheckpointBytes) fail("authorityCheckpointBytesExceeded");
  return result;
}

export function encodeAuthorityEvidence({ provider, lineage, manifest, checkpoint, continuity }) {
  const encodedRoots = [checkpoint.root.canonical, ...continuity.updates.map((root) => root.canonical)];
  const result = concat(
    textEncoder.encode(AUTHORITY_EVIDENCE_SCHEMA),
    u8(0x45),
    u8(1),
    framedText(provider, LIMITS.maxProviderBytes, "authorityProviderInvalid"),
    framedText(lineage, LIMITS.maxLineageBytes, "authorityLineageInvalid"),
    u8(0x4b),
    u32(encodedRoots.length),
    ...encodedRoots.map((root) => framedBytes(root, LIMITS.maxRootBytes, "authorityRootBytesExceeded")),
    u8(0x50),
    framedBytes(checkpoint.view.bytes, LIMITS.maxCheckpointBytes, "authorityCheckpointInvalid"),
    u8(0x43),
    u32(continuity.checkpointVersion),
    framedText(continuity.checkpointDigest, 71, "authorityCheckpointPayloadInvalid"),
    u8(0x4d),
    framedText(manifest.kind, 32, "authorityManifestInvalid"),
    framedText(manifest.locator, LIMITS.maxLocatorBytes, "authorityLocatorInvalid"),
    framedText(manifest.display, LIMITS.maxDisplayBytes, "authorityDisplayInvalid"),
    u8(0x4f),
    u32(continuity.observedRootVersion),
    framedText(continuity.observedRootDigest, 71, "authorityObservedRootInvalid"),
    framedText(continuity.transport, LIMITS.maxLocatorBytes, "authorityTransportInvalid"),
    framedText(continuity.alias, LIMITS.maxDisplayBytes, "authorityAliasInvalid"),
    u8(0x4d),
    u32(continuity.mirrors.length),
    ...continuity.mirrors.map((mirror) => concat(
      framedText(mirror.locator, LIMITS.maxLocatorBytes, "authorityMirrorInvalid"),
      textEncoder.encode(mirror.rootDigest),
    )),
    u8(0x4f),
    encodeMetadata(continuity.metadata),
  );
  if (result.length > LIMITS.maxEvidenceBytes) fail("authorityEvidenceBytesExceeded");
  return result;
}

export function encodeAuthorityRecord({ kind, locator, display, origin, evidence }) {
  return concat(
    textEncoder.encode(AUTHORITY_RECORD_SCHEMA),
    u8(0x52),
    framedText(kind, 32, "authorityManifestInvalid"),
    framedText(locator, LIMITS.maxLocatorBytes, "authorityLocatorInvalid"),
    framedText(display, LIMITS.maxDisplayBytes, "authorityDisplayInvalid"),
    u8(0x4f),
    framedBytes(origin, LIMITS.maxOriginBytes, "authorityOriginBytesExceeded"),
    u8(0x45),
    framedBytes(evidence, LIMITS.maxEvidenceBytes, "authorityEvidenceBytesExceeded"),
  );
}

function structuredRoot(root) {
  return {
    version: root.version,
    lineage: root.lineageText,
    threshold: root.threshold,
    keys: root.keys.map((key) => ({
      id: key.idText,
      algorithm: "ed25519",
      publicKey: Buffer.from(key.publicKey).toString("hex"),
    })),
    signatures: root.signatures.map((signature) => ({
      signer: signature.signerText,
      algorithm: "ed25519",
      signature: Buffer.from(signature.signature).toString("hex"),
    })),
  };
}

function checkpointView(root, origin) {
  const bytes = encodeAuthorityCheckpoint({ origin, rootPayload: root.payload });
  return {
    root: structuredRoot(root),
    view: { bytes, digest: sha256Bytes(bytes), length: bytes.length },
  };
}

function externalCheckpointView(checkpoint) {
  return {
    root: checkpoint.root,
    view: {
      bytes: Buffer.from(checkpoint.view.bytes).toString("hex"),
      digest: checkpoint.view.digest,
      length: checkpoint.view.length,
    },
  };
}

export function verifyRegistry(input) {
  try {
    closed(input, new Set(["manifest", "provider", "lineage", "continuity", "trustedGenesis", "trustedCheckpoint"]), "authorityRegistryInputInvalid");
    const manifest = validateManifest(input.manifest);
    if (input.provider !== REGISTRY_PROVIDER) fail("authorityProviderUnsupported", { provider: input.provider });
    textBytes(input.provider, LIMITS.maxProviderBytes, "authorityProviderInvalid");
    lineageBytes(input.lineage);
    const trustedGenesis = validateTrustedGenesis(input.trustedGenesis);
    const origin = encodeAuthorityOrigin({ provider: input.provider, lineage: input.lineage, genesisPayload: trustedGenesis.bytes });
    const checkpoint = validateCheckpoint(input.trustedCheckpoint, trustedGenesis, origin, input.lineage);
    const continuity = validateContinuity(input.continuity, checkpoint, input.lineage);
    const receipt = encodeAuthorityEvidence({ provider: input.provider, lineage: input.lineage, manifest, checkpoint, continuity });
    const record = encodeAuthorityRecord({
      kind: manifest.kind,
      locator: manifest.locator,
      display: manifest.display,
      origin,
      evidence: receipt,
    });
    const originDigest = sha256Bytes(origin);
    const receiptDigest = sha256Bytes(receipt);
    const nextCheckpoint = externalCheckpointView(checkpointView(continuity.observedRoot, origin));
    return {
      status: "accepted",
      route: "authorityLineageVerified",
      code: "authorityLineageVerified",
      kind: manifest.kind,
      locator: manifest.locator,
      display: manifest.display,
      provider: input.provider,
      lineage: input.lineage,
      origin,
      originDigest,
      originLength: origin.length,
      receipt,
      receiptDigest,
      receiptLength: receipt.length,
      record,
      recordDigest: sha256Bytes(record),
      nextCheckpoint,
      continuity: {
        ...continuity,
        rootDigests: [checkpoint.root.digest, ...continuity.updates.map((root) => root.digest)],
      },
    };
  } catch (error) {
    if (error instanceof AuthorityRegistryError) {
      return { status: "rejected", route: "rejected", code: error.code, details: error.details };
    }
    throw error;
  }
}

export function packageIdentity(origin, packageName) {
  if (!(origin instanceof Uint8Array) || origin.length > LIMITS.maxOriginBytes)
    fail("authorityOriginBytesExceeded");
  const packageBytes = textBytes(packageName, 127, "packageNameInvalid");
  if (!/^[a-z][a-z0-9-]{0,62}\/[a-z][a-z0-9-]{0,62}$/u.test(packageName))
    fail("packageNameInvalid");
  const result = concat(
    textEncoder.encode("w-package-identity-1"),
    u8(0x50),
    framedBytes(origin, LIMITS.maxOriginBytes, "authorityOriginBytesExceeded"),
    framedBytes(packageBytes, 127, "packageNameInvalid"),
  );
  return { bytes: result, digest: sha256Bytes(result), length: result.length };
}

export function authorityOriginEqual(left, right) {
  return Boolean(left && right && equalBytes(left, right));
}

function rejectResultEcho(value, path = "case") {
  if (!value || typeof value !== "object") return;
  if (Array.isArray(value)) {
    value.forEach((entry, index) => rejectResultEcho(entry, `${path}[${index}]`));
    return;
  }
  for (const [key, child] of Object.entries(value)) {
    if (RESULT_KEYS.has(key)) fail("callerResultEcho", { path: `${path}.${key}` });
    rejectResultEcho(child, `${path}.${key}`);
  }
}

function deepClone(value) {
  return structuredClone(value);
}

function setPath(target, path, value) {
  const parts = path.split(".");
  let cursor = target;
  for (const part of parts.slice(0, -1)) cursor = cursor[part];
  cursor[parts.at(-1)] = value;
}

function mutateFixture(base, scenario) {
  const fixture = deepClone(base);
  switch (scenario) {
    case "genesis":
      fixture.roots = fixture.roots.slice(0, 1);
      return fixture;
    case "rotation-valid":
      return fixture;
    case "genesis-signatures-different":
      fixture.roots[0].signatures = fixture.roots[0].signatures.filter((signature) =>
        [fixture.roots[0].keys[1].id, fixture.roots[0].keys[2].id].includes(signature.signer));
      return fixture;
    case "alias-change":
      fixture.manifest.locator = "w-mirror-alias";
      fixture.manifest.display = "W mirror alias";
      fixture.continuity.alias = "w-mirror-alias";
      return fixture;
    case "mirror-change":
      fixture.continuity.mirrors = [{
        locator: "https://mirror.example.test/w",
        rootDigest: sha256Bytes(encodeRoot(fixture.roots.at(-1))),
      }];
      return fixture;
    case "checkpoint-v2-stable":
    case "current-root-change":
      fixture.checkpointVersion = 2;
      return fixture;
    case "checkpoint-v2-replay-v1":
      fixture.checkpointVersion = 2;
      fixture.replayUpdates = [fixture.roots[0]];
      return fixture;
    case "checkpoint-v2-replay-v2":
      fixture.checkpointVersion = 2;
      fixture.replayUpdates = [fixture.roots[1]];
      return fixture;
    case "checkpoint-corrupt-bytes":
    case "checkpoint-corrupt-digest":
    case "checkpoint-corrupt-length":
    case "checkpoint-corrupt-origin":
      fixture.checkpointCorruption = scenario.slice("checkpoint-corrupt-".length);
      return fixture;
    case "checkpoint-crosswire-root":
    case "checkpoint-crosswire-origin":
      fixture.checkpointCorruption = scenario.slice("checkpoint-".length);
      return fixture;
    case "genesis-different":
      fixture.lineage = "w-lineage-different-1";
      fixture.roots = fixture.roots.map((root) => ({ ...root, lineage: fixture.lineage }));
      return fixture;
    case "insufficient-old":
      fixture.roots[1].signatures = fixture.roots[1].signatures.filter((signature) =>
        signature.signer === fixture.roots[1].keys[1].id);
      return fixture;
    case "insufficient-new":
      fixture.roots[1].signatures = fixture.roots[1].signatures.filter((signature) =>
        [fixture.roots[0].keys[0].id, fixture.roots[0].keys[1].id].includes(signature.signer));
      return fixture;
    case "duplicate-signer":
      fixture.roots[1].signatures.push(fixture.roots[1].signatures[0]);
      return fixture;
    case "rollback":
      fixture.roots[1].version = 1;
      return fixture;
    case "gap":
      fixture.roots[1].version = 3;
      return fixture;
    case "wrong-lineage":
      fixture.roots[1].lineage = "w-other-lineage";
      return fixture;
    case "tamper":
      for (const index of [0, 1]) fixture.roots[1].signatures[index].signature = "00".repeat(64);
      return fixture;
    case "malformed-key":
      fixture.roots[0].keys[0].publicKey = "00";
      return fixture;
    case "malformed-signature":
      fixture.roots[0].signatures[0].signature = "00";
      return fixture;
    case "key-id-mismatch":
      fixture.roots[0].keys[0].id = `sha256:${"00".repeat(32)}`;
      return fixture;
    case "over-limit-keys":
      fixture.roots[0].keys = Array.from({ length: LIMITS.maxKeys + 1 }, (_, index) => ({
        ...fixture.roots[0].keys[index % fixture.roots[0].keys.length],
        id: `sha256:${index.toString(16).padStart(64, "0")}`,
      }));
      return fixture;
    case "over-limit-signatures":
      fixture.roots[0].signatures = Array.from({ length: LIMITS.maxSignatures + 1 }, (_, index) => ({
        ...fixture.roots[0].signatures[0],
        signer: `sha256:${index.toString(16).padStart(64, "0")}`,
      }));
      return fixture;
    case "unsupported-algorithm":
      fixture.roots[0].keys[0].algorithm = "rsa";
      return fixture;
    case "over-limit":
      fixture.replayUpdates = Array.from({ length: LIMITS.maxUpdates + 1 }, (_, index) => ({
        ...fixture.roots[1], version: index + 2,
      }));
      return fixture;
    case "local-release":
      fixture.manifest.kind = "local";
      return fixture;
    case "git-snapshot":
      fixture.manifest.kind = "git";
      return fixture;
    default:
      if (scenario && scenario.path) {
        setPath(fixture, scenario.path, scenario.value);
        return fixture;
      }
      fail("authorityScenarioUnknown", { scenario });
  }
}

function externalByteView(bytes) {
  return { bytes: Buffer.from(bytes).toString("hex"), digest: sha256Bytes(bytes), length: bytes.length };
}

function fixtureToRegistryInput(fixture, trustedSource = fixture) {
  const trustedGenesis = fixture.trustedGenesis ?? trustedSource.trustedGenesis;
  const origin = encodeAuthorityOrigin({
    provider: fixture.provider,
    lineage: fixture.lineage,
    genesisPayload: hexBytes(trustedGenesis.bytes, "authorityTrustedGenesisMalformed"),
  });
  const checkpointIndex = fixture.checkpointVersion === 2 ? 1 : 0;
  const checkpointTrustRootSource = trustedSource.roots[checkpointIndex];
  const checkpointRoot = validateRoot(checkpointTrustRootSource, checkpointTrustRootSource.version, null);
  const checkpoint = checkpointView(checkpointRoot, origin);
  const updates = fixture.replayUpdates ?? fixture.roots.slice(checkpointIndex + 1);
  const observedRoot = updates.at(-1) ?? fixture.roots[checkpointIndex];
  const continuitySource = fixture.continuity;
  const continuityRest = { ...continuitySource };
  const input = {
    manifest: fixture.manifest,
    provider: fixture.provider,
    lineage: fixture.lineage,
    trustedGenesis,
    trustedCheckpoint: {
      root: fixture.roots[checkpointIndex],
      view: externalByteView(checkpoint.view.bytes),
    },
    continuity: {
      ...continuityRest,
      checkpointVersion: checkpointRoot.version,
      checkpointDigest: checkpoint.view.digest,
      updates,
      observedRootVersion: observedRoot.version,
      observedRootDigest: (() => {
        try { return sha256Bytes(encodeRoot(observedRoot)); } catch { return "sha256:" + "00".repeat(32); }
      })(),
    },
  };
  if (fixture.checkpointCorruption === "bytes") {
    const firstByte = Number.parseInt(input.trustedCheckpoint.view.bytes.slice(0, 2), 16) ^ 1;
    input.trustedCheckpoint.view.bytes = `${firstByte.toString(16).padStart(2, "0")}${input.trustedCheckpoint.view.bytes.slice(2)}`;
  } else if (fixture.checkpointCorruption === "digest") {
    input.trustedCheckpoint.view.digest = `sha256:${"00".repeat(32)}`;
  } else if (fixture.checkpointCorruption === "length") {
    input.trustedCheckpoint.view.length += 1;
  } else if (fixture.checkpointCorruption === "origin") {
    const originBytes = Uint8Array.from(Buffer.from(input.trustedCheckpoint.view.bytes, "hex"));
    originBytes[originBytes.length - 1] ^= 1;
    input.trustedCheckpoint.view.bytes = Buffer.from(originBytes).toString("hex");
    input.trustedCheckpoint.view.digest = sha256Bytes(originBytes);
  } else if (fixture.checkpointCorruption === "crosswire-root") {
    const alternateRootSource = trustedSource.roots[1];
    const alternateRoot = validateRoot(alternateRootSource, alternateRootSource.version, null);
    const crosswiredBytes = encodeAuthorityCheckpoint({ origin, rootPayload: alternateRoot.payload });
    input.trustedCheckpoint.view = externalByteView(crosswiredBytes);
  } else if (fixture.checkpointCorruption === "crosswire-origin") {
    const crosswiredOrigin = encodeAuthorityOrigin({
      provider: fixture.provider,
      lineage: "w-crosswire-origin-1",
      genesisPayload: hexBytes(trustedGenesis.bytes, "authorityTrustedGenesisMalformed"),
    });
    const checkpointRoot = validateRoot(trustedSource.roots[checkpointIndex], trustedSource.roots[checkpointIndex].version, null);
    const crosswiredBytes = encodeAuthorityCheckpoint({ origin: crosswiredOrigin, rootPayload: checkpointRoot.payload });
    input.trustedCheckpoint.view = externalByteView(crosswiredBytes);
  }
  return input;
}

export function registryFixtureInput(fixture) {
  return fixtureToRegistryInput(deepClone(fixture), deepClone(fixture));
}

export function deriveAuthorityCase(testCase, corpus) {
  if (!testCase || typeof testCase.id !== "string" || typeof testCase.fixture !== "string")
    throw new Error("AUL0 case requires id and fixture");
  rejectResultEcho(testCase);
  const base = corpus.fixtures?.[testCase.fixture];
  if (!base) throw new Error(`AUL0 fixture ${testCase.fixture} is missing`);
  const scenario = testCase.scenario ?? "rotation-valid";
  const trustedSource = deepClone(base);
  const fixture = mutateFixture(base, scenario);
  if (testCase.trustFixture !== undefined) {
    const trustedFixture = corpus.fixtures?.[testCase.trustFixture];
    if (!trustedFixture?.trustedGenesis) throw new Error(`AUL0 trust fixture ${testCase.trustFixture} is missing`);
    fixture.trustedGenesis = deepClone(trustedFixture.trustedGenesis);
  }
  const result = verifyRegistry(fixtureToRegistryInput(fixture, trustedSource));
  return {
    caseId: testCase.id,
    scenario,
    status: result.status,
    route: result.route,
    code: result.code,
    details: result.details ?? {},
    originDigest: result.originDigest ?? null,
    originLength: result.originLength ?? 0,
    receiptDigest: result.receiptDigest ?? null,
    receiptLength: result.receiptLength ?? 0,
    recordDigest: result.recordDigest ?? null,
    result,
  };
}

export function deriveAuthorityRegistry(corpus) {
  if (!corpus || corpus.$schema !== "w-authority-registry-cases-1" || corpus.status !== "design-oracle-input")
    throw new Error("invalid AUL0 corpus");
  if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) throw new Error("AUL0 corpus has no cases");
  return corpus.cases.map((testCase) => deriveAuthorityCase(testCase, corpus));
}

export function validateAuthorityRegistryCorpus(corpus) {
  const errors = [];
  try {
    const results = deriveAuthorityRegistry(corpus);
    if (new Set(results.map((entry) => entry.caseId)).size !== results.length) errors.push("duplicate case IDs");
    if (!results.some((entry) => entry.code === "authorityLineageVerified")) errors.push("no accepted registry case");
    return { errors, results };
  } catch (error) {
    return { errors: [error.message], results: [] };
  }
}

export function buildAuthoritySnapshot(corpus, corpusDigest) {
  const validation = validateAuthorityRegistryCorpus(corpus);
  if (validation.errors.length > 0) fail("authorityCorpusInvalid", { errors: validation.errors });
  const results = validation.results.map(({ result: _result, ...entry }) => entry);
  const statusCounts = Object.fromEntries(
    [...new Set(results.map((entry) => entry.status))].sort().map((status) => [
      status,
      results.filter((entry) => entry.status === status).length,
    ]),
  );
  const codeCounts = Object.fromEntries(
    [...new Set(results.map((entry) => entry.code))].sort().map((code) => [
      code,
      results.filter((entry) => entry.code === code).length,
    ]),
  );
  return {
    schema: "w-authority-registry-results-1",
    status: "design-oracle-output",
    corpus: "tooling/authority-registry-cases.json",
    corpusDigest,
    metrics: {
      caseCount: results.length,
      statusCounts,
      codeCounts,
      acceptedCount: results.filter((entry) => entry.status === "accepted").length,
      rejectedCount: results.filter((entry) => entry.status === "rejected").length,
    },
    results,
  };
}
