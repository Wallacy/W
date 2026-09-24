import {
  canonical,
  derivePackageBasis as deriveManifestPackageBasis,
  derivePackageDigest as deriveManifestPackageDigest,
  derivePackageSetDigest as deriveManifestPackageSetDigest,
  digestRecord,
} from "./w-manifest-data.mjs";

export class Pkg1TransactionError extends Error {
  constructor(code, details = {}) {
    super(code);
    this.code = code;
    this.details = details;
  }
}

const DIGEST = /^sha256:[0-9a-f]{64}$/u;
const CALLER_ECHO_KEYS = new Set([
  "expected",
  "status",
  "route",
  "result",
  "ok",
  "packageSetDigest",
  "resolutionDigest",
  "deploymentDigest",
  "buildPlanDigest",
  "receipt",
  "atomicVisible",
  "crashDurable",
]);
const LEGACY_PROVIDER_KEYS = new Set([
  "flushData",
  "flushParent",
  "replaceFile",
  "reopenVerify",
  "crossVolume",
  "partialReplace",
  "crash",
  "reducerDivergence",
  "forgedReceipt",
  "providerReceipt",
  "durabilityReceipt",
]);

export { canonical, digestRecord };

function clone(value) {
  return structuredClone(value);
}

function fail(code, details = {}) {
  throw new Pkg1TransactionError(code, details);
}

function requireObject(value, code) {
  if (!value || typeof value !== "object" || Array.isArray(value)) fail(code);
  return value;
}

function requireDigest(value, code) {
  if (typeof value !== "string" || !DIGEST.test(value)) fail(code);
  return value;
}

function rejectCallerEcho(value, path = "operation") {
  if (!value || typeof value !== "object") return;
  if (Array.isArray(value)) {
    value.forEach((item, index) => rejectCallerEcho(item, `${path}[${index}]`));
    return;
  }
  for (const [key, child] of Object.entries(value)) {
    if (CALLER_ECHO_KEYS.has(key)) fail("callerEchoRejected", { path: `${path}.${key}` });
    rejectCallerEcho(child, `${path}.${key}`);
  }
}

function rejectLegacyProviderClaims(operation) {
  for (const key of LEGACY_PROVIDER_KEYS) {
    if (Object.prototype.hasOwnProperty.call(operation, key)) fail("legacyProviderClaimRejected", { key });
  }
}

export function derivePackageBasis(packageRecord) {
  requireObject(packageRecord, "packageSchemaInvalid");
  if (packageRecord.kind !== "package") fail("packageKindInvalid");
  return deriveManifestPackageBasis(packageRecord);
}

export function derivePackageDigest(packageRecord) {
  return deriveManifestPackageDigest(packageRecord);
}

export function derivePackageSetDigest(packages) {
  return deriveManifestPackageSetDigest(packages);
}

function exactPackageRoot(root) {
  if (typeof root !== "string" || root.length === 0 || root.startsWith("/") || root.startsWith("\\") || /^[A-Za-z]:/u.test(root)) return false;
  if (/[\\*?\[\]{}!]/u.test(root)) return false;
  const segments = root.replaceAll("\\", "/").split("/");
  return root === "." || segments.every((segment) => segment.length > 0 && segment !== "." && segment !== "..");
}

function validateManifestDocument(document) {
  if (document.kind !== "build_manifest" || !Array.isArray(document.packages) || document.packages.length === 0) fail("buildManifestShapeInvalid");
  const names = new Set();
  const roots = new Set();
  for (const [index, packageRecord] of document.packages.entries()) {
    if (!packageRecord || packageRecord.kind !== "package" || packageRecord.schema !== "w.package/1" || !exactPackageRoot(packageRecord.root)) {
      fail("packageRecordInvalid", { index });
    }
    if (typeof packageRecord.name !== "string" || packageRecord.name.trim() === "") fail("packageNameInvalid", { index });
    const root = packageRecord.root.replaceAll("\\", "/");
    if (names.has(packageRecord.name)) fail("duplicatePackageIdentity", { name: packageRecord.name });
    if (roots.has(root)) fail("duplicatePackageRoot", { root });
    names.add(packageRecord.name);
    roots.add(root);
  }
  if (document.build !== undefined) {
    if (!document.build || document.build.schema !== "w.build/1") fail("buildCoordinatorInvalid");
  } else {
    if (document.packages.length !== 1) fail("buildCoordinatorRequired");
    const packageRecord = document.packages[0];
    const products = packageRecord.products ?? [];
    const dependencies = packageRecord.dependencies ?? [];
    const targets = products[0]?.targets ?? [];
    if (products.length !== 1 || dependencies.length !== 0 || targets.length !== 1 || packageRecord.unambiguousDeployment !== true) {
      fail("singlePackageContextAmbiguous");
    }
  }
  return document;
}

