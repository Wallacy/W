import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

export const PLATFORM_SUPPORT_SCHEMA = "w-platform-support-1";
export const TARGET_STATES = Object.freeze([
  "candidate",
  "evidence",
  "supported",
  "deprecated",
  "removed",
]);
export const VERIFICATION_LEVELS = Object.freeze([
  "experimental",
  "level-3",
  "level-2",
  "level-1",
  "long-term",
]);
export const AXIS_STATUSES = Object.freeze(["pass", "partial", "missing"]);
export const TARGET_AXES = Object.freeze([
  "backend",
  "runtime",
  "hostAdapter",
  "sdkProfile",
  "linkerSysrootPackaging",
  "ciEvidence",
]);
export const HOST_AXES = Object.freeze([
  "nativeToolchain",
  "toolchainBundle",
  "releasePackaging",
  "ciEvidence",
]);
export const CROSS_COMPILATION_STATES = Object.freeze([
  "candidate",
  "evidence",
  "supported",
]);
export const TARGET_GROUPS = Object.freeze([
  "desktop-server",
  "mobile",
  "webassembly",
  "embedded",
  "accelerator",
]);

export const EXPECTED_CANDIDATES = Object.freeze([
  { triple: "aarch64-unknown-linux-gnu", group: "desktop-server" },
  { triple: "x86_64-pc-windows-msvc", group: "desktop-server" },
  { triple: "aarch64-pc-windows-msvc", group: "desktop-server" },
  { triple: "aarch64-apple-darwin", group: "desktop-server" },
  { triple: "x86_64-apple-darwin", group: "desktop-server" },
  { triple: "aarch64-linux-android", group: "mobile" },
  { triple: "x86_64-linux-android", group: "mobile" },
  { triple: "aarch64-apple-ios", group: "mobile" },
  { triple: "aarch64-apple-ios-sim", group: "mobile" },
  { triple: "wasm32-wasip3", group: "webassembly" },
  { triple: "thumbv7em-none-eabihf", group: "embedded" },
  { triple: "riscv64gc-unknown-none-elf", group: "embedded" },
  { triple: "nvptx64-nvidia-cuda", group: "accelerator" },
  { triple: "amdgcn-amd-amdhsa", group: "accelerator" },
  { triple: "spirv64-unknown-vulkan", group: "accelerator" },
]);
export const EXPECTED_NATIVE_HOSTS = Object.freeze([
  { platform: "linux", architecture: "x86_64", hostTriple: "x86_64-unknown-linux-gnu" },
  { platform: "linux", architecture: "aarch64", hostTriple: "aarch64-unknown-linux-gnu" },
  { platform: "windows", architecture: "x86_64", hostTriple: "x86_64-pc-windows-msvc" },
  { platform: "windows", architecture: "aarch64", hostTriple: "aarch64-pc-windows-msvc" },
  { platform: "macos", architecture: "x86_64", hostTriple: "x86_64-apple-darwin" },
  { platform: "macos", architecture: "aarch64", hostTriple: "aarch64-apple-darwin" },
]);
export const PRIMARY_HOST_REFS = Object.freeze([
  "host-linux-x86_64-native",
  "host-windows-x86_64-native",
  "host-macos-aarch64-native",
]);
export const PRIMARY_TARGET_REFS = Object.freeze([
  "target-x86_64-unknown-linux-gnu",
  "target-x86_64-pc-windows-msvc",
  "target-aarch64-apple-darwin",
]);

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
export const repositoryRoot = path.resolve(toolingDirectory, "..");
export const platformSupportPath = path.join(toolingDirectory, "platform-support.json");
export const platformSupportDocumentPath = path.join(repositoryRoot, "PLATFORM-SUPPORT.md");
const mlir0ToolchainSchema = "w-seed-mlir0-toolchain-1";
const currentTargetTriple = "x86_64-unknown-linux-gnu";
const currentTargetScope = "w-seed-mlir0-6-linear-print-and-builtin-display";
const currentRuntimeVersion = "w-seed-mlir0-6";
const currentArtifactScope = "linear-print-and-builtin-display";
const pinnedToolchainVersion = "20.1.2";
const currentEvidenceCurrencyStatus = "update-required";
const futureNativePlanPolicy = "llvmorg-23.1.0-exact-pin-with-build-provenance-gate";
const successorToolchainVersion = "23.1.0";
const successorToolchainTag = "llvmorg-23.1.0";
const successorToolchainTagObject = "9b0f9b1eb4a233717c6ed014cff6f8a7c65512de";
const successorToolchainCommit = "ea7d852a70e8bdfaf601d6626a760f9771b2c4b4";
const dependencyCurrencyPromotionBlocker = "native-build-acquisition-provenance";
const evidenceKinds = new Set(["source", "unit", "check", "manifest"]);
const edgeEvidenceRoles = new Set([
  "toolchain",
  "sysroot",
  "linker",
  "packaging",
  "build",
  "execution",
  "development",
]);
const edgeExecutionModes = new Set(["local", "emulator", "simulator", "remote-runner"]);
const crossEdgeBlockers = Object.freeze([
  "hostEndpoint",
  "targetEndpoint",
  "toolchainSysrootLinkerPackaging",
  "buildExecution",
]);
const developmentEdgeBlockers = Object.freeze(["nativeHost"]);
const developmentEdgeId = "edge-dev-wsl2-windows-to-x86_64-unknown-linux-gnu";
const targetKeys = new Set([
  "id",
  "triple",
  "group",
  "targetSpec",
  "hostProfile",
  "artifactKind",
  "toolchainRoles",
  "sdkCapabilities",
  "compilerVersionSource",
  "runtimeVersion",
  "state",
  "verificationLevel",
  "scope",
  "claim",
  "axes",
  "blockers",
]);
const currentHostKeys = new Set([
  "id",
  "kind",
  "outerHostTriple",
  "toolExecutionTriple",
  "mode",
  "nativeForOuterHost",
  "state",
  "scope",
  "claim",
  "axes",
  "blockers",
]);
const candidateHostKeys = new Set([
  "id",
  "kind",
  "platform",
  "architecture",
  "hostTriple",
  "mode",
  "state",
  "scope",
  "claim",
  "axes",
  "blockers",
]);
const planKeys = new Set([
  "id",
  "platform",
  "architectures",
  "status",
  "source",
  "projects",
  "configuration",
  "outputs",
  "validation",
  "gaps",
]);
const crossCompilationKeys = new Set([
  "baselineHosts",
  "baselineTargets",
  "edges",
  "developmentEvidence",
]);
const crossEdgeKeys = new Set([
  "id",
  "hostRef",
  "targetRef",
  "state",
  "evidence",
  "blockers",
]);
const developmentEdgeKeys = new Set([
  "id",
  "hostRef",
  "targetRef",
  "state",
  "nativeHost",
  "evidence",
  "blockers",
]);
const edgeEvidenceKeys = new Set([
  "kind",
  "path",
  "symbol",
  "claim",
  "role",
  "executionMode",
]);
const externalToolchainCandidateKeys = new Set([
  "id",
  "status",
  "license",
  "observedRelease",
  "llvmTag",
  "publishedHostTriples",
  "evidenceCapabilities",
  "limitations",
]);
const dependencyCurrencyKeys = new Set([
  "currentEvidenceVersion",
  "currentEvidenceCurrencyStatus",
  "futureNativePlanPolicy",
  "successorVersion",
  "successorTag",
  "successorTagObject",
  "successorCommit",
  "promotionBlocker",
]);
const expectedExternalToolchainCandidate = Object.freeze({
  id: "portable-mlir-toolchain",
  status: "evaluation-only",
  license: "Apache-2.0",
  observedRelease: "2026.08.11",
  llvmTag: "llvmorg-22.1.8",
  publishedHostTriples: Object.freeze([
    "x86_64-unknown-linux-gnu",
    "aarch64-unknown-linux-gnu",
    "x86_64-pc-windows-msvc",
    "aarch64-pc-windows-msvc",
    "x86_64-apple-darwin",
    "aarch64-apple-darwin",
  ]),
  evidenceCapabilities: Object.freeze([
    "per-platform-build-scripts",
    "sha256-release-assets",
    "release-attestation",
    "verified-release-commit",
  ]),
  limitations: Object.freeze([
    "third-party not W authority/support",
    "version mismatch with current pinned 20.1.2",
    "no completed W trust/SBOM/provenance audit",
    "host binaries do not prove cross-compilation",
    "Apple SDK/license not supplied/proven",
  ]),
});

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function same(left, right) {
  return JSON.stringify(left) === JSON.stringify(right);
}

function addError(errors, message) {
  errors.push(message);
}

function idFor(row, fallback) {
  return typeof row?.id === "string" && row.id.length > 0 ? row.id : fallback;
}

function validateKeys(value, allowed, location, errors) {
  if (!isObject(value)) return;
  const unknown = Object.keys(value).filter((key) => !allowed.has(key));
  if (unknown.length > 0) {
    addError(errors, `${location} contains unknown field(s): ${unknown.join(", ")}.`);
  }
}

