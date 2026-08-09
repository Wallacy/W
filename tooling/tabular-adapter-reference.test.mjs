import { describe, expect, test } from "bun:test";
import fs from "node:fs";
import crypto from "node:crypto";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { runTabularAdapterProgram } from "./tabular-adapter-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "tabular-adapter-cases.json"), "utf8"));
const vectors = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "tabular-adapter-byte-vectors.json"), "utf8"));
const snapshotPath = path.join(toolingDirectory, "tabular-adapter-results.snapshot.jsonl");

function digestState(state) {
  return `sha256:${crypto.createHash("sha256").update(JSON.stringify(state)).digest("hex")}`;
}

describe("TAB1 host oracle", () => {
  test("derives every case result without W execution", () => {
    const snapshot = fs
      .readFileSync(snapshotPath, "utf8")
      .trim()
      .split("\n")
      .filter(Boolean)
      .map((line) => JSON.parse(line));
    expect(snapshot.length).toBe(corpus.cases.length);

    for (const [index, testCase] of corpus.cases.entries()) {
      const actual = runTabularAdapterProgram(testCase.operations);
      const recorded = snapshot[index];
      expect(recorded.id).toBe(testCase.id);
      expect(recorded.status).toBe(actual.status);
      expect(recorded.code ?? null).toBe(actual.code ?? null);
      expect(recorded.traceLength).toBe(actual.trace.length);
      expect(recorded.stateDigest).toBe(digestState(actual.state));
    }
  });

  test("keeps the small CSV byte vectors exact", () => {
    for (const vector of vectors.vectors) {
      expect(Buffer.from(vector.text, "utf8").toString("hex")).toBe(vector.hex);
    }
  });
});
