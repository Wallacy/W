import crypto from "node:crypto";
import fs from "node:fs";

/*
 * HRD0 is a host design oracle. It models a development runner around normal
 * W units. It does not implement a compiler, runtime, provider, or CLI.
 * Outcome fields are derived from facts and events; `expect` is a mutation
 * guard only.
 */

export const LOGICAL_CLEANUP_STEPS = [
  "cancelChildren",
  "drainWaits",
  "drainLoans",
  "drainStreams",
  "drainCallbacks",
  "drainResources",
  "unregister",
  "inFlightDrain",
  "destroy",
  "release",
];

/* Kept as an alias for callers that only need the logical contract. */
export const CLEANUP_STEPS = LOGICAL_CLEANUP_STEPS;

export const REJECTED_MECHANISMS = Object.freeze({
  eval: "arbitrary-evaluation",
  exec: "arbitrary-evaluation",
  monkeyPatch: "active-frame-mutation",
  activeFrameWrite: "active-frame-mutation",
  debuggerWrite: "active-frame-mutation",
  ambientLookup: "ambient-authority",
  nativeInProcessSandbox: "native-isolation",
  dlcloseLiveCallback: "ffi-lifetime",
  currentModuleInjection: "module-injection",
});

export const REQUIRED_CASES = [
  "HRD0-A-normal-unit-reopen",
  "HRD0-A-invalid-unit-preserves-old",
  "HRD0-A-prepublication-rollback",
  "HRD0-A-postpublication-drain-degraded",
  "HRD0-A-stale-completion-rejected",
  "HRD0-A-oom-preflight-rejected",
  "HRD0-A-cancel-before-publication",
  "HRD0-B-typed-service-generation",
  "HRD0-B-local-split-equivalence",
  "HRD0-B-schema-wabi-rejected",
  "HRD0-B-untrusted-native-rejected",
  "HRD0-C-generated-module-reopen-research",
  "HRD0-C-generated-module-injection-rejected",
  "HRD0-C-invocation-spelling-unresolved",
  "HRD0-D-production-reload-rejected",
  "HRD0-D-active-frame-rejected",
  "HRD0-D-eval-rejected",
  "HRD0-D-dlclose-live-callback-rejected",
  "HRD0-D-live-state-migration-rejected",
  "HRD0-D-crash-before-publication-unknown",
];

const FORBIDDEN_FACT_KEYS = new Set([
  "status",
  "route",
  "disposition",
  "outcome",
  "published",
  "rolledBack",
  "activeGeneration",
  "authority",
]);

const ALLOWED_TOP_LEVEL_KEYS = new Set(["id", "axis", "family", "problem", "facts", "events", "projections", "expect"]);
const ALLOWED_MUTATION_KEYS = new Set(["id", "baseCase", "target", "steps", "override", "expectedCode"]);
const ALLOWED_RESOURCES = new Set(["ffi", "pin", "mapping"]);
const ALLOWED_EVENT_OBJECT_KEYS = new Set([
  "op",
  "code",
  "kind",
  "generation",
  "position",
  "receipt",
  "steps",
  "mechanism",
  "payload",
  "reason",
]);
const PHASES = ["prepare", "validate", "preflight", "ready", "switch", "closeAdmission", "drain", "cleanup"];
const SOURCE_KINDS = new Set(["normal-unit", "typed-service-plugin", "generated-module-set"]);
const ISOLATIONS = new Set(["process", "wasm", "component", "native"]);
const IDENTITY_KEYS = ["packageIdentity", "recipeKey", "artifactKey", "sourceMapKey", "semanticInterfaceKey", "serviceIRKey", "wAbiKey", "runtimeClosureKey", "schemaDigest"];
const COMMON_NOMINAL_CONTRACT = "hot_reload_dev_contract::ReloadInput+ReloadResult";
const EVENT_NAMES = new Set([
  ...PHASES,
  "actionResult",
  "reopenUnit",
  "semanticCheck",
  "parseFailure",
  "schemaMismatch",
  "wabiMismatch",
  "capabilityMismatch",
  "effectMismatch",
  "oom",
  "cancel",
  "providerRollback",
  "drainFailure",
  "staleCompletion",
  "staleMessage",
  "staleCapability",
  "crash",
  "invocation",
  "rejectMechanism",
  "migrate",
  "discardCandidate",
]);

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function deepMerge(base, override) {
  if (!isObject(base)) return structuredClone(override);
  const result = structuredClone(base);
  if (!isObject(override)) return result;
  for (const [key, value] of Object.entries(override)) {
    result[key] = isObject(value) && isObject(result[key]) ? deepMerge(result[key], value) : structuredClone(value);
  }
  return result;
}

