import { describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";
import { derivePkg1, deriveOwnerDigest } from "../../pkg1-project-transaction-machine.mjs";

const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "../../pkg1-project-transaction-cases.json"), "utf8"));

describe("PKG1 study oracle", () => {
  test("records current identity and transaction routes", () => {
    const results = derivePkg1(corpus);
    expect(results.find((result) => result.caseId === "PKG1-deployment-only").status).toBe("accepted");
    expect(results.find((result) => result.caseId === "PKG1-resolution-only-refresh").status).toBe("accepted");
    expect(results.find((result) => result.caseId === "PKG1-solve-failure").code).toBe("resolutionFailed");
  });

  test("owner identity is independent from nested records", () => {
    const document = structuredClone(corpus.fixtures.workspace.operations[0].document);
    const digest = deriveOwnerDigest(document);
    document.resolution.changed = true;
    document.deployments[0].changed = true;
    expect(deriveOwnerDigest(document)).toBe(digest);
  });
});
