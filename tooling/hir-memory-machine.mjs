const OWNER_STATES = new Set(["owned", "moved", "dropped"]);
const ADDRESS_STATES = new Set(["unstable", "stable", "published"]);
const BOUNDARIES = new Set([
  "internal",
  "wExact",
  "foreignC",
  "wire",
  "persisted",
  "capability",
]);
const REPRESENTATIONS = new Set([
  "explicitTag",
  "provenNiche",
  "lowBit",
  "highBit",
  "nativeCarrier",
]);
const BORROW_MODES = new Set(["shared", "exclusive"]);
const ACCESS_MODES = new Set(["read", "write"]);
const DEPENDENCY_KINDS = new Set(["dynamic", "static", "immortal"]);
const FFI_FORMS = new Set(["ref", "inout", "languageFn"]);
const FFI_RETENTIONS = new Set(["none", "call", "persistent"]);
const ALLOCATOR_LIFETIMES = new Set(["static", "product", "parameter", "scoped"]);
const ALLOCATOR_MOBILITIES = new Set(["local", "crossDomain"]);
const AMBIGUOUS_BODYLESS_RESULT = "W-BORROW-0011";
const ALLOCATION_OUTCOMES = new Set(["success", "allocationError"]);
const PIN_CONSTRUCT_OUTCOMES = new Set([
  "success",
  "argumentError",
  "allocationError",
  "initializerError",
]);
const ERASURE_SPILL_POLICIES = new Set(["forbid", "allocator"]);
const SHARED_EDGE_MODES = new Set(["strong", "weak"]);
const SHARED_EDGE_RELEASES = new Set(["deinit", "explicitClose", "lifecycleDrain"]);
const SHARED_GRAPH_PHASES = new Set(["compile", "drainedBoundary"]);
const PROJECTION_KINDS = new Set([
  "field",
  "tuple",
  "enumPayload",
  "index",
  "range",
  "deref",
  "view",
  "union",
  "packed",
  "unaligned",
  "foreignBoundary",
]);
const ESCAPE_TARGETS = new Set([
  "return",
  "dependentStore",
  "static",
  "global",
  "shared",
  "service",
  "channel",
  "wire",
  "persistence",
  "detachedTask",
  "foreignRetention",
  "share",
  "structuredChild",
]);

export class HirMemoryError extends Error {
  constructor(code, facts = null) {
    super(code);
    this.code = code;
    this.facts = facts;
  }
}

function clone(value) {
  return structuredClone(value);
}

function activeLoansForPayload(state, payload) {
  return Object.values(state.loans).filter((loan) => loan.payload === payload);
}

function activeLoansForBinding(state, binding) {
  return Object.values(state.loans).filter((loan) => loan.binding === binding);
}

function requireBinding(state, name) {
  const binding = state.bindings[name];
  if (!binding || binding.state !== "owned") {
    throw new HirMemoryError("operationRequiresOwner");
  }
  return binding;
}

function allocatorTable(state) {
  if (!state.allocators) state.allocators = {};
  return state.allocators;
}

function controlBlockTable(state) {
  if (!state.controlBlocks) state.controlBlocks = {};
  return state.controlBlocks;
}

function classifySharedGraph(operation) {
  const nodes = [...operation.nodes].sort();
  const edges = [...operation.edges].sort((left, right) => left.id.localeCompare(right.id));
  const strongEdges = edges.filter((edge) => edge.mode === "strong");
  const adjacency = new Map(nodes.map((node) => [node, []]));
  for (const edge of strongEdges) adjacency.get(edge.from).push(edge.to);
  for (const targets of adjacency.values()) targets.sort();

  const rootReachable = new Set();
  const pendingRoots = [...(operation.externalRoots ?? [])].sort().reverse();
  while (pendingRoots.length > 0) {
    const node = pendingRoots.pop();
    if (rootReachable.has(node)) continue;
    rootReachable.add(node);
    for (const target of adjacency.get(node)) {
      if (!rootReachable.has(target)) pendingRoots.push(target);
    }
  }

  let nextIndex = 0;
  const stack = [];
  const onStack = new Set();
  const indexes = new Map();
  const lowLinks = new Map();
  const components = [];

  function visit(node) {
    indexes.set(node, nextIndex);
    lowLinks.set(node, nextIndex);
    nextIndex += 1;
    stack.push(node);
    onStack.add(node);

    for (const target of adjacency.get(node)) {
      if (!indexes.has(target)) {
        visit(target);
        lowLinks.set(node, Math.min(lowLinks.get(node), lowLinks.get(target)));
      } else if (onStack.has(target)) {
        lowLinks.set(node, Math.min(lowLinks.get(node), indexes.get(target)));
      }
    }

    if (lowLinks.get(node) !== indexes.get(node)) return;
    const component = [];
    while (stack.length > 0) {
      const member = stack.pop();
      onStack.delete(member);
      component.push(member);
      if (member === node) break;
    }
    component.sort();
    components.push(component);
  }

  for (const node of nodes) {
    if (!indexes.has(node)) visit(node);
  }

  const cycles = components
    .filter((component) => {
      if (component.length > 1) return true;
      return strongEdges.some(
        (edge) => edge.from === component[0] && edge.to === component[0],
      );
    })
    .map((component) => {
      const members = new Set(component);
      const internalEdges = strongEdges.filter(
        (edge) => members.has(edge.from) && members.has(edge.to),
      );
      return {
        nodes: component,
        edges: internalEdges.map((edge) => edge.id),
        origins: internalEdges.map((edge) => edge.origin),
        breakEdges: internalEdges
          .filter((edge) => edge.release !== "deinit")
          .map((edge) => edge.id),
        rooted: component.some((node) => rootReachable.has(node)),
        unbreakable: internalEdges.every((edge) => edge.release === "deinit"),
      };
    })
    .sort((left, right) => left.nodes[0].localeCompare(right.nodes[0]));

  return {
    phase: operation.phase,
    closed: operation.closed,
    drained: operation.drained ?? null,
    boundary: operation.boundary ?? null,
    externalRoots: [...(operation.externalRoots ?? [])].sort(),
    cycles,
  };
}

function outcomeTable(state) {
  if (!state.outcomes) state.outcomes = {};
  return state.outcomes;
}

function constructionReceiptTable(state) {
  if (!state.constructionReceipts) state.constructionReceipts = [];
  return state.constructionReceipts;
}

function requireAllocator(state, name) {
  const allocator = allocatorTable(state)[name];
  if (!allocator || allocator.state !== "open") {
    throw new HirMemoryError("allocatorUnavailable");
  }
  return allocator;
}

function chargeAllocator(state, name, bytes = 0) {
  const allocator = requireAllocator(state, name);
  const charge = Number.isSafeInteger(bytes) ? bytes : 0;
  const nextCharge = allocator.charged + charge;
  if (!Number.isSafeInteger(nextCharge)) {
    throw new HirMemoryError("sizeOverflow");
  }
  if (allocator.limit !== null && nextCharge > allocator.limit) {
    throw new HirMemoryError("budgetExceeded", {
      limitBytes: allocator.limit,
      committedBytes: allocator.charged,
      requestedBytes: charge,
    });
  }
  allocator.charged = nextCharge;
  return allocator;
}

function allocatorOutlives(state, candidateName, dependentName) {
  if (candidateName === dependentName) return true;
  const candidate = requireAllocator(state, candidateName);
  if (candidate.lifetime === "static" || candidate.lifetime === "product") return true;
  return candidate.outlives.includes(dependentName);
}

function storageOrigins(payload) {
  return Array.isArray(payload.storageOrigins) ? payload.storageOrigins : [];
}

function storageIsTransferable(state, payload) {
  return storageOrigins(payload).every(
    (origin) => requireAllocator(state, origin).mobility === "crossDomain",
  );
}

function recordOutcome(state, operation, outcome) {
  if (operation.result) outcomeTable(state)[operation.result] = outcome;
}

function consumeAfterAllocationFailure(state, source) {
  const payload = state.payloads[source.payload];
  releasePayloadEdges(payload);
  source.state = "dropped";
  payload.consumedOnFailure = true;
  payload.dropCount = (payload.dropCount ?? 0) + 1;
}

function consumeErasureAllocationFailure(state, source, payload, operation) {
  payload.erasureAttempt = {
    storage: "spill",
    boxOrigin: operation.using,
    outcome: "allocationError",
  };
  consumeAfterAllocationFailure(state, source);
  recordOutcome(state, operation, "allocationError");
}

function requireUniqueBinding(state, name) {
  const binding = requireBinding(state, name);
  if (binding.pinnedHandle || binding.sharedBlock || binding.weakBlock) {
    throw new HirMemoryError("operationRequiresUniqueOwner");
  }
  return binding;
}

function requireNoLoans(state, payload, code = "loanConflict", allowPinnedHandle = false, binding = null) {
  if (allowPinnedHandle && binding?.pinnedHandle) return;
  if (activeLoansForPayload(state, payload).length > 0) {
    throw new HirMemoryError(code);
  }
}

function requireNoBindingLoans(state, binding, code = "loanConflict") {
  if (activeLoansForBinding(state, binding).length > 0) {
    throw new HirMemoryError(code);
  }
}

function text(value) {
  return typeof value === "string" ? value : "";
}

function normalizeProjection(projection) {
  if (typeof projection === "string") {
    return { kind: "field", name: projection, container: "unknown" };
  }

  if (!projection || typeof projection !== "object") {
    return { kind: "opaque", name: "opaque" };
  }

  return {
    ...projection,
    kind: text(projection.kind) || "opaque",
  };
}

function normalizePlace(place, fallbackRoot) {
  if (typeof place === "string" && place.length > 0) {
    const parts = place.split(".");
    return {
      root: parts[0],
      projections: parts.slice(1).map((name) => normalizeProjection(name)),
    };
  }

  if (place && typeof place === "object") {
    const root = text(place.root) || fallbackRoot;
    const projections = Array.isArray(place.projections)
      ? place.projections.map(normalizeProjection)
      : [];
    return { root, projections };
  }

  return { root: fallbackRoot, projections: [] };
}

function projectionIdentity(projection) {
  if (projection.kind === "range") return `range:${projection.start ?? "*"}:${projection.endExclusive ?? "*"}`;
  if (projection.kind === "view") {
    const extent = Array.isArray(projection.extent) ? projection.extent.join(":") : "*";
    return `view:${extent}`;
  }
  return `${projection.kind}:${projection.name ?? projection.value ?? "*"}`;
}

function placePrefixIdentity(place, projectionCount = place.projections.length) {
  return [
    place.root,
    ...place.projections.slice(0, projectionCount).map(projectionIdentity),
  ].join("/");
}

function proofDisjoins(proofFacts = [], leftKey, rightKey) {
  return proofFacts.some((fact) => {
    if (!fact || typeof fact !== "object" || fact.kind !== "disjoint") return false;
    const pair = [text(fact.left), text(fact.right)].sort().join("|");
    return pair === [leftKey, rightKey].sort().join("|");
  });
}

