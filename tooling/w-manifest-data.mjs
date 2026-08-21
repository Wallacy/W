import crypto from "node:crypto";

const IDENTIFIER = /^[A-Za-z_][A-Za-z0-9_]*/u;
const QUANTITY = /^[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:[eE][+-]?[0-9](?:_?[0-9])*)?<[^>\r\n]+>/u;
const SIZE = /^[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:[eE][+-]?[0-9](?:_?[0-9])*)?(?:B|KiB|MiB|GiB)/u;
const NUMBER = /^(?:0[xX][0-9a-fA-F](?:_?[0-9a-fA-F])*(?:_[A-Za-z][A-Za-z0-9]*)?|0[bB][01](?:_?[01])*(?:_[A-Za-z][A-Za-z0-9]*)?|0[oO][0-7](?:_?[0-7])*(?:_[A-Za-z][A-Za-z0-9]*)?|[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:[eE][+-]?[0-9](?:_?[0-9])*)?(?:_[A-Za-z][A-Za-z0-9]*)?)/u;
const OWNER_EXCLUDED = new Set(["resolution", "deployments"]);

export class ManifestDataError extends Error {
  constructor(code, position) {
    super(`${code} at byte ${position}`);
    this.code = code;
    this.position = position;
  }
}

function fail(code, position) {
  throw new ManifestDataError(code, position);
}

function decodeString(raw, position) {
  try {
    return JSON.parse(raw);
  } catch {
    let output = "";
    for (let index = 1; index < raw.length - 1; index += 1) {
      if (raw[index] !== "\\") {
        output += raw[index];
        continue;
      }
      index += 1;
      const escape = raw[index];
      if (escape === "x") {
        const hex = raw.slice(index + 1, index + 3);
        if (!/^[0-9a-fA-F]{2}$/u.test(hex)) fail("manifestStringEscapeInvalid", position + index);
        output += String.fromCodePoint(Number.parseInt(hex, 16));
        index += 2;
        continue;
      }
      const escapes = { "0": "\0", n: "\n", r: "\r", t: "\t", "\\": "\\", '"': '"' };
      if (!(escape in escapes)) fail("manifestStringEscapeInvalid", position + index);
      output += escapes[escape];
    }
    return output;
  }
}

function normalizeDecimal(raw) {
  const compact = raw.replaceAll("_", "").toLowerCase();
  const [mantissa, exponentText = "0"] = compact.split("e");
  const [whole, fraction = ""] = mantissa.split(".");
  let digits = `${whole}${fraction}`.replace(/^0+(?=\d)/u, "");
  let exponent = Number.parseInt(exponentText, 10) - fraction.length;
  if (/^0+$/u.test(digits)) return { coefficient: "0", exponent: 0 };
  while (digits.endsWith("0")) {
    digits = digits.slice(0, -1);
    exponent += 1;
  }
  return { coefficient: digits, exponent };
}

function normalizeNumber(raw, position) {
  const suffixMatch = /_([A-Za-z][A-Za-z0-9]*)$/u.exec(raw);
  const suffix = suffixMatch?.[1];
  const numeric = suffixMatch ? raw.slice(0, suffixMatch.index) : raw;
  const compact = numeric.replaceAll("_", "");
  let value;
  try {
    if (/^0[xX]/u.test(compact)) value = { integer: BigInt(compact).toString(10) };
    else if (/^0[bB]/u.test(compact)) value = { integer: BigInt(compact).toString(10) };
    else if (/^0[oO]/u.test(compact)) value = { integer: BigInt(compact).toString(10) };
    else value = normalizeDecimal(numeric);
  } catch {
    fail("manifestNumberInvalid", position);
  }
  return suffix ? { $number: value, suffix } : { $number: value };
}

function tokenize(source) {
  const tokens = [];
  let index = 0;
  while (index < source.length) {
    const character = source[index];
    if (/\s/u.test(character)) {
      index += 1;
      continue;
    }
    if (character === "/" && source[index + 1] === "/") {
      index += 2;
      while (index < source.length && source[index] !== "\n") index += 1;
      continue;
    }
    if (character === "/" && source[index + 1] === "*") {
      const start = index;
      index += 2;
      while (index < source.length && !(source[index] === "*" && source[index + 1] === "/")) index += 1;
      if (index >= source.length) fail("manifestCommentUnterminated", start);
      index += 2;
      continue;
    }
    if (character === '"') {
      const start = index;
      index += 1;
      while (index < source.length) {
        if (source[index] === "\\") index += 2;
        else if (source[index] === '"') {
          index += 1;
          break;
        } else index += 1;
      }
      if (source[index - 1] !== '"') fail("manifestStringUnterminated", start);
      const raw = source.slice(start, index);
      tokens.push({ kind: "string", value: decodeString(raw, start), position: start });
      continue;
    }
    const rest = source.slice(index);
    const quantity = QUANTITY.exec(rest)?.[0];
    if (quantity) {
      const angle = quantity.indexOf("<");
      tokens.push({ kind: "quantity", value: { amount: normalizeDecimal(quantity.slice(0, angle)), unit: quantity.slice(angle + 1, -1).replace(/\s+/gu, "") }, position: index });
      index += quantity.length;
      continue;
    }
    const size = SIZE.exec(rest)?.[0];
    if (size) {
      const unit = /(KiB|MiB|GiB|B)$/u.exec(size)?.[1];
      tokens.push({ kind: "size", value: { amount: normalizeDecimal(size.slice(0, -unit.length)), unit }, position: index });
      index += size.length;
      continue;
    }
    const number = NUMBER.exec(rest)?.[0];
    if (number) {
      tokens.push({ kind: "number", value: normalizeNumber(number, index), position: index });
      index += number.length;
      continue;
    }
    const identifier = IDENTIFIER.exec(rest)?.[0];
    if (identifier) {
      tokens.push({ kind: "identifier", value: identifier, position: index });
      index += identifier.length;
      continue;
    }
    if ("{}[]():,.".includes(character)) {
      tokens.push({ kind: character, value: character, position: index });
      index += 1;
      continue;
    }
    fail("manifestTokenInvalid", index);
  }
  tokens.push({ kind: "eof", value: "", position: source.length });
  return tokens;
}

