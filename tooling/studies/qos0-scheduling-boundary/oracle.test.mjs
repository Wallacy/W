import { describe, expect, test } from "bun:test";
import { evaluateCase, loadCases, validate } from "./oracle.mjs";

const expected = new Map([
  ["QOS0-current-host-default", ["accepted", "physicalPolicyOnly"]],
  ["QOS0-current-policy-varies", ["accepted", "physicalPolicyOnly"]],
  ["QOS0-current-policy-unsupported", ["accepted", "physicalPolicyOnly"]],
  ["QOS0-current-different-logical-trace", ["accepted", "physicalPolicyOnly"]],
  ["QOS0-current-allergy-single-cpu", ["accepted", "allergySafeSuccess"]],
  ["QOS0-current-allergy-shared-domain", ["accepted", "allergySafeSuccess"]],
  ["QOS0-current-allergy-deadline-cancellation", ["accepted", "allergySafeCancellationOrRejection"]],
  ...[
    "spawn-priority", "async-priority", "task-group-priority",
    "function-priority", "task-priority", "service-call-priority",
    "entry-priority", "service-priority", "service-descriptor-priority",
    "profile-priority",
  ].map((id) => [`QOS0-${id}`, ["rejected", "portablePrioritySlotRejected"]]),
  ...[
    "spawn-qos", "async-qos", "task-group-qos", "function-qos", "task-qos",
    "service-call-qos", "entry-qos", "service-qos", "service-descriptor-qos",
    "profile-qos",
  ]
    .map((id) => [`QOS0-${id}`, ["rejected", "portableQosSlotRejected"]]),
  ...[
    "background-standard", "user-interactive-standard", "current-priority-api",
    "with-priority-api",
  ].map((id) => [`QOS0-${id}`, ["rejected", "portableStandardApiRejected"]]),
  ["QOS0-priority-inheritance", ["rejected", "priorityInheritanceRejected"]],
  ["QOS0-priority-escalation", ["rejected", "priorityEscalationRejected"]],
  ["QOS0-priority-donation", ["rejected", "priorityDonationRejected"]],
  ...["priority-correctness", "priority-deadline", "priority-authority"]
    .map((id) => [`QOS0-${id}`, ["rejected", "prioritySemanticRoleRejected"]]),
  ["QOS0-branchable-receipt", ["rejected", "receiptMustNotBeBranchable"]],
  ["QOS0-source-priority-present", ["rejected", "sourcePriorityMustBeAbsent"]],
  ["QOS0-outcome-fabricated", ["rejected", "outcomeRulesMustBePreserved"]],
  ["QOS0-owner-drop-rule-drift", ["rejected", "ownerDropRulesMustBePreserved"]],
  ["QOS0-derived-decision-rule-drift", ["rejected", "derivedDecisionRulesMustBePreserved"]],
  ["QOS0-fixed-logical-trace-divergence", ["rejected", "fixedLogicalTraceMustBeStable"]],
  ["QOS0-authority-drift", ["rejected", "authorityMustNotChange"]],
  ["QOS0-isolation-drift", ["rejected", "isolationMustNotChange"]],
  ["QOS0-affinity-drift", ["rejected", "affinityMustNotChange"]],
  ["QOS0-capability-drift", ["rejected", "capabilityMustNotChange"]],
  ["QOS0-admission-bypass", ["rejected", "admissionRulesMustBePreserved"]],
  ["QOS0-arbitration-bypass", ["rejected", "arbitrationRulesMustBePreserved"]],
  ["QOS0-capacity-drift", ["rejected", "capacityMustNotChange"]],
  ["QOS0-budget-bypass", ["rejected", "budgetRulesMustBePreserved"]],
  ["QOS0-serial-fifo-drift", ["rejected", "serialDomainGuaranteedOrderMustBePreserved"]],
  ["QOS0-barrier-ticket-drift", ["rejected", "barrierGuaranteedOrderMustBePreserved"]],
  ["QOS0-channel-order-drift", ["rejected", "channelGuaranteedOrderMustBePreserved"]],
  ["QOS0-service-order-drift", ["rejected", "serviceGuaranteedOrderMustBePreserved"]],
  ["QOS0-unspecified-order-frozen", ["rejected", "unspecifiedOrderMustRemainUnconstrained"]],
  ["QOS0-drain-drift", ["rejected", "structuredDrainMustNotChange"]],
  ["QOS0-liveness-drift", ["rejected", "profileLivenessMustNotChange"]],
  ["QOS0-fairness-drift", ["rejected", "profileFairnessMustNotChange"]],
  ["QOS0-cancellation-ignored", ["rejected", "cancellationRulesMustBePreserved"]],
  ["QOS0-deadline-ignored", ["rejected", "deadlineRulesMustBePreserved"]],
  ["QOS0-delivery-guarantee", ["rejected", "deliveryGuaranteeRejected"]],
  ["QOS0-allergy-priority", ["rejected", "allergyPriorityRejected"]],
  ["QOS0-allergy-no-deadline", ["rejected", "allergyDeadlineRequired"]],
  ["QOS0-allergy-no-service", ["rejected", "allergyDedicatedServiceRequired"]],
  ["QOS0-allergy-relies-on-domain-safety", ["rejected", "allergyDomainSafetyRejected"]],
  ["QOS0-allergy-no-admission", ["rejected", "allergyDedicatedAdmissionRequired"]],
  ["QOS0-allergy-no-reservation-budget", ["rejected", "allergyReservationBudgetRequired"]],
  ["QOS0-allergy-unsafe-validation", ["rejected", "allergySafeValidationRequired"]],
  ["QOS0-allergy-unsafe-deadline-terminal", ["rejected", "allergySafeTerminalRequired"]],
  ["QOS0-allergy-unsafe-fulfillment", ["rejected", "allergyUnsafeFulfillmentRejected"]],
  ["QOS0-allergy-partial-commit", ["rejected", "allergyPartialCommitRejected"]],
  ["QOS0-allergy-order-rule-drift", ["rejected", "allergyOrderingRulesRequired"]],
  ["QOS0-allergy-drain-drift", ["rejected", "allergyTerminalDrainRequired"]],
  ["QOS0-allergy-progress-precondition-drift", ["rejected", "allergyConditionalProgressRequired"]],
  ["QOS0-reopen-no-workload", ["rejected", "boundedLastLightWorkloadRequired"]],
  ["QOS0-reopen-no-material-loss", ["rejected", "materialUnexpressedLossRequired"]],
  ["QOS0-reopen-no-cross-target", ["rejected", "crossTargetEvidenceRequired"]],
  ["QOS0-reopen-incomplete-contract", ["rejected", "completePriorityContractRequired"]],
  ["QOS0-reopen-no-human-model", ["rejected", "humanModelStudiesRequired"]],
  ["QOS0-reopen-complete-evidence", ["eligible", "researchReopenOnly"]],
]);

