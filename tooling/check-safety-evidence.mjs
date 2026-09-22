import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const evidenceFile = path.join(root, "tooling", "safety-evidence.json");
const commandRegistryFile = path.join(root, "tooling", "command-registry.json");

const expectedAreas = [
  "memory-lifetime",
  "numeric",
  "errors-panic-oom-cleanup",
  "concurrency-reclamation",
  "ffi-unsafe-abi",
  "resource-exhaustion-io",
  "optimizer-codegen",
  "build-supply-chain",
];
const expectedGates = [
  "source", "verifiedHir", "lowering", "nativeProduct", "runtimeProvider",
  "targetExecution", "negative", "sanitizerFuzz", "faultInjection",
  "scheduleExploration", "resourceExhaustion", "abiLayout",
];
const expectedStates = ["not-applicable", "missing", "bounded", "current"];
const expectedPromotions = ["blocked", "bounded", "general"];
const expectedEvidence = [
  "scope", "sources", "command", "route", "targets", "profile", "oracle", "freshness",
];
const expectedRoutes = ["public-w-source-to-native"];
const expectedTargets = ["target-neutral", "windows-x64", "linux-wsl-x64"];
const expectedProfiles = ["debug", "release", "not-applicable"];
const expectedOracleKinds = ["c23-and-rust", "c23", "rust", "none"];
const expectedDurableReceipts = ["benchmarks/executable-catalog.json"];
const expectedRecordIds = ["numeric-runtime-i8-process-v1"];
const digestPattern = /^sha256:[0-9a-f]{64}$/;
const recordIdPattern = /^[a-z0-9]+(?:-[a-z0-9]+)*-v[0-9]+$/;

function fail(message) {
  throw new Error(`safety evidence: ${message}`);
}