class Parser {
  constructor(source) {
    this.tokens = tokenize(source);
    this.index = 0;
  }

  current() {
    return this.tokens[this.index];
  }

  at(kind) {
    return this.current().kind === kind;
  }

  take(kind, code = "manifestUnexpectedToken") {
    const token = this.current();
    if (token.kind !== kind) fail(code, token.position);
    this.index += 1;
    return token;
  }

  parseRootRecord() {
    const root = this.take("identifier", "manifestRootMissing");
    if (!new Set(["package", "workspace"]).has(root.value)) fail("manifestRootInvalid", root.position);
    return { kind: root.value, ...this.parseRecord() };
  }

  parseBuildManifest() {
    const records = [];
    const seen = new Set();
    while (!this.at("eof")) {
      const rootPosition = this.current().position;
      const record = this.parseRootRecord();
      if (seen.has(record.kind)) fail("manifestDuplicateRoot", rootPosition);
      seen.add(record.kind);
      records.push(record);
    }
    if (records.length === 0) fail("manifestRootMissing", this.current().position);
    return {
      kind: "build_manifest",
      records,
      package: records.find((record) => record.kind === "package") ?? null,
      workspace: records.find((record) => record.kind === "workspace") ?? null,
    };
  }

  parseDocument() {
    const record = this.parseRootRecord();
    this.take("eof", "manifestTrailingInput");
    return record;
  }

  parseRecord() {
    this.take("{");
    const fields = {};
    while (!this.at("}")) {
      const name = this.take("identifier", "manifestFieldNameMissing");
      this.take(":", "manifestFieldColonMissing");
      if (Object.hasOwn(fields, name.value)) fail("manifestDuplicateField", name.position);
      fields[name.value] = this.parseValue();
      if (this.at(",")) this.take(",");
    }
    this.take("}");
    return fields;
  }

  parseList() {
    this.take("[");
    const items = [];
    while (!this.at("]")) {
      items.push(this.parseValue());
      if (this.at(",")) this.take(",");
    }
    this.take("]");
    return items;
  }

  parseMember() {
    this.take(".");
    const name = this.take("identifier", "manifestMemberNameMissing");
    if (!this.at("(")) return { $member: name.value };
    this.take("(");
    const argumentsList = [];
    while (!this.at(")")) {
      let label = null;
      if (this.at("identifier") && this.tokens[this.index + 1]?.kind === ":") {
        label = this.take("identifier").value;
        this.take(":");
      }
      argumentsList.push({ label, value: this.parseValue() });
      if (this.at(",")) this.take(",");
    }
    this.take(")");
    return { $constructor: name.value, arguments: argumentsList };
  }

  parseValue() {
    const token = this.current();
    if (token.kind === "{") return this.parseRecord();
    if (token.kind === "[") return this.parseList();
    if (token.kind === ".") return this.parseMember();
    if (token.kind === "string") {
      this.index += 1;
      return token.value;
    }
    if (token.kind === "number") {
      this.index += 1;
      return token.value;
    }
    if (token.kind === "size") {
      this.index += 1;
      return { $size: token.value };
    }
    if (token.kind === "quantity") {
      this.index += 1;
      return { $quantity: token.value };
    }
    if (token.kind === "identifier" && (token.value === "true" || token.value === "false")) {
      this.index += 1;
      return token.value === "true";
    }
    fail("manifestValueInvalid", token.position);
  }
}

export function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") {
    return Object.fromEntries(
      Object.keys(value)
        .filter((key) => value[key] !== undefined)
        .sort()
        .map((key) => [key, canonical(value[key])]),
    );
  }
  return value;
}

export function digestRecord(tag, value) {
  return `sha256:${crypto.createHash("sha256").update(`${tag}\0${JSON.stringify(canonical(value))}`, "utf8").digest("hex")}`;
}

export function parseManifestDocument(source) {
  if (typeof source !== "string") fail("manifestSourceInvalid", 0);
  return new Parser(source).parseDocument();
}

export function parseBuildManifest(source) {
  if (typeof source !== "string") fail("manifestSourceInvalid", 0);
  return new Parser(source).parseBuildManifest();
}

export function deriveOwnerBasis(document) {
  if (!document || typeof document !== "object" || Array.isArray(document)) fail("manifestDocumentInvalid", 0);
  if (!new Set(["package", "workspace"]).has(document.kind)) fail("manifestRootInvalid", 0);
  return canonical(Object.fromEntries(Object.entries(document).filter(([key]) => !OWNER_EXCLUDED.has(key))));
}

export function deriveOwnerDigest(document) {
  return digestRecord("w.owner/1", deriveOwnerBasis(document));
}
