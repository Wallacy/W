import crypto from "node:crypto";

const BORROW_MODES = new Set(["ref", "view", "mut ref"]);
const DEPENDENT_MODES = new Set(["ref", "view", "mut ref"]);
const RELATION_OWNERS = new Set(["requirement", "interface"]);
const AMBIGUOUS_BODYLESS_RESULT = "W-BORROW-0011";
const DECLARATION_KINDS = new Set(["free", "static", "protocol", "instance", "member", "init"]);
const VERIFICATION_SCOPES = new Set(["separate-compilation", "open-dispatch", "generic-dispatch"]);
const DYNAMIC_BOUNDARIES = new Set([
  "channel", "service", "wire", "persistence", "detachedTask", "foreignRetention",
]);

export class BorrowRelationError extends Error {
  constructor(code, facts = {}) {
    super(code);
    this.code = code;
    this.facts = facts;
  }
}

const text = (value) => typeof value === "string" ? value : "";
const clone = (value) => structuredClone(value);

function declarationKind(declaration) {
  if (Object.prototype.hasOwnProperty.call(declaration ?? {}, "resultIndependent") ||
    Object.prototype.hasOwnProperty.call(declaration ?? {}, "resultStatic")) {
    fail("legacyResultDependencyFlagRejected");
  }
  const kind = text(declaration?.kind) || "free";
  if (!DECLARATION_KINDS.has(kind)) fail("declarationKindUnsupported", { kind });
  return kind;
}

function stableJson(value) {
  if (Array.isArray(value)) return "[" + value.map(stableJson).join(",") + "]";
  if (!value || typeof value !== "object") return JSON.stringify(value);
  return "{" + Object.keys(value).sort().map((key) =>
    JSON.stringify(key) + ":" + stableJson(value[key])).join(",") + "}";
}

export function digest(value) {
  return "sha256:" + crypto.createHash("sha256").update(stableJson(value)).digest("hex");
}

function slotName(slot, index) {
  if (typeof slot === "string") return slot;
  return text(slot?.slot) || text(slot?.name) || "parameter:" + index;
}

function inputSlots(declaration) {
  return (declaration?.inputs ?? []).map((slot, index) => ({
    ...slot, slot: slotName(slot, index), mode: text(slot?.mode) || "value",
  }));
}

function resultSlots(declaration) {
  return (declaration?.results ?? []).map((slot, index) => ({
    ...slot, slot: slotName(slot, index), mode: text(slot?.mode) || "value",
  }));
}

function isDependentResult(slot) {
  return slot.dependent === true || slot.borrowed === true || DEPENDENT_MODES.has(slot.mode);
}

function isCompatibleInput(slot) {
  return slot.compatible !== false && (
    slot.borrowed === true || slot.dependent === true || BORROW_MODES.has(slot.mode)
  );
}

function canonicalSources(sources) {
  return [...new Set((sources ?? []).map(text).filter(Boolean))].sort();
}

function canonicalMapping(mapping) {
  if (!mapping || typeof mapping !== "object" || Array.isArray(mapping)) return {};
  return Object.fromEntries(Object.keys(mapping).sort().map((slot) => [
    slot, canonicalSources(mapping[slot]),
  ]));
}

function mappingsEqual(left, right) {
  return stableJson(canonicalMapping(left)) === stableJson(canonicalMapping(right));
}

function dependentSlotNames(declaration) {
  return resultSlots(declaration).filter(isDependentResult).map((slot) => slot.slot);
}

function sourceNames(declaration) {
  return inputSlots(declaration).filter(isCompatibleInput).map((slot) => slot.slot);
}

function fail(code, facts = {}) {
  throw new BorrowRelationError(code, facts);
}

