import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const directory = path.dirname(fileURLToPath(import.meta.url));
const canonicalLabels = ["limit", "ordering", "using"];

export function loadCases() {
  const source = JSON.parse(fs.readFileSync(path.join(directory, "cases.json"), "utf8"));
  return {
    ...source,
    cases: (source.cases ?? []).map((testCase) => ({ ...source.defaults, ...testCase })),
  };
}

function rejected(testCase, code) {
  return { id: testCase.id, status: "rejected", code };
}

function publicationStep(items) {
  return Math.max(0, ...items.map((item) => item.cleanupCompleteStep));
}

function committedEffects(items) {
  return items.filter((item) => item.effectCommitted === true).map((item) => item.index);
}

function orderedItems(testCase) {
  return [...testCase.items].sort(testCase.ordering === "input"
    ? (left, right) => left.index - right.index
    : (left, right) => left.bodySettlementStep - right.bodySettlementStep);
}

export function evaluateCase(testCase) {
  if (JSON.stringify(testCase.callLabels) !== JSON.stringify(canonicalLabels)) return rejected(testCase, "canonicalLabelsRequired");
  if (!Number.isInteger(testCase.limit) || testCase.limit <= 0) return rejected(testCase, "positiveLimitRequired");
  if (testCase.implicitUnbounded === true) return rejected(testCase, "explicitBoundRequired");
  if (testCase.family === "parallel" && testCase.domainExplicit !== true) return rejected(testCase, "parallelDomainRequired");
  if (testCase.family === "parallel" && testCase.parallelCapability !== true) return rejected(testCase, "parallelCapabilityRequired");
  if (testCase.family === "concurrent" && testCase.domainExplicit === true) return rejected(testCase, "concurrentDomainSlotRejected");
  if (testCase.stagesInputOnce !== true) return rejected(testCase, "inputMustStageOnce");
  if (testCase.stagesCallableOnce !== true) return rejected(testCase, "callableMustStageOnce");
  if (testCase.callableMode !== "some") return rejected(testCase, "concurrentCallableRequired");
  if (testCase.admittedItemsMoveOnce !== true) return rejected(testCase, "admittedItemMustMoveOnce");
  if (testCase.remainingInputsDropOnce !== true) return rejected(testCase, "remainingInputsMustDropOnce");
  if (testCase.resultStorageReservedBeforeChildren !== true) return rejected(testCase, "resultPreflightRequired");
  if (testCase.returnAfterDrain !== true) return rejected(testCase, "drainRequired");
  if (testCase.maxActiveChildren > testCase.limit) return rejected(testCase, "activeChildLimitExceeded");
  if (testCase.operation === "collect" && testCase.collectCancelsOnApplicationError === true) return rejected(testCase, "collectMustObserveApplicationErrors");
  if (testCase.operation === "collect" && testCase.collectKeepsInputIndex !== true) return rejected(testCase, "collectInputIdentityRequired");

  const items = testCase.items;
  const base = {
    id: testCase.id,
    status: "accepted",
    publicationStep: publicationStep(items),
    committedEffectsRemain: committedEffects(items),
  };
  if (testCase.parentCanceled === true) return { ...base, code: "parent-canceled", result: null };
  if (testCase.faulted === true) return { ...base, code: "fault-boundary", result: null };

  if (testCase.operation === "collect") {
    return {
      ...base,
      code: "collect",
      settlements: orderedItems(testCase).map((item) => ({ index: item.index, outcome: item.outcome })),
    };
  }

  const errors = items.filter((item) => item.outcome === "error");
  if (errors.length > 0) {
    const firstError = [...errors].sort((left, right) => left.bodySettlementStep - right.bodySettlementStep)[0];
    const primary = testCase.ordering === "input"
      ? [...errors].sort((left, right) => left.index - right.index)[0]
      : firstError;
    return {
      ...base,
      code: "map-error",
      primaryErrorIndex: primary.index,
      cancellationRequestedAt: firstError.bodySettlementStep,
      result: null,
    };
  }
  if (items.some((item) => item.outcome === "canceled")) return { ...base, code: "map-canceled", result: null };
  return {
    ...base,
    code: "map-success",
    result: orderedItems(testCase).map((item) => item.value),
  };
}