function validateResolution(build, packageSetDigest) {
  const resolution = requireObject(build?.resolution, "resolutionMissing");
  if (resolution.schema !== "w.resolution/1") fail("resolutionSchemaInvalid");
  if (resolution.resolver !== "w.resolver/1") fail("resolverSchemaInvalid");
  if (resolution.packageSetDigest !== packageSetDigest) fail("resolutionPackageSetMismatch");
  if (resolution.resolutionDigest !== undefined) fail("resolutionSelfReference");
  if (!Array.isArray(resolution.contexts) || !Array.isArray(resolution.packages)) fail("resolutionRecordInvalid");

  const packageIds = new Set();
  const packageNames = new Set();
  for (const [index, entry] of resolution.packages.entries()) {
    if (!entry || typeof entry !== "object" || !DIGEST.test(entry.id ?? "")) fail("resolutionPackageInvalid", { index });
    if (packageIds.has(entry.id)) fail("resolutionDuplicatePackageId", { index });
    packageIds.add(entry.id);
    if (typeof entry.name !== "string" || entry.name.trim() === "") fail("resolutionPackageNameInvalid", { index });
    if (packageNames.has(entry.name)) fail("resolutionDuplicatePackageName", { index });
    packageNames.add(entry.name);
    if (!Array.isArray(entry.dependencies ?? [])) fail("resolutionDependenciesInvalid", { index });
  }

  const contextKeys = new Set();
  for (const [index, context] of resolution.contexts.entries()) {
    if (!context || typeof context !== "object") fail("resolutionContextInvalid", { index });
    const key = `${context.package ?? ""}\0${context.product ?? ""}\0${context.target ?? ""}\0${context.use ?? ""}`;
    if (contextKeys.has(key)) fail("resolutionContextDuplicate", { index });
    contextKeys.add(key);
    if (typeof context.package !== "string" || typeof context.product !== "string" || typeof context.target !== "string" || typeof context.use !== "string") {
      fail("resolutionContextClosureInvalid", { index });
    }
    if (!Array.isArray(context.nodes)) fail("resolutionContextNodesInvalid", { index });
    for (const node of context.nodes) {
      if (!packageIds.has(node)) fail("resolutionDanglingNode", { index, node });
    }
    if (context.rootEdges !== undefined) {
      if (!Array.isArray(context.rootEdges)) fail("resolutionRootEdgesInvalid", { index });
      const aliases = new Set();
      for (const edge of context.rootEdges) {
        if (!edge || typeof edge.alias !== "string" || !DIGEST.test(edge.id ?? "")) fail("resolutionRootEdgeInvalid", { index });
        if (aliases.has(edge.alias)) fail("aliasCollision", { index, alias: edge.alias });
        aliases.add(edge.alias);
        if (!packageIds.has(edge.id)) fail("resolutionDanglingEdge", { index, id: edge.id });
      }
    }
  }
  return resolution;
}

function validateDeployments(build) {
  const deployments = build?.deployments ?? [];
  if (!Array.isArray(deployments)) fail("deploymentsInvalid");
  const names = new Set();
  return deployments.map((deployment, index) => {
    if (!deployment || typeof deployment !== "object" || typeof deployment.name !== "string" || deployment.name.trim() === "") {
      fail("deploymentInvalid", { index });
    }
    if (names.has(deployment.name)) fail("deploymentDuplicate", { name: deployment.name });
    names.add(deployment.name);
    if (!Array.isArray(deployment.artifacts)) fail("deploymentArtifactsInvalid", { name: deployment.name });
    for (const artifact of deployment.artifacts) {
      if (!artifact || typeof artifact.name !== "string" || !artifact.source) fail("deploymentArtifactLinkInvalid", { name: deployment.name });
    }
    if (deployment.resolution !== undefined) fail("deploymentImplicitResolution");
    return deployment;
  });
}