describe("QOS0 physical scheduling boundary", () => {
  test("derives every current, rejected, and reopen route", () => {
    const checked = validate(loadCases());
    expect(checked.errors).toEqual([]);
    expect(checked.results).toHaveLength(expected.size);
    for (const result of checked.results) {
      expect([result.status, result.code]).toEqual(expected.get(result.id));
    }
  });

  test("preserves semantic rules while permitting variation across logical traces", () => {
    const testCase = loadCases().cases.find((item) => item.id === "QOS0-current-policy-varies");
    expect(evaluateCase(testCase)).toMatchObject({
      status: "accepted",
      receipt: { physicalPolicy: "latency-biased", support: "supported", sourcePriority: "absent", branchable: false },
      allowedVariation: {
        latency: true,
        unspecifiedOrder: true,
        deadlineObservation: false,
        admissionResult: false,
        firstSettledWinner: false,
        permittedOutcome: false,
      },
      logicalTrace: { differs: true, fixedTracePreserved: true },
    });
    expect(evaluateCase({ ...testCase, outcomeRulesPreserved: false })).toMatchObject({
      status: "rejected",
      code: "outcomeRulesMustBePreserved",
    });

    const differentTrace = loadCases().cases.find((item) => item.id === "QOS0-current-different-logical-trace");
    expect(evaluateCase(differentTrace)).toMatchObject({
      status: "accepted",
      logicalTrace: { differs: true, fixedTracePreserved: true },
      allowedVariation: {
        deadlineObservation: true,
        admissionResult: true,
        firstSettledWinner: true,
        permittedOutcome: true,
      },
    });
    expect(evaluateCase({ ...differentTrace, logicalTraceDiffers: false })).toMatchObject({
      status: "rejected",
      code: "logicalTraceDifferenceRequired",
    });
    expect(evaluateCase({ ...testCase, logicalTraceDiffers: false })).toMatchObject({
      status: "rejected",
      code: "logicalTraceDifferenceRequired",
    });
    const sameTraceLatency = loadCases().cases.find((item) => item.id === "QOS0-current-policy-unsupported");
    expect(evaluateCase(sameTraceLatency)).toMatchObject({
      status: "accepted",
      logicalTrace: { differs: false, fixedTracePreserved: true },
      allowedVariation: { latency: true, unspecifiedOrder: false },
    });
    expect(evaluateCase({ ...testCase, unspecifiedOrderVariationAllowed: false })).toMatchObject({
      status: "rejected",
      code: "unspecifiedOrderMustRemainUnconstrained",
    });
  });

  test("keeps the allergy route correct under adversarial single-CPU scheduling", () => {
    const testCase = loadCases().cases.find((item) => item.id === "QOS0-current-allergy-single-cpu");
    expect(evaluateCase(testCase)).toMatchObject({
      status: "accepted",
      code: "allergySafeSuccess",
    });
    expect(testCase).toMatchObject({
      allergyDeadline: true,
      allergyDedicatedService: true,
      allergyDomainPlacement: "dedicated",
      allergyDedicatedAdmission: true,
      allergyReservationBudgeted: true,
      reliesOnDomainForSafety: false,
      allergyUsesPriority: false,
      singleCpu: true,
      adversarialScheduling: true,
    });

    for (const placement of ["current", "shared", "dedicated"]) {
      expect(evaluateCase({ ...testCase, allergyDomainPlacement: placement })).toMatchObject({
        status: "accepted",
        code: "allergySafeSuccess",
      });
    }
    expect(evaluateCase({ ...testCase, reliesOnDomainForSafety: true })).toMatchObject({
      status: "rejected",
      code: "allergyDomainSafetyRejected",
    });

    const deadlineCase = loadCases().cases.find((item) => item.id === "QOS0-current-allergy-deadline-cancellation");
    expect(evaluateCase(deadlineCase)).toMatchObject({
      status: "accepted",
      code: "allergySafeCancellationOrRejection",
      logicalTrace: { differs: true },
      allowedVariation: { deadlineObservation: true, permittedOutcome: true },
    });
  });

  test("keeps receipt evidence non-branchable", () => {
    const testCase = loadCases().cases.find((item) => item.id === "QOS0-current-host-default");
    expect(evaluateCase({ ...testCase, receiptBranchable: true })).toMatchObject({
      status: "rejected",
      code: "receiptMustNotBeBranchable",
    });
  });

  test("makes complete evidence eligible only to reopen research", () => {
    const testCase = loadCases().cases.find((item) => item.id === "QOS0-reopen-complete-evidence");
    expect(evaluateCase(testCase)).toEqual({
      id: "QOS0-reopen-complete-evidence",
      status: "eligible",
      code: "researchReopenOnly",
    });
    expect(evaluateCase({ ...testCase, faultContract: false })).toMatchObject({
      status: "rejected",
      code: "completePriorityContractRequired",
    });
  });

  test("rejects caller-owned answers", () => {
    const data = structuredClone(loadCases());
    data.cases[0].expected = { status: "accepted" };
    expect(validate(data).errors).toContain("QOS0-current-host-default.expected is caller-owned");
  });
});
