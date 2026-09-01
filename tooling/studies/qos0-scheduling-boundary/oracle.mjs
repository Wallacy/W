import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const directory = path.dirname(fileURLToPath(import.meta.url));
const sourceSurfaces = new Set([
  "none", "spawn", "async", "pipelineTasks", "function", "Task",
  "serviceCall", "entry", "service", "serviceDescriptor", "executionProfile",
]);
const prioritySemanticRoles = new Set(["none", "correctness", "deadline", "authority"]);
const policySupport = new Set(["supported", "unsupported", "host-defined"]);
const domainPlacements = new Set(["current", "shared", "dedicated"]);
const allergyTerminals = new Set(["success", "deadlineCancellation", "admissionRejection"]);
const invariantCodes = new Map([
  ["outcomeRulesPreserved", "outcomeRulesMustBePreserved"],
  ["ownerDropRulesPreserved", "ownerDropRulesMustBePreserved"],
  ["derivedDecisionRulesPreserved", "derivedDecisionRulesMustBePreserved"],
  ["fixedLogicalTracePreserved", "fixedLogicalTraceMustBeStable"],
  ["authorityPreserved", "authorityMustNotChange"],
  ["isolationPreserved", "isolationMustNotChange"],
  ["affinityPreserved", "affinityMustNotChange"],
  ["capabilityPreserved", "capabilityMustNotChange"],
  ["admissionRulesPreserved", "admissionRulesMustBePreserved"],
  ["arbitrationRulesPreserved", "arbitrationRulesMustBePreserved"],
  ["capacityPreserved", "capacityMustNotChange"],
  ["budgetRulesPreserved", "budgetRulesMustBePreserved"],
  ["serialDomainFirstStartOrderPreserved", "serialDomainGuaranteedOrderMustBePreserved"],
  ["barrierTicketOrderPreserved", "barrierGuaranteedOrderMustBePreserved"],
  ["channelGuaranteedOrderPreserved", "channelGuaranteedOrderMustBePreserved"],
  ["serviceGuaranteedOrderPreserved", "serviceGuaranteedOrderMustBePreserved"],
  ["unspecifiedOrderVariationAllowed", "unspecifiedOrderMustRemainUnconstrained"],
  ["structuredDrainPreserved", "structuredDrainMustNotChange"],
  ["livenessPreserved", "profileLivenessMustNotChange"],
  ["fairnessPreserved", "profileFairnessMustNotChange"],
  ["cancellationRulesPreserved", "cancellationRulesMustBePreserved"],
  ["deadlineRulesPreserved", "deadlineRulesMustBePreserved"],
]);
const reopenContracts = [
  "inversionContract", "donationContract", "starvationContract",
  "cancellationContract", "deadlineContract", "admissionContract",
  "faultContract", "livenessContract",
];

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

function evaluateReopen(testCase) {
  if (testCase.reopenRequest !== true || testCase.boundedLastLightWorkload !== true) {
    return rejected(testCase, "boundedLastLightWorkloadRequired");
  }
  if (testCase.materialLoss !== true || testCase.currentMechanismsInsufficient !== true) {
    return rejected(testCase, "materialUnexpressedLossRequired");
  }
  if (testCase.crossTargetEvidence !== true) {
    return rejected(testCase, "crossTargetEvidenceRequired");
  }
  if (reopenContracts.some((field) => testCase[field] !== true)) {
    return rejected(testCase, "completePriorityContractRequired");
  }
  if (testCase.humanStudy !== true || testCase.modelStudy !== true) {
    return rejected(testCase, "humanModelStudiesRequired");
  }
  return { id: testCase.id, status: "eligible", code: "researchReopenOnly" };
}

