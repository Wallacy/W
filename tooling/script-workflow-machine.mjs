import { createHash } from "node:crypto";

/**
 * Host-only PYN1 oracle. It models admission and provenance. It does not
 * compile, resolve a live registry, fetch bytes, or execute W source.
 */
export class ScriptWorkflowError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

const HEADER_FIELDS = new Set(["edition", "dependencies", "lock", "requires"]);
const REQUIREMENTS = new Set([
  ".filesystem",
  ".network",
  ".clock",
  ".random",
  ".storage",
]);
const ALLOWED_USES = new Set(["product", "build", "test", "benchmark"]);
// Channels and authorities are different contracts.  Keep them separate so
// a process-args channel is never mistaken for an authority grant.
const BASELINE_CHANNELS = ["process-args"];
const BASELINE_AUTHORITIES = [".stdio"];
const LOCK_SCHEMA = "w.package-lock/1";
const LOCK_RESOLVER = "w.resolver/1";

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") {
    return Object.fromEntries(
      Object.keys(value)
        .sort()
        .map((key) => [key, canonical(value[key])]),
    );
  }
  return value;
}

export function scriptDigest(tag, value) {
  const bytes = typeof value === "string" ? value : JSON.stringify(canonical(value));
  return `sha256:${createHash("sha256").update(`${tag}\0${bytes}`, "utf8").digest("hex")}`;
}

function lockPayload(lock) {
  if (!lock || typeof lock !== "object") fail("lockObjectMissing");
  const {
    digest: _digest,
    rootDigest: _rootDigest,
    // Artifact/CAS/action/handle records are P0 sidecars. They are verified by
    // the host, but are not part of the package.lock root payload.
    artifacts: _artifacts,
    cas: _cas,
    actionOutputs: _actionOutputs,
    requiredHandles: _requiredHandles,
    signature: _signature,
    signatureValid: _signatureValid,
    ...payload
  } = lock;
  if (payload.schema !== LOCK_SCHEMA || payload.resolver !== LOCK_RESOLVER) {
    fail("invalidLockFormat");
  }
  if (!Array.isArray(payload.contexts) || !Array.isArray(payload.packages)) {
    fail("lockPayloadIncomplete");
  }
  // P0 canonicalization sorts contexts and package identities. It does not
  // invent fields or defaults while hashing.
  payload.contexts = payload.contexts
    .map((context) => {
      const normalized = { ...context };
      if (Array.isArray(normalized.features)) normalized.features = [...normalized.features].sort();
      if (Array.isArray(normalized.targetVariants)) normalized.targetVariants = [...normalized.targetVariants].sort();
      if (Array.isArray(normalized.nodes)) normalized.nodes = [...normalized.nodes].sort();
      if (Array.isArray(normalized.rootEdges)) normalized.rootEdges = [...normalized.rootEdges].sort((left, right) => `${left.alias}\0${left.id}`.localeCompare(`${right.alias}\0${right.id}`));
      return canonical(normalized);
    })
    .sort((left, right) => JSON.stringify(left).localeCompare(JSON.stringify(right)));
  payload.packages = payload.packages
    .map((pkg) => {
      const normalized = { ...pkg };
      if (Array.isArray(normalized.dependencies)) normalized.dependencies = [...normalized.dependencies].sort((left, right) => `${left.alias}\0${left.id}`.localeCompare(`${right.alias}\0${right.id}`));
      return canonical(normalized);
    })
    .sort((left, right) => String(left.id).localeCompare(String(right.id)));
  return canonical(payload);
}

export function lockRootDigest(lock) {
  return scriptDigest("w-package-lock-v1", lockPayload(lock));
}

const SCRIPT_ROOT = '.product("script")';
const DEFAULT_RECIPE_OWNER = SCRIPT_ROOT;

function parseEvidence(state, operation, header) {
  const evidence = operation.parseEvidence;
  if (!evidence || typeof evidence !== "object") fail("parseEvidenceMissing");
  const sourceDigest = state.source.rootDigest ?? null;
  if ((evidence.sourceDigest ?? null) !== sourceDigest) fail("parseEvidenceSourceMismatch");
  const headerFacts = evidence.headerFacts ?? null;
  if (header === null) {
    if (headerFacts !== null) fail("parseEvidenceHeaderMismatch");
  } else {
    if (headerFacts === null || JSON.stringify(canonical(normalizeFields(headerFacts))) !== JSON.stringify(canonical(header))) {
      fail("parseEvidenceHeaderMismatch");
    }
  }
  const expectedEntry = operation.entry ?? null;
  if ((evidence.entry ?? null) !== expectedEntry) fail("parseEvidenceEntryMismatch");
  if (Boolean(evidence.topLevelExecution) !== Boolean(operation.topLevelExecution === true)) {
    fail("parseEvidenceTopLevelMismatch");
  }
  if (Boolean(evidence.hasHeader) !== (header !== null)) fail("parseEvidenceHeaderMismatch");
  return {
    sourceDigest,
    headerFacts: header === null ? null : clone(header),
    entry: expectedEntry,
    topLevelExecution: operation.topLevelExecution === true,
    hasHeader: header !== null,
  };
}

function resultParseEvidence(resultSourceDigest, resultHeader, entry, topLevelExecution, evidence) {
  if (!evidence || typeof evidence !== "object") fail("resultParseEvidenceMissing");
  if ((evidence.sourceDigest ?? null) !== resultSourceDigest) fail("resultParseEvidenceSourceMismatch");
  const headerFacts = evidence.headerFacts ?? null;
  if (headerFacts === null || JSON.stringify(canonical(normalizeFields(headerFacts))) !== JSON.stringify(canonical(resultHeader))) {
    fail("resultParseEvidenceHeaderMismatch");
  }
  if ((evidence.entry ?? null) !== (entry ?? null)) fail("resultParseEvidenceEntryMismatch");
  if (Boolean(evidence.topLevelExecution) !== Boolean(topLevelExecution)) fail("resultParseEvidenceTopLevelMismatch");
  if (evidence.hasHeader !== true) fail("resultParseEvidenceHeaderMismatch");
  return {
    sourceDigest: resultSourceDigest,
    headerFacts: clone(resultHeader),
    entry: entry ?? null,
    topLevelExecution: Boolean(topLevelExecution),
    hasHeader: true,
  };
}

function artifactRecordDigest(facts) {
  return scriptDigest("w-script-artifact-record-v1", {
    lockRootDigest: facts.lockRootDigest,
    nodeId: facts.nodeId,
    metadataDigest: facts.metadataDigest,
    contentDigest: facts.contentDigest,
    target: facts.target,
    use: facts.use,
    artifactDigest: facts.artifactDigest,
    toolchainDigest: facts.toolchainDigest,
    hostProfile: facts.hostProfile,
    recipeOwner: facts.recipeOwner,
    authority: facts.authority,
    signatureRequired: facts.signatureRequired,
    signatureEvidence: facts.signatureEvidence,
  });
}

function handleRecordDigest(facts) {
  return scriptDigest("w-script-handle-record-v1", {
    lockRootDigest: facts.lockRootDigest,
    owner: facts.owner,
    metadataDigest: facts.metadataDigest,
    name: facts.name,
    capability: facts.capability,
    contract: facts.contract,
  });
}

function actionOutputRecordDigest(facts) {
  return scriptDigest("w-script-action-output-record-v1", {
    lockRootDigest: facts.lockRootDigest,
    owner: facts.owner,
    actionDigest: facts.actionDigest,
    outputDigest: facts.outputDigest,
    policy: facts.policy,
    provenanceDigest: facts.provenanceDigest,
  });
}

function clone(value) {
  return value === undefined ? undefined : JSON.parse(JSON.stringify(value));
}

function fail(code) {
  throw new ScriptWorkflowError(code);
}

function requireParsed(state) {
  if (state.phase === "empty") fail("sourceNotParsed");
}

function requireContext(state) {
  requireParsed(state);
  if (!state.context.mode) fail("contextNotSelected");
}

function requireRoots(state) {
  requireContext(state);
  if (!state.roots.local) fail("rootsNotResolved");
}

function requireImports(state) {
  requireRoots(state);
  if (!state.imports.validated) fail("importsNotValidated");
}

function requireResolution(state) {
  requireImports(state);
  if (!state.resolution.validated) fail("resolutionNotValidated");
}

function requireArtifacts(state) {
  requireResolution(state);
  if (!state.artifacts.verified) fail("artifactsNotVerified");
}

function requireCapabilities(state) {
  requireArtifacts(state);
  if (!state.capabilities.admitted) fail("capabilitiesNotAdmitted");
}

function requireBuild(state) {
  requireCapabilities(state);
  if (!state.product) fail("ephemeralProductNotBuilt");
}

