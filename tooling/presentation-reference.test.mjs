import { describe, expect, test } from "bun:test";
import {
  compactPresentationState,
  runPresentationProgram,
} from "./presentation-machine.mjs";

describe("PYN3 presentation host oracle", () => {
  test("requires text/plain and keeps media unique", () => {
    const missing = runPresentationProgram([
      { op: "open" },
      { op: "json", payload: { value: 1 } },
      { op: "finish" },
    ]);
    expect(missing.status).toBe("rejected");
    expect(missing.error.code).toBe("W-PRESENTATION-0007");

    const duplicate = runPresentationProgram([
      { op: "open" },
      { op: "text", value: "a" },
      { op: "text", value: "b" },
    ]);
    expect(duplicate.error.code).toBe("W-PRESENTATION-0003");
  });

  test("uses a closed effect mask", () => {
    const result = runPresentationProgram([
      { op: "open" },
      { op: "effect", effect: "io" },
    ]);
    expect(result.error.code).toBe("W-PRESENTATION-0005");
  });

  test("does not collect streams or copy device tensors", () => {
    const table = runPresentationProgram([
      { op: "open" },
      { op: "tablePreview", plan: { source: { kind: "stream", rows: 2, hasMore: false, bounded: false }, inspectedRows: 2, emittedRows: 2, columns: 2 } },
    ]);
    expect(table.error.code).toBe("W-PRESENTATION-0009");

    const tensor = runPresentationProgram([
      { op: "open" },
      { op: "tensorSummary", plan: { shape: "2x2", dtype: "f32", device: "gpu:0", storage: { view: "bytes" } } },
    ]);
    expect(tensor.error.code).toBe("W-PRESENTATION-0009");
  });

  test("returns only safe or explicitly missing media outcomes", () => {
    const missing = runPresentationProgram([
      { op: "open" },
      { op: "media", media: "text/html" },
    ]);
    expect(missing.status).toBe("rejected");
    expect(missing.error.code).toBe("W-PRESENTATION-0006");

    const active = runPresentationProgram([
      { op: "open" },
      { op: "media", media: "text/javascript" },
    ]);
    expect(active.error.code).toBe("W-PRESENTATION-0002");
  });

  test("cancellation does not publish an output", () => {
    const result = runPresentationProgram([
      { op: "open" },
      { op: "text", value: "cancelled" },
      { op: "cancel" },
    ]);
    expect(result.status).toBe("rejected");
    expect(compactPresentationState(result.state).fallback).toBe("compilerSummary");
  });
});
