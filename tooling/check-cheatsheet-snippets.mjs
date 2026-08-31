import fs from "node:fs"
import os from "node:os"
import path from "node:path"
import { spawnSync } from "node:child_process"

const repositoryRoot = path.resolve(import.meta.dir, "..")
const grammarPath = path.join(repositoryRoot, "tooling", "tree-sitter-w")
const excerptKinds = new Set(["composed", "contrafactual", "manifest-fragment"])
const exampleRoles = new Set(["executable", "logical-contract", "signature-reference"])
const observableKinds = new Set(["value", "effect", "diagnostic"])
const excerptMetadataPattern = /^\/\/ excerpt-(?:source|kind):/u
const declarationKeywords = new Set([
  "behavior",
  "dimension",
  "enum",
  "entry",
  "extension",
  "fn",
  "object",
  "protocol",
  "service",
  "struct",
  "type",
  "alias",
  "unit",
])

function normalizeLf(value) {
  return value.replace(/\r\n?/gu, "\n")
}

function lineNumberedError(fence, message) {
  return `CHEATSHEET.md:${fence.startLine}: ${message}`
}

function parseFenceInfo(info) {
  const normalized = info.trim()
  if (normalized === "w") return { kind: "w", role: null, uses: [], observable: null, errors: [] }
  if (normalized === "w excerpt") return { kind: "w excerpt", role: null, uses: [], observable: null, errors: [] }
  if (normalized === "text") return { kind: "text", role: null, uses: [], observable: null, errors: [] }

  const baseMatch = normalized.match(/^(w excerpt|w|text)\s+(.+)$/u)
  if (!baseMatch) return null
  const [base, ...fields] = baseMatch[2].split(/\s+/u)
  const errors = []
  const role = base
  if (!exampleRoles.has(role)) return null
  if (baseMatch[1] === "text" && role !== "logical-contract") {
    errors.push("text fences only accept logical-contract role")
  }
  const values = { role, uses: [], observable: null, errors }
  const seen = new Set(["role"])
  for (const field of fields) {
    const separator = field.indexOf("=")
    const key = separator < 0 ? field : field.slice(0, separator)
    const value = separator < 0 ? "" : field.slice(separator + 1)
    if (seen.has(key)) {
      errors.push(`duplicate fence metadata key ${key}`)
      continue
    }
    seen.add(key)
    if (key === "use") {
      if (!value || !value.split(",").every((name) => /^[A-Za-z_][A-Za-z0-9_]*$/u.test(name))) {
        errors.push("fence use must list identifier names separated by commas")
      } else {
        values.uses = value.split(",")
      }
    } else if (key === "observable") {
      if (!observableKinds.has(value)) errors.push(`invalid fence observable ${value || "(empty)"}`)
      else values.observable = value
    } else {
      errors.push(`unknown fence metadata key ${key}`)
    }
  }
  if ((role === "logical-contract" || role === "signature-reference") && (values.uses.length > 0 || values.observable !== null)) {
    errors.push("logical contract or signature reference cannot define use or observable")
  }
  if (role === "executable") {
    if (values.uses.length === 0 && !fields.some((field) => field === "use=")) errors.push("executable fence requires use=...")
    if (values.observable === null && !fields.some((field) => field === "observable=")) errors.push("executable fence requires observable=...")
  }
  return { kind: baseMatch[1], ...values }
}