function canonical(value) {
  if (Array.isArray(value)) return `[${value.map(canonical).join(",")}]`;
  if (isObject(value)) return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonical(value[key])}`).join(",")}}`;
  return JSON.stringify(value);
}

export function digestValue(value) {
  return `sha256:${crypto.createHash("sha256").update(canonical(value)).digest("hex")}`;
}

function validDigest(value) {
  return typeof value === "string" && /^sha256:[0-9a-f]{64}$/u.test(value);
}

function eventOperation(event) {
  return typeof event === "string" ? event : event?.op;
}

function eventValue(event, key, fallback = undefined) {
  return typeof event === "string" ? fallback : event?.[key] ?? fallback;
}

function walkForbidden(value, location, errors) {
  if (Array.isArray(value)) {
    value.forEach((item, index) => walkForbidden(item, `${location}[${index}]`, errors));
    return;
  }
  if (!isObject(value)) return;
  for (const [key, child] of Object.entries(value)) {
    if (FORBIDDEN_FACT_KEYS.has(key)) errors.push(`${location}.${key} is a derived fact and is not accepted as input.`);
    walkForbidden(child, `${location}.${key}`, errors);
  }
}

function mergedFacts(corpus, testCase) {
  return deepMerge(corpus.defaults, testCase.facts ?? {});
}

function routeForFacts(facts, invocation = false) {
  if (facts.runner.mode !== "development" || facts.runner.releaseDynamicMode || facts.runner.untrustedNative || (facts.runner.isolation === "native" && facts.runner.untrusted)) return "intentionally-rejected";
  if (REJECTED_MECHANISMS[facts.runner.mechanism]) return "intentionally-rejected";
  if (invocation || facts.runner.sourceKind === "generated-module-set") return "research";
  return "current-composition";
}

function baseResult(facts) {
  return {
    status: "pending",
    route: routeForFacts(facts),
    disposition: routeForFacts(facts),
    code: null,
    generation: facts.generations.old,
    staleRejections: [],
    cleanupOrder: [],
    logicalCleanupOrder: [],
    nativeMappingRetained: false,
    phaseTrace: [],
    physicalTrace: [],
    rollback: false,
    published: false,
    oldAdmissionClosed: false,
    drainAttempted: false,
    candidateDiscarded: false,
    languageSurface: facts.runner.languageSurface,
    sourceKind: facts.runner.sourceKind,
    invocation: facts.runner.invocation,
    generatedReopened: false,
    generatedChecked: false,
  };
}

function terminal(result, status, code, generation = result.generation) {
  result.status = status;
  result.code = code;
  result.generation = generation;
  return result;
}

function rejectInputRunner(result, facts) {
  if (facts.runner.languageSurface !== "none") return terminal(result, "intentionally-rejected", "language-surface-introduced", facts.generations.old);
  if (facts.runner.mode !== "development" || facts.runner.releaseDynamicMode) return terminal(result, "intentionally-rejected", "production-dynamic-mode-forbidden", facts.generations.old);
  if (facts.runner.profile !== "existing") return terminal(result, "intentionally-rejected", "profile-not-selected", facts.generations.old);
  if (facts.runner.untrustedNative || (facts.runner.isolation === "native" && facts.runner.untrusted)) return terminal(result, "intentionally-rejected", "native-not-sandbox", facts.generations.old);
  if (facts.runner.mechanism && REJECTED_MECHANISMS[facts.runner.mechanism]) return terminal(result, "intentionally-rejected", `mechanism-rejected:${facts.runner.mechanism}`, facts.generations.old);
  return null;
}

