import { createHash } from "node:crypto";

/**
 * Host-only RU0 machine for an ephemeral local module graph.
 *
 * The machine consumes parser evidence and provider facts. It does not read a
 * filesystem, scan a directory, resolve a registry, or execute W source.
 * Physical paths and provider tokens remain provenance and never enter the
 * logical recipe.
 */
export const EPHEMERAL_FIXTURE_LIMITS = Object.freeze({
  maxSources: 64,
  maxEdges: 4_096,
  maxDepth: 64,
  maxTotalSourceBytes: 16 * 1024 * 1024,
});

const IMPORT_KINDS = new Set(["import", "reexport", "service-import"]);

export class EphemeralModuleGraphError extends Error {
  constructor(code, facts = {}) {
    super(code);
    this.name = "EphemeralModuleGraphError";
    this.code = code;
    this.facts = facts;
  }
}

function fail(code, facts = {}) {
  throw new EphemeralModuleGraphError(code, facts);
}

function clone(value) {
  return value === undefined ? undefined : structuredClone(value);
}

function text(value) {
  return typeof value === "string" ? value : null;
}

function nonEmptyText(value) {
  return typeof value === "string" && value.length > 0 ? value : null;
}

function bytesCompare(left, right) {
  return Buffer.compare(Buffer.from(left, "utf8"), Buffer.from(right, "utf8"));
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

function digest(tag, value) {
  const bytes = typeof value === "string" ? value : JSON.stringify(canonical(value));
  return `sha256:${createHash("sha256").update(`${tag}\0${bytes}`, "utf8").digest("hex")}`;
}

/** Return the source digest used by provider snapshot evidence. */
export function ephemeralSourceDigest(sourceText) {
  if (typeof sourceText !== "string") fail("sourceBytesRequired");
  return digest("w-module-source-v1", sourceText);
}

function normalizeSourceId(value, location = "sourceId") {
  const raw = nonEmptyText(value);
  if (!raw) fail("sourceIdMalformed", { location });
  const normalized = raw.normalize("NFC");
  if (normalized.includes("\\") || normalized.includes("\0") || normalized.startsWith("/") || /^[A-Za-z]:/.test(normalized)) {
    fail("sourceIdMalformed", { location, sourceId: raw });
  }
  const parts = normalized.split("/");
  if (!normalized.endsWith(".w") || normalized === ".w" || parts.some((part) => part.length === 0 || part === "." || part === ".." || part === ".w")) {
    fail("sourceIdMalformed", { location, sourceId: raw });
  }
  return normalized;
}

function normalizeImportPath(value, location = "import") {
  const raw = nonEmptyText(value);
  if (!raw) fail("importMalformed", { location });
  const normalized = raw.normalize("NFC");
  if (normalized.includes("/") || normalized.includes("\\") || normalized.includes("\0")) {
    fail("importComponentRejected", { location, path: raw, reason: "separator" });
  }
  const components = normalized.split(".");
  if (components.some((component) => component.length === 0 || component === "." || component === ".." || /\s/.test(component))) {
    fail("importComponentRejected", { location, path: raw, reason: "component" });
  }
  return {
    raw,
    normalized,
    components,
    sourceId: `${components.join("/")}.w`,
  };
}

function headerName(record, location) {
  const value = record.moduleHeader ?? record.header;
  if (value === undefined || value === null) return null;
  const name = typeof value === "string"
    ? value
    : value && typeof value === "object"
      ? (value.name ?? value.modulePath)
      : null;
  const normalized = nonEmptyText(name)?.normalize("NFC");
  if (!normalized || normalized.includes(".") || normalized.includes("/") || normalized.includes("\\") || /\s|\0/.test(normalized)) {
    fail("moduleHeaderMalformed", { location });
  }
  return normalized;
}

function logicalStem(sourceId) {
  return sourceId.slice(0, -2).split("/").join(".");
}

function normalizeImports(record, location) {
  const imports = record.imports ?? record.parseEvidence?.imports;
  if (!Array.isArray(imports)) fail("importsMalformed", { location });
  return imports.map((item, index) => {
    const source = typeof item === "string" ? { path: item } : item;
    if (!source || typeof source !== "object") fail("importMalformed", { location, index });
    const normalized = normalizeImportPath(source.path, `${location}[${index}]`);
    const kind = source.kind ?? source.originKind ?? "import";
    if (!IMPORT_KINDS.has(kind)) fail("importKindRejected", { location, index, kind });
    return {
      raw: normalized.raw,
      path: normalized.normalized,
      sourceId: normalized.sourceId,
      kind,
      external: source.external === true,
    };
  });
}

function providerFacts(record, location) {
  const provider = record.providerFacts ?? record.providerEvidence ?? record.provider;
  if (!provider || typeof provider !== "object") fail("providerFactMissing", { location, field: "provider" });
  const providerId = nonEmptyText(provider.providerId ?? provider.id ?? provider.name);
  const rootToken = nonEmptyText(provider.rootToken ?? provider.rootId ?? provider.root);
  const owner = nonEmptyText(provider.owner ?? provider.ownerId);
  const canonicalToken = nonEmptyText(
    provider.canonicalToken ?? provider.canonical ?? provider.token ?? provider.canonicalPath?.token,
  );
  if (!providerId || !rootToken || !owner || !canonicalToken) {
    fail("providerFactMissing", { location, fields: ["providerId", "rootToken", "owner", "canonicalToken"] });
  }
  const opened = provider.opened ?? provider.openConfirmed ?? provider.open;
  if (opened !== true) fail("providerRootUnconfirmed", { location });
  const containment = provider.containment ?? provider.containmentProof;
  if (containment !== "inside") fail("outsideContainment", { location, containment: containment ?? null });

  const symlink = provider.symlink ?? provider.link;
  if (provider.symlinkEscape === true || provider.escape === true ||
      (symlink && typeof symlink === "object" &&
        (symlink.escape === true || symlink.inside === false ||
          symlink.tokenUnique === false || symlink.uniqueToken === false ||
          (symlink.containment !== undefined && symlink.containment !== "inside"))) ||
      (symlink && symlink !== true && typeof symlink !== "object")) {
    fail("symlinkEscape", { location });
  }
  if (symlink === true && provider.symlinkContainment !== "inside") {
    fail("symlinkEscape", { location });
  }

  const snapshot = record.snapshot ?? provider.snapshot ?? provider.stableSnapshot;
  if (!snapshot || typeof snapshot !== "object") fail("providerFactMissing", { location, field: "snapshot" });
  if (snapshot.stable !== true || provider.snapshotStable === false || provider.mutated === true) {
    fail("unstableSnapshot", { location });
  }
  const sourceText = record.sourceText ?? record.text;
  if (typeof sourceText !== "string") fail("sourceBytesRequired", { location });
  const sourceDigest = ephemeralSourceDigest(sourceText);
  const byteLength = Buffer.byteLength(sourceText, "utf8");
  const observedBytes = snapshot.bytes ?? snapshot.byteLength ?? record.byteLength;
  if (!Number.isSafeInteger(observedBytes) || observedBytes < 0 || observedBytes !== byteLength) {
    fail("sourceBytesMismatch", { location, expected: byteLength, observed: observedBytes ?? null });
  }
  const observedDigests = [snapshot.digest, record.digest, record.sourceDigest].filter((value) => value !== undefined && value !== null);
  if (observedDigests.length === 0 || observedDigests.some((value) => value !== sourceDigest)) {
    fail("sourceDigestMismatch", { location, expected: sourceDigest, observed: observedDigests });
  }
  for (const key of ["before", "after"]) {
    if (snapshot[key] === undefined) continue;
    const sample = snapshot[key];
    if (!sample || sample.bytes !== byteLength || sample.digest !== sourceDigest) {
      fail("unstableSnapshot", { location, phase: key });
    }
  }
  return {
    providerId,
    rootToken,
    owner,
    canonicalToken,
    containment,
    byteLength,
    digest: sourceDigest,
    physicalDisplay: record.physicalDisplay ?? record.path ?? null,
    symlink: symlink === true || (symlink && typeof symlink === "object") ? true : false,
  };
}

function limitsFor(evidence, options) {
  const provided = options.limits ?? evidence.limits ?? evidence.profile?.limits ?? evidence.provider?.limits;
  if (provided === undefined || provided === null) {
    return { ...EPHEMERAL_FIXTURE_LIMITS, source: "fixture" };
  }
  if (!provided || typeof provided !== "object") fail("limitsMalformed");
  const aliases = {
    maxSources: ["maxSources", "sources"],
    maxEdges: ["maxEdges", "edges"],
    maxDepth: ["maxDepth", "depth"],
    maxTotalSourceBytes: ["maxTotalSourceBytes", "totalSourceBytes", "bytes"],
  };
  const resolved = { source: "provider" };
  for (const [name, keys] of Object.entries(aliases)) {
    const value = keys.map((key) => provided[key]).find((candidate) => candidate !== undefined);
    const minimum = name === "maxSources" ? 1 : 0;
    if (!Number.isSafeInteger(value) || value < minimum) fail("limitsMalformed", { field: name, value: value ?? null });
    resolved[name] = value;
  }
  return resolved;
}

function compareRootImports(actual, expected) {
  if (expected === undefined) return;
  const normalized = normalizeImports({ imports: expected }, "rootImports");
  if (normalized.length !== actual.length || normalized.some((item, index) =>
      item.path !== actual[index].path || item.external !== actual[index].external || item.kind !== actual[index].kind)) {
    fail("importEvidenceMismatch", { scope: "root" });
  }
}

function compareRootModuleHeader(rootRecord, expected) {
  if (expected === undefined) return;
  const actual = headerName(rootRecord, "root");
  const normalizedExpected = headerName({ moduleHeader: expected }, "rootModuleHeader");
  if (actual !== normalizedExpected) {
    fail("moduleHeaderEvidenceMismatch", { expected: normalizedExpected, actual });
  }
}

function candidateIndex(records) {
  return records.map((record, index) => ({ record, index, rawSourceId: record?.sourceId }));
}

function detectCycle(nodes, edges, ordinals) {
  const adjacency = new Map([...nodes.keys()].map((id) => [id, []]));
  for (const edge of edges) {
    if (edge.provider === "local") adjacency.get(edge.sourceId).push(edge.targetSourceId);
  }
  for (const targets of adjacency.values()) targets.sort((a, b) => (ordinals.get(a) ?? 0) - (ordinals.get(b) ?? 0));
  let nextIndex = 0;
  const stack = [];
  const onStack = new Set();
  const indices = new Map();
  const lowLinks = new Map();
  const components = [];
  function strongConnect(id) {
    indices.set(id, nextIndex);
    lowLinks.set(id, nextIndex);
    nextIndex += 1;
    stack.push(id);
    onStack.add(id);
    for (const target of adjacency.get(id)) {
      if (!indices.has(target)) {
        strongConnect(target);
        lowLinks.set(id, Math.min(lowLinks.get(id), lowLinks.get(target)));
      } else if (onStack.has(target)) {
        lowLinks.set(id, Math.min(lowLinks.get(id), indices.get(target)));
      }
    }
    if (lowLinks.get(id) !== indices.get(id)) return;
    const component = [];
    let member;
    do {
      member = stack.pop();
      onStack.delete(member);
      component.push(member);
    } while (member !== id);
    component.sort((a, b) => (ordinals.get(a) ?? 0) - (ordinals.get(b) ?? 0));
    components.push(component);
  }
  [...nodes.keys()].sort((a, b) => (ordinals.get(a) ?? 0) - (ordinals.get(b) ?? 0)).forEach((id) => {
    if (!indices.has(id)) strongConnect(id);
  });
  const cycle = components.find((component) => component.length > 1 ||
    (component.length === 1 && adjacency.get(component[0]).includes(component[0])));
  if (cycle) fail("moduleGraphCycle", { component: cycle });
}

function longestRootDepth(nodes, edges, ordinals, rootSourceId) {
  const adjacency = new Map([...nodes.keys()].map((id) => [id, []]));
  const indegree = new Map([...nodes.keys()].map((id) => [id, 0]));
  for (const edge of edges) {
    if (edge.provider !== "local") continue;
    adjacency.get(edge.sourceId).push(edge);
    indegree.set(edge.targetSourceId, indegree.get(edge.targetSourceId) + 1);
  }
  for (const outgoing of adjacency.values()) {
    outgoing.sort((left, right) =>
      (ordinals.get(left.targetSourceId) ?? 0) - (ordinals.get(right.targetSourceId) ?? 0) ||
      bytesCompare(left.origin, right.origin) ||
      bytesCompare(left.kind, right.kind) ||
      bytesCompare(left.provider, right.provider));
  }
  const depth = new Map([...nodes.keys()].map((id) => [id, Number.NEGATIVE_INFINITY]));
  depth.set(rootSourceId, 0);
  const ready = [...nodes.keys()].filter((id) => indegree.get(id) === 0);
  const ordinalCompare = (left, right) => (ordinals.get(left) ?? 0) - (ordinals.get(right) ?? 0);
  ready.sort(ordinalCompare);
  let processed = 0;
  while (ready.length > 0) {
    const sourceId = ready.shift();
    processed += 1;
    const sourceDepth = depth.get(sourceId);
    for (const edge of adjacency.get(sourceId)) {
      const targetDepth = sourceDepth + 1;
      if (targetDepth > depth.get(edge.targetSourceId)) depth.set(edge.targetSourceId, targetDepth);
      const remaining = indegree.get(edge.targetSourceId) - 1;
      indegree.set(edge.targetSourceId, remaining);
      if (remaining === 0) {
        ready.push(edge.targetSourceId);
        ready.sort(ordinalCompare);
      }
    }
  }
  if (processed !== nodes.size || [...depth.values()].some((value) => value === Number.NEGATIVE_INFINITY)) {
    fail("moduleGraphCycle");
  }
  return depth;
}

function logicalRecipe(inventory, edges) {
  return {
    schema: "w.ephemeral-module-recipe/1",
    inventory: inventory.map(({ sourceId, modulePath, digest: sourceDigest }) => ({ sourceId, modulePath, digest: sourceDigest })),
    edges: edges.map(({ sourceOrdinal, targetOrdinal, sourceId, targetSourceId, origin, provider, kind }) => ({
      sourceOrdinal,
      targetOrdinal,
      source: sourceId,
      target: targetSourceId,
      origin,
      provider,
      kind,
    })),
  };
}

/** Derive an ephemeral graph from parser and provider evidence. Throws on failure. */
export function buildEphemeralModuleGraph(evidence, options = {}) {
  if (!evidence || typeof evidence !== "object") fail("graphEvidenceMalformed");
  const rootRecord = evidence.root ?? evidence.rootSource;
  if (!rootRecord || typeof rootRecord !== "object") fail("rootEvidenceMissing");
  const rootSourceId = normalizeSourceId(evidence.rootSourceId ?? rootRecord.sourceId, "rootSourceId");
  if (rootSourceId.includes("/")) fail("rootSourceIdNested", { sourceId: rootSourceId });
  const records = Array.isArray(evidence.sources) ? evidence.sources : [];
  const limits = limitsFor(evidence, options);
  if (limits.maxSources < 1) fail("limitsMalformed", { field: "maxSources" });

  const candidates = candidateIndex(records);
  const rootImports = normalizeImports(rootRecord, "root.imports");
  compareRootImports(rootImports, options.rootImports);
  compareRootModuleHeader(rootRecord, options.rootModuleHeader);
  const rootFacts = providerFacts(rootRecord, "root");
  const expectedRootSourceDigest = options.rootSourceBytesDigest ?? options.rootSourceDigest;
  if (expectedRootSourceDigest !== undefined && rootFacts.digest !== expectedRootSourceDigest) {
    fail("rootSourceDigestMismatch", { expected: expectedRootSourceDigest, observed: rootFacts.digest });
  }
  const providerIdentity = {
    providerId: rootFacts.providerId,
    rootToken: rootFacts.rootToken,
    owner: rootFacts.owner,
  };
  const nodes = new Map();
  const sourceFacts = new Map();
  const sourceRawIds = new Map();
  const canonicalTokens = new Map();
  const modulePaths = new Map();
  let totalSourceBytes = 0;
  function addNode(record, sourceId, modulePath, location) {
    const facts = providerFacts(record, location);
    if (facts.providerId !== providerIdentity.providerId || facts.rootToken !== providerIdentity.rootToken || facts.owner !== providerIdentity.owner) {
      fail("providerMismatch", { location, expected: providerIdentity, observed: { providerId: facts.providerId, rootToken: facts.rootToken, owner: facts.owner } });
    }
    if (sourceRawIds.has(sourceId) && sourceRawIds.get(sourceId) !== record.sourceId) {
      fail("nfcLogicalCollision", { sourceId, spellings: [sourceRawIds.get(sourceId), record.sourceId] });
    }
    const header = headerName(record, location);
    const effectiveModulePath = modulePath ?? header ?? logicalStem(sourceId);
    if (modulePath !== null && modulePath !== undefined && header !== null && header !== modulePath.split(".").at(-1)) {
      fail("moduleHeaderMismatch", { sourceId, expected: modulePath.split(".").at(-1), actual: header });
    }
    if (modulePaths.has(effectiveModulePath) && modulePaths.get(effectiveModulePath) !== sourceId) {
      fail("multiFileModule", { modulePath: effectiveModulePath, sources: [modulePaths.get(effectiveModulePath), sourceId] });
    }
    const previousSourceId = canonicalTokens.get(facts.canonicalToken);
    if (previousSourceId !== undefined && previousSourceId !== sourceId) {
      fail("duplicateCanonicalToken", { canonicalToken: facts.canonicalToken, sources: [previousSourceId, sourceId] });
    }
    if (nodes.size + 1 > limits.maxSources) fail("ephemeralSourceLimit", { sources: nodes.size + 1, limit: limits.maxSources });
    totalSourceBytes += facts.byteLength;
    if (totalSourceBytes > limits.maxTotalSourceBytes) fail("ephemeralSourceBytesLimit", { bytes: totalSourceBytes, limit: limits.maxTotalSourceBytes });
    nodes.set(sourceId, { record, sourceId, modulePath: effectiveModulePath, imports: normalizeImports(record, `${location}.imports`) });
    sourceFacts.set(sourceId, facts);
    sourceRawIds.set(sourceId, record.sourceId ?? sourceId);
    canonicalTokens.set(facts.canonicalToken, sourceId);
    modulePaths.set(effectiveModulePath, sourceId);
    const importSpellings = new Map();
    for (const item of nodes.get(sourceId).imports) {
      const previous = importSpellings.get(item.path);
      if (previous !== undefined && previous !== item.raw) {
        fail("nfcLogicalCollision", { sourceId, path: item.path, spellings: [previous, item.raw] });
      }
      importSpellings.set(item.path, item.raw);
    }
  }

  addNode(rootRecord, rootSourceId, headerName(rootRecord, "root") ?? logicalStem(rootSourceId), "root");
  const edges = [];
  const queue = [rootSourceId];
  let cursor = 0;
  while (cursor < queue.length) {
    const sourceId = queue[cursor++];
    const node = nodes.get(sourceId);
    for (const item of node.imports) {
      if (edges.length >= limits.maxEdges) fail("ephemeralEdgeLimit", { edges: edges.length + 1, limit: limits.maxEdges });
      if (item.path === "std" || item.path.startsWith("std.")) {
        edges.push({ sourceId, targetSourceId: null, origin: item.path, provider: "std", kind: item.kind });
        continue;
      }
      if (item.external) {
        fail("externalDependencyInEphemeral", { origin: item.path, targetSourceId: item.sourceId, guidance: "create-or-adopt-package-workspace" });
      }
      const targetSourceId = item.path === node.modulePath && item.path === nodes.get(rootSourceId).modulePath
        ? rootSourceId
        : item.path === nodes.get(rootSourceId).modulePath
          ? rootSourceId
          : item.sourceId;
      if (targetSourceId === rootSourceId) {
        edges.push({ sourceId, targetSourceId, origin: item.path, provider: "local", kind: item.kind });
        continue;
      }
      const matching = candidates.filter((candidate) => {
        try {
          return normalizeSourceId(candidate.rawSourceId, `sources[${candidate.index}].sourceId`) === targetSourceId;
        } catch (error) {
          if (error instanceof EphemeralModuleGraphError &&
              ["sourceIdMalformed", "providerFactMissing"].includes(error.code)) return false;
          throw error;
        }
      });
      if (matching.length === 0) fail("externalDependencyInEphemeral", { origin: item.path, targetSourceId, guidance: "create-or-adopt-package-workspace" });
      if (matching.length > 1) fail("nfcLogicalCollision", { sourceId: targetSourceId, spellings: matching.map((candidate) => candidate.rawSourceId) });
      if (!nodes.has(targetSourceId)) {
        addNode(matching[0].record, targetSourceId, item.path, `sources[${matching[0].index}]`);
        queue.push(targetSourceId);
      } else if (nodes.get(targetSourceId).modulePath !== item.path) {
        fail("multiFileModule", { sourceId: targetSourceId, expected: item.path, actual: nodes.get(targetSourceId).modulePath });
      }
      edges.push({ sourceId, targetSourceId, origin: item.path, provider: "local", kind: item.kind });
    }
  }

  const inventory = [...nodes.values()]
    .sort((left, right) => left.sourceId === right.sourceId ? 0 :
      left.sourceId === rootSourceId ? -1 : right.sourceId === rootSourceId ? 1 : bytesCompare(left.sourceId, right.sourceId))
    .map((node, ordinal) => ({
      ordinal,
      sourceId: node.sourceId,
      modulePath: node.modulePath,
      digest: sourceFacts.get(node.sourceId).digest,
    }));
  const ordinals = new Map(inventory.map((item) => [item.sourceId, item.ordinal]));
  detectCycle(nodes, edges, ordinals);
  const depths = longestRootDepth(nodes, edges, ordinals, rootSourceId);
  for (const item of inventory) {
    item.depth = depths.get(item.sourceId);
    if (item.depth > limits.maxDepth) {
      fail("ephemeralDepthLimit", { depth: item.depth, limit: limits.maxDepth, sourceId: item.sourceId });
    }
  }
  const orderedEdges = edges
    .map((edge) => ({
      ...edge,
      sourceOrdinal: ordinals.get(edge.sourceId),
      targetOrdinal: edge.targetSourceId === null ? null : ordinals.get(edge.targetSourceId),
    }))
    .sort((left, right) =>
      left.sourceOrdinal - right.sourceOrdinal || bytesCompare(left.origin, right.origin) ||
      (left.targetOrdinal ?? Number.MAX_SAFE_INTEGER) - (right.targetOrdinal ?? Number.MAX_SAFE_INTEGER) ||
      bytesCompare(left.kind, right.kind) || bytesCompare(left.provider, right.provider));
  const recipe = logicalRecipe(inventory, orderedEdges);
  const recipeKey = digest("w-ephemeral-module-recipe-v1", recipe);
  return {
    schema: "w.ephemeral-module-graph/1",
    root: {
      sourceId: rootSourceId,
      modulePath: nodes.get(rootSourceId).modulePath,
      ordinal: 0,
    },
    inventory,
    edges: orderedEdges,
    recipe,
    recipeKey,
    limits: {
      maxSources: limits.maxSources,
      maxEdges: limits.maxEdges,
      maxDepth: limits.maxDepth,
      maxTotalSourceBytes: limits.maxTotalSourceBytes,
      source: limits.source,
    },
    provenance: {
      providerId: providerIdentity.providerId,
      rootToken: providerIdentity.rootToken,
      owner: providerIdentity.owner,
      sources: inventory.map((item) => {
        const facts = sourceFacts.get(item.sourceId);
        return {
          sourceId: item.sourceId,
          canonicalToken: facts.canonicalToken,
          physicalDisplay: facts.physicalDisplay,
          containment: facts.containment,
          symlink: facts.symlink,
        };
      }),
    },
    counts: {
      sources: inventory.length,
      edges: orderedEdges.length,
      depth: Math.max(...inventory.map((item) => item.depth)),
      totalSourceBytes,
    },
  };
}

/** Return a bounded result record instead of throwing for RU0 corpus checks. */
export function runEphemeralModuleGraph(evidence, options = {}) {
  try {
    return { status: "accepted", graph: buildEphemeralModuleGraph(evidence, options) };
  } catch (error) {
    if (error instanceof EphemeralModuleGraphError) {
      return { status: "rejected", code: error.code, facts: clone(error.facts) };
    }
    return { status: "rejected", code: "oracleFailure", facts: { message: String(error?.message ?? error) } };
  }
}
