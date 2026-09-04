import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const VALID_MODES = new Set(["strong", "weak", "service"]);
const VALID_RELEASES = new Set(["deinitOnly", "explicitClose", "lifecycleDrain", "weak"]);
const VALID_AUTHORITIES = new Set(["w", "adapter", "foreign", "foreign-hidden"]);
const VALID_CALL_CYCLES = new Set(["metadata", "external"]);
const VALID_EVENT_NAMES = new Set([
  "admit",
  "closeAdmission",
  "addStrong",
  "addWeak",
  "removeEdge",
  "close",
  "unlink",
  "rootAdd",
  "rootDrop",
  "callbackEnter",
  "callbackExit",
  "unregister",
  "cancel",
  "drain",
  "finish",
  "quiesce",
  "typedDrop",
  "destroy",
  "unpin",
  "reclaim",
  "weakRead",
  "weakDrop",
  "lock",
  "unlock",
  "reuseAddress",
  "selfWeakInit",
  "publish",
  "panic",
  "faultBoundary",
  "census",
]);
const FORBIDDEN_INPUT_KEYS = new Set([
  "accepted",
  "result",
  "status",
  "disposition",
  "collector",
  "collect",
  "target",
  "provider",
  "providerReady",
]);
const FORBIDDEN_EVENT_KEYS = new Set([...FORBIDDEN_INPUT_KEYS].filter((key) => key !== "target"));
const REQUIRED_CASES = [
  "CYC1-POS-menu-weak-parent",
  "CYC1-POS-observer-weak-capture",
  "CYC1-NEG-strong-callback-scc",
  "CYC1-POS-explicit-close-break",
  "CYC1-POS-lifecycle-drain-break",
  "CYC1-NEG-self-cycle",
  "CYC1-DYN-runtime-strong-cycle",
  "CYC1-POS-live-root-after-drain",
  "CYC1-NEG-residual-after-drain",
  "CYC1-NEG-unrelated-root-does-not-hide-residual",
  "CYC1-NEG-census-before-drain",
  "CYC1-POS-ffi-order",
  "CYC1-NEG-ffi-inflight",
  "CYC1-UNK-hidden-foreign-root",
  "CYC1-POS-adapter-foreign-root",
  "CYC1-POS-service-ref-not-ownership",
  "CYC1-NEG-service-call-cycle",
  "CYC1-POS-external-service-deadline",
  "CYC1-POS-resource-async-finish",
  "CYC1-FAULT-panic-fault-boundary",
  "CYC1-POS-cross-domain-control-block",
  "CYC1-NEG-cross-domain-missing-facts",
  "CYC1-POS-concurrent-unlink-under-lock",
  "CYC1-NEG-concurrent-unlink-without-lock",
  "CYC1-NEG-address-reuse-before-weak-zero",
  "CYC1-POS-weak-read-linearization",
  "CYC1-POS-weak-blocks-per-target",
  "CYC1-NEG-resurrection",
  "CYC1-POS-self-weak-two-phase",
  "CYC1-RESEARCH-self-weak-constructor",
  "CYC1-POS-linked-list-weak-back-edge",
  "CYC1-NEG-linked-list-strong-back-edge",
  "CYC1-POS-generation-id-cache",
  "CYC1-POS-owner-scoped-cache-lease",
  "CYC1-POS-detached-cache-value",
  "CYC1-RESEARCH-naive-weak-key",
  "CYC1-RESEARCH-ephemeron-value-key-cycle",
  "CYC1-REJECT-transparent-collector",
  "CYC1-INFO-census-quota",
  "CYC1-INFO-large-chain-lowering-required",
];

function output(testCase, status, code, details = {}) {
  return { caseId: testCase.id, status, code, ...details };
}

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function nonEmpty(value) {
  return typeof value === "string" && value.trim() !== "";
}

function clone(value) {
  return structuredClone(value);
}

function edgeIsStrong(edge) {
  return edge.mode === "strong";
}

function edgeIsActive(edge) {
  return edge.active !== false;
}

function graphNodes(state) {
  return [...state.nodes.values()].filter((node) => node.alive);
}

function activeStrongEdges(state) {
  return [...state.edges.values()].filter(
    (edge) => edgeIsActive(edge) && edgeIsStrong(edge) && state.nodes.get(edge.from)?.alive && state.nodes.get(edge.to)?.alive,
  );
}

function payloadStrongOwnersZero(state, target) {
  const node = state.nodes.get(target);
  if (!node || node.alive) return false;
  if (activeStrongEdges(state).some((edge) => edge.to === target)) return false;
  return ![...state.roots.values()].some((root) => root.active && root.node === target);
}

function updateControlBlocks(state, target) {
  const block = state.controlBlocks.get(target);
  if (!block || block.freed) return;
  const hasLiveWeak = [...state.weakHandles.values()].some((handle) => handle.target === target && handle.alive);
  if (!hasLiveWeak && payloadStrongOwnersZero(state, target)) {
    block.freed = true;
    block.freeCount += 1;
  }
  state.controlBlockFreed = state.controlBlocks.size > 0 && [...state.controlBlocks.values()].every((candidate) => candidate.freed);
}

function controlBlockMap(state) {
  return Object.fromEntries([...state.controlBlocks.entries()].sort(([left], [right]) => left.localeCompare(right)).map(([target, block]) => [target, block.freed]));
}

function controlBlockFreeCountMap(state) {
  return Object.fromEntries([...state.controlBlocks.entries()].sort(([left], [right]) => left.localeCompare(right)).map(([target, block]) => [target, block.freeCount]));
}

