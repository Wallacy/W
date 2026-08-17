import crypto from "node:crypto";
import fs from "node:fs";

export const WLO1_SCHEMA = "w-wlo1-closure-case-corpus-1";
export const WLO1_CODEC_SCHEMA = "wlo.string.v1";
export const WLO1_VERSION = 1;
export const WLO1_RECEIPT_FIELDS = Object.freeze(["schemaId", "version"]);
export const WLO1_LIMITS = Object.freeze({
  maxEncodedBytes: 64,
  maxStringBytes: 64,
  maxDepth: 1,
  maxFields: 0,
});
export const WLO1_FORBIDDEN = new Set(["pointer", "owner", "capability", "drop"]);

function isByte(value) {
  return Number.isInteger(value) && value >= 0 && value <= 255;
}

function assertUtf8ScalarString(value) {
  if (typeof value !== "string") throw new TypeError("input must be a String");
  for (let index = 0; index < value.length; index += 1) {
    const code = value.charCodeAt(index);
    if (code >= 0xd800 && code <= 0xdbff) {
      const next = value.charCodeAt(index + 1);
      if (next < 0xdc00 || next > 0xdfff) throw new Error("invalidUtf8");
      index += 1;
    } else if (code >= 0xdc00 && code <= 0xdfff) {
      throw new Error("invalidUtf8");
    }
  }
}

export function encodeString(value) {
  assertUtf8ScalarString(value);
  const payload = [...new TextEncoder().encode(value)];
  if (payload.length < 24) return [0x60 + payload.length, ...payload];
  if (payload.length <= 0xff) return [0x78, payload.length, ...payload];
  if (payload.length <= 0xffff) return [0x79, payload.length >> 8, payload.length & 0xff, ...payload];
  throw new Error("payloadLimit");
}

function parseLength(bytes, offset, additionalInformation) {
  if (additionalInformation < 24) return { length: additionalInformation, next: offset };
  if (additionalInformation === 24) {
    if (offset >= bytes.length) throw new Error("truncatedLength");
    const length = bytes[offset];
    if (length < 24) throw new Error("nonCanonicalLength");
    return { length, next: offset + 1 };
  }
  if (additionalInformation === 25) {
    if (offset + 2 > bytes.length) throw new Error("truncatedLength");
    const length = (bytes[offset] << 8) | bytes[offset + 1];
    if (length <= 0xff) throw new Error("nonCanonicalLength");
    return { length, next: offset + 2 };
  }
  if (additionalInformation === 26) {
    if (offset + 4 > bytes.length) throw new Error("truncatedLength");
    const length =
      bytes[offset] * 0x1000000 +
      bytes[offset + 1] * 0x10000 +
      bytes[offset + 2] * 0x100 +
      bytes[offset + 3];
    if (length <= 0xffff) throw new Error("nonCanonicalLength");
    return { length, next: offset + 4 };
  }
  throw new Error(additionalInformation === 31 ? "indefiniteLength" : "lengthWidthUnsupported");
}

export function decodeString(inputBytes, limits = WLO1_LIMITS) {
  if (!Array.isArray(inputBytes) || inputBytes.some((byte) => !isByte(byte))) {
    throw new Error("invalidBytes");
  }
  if (inputBytes.length > limits.maxEncodedBytes) throw new Error("payloadLimit");
  if (inputBytes.length === 0) throw new Error("truncatedHeader");
  const first = inputBytes[0];
  const majorType = first >> 5;
  const additionalInformation = first & 0x1f;
  if (majorType !== 3) throw new Error("invalidMajorType");
  const parsed = parseLength(inputBytes, 1, additionalInformation);
  if (parsed.length > limits.maxStringBytes) throw new Error("payloadLimit");
  if (parsed.next + parsed.length !== inputBytes.length) {
    if (parsed.next + parsed.length < inputBytes.length) throw new Error("trailingBytes");
    throw new Error("truncatedPayload");
  }
  const payload = Uint8Array.from(inputBytes.slice(parsed.next));
  let value;
  try {
    value = new TextDecoder("utf-8", { fatal: true }).decode(payload);
  } catch {
    throw new Error("invalidUtf8");
  }
  const canonical = encodeString(value);
  if (canonical.length !== inputBytes.length || canonical.some((byte, index) => byte !== inputBytes[index])) {
    throw new Error("nonCanonicalValue");
  }
  return { value, encodedBytes: inputBytes.length, depth: 1, fields: 0 };
}

