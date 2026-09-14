import { describe, expect, test } from "bun:test";
import { readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  checkGpu0Catalog,
  renderGpu0Catalog,
  validateGpu0Catalog,
} from "./gpu0-device-linkage.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const live = JSON.parse(await readFile(
  path.join(root, "benchmarks", "gpu0-device-linkage-catalog.json"),
  "utf8",
));

describe("GPU0 diagnostic catalog", () => {
  test("live snapshot and concise projection are current", async () => {
    expect(validateGpu0Catalog(structuredClone(live))).toEqual(live);
    expect(renderGpu0Catalog(live)).toContain("not source-backed W");
    await expect(checkGpu0Catalog()).resolves.toEqual(live);
  });

  test("artifact identities are complete and unique", () => {
    const forged = structuredClone(live);
    forged.artifacts[1].kind = forged.artifacts[0].kind;
    expect(() => validateGpu0Catalog(forged)).toThrow("artifact identities");
  });

  test("metric summaries preserve percentile ordering", () => {
    const forged = structuredClone(live);
    forged.metrics[0].p95 = forged.metrics[0].p50 - 1;
    expect(() => validateGpu0Catalog(forged)).toThrow("metric record");
  });

  test("product claims stay explicitly false", () => {
    const forged = structuredClone(live);
    forged.boundaries.sourceBackedW = true;
    expect(() => validateGpu0Catalog(forged)).toThrow("evidence boundaries");
  });
});
