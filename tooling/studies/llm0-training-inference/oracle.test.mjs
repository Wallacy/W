import { describe, expect, test } from "bun:test";
import { loadData, validate } from "./oracle.mjs";

describe("LLM0 training and inference readiness oracle", () => {
  test("maps all finite training and inference gaps", () => {
    const checked = validate(loadData());
    expect(checked.errors).toEqual([]);
    expect(checked.gaps.filter((gap) => gap.track === "training").length).toBeGreaterThanOrEqual(10);
    expect(checked.gaps.filter((gap) => gap.track === "inference").length).toBeGreaterThanOrEqual(9);
  });
  test("keeps workload contracts separate from performance claims", () => {
    const data = structuredClone(loadData());
    data.workloads[0].oracleChecks = ["throughput"];
    expect(validate(data).errors.some((error) => error.includes("not structured"))).toBe(true);
    const forged = structuredClone(loadData());
    forged.gaps[0].default = "promote core";
    expect(validate(forged).errors.some((error) => error.includes("core boundary"))).toBe(true);
  });
});