function bytesFromHex(hex) {
  if (typeof hex !== "string" || hex.length % 2 !== 0 || !/^[0-9a-f]*$/.test(hex)) {
    throw new Error("invalidHex");
  }
  return [...hex.matchAll(/../g)].map((match) => Number.parseInt(match[0], 16));
}

function stableDigest(value) {
  const normalized = JSON.stringify(value, Object.keys(value).sort());
  return `sha256:${crypto.createHash("sha256").update(normalized).digest("hex")}`;
}

export function evaluateWloCase(testCase, corpus) {
  const common = {
    caseId: testCase.id,
    kind: testCase.kind,
    sourceInput: corpus.source.inputUtf8,
    sourceResult: corpus.source.resultUtf8,
    codec: corpus.codec.encoding,
  };
  try {
    if (testCase.kind === "roundtrip") {
      assertUtf8ScalarString(testCase.inputUtf8);
      const bytes = encodeString(testCase.inputUtf8);
      const decoded = decodeString(bytes);
      return {
        ...common,
        status: "accepted",
        value: decoded.value,
        bytesHex: Buffer.from(bytes).toString("hex"),
        roundtrip: decoded.value === testCase.inputUtf8,
        targets: [...testCase.targets],
      };
    }
    if (testCase.kind === "target-parity") {
      const bytes = encodeString(testCase.inputUtf8);
      const projections = testCase.targets.map((target) => ({
        target,
        bytesHex: Buffer.from(bytes).toString("hex"),
        layoutAuthority: "logical-bytes-only",
      }));
      const equal = projections.every((projection) => projection.bytesHex === projections[0].bytesHex);
      if (!equal) throw new Error("targetProjectionDivergence");
      return {
        ...common,
        status: "accepted",
        value: testCase.inputUtf8,
        bytesHex: projections[0].bytesHex,
        projections,
        logicalResultEqual: true,
      };
    }
    if (testCase.kind === "decode") {
      const bytes = bytesFromHex(testCase.bytesHex);
      const decoded = decodeString(bytes, corpus.limits);
      return { ...common, status: "accepted", value: decoded.value, bytesHex: testCase.bytesHex };
    }
    if (testCase.kind === "profile-receipt") {
      if (testCase.version !== corpus.codec.version) throw new Error("versionMismatch");
      if (testCase.schemaId !== corpus.codec.schemaId) throw new Error("schemaMismatch");
      return { ...common, status: "accepted", profileReceipt: "schema-match" };
    }
    if (testCase.kind === "profile-receipt-fields") {
      const fields = testCase.fields ?? [];
      if (new Set(fields).size !== fields.length) throw new Error("duplicateField");
      if (fields.some((field) => !WLO1_RECEIPT_FIELDS.includes(field))) throw new Error("unknownField");
      if (new Set(fields).size !== new Set(WLO1_RECEIPT_FIELDS).size) throw new Error("missingField");
      return { ...common, status: "accepted", profileReceipt: "required-fields" };
    }
    if (testCase.kind === "representation") {
      const values = new Set(testCase.representation ?? []);
      if (values.has("editorTree")) throw new Error("editorTreeNotAuthority");
      if ([...WLO1_FORBIDDEN].some((value) => values.has(value))) throw new Error("forbiddenRepresentation");
      return { ...common, status: "accepted", representation: "data-only" };
    }
    throw new Error("unknownCaseKind");
  } catch (error) {
    return {
      ...common,
      status: "rejected",
      error: error instanceof Error ? error.message : String(error),
      preflight: ["schema", "version", "limits", "canonical-bytes"].slice(0, testCase.kind === "decode" ? 3 : 2),
    };
  }
}