function traceData(declaration, trace) {
  const dependent = dependentSlotNames(declaration);
  if (dependent.length === 0) return { mapping: {}, occurrences: {} };
  if (!Array.isArray(trace)) fail("interfaceOriginUnknown", { reason: "bodyTraceMissing" });
  const inputs = new Set(sourceNames(declaration));
  const graph = new Map();
  for (const event of trace) {
    const operation = text(event?.operation);
    if (!["adapter", "union", "return"].includes(operation)) {
      fail("problemTraceOperationInvalid", { operation });
    }
    if (operation !== "adapter" && operation !== "union") continue;
    const output = text(event.output);
    const sources = event.operation === "union" ? event.inputs : [event.input];
    if (!output || !Array.isArray(sources) || sources.length === 0 ||
      sources.some((source) => !text(source))) fail("interfaceOriginMismatch", { event });
    graph.set(output, sources.map(text));
  }
  const resolve = (name, active = new Set()) => {
    if (inputs.has(name)) return [name];
    if (!graph.has(name)) fail("interfaceOriginMismatch", { source: name });
    if (active.has(name)) fail("interfaceOriginCycle", { source: name });
    const next = new Set(active);
    next.add(name);
    return graph.get(name).flatMap((source) => resolve(source, next));
  };
  const occurrences = {};
  for (const event of trace) {
    if (event?.operation !== "return") continue;
    const result = text(event.result);
    const source = text(event.source);
    if (!dependent.includes(result) || !source) fail("interfaceOriginMismatch", { result, source });
    occurrences[result] ??= [];
    occurrences[result].push(...resolve(source));
  }
  for (const result of dependent) {
    if (!occurrences[result] || occurrences[result].length === 0) {
      fail("interfaceOriginUnknown", { result });
    }
  }
  return {
    mapping: canonicalMapping(occurrences),
    occurrences: Object.fromEntries(Object.keys(occurrences).sort().map((key) => [
      key, [...occurrences[key]],
    ])),
  };
}

function assayProblemTrace(input) {
  const declaration = input?.declaration ?? {};
  if (declaration.problemTrace !== undefined || declaration.problemTraceKind !== undefined) {
    fail("problemTracePlacementInvalid", { reason: "caseAssayRequired" });
  }
  const assay = input?.assay;
  if (assay === undefined) return null;
  if (assay?.kind !== "independent-assay" || !Array.isArray(assay.problemTrace)) {
    fail("problemTraceInvalid", { reason: "independentAssayRequired" });
  }
  for (const event of assay.problemTrace) {
    const operation = text(event?.operation);
    if (!["adapter", "union", "return"].includes(operation)) {
      fail("problemTraceOperationInvalid", { operation });
    }
  }
  return assay.problemTrace;
}

export function deriveBodyMapping(declaration, trace = declaration.bodyTrace) {
  return traceData(declaration, trace).mapping;
}

function requiredMapping(declaration, assayTrace = null) {
  const kind = declarationKind(declaration);
  if (dependentSlotNames(declaration).length === 0) return {};
  if (kind === "init") fail("initBorrowResultUnsupported", { kind });
  if (declaration.body === true) return deriveBodyMapping(declaration, declaration.bodyTrace);
  if (assayTrace !== null) return deriveBodyMapping(declaration, assayTrace);
  if (kind === "instance" || kind === "member") return deriveBaselineMapping(declaration);
  fail("interfaceOriginUnknown", { reason: "problemTraceMissing" });
}

export function deriveBaselineMapping(declaration) {
  const kind = declarationKind(declaration);
  const dependent = dependentSlotNames(declaration);
  if (dependent.length === 0) return {};
  if (declaration.body === true) return deriveBodyMapping(declaration, declaration.bodyTrace);
  const inputs = inputSlots(declaration);
  if (kind === "init") fail("initBorrowResultUnsupported", { kind });
  const sources = kind === "instance" || kind === "member"
    ? (() => {
        const receiver = inputs.find((slot) => slot.slot === "receiver");
        return receiver && isCompatibleInput(receiver) ? ["receiver"] : [];
      })()
    : inputs.filter(isCompatibleInput).map((slot) => slot.slot);
  if (sources.length === 0) {
    fail("interfaceOriginUnknown", { reason: "noCompatibleInput" });
  }
  if (kind !== "instance" && kind !== "member" && sources.length > 1) {
    fail(AMBIGUOUS_BODYLESS_RESULT, {
      authority: "none",
      compatibleInputs: [...sources].sort(),
      declarationKind: kind,
      result: dependent,
      reason: "ambiguousBodylessResultOrigin",
    });
  }
  return Object.fromEntries(dependent.map((result) => [result, [...sources].sort()]));
}