function requireString(value, location, errors) {
  if (typeof value !== "string" || value.trim() === "") {
    addError(errors, `${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

function requireArray(value, location, errors) {
  if (!Array.isArray(value)) {
    addError(errors, `${location} must be an array.`);
    return false;
  }
  return true;
}

function requireNullableString(value, location, errors) {
  if (value === null) return true;
  return requireString(value, location, errors);
}

function validateStringArray(value, location, errors) {
  if (!requireArray(value, location, errors)) return;
  for (const [index, item] of value.entries()) {
    requireString(item, `${location}[${index}]`, errors);
  }
}

function requireBoolean(value, location, errors) {
  if (typeof value !== "boolean") {
    addError(errors, `${location} must be a boolean.`);
    return false;
  }
  return true;
}

function resolveInside(root, relativePath) {
  if (typeof relativePath !== "string" || relativePath.trim() === "" || path.isAbsolute(relativePath)) {
    return null;
  }
  const resolved = path.resolve(root, relativePath);
  const relative = path.relative(root, resolved);
  if (relative === ".." || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) return null;
  return resolved;
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function declaredJavaScriptName(symbol) {
  if (typeof symbol !== "string") return null;
  const value = symbol.trim();
  const declaration = /^(?:export\s+)?(?:async\s+)?(?:function|class)\s+([A-Za-z_$][\w$]*)\b/u.exec(value) ??
    /^(?:export\s+)?(?:const|let|var)\s+([A-Za-z_$][\w$]*)\b/u.exec(value);
  if (declaration) return declaration[1];
  return /^[A-Za-z_$][\w$]*$/u.test(value) ? value : null;
}

function sourceSymbolExists(source, symbol, extension) {
  if (typeof symbol !== "string") return false;
  const name = extension === ".mjs" || extension === ".js"
    ? declaredJavaScriptName(symbol)
    : /^[A-Za-z_][\w]*$/u.test(symbol.trim()) ? symbol.trim() : null;
  if (!name) return false;
  const escaped = escapeRegExp(name);
  if (extension === ".mjs" || extension === ".js") {
    return [
      new RegExp(`^\\s*(?:export\\s+)?(?:async\\s+)?function\\s+${escaped}\\b`, "mu"),
      new RegExp(`^\\s*(?:export\\s+)?(?:const|let|var|class)\\s+${escaped}\\b`, "mu"),
    ].some((pattern) => pattern.test(source));
  }
  return [
    new RegExp(`^\\s*(?:[A-Za-z_][\\w]*\\s+)+${escaped}\\s*\\(`, "mu"),
    new RegExp(`^\\s*#\\s*(?:define|if|ifdef|ifndef)\\b[^\\n]*\\b${escaped}\\b`, "mu"),
  ].some((pattern) => pattern.test(source));
}

function manifestTopLevelKeyExists(filePath, symbol) {
  try {
    const manifest = JSON.parse(fs.readFileSync(filePath, "utf8"));
    return typeof symbol === "string" && /^[A-Za-z_$][\w$]*$/u.test(symbol.trim()) && Object.hasOwn(manifest, symbol.trim());
  } catch {
    return false;
  }
}

function validateEvidenceSymbol(filePath, evidence, location, errors) {
  const extension = path.extname(filePath).toLowerCase();
  const source = fs.readFileSync(filePath, "utf8");
  const exists = evidence.kind === "manifest" && extension === ".json"
    ? manifestTopLevelKeyExists(filePath, evidence.symbol)
    : sourceSymbolExists(source, evidence.symbol, extension);
  if (!exists) {
    addError(errors, `${location}.symbol must identify a declared symbol in ${evidence.path}.`);
  }
}

function validateEvidenceArray(value, location, root, errors) {
  if (!requireArray(value, location, errors)) return;
  for (const [index, evidence] of value.entries()) {
    const evidenceLocation = `${location}[${index}]`;
    if (!isObject(evidence)) {
      addError(errors, `${evidenceLocation} must be an object.`);
      continue;
    }
    validateKeys(evidence, new Set(["kind", "path", "symbol", "claim"]), evidenceLocation, errors);
    if (!evidenceKinds.has(evidence.kind)) {
      addError(errors, `${evidenceLocation}.kind must be source, unit, check, or manifest.`);
    }
    if (!requireString(evidence.path, `${evidenceLocation}.path`, errors)) continue;
    const resolved = resolveInside(root, evidence.path);
    if (!resolved) {
      addError(errors, `${evidenceLocation}.path must stay inside the repository.`);
    } else if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
      addError(errors, `${evidenceLocation}.path does not exist: ${evidence.path}.`);
    } else {
      if (typeof evidence.symbol === "string") validateEvidenceSymbol(resolved, evidence, evidenceLocation, errors);
    }
    requireString(evidence.symbol, `${evidenceLocation}.symbol`, errors);
    requireString(evidence.claim, `${evidenceLocation}.claim`, errors);
  }
}

function validateEdgeEvidenceArray(value, location, root, errors) {
  if (!requireArray(value, location, errors)) return;
  for (const [index, evidence] of value.entries()) {
    const evidenceLocation = `${location}[${index}]`;
    if (!isObject(evidence)) {
      addError(errors, `${evidenceLocation} must be an object.`);
      continue;
    }
    validateKeys(evidence, edgeEvidenceKeys, evidenceLocation, errors);
    if (!evidenceKinds.has(evidence.kind)) {
      addError(errors, `${evidenceLocation}.kind must be source, unit, check, or manifest.`);
    }
    if (!edgeEvidenceRoles.has(evidence.role)) {
      addError(errors, `${evidenceLocation}.role must be toolchain, sysroot, linker, packaging, build, execution, or development.`);
    }
    if (!requireString(evidence.path, `${evidenceLocation}.path`, errors)) continue;
    const resolved = resolveInside(root, evidence.path);
    if (!resolved) {
      addError(errors, `${evidenceLocation}.path must stay inside the repository.`);
    } else if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
      addError(errors, `${evidenceLocation}.path does not exist: ${evidence.path}.`);
    } else {
      if (typeof evidence.symbol === "string") validateEvidenceSymbol(resolved, evidence, evidenceLocation, errors);
    }
    requireString(evidence.symbol, `${evidenceLocation}.symbol`, errors);
    requireString(evidence.claim, `${evidenceLocation}.claim`, errors);
    if (evidence.role === "execution") {
      if (!edgeExecutionModes.has(evidence.executionMode)) {
        addError(errors, `${evidenceLocation}.executionMode must be local, emulator, simulator, or remote-runner for execution evidence.`);
      }
    } else if (evidence.executionMode !== null) {
      addError(errors, `${evidenceLocation}.executionMode must be null for non-execution evidence.`);
    }
  }
}

function validateAxis(value, location, root, errors) {
  if (!isObject(value)) {
    addError(errors, `${location} must be an object.`);
    return { status: undefined, evidence: [] };
  }
  validateKeys(value, new Set(["status", "evidence"]), location, errors);
  if (!AXIS_STATUSES.includes(value.status)) {
    addError(errors, `${location}.status must be pass, partial, or missing.`);
  }
  validateEvidenceArray(value.evidence, `${location}.evidence`, root, errors);
  const evidence = Array.isArray(value.evidence) ? value.evidence : [];
  if (value.status === "pass" && evidence.length === 0) {
    addError(errors, `${location} with status pass requires evidence.`);
  }
  if (value.status === "missing" && evidence.length > 0) {
    addError(errors, `${location} with status missing must have empty evidence.`);
  }
  return { status: value.status, evidence };
}

function validateAxes(axes, axisNames, location, root, errors) {
  if (!isObject(axes)) {
    addError(errors, `${location} must declare all promotion axes; LLVM triple alone is not support evidence.`);
    return new Map();
  }
  const actual = new Set(Object.keys(axes));
  for (const axis of axisNames) {
    if (!Object.hasOwn(axes, axis)) {
      addError(errors, `${location} is missing required axis "${axis}".`);
    }
  }
  for (const axis of actual) {
    if (!axisNames.includes(axis)) addError(errors, `${location} contains unknown axis "${axis}".`);
  }
  const facts = new Map();
  for (const axis of axisNames) {
    facts.set(axis, validateAxis(axes[axis], `${location} axis "${axis}"`, root, errors));
  }
  return facts;
}

function validateBlockers(value, expected, location, errors) {
  if (Array.isArray(value)) {
    for (const [index, blocker] of value.entries()) {
      requireString(blocker, `${location}[${index}]`, errors);
    }
  }
  if (!Array.isArray(value) || !same(value, expected)) {
    addError(errors, `${location} blockers must equal non-pass axes: ${expected.join(", ")}.`);
  }
}

function validateVerificationLevel(row, location, errors) {
  if (row.state === "supported") {
    if (!VERIFICATION_LEVELS.includes(row.verificationLevel)) {
      addError(errors, `${location}.verificationLevel must be one of ${VERIFICATION_LEVELS.join(", ")}.`);
    }
  } else if (row.verificationLevel !== null) {
    addError(errors, `${location}.verificationLevel must be null for state ${row.state}.`);
  }
}

