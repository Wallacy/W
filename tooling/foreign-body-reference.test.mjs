import { createHash } from "node:crypto"
import { describe, expect, test } from "bun:test"
import { runForeignBodyOperations, scanForeignBody } from "./foreign-body-machine.mjs"

const adapter = {
  op: "resolve",
  language: "C",
  mode: "build",
  available: true,
  locked: true,
  adapterDigest: "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  scannerDigest: "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
  scannerAbi: "w.foreign-scanner@1",
  scannerProfile: "c-inline-1",
}

describe("foreign body FB0 host oracle", () => {
  test("a close inside C strings and comments is not the W delimiter", () => {
    const source = "{ const char *s = \"}\"; /* } */ if (ok) { return 1; %> return 0; } tail"
    const result = scanForeignBody({ source })
    expect(source[result.closeOffset]).toBe("}")
    expect(source.slice(result.nextOffset)).toBe(" tail")
    expect(result.maximumDepthObserved).toBe(1)
  })

  test("the body digest preserves CRLF and UTF-8 bytes exactly", () => {
    const source = "{\r\n  const char *s = \"Café\";\r\n}"
    const result = scanForeignBody({ source })
    const body = Buffer.from(source).subarray(1, result.closeOffset)
    const expected = `sha256:${createHash("sha256").update(body).digest("hex")}`
    expect(result.bodyDigest).toBe(expected)
    expect(Buffer.from(result.exactBytesHex, "hex")).toEqual(body)
  })

  test("inline C directives are rejected after structural delimitation", () => {
    const result = runForeignBodyOperations([
      adapter,
      { op: "scan", source: "{\n#define CLOSE }\nreturn 0;\n}" },
    ])
    expect(result.error).toBe("W-FOREIGN-0005")
    expect(result.errorFacts.closeOffset).toBeGreaterThan(1)
    const digraph = runForeignBodyOperations([
      adapter,
      { op: "scan", source: "{\n%:define CLOSE }\nreturn 0;\n}" },
    ])
    expect(digraph.errorFacts.reason).toBe("preprocessorDirective")
    const splice = runForeignBodyOperations([
      adapter,
      { op: "scan", source: "{int val\\\nue = 0;}" },
    ])
    expect(splice.errorFacts.reason).toBe("lineSplice")
  })

  test("tooling can preserve an unknown adapter body but cannot publish it", () => {
    const operations = [
      { op: "resolve", language: "Rust", mode: "tooling", available: false },
      { op: "scan", source: "{ if ready { 1 } else { 0 } }" },
    ]
    expect(runForeignBodyOperations(operations).status).toBe("info")
    const rejected = runForeignBodyOperations([
      ...operations,
      { op: "publish", scannerDigest: "none", bodyDigest: "none" },
    ])
    expect(rejected.error).toBe("W-FOREIGN-0008")
  })

  test("adapter diagnostic offsets map into the W source range", () => {
    const result = runForeignBodyOperations([
      adapter,
      { op: "scan", source: "{return missing;}" },
      { op: "mapDiagnostic", code: "C-UNDECLARED", start: 7, end: 14 },
    ])
    expect(result.state.mappedDiagnostics).toEqual([
      { code: "C-UNDECLARED", start: 8, end: 15 },
    ])
  })

  test("body byte and nesting limits fail before recipe publication", () => {
    const bytes = runForeignBodyOperations([
      adapter,
      { op: "scan", source: "{return 1234;}", maximumBodyBytes: 4 },
    ])
    const depth = runForeignBodyOperations([
      adapter,
      { op: "scan", source: "{{{return 0;}}}", maximumBraceDepth: 1 },
    ])
    expect(bytes.errorFacts.reason).toBe("bodyBytes")
    expect(depth.errorFacts.reason).toBe("braceDepth")
    expect(bytes.state.recipe).toBeNull()
    expect(depth.state.recipe).toBeNull()
  })

  test("recipe publication binds the scanner and exact body digest", () => {
    const scanned = runForeignBodyOperations([
      adapter,
      { op: "scan", source: "{return 0;}" },
    ])
    const bodyDigest = scanned.state.scan.bodyDigest
    const published = runForeignBodyOperations([
      adapter,
      { op: "scan", source: "{return 0;}" },
      {
        op: "publish",
        scannerDigest: "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        bodyDigest,
      },
    ])
    expect(published.state.phase).toBe("published")
    expect(published.state.recipe.bodyDigest).toBe(bodyDigest)
    expect(published.state.recipe.scannerAbi).toBe("w.foreign-scanner@1")
  })

  test("resolution rejects malformed scanner identity", () => {
    const badDigest = runForeignBodyOperations([
      { ...adapter, scannerDigest: "sha256:not-a-digest" },
    ])
    const badAbi = runForeignBodyOperations([
      { ...adapter, scannerAbi: "w.foreign-scanner@2" },
    ])
    expect(badDigest.errorFacts.reason).toBe("scannerDigestInvalid")
    expect(badAbi.errorFacts.reason).toBe("scannerAbi")
  })

  test("an edit creates a new exact digest without normalizing source", () => {
    const result = runForeignBodyOperations([
      adapter,
      { op: "scan", source: "{return 0;}" },
      { op: "replaceSource", source: "{return 1;}" },
    ])
    expect(result.state.revision).toBe(2)
    expect(Buffer.from(result.state.scan.exactBytesHex, "hex").toString()).toBe("return 1;")
  })
})