function tarjan(state) {
  const nodes = graphNodes(state).map((node) => node.id);
  const adjacency = new Map(nodes.map((id) => [id, []]));
  for (const edge of activeStrongEdges(state)) adjacency.get(edge.from)?.push(edge.to);
  let index = 0;
  const indices = new Map();
  const lowLinks = new Map();
  const stack = [];
  const onStack = new Set();
  const components = [];

  function visit(nodeId) {
    indices.set(nodeId, index);
    lowLinks.set(nodeId, index);
    index += 1;
    stack.push(nodeId);
    onStack.add(nodeId);
    for (const child of adjacency.get(nodeId) ?? []) {
      if (!indices.has(child)) {
        visit(child);
        lowLinks.set(nodeId, Math.min(lowLinks.get(nodeId), lowLinks.get(child)));
      } else if (onStack.has(child)) {
        lowLinks.set(nodeId, Math.min(lowLinks.get(nodeId), indices.get(child)));
      }
    }
    if (lowLinks.get(nodeId) === indices.get(nodeId)) {
      const component = [];
      let child;
      do {
        child = stack.pop();
        onStack.delete(child);
        component.push(child);
      } while (child !== nodeId);
      components.push(component.sort());
    }
  }

  for (const nodeId of nodes) if (!indices.has(nodeId)) visit(nodeId);
  return components.filter((component) => {
    if (component.length > 1) return true;
    const nodeId = component[0];
    return activeStrongEdges(state).some((edge) => edge.from === nodeId && edge.to === nodeId);
  });
}

function componentEdges(state, component) {
  const members = new Set(component);
  return activeStrongEdges(state).filter((edge) => members.has(edge.from) && members.has(edge.to));
}

function knownRootReachability(state) {
  const reachable = new Set();
  const unknownReachable = new Set();
  const adjacency = new Map(graphNodes(state).map((node) => [node.id, []]));
  for (const edge of activeStrongEdges(state)) adjacency.get(edge.from)?.push(edge);
  const queue = [];
  for (const root of state.roots.values()) {
    if (!root.active || !state.nodes.get(root.node)?.alive) continue;
    queue.push({ node: root.node, unknown: root.known === false });
  }
  while (queue.length > 0) {
    const current = queue.shift();
    const target = current.unknown ? unknownReachable : reachable;
    if (target.has(current.node)) continue;
    target.add(current.node);
    for (const edge of adjacency.get(current.node) ?? []) {
      queue.push({ node: edge.to, unknown: current.unknown || edge.known === false });
    }
  }
  return { reachable, unknownReachable };
}

function componentTouchesUnknown(state, component, unknownReachable) {
  const members = new Set(component);
  if (component.some((nodeId) => unknownReachable.has(nodeId))) return true;
  return componentEdges(state, component).some(
    (edge) => edge.known === false || edge.authority === "foreign-hidden",
  ) || [...state.roots.values()].some(
    (root) => root.active && root.known === false && members.has(root.node),
  );
}

function hasOpaqueBoundary(state) {
  return [...state.edges.values()].some(
    (edge) => edgeIsActive(edge) && (edge.known === false || edge.authority === "foreign-hidden"),
  ) || [...state.roots.values()].some((root) => root.active && root.known === false);
}

function staticCycleDisposition(state) {
  if (state.graph.identitiesKnown === false) {
    return { classification: "dynamic-open", code: "dynamic-identities-unknown", components: [] };
  }
  if (hasOpaqueBoundary(state)) {
    return { classification: "dynamic-open", code: "opaque-boundary-unknown", components: [] };
  }
  const components = tarjan(state);
  if (components.length === 0) return { classification: "accepted", code: "no-closed-strong-scc", components };
  const closed = components.filter((component) =>
    componentEdges(state, component).every((edge) => edge.release === "deinitOnly"),
  );
  if (closed.length > 0) {
    return {
      classification: "rejected",
      code: "W-OWNERSHIP-0014",
      components,
      closedComponents: closed,
    };
  }
  return { classification: "breakable", code: "strong-scc-has-explicit-break", components };
}

function allRegistrationsDrained(state) {
  return [...state.registrations.values()].every(
    (registration) => registration.inFlight === 0 && registration.unregistered && registration.drained,
  );
}

function allResourcesDrained(state) {
  return [...state.resources.values()].every(
    (resource) => resource.drained && (!resource.requiresFinish || resource.finished),
  );
}

function drainReady(state) {
  return state.admission === "closed" && allRegistrationsDrained(state) && allResourcesDrained(state);
}

function findRegistration(state, id) {
  return state.registrations.get(id);
}

function findResource(state, id) {
  return state.resources.get(id);
}

function eventTarget(event) {
  return event.registration ?? event.resource ?? event.target ?? event.node ?? event.owner;
}

function requireNode(state, id) {
  return nonEmpty(id) && state.nodes.has(id);
}

function lifecycleOwnerExists(state, owner) {
  return nonEmpty(owner) && (state.nodes.has(owner)
    || state.registrations.has(owner)
    || state.resources.has(owner)
    || (state.ownerRegistryClosed === true && state.ownerRegistry.has(owner)));
}