function normalizedPath(value, code = "sourcePathMissing") {
  if (typeof value !== "string" || value.trim() === "") fail(code);
  const path = value.replaceAll("\\", "/");
  if (/^(?:[A-Za-z]:|\\\\|\/)/.test(path)) fail("absolutePathForbidden");
  const segments = path.split("/");
  if (segments.some((segment) => segment === "..")) fail("canonicalRootEscape");
  return segments.filter((segment) => segment !== "." && segment !== "").join("/") || ".";
}

function normalizedLogicalPath(value) {
  return normalizedPath(value, "importPathMissing");
}

function physicalInputPath(value) {
  if (typeof value !== "string" || value.trim() === "") fail("sourcePathMissing");
  // A provider owns physical path canonicalization. The oracle keeps this
  // value opaque and uses only provider containment facts below.
  return value;
}

function isDigest(value) {
  return typeof value === "string" && /^sha256:[0-9a-f]{64}$/.test(value);
}

function requireDigest(value, code = "invalidDigest") {
  if (!isDigest(value)) fail(code);
  return value;
}

function normalizedRequirement(value) {
  if (typeof value !== "string" || !REQUIREMENTS.has(value)) fail("unknownRequirement");
  return value;
}

function normalizedDependency(value) {
  if (!value || typeof value !== "object") fail("invalidDependency");
  const alias = value.alias;
  if (typeof alias !== "string" || !/^[A-Za-z_][A-Za-z0-9_]*$/.test(alias)) {
    fail("invalidDependencyAlias");
  }
  if (value.path !== undefined || value.source?.kind === "path") fail("pathDependencyForbidden");
  if (
    value.mutable === true ||
    value.branch !== undefined ||
    value.ref !== undefined ||
    value.source?.mutable === true ||
    value.source?.branch !== undefined ||
    value.source?.ref !== undefined
  ) {
    fail("mutableDependencyForbidden");
  }
  if (
    value.source === "ambient" ||
    value.source?.kind === "ambient" ||
    !value.source ||
    typeof value.source !== "object"
  ) {
    fail("ambientDependencyForbidden");
  }
  if (
    value.localOverride === true ||
    value.override !== undefined ||
    value.source.kind === "local"
  ) {
    fail("localOverrideForbidden");
  }
  if (value.source.kind !== "registry" || typeof value.source.authority !== "string") {
    fail("dependencyAuthorityMissing");
  }
  if (value.source.immutable === false) fail("mutableDependencyForbidden");
  if (typeof value.package !== "string" || value.package.trim() === "") fail("packageIdentityMissing");
  if (typeof value.version !== "string" || value.version.trim() === "") fail("versionConstraintMissing");
  const use = typeof value.use === "string" ? value.use.replace(/^\./, "") : value.use;
  if (!ALLOWED_USES.has(use)) fail("invalidDependencyUse");
  return {
    alias,
    package: value.package,
    version: value.version,
    use,
    source: { kind: "registry", authority: value.source.authority, immutable: true },
  };
}

function normalizeDependencies(values) {
  if (values === undefined) return [];
  if (!Array.isArray(values)) fail("invalidDependencies");
  const dependencies = values.map(normalizedDependency);
  const aliases = new Set();
  for (const dependency of dependencies) {
    if (aliases.has(dependency.alias)) fail("duplicateDependencyAlias");
    aliases.add(dependency.alias);
  }
  return dependencies.sort((left, right) => left.alias.localeCompare(right.alias));
}

function normalizeFields(header) {
  if (!header || typeof header !== "object") fail("invalidScriptHeader");
  const entries = Array.isArray(header.fields)
    ? header.fields.map((field) => [field?.name, field?.value])
    : Object.entries(header);
  const values = {};
  for (const [name, value] of entries) {
    if (typeof name !== "string" || !HEADER_FIELDS.has(name)) fail("unknownScriptField");
    if (Object.hasOwn(values, name)) fail("duplicateScriptField");
    values[name] = value;
  }
  if (typeof values.edition !== "string" || values.edition.trim() === "") {
    fail("missingScriptEdition");
  }
  const dependencies = normalizeDependencies(values.dependencies);
  const lock = values.lock ?? null;
  if (lock !== null) requireDigest(lock, "invalidLockDigest");
  if (dependencies.length > 0 && lock === null) fail("missingLock");
  if (dependencies.length === 0 && lock !== null) fail("lockWithoutDependencies");
  if (!Array.isArray(values.requires ?? [])) fail("invalidRequirements");
  const rawRequirements = (values.requires ?? []).map(normalizedRequirement);
  if (new Set(rawRequirements).size !== rawRequirements.length) fail("duplicateRequirement");
  const requires = [...new Set(rawRequirements)].sort();
  return { edition: values.edition, dependencies, lock, requires };
}

function sourceSelectionDigest(header) {
  return scriptDigest("w-script-selection-v1", {
    edition: header.edition,
    dependencies: header.dependencies,
  });
}

function headerDigest(header) {
  return scriptDigest("w-script-header-v2", header);
}

function packageNodeDigest(pkg) {
  const { id: _id, ...identity } = pkg;
  return scriptDigest("w-package-node-v1", canonical(identity));
}

function normalizePackage(pkg) {
  if (!pkg || typeof pkg !== "object") fail("lockPackageMissing");
  requireDigest(pkg.id, "packageIdMissing");
  if (typeof pkg.name !== "string" || pkg.name.trim() === "") fail("packageNameMissing");
  if (typeof pkg.version !== "string" || pkg.version.trim() === "") fail("packageVersionMissing");
  if (!pkg.source || typeof pkg.source !== "object") fail("packageSourceMissing");
  if (pkg.source.kind !== "registry" || typeof pkg.source.authority !== "string" || pkg.source.authority.trim() === "") {
    fail("packageAuthorityMissing");
  }
  if (pkg.source.immutable !== true) fail("mutableDependencyForbidden");
  requireDigest(pkg.metadataDigest, "packageMetadataDigestMissing");
  requireDigest(pkg.contentDigest, "packageContentDigestMissing");
  if (!Array.isArray(pkg.dependencies)) fail("packageEdgesMissing");
  const dependencies = pkg.dependencies.map((edge) => {
    if (!edge || typeof edge !== "object" || typeof edge.alias !== "string" || !/^[A-Za-z_][A-Za-z0-9_]*$/.test(edge.alias)) {
      fail("invalidPackageEdge");
    }
    requireDigest(edge.id, "danglingPackageEdge");
    return { alias: edge.alias, id: edge.id };
  }).sort((left, right) => `${left.alias}\0${left.id}`.localeCompare(`${right.alias}\0${right.id}`));
  if (new Set(dependencies.map((edge) => edge.alias)).size !== dependencies.length) fail("packageAliasCollision");
  const normalized = {
    id: pkg.id,
    name: pkg.name,
    version: pkg.version,
    source: canonical(pkg.source),
    metadataDigest: pkg.metadataDigest,
    contentDigest: pkg.contentDigest,
    dependencies,
  };
  if (packageNodeDigest(normalized) !== pkg.id) fail("packageIdMismatch");
  return normalized;
}

function normalizeContext(context) {
  if (!context || typeof context !== "object") fail("lockContextMissing");
  if (typeof context.root !== "string" || context.root.trim() === "") fail("lockRootMissing");
  const use = typeof context.use === "string" ? context.use.replace(/^\./, "") : null;
  if (!use || !ALLOWED_USES.has(use)) fail("lockUseMismatch");
  const targetRole = typeof context.targetRole === "string" ? context.targetRole.replace(/^\./, "") : null;
  if (!targetRole || targetRole.trim() === "") fail("lockTargetRoleMissing");
  if (typeof context.target !== "string" || context.target.trim() === "") fail("lockTargetMissing");
  if (!Array.isArray(context.features) || !Array.isArray(context.targetVariants)) fail("lockContextIncomplete");
  requireDigest(context.activeSourceSet, "activeSourceSetMissing");
  requireDigest(context.resolutionDigest, "resolutionDigestMissing");
  if (!Array.isArray(context.nodes)) fail("lockContextNodesMissing");
  const hasRootEdges = Array.isArray(context.rootEdges);
  if (context.root === SCRIPT_ROOT && !hasRootEdges) fail("lockContextRootEdgesMissing");
  if (context.root !== SCRIPT_ROOT && hasRootEdges) fail("lockPackageRootEdgesForbidden");
  context.nodes.forEach((id) => requireDigest(id, "lockNodeIdMissing"));
  if (new Set(context.nodes).size !== context.nodes.length) fail("duplicateContextNode");
  const rootEdges = (context.rootEdges ?? []).map((edge) => {
    if (!edge || typeof edge.alias !== "string") fail("lockRootEdgeMissing");
    requireDigest(edge.id, "lockRootEdgeMissing");
    return { alias: edge.alias, id: edge.id };
  }).sort((left, right) => left.alias.localeCompare(right.alias));
  if (new Set(rootEdges.map((edge) => edge.alias)).size !== rootEdges.length) fail("lockRootAliasCollision");
  return {
    root: context.root,
    use,
    targetRole,
    target: context.target,
    features: clone(context.features),
    targetVariants: clone(context.targetVariants),
    activeSourceSet: context.activeSourceSet,
    resolutionDigest: context.resolutionDigest,
    nodes: [...new Set(context.nodes)].sort(),
    rootEdges,
    hasRootEdges,
  };
}