function validateTargetRow(row, index, root, errors) {
  const location = `targets[${index}]`;
  if (!isObject(row)) {
    addError(errors, `${location} must be an object.`);
    return null;
  }
  const id = idFor(row, `targets[${index}]`);
  validateKeys(row, targetKeys, location, errors);
  requireString(row.id, `${location}.id`, errors);
  requireString(row.triple, `${location}.triple`, errors);
  if (!TARGET_GROUPS.includes(row.group)) {
    addError(errors, `${location}.group must be one of ${TARGET_GROUPS.join(", ")}.`);
  }
  if (!isObject(row.targetSpec)) {
    addError(errors, `target "${id}" targetSpec must be an object.`);
  } else {
    validateKeys(row.targetSpec, new Set(["arch", "os", "abi"]), `target "${id}" targetSpec`, errors);
    requireString(row.targetSpec.arch, `target "${id}" targetSpec.arch`, errors);
    requireString(row.targetSpec.os, `target "${id}" targetSpec.os`, errors);
    requireString(row.targetSpec.abi, `target "${id}" targetSpec.abi`, errors);
  }
  if (!TARGET_STATES.includes(row.state)) {
    addError(errors, `target "${id}" state must be one of ${TARGET_STATES.join(", ")}.`);
  }
  validateVerificationLevel(row, `target "${id}"`, errors);
  requireNullableString(row.hostProfile, `target "${id}" hostProfile`, errors);
  requireNullableString(row.artifactKind, `target "${id}" artifactKind`, errors);
  validateStringArray(row.toolchainRoles, `target "${id}" toolchainRoles`, errors);
  validateStringArray(row.sdkCapabilities, `target "${id}" sdkCapabilities`, errors);
  requireNullableString(row.compilerVersionSource, `target "${id}" compilerVersionSource`, errors);
  requireNullableString(row.runtimeVersion, `target "${id}" runtimeVersion`, errors);
  requireNullableString(row.scope, `target "${id}" scope`, errors);
  requireNullableString(row.claim, `target "${id}" claim`, errors);
  const facts = validateAxes(row.axes, TARGET_AXES, `target "${id}"`, root, errors);
  const nonPass = TARGET_AXES.filter((axis) => facts.get(axis)?.status !== "pass");
  validateBlockers(row.blockers, nonPass, `target "${id}"`, errors);
  if (row.state === "supported") {
    for (const axis of TARGET_AXES) {
      if (facts.get(axis)?.status !== "pass") {
        addError(errors, `supported target "${id}" requires axis "${axis}" status pass.`);
      }
      if ((facts.get(axis)?.evidence ?? []).length === 0) {
        addError(errors, `supported target "${id}" axis "${axis}" requires evidence.`);
      }
    }
    if (typeof row.claim !== "string" || row.claim.trim() === "") {
      addError(errors, `supported target "${id}" must declare a claim.`);
    }
  }
  if (row.state === "candidate") {
    if (row.claim !== null) addError(errors, `candidate target "${id}" must not declare a claim.`);
    if (row.scope !== null) addError(errors, `candidate target "${id}" scope must be null.`);
    for (const axis of TARGET_AXES) {
      if (facts.get(axis)?.status !== "missing") {
        addError(errors, `candidate target "${id}" axis "${axis}" must be missing.`);
      }
    }
  }
  if (row.state === "evidence" && row.claim !== null) {
    addError(errors, `evidence target "${id}" must not declare a general claim.`);
  }
  return { id, row, facts };
}

function validateTargetCatalog(value, root, errors) {
  if (!requireArray(value.targets, "targets", errors)) return { current: null, rows: [] };
  const rows = value.targets.map((row, index) => validateTargetRow(row, index, root, errors)).filter(Boolean);
  const seen = new Set();
  const seenTriples = new Set();
  for (const row of rows) {
    if (seen.has(row.row.id)) addError(errors, `targets contains duplicate id "${row.row.id}".`);
    seen.add(row.row.id);
    if (seenTriples.has(row.row.triple)) addError(errors, `targets contains duplicate triple "${row.row.triple}".`);
    seenTriples.add(row.row.triple);
  }
  const current = rows.find((entry) => entry.row.triple === currentTargetTriple) ?? null;
  if (!current) {
    addError(errors, `targets must include current evidence target "${currentTargetTriple}".`);
  } else {
    const row = current.row;
    if (row.state !== "evidence") addError(errors, `current target "${currentTargetTriple}" must have state evidence.`);
    if (row.verificationLevel !== null) addError(errors, `current target "${currentTargetTriple}" verificationLevel must be null.`);
    if (row.scope !== currentTargetScope) addError(errors, `current target "${currentTargetTriple}" scope must be ${currentTargetScope}.`);
    if (row.group !== "desktop-server") addError(errors, `current target "${currentTargetTriple}" must use group desktop-server.`);
    const expectedStatuses = {
      backend: "pass",
      runtime: "partial",
      hostAdapter: "partial",
      sdkProfile: "partial",
      linkerSysrootPackaging: "partial",
      ciEvidence: "partial",
    };
    for (const [axis, status] of Object.entries(expectedStatuses)) {
      if (row.axes?.[axis]?.status !== status) {
        addError(errors, `current target "${currentTargetTriple}" axis "${axis}" must have status ${status}.`);
      }
    }
    const evidencePaths = new Set(TARGET_AXES.flatMap((axis) =>
      (row.axes?.[axis]?.evidence ?? []).map((evidence) => evidence.path)));
    for (const requiredPath of [
      "compiler/seed-c/include/w_seed_mlir0.h",
      "compiler/seed-c/src/w_seed_mlir0.c",
      "compiler/seed-c/tests/test_mlir0.c",
      "compiler/seed-c/tests/hlo1_gate.c",
      "tooling/check-mlir0.mjs",
      "tooling/mlir0-toolchain.json",
    ]) {
      if (!evidencePaths.has(requiredPath)) {
        addError(errors, `current target "${currentTargetTriple}" evidence must include ${requiredPath}.`);
      }
    }
    if (row.compilerVersionSource !== "tooling/mlir0-toolchain.json") {
      addError(errors, `current target "${currentTargetTriple}" compilerVersionSource must use tooling/mlir0-toolchain.json.`);
    }
    if (row.runtimeVersion !== currentRuntimeVersion) {
      addError(errors, `current target "${currentTargetTriple}" runtimeVersion must be ${currentRuntimeVersion}.`);
    }
  }
  const candidateRows = rows.filter((entry) => entry.row.state === "candidate");
  const actualCandidates = candidateRows.map((entry) => ({ triple: entry.row.triple, group: entry.row.group }));
  if (!same(actualCandidates, EXPECTED_CANDIDATES)) {
    addError(errors, "candidate target rows must match the closed W-1507 candidate list and order.");
  }
  if (rows.filter((entry) => entry.row.state === "supported").length !== 0) {
    addError(errors, "current platform reality must have zero supported targets.");
  }
  if (rows.filter((entry) => entry.row.state === "evidence").length !== 1) {
    addError(errors, "current platform reality must have one evidence-only target.");
  }
  return { current, rows };
}

function validateHostRow(row, index, root, errors) {
  const location = `compilerHosts[${index}]`;
  if (!isObject(row)) {
    addError(errors, `${location} must be an object.`);
    return null;
  }
  const id = idFor(row, `compilerHosts[${index}]`);
  const hasTargetFields = ["triple", "group", "targetSpec", "hostProfile", "artifactKind"].some((key) => Object.hasOwn(row, key));
  if (hasTargetFields) addError(errors, `compiler host "${id}" must not contain emitted-target fields.`);
  const allowed = row.kind === "compiler-host-composite" ? currentHostKeys : candidateHostKeys;
  validateKeys(row, allowed, location, errors);
  requireString(row.id, `${location}.id`, errors);
  if (row.kind === "compiler-host-composite") {
    requireString(row.outerHostTriple, `compiler host "${id}" outerHostTriple`, errors);
    requireString(row.toolExecutionTriple, `compiler host "${id}" toolExecutionTriple`, errors);
    requireString(row.mode, `compiler host "${id}" mode`, errors);
    requireBoolean(row.nativeForOuterHost, `compiler host "${id}" nativeForOuterHost`, errors);
    if (row.mode.startsWith("wsl") && row.nativeForOuterHost !== false) {
      addError(errors, `compiler host "${id}" with WSL mode must set nativeForOuterHost=false.`);
    }
    if (row.mode.startsWith("wsl") && !row.outerHostTriple?.includes("windows")) {
      addError(errors, `compiler host "${id}" WSL mode must name a Windows outer host.`);
    }
    if (row.nativeForOuterHost === true && row.mode.startsWith("wsl")) {
      addError(errors, "WSL is never native for a Windows outer host.");
    }
  } else {
    if (row.kind !== "compiler-host") addError(errors, `compiler host "${id}" kind must be compiler-host.`);
    requireString(row.platform, `compiler host "${id}" platform`, errors);
    requireString(row.architecture, `compiler host "${id}" architecture`, errors);
    requireString(row.hostTriple, `compiler host "${id}" hostTriple`, errors);
    if (row.mode !== "native") addError(errors, `compiler host "${id}" candidate mode must be native.`);
  }
  if (!TARGET_STATES.includes(row.state)) addError(errors, `compiler host "${id}" state must be one of ${TARGET_STATES.join(", ")}.`);
  requireNullableString(row.scope, `compiler host "${id}" scope`, errors);
  requireNullableString(row.claim, `compiler host "${id}" claim`, errors);
  const facts = validateAxes(row.axes, HOST_AXES, `compiler host "${id}"`, root, errors);
  const nonPass = HOST_AXES.filter((axis) => facts.get(axis)?.status !== "pass");
  validateBlockers(row.blockers, nonPass, `compiler host "${id}"`, errors);
  if (row.state === "candidate") {
    if (row.claim !== null) addError(errors, `candidate compiler host "${id}" must not declare a claim.`);
    if (row.scope !== null) addError(errors, `candidate compiler host "${id}" scope must be null.`);
    for (const axis of HOST_AXES) {
      if (facts.get(axis)?.status !== "missing") addError(errors, `candidate compiler host "${id}" axis "${axis}" must be missing.`);
    }
  }
  if (row.kind === "compiler-host" && row.state !== "candidate") {
    addError(errors, `native compiler host candidate "${id}" must have state candidate.`);
  }
  if (row.state === "supported") {
    for (const axis of HOST_AXES) {
      if (facts.get(axis)?.status !== "pass") addError(errors, `supported compiler host "${id}" requires axis "${axis}" status pass.`);
      if ((facts.get(axis)?.evidence ?? []).length === 0) addError(errors, `supported compiler host "${id}" axis "${axis}" requires evidence.`);
    }
  }
  return { id, row, facts };
}

