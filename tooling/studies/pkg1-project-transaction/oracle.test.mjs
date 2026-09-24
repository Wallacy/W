import { describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";
import { derivePkg1, derivePackageDigest, derivePackageSetDigest, deriveDocumentState } from "../../pkg1-project-transaction-machine.mjs";

const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "../../pkg1-project-transaction-cases.json"), "utf8"));

describe("PKG1 study oracle", () => {
  test("records current identity and transaction routes", () => {
    const results = derivePkg1(corpus);
    expect(results.find((result) => result.caseId === "PKG1-deployment-only").status).toBe("accepted");
    expect(results.find((result) => result.caseId === "PKG1-resolution-only-refresh").status).toBe("accepted");
    expect(results.find((result) => result.caseId === "PKG1-solve-failure").code).toBe("resolutionFailed");
  });

  test("package identity excludes local root and coordinator facts", () => {
    const document = structuredClone(corpus.fixtures.buildRoot.operations[0].document);
    const packageDigest = derivePackageDigest(document.packages[0]);
    const packageSetDigest = derivePackageSetDigest(document.packages);
    document.build.resolution.packageSetDigest = packageSetDigest;
    const reordered = structuredClone(document);
    reordered.packages.reverse();
    expect(derivePackageSetDigest(reordered.packages)).toBe(packageSetDigest);
    const before = deriveDocumentState(document);
    document.packages[0].root = "relocated/exact-root";
    expect(derivePackageDigest(document.packages[0])).toBe(packageDigest);
    const relocated = deriveDocumentState(document);
    expect(relocated.packageSetDigest).toBe(before.packageSetDigest);
    expect(relocated.buildPlanDigest).not.toBe(before.buildPlanDigest);
    document.build.default.package = "last-light/menu-compiler";
    document.build.default.product = "menu-compiler";
    const after = deriveDocumentState(document);
    expect(after.packageSetDigest).toBe(before.packageSetDigest);
    expect(after.buildPlanDigest).not.toBe(before.buildPlanDigest);
  });
});