function relationPayload(declaration, pairs) {
  if (!Array.isArray(pairs)) fail("relationOmitted");
  const inputs = new Map(inputSlots(declaration).map((slot) => [slot.slot, slot]));
  const results = new Map(resultSlots(declaration).map((slot) => [slot.slot, slot]));
  const dependent = new Set(dependentSlotNames(declaration));
  const seen = new Set();
  const mapping = {};
  const slotModes = {};
  for (const pair of pairs) {
    const result = text(pair?.result);
    if (!result || !dependent.has(result)) fail("relationResultSlotUnknown", { result });
    if (seen.has(result)) fail("relationResultDuplicate", { result });
    seen.add(result);
    const rawSources = Array.isArray(pair?.sources) ? pair.sources.map(text) : [];
    const sources = canonicalSources(rawSources);
    if (sources.length === 0) fail("relationEmptyDependent", { result });
    if (sources.length !== rawSources.length) fail("relationSourceDuplicate", { result });
    for (const source of sources) {
      const slot = inputs.get(source);
      if (!slot) {
        if (results.has(source)) fail("relationResultRecursion", { result, source });
        fail("relationInputSlotUnknown", { result, source });
      }
      if (!isCompatibleInput(slot)) fail("relationSourceModeInvalid", {
        result, source, mode: slot.mode,
      });
    }
    if (pair?.mode !== undefined) {
      const actual = [...new Set(sources.map((source) =>
        inputs.get(source).mode === "mut ref" ? "exclusive" : "shared"))].sort();
      if (!actual.includes(text(pair.mode)) || actual.length !== 1) {
        fail("relationEdgeModeInvalid", { result, expectedMode: pair.mode, actualModes: actual });
      }
    }
    mapping[result] = sources;
    slotModes[result] = {
      resultMode: results.get(result).mode,
      sources: sources.map((source) => ({ slot: source, mode: inputs.get(source).mode })),
    };
  }
  for (const result of dependent) {
    if (!seen.has(result)) fail("relationResultSlotMissing", { result });
  }
  return {
    schema: "w-borrow-relation/1",
    mapping: canonicalMapping(mapping),
    slotModes: Object.fromEntries(Object.keys(slotModes).sort().map((key) => [key, slotModes[key]])),
  };
}

export function deriveRelationContract(declaration) {
  declarationKind(declaration);
  const contract = declaration.relationContract;
  if (!contract) fail("relationOmitted");
  const owner = text(contract.owner);
  if (!RELATION_OWNERS.has(owner)) {
    fail(owner === "witness" ? "relationWitnessOnly" : "relationOwnerInvalid", { owner });
  }
  if (contract.sealed !== true) fail("relationNotSealed", { owner });
  const payload = relationPayload(declaration, contract.pairs);
  const relationDigest = digest(payload);
  if (contract.digest !== undefined && contract.digest !== relationDigest) {
    fail("relationDigestStale", { expected: relationDigest, got: contract.digest });
  }
  return { owner, sealed: true, payload, relationDigest };
}

function deriveEdges(declaration, mapping, occurrences = mapping) {
  const inputs = new Map(inputSlots(declaration).map((slot) => [slot.slot, slot]));
  const edges = [];
  for (const result of Object.keys(occurrences ?? {}).sort()) {
    const sources = Array.isArray(occurrences[result]) ? occurrences[result] : [];
    for (const [index, source] of sources.entries()) {
      const slot = inputs.get(source);
      if (!slot) continue;
      edges.push({
        id: "edge:" + result + ":" + source + ":" + index,
        result,
        ownerSlot: source,
        mode: slot.mode === "mut ref" ? "exclusive" : "shared",
        dynamic: slot.static !== true && slot.immortal !== true,
        origin: text(slot.origin) || source,
      });
    }
  }
  return edges;
}

function genericChecks(declaration, relation) {
  if (!Array.isArray(declaration.genericSubstitutions)) return { status: "not-requested", diagnostics: [] };
  const diagnostics = [];
  for (const check of declaration.genericSubstitutions) {
    try {
      const candidate = relationPayload({
        inputs: check.inputs ?? declaration.inputs,
        results: check.results ?? declaration.results,
      }, check.pairs);
      if (!mappingsEqual(candidate.mapping, relation.payload.mapping) ||
        stableJson(candidate.slotModes) !== stableJson(relation.payload.slotModes)) {
        diagnostics.push({ code: "genericRelationVariance", facts: { substitution: check.id } });
      }
    } catch (error) {
      diagnostics.push({ code: error.code, facts: { substitution: check.id, ...error.facts } });
    }
  }
  return { status: diagnostics.length === 0 ? "accepted" : "rejected", diagnostics };
}