function validateHostCatalog(value, root, errors) {
  if (!requireArray(value.compilerHosts, "compilerHosts", errors)) return { current: null, rows: [] };
  if (value.compilerHosts === value.targets) addError(errors, "compiler host and target collections must remain separate.");
  const rows = value.compilerHosts.map((row, index) => validateHostRow(row, index, root, errors)).filter(Boolean);
  const seen = new Set();
  for (const entry of rows) {
    if (seen.has(entry.row.id)) addError(errors, `compilerHosts contains duplicate id "${entry.row.id}".`);
    seen.add(entry.row.id);
  }
  const compositeRows = rows.filter((entry) => entry.row.kind === "compiler-host-composite");
  if (compositeRows.length !== 1) {
    addError(errors, "compilerHosts must contain exactly one composite host evidence row.");
  }
  const current = compositeRows[0] ?? null;
  if (!current) {
    addError(errors, "compilerHosts must include the current composite WSL2 host evidence.");
  } else {
    const row = current.row;
    if (row.outerHostTriple !== "x86_64-pc-windows-msvc") addError(errors, "current compiler host outerHostTriple must be x86_64-pc-windows-msvc.");
    if (row.toolExecutionTriple !== currentTargetTriple) addError(errors, "current compiler host toolExecutionTriple must match the evidence target execution triple.");
    if (row.mode !== "wsl2") addError(errors, "current compiler host mode must be wsl2.");
    if (row.nativeForOuterHost !== false) addError(errors, "current compiler host nativeForOuterHost must be false.");
    if (row.state !== "evidence") addError(errors, "current compiler host state must be evidence.");
    if (row.scope !== "dev-only") addError(errors, "current compiler host scope must be dev-only.");
    const expectedStatuses = {
      nativeToolchain: "partial",
      toolchainBundle: "partial",
      releasePackaging: "missing",
      ciEvidence: "partial",
    };
    for (const [axis, status] of Object.entries(expectedStatuses)) {
      if (row.axes?.[axis]?.status !== status) addError(errors, `current compiler host axis "${axis}" must have status ${status}.`);
    }
    if (row.nativeForOuterHost === false && row.axes?.nativeToolchain?.status === "pass") {
      addError(errors, "current non-native composite compiler host nativeToolchain must not have status pass.");
    }
  }
  const candidates = rows.filter((entry) => entry.row.kind === "compiler-host");
  const actual = candidates.map((entry) => ({
    platform: entry.row.platform,
    architecture: entry.row.architecture,
    hostTriple: entry.row.hostTriple,
  }));
  if (!same(actual, EXPECTED_NATIVE_HOSTS)) addError(errors, "native compiler host candidates must match the closed W-1507 host list and order.");
  return { current, rows };
}

function validateNativeToolchainPlans(value, errors) {
  if (!requireArray(value.nativeToolchainPlans, "nativeToolchainPlans", errors)) return;
  const seen = new Set();
  for (const [index, plan] of value.nativeToolchainPlans.entries()) {
    const location = `nativeToolchainPlans[${index}]`;
    if (!isObject(plan)) {
      addError(errors, `${location} must be an object.`);
      continue;
    }
    validateKeys(plan, planKeys, location, errors);
    if (seen.has(plan.id)) addError(errors, `nativeToolchainPlans contains duplicate id "${plan.id}".`);
    seen.add(plan.id);
    requireString(plan.id, `${location}.id`, errors);
    validateStringArray(plan.architectures, `${location}.architectures`, errors);
    if (!["linux", "windows", "macos"].includes(plan.platform)) addError(errors, `${location}.platform must be linux, windows, or macos.`);
    if (!same(plan.architectures, ["x86_64", "aarch64"])) addError(errors, `${location}.architectures must list x86_64 and aarch64.`);
    if (plan.status !== "planned") addError(errors, `${location}.status must be planned.`);
    if (!isObject(plan.source)) {
      addError(errors, `${location}.source must be an object.`);
    } else {
      validateKeys(plan.source, new Set(["repository", "url", "tag", "tagObject", "commit"]), `${location}.source`, errors);
      requireString(plan.source.repository, `${location}.source.repository`, errors);
      requireString(plan.source.url, `${location}.source.url`, errors);
      requireString(plan.source.tag, `${location}.source.tag`, errors);
      requireString(plan.source.tagObject, `${location}.source.tagObject`, errors);
      requireString(plan.source.commit, `${location}.source.commit`, errors);
    }
    if (!isObject(plan.source) ||
        plan.source.repository !== "llvm-project" ||
        plan.source.url !== "https://github.com/llvm/llvm-project" ||
        plan.source.tag !== successorToolchainTag ||
        plan.source.tagObject !== successorToolchainTagObject ||
        plan.source.commit !== successorToolchainCommit) {
      addError(errors, `${location}.source must pin ${successorToolchainTag} with tag object ${successorToolchainTagObject} and commit ${successorToolchainCommit}.`);
    }
    validateStringArray(plan.projects, `${location}.projects`, errors);
    if (!same(plan.projects, ["MLIR", "Clang", "LLD"])) addError(errors, `${location}.projects must list MLIR, Clang, and LLD.`);
    if (!isObject(plan.configuration)) {
      addError(errors, `${location}.configuration must be an object.`);
    } else {
      validateKeys(plan.configuration, new Set(["buildType", "generator", "targets", "linkerDrivers"]), `${location}.configuration`, errors);
      requireString(plan.configuration.buildType, `${location}.configuration.buildType`, errors);
      requireString(plan.configuration.generator, `${location}.configuration.generator`, errors);
      validateStringArray(plan.configuration.targets, `${location}.configuration.targets`, errors);
      validateStringArray(plan.configuration.linkerDrivers, `${location}.configuration.linkerDrivers`, errors);
    }
    if (!isObject(plan.configuration) || plan.configuration.buildType !== "Release" || plan.configuration.generator !== "Ninja" ||
        !same(plan.configuration.targets, ["X86", "AArch64"]) || !same(plan.configuration.linkerDrivers, ["lld-link", "ld.lld", "ld64.lld"])) {
      addError(errors, `${location}.configuration must use Release, Ninja, X86, AArch64, and explicit LLD linker drivers.`);
    }
    if (!isObject(plan.outputs)) {
      addError(errors, `${location}.outputs must be an object.`);
    } else {
      validateKeys(plan.outputs, new Set(["status", "pin", "artifacts", "sha256", "sbom", "provenance", "signing"]), `${location}.outputs`, errors);
      requireString(plan.outputs.status, `${location}.outputs.status`, errors);
      requireString(plan.outputs.pin, `${location}.outputs.pin`, errors);
      validateStringArray(plan.outputs.artifacts, `${location}.outputs.artifacts`, errors);
      requireString(plan.outputs.sha256, `${location}.outputs.sha256`, errors);
      requireString(plan.outputs.sbom, `${location}.outputs.sbom`, errors);
      requireString(plan.outputs.provenance, `${location}.outputs.provenance`, errors);
      requireString(plan.outputs.signing, `${location}.outputs.signing`, errors);
    }
    if (!isObject(plan.outputs) || plan.outputs.status !== "planned" || plan.outputs.pin !== "required" ||
        !same(plan.outputs.artifacts, ["mlir-opt", "mlir-translate", "clang", "lld", "llvm-config"]) ||
        plan.outputs.sha256 !== "required" || plan.outputs.sbom !== "required" ||
        plan.outputs.provenance !== "required" || plan.outputs.signing !== "required") {
      addError(errors, `${location}.outputs must require mlir-opt, mlir-translate, clang, lld, llvm-config, SHA256, SBOM, provenance, and signing.`);
    }
    if (!isObject(plan.validation)) {
      addError(errors, `${location}.validation must be an object.`);
    } else {
      validateKeys(plan.validation, new Set(["ci", "smoke", "promotion"]), `${location}.validation`, errors);
      requireString(plan.validation.ci, `${location}.validation.ci`, errors);
      requireString(plan.validation.smoke, `${location}.validation.smoke`, errors);
      requireString(plan.validation.promotion, `${location}.validation.promotion`, errors);
    }
    if (!isObject(plan.validation) || plan.validation.ci !== "required" || plan.validation.smoke !== "required" || plan.validation.promotion !== "blocked-until-all-evidence") {
      addError(errors, `${location}.validation must require CI and smoke before promotion.`);
    }
    if (!Array.isArray(plan.gaps) || plan.gaps.length === 0) addError(errors, `${location}.gaps must be explicit and non-empty.`);
    validateStringArray(plan.gaps, `${location}.gaps`, errors);
    if (!Array.isArray(plan.gaps) || !plan.gaps.includes(dependencyCurrencyPromotionBlocker)) {
      addError(errors, `${location}.gaps must include ${dependencyCurrencyPromotionBlocker}.`);
    }
  }
  const platforms = (value.nativeToolchainPlans ?? []).map((plan) => plan.platform);
  if (!same(platforms, ["linux", "windows", "macos"])) addError(errors, "native toolchain plans must contain Linux, Windows, and macOS in order.");
}

function validateExternalToolchainCandidates(value, errors) {
  if (!requireArray(value.externalToolchainCandidates, "externalToolchainCandidates", errors)) return;
  if (value.externalToolchainCandidates.length !== 1) {
    addError(errors, "externalToolchainCandidates must contain exactly one evaluation-only record.");
  }
  for (const [index, candidate] of value.externalToolchainCandidates.entries()) {
    const location = `externalToolchainCandidates[${index}]`;
    if (!isObject(candidate)) {
      addError(errors, `${location} must be an object.`);
      continue;
    }
    validateKeys(candidate, externalToolchainCandidateKeys, location, errors);
    requireString(candidate.id, `${location}.id`, errors);
    requireString(candidate.status, `${location}.status`, errors);
    requireString(candidate.license, `${location}.license`, errors);
    requireString(candidate.observedRelease, `${location}.observedRelease`, errors);
    requireString(candidate.llvmTag, `${location}.llvmTag`, errors);
    validateStringArray(candidate.publishedHostTriples, `${location}.publishedHostTriples`, errors);
    validateStringArray(candidate.evidenceCapabilities, `${location}.evidenceCapabilities`, errors);
    validateStringArray(candidate.limitations, `${location}.limitations`, errors);
    if (candidate.id !== expectedExternalToolchainCandidate.id) {
      addError(errors, `${location}.id must be ${expectedExternalToolchainCandidate.id}.`);
    }
    if (candidate.status !== expectedExternalToolchainCandidate.status) {
      addError(errors, `${location}.status must be evaluation-only; promotion and support claims are forbidden.`);
    }
    if (candidate.license !== expectedExternalToolchainCandidate.license) {
      addError(errors, `${location}.license must be Apache-2.0.`);
    }
    if (candidate.observedRelease !== expectedExternalToolchainCandidate.observedRelease) {
      addError(errors, `${location}.observedRelease must be 2026.08.11.`);
    }
    if (candidate.llvmTag !== expectedExternalToolchainCandidate.llvmTag) {
      addError(errors, `${location}.llvmTag must be llvmorg-22.1.8.`);
    }
    if (!same(candidate.publishedHostTriples, expectedExternalToolchainCandidate.publishedHostTriples)) {
      addError(errors, `${location}.publishedHostTriples must list exactly the six published host triples.`);
    }
    if (!same(candidate.evidenceCapabilities, expectedExternalToolchainCandidate.evidenceCapabilities)) {
      addError(errors, `${location}.evidenceCapabilities must list the closed capability set.`);
    }
    if (!same(candidate.limitations, expectedExternalToolchainCandidate.limitations)) {
      addError(errors, `${location}.limitations must preserve the explicit non-promotion limitations.`);
    }
  }
}