function applyEvent(state, event, operation) {
  if (!isObject(event) || !VALID_EVENT_NAMES.has(event.op)) {
    return { ok: false, code: "event-name-invalid", operation };
  }
  if (state.faulted && event.op !== "census") return { ok: false, code: "event-after-fault-boundary", operation };
  if (state.censusRequested && event.op !== "census") return { ok: false, code: "event-after-census", operation };
  switch (event.op) {
    case "admit":
      if (state.admission !== "open") return { ok: false, code: "admission-already-closed", operation };
      state.admitted = true;
      return { ok: true };
    case "closeAdmission":
      if (!state.admitted || state.admission !== "open") return { ok: false, code: "admission-close-order", operation };
      state.admission = "closed";
      return { ok: true };
    case "addStrong":
    case "addWeak": {
      if (state.admission !== "open") return { ok: false, code: "edge-after-admission-close", operation };
      const edge = event.edge;
      if (!isObject(edge) || !nonEmpty(edge.id) || state.edges.has(edge.id) || !requireNode(state, edge.from) || !requireNode(state, edge.to)) {
        return { ok: false, code: "edge-schema-invalid", operation };
      }
      const authority = edge.authority ?? "w";
      if (!VALID_AUTHORITIES.has(authority)) return { ok: false, code: "edge-authority-invalid", operation };
      const mode = event.op === "addWeak" ? "weak" : "strong";
      const release = mode === "weak" ? "weak" : (edge.release ?? "deinitOnly");
      if (!VALID_RELEASES.has(release)
        || ((release === "explicitClose" || release === "lifecycleDrain")
          && (!nonEmpty(edge.owner) || !lifecycleOwnerExists(state, edge.owner)))) {
        return { ok: false, code: "edge-release-invalid", operation };
      }
      // Foreign boundaries are opaque until an adapter supplies metadata.
      const known = edge.known !== false && !["foreign", "foreign-hidden"].includes(authority);
      state.edges.set(edge.id, {
        id: edge.id,
        from: edge.from,
        to: edge.to,
        mode,
        release,
        authority,
        owner: edge.owner,
        known,
        active: true,
      });
      return { ok: true };
    }
    case "removeEdge": {
      const edge = state.edges.get(event.edge);
      if (!edge || !edgeIsActive(edge)) return { ok: false, code: "edge-remove-missing", operation };
      if (state.graph.concurrent && !state.locked) return { ok: false, code: "mutation-without-lock", operation };
      if (edge.release !== "explicitClose") return { ok: false, code: "remove-edge-not-authorized", operation };
      if (!nonEmpty(event.owner) || !lifecycleOwnerExists(state, event.owner)) return { ok: false, code: "remove-edge-owner-invalid", operation };
      if (edge.owner !== event.owner) return { ok: false, code: "remove-edge-owner-mismatch", operation };
      edge.active = false;
      return { ok: true };
    }
    case "close":
    case "unlink": {
      const edgeIds = Array.isArray(event.edges) ? event.edges : [event.edge];
      if (edgeIds.length === 0 || edgeIds.some((id) => !nonEmpty(id))) return { ok: false, code: "close-edge-list-invalid", operation };
      if (state.graph.concurrent && !state.locked) return { ok: false, code: "mutation-without-lock", operation };
      const owner = event.owner;
      if (!nonEmpty(owner) || !lifecycleOwnerExists(state, owner)) return { ok: false, code: "close-owner-invalid", operation };
      for (const id of edgeIds) {
        const edge = state.edges.get(id);
        if (!edge || !edgeIsActive(edge)) return { ok: false, code: "close-edge-missing", operation };
        if (edge.release !== "explicitClose") return { ok: false, code: "close-not-authorized", operation };
        if (edge.owner !== owner) return { ok: false, code: "close-owner-mismatch", operation };
      }
      for (const id of edgeIds) state.edges.get(id).active = false;
      state.closedOwners.add(owner);
      return { ok: true };
    }
    case "rootAdd": {
      if (state.admission !== "open") return { ok: false, code: "root-after-admission-close", operation };
      if (!requireNode(state, event.node)) return { ok: false, code: "root-node-missing", operation };
      const authority = event.authority ?? "w";
      if (!VALID_AUTHORITIES.has(authority)) return { ok: false, code: "root-authority-invalid", operation };
      const id = event.id ?? `root:${event.node}:${state.roots.size}`;
      if (state.roots.has(id)) return { ok: false, code: "root-duplicate", operation };
      state.roots.set(id, {
        id,
        node: event.node,
        active: true,
        known: event.known !== false && !["foreign", "foreign-hidden"].includes(authority),
        authority,
      });
      return { ok: true };
    }
    case "rootDrop": {
      const root = state.roots.get(event.id ?? event.node);
      if (!root || !root.active) return { ok: false, code: "root-drop-missing", operation };
      root.active = false;
      return { ok: true };
    }
    case "callbackEnter": {
      const registration = findRegistration(state, event.registration);
      if (!registration || registration.unregistered || state.admission !== "open") return { ok: false, code: "callback-admission-closed", operation };
      registration.inFlight += 1;
      return { ok: true };
    }
    case "callbackExit": {
      const registration = findRegistration(state, event.registration);
      if (!registration || registration.inFlight < 1) return { ok: false, code: "callback-exit-underflow", operation };
      registration.inFlight -= 1;
      return { ok: true };
    }
    case "unregister": {
      const registration = findRegistration(state, event.registration);
      if (!registration || registration.unregistered) return { ok: false, code: "unregister-missing", operation };
      if (state.admission !== "closed") return { ok: false, code: "unregister-before-admission-close", operation };
      registration.unregistered = true;
      for (const resource of state.resources.values()) {
        if (resource.registration === registration.id) resource.unregistered = true;
      }
      return { ok: true };
    }
    case "cancel":
      state.cancellationRequested = true;
      return { ok: true };
    case "drain": {
      if (state.admission !== "closed") return { ok: false, code: "drain-before-admission-close", operation };
      const target = eventTarget(event);
      const drainOwnerEdges = (owner) => {
        for (const edge of state.edges.values()) {
          if (edgeIsActive(edge) && edge.release === "lifecycleDrain" && !nonEmpty(edge.owner)) return false;
        }
        for (const edge of state.edges.values()) {
          if (!edgeIsActive(edge) || edge.release !== "lifecycleDrain") continue;
          if (owner === "all" || owner === undefined || edge.owner === owner) edge.active = false;
        }
        return true;
      };
      if (target === "all" || target === undefined) {
        for (const registration of state.registrations.values()) {
          if (!registration.unregistered || registration.inFlight !== 0) return { ok: false, code: "drain-with-live-callback", operation };
        }
        for (const resource of state.resources.values()) {
          if (resource.inFlight > 0) return { ok: false, code: "drain-with-live-resource", operation };
        }
        if (!drainOwnerEdges("all")) return { ok: false, code: "drain-edge-owner-missing", operation };
        for (const registration of state.registrations.values()) registration.drained = true;
        for (const resource of state.resources.values()) resource.drained = true;
      } else if (state.registrations.has(target)) {
        const registration = state.registrations.get(target);
        if (!registration.unregistered || registration.inFlight !== 0) return { ok: false, code: "drain-with-live-callback", operation };
        if ([...state.resources.values()].some((resource) => resource.registration === registration.id && resource.inFlight > 0)) {
          return { ok: false, code: "drain-with-live-resource", operation };
        }
        if (!drainOwnerEdges(registration.id)) return { ok: false, code: "drain-edge-owner-missing", operation };
        registration.drained = true;
        for (const resource of state.resources.values()) {
          if (resource.registration === registration.id) resource.drained = true;
        }
      } else if (state.resources.has(target)) {
        const resource = state.resources.get(target);
        if (resource.inFlight > 0) return { ok: false, code: "drain-with-live-resource", operation };
        if (!drainOwnerEdges(resource.id)) return { ok: false, code: "drain-edge-owner-missing", operation };
        resource.drained = true;
      } else if (state.nodes.has(target)) {
        if (!drainOwnerEdges(target)) return { ok: false, code: "drain-edge-owner-missing", operation };
      } else {
        return { ok: false, code: "drain-target-missing", operation };
      }
      state.drainRequested = true;
      state.drainComplete = drainReady(state);
      return { ok: true };
    }
    case "finish": {
      const resource = findResource(state, event.resource);
      if (!resource || resource.finished) return { ok: false, code: "finish-resource-missing", operation };
      if (state.admission !== "closed" || !resource.drained) return { ok: false, code: "finish-before-drain", operation };
      resource.finished = true;
      state.drainComplete = drainReady(state);
      return { ok: true };
    }
    case "quiesce":
      if (!drainReady(state)) return { ok: false, code: "quiesce-before-drain", operation };
      state.quiescent = true;
      state.drainComplete = true;
      return { ok: true };
    case "typedDrop": {
      const node = state.nodes.get(event.node);
      if (!node || !node.alive) return { ok: false, code: "typed-drop-missing", operation };
      if ([...state.roots.values()].some((root) => root.active && root.node === node.id)) return { ok: false, code: "typed-drop-live-root", operation };
      const incoming = activeStrongEdges(state).filter((edge) => edge.to === node.id);
      if (incoming.length > 0) return { ok: false, code: "typed-drop-live-incoming-owner", operation };
      const resources = [...state.resources.values()].filter((resource) => resource.node === node.id && (!resource.finished || !resource.drained));
      if (resources.length > 0) return { ok: false, code: "typed-drop-live-resource", operation };
      node.alive = false;
      node.dropped = true;
      for (const edge of state.edges.values()) if (edge.from === node.id) edge.active = false;
      state.dropOrder.push(node.id);
      updateControlBlocks(state, node.id);
      return { ok: true };
    }
    case "destroy": {
      const resource = findResource(state, event.resource);
      if (!resource || resource.destroyed) return { ok: false, code: "destroy-resource-missing", operation };
      if (!resource.drained || !resource.unregistered || resource.inFlight !== 0) return { ok: false, code: "destroy-before-ffi-drain", operation };
      if (resource.requiresFinish && !resource.finished) return { ok: false, code: "destroy-before-finish", operation };
      resource.destroyed = true;
      return { ok: true };
    }
    case "unpin": {
      const resource = findResource(state, event.resource);
      if (!resource || resource.unpinned || !resource.destroyed) return { ok: false, code: "unpin-before-destroy", operation };
      resource.unpinned = true;
      return { ok: true };
    }
    case "reclaim": {
      const resource = findResource(state, event.resource);
      if (!resource || resource.reclaimed || !resource.unpinned) return { ok: false, code: "reclaim-before-unpin", operation };
      resource.reclaimed = true;
      return { ok: true };
    }
    case "weakRead": {
      const handle = state.weakHandles.get(event.handle);
      if (!handle || !handle.alive || !state.nodes.has(handle.target)) return { ok: false, code: "weak-handle-missing", operation };
      const alive = state.nodes.get(handle.target).alive;
      if (!alive && event.resurrect === true) return { ok: false, code: "weak-resurrection-forbidden", operation };
      state.weakReads.push({ handle: handle.id, value: alive ? "some" : "none" });
      return { ok: true };
    }
    case "weakDrop": {
      const handle = state.weakHandles.get(event.handle);
      if (!handle || !handle.alive) return { ok: false, code: "weak-drop-missing", operation };
      handle.alive = false;
      updateControlBlocks(state, handle.target);
      state.controlBlockTrace.push({ handle: handle.id, target: handle.target, freed: state.controlBlocks.get(handle.target)?.freed === true });
      return { ok: true };
    }
    case "lock":
      if (state.locked) return { ok: false, code: "lock-double", operation };
      state.locked = true;
      state.lockOwner = event.owner ?? "anonymous";
      return { ok: true };
    case "unlock":
      if (!state.locked) return { ok: false, code: "unlock-without-lock", operation };
      state.locked = false;
      state.lockOwner = undefined;
      return { ok: true };
    case "reuseAddress":
      if ([...state.weakHandles.values()].some((handle) => handle.alive && state.nodes.get(handle.target)?.alive === false)) return { ok: false, code: "address-reuse-before-weak-zero", operation };
      state.addressReuse = true;
      return { ok: true };
    case "selfWeakInit":
      if (!requireNode(state, event.node)) return { ok: false, code: "self-node-missing", operation };
      if (!state.published.has(event.node)) return { ok: false, code: "self-weak-before-publication", operation };
      if (event.phase === "constructor") return { ok: false, code: "self-weak-constructor-is-candidate", operation };
      state.selfWeakInitialized.add(event.node);
      return { ok: true };
    case "publish":
      if (!requireNode(state, event.node) || state.published.has(event.node)) return { ok: false, code: "publish-invalid", operation };
      state.published.add(event.node);
      return { ok: true };
    case "panic":
    case "faultBoundary":
      state.faulted = true;
      state.faultCode = event.op === "panic" ? "panic-fault-boundary" : "forced-termination-fault";
      return { ok: true };
    case "census":
      if (state.censusRequested) return { ok: false, code: "census-repeated", operation };
      state.censusRequested = true;
      return { ok: true };
    default:
      return { ok: false, code: "event-not-implemented", operation };
  }
}