function formatDocument(document) {
  return `${JSON.stringify(canonical(document), null, 2)}\n`;
}

export function deriveDocumentState(document) {
  requireObject(document, "documentSchemaInvalid");
  validateManifestDocument(document);
  const packageSetDigest = derivePackageSetDigest(document.packages);
  const resolution = document.build ? validateResolution(document.build, packageSetDigest) : null;
  const deployments = document.build ? validateDeployments(document.build) : [];
  const resolutionDigest = resolution ? digestRecord("w.resolution/1", resolution) : null;
  const packageRoots = document.packages
    .map(({ name, root }) => ({ name, root }))
    .sort((left, right) => left.name < right.name ? -1 : left.name > right.name ? 1 : 0);
  const deploymentDigests = deployments.map((deployment) => ({
    name: deployment.name,
    digest: digestRecord("w.deployment/1", deployment),
  }));
  const buildPlanDigest = document.build
    ? digestRecord("w.build-plan/1", { packageSetDigest, packageRoots, resolutionDigest, coordinator: document.build })
    : null;
  const formatted = formatDocument(document);
  return {
    packageSetDigest,
    packageDigests: document.packages.map((packageRecord) => ({ name: packageRecord.name, digest: derivePackageDigest(packageRecord) })).sort((a, b) => a.name < b.name ? -1 : a.name > b.name ? 1 : 0),
    resolutionDigest,
    deploymentDigests,
    buildPlanDigest,
    documentDigest: digestRecord("w.document/1", formatted),
    bytes: formatted,
  };
}

function ensureNoDerivedFields(document) {
  if (!document || typeof document !== "object") return;
  for (const key of ["packageSetDigest", "resolutionDigest", "buildPlanDigest", "deploymentDigest", "receipt"]) {
    if (Object.prototype.hasOwnProperty.call(document, key)) fail("forgedDigestOrReceipt", { key });
  }
  if (document.build?.resolution && Object.prototype.hasOwnProperty.call(document.build.resolution, "resolutionDigest")) fail("resolutionSelfReference");
  for (const deployment of document.build?.deployments ?? []) {
    if (deployment && Object.prototype.hasOwnProperty.call(deployment, "deploymentDigest")) fail("forgedDigestOrReceipt", { key: "deploymentDigest" });
  }
}

function merge(base, patch) {
  const output = clone(base);
  for (const [key, value] of Object.entries(patch ?? {})) {
    if (value && typeof value === "object" && !Array.isArray(value) && output[key] && typeof output[key] === "object" && !Array.isArray(output[key])) {
      output[key] = merge(output[key], value);
    } else {
      output[key] = clone(value);
    }
  }
  return output;
}

function makeInitialState() {
  return {
    document: null,
    bytes: null,
    documentDigest: null,
    temporary: 0,
    receipts: [],
    commands: new Map(),
  };
}

export function createPkg1State() {
  return makeInitialState();
}

function stateSnapshot(state) {
  return {
    document: state.document,
    bytes: state.bytes,
    documentDigest: state.documentDigest,
    temporary: state.temporary,
    receipts: state.receipts,
    commands: [...state.commands.keys()].sort(),
  };
}

function initialize(state, operation) {
  if (state.document) fail("buildRootAlreadyInitialized");
  ensureNoDerivedFields(operation.document);
  rejectCallerEcho(operation.document);
  const document = clone(operation.document);
  if (operation.selectionAmbiguous) fail("buildSelectionAmbiguous");
  if (document.build?.resolution && Object.prototype.hasOwnProperty.call(document.build.resolution, "packageSetDigest")) fail("forgedDigestOrReceipt", { key: "packageSetDigest" });
  validateManifestDocument(document);
  if (document.build) {
    document.build.resolution = updateResolution(document, {}, derivePackageSetDigest(document.packages));
  }
  const derived = deriveDocumentState(document);
  state.document = document;
  state.bytes = derived.bytes;
  state.documentDigest = derived.documentDigest;
  return {
    status: "accepted",
    code: "initialized",
    packageSetDigest: derived.packageSetDigest,
    packageDigests: derived.packageDigests,
    resolutionDigest: derived.resolutionDigest,
    deploymentDigests: derived.deploymentDigests,
    buildPlanDigest: derived.buildPlanDigest,
    documentDigest: derived.documentDigest,
  };
}