function cleanupPlan(facts) {
  const resources = new Set(facts.resources ?? []);
  const physical = [...LOGICAL_CLEANUP_STEPS];
  /* A pin (including a foreign-resource pin) must be declared before unpin. */
  if (resources.has("pin") || resources.has("ffi")) physical.splice(physical.indexOf("release"), 0, "unpin");
  if (resources.has("mapping") && facts.runner.isolation !== "native") physical.push("unmap");
  return {
    logical: [...LOGICAL_CLEANUP_STEPS],
    physical,
    nativeMappingRetained: resources.has("mapping") && facts.runner.isolation === "native",
  };
}

function cleanupEventReplacement(events, steps) {
  return (events ?? []).map((event) => eventOperation(event) === "cleanup" ? { ...event, steps: structuredClone(steps) } : structuredClone(event));
}

export function evaluateHotReloadMutation(mutation, { corpus }) {
  const base = corpus.cases.find((testCase) => testCase.id === mutation?.baseCase);
  if (!base) return { status: "rejected", code: "mutation-base-missing", mutationId: mutation?.id ?? null };
  const mutated = structuredClone(base);
  if (mutation.target === "cleanup" && mutated.projections) {
    mutated.projections.local = cleanupEventReplacement(mutated.projections.local, mutation.steps);
    mutated.projections.split = cleanupEventReplacement(mutated.projections.split, mutation.steps);
  } else if (mutation.target === "cleanup") {
    mutated.events = cleanupEventReplacement(mutated.events, mutation.steps);
  } else if (mutation.target === "contract") {
    mutated.facts = deepMerge(mutated.facts ?? {}, mutation.override ?? {});
  }
  return { ...evaluateHotReloadCase(mutated, { corpus }), mutationId: mutation.id };
}

function projectionContractError(testCase, facts) {
  if (!testCase.projections) return null;
  const contract = facts.contract;
  if (!isObject(contract)) return "projection-contract-missing";
  if (contract.localNominal !== COMMON_NOMINAL_CONTRACT || contract.splitNominal !== COMMON_NOMINAL_CONTRACT) return "nominal-contract-divergence";
  if (!validDigest(contract.localInterfaceDigest) || !validDigest(contract.splitInterfaceDigest)) return "interface-digest-invalid";
  if (contract.localInterfaceDigest !== contract.splitInterfaceDigest || contract.localInterfaceDigest !== facts.identities.semanticInterfaceKey) return "interface-digest-divergence";
  return null;
}

function cleanupError(steps, facts) {
  if (!Array.isArray(steps)) return "cleanup-missing-step";
  const expected = cleanupPlan(facts).physical;
  const expectedSet = new Set(expected);
  const actualSet = new Set(steps);
  if (steps.some((step) => !expectedSet.has(step))) return "cleanup-unneeded-step";
  if (expected.some((step) => !actualSet.has(step))) return "cleanup-missing-step";
  if (JSON.stringify(steps) !== JSON.stringify(expected)) return "cleanup-order";
  return null;
}

function generatedReady(result, facts, seen) {
  if (facts.runner.sourceKind !== "generated-module-set") return true;
  if (!seen.has("actionResult") || !seen.has("reopenUnit") || !seen.has("semanticCheck")) {
    terminal(result, "rejected", "generated-unit-not-reopened", facts.generations.old);
    result.route = "research";
    result.disposition = "research";
    return false;
  }
  result.generatedReopened = true;
  result.generatedChecked = true;
  return true;
}

function finish(result, facts, seen) {
  if (result.status !== "pending") return result;
  if (result.invocation === "tooling-owned-unselected" && seen.has("invocation")) {
    result.route = "research";
    result.disposition = "research";
    return terminal(result, "research", "invocation-not-selected", facts.generations.old);
  }
  if (!result.published) return terminal(result, "rejected", "publication-not-reached", facts.generations.old);
  if (!result.oldAdmissionClosed) return terminal(result, "rejected", "admission-not-closed", facts.generations.old);
  if (!result.drainAttempted) return terminal(result, "rejected", "old-generation-not-drained", facts.generations.old);
  if (!result.candidateDiscarded && result.cleanupOrder.length === 0) return terminal(result, "rejected", "cleanup-not-recorded", facts.generations.old);
  if (!generatedReady(result, facts, seen)) return result;
  if (result.status === "pending") {
    if (facts.runner.sourceKind === "generated-module-set") {
      result.route = "research";
      result.disposition = "research";
      result.status = "research";
      result.code = "generated-module-research";
    } else {
      result.status = "committed";
      result.code = "dev-runner-switch-committed";
    }
  }
  return result;
}

