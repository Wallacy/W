import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const directory = path.dirname(fileURLToPath(import.meta.url));
export const states = ["proposed", "simulated", "awaitingApproval", "revalidating", "committing", "committed", "rejected", "conflict", "unknown"];
const terminal = new Set(["committed", "rejected", "conflict", "unknown"]);
export function loadData() { return JSON.parse(fs.readFileSync(path.join(directory, "cases.json"), "utf8")); }
export function validate(data = loadData()) {
  const errors = [];
  if (data.study !== "SEA0" || data.machine?.productionModel !== data.machine?.testDoubleModel) errors.push("production and test double do not share the machine");
  if (data.maxDagNodes !== 32 || !data.machine?.deterministicScheduler || !data.machine?.virtualClock || !data.machine?.deterministicRng) errors.push("deterministic bounded test machine is incomplete");
  const proposals = new Map();
  const required = ["effectId", "inputDigest", "authority", "capability", "provider", "providerGeneration", "simulatedResultDigest", "dependencies", "approvalGroup", "limits", "expiry"];
  for (const proposal of data.proposals ?? []) {
    if (proposals.has(proposal.effectId)) errors.push(`duplicate proposal ${proposal.effectId}`);
    proposals.set(proposal.effectId, proposal);
    for (const field of required) if (proposal[field] === undefined) errors.push(`${proposal.effectId} omits ${field}`);
    if (proposal.simulatedValue === proposal.committedValue && proposal.committedValue === undefined) errors.push(`${proposal.effectId} does not separate value carriers`);
    for (const dependency of proposal.dependencies ?? []) if (!proposals.has(dependency) && !data.proposals.some((candidate) => candidate.effectId === dependency)) errors.push(`${proposal.effectId} has unknown dependency ${dependency}`);
  }
  for (const item of data.cases ?? []) {
    if (item.trace) {
      for (const state of item.trace) if (!states.includes(state)) errors.push(`${item.id} has invalid state ${state}`);
      if (item.trace.length && !["proposed", "rejected"].includes(item.trace[0])) errors.push(`${item.id} must begin proposed or rejected`);
      if (item.trace.length && !terminal.has(item.trace.at(-1))) errors.push(`${item.id} must end in a terminal outcome`);
      for (let index = 1; index < item.trace.length; index++) {
        const previous = item.trace[index - 1];
        const current = item.trace[index];
        if (previous === "proposed" && current !== "simulated") errors.push(`${item.id} skips simulation`);
        if (previous === "simulated" && current !== "awaitingApproval") errors.push(`${item.id} skips approval wait`);
        if (previous === "awaitingApproval" && !["revalidating", "rejected"].includes(current)) errors.push(`${item.id} has invalid approval transition`);
        if (previous === "revalidating" && !["committing", "conflict", "rejected"].includes(current)) errors.push(`${item.id} has invalid revalidation transition`);
        if (previous === "committing" && !["committed", "unknown"].includes(current)) errors.push(`${item.id} has invalid commit transition`);
      }
    }
    if (item.dependencyCount > data.maxDagNodes && item.expectedOutcome !== "rejected") errors.push(`${item.id} exceeds DAG bound`);
    if (item.dispatch?.startsWith("unknownOutcome(") && item.trace?.at(-1) !== "unknown") errors.push(`${item.id} must preserve unknownOutcome`);
    if (item.approvedInputDigest && item.effectId && item.approvedInputDigest !== proposals.get(item.effectId)?.inputDigest && item.expectedOutcome !== "rejected") errors.push(`${item.id} dispatches input different from approved input`);
    if (item.receiptBindsExactSet !== undefined && item.receiptBindsExactSet !== true) errors.push(`${item.id} bulk receipt is not exact-set bound`);
  }
  const deny = data.cases.find((item) => item.id === "SEA0-causal-deny");
  if (!deny || deny.dependentTrace?.at(-1) !== "rejected") errors.push("transitive dependent invalidation is missing");
  const partial = data.cases.find((item) => item.id === "SEA0-bulk-topological-partial");
  if (!partial || !Array.isArray(partial.approvalOrder) || partial.approvalOrder[0] !== "fx-read-menu" || !partial.approved.includes("fx-independent-metrics")) errors.push("bulk approval is not topological and independent-aware");
  for (const fault of data.faultCases ?? []) if (!fault) errors.push("empty fault case");
  for (const coverage of ["state-model", "property", "fuzz", "differential", "metamorphic"]) if (!data.coverageCases?.includes(coverage)) errors.push(`missing ${coverage} coverage lane`);
  return { errors, proposals, cases: data.cases };
}