function currentState(state) {
  if (!state.document) fail("documentMissing");
  const derived = deriveDocumentState(state.document);
  if (derived.bytes !== state.bytes || derived.documentDigest !== state.documentDigest) fail("stateDigestMismatch");
  return derived;
}

function checkExpectedDigest(state, operation, derived) {
  if (operation.expectedDocumentDigest !== derived.documentDigest) fail("staleWrite", { expected: operation.expectedDocumentDigest, actual: derived.documentDigest });
}

function eventKinds(events, code) {
  if (!Array.isArray(events)) fail(code);
  return events.map((event, index) => {
    if (!event || typeof event !== "object" || Array.isArray(event) || typeof event.kind !== "string") fail(code, { index });
    const allowed = event.kind === "provider-durability-receipt"
      ? new Set(["kind", "schema", "durable", "digest"])
      : new Set(["kind"]);
    for (const key of Object.keys(event)) if (!allowed.has(key)) fail("providerEventFieldInvalid", { index, key });
    return event.kind;
  });
}

function sequenceStarts(kinds, prefix) {
  return prefix.every((kind, index) => kinds[index] === kind);
}

function requireSequence(kinds, expected, code) {
  if (kinds.length !== expected.length || !sequenceStarts(kinds, expected)) fail(code, { expected, actual: kinds });
}

export function deriveProviderDurabilityReceiptDigest(platform, context) {
  return digestRecord("w.provider-durability-receipt/1", {
    platform,
    oldDocumentDigest: requireDigest(context.oldDocumentDigest, "providerContextInvalid"),
    newDocumentDigest: requireDigest(context.newDocumentDigest, "providerContextInvalid"),
    boundary: platform === "posix" ? "rename+parent-directory-flush" : "replace-file+reopen-verify",
  });
}

function validateDurabilityReceipt(event, platform, context) {
  if (event === undefined) return { crashDurable: false, durabilityEvidence: "evidence-missing", providerReceiptDigest: undefined };
  const expectedDigest = deriveProviderDurabilityReceiptDigest(platform, context);
  if (event.schema !== "w.provider-durability-receipt/1" || event.durable !== true || event.digest !== expectedDigest) {
    fail("forgedDurabilityReceipt");
  }
  return { crashDurable: true, durabilityEvidence: "provider-receipt", providerReceiptDigest: event.digest };
}

export function reducePosixEvents(events, context) {
  const kinds = eventKinds(events, "posixEventsMissing");
  const prepared = ["temp-created", "temp-written", "temp-data-flushed", "compare-verified"];
  if (kinds[0] !== "temp-created" || kinds[1] !== "temp-written") fail("posixEventOrderInvalid", { actual: kinds });
  if (kinds[2] !== "temp-data-flushed") fail("missingDataFlushReceipt");
  if (kinds[3] !== "compare-verified") fail("compareReceiptMissing");
  if (!sequenceStarts(kinds, prepared)) fail("posixEventOrderInvalid", { actual: kinds });

  if (kinds[4] === "cross-volume-detected") {
    requireSequence(kinds, [...prepared, "cross-volume-detected"], "posixEventOrderInvalid");
    fail("crossVolumeRenameRejected");
  }
  if (kinds[4] === "process-crash") {
    requireSequence(kinds, [...prepared, "process-crash", "recovery-open", "orphan-temp-removed", "old-content-verified"], "posixRecoveryInvalid");
    return { fault: "crashBeforeReplace", atomicVisible: false, published: "old", temporaryAfterRecovery: 0 };
  }
  if (kinds[4] !== "rename-committed") fail("renameCommitMissing");
  if (kinds[5] === "process-crash") {
    requireSequence(kinds, [...prepared, "rename-committed", "process-crash", "recovery-open", "new-content-verified"], "posixRecoveryInvalid");
    return { fault: "crashAfterReplace", atomicVisible: true, published: "new", temporaryAfterRecovery: 0 };
  }
  if (kinds[5] !== "target-reopened") fail("reopenVerificationMissing");
  if (kinds[6] === "publication-receipt-mismatch") fail("publicationReceiptMismatch");
  if (kinds[6] !== "content-verified") fail("publicationReceiptMismatch");
  if (kinds[7] !== "parent-directory-flushed") fail("missingParentDirectoryReceipt");
  const base = [...prepared, "rename-committed", "target-reopened", "content-verified", "parent-directory-flushed"];
  const receipt = events[8];
  requireSequence(kinds, receipt ? [...base, "provider-durability-receipt"] : base, "posixEventOrderInvalid");
  return { fault: null, atomicVisible: true, published: "new", temporaryAfterRecovery: 0, ...validateDurabilityReceipt(receipt, "posix", context) };
}

