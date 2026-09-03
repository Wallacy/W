import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { deriveExecutionErgonomics, summarizeDiagnostics } from "./execution-ergonomics-machine.mjs";
import { loadData as loadMem0, validate as validateMem0 } from "./studies/mem0-virtual-memory-data-movement/oracle.mjs";
import { loadData as loadSea0, validate as validateSea0 } from "./studies/sea0-simulated-effects-approval/oracle.mjs";
import { loadData as loadLlm0, validate as validateLlm0 } from "./studies/llm0-training-inference/oracle.mjs";

const directory = path.dirname(fileURLToPath(import.meta.url));
export const decisions = Object.freeze(["W-1484", "W-1473", "W-1474", "W-1475"]);
export const activeResearchGates = Object.freeze(["W-1486", "W-1503"]);
export const historicalResearchZeroThrough = "W-1459";

export function loadCorpus() {
  return JSON.parse(fs.readFileSync(path.join(directory, "design-research-closure-cases.json"), "utf8"));
}

function validateSync1() {
  const accepted = deriveExecutionErgonomics("async fn func(): Value throws String { return value }\nlet y = try sync func()");
  const suspending = deriveExecutionErgonomics("async fn func(): Value { await execution#yield(); return value }\nlet y = sync func()");
  const dynamicPath = deriveExecutionErgonomics("async fn cached(_ hit: Bool): Value { if hit { return value }; return await catalog() }\nlet y = sync cached(true)", {
    functionTypes: [{ name: "catalog", suspension: "may", sourceSpelling: "explicit", directEntry: "absent" }],
  });
  const inferred = deriveExecutionErgonomics("fn inferredMay(): Value { await execution#yield(); return value }\nlet y = sync inferredMay()");
  const ordinary = deriveExecutionErgonomics("fn ordinary(): Value { return value }\nlet y = sync ordinary()");
  const bare = deriveExecutionErgonomics("async fn func(): Value { await execution#yield(); return value }\nlet y = func()");
  const protocol = deriveExecutionErgonomics("protocol Loader { async fn load(): Value }\nlet y = sync load()");
  const foreign = deriveExecutionErgonomics("foreign c { async fn load(): Value }\nlet y = sync load()");
  const indirect = deriveExecutionErgonomics("let y = sync worker()", {
    functionTypes: [{ name: "worker", suspension: "may", sourceSpelling: "explicit", directEntry: "available" }],
  });
  const erased = deriveExecutionErgonomics("let y = sync worker()", {
    functionTypes: [{ name: "worker", suspension: "may", sourceSpelling: "explicit", directEntry: "absent" }],
  });
  const composed = deriveExecutionErgonomics("async fn leaf(): Value { return value }\nasync fn wrapper(): Value { return sync leaf() }\nlet y = sync wrapper()");
  const transitiveLoss = deriveExecutionErgonomics("async fn leaf(): Value { return await catalog() }\nasync fn wrapper(): Value { return sync leaf() }\nlet y = sync wrapper()", {
    functionTypes: [{ name: "catalog", suspension: "may", sourceSpelling: "explicit", directEntry: "absent" }],
  });
  const invalidSyncCaller = deriveExecutionErgonomics("fn ordinary(): Value { return value }\nasync fn wrapper(): Value { return sync ordinary() }\nlet y = sync wrapper()");
  const recursive = deriveExecutionErgonomics("async fn even(_ n: usize): Bool { return if n == 0 { true } else { sync odd(n - 1) } }\nasync fn odd(_ n: usize): Bool { return if n == 0 { false } else { sync even(n - 1) } }\nlet y = sync even(2)");
  const publicFacet = deriveExecutionErgonomics("export async fn published(): Value { return value }", {
    publicContract: {
      previous: "may",
      current: "may",
      previousDirectEntry: "available",
      currentDirectEntry: "absent",
      exported: true,
    },
  });
  const direct = accepted.suspension.syncCalls.find((item) => item.callee === "func");
  const composedCall = composed.suspension.syncCalls.find((item) => item.callee === "wrapper");
  const recursiveComponent = recursive.suspension.directEntryScc.find((item) =>
    item.members.includes("even") && item.members.includes("odd"));
  const rejected = [suspending, dynamicPath, inferred, ordinary, protocol, foreign, erased];
  return {
    valid: direct?.eligible === true
      && direct?.directEntry === "available"
      && direct?.blocksThread === false
      && direct?.createsTask === false
      && direct?.suspendsTask === false
      && direct?.sameTask === true
      && direct?.sameContext === true
      && direct?.sameDomain === true
      && direct?.runtimeFallback === false
      && direct?.publishedSuspension === "may"
      && direct?.selectedEntrySuspension === "never"
      && accepted.suspension.tryOrthogonal === true
      && rejected.every((result) => summarizeDiagnostics(result).includes("W-SUSPEND-0005"))
      && indirect.suspension.syncCalls[0]?.eligible === true
      && composedCall?.eligible === true
      && composedCall?.publishedSuspension === "may"
      && composedCall?.selectedEntrySuspension === "never"
      && summarizeDiagnostics(transitiveLoss).includes("W-SUSPEND-0005")
      && invalidSyncCaller.suspension.declarations.find((item) => item.name === "wrapper")?.directEntry === "absent"
      && recursiveComponent?.directEntry === "available"
      && recursiveComponent?.terminationProven === false
      && recursiveComponent?.evaluationPerformed === false
      && publicFacet.suspension.public?.sourceBreaking === true
      && publicFacet.suspension.public?.semanticInterfaceKeyChanged === true
      && summarizeDiagnostics(bare).includes("W-SUSPEND-0001"),
    facts: {
      explicitNeverSuspendAccepted: direct?.eligible === true,
      blocksThread: direct?.blocksThread,
      createsTask: direct?.createsTask,
      suspendsTask: direct?.suspendsTask,
      sameExecutionContext: direct?.sameTask === true && direct?.sameContext === true && direct?.sameDomain === true,
      runtimeFallback: direct?.runtimeFallback,
      asyncEntryPublishesMay: direct?.publishedSuspension === "may",
      selectedDirectEntryNeverSuspend: direct?.selectedEntrySuspension === "never",
      tryOrthogonal: accepted.suspension.tryOrthogonal,
      explicitAwaitRejected: summarizeDiagnostics(suspending).includes("W-SUSPEND-0005"),
      dynamicPathRejected: summarizeDiagnostics(dynamicPath).includes("W-SUSPEND-0005"),
      inferredRejected: summarizeDiagnostics(inferred).includes("W-SUSPEND-0005"),
      ordinaryRejected: summarizeDiagnostics(ordinary).includes("W-SUSPEND-0005"),
      protocolBodylessRejected: summarizeDiagnostics(protocol).includes("W-SUSPEND-0005"),
      foreignBodylessRejected: summarizeDiagnostics(foreign).includes("W-SUSPEND-0005"),
      indirectFacetAccepted: indirect.suspension.syncCalls[0]?.eligible === true,
      composedDirectEntryAccepted: composedCall?.eligible === true,
      transitiveFacetLossRejected: summarizeDiagnostics(transitiveLoss).includes("W-SUSPEND-0005"),
      invalidSyncPoisonsCaller: invalidSyncCaller.suspension.declarations.find((item) => item.name === "wrapper")?.directEntry === "absent",
      syncSccEligible: recursiveComponent?.directEntry === "available",
      syncSccTerminationUnproven: recursiveComponent?.terminationProven === false,
      syncSccNotExecuted: recursiveComponent?.evaluationPerformed === false,
      erasedFacetRejected: summarizeDiagnostics(erased).includes("W-SUSPEND-0005"),
      facetRemovalSourceBreaking: publicFacet.suspension.public?.sourceBreaking === true,
      semanticInterfaceKeyChanged: publicFacet.suspension.public?.semanticInterfaceKeyChanged === true,
      bareMaySuspendRejected: summarizeDiagnostics(bare).includes("W-SUSPEND-0001"),
    },
  };
}