function isRecord(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function assertRecord(value, label) {
  if (!isRecord(value)) fail(`${label} must be an object`);
}

function assertExactKeys(value, expected, label) {
  assertRecord(value, label);
  const actual = Object.keys(value).sort();
  const wanted = [...expected].sort();
  if (JSON.stringify(actual) !== JSON.stringify(wanted)) {
    fail(`${label} keys drift (expected ${expected.join(",")}, got ${Object.keys(value).join(",")})`);
  }
}

function assertNonEmptyString(value, label) {
  if (typeof value !== "string" || value.trim().length === 0) fail(`${label} must be nonempty`);
}

function assertStringArray(value, label, { allowEmpty = false } = {}) {
  if (!Array.isArray(value) || (!allowEmpty && value.length === 0)) fail(`${label} must be a nonempty array`);
  const seen = new Set();
  for (const [index, item] of value.entries()) {
    assertNonEmptyString(item, `${label}[${index}]`);
    if (seen.has(item)) fail(`${label} contains duplicate ${JSON.stringify(item)}`);
    seen.add(item);
  }
}

function assertClosedArray(value, expected, label) {
  if (JSON.stringify(value) !== JSON.stringify(expected)) fail(`${label} vocabulary drift`);
}

function safeRelativePath(relativePath, label, workspaceRoot) {
  assertNonEmptyString(relativePath, label);
  if (relativePath.includes("\\") || relativePath.includes("\0") || relativePath.startsWith("/") || path.isAbsolute(relativePath)) {
    fail(`${label} must be a normalized relative path`);
  }
  const segments = relativePath.split("/");
  if (segments.some((segment) => segment.length === 0 || segment === "." || segment === "..")) {
    fail(`${label} must be a normalized relative path`);
  }
  const absolute = path.resolve(workspaceRoot, relativePath);
  const prefix = workspaceRoot.endsWith(path.sep) ? workspaceRoot : `${workspaceRoot}${path.sep}`;
  if (absolute !== workspaceRoot && !absolute.startsWith(prefix)) fail(`${label} escapes the workspace`);
  let stat;
  try {
    stat = fs.lstatSync(absolute);
  } catch {
    fail(`${label} does not exist: ${relativePath}`);
  }
  if (!stat.isFile() || stat.isSymbolicLink()) fail(`${label} is not a regular file: ${relativePath}`);
  const workspaceRealPath = fs.realpathSync(workspaceRoot);
  const targetRealPath = fs.realpathSync(absolute);
  const realRelative = path.relative(workspaceRealPath, targetRealPath);
  if (realRelative.startsWith("..") || path.isAbsolute(realRelative)) fail(`${label} escapes the workspace`);
  return absolute;
}

function sha256File(absolutePath) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(absolutePath)).digest("hex")}`;
}

function readJson(absolutePath, label) {
  let text;
  try {
    text = fs.readFileSync(absolutePath, "utf8");
  } catch {
    fail(`${label} cannot be read`);
  }
  try {
    return JSON.parse(text);
  } catch (error) {
    fail(`${label} is not valid JSON (${error.message})`);
  }
}

function collectReceiptDigests(value, result = new Set()) {
  if (Array.isArray(value)) {
    for (const item of value) collectReceiptDigests(item, result);
  } else if (isRecord(value)) {
    if (typeof value.path === "string" && typeof value.digest === "string") {
      result.add(`${value.path}\0${value.digest}`);
    }
    for (const child of Object.values(value)) collectReceiptDigests(child, result);
  }
  return result;
}

function validateFreshness(freshness, evidence, oracle, policy, workspaceRoot, receiptCache, label) {
  assertExactKeys(freshness, ["receipt", "digests"], `${label}.freshness`);
  if (!policy.durableReceipts.includes(freshness.receipt)) fail(`${label}.freshness receipt is not durable`);
  const receiptPath = safeRelativePath(freshness.receipt, `${label}.freshness.receipt`, workspaceRoot);
  if (!Array.isArray(freshness.digests) || freshness.digests.length === 0) fail(`${label}.freshness.digests must be a nonempty array`);
  const expectedPaths = new Set([...evidence.sources, ...oracle.sources]);
  const actualPaths = new Set();
  let receiptDigests = receiptCache.get(freshness.receipt);
  if (!receiptDigests) {
    receiptDigests = collectReceiptDigests(readJson(receiptPath, `${label}.freshness.receipt`));
    receiptCache.set(freshness.receipt, receiptDigests);
  }
  for (const [index, reference] of freshness.digests.entries()) {
    assertExactKeys(reference, ["path", "digest"], `${label}.freshness.digests[${index}]`);
    const referencePath = reference.path;
    if (actualPaths.has(referencePath)) fail(`${label}.freshness has duplicate path ${referencePath}`);
    actualPaths.add(referencePath);
    const absolute = safeRelativePath(referencePath, `${label}.freshness.digests[${index}].path`, workspaceRoot);
    if (!digestPattern.test(reference.digest)) fail(`${label}.freshness.digests[${index}].digest is invalid`);
    const actualDigest = sha256File(absolute);
    if (actualDigest !== reference.digest) fail(`${label}.freshness digest drift for ${referencePath}`);
    if (!receiptDigests.has(`${referencePath}\0${reference.digest}`)) {
      fail(`${label}.freshness digest is not present in ${freshness.receipt}: ${referencePath}`);
    }
  }
  if (JSON.stringify([...actualPaths].sort()) !== JSON.stringify([...expectedPaths].sort())) {
    fail(`${label}.freshness must cover every maintained and oracle source path`);
  }
}

function validateOracle(oracle, evidence, policy, workspaceRoot, label) {
  assertExactKeys(oracle, ["kind", "sources", "declaration", "ubSafety"], `${label}.oracle`);
  if (!policy.oracleKinds.includes(oracle.kind)) fail(`${label}.oracle has invalid kind`);
  assertStringArray(oracle.sources, `${label}.oracle.sources`, { allowEmpty: oracle.kind === "none" });
  assertNonEmptyString(oracle.declaration, `${label}.oracle.declaration`);
  assertNonEmptyString(oracle.ubSafety, `${label}.oracle.ubSafety`);
  const evidenceSources = new Set(evidence.sources);
  for (const [index, source] of oracle.sources.entries()) {
    safeRelativePath(source, `${label}.oracle.sources[${index}]`, workspaceRoot);
    if (evidenceSources.has(source)) fail(`${label}.oracle source must be independent: ${source}`);
  }
  if (oracle.kind === "none") fail(`${label}.oracle must declare an independent oracle`);
  if (oracle.kind === "c23-and-rust") {
    if (!oracle.sources.some((source) => source.endsWith(".c")) || !oracle.sources.some((source) => source.endsWith(".rs"))) {
      fail(`${label}.oracle c23-and-rust requires C and Rust source paths`);
    }
  } else if (oracle.kind === "c23" && !oracle.sources.some((source) => source.endsWith(".c"))) {
    fail(`${label}.oracle c23 requires a C source path`);
  } else if (oracle.kind === "rust" && !oracle.sources.some((source) => source.endsWith(".rs"))) {
    fail(`${label}.oracle rust requires a Rust source path`);
  }
}

function validateRecord(record, policy, registeredCommands, workspaceRoot, receiptCache, label) {
  assertExactKeys(record, ["gates", ...expectedEvidence], label);
  assertStringArray(record.gates, `${label}.gates`);
  if (record.gates.some((gate) => !expectedGates.includes(gate))) fail(`${label}.gates has an unknown gate`);
  assertExactKeys(record.scope, ["claim", "limits"], `${label}.scope`);
  assertNonEmptyString(record.scope.claim, `${label}.scope.claim`);
  assertStringArray(record.scope.limits, `${label}.scope.limits`);
  assertStringArray(record.sources, `${label}.sources`);
  for (const [index, source] of record.sources.entries()) safeRelativePath(source, `${label}.sources[${index}]`, workspaceRoot);
  assertNonEmptyString(record.command, `${label}.command`);
  if (!registeredCommands.has(record.command)) fail(`${label}.command is not registered: ${record.command}`);
  const commandSpec = registeredCommands.get(record.command);
  assertRecord(commandSpec, `${label}.command registration`);
  if (!Array.isArray(commandSpec.steps) || commandSpec.steps.length === 0) fail(`${label}.command registration has no reproducible steps`);
  assertNonEmptyString(record.route, `${label}.route`);
  if (!policy.routes.includes(record.route)) fail(`${label}.route is not in the closed route vocabulary`);
  assertStringArray(record.targets, `${label}.targets`);
  for (const target of record.targets) if (!policy.targets.includes(target)) fail(`${label}.targets has invalid target: ${target}`);
  assertNonEmptyString(record.profile, `${label}.profile`);
  if (!policy.profiles.includes(record.profile)) fail(`${label}.profile is not in the closed profile vocabulary`);
  validateOracle(record.oracle, record, policy, workspaceRoot, label);
  validateFreshness(record.freshness, record, record.oracle, policy, workspaceRoot, receiptCache, label);
}

export function validateSafetyEvidence(value, { workspaceRoot = root, commandRegistry = null } = {}) {
  assertExactKeys(value, ["$schema", "version", "policy", "records", "areas"], "catalog");
  if (value.$schema !== "w-safety-evidence-2" || value.version !== 2) fail("invalid schema/version");
  assertExactKeys(value.policy, [
    "promotionRule", "currentEvidence", "gateOrder", "states", "routes", "targets", "profiles", "oracleKinds", "durableReceipts", "recordIds",
  ], "policy");
  assertNonEmptyString(value.policy.promotionRule, "policy.promotionRule");
  assertClosedArray(value.policy.currentEvidence, expectedEvidence, "current evidence fields");
  assertClosedArray(value.policy.gateOrder, expectedGates, "gate order");
  assertClosedArray(value.policy.states, expectedStates, "state vocabulary");
  assertClosedArray(value.policy.routes, expectedRoutes, "route vocabulary");
  assertClosedArray(value.policy.targets, expectedTargets, "target vocabulary");
  assertClosedArray(value.policy.profiles, expectedProfiles, "profile vocabulary");
  assertClosedArray(value.policy.oracleKinds, expectedOracleKinds, "oracle vocabulary");
  assertClosedArray(value.policy.durableReceipts, expectedDurableReceipts, "durable receipt vocabulary");
  assertClosedArray(value.policy.recordIds, expectedRecordIds, "record ID vocabulary");
  if (!Array.isArray(value.areas) || JSON.stringify(value.areas.map((area) => area.id)) !== JSON.stringify(expectedAreas)) {
    fail("area inventory/order drift");
  }

  assertRecord(value.records, "records");
  if (JSON.stringify(Object.keys(value.records)) !== JSON.stringify(expectedRecordIds)) fail("record inventory drift");
  const registry = commandRegistry ?? readJson(commandRegistryFile, "command registry");
  assertRecord(registry.commands, "command registry.commands");
  const registeredCommands = new Map(Object.entries(registry.commands));
  const receiptCache = new Map();
  const records = new Map();
  for (const recordId of expectedRecordIds) {
    if (!recordIdPattern.test(recordId)) fail(`record ID is not stable: ${recordId}`);
    const record = value.records[recordId];
    validateRecord(record, value.policy, registeredCommands, workspaceRoot, receiptCache, `records.${recordId}`);
    records.set(recordId, record);
  }
  const referencedGates = new Map(expectedRecordIds.map((recordId) => [recordId, new Set()]));
  for (const area of value.areas) {
    const currentGates = [];
    assertRecord(area, `area ${area.id}`);
    assertExactKeys(area, ["id", "promotion", "risks", "gates", "nextEvidence", ...(area.evidence === undefined ? [] : ["evidence"])], `area ${area.id}`);
    if (!expectedPromotions.includes(area.promotion)) fail(`${area.id} has invalid promotion`);
    assertStringArray(area.risks, `${area.id}.risks`);
    if (JSON.stringify(Object.keys(area.gates)) !== JSON.stringify(expectedGates)) fail(`${area.id} gates invalid`);
    for (const [gate, state] of Object.entries(area.gates)) {
      if (!expectedStates.includes(state)) fail(`${area.id}.${gate} has invalid state`);
      if (state === "current") currentGates.push(gate);
    }
    assertStringArray(area.nextEvidence, `${area.id}.nextEvidence`);
    const applicable = Object.values(area.gates).filter((state) => state !== "not-applicable");
    const complete = applicable.every((state) => state === "current");
    const derivedPromotion = complete ? "general" : currentGates.length > 0 ? "bounded" : "blocked";
    if (area.promotion !== derivedPromotion) fail(`${area.id} promotion is inconsistent with gate states`);
    if (currentGates.length === 0) {
      if (area.evidence !== undefined) fail(`${area.id} has evidence for no current gates`);
      continue;
    }
    assertExactKeys(area.evidence, currentGates, `${area.id}.evidence`);
    for (const gate of currentGates) {
      const recordId = area.evidence[gate];
      assertNonEmptyString(recordId, `${area.id}.evidence.${gate}`);
      if (!records.has(recordId)) fail(`${area.id}.${gate} references an unknown record: ${recordId}`);
      const record = records.get(recordId);
      if (!record.gates.includes(gate)) fail(`${area.id}.${gate} is not listed by record ${recordId}`);
      referencedGates.get(recordId).add(gate);
    }
  }
  for (const [recordId, record] of records) {
    const usedGates = [...referencedGates.get(recordId)].sort();
    if (usedGates.length === 0) fail(`record ${recordId} is unused`);
    if (JSON.stringify(usedGates) !== JSON.stringify([...record.gates].sort())) {
      fail(`record ${recordId} gate inventory does not match its area references`);
    }
  }

  return {
    areas: value.areas.length,
    summary: Object.fromEntries(expectedPromotions.map((state) => [
      state,
      value.areas.filter((area) => area.promotion === state).length,
    ])),
  };
}

function main() {
  const value = readJson(evidenceFile, "safety evidence catalog");
  const summary = validateSafetyEvidence(value);
  process.stdout.write(`Safety evidence: ${summary.areas} areas; ${JSON.stringify(summary.summary)}\n`);
}

const invokedPath = process.argv[1] ? path.resolve(process.argv[1]) : "";
if (invokedPath === path.resolve(fileURLToPath(import.meta.url))) main();
