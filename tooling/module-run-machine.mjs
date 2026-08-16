import { createHash } from "node:crypto";

/**
 * Host-only RU0 oracle for the current module-run workflow.
 * It validates state transitions and provenance. It does not compile, resolve
 * a live registry, fetch bytes, or execute W source.
 */
export class ModuleRunError extends Error {
  constructor(code, facts = {}) {
    super(code);
    this.code = code;
    this.facts = facts;
  }
}

const ROOT_KINDS = new Set(["package", "workspace"]);
const CONTEXT_KINDS = new Set(["package", "workspace", "ephemeral"]);
const BASELINE_CHANNELS = ["process-args"];
const BASELINE_AUTHORITIES = [".stdio"];
const RESOLUTION_SCHEMA = "w.resolution/1";
const RESOLVER_SCHEMA = "w.resolver/1";

function clone(value) {
  return value === undefined ? undefined : structuredClone(value);
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") {
    return Object.fromEntries(
      Object.keys(value).sort().map((key) => [key, canonical(value[key])]),
    );
  }
  return value;
}

export function moduleRunDigest(tag, value) {
  const bytes = typeof value === "string"
    ? value
    : JSON.stringify(canonical(value));
  return `sha256:${createHash("sha256").update(`${tag}\0${bytes}`, "utf8").digest("hex")}`;
}

function text(value) {
  return typeof value === "string" ? value : "";
}

function fail(code, facts = {}) {
  throw new ModuleRunError(code, facts);
}

function requirePhase(state, phase, code) {
  if (state.phase !== phase) fail(code);
}

function normalizeImports(imports) {
  if (!Array.isArray(imports)) fail("importsMalformed");
  return imports.map((item, index) => {
    if (!item || typeof item !== "object" || typeof item.path !== "string" || item.path.length === 0) {
      fail("importMalformed", { index });
    }
    return {
      path: item.path,
      digest: item.digest ?? null,
      external: item.external === true,
    };
  });
}

function sourceDigest(sourceText) {
  if (typeof sourceText !== "string") fail("sourceBytesRequired");
  return moduleRunDigest("w-module-source-v1", sourceText.replace(/\r\n?/g, "\n"));
}

function parseEvidenceFor(operation, digest, entry, imports) {
  if (operation.parseEvidence === undefined) return null;
  const evidence = operation.parseEvidence;
  if (!evidence || evidence.sourceDigest !== digest) fail("parseEvidenceSourceMismatch");
  if (evidence.entry !== (entry ?? null)) fail("parseEvidenceEntryMismatch");
  if (JSON.stringify(evidence.imports ?? []) !== JSON.stringify(imports.map((item) => item.path))) {
    fail("parseEvidenceImportsMismatch");
  }
  if (evidence.entryForm !== (entry === null ? "missing" : "explicit")) {
    fail("parseEvidenceEntryFormMismatch");
  }
  return clone(evidence);
}

function parseModule(state, operation) {
  if (state.phase !== "empty") fail("sourceAlreadyParsed");
  if (operation.sourceKind !== undefined && operation.sourceKind !== "module") {
    fail("sourceKindRejected");
  }
  if (operation.scriptHeader !== undefined || operation.legacyHeaderKind === "script") {
    fail("scriptHeaderRejected");
  }
  if (operation.implicitEntryBody === true || operation.entryForm === "implicit") {
    fail("implicitEntryBodyRejected");
  }
  const digest = sourceDigest(operation.sourceText);
  const imports = normalizeImports(operation.imports ?? []);
  const entry = operation.entry === undefined || operation.entry === null
    ? null
    : text(operation.entry);
  if (operation.entry !== undefined && operation.entry !== null && entry.length === 0) {
    fail("entryMalformed");
  }
  const evidence = parseEvidenceFor(operation, digest, entry, imports);
  state.source = {
    path: text(operation.path) || "<memory>",
    kind: "module",
    sourceDigest: digest,
    textDigest: digest,
    entry,
    entryForm: entry === null ? "missing" : "explicit",
    imports,
    moduleHeader: operation.moduleHeader === undefined ? null : clone(operation.moduleHeader),
    parseEvidence: evidence,
  };
  state.phase = "parsed";
  state.trace.push({
    event: "parseModule",
    sourceDigest: digest,
    entry,
    entryForm: state.source.entryForm,
    imports: imports.map((item) => item.path),
  });
}

function selectContext(state, operation) {
  requirePhase(state, "parsed", "sourceNotParsed");
  const mode = text(operation.mode) || "ephemeral";
  if (!CONTEXT_KINDS.has(mode)) fail("contextKindRejected", { mode });
  if (operation.rootKind !== undefined && !ROOT_KINDS.has(operation.rootKind)) {
    fail("moduleRootRejected", { rootKind: operation.rootKind });
  }
  if (mode === "ephemeral" && operation.rootKind !== undefined) {
    fail("moduleRootRejected", { rootKind: operation.rootKind, mode });
  }
  state.context = {
    mode,
    reason: mode === "ephemeral" ? "outside-project" : "owner-selected",
    owner: mode === "ephemeral" ? null : (operation.owner ?? mode),
    deployment: operation.deployment ?? null,
  };
  state.phase = "context";
  state.trace.push({ event: "selectContext", mode, reason: state.context.reason });
}