export function reduceWindowsEvents(events, context) {
  const kinds = eventKinds(events, "windowsEventsMissing");
  const staged = ["temp-created", "temp-written", "temp-data-flushed", "compare-verified"];
  if (kinds[0] !== "temp-created" || kinds[1] !== "temp-written") fail("windowsEventOrderInvalid", { actual: kinds });
  if (kinds[2] !== "temp-data-flushed") fail("missingDataFlushReceipt");
  if (kinds[3] !== "compare-verified") fail("compareReceiptMissing");
  if (!sequenceStarts(kinds, staged)) fail("windowsEventOrderInvalid", { actual: kinds });

  if (kinds[4] === "replace-file-partial") {
    requireSequence(kinds, [...staged, "replace-file-partial"], "windowsEventOrderInvalid");
    fail("replacePartialFailure");
  }
  if (kinds[4] === "process-crash") {
    requireSequence(kinds, [...staged, "process-crash", "recovery-open", "orphan-temp-removed", "old-content-verified"], "windowsRecoveryInvalid");
    return { fault: "crashBeforeReplace", atomicVisible: false, published: "old", temporaryAfterRecovery: 0 };
  }
  if (kinds[4] !== "replace-file-committed") fail("replaceFileFailed");
  if (kinds[5] === "process-crash") {
    requireSequence(kinds, [...staged, "replace-file-committed", "process-crash", "recovery-open", "new-content-verified"], "windowsRecoveryInvalid");
    return { fault: "crashAfterReplace", atomicVisible: true, published: "new", temporaryAfterRecovery: 0 };
  }
  if (kinds[5] !== "target-reopened") fail("reopenVerificationMissing");
  if (kinds[6] === "publication-receipt-mismatch") fail("publicationReceiptMismatch");
  if (kinds[6] !== "content-verified") fail("publicationReceiptMismatch");
  const base = [...staged, "replace-file-committed", "target-reopened", "content-verified"];
  const receipt = events[7];
  requireSequence(kinds, receipt ? [...base, "provider-durability-receipt"] : base, "windowsEventOrderInvalid");
  return { fault: null, atomicVisible: true, published: "new", temporaryAfterRecovery: 0, ...validateDurabilityReceipt(receipt, "windows", context) };
}

function providerProjection(result) {
  return {
    fault: result.fault,
    atomicVisible: result.atomicVisible,
    published: result.published,
    temporaryAfterRecovery: result.temporaryAfterRecovery,
    crashDurable: result.crashDurable ?? false,
  };
}

export function compareProviderTraces(comparison, context) {
  requireObject(comparison, "providerComparisonInvalid");
  const posix = reducePosixEvents(comparison.posix, context);
  const windows = reduceWindowsEvents(comparison.windows, context);
  if (JSON.stringify(canonical(providerProjection(posix))) !== JSON.stringify(canonical(providerProjection(windows)))) {
    fail("reducerDivergence", { posix: providerProjection(posix), windows: providerProjection(windows) });
  }
  return { posix, windows };
}

function reduceProvider(operation, context) {
  const platform = operation.platform ?? "posix";
  if (!new Set(["posix", "windows"]).has(platform)) fail("platformUnsupported");
  if (operation.providerComparison !== undefined) compareProviderTraces(operation.providerComparison, context);
  const result = platform === "posix"
    ? reducePosixEvents(operation.providerEvents, context)
    : reduceWindowsEvents(operation.providerEvents, context);
  return { platform, result };
}

