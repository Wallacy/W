import { describe, expect, test } from "bun:test";

// This host oracle checks the study inputs. It does not compile or execute W.
const LIMITATION = "host-oracle; does not compile or execute W; design evidence only";

function typedBatchSummary(rows) {
  if (!Array.isArray(rows)) return { status: "rejected", code: "invalidRows" };
  const columns = { sequence: [], hawkingFlux: [], warning: [] };
  for (const row of rows) {
    if (!row || typeof row !== "object") return { status: "rejected", code: "invalidRow" };
    columns.sequence.push(row.sequence);
    columns.hawkingFlux.push(row.hawkingFlux);
    columns.warning.push(row.warning);
  }
  const finiteFlux = columns.hawkingFlux.filter(
    (value) => typeof value === "number" && Number.isFinite(value),
  );
  return {
    count: columns.sequence.length,
    lastSequence: columns.sequence.at(-1) ?? null,
    maxHawkingFlux: finiteFlux.length > 0 ? Math.max(...finiteFlux) : null,
    warnings: columns.warning.filter((value) => value !== null).length,
    nullWarning: columns.warning.some((value) => value === null),
    nanIsValue: columns.hawkingFlux.some((value) => value === "NaN"),
    fields: ["sequence", "hawkingFlux", "warning"],
    status: "accepted",
  };
}

function dynamicBatchSummary(rows) {
  if (!Array.isArray(rows)) return { status: "rejected", code: "invalidRows" };
  const schema = ["sequence", "hawkingFlux", "warning"];
  const columns = Object.fromEntries(schema.map((name) => [name, []]));
  for (const row of rows) {
    if (!row || typeof row !== "object") return { status: "rejected", code: "invalidRow" };
    if (JSON.stringify(Object.keys(row)) !== JSON.stringify(schema)) {
      return { status: "rejected", code: "dynamicSchemaMismatch" };
    }
    for (const name of schema) columns[name].push(row[name]);
  }
  const finiteFlux = columns.hawkingFlux.filter(
    (value) => typeof value === "number" && Number.isFinite(value),
  );
  return {
    count: columns.sequence.length,
    lastSequence: columns.sequence.at(-1) ?? null,
    maxHawkingFlux: finiteFlux.length > 0 ? Math.max(...finiteFlux) : null,
    warnings: columns.warning.reduce((count, value) => count + (value === null ? 0 : 1), 0),
    nullWarning: columns.warning.includes(null),
    nanIsValue: columns.hawkingFlux.includes("NaN"),
    fields: schema,
    status: "accepted",
  };
}

function rowArraySummary(readings) {
  if (!Array.isArray(readings)) return { status: "rejected", code: "invalidRows" };
  let lastSequence = null;
  let maxHawkingFlux = null;
  let warnings = 0;
  let nullWarning = false;
  let nanIsValue = false;
  for (const reading of readings) {
    if (!reading || typeof reading !== "object") return { status: "rejected", code: "invalidRow" };
    lastSequence = reading.sequence;
    if (reading.warning === null) nullWarning = true;
    else warnings += 1;
    if (reading.hawkingFlux === "NaN") nanIsValue = true;
    if (typeof reading.hawkingFlux === "number" && Number.isFinite(reading.hawkingFlux)) {
      maxHawkingFlux = maxHawkingFlux === null
        ? reading.hawkingFlux
        : Math.max(maxHawkingFlux, reading.hawkingFlux);
    }
  }
  return {
    count: readings.length,
    lastSequence,
    maxHawkingFlux,
    warnings,
    nullWarning,
    nanIsValue,
    fields: ["sequence", "hawkingFlux", "warning"],
    status: "accepted",
  };
}

function rejectDuplicate(names) {
  return new Set(names).size === names.length
    ? { status: "accepted" }
    : { status: "rejected", code: "duplicateFieldName" };
}

function rejectUnequal(columns) {
  const lengths = Object.values(columns).map((values) => values.length);
  return new Set(lengths).size === 1
    ? { status: "accepted" }
    : { status: "rejected", code: "unequalColumnLength" };
}

function rejectSchemaChange(chunks) {
  const identity = JSON.stringify(chunks[0]?.schema ?? []);
  return chunks.every((chunk) => JSON.stringify(chunk.schema) === identity)
    ? { status: "accepted" }
    : { status: "rejected", code: "streamSchemaChange" };
}

function rejectCopyNever(device) {
  return device.source === device.target && device.copy === "never"
    ? { status: "accepted" }
    : { status: "rejected", code: "copyNeverDeviceMismatch" };
}