/* First independent reducer: local in-process runner projection. */
export function reduceLocal(testCase, corpus) {
  const facts = mergedFacts(corpus, testCase);
  const result = baseResult(facts);
  const rejected = rejectInputRunner(result, facts);
  if (rejected) return rejected;
  const seen = new Set();
  let phaseIndex = -1;
  for (const event of testCase.events ?? []) {
    const op = eventOperation(event);
    result.physicalTrace.push(`local:${op}`);
    if (!EVENT_NAMES.has(op)) return terminal(result, "rejected", "unknown-event", facts.generations.old);
    if (op === "actionResult" || op === "reopenUnit" || op === "semanticCheck" || op === "invocation") {
      seen.add(op);
      continue;
    }
    if (op === "rejectMechanism") {
      const mechanism = eventValue(event, "mechanism");
      if (!REJECTED_MECHANISMS[mechanism]) return terminal(result, "rejected", "unknown-mechanism", facts.generations.old);
      result.route = "intentionally-rejected";
      result.disposition = "intentionally-rejected";
      return terminal(result, "intentionally-rejected", `mechanism-rejected:${mechanism}`, facts.generations.old);
    }
    if (op === "migrate") {
      result.route = "intentionally-rejected";
      result.disposition = "intentionally-rejected";
      return terminal(result, "intentionally-rejected", "live-state-migration-forbidden", facts.generations.old);
    }
    if (["parseFailure", "schemaMismatch", "wabiMismatch", "capabilityMismatch", "effectMismatch", "oom", "cancel"].includes(op)) {
      result.candidateDiscarded = true;
      return terminal(result, "rejected", op === "cancel" ? "cancelled-before-publication" : `prepublication-${op}`, facts.generations.old);
    }
    if (op === "providerRollback") {
      if (result.published) return terminal(result, "rejected", "rollback-after-publication", facts.generations.next);
      if (phaseIndex < 2 || eventValue(event, "receipt") !== "valid") return terminal(result, "rejected", "rollback-receipt-invalid", facts.generations.old);
      result.rollback = true;
      result.candidateDiscarded = true;
      return terminal(result, "rolled-back", "provider-receipt-rollback", facts.generations.old);
    }
    if (op === "crash") {
      const position = eventValue(event, "position");
      const receipt = eventValue(event, "receipt", "unknown");
      if (position === "before-publication") return terminal(result, receipt === "unknown" ? "unknown-effect" : "fault-boundary", receipt === "unknown" ? "unknown-provider-effect" : "prepublication-crash", facts.generations.old);
      if (position === "after-publication") return terminal(result, receipt === "committed" ? "fault-boundary" : "unknown-effect", receipt === "committed" ? "post-publication-crash" : "unknown-post-publication-effect", facts.generations.next);
      return terminal(result, "rejected", "crash-position-invalid", facts.generations.old);
    }
    if (["staleCompletion", "staleMessage", "staleCapability"].includes(op)) {
      if (!result.published || eventValue(event, "generation") !== facts.generations.old) return terminal(result, "rejected", "stale-event-before-switch", result.generation);
      result.staleRejections.push(op);
      seen.add(op);
      continue;
    }
    if (op === "drainFailure") {
      if (!result.published || !result.oldAdmissionClosed) return terminal(result, "rejected", "drain-before-admission-close", result.generation);
      result.drainAttempted = true;
      result.status = "degraded";
      result.code = "post-publication-drain-failure";
      result.postSwitchDrainFailure = eventValue(event, "reason", "unknown");
      seen.add(op);
      continue;
    }
    if (op === "discardCandidate") {
      result.candidateDiscarded = true;
      seen.add(op);
      continue;
    }
    if (op === "closeAdmission") {
      if (!result.published) return terminal(result, "rejected", "close-before-publication", result.generation);
      result.oldAdmissionClosed = true;
      phaseIndex = Math.max(phaseIndex, 5);
      seen.add(op);
      continue;
    }
    if (op === "drain") {
      if (!result.oldAdmissionClosed) return terminal(result, "rejected", "drain-before-admission-close", result.generation);
      result.drainAttempted = true;
      phaseIndex = Math.max(phaseIndex, 6);
      seen.add(op);
      continue;
    }
    if (op === "cleanup") {
      if (!result.drainAttempted) return terminal(result, "rejected", "cleanup-before-drain", result.generation);
      const steps = eventValue(event, "steps", []);
      const cleanupFailure = cleanupError(steps, facts);
      if (cleanupFailure) return terminal(result, "rejected", cleanupFailure, result.generation);
      result.cleanupOrder = [...steps];
      result.logicalCleanupOrder = [...LOGICAL_CLEANUP_STEPS];
      result.nativeMappingRetained = cleanupPlan(facts).nativeMappingRetained;
      phaseIndex = Math.max(phaseIndex, 7);
      seen.add(op);
      continue;
    }
    const next = PHASES.indexOf(op);
    if (next < 0) continue;
    if (op === "switch") {
      if (phaseIndex < PHASES.indexOf("ready") || result.published) return terminal(result, "rejected", "phase-order", result.generation);
      result.published = true;
      result.generation = facts.generations.next;
      phaseIndex = next;
      seen.add(op);
      continue;
    }
    if (next !== phaseIndex + 1 && !(op === "prepare" && phaseIndex === -1)) return terminal(result, "rejected", "phase-order", result.generation);
    phaseIndex = next;
    result.phaseTrace.push(op);
    seen.add(op);
  }
  return finish(result, facts, seen);
}

