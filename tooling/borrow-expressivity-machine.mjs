import crypto from "node:crypto";

const BORROW_MODES = new Set(["ref", "view", "mut ref"]);
const DEPENDENT_MODES = new Set(["ref", "view", "mut ref"]);
const AMBIGUOUS_BODYLESS_RESULT = "W-BORROW-0011";
const DYNAMIC_BOUNDARIES = new Set([
  "channel",
  "service",
  "wire",
  "persistence",
  "detachedTask",
  "foreignRetention",
]);

export class BorrowExpressivityError extends Error {
  constructor(code, facts = {}) {
    super(code);
    this.code = code;
    this.facts = facts;
  }
}

function text(value) {
  return typeof value === "string" ? value : "";
}

function clone(value) {
  return structuredClone(value);
}

function stableJson(value) {
  if (Array.isArray(value)) return `[${value.map(stableJson).join(",")}]`;
  if (!value || typeof value !== "object") return JSON.stringify(value);
  return `{${Object.keys(value)
    .sort()
    .map((key) => `${JSON.stringify(key)}:${stableJson(value[key])}`)
    .join(",")}}`;
}

function digest(value) {
  return `sha256:${crypto.createHash("sha256").update(stableJson(value)).digest("hex")}`;
}

function slotName(slot, index) {
  if (typeof slot === "string") return slot;
  return text(slot?.slot) || text(slot?.name) || `parameter:${index}`;
}

function inputSlots(declaration) {
  return (declaration?.inputs ?? []).map((slot, index) => ({
    ...slot,
    slot: slotName(slot, index),
    mode: text(slot?.mode) || "value",
  }));
}

function resultSlots(declaration) {
  return (declaration?.results ?? []).map((slot, index) => ({
    ...slot,
    slot: slotName(slot, index),
    mode: text(slot?.mode) || "value",
  }));
}

function isDependentResult(slot) {
  return slot.dependent === true || slot.borrowed === true || DEPENDENT_MODES.has(slot.mode);
}

function isCompatibleInput(slot) {
  return slot.compatible !== false && (
    slot.borrowed === true ||
    slot.dependent === true ||
    BORROW_MODES.has(slot.mode)
  );
}

function canonicalSources(sources) {
  return [...new Set((sources ?? []).map(text).filter(Boolean))].sort();
}

function canonicalMapping(mapping) {
  if (!mapping || typeof mapping !== "object" || Array.isArray(mapping)) return {};
  return Object.fromEntries(
    Object.keys(mapping)
      .sort()
      .map((slot) => [slot, canonicalSources(mapping[slot])]),
  );
}

function mappingsEqual(left, right) {
  return stableJson(canonicalMapping(left)) === stableJson(canonicalMapping(right));
}

function dependentSlotNames(declaration) {
  return resultSlots(declaration)
    .filter(isDependentResult)
    .map((slot) => slot.slot);
}

function sourceNames(declaration) {
  return inputSlots(declaration)
    .filter(isCompatibleInput)
    .map((slot) => slot.slot);
}

function relationPairsToMapping(pairs, declaration) {
  if (!Array.isArray(pairs)) {
    throw new BorrowExpressivityError("researchRelationMissing");
  }

  const knownInputs = new Set(sourceNames(declaration));
  const knownResults = new Set(dependentSlotNames(declaration));
  const mapping = {};
  const seen = new Set();

  for (const pair of pairs) {
    const result = text(pair?.result);
    if (!result || !knownResults.has(result)) {
      throw new BorrowExpressivityError("researchRelationForged", { result });
    }
    if (seen.has(result)) {
      throw new BorrowExpressivityError("researchRelationDuplicate", { result });
    }
    seen.add(result);
    const rawSources = Array.isArray(pair?.sources) ? pair.sources.map(text) : [];
    const sources = canonicalSources(rawSources);
    if (rawSources.length !== sources.length) {
      throw new BorrowExpressivityError("researchRelationDuplicate", { result, sources });
    }
    if (sources.length === 0 || sources.some((source) => !knownInputs.has(source))) {
      throw new BorrowExpressivityError("researchRelationForged", { result, sources });
    }
    mapping[result] = sources;
  }

  for (const result of knownResults) {
    if (!seen.has(result)) {
      throw new BorrowExpressivityError("researchRelationMissing", { result });
    }
  }

  return mapping;
}