function runtimeCarrier(slot, dependent) {
  if (dependent && slot.mode === "view") return "borrow-view";
  if (slot.mode === "ref") return "borrow-ref";
  if (slot.mode === "mut ref") return "borrow-mut-ref";
  return "value";
}

function deriveAbiFacts(input, declaration) {
  const signature = {
    schema: "w-runtime-signature/1",
    kind: text(declaration.kind) || "free",
    inputs: inputSlots(declaration).map((slot) => ({
      slot: slot.slot, mode: slot.mode, carrier: runtimeCarrier(slot, false),
    })),
    results: resultSlots(declaration).map((slot) => ({
      slot: slot.slot, mode: slot.mode, dependent: isDependentResult(slot),
      carrier: runtimeCarrier(slot, isDependentResult(slot)),
    })),
  };
  const attemptedRuntimeFields = [];
  const recordAttempt = (source, value) => {
    if (value !== undefined) attemptedRuntimeFields.push({ source, value: clone(value) });
  };
  recordAttempt("interface.runtimeFields", input.interface?.runtimeFields);
  recordAttempt("interface.wAbiKeyDelta", input.interface?.wAbiKeyDelta);
  recordAttempt("artifacts.wAbiRelationField", input.artifacts?.wAbiRelationField);
  recordAttempt("artifacts.runtimeCarrier", input.artifacts?.runtimeCarrier);
  recordAttempt("artifacts.runtimeLifetimeMetadata", input.artifacts?.runtimeLifetimeMetadata);
  recordAttempt("artifacts.runtimeRelationTable", input.artifacts?.runtimeRelationTable);
  recordAttempt("input.runtimeLifetimeMetadata", input.runtimeLifetimeMetadata);
  recordAttempt("input.runtimeRelationTable", input.runtimeRelationTable);
  const baselineWAbiKey = digest({ schema: "w-wabi-key/1", signature });
  const candidatePayload = { schema: "w-wabi-key/1", signature };
  if (attemptedRuntimeFields.length > 0) candidatePayload.attemptedRuntimeFields = attemptedRuntimeFields;
  const candidateWAbiKey = digest(candidatePayload);
  const runtimeFields = Object.keys(signature).filter((key) => key.startsWith("runtime:"));
  return {
    runtimeSignature: signature,
    baselineWAbiKey,
    candidateWAbiKey,
    wAbiChanged: candidateWAbiKey !== baselineWAbiKey,
    attemptedRuntimeFields,
    runtimeFields,
    relationMetadataExcluded: !Object.keys(signature).some((key) =>
      /(?:lifetime|relation)/iu.test(key)),
  };
}

function interfaceFacts(input, declaration, relation) {
  if (!relation) return { status: "not-requested", diagnostics: [], semanticInterfaceKey: null };
  const payload = {
    kind: text(declaration.kind) || "free",
    inputs: inputSlots(declaration).map((slot) => ({
      slot: slot.slot, mode: slot.mode, static: slot.static === true, immortal: slot.immortal === true,
    })),
    results: resultSlots(declaration).map((slot) => ({
      slot: slot.slot, mode: slot.mode, dependent: isDependentResult(slot),
    })),
    relation: relation.payload,
  };
  const semanticInterfaceKey = digest(payload);
  const diagnostics = [];
  const iface = input.interface ?? {};
  if (iface.semanticInterfaceKey !== undefined && iface.semanticInterfaceKey !== semanticInterfaceKey) {
    diagnostics.push({ code: "semanticInterfaceKeyMismatch" });
  }
  if (iface.relationDigest !== undefined && iface.relationDigest !== relation.relationDigest) {
    diagnostics.push({ code: "interfaceRelationDigestMismatch" });
  }
  if (iface.wAbiKeyDelta !== undefined || iface.runtimeFields !== undefined) {
    diagnostics.push({ code: "wAbiRuntimeFieldRejected" });
  }
  return {
    status: diagnostics.length === 0 ? "accepted" : "rejected",
    diagnostics, semanticInterfaceKey, relationDigest: relation.relationDigest,
  };
}