function classifyCensus(state) {
  if (!state.censusRequested) {
    return {
      classification: "not-audited",
      code: "W-MEMORY-CENSUS-NOT-REQUESTED",
      components: [],
      mutation: "none",
    };
  }
  if (state.admission !== "closed" || !drainReady(state) || !state.quiescent) {
    return { classification: "audit-before-drain", code: "W-MEMORY-AUDIT-BEFORE-DRAIN", components: [], mutation: "none" };
  }
  if (state.censusBudget !== undefined && graphNodes(state).length > state.censusBudget) {
    return {
      classification: "inconclusive",
      code: "W-MEMORY-CENSUS-BOUNDED",
      components: [],
      mutation: "none",
    };
  }
  const components = tarjan(state);
  const { reachable, unknownReachable } = knownRootReachability(state);
  const unknown = components.filter((component) => componentTouchesUnknown(state, component, unknownReachable));
  if (unknown.length > 0) {
    return {
      classification: "unknown",
      code: "W-MEMORY-UNKNOWN-BOUNDARY",
      components,
      unknownComponents: unknown,
      mutation: "none",
    };
  }
  if (hasOpaqueBoundary(state)) {
    return {
      classification: "unknown",
      code: "W-MEMORY-UNKNOWN-BOUNDARY",
      components,
      unknownComponents: [],
      mutation: "none",
    };
  }
  const residual = components.filter((component) => !component.some((nodeId) => reachable.has(nodeId)));
  if (residual.length > 0) {
    return {
      classification: "residual-cycle",
      code: "W-MEMORY-0001",
      components,
      residualComponents: residual,
      mutation: "none",
    };
  }
  if (components.length > 0) {
    return { classification: "live-root", code: "live-root-reachable", components, mutation: "none" };
  }
  return { classification: "clean", code: "no-residual-strong-scc", components: [], mutation: "none" };
}