function maskWSource(source) {
  const chars = source.split("")
  let state = "code"
  for (let index = 0; index < chars.length; index += 1) {
    const current = chars[index]
    const next = chars[index + 1]
    if (state === "line-comment") {
      if (current === "\n" || current === "\r") state = "code"
      else chars[index] = " "
      continue
    }
    if (state === "block-comment") {
      if (current === "*" && next === "/") {
        chars[index] = " "
        chars[index + 1] = " "
        index += 1
        state = "code"
      } else if (current !== "\n" && current !== "\r") {
        chars[index] = " "
      }
      continue
    }
    if (state === "string" || state === "scalar") {
      if (current === "\\") {
        chars[index] = " "
        if (index + 1 < chars.length && chars[index + 1] !== "\n" && chars[index + 1] !== "\r") {
          chars[index + 1] = " "
          index += 1
        }
      } else if ((state === "string" && current === '"') || (state === "scalar" && current === "'")) {
        chars[index] = " "
        state = "code"
      } else if (current !== "\n" && current !== "\r") {
        chars[index] = " "
      }
      continue
    }
    if (current === "/" && next === "/") {
      chars[index] = " "
      chars[index + 1] = " "
      index += 1
      state = "line-comment"
    } else if (current === "/" && next === "*") {
      chars[index] = " "
      chars[index + 1] = " "
      index += 1
      state = "block-comment"
    } else if (current === '"') {
      chars[index] = " "
      state = "string"
    } else if (current === "'") {
      chars[index] = " "
      state = "scalar"
    }
  }
  return chars.join("")
}

function scanWTokens(source) {
  const masked = maskWSource(source)
  const tokens = []
  let depth = 0
  let index = 0
  while (index < masked.length) {
    const current = masked[index]
    if (current === "{") {
      depth += 1
      index += 1
      continue
    }
    if (current === "}") {
      depth = Math.max(0, depth - 1)
      index += 1
      continue
    }
    if (/[A-Za-z_]/u.test(current)) {
      const start = index
      index += 1
      while (index < masked.length && /[A-Za-z0-9_]/u.test(masked[index])) index += 1
      tokens.push({ word: masked.slice(start, index), start, end: index, depth })
      continue
    }
    index += 1
  }
  return { masked, tokens }
}

function matchingBrace(source, openIndex) {
  let depth = 0
  for (let index = openIndex; index < source.length; index += 1) {
    if (source[index] === "{") depth += 1
    else if (source[index] === "}") {
      depth -= 1
      if (depth === 0) return index + 1
    }
  }
  return source.length
}

function findDeclarationScopes(source) {
  const { masked, tokens } = scanWTokens(source)
  const declarations = []
  const consumers = []
  for (let index = 0; index < tokens.length; index += 1) {
    const token = tokens[index]
    if (token.depth !== 0) continue
    const keyword = token.word
    if (!declarationKeywords.has(keyword) && keyword !== "test") continue
    const next = tokens[index + 1]
    if (!next) continue
    const open = masked.indexOf("{", token.end)
    const lineEnd = masked.indexOf("\n", token.end)
    const statementEnd = lineEnd < 0 ? source.length : lineEnd
    const hasBody = open >= 0 && (lineEnd < 0 || open < statementEnd || keyword === "test")
    const end = hasBody ? matchingBrace(masked, open) : statementEnd
    const name = keyword === "test" ? null : next.word
    const declaration = { keyword, name, start: token.start, end, bodyStart: hasBody ? open + 1 : end }
    if (keyword !== "test" && keyword !== "entry" && name) declarations.push(declaration)
    if (keyword === "test") consumers.push(declaration)
    if (keyword === "entry") consumers.push(declaration)
  }
  return { masked, declarations, consumers }
}

function scopeText(source, scope, declarations) {
  let text = source.slice(scope.start, scope.end)
  if (scope.keyword === "entry") {
    const entryTarget = source.slice(scope.start, scope.end).match(/\bentry(?:\s+[A-Za-z_][A-Za-z0-9_]*)?\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)/u)?.[1]
    const target = declarations.find((declaration) => declaration.name === entryTarget)
    if (target) text += source.slice(target.start, target.end)
  }
  return text
}