function numeric(value) {
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}

function projectionDisjoint(left, right, proofFacts, leftKey, rightKey) {
  const leftKind = left.kind;
  const rightKind = right.kind;

  // These projections hide the touched storage. ProofFacts cannot make them
  // precise because the foreign or opaque operation can alias any byte.
  if (["deref", "union", "packed", "unaligned", "foreignBoundary", "opaque"].includes(leftKind)) {
    return false;
  }
  if (["deref", "union", "packed", "unaligned", "foreignBoundary", "opaque"].includes(rightKind)) {
    return false;
  }

  // Distinct enum variants do not coexist in the active payload. They are
  // not disjoint storage proofs. Fields inside one proven active variant may
  // be separated by the ordinary field rule below.
  if (leftKind === "enumPayload" || rightKind === "enumPayload") {
    if (leftKind !== "enumPayload" || rightKind !== "enumPayload") return false;
    if (left.name !== right.name) return false;
    return true;
  }

  const factKinds = new Set(["index", "range", "view"]);
  if (
    factKinds.has(leftKind) &&
    factKinds.has(rightKind) &&
    proofDisjoins(proofFacts, leftKey, rightKey)
  ) {
    return true;
  }

  if (
    ["field", "tuple", "enumPayload"].includes(leftKind) &&
    ["field", "tuple", "enumPayload"].includes(rightKind) &&
    left.name !== right.name
  ) {
    const knownContainer = [left.container, right.container].every((value) =>
      ["struct", "tuple", "enum"].includes(value),
    );
    return Boolean(left.knownDistinct || right.knownDistinct || knownContainer);
  }

  if (leftKind === "index" && rightKind === "index") {
    const leftValue = numeric(left.value);
    const rightValue = numeric(right.value);
    return leftValue !== null && rightValue !== null && leftValue !== rightValue;
  }

  if (leftKind === "range" && rightKind === "range") {
    const leftStart = numeric(left.start);
    const leftEnd = numeric(left.endExclusive);
    const rightStart = numeric(right.start);
    const rightEnd = numeric(right.endExclusive);
    return (
      leftStart !== null &&
      leftEnd !== null &&
      rightStart !== null &&
      rightEnd !== null &&
      (leftEnd <= rightStart || rightEnd <= leftStart)
    );
  }

  if (
    (leftKind === "index" && rightKind === "range") ||
    (leftKind === "range" && rightKind === "index")
  ) {
    const index = leftKind === "index" ? left : right;
    const range = leftKind === "range" ? left : right;
    const indexValue = numeric(index.value);
    const start = numeric(range.start);
    const end = numeric(range.endExclusive);
    return indexValue !== null && start !== null && end !== null && (indexValue < start || indexValue >= end);
  }

  if (leftKind === "view" || rightKind === "view") {
    const leftExtent = leftKind === "view" ? left : right;
    const rightExtent = leftKind === "view" ? right : left;
    if (leftExtent.extent && rightExtent.extent) {
      const [leftStart, leftEnd] = leftExtent.extent;
      const [rightStart, rightEnd] = rightExtent.extent;
      if ([leftStart, leftEnd, rightStart, rightEnd].every((value) => numeric(value) !== null)) {
        return leftEnd <= rightStart || rightEnd <= leftStart;
      }
    }
  }

  return false;
}

export function placesOverlap(left, right, proofFacts = []) {
  if (!left || !right || left.root !== right.root) return false;
  const limit = Math.min(left.projections.length, right.projections.length);
  for (let index = 0; index < limit; index += 1) {
    const a = left.projections[index];
    const b = right.projections[index];
    if (projectionIdentity(a) === projectionIdentity(b)) {
      if (
        a.kind === "enumPayload" &&
        b.kind === "enumPayload" &&
        index + 1 < limit &&
        !activeVariantProof(
          proofFacts,
          a,
          b,
          placePrefixIdentity(left, index),
        )
      ) {
        return true;
      }
      continue;
    }
    return !projectionDisjoint(
      a,
      b,
      proofFacts,
      placePrefixIdentity(left, index + 1),
      placePrefixIdentity(right, index + 1),
    );
  }
  return true;
}

function isSubplace(child, parent) {
  if (!child || !parent || child.root !== parent.root) return false;
  if (child.projections.length < parent.projections.length) return false;
  for (let index = 0; index < parent.projections.length; index += 1) {
    if (projectionIdentity(child.projections[index]) !== projectionIdentity(parent.projections[index])) {
      return false;
    }
  }
  return true;
}

function activeVariantProof(proofFacts = [], left, right, enumPlace) {
  const variants = [left?.name, right?.name].filter(Boolean);
  if (variants.length !== 2 || variants[0] !== variants[1]) return false;
  return proofFacts.some((fact) => {
    if (!fact || typeof fact !== "object") return false;
    if (fact.kind !== "activeVariant" && fact.kind !== "enumActiveVariant") return false;
    return fact.place === enumPlace && (fact.variant === variants[0] || fact.name === variants[0]);
  });
}

function loanConflicts(candidate, existing) {
  if (!placesOverlap(candidate.place, existing.place, candidate.proofFacts)) return false;
  return candidate.mode === "exclusive" || existing.mode === "exclusive";
}

function acceptsRepresentation(boundary, representation) {
  if (representation === "explicitTag") return true;
  if (representation === "provenNiche") {
    return !["foreignC", "wire", "persisted"].includes(boundary);
  }
  if (representation === "lowBit") return boundary === "internal";
  if (representation === "nativeCarrier") {
    return ["foreignC", "capability"].includes(boundary);
  }
  return false;
}

function origins(value) {
  if (!Array.isArray(value)) return [];
  return [...new Set(value.filter((origin) => typeof origin === "string" && origin.length > 0))].sort();
}

function dynamicEdge(edge) {
  return edge && edge.dynamic !== false && edge.kind !== "static" && edge.kind !== "immortal";
}

function edgeOrigin(edge) {
  return text(edge?.origin) || text(edge?.owner) || "origin:unknown";
}

function projectOrigins(payload) {
  const dynamic = (payload.edges ?? []).filter(dynamicEdge).map(edgeOrigin);
  const staticOrigins = (payload.edges ?? [])
    .filter((edge) => !dynamicEdge(edge))
    .map(edgeOrigin);
  payload.origins = [...new Set([...dynamic, ...staticOrigins])].sort();
  payload.dynamicOrigins = [...new Set(dynamic)].sort();
  payload.lifetimeIndependent = dynamic.length === 0;
  payload.dependent = payload.origins.length > 0;
}

function nextDependencyEdgeId(state) {
  let id = `edge:${state.nextEdge++}`;
  while (allDependencyEdges(state).some((edge) => edge.id === id)) {
    id = `edge:${state.nextEdge++}`;
  }
  return id;
}

function normalizeEdge(edge, state) {
  if (!edge || typeof edge !== "object") throw new HirMemoryError("invalidDependencyEdge");
  const input = { ...edge };
  const origin = text(input.origin);
  if (!origin) throw new HirMemoryError("invalidDependencyEdge");
  const ownerBindingName = text(input.ownerBinding);
  const ownerBinding = ownerBindingName ? state.bindings[ownerBindingName] : null;
  if (ownerBindingName && (!ownerBinding || ownerBinding.state !== "owned")) {
    throw new HirMemoryError("dependencyOwnerMissing");
  }
  const explicitOwnerPayload = text(input.ownerPayload) || null;
  if (explicitOwnerPayload && !state.payloads[explicitOwnerPayload]) {
    throw new HirMemoryError("dependencyOwnerMissing");
  }
  const ownerPayloadId = explicitOwnerPayload ?? ownerBinding?.payload ?? null;
  const ownerSlot = text(input.ownerSlot) || null;
  const mode = BORROW_MODES.has(input.mode) ? input.mode : "shared";
  const staticEdge =
    input.static === true ||
    input.immortal === true ||
    input.dynamic === false ||
    input.kind === "static" ||
    input.kind === "immortal";
  const ownerSourceCount = [ownerBindingName, explicitOwnerPayload, ownerSlot].filter(Boolean).length;
  if (staticEdge && (ownerBindingName || explicitOwnerPayload)) {
    throw new HirMemoryError("staticDependencyNeedsImmortalOwner");
  }
  if (!staticEdge && ownerSourceCount !== 1) {
    throw new HirMemoryError("dependencyOwnerMissing");
  }
  const ownerRoot = ownerPayloadId && state.payloads[ownerPayloadId]
    ? state.payloads[ownerPayloadId].root
    : text(input.ownerRoot) || ownerSlot || origin;
  const ownerPlace = normalizePlace(input.place, ownerRoot);
  if (ownerPlace.root !== ownerRoot) throw new HirMemoryError("dependencyPlaceRootMismatch");
  const id = text(input.id) || nextDependencyEdgeId(state);
  if (allDependencyEdges(state).some((existing) => existing.id === id)) {
    throw new HirMemoryError("dependencyEdgeIdAlreadyActive");
  }
  return {
    id,
    ownerBinding: ownerBinding ? ownerBindingName : null,
    ownerPayload: ownerPayloadId,
    ownerSlot,
    ownerRoot,
    ownerPlace,
    mode,
    origin,
    kind: input.kind ?? (input.immortal ? "immortal" : input.static ? "static" : "dynamic"),
    dynamic: !staticEdge,
    externalAddress: input.address ?? null,
    externalOwnerValid: input.ownerValid !== false,
  };
}

function allDependencyEdges(state) {
  return Object.entries(state.payloads).flatMap(([payloadId, payload]) =>
    (payload.edges ?? []).map((edge) => ({ ...edge, dependentPayload: payloadId })),
  );
}

function resolveEdgeOwner(state, edge) {
  if (edge.ownerPayload && state.payloads[edge.ownerPayload]) {
    return { payloadId: edge.ownerPayload, payload: state.payloads[edge.ownerPayload] };
  }
  return null;
}

function dependencyConflicts(state, place, mode, proofFacts = []) {
  return allDependencyEdges(state).filter((edge) => {
    if (!dynamicEdge(edge)) return false;
    const owner = resolveEdgeOwner(state, edge);
    if (!owner) return false;
    if (!placesOverlap(place, edge.ownerPlace, proofFacts)) return false;
    return mode === "write" || edge.mode === "exclusive";
  });
}

function sameDependencyOwner(left, right) {
  if (left.ownerPayload && right.ownerPayload) return left.ownerPayload === right.ownerPayload;
  if (left.ownerSlot && right.ownerSlot) return left.ownerSlot === right.ownerSlot;
  return false;
}

function dependencyEdgesConflict(left, right, proofFacts = []) {
  if (!sameDependencyOwner(left, right)) return false;
  if (!placesOverlap(left.ownerPlace, right.ownerPlace, proofFacts)) return false;
  return left.mode === "exclusive" || right.mode === "exclusive";
}

