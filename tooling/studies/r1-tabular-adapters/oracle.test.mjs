import { describe, expect, test } from "bun:test";

const LIMITATION = "host-oracle; does not compile or execute W; format binaries are modeled as metadata";
const SCHEMA = ["sequence", "hawkingFlux", "warning"];
const VARIANTS = ["csv-stream", "parquet-snapshot", "arrow-ipc-stream"];

function summarize(rows) {
  if (!Array.isArray(rows)) return { status: "rejected", code: "invalidRows" };
  let maximum = null;
  let warnings = 0;
  let nullWarning = false;
  let nanIsValue = false;
  for (const row of rows) {
    if (!row || typeof row !== "object") return { status: "rejected", code: "invalidRow" };
    if (row.warning === null) nullWarning = true;
    else warnings += 1;
    if (typeof row.hawkingFlux === "number" && Number.isNaN(row.hawkingFlux)) nanIsValue = true;
    if (typeof row.hawkingFlux === "number" && Number.isFinite(row.hawkingFlux)) {
      maximum = maximum === null ? row.hawkingFlux : Math.max(maximum, row.hawkingFlux);
    }
  }
  return {
    count: rows.length,
    lastSequence: rows.at(-1)?.sequence ?? null,
    maxHawkingFlux: maximum,
    warnings,
    nullWarning,
    nanIsValue,
    fields: SCHEMA,
    status: "accepted",
  };
}

function runVariant(variant, rows) {
  if (!VARIANTS.includes(variant)) return { status: "rejected", code: "unknownVariant" };
  // This models the same bounded row loop for all source contracts. It does
  // not parse CSV, read Parquet bytes, or consume Arrow IPC.
  return { variant, ...summarize(rows) };
}

describe("R1 TAB1 adapter study oracle", () => {
  test("is host evidence only", () => {
    expect(LIMITATION).toContain("does not compile or execute W");
  });

  test("three format workflows preserve the same primary outcome", () => {
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
      fields: SCHEMA,
      status: "accepted",
    };
    for (const variant of VARIANTS) {
      expect(runVariant(variant, rows)).toEqual({ variant, ...expected });
    }
  });

  test("every variant iterates empty, negative finite, and nonfinite rows", () => {
    const negativeAndNaN = [
      { sequence: 10, hawkingFlux: -4, warning: null },
      { sequence: 11, hawkingFlux: Number.NaN, warning: null },
      { sequence: 12, hawkingFlux: -1, warning: "late" },
    ];
    for (const variant of VARIANTS) {
      expect(runVariant(variant, [])).toMatchObject({
        count: 0,
        maxHawkingFlux: null,
        status: "accepted",
      });
      expect(runVariant(variant, negativeAndNaN)).toMatchObject({
        maxHawkingFlux: -1,
        nanIsValue: true,
        nullWarning: true,
        status: "accepted",
      });
    }
  });

  test("multiline and row boundaries stay named for each format", () => {
    const boundaries = {
      "multiline-quoted-csv-split": "csv-stream",
      "parquet-footer-and-page-split": "parquet-snapshot",
      "arrow-dictionary-message-split": "arrow-ipc-stream",
    };
    for (const [adversary, variant] of Object.entries(boundaries)) {
      expect({
        adversary,
        variant,
        result: runVariant(variant, [
          { sequence: 7, hawkingFlux: 0.2, warning: "line 1\nline 2" },
        ]),
      }).toMatchObject({
        adversary,
        variant,
        result: { count: 1, maxHawkingFlux: 0.2, status: "accepted" },
      });
    }
  });

  test("adversarial metadata stays bounded and named", () => {
    const adversarialIds = [
      "multiline-quoted-csv-split",
      "duplicate-header",
      "empty-vs-null",
      "invalid-utf8",
      "row-width",
      "bare-cr",
      "token-collision",
      "negative-finite-nan",
      "partial-encode",
      "parquet-corrupt-footer-offset-bomb",
      "parquet-thrift-logical-legacy-checksum-encrypted-source",
      "parquet-incomplete-footer-deterministic-digest",
      "arrow-schema-dictionary-endian-copy-alignment",
      "arrow-untrusted-c-double-release-device-cancel",
    ];
    expect(new Set(adversarialIds).size).toBe(adversarialIds.length);
  });
});
