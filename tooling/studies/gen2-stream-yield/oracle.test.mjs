import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, test } from "bun:test";
import { buildGen2Snapshot, validateGen2Corpus } from "../../gen2-stream-yield-machine.mjs";

const studyDirectory = path.dirname(fileURLToPath(import.meta.url));
const toolingDirectory = path.resolve(studyDirectory, "../..");
const readJson = (file) => JSON.parse(fs.readFileSync(path.join(studyDirectory, file), "utf8"));
const corpus = readJson("../../gen2-stream-yield-cases.json");
const bundle = readJson("bundle.json");

describe("GEN2 study bundle oracle", () => {
  test("bundle records the promoted narrow expression and rejected frame surface", () => {
    const checked = validateGen2Corpus(corpus);
    expect(checked.errors).toEqual([]);
    expect(checked.decision.status).toBe("promote-narrow-form");
    expect(bundle.entry).toBe("gen2Fixture");
    expect(bundle.variants.find((variant) => variant.role === "selected").id).toBe("narrow-stream-yield");
    expect(bundle.variants.find((variant) => variant.role === "rejected-witness").disposition).toBe("intentionally-rejected");
    expect(bundle.decision).toMatchObject({ status: "promote-narrow-form", generalGenerator: "intentionally-rejected" });
  });

  test("explicit captures are construction-time and no public resume protocol leaks", () => {
    const fixture = fs.readFileSync(path.join(studyDirectory, "yield.w"), "utf8");
    expect(fixture).toContain("stream <[take source]>");
    expect(fixture).not.toContain("stream fn");
    expect(fixture).not.toContain("return stream {");
    expect(fixture).toContain("yield take order");
    const rejected = fs.readFileSync(path.join(studyDirectory, "rejected.txt"), "utf8");
    expect(rejected).toContain("public frame resumeToken");
    expect(rejected).toContain("yield from source");
    expect(fixture).toContain("yield copy line");
  });

  test("host oracle snapshot is deterministic", () => {
    const snapshot = buildGen2Snapshot(corpus).text;
    const stored = fs.readFileSync(path.join(toolingDirectory, "gen2-stream-yield-results.snapshot.jsonl"), "utf8");
    expect(stored).toBe(snapshot);
    expect(snapshot).toContain('"status":"promote-narrow-form"');
  });
});