function rejectForeign(foreign) {
  if (foreign.utf8Declared === true && foreign.validUtf8 !== true) return { status: "rejected", code: "invalidUtf8" };
  if (foreign.offset + foreign.length > foreign.byteLength) return { status: "rejected", code: "offsetOutOfBounds" };
  return { status: "accepted" };
}

function releaseOnce(owner) {
  if (owner.views !== 0) return { status: "rejected", code: "ownerStillInUse" };
  return owner.releaseAttempts === 1
    ? { status: "accepted" }
    : { status: "rejected", code: "releaseExactlyOnce" };
}

describe("R1 tabular-carrier host oracle", () => {
  test("declares that it does not execute W", () => {
    expect(LIMITATION).toContain("does not compile or execute W");
  });

  test("typed, dynamic, and row-array variants preserve the primary output", () => {
    const rows = [
      { sequence: 7, hawkingFlux: 0.2, warning: null },
      { sequence: 8, hawkingFlux: 0.8, warning: "evacuate" },
    ];
    const expected = {
      count: 2,
      lastSequence: 8,
      maxHawkingFlux: 0.8,
      warnings: 1,
      nullWarning: true,
      nanIsValue: false,
      fields: ["sequence", "hawkingFlux", "warning"],
      status: "accepted",
    };
    expect(typedBatchSummary(rows)).toEqual(expected);
    expect(dynamicBatchSummary(rows)).toEqual(expected);
    expect(rowArraySummary(rows)).toEqual(expected);
    expect(dynamicBatchSummary([
      rows[0],
      { sequence: 8, hawkingFlux: 0.8, warning: "evacuate", satelliteId: "extra" },
    ])).toEqual({ status: "rejected", code: "dynamicSchemaMismatch" });
  });

  test("null warning and NaN flux remain different values", () => {
    expect(typedBatchSummary([{ sequence: 9, hawkingFlux: "NaN", warning: null }])).toMatchObject({
      nullWarning: true,
      nanIsValue: true,
      status: "accepted",
    });
  });

  test("all variants return an optional maximum for empty and finite-negative inputs", () => {
    const variants = [typedBatchSummary, dynamicBatchSummary, rowArraySummary];
    for (const summarize of variants) {
      expect(summarize([])).toMatchObject({
        count: 0,
        lastSequence: null,
        maxHawkingFlux: null,
        warnings: 0,
        nullWarning: false,
        nanIsValue: false,
        status: "accepted",
      });
      expect(summarize([
        { sequence: 10, hawkingFlux: -4, warning: null },
        { sequence: 11, hawkingFlux: "NaN", warning: null },
        { sequence: 12, hawkingFlux: -1, warning: "late" },
      ])).toMatchObject({
        count: 3,
        lastSequence: 12,
        maxHawkingFlux: -1,
        warnings: 1,
        nullWarning: true,
        nanIsValue: true,
        status: "accepted",
      });
    }
  });

  test("duplicate names and unequal lengths fail before a batch summary", () => {
    expect(rejectDuplicate(["sequence", "sequence", "hawkingFlux"])).toEqual({ status: "rejected", code: "duplicateFieldName" });
    expect(rejectUnequal({ sequence: [10, 11], hawkingFlux: [0.5], warning: [null, null] })).toEqual({ status: "rejected", code: "unequalColumnLength" });
  });

  test("a stream rejects a schema change between satellite chunks", () => {
    expect(rejectSchemaChange([
      { schema: ["sequence", "hawkingFlux", "warning"] },
      { schema: ["sequence", "hawkingFlux", "warning", "satelliteId"] },
    ])).toEqual({ status: "rejected", code: "streamSchemaChange" });
  });

  test("copy-never rejects a device mismatch", () => {
    expect(rejectCopyNever({ source: "gpu", target: "cpu", copy: "never" })).toEqual({ status: "rejected", code: "copyNeverDeviceMismatch" });
  });

  test("untrusted foreign input rejects invalid UTF-8 and offsets", () => {
    expect(rejectForeign({ utf8Declared: true, validUtf8: false, offset: 0, length: 4, byteLength: 4 })).toEqual({ status: "rejected", code: "invalidUtf8" });
    expect(rejectForeign({ offset: 8, length: 4, byteLength: 8 })).toEqual({ status: "rejected", code: "offsetOutOfBounds" });
  });

  test("foreign release waits for views and occurs exactly once", () => {
    expect(releaseOnce({ views: 1, releaseAttempts: 1 })).toEqual({ status: "rejected", code: "ownerStillInUse" });
    expect(releaseOnce({ views: 0, releaseAttempts: 1 })).toEqual({ status: "accepted" });
    expect(releaseOnce({ views: 0, releaseAttempts: 2 })).toEqual({ status: "rejected", code: "releaseExactlyOnce" });
  });
});