/* Second independent reducer: split process/component projection. */
export function reduceSplit(testCase, corpus) {
  const facts = mergedFacts(corpus, testCase);
  const result = baseResult(facts);
  const rejected = rejectInputRunner(result, facts);
  if (rejected) return rejected;
  const seen = new Set();
  let phase = "none";
  const transition = { none: "prepare", prepare: "validate", validate: "preflight", preflight: "ready", ready: "switch", switch: "closeAdmission", closeAdmission: "drain", drain: "cleanup" };
  for (const event of testCase.events ?? []) {
    const op = eventOperation(event);
    result.physicalTrace.push(`split:${op}`);
    if (!EVENT_NAMES.has(op)) return terminal(result, "rejected", "unknown-event", facts.generations.old);
    if (["actionResult", "reopenUnit", "semanticCheck", "invocation"].includes(op)) {
      seen.add(op);
      continue;
    }
    if (op === "rejectMechanism") {
      const mechanism = eventValue(event, "mechanism");
      if (!REJECTED_MECHANISMS[mechanism]) return terminal(result, "rejected", "unknown-mechanism", facts.generations.old);
      result.route = "intentionally-rejected";
      result.disposition = "intentionally-rejected";
      return terminal(result, "intentionally-rejected", `mechanism-rejected:${mechanism}`, facts.generations.old);
    }
    if (op === "migrate") {
      result.route = "intentionally-rejected";
      result.disposition = "intentionally-rejected";
      return terminal(result, "intentionally-rejected", "live-state-migration-forbidden", facts.generations.old);
    }
    if (["parseFailure", "schemaMismatch", "wabiMismatch", "capabilityMismatch", "effectMismatch", "oom", "cancel"].includes(op)) {
      result.candidateDiscarded = true;
      return terminal(result, "rejected", op === "cancel" ? "cancelled-before-publication" : `prepublication-${op}`, facts.generations.old);
    }
    if (op === "providerRollback") {
      if (result.published) return terminal(result, "rejected", "rollback-after-publication", facts.generations.next);
      if (!["preflight", "ready"].includes(phase) || eventValue(event, "receipt") !== "valid") return terminal(result, "rejected", "rollback-receipt-invalid", facts.generations.old);
      result.rollback = true;
      result.candidateDiscarded = true;
      return terminal(result, "rolled-back", "provider-receipt-rollback", facts.generations.old);
    }
    if (op === "crash") {
      const position = eventValue(event, "position");
      const receipt = eventValue(event, "receipt", "unknown");
      if (position === "before-publication") return terminal(result, receipt === "unknown" ? "unknown-effect" : "fault-boundary", receipt === "unknown" ? "unknown-provider-effect" : "prepublication-crash", facts.generations.old);
      if (position === "after-publication") return terminal(result, receipt === "committed" ? "fault-boundary" : "unknown-effect", receipt === "committed" ? "post-publication-crash" : "unknown-post-publication-effect", facts.generations.next);
      return terminal(result, "rejected", "crash-position-invalid", facts.generations.old);
    }
    if (["staleCompletion", "staleMessage", "staleCapability"].includes(op)) {
      if (phase !== "switch" && phase !== "closeAdmission" && phase !== "drain" && phase !== "cleanup") return terminal(result, "rejected", "stale-event-before-switch", result.generation);
      if (eventValue(event, "generation") !== facts.generations.old) return terminal(result, "rejected", "stale-event-generation-mismatch", result.generation);
      result.staleRejections.push(op);
      seen.add(op);
      continue;
    }
    if (op === "drainFailure") {
      if (!result.published || !result.oldAdmissionClosed) return terminal(result, "rejected", "drain-before-admission-close", result.generation);
      result.drainAttempted = true;
      result.status = "degraded";
      result.code = "post-publication-drain-failure";
      result.postSwitchDrainFailure = eventValue(event, "reason", "unknown");
      seen.add(op);
      continue;
    }
    if (op === "discardCandidate") {
      result.candidateDiscarded = true;
      seen.add(op);
      continue;
    }
    if (op === "closeAdmission") {
      if (!result.published || phase !== "switch") return terminal(result, "rejected", "close-before-publication", result.generation);
      result.oldAdmissionClosed = true;
      phase = "closeAdmission";
      seen.add(op);
      continue;
    }
    if (op === "drain") {
      if (!result.oldAdmissionClosed || phase !== "closeAdmission") return terminal(result, "rejected", "drain-before-admission-close", result.generation);
      result.drainAttempted = true;
      phase = "drain";
      seen.add(op);
      continue;
    }
    if (op === "cleanup") {
      if (!result.drainAttempted || phase !== "drain") return terminal(result, "rejected", "cleanup-before-drain", result.generation);
      const steps = eventValue(event, "steps", []);
      const cleanupFailure = cleanupError(steps, facts);
      if (cleanupFailure) return terminal(result, "rejected", cleanupFailure, result.generation);
      result.cleanupOrder = [...steps];
      result.logicalCleanupOrder = [...LOGICAL_CLEANUP_STEPS];
      result.nativeMappingRetained = cleanupPlan(facts).nativeMappingRetained;
      phase = "cleanup";
      seen.add(op);
      continue;
    }
    if (op === "switch") {
      if (phase !== "ready" || result.published) return terminal(result, "rejected", "phase-order", result.generation);
      result.published = true;
      result.generation = facts.generations.next;
      phase = "switch";
      seen.add(op);
      continue;
    }
    if (!["prepare", "validate", "preflight", "ready"].includes(op)) continue;
    if (transition[phase] !== op) return terminal(result, "rejected", "phase-order", result.generation);
    phase = op;
    result.phaseTrace.push(op);
    seen.add(op);
  }
  return finish(result, facts, seen);
}