function normalizedLock(state, header, lock, options = {}) {
  const payload = lockPayload(lock);
  const digest = lockRootDigest(lock);
  if (options.expectDigest !== false && digest !== header.lock) fail("lockRootDigestMismatch");
  const selectionDigest = sourceSelectionDigest(header);
  if (payload.workspaceDigest !== selectionDigest) fail("selectionDigestMismatch");
  if (!payload.manifestDigests || payload.manifestDigests["virtual-script"] !== selectionDigest) fail("selectionDigestMismatch");
  const lockDependencies = normalizeDependencies(header.dependencies);
  const contexts = payload.contexts.map(normalizeContext);
  const rootName = options.rootName ?? ".product(\"script\")";
  const requestedUse = typeof options.use === "string" ? options.use.replace(/^\./, "") : "product";
  const targetRole = typeof options.targetRole === "string" ? options.targetRole.replace(/^\./, "") : "target";
  const matchingContexts = contexts.filter((candidate) => (
    candidate.root === rootName &&
    candidate.use === requestedUse &&
    candidate.targetRole === targetRole &&
    (options.target === undefined || candidate.target === options.target)
  ));
  if (matchingContexts.length === 0) fail(options.target === undefined ? "lockContextMissing" : "targetMismatch");
  if (matchingContexts.length > 1) fail("lockContextTargetAmbiguous");
  const context = matchingContexts[0];
  if (context.resolutionDigest !== selectionDigest || context.activeSourceSet !== selectionDigest) fail("selectionDigestMismatch");
  if (context.target !== payload.target && payload.target !== undefined) fail("targetMismatch");
  const packages = payload.packages.map(normalizePackage);
  const byId = new Map();
  for (const pkg of packages) {
    if (byId.has(pkg.id)) fail("duplicatePackageId");
    byId.set(pkg.id, pkg);
  }
  const headerAliases = lockDependencies.map((dependency) => dependency.alias).sort();
  if (context.root === SCRIPT_ROOT && context.hasRootEdges) {
    const rootAliases = context.rootEdges.map((edge) => edge.alias).sort();
    if (JSON.stringify(rootAliases) !== JSON.stringify(headerAliases)) fail("lockRootEdgeMismatch");
  }
  const visited = new Set();
  const allVisited = new Set();
  const visiting = new Set();
  const walk = (id, targetSet = visited) => {
    if (!byId.has(id)) fail("danglingPackageEdge");
    if (visiting.has(id)) fail("packageGraphCycle");
    if (targetSet.has(id)) return;
    visiting.add(id);
    const pkg = byId.get(id);
    for (const edge of pkg.dependencies) walk(edge.id, targetSet);
    visiting.delete(id);
    targetSet.add(id);
  };
  for (const candidate of contexts) {
    const candidateVisited = new Set();
    if (candidate.hasRootEdges) {
      for (const edge of candidate.rootEdges) walk(edge.id, candidateVisited);
    } else {
      for (const id of candidate.nodes) walk(id, candidateVisited);
    }
    if (JSON.stringify([...candidateVisited].sort()) !== JSON.stringify([...candidate.nodes].sort())) fail("lockClosureMismatch");
    for (const id of candidateVisited) allVisited.add(id);
  }
  if (context.hasRootEdges) for (const edge of context.rootEdges) walk(edge.id, visited);
  else for (const id of context.nodes) walk(id, visited);
  if (JSON.stringify([...visited].sort()) !== JSON.stringify([...context.nodes].sort())) fail("lockClosureMismatch");
  for (const pkg of packages) if (!allVisited.has(pkg.id)) fail("unreachablePackage");
  // A virtual script has no package manifest root. Its closed `rootEdges`
  // extension carries the direct aliases that must match the header. Normal
  // package contexts use their package manifest and can omit `rootEdges`; the
  // promotion comparison validates that graph through the package lock itself.
  if (context.root === SCRIPT_ROOT && context.hasRootEdges) {
    for (const dependency of lockDependencies) {
      const edge = context.rootEdges.find((candidate) => candidate.alias === dependency.alias);
      const pkg = edge && byId.get(edge.id);
      if (!pkg || pkg.name !== dependency.package || pkg.source.authority !== dependency.source.authority) fail("lockNodeMismatch");
    }
  }
  const artifacts = Array.isArray(lock.artifacts) ? lock.artifacts.map((artifact) => {
    if (!artifact || typeof artifact !== "object") fail("lockArtifactsMissing");
    requireDigest(artifact.nodeId, "artifactNodeMissing");
    const packageNode = byId.get(artifact.nodeId);
    if (!packageNode || !allVisited.has(artifact.nodeId)) fail("artifactNodeUnreachable");
    requireDigest(artifact.digest, "invalidArtifactDigest");
    if (typeof artifact.authority !== "string" || artifact.authority.trim() === "") fail("artifactAuthorityMissing");
    const artifactUse = typeof artifact.use === "string" ? artifact.use.replace(/^\./, "") : null;
    const artifactTargetRole = typeof artifact.targetRole === "string" ? artifact.targetRole.replace(/^\./, "") : "target";
    if (typeof artifact.target !== "string" || artifact.target.trim() === "" || artifactUse !== "product") fail("artifactContextMismatch");
    if (!contexts.some((candidate) => candidate.root === rootName && candidate.use === artifactUse && candidate.targetRole === artifactTargetRole && candidate.target === artifact.target)) fail("artifactContextMismatch");
    if (typeof artifact.signatureRequired !== "boolean") fail("artifactSignatureEvidenceMissing");
    if (artifact.signatureRequired && (typeof artifact.signatureEvidence !== "string" || artifact.signatureEvidence.trim() === "")) fail("artifactSignatureEvidenceMissing");
    if (artifact.recipeOwner !== undefined && (typeof artifact.recipeOwner !== "string" || artifact.recipeOwner.trim() === "")) fail("artifactRecipeOwnerMissing");
    if (artifact.recordDigest !== undefined) requireDigest(artifact.recordDigest, "artifactRecordDigestMissing");
    return {
      nodeId: artifact.nodeId,
      digest: artifact.digest,
      authority: artifact.authority,
      target: artifact.target,
      use: artifactUse,
      targetRole: artifactTargetRole,
      signatureRequired: artifact.signatureRequired,
      signatureEvidence: artifact.signatureEvidence ?? null,
      recipeOwner: artifact.recipeOwner ?? DEFAULT_RECIPE_OWNER,
      recordDigest: artifact.recordDigest ?? null,
      metadataDigest: packageNode.metadataDigest,
      contentDigest: packageNode.contentDigest,
      selected: artifact.target === context.target && artifactTargetRole === context.targetRole && visited.has(artifact.nodeId),
    };
  }) : fail("lockArtifactsMissing");
  if (artifacts.length === 0) fail("lockArtifactsMissing");
  if (!Array.isArray(lock.cas)) fail("lockCasObjectsMissing");
  const cas = lock.cas.map((object) => requireDigest(object, "invalidCasDigest"));
  const selectedArtifacts = artifacts.filter((artifact) => artifact.selected);
  const requiredCas = new Set([digest]);
  for (const pkg of packages) if (visited.has(pkg.id)) {
    requiredCas.add(pkg.metadataDigest);
    requiredCas.add(pkg.contentDigest);
  }
  for (const artifact of selectedArtifacts) requiredCas.add(artifact.digest);
  for (const object of requiredCas) if (!cas.includes(object) && object !== digest) fail("lockCasObjectMissing");
  const allRequiredHandles = Array.isArray(lock.requiredHandles) ? lock.requiredHandles.map((handle) => {
    if (!handle || typeof handle !== "object" || typeof handle.name !== "string" || typeof handle.capability !== "string" || typeof handle.contract !== "string") fail("invalidRequiredHandle");
    normalizedRequirement(handle.capability);
    requireDigest(handle.owner, "invalidRequiredHandle");
    if (!allVisited.has(handle.owner)) fail("requiredHandleOwnerUnreachable");
    const ownerPackage = byId.get(handle.owner);
    const metadataDigest = handle.metadataDigest ?? ownerPackage.metadataDigest;
    requireDigest(metadataDigest, "requiredHandleMetadataMissing");
    if (metadataDigest !== ownerPackage.metadataDigest) fail("requiredHandleMetadataMismatch");
    const recordDigest = handleRecordDigest({ lockRootDigest: digest, owner: handle.owner, metadataDigest, name: handle.name, capability: handle.capability, contract: handle.contract });
    if (handle.recordDigest !== undefined && handle.recordDigest !== recordDigest) fail("requiredHandleRecordDigestMismatch");
    return { name: handle.name, capability: handle.capability, contract: handle.contract, owner: handle.owner, metadataDigest, recordDigest, selected: visited.has(handle.owner) };
  }) : [];
  const actionOutputs = Array.isArray(lock.actionOutputs) ? lock.actionOutputs.map((output) => {
    if (!output || typeof output !== "object" || typeof output.name !== "string" || typeof output.policy !== "string") fail("invalidActionOutput");
    requireDigest(output.digest, "actionOutputDigestMissing");
    requireDigest(output.owner, "actionOutputOwnerMissing");
    if (!allVisited.has(output.owner)) fail("actionOutputOwnerUnreachable");
    requireDigest(output.actionDigest, "actionDigestMissing");
    requireDigest(output.provenanceDigest, "actionOutputProvenanceMissing");
    if (output.recordDigest !== undefined) requireDigest(output.recordDigest, "actionOutputRecordDigestMissing");
    const recordDigest = actionOutputRecordDigest({ lockRootDigest: digest, owner: output.owner, actionDigest: output.actionDigest, outputDigest: output.digest, policy: output.policy, provenanceDigest: output.provenanceDigest });
    if (output.recordDigest !== undefined && output.recordDigest !== recordDigest) fail("actionOutputRecordDigestMismatch");
    return { name: output.name, owner: output.owner, digest: output.digest, policy: output.policy, actionDigest: output.actionDigest, provenanceDigest: output.provenanceDigest, recordDigest };
  }) : [];
  for (const output of actionOutputs) if (visited.has(output.owner)) requiredCas.add(output.digest);
  for (const object of requiredCas) if (!cas.includes(object) && object !== digest) fail("lockCasObjectMissing");
  const selectedCas = new Set([digest]);
  for (const pkg of packages) if (visited.has(pkg.id)) {
    selectedCas.add(pkg.metadataDigest);
    selectedCas.add(pkg.contentDigest);
  }
  for (const artifact of selectedArtifacts) selectedCas.add(artifact.digest);
  for (const output of actionOutputs) if (visited.has(output.owner)) selectedCas.add(output.digest);
  const requiredHandles = allRequiredHandles.filter((handle) => handle.selected);
  return {
    validated: true,
    digest: header.lock ?? digest,
    rootDigest: digest,
    authority: null,
    selectionDigest,
    edition: header.edition,
    target: context.target,
    use: "product",
    dependencies: lockDependencies,
    selectedContext: context,
    nodes: packages.filter((pkg) => visited.has(pkg.id)),
    packages,
    contexts,
    edges: context.rootEdges,
    closure: [...visited].sort(),
    artifacts: selectedArtifacts,
    allArtifacts: artifacts,
    cas: [...selectedCas].sort(),
    requiredHandles,
    actionOutputs,
    selectedActionOutputs: actionOutputs.filter((output) => visited.has(output.owner)),
    consumedActionOutputs: [],
    requiredHandleRecordDigests: requiredHandles.filter((handle) => visited.has(handle.owner)).map((handle) => handle.recordDigest).sort(),
  };
}