function evaluateConditional(testCase) {
  const composition = testCase.composition ?? {};
  const kind = composition.kind;
  if (!["generation-id-cache", "owner-scoped-cache", "detached-value", "naive-weak-key", "ephemeron", "transparent-collector"].includes(kind)) {
    return output(testCase, "invalid", "conditional-composition-kind");
  }
  if (kind === "transparent-collector") return output(testCase, "intentionally-rejected", "transparent-collector-rejected", { route: "foreign-mechanism" });
  if (kind === "naive-weak-key") return output(testCase, "future-reopen-candidate", "ordinary-weak-insufficient", { route: "conditional-liveness" });
  if (kind === "ephemeron") return output(testCase, "future-reopen-candidate", "ephemeron-value-key-cycle", { route: "conditional-liveness" });
  if (composition.valueToKeyStrong === true) return output(testCase, "future-reopen-candidate", "value-to-key-strong-back-edge", { route: "conditional-liveness" });
  if (kind === "generation-id-cache") {
    if (!composition.idDetached || !composition.explicitInvalidation) return output(testCase, "future-reopen-candidate", "generation-cache-invalidation-missing", { route: "conditional-liveness" });
    return output(testCase, "composable-alternative", "generation-id-detaches-key", { route: "explicit-owner-composition" });
  }
  if (kind === "owner-scoped-cache") {
    if (!composition.ownerLease || !composition.explicitClose) return output(testCase, "future-reopen-candidate", "owner-cache-close-missing", { route: "conditional-liveness" });
    return output(testCase, "composable-alternative", "owner-lease-breaks-edge", { route: "explicit-owner-composition" });
  }
  if (kind === "detached-value") {
    if (!composition.detached || composition.keyIdentityRequired === true) return output(testCase, "future-reopen-candidate", "detached-value-changes-key-identity", { route: "conditional-liveness" });
    return output(testCase, "composable-alternative", "detached-value-no-back-edge", { route: "explicit-owner-composition" });
  }
  return output(testCase, "future-reopen-candidate", "conditional-liveness-unresolved", { route: "conditional-liveness" });
}