function artifactFacts(input, declaration, relation) {
  const artifacts = input.artifacts ?? {};
  const diagnostics = [];
  const verificationScope = text(artifacts.verificationScope);
  const verificationStage = text(artifacts.verificationStage);
  if (Object.prototype.hasOwnProperty.call(artifacts, "verified")) {
    diagnostics.push({ code: "legacyVerificationFlagRejected" });
  }
  const allegesVerification = verificationScope !== "" || verificationStage !== "";
  if (artifacts.callerClaim !== undefined || artifacts.callSiteRelation !== undefined) {
    diagnostics.push({ code: "callerRelationClaimRejected" });
  }
  if (artifacts.runtimeLifetimeMetadata !== undefined || artifacts.runtimeRelationTable !== undefined) {
    diagnostics.push({ code: "runtimeLifetimeMetadataRejected" });
  }
  if (input.runtimeLifetimeMetadata !== undefined || input.runtimeRelationTable !== undefined) {
    diagnostics.push({ code: "runtimeLifetimeMetadataRejected" });
  }
  if (artifacts.wAbiRelationField !== undefined || artifacts.runtimeCarrier !== undefined) {
    diagnostics.push({ code: "wAbiRuntimeFieldRejected" });
  }
  if (artifacts.erasedWitness === true || artifacts.foreignWitness === true) {
    diagnostics.push({ code: "unverifiedWitnessRejected" });
  }
  if (verificationScope !== "" && !VERIFICATION_SCOPES.has(verificationScope)) {
    diagnostics.push({ code: "verificationScopeInvalid" });
  }
  if (allegesVerification && verificationStage !== "verified") {
    diagnostics.push({ code: "verificationStageInvalid" });
  }
  const implementationReceipt = artifacts.implementationReceipt;
  const witnessReceipt = artifacts.witnessReceipt;
  if (allegesVerification && (!implementationReceipt || typeof implementationReceipt !== "object")) {
    diagnostics.push({ code: "implementationReceiptMissing" });
  }
  if (allegesVerification && (!witnessReceipt || typeof witnessReceipt !== "object")) {
    diagnostics.push({ code: "witnessReceiptMissing" });
  }
  if (allegesVerification && artifacts.providerInterfaceKey === undefined) {
    diagnostics.push({ code: "providerInterfaceKeyMissing" });
  }
  if (allegesVerification && artifacts.consumerExpectedProviderKey === undefined) {
    diagnostics.push({ code: "consumerProviderExpectationMissing" });
  }
  if (!relation) return {
    status: diagnostics.length === 0 ? "not-requested" : "rejected",
    diagnostics, relationDigest: null, verificationScope, verificationStage,
    implementationVerified: false, witnessVerified: false,
  };
  const relationDigest = relation.relationDigest;
  if (artifacts.relationDigest !== undefined && artifacts.relationDigest !== relationDigest) {
    diagnostics.push({ code: "relationDigestMismatch" });
  }
  if (artifacts.interfaceLockRelationDigest !== undefined &&
    artifacts.interfaceLockRelationDigest !== relationDigest) {
    diagnostics.push({ code: "interfaceLockMismatch" });
  }
  const implementationRaw = implementationReceipt?.mapping ?? artifacts.implementationMapping;
  const witnessRaw = witnessReceipt?.mapping ?? artifacts.witnessMapping;
  const implementation = implementationRaw === undefined
    ? (allegesVerification ? null : relation.payload.mapping) : canonicalMapping(implementationRaw);
  const witness = witnessRaw === undefined
    ? (allegesVerification ? null : implementation) : canonicalMapping(witnessRaw);
  if (implementationReceipt && implementationReceipt.relationDigest !== relationDigest) {
    diagnostics.push({ code: "implementationReceiptRelationMismatch" });
  }
  if (witnessReceipt && witnessReceipt.relationDigest !== relationDigest) {
    diagnostics.push({ code: "witnessReceiptRelationMismatch" });
  }
  if (implementation === null || (allegesVerification && implementationReceipt?.mapping === undefined)) {
    if (allegesVerification) diagnostics.push({ code: "implementationReceiptMissing" });
  } else if (!mappingsEqual(implementation, relation.payload.mapping)) {
    diagnostics.push({ code: "implementationRelationConflict" });
  }
  if (witness === null || (allegesVerification && witnessReceipt?.mapping === undefined)) {
    if (allegesVerification) diagnostics.push({ code: "witnessReceiptMissing" });
  } else if (!mappingsEqual(witness, relation.payload.mapping)) {
    diagnostics.push({ code: "interfaceWitnessMismatch" });
  }
  if (artifacts.witnessOnly === true) diagnostics.push({ code: "relationWitnessOnly" });
  const ifacePayload = {
    relationDigest, mapping: relation.payload.mapping, slotModes: relation.payload.slotModes,
    publicInputs: inputSlots(declaration).map((slot) => ({ slot: slot.slot, mode: slot.mode })),
  };
  const providerKey = digest(ifacePayload);
  if (artifacts.providerInterfaceKey !== undefined && artifacts.providerInterfaceKey !== providerKey) {
    diagnostics.push({ code: "providerInterfaceKeyMismatch" });
  }
  if (artifacts.consumerExpectedProviderKey !== undefined &&
    artifacts.consumerExpectedProviderKey !== providerKey) {
    diagnostics.push({ code: "consumerProviderExpectationMismatch" });
  }
  if (allegesVerification && implementationReceipt && implementationReceipt.relationDigest === undefined) {
    diagnostics.push({ code: "implementationReceiptRelationMissing" });
  }
  if (allegesVerification && witnessReceipt && witnessReceipt.relationDigest === undefined) {
    diagnostics.push({ code: "witnessReceiptRelationMissing" });
  }
  return {
    status: diagnostics.length === 0 ? "accepted" : "rejected",
    diagnostics, relationDigest, implementation, witness, providerKey,
    verificationScope, verificationStage,
    implementationVerified: allegesVerification && implementation !== null &&
      implementationReceipt?.relationDigest === relationDigest &&
      mappingsEqual(implementation, relation.payload.mapping),
    witnessVerified: allegesVerification && witness !== null &&
      witnessReceipt?.relationDigest === relationDigest &&
      mappingsEqual(witness, relation.payload.mapping),
  };
}

