import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import { CURRENT_PATHS, evaluateW1518Contract, runW1518Case } from "./machine.mjs";

const directory = new URL(".", import.meta.url);
const corpus = JSON.parse(fs.readFileSync(new URL("cases.json", directory), "utf8"));

test("all W-1518 RDX0 cases reduce to their declared design outcome", () => {
  for (const testCase of corpus.cases) {
    const actual = runW1518Case(testCase);
    assert.equal(actual.status, testCase.expected.status, testCase.id);
    if (actual.status === "rejected") assert.equal(actual.code, testCase.expected.code, testCase.id);
    for (const [key, value] of Object.entries(testCase.expected.facts ?? {})) {
      assert.deepEqual(actual.facts?.[key], value, `${testCase.id}.${key}`);
    }
  }
});

test("the design oracle keeps convenience, authority and implementation claims separate", () => {
  assert.equal(CURRENT_PATHS.object, "/v1/o/sha256/<hex>");
  assert.equal(evaluateW1518Contract({ kind: "paths", paths: CURRENT_PATHS, objectMethods: ["GET", "HEAD"], range: "optional", searchAuthority: true }).code, "convenienceAuthorityForbidden");
  assert.equal(evaluateW1518Contract({ kind: "run", claim: "provider-conformant" }).code, "implementationClaimForbidden");
});
