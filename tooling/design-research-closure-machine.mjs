import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { deriveExecutionErgonomics, summarizeDiagnostics } from "./execution-ergonomics-machine.mjs";
import { loadData as loadMem0, validate as validateMem0 } from "./studies/mem0-virtual-memory-data-movement/oracle.mjs";
import { loadData as loadSea0, validate as validateSea0 } from "./studies/sea0-simulated-effects-approval/oracle.mjs";
import { loadData as loadLlm0, validate as validateLlm0 } from "./studies/llm0-training-inference/oracle.mjs";

const directory = path.dirname(fileURLToPath(import.meta.url));
export const decisions = Object.freeze(["W-1471", "W-1473", "W-1474", "W-1475"]);

export function loadCorpus() {
  return JSON.parse(fs.readFileSync(path.join(directory, "design-research-closure-cases.json"), "utf8"));
}

function validateSync0() {
  const accepted = deriveExecutionErgonomics("async fn func(): Value { await Task.yield(); return value }\nlet y = sync func()");
  const inferred = deriveExecutionErgonomics("fn inferredMay(): Value { await Task.yield(); return value }\nlet y = sync inferredMay()");
  const ordinary = deriveExecutionErgonomics("fn ordinary(): Value { return value }\nlet y = sync ordinary()");
  const never = deriveExecutionErgonomics("fn neverSuspend(): Value { return value }\nlet y = sync neverSuspend()");
  const bare = deriveExecutionErgonomics("async fn func(): Value { await Task.yield(); return value }\nlet y = func()");
  const bridge = accepted.suspension.syncBridges.find((item) => item.callee === "func");
  return {
    valid: bridge?.eligible === true && bridge?.blocksThread === true && bridge?.sourceSpelling === "explicit" &&
      [inferred, ordinary, never].every((result) => summarizeDiagnostics(result).includes("W-SUSPEND-0005")) &&
      summarizeDiagnostics(bare).includes("W-SUSPEND-0001"),
    facts: {
      explicitAsyncAccepted: bridge?.eligible === true,
      inferredRejected: summarizeDiagnostics(inferred).includes("W-SUSPEND-0005"),
      ordinaryRejected: summarizeDiagnostics(ordinary).includes("W-SUSPEND-0005"),
      neverSuspendRejected: summarizeDiagnostics(never).includes("W-SUSPEND-0005"),
      bareMaySuspendRejected: summarizeDiagnostics(bare).includes("W-SUSPEND-0001"),
    },
  };
}

export function deriveClosure(corpus = loadCorpus()) {
  const sync0 = validateSync0();
  const mem0 = validateMem0(loadMem0());
  const sea0 = validateSea0(loadSea0());
  const llm0 = validateLlm0(loadLlm0());
  const studyFacts = {
    SYNC0: sync0,
    MEM0: { valid: mem0.errors.length === 0, facts: { mechanisms: mem0.mechanisms.length } },
    SEA0: { valid: sea0.errors.length === 0, facts: { proposals: sea0.proposals.size, cases: sea0.cases.length } },
    LLM0: { valid: llm0.errors.length === 0, facts: { gaps: llm0.gaps.length, workloads: llm0.workloads.length } },
  };
  const errors = [];
  if (corpus.$schema !== "w-design-research-closure-cases-1" || corpus.id !== "DRC0" || corpus.status !== "design-oracle-input") errors.push("DRC0 identity is invalid");
  if (JSON.stringify(corpus.decisions) !== JSON.stringify(decisions)) errors.push("DRC0 decision set is invalid");
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
  return { errors, studyFacts, researchGates: errors.length === 0 ? [] : decisions };
}