function validateAddedDependencyEdges(state, addedEdges, proofFacts = [], excludedPayload = null) {
  const existingEdges = allDependencyEdges(state).filter(
    (edge) => edge.dependentPayload !== excludedPayload,
  );
  for (const [index, edge] of addedEdges.entries()) {
    const priorAddedEdges = addedEdges.slice(0, index);
    if (
      existingEdges.some((existing) => existing.id === edge.id) ||
      priorAddedEdges.some((existing) => existing.id === edge.id)
    ) {
      throw new HirMemoryError("dependencyEdgeIdAlreadyActive");
    }
    if (existingEdges.some((existing) => dependencyEdgesConflict(edge, existing, proofFacts))) {
      throw new HirMemoryError("dependencyConflict");
    }
    if (priorAddedEdges.some((existing) => dependencyEdgesConflict(edge, existing, proofFacts))) {
      throw new HirMemoryError("dependencyConflict");
    }
    const owner = resolveEdgeOwner(state, edge);
    if (!owner) continue;
    const candidate = {
      mode: edge.mode,
      place: edge.ownerPlace,
      proofFacts,
    };
    if (activeLoansForPayload(state, owner.payloadId).some((loan) => loanConflicts(candidate, loan))) {
      throw new HirMemoryError("dependencyConflict");
    }
  }
}

function resolveDependencyEdge(payload, operation) {
  if (operation.edgeId) {
    const edge = payload.edges.find((candidate) => candidate.id === operation.edgeId);
    if (!edge) throw new HirMemoryError("dependencyEdgeMissing");
    return edge;
  }

  const candidates = payload.edges.filter(
    (candidate) => edgeOrigin(candidate) === operation.origin,
  );
  if (candidates.length === 0) throw new HirMemoryError("dependencyEdgeMissing");
  if (candidates.length > 1) throw new HirMemoryError("dependencyEdgeAmbiguous");
  return candidates[0];
}

function ownerHasDynamicEdges(state, payloadId) {
  return allDependencyEdges(state).some(
    (edge) => dynamicEdge(edge) && resolveEdgeOwner(state, edge)?.payloadId === payloadId,
  );
}

function releasePayloadEdges(payload) {
  payload.edges = [];
  payload.origins = [];
  payload.dynamicOrigins = [];
  payload.dependent = false;
  payload.lifetimeIndependent = true;
}

function slotKey(slot, index) {
  if (typeof slot === "string") return slot;
  if (!slot || typeof slot !== "object") return `parameter:${index}`;
  return text(slot.slot) || text(slot.name) || text(slot.id) || `parameter:${index}`;
}

function compatibleInput(slot) {
  if (typeof slot === "string") return true;
  if (!slot || typeof slot !== "object") return false;
  return slot.compatible !== false && (
    slot.borrowed === true ||
    slot.dependent === true ||
    slot.kind === "borrowed" ||
    slot.kind === "dependent" ||
    slot.mode === "ref" ||
    slot.mode === "inout" ||
    slot.mode === "view"
  );
}

function dependentResultSlots(operation) {
  const declaredResults = Array.isArray(operation.resultSlots) ? operation.resultSlots : [];
  const dependentResults = declaredResults
    .map((slot, index) => ({ slot, index }))
    .filter(({ slot }) =>
      typeof slot === "string" || slot?.dependent === true || slot?.borrowed === true,
    )
    .map(({ slot, index }) => slotKey(slot, index));
  if (operation.resultDependent === true && dependentResults.length === 0) {
    dependentResults.push("result");
  }
  return dependentResults;
}

function deriveInterfaceMapping(operation) {
  const dependentResults = dependentResultSlots(operation);
  if (operation.body === true) {
    const mapping = operation.inferredMapping ?? {};
    if (
      dependentResults.some(
        (result) => !Array.isArray(mapping[result]) || mapping[result].length === 0,
      )
    ) {
      throw new HirMemoryError("interfaceOriginUnknown");
    }
    return mapping;
  }
  const inputSlots = Array.isArray(operation.inputSlots) ? operation.inputSlots : [];
  const kind = operation.kind ?? (operation.receiverOnly ? "instance" : "free");
  let sources = [];
  if (kind === "instance" || kind === "member" || operation.receiverOnly === true) {
    const receiver = inputSlots.find((slot, index) => slotKey(slot, index) === "receiver");
    if (
      (receiver && compatibleInput(receiver)) ||
      operation.receiverCompatible === true ||
      operation.receiverOnly === true
    ) {
      sources = ["receiver"];
    }
  } else if (["init", "initializer", "static", "free", "protocol"].includes(kind)) {
    sources = inputSlots
      .map((slot, index) => ({ slot, index }))
      .filter(({ slot }) => compatibleInput(slot))
      .map(({ slot, index }) => slotKey(slot, index));
  } else {
    sources = inputSlots
      .map((slot, index) => ({ slot, index }))
      .filter(({ slot }) => compatibleInput(slot))
      .map(({ slot, index }) => slotKey(slot, index));
  }
  if (dependentResults.length > 0 && ["init", "initializer"].includes(kind)) {
    throw new HirMemoryError("initBorrowResultUnsupported");
  }
  if (dependentResults.length > 0 && !["instance", "member"].includes(kind) && sources.length > 1) {
    throw new HirMemoryError(AMBIGUOUS_BODYLESS_RESULT, {
      authority: "none",
      compatibleInputs: [...sources].sort(),
      declarationKind: kind,
      result: dependentResults,
      reason: "ambiguousBodylessResultOrigin",
    });
  }
  if (dependentResults.length > 0 && sources.length === 0) {
    if (operation.resultStatic === true || operation.resultIndependent === true) return {};
    throw new HirMemoryError("interfaceOriginUnknown");
  }
  return Object.fromEntries(dependentResults.map((result) => [result, [...sources]]));
}

function verifyDynamicDependenciesStable(state, payload) {
  for (const edge of payload.edges ?? []) {
    if (!dynamicEdge(edge)) continue;
    const owner = resolveEdgeOwner(state, edge);
    if (owner) {
      const ownerBinding = Object.values(state.bindings).find(
        (binding) => binding.payload === owner.payloadId && binding.state === "owned",
      );
      if (!ownerBinding) throw new HirMemoryError("awaitOwnerInvalid");
      if (!["stable", "published"].includes(owner.payload.address)) {
        throw new HirMemoryError("unstableReferentSuspension");
      }
      continue;
    }
    if (!edge.ownerSlot || edge.externalOwnerValid !== true) {
      throw new HirMemoryError("awaitOwnerInvalid");
    }
    if (!["stable", "published"].includes(edge.externalAddress)) {
      throw new HirMemoryError("unstableReferentSuspension");
    }
  }
}

function verifyActiveLoansStable(state) {
  for (const loan of Object.values(state.loans)) {
    const payload = state.payloads[loan.payload];
    if (payload.address === "unstable") throw new HirMemoryError("unstableSuspension");
    verifyDynamicDependenciesStable(state, payload);
  }
}

function requirePayloadForPlace(state, binding, operation) {
  if (binding.weakBlock) throw new HirMemoryError("weakRequiresUpgrade");
  const payload = state.payloads[binding.payload];
  const place = normalizePlace(operation.place, payload.root);
  if (place.root !== payload.root) throw new HirMemoryError("placeRootMismatch");
  return { payload, place };
}

function activeLoansOverlapping(state, payloadId, place, proofFacts = []) {
  return activeLoansForPayload(state, payloadId).filter((loan) => placesOverlap(place, loan.place, proofFacts));
}

