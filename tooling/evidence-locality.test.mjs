import { expect, test } from "bun:test";
import { caseDigest } from "./evidence-locality.mjs";

test("a sibling case does not change a case digest", () => {
  const selected = { id: "selected", source: ["entry {}"], checks: ["clean"] };
  const sibling = { id: "sibling", source: ["entry { print(\"old\") }"] };
  const before = caseDigest(selected);
  sibling.source[0] = "entry { print(\"new\") }";
  expect(caseDigest(selected)).toBe(before);
});

test("object key order does not change a case digest", () => {
  expect(caseDigest({ id: "case", checks: ["a"], source: ["b"] })).toBe(
    caseDigest({ source: ["b"], id: "case", checks: ["a"] }),
  );
});

test("a semantic case change changes its digest", () => {
  const before = { id: "case", source: ["entry {}"] };
  const after = { id: "case", source: ["entry { print(\"changed\") }"] };
  expect(caseDigest(after)).not.toBe(caseDigest(before));
});
