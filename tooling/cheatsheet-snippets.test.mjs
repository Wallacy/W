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

describe("cheatsheet snippet checker", () => {
  test("accepts the current extraction and inventory", () => {
    const result = validateCheatsheetText(cheatsheet, { repositoryRoot })
    expect(result.errors).toEqual([])
    expect(result.counts.w).toBe(25)
    expect(result.counts.source).toBe(4)
    expect(result.counts.composed).toBe(4)
    expect(result.counts.contrafactual).toBe(1)
    expect(result.counts["manifest-fragment"]).toBe(1)
  })

  test("rejects an unclosed fence", () => {
    const candidate = cheatsheet + "\n" + "```w\nfn dangling() {}\n"
    expect(errorsFor(candidate).some((error) => error.includes("unclosed"))).toBe(true)
  })

  test("rejects unknown W fence info", () => {
    const candidate = mutate(cheatsheet, "```w\nmodule hello", "```w unknown\nmodule hello")
    expect(errorsFor(candidate).some((error) => error.includes("unknown fence info"))).toBe(true)
  })

  test("rejects an empty or whitespace-only plain W fence", () => {
    const candidate = cheatsheet + "\n" + "```w\n  \n```\n"
    expect(errorsFor(candidate).some((error) => error.includes("empty or whitespace-only"))).toBe(true)
  })

  test("rejects missing excerpt metadata", () => {
    const candidate = mutate(
      cheatsheet,
      "```w excerpt\n// excerpt-kind: composed\n// entrada: dois valores Order",
      "```w excerpt\n// entrada: dois valores Order",
    )
    expect(errorsFor(candidate).some((error) => error.includes("requires one metadata line"))).toBe(true)
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
      "```w excerpt\n// excerpt-kind: composed\n// entrada: dois valores Order",
      "```w excerpt\n// excerpt-kind: composed\n// excerpt-kind: composed\n// entrada: dois valores Order",
    )
    expect(errorsFor(duplicate).some((error) => error.includes("duplicate excerpt metadata"))).toBe(true)

    const invalid = mutate(
      cheatsheet,
      "// excerpt-kind: contrafactual\nlet lease = try await ovens.acquire",
      "// excerpt-kind: unknown\nlet lease = try await ovens.acquire",
    )
    expect(errorsFor(invalid).some((error) => error.includes("invalid excerpt metadata"))).toBe(true)
  })
})
