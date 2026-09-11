import crypto from "node:crypto";

export function stableJson(value) {
  if (value === null || typeof value !== "object") return JSON.stringify(value);
  if (Array.isArray(value)) return `[${value.map(stableJson).join(",")}]`;
  return `{${Object.keys(value).sort().map((key) =>
    `${JSON.stringify(key)}:${stableJson(value[key])}`).join(",")}}`;
}

export function semanticDigest(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

export function caseDigest(value) {
  return semanticDigest(stableJson(value));
}