export function evaluateCase(testCase) {
  if (testCase.operation === "reopen") return evaluateReopen(testCase);
  if (testCase.sourcePrioritySlot === true) return rejected(testCase, "portablePrioritySlotRejected");
  if (testCase.sourceQosSlot === true) return rejected(testCase, "portableQosSlotRejected");
  if (testCase.standardPriorityApi !== "absent") return rejected(testCase, "portableStandardApiRejected");
  if (testCase.priorityInheritance === true) return rejected(testCase, "priorityInheritanceRejected");
  if (testCase.priorityEscalation === true) return rejected(testCase, "priorityEscalationRejected");
  if (testCase.priorityDonation === true) return rejected(testCase, "priorityDonationRejected");
  if (testCase.prioritySemanticRole !== "none") return rejected(testCase, "prioritySemanticRoleRejected");
  if (testCase.receiptSourcePriority !== "absent") return rejected(testCase, "sourcePriorityMustBeAbsent");
  if (testCase.receiptBranchable === true) return rejected(testCase, "receiptMustNotBeBranchable");
  for (const [field, code] of invariantCodes) {
    if (testCase[field] !== true) return rejected(testCase, code);
  }
  if (testCase.deliveryGuaranteed === true) return rejected(testCase, "deliveryGuaranteeRejected");

  const traceDependentVariation = [
    "unspecifiedOrderMayDiffer", "deadlineObservationMayDiffer", "admissionResultMayDiffer",
    "firstSettledWinnerMayDiffer", "permittedOutcomeMayDiffer",
  ];
  if (traceDependentVariation.some((field) => testCase[field] === true)
    && testCase.logicalTraceDiffers !== true) {
    return rejected(testCase, "logicalTraceDifferenceRequired");
  }

  if (testCase.allergyOrder === true) {
    if (testCase.allergyUsesPriority === true) return rejected(testCase, "allergyPriorityRejected");
    if (testCase.allergyDeadline !== true) return rejected(testCase, "allergyDeadlineRequired");
    if (testCase.allergyDedicatedService !== true) return rejected(testCase, "allergyDedicatedServiceRequired");
    if (testCase.reliesOnDomainForSafety === true) return rejected(testCase, "allergyDomainSafetyRejected");
    if (testCase.allergyDedicatedAdmission !== true) return rejected(testCase, "allergyDedicatedAdmissionRequired");
    if (testCase.allergyReservationBudgeted !== true) return rejected(testCase, "allergyReservationBudgetRequired");
    if (testCase.singleCpu !== true || testCase.adversarialScheduling !== true) {
      return rejected(testCase, "allergyAdversarialSingleCpuRequired");
    }
    if (testCase.allergySuccessAfterSafeValidation !== true) return rejected(testCase, "allergySafeValidationRequired");
    if (testCase.allergySafeCancellationOrRejection !== true) return rejected(testCase, "allergySafeTerminalRequired");
    if (testCase.allergyNoUnsafeFulfillment !== true) return rejected(testCase, "allergyUnsafeFulfillmentRejected");
    if (testCase.allergyNoPartialCommit !== true) return rejected(testCase, "allergyPartialCommitRejected");
    if (testCase.allergyOrderingRulesPreserved !== true) return rejected(testCase, "allergyOrderingRulesRequired");
    if (testCase.allergyTerminalDrainPreserved !== true) return rejected(testCase, "allergyTerminalDrainRequired");
    if (testCase.allergyProgressUnderPreconditions !== true) return rejected(testCase, "allergyConditionalProgressRequired");
  }

  const allergyCode = testCase.allergyObservedTerminal === "success"
    ? "allergySafeSuccess"
    : "allergySafeCancellationOrRejection";
  return {
    id: testCase.id,
    status: "accepted",
    code: testCase.allergyOrder ? allergyCode : "physicalPolicyOnly",
    receipt: {
      physicalPolicy: testCase.physicalPolicy,
      support: testCase.policySupport,
      sourcePriority: "absent",
      branchable: false,
    },
    allowedVariation: {
      latency: testCase.latencyMayDiffer,
      unspecifiedOrder: testCase.unspecifiedOrderMayDiffer,
      deadlineObservation: testCase.deadlineObservationMayDiffer,
      admissionResult: testCase.admissionResultMayDiffer,
      firstSettledWinner: testCase.firstSettledWinnerMayDiffer,
      permittedOutcome: testCase.permittedOutcomeMayDiffer,
    },
    logicalTrace: {
      differs: testCase.logicalTraceDiffers,
      fixedTracePreserved: testCase.fixedLogicalTracePreserved,
    },
  };
}

