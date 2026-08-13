import crypto from "node:crypto";
import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { validateSourceRefs } from "./study-source-refs.mjs";

function sha256(bytes) {
  return `sha256:${crypto.createHash("sha256").update(bytes).digest("hex")}`;
}

describe("sourceRefs validation", () => {
  test("reports stale digest, missing symbol, duplicate ref, and sourceBase duplicate", async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), "w-source-refs-"));
    try {
      const base = path.join(root, "base.w");
      const ref = path.join(root, "ref.w");
      await fs.writeFile(base, "baseSymbol\n", "utf8");
      await fs.writeFile(ref, "alpha\n", "utf8");
      const baseDigest = sha256(await fs.readFile(base));
      const refDigest = sha256(await fs.readFile(ref));
      const errors = validateSourceRefs({
        bundleDirectory: root,
        wDirectory: root,
        sourceBaseFile: base,
        sourceBaseSymbol: "baseSymbol",
        location: "fixture/bundle.json",
        sourceRefs: [
          { path: "ref.w", symbol: "alpha", digest: refDigest },
          { path: "ref.w", symbol: "alpha", digest: refDigest },
          { path: "ref.w", symbol: "missing", digest: refDigest },
          { path: "ref.w", symbol: "alpha", digest: "sha256:0000000000000000000000000000000000000000000000000000000000000000" },
          { path: "base.w", symbol: "baseSymbol", digest: baseDigest },
        ],
      });
      expect(errors.some((error) => error.includes("duplicates source reference"))).toBe(true);
      expect(errors.some((error) => error.includes("symbol is absent"))).toBe(true);
      expect(errors.some((error) => error.includes("digest is stale"))).toBe(true);
      expect(errors.some((error) => error.includes("duplicates sourceBase"))).toBe(true);
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });
});
