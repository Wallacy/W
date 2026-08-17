import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export const PROBE_OUTPUT_SCHEMA = "w-ipc1-probe-output-1";
export const PROBE_CLAIM_BOUNDARY = "Observed POSIX two-process mapping behavior only; this is not a W implementation or a provider readiness receipt.";
export const REQUIRED_NOT_PROVEN = [
  "w-compile",
  "w-run",
  "provider-receipt",
  "crash-recovery",
  "durability",
  "windows-two-process",
];

const OUTPUT_KEYS = ["schema", "id", "status", "target", "observed", "providerReceipt"];
const OBSERVED_KEYS = [
  "twoProcess",
  "addressesDistinct",
  "headerValidated",
  "committedRead",
  "wake",
  "staleNameRejected",
  "remapGeneration",
  "cleanup",
];
const RECEIPT_KEYS = ["schema", "id", "status", "target", "identity", "host", "source", "output", "command", "observed", "providerReceipt", "notProven", "claimBoundary"];
const HOST_KEYS = ["os", "kernel", "compiler", "date"];
const EXPECTED_OBSERVED = {
  twoProcess: true,
  addressesDistinct: true,
  headerValidated: true,
  committedRead: true,
  wake: "bounded-polling",
  staleNameRejected: true,
  remapGeneration: 2,
  cleanup: "unmap-close-unlink",
};

function validDigest(value) {
  return typeof value === "string" && /^sha256:[0-9a-f]{64}$/u.test(value);
}

function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function exactKeys(value, keys) {
  return value && typeof value === "object" && !Array.isArray(value)
    && JSON.stringify(Object.keys(value)) === JSON.stringify(keys);
}

function containedFile(boundaryRoot, base, relative) {
  if (typeof relative !== "string" || relative.trim() === "" || path.isAbsolute(relative)) return undefined;
  const resolved = path.resolve(base, relative);
  const relativeToRoot = path.relative(path.resolve(boundaryRoot), resolved);
  if (!relativeToRoot || relativeToRoot.startsWith(`..${path.sep}`) || path.isAbsolute(relativeToRoot)) return undefined;
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) return undefined;
  return resolved;
}

function parseJson(file) {
  try { return JSON.parse(fs.readFileSync(file, "utf8")); } catch { return undefined; }
}

function equalJson(left, right) {
  return JSON.stringify(left) === JSON.stringify(right);
}

function addTypeError(errors, location, value, type) {
  if (typeof value !== type) errors.push(`${location} must be ${type}.`);
}

