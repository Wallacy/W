import fs from "node:fs"
import path from "node:path"
import { describe, expect, test } from "bun:test"
import { validateCheatsheetText, validateExcerptMetadata } from "./check-cheatsheet-snippets.mjs"

const repositoryRoot = path.resolve(import.meta.dir, "..")
const cheatsheet = fs.readFileSync(path.join(repositoryRoot, "CHEATSHEET.md"), "utf8")

function mutate(source, before, after) {
  expect(source.includes(before)).toBe(true)
  return source.replace(before, after)
}

function errorsFor(candidate) {
  return validateCheatsheetText(candidate, { repositoryRoot }).errors
}

function exampleDirective(role, use = null, observable = null) {
  const fields = [`role=${role}`]
  if (use !== null) fields.push(`use=${use}`)
  if (observable !== null) fields.push(`observable=${observable}`)
  return `<!-- w-example ${fields.join(" ")} -->`
}

function wSnippet(body, { role = null, use = null, observable = null } = {}) {
  return [role === null ? null : exampleDirective(role, use, observable), "```w", ...body, "```"]
    .filter((line) => line !== null)
    .join("\n")
}

describe("cheatsheet snippet checker", () => {
  test("accepts the current extraction and inventory", () => {
    const result = validateCheatsheetText(cheatsheet, { repositoryRoot })
    expect(result.errors).toEqual([])
    expect(result.counts.w).toBe(22)
    expect(result.counts.source).toBe(30)
    expect(result.counts.composed).toBe(4)
    expect(result.counts.contrafactual).toBe(1)
    expect(result.counts["manifest-fragment"]).toBe(1)
  })

  test("rejects an unclosed fence", () => {
    const candidate = cheatsheet + "\n" + "```w\nfn dangling() {}\n"
    expect(errorsFor(candidate).some((error) => error.includes("unclosed"))).toBe(true)
  })

  test("accepts only portable fence infos in the CHEATSHEET", () => {
    const result = validateCheatsheetText(cheatsheet, { repositoryRoot })
    expect(result.fences.every((fence) => ["", "w", "text"].includes(fence.info))).toBe(true)
    expect([...new Set(result.fences.map((fence) => fence.info))].sort()).toEqual(["text", "w"])
  })

  test("rejects metadata in a non-portable fence info", () => {
    const candidate = [
      "```w executable use=add observable=value",
      "fn add(): i32 { return 1 }",
      "test \"add\" for add { expect add() == 1 }",
      "```",
    ].join("\n")
    expect(errorsFor(candidate).some((error) => error.includes("metadata must be in the immediately preceding HTML comment"))).toBe(true)
  })

  test("rejects an empty or whitespace-only plain W fence", () => {
    const candidate = cheatsheet + "\n" + "```w\n  \n```\n"
    expect(errorsFor(candidate).some((error) => error.includes("empty or whitespace-only"))).toBe(true)
  })

  test("rejects malformed excerpt metadata", () => {
    const candidate = mutate(
      cheatsheet,
      "<!-- w-example role=logical-contract -->\n```w\n// excerpt-kind: composed\n// entrada: dois valores Order",
      "<!-- w-example role=logical-contract -->\n```w\n// excerpt-kind composed\n// entrada: dois valores Order",
    )
    expect(errorsFor(candidate).some((error) => error.includes("invalid excerpt metadata"))).toBe(true)
  })

  test("rejects path escape, absolute path, and history path", () => {
    const escape = mutate(cheatsheet, "reference/last-light/restaurant.w::prepareDish", "../reference/last-light/restaurant.w::prepareDish")
    expect(errorsFor(escape).some((error) => error.includes("must not escape"))).toBe(true)

    const absolute = mutate(cheatsheet, "reference/last-light/restaurant.w::prepareDish", "C:/outside.w::prepareDish")
    expect(errorsFor(absolute).some((error) => error.includes("must be relative"))).toBe(true)

    const history = mutate(cheatsheet, "reference/last-light/restaurant.w::prepareDish", "history/archive/restaurant.w::prepareDish")
    expect(errorsFor(history).some((error) => error.includes("history or generated"))).toBe(true)
  })

  test("rejects missing source and missing symbol", () => {
    const missingSource = mutate(cheatsheet, "reference/last-light/restaurant.w::prepareDish", "reference/last-light/missing.w::prepareDish")
    expect(errorsFor(missingSource).some((error) => error.includes("source file does not exist"))).toBe(true)

    const missingSymbol = mutate(cheatsheet, "reference/last-light/restaurant.w::prepareDish", "reference/last-light/restaurant.w::notPresent")
    expect(errorsFor(missingSymbol).some((error) => error.includes("symbol is missing"))).toBe(true)

    const emptySymbol = mutate(cheatsheet, "reference/last-light/restaurant.w::prepareDish", "reference/last-light/restaurant.w::")
    expect(errorsFor(emptySymbol).some((error) => error.includes("symbol must not be empty"))).toBe(true)
  })

  test("rejects a source excerpt whose body is not LF-normalized exact content", () => {
    const candidate = mutate(
      cheatsheet,
      "  let mixture = spawn<.compute> mix(stock.ingredients, recipe: schedule.recipe)",
      "  let mixture = spawn<.compute> mix(stock.ingredients, recipe: schedule.otherRecipe)",
    )
    expect(errorsFor(candidate).some((error) => error.includes("LF-normalized exact content"))).toBe(true)
  })

  test("returns before filesystem access for a lexically unsafe path", () => {
    const fence = {
      startLine: 1,
      body: "// excerpt-source: ../outside.w::symbol\nfn example() {}",
    }
    const noFilesystemAccess = {
      existsSync() {
        throw new Error("unsafe path reached the filesystem")
      },
    }
    const result = validateExcerptMetadata(fence, repositoryRoot, noFilesystemAccess)
    expect(result.errors.some((error) => error.includes("must not escape"))).toBe(true)
  })

  test("rejects a realpath candidate outside the repository", () => {
    const body = "fn example() {}"
    const fence = {
      startLine: 1,
      body: `// excerpt-source: reference/last-light/execution.w::example\n${body}`,
    }
    // The seam avoids platform-specific symlink setup while exercising the realpath gate.
    const fakeFilesystem = {
      existsSync() {
        return true
      },
      realpathSync(candidate) {
        return candidate === repositoryRoot ? repositoryRoot : path.join(path.dirname(repositoryRoot), "outside", "execution.w")
      },
      statSync() {
        return { isFile: () => true }
      },
      readFileSync() {
        return "example\nfn example() {}"
      },
    }
    const result = validateExcerptMetadata(fence, repositoryRoot, fakeFilesystem)
    expect(result.errors.some((error) => error.includes("after realpath resolution"))).toBe(true)
  })

  test("rejects duplicate and invalid metadata", () => {
    const duplicate = mutate(
      cheatsheet,
      "<!-- w-example role=logical-contract -->\n```w\n// excerpt-kind: composed\n// entrada: dois valores Order",
      "<!-- w-example role=logical-contract -->\n```w\n// excerpt-kind: composed\n// excerpt-kind: composed\n// entrada: dois valores Order",
    )
    expect(errorsFor(duplicate).some((error) => error.includes("duplicate excerpt metadata"))).toBe(true)

    const invalid = mutate(
      cheatsheet,
      "// excerpt-kind: contrafactual\nlet lease = try await ovens.acquire",
      "// excerpt-kind: unknown\nlet lease = try await ovens.acquire",
    )
    expect(errorsFor(invalid).some((error) => error.includes("invalid excerpt metadata"))).toBe(true)
  })

  test("rejects orphan and duplicate w-example directives", () => {
    const orphan = [
      "<!-- w-example role=signature-reference -->",
      "prose without a fence",
    ].join("\n")
    expect(errorsFor(orphan).some((error) => error.includes("orphan w-example directive"))).toBe(true)

    const duplicate = [
      "<!-- w-example role=logical-contract -->",
      "<!-- w-example role=signature-reference -->",
      "```w",
      "protocol Directory<Key> { fn lookup(key: Key): String }",
      "```",
    ].join("\n")
    expect(errorsFor(duplicate).some((error) => error.includes("duplicate w-example directive"))).toBe(true)
  })

  test("accepts an executable declaration with a real use and value observation", () => {
    const candidate = wSnippet([
      "fn add(): i32 { return 1 }",
      "test \"add\" for add { expect add() == 1 }",
    ], { role: "executable", use: "add", observable: "value" })
    expect(errorsFor(candidate)).toEqual([])
  })

  test("accepts a named entry as a consumer of its target", () => {
    const candidate = wSnippet([
      "fn runHello() { print(\"hello\") }",
      "entry Hello(runHello)",
    ], { role: "executable", use: "runHello", observable: "effect" })
    expect(errorsFor(candidate)).toEqual([])
  })

  test("reaches helpers transitively from a real consumer", () => {
    const candidate = wSnippet([
      "fn helper(): i32 { print(\"helper\"); return 1 }",
      "fn main(): i32 { return helper() }",
      "test \"main\" for main { let result = main(); print(result) }",
    ], { role: "executable", use: "main,helper", observable: "effect" })
    expect(errorsFor(candidate)).toEqual([])
  })

  test("rejects a declaration without a consumer", () => {
    const candidate = wSnippet([
      "fn add(): i32 { return 1 }",
    ], { role: "executable", use: "add", observable: "value" })
    expect(errorsFor(candidate).some((error) => error.includes("test or entry consumer"))).toBe(true)
  })

  test("rejects body-only return, discard, and print as observation", () => {
    const bodyOnly = wSnippet([
      "fn add(): i32 { return 1 }",
      "test \"discard\" { let _ = add() }",
    ], { role: "executable", use: "add", observable: "value" })
    expect(errorsFor(bodyOnly).some((error) => error.includes("observable value"))).toBe(true)

    const printedBody = wSnippet([
      "fn add() { print(\"inside declaration\") }",
    ], { role: "executable", use: "add", observable: "effect" })
    expect(errorsFor(printedBody).some((error) => error.includes("test or entry consumer"))).toBe(true)
  })

  test("rejects a literal-only sentinel print as an effect observation", () => {
    const candidate = wSnippet([
      "fn add(): i32 { return 1 }",
      "test \"literal sentinel\" for add { add(); print(\"only a sentinel\") }",
    ], { role: "executable", use: "add", observable: "effect" })
    expect(errorsFor(candidate).some((error) => error.includes("observable effect"))).toBe(true)
  })

  test("rejects an unpaired declaration and use", () => {
    const candidate = wSnippet([
      "fn add(): i32 { return 1 }",
      "fn other(): i32 { return 2 }",
      "test \"other\" { expect other() == 2 }",
    ], { role: "executable", use: "add", observable: "value" })
    expect(errorsFor(candidate).some((error) => error.includes("example-use add has no application"))).toBe(true)
  })

  test("requires a closed diagnostic witness", () => {
    const generic = wSnippet([
      "fn check(): i32 { return 1 }",
      "test \"generic value\" for check { expect check() == 1 }",
    ], { role: "executable", use: "check", observable: "diagnostic" })
    expect(errorsFor(generic).some((error) => error.includes("observable diagnostic"))).toBe(true)

    const caught = wSnippet([
      "fn check(): i32 throws Error { throw Error() }",
      "test \"caught diagnostic\" for check {",
      "  do { let _ = try check() } catch error { expect error == error }",
      "}",
    ], { role: "executable", use: "check", observable: "diagnostic" })
    expect(errorsFor(caught)).toEqual([])
  })

  test("rejects unknown, duplicate, and incomplete executable metadata", () => {
    const unknown = [
      "<!-- w-example role=executable use=add observable=value budget=small -->",
      "```w",
      "fn add(): i32 { return 1 }",
      "test \"add\" for add { expect add() == 1 }",
      "```",
    ].join("\n")
    expect(errorsFor(unknown).some((error) => error.includes("unknown w-example directive key budget"))).toBe(true)

    const duplicate = unknown.replace(" budget=small", " use=other")
    expect(errorsFor(duplicate).some((error) => error.includes("duplicate w-example directive key use"))).toBe(true)

    const incomplete = [
      "<!-- w-example role=executable use=add observable= -->",
      "```w",
      "fn add(): i32 { return 1 }",
      "test \"add\" for add { expect add() == 1 }",
      "```",
    ].join("\n")
    expect(errorsFor(incomplete).some((error) => error.includes("invalid w-example observable"))).toBe(true)
  })

  test("accepts only the two closed logical declaration exemptions", () => {
    const logical = wSnippet([
      "protocol Directory<Key> { fn lookup(key: Key): String }",
    ], { role: "logical-contract" })
    const signature = wSnippet([
      "fn lookup(key: String): String",
    ], { role: "signature-reference" })
    expect(errorsFor(logical)).toEqual([])
    expect(errorsFor(signature)).toEqual([])

    const unknown = logical.replace("role=logical-contract", "role=logical")
    expect(errorsFor(unknown).some((error) => error.includes("unknown w-example role"))).toBe(true)
  })
})