function deriveTraceData(declaration, trace = declaration.bodyTrace) {
  const dependentResults = dependentSlotNames(declaration);
  if (dependentResults.length === 0) return { mapping: {}, occurrences: {} };
  if (!Array.isArray(trace)) {
    throw new BorrowExpressivityError("interfaceOriginUnknown", { reason: "bodyTraceMissing" });
  }

  const knownInputs = new Set(sourceNames(declaration));
  const originGraph = new Map();
  for (const event of trace) {
    if (event?.operation !== "adapter" && event?.operation !== "union") continue;
    const output = text(event.output);
    const inputs = event.operation === "union"
      ? event.inputs
      : [event.input];
    if (!output || !Array.isArray(inputs) || inputs.some((input) => !text(input))) {
      throw new BorrowExpressivityError("interfaceOriginMismatch", { event });
    }
    originGraph.set(output, inputs.map(text));
  }

  const resolveOrigins = (name, active = new Set()) => {
    if (knownInputs.has(name)) return [name];
    if (!originGraph.has(name)) {
      throw new BorrowExpressivityError("interfaceOriginMismatch", { source: name });
    }
    if (active.has(name)) {
      throw new BorrowExpressivityError("interfaceOriginMismatch", { source: name, reason: "originCycle" });
    }
    const nextActive = new Set(active).add(name);
    return originGraph.get(name).flatMap((source) => resolveOrigins(source, nextActive));
  };

  const mapping = {};
  for (const event of trace) {
    if (event?.operation !== "return") continue;
    const result = text(event.result);
    const source = text(event.source);
    if (!dependentResults.includes(result) || !source) {
      throw new BorrowExpressivityError("interfaceOriginMismatch", { result, source });
    }
    mapping[result] ??= [];
    mapping[result].push(...resolveOrigins(source));
  }

  for (const result of dependentResults) {
    if (!mapping[result] || mapping[result].length === 0) {
      throw new BorrowExpressivityError("interfaceOriginUnknown", { result });
    }
  }
  return {
    mapping: canonicalMapping(mapping),
    occurrences: Object.fromEntries(
      Object.keys(mapping)
        .sort()
        .map((result) => [result, [...mapping[result]]]),
    ),
  };
}

function deriveBodyMapping(declaration, trace = declaration.bodyTrace) {
  return deriveTraceData(declaration, trace).mapping;
}

/**
 * Derive the current W-914 body/bodyless mapping.
 *
 * Bodyless results use one uniquely derivable origin. Ambiguous free,
 * static, or protocol results reject with W-BORROW-0011. This function does
 * not consume a caller supplied expected mapping.
 */
export function deriveBaselineMapping(declaration) {
  const dependentResults = dependentSlotNames(declaration);
  if (dependentResults.length === 0) return {};
  if (declaration.body === true) return deriveBodyMapping(declaration);

  const inputs = inputSlots(declaration);
  const kind = text(declaration.kind) || "free";
  if (kind === "init") {
    throw new BorrowExpressivityError("initBorrowResultUnsupported", { kind });
  }
  let sources;
  if (kind === "instance" || kind === "member") {
    const receiver = inputs.find((slot) => slot.slot === "receiver");
    sources = receiver && isCompatibleInput(receiver) ? ["receiver"] : [];
  } else {
    sources = inputs.filter(isCompatibleInput).map((slot) => slot.slot);
  }

  if (sources.length === 0) {
    throw new BorrowExpressivityError("interfaceOriginUnknown", { reason: "noCompatibleInput" });
  }
  if (kind !== "instance" && kind !== "member" && sources.length > 1) {
    throw new BorrowExpressivityError(AMBIGUOUS_BODYLESS_RESULT, {
      authority: "none",
      compatibleInputs: [...sources].sort(),
      declarationKind: kind,
      result: dependentResults,
      reason: "ambiguousBodylessResultOrigin",
    });
  }
  return Object.fromEntries(dependentResults.map((result) => [result, [...sources].sort()]));
}

/** Derive the historical-superseded relational candidate from structured pairs. */
export function deriveRelationalMapping(declaration) {
  return relationPairsToMapping(declaration.relationSchema?.pairs, declaration);
}

function deriveRequiredMapping(declaration) {
  const dependentResults = dependentSlotNames(declaration);
  if (dependentResults.length === 0) return {};
  if (declaration.body === true && Array.isArray(declaration.bodyTrace)) {
    return deriveBodyMapping(declaration, declaration.bodyTrace);
  }
  if (Array.isArray(declaration.problemTrace)) {
    return deriveBodyMapping(declaration, declaration.problemTrace);
  }
  return {};
}

