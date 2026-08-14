import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { evaluateDyn1Case, validateDyn1 } from "../../dyn1-versioned-behavior-machine.mjs";
import { validateDyn1StudyManifest } from "../../dyn1-versioned-behavior-manifest.mjs";

const studyDirectory = import.meta.dir;
const toolingDirectory = path.resolve(studyDirectory, "../..");
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "dyn1-versioned-behavior-cases.json"), "utf8"));
const study = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const bundle = JSON.parse(fs.readFileSync(path.join(studyDirectory, "bundle.json"), "utf8"));

describe("DYN1 study oracle", () => {
  test("keeps manifest, corpus, and evidence roles separate", () => {
    expect(validateDyn1(corpus, { root: repositoryRoot }).errors).toEqual([]);
    expect(validateDyn1StudyManifest(study, { studyDirectory, repositoryRoot })).toEqual([]);
    expect(study.evidence.missing).toContain("provider");
    expect(study.evidence.current).toContain("dyn1-design-oracle");
    expect(study.metrics.caseCount).toBe(70);
    expect(bundle.variants.find((variant) => variant.id === "split-service-plugin")).toMatchObject({ role: "alternative", disposition: "current-composable" });
    expect(study.exactGap.id).toBe("DYN0-persistent-generation-reference");
    expect(study.exactGap.languageSurface).toBe("unresolved");
  });

  test("derives the four route dispositions from events", () => {
    const get = (id) => evaluateDyn1Case(corpus.cases.find((item) => item.id === id), { corpus });
    expect(get("DYN1-A-repl-snapshot").route).toBe("composable");
    expect(get("DYN1-B-local-plugin-generation").route).toBe("composable");
    expect(get("DYN1-C-persistent-generation-reference").route).toBe("research");
    expect(get("DYN1-D-eval-rejected").route).toBe("intentionally-rejected");
  });
});