export function validateProbeEvidence({ probe, receiptFile, boundaryRoot, location = "probe" } = {}) {
  const errors = [];
  const receipt = receiptFile && fs.existsSync(receiptFile) ? parseJson(receiptFile) : undefined;
  if (!receipt || typeof receipt !== "object" || Array.isArray(receipt)) {
    errors.push(`${location} receipt is not valid JSON.`);
    return { errors, receipt, output: undefined };
  }
  if (!exactKeys(receipt, RECEIPT_KEYS)) errors.push(`${location} receipt top-level keys are invalid.`);
  const receiptDirectory = path.dirname(receiptFile);
  const expectedIdentity = {
    schema: PROBE_OUTPUT_SCHEMA,
    id: probe?.id,
    status: probe?.status,
    target: probe?.target,
  };
  if (receipt.schema !== "w-ipc1-probe-receipt-1") errors.push(`${location}.schema is invalid.`);
  if (receipt.id !== probe?.id) errors.push(`${location}.id must match probeRef.id.`);
  if (receipt.status !== probe?.status) errors.push(`${location}.status must match probeRef.status.`);
  if (receipt.target !== probe?.target) errors.push(`${location}.target must match probeRef.target.`);
  if (!exactKeys(receipt.identity, ["schema", "id", "status", "target"]) || !equalJson(receipt.identity, expectedIdentity)) errors.push(`${location}.identity does not match probe output identity.`);
  if (receipt.providerReceipt !== false) errors.push(`${location}.providerReceipt must be false.`);
  if (!exactKeys(receipt.host, HOST_KEYS)) errors.push(`${location}.host keys are invalid.`);
  for (const key of HOST_KEYS) if (typeof receipt.host?.[key] !== "string" || receipt.host[key].trim() === "") errors.push(`${location}.host.${key} must be a non-empty string.`);
  if (!/^\d{4}-\d{2}-\d{2}$/u.test(receipt.host?.date ?? "")) errors.push(`${location}.host.date must use YYYY-MM-DD.`);
  if (typeof receipt.command !== "string" || receipt.command.trim() === "") errors.push(`${location}.command must be a non-empty string.`);
  if (/provider-ready|provider readiness|runtime-executed|implemented|W implementation|W execution/iu.test(receipt.command ?? "")) errors.push(`${location}.command must not claim readiness or W execution.`);
  if (!Array.isArray(receipt.notProven) || !equalJson(receipt.notProven, REQUIRED_NOT_PROVEN)) errors.push(`${location}.notProven must equal the canonical ordered set.`);
  if (receipt.claimBoundary !== PROBE_CLAIM_BOUNDARY) errors.push(`${location}.claimBoundary must use the canonical negative boundary.`);

  const source = receipt.source;
  const outputRef = receipt.output;
  if (!exactKeys(source, ["path", "digest"]) || !validDigest(source?.digest) || !source?.path?.endsWith(".c")) errors.push(`${location}.source must contain a .c path and exact sha256 digest.`);
  if (!exactKeys(outputRef, ["path", "digest"]) || !validDigest(outputRef?.digest) || !outputRef?.path?.endsWith(".json")) errors.push(`${location}.output must contain a .json path and exact sha256 digest.`);
  const sourceFile = containedFile(boundaryRoot, receiptDirectory, source?.path);
  const outputFile = containedFile(boundaryRoot, receiptDirectory, outputRef?.path);
  if (!sourceFile) errors.push(`${location}.source.path is missing or out-of-tree.`);
  if (!outputFile) errors.push(`${location}.output.path is missing or out-of-tree.`);
  if (sourceFile && source?.digest !== digestFile(sourceFile)) errors.push(`${location}.source.digest is stale.`);
  if (outputFile && outputRef?.digest !== digestFile(outputFile)) errors.push(`${location}.output.digest is stale.`);

  const output = outputFile ? parseJson(outputFile) : undefined;
  if (!output || typeof output !== "object" || Array.isArray(output)) {
    errors.push(`${location}.output transcript is not valid JSON.`);
    return { errors, receipt, output, sourceFile, outputFile };
  }
  if (!exactKeys(output, OUTPUT_KEYS)) errors.push(`${location}.output transcript keys are invalid.`);
  if (output.schema !== PROBE_OUTPUT_SCHEMA) errors.push(`${location}.output transcript schema is invalid.`);
  if (output.id !== probe?.id || output.id !== receipt.id) errors.push(`${location}.output transcript id does not match probe identity.`);
  if (output.status !== probe?.status || output.status !== receipt.status) errors.push(`${location}.output transcript status is invalid.`);
  if (output.target !== probe?.target || output.target !== receipt.target) errors.push(`${location}.output transcript target is invalid.`);
  if (output.providerReceipt !== false) errors.push(`${location}.output transcript providerReceipt must be false.`);
  if (!exactKeys(output.observed, OBSERVED_KEYS)) errors.push(`${location}.output transcript observed keys are invalid.`);
  const observedTypes = {
    twoProcess: "boolean",
    addressesDistinct: "boolean",
    headerValidated: "boolean",
    committedRead: "boolean",
    wake: "string",
    staleNameRejected: "boolean",
    remapGeneration: "number",
    cleanup: "string",
  };
  for (const [key, type] of Object.entries(observedTypes)) addTypeError(errors, `${location}.output.observed.${key}`, output.observed?.[key], type);
  if (!equalJson(output.observed, EXPECTED_OBSERVED)) errors.push(`${location}.output.observed facts must equal the canonical observed result.`);
  if (!exactKeys(receipt.observed, OBSERVED_KEYS) || !equalJson(receipt.observed, output.observed)) errors.push(`${location}.observed must match output transcript exactly.`);
  if (!equalJson(receipt.identity, { schema: output.schema, id: output.id, status: output.status, target: output.target })) errors.push(`${location}.identity must match output transcript exactly.`);
  return { errors, receipt, output, sourceFile, outputFile };
}