export function deriveClosure(corpus = loadCorpus()) {
  const sync1 = validateSync1();
  const mem0 = validateMem0(loadMem0());
  const sea0 = validateSea0(loadSea0());
  const llm0 = validateLlm0(loadLlm0());
  const studyFacts = {
    SYNC1: sync1,
    MEM0: { valid: mem0.errors.length === 0, facts: { mechanisms: mem0.mechanisms.length } },
    SEA0: { valid: sea0.errors.length === 0, facts: { proposals: sea0.proposals.size, cases: sea0.cases.length } },
    LLM0: { valid: llm0.errors.length === 0, facts: { gaps: llm0.gaps.length, workloads: llm0.workloads.length } },
  };
  const errors = [];
  if (corpus.$schema !== "w-design-research-closure-cases-1" || corpus.id !== "DRC0" || corpus.status !== "design-oracle-input") errors.push("DRC0 identity is invalid");
  if (JSON.stringify(corpus.decisions) !== JSON.stringify(decisions)) errors.push("DRC0 decision set is invalid");
  if (JSON.stringify(corpus.activeResearchGates) !== JSON.stringify(activeResearchGates)) errors.push("DRC0 active research gate set is invalid");
  if (corpus.historicalResearchZeroThrough !== historicalResearchZeroThrough) errors.push("DRC0 historical Research=0 boundary is invalid");
  if (!Array.isArray(corpus.cases) || corpus.cases.length !== decisions.length) errors.push("DRC0 requires exactly four current cases");
  const ids = new Set();
  for (const item of corpus.cases ?? []) {
    if (ids.has(item.id)) errors.push(`${item.id}: duplicate case`);
    ids.add(item.id);
    const decision = item.decisions?.[0];
    if (item.id !== `DRC0-${decision}-current` || item.kind !== "current-contract") errors.push(`${item.id}: invalid case identity`);
    if (!decisions.includes(decision)) errors.push(`${item.id}: unknown decision`);
    if (!studyFacts[item.study]?.valid) errors.push(`${item.id}: ${item.study} stop condition is not satisfied`);
    if (typeof item.stopCondition !== "string" || item.stopCondition.length < 40) errors.push(`${item.id}: stop condition is not explicit`);
    if (!Array.isArray(item.missingEvidence) || item.missingEvidence.length === 0) errors.push(`${item.id}: missing evidence boundary is absent`);
    if (Object.hasOwn(item, "implemented") || Object.hasOwn(item, "expected") || Object.hasOwn(item, "result")) errors.push(`${item.id}: caller-owned result claim is forbidden`);
  }
  for (const decision of decisions) if (!ids.has(`DRC0-${decision}-current`)) errors.push(`${decision}: closure case is missing`);
  return {
    errors,
    studyFacts,
    researchGates: errors.length === 0 ? [] : decisions,
    activeResearchGates,
    historicalResearchZeroThrough,
  };
}
