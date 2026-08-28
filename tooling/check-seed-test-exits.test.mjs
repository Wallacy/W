import { expect, test } from "bun:test"

import { analyzeCSource, checkSeedTestExits } from "./check-seed-test-exits.mjs"

test("enumerates every seed test main and finds no unsafe exit path", async () => {
  const result = await checkSeedTestExits()
  expect(result.findings).toEqual([])
  expect(result.mains.length).toBe(result.analyses.flatMap((item) => item.candidates).length)
})

test("rejects a CHECK macro that returns false directly from main", () => {
  const source = `
#include <stdbool.h>
#define CHECK(value) do { if (!(value)) return false; } while (0)
static bool test_case(void) { return true; }
int main(void) { CHECK(test_case()); return 0; }
`
  const result = analyzeCSource(source, "fixture.c")
  expect(result.mains.length).toBe(1)
  expect(result.findings.map((item) => item.kind)).toEqual(["macro-in-main"])
})

test("covers conditional mains with qualified multiline signatures", () => {
  const source = String.raw`
#if defined(_WIN32)
static int
main(
  void
) {
  return 0;
}
#else
int
main(
  int argc,
  char **argv
) {
  (void)argc;
  (void)argv;
  return 0;
}
#endif
`
  const result = analyzeCSource(source, "fixture.c")
  expect(result.candidates.length).toBe(2)
  expect(result.mains.length).toBe(2)
  expect(result.findings).toEqual([])
})

test("rejects a non-CHECK macro with a boolean return directly from main", () => {
  const source = `
#include <stdbool.h>
#define ASSERT_OK(value) do { if (!(value)) return false; } while (0)
static bool test_case(void) { return true; }
int main(void) { ASSERT_OK(test_case()); return 0; }
`
  const result = analyzeCSource(source, "fixture.c")
  expect(result.findings.map((item) => item.kind)).toEqual(["macro-in-main"])
})

test("rejects returning a bool helper directly from main", () => {
  const source = `
#include <stdbool.h>
static bool test_case(void) { return false; }
int main(void) { return test_case(); }
`
  const result = analyzeCSource(source, "fixture.c")
  expect(result.findings.map((item) => item.kind)).toEqual(["return-bool-helper-in-main"])
})

test("rejects direct false and discarded bool helper returns in main", () => {
  const source = `
#include <stdbool.h>
static bool test_case(void) { return false; }
int main(void) { test_case(); return false; }
`
  const result = analyzeCSource(source, "fixture.c")
  expect(result.findings.map((item) => item.kind)).toEqual([
    "discarded-bool-helper-in-main",
    "return-false-in-main",
  ])
})

test("rejects a multiline macro with a boolean return directly from main", () => {
  const source = String.raw`
#include <stdbool.h>
#define ASSERT_OK(value) \
  do { \
    if (!(value)) return false; \
  } while (0)
static bool test_case(void) { return true; }
int main(void) { ASSERT_OK(test_case()); return 0; }
`
  const result = analyzeCSource(source, "fixture.c")
  expect(result.findings.map((item) => item.kind)).toEqual(["macro-in-main"])
})

test("rejects returning a bool variable from main", () => {
  const source = `
#include <stdbool.h>
int main(void) { bool passed = true; return passed; }
`
  const result = analyzeCSource(source, "fixture.c")
  expect(result.findings.map((item) => item.kind)).toEqual(["return-bool-variable-in-main"])
})

test("accepts a CHECK macro in a bool helper and an explicit main guard", () => {
  const source = `
#include <stdbool.h>
#define CHECK(value) do { if (!(value)) return false; } while (0)
static bool test_case(void) { CHECK(true); return true; }
int main(void) { if (!test_case()) return 1; return 0; }
`
  const result = analyzeCSource(source, "fixture.c")
  expect(result.findings).toEqual([])
})

test("ignores return false text in comments and string literals", () => {
  const source = `
#include <stdbool.h>
static bool test_case(void) { return true; }
int main(void) {
  const char *text = "return false; CHECK(test_case());";
  // return false; CHECK(test_case());
  if (!test_case()) return 1;
  return 0;
}
`
  const result = analyzeCSource(source, "fixture.c")
  expect(result.findings).toEqual([])
})