function resolveRoots(state, operation) {
  requirePhase(state, "context", "contextNotSelected");
  const rootKind = state.context.mode === "ephemeral"
    ? "local"
    : (operation.rootKind ?? state.context.mode);
  if (operation.rootName === "package.lock" || operation.rootName === "deployment" || operation.rootKind === "lock" || operation.rootKind === "deployment") {
    fail("moduleRootRejected", { rootKind: operation.rootKind ?? operation.rootName });
  }
  if (state.context.mode !== "ephemeral" && rootKind !== state.context.mode) {
    fail("moduleRootMismatch", { expected: state.context.mode, actual: rootKind });
  }
  const local = text(operation.localRoot) || (state.context.mode === "ephemeral" ? "local" : `${rootKind}.w`);
  state.roots = {
    kind: rootKind,
    local,
    canonical: text(operation.canonicalRoot) || local,
    physicalDisplay: state.source.path,
    owner: state.context.owner,
    withinRoot: true,
  };
  state.phase = "roots";
  state.trace.push({ event: "resolveRoots", kind: rootKind, local, identityExcludesPhysicalPath: true });
}

function validateImports(state, operation) {
  requirePhase(state, "roots", "rootsNotResolved");
  if (operation.recursiveScan === true || operation.cwdScan === true || operation.pathScan === true || operation.environmentScan === true) {
    fail("ambientImportRejected");
  }
  const imports = normalizeImports(operation.imports ?? state.source.imports);
  if (JSON.stringify(imports) !== JSON.stringify(state.source.imports)) fail("importEvidenceMismatch");
  if (state.context.mode === "ephemeral" && imports.some((item) => item.external)) {
    fail("externalDependencyInEphemeral");
  }
  state.imports = {
    validated: true,
    modules: imports,
    paths: imports.map((item) => item.path),
    digests: imports.map((item) => item.digest).filter(Boolean),
  };
  state.phase = "imports";
  state.trace.push({ event: "validateImports", count: imports.length, paths: state.imports.paths });
}

function resolutionPayload(resolution) {
  if (!resolution || typeof resolution !== "object") fail("resolutionMissing");
  if (resolution.schema !== RESOLUTION_SCHEMA || resolution.resolver !== RESOLVER_SCHEMA) {
    fail("resolutionFormatInvalid");
  }
  if (!Array.isArray(resolution.contexts) || !Array.isArray(resolution.packages)) {
    fail("resolutionPayloadIncomplete");
  }
  if (resolution.rootKind !== undefined && !ROOT_KINDS.has(resolution.rootKind)) {
    fail("moduleRootRejected", { rootKind: resolution.rootKind });
  }
  return canonical(resolution);
}

function validateResolution(state, operation) {
  requirePhase(state, "imports", "importsNotValidated");
  if (operation.packageLock !== undefined || operation.lock !== undefined || operation.deploymentRoot !== undefined) {
    fail("legacyRootFieldRejected");
  }
  if (state.context.mode === "ephemeral") {
    if (operation.resolution !== undefined && operation.resolution !== null) fail("resolutionInEphemeral");
    state.resolution = { validated: true, digest: null, selectedContext: null, packages: [], deployments: [] };
    state.phase = "resolved";
    state.trace.push({ event: "validateResolution", source: "ephemeral", digest: null });
    return;
  }
  const payload = resolutionPayload(operation.resolution);
  const selectedContext = payload.contexts.find((item) => item.kind === state.context.mode || item.root === state.context.mode);
  if (!selectedContext) fail("resolutionContextMissing", { mode: state.context.mode });
  if (selectedContext.root === ".product(\"script\")" || selectedContext.root === "package.lock" || selectedContext.root === "deployment") {
    fail("legacyRootFieldRejected", { root: selectedContext.root });
  }
  const digest = moduleRunDigest("w-resolution-v1", payload);
  if (operation.resolutionDigest !== undefined && operation.resolutionDigest !== digest) fail("resolutionDigestMismatch");
  state.resolution = {
    validated: true,
    digest,
    rootDigest: digest,
    selectedContext: clone(selectedContext),
    contexts: clone(payload.contexts),
    packages: clone(payload.packages),
    deployments: clone(operation.deployments ?? payload.deployments ?? []),
  };
  state.phase = "resolved";
  state.trace.push({ event: "validateResolution", digest, context: state.context.mode, packageCount: payload.packages.length });
}