function trace(state, event, details = {}) {
  state.trace.push({ event, ...canonical(details) });
}

function parseHeader(state, operation) {
  const sourceKind = operation.sourceKind ?? "file";
  if (["url", "stdin", "shebang"].includes(sourceKind)) fail("scriptSourceKindRejected");
  state.source.path = physicalInputPath(operation.path ?? "script.w");
  state.source.kind = sourceKind;
  state.source.topLevelExecution = operation.topLevelExecution === true;
  state.source.entry = operation.entry ?? null;
  state.source.imports = clone(operation.imports ?? []);
  state.source.text = operation.sourceText === undefined ? null : String(operation.sourceText).replace(/\r\n?/g, "\n");
  const computedSourceDigest = operation.sourceText !== undefined
    ? scriptDigest("w-script-source-v2", state.source.text)
    : null;
  if (operation.sourceDigest !== undefined) {
    const suppliedSourceDigest = requireDigest(operation.sourceDigest, "rootDigestMissing");
    if (computedSourceDigest && suppliedSourceDigest !== computedSourceDigest) fail("rootDigestMismatch");
    state.source.textDigest = suppliedSourceDigest;
  } else {
    state.source.textDigest = computedSourceDigest;
  }
  state.source.rootDigest = state.source.textDigest;
  if (operation.header === null || (operation.header === undefined && operation.hasHeader !== true) || operation.hasHeader === false) {
    state.source.header = null;
    state.source.headerDigest = null;
    state.source.parseEvidence = operation.parseEvidence === undefined && operation.sourceText === undefined
      ? null
      : parseEvidence(state, operation, null);
    state.phase = "parsed";
    trace(state, "parseHeader", { header: null, rootSourceDigest: state.source.rootDigest, parseEvidence: state.source.parseEvidence });
    return;
  }
  if (operation.secondHeader === true) fail("duplicateScriptHeader");
  if (operation.first === false || operation.headerPosition === "after-source") fail("scriptHeaderNotFirst");
  const header = normalizeFields(operation.header);
  state.source.header = header;
  state.source.headerDigest = headerDigest(header);
  state.source.parseEvidence = operation.parseEvidence === undefined && operation.sourceText === undefined
    ? null
    : parseEvidence(state, operation, header);
  state.phase = "parsed";
  trace(state, "parseHeader", {
    header: { edition: header.edition, dependencyCount: header.dependencies.length, requirementCount: header.requires.length },
    rootSourceDigest: state.source.rootDigest,
    parseEvidence: state.source.parseEvidence,
  });
}

function selectContext(state, operation) {
  requireParsed(state);
  const packageContext = operation.packageContext === true;
  if (state.source.header) {
    state.context = { mode: "standalone", packageContext, reason: "script-header", packageRoot: null };
  } else if (packageContext) {
    state.context = {
      mode: "package",
      packageContext: true,
      reason: "package-context",
      packageRoot: normalizedPath(operation.packageRoot ?? "package"),
    };
  } else {
    state.context = { mode: "ephemeral", packageContext: false, reason: "no-package", packageRoot: null };
  }
  state.phase = "context";
  trace(state, "selectContext", {
    mode: state.context.mode,
    reason: state.context.reason,
    packageRoot: state.context.packageRoot,
  });
}

function resolveRoots(state, operation) {
  requireContext(state);
  const scan = typeof operation.scan === "string" ? operation.scan.toLowerCase() : operation.scan;
  if (["recursive", "cwd", "path", "env", "environment"].includes(scan)) fail("discoveryForbidden");
  const facts = operation.provider ?? {};
  if (facts.symlinkEscape === true || facts.withinRoot === false) fail("canonicalRootEscape");
  if (facts.rootOwner !== undefined && facts.candidateOwner !== undefined && facts.rootOwner !== facts.candidateOwner) {
    fail("canonicalRootEscape");
  }
  const sourceDirectory = state.context.mode === "package"
    ? state.context.packageRoot
    : (typeof facts.logicalRoot === "string"
      ? normalizedLogicalPath(facts.logicalRoot)
      : (/^(?:[A-Za-z]:|\\\\|\/)/.test(state.source.path)
        ? "."
        : normalizedLogicalPath(state.source.path.split(/[\\/]/).slice(0, -1).join("/") || ".")));
  const local = state.context.mode === "package" ? state.context.packageRoot : sourceDirectory;
  state.roots.local = normalizedPath(local);
  state.roots.canonical = facts.canonicalRootToken ?? operation.canonicalRoot ?? state.roots.local;
  state.roots.physicalDisplay = facts.physicalDisplayPath ?? state.source.path;
  state.roots.owner = facts.rootOwner ?? null;
  state.roots.withinRoot = facts.withinRoot !== false;
  state.phase = "roots";
  trace(state, "resolveRoots", {
    local: state.roots.local,
    canonical: state.roots.canonical,
    withinRoot: state.roots.withinRoot,
    physicalDisplay: state.roots.physicalDisplay,
    owner: state.roots.owner,
    boundaryLabel: operation.pathBoundary ?? null,
    identityExcludesPhysicalPath: true,
  });
}