function applyBorrow(state, operation, reborrow) {
  const binding = requireBinding(state, operation.binding);
  if (!BORROW_MODES.has(operation.mode)) throw new HirMemoryError("invalidBorrowMode");
  if (binding.weakBlock) throw new HirMemoryError("weakRequiresUpgrade");
  if (binding.sharedBlock && operation.mode === "exclusive") {
    throw new HirMemoryError("exclusiveBorrowRequiresUniqueOwner");
  }
  const { payload, place } = requirePayloadForPlace(state, binding, operation);
  const loanId = operation.token;
  if (state.loans[loanId]) throw new HirMemoryError("loanIdAlreadyActive");

  if (!reborrow && operation.parent !== undefined) {
    throw new HirMemoryError("invalidReborrowOperation");
  }
  const parent = operation.parent === undefined ? null : state.loans[operation.parent];
  if (reborrow && !parent) throw new HirMemoryError("reborrowParentMissing");
  if (parent) {
    if (parent.payload !== binding.payload || !isSubplace(place, parent.place)) {
      throw new HirMemoryError("reborrowPlaceMismatch");
    }
    if (operation.mode === "exclusive" && parent.mode !== "exclusive") {
      throw new HirMemoryError("reborrowRequiresExclusiveParent");
    }
  }

  const candidate = {
    mode: operation.mode,
    place,
    proofFacts: operation.proofFacts ?? [],
  };
  const dependencyAccess = operation.mode === "exclusive" ? "write" : "read";
  if (dependencyConflicts(state, place, dependencyAccess, candidate.proofFacts).length > 0) {
    throw new HirMemoryError("dependencyConflict");
  }
  const conflicts = activeLoansForPayload(state, binding.payload).filter((loan) => {
    if (parent && loan.id === parent.id) return false;
    return loanConflicts(candidate, loan);
  });
  if (conflicts.length > 0) throw new HirMemoryError("loanOverlap");

  state.nextLoan += 1;
  state.loans[loanId] = {
    id: loanId,
    sequence: state.nextLoan,
    mode: operation.mode,
    binding: operation.binding,
    payload: binding.payload,
    place,
    origin: text(operation.origin) || `origin:${loanId}`,
    emittedAt: operation.emittedAt ?? state.nextLoan,
    endsAt: operation.endsAt ?? null,
    stability: payload.address,
    parent: parent?.id ?? null,
    childCount: 0,
  };
  if (parent) parent.childCount += 1;
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "defineAllocator": {
      const allocators = allocatorTable(state);
      if (allocators[operation.allocator]) {
        throw new HirMemoryError("allocatorAlreadyDefined");
      }
      allocators[operation.allocator] = {
        state: "open",
        lifetime: operation.lifetime,
        mobility: operation.mobility,
        adoptionFamily: operation.adoptionFamily,
        limit: operation.limit ?? null,
        charged: 0,
        outlives: [...(operation.outlives ?? [])],
      };
      return;
    }
    case "closeAllocator": {
      const allocator = requireAllocator(state, operation.allocator);
      const livePayload = Object.values(state.bindings).some((binding) => {
        if (binding.state !== "owned" || binding.weakBlock) return false;
        const payload = state.payloads[binding.payload];
        return payload && storageOrigins(payload).includes(operation.allocator);
      });
      const liveControlBlock = Object.values(state.controlBlocks ?? {}).some(
        (block) => block.blockAlive && block.allocator === operation.allocator,
      );
      if (livePayload || liveControlBlock) {
        throw new HirMemoryError("storageOriginStillLive");
      }
      allocator.state = "closed";
      return;
    }
    case "initialize": {
      if (state.bindings[operation.binding]) {
        throw new HirMemoryError("bindingAlreadyInitialized");
      }
      if (operation.selfReference) throw new HirMemoryError("selfReferentialValue");
      if (operation.using) chargeAllocator(state, operation.using, operation.bytes ?? 0);
      const payload = `p${state.nextPayload}`;
      state.nextPayload += 1;
      const edgeInputs = Array.isArray(operation.edges) ? operation.edges : [];
      const payloadState = {
        root: operation.root ?? operation.binding,
        address: operation.address ?? "unstable",
        allocatorKnown: operation.allocatorKnown ?? false,
        origins: [],
        dynamicOrigins: [],
        lifetimeIndependent: true,
        dependent: false,
        copyable: operation.copyable !== false && operation.inoutField !== true,
        inoutField: operation.inoutField === true,
        pinnedPayload: false,
        edges: [],
        ...(operation.using ? { storageOrigins: [operation.using] } : {}),
      };
      state.payloads[payload] = payloadState;
      payloadState.edges = edgeInputs.map((edge) => normalizeEdge(edge, state));
      if (payloadState.edges.some((edge) => edge.ownerPayload === payload)) {
        throw new HirMemoryError("selfReferentialValue");
      }
      validateAddedDependencyEdges(state, payloadState.edges, operation.proofFacts ?? [], payload);
      if (payloadState.edges.some((edge) => edge.mode === "exclusive")) payloadState.copyable = false;
      projectOrigins(payloadState);
      state.bindings[operation.binding] = {
        state: "owned",
        payload,
        pinnedHandle: false,
      };
      return;
    }
    case "pinConstruct": {
      if (state.bindings[operation.binding]) {
        throw new HirMemoryError("bindingAlreadyInitialized");
      }
      if (operation.selfReference) throw new HirMemoryError("selfReferentialValue");
      if (operation.publishBeforeCommit) {
        throw new HirMemoryError("addressPublicationBeforeInitialization");
      }

      const argumentsInOrder = operation.arguments ?? [];
      const fieldsInOrder = operation.fields ?? [];
      const outcome = operation.outcome ?? "success";
      const failedArgumentIndex = operation.failedArgumentIndex;
      const evaluatedArguments = outcome === "argumentError"
        ? argumentsInOrder.slice(0, failedArgumentIndex)
        : [...argumentsInOrder];
      const initializedFields = operation.initializedFields ?? (
        outcome === "success" ? [...fieldsInOrder] : []
      );
      const consumedArguments = new Set(operation.consumedArguments ?? []);
      const expectedFieldPrefix = fieldsInOrder.slice(0, initializedFields.length);
      if (JSON.stringify(initializedFields) !== JSON.stringify(expectedFieldPrefix)) {
        throw new HirMemoryError("invalidInitializationProgress");
      }
      if (outcome === "success" && initializedFields.length !== fieldsInOrder.length) {
        throw new HirMemoryError("incompletePinnedInitialization");
      }
      if (
        (outcome === "argumentError" || outcome === "allocationError") &&
        initializedFields.length !== 0
      ) {
        throw new HirMemoryError("initializationBeforePinnedStorage");
      }

      const root = `pin:${operation.root ?? operation.binding}`;
      const receipt = {
        binding: operation.binding,
        outcome,
        evaluatedArguments,
        destinationRoot: root,
        delegationDepth: operation.delegationDepth ?? 0,
        intermediateMoves: 0,
        storageReserved: !["argumentError", "allocationError"].includes(outcome),
        initializedFields: [...initializedFields],
        fieldCleanup: [],
        stagingCleanup: [],
        storageReleased: false,
        deinitCount: 0,
        bindingCommitted: false,
        addressPublishedBeforeCommit: false,
      };

      if (outcome === "argumentError" || outcome === "allocationError") {
        receipt.stagingCleanup = [...evaluatedArguments].reverse();
        constructionReceiptTable(state).push(receipt);
        recordOutcome(state, operation, outcome);
        return;
      }

      if (operation.using) {
        chargeAllocator(state, operation.using, operation.bytes ?? 0);
      }

      if (outcome === "initializerError") {
        receipt.fieldCleanup = [...initializedFields].reverse();
        receipt.stagingCleanup = evaluatedArguments
          .filter((argument) => !consumedArguments.has(argument))
          .reverse();
        receipt.storageReleased = true;
        constructionReceiptTable(state).push(receipt);
        recordOutcome(state, operation, outcome);
        return;
      }

      const payload = `p${state.nextPayload}`;
      state.nextPayload += 1;
      state.payloads[payload] = {
        root,
        pinnedRoot: root,
        address: "stable",
        allocatorKnown: operation.allocatorKnown ?? false,
        origins: [],
        dynamicOrigins: [],
        lifetimeIndependent: true,
        dependent: false,
        copyable: operation.copyable !== false,
        inoutField: false,
        pinnedPayload: true,
        edges: [],
        construction: {
          direct: true,
          delegationDepth: operation.delegationDepth ?? 0,
          intermediateMoves: 0,
        },
        ...(operation.using ? { storageOrigins: [operation.using] } : {}),
      };
      state.bindings[operation.binding] = {
        state: "owned",
        payload,
        pinnedHandle: true,
      };
      receipt.bindingCommitted = true;
      constructionReceiptTable(state).push(receipt);
      recordOutcome(state, operation, outcome);
      return;
    }
    case "use": {
      const binding = requireBinding(state, operation.binding);
      const { place } = requirePayloadForPlace(state, binding, operation);
      if (dependencyConflicts(state, place, "read", operation.proofFacts ?? []).length > 0) {
        throw new HirMemoryError("dependencyConflict");
      }
      const exclusive = activeLoansOverlapping(
        state,
        binding.payload,
        place,
        operation.proofFacts ?? [],
      ).filter((loan) => loan.mode === "exclusive");
      if (exclusive.length > 0) throw new HirMemoryError("loanOverlap");
      return;
    }
    case "read": {
      const binding = requireBinding(state, operation.binding);
      const { place } = requirePayloadForPlace(state, binding, operation);
      if (dependencyConflicts(state, place, "read", operation.proofFacts ?? []).length > 0) {
        throw new HirMemoryError("dependencyConflict");
      }
      if (
        activeLoansOverlapping(
          state,
          binding.payload,
          place,
          operation.proofFacts ?? [],
        ).some((loan) => loan.mode === "exclusive")
      ) {
        throw new HirMemoryError("loanOverlap");
      }
      return;
    }
    case "write": {
      const binding = requireBinding(state, operation.binding);
      if (binding.sharedBlock) throw new HirMemoryError("exclusiveBorrowRequiresUniqueOwner");
      const { place } = requirePayloadForPlace(state, binding, operation);
      if (dependencyConflicts(state, place, "write", operation.proofFacts ?? []).length > 0) {
        throw new HirMemoryError("dependencyConflict");
      }
      if (
        activeLoansOverlapping(
          state,
          binding.payload,
          place,
          operation.proofFacts ?? [],
        ).length > 0
      ) {
        throw new HirMemoryError("loanOverlap");
      }
      return;
    }
    case "move": {
      const source = requireBinding(state, operation.from);
      if (source.sharedBlock) {
        requireNoBindingLoans(state, operation.from, "moveWithLoan");
      } else if (!source.weakBlock) {
        requireNoLoans(state, source.payload, "moveWithLoan", true, source);
      }
      if (
        !source.pinnedHandle &&
        !source.sharedBlock &&
        !source.weakBlock &&
        ownerHasDynamicEdges(state, source.payload)
      ) {
        throw new HirMemoryError("ownerMoveWithDependency");
      }
      if (state.bindings[operation.to]) {
        throw new HirMemoryError("moveTargetAlreadyInitialized");
      }
      const destination = { ...source, state: "owned" };
      source.state = "moved";
      state.bindings[operation.to] = destination;
      return;
    }
    case "drop": {
      const binding = requireBinding(state, operation.binding);
      if (binding.weakBlock) {
        const block = controlBlockTable(state)[binding.weakBlock];
        if (!block || !block.blockAlive || block.weak < 1) {
          throw new HirMemoryError("weakControlBlockUnavailable");
        }
        binding.state = "dropped";
        block.weak -= 1;
        if (block.strong === 0 && block.weak === 0) block.blockAlive = false;
        return;
      }
      if (binding.sharedBlock) {
        const block = controlBlockTable(state)[binding.sharedBlock];
        if (!block || !block.blockAlive || !block.payloadAlive || block.strong < 1) {
          throw new HirMemoryError("sharedControlBlockUnavailable");
        }
        requireNoBindingLoans(state, operation.binding, "dropWithLoan");
        binding.state = "dropped";
        block.strong -= 1;
        if (block.strong === 0) {
          const payload = state.payloads[block.payload];
          releasePayloadEdges(payload);
          payload.sharedDestroyed = true;
          payload.dropCount = (payload.dropCount ?? 0) + 1;
          block.payloadAlive = false;
          block.deinitCount += 1;
          if (block.weak === 0) block.blockAlive = false;
        }
        return;
      }
      requireNoLoans(state, binding.payload, "dropWithLoan");
      if (ownerHasDynamicEdges(state, binding.payload)) {
        throw new HirMemoryError("ownerDropWithDependency");
      }
      const payload = state.payloads[binding.payload];
      releasePayloadEdges(payload);
      if (payload.erasure && payload.erasure.destroyed !== true) {
        payload.erasure.destroyed = true;
        payload.dropCount = (payload.dropCount ?? 0) + 1;
      }
      binding.state = "dropped";
      return;
    }
    case "beginBorrow": {
      applyBorrow(state, operation, false);
      return;
    }
    case "reborrow": {
      applyBorrow(state, operation, true);
      return;
    }
    case "duplicateLoan": {
      const source = state.loans[operation.from];
      if (!source) throw new HirMemoryError("loanSourceMissing");
      if (source.mode !== "shared") throw new HirMemoryError("duplicateExclusiveLoan");
      if (source.childCount > 0) throw new HirMemoryError("frozenReborrowParent");
      if (state.loans[operation.token]) throw new HirMemoryError("loanIdAlreadyActive");
      const parent = source.parent ? state.loans[source.parent] : null;
      if (source.parent && !parent) throw new HirMemoryError("reborrowParentMissing");
      state.nextLoan += 1;
      state.loans[operation.token] = {
        ...clone(source),
        id: operation.token,
        sequence: state.nextLoan,
        childCount: 0,
      };
      if (parent) parent.childCount += 1;
      return;
    }
    case "endBorrow": {
      const loan = state.loans[operation.token];
      if (!loan) throw new HirMemoryError("invalidLoanEnd");
      if (loan.childCount > 0) throw new HirMemoryError("frozenReborrowParent");
      if (loan.parent && state.loans[loan.parent]) state.loans[loan.parent].childCount -= 1;
      delete state.loans[loan.id];
      return;
    }
    case "accessLoan": {
      const loan = state.loans[operation.token];
      if (!loan) throw new HirMemoryError("loanSourceMissing");
      const access = operation.access ?? (operation.mode === "exclusive" ? "write" : "read");
      if (!ACCESS_MODES.has(access)) throw new HirMemoryError("invalidLoanAccess");
      if (loan.childCount > 0) throw new HirMemoryError("frozenReborrowParent");
      if (access === "write" && loan.mode !== "exclusive") {
        throw new HirMemoryError("exclusiveAccessRequiresExclusiveLoan");
      }
      if (access === "read" && !["shared", "exclusive"].includes(loan.mode)) {
        throw new HirMemoryError("invalidLoanAccess");
      }
      return;
    }
    case "accessDependency": {
      const binding = requireBinding(state, operation.binding);
      const payload = state.payloads[binding.payload];
      const edge = resolveDependencyEdge(payload, operation);
      const access = operation.access ?? "read";
      if (!ACCESS_MODES.has(access)) throw new HirMemoryError("invalidLoanAccess");
      if (access === "write" && edge.mode !== "exclusive") {
        throw new HirMemoryError("dependencyAccessModeMismatch");
      }
      if (dynamicEdge(edge)) {
        const owner = resolveEdgeOwner(state, edge);
        if (owner) {
          const ownerBinding = Object.values(state.bindings).find(
            (candidate) => candidate.payload === owner.payloadId && candidate.state === "owned",
          );
          if (!ownerBinding) throw new HirMemoryError("dependencyOwnerMissing");
        } else if (!edge.ownerSlot || edge.externalOwnerValid !== true) {
          throw new HirMemoryError("dependencyOwnerMissing");
        }
      }
      return;
    }
    case "mutate":
    case "replace": {
      const binding = requireBinding(state, operation.binding);
      if (binding.sharedBlock || binding.weakBlock) {
        throw new HirMemoryError("exclusiveBorrowRequiresUniqueOwner");
      }
      const { payload, place } = requirePayloadForPlace(state, binding, operation);
      if (dependencyConflicts(state, place, "write", operation.proofFacts ?? []).length > 0) {
        throw new HirMemoryError("dependencyConflict");
      }
      const overlapping = operation.structural
        ? activeLoansForPayload(state, binding.payload)
        : activeLoansOverlapping(state, binding.payload, place, operation.proofFacts ?? []);
      if (overlapping.length > 0) throw new HirMemoryError("loanOverlap");
      if (operation.invalidateOrigins) {
        if (payload.edges.length > 0 && operation.cleanupDone !== true) {
          throw new HirMemoryError("cleanupNotDrained");
        }
        releasePayloadEdges(payload);
      }
      return;
    }
    case "copy": {
      const source = requireBinding(state, operation.from);
      if (source.sharedBlock || source.weakBlock) {
        throw new HirMemoryError("copyRequiresHandleOperation");
      }
      const sourcePayload = state.payloads[source.payload];
      const sourcePlace = normalizePlace(undefined, sourcePayload.root);
      if (dependencyConflicts(state, sourcePlace, "read").length > 0) {
        throw new HirMemoryError("dependencyConflict");
      }
      if (activeLoansOverlapping(state, source.payload, sourcePlace).some((loan) => loan.mode === "exclusive")) {
        throw new HirMemoryError("loanOverlap");
      }
      if (
        source.pinnedHandle ||
        !sourcePayload.copyable ||
        (sourcePayload.edges ?? []).some((edge) => edge.mode === "exclusive")
      ) {
        throw new HirMemoryError("copyRequiresCopyableAggregate");
      }
      if (state.bindings[operation.to]) throw new HirMemoryError("copyTargetAlreadyInitialized");
      const payload = `p${state.nextPayload}`;
      state.nextPayload += 1;
      const copiedEdges = (sourcePayload.edges ?? []).map((edge) => ({
        ...clone(edge),
        id: nextDependencyEdgeId(state),
      }));
      state.payloads[payload] = {
        ...clone(sourcePayload),
        root: operation.to,
        pinnedPayload: false,
        edges: copiedEdges,
      };
      projectOrigins(state.payloads[payload]);
      state.bindings[operation.to] = {
        state: "owned",
        payload,
        pinnedHandle: false,
      };
      return;
    }
    case "insert":
    case "joinDependencies": {
      const target = requireBinding(state, operation.binding);
      const targetPayload = state.payloads[target.payload];
      if (ownerHasDynamicEdges(state, target.payload)) throw new HirMemoryError("dependencyConflict");
      if (activeLoansForPayload(state, target.payload).length > 0) throw new HirMemoryError("loanOverlap");
      const addedEdges = (Array.isArray(operation.edges) ? operation.edges : []).map((edge) =>
        normalizeEdge(edge, state),
      );
      if (operation.from) {
        const source = requireBinding(state, operation.from);
        const sourcePayload = state.payloads[source.payload];
        const sourcePlace = normalizePlace(undefined, sourcePayload.root);
        if (
          dependencyConflicts(
            state,
            sourcePlace,
            "read",
            operation.proofFacts ?? [],
          ).length > 0
        ) {
          throw new HirMemoryError("dependencyConflict");
        }
        if (
          activeLoansOverlapping(
            state,
            source.payload,
            sourcePlace,
            operation.proofFacts ?? [],
          ).some((loan) => loan.mode === "exclusive")
        ) {
          throw new HirMemoryError("loanOverlap");
        }
        for (const edge of sourcePayload.edges ?? []) {
          if (edge.mode === "exclusive") {
            throw new HirMemoryError("copyRequiresCopyableAggregate");
          }
          addedEdges.push({ ...clone(edge), id: nextDependencyEdgeId(state) });
        }
      }
      if (addedEdges.some((edge) => edge.ownerPayload === target.payload)) {
        throw new HirMemoryError("selfReferentialValue");
      }
      validateAddedDependencyEdges(state, addedEdges, operation.proofFacts ?? []);
      targetPayload.edges.push(...addedEdges);
      if (addedEdges.some((edge) => edge.mode === "exclusive")) targetPayload.copyable = false;
      projectOrigins(targetPayload);
      return;
    }
    case "remove": {
      const target = requireBinding(state, operation.binding);
      const targetPayload = state.payloads[target.payload];
      if (ownerHasDynamicEdges(state, target.payload)) throw new HirMemoryError("dependencyConflict");
      if (activeLoansForPayload(state, target.payload).length > 0) throw new HirMemoryError("loanOverlap");
      if (operation.proven === true) {
        const edge = resolveDependencyEdge(targetPayload, operation);
        const edgeIndex = targetPayload.edges.findIndex((candidate) => candidate.id === edge.id);
        targetPayload.edges.splice(edgeIndex, 1);
        projectOrigins(targetPayload);
      }
      return;
    }
    case "clear": {
      const target = requireBinding(state, operation.binding);
      if (target.sharedBlock || target.weakBlock) {
        throw new HirMemoryError("exclusiveBorrowRequiresUniqueOwner");
      }
      const targetPayload = state.payloads[target.payload];
      if (ownerHasDynamicEdges(state, target.payload)) throw new HirMemoryError("dependencyConflict");
      if (activeLoansForPayload(state, target.payload).length > 0) throw new HirMemoryError("loanOverlap");
      if (targetPayload.edges.length > 0 && operation.cleanupDone !== true) {
        throw new HirMemoryError("cleanupNotDrained");
      }
      releasePayloadEdges(targetPayload);
      return;
    }
    case "escape": {
      const binding = requireBinding(state, operation.binding);
      const payload = state.payloads[binding.payload];
      if (!ESCAPE_TARGETS.has(operation.target)) throw new HirMemoryError("unknownEscapeTarget");
      const crossDomainTargets = new Set([
        "service",
        "channel",
        "detachedTask",
        "structuredChild",
        "foreignRetention",
      ]);
      const hasStorageOrigin = storageOrigins(payload).length > 0;
      const derivedMobility = hasStorageOrigin && storageIsTransferable(state, payload);
      if (crossDomainTargets.has(operation.target) && hasStorageOrigin && !derivedMobility) {
        throw new HirMemoryError("allocationOriginNotTransferable");
      }
      if (binding.sharedBlock && crossDomainTargets.has(operation.target)) {
        const block = controlBlockTable(state)[binding.sharedBlock];
        const allocator = requireAllocator(state, block.allocator);
        if (operation.shareable !== true) {
          throw new HirMemoryError("sharedPayloadNotShareable");
        }
        if (!block.threadSafe) throw new HirMemoryError("sharedCounterNotThreadSafe");
        if (allocator.mobility !== "crossDomain") {
          throw new HirMemoryError("allocationOriginNotTransferable");
        }
      }
      projectOrigins(payload);
      const dynamic = payload.dynamicOrigins ?? [];
      if (dynamic.length === 0 && !payload.dependent && payload.origins.length === 0) return;
      const externalTargets = [
        "static",
        "global",
        "shared",
        "service",
        "channel",
        "wire",
        "persistence",
        "detachedTask",
        "foreignRetention",
        "share",
      ];
      if (dynamic.length > 0 && externalTargets.includes(operation.target)) {
        throw new HirMemoryError("dependentEscape");
      }
      if (operation.target === "shared" || operation.target === "share") {
        if (operation.shareable !== true) throw new HirMemoryError("dependentEscape");
      }
      if (
        ["channel", "detachedTask", "structuredChild"].includes(operation.target) &&
        operation.transferable !== true &&
        dynamic.length === 0
      ) {
        throw new HirMemoryError("dependentEscape");
      }
      if (operation.target === "wire" && operation.wireValue !== true) {
        throw new HirMemoryError("dependentEscape");
      }
      if (
        operation.target === "service" &&
        (operation.wireValue !== true || operation.transferable !== true)
      ) {
        throw new HirMemoryError("dependentEscape");
      }
      if (operation.target === "persistence" && operation.persistentValue !== true) {
        throw new HirMemoryError("dependentEscape");
      }
      if (operation.target === "foreignRetention" && operation.ffiSafe !== true) {
        throw new HirMemoryError("dependentEscape");
      }
      if (operation.target === "structuredChild") {
        if (
          operation.joinPrecedesOrigins !== true ||
          (operation.mobility !== true && !derivedMobility)
        ) {
          throw new HirMemoryError("structuredChildDependency");
        }
        return;
      }
      if (dynamic.length === 0) return;
      const available = origins(operation.availableOrigins);
      if (!dynamic.every((origin) => available.includes(origin))) {
        throw new HirMemoryError("dependentOriginDoesNotSurvive");
      }
      return;
    }
    case "await": {
      const binding = requireBinding(state, operation.binding);
      const payload = state.payloads[binding.payload];
      verifyActiveLoansStable(state);
      verifyDynamicDependenciesStable(state, payload);
      if (operation.conflictFree === false) throw new HirMemoryError("awaitConflict");
      if (operation.cleanupDrained !== true || operation.cancelDrained !== true) {
        throw new HirMemoryError("awaitCleanupNotDrained");
      }
      return;
    }
    case "rehome": {
      const source = requireUniqueBinding(state, operation.from);
      const payload = state.payloads[source.payload];
      requireNoLoans(state, source.payload, "moveWithLoan");
      if (ownerHasDynamicEdges(state, source.payload)) {
        throw new HirMemoryError("ownerMoveWithDependency");
      }
      if (state.bindings[operation.to]) {
        throw new HirMemoryError("moveTargetAlreadyInitialized");
      }
      const destinationAllocator = requireAllocator(state, operation.using);
      if (operation.adopt === true) {
        const compatible = storageOrigins(payload).every(
          (origin) => requireAllocator(state, origin).adoptionFamily === destinationAllocator.adoptionFamily,
        );
        if (!compatible) throw new HirMemoryError("incompatibleAllocatorAdoption");
      }
      const outcome = operation.outcome ?? "success";
      if (outcome === "allocationError") {
        consumeAfterAllocationFailure(state, source);
        recordOutcome(state, operation, outcome);
        return;
      }
      if (operation.adopt !== true) {
        chargeAllocator(state, operation.using, operation.bytes ?? 0);
      }
      source.state = "moved";
      payload.storageOrigins = [operation.using];
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: false,
      };
      recordOutcome(state, operation, outcome);
      return;
    }
    case "erase": {
      const source = requireUniqueBinding(state, operation.from);
      const payload = state.payloads[source.payload];
      requireNoLoans(state, source.payload, "moveWithLoan");
      if (ownerHasDynamicEdges(state, source.payload)) {
        throw new HirMemoryError("ownerMoveWithDependency");
      }
      if (state.bindings[operation.to]) {
        throw new HirMemoryError("moveTargetAlreadyInitialized");
      }

      const fitsInline =
        operation.payloadBytes <= operation.inlineBytes &&
        operation.payloadAlignment <= operation.inlineAlignment;
      if (operation.using) requireAllocator(state, operation.using);
      if (!fitsInline && operation.spill === "forbid") {
        throw new HirMemoryError("erasureSpillForbidden");
      }

      const outcome = operation.outcome ?? "success";
      if (fitsInline && outcome === "allocationError") {
        throw new HirMemoryError("inlineErasureCannotFailAllocation");
      }
      if (!fitsInline && outcome === "allocationError") {
        consumeErasureAllocationFailure(state, source, payload, operation);
        return;
      }

      const payloadStorageOrigins = storageOrigins(payload);
      const storage = fitsInline ? "inline" : "spill";
      if (!fitsInline) {
        try {
          chargeAllocator(state, operation.using, operation.boxBytes);
        } catch (error) {
          if (!(error instanceof HirMemoryError) || error.code !== "budgetExceeded") throw error;
          consumeErasureAllocationFailure(state, source, payload, operation);
          return;
        }
        payload.storageOrigins = origins([...payloadStorageOrigins, operation.using]);
      }
      payload.erasure = {
        storage,
        inlineBytes: operation.inlineBytes,
        inlineAlignment: operation.inlineAlignment,
        payloadBytes: operation.payloadBytes,
        payloadAlignment: operation.payloadAlignment,
        boxOrigin: fitsInline ? null : operation.using,
        destroyed: false,
      };
      source.state = "moved";
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: false,
      };
      recordOutcome(state, operation, outcome);
      return;
    }
    case "share": {
      const source = requireUniqueBinding(state, operation.from);
      const payload = state.payloads[source.payload];
      requireNoLoans(state, source.payload, "moveWithLoan");
      if (ownerHasDynamicEdges(state, source.payload)) {
        throw new HirMemoryError("ownerMoveWithDependency");
      }
      projectOrigins(payload);
      if (!payload.lifetimeIndependent || payload.dynamicOrigins.length > 0) {
        throw new HirMemoryError("shareRequiresLifetimeIndependent");
      }
      if (state.bindings[operation.to]) {
        throw new HirMemoryError("moveTargetAlreadyInitialized");
      }
      const sharedAllocator = requireAllocator(state, operation.using);
      if (
        storageOrigins(payload).some(
          (origin) => !allocatorOutlives(state, origin, operation.using),
        )
      ) {
        throw new HirMemoryError("shareRequiresRehome");
      }
      const outcome = operation.outcome ?? "success";
      if (outcome === "allocationError") {
        consumeAfterAllocationFailure(state, source);
        recordOutcome(state, operation, outcome);
        return;
      }
      chargeAllocator(state, operation.using, operation.bytes ?? 0);
      const blocks = controlBlockTable(state);
      const blockId = `c${state.nextControlBlock ?? 0}`;
      state.nextControlBlock = (state.nextControlBlock ?? 0) + 1;
      source.state = "moved";
      const controlBlockOrigin = operation.controlBlockOrigin ?? operation.using;
      blocks[blockId] = {
        payload: source.payload,
        allocator: operation.using,
        allocatorContract: operation.allocatorContract ?? operation.using,
        controlBlockOrigin,
        controlBlockDeallocator: operation.controlBlockDeallocator ?? "provider",
        controlBlockMobility: operation.controlBlockMobility ?? sharedAllocator.mobility ?? "local",
        controlBlockLifetime: operation.controlBlockLifetime ?? sharedAllocator.lifetime ?? null,
        controlBlockAdoptionFamily: operation.controlBlockAdoptionFamily ?? sharedAllocator.adoptionFamily ?? null,
        controlBlockBulkReleaseOwner: operation.controlBlockBulkReleaseOwner ?? null,
        allocationOriginMap: {
          "$storage": storageOrigins(payload),
          "$controlBlock": controlBlockOrigin,
        },
        strong: 1,
        weak: 0,
        payloadAlive: true,
        blockAlive: true,
        deinitCount: 0,
        threadSafe: operation.threadSafe === true,
      };
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: false,
        sharedBlock: blockId,
      };
      recordOutcome(state, operation, outcome);
      return;
    }
    case "copyShared": {
      const source = requireBinding(state, operation.from);
      if (!source.sharedBlock) throw new HirMemoryError("operationRequiresSharedOwner");
      if (state.bindings[operation.to]) throw new HirMemoryError("copyTargetAlreadyInitialized");
      const block = controlBlockTable(state)[source.sharedBlock];
      if (!block || !block.blockAlive || !block.payloadAlive || block.strong < 1) {
        throw new HirMemoryError("sharedControlBlockUnavailable");
      }
      block.strong += 1;
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: false,
        sharedBlock: source.sharedBlock,
      };
      return;
    }
    case "copyWeak": {
      const source = requireBinding(state, operation.from);
      if (!source.weakBlock) throw new HirMemoryError("operationRequiresWeakOwner");
      if (state.bindings[operation.to]) throw new HirMemoryError("copyTargetAlreadyInitialized");
      const block = controlBlockTable(state)[source.weakBlock];
      if (!block || !block.blockAlive || block.weak < 1) {
        throw new HirMemoryError("weakControlBlockUnavailable");
      }
      block.weak += 1;
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: false,
        weakBlock: source.weakBlock,
      };
      return;
    }
    case "makeWeak": {
      const source = requireBinding(state, operation.from);
      if (!source.sharedBlock) throw new HirMemoryError("operationRequiresSharedOwner");
      if (state.bindings[operation.to]) throw new HirMemoryError("copyTargetAlreadyInitialized");
      const block = controlBlockTable(state)[source.sharedBlock];
      if (!block || !block.blockAlive || !block.payloadAlive) {
        throw new HirMemoryError("sharedControlBlockUnavailable");
      }
      block.weak += 1;
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: false,
        weakBlock: source.sharedBlock,
      };
      return;
    }
    case "readWeak":
    case "upgradeWeak": {
      const source = requireBinding(state, operation.from);
      if (!source.weakBlock) throw new HirMemoryError("operationRequiresWeakOwner");
      if (state.bindings[operation.to]) throw new HirMemoryError("copyTargetAlreadyInitialized");
      const block = controlBlockTable(state)[source.weakBlock];
      if (!block || !block.blockAlive) throw new HirMemoryError("weakControlBlockUnavailable");
      if (!block.payloadAlive || block.strong === 0) {
        recordOutcome(state, operation, "none");
        return;
      }
      block.strong += 1;
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: false,
        sharedBlock: source.weakBlock,
      };
      recordOutcome(state, operation, "some");
      return;
    }
    case "analyzeSharedGraph": {
      const analysis = classifySharedGraph(operation);
      if (
        analysis.phase === "compile" &&
        analysis.closed &&
        analysis.cycles.some((cycle) => cycle.unbreakable)
      ) {
        throw new HirMemoryError("unbreakableStrongCycle");
      }
      if (analysis.phase === "drainedBoundary") {
        if (!analysis.drained) throw new HirMemoryError("sharedCycleAuditBeforeDrain");
        if (analysis.cycles.some((cycle) => !cycle.rooted)) {
          throw new HirMemoryError("residualStrongCycle");
        }
      }
      if (!state.sharedCycleAnalyses) state.sharedCycleAnalyses = [];
      state.sharedCycleAnalyses.push(analysis);
      return;
    }
    case "pin": {
      const source = requireUniqueBinding(state, operation.from);
      const payload = state.payloads[source.payload];
      requireNoLoans(state, source.payload, "pinWithLoan");
      if (ownerHasDynamicEdges(state, source.payload)) {
        throw new HirMemoryError("pinWithDependency");
      }
      if (payload.selfReference || operation.selfReference) throw new HirMemoryError("selfReferentialValue");
      if (state.bindings[operation.to]) throw new HirMemoryError("pinTargetAlreadyInitialized");
      const outcome = operation.outcome ?? "success";
      if (operation.using) requireAllocator(state, operation.using);
      if (outcome === "allocationError") {
        consumeAfterAllocationFailure(state, source);
        recordOutcome(state, operation, outcome);
        return;
      }
      if (operation.using && operation.adopt !== true) {
        chargeAllocator(state, operation.using, operation.bytes ?? 0);
        payload.storageOrigins = [operation.using];
      }
      source.state = "moved";
      payload.address = "stable";
      payload.pinnedPayload = true;
      payload.pinnedRoot = `pin:${payload.root}`;
      payload.root = payload.pinnedRoot;
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: true,
      };
      recordOutcome(state, operation, outcome);
      return;
    }
    case "publishAddress": {
      const binding = requireBinding(state, operation.binding);
      const payload = state.payloads[binding.payload];
      if (!binding.pinnedHandle || payload.address === "unstable") {
        throw new HirMemoryError("publishRequiresPinnedStorage");
      }
      payload.address = "published";
      return;
    }
    case "suspend": {
      verifyActiveLoansStable(state);
      return;
    }
    case "verifyBoundary": {
      if (!acceptsRepresentation(operation.boundary, operation.representation)) {
        throw new HirMemoryError("nonCanonicalBoundaryRepresentation");
      }
      if (operation.binding) {
        const binding = requireBinding(state, operation.binding);
        if (!state.payloads[binding.payload].allocatorKnown) {
          throw new HirMemoryError("missingAllocatorOrigin");
        }
      }
      return;
    }
    case "verifyAbi": {
      const fields = ["target", "callingConvention", "representationPolicy", "runtimeAbi"];
      if (fields.some((field) => operation.expectation[field] !== operation.provider[field])) {
        throw new HirMemoryError("abiMismatch");
      }
      if (
        typeof operation.expectation.providerInterfaceKey !== "string" ||
        operation.expectation.providerInterfaceKey.length === 0 ||
        typeof operation.provider.semanticInterfaceKey !== "string" ||
        operation.provider.semanticInterfaceKey.length === 0 ||
        operation.expectation.providerInterfaceKey !== operation.provider.semanticInterfaceKey
      ) {
        throw new HirMemoryError("interfaceLockMismatch");
      }
      return;
    }
    case "verifyInterface": {
      const mapping = deriveInterfaceMapping(operation);
      if (operation.expectedMapping && JSON.stringify(mapping) !== JSON.stringify(operation.expectedMapping)) {
        throw new HirMemoryError("interfaceOriginMismatch");
      }
      if (operation.previousMapping && JSON.stringify(operation.previousMapping) !== JSON.stringify(mapping)) {
        throw new HirMemoryError("interfaceMappingChanged");
      }
      const witness = operation.witnessMapping ?? operation.witness;
      if (witness && JSON.stringify(witness) !== JSON.stringify(mapping)) {
        throw new HirMemoryError("interfaceWitnessMismatch");
      }
      if (operation.lockMapping && JSON.stringify(operation.lockMapping) !== JSON.stringify(mapping)) {
        throw new HirMemoryError("interfaceLockMismatch");
      }
      const storageMapping = operation.inferredStorageMapping ?? {};
      if (
        operation.expectedStorageMapping &&
        JSON.stringify(operation.expectedStorageMapping) !== JSON.stringify(storageMapping)
      ) {
        throw new HirMemoryError("interfaceStorageOriginMismatch");
      }
      if (
        operation.previousStorageMapping &&
        JSON.stringify(operation.previousStorageMapping) !== JSON.stringify(storageMapping)
      ) {
        throw new HirMemoryError("interfaceStorageMappingChanged");
      }
      return;
    }
    case "verifyFfi": {
      if (
        operation.projection &&
        ["packed", "unaligned", "union", "opaque", "foreign", "foreignBoundary"].includes(
          operation.projection,
        )
      ) {
        throw new HirMemoryError("unsafeFfiProjection");
      }
      if (operation.result === "opaqueBorrowed") throw new HirMemoryError("opaqueBorrowedReturn");
      if (operation.form === "languageFn") {
        const trustedWAdapter = operation.trustedWAdapterProof === true && operation.adapterLanguage === "W";
        if (!trustedWAdapter) {
          throw new HirMemoryError("opaqueLanguageLifetime");
        }
        if (operation.retention === "call" || operation.retention === "none") return;
      }
      if (operation.retention === "call" && ["ref", "inout"].includes(operation.form)) return;
      if (operation.retention === "persistent") {
        if (operation.pinned !== true || operation.destroy !== true || operation.unregister !== true) {
          throw new HirMemoryError("ffiRetentionNeedsLease");
        }
        return;
      }
      if (operation.retention === "none" && ["ref", "inout"].includes(operation.form)) return;
      throw new HirMemoryError("ffiLifetimeBoundary");
    }
    case "joinOwnerStates": {
      if (
        operation.states.length === 0 ||
        operation.states.some((ownerState) => ownerState !== operation.states[0])
      ) {
        throw new HirMemoryError("ownerStateMismatchAtJoin");
      }
      return;
    }
    default:
      throw new HirMemoryError("unknownMemoryOperation");
  }
}

