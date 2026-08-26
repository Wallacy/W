import { describe, expect, test } from "bun:test";
import { evaluateCase, loadCases, validate } from "./oracle.mjs";

const expected = new Map([
  ["FST0-empty-current", ["accepted", "empty"]],
  ["FST0-success-current", ["accepted", "first-settled"]],
  ["FST0-error-current", ["accepted", "first-settled"]],
  ["FST0-pre-settled-current", ["accepted", "first-settled"]],
  ["FST0-canceled-child-current", ["accepted", "first-settled"]],
  ["FST0-parent-cancel-current", ["accepted", "parent-canceled"]],
  ["FST0-borrowed-handles", ["rejected", "taskHandlesMustMove"]],
  ["FST0-hidden-branches", ["rejected", "hiddenBranchesRejected"]],
  ["FST0-lexical-selection", ["rejected", "completionOrderRequired"]],
  ["FST0-return-before-drain", ["rejected", "losersMustDrain"]],
  ["FST0-first-success", ["rejected", "firstSuccessNotImplied"]],
  ["FST0-rollback-claim", ["rejected", "cancellationIsNotRollback"]],
  ["FST0-heterogeneous-task", ["rejected", "candidateTaskTypesMustMatch"]],
  ["FST0-persistent-multiplex", ["rejected", "oneShotSelectionOnly"]],
  ["FST0-parent-cancel-value", ["rejected", "parentCancellationIsControl"]],
  ["FST0-duplicate-handle", ["rejected", "duplicateTaskHandle"]],
]);

describe("FST0 first-settled structured task selection", () => {
  test("derives every current and rejected route without caller expected data", () => {
    const checked = validate(loadCases());
    expect(checked.errors).toEqual([]);
    expect(checked.results).toHaveLength(expected.size);
    for (const result of checked.results) expect([result.status, result.code]).toEqual(expected.get(result.id));
  });

  test("selects completion order and drains all losers before publication", () => {
    const cases = loadCases().cases;
    expect(evaluateCase(cases.find((item) => item.id === "FST0-success-current"))).toMatchObject({
      winner: { index: 0, outcome: "success" },
      publicationStep: 7,
      committedEffectsRemain: [1],
    });
    expect(evaluateCase(cases.find((item) => item.id === "FST0-error-current"))).toMatchObject({
      winner: { index: 1, outcome: "error" },
      publicationStep: 7,
    });
  });

  test("uses input order only as the already-settled arm tie-break", () => {
    const testCase = loadCases().cases.find((item) => item.id === "FST0-pre-settled-current");
    expect(evaluateCase(testCase).winner).toEqual({ index: 0, outcome: "success" });
    const pending = structuredClone(testCase);
    for (const task of pending.tasks) task.stateAtArm = "pending";
    expect(evaluateCase(pending).winner).toEqual({ index: 1, outcome: "error" });
  });

  test("does not let a label or expected field select the result", () => {
    const testCase = structuredClone(loadCases().cases.find((item) => item.id === "FST0-error-current"));
    testCase.expected = { winner: 0 };
    expect(evaluateCase(testCase).winner).toEqual({ index: 1, outcome: "error" });
  });

  test("suppresses publication on parent cancellation and still drains", () => {
    const testCase = loadCases().cases.find((item) => item.id === "FST0-parent-cancel-current");
    expect(evaluateCase(testCase)).toMatchObject({
      code: "parent-canceled",
      winner: null,
      publicationStep: 6,
    });
  });

  test("rejects fixtures that invent a post-arm lexical tie-break", () => {
    const data = structuredClone(loadCases());
    const testCase = data.cases.find((item) => item.id === "FST0-success-current");
    testCase.tasks[1].settlementStep = testCase.tasks[0].settlementStep;
    expect(validate(data).errors).toContain("FST0-success-current does not linearize pending settlement commits");
  });
});