function hasApplicationUse(text, name) {
  text = maskWSource(text)
  const escaped = name.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&")
  const call = new RegExp(`\\b${escaped}\\s*(?:<[^{};\\n]*>)?\\s*\\(`, "u")
  const assignment = new RegExp(`\\b${escaped}\\s*(?:\\+=|-=|\\*=|/=|%=|=)`, "u")
  const member = new RegExp(`\\.${escaped}\\b`, "u")
  const typed = new RegExp(`(?:\\b(?:let|var|ref|inout)\\s+\\w+\\s*:\\s*|\\bvar\\s+)${escaped}\\b`, "u")
  const entry = new RegExp(`\\bentry\\s*\\(\\s*${escaped}\\b`, "u")
  return call.test(text) || assignment.test(text) || member.test(text) || typed.test(text) || entry.test(text)
}

function hasTypedApplication(source, name) {
  const escaped = name.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&")
  const masked = maskWSource(source)
  return new RegExp(`(?:\\b(?:let|var|ref|inout)\\s+\\w+\\s*:\\s*|\\bvar\\s+)${escaped}\\b`, "u").test(masked)
}

function hasObservable(text, kind) {
  const masked = maskWSource(text)
  if (kind === "effect") {
    for (const match of masked.matchAll(/\bprint\s*\(/gu)) {
      const open = match.index + match[0].lastIndexOf("(")
      const close = masked.indexOf(")", open + 1)
      const argument = text.slice(open + 1, close < 0 ? text.length : close).trim()
      if (argument && !/^(?:"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')$/u.test(argument)) return true
    }
    // A literal entry output is an observable process effect. A literal-only
    // print in a test is not enough to close an illustrative example.
    return /\bentry(?:\s+[A-Za-z_][A-Za-z0-9_]*)?\s*\(/u.test(masked) && /\bprint\s*\(/u.test(masked)
  }
  text = masked
  if (kind === "diagnostic") {
    if (/\bexpect\s+diagnostic\b/u.test(text)) return true
    return /\bcatch\b/u.test(text) && /\bexpect\b/u.test(text)
  }
  return /\bexpect\b/u.test(text)
}

function reachableDeclarations(source, scopes) {
  const reachable = new Set()
  const consumerTexts = scopes.consumers.map((scope) => scopeText(source, scope, scopes.declarations))
  for (const declaration of scopes.declarations) {
    if (consumerTexts.some((text) => hasApplicationUse(text, declaration.name))) reachable.add(declaration.name)
  }
  let changed = true
  while (changed) {
    changed = false
    for (const declaration of scopes.declarations) {
      if (!reachable.has(declaration.name)) continue
      const body = source.slice(declaration.bodyStart, declaration.end)
      for (const candidate of scopes.declarations) {
        if (reachable.has(candidate.name) || candidate.name === declaration.name) continue
        if (hasApplicationUse(body, candidate.name)) {
          reachable.add(candidate.name)
          changed = true
        }
      }
    }
  }
  return reachable
}

function validateExecutableExample(fence, source, metadata) {
  const errors = []
  if (metadata.role !== "executable") return errors
  if (metadata.uses.length === 0) errors.push(lineNumberedError(fence, "executable W fence requires example-use metadata"))
  if (metadata.observable === null) errors.push(lineNumberedError(fence, "executable W fence requires example-observable metadata"))
  const scopes = findDeclarationScopes(source)
  const localNames = new Set(scopes.declarations.map((declaration) => declaration.name))
  for (const name of localNames) {
    if (!metadata.uses.includes(name)) errors.push(lineNumberedError(fence, `declaration ${name} is missing from example-use metadata`))
  }
  if (scopes.consumers.length === 0) {
    errors.push(lineNumberedError(fence, "executable declaration requires a test or entry consumer"))
    return errors
  }
  const consumerTexts = scopes.consumers.map((scope) => scopeText(source, scope, scopes.declarations))
  const reachable = reachableDeclarations(source, scopes)
  for (const name of metadata.uses) {
    const declaration = scopes.declarations.find((candidate) => candidate.name === name)
    if (!declaration) {
      if (!consumerTexts.some((text) => hasApplicationUse(text, name))) {
        errors.push(lineNumberedError(fence, `example-use ${name} has no application in a test or entry consumer`))
      }
      continue
    }
    const appliedAsBehavior = declaration?.keyword === "behavior" && scopes.declarations.some((candidate) => {
      if (candidate.name === name) return false
      return hasTypedApplication(source.slice(candidate.bodyStart, candidate.end), name)
    })
    if (!reachable.has(name) && !appliedAsBehavior) {
      errors.push(lineNumberedError(fence, `example-use ${name} has no application in a test or entry consumer`))
    }
  }
  if (metadata.observable !== null && !consumerTexts.some((text) => hasObservable(text, metadata.observable))) {
    errors.push(lineNumberedError(fence, `example consumer has no observable ${metadata.observable} terminal`))
  }
  return errors
}

function validateFenceRole(fence, source, metadata) {
  const role = metadata?.role ?? null
  if (role === "logical-contract" || role === "signature-reference") return []
  const declarations = findDeclarationScopes(source).declarations
  if (declarations.length === 0) return []
  if (metadata.role !== "executable") {
    return [lineNumberedError(fence, "declaration-bearing W fence requires example-role: executable or an explicit logical-contract/signature-reference exemption")]
  }
  return validateExecutableExample(fence, source, metadata)
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
        const parsedInfo = parseFenceInfo(fence.info)
        fence.infoKind = parsedInfo?.kind ?? null
        fence.exampleMetadata = parsedInfo ?? { role: null, uses: [], observable: null, errors: [] }
        if (parsedInfo === null) {
          errors.push(lineNumberedError(fence, `unknown fence info ${JSON.stringify(fence.info || "(empty)")}`))
        } else {
          for (const error of parsedInfo.errors) errors.push(lineNumberedError(fence, error))
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
  const kindMatch = first.match(/^\/\/ excerpt-kind: ([^\s]+)$/u)
  if (!sourceMatch && !kindMatch) {
    errors.push(lineNumberedError(fence, "invalid excerpt metadata"))
    return { errors, body, kind: null }
  }

  const metadataLines = lines.filter((line, index) => index > 0 && excerptMetadataPattern.test(line))
  if (metadataLines.length > 0) {
    errors.push(lineNumberedError(fence, "duplicate excerpt metadata"))
  }
  if (body.length === 0) {
    errors.push(lineNumberedError(fence, "w excerpt body must not be empty"))
  }

  if (kindMatch) {
    const kind = kindMatch[1]
    if (!excerptKinds.has(kind)) {
      errors.push(lineNumberedError(fence, `invalid excerpt metadata: invalid excerpt kind ${kind}`))
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
    return { errors, body, kind: "source", symbol, sourcePath }
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
  const counts = {
    w: 0,
    excerpt: 0,
    source: 0,
    composed: 0,
    contrafactual: 0,
    "manifest-fragment": 0,
    text: 0,
  }

  for (const fence of extracted.fences) {
    if (fence.infoKind === "w") {
      counts.w += 1
      if (fence.body.trim().length === 0) {
        errors.push(lineNumberedError(fence, "plain w fence body must not be empty or whitespace-only"))
        continue
      }
      const metadata = fence.exampleMetadata
      errors.push(...metadata.errors)
      if (metadata.role === null) errors.push(lineNumberedError(fence, "W fence requires a role in the fence info"))
      errors.push(...validateFenceRole(fence, fence.body, metadata))
      units.push(fence)
      continue
    }
    if (fence.infoKind === "w excerpt") {
      counts.excerpt += 1
      const metadata = validateExcerptMetadata(fence, repository)
      errors.push(...metadata.errors)
      if (metadata.kind === "source") counts.source += 1
      else if (metadata.kind && Object.hasOwn(counts, metadata.kind)) counts[metadata.kind] += 1
      if (fence.exampleMetadata.role === null) errors.push(lineNumberedError(fence, "W excerpt requires a role in the fence info"))
      errors.push(...fence.exampleMetadata.errors)
      errors.push(...validateFenceRole(fence, metadata.body, fence.exampleMetadata))
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
