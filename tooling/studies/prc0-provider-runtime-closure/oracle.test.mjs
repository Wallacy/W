import { describe, expect, test } from "bun:test";
import corpus from "../../prc0-provider-runtime-closure-cases.json" with { type: "json" };
import { runPRC0Case } from "../../prc0-provider-runtime-closure-machine.mjs";

const casesById = new Map(corpus.cases.map((item) => [item.id, item]));

describe("PRC0 provider/runtime closure host oracle", () => {
  test("has one current and one rejected route for each target gate", () => {
    expect(corpus.cases).toHaveLength(14);
    for (const decision of corpus.decisions) {
      const routes = corpus.cases.filter((item) => item.decisions.includes(decision));
      expect(routes.filter((item) => item.kind === "current-contract")).toHaveLength(1);
      expect(routes.filter((item) => item.kind === "rejected-route")).toHaveLength(1);
    }
  });

  test("derives seven accepted and seven rejected host routes", () => {
    const results = corpus.cases.map((item) => runPRC0Case(item));
    expect(results.filter((result) => result.status === "accepted")).toHaveLength(7);
    expect(results.filter((result) => result.status === "rejected")).toHaveLength(7);
  });

  test("keeps durable recovery and quantity boundaries explicit", () => {
    const recovery = runPRC0Case(casesById.get("PRC0-W-133-current"));
    expect(recovery.state.journal.records).toEqual(["1:input:effect-order-42", "2:outcome:effect-order-42"]);
    expect(recovery.state.calls["call-order-1"].frameResolution).toBe("runtimeClosure");
    const quantity = runPRC0Case(casesById.get("PRC0-W-903-current"));
    expect(quantity.canonicalSeconds).toBe(30);
    expect(quantity.deltaK).toBe(160);
    expect(quantity.referenceBits).toBe(524288);
    expect(quantity.exactBytes).toBe(65536);
    expect(runPRC0Case(casesById.get("PRC0-W-903-adversarial")).error).toBe("fixedUnitToken");
  });

  test("retains provider/bridge adversarial invariants", () => {
    const dlpack = runPRC0Case(casesById.get("PRC0-W-1147-adversarial"));
    expect(dlpack.error).toBe("W-DLPACK-0025");
    expect(dlpack.state.leases).toBe(1);
    expect(dlpack.state.releaseCalls).toBe(0);
    const allocator = runPRC0Case(casesById.get("PRC0-W-1328-current"));
    expect(allocator.state.typedDropsBeforeReclaim).toBe(true);
    expect(allocator.state.providerCloseCount).toBe(1);
  });
});