function updateResolution(document, resolutionPatch, packageSetDigest) {
  if (!document.build) fail("buildCoordinatorRequired");
  const current = clone(document.build.resolution);
  const next = merge(current, resolutionPatch ?? {});
  next.packageSetDigest = packageSetDigest;
  delete next.resolutionDigest;
  document.build.resolution = next;
  return next;
}

function transaction(state, operation) {
  const before = currentState(state);
  checkExpectedDigest(state, operation, before);
  if (operation.recheckDocumentDigest !== undefined && operation.recheckDocumentDigest !== before.documentDigest) fail("staleWrite", { expected: operation.recheckDocumentDigest, actual: before.documentDigest });
  const commandKey = operation.idempotencyKey;
  if (commandKey && state.commands.has(commandKey)) {
    return { status: "accepted", code: "duplicateCommand", duplicateOf: commandKey, documentDigest: state.documentDigest };
  }

  let next = clone(state.document);
  if (!["resolve", "add", "remove", "update", "deployment"].includes(operation.command)) fail("commandUnsupported");
  if (operation.command === "resolve") {
    if (operation.solve === "fail" || operation.fetch === "missing" || operation.policy === "missing") fail(operation.solve === "fail" ? "resolutionFailed" : operation.fetch === "missing" ? "externalFetchMissing" : "policyMissing");
    updateResolution(next, operation.resolution, before.packageSetDigest);
  } else if (operation.command === "deployment") {
    if (!next.build) fail("buildCoordinatorRequired");
    const name = operation.deployment?.name;
    const index = next.build.deployments.findIndex((entry) => entry.name === name);
    if (index < 0) fail("deploymentUnknown", { name });
    const patch = clone(operation.deployment);
    delete patch.name;
    next.build.deployments[index] = merge(next.build.deployments[index], patch);
  } else {
    if (operation.solve === "fail" || operation.fetch === "missing" || operation.policy === "missing") fail(operation.solve === "fail" ? "resolutionFailed" : operation.fetch === "missing" ? "externalFetchMissing" : "policyMissing");
    if (!next.build) fail("buildCoordinatorRequired");
    if (operation.packagePatch) {
      const packageRecord = next.packages.find(({ name }) => name === operation.packagePatch.name);
      if (!packageRecord) fail("packageUnknown", { name: operation.packagePatch.name });
      const fields = clone(operation.packagePatch.fields ?? {});
      delete fields.packageSetDigest;
      Object.assign(packageRecord, merge(packageRecord, fields));
    }
    if (operation.packageAppend) next.packages.push(clone(operation.packageAppend));
    if (operation.packageRemove) {
      const index = next.packages.findIndex(({ name }) => name === operation.packageRemove);
      if (index < 0) fail("packageUnknown", { name: operation.packageRemove });
      next.packages.splice(index, 1);
    }
    updateResolution(next, operation.resolution, derivePackageSetDigest(next.packages));
  }
  ensureNoDerivedFields(next);
  const after = deriveDocumentState(next);
  const packageSetChanged = after.packageSetDigest !== before.packageSetDigest;
  const resolutionChanged = after.resolutionDigest !== before.resolutionDigest;
  const deploymentChanged = JSON.stringify(after.deploymentDigests) !== JSON.stringify(before.deploymentDigests);
  if (operation.command === "resolve" && packageSetChanged) fail("resolutionChangedPackageSet");
  if (operation.command === "deployment" && (packageSetChanged || resolutionChanged)) fail("deploymentChangedPackageSet");

  const dryRun = operation.dryRun === true;
  if (dryRun) {
    return {
      status: "accepted",
      code: "dryRun",
      dryRun: true,
      packageSetDigest: after.packageSetDigest,
      packageDigests: after.packageDigests,
      resolutionDigest: after.resolutionDigest,
      deploymentDigests: after.deploymentDigests,
      buildPlanDigest: after.buildPlanDigest,
      documentDigest: before.documentDigest,
      wouldDocumentDigest: after.documentDigest,
      changed: { packageSet: packageSetChanged, resolution: resolutionChanged, deployment: deploymentChanged, buildPlan: after.buildPlanDigest !== before.buildPlanDigest },
    };
  }

  const providerContext = { oldDocumentDigest: before.documentDigest, newDocumentDigest: after.documentDigest };
  const { platform, result: provider } = reduceProvider(operation, providerContext);
  state.temporary += 1;
  try {
    if (operation.recheckDocumentDigest !== undefined && operation.recheckDocumentDigest !== before.documentDigest) fail("staleWrite");
    if (provider.fault === "crashBeforeReplace") {
      throw new Pkg1TransactionError("crashBeforeReplace", { atomicVisible: false, published: "old" });
    }
    if (provider.fault === "crashAfterReplace") {
      state.document = next;
      state.bytes = after.bytes;
      state.documentDigest = after.documentDigest;
      throw new Pkg1TransactionError("crashAfterReplace", { atomicVisible: true, published: "new" });
    }
    state.document = next;
    state.bytes = after.bytes;
    state.documentDigest = after.documentDigest;
    const reopened = currentState(state);
    if (reopened.documentDigest !== after.documentDigest || reopened.packageSetDigest !== after.packageSetDigest || reopened.resolutionDigest !== after.resolutionDigest || reopened.buildPlanDigest !== after.buildPlanDigest) fail("publicationReceiptMismatch");
    const crashDurable = provider.crashDurable === true;
    const receipt = {
      schema: "w.project-transaction-receipt/1",
      command: operation.command,
      platform,
      oldDocumentDigest: before.documentDigest,
      newDocumentDigest: after.documentDigest,
      packageSetDigest: after.packageSetDigest,
      resolutionDigest: after.resolutionDigest,
      buildPlanDigest: after.buildPlanDigest,
      deploymentDigests: after.deploymentDigests,
      atomicVisible: true,
      crashDurable,
      durabilityEvidence: provider.durabilityEvidence,
      providerReceiptDigest: provider.providerReceiptDigest,
    };
    state.receipts.push(receipt);
    if (commandKey) state.commands.set(commandKey, receipt);
    return {
      status: "accepted",
      code: "replaced",
      route: platform === "posix" ? "posix-rename" : "windows-replacefile",
      packageSetDigest: after.packageSetDigest,
      packageDigests: after.packageDigests,
      resolutionDigest: after.resolutionDigest,
      deploymentDigests: after.deploymentDigests,
      buildPlanDigest: after.buildPlanDigest,
      documentDigest: after.documentDigest,
      atomicVisible: true,
      crashDurable,
      durabilityEvidence: receipt.durabilityEvidence,
      providerReceiptDigest: receipt.providerReceiptDigest,
      changed: { packageSet: packageSetChanged, resolution: resolutionChanged, deployment: deploymentChanged, buildPlan: after.buildPlanDigest !== before.buildPlanDigest },
    };
  } finally {
    state.temporary = Math.max(0, state.temporary - 1);
  }
}