function deterministicCrossEdgeId(hostRef, targetRef) {
  return `edge-${hostRef}-to-${targetRef}`;
}

function validateCrossEdgeEvidence(value, location, root, errors) {
  validateEdgeEvidenceArray(value, `${location}.evidence`, root, errors);
  return Array.isArray(value) ? value : [];
}

function edgeExpectedBlockers(edge, host, target) {
  const blockers = [];
  if (host?.row?.state !== "supported") blockers.push("hostEndpoint");
  if (target?.row?.state !== "supported") blockers.push("targetEndpoint");
  const roles = new Set((edge.evidence ?? []).map((evidence) => evidence.role));
  if (!["toolchain", "sysroot", "linker", "packaging"].every((role) => roles.has(role))) {
    blockers.push("toolchainSysrootLinkerPackaging");
  }
  if (!["build", "execution"].every((role) => roles.has(role))) blockers.push("buildExecution");
  return blockers;
}

function developmentEdgeExpectedBlockers(edge, host, target) {
  return [developmentEdgeBlockers[0], ...edgeExpectedBlockers(edge, host, target)];
}

function validateCrossCompilation(value, targetRows, hostRows, root, errors) {
  if (!isObject(value)) {
    addError(errors, "crossCompilation must be an object.");
    return;
  }
  validateKeys(value, crossCompilationKeys, "crossCompilation", errors);
  if (!same(value.baselineHosts, PRIMARY_HOST_REFS)) {
    addError(errors, "crossCompilation.baselineHosts must list the three primary native host references in order.");
  }
  if (!same(value.baselineTargets, PRIMARY_TARGET_REFS)) {
    addError(errors, "crossCompilation.baselineTargets must list the three primary emitted target references in order.");
  }
  validateStringArray(value.baselineHosts, "crossCompilation.baselineHosts", errors);
  validateStringArray(value.baselineTargets, "crossCompilation.baselineTargets", errors);

  const hostsById = new Map(hostRows.map((entry) => [entry.row.id, entry]));
  const targetsById = new Map(targetRows.map((entry) => [entry.row.id, entry]));
  const baselineHostSet = new Set(PRIMARY_HOST_REFS);
  const baselineTargetSet = new Set(PRIMARY_TARGET_REFS);
  const edges = requireArray(value.edges, "crossCompilation.edges", errors) ? value.edges : [];
  const expectedPairs = new Set(
    PRIMARY_HOST_REFS.flatMap((hostRef) => PRIMARY_TARGET_REFS.map((targetRef) => `${hostRef}\u0000${targetRef}`)),
  );
  const seenPairs = new Set();
  const seenIds = new Set();
  let hasOffDiagonalEdge = false;
  for (const [index, edge] of edges.entries()) {
    const location = `crossCompilation.edges[${index}]`;
    if (!isObject(edge)) {
      addError(errors, `${location} must be an object.`);
      continue;
    }
    validateKeys(edge, crossEdgeKeys, location, errors);
    requireString(edge.id, `${location}.id`, errors);
    requireString(edge.hostRef, `${location}.hostRef`, errors);
    requireString(edge.targetRef, `${location}.targetRef`, errors);
    if (!CROSS_COMPILATION_STATES.includes(edge.state)) {
      addError(errors, `${location}.state must be one of ${CROSS_COMPILATION_STATES.join(", ")}.`);
    }
    if (edge.id !== deterministicCrossEdgeId(edge.hostRef, edge.targetRef)) {
      addError(errors, `${location}.id must be deterministic from hostRef and targetRef.`);
    }
    if (!baselineHostSet.has(edge.hostRef)) addError(errors, `${location}.hostRef must reference a primary native compiler host.`);
    if (!baselineTargetSet.has(edge.targetRef)) addError(errors, `${location}.targetRef must reference a primary emitted target.`);
    const pair = `${edge.hostRef}\u0000${edge.targetRef}`;
    if (seenPairs.has(pair)) addError(errors, `crossCompilation.edges contains duplicate hostRef/targetRef pair "${edge.hostRef} -> ${edge.targetRef}".`);
    seenPairs.add(pair);
    if (seenIds.has(edge.id)) addError(errors, `crossCompilation.edges contains duplicate id "${edge.id}".`);
    seenIds.add(edge.id);
    const host = hostsById.get(edge.hostRef);
    const target = targetsById.get(edge.targetRef);
    if (!host) addError(errors, `${location}.hostRef does not resolve to a compiler host row.`);
    if (!target) addError(errors, `${location}.targetRef does not resolve to an emitted target row.`);
    if (host?.row?.kind !== "compiler-host") addError(errors, `${location}.hostRef must resolve to a native compiler host, not a composite or target row.`);
    if (target && host && host.row.hostTriple !== target.row.triple) hasOffDiagonalEdge = true;
    const evidence = validateCrossEdgeEvidence(edge.evidence, location, root, errors);
    if (edge.state === "supported") {
      if (host?.row?.state !== "supported") addError(errors, `supported cross-compilation edge "${edge.id}" requires a supported compiler host endpoint.`);
      if (target?.row?.state !== "supported") addError(errors, `supported cross-compilation edge "${edge.id}" requires a supported emitted target endpoint.`);
      if (evidence.length === 0) addError(errors, `supported cross-compilation edge "${edge.id}" requires build and execution evidence.`);
      const roles = new Set(evidence.map((entry) => entry.role));
      for (const role of ["toolchain", "sysroot", "linker", "packaging", "build", "execution"]) {
        if (!roles.has(role)) addError(errors, `supported cross-compilation edge "${edge.id}" requires ${role} evidence.`);
      }
    }
    if (edge.state === "evidence" && evidence.length === 0) {
      addError(errors, `evidence cross-compilation edge "${edge.id}" requires evidence.`);
    }
    const expectedBlockers = edgeExpectedBlockers(edge, host, target);
    if (!Array.isArray(edge.blockers) || !same(edge.blockers, expectedBlockers)) {
      addError(errors, `${location}.blockers must equal ${expectedBlockers.join(", ") || "none"}.`);
    }
    if (Array.isArray(edge.blockers)) {
      for (const [blockerIndex, blocker] of edge.blockers.entries()) {
        requireString(blocker, `${location}.blockers[${blockerIndex}]`, errors);
      }
    }
  }
  for (const pair of expectedPairs) {
    if (!seenPairs.has(pair)) {
      const [hostRef, targetRef] = pair.split("\u0000");
      addError(errors, `crossCompilation.edges is missing baseline edge ${hostRef} -> ${targetRef}.`);
    }
  }
  for (const pair of seenPairs) {
    if (!expectedPairs.has(pair)) addError(errors, "crossCompilation.edges contains an edge outside the closed 3x3 baseline matrix.");
  }
  if (edges.length !== expectedPairs.size) {
    addError(errors, "crossCompilation.edges must contain exactly the nine baseline host-to-target edges.");
  }
  if (edges.length === expectedPairs.size && !hasOffDiagonalEdge) {
    addError(errors, "crossCompilation.edges must include cross-host or cross-target edges; an only-self matrix is invalid.");
  }

  const development = requireArray(value.developmentEvidence, "crossCompilation.developmentEvidence", errors)
    ? value.developmentEvidence
    : [];
  if (development.length !== 1) addError(errors, "crossCompilation.developmentEvidence must contain exactly one current WSL evidence edge.");
  const developmentIds = new Set();
  for (const [index, edge] of development.entries()) {
    const location = `crossCompilation.developmentEvidence[${index}]`;
    if (!isObject(edge)) {
      addError(errors, `${location} must be an object.`);
      continue;
    }
    validateKeys(edge, developmentEdgeKeys, location, errors);
    requireString(edge.id, `${location}.id`, errors);
    requireString(edge.hostRef, `${location}.hostRef`, errors);
    requireString(edge.targetRef, `${location}.targetRef`, errors);
    if (!CROSS_COMPILATION_STATES.includes(edge.state)) addError(errors, `${location}.state must be one of ${CROSS_COMPILATION_STATES.join(", ")}.`);
    requireBoolean(edge.nativeHost, `${location}.nativeHost`, errors);
    validateCrossEdgeEvidence(edge.evidence, location, root, errors);
    if (Array.isArray(edge.blockers)) {
      for (const [blockerIndex, blocker] of edge.blockers.entries()) {
        requireString(blocker, `${location}.blockers[${blockerIndex}]`, errors);
      }
    }
    if (developmentIds.has(edge.id)) addError(errors, `crossCompilation.developmentEvidence contains duplicate id "${edge.id}".`);
    developmentIds.add(edge.id);
    if (edge.id !== developmentEdgeId) addError(errors, `${location}.id must be ${developmentEdgeId}.`);
    if (edge.hostRef !== "host-x86_64-pc-windows-msvc-wsl2") addError(errors, `${location}.hostRef must reference the current WSL2 composite host.`);
    if (edge.targetRef !== "target-x86_64-unknown-linux-gnu") addError(errors, `${location}.targetRef must reference the current evidence target.`);
    if (edge.state !== "evidence") addError(errors, `${location}.state must be evidence.`);
    if (edge.nativeHost !== false) addError(errors, `${location}.nativeHost must be false for WSL evidence.`);
    if (!hostsById.has(edge.hostRef) || hostsById.get(edge.hostRef)?.row?.kind !== "compiler-host-composite") {
      addError(errors, `${location}.hostRef must resolve to the WSL composite compiler host.`);
    }
    if (!targetsById.has(edge.targetRef)) addError(errors, `${location}.targetRef must resolve to the current emitted target.`);
    if (edge.evidence?.length === 0) addError(errors, `${location}.evidence must not be empty.`);
    const evidenceRoles = new Set((edge.evidence ?? []).map((evidence) => evidence.role));
    for (const role of ["development", "toolchain", "build", "execution"]) {
      if (!evidenceRoles.has(role)) addError(errors, `${location}.evidence must include ${role} role.`);
    }
    const host = hostsById.get(edge.hostRef);
    const target = targetsById.get(edge.targetRef);
    const expectedBlockers = developmentEdgeExpectedBlockers(edge, host, target);
    if (!Array.isArray(edge.blockers) || !same(edge.blockers, expectedBlockers)) {
      addError(errors, `${location}.blockers must equal ${expectedBlockers.join(", ") || "none"}.`);
    }
  }
}

