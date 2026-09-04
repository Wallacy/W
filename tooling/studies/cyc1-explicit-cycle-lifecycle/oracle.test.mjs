import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { fileURLToPath } from "node:url";
import { evaluateCyc1Case, validateCyc1 } from "../../cyc1-explicit-cycle-machine.mjs";
import { validateCyc1StudyManifest } from "../../cyc1-explicit-cycle-manifest.mjs";

const studyDirectory = path.dirname(fileURLToPath(import.meta.url));
const toolingDirectory = path.resolve(studyDirectory, "../..");
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "cyc1-explicit-cycle-cases.json"), "utf8"));
const study = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));

describe("CYC1 study oracle", () => {
  test("keeps study evidence separate from W implementation", () => {
    expect(validateCyc1(corpus, { root: repositoryRoot }).errors).toEqual([]);
    expect(validateCyc1StudyManifest(study, { studyDirectory, repositoryRoot })).toEqual([]);
  });

  test("derives a static rejection and an unknown opaque boundary", () => {
    const strong = corpus.cases.find((testCase) => testCase.id === "CYC1-NEG-strong-callback-scc");
    const hidden = corpus.cases.find((testCase) => testCase.id === "CYC1-UNK-hidden-foreign-root");
    expect(evaluateCyc1Case(strong).code).toBe("W-OWNERSHIP-0014");
    expect(evaluateCyc1Case(hidden).status).toBe("unknown");
  });

  test("keeps weak-key and ephemeron routes future-only", () => {
    const weakKey = corpus.cases.find((testCase) => testCase.id === "CYC1-RESEARCH-naive-weak-key");
    const ephemeron = corpus.cases.find((testCase) => testCase.id === "CYC1-RESEARCH-ephemeron-value-key-cycle");
    expect(evaluateCyc1Case(weakKey).status).toBe("future-reopen-candidate");
    expect(evaluateCyc1Case(ephemeron).status).toBe("future-reopen-candidate");
  });
});