function invocationFacts(input, declaration, effectiveEdges, effectiveMapping, mappingSource) {
  const invocation = input.invocation ?? {};
  const facts = {
    freshLoans: [], invocationEdges: [], persistentEdges: effectiveEdges,
    nextAllowedWhileViewLive: true, suspension: "not-requested",
    boundary: text(invocation.boundary) || "internal",
    closureStorage: "not-requested", erasure: "not-requested",
    mappingSource,
    effectiveMapping: canonicalMapping(effectiveMapping),
    effectiveEdges: clone(effectiveEdges),
    effectiveOriginSets: canonicalSources(effectiveEdges.map((edge) => edge.origin)),
  };
  if (invocation.kind === "callable") {
    const parameter = text(invocation.parameter) || "parameter:0";
    const calls = Number.isSafeInteger(invocation.calls) ? Math.max(0, invocation.calls) : 1;
    facts.freshLoans = Array.from({ length: calls }, (_, index) => ({
      id: "call:" + index + ":loan", ownerSlot: parameter, mode: "shared",
      createdAt: "invocation", lifetime: "result-use", end: "last-result-use",
    }));
    facts.invocationEdges = facts.freshLoans.flatMap((loan, callIndex) =>
      Object.entries(canonicalMapping(effectiveMapping)).flatMap(([result, sources]) =>
        sources.filter((source) => source === parameter || source === "receiver").map((source, edgeIndex) => ({
          id: "call:" + callIndex + ":edge:" + result + ":" + edgeIndex,
          result, ownerSlot: loan.id, mode: "shared", dynamic: true,
          origin: loan.id + ":" + source, lifetime: "result-use", end: "last-result-use",
        }))));
    facts.persistentEdges = [];
    facts.resultEdgeLifetime = "last-result-use";
  }
  if (invocation.erasure === "any-fn") {
    facts.erasure = {
      representation: "any-fn", mapping: canonicalMapping(effectiveMapping),
      mappingComponentDigest: digest(canonicalMapping(effectiveMapping)),
    };
  }
  if (invocation.kind === "stream-next" && invocation.viewLive === true &&
    invocation.reusesStorage === true && effectiveEdges.some((edge) => edge.dynamic)) {
    facts.nextAllowedWhileViewLive = false;
    return { ...facts, status: "rejected", code: "W-BORROW-0006", reason: "nextConflictsWithLiveView" };
  }
  if (invocation.await === true) {
    const stable = invocation.ownerStable === true && invocation.storageStable === true &&
      invocation.noConflict === true && invocation.cleanupComplete === true &&
      invocation.cancelDrainComplete === true;
    facts.suspension = stable ? "accepted" : "rejected";
    if (!stable) return { ...facts, status: "rejected", code: "W-BORROW-0007", reason: "unstableReferentOrCleanupSuspension" };
  }
  if (invocation.closureStorage) {
    facts.closureStorage = invocation.closureStorage;
    if (invocation.closureStorage.escape === true &&
      (invocation.closureStorage.returnedView === true || effectiveEdges.some((edge) => edge.dynamic))) {
      return { ...facts, status: "rejected", code: "W-BORROW-0003", reason: "dependentClosureEscape" };
    }
  }
  if (DYNAMIC_BOUNDARIES.has(facts.boundary) && effectiveEdges.some((edge) => edge.dynamic)) {
    return { ...facts, status: "rejected", code: "W-BORROW-0003", reason: "dependentEscape" };
  }
  return { ...facts, status: "accepted", code: null, reason: null };
}