function validatePolicy(value, errors) {
  if (!isObject(value.policy)) {
    addError(errors, "policy must be an object.");
    return;
  }
  const policy = value.policy;
  validateKeys(policy, new Set(["referenceBreadth", "llvmTripleIsInsufficient", "wslIsNotWindowsNative", "promotionAxes", "dependencyCurrency"]), "policy", errors);
  if (!isObject(policy.referenceBreadth)) {
    addError(errors, "policy.referenceBreadth must be an object.");
  } else {
    const referenceBreadth = policy.referenceBreadth;
    validateKeys(referenceBreadth, new Set(["goal", "sources", "observed", "importsRustTiers"]), "policy.referenceBreadth", errors);
    if (Array.isArray(referenceBreadth.sources)) {
      for (const [index, source] of referenceBreadth.sources.entries()) {
        const location = `policy.referenceBreadth.sources[${index}]`;
        if (!isObject(source)) {
          addError(errors, `${location} must be an object.`);
          continue;
        }
        validateKeys(source, new Set(["name", "url"]), location, errors);
        requireString(source.name, `${location}.name`, errors);
        requireString(source.url, `${location}.url`, errors);
      }
    }
    if (!Array.isArray(referenceBreadth.sources)) {
      addError(errors, "policy.referenceBreadth.sources must be an array.");
    }
    if (referenceBreadth.goal !== "at-least-rust-breadth") addError(errors, "policy.referenceBreadth.goal must be at-least-rust-breadth.");
    if (referenceBreadth.observed !== "2026-08-31") addError(errors, "policy.referenceBreadth.observed must be 2026-08-31.");
    if (referenceBreadth.importsRustTiers !== false) addError(errors, "policy.referenceBreadth.importsRustTiers must be false.");
    const expectedSources = [
      ["Rust platform support", "https://doc.rust-lang.org/rustc/platform-support.html"],
      ["Rust target tier policy", "https://doc.rust-lang.org/rustc/target-tier-policy.html"],
      ["MLIR getting started", "https://mlir.llvm.org/getting_started/"],
      ["LLVM getting started", "https://llvm.org/docs/GettingStarted.html"],
      ["LLVM CMake target selection", "https://llvm.org/docs/CMake.html"],
    ];
    const actualSources = (referenceBreadth.sources ?? []).map((source) => [source.name, source.url]);
    if (!same(actualSources, expectedSources)) addError(errors, "policy.referenceBreadth.sources must use the five official source URLs in order.");
  }
  validateKeys(policy.promotionAxes, new Set(["target", "compilerHost"]), "policy.promotionAxes", errors);
  validateStringArray(policy.promotionAxes?.target, "policy.promotionAxes.target", errors);
  validateStringArray(policy.promotionAxes?.compilerHost, "policy.promotionAxes.compilerHost", errors);
  if (policy.llvmTripleIsInsufficient !== true) addError(errors, "policy.llvmTripleIsInsufficient must be true.");
  if (policy.wslIsNotWindowsNative !== true) addError(errors, "policy.wslIsNotWindowsNative must be true.");
  if (!isObject(policy.promotionAxes) || !same(policy.promotionAxes.target, TARGET_AXES) || !same(policy.promotionAxes.compilerHost, HOST_AXES)) {
    addError(errors, "policy.promotionAxes must match the target and compiler-host axis lists.");
  }
  if (!isObject(policy.dependencyCurrency)) {
    addError(errors, "policy.dependencyCurrency must be an object.");
  } else {
    const currency = policy.dependencyCurrency;
    const location = "policy.dependencyCurrency";
    validateKeys(currency, dependencyCurrencyKeys, location, errors);
    requireString(currency.currentEvidenceVersion, `${location}.currentEvidenceVersion`, errors);
    requireString(currency.currentEvidenceCurrencyStatus, `${location}.currentEvidenceCurrencyStatus`, errors);
    requireString(currency.futureNativePlanPolicy, `${location}.futureNativePlanPolicy`, errors);
    requireString(currency.successorVersion, `${location}.successorVersion`, errors);
    requireString(currency.successorTag, `${location}.successorTag`, errors);
    requireString(currency.successorTagObject, `${location}.successorTagObject`, errors);
    requireString(currency.successorCommit, `${location}.successorCommit`, errors);
    requireString(currency.promotionBlocker, `${location}.promotionBlocker`, errors);
    if (currency.currentEvidenceVersion !== pinnedToolchainVersion) {
      addError(errors, `${location}.currentEvidenceVersion must remain ${pinnedToolchainVersion} for the factual MLIR0 evidence.`);
    }
    if (currency.currentEvidenceCurrencyStatus !== currentEvidenceCurrencyStatus) {
      addError(errors, `${location}.currentEvidenceCurrencyStatus must be ${currentEvidenceCurrencyStatus}.`);
    }
    if (currency.futureNativePlanPolicy !== futureNativePlanPolicy) {
      addError(errors, `${location}.futureNativePlanPolicy must be ${futureNativePlanPolicy}.`);
    }
    if (currency.successorVersion !== successorToolchainVersion) {
      addError(errors, `${location}.successorVersion must be ${successorToolchainVersion}.`);
    }
    if (currency.successorTag !== successorToolchainTag) {
      addError(errors, `${location}.successorTag must be ${successorToolchainTag}.`);
    }
    if (currency.successorTagObject !== successorToolchainTagObject) {
      addError(errors, `${location}.successorTagObject must be ${successorToolchainTagObject}.`);
    }
    if (currency.successorCommit !== successorToolchainCommit) {
      addError(errors, `${location}.successorCommit must be ${successorToolchainCommit}.`);
    }
    if (currency.promotionBlocker !== dependencyCurrencyPromotionBlocker) {
      addError(errors, `${location}.promotionBlocker must be ${dependencyCurrencyPromotionBlocker}.`);
    }
  }
}

function loadJson(filePath, label, errors) {
  try {
    return JSON.parse(fs.readFileSync(filePath, "utf8"));
  } catch (error) {
    addError(errors, `${label} is not valid JSON: ${error.message}.`);
    return null;
  }
}

function validateDependencyCurrencyCrossCheck(value, root, errors) {
  const catalogPath = path.join(root, "tooling", "dependency-currency.json");
  if (!fs.existsSync(catalogPath)) {
    addError(errors, "tooling/dependency-currency.json is required as the currency source.");
    return;
  }
  const catalog = loadJson(catalogPath, "tooling/dependency-currency.json", errors);
  if (!catalog || !Array.isArray(catalog.dependencies)) return;
  const byId = new Map(catalog.dependencies.map((entry) => [entry.id, entry]));
  const mlir = byId.get("mlir0-llvm-clang");
  const selected = mlir?.selected;
  const policy = value.policy?.dependencyCurrency;
  if (!selected || !policy) return;
  const fields = ["version", "tag", "tagObject", "commit"];
  for (const field of fields) {
    const key = field === "version" ? "successorVersion" : `successor${field[0].toUpperCase()}${field.slice(1)}`;
    if (policy[key] !== selected[field]) {
      addError(errors, `platform support currency disagrees with dependency-currency.json for successor ${field}.`);
    }
  }
  for (const plan of value.nativeToolchainPlans ?? []) {
    if (plan.source?.tag !== selected.tag || plan.source?.tagObject !== selected.tagObject || plan.source?.commit !== selected.commit) {
      addError(errors, `native plan ${plan.id} must match the selected dependency-currency successor.`);
    }
  }
}