export function validateWloCorpus(corpus) {
  const errors = [];
  if (corpus?.$schema !== WLO1_SCHEMA) errors.push("WLO1 corpus schema mismatch.");
  if (corpus?.status !== "design-oracle-input") errors.push("WLO1 corpus must remain design-oracle-input.");
  if (corpus?.id !== "WLO1") errors.push("WLO1 corpus id mismatch.");
  if (corpus?.codec?.encoding !== "deterministic-cbor" || corpus.codec.rfc !== "8949") {
    errors.push("WLO1 must use deterministic CBOR RFC 8949.");
  }
  if (corpus?.codec?.profileFamily !== "schema-profiled-data-codec" || corpus.codec.automaticUniversalEncoding !== false) {
    errors.push("WLO1 must remain an explicitly profiled, non-universal codec.");
  }
  if (corpus?.codec?.wAbiImpact !== "none" || corpus.codec.targetWAbiIndependent !== true) {
    errors.push("WLO1 must not claim W ABI authority.");
  }
  if (corpus?.codec?.schemaId !== WLO1_CODEC_SCHEMA || corpus.codec.version !== WLO1_VERSION) {
    errors.push("WLO1 codec schema/version mismatch.");
  }
  const sourceValue = corpus?.source?.inputUtf8;
  if (sourceValue !== corpus?.source?.resultUtf8 || sourceValue !== "Last Light") {
    errors.push("WLO1 must use the same Last Light input and logical result.");
  }
  const canonical = Buffer.from(encodeString(sourceValue)).toString("hex");
  if (corpus.codec.canonicalBytes !== canonical) errors.push("WLO1 canonical bytes are stale.");
  for (const key of Object.keys(WLO1_LIMITS)) {
    if (corpus.limits?.[key] !== WLO1_LIMITS[key]) errors.push(`WLO1 limit ${key} is not the bounded contract.`);
  }
  const targets = corpus.targets ?? [];
  if (targets.length !== 2 || new Set(targets.map((target) => target.id)).size !== 2) {
    errors.push("WLO1 requires two distinct target projections.");
  }
  const targetKeys = ["id", "targetId", "layoutReceipt", "interopSchemaReceipt", "wAbiImpact", "targetWAbiIndependent", "packageReceipt"];
  for (const target of targets) {
    if (JSON.stringify(Object.keys(target).sort()) !== JSON.stringify([...targetKeys].sort())) errors.push(`unexpected target receipt fields for ${target.id}`);
    if (target.interopSchemaReceipt !== "WLO-String-v1-data-only" || target.wAbiImpact !== "none" || target.targetWAbiIndependent !== true) {
      errors.push(`target receipt authority drift for ${target.id}`);
    }
  }
  const cases = corpus.cases ?? [];
  const ids = new Set();
  for (const testCase of cases) {
    if (!/^WLO1-(?:POS|NEG)-[a-z0-9-]+$/.test(testCase.id ?? "")) errors.push(`invalid WLO1 case id ${testCase.id}`);
    if (ids.has(testCase.id)) errors.push(`duplicate WLO1 case ${testCase.id}`);
    ids.add(testCase.id);
  }
  const required = [
    "WLO1-POS-canonical-string",
    "WLO1-POS-target-parity",
    "WLO1-POS-empty-bounded",
    "WLO1-NEG-invalid-major-type",
    "WLO1-NEG-version-mismatch",
    "WLO1-NEG-schema-mismatch",
    "WLO1-NEG-field-order-duplicate",
    "WLO1-NEG-profile-unknown-field",
    "WLO1-NEG-invalid-utf8",
    "WLO1-NEG-noncanonical-length",
    "WLO1-NEG-trailing-bytes",
    "WLO1-NEG-limit-oom-preflight",
    "WLO1-NEG-pointer-owner-capability-drop",
    "WLO1-NEG-editor-tree-authority",
  ];
  for (const id of required) if (!ids.has(id)) errors.push(`missing WLO1 case ${id}`);
  return { errors, results: cases.map((testCase) => evaluateWloCase(testCase, corpus)) };
}

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

export function canonicalDigest(value) {
  return stableDigest(value);
}
