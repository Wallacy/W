import { expect, test } from "bun:test"

import {
  formatterInputText,
  parseProbeSummary,
  sourceFacts,
  validateSourceRefPath,
  validateProbeExecution,
} from "./check-seed-source-reader.mjs"

test("source facts count LF and preserve an initial BOM", () => {
  const bytes = Buffer.from("\uFEFFa\r\nb", "utf8")
  expect(sourceFacts(bytes)).toEqual({ length: bytes.length, bom: 1, lines: 2 })
})

test("F0 byte modes are reconstructed without normalization", () => {
  const source = formatterInputText({
    lines: ["fn answer():Int{", "\treturn 42;", "}"],
    newline: "crlf",
    bom: true,
    finalNewline: false,
  })
  expect(Buffer.from(source, "utf8")).toEqual(
    Buffer.from("\uFEFFfn answer():Int{\r\n\treturn 42;\r\n}", "utf8"),
  )
})

test("F0 input metadata and line payloads use closed types", () => {
  expect(() => formatterInputText({ lines: ["a\r"] })).toThrow()
  expect(() => formatterInputText({ lines: ["a\nb"] })).toThrow()
  expect(() => formatterInputText({ lines: ["a"], bom: "yes" })).toThrow()
  expect(() => formatterInputText({ lines: ["a"], finalNewline: 1 })).toThrow()
})

test("FZ0 source refs stay on maintained relative W sources", () => {
  expect(validateSourceRefPath("reference/last-light/formatting.w")).toBe(
    "reference/last-light/formatting.w",
  )
  for (const path of [
    "../reference/last-light/formatting.w",
    "/reference/last-light/formatting.w",
    "C:/reference/last-light/formatting.w",
    "https://example.invalid/formatting.w",
    "w+seed:formatting.w",
    "reference//last-light/formatting.w",
    "reference/./last-light/formatting.w",
    "history/old.w",
    "generated/source.w",
    "tooling/tree-sitter-w/src/generated.w",
    "reference/last-light/formatting.txt",
  ]) {
    expect(() => validateSourceRefPath(path)).toThrow()
  }
})

test("probe summaries use the closed fact shape", () => {
  expect(parseProbeSummary("ok length=7 bom=1 lines=2")).toEqual({
    length: 7,
    bom: 1,
    lines: 2,
  })
  expect(() => parseProbeSummary("ok length=7 bom=2 lines=2")).toThrow()
  expect(() => parseProbeSummary("ok length=7 bom=1")).toThrow()
})

test("probe validation rejects byte mutations", () => {
  const bytes = Buffer.from("A\nB", "utf8")
  const execution = {
    exitCode: 0,
    stdout: bytes,
    stderr: Buffer.from("ok length=3 bom=0 lines=2\n"),
  }
  expect(() => validateProbeExecution("baseline", bytes, execution)).not.toThrow()
  expect(() =>
    validateProbeExecution("changed-output", Buffer.from("A\nC", "utf8"), execution),
  ).toThrow()
  expect(() =>
    validateProbeExecution("changed-facts", bytes, {
      ...execution,
      stderr: Buffer.from("ok length=3 bom=0 lines=1\n"),
    }),
  ).toThrow()
})

test("probe validation rejects nonzero and malformed runs", () => {
  const bytes = Buffer.from("A", "utf8")
  expect(() =>
    validateProbeExecution("failed", bytes, {
      exitCode: 2,
      stdout: Buffer.alloc(0),
      stderr: Buffer.from("error operation=init kind=utf8-truncated offset=0\n"),
    }),
  ).toThrow()
  expect(() =>
    validateProbeExecution("malformed", bytes, {
      exitCode: 0,
      stdout: bytes,
      stderr: Buffer.from("ok length=1 bom=0\n"),
    }),
  ).toThrow()
})