function validateImports(state, operation) {
  requireRoots(state);
  const scan = typeof operation.scan === "string" ? operation.scan.toLowerCase() : operation.scan;
  if (
    ["recursive", "cwd", "path", "env", "environment"].includes(scan) ||
    operation.recursiveScan === true ||
    operation.cwdScan === true ||
    operation.pathScan === true ||
    operation.environmentScan === true
  ) {
    fail("discoveryForbidden");
  }
  if (operation.ambiguousMerge === true) fail("ambiguousContext");
  const imports = operation.imports ?? state.source.imports ?? [];
  if (!Array.isArray(imports)) fail("invalidImports");
  const modules = [];
  for (const item of imports) {
    if (!item || typeof item !== "object") fail("importDigestMissing");
    const path = normalizedLogicalPath(item.path);
    if (item.kind === "script" || item.scriptRoot === true) fail("scriptImportForbidden");
    if (item.ambient === true) fail("ambientImportForbidden");
    if (
      item.escape === true ||
      item.symlinkEscape === true ||
      item.withinRoot === false ||
      (item.rootOwner !== undefined && item.candidateOwner !== undefined && item.rootOwner !== item.candidateOwner)
    ) fail("canonicalRootEscape");
    if (item.external === true) {
      if (!state.source.header || typeof item.alias !== "string" || !state.source.header.dependencies.some((dependency) => dependency.alias === item.alias)) {
        fail("externalImportNotLocked");
      }
      modules.push({ path, digest: null, alias: item.alias ?? null, external: true });
      continue;
    }
    requireDigest(item.digest, "importDigestMissing");
    modules.push({ path, digest: item.digest, alias: item.alias ?? null, external: false });
  }
  if (operation.importedScript === true) fail("scriptImportForbidden");
  state.imports = {
    validated: true,
    modules: modules.sort((left, right) => left.path.localeCompare(right.path)),
    paths: modules.map((module) => module.path),
    digests: modules.filter((module) => module.digest).map((module) => module.digest),
  };
  state.phase = "imports";
  trace(state, "validateImports", {
    count: state.imports.modules.length,
    modules: state.imports.modules,
    graphDigests: state.imports.digests,
  });
}

function validateResolution(state, operation, headerOverride = null) {
  requireImports(state);
  const header = headerOverride ?? state.source.header;
  if (!header) {
    state.resolution = {
      validated: true,
      digest: null,
      rootDigest: null,
      authority: null,
      selectionDigest: null,
      edition: null,
      target: null,
      use: null,
      dependencies: [],
      nodes: [],
      edges: [],
      closure: [],
      artifacts: [],
      cas: [],
      requiredHandles: [],
      actionOutputs: [],
      selectedActionOutputs: [],
      consumedActionOutputs: [],
    };
    state.phase = "resolution";
    trace(state, "validateResolution", { dependencies: 0, digest: null, source: state.context.mode });
    return state.resolution;
  }
  if (header.dependencies.length === 0) {
    if (header.lock !== null) fail("lockWithoutDependencies");
    state.resolution = {
      validated: true,
      digest: null,
      rootDigest: null,
      authority: null,
      selectionDigest: sourceSelectionDigest(header),
      edition: header.edition,
      target: null,
      use: null,
      dependencies: [],
      nodes: [],
      edges: [],
      closure: [],
      artifacts: [],
      cas: [],
      requiredHandles: [],
      actionOutputs: [],
      selectedActionOutputs: [],
      consumedActionOutputs: [],
    };
    state.phase = "resolution";
    trace(state, "validateResolution", { dependencies: 0, digest: null });
    return state.resolution;
  }
  if (operation.edition !== undefined && operation.edition !== header.edition) fail("editionMismatch");
  const resolution = normalizedLock(state, header, operation.lockObject ?? operation.lock, { target: operation.target, use: operation.use, targetRole: operation.targetRole });
  state.resolution = resolution;
  state.phase = "resolution";
  trace(state, "validateResolution", {
    dependencies: header.dependencies.length,
    digest: resolution.digest,
    rootDigest: resolution.rootDigest,
    selectionDigest: resolution.selectionDigest,
  });
  return state.resolution;
}

function admitFetch(state, operation) {
  requireResolution(state);
  if (operation.actions?.some((action) => ["install", "build", "install-script", "build-script"].includes(action.kind))) {
    fail("hiddenBuildActionForbidden");
  }
  const policyActionOutputs = Array.isArray(operation.policy?.actionOutputs) ? operation.policy.actionOutputs : [];
  if (operation.requiredActionOutput) {
    const lockOutput = state.resolution.selectedActionOutputs.find((output) => output.name === operation.requiredActionOutput);
    const policyOutput = policyActionOutputs.find((output) => output?.name === operation.requiredActionOutput);
    if ((!lockOutput && !policyOutput) || (lockOutput && !state.resolution.cas.includes(lockOutput.digest) && !policyOutput)) fail("actionOutputMissing");
    if (policyOutput) {
      requireDigest(policyOutput.owner, "actionOutputOwnerMissing");
      requireDigest(policyOutput.digest, "actionOutputDigestMissing");
      requireDigest(policyOutput.actionDigest, "actionDigestMissing");
      requireDigest(policyOutput.provenanceDigest, "actionOutputProvenanceMissing");
      requireDigest(policyOutput.recordDigest, "actionOutputRecordDigestMissing");
      if (typeof policyOutput.policy !== "string" || policyOutput.policy.trim() === "") fail("invalidActionOutput");
      const owner = state.resolution.nodes.find((pkg) => pkg.id === policyOutput.owner);
      if (!owner) fail("actionOutputOwnerUnreachable");
      const recordDigest = actionOutputRecordDigest({
        lockRootDigest: state.resolution.rootDigest,
        owner: policyOutput.owner,
        actionDigest: policyOutput.actionDigest,
        outputDigest: policyOutput.digest,
        policy: policyOutput.policy,
        provenanceDigest: policyOutput.provenanceDigest,
      });
      if (policyOutput.recordDigest !== recordDigest) fail("actionOutputRecordDigestMismatch");
      if (lockOutput && (lockOutput.owner !== policyOutput.owner || lockOutput.digest !== policyOutput.digest || lockOutput.recordDigest !== policyOutput.recordDigest)) {
        fail("actionOutputEvidenceMismatch");
      }
      state.resolution.selectedActionOutputs = [lockOutput ?? {
        name: policyOutput.name,
        owner: policyOutput.owner,
        digest: policyOutput.digest,
        policy: policyOutput.policy,
        actionDigest: policyOutput.actionDigest,
        provenanceDigest: policyOutput.provenanceDigest,
        recordDigest: policyOutput.recordDigest,
      }];
      state.resolution.consumedActionOutputs = [...state.resolution.selectedActionOutputs];
    } else {
      state.resolution.selectedActionOutputs = [lockOutput];
      state.resolution.consumedActionOutputs = [lockOutput];
    }
  } else {
    state.resolution.consumedActionOutputs = [];
  }
  const expected = state.resolution.artifacts;
  const offline = operation.offline === true || operation.networkPolicy === "offline";
  if (expected.length === 0) {
    if (operation.networkPolicy === undefined && operation.offline !== true) fail("networkPolicyMissing");
    state.fetches = [];
    state.phase = "fetch";
    trace(state, "admitFetch", { mode: offline ? "offline" : "none", count: 0 });
    return;
  }
  if (!offline && operation.networkPolicy !== "allow-pinned") fail("networkPolicyDenied");
  const cache = new Set(operation.cache ?? []);
  if (offline) {
    for (const object of [state.resolution.rootDigest, ...state.resolution.cas]) {
      if (!cache.has(object)) fail("offlineCacheMiss");
    }
  } else if (!cache.has(state.resolution.rootDigest)) {
    const lockCandidate = operation.lockCandidate;
    if (!lockCandidate || !lockCandidate.bytes) {
      fail("lockFetchMissing");
    }
    let candidateDigest;
    try {
      candidateDigest = lockRootDigest(lockCandidate.bytes);
    } catch {
      fail("lockFetchMissing");
    }
    if (candidateDigest !== state.resolution.rootDigest) {
      state.fetches.push({ kind: "lock", digest: candidateDigest, status: "retired" });
      fail("fetchDigestMismatch");
    }
  }
  const candidates = Array.isArray(operation.fetches) ? operation.fetches : [];
  const records = [];
  for (const artifact of expected) {
    const candidate = candidates.find((value) => value.nodeId === artifact.nodeId);
    const cached = cache.has(artifact.digest);
    if (!cached && !candidate) fail("artifactFetchMissing");
    const record = {
      nodeId: artifact.nodeId,
      digest: cached ? artifact.digest : candidate.digest,
      authority: cached ? artifact.authority : candidate.authority,
      signatureEvidence: cached ? artifact.signatureEvidence : candidate.signatureEvidence,
      source: cached ? "cas" : "pinned-fetch",
      status: "admitted",
    };
    if (record.digest !== artifact.digest) {
      record.status = "retired";
      state.fetches.push(record);
      fail("fetchDigestMismatch");
    }
    if (record.authority !== artifact.authority) {
      record.status = "retired";
      state.fetches.push(record);
      fail("fetchAuthorityMismatch");
    }
    if (record.signatureEvidence === false || (artifact.signatureRequired && record.signatureEvidence !== artifact.signatureEvidence)) {
      record.status = "retired";
      state.fetches.push(record);
      fail("fetchSignatureMismatch");
    }
    records.push(record);
  }
  state.fetches = records;
  state.phase = "fetch";
  trace(state, "admitFetch", { mode: offline ? "offline" : "pinned", count: records.length, closureRequired: state.resolution.closure, casRequired: state.resolution.cas, casPublishedAfterVerification: true });
}

