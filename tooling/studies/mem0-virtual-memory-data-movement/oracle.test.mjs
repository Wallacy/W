import { describe, expect, test } from "bun:test";
import { loadData, validate } from "./oracle.mjs";

describe("MEM0 virtual memory and data movement oracle", () => {
  test("classifies every bounded mechanism and workload", () => {
    const checked = validate(loadData());
    expect(checked.errors).toEqual([]);
    expect(checked.mechanisms.length).toBeGreaterThanOrEqual(16);
  });
  test("keeps adversarial universal and target-specific routes explicit", () => {
    const data = structuredClone(loadData());
    data.mechanisms.find((item) => item.id === "mapped-universal-wrapper").classification = "portable semantic owner/API candidate";
    expect(validate(data).errors.some((error) => error.includes("rejected route is absent"))).toBe(true);
    const mmio = structuredClone(loadData());
    mmio.mechanisms.find((item) => item.id === "mapped-device-mmio-boundary").boundedExtent = false;
    expect(validate(mmio).errors.some((error) => error.includes("ownership, bounds"))).toBe(true);
  });
});