export function applyPkg1Operation(state, operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") fail("operationSchemaInvalid");
  if (operation.op !== "initialize") {
    rejectCallerEcho(operation);
    rejectLegacyProviderClaims(operation);
  }
  if (operation.op === "initialize") return initialize(state, operation);
  if (operation.op === "transaction") return transaction(state, operation);
  if (operation.op === "assert") {
    const derived = currentState(state);
    for (const [key, value] of Object.entries(operation.facts ?? {})) {
      const actual = key in derived ? derived[key] : state[key];
      if (actual !== value) fail("assertionFailed", { key });
    }
    return { status: "accepted", code: "asserted", documentDigest: derived.documentDigest };
  }
  fail("operationUnsupported");
}

export function runPkg1Program(testCase) {
  const state = makeInitialState();
  const trace = [];
  try {
    for (const operation of testCase.operations ?? []) trace.push({ op: operation.op, outcome: applyPkg1Operation(state, clone(operation)) });
    const derived = state.document ? currentState(state) : undefined;
    return {
      status: "accepted",
      code: "programComplete",
      trace,
      state: stateSnapshot(state),
      final: derived
        ? { packageSetDigest: derived.packageSetDigest, packageDigests: derived.packageDigests, resolutionDigest: derived.resolutionDigest, deploymentDigests: derived.deploymentDigests, buildPlanDigest: derived.buildPlanDigest, documentDigest: derived.documentDigest }
        : null,
    };
  } catch (error) {
    if (!(error instanceof Pkg1TransactionError)) throw error;
    return {
      status: error.code === "crashAfterReplace" ? "faulted" : "rejected",
      code: error.code,
      details: error.details,
      trace,
      state: stateSnapshot(state),
    };
  }
}