export function validate(data) {
  const errors = [];
  if (data?.$schema !== "w-qos0-scheduling-boundary-cases-1") errors.push("QOS0 schema is invalid");
  if (!Array.isArray(data?.cases)) return { errors: [...errors, "QOS0 cases are missing"], results: [] };

  const ids = new Set();
  const booleanFields = [
    "sourcePrioritySlot", "sourceQosSlot", "priorityInheritance",
    "priorityEscalation", "priorityDonation", "receiptBranchable",
    "latencyMayDiffer", "unspecifiedOrderMayDiffer",
    "logicalTraceDiffers", "deadlineObservationMayDiffer",
    "admissionResultMayDiffer", "firstSettledWinnerMayDiffer",
    "permittedOutcomeMayDiffer", ...invariantCodes.keys(), "deliveryGuaranteed",
    "allergyOrder", "allergyUsesPriority", "allergyDeadline",
    "allergyDedicatedService", "allergyDedicatedAdmission",
    "allergyReservationBudgeted", "reliesOnDomainForSafety",
    "singleCpu", "adversarialScheduling", "allergySuccessAfterSafeValidation",
    "allergySafeCancellationOrRejection", "allergyNoUnsafeFulfillment",
    "allergyNoPartialCommit", "allergyOrderingRulesPreserved",
    "allergyTerminalDrainPreserved", "allergyProgressUnderPreconditions",
    "reopenRequest",
    "boundedLastLightWorkload", "materialLoss", "currentMechanismsInsufficient",
    "crossTargetEvidence", ...reopenContracts, "humanStudy", "modelStudy",
  ];

  for (const [index, testCase] of data.cases.entries()) {
    if (!testCase || typeof testCase !== "object") {
      errors.push(`cases[${index}] is invalid`);
      continue;
    }
    if (typeof testCase.id !== "string" || testCase.id === "") errors.push(`cases[${index}].id is invalid`);
    else if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}`);
    else ids.add(testCase.id);
    if (Object.prototype.hasOwnProperty.call(testCase, "expected")) errors.push(`${testCase.id}.expected is caller-owned`);
    if (!new Set(["current", "reopen"]).has(testCase.operation)) errors.push(`${testCase.id}.operation is invalid`);
    if (!sourceSurfaces.has(testCase.sourceSurface)) errors.push(`${testCase.id}.sourceSurface is invalid`);
    if (!prioritySemanticRoles.has(testCase.prioritySemanticRole)) errors.push(`${testCase.id}.prioritySemanticRole is invalid`);
    if (typeof testCase.standardPriorityApi !== "string") errors.push(`${testCase.id}.standardPriorityApi is invalid`);
    if (typeof testCase.physicalPolicy !== "string" || testCase.physicalPolicy === "") errors.push(`${testCase.id}.physicalPolicy is invalid`);
    if (!policySupport.has(testCase.policySupport)) errors.push(`${testCase.id}.policySupport is invalid`);
    if (typeof testCase.receiptSourcePriority !== "string") errors.push(`${testCase.id}.receiptSourcePriority is invalid`);
    if (!domainPlacements.has(testCase.allergyDomainPlacement)) errors.push(`${testCase.id}.allergyDomainPlacement is invalid`);
    if (!allergyTerminals.has(testCase.allergyObservedTerminal)) errors.push(`${testCase.id}.allergyObservedTerminal is invalid`);
    for (const field of booleanFields) {
      if (typeof testCase[field] !== "boolean") errors.push(`${testCase.id}.${field} is invalid`);
    }
  }

  return { errors, results: data.cases.map(evaluateCase) };
}