function build(state, operation) {
  requirePhase(state, "resolved", "resolutionNotValidated");
  const target = text(operation.target) || "x86_64-unknown-linux-gnu";
  const hostProfile = text(operation.hostProfile) || "native-process@1";
  const toolchainDigest = text(operation.toolchainDigest) || moduleRunDigest("w-toolchain-v1", hostProfile);
  const localModules = state.imports.modules.filter((item) => !item.external).map((item) => ({ path: item.path, digest: item.digest }));
  const recipe = {
    sourceDigest: state.source.sourceDigest,
    localModules,
    context: state.context.mode,
    root: state.roots.kind,
    resolutionDigest: state.resolution.digest,
    target,
    hostProfile,
    toolchainDigest,
    deployment: state.context.deployment,
  };
  const identity = moduleRunDigest("w-module-product-v1", recipe);
  state.product = {
    kind: "module",
    identity,
    recipeKey: identity,
    rootSourceDigest: state.source.sourceDigest,
    selectedContext: state.resolution.selectedContext,
    target,
    hostProfile,
    toolchainDigest,
    entry: state.source.entry,
    physicalRoot: state.roots.local,
  };
  state.phase = "built";
  state.trace.push({ event: "buildModule", identity, target, hostProfile, physicalPathExcluded: true });
}

function runEntry(state, operation) {
  requirePhase(state, "built", "moduleNotBuilt");
  if (state.source.entry === null) fail("defaultEntryMissing");
  const selected = operation.entry ?? "default";
  if (selected === "default" && state.source.entry !== "default" && state.source.entry !== "unnamed") {
    fail("defaultEntryMissing");
  }
  if (selected !== "default" && selected !== state.source.entry) {
    fail("entrySelectionMismatch", { selected, available: state.source.entry });
  }
  state.run = {
    entry: selected,
    entryForm: "explicit",
    args: clone(operation.args ?? []),
    outcome: operation.outcome ?? "success",
  };
  state.phase = "ran";
  state.trace.push({ event: "runEntry", entry: selected, entryForm: "explicit", outcome: state.run.outcome });
}

function cleanup(state, operation) {
  if (!new Set(["built", "ran", "cleaned"]).has(state.phase)) fail("nothingToCleanup");
  if (operation.hiddenState === true || operation.hiddenArtifacts === true || operation.complete === false) fail("cleanupIncomplete");
  state.cleanup = { done: true, hiddenArtifacts: [], manifest: null, lock: null };
  state.phase = "cleaned";
  state.trace.push({ event: "cleanup", hiddenState: false });
}

function explainContext(state) {
  if (!state.roots.local) fail("rootsNotResolved");
  state.context.explanation = {
    mode: state.context.mode,
    reason: state.context.reason,
    root: state.roots,
    selectedContext: state.resolution.selectedContext,
    sourceDigest: state.source.sourceDigest,
    resolutionDigest: state.resolution.digest,
    recipeKey: state.product?.recipeKey ?? null,
  };
  state.trace.push({ event: "contextExplanation", mode: state.context.mode, resolutionDigest: state.resolution.digest });
}

export function validateModuleRunOperation(operation) {
  return Boolean(operation && typeof operation === "object" && typeof operation.op === "string" && [
    "parseModule",
    "selectContext",
    "resolveRoots",
    "validateImports",
    "validateResolution",
    "buildModule",
    "runEntry",
    "cleanup",
    "contextExplanation",
  ].includes(operation.op));
}

export function runModuleRunProgram(operations) {
  if (!Array.isArray(operations) || operations.length === 0) {
    return { status: "rejected", code: "operationsMissing", operation: 0, state: initialState() };
  }
  const state = initialState();
  for (const [index, operation] of operations.entries()) {
    try {
      if (!validateModuleRunOperation(operation)) fail("operationMalformed");
      switch (operation.op) {
        case "parseModule": parseModule(state, operation); break;
        case "selectContext": selectContext(state, operation); break;
        case "resolveRoots": resolveRoots(state, operation); break;
        case "validateImports": validateImports(state, operation); break;
        case "validateResolution": validateResolution(state, operation); break;
        case "buildModule": build(state, operation); break;
        case "runEntry": runEntry(state, operation); break;
        case "cleanup": cleanup(state, operation); break;
        case "contextExplanation": explainContext(state); break;
        default: fail("operationMalformed");
      }
    } catch (error) {
      const code = error instanceof ModuleRunError ? error.code : "oracleFailure";
      state.trace.push({ event: "reject", code, operation: operation.op });
      return { status: "rejected", code, operation: index, state };
    }
  }
  return { status: "accepted", state };
}

function initialState() {
  return {
    phase: "empty",
    source: { path: null, kind: null, sourceDigest: null, textDigest: null, entry: null, entryForm: null, imports: [], moduleHeader: null, parseEvidence: null },
    context: { mode: null, reason: null, owner: null, deployment: null, explanation: null },
    roots: { kind: null, local: null, canonical: null, physicalDisplay: null, owner: null, withinRoot: null },
    imports: { validated: false, modules: [], paths: [], digests: [] },
    resolution: { validated: false, digest: null, rootDigest: null, selectedContext: null, contexts: [], packages: [], deployments: [] },
    product: null,
    run: null,
    cleanup: { done: false, hiddenArtifacts: [], manifest: null, lock: null },
    trace: [],
  };
}