function validateMlir0Manifest(value, current, root, errors) {
  if (!isObject(value.crossChecks?.mlir0Toolchain)) {
    addError(errors, "crossChecks.mlir0Toolchain must be an object.");
    return;
  }
  const reference = value.crossChecks.mlir0Toolchain;
  validateKeys(reference, new Set(["path", "targetTriple", "hostEvidence", "windowsNative", "currencyStatus"]), "crossChecks.mlir0Toolchain", errors);
  const manifestPath = resolveInside(root, reference.path);
  if (!manifestPath || !fs.existsSync(manifestPath)) {
    addError(errors, "crossChecks.mlir0Toolchain.path must point to tooling/mlir0-toolchain.json.");
    return;
  }
  if (reference.path !== "tooling/mlir0-toolchain.json") addError(errors, "crossChecks.mlir0Toolchain.path must be tooling/mlir0-toolchain.json.");
  if (reference.targetTriple !== currentTargetTriple) addError(errors, "mlir0 toolchain manifest mismatch: cross-check targetTriple must match the current evidence target.");
  if (reference.hostEvidence !== "wsl-linux") addError(errors, "mlir0 toolchain manifest mismatch: hostEvidence must be wsl-linux.");
  if (reference.windowsNative !== false) addError(errors, "mlir0 toolchain manifest mismatch: windowsNative must be false.");
  if (reference.currencyStatus !== currentEvidenceCurrencyStatus) addError(errors, `mlir0 toolchain manifest mismatch: currencyStatus must be ${currentEvidenceCurrencyStatus}.`);
  const manifest = loadJson(manifestPath, "tooling/mlir0-toolchain.json", errors);
  if (!manifest) return;
  if (manifest.$schema !== mlir0ToolchainSchema || manifest.version !== 1 || manifest.status !== "pinned") {
    addError(errors, "mlir0 toolchain manifest mismatch: schema, version, or pinned status is invalid.");
  }
  if (manifest.artifact?.schema !== currentRuntimeVersion ||
      manifest.artifact?.scope !== currentArtifactScope) {
    addError(errors, "mlir0 toolchain manifest mismatch: artifact schema or scope disagrees with the current MLIR0 evidence.");
  }
  if (manifest.target?.triple !== currentTargetTriple || manifest.target?.triple !== current?.row?.triple) {
    addError(errors, "mlir0 toolchain manifest mismatch: target triple does not match the current evidence row.");
  }
  if (manifest.hostEvidence !== "wsl-linux" || manifest.windowsNative !== false) {
    addError(errors, "mlir0 toolchain manifest mismatch: hostEvidence or windowsNative disagrees with the current row.");
  }
  if (!(manifest.toolchain && ["mlir", "llvm", "clang"].every((role) => manifest.toolchain[role] === pinnedToolchainVersion))) {
    addError(errors, "mlir0 toolchain manifest mismatch: MLIR, LLVM, and Clang must remain pinned at 20.1.2.");
  }
  const pipeline = manifest.pipeline;
  const recipeValid = Array.isArray(pipeline) && pipeline.length === 3 &&
    pipeline[0]?.tool === "mlir-opt" && pipeline[1]?.tool === "mlir-translate" && pipeline[2]?.tool === "clang" &&
    Array.isArray(pipeline[2]?.args) && pipeline[2].args.includes(`--target=${currentTargetTriple}`);
  if (!recipeValid) addError(errors, "mlir0 toolchain manifest mismatch: the existing MLIR0 recipe must keep mlir-opt, mlir-translate, and clang for the current target.");
  if (manifest.hostModes?.linux !== "direct" || manifest.hostModes?.windows !== "wsl:Ubuntu") {
    addError(errors, "mlir0 toolchain manifest mismatch: Linux must be direct and Windows must use WSL Ubuntu.");
  }
  if (manifest.emittedTargets?.supported !== undefined || !same(manifest.emittedTargets?.evidence, [currentTargetTriple])) {
    addError(errors, "mlir0 toolchain manifest mismatch: emittedTargets must record evidence only for the current target.");
  }
}

export function validatePlatformSupport(value, { root = repositoryRoot, checkManifest = true } = {}) {
  const errors = [];
  if (!isObject(value)) {
    return { errors: ["platform support record must be an object."], targets: [], compilerHosts: [] };
  }
  validateKeys(value, new Set(["$schema", "version", "status", "observed", "benchmarkDisposition", "policy", "crossChecks", "targets", "compilerHosts", "crossCompilation", "nativeToolchainPlans", "externalToolchainCandidates"]), "platform support record", errors);
  if (value.$schema !== PLATFORM_SUPPORT_SCHEMA) addError(errors, `platform support record.$schema must be ${PLATFORM_SUPPORT_SCHEMA}.`);
  if (value.version !== 1) addError(errors, "platform support record.version must be 1.");
  if (value.status !== "operational-evidence") addError(errors, "platform support record.status must be operational-evidence.");
  if (value.observed !== "2026-08-31") addError(errors, "platform support record.observed must be 2026-08-31.");
  if (!isObject(value.benchmarkDisposition) || value.benchmarkDisposition.kind !== "not-applicable" ||
      value.benchmarkDisposition.reason !== "This record gates metadata, projection, and policy. It has no runtime or performance measurement.") {
    addError(errors, "benchmarkDisposition must be not-applicable with the metadata, projection, and policy reason.");
  }
  if (isObject(value.benchmarkDisposition)) {
    validateKeys(value.benchmarkDisposition, new Set(["kind", "reason"]), "benchmarkDisposition", errors);
  }
  if (isObject(value.crossChecks)) {
    validateKeys(value.crossChecks, new Set(["mlir0Toolchain"]), "crossChecks", errors);
  } else {
    addError(errors, "crossChecks must be an object.");
  }
  validatePolicy(value, errors);
  validateDependencyCurrencyCrossCheck(value, root, errors);
  const targetResult = validateTargetCatalog(value, root, errors);
  const hostResult = validateHostCatalog(value, root, errors);
  validateCrossCompilation(value.crossCompilation, targetResult.rows, hostResult.rows, root, errors);
  validateExternalToolchainCandidates(value, errors);
  validateNativeToolchainPlans(value, errors);
  if (checkManifest) validateMlir0Manifest(value, targetResult.current, root, errors);
  return {
    errors,
    targets: targetResult.rows,
    currentTarget: targetResult.current,
  };
}

export function loadPlatformSupport({ root = repositoryRoot } = {}) {
  const filePath = path.join(root, "tooling", "platform-support.json");
  return loadJson(filePath, "tooling/platform-support.json", []);
}

function markdownCell(value) {
  return String(value ?? "—").replaceAll("|", "\\|").replaceAll("\n", "<br>");
}

function markdownTable(headers, rows) {
  const output = [
    `| ${headers.join(" | ")} |`,
    `| ${headers.map(() => "---").join(" | ")} |`,
  ];
  for (const row of rows) output.push(`| ${row.map(markdownCell).join(" | ")} |`);
  return output;
}

function blockersFor(row) {
  return row.blockers?.length ? row.blockers.join(", ") : "none";
}

function titleForGroup(group) {
  return group === "webassembly" ? "WebAssembly" : group[0].toUpperCase() + group.slice(1);
}

function evidenceSummary(row) {
  return TARGET_AXES.map((axis) => `${axis}: ${row.axes?.[axis]?.status ?? "missing"}`).join("; ");
}

function crossHostLabel(host) {
  return host?.hostTriple ?? host?.outerHostTriple ?? "—";
}

function crossTargetLabel(target) {
  return target?.triple ?? "—";
}

function crossEdgeCell(edge) {
  if (!edge) return "missing";
  return `${edge.state}<br>blockers: ${blockersFor(edge)}`;
}