function validPlace(place) {
  if (!place || typeof place !== "object" || typeof place.root !== "string" || place.root.length === 0) {
    return false;
  }
  return (
    Array.isArray(place.projections) &&
    place.projections.every((projection) => {
      if (
        !projection ||
        typeof projection !== "object" ||
        !(PROJECTION_KINDS.has(projection.kind) || projection.kind === "opaque")
      ) {
        return false;
      }
      if (projection.kind !== "range") return true;
      if (projection.end !== undefined) return false;
      if (projection.dynamic === true) return true;
      const start = numeric(projection.start);
      const endExclusive = numeric(projection.endExclusive);
      return start !== null && endExclusive !== null && start <= endExclusive;
    })
  );
}

function validEdgeInput(edge) {
  if (!edge || typeof edge !== "object" || typeof edge.origin !== "string" || edge.origin.length === 0) {
    return false;
  }
  if (edge.id !== undefined && (typeof edge.id !== "string" || edge.id.length === 0)) return false;
  if (edge.mode !== undefined && !BORROW_MODES.has(edge.mode)) return false;
  if (edge.kind !== undefined && !DEPENDENCY_KINDS.has(edge.kind)) return false;
  if (edge.address !== undefined && !ADDRESS_STATES.has(edge.address)) return false;
  if (edge.ownerRoot !== undefined && (typeof edge.ownerRoot !== "string" || edge.ownerRoot.length === 0)) return false;
  if (
    edge.place !== undefined &&
    !(
      (typeof edge.place === "string" && edge.place.length > 0) ||
      validPlace(edge.place)
    )
  ) {
    return false;
  }
  for (const flag of ["dynamic", "static", "immortal", "ownerValid"]) {
    if (edge[flag] !== undefined && typeof edge[flag] !== "boolean") return false;
  }
  const staticEdge =
    edge.static === true ||
    edge.immortal === true ||
    edge.dynamic === false ||
    edge.kind === "static" ||
    edge.kind === "immortal";
  const ownerSources = [edge.ownerBinding, edge.ownerPayload, edge.ownerSlot].filter(
    (value) => typeof value === "string" && value.length > 0,
  );
  if (staticEdge && (edge.ownerBinding !== undefined || edge.ownerPayload !== undefined)) return false;
  return staticEdge ? ownerSources.length <= 1 : ownerSources.length === 1;
}

