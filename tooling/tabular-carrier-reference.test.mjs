import { describe, expect, test } from "bun:test";
import {
  runTabularCarrierProgram,
  schemaIdentity,
  validateExternalPayload,
} from "./tabular-carrier-machine.mjs";

// This is a host oracle. It models the TAB0 contract and never executes W.
export const HOST_ORACLE_LIMITATION = "host-oracle; does not compile or execute W";

const schema = {
  fields: [
    { name: "sequence", type: "u64", nullable: false },
    { name: "hawkingFlux", type: "f64", nullable: false },
    { name: "warning", type: "String", nullable: true },
  ],
  metadata: { source: "restaurant" },
};

function publishBatch(batch, schemaOverride = schema) {
  const normalizedBatch = {
    totalBytes: batch.totalBytes ?? 64,
    allocationBytes: batch.allocationBytes ?? 64,
    ...batch,
    columns: batch.columns.map((column) => ({ bufferCount: 1, ...column })),
  };
  return [
    { op: "publishSchema", schemaId: "telemetry", schema: schemaOverride },
    { op: "publishBatch", batchId: "batch", schemaId: "telemetry", batch: normalizedBatch },
  ];
}

describe("tabular-carrier TAB0 host oracle", () => {
  test("declares the host-only evidence boundary", () => {
    expect(HOST_ORACLE_LIMITATION).toContain("does not compile or execute W");
  });

  test("publishes a finite owned columnar batch with an optional field", () => {
    const result = runTabularCarrierProgram(
      publishBatch({
        rows: 2,
        columns: [
          { name: "sequence", values: [41, 42] },
          { name: "hawkingFlux", values: [3.5, 4.0] },
          { name: "warning", values: [null, "late"] },
        ],
      }),
    );
    expect(result.status).toBe("accepted");
    expect(result.state.batches.batch.immutable).toBe(true);
    expect(result.state.batches.batch.rows).toBe(2);
  });

  test("rejects unequal column lengths before publication", () => {
    const result = runTabularCarrierProgram(
      publishBatch({
        rows: 2,
        columns: [
          { name: "sequence", values: [41] },
          { name: "hawkingFlux", values: [3.5, 4.0] },
          { name: "warning", values: [null, null] },
        ],
      }),
    );
    expect(result).toMatchObject({ status: "rejected", code: "unequalColumnLength", operation: 1 });
  });

  test("keeps null distinct from a NaN value", () => {
    const result = runTabularCarrierProgram([
      ...publishBatch({
        rows: 1,
        columns: [
          { name: "sequence", values: [1] },
          { name: "hawkingFlux", values: [{ $value: "NaN" }] },
          { name: "warning", values: [null] },
        ],
      }),
      { op: "checkNullSemantics", batchId: "batch", column: "warning", row: 0, expected: "null" },
      { op: "checkNullSemantics", batchId: "batch", column: "hawkingFlux", row: 0, expected: "nan" },
    ]);
    expect(result.status).toBe("accepted");
  });

  test("requires exact static binding and explicit dynamic binding", () => {
    const rowType = {
      kind: "struct",
      protocol: "data.Row",
      synthesis: "stored-fields-order",
      nullable: false,
      fields: schema.fields,
    };
    const accepted = runTabularCarrierProgram([
      { op: "publishSchema", schemaId: "telemetry", schema },
      { op: "bindStatic", bindingId: "static", schemaId: "telemetry", rowType },
      { op: "bindDynamic", bindingId: "dynamic", schemaId: "telemetry", rowType: { ...rowType, binding: "explicit", synthesis: undefined } },
    ]);
    expect(accepted.status).toBe("accepted");
    const rejected = runTabularCarrierProgram([
      { op: "publishSchema", schemaId: "telemetry", schema },
      { op: "bindStatic", bindingId: "wrong", schemaId: "telemetry", rowType: { ...rowType, fields: [{ ...rowType.fields[0], type: "u32" }, ...rowType.fields.slice(1)] } },
    ]);
    expect(rejected).toMatchObject({ status: "rejected", code: "staticSchemaMismatch" });
  });

  test("allows row arrays for row algorithms but rejects universal carrier use", () => {
    expect(runTabularCarrierProgram([{ op: "bindArrayCarrier", use: "rowAlgorithm" }]).status).toBe("accepted");
    expect(runTabularCarrierProgram([{ op: "bindArrayCarrier", use: "universalCarrier" }])).toMatchObject({ status: "rejected", code: "arrayRowNotTabularCarrier" });
  });

  test("uses typed descriptors for static selection and checked access", () => {
    const result = runTabularCarrierProgram([
      ...publishBatch({ rows: 1, columns: [
        { name: "sequence", values: [1] },
        { name: "hawkingFlux", values: [3.5] },
        { name: "warning", values: [null] },
      ] }),
      { op: "selectColumn", batchId: "batch", mode: "static", descriptor: "hawkingFlux" },
      { op: "readValue", batchId: "batch", column: "hawkingFlux", row: 0 },
    ]);
    expect(result.status).toBe("accepted");
    expect(result.state.selections[0].complexity).toBe("O(1)");
    expect(result.state.reads[0].value).toBe(3.5);
  });

  test("rejects a run-end encoding without an O(1) materialization", () => {
    const result = runTabularCarrierProgram([
      { op: "publishSchema", schemaId: "encoded", schema: { fields: [{ name: "sequence", type: "u64", nullable: false }] } },
      { op: "publishBatch", batchId: "batch", schemaId: "encoded", batch: { rows: 2, totalBytes: 16, allocationBytes: 16, columns: [{ name: "sequence", values: [1, 2], bufferCount: 1, encoding: "runEnd" }] } },
    ]);
    expect(result).toMatchObject({ status: "rejected", code: "encodingNeedsMaterialization" });
  });

  test("publishes materialized run-end provenance as plain random access", () => {
    const result = runTabularCarrierProgram([
      { op: "publishSchema", schemaId: "encoded", schema: { fields: [{ name: "sequence", type: "u64", nullable: false }] } },
      { op: "publishBatch", batchId: "batch", schemaId: "encoded", batch: { rows: 2, totalBytes: 16, allocationBytes: 16, columns: [{ name: "sequence", values: [1, 2], bufferCount: 1, encoding: "plain", sourceEncoding: "runEnd", materialized: true }] } },
    ]);
    expect(result.status).toBe("accepted");
  });

  test("enforces copy policy and explicit conversion", () => {
    const operations = publishBatch({ rows: 1, device: "gpu", columns: [
      { name: "sequence", values: [1], device: "gpu" },
      { name: "hawkingFlux", values: [3.5], device: "gpu" },
      { name: "warning", values: [null], device: "gpu" },
    ] });
    expect(runTabularCarrierProgram([...operations, { op: "copy", batchId: "batch", policy: "ifNeeded", targetDevice: "cpu" }]).status).toBe("accepted");
    expect(runTabularCarrierProgram([...operations, { op: "copy", batchId: "batch", policy: "never", targetDevice: "cpu" }])).toMatchObject({ status: "rejected", code: "copyNeverDeviceMismatch" });
    expect(runTabularCarrierProgram([...operations, { op: "copy", batchId: "batch", policy: "ifNeeded", conversion: "narrow" }])).toMatchObject({ status: "rejected", code: "explicitConversionRequired" });
    const retained = runTabularCarrierProgram([...operations, { op: "copy", batchId: "batch", policy: "ifNeeded" }]);
    expect(retained).toMatchObject({ status: "accepted" });
    expect(retained.state.copies[0]).toMatchObject({ targetDevice: "gpu", targetExplicit: false, deviceTransferred: false, payloadCopyRequired: false, payloadCopied: false });
  });

  test("keeps all stream chunks at one schema identity", () => {
    const same = { ...schema, metadata: { source: "other" } };
    const result = runTabularCarrierProgram([
      { op: "publishSchema", schemaId: "one", schema },
      { op: "publishSchema", schemaId: "two", schema: same },
      { op: "publishBatch", batchId: "one", schemaId: "one", batch: { rows: 1, totalBytes: 64, allocationBytes: 64, columns: [{ name: "sequence", values: [1], bufferCount: 1 }, { name: "hawkingFlux", values: [3.5], bufferCount: 1 }, { name: "warning", values: [null], bufferCount: 1 }] } },
      { op: "publishBatch", batchId: "two", schemaId: "two", batch: { rows: 1, totalBytes: 64, allocationBytes: 64, columns: [{ name: "sequence", values: [2], bufferCount: 1 }, { name: "hawkingFlux", values: [4.0], bufferCount: 1 }, { name: "warning", values: [null], bufferCount: 1 }] } },
      { op: "openStream", streamId: "stream", schemaId: "one" },
      { op: "emitChunk", streamId: "stream", batchId: "one" },
      { op: "emitChunk", streamId: "stream", batchId: "two" },
    ]);
    expect(result.status).toBe("accepted");
    expect(result.state.streams.stream.chunks).toBe(2);
    expect(schemaIdentity(schema)).toBe(schemaIdentity(same));
  });

  test("releases a foreign owner exactly once after views and waits drain", () => {
    const accepted = runTabularCarrierProgram([
      { op: "importForeign", ownerId: "arrow", trust: "trustedInProcess", payload: { schema: "trusted" } },
      { op: "createView", ownerId: "arrow" },
      { op: "retainWait", ownerId: "arrow" },
      { op: "drainView", viewIndex: 0 },
      { op: "drainWait", ownerId: "arrow" },
      { op: "releaseOwner", ownerId: "arrow" },
    ]);
    expect(accepted.status).toBe("accepted");
    const rejected = runTabularCarrierProgram([
      { op: "importForeign", ownerId: "arrow", trust: "trustedInProcess", payload: { schema: "trusted" } },
      { op: "createView", ownerId: "arrow" },
      { op: "releaseOwner", ownerId: "arrow" },
    ]);
    expect(rejected).toMatchObject({ status: "rejected", code: "ownerStillInUse" });
  });

  test("validates untrusted offsets and UTF-8 before publication", () => {
    expect(validateExternalPayload({ bufferCount: 1, offsets: [0], lengths: [4], byteLength: 4, bytes: 4 })).toBe(true);
    expect(() => validateExternalPayload({ bufferCount: 1, offsets: [3], lengths: [4], byteLength: 4, bytes: 4 })).toThrow("offsetOutOfBounds");
    expect(() => validateExternalPayload({ bufferCount: 1, offsets: [0], lengths: [4], byteLength: 4, utf8Declared: true, validUtf8: false, bytes: 4 })).toThrow("invalidUtf8");
  });

  test("keeps TAB1 formats separate from the tensor-only DLPack direction", () => {
    const tensor = runTabularCarrierProgram([
      { op: "classifyAdapter", format: "dlpack", domain: "tensor" },
    ]);
    expect(tensor).toMatchObject({ status: "accepted" });
    expect(tensor.state.adapterClassifications).toEqual([
      { format: "dlpack", domain: "tensor", status: "direction" },
    ]);
    expect(runTabularCarrierProgram([
      { op: "classifyAdapter", format: "dlpack", domain: "tabular" },
    ])).toMatchObject({ status: "rejected", code: "dlpackTabularCarrier" });
    expect(runTabularCarrierProgram([
      { op: "deferAdapterSignatures", formats: ["csv", "parquet", "arrow"] },
    ]).status).toBe("accepted");
    expect(runTabularCarrierProgram([
      { op: "deferAdapterSignatures", formats: ["csv", "parquet", "arrow", "dlpack"] },
    ])).toMatchObject({ status: "rejected", code: "invalidAdapterDeferral" });
    expect(runTabularCarrierProgram([
      { op: "deferAdapterSignatures", formats: ["parquet", "csv", "arrow"] },
    ])).toMatchObject({ status: "rejected", code: "invalidAdapterDeferral" });
  });

  test("requires initialized bytes in every physical null slot", () => {
    expect(runTabularCarrierProgram([
      { op: "sanitizeNulls", validity: [false, true], physicalValues: [{ initialized: true }, 7] },
    ]).status).toBe("accepted");
    expect(runTabularCarrierProgram([
      { op: "sanitizeNulls", validity: [false], physicalValues: [{ initialized: false }] },
    ])).toMatchObject({ status: "rejected", code: "nullPhysicalSlotNotSanitized" });
  });

  test("fails limit overflow before allocation or publication", () => {
    const result = runTabularCarrierProgram([
      { op: "setLimits", limits: { rows: 2, columns: 2, fields: 2, buffers: 2, totalBytes: 10, allocationBytes: 10, nesting: 2, metadataBytes: 10, stringBytes: 4, chunks: 2 } },
      { op: "publicationOverflow", rows: 3, totalBytes: 11 },
    ]);
    expect(result).toMatchObject({ status: "rejected", code: "limitBeforePublication" });
  });

  test("separates semantic schema identity from bounded metadata", () => {
    const left = { fields: [{ name: "sequence", type: "u64", nullable: false }], metadata: { title: "one" } };
    const right = { fields: [{ name: "sequence", type: "u64", nullable: false }], metadata: { title: "two" } };
    expect(schemaIdentity(left)).toBe(schemaIdentity(right));
    expect(schemaIdentity({ fields: [{ name: "sequence", type: "u32", nullable: false }] })).not.toBe(schemaIdentity(left));
    expect(schemaIdentity({ ...left, extensions: [{ id: "vendor.sequence", version: 1, parameters: { unit: "sequence" } }] })).not.toBe(schemaIdentity(left));
  });

  test("keeps external extensions opaque for dynamic nominal binding", () => {
    const result = runTabularCarrierProgram([
      {
        op: "publishSchema",
        schemaId: "extension",
        schema: {
          fields: [{ name: "sequence", type: "vendor.Sequence", nullable: false }],
          extensions: [{ id: "vendor.sequence", version: 1, parameters: { unit: "sequence" } }],
        },
      },
      {
        op: "bindDynamic",
        bindingId: "extension-row",
        schemaId: "extension",
        rowType: {
          kind: "struct",
          protocol: "data.Row",
          binding: "explicit",
          nullable: false,
          fields: [{ name: "sequence", type: "vendor.Sequence", nullable: false }],
        },
      },
    ]);
    expect(result).toMatchObject({ status: "rejected", code: "extensionAdapterRequired", operation: 1 });
  });

  test("computes published schema identity without revalidating old limits", () => {
    const longName = "longSequence";
    const rowType = {
      kind: "struct",
      protocol: "data.Row",
      synthesis: "stored-fields-order",
      nullable: false,
      fields: [{ name: longName, type: "u64", nullable: false }],
    };
    const result = runTabularCarrierProgram([
      { op: "setLimits", limits: { stringBytes: 64 } },
      { op: "publishSchema", schemaId: "profiled", schema: { fields: rowType.fields } },
      { op: "setLimits", limits: { stringBytes: 1 } },
      { op: "bindStatic", bindingId: "profiled-row", schemaId: "profiled", rowType },
    ]);
    expect(result.status).toBe("accepted");
  });
});