export function derivePkg1Case(testCase) {
  if (!testCase || typeof testCase.id !== "string") throw new Error("PKG1 case requires id");
  return { caseId: testCase.id, ...runPkg1Program(testCase) };
}

function expandPkg1Operations(corpus, testCase, stack = []) {
  const fixtureNames = testCase.fixtures ?? [];
  const fixtureOperations = fixtureNames.flatMap((name) => {
    if (stack.includes(name)) fail("fixtureCycle", { cycle: [...stack, name] });
    const fixture = corpus.fixtures?.[name];
    if (!fixture) fail("unknownFixture", { name });
    return [
      ...(fixture.includes ? expandPkg1Operations(corpus, { fixtures: fixture.includes, operations: [] }, [...stack, name]) : []),
      ...(fixture.operations ?? []),
    ];
  });
  return [...fixtureOperations, ...(testCase.operations ?? [])];
}

export function derivePkg1(corpus) {
  return (corpus.cases ?? []).map((testCase) => derivePkg1Case({ ...testCase, operations: expandPkg1Operations(corpus, testCase) }));
}

export function validatePkg1Operation(operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") return false;
  if (operation.op === "initialize") return !!operation.document && typeof operation.document === "object";
  if (operation.op === "assert") return !!operation.facts && typeof operation.facts === "object";
  if (operation.op !== "transaction") return false;
  if (typeof operation.command !== "string" || typeof operation.expectedDocumentDigest !== "string") return false;
  return true;
}

export function validatePkg1(corpus, { root } = {}) {
  const errors = [];
  if (corpus?.$schema !== "w-pkg1-project-transaction-cases-1") errors.push("PKG1 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("PKG1 corpus status must be design-oracle-input.");
  if (!Array.isArray(corpus?.cases) || corpus.cases.length < 20) errors.push("PKG1 corpus must contain at least 20 cases.");
  const ids = new Set();
  const operations = new Set();
  for (const [index, testCase] of (corpus.cases ?? []).entries()) {
    if (!/^PKG1-[a-z0-9]+(?:-[a-z0-9]+)*$/u.test(testCase.id ?? "")) errors.push(`cases[${index}] id is invalid.`);
    if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}.`);
    ids.add(testCase.id);
    const expandedOperations = (() => {
      const output = [];
      for (const name of testCase.fixtures ?? []) {
        const fixture = corpus.fixtures?.[name];
        if (!fixture) {
          errors.push(`${testCase.id} references unknown fixture ${name}.`);
          continue;
        }
        output.push(...(fixture.operations ?? []));
      }
      output.push(...(testCase.operations ?? []));
      return output;
    })();
    if (expandedOperations.length === 0) errors.push(`${testCase.id} operations are missing.`);
    for (const operation of expandedOperations) {
      if (!validatePkg1Operation(operation)) errors.push(`${testCase.id} has malformed operation.`);
      if (operation?.op === "transaction") operations.add(operation.command);
    }
    if (!Array.isArray(testCase.references) || testCase.references.length === 0) errors.push(`${testCase.id} must reference Last Light.`);
    if (root && testCase.references) {
      for (const reference of testCase.references) {
        if (typeof reference.path !== "string" || typeof reference.symbol !== "string") errors.push(`${testCase.id} reference is malformed.`);
      }
    }
    if (!testCase.expected || !["accepted", "rejected", "faulted"].includes(testCase.expected.status)) errors.push(`${testCase.id} expected status is invalid.`);
  }
  for (const command of ["resolve", "add", "remove", "update", "deployment"]) if (!operations.has(command)) errors.push(`PKG1 corpus does not cover ${command}.`);
  return { errors, results: derivePkg1(corpus) };
}