export function validate(data) {
  const errors = [];
  if (data?.$schema !== "w-tgm0-task-group-map-collect-cases-1") errors.push("TGM0 schema is invalid");
  if (!Array.isArray(data?.cases)) return { errors: [...errors, "TGM0 cases are missing"], results: [] };
  const ids = new Set();
  const booleans = [
    "domainExplicit", "parallelCapability", "stagesInputOnce", "stagesCallableOnce",
    "admittedItemsMoveOnce", "remainingInputsDropOnce", "resultStorageReservedBeforeChildren",
    "returnAfterDrain", "collectCancelsOnApplicationError", "collectKeepsInputIndex",
    "implicitUnbounded", "parentCanceled", "faulted",
  ];
  for (const [caseIndex, testCase] of data.cases.entries()) {
    if (!testCase || typeof testCase !== "object") { errors.push(`cases[${caseIndex}] is invalid`); continue; }
    if (typeof testCase.id !== "string" || testCase.id === "") errors.push(`cases[${caseIndex}].id is invalid`);
    else if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}`);
    else ids.add(testCase.id);
    if (Object.prototype.hasOwnProperty.call(testCase, "expected")) errors.push(`${testCase.id}.expected is caller-owned`);
    if (!["concurrent", "parallel"].includes(testCase.family)) errors.push(`${testCase.id}.family is invalid`);
    if (!["map", "collect"].includes(testCase.operation)) errors.push(`${testCase.id}.operation is invalid`);
    if (!["input", "completion"].includes(testCase.ordering)) errors.push(`${testCase.id}.ordering is invalid`);
    if (!["some", "mut", "take"].includes(testCase.callableMode)) errors.push(`${testCase.id}.callableMode is invalid`);
    if (!Array.isArray(testCase.callLabels)) errors.push(`${testCase.id}.callLabels is invalid`);
    if (!Number.isInteger(testCase.inputCount) || testCase.inputCount < 0) errors.push(`${testCase.id}.inputCount is invalid`);
    if (!Number.isInteger(testCase.limit)) errors.push(`${testCase.id}.limit is invalid`);
    if (!Number.isInteger(testCase.maxActiveChildren) || testCase.maxActiveChildren < 0) errors.push(`${testCase.id}.maxActiveChildren is invalid`);
    for (const field of booleans) if (typeof testCase[field] !== "boolean") errors.push(`${testCase.id}.${field} is invalid`);
    if (!Array.isArray(testCase.items)) { errors.push(`${testCase.id}.items is invalid`); continue; }
    const indices = new Set();
    const steps = new Set();
    for (const [itemIndex, item] of testCase.items.entries()) {
      const label = `${testCase.id}.items[${itemIndex}]`;
      if (!item || typeof item !== "object") { errors.push(`${label} is invalid`); continue; }
      if (!Number.isInteger(item.index) || item.index < 0 || item.index >= testCase.inputCount) errors.push(`${label}.index is invalid`);
      else if (indices.has(item.index)) errors.push(`${testCase.id} duplicates input index ${item.index}`);
      else indices.add(item.index);
      if (!Number.isInteger(item.bodySettlementStep) || item.bodySettlementStep < 0) errors.push(`${label}.bodySettlementStep is invalid`);
      else if (steps.has(item.bodySettlementStep)) errors.push(`${testCase.id} does not linearize body settlements`);
      else steps.add(item.bodySettlementStep);
      if (!Number.isInteger(item.cleanupCompleteStep) || item.cleanupCompleteStep < item.bodySettlementStep) errors.push(`${label}.cleanupCompleteStep is invalid`);
      if (!["success", "error", "canceled"].includes(item.outcome)) errors.push(`${label}.outcome is invalid`);
      if (typeof item.effectCommitted !== "boolean") errors.push(`${label}.effectCommitted is invalid`);
      if (item.outcome === "success" && !Object.prototype.hasOwnProperty.call(item, "value")) errors.push(`${label}.value is missing`);
    }
    if (!testCase.parentCanceled && !testCase.faulted && testCase.items.length !== testCase.inputCount) errors.push(`${testCase.id} does not settle every input`);
  }
  return { errors, results: data.cases.map(evaluateCase) };
}