function verifyArtifact(state, operation) {
  requireResolution(state);
  if (state.resolution.artifacts.length === 0) {
    state.artifacts = { verified: true, records: [] };
    state.phase = "artifacts";
    trace(state, "verifyArtifact", { count: 0 });
    return;
  }
  if (state.phase !== "fetch" && state.fetches.length === 0) fail("fetchNotAdmitted");
  const supplied = Array.isArray(operation.artifacts) ? operation.artifacts : [];
  const records = [];
  for (const expected of state.resolution.artifacts) {
    const fetched = state.fetches.find((record) => record.nodeId === expected.nodeId);
    const candidate = supplied.find((record) => record.nodeId === expected.nodeId) ?? fetched;
    if (!candidate) fail("artifactMissing");
    const record = {
      nodeId: expected.nodeId,
      digest: candidate.digest,
      authority: candidate.authority,
      signatureEvidence: candidate.signatureEvidence,
      status: "verified",
    };
    if (record.digest !== expected.digest) fail("artifactDigestMismatch");
    if (record.authority !== expected.authority) fail("artifactAuthorityMismatch");
    if (expected.signatureRequired && record.signatureEvidence !== expected.signatureEvidence) fail("artifactSignatureMismatch");
    records.push(record);
  }
  state.artifacts = { verified: true, records };
  state.phase = "artifacts";
  trace(state, "verifyArtifact", { count: records.length, digests: records.map((record) => record.digest), signaturesVerifiedBeforeBuild: true });
}

function admitCapabilities(state, operation) {
  requireArtifacts(state);
  if (operation.sourceGrants?.length > 0 || operation.sourceSecrets?.length > 0) fail("sourceGrantForbidden");
  const requirements = state.source.header?.requires ?? [];
  const offeredChannels = [...BASELINE_CHANNELS];
  const offeredAuthorities = [...BASELINE_AUTHORITIES];
  const deployment = operation.deployment ?? {};
  if (Object.hasOwn(deployment, "capabilities")) fail("deploymentCapabilitiesFieldRejected");
  const deploymentGrants = [...new Set(deployment.grants ?? [])];
  for (const grant of deploymentGrants) normalizedRequirement(grant);
  const matched = requirements.filter((requirement) => deploymentGrants.includes(requirement));
  for (const requirement of requirements) if (!matched.includes(requirement)) fail("capabilityMissing");
  const receivedHandles = Array.isArray(operation.receivedHandles) ? clone(operation.receivedHandles) : [];
  for (const required of state.resolution.requiredHandles) {
    const handle = receivedHandles.find((candidate) => (
      candidate.name === required.name &&
      candidate.contract === required.contract &&
      candidate.capability === required.capability &&
      candidate.owner === required.owner &&
      candidate.metadataDigest === required.metadataDigest &&
      candidate.recordDigest === required.recordDigest
    ));
    if (!handle) {
      fail("transitiveHandleMissing");
    }
  }
  const effectiveAuthorities = [...new Set([...BASELINE_AUTHORITIES, ...matched])].sort();
  state.capabilities = {
    admitted: true,
    requirements: [...requirements].sort(),
    offeredChannels,
    offeredAuthorities,
    baselineChannels: [...BASELINE_CHANNELS],
    baselineAuthorities: [...BASELINE_AUTHORITIES],
    matched: [...matched].sort(),
    effective: effectiveAuthorities,
    effectiveChannels: [...BASELINE_CHANNELS],
    deployment: clone(deployment),
    receivedHandles,
  };
  state.phase = "capabilities";
  trace(state, "admitCapabilities", {
    requirements: state.capabilities.requirements,
    offeredChannels: state.capabilities.offeredChannels,
    offeredAuthorities: state.capabilities.offeredAuthorities,
    baselineChannels: state.capabilities.baselineChannels,
    baselineAuthorities: state.capabilities.baselineAuthorities,
    matched: state.capabilities.matched,
    effective: state.capabilities.effective,
    effectiveChannels: state.capabilities.effectiveChannels,
    receivedHandles: state.capabilities.receivedHandles,
  });
}

function buildEphemeral(state, operation) {
  requireCapabilities(state);
  requireDigest(state.source.rootDigest, "rootDigestMissing");
  if (operation.hiddenManifest === true || operation.hiddenLock === true) fail("hiddenStateForbidden");
  if (typeof operation.target !== "string" || operation.target.trim() === "") fail("targetMissing");
  if (typeof operation.hostProfile !== "string" || operation.hostProfile.trim() === "") fail("hostProfileMissing");
  const toolchain = operation.toolchain;
  if (!toolchain || typeof toolchain !== "object") fail("toolchainMissing");
  requireDigest(toolchain.digest, "toolchainDigestMissing");
  const edition = state.source.header?.edition ?? operation.edition ?? "2026";
  if (operation.edition !== undefined && operation.edition !== edition) fail("editionMismatch");
  if (state.resolution.target && state.resolution.target !== operation.target) fail("targetMismatch");
  const localModules = state.imports.modules.filter((module) => !module.external).map((module) => ({ path: module.path, digest: module.digest }));
  const recipeOwner = operation.recipeOwner ?? DEFAULT_RECIPE_OWNER;
  if (typeof recipeOwner !== "string" || recipeOwner.trim() === "") fail("recipeOwnerMissing");
  const selectedContentDigests = state.resolution.nodes.map((pkg) => pkg.contentDigest).sort();
  const artifactRecords = state.resolution.artifacts.map((artifact) => {
    const packageNode = state.resolution.nodes.find((pkg) => pkg.id === artifact.nodeId);
    if (!packageNode) fail("artifactNodeUnreachable");
    if (artifact.recipeOwner !== recipeOwner) fail("artifactRecipeOwnerMismatch");
    const recordDigest = artifactRecordDigest({
      lockRootDigest: state.resolution.rootDigest,
      nodeId: artifact.nodeId,
      metadataDigest: packageNode.metadataDigest,
      contentDigest: packageNode.contentDigest,
      target: artifact.target,
      use: artifact.use,
      artifactDigest: artifact.digest,
      toolchainDigest: toolchain.digest,
      hostProfile: operation.hostProfile,
      recipeOwner,
      authority: artifact.authority,
      signatureRequired: artifact.signatureRequired,
      signatureEvidence: artifact.signatureEvidence,
    });
    if (artifact.recordDigest !== null && artifact.recordDigest !== recordDigest) fail("artifactRecordDigestMismatch");
    return { ...artifact, recordDigest };
  });
  const artifactRecordDigests = artifactRecords.map((record) => record.recordDigest).sort();
  const requiredHandleRecordDigests = state.resolution.requiredHandles.map((handle) => handle.recordDigest).sort();
  const actionOutputRecords = state.resolution.consumedActionOutputs ?? [];
  const actionOutputRecordDigests = actionOutputRecords.map((output) => output.recordDigest).sort();
  const identityInputs = {
    rootSourceDigest: state.source.rootDigest,
    localModules,
    selectedContext: {
      root: state.resolution.selectedContext?.root ?? null,
      use: state.resolution.selectedContext?.use ?? null,
      targetRole: state.resolution.selectedContext?.targetRole ?? null,
      target: state.resolution.selectedContext?.target ?? operation.target,
    },
    selectedContentDigests,
    artifactRecordDigests,
    requiredHandleRecordDigests,
    actionOutputRecordDigests,
    recipeOwner,
    edition,
    target: operation.target,
    hostProfile: operation.hostProfile,
    lockDigest: state.resolution.digest,
    requirements: state.capabilities.requirements,
    toolchainDigest: toolchain.digest,
  };
  const identity = scriptDigest("w-ephemeral-product-v2", identityInputs);
  state.product = {
    kind: "ephemeral",
    identity,
    recipeKey: identity,
    rootSourceDigest: state.source.rootDigest,
    localModules,
    selectedContext: identityInputs.selectedContext,
    selectedContentDigests,
    artifactRecords,
    artifactRecordDigests,
    requiredHandleRecordDigests,
    actionOutputRecords,
    actionOutputRecordDigests,
    recipeOwner,
    edition,
    target: operation.target,
    hostProfile: operation.hostProfile,
    lockDigest: state.resolution.digest,
    requirements: state.capabilities.requirements,
    toolchainDigest: toolchain.digest,
    physicalRoot: state.roots.canonical,
    physicalDisplay: state.roots.physicalDisplay,
    hiddenManifest: false,
  };
  state.phase = "built";
  trace(state, "buildEphemeral", {
    identity,
    recipeKey: identity,
    rootSourceDigest: state.source.rootDigest,
    selectedContentDigests,
    artifactRecordDigests,
    requiredHandleRecordDigests,
    actionOutputRecordDigests,
    recipeOwner,
    physicalRootExcluded: true,
    baselineAuthoritiesFromHostProfile: true,
  });
}