function logicalSignature(result) {
  return JSON.stringify({
    status: result.status,
    route: result.route,
    disposition: result.disposition,
    code: result.code,
    generation: result.generation,
    staleRejections: result.staleRejections,
    cleanupOrder: result.logicalCleanupOrder,
    rollback: result.rollback,
    postSwitchDrainFailure: result.postSwitchDrainFailure ?? null,
    languageSurface: result.languageSurface,
    sourceKind: result.sourceKind,
  });
}

export function evaluateHotReloadCase(testCase, { corpus }) {
  const facts = mergedFacts(corpus, testCase);
  const projections = testCase.projections;
  const contractFailure = projectionContractError(testCase, facts);
  if (contractFailure) {
    const result = baseResult(facts);
    return { ...terminal(result, "rejected", contractFailure, facts.generations.old), route: "intentionally-rejected", disposition: "intentionally-rejected", mode: "paired" };
  }
  if (projections) {
    const local = reduceLocal({ ...testCase, events: projections.local, projections: undefined }, corpus);
    const split = reduceSplit({ ...testCase, events: projections.split, projections: undefined }, corpus);
    if (logicalSignature(local) !== logicalSignature(split)) {
      return {
        ...local,
        status: "rejected",
        route: "intentionally-rejected",
        disposition: "intentionally-rejected",
        code: "projection-divergence",
        mode: "paired",
        projections: { local, split },
      };
    }
    return { ...local, mode: "paired", projections: { local, split } };
  }
  return reduceLocal(testCase, corpus);
}

