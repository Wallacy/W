import { describe, expect, test } from "bun:test";
import { evaluateCase, loadCases, validate } from "./oracle.mjs";

const expected = new Map([
  ["SVC0-unary-current", ["accepted", null]],
  ["SVC0-server-stream-current", ["accepted", null]],
  ["SVC0-client-stream-current", ["accepted", null]],
  ["SVC0-bidirectional-current", ["accepted", null]],
  ["SVC0-input-not-taken", ["rejected", "inputStreamMustTransfer"]],
  ["SVC0-borrowed-item", ["rejected", "borrowedStreamItem"]],
  ["SVC0-non-wire-item", ["rejected", "nonWireStreamItem"]],
  ["SVC0-missing-boundary-failure", ["rejected", "boundaryFailureMissing"]],
  ["SVC0-nested-stream", ["rejected", "nestedStreamRejected"]],
  ["SVC0-published-any-stream", ["rejected", "publishedAnyStreamRejected"]],
  ["SVC0-implicit-channel", ["rejected", "implicitChannelRejected"]],
  ["SVC0-stream-fn", ["rejected", "streamFunctionRejected"]],
  ["SVC0-open-without-await", ["rejected", "serviceOpenRequiresAwait"]],
  ["SVC0-undrained-early-settlement", ["rejected", "streamEdgesMustDrain"]],
]);

describe("SVC0 directional service streams", () => {
  test("derives all four service topologies from direct stream positions", () => {
    const checked = validate(loadCases());
    expect(checked.errors).toEqual([]);
    expect(new Set(checked.results.filter((item) => item.status === "accepted").map((item) => item.topology))).toEqual(
      new Set(["unary", "server-streaming", "client-streaming", "bidirectional"]),
    );
    for (const result of checked.results) expect([result.status, result.code]).toEqual(expected.get(result.id));
  });

  test("does not let a source label or expected outcome select acceptance", () => {
    const current = structuredClone(loadCases().cases.find((item) => item.id === "SVC0-client-stream-current"));
    current.expected = "rejected";
    expect(evaluateCase(current).status).toBe("accepted");
    current.input.taken = false;
    expect(evaluateCase(current)).toMatchObject({ status: "rejected", code: "inputStreamMustTransfer" });
  });

  test("keeps channel construction and stream settlement explicit", () => {
    const bidi = structuredClone(loadCases().cases.find((item) => item.id === "SVC0-bidirectional-current"));
    bidi.implicitChannel = true;
    expect(evaluateCase(bidi).code).toBe("implicitChannelRejected");
    bidi.implicitChannel = false;
    bidi.drainsOnSettlement = false;
    expect(evaluateCase(bidi).code).toBe("streamEdgesMustDrain");
  });
});