function runEntry(state, operation) {
  requireBuild(state);
  if (state.source.topLevelExecution || operation.topLevelExecution === true) fail("topLevelExecutionForbidden");
  const entry = operation.entry ?? state.source.entry;
  if (entry === undefined || entry === null) fail("defaultEntryMissing");
  if (entry !== undefined && entry !== null && entry !== "default" && entry !== "unnamed") fail("defaultEntryMissing");
  state.run = { entry: "default", args: clone(operation.args ?? []), outcome: operation.outcome ?? "success" };
  state.phase = "ran";
  state.cleanup.hiddenArtifacts = operation.hiddenArtifacts === true ? ["transient"] : [];
  trace(state, "runEntry", { entry: "default", args: state.run.args, outcome: state.run.outcome });
}

function cleanup(state, operation) {
  requireParsed(state);
  if (!state.product && !state.run) fail("nothingToCleanup");
  if (operation.complete === false || operation.hiddenState === true) fail("cleanupIncomplete");
  state.cleanup = { done: true, hiddenArtifacts: [], manifest: null, lock: null, provenance: state.product?.identity ?? null };
  state.phase = "cleaned";
  trace(state, "cleanup", { hiddenState: false, productIdentity: state.product?.identity ?? null });
}

function contextExplanation(state, operation) {
  requireRoots(state);
  const explanation = {
    mode: state.context.mode,
    reason: state.context.reason,
    sourceRoot: state.roots.local,
    canonicalRoot: state.roots.canonical ?? null,
    selectedContext: state.resolution.selectedContext ?? null,
    sourceDigest: state.source.rootDigest,
    contentDigests: state.resolution.nodes.map((pkg) => ({ id: pkg.id, digest: pkg.contentDigest })).sort((left, right) => left.id.localeCompare(right.id)),
    lockDigest: state.resolution.digest,
    fetches: state.fetches.map((fetch) => ({ nodeId: fetch.nodeId, digest: fetch.digest, authority: fetch.authority, signatureEvidence: fetch.signatureEvidence, source: fetch.source })),
    artifactRecordDigests: state.product?.artifactRecordDigests ?? [],
    requiredHandleRecordDigests: state.product?.requiredHandleRecordDigests ?? state.resolution.requiredHandleRecordDigests ?? [],
    actionOutputRecordDigests: state.product?.actionOutputRecordDigests ?? [],
    authorities: [...new Set([state.resolution.authority, ...state.resolution.artifacts.map((artifact) => artifact.authority)].filter(Boolean))].sort(),
    capabilities: {
      offeredChannels: state.capabilities.offeredChannels,
      offeredAuthorities: state.capabilities.offeredAuthorities,
      baselineChannels: state.capabilities.baselineChannels,
      baselineAuthorities: state.capabilities.baselineAuthorities,
      matched: state.capabilities.matched,
      effective: state.capabilities.effective,
      effectiveChannels: state.capabilities.effectiveChannels,
    },
    recipe: state.product?.recipeKey ?? null,
    effectiveAuthority: state.capabilities.effective,
  };
  if (operation.expectedMode !== undefined && operation.expectedMode !== explanation.mode) fail("contextExplanationMismatch");
  state.context.explanation = explanation;
  trace(state, "contextExplanation", explanation);
}

function invalidateExecutionAfterSourceMutation(state) {
  state.fetches = [];
  state.artifacts = { verified: false, records: [] };
  state.capabilities = { admitted: false, requirements: [], offeredChannels: [], offeredAuthorities: [], baselineChannels: [], baselineAuthorities: [], matched: [], effective: [], effectiveChannels: [], deployment: null, receivedHandles: [] };
  state.product = null;
  state.run = null;
  state.cleanup = { done: false, hiddenArtifacts: [], manifest: null, lock: null, provenance: null };
  state.promotion = null;
}

function editHeader(state, operation, kind) {
  requireParsed(state);
  if (!state.source.header) fail("scriptHeaderRequired");
  const current = clone(state.source.header);
  const dependencies = [...current.dependencies];
  if (kind === "add") {
    if (!operation.dependency) fail("invalidDependency");
    const candidate = normalizedDependency(operation.dependency);
    if (dependencies.some((dependency) => dependency.alias === candidate.alias)) fail("duplicateDependencyAlias");
    dependencies.push(candidate);
  } else {
    const index = dependencies.findIndex((dependency) => dependency.alias === operation.alias);
    if (index < 0) fail("unknownDependencyAlias");
    dependencies.splice(index, 1);
  }
  const candidateHeader = {
    edition: current.edition,
    dependencies: dependencies.sort((left, right) => left.alias.localeCompare(right.alias)),
    lock: operation.lockDigest ?? (dependencies.length === 0 ? null : null),
    requires: current.requires,
  };
  const nextState = clone(state);
  nextState.source.header = candidateHeader;
  if (candidateHeader.dependencies.length > 0) {
    validateResolution(nextState, { lockObject: operation.lockObject, target: operation.target });
  } else {
    if (candidateHeader.lock !== null) fail("lockWithoutDependencies");
    validateResolution(nextState);
  }
  const resultSourceText = operation.resultSourceText;
  if (typeof resultSourceText !== "string") fail("sourceBytesRequired");
  const normalizedSourceText = resultSourceText.replace(/\r\n?/g, "\n");
  const resultSourceDigest = scriptDigest("w-script-source-v2", normalizedSourceText);
  if (operation.resultSourceDigest !== undefined && operation.resultSourceDigest !== resultSourceDigest) fail("rootDigestMismatch");
  if (operation.resultHeader !== undefined && JSON.stringify(canonical(normalizeFields(operation.resultHeader))) !== JSON.stringify(canonical(candidateHeader))) fail("sourceHeaderMismatch");
  const resultEvidence = resultParseEvidence(resultSourceDigest, candidateHeader, state.source.entry, state.source.topLevelExecution, operation.resultParseEvidence);
  state.source.header = candidateHeader;
  state.source.headerDigest = headerDigest(candidateHeader);
  state.source.text = normalizedSourceText;
  state.source.textDigest = resultSourceDigest;
  state.source.rootDigest = resultSourceDigest;
  state.source.parseEvidence = resultEvidence;
  state.resolution = nextState.resolution;
  invalidateExecutionAfterSourceMutation(state);
  state.edits.push({ kind, atomic: true, headerDigest: state.source.headerDigest, selectionDigest: sourceSelectionDigest(candidateHeader) });
  state.phase = "resolution";
  trace(state, kind === "add" ? "scriptAdd" : "scriptRemove", { alias: kind === "add" ? operation.dependency.alias : operation.alias, atomic: true, resultSourceDigest, resultParseEvidence: resultEvidence });
}