function initialiseState(testCase) {
  const graph = testCase.graph ?? {};
  if (!isObject(graph) || !Array.isArray(graph.nodes)) return { error: "graph-schema-invalid" };
  const ownerRegistryClosed = graph.ownerRegistryClosed === true;
  const ownerRegistry = new Set(Array.isArray(graph.ownerRegistry) ? graph.ownerRegistry.filter(nonEmpty) : []);
  const nodes = new Map();
  for (const rawNode of graph.nodes) {
    if (!isObject(rawNode) || !nonEmpty(rawNode.id) || nodes.has(rawNode.id)) return { error: "node-schema-invalid" };
    nodes.set(rawNode.id, { id: rawNode.id, kind: rawNode.kind ?? "object", alive: true, dropped: false });
  }
  const edges = new Map();
  for (const rawEdge of graph.edges ?? []) {
    if (!isObject(rawEdge) || !nonEmpty(rawEdge.id) || edges.has(rawEdge.id) || !requireNode({ nodes }, rawEdge.from) || !requireNode({ nodes }, rawEdge.to)) return { error: "edge-schema-invalid" };
    const mode = rawEdge.mode ?? "strong";
    if (!VALID_MODES.has(mode) || (mode === "service" && rawEdge.release !== undefined)) return { error: "edge-mode-invalid" };
    const authority = rawEdge.authority ?? "w";
    if (!VALID_AUTHORITIES.has(authority)) return { error: "edge-authority-invalid" };
    const release = mode === "service" ? undefined : (rawEdge.release ?? (mode === "weak" ? "weak" : "deinitOnly"));
    if (mode !== "service" && !VALID_RELEASES.has(release)) return { error: "edge-release-invalid" };
    if (release === "lifecycleDrain" && !nonEmpty(rawEdge.owner)) return { error: "edge-owner-missing" };
    if (release === "explicitClose" && !nonEmpty(rawEdge.owner)) return { error: "edge-owner-missing" };
    edges.set(rawEdge.id, {
      id: rawEdge.id,
      from: rawEdge.from,
      to: rawEdge.to,
      mode,
      release,
      authority,
      owner: rawEdge.owner,
      // Keep direct oracle calls safe even when validation is bypassed.
      known: rawEdge.known !== false && !["foreign", "foreign-hidden"].includes(authority),
      active: rawEdge.active !== false,
    });
  }
  const roots = new Map();
  for (const rawRoot of graph.roots ?? []) {
    const root = typeof rawRoot === "string" ? { id: rawRoot, node: rawRoot } : rawRoot;
    if (!isObject(root) || !nonEmpty(root.id) || roots.has(root.id) || !requireNode({ nodes }, root.node)) return { error: "root-schema-invalid" };
    const authority = root.authority ?? "w";
    if (!VALID_AUTHORITIES.has(authority)) return { error: "root-authority-invalid" };
    roots.set(root.id, {
      id: root.id,
      node: root.node,
      active: root.active !== false,
      known: root.known !== false && !["foreign", "foreign-hidden"].includes(authority),
      authority,
    });
  }
  const registrations = new Map();
  for (const rawRegistration of graph.registrations ?? []) {
    if (!isObject(rawRegistration) || !nonEmpty(rawRegistration.id) || registrations.has(rawRegistration.id) || !requireNode({ nodes }, rawRegistration.node)) return { error: "registration-schema-invalid" };
    registrations.set(rawRegistration.id, {
      id: rawRegistration.id,
      node: rawRegistration.node,
      foreign: rawRegistration.foreign === true,
      inFlight: Number.isSafeInteger(rawRegistration.inFlight) && rawRegistration.inFlight >= 0 ? rawRegistration.inFlight : 0,
      unregistered: rawRegistration.unregistered === true,
      drained: rawRegistration.drained === true,
    });
  }
  const resources = new Map();
  for (const rawResource of graph.resources ?? []) {
    if (!isObject(rawResource) || !nonEmpty(rawResource.id) || resources.has(rawResource.id) || !requireNode({ nodes }, rawResource.node)) return { error: "resource-schema-invalid" };
    resources.set(rawResource.id, {
      id: rawResource.id,
      node: rawResource.node,
      kind: rawResource.kind ?? "resource",
      requiresFinish: rawResource.requiresFinish === true,
      registration: rawResource.registration,
      inFlight: Number.isSafeInteger(rawResource.inFlight) && rawResource.inFlight >= 0 ? rawResource.inFlight : 0,
      drained: rawResource.drained === true,
      finished: rawResource.finished === true,
      // A resource without a foreign registration has no unregister phase.
      unregistered: rawResource.unregistered === true || !nonEmpty(rawResource.registration),
      destroyed: rawResource.destroyed === true,
      unpinned: rawResource.unpinned === true,
      reclaimed: rawResource.reclaimed === true,
    });
  }
  for (const edge of edges.values()) {
    if (["explicitClose", "lifecycleDrain"].includes(edge.release) && !lifecycleOwnerExists({ nodes, registrations, resources, ownerRegistry, ownerRegistryClosed }, edge.owner)) {
      return { error: "edge-owner-unknown" };
    }
  }
  const weakHandles = new Map();
  for (const rawHandle of graph.weakHandles ?? []) {
    if (!isObject(rawHandle) || !nonEmpty(rawHandle.id) || weakHandles.has(rawHandle.id) || !requireNode({ nodes }, rawHandle.target)) return { error: "weak-handle-schema-invalid" };
    weakHandles.set(rawHandle.id, { id: rawHandle.id, target: rawHandle.target, alive: rawHandle.alive !== false });
  }
  const controlBlocks = new Map([...new Set([...weakHandles.values()].map((handle) => handle.target))].map((target) => [target, { freed: false, freeCount: 0 }]));
  if (graph.domain === "crossDomain" && (graph.counterThreadSafe !== true || graph.originsCrossDomain !== true)) return { error: "cross-domain-facts-missing" };
  if (Number.isSafeInteger(graph.censusBudget) && graph.censusBudget < 1) return { error: "census-budget-invalid" };
  return {
    graph,
    nodes,
    edges,
    roots,
    registrations,
    resources,
    ownerRegistry,
    ownerRegistryClosed,
    weakHandles,
    controlBlocks,
    admission: "open",
    admitted: false,
    cancellationRequested: false,
    drainRequested: false,
    drainComplete: false,
    quiescent: false,
    censusRequested: false,
    faulted: false,
    faultCode: undefined,
    locked: false,
    lockOwner: undefined,
    closedOwners: new Set(),
    published: new Set(),
    selfWeakInitialized: new Set(),
    weakReads: [],
    controlBlockFreed: false,
    controlBlockTrace: [],
    addressReuse: false,
    dropOrder: [],
    censusBudget: graph.censusBudget,
  };
}

function validateExpected(actual, expected, caseId) {
  if (!isObject(expected)) return [`${caseId}.expect must be an object.`];
  const errors = [];
  for (const [key, value] of Object.entries(expected)) {
    if (JSON.stringify(actual[key]) !== JSON.stringify(value)) errors.push(`${caseId}.expect.${key} does not match the derived result.`);
  }
  return errors;
}