const REJECTING_DIAGNOSTICS = new Set([
  "callerRelationClaimRejected", "runtimeLifetimeMetadataRejected", "wAbiRuntimeFieldRejected",
  "legacyResultDependencyFlagRejected", "legacyVerificationFlagRejected",
  "relationWitnessOnly", "relationOwnerInvalid", "relationNotSealed", "relationDigestStale",
  "relationInputSlotUnknown", "relationResultSlotUnknown", "relationResultDuplicate",
  "relationResultSlotMissing", "relationEmptyDependent", "relationSourceModeInvalid",
  "relationSourceDuplicate",
  "relationEdgeModeInvalid", "relationResultRecursion", "implementationRelationConflict",
  "unverifiedWitnessRejected",
  "problemTracePlacementInvalid", "problemTraceInvalid", "problemTraceOperationInvalid",
  "interfaceOriginMismatch", "interfaceOriginCycle", "interfaceOriginUnknown",
  "verificationScopeInvalid", "verificationStageInvalid", "implementationReceiptMissing",
  "witnessReceiptMissing", "providerInterfaceKeyMissing", "consumerProviderExpectationMissing",
  "implementationReceiptRelationMismatch", "witnessReceiptRelationMismatch",
  "implementationReceiptRelationMissing", "witnessReceiptRelationMissing",
  "interfaceWitnessMismatch", "genericRelationVariance", "providerInterfaceKeyMismatch",
  "consumerProviderExpectationMismatch", "interfaceLockMismatch", "semanticInterfaceKeyMismatch",
  "interfaceRelationDigestMismatch", "relationDigestMismatch",
]);