export function renderPlatformSupport(value, { root = repositoryRoot } = {}) {
  const targets = Array.isArray(value.targets) ? value.targets : [];
  const hosts = Array.isArray(value.compilerHosts) ? value.compilerHosts : [];
  const crossCompilation = isObject(value.crossCompilation) ? value.crossCompilation : {};
  const plans = Array.isArray(value.nativeToolchainPlans) ? value.nativeToolchainPlans : [];
  const externalCandidates = Array.isArray(value.externalToolchainCandidates)
    ? value.externalToolchainCandidates
    : [];
  const supported = targets.filter((target) => target.state === "supported");
  const evidence = targets.filter((target) => target.state === "evidence");
  const candidates = targets.filter((target) => target.state === "candidate");
  const hostById = new Map(hosts.map((host) => [host.id, host]));
  const targetById = new Map(targets.map((target) => [target.id, target]));
  const baselineHosts = Array.isArray(crossCompilation.baselineHosts) ? crossCompilation.baselineHosts : [];
  const baselineTargets = Array.isArray(crossCompilation.baselineTargets) ? crossCompilation.baselineTargets : [];
  const crossEdges = Array.isArray(crossCompilation.edges) ? crossCompilation.edges : [];
  const developmentEdges = Array.isArray(crossCompilation.developmentEvidence)
    ? crossCompilation.developmentEvidence
    : [];
  const crossEdgeByPair = new Map(crossEdges.map((edge) => [`${edge.hostRef}\u0000${edge.targetRef}`, edge]));
  const supportedCrossEdges = crossEdges.filter((edge) => edge.state === "supported").length;
  const manifest = (() => {
    try {
      return JSON.parse(fs.readFileSync(path.join(root, "tooling", "mlir0-toolchain.json"), "utf8"));
    } catch {
      return null;
    }
  })();
  const lines = [
    "# Platform support",
    "",
    "<!-- Generated by tooling/platform-support.mjs. Edit tooling/platform-support.json. -->",
    "",
    "Current reality:",
    "",
    `- Supported targets: ${supported.length}.`,
    `- Evidence-only targets: ${evidence.length}${evidence.length > 0 ? ` (${evidence.map((target) => `\`${target.triple}\``).join(", ")})` : ""}.`,
    "- WSL is not Windows native.",
    "",
    "This catalog records evidence and promotion gates. It does not define language semantics.",
    "An LLVM triple is not sufficient evidence for W target support.",
    "",
    "## Compiler hosts",
    "",
    "Compiler hosts are separate from emitted targets.",
    "",
    ...markdownTable(
      ["ID", "Outer host", "Tool execution", "Mode", "State", "Scope", "Blockers"],
      hosts.map((host) => [
        `\`${host.id}\``,
        host.outerHostTriple ?? host.hostTriple,
        host.toolExecutionTriple ?? "—",
        host.mode,
        host.state,
        host.scope ?? "—",
        blockersFor(host),
      ]),
    ),
    "",
    "WSL execution is development evidence. It does not promote the Windows outer host to native support.",
    "",
    "## Cross-compilation baseline",
    "",
    "The primary baseline has three native compiler hosts and three emitted targets.",
    "The matrix contains all nine host-to-target edges, including self edges.",
    `Supported edges: ${supportedCrossEdges}/${baselineHosts.length * baselineTargets.length}. Every baseline edge remains a candidate until endpoint, toolchain, SDK, sysroot, linker, packaging, CI, build, and execution evidence passes.`,
    "",
    ...markdownTable(
      ["Host \\ Target", ...baselineTargets.map((targetRef) => crossTargetLabel(targetById.get(targetRef)))],
      baselineHosts.map((hostRef) => [
        crossHostLabel(hostById.get(hostRef)),
        ...baselineTargets.map((targetRef) => crossEdgeCell(crossEdgeByPair.get(`${hostRef}\u0000${targetRef}`))),
      ]),
    ),
    "",
    "Linux-to-Windows edges require explicit SDK, sysroot, object, and linker evidence.",
    "Apple targets require Apple SDK, license, and provenance evidence. The catalog does not assume redistribution.",
    "Remote execution evidence is separate from build evidence.",
    "",
    "### Development evidence edges",
    "",
    "The WSL edge is outside the 3x3 native baseline and does not count as a native Windows edge.",
    "",
    ...markdownTable(
      ["ID", "Host", "Target", "State", "Native host", "Evidence", "Blockers"],
      developmentEdges.map((edge) => [
        `\`${edge.id}\``,
        crossHostLabel(hostById.get(edge.hostRef)),
        crossTargetLabel(targetById.get(edge.targetRef)),
        edge.state,
        edge.nativeHost === false ? "false" : "true",
        `${edge.evidence?.length ?? 0} record(s): ${edge.evidence?.map((evidence) => evidence.role).join(", ") ?? "—"}`,
        blockersFor(edge),
      ]),
    ),
    "",
    "## Target evidence",
    "",
    ...markdownTable(
      ["Triple", "State", "Verification", "Scope", "Host profile", "Artifact", "Compiler/runtime", "Axes", "Blockers"],
      evidence.map((target) => [
        `\`${target.triple}\``,
        target.state,
        target.verificationLevel ?? "null",
        target.scope,
        target.hostProfile,
        target.artifactKind,
        `${target.compilerVersionSource ?? "—"} / ${target.runtimeVersion ?? "—"}`,
        evidenceSummary(target),
        blockersFor(target),
      ]),
    ),
    "",
    `The current evidence target is \`${currentTargetTriple}\`. Its scope is \`${currentTargetScope}\`.`,
    "The evidence covers the MLIR0 source, unit, focal check, and pinned manifest.",
    "It does not claim a general W target, SDK, packaging, or official CI.",
    "",
    "## Target candidates",
    "",
    "Candidates have no support claim. Each candidate needs all six promotion axes.",
  ];
  for (const group of TARGET_GROUPS) {
    lines.push("", `### ${titleForGroup(group)}`, "", ...markdownTable(
      ["Triple", "State", "Verification", "Blockers"],
      candidates.filter((target) => target.group === group).map((target) => [
        `\`${target.triple}\``,
        target.state,
        target.verificationLevel ?? "null",
        blockersFor(target),
      ]),
    ));
  }
  lines.push(
    "",
    "## Promotion axes",
    "",
    "A target becomes supported only when every target axis passes with evidence.",
    "A supported target also needs one verification level.",
    "",
    ...markdownTable(
      ["Collection", "Axis", "Pass requirement"],
      [
        ...TARGET_AXES.map((axis) => ["Target", `\`${axis}\``, "Pass status and non-empty evidence"]),
        ...HOST_AXES.map((axis) => ["Compiler host", `\`${axis}\``, "Pass status and non-empty evidence"]),
      ],
    ),
    "",
    "Every non-pass axis appears in the row blocker list.",
    "Device targets use the same six target axes. They do not use N/A.",
    "",
    "## Native toolchain plans",
    "",
    "These plans are future work. They are not native host or target support evidence.",
    "",
    ...markdownTable(
      ["ID", "Platform", "Architectures", "Source", "Projects", "Build", "Outputs", "Validation", "Gaps"],
      plans.map((plan) => [
        `\`${plan.id ?? "—"}\``,
        plan.platform,
        plan.architectures?.join(", "),
        `${plan.source?.repository ?? "—"} @ ${plan.source?.tag ?? "—"}<br>commit: ${plan.source?.commit ?? "—"}`,
        plan.projects?.join(", "),
        `${plan.configuration?.buildType ?? "—"} / ${plan.configuration?.generator ?? "—"} / ${plan.configuration?.targets?.join(", ") ?? "—"} / ${plan.configuration?.linkerDrivers?.join(", ") ?? "—"}`,
        `${plan.outputs?.artifacts?.join(", ") ?? "—"} (${plan.outputs?.sha256 ?? "—"}, ${plan.outputs?.sbom ?? "—"}, ${plan.outputs?.provenance ?? "—"}, ${plan.outputs?.signing ?? "—"})`,
        `${plan.validation?.ci ?? "—"} CI, ${plan.validation?.smoke ?? "—"} smoke, ${plan.validation?.promotion ?? "—"}`,
        plan.gaps?.join("<br>") ?? "—",
      ]),
    ),
    "",
    `Each future plan pins ${successorToolchainTag} at commit ${successorToolchainCommit} and builds MLIR, Clang, and LLD with Release and Ninja.`,
    `The ${dependencyCurrencyPromotionBlocker} blocker remains until exact outputs, provenance, and host evidence exist.`,
    "Promotion waits for pinned outputs, SHA256, SBOM, provenance, signing, CI, and smoke evidence.",
    "",
    "## External toolchain candidates",
    "External records are evaluation-only. They cannot promote a W target, compiler host, or cross-compilation edge.",
    "",
    ...markdownTable(
      ["ID", "Status", "License", "Observed release", "LLVM tag", "Published hosts", "Evidence capabilities", "Limitations"],
      externalCandidates.map((candidate) => [
        `\`${candidate.id ?? "—"}\``,
        candidate.status ?? "—",
        candidate.license ?? "—",
        candidate.observedRelease ?? "—",
        candidate.llvmTag ?? "—",
        candidate.publishedHostTriples?.join("<br>") ?? "—",
        candidate.evidenceCapabilities?.join("<br>") ?? "—",
        candidate.limitations?.join("<br>") ?? "—",
      ]),
    ),
    "",
    "The portable MLIR record is a possible bootstrap, mirror, or rebuild input for a future Windows-native bundle; it is not W trust, SBOM, provenance, or cross-compilation evidence.",
    "",
    "## Policy",
    "",
    `- Observed: ${value.observed ?? "—"}.`,
    `- Reference breadth goal: \`${value.policy?.referenceBreadth?.goal ?? "—"}\`.`,
    `- Rust target tiers imported: ${value.policy?.referenceBreadth?.importsRustTiers === false ? "no" : "yes"}.`,
    `- Current evidence version: \`${value.policy?.dependencyCurrency?.currentEvidenceVersion ?? "—"}\` (${value.policy?.dependencyCurrency?.currentEvidenceCurrencyStatus ?? "—"}).`,
    `- Future native plan policy: \`${value.policy?.dependencyCurrency?.futureNativePlanPolicy ?? "—"}\`; successor: \`${value.policy?.dependencyCurrency?.successorTag ?? "—"}\` at \`${value.policy?.dependencyCurrency?.successorCommit ?? "—"}\`.`,
    `- Build and provenance blocker: \`${value.policy?.dependencyCurrency?.promotionBlocker ?? "—"}\`.`,
    "- The breadth goal is comparative. It is not an inherited Rust claim or tier snapshot.",
    "",
    ...markdownTable(
      ["Reference", "Official source"],
      (value.policy?.referenceBreadth?.sources ?? []).map((source) => [`${source.name}`, `[link](${source.url})`]),
    ),
    "",
    "## MLIR0 manifest cross-check",
    "",
    `The current row references [${value.crossChecks?.mlir0Toolchain?.path ?? "tooling/mlir0-toolchain.json"}](${value.crossChecks?.mlir0Toolchain?.path ?? "tooling/mlir0-toolchain.json"}).`,
    `The manifest target is \`${manifest?.target?.triple ?? "unknown"}\` with MLIR, LLVM, and Clang ${manifest?.toolchain?.mlir ?? "unknown"}.`,
    `This ${manifest?.toolchain?.mlir ?? "unknown"} version is factual current evidence and is marked \`${value.crossChecks?.mlir0Toolchain?.currencyStatus ?? "—"}\`; it is not the intended future native-plan release.`,
    "The manifest records WSL Linux evidence and no Windows native evidence.",
    "",
    "## Benchmark disposition",
    "",
    "Not applicable. This catalog gates metadata, projection, and policy.",
    "It records no runtime or performance result.",
    "",
  );
  return `${lines.join("\n").replace(/\n+$/u, "")}\n`;
}

function main() {
  const action = process.argv.slice(2);
  if (action.length !== 1 || !["--write", "--check"].includes(action[0])) {
    process.stderr.write("Usage: bun tooling/platform-support.mjs --write|--check\n");
    process.exitCode = 2;
    return;
  }
  const value = loadPlatformSupport();
  const validation = validatePlatformSupport(value);
  if (validation.errors.length > 0) {
    process.stderr.write(`${validation.errors.join("\n")}\n`);
    process.exitCode = 1;
    return;
  }
  const expected = renderPlatformSupport(value);
  if (action[0] === "--write") {
    fs.writeFileSync(platformSupportDocumentPath, expected, "utf8");
    process.stdout.write("Platform support: wrote PLATFORM-SUPPORT.md.\n");
    return;
  }
  if (!fs.existsSync(platformSupportDocumentPath)) {
    process.stderr.write("PLATFORM-SUPPORT.md is missing; run with --write.\n");
    process.exitCode = 1;
    return;
  }
  const actual = fs.readFileSync(platformSupportDocumentPath, "utf8");
  if (actual !== expected) {
    process.stderr.write("PLATFORM-SUPPORT.md is stale; run with --write.\n");
    process.exitCode = 1;
    return;
  }
  process.stdout.write("Platform support: source and projection are current.\n");
}

if (import.meta.main) main();
