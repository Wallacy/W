import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const directory = path.dirname(fileURLToPath(import.meta.url));

export function loadCases() {
  return JSON.parse(fs.readFileSync(path.join(directory, "cases.json"), "utf8"));
}

function rejected(testCase, code) {
  return { id: testCase.id, status: "rejected", code };
}

export function evaluateCase(testCase) {
  if (testCase.consumesHandles !== true) return rejected(testCase, "taskHandlesMustMove");
  if (testCase.commonTaskType !== true) return rejected(testCase, "candidateTaskTypesMustMatch");
  if (testCase.createsHiddenTask === true) return rejected(testCase, "hiddenBranchesRejected");
  if (testCase.selectionPolicy !== "first-settled") return rejected(testCase, "completionOrderRequired");
  if (testCase.returnAfterDrain !== true) return rejected(testCase, "losersMustDrain");
  if (testCase.firstSuccess === true) return rejected(testCase, "firstSuccessNotImplied");
  if (testCase.rollbackClaim === true) return rejected(testCase, "cancellationIsNotRollback");
  if (testCase.persistentMultiplex === true) return rejected(testCase, "oneShotSelectionOnly");
  if (testCase.parentCanceled === true && testCase.returnsSettlementOnParentCancel === true) {
    return rejected(testCase, "parentCancellationIsControl");
  }

  const tasks = testCase.tasks ?? [];
  const handles = tasks.map((task) => task.handle);
  if (new Set(handles).size !== handles.length) return rejected(testCase, "duplicateTaskHandle");
  if (testCase.parentCanceled === true) {
    return { id: testCase.id, status: "accepted", code: "parent-canceled", winner: null, publicationStep: Math.max(0, ...tasks.map((task) => task.cleanupCompleteStep)) };
  }
  if (tasks.length === 0) return { id: testCase.id, status: "accepted", code: "empty", winner: null, publicationStep: 0 };

  const settledAtArm = tasks.filter((task) => task.stateAtArm === "settled");
  const winner = settledAtArm.length > 0
    ? [...settledAtArm].sort((left, right) => left.index - right.index)[0]
    : [...tasks].sort((left, right) => left.settlementStep - right.settlementStep)[0];
  return {
    id: testCase.id,
    status: "accepted",
    code: "first-settled",
    winner: { index: winner.index, outcome: winner.outcome },
    publicationStep: Math.max(winner.settlementStep, ...tasks.map((task) => task.cleanupCompleteStep)),
    committedEffectsRemain: tasks.filter((task) => task.effectCommitted === true).map((task) => task.index),
  };
}

export function validate(data) {
  const errors = [];
  if (data?.$schema !== "w-fst0-first-settled-task-cases-1") errors.push("FST0 schema is invalid");
  if (!Array.isArray(data?.cases)) return { errors: [...errors, "FST0 cases are missing"], results: [] };
  const booleanFields = [
    "consumesHandles", "commonTaskType", "createsHiddenTask", "returnAfterDrain",
    "firstSuccess", "rollbackClaim", "persistentMultiplex", "parentCanceled",
    "returnsSettlementOnParentCancel",
  ];
  const ids = new Set();
  for (const [index, testCase] of data.cases.entries()) {
    if (!testCase || typeof testCase !== "object") { errors.push(`cases[${index}] is invalid`); continue; }
    if (typeof testCase.id !== "string" || testCase.id === "") errors.push(`cases[${index}].id is invalid`);
    else if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}`);
    else ids.add(testCase.id);
    for (const field of booleanFields) {
      if (typeof testCase[field] !== "boolean") errors.push(`${testCase.id}.${field} is invalid`);
    }
    if (typeof testCase.selectionPolicy !== "string") errors.push(`${testCase.id}.selectionPolicy is invalid`);
    if (!Array.isArray(testCase.tasks)) {
      errors.push(`${testCase.id}.tasks is invalid`);
      continue;
    }
    const pendingSteps = new Set();
    for (const [taskPosition, task] of testCase.tasks.entries()) {
      const label = `${testCase.id}.tasks[${taskPosition}]`;
      if (!task || typeof task !== "object") { errors.push(`${label} is invalid`); continue; }
      if (task.index !== taskPosition) errors.push(`${label}.index must equal its input position`);
      if (typeof task.handle !== "string" || task.handle === "") errors.push(`${label}.handle is invalid`);
      if (!new Set(["pending", "settled"]).has(task.stateAtArm)) errors.push(`${label}.stateAtArm is invalid`);
      if (!Number.isInteger(task.settlementStep) || task.settlementStep < 0) errors.push(`${label}.settlementStep is invalid`);
      if (!new Set(["success", "error", "canceled"]).has(task.outcome)) errors.push(`${label}.outcome is invalid`);
      if (!Number.isInteger(task.cleanupCompleteStep) || task.cleanupCompleteStep < task.settlementStep) errors.push(`${label}.cleanupCompleteStep is invalid`);
      if (typeof task.effectCommitted !== "boolean") errors.push(`${label}.effectCommitted is invalid`);
      if (task.stateAtArm === "pending" && Number.isInteger(task.settlementStep)) {
        if (pendingSteps.has(task.settlementStep)) errors.push(`${testCase.id} does not linearize pending settlement commits`);
        pendingSteps.add(task.settlementStep);
      }
    }
  }
  return { errors, results: data.cases.map(evaluateCase) };
}