function validateEvent(event, location, errors, facts) {
  const op = eventOperation(event);
  if (!EVENT_NAMES.has(op)) errors.push(`${location} uses unknown operation ${op}.`);
  if (!isObject(event)) return;
  for (const key of Object.keys(event)) if (!ALLOWED_EVENT_OBJECT_KEYS.has(key)) errors.push(`${location}.${key} is not in the closed event schema.`);
  const cleanupFailure = op === "cleanup" ? cleanupError(event.steps, facts) : null;
  if (cleanupFailure) errors.push(`${location}.steps violates canonical cleanup order/resource plan (${cleanupFailure}).`);
  if (op === "rejectMechanism" && !REJECTED_MECHANISMS[event.mechanism]) errors.push(`${location}.mechanism is not a rejected HRD0 mechanism.`);
  if (["staleCompletion", "staleMessage", "staleCapability"].includes(op) && event.generation !== "g1") errors.push(`${location}.generation must identify the old generation g1.`);
}

export function validateHotReload(corpus, { root } = {}) {
  const errors = [];
  if (corpus?.$schema !== "w-hrd0-hot-reload-dev-cases-1") errors.push("HRD0 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("HRD0 corpus status must be design-oracle-input.");
  if (!isObject(corpus?.defaults) || !isObject(corpus.defaults.runner) || !isObject(corpus.defaults.generations)) errors.push("HRD0 defaults must include runner and generations facts.");
  const runner = corpus.defaults?.runner ?? {};
  if (runner.mode !== "development" || runner.profile !== "existing" || runner.languageSurface !== "none" || runner.releaseDynamicMode !== false) errors.push("HRD0 defaults must be development-only with no language surface or release dynamic mode.");
  if (!SOURCE_KINDS.has(runner.sourceKind)) errors.push("HRD0 default sourceKind is invalid.");
  if (!ISOLATIONS.has(runner.isolation)) errors.push("HRD0 default isolation is invalid.");
  if (!Array.isArray(corpus.defaults?.resources)) errors.push("HRD0 defaults.resources must declare resource-specific cleanup facts.");
  for (const resource of corpus.defaults?.resources ?? []) if (!ALLOWED_RESOURCES.has(resource)) errors.push(`HRD0 default resource is invalid: ${resource}.`);
  if (corpus.defaults?.contract?.schemaPolicy !== "exact" || corpus.defaults?.contract?.effectMode !== "declared" || corpus.defaults?.contract?.capabilityMode !== "attenuated-generation-exact" || corpus.defaults?.contract?.sourceMapMode !== "bounded-byte" || corpus.defaults?.contract?.identityMode !== "digest-exact") errors.push("HRD0 defaults must keep exact interface/schema/effect/capability/source-map identity facts.");
  for (const key of IDENTITY_KEYS) if (!validDigest(corpus.defaults?.identities?.[key])) errors.push(`HRD0 identity ${key} must use a digest.`);
  if (!Array.isArray(corpus.cases) || corpus.cases.length < REQUIRED_CASES.length) errors.push(`HRD0 requires at least ${REQUIRED_CASES.length} cases.`);
  const ids = new Set();
  for (const [index, testCase] of (corpus.cases ?? []).entries()) {
    const location = `cases[${index}]`;
    if (typeof testCase?.id !== "string" || testCase.id.trim() === "") errors.push(`${location}.id is required.`);
    if (ids.has(testCase.id)) errors.push(`${location}.id duplicates ${testCase.id}.`);
    ids.add(testCase.id);
    for (const key of Object.keys(testCase ?? {})) if (!ALLOWED_TOP_LEVEL_KEYS.has(key)) errors.push(`${location}.${key} is not in the closed case schema.`);
    if (!SOURCE_KINDS.has(testCase?.facts?.runner?.sourceKind ?? runner.sourceKind)) errors.push(`${location} has an invalid sourceKind.`);
    walkForbidden(testCase?.facts ?? {}, `${location}.facts`, errors);
    if (!Array.isArray(testCase?.events) && !isObject(testCase?.projections)) errors.push(`${location} requires events or projections.`);
    const facts = mergedFacts(corpus, testCase);
    if (!Array.isArray(facts.resources)) errors.push(`${location}.resources must declare logical/resource cleanup facts.`);
    for (const resource of facts.resources ?? []) if (!ALLOWED_RESOURCES.has(resource)) errors.push(`${location}.resources contains an invalid resource: ${resource}.`);
    const contractFailure = projectionContractError(testCase, facts);
    if (contractFailure) errors.push(`${location}.contract violates the shared nominal/interface contract (${contractFailure}).`);
    for (const [eventIndex, event] of (testCase.events ?? []).entries()) validateEvent(event, `${location}.events[${eventIndex}]`, errors, facts);
    if (testCase.projections) {
      for (const projection of ["local", "split"]) {
        if (!Array.isArray(testCase.projections[projection])) errors.push(`${location}.projections.${projection} must be an event array.`);
        for (const [eventIndex, event] of (testCase.projections[projection] ?? []).entries()) validateEvent(event, `${location}.projections.${projection}[${eventIndex}]`, errors, facts);
      }
    }
  }
  for (const id of REQUIRED_CASES) if (!ids.has(id)) errors.push(`HRD0 required case missing: ${id}`);
  const mutationIds = new Set();
  if (!Array.isArray(corpus.adversarialMutations) || corpus.adversarialMutations.length < 5) errors.push("HRD0 requires cleanup, duplicate-nominal, and interface-digest adversarial mutations.");
  for (const [index, mutation] of (corpus.adversarialMutations ?? []).entries()) {
    const location = `adversarialMutations[${index}]`;
    if (typeof mutation?.id !== "string" || mutation.id.trim() === "") errors.push(`${location}.id is required.`);
    if (mutationIds.has(mutation?.id)) errors.push(`${location}.id duplicates ${mutation.id}.`);
    mutationIds.add(mutation?.id);
    for (const key of Object.keys(mutation ?? {})) if (!ALLOWED_MUTATION_KEYS.has(key)) errors.push(`${location}.${key} is not in the closed mutation schema.`);
    if (!ids.has(mutation?.baseCase)) errors.push(`${location}.baseCase is not a corpus case.`);
    const base = corpus.cases.find((testCase) => testCase.id === mutation?.baseCase);
    if (mutation?.target === "cleanup" && !Array.isArray(mutation?.steps)) errors.push(`${location}.steps must be an event array.`);
    if (mutation?.target === "contract" && !isObject(mutation?.override)) errors.push(`${location}.override must declare the contract mutation.`);
    if (!new Set(["cleanup", "contract"]).has(mutation?.target)) errors.push(`${location}.target must be cleanup or contract.`);
    if (base && mutation?.target === "cleanup" && Array.isArray(mutation?.steps)) {
      const failure = cleanupError(mutation.steps, mergedFacts(corpus, base));
      if (failure !== mutation.expectedCode) errors.push(`${location}.expectedCode must equal ${failure ?? "no-error"}.`);
    }
    if (base && mutation?.target === "contract" && isObject(mutation?.override)) {
      const mutated = structuredClone(base);
      mutated.facts = deepMerge(mutated.facts ?? {}, mutation.override);
      const failure = projectionContractError(mutated, mergedFacts(corpus, mutated));
      if (failure !== mutation.expectedCode) errors.push(`${location}.expectedCode must equal ${failure ?? "no-error"}.`);
    }
  }
  return { errors, results: (corpus.cases ?? []).map((testCase) => evaluateHotReloadCase(testCase, { corpus })) };
}

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}