function scriptResolve(state, operation) {
  requireParsed(state);
  if (!state.source.header) fail("scriptHeaderRequired");
  if (state.source.header.dependencies.length === 0) fail("noDependenciesToResolve");
  const candidate = clone(state.source.header);
  candidate.lock = operation.lockDigest;
  const nextState = clone(state);
  nextState.source.header = candidate;
  validateResolution(nextState, { lockObject: operation.lockObject, target: operation.target });
  const resultSourceText = operation.resultSourceText;
  if (typeof resultSourceText !== "string") fail("sourceBytesRequired");
  const normalizedSourceText = resultSourceText.replace(/\r\n?/g, "\n");
  const resultSourceDigest = scriptDigest("w-script-source-v2", normalizedSourceText);
  if (operation.resultSourceDigest !== undefined && operation.resultSourceDigest !== resultSourceDigest) fail("rootDigestMismatch");
  if (operation.resultHeader !== undefined && JSON.stringify(canonical(normalizeFields(operation.resultHeader))) !== JSON.stringify(canonical(candidate))) fail("sourceHeaderMismatch");
  const resultEvidence = resultParseEvidence(resultSourceDigest, candidate, state.source.entry, state.source.topLevelExecution, operation.resultParseEvidence);
  state.source.header = candidate;
  state.source.headerDigest = headerDigest(candidate);
  state.source.text = normalizedSourceText;
  state.source.textDigest = resultSourceDigest;
  state.source.rootDigest = resultSourceDigest;
  state.source.parseEvidence = resultEvidence;
  state.resolution = nextState.resolution;
  invalidateExecutionAfterSourceMutation(state);
  state.edits.push({ kind: "resolve", atomic: true, headerDigest: state.source.headerDigest, selectionDigest: nextState.resolution.selectionDigest });
  state.phase = "resolution";
  trace(state, "scriptResolve", { atomic: true, digest: candidate.lock, resultSourceDigest, resultParseEvidence: resultEvidence });
}

function comparePromotionLock(state, candidateLock) {
  if (!candidateLock || typeof candidateLock !== "object") fail("promotionLockMissing");
  try {
    // A package lock may encode a different root (`.product("package")`),
    // but its own P0 payload and root digest must validate before graph
    // equivalence is checked.
    const candidate = normalizedLock(state, state.source.header, candidateLock, { expectDigest: false, rootName: ".product(\"package\")", target: state.resolution.target, targetRole: state.resolution.selectedContext?.targetRole });
    if (candidate.selectionDigest !== state.resolution.selectionDigest) fail("promotionLockMismatch");
    const expectedPackages = canonical(state.resolution.nodes.map((pkg) => ({ ...pkg, id: pkg.id })).sort((left, right) => left.id.localeCompare(right.id)));
    const candidatePackages = canonical(candidate.nodes.map((pkg) => ({ ...pkg, id: pkg.id })).sort((left, right) => left.id.localeCompare(right.id)));
    if (JSON.stringify(candidatePackages) !== JSON.stringify(expectedPackages)) fail("promotionLockMismatch");
    if (JSON.stringify(candidate.closure) !== JSON.stringify(state.resolution.closure)) fail("promotionLockMismatch");
  } catch (error) {
    if (error instanceof ScriptWorkflowError) fail("promotionLockMismatch");
    throw error;
  }
  return lockRootDigest(candidateLock);
}

function promote(state, operation) {
  requireBuild(state);
  if (!state.source.header) fail("scriptHeaderRequired");
  const candidate = operation.package;
  if (!candidate || typeof candidate !== "object") fail("promotionPackageMissing");
  if (candidate.manifest?.edition !== state.source.header.edition) fail("promotionEditionMismatch");
  const candidateDependencies = normalizeDependencies(candidate.manifest?.dependencies);
  if (JSON.stringify(canonical(candidateDependencies)) !== JSON.stringify(canonical(state.source.header.dependencies))) fail("promotionDependencyMismatch");
  const localGraph = candidate.manifest?.localModules;
  const expectedGraph = state.imports.modules.filter((module) => !module.external).map((module) => ({ path: module.path, digest: module.digest }));
  if (JSON.stringify(canonical(localGraph ?? [])) !== JSON.stringify(canonical(expectedGraph))) fail("promotionLocalGraphMismatch");
  if (candidate.manifest?.entry !== "default") fail("promotionEntryMismatch");
  if (!Array.isArray(candidate.manifest?.requires ?? [])) fail("promotionRequirementsMismatch");
  const candidateRequirements = (candidate.manifest?.requires ?? []).map(normalizedRequirement).sort();
  if (new Set(candidateRequirements).size !== candidateRequirements.length) fail("promotionRequirementsMismatch");
  if (JSON.stringify(canonical(candidateRequirements)) !== JSON.stringify(canonical(state.source.header.requires))) fail("promotionRequirementsMismatch");
  const packageLockDigest = comparePromotionLock(state, candidate.lock);
  const packageManifestDigest = scriptDigest("w-package-manifest-v1", candidate.manifest);
  if (!candidate.provenance || candidate.provenance.sourceDigest !== state.source.rootDigest || candidate.provenance.scriptLockDigest !== state.resolution.digest || candidate.provenance.packageManifestDigest !== packageManifestDigest || candidate.provenance.packageLockDigest !== packageLockDigest) fail("promotionProvenanceMismatch");
  if (typeof operation.outputDir !== "string" || operation.outputDir.trim() === "") fail("promotionOutputMissing");
  const outputDir = normalizedPath(operation.outputDir);
  const provenanceDigest = scriptDigest("w-script-promotion-v2", {
    sourceDigest: candidate.provenance.sourceDigest,
    scriptLockDigest: candidate.provenance.scriptLockDigest,
    packageManifestDigest: candidate.provenance.packageManifestDigest,
    packageLockDigest: candidate.provenance.packageLockDigest,
  });
  if (candidate.provenance.digest !== undefined && candidate.provenance.digest !== provenanceDigest) fail("promotionProvenanceMismatch");
  state.promotion = { outputDir, packageSource: "package.w", packageLock: "package.lock", provenanceDigest, graphPreserved: true, entryPreserved: true };
  state.source.header = null;
  state.source.headerDigest = null;
  state.phase = "promoted";
  trace(state, "promote", { outputDir, provenanceDigest: state.promotion.provenanceDigest, graphPreserved: true, entryPreserved: true });
}

export function createScriptWorkflowState() {
  return {
    phase: "empty",
    source: { path: null, kind: null, header: null, headerDigest: null, parseEvidence: null, text: null, textDigest: null, rootDigest: null, entry: null, imports: [], topLevelExecution: false },
    context: { mode: null, packageContext: false, reason: null, packageRoot: null, explanation: null },
    roots: { local: null, canonical: null, physicalDisplay: null, owner: null, withinRoot: null },
    imports: { validated: false, modules: [], paths: [], digests: [] },
    resolution: { validated: false, digest: null, rootDigest: null, authority: null, selectionDigest: null, edition: null, target: null, use: null, dependencies: [], nodes: [], edges: [], closure: [], artifacts: [], cas: [], requiredHandles: [], actionOutputs: [], selectedActionOutputs: [], consumedActionOutputs: [], selectedContext: null, requiredHandleRecordDigests: [] },
    fetches: [],
    artifacts: { verified: false, records: [] },
    capabilities: { admitted: false, requirements: [], offeredChannels: [], offeredAuthorities: [], baselineChannels: [], baselineAuthorities: [], matched: [], effective: [], effectiveChannels: [], deployment: null, receivedHandles: [] },
    product: null,
    run: null,
    cleanup: { done: false, hiddenArtifacts: [], manifest: null, lock: null, provenance: null },
    edits: [],
    promotion: null,
    trace: [],
  };
}

export function validateScriptWorkflowOperation(operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") return false;
  return new Set([
    "parseHeader", "selectContext", "resolveRoots", "validateImports", "validateResolution", "admitFetch", "verifyArtifact", "admitCapabilities", "buildEphemeral", "runEntry", "cleanup", "contextExplanation", "scriptAdd", "scriptRemove", "scriptResolve", "promote",
  ]).has(operation.op);
}

export function runScriptWorkflowProgram(operations) {
  const state = createScriptWorkflowState();
  for (const [operationIndex, operation] of operations.entries()) {
    try {
      if (!validateScriptWorkflowOperation(operation)) fail("invalidOperation");
      switch (operation.op) {
        case "parseHeader": parseHeader(state, operation); break;
        case "selectContext": selectContext(state, operation); break;
        case "resolveRoots": resolveRoots(state, operation); break;
        case "validateImports": validateImports(state, operation); break;
        case "validateResolution": validateResolution(state, operation); break;
        case "admitFetch": admitFetch(state, operation); break;
        case "verifyArtifact": verifyArtifact(state, operation); break;
        case "admitCapabilities": admitCapabilities(state, operation); break;
        case "buildEphemeral": buildEphemeral(state, operation); break;
        case "runEntry": runEntry(state, operation); break;
        case "cleanup": cleanup(state, operation); break;
        case "contextExplanation": contextExplanation(state, operation); break;
        case "scriptAdd": editHeader(state, operation, "add"); break;
        case "scriptRemove": editHeader(state, operation, "remove"); break;
        case "scriptResolve": scriptResolve(state, operation); break;
        case "promote": promote(state, operation); break;
        default: fail("invalidOperation");
      }
    } catch (error) {
      const code = error instanceof ScriptWorkflowError ? error.code : "oracleInternalError";
      trace(state, "reject", { code, operation: operation.op });
      return { status: "rejected", code, operation: operationIndex, state };
    }
  }
  return { status: "accepted", state };
}
