import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { deriveReadiness, validateProtocol } from "../../hum0-human-review-machine.mjs";

const studyDirectory = import.meta.dir;
const toolingDirectory = path.resolve(studyDirectory, "../..");
const repositoryRoot = path.resolve(toolingDirectory, "..");
const study = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const protocol = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "hum0-human-review-protocol.json"), "utf8"));

describe("HUM0 study oracle", () => {
  test("keeps protocol structure separate from R1 results", () => {
    expect(validateProtocol(protocol, { root: repositoryRoot })).toEqual([]);
    expect(study.bundle).toBe(false);
    expect(study.slices).toBe(8);
    expect(study.tasks).toBe(32);
    expect(study.records).toEqual({ human: 0, model: 0 });
    expect(study.evidence.missing).toContain("human-study");
    expect(study.evidence.missing).toContain("model-study");
    expect(study.promotionPolicy).toBe("no-automatic-promotion");
  });

  test("derives readiness without score or preference", () => {
    const readiness = deriveReadiness(protocol, []);
    expect(readiness).toMatchObject({ status: "protocol-ready", sliceCount: 8, taskCount: 32 });
    expect(Object.keys(readiness)).not.toContain("score");
    expect(Object.keys(readiness)).not.toContain("preference");
  });
});