export function evaluateCyc1Case(testCase) {
  if (!isObject(testCase) || !nonEmpty(testCase.id)) throw new Error("CYC1 case must have an id");
  if (testCase.family === "conditional-liveness") return evaluateConditional(testCase);
  if (testCase.service?.callCycle !== undefined && !VALID_CALL_CYCLES.has(testCase.service.callCycle)) {
    return output(testCase, "invalid", "service-call-cycle-invalid", { mutation: "none" });
  }
  const initial = initialiseState(testCase);
  if (initial.error) return output(testCase, "invalid", initial.error);
  const compile = staticCycleDisposition(initial);
  if (compile.classification === "rejected") {
    return output(testCase, "compile-rejected", compile.code, {
      compileDisposition: compile.classification,
      staticComponents: compile.components,
      closedComponents: compile.closedComponents,
      mutation: "none",
    });
  }
  let state = initial;
  for (const [operation, event] of (testCase.events ?? []).entries()) {
    const result = applyEvent(state, event, operation);
    if (!result.ok) return output(testCase, "invalid", result.code, { operation, compileDisposition: compile.classification, mutation: "none" });
  }
  if (state.faulted) return output(testCase, "fault-boundary", state.faultCode, { compileDisposition: compile.classification, userCleanup: "not-guaranteed", hostRelease: "boundary", mutation: "none" });
  const census = classifyCensus(state);
  const serviceCallDisposition = testCase.service?.callCycle === "metadata"
    ? "call-cycle"
    : testCase.service?.callCycle === "external"
      ? "deadline"
      : undefined;
  const result = output(testCase, census.classification, census.code, {
    compileDisposition: compile.classification,
    staticComponents: compile.components,
    components: census.components,
    residualComponents: census.residualComponents,
    unknownComponents: census.unknownComponents,
    dropOrder: state.dropOrder,
    weakReads: state.weakReads,
    controlBlockFreed: state.controlBlockFreed,
    controlBlocksFreed: controlBlockMap(state),
    controlBlockFreeCount: controlBlockFreeCountMap(state),
    controlBlockTrace: state.controlBlockTrace,
    cancellationRequested: state.cancellationRequested,
    serviceRefsAreOwnership: (testCase.graph.edges ?? []).every((edge) => edge.mode !== "service"),
    serviceCallDisposition,
    loweringRequirement: state.graph.dropDepth > 256 ? "iterative-drop-implementation-required" : undefined,
    loweringStatus: state.graph.dropDepth > 256 ? "inconclusive-concern" : undefined,
    mutation: census.mutation,
  });
  return result;
}

export function deriveCyc1(corpus) {
  return (corpus.cases ?? []).map(evaluateCyc1Case);
}

