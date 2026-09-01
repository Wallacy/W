import { describe, expect, test } from "bun:test";
import { evaluateCase, loadCases, validate } from "./oracle.mjs";

const expected = new Map([
  ["TGM0-map-input-current", ["accepted", "map-success"]],
  ["TGM0-map-completion-current", ["accepted", "map-success"]],
  ["TGM0-map-input-error-current", ["accepted", "map-error"]],
  ["TGM0-map-completion-error-current", ["accepted", "map-error"]],
  ["TGM0-collect-input-current", ["accepted", "collect"]],
  ["TGM0-collect-completion-current", ["accepted", "collect"]],
  ["TGM0-empty-current", ["accepted", "collect"]],
  ["TGM0-parent-cancel-current", ["accepted", "parent-canceled"]],
  ["TGM0-fault-current", ["accepted", "fault-boundary"]],
  ["TGM0-legacy-labels", ["rejected", "canonicalLabelsRequired"]],
  ["TGM0-zero-limit", ["rejected", "positiveLimitRequired"]],
  ["TGM0-implicit-unbounded", ["rejected", "explicitBoundRequired"]],
  ["TGM0-parallel-no-domain", ["rejected", "parallelDomainRequired"]],
  ["TGM0-parallel-no-capability", ["rejected", "parallelCapabilityRequired"]],
  ["TGM0-concurrent-domain-slot", ["rejected", "concurrentDomainSlotRejected"]],
  ["TGM0-input-restaged", ["rejected", "inputMustStageOnce"]],
  ["TGM0-callable-restaged", ["rejected", "callableMustStageOnce"]],
  ["TGM0-mutable-callable", ["rejected", "concurrentCallableRequired"]],
  ["TGM0-consuming-callable", ["rejected", "concurrentCallableRequired"]],
  ["TGM0-admitted-item-moved-twice", ["rejected", "admittedItemMustMoveOnce"]],
  ["TGM0-remaining-input-leak", ["rejected", "remainingInputsMustDropOnce"]],
  ["TGM0-late-result-reservation", ["rejected", "resultPreflightRequired"]],
  ["TGM0-return-before-drain", ["rejected", "drainRequired"]],
  ["TGM0-collect-cancels-error", ["rejected", "collectMustObserveApplicationErrors"]],
  ["TGM0-collect-loses-index", ["rejected", "collectInputIdentityRequired"]],
  ["TGM0-active-limit-exceeded", ["rejected", "activeChildLimitExceeded"]],
]);

describe("TGM0 pipeline task map and collect evidence", () => {
  test("derives every current and rejected route", () => {
    const checked = validate(loadCases());
    expect(checked.errors).toEqual([]);
    expect(checked.results).toHaveLength(expected.size);
    for (const result of checked.results) expect([result.status, result.code]).toEqual(expected.get(result.id));
  });

  test("separates input and completion ordering for successful map", () => {
    const cases = loadCases().cases;
    expect(evaluateCase(cases.find((item) => item.id === "TGM0-map-input-current")).result).toEqual(["A", "B", "C"]);
    expect(evaluateCase(cases.find((item) => item.id === "TGM0-map-completion-current")).result).toEqual(["B", "C", "A"]);
  });

  test("uses completion to cancel but declared ordering to select the primary error", () => {
    const cases = loadCases().cases;
    expect(evaluateCase(cases.find((item) => item.id === "TGM0-map-input-error-current"))).toMatchObject({
      primaryErrorIndex: 0,
      cancellationRequestedAt: 2,
      publicationStep: 7,
    });
    expect(evaluateCase(cases.find((item) => item.id === "TGM0-map-completion-error-current"))).toMatchObject({
      primaryErrorIndex: 1,
      cancellationRequestedAt: 2,
      publicationStep: 7,
    });
  });

  test("completion collect retains every original input index", () => {
    const testCase = loadCases().cases.find((item) => item.id === "TGM0-collect-completion-current");
    expect(evaluateCase(testCase).settlements).toEqual([
      { index: 1, outcome: "error" },
      { index: 2, outcome: "canceled" },
      { index: 0, outcome: "success" },
    ]);
  });

  test("parent cancellation and fault suppress partial arrays until drain", () => {
    const cases = loadCases().cases;
    expect(evaluateCase(cases.find((item) => item.id === "TGM0-parent-cancel-current"))).toMatchObject({ result: null, publicationStep: 6 });
    expect(evaluateCase(cases.find((item) => item.id === "TGM0-fault-current"))).toMatchObject({ result: null, publicationStep: 5 });
  });

  test("rejects caller-owned answers and non-linearized settlements", () => {
    const echoed = structuredClone(loadCases());
    echoed.cases[0].expected = { code: "map-success" };
    expect(validate(echoed).errors).toContain("TGM0-map-input-current.expected is caller-owned");

    const tied = structuredClone(loadCases());
    tied.cases[0].items[1].bodySettlementStep = tied.cases[0].items[0].bodySettlementStep;
    expect(validate(tied).errors).toContain("TGM0-map-input-current does not linearize body settlements");
  });
});