export function evaluateBorrowRelationCase(rawInput) {
  const input = clone(rawInput ?? {});
  const declaration = clone(input.declaration ?? {});
  let assayTrace = null;
  let assayError = null;
  try { assayTrace = assayProblemTrace(input); } catch (error) {
    assayError = { code: error.code, facts: error.facts };
  }
  let required = {};
  let requiredError = null;
  try { required = requiredMapping(declaration, assayTrace); } catch (error) {
    requiredError = { code: error.code, facts: error.facts };
  }
  let baseline = {};
  let baselineError = null;
  try { baseline = deriveBaselineMapping(declaration); } catch (error) {
    baselineError = { code: error.code, facts: error.facts };
  }
  let relation = null;
  let relationError = null;
  try { relation = deriveRelationContract(declaration); } catch (error) {
    relationError = { code: error.code, facts: error.facts };
  }
  let bodyData = { mapping: baseline, occurrences: baseline };
  if (declaration.body === true && Array.isArray(declaration.bodyTrace)) {
    try { bodyData = traceData(declaration, declaration.bodyTrace); } catch { }
  }
  const baselineEdges = deriveEdges(declaration, baseline, bodyData.occurrences);
  const relationEdges = relation ? deriveEdges(declaration, relation.payload.mapping) : [];
  const baselineExact = !baselineError && !requiredError && mappingsEqual(baseline, required);
  const relationExact = !!relation && !relationError && !requiredError &&
    mappingsEqual(relation.payload.mapping, required);
  const abi = deriveAbiFacts(input, declaration);
  const artifacts = artifactFacts(input, declaration, relation);
  const generic = relation ? genericChecks(declaration, relation) : { status: "not-requested", diagnostics: [] };
  const interfaces = interfaceFacts(input, declaration, relation);
  const diagnostics = [
    ...(relationError ? [{ code: relationError.code, facts: relationError.facts }] : []),
    ...artifacts.diagnostics, ...generic.diagnostics, ...interfaces.diagnostics,
  ];
  const suppressAmbiguousBaseline = baselineError?.code === AMBIGUOUS_BODYLESS_RESULT &&
    (declaration.relationContract !== undefined || declaration.requireRelation === true ||
      input.artifacts?.callerClaim !== undefined || input.artifacts?.callSiteRelation !== undefined);
  const structuralErrors = [
    assayError,
    suppressAmbiguousBaseline ? null : baselineError,
    (assayTrace !== null || assayError) ? requiredError : null,
  ].filter(Boolean);
  for (const structuralError of structuralErrors.reverse()) {
    if (!diagnostics.some((item) => item.code === structuralError.code)) {
      diagnostics.unshift({ code: structuralError.code, facts: structuralError.facts });
    }
  }
  if (declaration.body === true && relation &&
    !mappingsEqual(relation.payload.mapping, baseline)) {
    diagnostics.push({ code: "implementationRelationConflict" });
  }
  const uniqueDiagnostics = [];
  for (const diagnostic of diagnostics) {
    if (!uniqueDiagnostics.some((item) => item.code === diagnostic.code)) uniqueDiagnostics.push(diagnostic);
  }
  diagnostics.splice(0, diagnostics.length, ...uniqueDiagnostics);
  const relationDiagnostic = diagnostics.some((item) => REJECTING_DIAGNOSTICS.has(item.code));
  const ambiguousBaseline = baselineError?.code === AMBIGUOUS_BODYLESS_RESULT;
  const relationApplicable = !!relation && relationExact && !relationDiagnostic &&
    !requiredError && (!baselineError || ambiguousBaseline);
  const effectiveMapping = relationApplicable ? relation.payload.mapping : baseline;
  const effectiveEdges = relationApplicable ? relationEdges : baselineEdges;
  const invocation = invocationFacts(
    input, declaration, effectiveEdges, effectiveMapping,
    relationApplicable ? "relation" : "baseline",
  );
  const missingRequiredRelation = declaration.requireRelation === true &&
    relationError?.code === "relationOmitted";
  const invalidAssay = !!assayError;
  const rejected = (baselineError && !relationApplicable) || invalidAssay || missingRequiredRelation ||
    abi.wAbiChanged || diagnostics.some((item) => REJECTING_DIAGNOSTICS.has(item.code));
  // BRX2 is retained as historical provenance. BRX3 owns the current source
  // clause, so a valid relation candidate must not appear as active research.
  const route = rejected ? "rejected" : baselineExact ? "current" : "historical-candidate";
  const declarationDecision = rejected ? "rejected" : baselineExact ? "accepted" : "historical-candidate";
  return {
    id: text(input.id),
    assay: {
      kind: assayTrace === null ? "none" : "independent-assay",
      operationCount: Array.isArray(assayTrace) ? assayTrace.length : 0,
    },
    route,
    decision: declarationDecision,
    declarationDecision,
    invocationStatus: invocation.status,
    mapping: {
      required, requiredError, baseline, baselineError, baselineExact, baselineEdges,
      baselineOriginSets: canonicalSources(baselineEdges.map((edge) => edge.origin)),
      relation: relation?.payload.mapping ?? {}, relationError, relationExact,
      relationDigest: relation?.relationDigest ?? null, relationEdges,
      relationOriginSets: canonicalSources(relationEdges.map((edge) => edge.origin)),
      relationApplicable, effective: effectiveMapping,
      effectiveEdges, effectiveOriginSets: canonicalSources(effectiveEdges.map((edge) => edge.origin)),
    },
    invocation, artifacts, generic, interfaces, abi, diagnostics,
    digest: digest({
      route, baseline, relation: relation?.payload ?? null, baselineEdges, relationEdges,
      effectiveMapping, effectiveEdges, semanticInterfaceKey: interfaces.semanticInterfaceKey,
      baselineWAbiKey: abi.baselineWAbiKey, candidateWAbiKey: abi.candidateWAbiKey,
    }),
  };
}

export function deriveBorrowRelationCorpus(corpus) {
  if (!corpus || !Array.isArray(corpus.cases)) throw new BorrowRelationError("invalidCorpus");
  return corpus.cases.map(evaluateBorrowRelationCase);
}

export function assertNoRuntimeLifetimeMetadata(result) {
  return Array.isArray(result?.abi?.runtimeFields) && result.abi.runtimeFields.length === 0 &&
    result.abi.relationMetadataExcluded === true;
}