function validateCaseInput(testCase, root, errors) {
  if (!isObject(testCase) || !nonEmpty(testCase.id)) {
    errors.push("case id must be non-empty");
    return;
  }
  const forbidden = Object.keys(testCase).filter((key) => FORBIDDEN_INPUT_KEYS.has(key));
  if (forbidden.length > 0) errors.push(`${testCase.id} uses caller outcome keys: ${forbidden.join(", ")}`);
  if (!Array.isArray(testCase.events) && testCase.family !== "conditional-liveness") errors.push(`${testCase.id}.events must be an array.`);
  if (testCase.family !== "conditional-liveness" && !isObject(testCase.graph)) errors.push(`${testCase.id}.graph must be an object.`);
  const ownerIds = new Set([
    ...(Array.isArray(testCase.graph?.nodes) ? testCase.graph.nodes : []).map((node) => node?.id).filter(nonEmpty),
    ...(Array.isArray(testCase.graph?.registrations) ? testCase.graph.registrations : []).map((registration) => registration?.id).filter(nonEmpty),
    ...(Array.isArray(testCase.graph?.resources) ? testCase.graph.resources : []).map((resource) => resource?.id).filter(nonEmpty),
    ...(testCase.graph?.ownerRegistryClosed === true && Array.isArray(testCase.graph?.ownerRegistry)
      ? testCase.graph.ownerRegistry.filter(nonEmpty)
      : []),
  ]);
  if (testCase.graph?.ownerRegistry !== undefined && !Array.isArray(testCase.graph.ownerRegistry)) errors.push(`${testCase.id} ownerRegistry must be an array.`);
  for (const edge of testCase.graph?.edges ?? []) {
    if (edge?.release === "lifecycleDrain" && !nonEmpty(edge.owner)) errors.push(`${testCase.id} lifecycleDrain edge must declare an owner.`);
    if (edge?.release === "lifecycleDrain" && nonEmpty(edge.owner) && !ownerIds.has(edge.owner)) errors.push(`${testCase.id} lifecycleDrain edge owner is not a declared node, registration, or resource.`);
    if (edge?.release === "explicitClose" && !nonEmpty(edge.owner)) errors.push(`${testCase.id} explicitClose edge must declare an owner.`);
    if (edge?.release === "explicitClose" && nonEmpty(edge.owner) && !ownerIds.has(edge.owner)) errors.push(`${testCase.id} explicitClose edge owner is not a declared node, registration, or resource.`);
    // A foreign-hidden edge has no adapter metadata.  Marking it known would
    // turn an opaque boundary into a false proof of reclamation.
    if (["foreign", "foreign-hidden"].includes(edge?.authority) && edge.known === true) {
      errors.push(`${testCase.id} foreign edge must remain unknown without adapter metadata.`);
    }
  }
  for (const rootEntry of testCase.graph?.roots ?? []) {
    if (rootEntry?.authority !== undefined && !VALID_AUTHORITIES.has(rootEntry.authority)) errors.push(`${testCase.id} root authority is invalid.`);
    if (["foreign", "foreign-hidden"].includes(rootEntry?.authority) && rootEntry.known === true) errors.push(`${testCase.id} foreign root must remain unknown without adapter metadata.`);
  }
  for (const event of testCase.events ?? []) {
    if (!isObject(event) || !VALID_EVENT_NAMES.has(event.op)) errors.push(`${testCase.id} contains an invalid event.`);
    if (event && Object.keys(event).some((key) => FORBIDDEN_EVENT_KEYS.has(key))) errors.push(`${testCase.id} event uses a caller outcome flag.`);
    if (event?.op === "collect") errors.push(`${testCase.id} cannot request collector side effects.`);
    const eventEdge = event?.edge;
    if (["addStrong", "addWeak"].includes(event?.op) && ["foreign", "foreign-hidden"].includes(eventEdge?.authority) && eventEdge.known === true) {
      errors.push(`${testCase.id} foreign event edge must remain unknown without adapter metadata.`);
    }
    if (["addStrong", "addWeak"].includes(event?.op) && eventEdge?.release === "lifecycleDrain" && !nonEmpty(eventEdge.owner)) errors.push(`${testCase.id} lifecycleDrain event edge must declare an owner.`);
    if (["addStrong", "addWeak"].includes(event?.op) && eventEdge?.release === "lifecycleDrain" && nonEmpty(eventEdge.owner) && !ownerIds.has(eventEdge.owner)) errors.push(`${testCase.id} lifecycleDrain event edge owner is not declared.`);
    if (event?.op === "rootAdd" && event.authority !== undefined && !VALID_AUTHORITIES.has(event.authority)) errors.push(`${testCase.id} root event authority is invalid.`);
    if (event?.op === "rootAdd" && ["foreign", "foreign-hidden"].includes(event.authority) && event.known === true) errors.push(`${testCase.id} foreign event root must remain unknown without adapter metadata.`);
    if (["close", "unlink"].includes(event?.op) && !nonEmpty(event.owner)) errors.push(`${testCase.id} close/unlink event must declare an owner.`);
    if (["close", "unlink"].includes(event?.op)) {
      const edgeIds = Array.isArray(event.edges) ? event.edges : [event.edge];
      for (const edgeId of edgeIds) {
        const graphEdge = (testCase.graph?.edges ?? []).find((edge) => edge?.id === edgeId);
        if (graphEdge?.release === "explicitClose" && nonEmpty(event.owner) && graphEdge.owner !== event.owner) {
          errors.push(`${testCase.id} close/unlink owner must match explicitClose edge owner.`);
        }
      }
    }
    if (event?.op === "removeEdge" && !nonEmpty(event.owner)) errors.push(`${testCase.id} removeEdge event must declare an owner.`);
    if (event?.op === "removeEdge" && nonEmpty(event.owner)) {
      const graphEdge = (testCase.graph?.edges ?? []).find((edge) => edge?.id === event.edge);
      if (graphEdge?.release === "explicitClose" && graphEdge.owner !== event.owner) errors.push(`${testCase.id} removeEdge owner must match explicitClose edge owner.`);
    }
  }
  if (testCase.service?.callCycle !== undefined && !VALID_CALL_CYCLES.has(testCase.service.callCycle)) errors.push(`${testCase.id} service.callCycle must be metadata or external.`);
  const sourceKeys = new Set();
  if (testCase.sourceRefs !== undefined && !Array.isArray(testCase.sourceRefs)) errors.push(`${testCase.id}.sourceRefs must be an array.`);
  for (const ref of testCase.sourceRefs ?? []) {
    if (!isObject(ref) || !nonEmpty(ref.path) || !nonEmpty(ref.symbol)) {
      errors.push(`${testCase.id} source reference schema is invalid.`);
      continue;
    }
    const key = `${ref.path}\0${ref.symbol}`;
    if (sourceKeys.has(key)) errors.push(`${testCase.id} duplicates source reference ${key}`);
    sourceKeys.add(key);
    const file = path.resolve(root, ref.path);
    if (!fs.existsSync(file)) errors.push(`${testCase.id} source reference is missing: ${ref.path}`);
    else {
      if (!fs.readFileSync(file, "utf8").includes(ref.symbol)) errors.push(`${testCase.id} source symbol is absent: ${ref.symbol}`);
      if (ref.digest && digestFor(file) !== ref.digest) errors.push(`${testCase.id} source reference digest is stale: ${ref.path}`);
    }
  }
}

function digestFor(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

export function validateCyc1(corpus, { root } = {}) {
  const errors = [];
  if (corpus?.$schema !== "w-cyc1-explicit-cycle-cases-1") errors.push("CYC1 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("CYC1 corpus status must be design-oracle-input.");
  if (!Array.isArray(corpus?.cases) || corpus.cases.length < REQUIRED_CASES.length) {
    errors.push(`CYC1 corpus must contain at least ${REQUIRED_CASES.length} cases.`);
    return { errors, results: [] };
  }
  const ids = new Set();
  for (const testCase of corpus.cases) {
    if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}`);
    ids.add(testCase.id);
    validateCaseInput(testCase, root ?? process.cwd(), errors);
  }
  for (const id of REQUIRED_CASES) if (!ids.has(id)) errors.push(`CYC1 required case missing: ${id}`);
  const results = deriveCyc1(corpus);
  const resultById = new Map(results.map((item) => [item.caseId, item]));
  if (results.some((item) => ["research", "research-candidate", "candidate-research"].includes(item.status))) errors.push("CYC1 must not retain an active research status.");
  for (const testCase of corpus.cases) errors.push(...validateExpected(resultById.get(testCase.id), testCase.expect, testCase.id));
  const strongRejected = resultById.get("CYC1-NEG-strong-callback-scc");
  if (strongRejected?.code !== "W-OWNERSHIP-0014") errors.push("strong callback SCC must derive W-OWNERSHIP-0014.");
  const residual = resultById.get("CYC1-NEG-residual-after-drain");
  if (residual?.code !== "W-MEMORY-0001") errors.push("drained residual cycle must derive W-MEMORY-0001.");
  const hidden = resultById.get("CYC1-UNK-hidden-foreign-root");
  if (hidden?.status !== "unknown") errors.push("hidden foreign roots must remain unknown.");
  const collector = resultById.get("CYC1-REJECT-transparent-collector");
  if (collector?.status !== "intentionally-rejected") errors.push("transparent collector must remain rejected.");
  return { errors, results };
}

export { REQUIRED_CASES };
