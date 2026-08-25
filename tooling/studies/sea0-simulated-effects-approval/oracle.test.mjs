import { describe, expect, test } from "bun:test";
import { loadData, validate } from "./oracle.mjs";

describe("SEA0 simulated effects and approval oracle", () => {
  test("accepts bounded state traces and exact bulk approval", () => {
    const checked = validate(loadData());
    expect(checked.errors).toEqual([]);
    expect(checked.cases).toHaveLength(8);
  });
  test("rejects stale or different approved input and unbounded DAG", () => {
    const input = structuredClone(loadData());
    input.cases.find((item) => item.id === "SEA0-commit-linear").approvedInputDigest = "sha256:other";
    expect(validate(input).errors.some((error) => error.includes("different from approved"))).toBe(true);
    const dag = structuredClone(loadData());
    const dagCase = dag.cases.find((item) => item.id === "SEA0-unbounded-dag-reject");
    dagCase.dependencyCount = 34;
    delete dagCase.expectedOutcome;
    expect(validate(dag).errors.some((error) => error.includes("DAG bound"))).toBe(true);
  });
  test("preserves unknown outcome after real dispatch", () => {
    const item = loadData().cases.find((candidate) => candidate.id === "SEA0-unknown-after-dispatch");
    expect(item.dispatch).toBe("unknownOutcome(fx-independent-metrics)");
    expect(item.trace.at(-1)).toBe("unknown");
  });
});