function deriveEdges(declaration, mapping) {
  const inputByName = new Map(inputSlots(declaration).map((slot) => [slot.slot, slot]));
  const edges = [];
  for (const result of Object.keys(mapping ?? {}).sort()) {
    const sources = Array.isArray(mapping[result]) ? [...mapping[result]].sort() : [];
    for (const [index, source] of sources.entries()) {
      const slot = inputByName.get(source);
      if (!slot) continue;
      edges.push({
        id: `edge:${result}:${source}:${index}`,
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

function deriveBodyEdges(declaration, mapping) {
  const trace = declaration.body === true
    ? declaration.bodyTrace
    : declaration.problemTrace;
  if (!Array.isArray(trace)) return deriveEdges(declaration, mapping);
  try {
    return deriveEdges(declaration, deriveTraceData(declaration, trace).occurrences);
  } catch {
    return deriveEdges(declaration, mapping);
  }
}

function evaluateInvocation(input, edges, mapping) {
  const invocation = input.invocation ?? {};
  const facts = {
    freshLoans: [],
    invocationEdges: [],
    persistentEdges: edges,
    nextAllowedWhileViewLive: true,
    suspension: "not-requested",
    boundary: "internal",
    closureStorage: "not-requested",
    erasure: "not-requested",
  };

  if (invocation.kind === "callable") {
    const parameter = text(invocation.parameter) || "parameter:0";
    const calls = Number.isSafeInteger(invocation.calls) ? invocation.calls : 1;
    facts.freshLoans = Array.from({ length: Math.max(0, calls) }, (_, index) => ({
      id: `call:${index}:loan`,
      ownerSlot: parameter,
      mode: "shared",
      createdAt: "invocation",
      scope: "result-use",
      lifetime: "result-use",
      end: "last-result-use",
    }));
    facts.invocationEdges = facts.freshLoans.flatMap((loan, callIndex) =>
      Object.entries(canonicalMapping(mapping)).flatMap(([result, sources]) =>
        sources
          .filter((source) => source === parameter || source === "receiver")
          .map((source, edgeIndex) => ({
            id: `call:${callIndex}:edge:${result}:${edgeIndex}`,
            result,
            ownerSlot: loan.id,
            mode: "shared",
            dynamic: true,
            origin: `${loan.id}:${source}`,
            lifetime: "result-use",
            end: invocation.resultLastUse === "explicit"
              ? "explicit-last-use"
              : "last-result-use",
          })),
      ),
    );
    facts.resultEdgeLifetime = "last-result-use";
    facts.persistentEdges = [];
  }

  if (invocation.erasure === "any-fn") {
    facts.erasure = {
      representation: "any-fn",
      mapping: canonicalMapping(mapping),
      mappingComponentDigest: digest(canonicalMapping(mapping)),
    };
  }

  if (invocation.kind === "stream-next" && invocation.viewLive === true) {
    facts.nextAllowedWhileViewLive = invocation.reusesStorage !== true;
    if (invocation.reusesStorage === true) {
      return {
        ...facts,
        status: "rejected",
        code: "W-BORROW-0006",
        reason: "nextConflictsWithLiveView",
      };
    }
  }

  if (invocation.await === true) {
    const stable = invocation.ownerStable === true && invocation.storageStable === true;
    facts.suspension = stable ? "accepted" : "rejected";
    if (!stable) {
      return {
        ...facts,
        status: "rejected",
        code: "W-BORROW-0007",
        reason: "unstableReferentSuspension",
      };
    }
  }

  if (invocation.closureStorage) {
    facts.closureStorage = invocation.closureStorage;
    if (
      invocation.closureStorage.escape === true &&
      (invocation.closureStorage.returnedView === true || edges.some((edge) => edge.dynamic))
    ) {
      return {
        ...facts,
        status: "rejected",
        code: "W-BORROW-0003",
        reason: "dependentClosureEscape",
      };
    }
  }

  const boundary = text(invocation.boundary) || "internal";
  facts.boundary = boundary;
  if (DYNAMIC_BOUNDARIES.has(boundary) && edges.some((edge) => edge.dynamic)) {
    return {
      ...facts,
      status: "rejected",
      code: "W-BORROW-0003",
      reason: "dependentEscape",
    };
  }

  return { ...facts, status: "accepted", code: null, reason: null };
}

function compareArtifacts(input, mapping) {
  const artifacts = input.artifacts;
  if (!artifacts) {
    return {
      status: "not-requested",
      diagnostics: [],
      mappingComponentDigest: digest(mapping),
    };
  }

  const diagnostics = [];
  const implementation = canonicalMapping(artifacts.implementationMapping ?? mapping);
  const witness = artifacts.witnessMapping === undefined
    ? implementation
    : canonicalMapping(artifacts.witnessMapping);
  const lock = artifacts.interfaceLockMapping === undefined
    ? implementation
    : canonicalMapping(artifacts.interfaceLockMapping);
  const key = artifacts.mappingComponentDigest ?? digest(implementation);
  const computedKey = digest(implementation);

  if (artifacts.mappingPairs !== undefined) {
    try {
      relationPairsToMapping(artifacts.mappingPairs, input.declaration);
    } catch (error) {
      diagnostics.push({ code: error.code, facts: error.facts });
    }
  }
  if (!mappingsEqual(implementation, witness)) diagnostics.push({ code: "interfaceWitnessMismatch" });
  if (!mappingsEqual(implementation, lock)) diagnostics.push({ code: "interfaceMappingChanged" });
  if (key !== computedKey) diagnostics.push({ code: "interfaceLockMismatch" });

  return {
    status: diagnostics.length === 0 ? "accepted" : "rejected",
    diagnostics,
    mappingComponentDigest: computedKey,
    implementation,
    witness,
    lock,
  };
}

function compareForms(declaration, baseline, required, relational, baselineError) {
  const kind = text(declaration.kind) || "free";
  const baselineExact = mappingsEqual(baseline, required);
  const relationalExact = mappingsEqual(relational, required);
  const aggregate = declaration.behavior?.returnShape === "sum";
  const nominalOwned = declaration.behavior?.returnShape === "nominal-owned";

  return {
    A1_memberReceiver: kind === "instance" && baselineExact ? "closes" : "not-general",
    A1_bodyDerivedFree: declaration.body === true && baselineExact ? "closes" : "not-general",
    A2_freeAllInputs: baselineError?.code === AMBIGUOUS_BODYLESS_RESULT
      ? "rejects-ambiguous-inputs"
      : kind !== "instance" && !baselineExact ? "not-general" : "closes",
    B1_relationalSchema: dependentSlotNames(declaration).length === 0
      ? "not-applicable"
      : relationalExact ? "candidate-closes" : "candidate-missing",
    B2_returnAggregate: nominalOwned ? "owned-nominal-alternative"
      : aggregate ? "api-change" : "does-not-preserve-direct-result",
  };
}

export function evaluateBorrowCase(input) {
  const declaration = clone(input.declaration ?? {});
  let baseline;
  let baselineError = null;
  try {
    baseline = deriveBaselineMapping(declaration);
  } catch (error) {
    baseline = {};
    baselineError = { code: error.code, facts: error.facts };
  }

  let required;
  try {
    required = deriveRequiredMapping(declaration);
  } catch (error) {
    required = {};
  }
  if (!declaration.body && !Array.isArray(declaration.problemTrace) && !baselineError) {
    required = baseline;
  }

  let relational;
  let relationalError = null;
  try {
    relational = deriveRelationalMapping(declaration);
  } catch (error) {
    relational = {};
    relationalError = { code: error.code, facts: error.facts };
  }

  const baselineEdges = declaration.body === true
    ? deriveBodyEdges(declaration, baseline)
    : deriveEdges(declaration, baseline);
  const relationalEdges = deriveEdges(declaration, relational);
  const invocation = evaluateInvocation(input, baselineEdges, baseline);
  const artifacts = compareArtifacts(input, baseline);
  const baselineExact = !baselineError && mappingsEqual(baseline, required);
  const relationalExact = !relationalError && mappingsEqual(relational, required);
  const mappingDecision = baselineError && !(baselineError.code === AMBIGUOUS_BODYLESS_RESULT && relationalExact)
    ? "rejected"
    : baselineExact
      ? "accepted"
      : "historical-candidate";
  const runtimeLifetimeMetadata = [];

  return {
    id: text(input.id),
    mapping: {
      required,
      baseline,
      baselineEdges,
      baselineOriginSets: canonicalMapping(baseline),
      baselineError,
      relational,
      relationalEdges,
      relationalOriginSets: canonicalMapping(relational),
      relationalError,
      baselineExact,
      relationalExact,
    },
    invocation,
    artifacts,
    forms: compareForms(declaration, baseline, required, relational, baselineError),
    decision: mappingDecision,
    runtimeLifetimeMetadata,
    digest: digest({ baseline, relational, baselineEdges, relationalEdges, runtimeLifetimeMetadata }),
  };
}

export function evaluateCorpus(corpus) {
  if (!corpus || !Array.isArray(corpus.cases)) {
    throw new BorrowExpressivityError("invalidCorpus");
  }
  return corpus.cases.map(evaluateBorrowCase);
}

export function assertNoRuntimeLifetimeMetadata(result) {
  return Array.isArray(result.runtimeLifetimeMetadata) && result.runtimeLifetimeMetadata.length === 0;
}