function validProofFact(fact) {
  if (!fact || typeof fact !== "object") return false;
  if (fact.kind === "disjoint") {
    return [fact.left, fact.right].every(
      (value) => typeof value === "string" && value.length > 0,
    );
  }
  if (fact.kind === "activeVariant" || fact.kind === "enumActiveVariant") {
    const variant = fact.variant ?? fact.name;
    return (
      typeof fact.place === "string" &&
      fact.place.length > 0 &&
      typeof variant === "string" &&
      variant.length > 0
    );
  }
  return false;
}

function validInterfaceMapping(mapping) {
  if (!mapping || typeof mapping !== "object" || Array.isArray(mapping)) return false;
  return Object.entries(mapping).every(
    ([slot, sources]) =>
      slot.length > 0 &&
      Array.isArray(sources) &&
      sources.length > 0 &&
      sources.every((source) => typeof source === "string" && source.length > 0),
  );
}

export function validateMemoryOperation(operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") {
    return false;
  }
  if (
    operation.proofFacts !== undefined &&
    (!Array.isArray(operation.proofFacts) || !operation.proofFacts.every(validProofFact))
  ) {
    return false;
  }
  const hasString = (field) => typeof operation[field] === "string" && operation[field].length > 0;
  const hasPlace = () =>
    operation.place === undefined ||
    (typeof operation.place === "string" && operation.place.length > 0) ||
    validPlace(operation.place);

  switch (operation.op) {
    case "defineAllocator":
      return (
        hasString("allocator") &&
        ALLOCATOR_LIFETIMES.has(operation.lifetime) &&
        ALLOCATOR_MOBILITIES.has(operation.mobility) &&
        hasString("adoptionFamily") &&
        (operation.limit === undefined || (Number.isSafeInteger(operation.limit) && operation.limit >= 0)) &&
        (
          operation.outlives === undefined ||
          (
            Array.isArray(operation.outlives) &&
            operation.outlives.every((name) => typeof name === "string" && name.length > 0)
          )
        )
      );
    case "closeAllocator":
      return hasString("allocator");
    case "initialize":
      return (
        hasString("binding") &&
        (operation.root === undefined || hasString("root")) &&
        (!operation.address || ADDRESS_STATES.has(operation.address)) &&
        (operation.allocatorKnown === undefined || typeof operation.allocatorKnown === "boolean") &&
        (operation.copyable === undefined || typeof operation.copyable === "boolean") &&
        (operation.inoutField === undefined || typeof operation.inoutField === "boolean") &&
        (operation.edges === undefined || (Array.isArray(operation.edges) && operation.edges.every(validEdgeInput))) &&
        (operation.selfReference === undefined || typeof operation.selfReference === "boolean") &&
        (operation.using === undefined || hasString("using")) &&
        (operation.bytes === undefined || (Number.isSafeInteger(operation.bytes) && operation.bytes >= 0))
      );
    case "pinConstruct": {
      const strings = (value) =>
        Array.isArray(value) && value.every((item) => typeof item === "string" && item.length > 0);
      const argumentsInOrder = operation.arguments ?? [];
      const consumedArguments = operation.consumedArguments ?? [];
      const initializedFields = operation.initializedFields ?? [];
      const outcome = operation.outcome ?? "success";
      return (
        hasString("binding") &&
        (operation.root === undefined || hasString("root")) &&
        strings(argumentsInOrder) &&
        strings(operation.fields ?? []) &&
        strings(consumedArguments) &&
        strings(initializedFields) &&
        new Set(argumentsInOrder).size === argumentsInOrder.length &&
        new Set(operation.fields ?? []).size === (operation.fields ?? []).length &&
        consumedArguments.every((argument) => argumentsInOrder.includes(argument)) &&
        PIN_CONSTRUCT_OUTCOMES.has(outcome) &&
        (
          outcome === "argumentError"
            ? Number.isInteger(operation.failedArgumentIndex) &&
              operation.failedArgumentIndex >= 0 &&
              operation.failedArgumentIndex < argumentsInOrder.length
            : operation.failedArgumentIndex === undefined
        ) &&
        (operation.delegationDepth === undefined ||
          (Number.isSafeInteger(operation.delegationDepth) && operation.delegationDepth >= 0)) &&
        (operation.selfReference === undefined || typeof operation.selfReference === "boolean") &&
        (operation.publishBeforeCommit === undefined || typeof operation.publishBeforeCommit === "boolean") &&
        (operation.allocatorKnown === undefined || typeof operation.allocatorKnown === "boolean") &&
        (operation.copyable === undefined || typeof operation.copyable === "boolean") &&
        (operation.using === undefined || hasString("using")) &&
        (operation.bytes === undefined || (Number.isSafeInteger(operation.bytes) && operation.bytes >= 0)) &&
        (operation.result === undefined || hasString("result"))
      );
    }
    case "use":
    case "read":
    case "write":
      return hasString("binding") && hasPlace();
    case "drop":
    case "publishAddress":
      return hasString("binding");
    case "move":
    case "copy":
      return hasString("from") && hasString("to") && operation.from !== operation.to;
    case "pin":
      return (
        hasString("from") &&
        hasString("to") &&
        operation.from !== operation.to &&
        (operation.outcome === undefined || ALLOCATION_OUTCOMES.has(operation.outcome)) &&
        (operation.result === undefined || hasString("result")) &&
        (operation.using === undefined || hasString("using")) &&
        (operation.bytes === undefined || (Number.isSafeInteger(operation.bytes) && operation.bytes >= 0)) &&
        (operation.adopt === undefined || typeof operation.adopt === "boolean")
      );
    case "rehome":
      return (
        hasString("from") &&
        hasString("to") &&
        operation.from !== operation.to &&
        hasString("using") &&
        (operation.outcome === undefined || ALLOCATION_OUTCOMES.has(operation.outcome)) &&
        (operation.result === undefined || hasString("result")) &&
        (operation.bytes === undefined || (Number.isSafeInteger(operation.bytes) && operation.bytes >= 0)) &&
        (operation.adopt === undefined || typeof operation.adopt === "boolean")
      );
    case "erase": {
      const validSize = (value) => Number.isSafeInteger(value) && value >= 0;
      const validAlignment = (value) =>
        Number.isSafeInteger(value) && value > 0 && Number.isInteger(Math.log2(value));
      return (
        hasString("from") &&
        hasString("to") &&
        operation.from !== operation.to &&
        validSize(operation.payloadBytes) &&
        validAlignment(operation.payloadAlignment) &&
        validSize(operation.inlineBytes) &&
        validAlignment(operation.inlineAlignment) &&
        ERASURE_SPILL_POLICIES.has(operation.spill) &&
        (
          (operation.spill === "allocator" && hasString("using")) ||
          (operation.spill === "forbid" && operation.using === undefined)
        ) &&
        validSize(operation.boxBytes) &&
        (operation.outcome === undefined || ALLOCATION_OUTCOMES.has(operation.outcome)) &&
        (operation.result === undefined || hasString("result"))
      );
    }
    case "share":
      return (
        hasString("from") &&
        hasString("to") &&
        operation.from !== operation.to &&
        hasString("using") &&
        (operation.outcome === undefined || ALLOCATION_OUTCOMES.has(operation.outcome)) &&
        (operation.result === undefined || hasString("result")) &&
        (operation.bytes === undefined || (Number.isSafeInteger(operation.bytes) && operation.bytes >= 0)) &&
        (operation.threadSafe === undefined || typeof operation.threadSafe === "boolean") &&
        [
          "allocatorContract",
          "controlBlockOrigin",
          "controlBlockDeallocator",
          "controlBlockMobility",
          "controlBlockLifetime",
          "controlBlockAdoptionFamily",
          "controlBlockBulkReleaseOwner",
        ].every((field) => operation[field] === undefined || hasString(field))
      );
    case "copyShared":
    case "copyWeak":
    case "makeWeak":
      return hasString("from") && hasString("to") && operation.from !== operation.to;
    case "readWeak":
    case "upgradeWeak":
      return (
        hasString("from") &&
        hasString("to") &&
        operation.from !== operation.to &&
        hasString("result")
      );
    case "analyzeSharedGraph": {
      const nodes = operation.nodes;
      const edges = operation.edges;
      const externalRoots = operation.externalRoots ?? [];
      const validNames =
        Array.isArray(nodes) &&
        nodes.length > 0 &&
        nodes.every((node) => typeof node === "string" && node.length > 0) &&
        new Set(nodes).size === nodes.length;
      const validEdges =
        Array.isArray(edges) &&
        edges.length > 0 &&
        edges.every(
          (edge) =>
            edge &&
            typeof edge === "object" &&
            Object.keys(edge).sort().join(",") ===
              "from,id,mode,origin,release,to" &&
            typeof edge.id === "string" &&
            edge.id.length > 0 &&
            typeof edge.from === "string" &&
            nodes?.includes(edge.from) &&
            typeof edge.to === "string" &&
            nodes?.includes(edge.to) &&
            SHARED_EDGE_MODES.has(edge.mode) &&
            SHARED_EDGE_RELEASES.has(edge.release) &&
            typeof edge.origin === "string" &&
            edge.origin.length > 0,
        ) &&
        new Set(edges?.map((edge) => edge.id)).size === edges?.length;
      const validRoots =
        Array.isArray(externalRoots) &&
        externalRoots.every((root) => typeof root === "string" && nodes?.includes(root)) &&
        new Set(externalRoots).size === externalRoots.length;
      const validPhase =
        SHARED_GRAPH_PHASES.has(operation.phase) &&
        typeof operation.closed === "boolean" &&
        (operation.phase === "compile"
          ? operation.drained === undefined && operation.boundary === undefined
          : typeof operation.drained === "boolean" && hasString("boundary"));
      return validNames && validEdges && validRoots && validPhase;
    }
    case "beginBorrow":
      return (
        hasString("binding") &&
        hasString("token") &&
        operation.loanId === undefined &&
        BORROW_MODES.has(operation.mode) &&
        operation.parent === undefined &&
        hasPlace()
      );
    case "reborrow":
      return (
        hasString("binding") &&
        hasString("token") &&
        operation.loanId === undefined &&
        hasString("parent") &&
        BORROW_MODES.has(operation.mode) &&
        hasPlace()
      );
    case "duplicateLoan":
      return hasString("from") && hasString("token") && operation.from !== operation.token;
    case "endBorrow":
      return hasString("token") && operation.loanId === undefined;
    case "accessLoan":
      return (
        hasString("token") &&
        operation.loanId === undefined &&
        (operation.access === undefined || ACCESS_MODES.has(operation.access))
      );
    case "accessDependency":
      return (
        hasString("binding") &&
        hasString("edgeId") !== hasString("origin") &&
        (operation.access === undefined || ACCESS_MODES.has(operation.access))
      );
    case "mutate":
    case "replace":
      return hasString("binding") && hasPlace();
    case "insert":
    case "joinDependencies":
      return (
        hasString("binding") &&
        (
          operation.from === undefined ||
          (hasString("from") && operation.from !== operation.binding)
        ) &&
        (operation.edges === undefined || (Array.isArray(operation.edges) && operation.edges.every(validEdgeInput)))
      );
    case "remove":
      return hasString("binding") && (hasString("edgeId") !== hasString("origin"));
    case "clear":
      return hasString("binding");
    case "escape": {
      const flags = [
        "shareable",
        "transferable",
        "wireValue",
        "persistentValue",
        "ffiSafe",
        "joinPrecedesOrigins",
        "mobility",
      ];
      return (
        hasString("binding") &&
        ESCAPE_TARGETS.has(operation.target) &&
        flags.every(
          (flag) => operation[flag] === undefined || typeof operation[flag] === "boolean",
        ) &&
        (
          operation.availableOrigins === undefined ||
          (
            Array.isArray(operation.availableOrigins) &&
            operation.availableOrigins.every(
              (origin) => typeof origin === "string" && origin.length > 0,
            )
          )
        )
      );
    }
    case "await":
      return hasString("binding");
    case "suspend":
      return true;
    case "verifyBoundary":
      return (
        BOUNDARIES.has(operation.boundary) &&
        REPRESENTATIONS.has(operation.representation) &&
        (operation.binding === undefined || hasString("binding"))
      );
    case "verifyAbi": {
      const fields = ["target", "callingConvention", "representationPolicy", "runtimeAbi"];
      const expectation = operation.expectation;
      const provider = operation.provider;
      return (
        expectation &&
        typeof expectation === "object" &&
        provider &&
        typeof provider === "object" &&
        fields.every(
          (field) =>
            typeof expectation[field] === "string" &&
            expectation[field].length > 0 &&
            typeof provider[field] === "string" &&
            provider[field].length > 0,
        ) &&
        (
          expectation.providerInterfaceKey === undefined ||
          (typeof expectation.providerInterfaceKey === "string" &&
            expectation.providerInterfaceKey.length > 0)
        ) &&
        (
          provider.semanticInterfaceKey === undefined ||
          (typeof provider.semanticInterfaceKey === "string" &&
            provider.semanticInterfaceKey.length > 0)
        )
      );
    }
    case "verifyInterface":
      return (
        typeof operation.body === "boolean" &&
        [
          "inferredMapping",
          "expectedMapping",
          "previousMapping",
          "witnessMapping",
          "witness",
          "lockMapping",
          "inferredStorageMapping",
          "expectedStorageMapping",
          "previousStorageMapping",
        ].every(
          (field) =>
            operation[field] === undefined || validInterfaceMapping(operation[field]),
        )
      );
    case "verifyFfi":
      return (
        FFI_FORMS.has(operation.form) &&
        FFI_RETENTIONS.has(operation.retention) &&
        ["trustedWAdapterProof", "pinned", "destroy", "unregister"].every(
          (field) =>
            operation[field] === undefined || typeof operation[field] === "boolean",
        ) &&
        (
          operation.adapterLanguage === undefined ||
          (typeof operation.adapterLanguage === "string" && operation.adapterLanguage.length > 0)
        )
      );
    case "joinOwnerStates":
      return Array.isArray(operation.states) && operation.states.every((ownerState) => OWNER_STATES.has(ownerState));
    default:
      return false;
  }
}

export function runMemoryProgram(operations) {
  let state = {
    schema: "w-memory-state-m1",
    bindings: {},
    payloads: {},
    loans: {},
    nextPayload: 0,
    nextLoan: 0,
    nextEdge: 0,
  };
  const trace = [];

  for (const [index, operation] of operations.entries()) {
    const before = clone(state);
    const next = clone(state);
    try {
      if (!validateMemoryOperation(operation)) {
        throw new HirMemoryError("invalidMemoryOperation");
      }
      applyOperation(next, operation);
      state = next;
      trace.push({ index, operation: clone(operation), before, after: clone(state) });
    } catch (error) {
      if (!(error instanceof HirMemoryError)) throw error;
      trace.push({
        index,
        operation: clone(operation),
        before,
        rejected: error.code,
        ...(error.facts ? { facts: clone(error.facts) } : {}),
      });
      return {
        status: "rejected",
        code: error.code,
        operation: index,
        ...(error.facts ? { facts: clone(error.facts) } : {}),
        state: before,
        trace,
      };
    }
  }

  return { status: "accepted", state, trace };
}
