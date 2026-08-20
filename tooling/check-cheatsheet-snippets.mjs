import fs from "node:fs"
import os from "node:os"
import path from "node:path"
import { spawnSync } from "node:child_process"

const repositoryRoot = path.resolve(import.meta.dir, "..")
const grammarPath = path.join(repositoryRoot, "tooling", "tree-sitter-w")
const excerptKinds = new Set(["composed", "contrafactual", "manifest-fragment"])
const metadataPattern = /^\/\/ excerpt-(?:source|kind):/u

function normalizeLf(value) {
  return value.replace(/\r\n?/gu, "\n")
}

function lineNumberedError(fence, message) {
  return `CHEATSHEET.md:${fence.startLine}: ${message}`
}

function parseFenceInfo(info) {
  const normalized = info.trim()
  if (normalized === "w") return "w"
  if (normalized === "w excerpt") return "w excerpt"
  if (normalized === "text") return "text"
  return null
}

/**
 * Extract fenced blocks and report malformed or unsupported fence metadata.
 * The parser intentionally accepts only the three documented fence infos.
 */
export function extractFences(markdown) {
  const lines = normalizeLf(markdown).split("\n")
  const fences = []
  const errors = []
  let open = null

  for (let index = 0; index < lines.length; index += 1) {
    const line = lines[index]
    const marker = line.match(/^\s*(`{3,}|~{3,})([^`]*)$/u)
    if (!open && marker) {
      const markerText = marker[1]
      open = {
        char: markerText[0],
        length: markerText.length,
        info: marker[2].trim(),
        startLine: index + 1,
        body: [],
      }
      continue
    }

    if (open) {
      const close = line.match(/^\s*(`{3,}|~{3,})\s*$/u)
      if (close && close[1][0] === open.char && close[1].length >= open.length) {
        const fence = { ...open, endLine: index + 1, body: open.body.join("\n") }
        fence.infoKind = parseFenceInfo(fence.info)
        if (fence.infoKind === null) {
          errors.push(lineNumberedError(fence, `unknown fence info ${JSON.stringify(fence.info || "(empty)")}`))
        }
        fences.push(fence)
        open = null
        continue
      }
      open.body.push(line)
    }
  }

  if (open) {
    errors.push(`CHEATSHEET.md:${open.startLine}: unclosed ${open.char.repeat(open.length)} fence`)
  }

  return { fences, errors }
}

function pathIsWithin(root, candidate) {
  const relative = path.relative(root, candidate)
  return relative === "" || (relative !== ".." && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative))
}

export function validateExcerptMetadata(fence, repository, fileSystem = fs) {
  const lines = fence.body.split("\n")
  const first = lines[0] ?? ""
  const errors = []
  const body = lines.slice(1).join("\n")

  if (!first.startsWith("// excerpt-")) {
    errors.push(lineNumberedError(fence, "w excerpt requires one metadata line first"))
    return { errors, body, kind: null }
  }

  const sourceMatch = first.match(/^\/\/ excerpt-source: ([^\s]+)::(.*)$/u)
  const kindMatch = first.match(/^\/\/ excerpt-kind: (composed|contrafactual|manifest-fragment)$/u)
  if (!sourceMatch && !kindMatch) {
    errors.push(lineNumberedError(fence, "invalid excerpt metadata"))
    return { errors, body, kind: null }
  }

  const metadataLines = lines.filter((line, index) => index > 0 && metadataPattern.test(line))
  if (metadataLines.length > 0) {
    errors.push(lineNumberedError(fence, "duplicate excerpt metadata"))
  }
  if (body.length === 0) {
    errors.push(lineNumberedError(fence, "w excerpt body must not be empty"))
  }

  if (kindMatch) {
    const kind = kindMatch[1]
    if (!excerptKinds.has(kind)) {
      errors.push(lineNumberedError(fence, `invalid excerpt kind ${kind}`))
    }
    return { errors, body, kind }
  }

  const sourcePath = sourceMatch[1]
  const symbol = sourceMatch[2]
  let unsafePath = false
  const rejectPath = (message) => {
    errors.push(lineNumberedError(fence, message))
    unsafePath = true
  }
  if (/^[A-Za-z][A-Za-z0-9+.-]*:/u.test(sourcePath) || sourcePath.startsWith("/") || sourcePath.startsWith("\\")) {
    rejectPath("excerpt source path must be relative")
  }
  if (sourcePath.split(/[\\/]/u).some((part) => part === ".." || part === "")) {
    rejectPath("excerpt source path must not escape the repository")
  }
  if (!sourcePath.endsWith(".w")) {
    rejectPath("excerpt source path must end in .w")
  }

  const resolved = path.resolve(repository, sourcePath.replace(/\//gu, path.sep))
  if (!pathIsWithin(repository, resolved)) {
    rejectPath("excerpt source path must stay inside the repository")
  }
  const relative = path.relative(repository, resolved).split(path.sep).join("/")
  const segments = relative.split("/")
  if (segments.includes("history") || relative === "tooling/tree-sitter-w/src" || relative.startsWith("tooling/tree-sitter-w/src/")) {
    rejectPath("excerpt source path must not use history or generated parser sources")
  }
  if (unsafePath) {
    return { errors, body, kind: "source", sourcePath, symbol }
  }

  if (!fileSystem.existsSync(resolved)) {
    errors.push(lineNumberedError(fence, `excerpt source file does not exist: ${sourcePath}`))
    return { errors, body, kind: "source", sourcePath, symbol }
  }

  let realRepository
  let realCandidate
  try {
    realRepository = fileSystem.realpathSync(repository)
    realCandidate = fileSystem.realpathSync(resolved)
  } catch {
    errors.push(lineNumberedError(fence, `excerpt source path cannot be resolved: ${sourcePath}`))
    return { errors, body, kind: "source", sourcePath, symbol }
  }
  if (!pathIsWithin(realRepository, realCandidate)) {
    errors.push(lineNumberedError(fence, "excerpt source path must stay inside the repository after realpath resolution"))
    return { errors, body, kind: "source", sourcePath, symbol }
  }
  if (!fileSystem.statSync(realCandidate).isFile()) {
    errors.push(lineNumberedError(fence, `excerpt source file does not exist: ${sourcePath}`))
    return { errors, body, kind: "source", sourcePath, symbol }
  }

  const source = normalizeLf(fileSystem.readFileSync(realCandidate, "utf8"))
  if (!symbol.trim()) {
    errors.push(lineNumberedError(fence, "excerpt source symbol must not be empty"))
  } else if (!source.includes(symbol)) {
    errors.push(lineNumberedError(fence, `excerpt source symbol is missing: ${symbol}`))
  }
  if (!source.includes(normalizeLf(body))) {
    errors.push(lineNumberedError(fence, `excerpt body is not LF-normalized exact content in ${sourcePath}`))
  }
  return { errors, body, kind: "source", sourcePath, symbol }
}

/**
 * Validate fence shape and excerpt provenance without invoking Tree-sitter.
 * This is exported so unit tests can exercise mutations without a compiler.
 */
export function validateCheatsheetText(markdown, options = {}) {
  const repository = path.resolve(options.repositoryRoot ?? repositoryRoot)
  const extracted = extractFences(markdown)
  const errors = [...extracted.errors]
  const units = []
  const excerpts = []
  const counts = { w: 0, excerpt: 0, source: 0, composed: 0, contrafactual: 0, "manifest-fragment": 0, text: 0 }

  for (const fence of extracted.fences) {
    if (fence.infoKind === "w") {
      counts.w += 1
      if (fence.body.trim().length === 0) {
        errors.push(lineNumberedError(fence, "plain w fence body must not be empty or whitespace-only"))
        continue
      }
      units.push(fence)
      continue
    }
    if (fence.infoKind === "w excerpt") {
      counts.excerpt += 1
      const metadata = validateExcerptMetadata(fence, repository)
      errors.push(...metadata.errors)
      if (metadata.kind === "source") counts.source += 1
      else if (metadata.kind) counts[metadata.kind] += 1
      excerpts.push({ fence, ...metadata })
      continue
    }
    if (fence.infoKind === "text") {
      counts.text += 1
    }
  }

  return { errors, fences: extracted.fences, units, excerpts, counts }
}

function findTreeSitter(root) {
  const candidates = process.platform === "win32"
    ? [
        path.join(root, "tooling", "tree-sitter-w", "node_modules", "tree-sitter-cli", "tree-sitter.exe"),
        path.join(root, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter.exe"),
      ]
    : [
        path.join(root, "tooling", "tree-sitter-w", "node_modules", "tree-sitter-cli", "tree-sitter"),
        path.join(root, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter"),
      ]
  return candidates.find((candidate) => fs.existsSync(candidate)) ?? "tree-sitter"
}

function parseUnits(units, options) {
  if (units.length === 0) return { errors: [], output: "", parsedCount: 0 }
  const root = path.resolve(options.repositoryRoot ?? repositoryRoot)
  const grammar = path.resolve(options.grammarPath ?? path.join(root, "tooling", "tree-sitter-w"))
  const treeSitter = options.treeSitterPath ?? findTreeSitter(root)
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "w-cheatsheet-"))
  const files = []
  try {
    for (const [index, unit] of units.entries()) {
      const file = path.join(temporary, `snippet-${String(index + 1).padStart(3, "0")}.w`)
      fs.writeFileSync(file, unit.body.endsWith("\n") ? unit.body : `${unit.body}\n`, "utf8")
      files.push(file)
    }

    const result = spawnSync(treeSitter, ["parse", "--grammar-path", grammar, "--no-ranges", ...files], {
      cwd: grammar,
      encoding: "utf8",
      windowsHide: true,
    })
    const stdout = result.stdout ?? ""
    const stderr = result.stderr ?? ""
    const errors = []
    const rootCount = (stdout.match(/\(source_file(?:\s|\)|\r?\n)/gu) ?? []).length
    if ((result.status ?? result.exitCode ?? 1) !== 0) {
      errors.push(`Tree-sitter parse failed: ${stderr.trim() || `exit ${result.status ?? result.exitCode}`}`)
    }
    if (rootCount !== units.length) {
      errors.push(`Tree-sitter source-file count ${rootCount} does not match w fence count ${units.length}`)
    }
    const recovery = stdout.match(/\((?:ERROR|MISSING)(?:\s|\)|\r?\n)/gu) ?? []
    if (recovery.length > 0) {
      errors.push(`Tree-sitter reported ${recovery.length} ERROR/MISSING recovery node(s)`)
    }
    return { errors, output: stdout, parsedCount: rootCount }
  } finally {
    fs.rmSync(temporary, { recursive: true, force: true })
  }
}

export function checkCheatsheet(options = {}) {
  const file = path.resolve(options.file ?? path.join(repositoryRoot, "CHEATSHEET.md"))
  const markdown = options.markdown ?? fs.readFileSync(file, "utf8")
  const structural = validateCheatsheetText(markdown, options)
  const errors = [...structural.errors]
  let parser = { errors: [], parsedCount: 0 }
  if (errors.length === 0 && options.parse !== false) {
    parser = parseUnits(structural.units, options)
    errors.push(...parser.errors)
  }
  return { ...structural, parser, errors, file }
}

function main() {
  const result = checkCheatsheet({ parse: true })
  if (result.errors.length > 0) {
    process.stderr.write(`${result.errors.join("\n")}\n`)
    process.exitCode = 1
    return
  }
  const { counts } = result
  process.stdout.write(`Cheatsheet snippets: ${counts.w} source units parsed, ${counts.source} source-backed excerpts, ${counts.composed} composed, ${counts.contrafactual} contrafactual, ${counts["manifest-fragment"]} manifest fragments.\n`)
}

if (import.meta.main) main()
