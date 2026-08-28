import { readdir, readFile } from "node:fs/promises"
import { join, relative, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const testsDirectory = resolve(root, "compiler", "seed-c", "tests")
const controlWords = new Set(["if", "for", "while", "switch"])
const returnTypeWords = new Set([
  "_Bool", "bool", "char", "double", "float", "int", "long", "short",
  "signed", "size_t", "struct", "uint8_t", "unsigned", "void",
])
const punctuators = [
  ">>>=", "<<=", ">>=", "...", "->", "++", "--", "&&", "||", "==", "!=",
  "<=", ">=", "+=", "-=", "*=", "/=", "%=", "&=", "^=", "|=", "<<", ">>",
  "##", "=>",
]

function advanceLine(source, state, end) {
  for (; state.offset < end; state.offset += 1) {
    if (source[state.offset] === "\n") {
      state.line += 1
      state.atLineStart = true
    }
    else if (state.atLineStart && /\s/u.test(source[state.offset])) continue
    else state.atLineStart = false
  }
}

function consumeQuoted(source, state, quote) {
  const start = state.offset
  const startLine = state.line
  state.offset += 1
  while (state.offset < source.length) {
    const value = source[state.offset]
    if (value === "\\") {
      state.offset += 2
      continue
    }
    state.offset += 1
    if (value === "\n") state.line += 1
    if (value === quote) break
  }
  return { kind: "literal", value: source.slice(start, state.offset), offset: start, line: startLine }
}

function consumePreprocessor(source, state) {
  while (state.offset < source.length) {
    const value = source[state.offset]
    const newline = value === "\n"
    state.offset += 1
    if (newline) {
      state.line += 1
      const continued = state.offset >= 2 && source[state.offset - 2] === "\\"
      if (!continued) {
        state.atLineStart = true
        return
      }
    }
  }
  state.atLineStart = true
}

export function tokenizeC(source) {
  const tokens = []
  const state = { offset: 0, atLineStart: true, line: 1 }
  while (state.offset < source.length) {
    const start = state.offset
    const value = source[state.offset]
    if (/\s/u.test(value)) {
      state.offset += 1
      if (value === "\n") {
        state.line += 1
        state.atLineStart = true
      }
      continue
    }
    if (state.atLineStart && value === "#") {
      consumePreprocessor(source, state)
      continue
    }
    if (value === "/" && source[state.offset + 1] === "/") {
      const end = source.indexOf("\n", state.offset + 2)
      state.offset = end < 0 ? source.length : end
      continue
    }
    if (value === "/" && source[state.offset + 1] === "*") {
      const end = source.indexOf("*/", state.offset + 2)
      const finish = end < 0 ? source.length : end + 2
      advanceLine(source, state, finish)
      continue
    }
    if (value === '"' || value === "'") {
      tokens.push(consumeQuoted(source, state, value))
      state.atLineStart = false
      continue
    }
    if (/[A-Za-z_]/u.test(value)) {
      state.offset += 1
      while (state.offset < source.length && /[A-Za-z0-9_]/u.test(source[state.offset])) {
        state.offset += 1
      }
      tokens.push({ kind: "identifier", value: source.slice(start, state.offset), offset: start, line: state.line })
      state.atLineStart = false
      continue
    }
    if (/[0-9]/u.test(value)) {
      state.offset += 1
      while (state.offset < source.length && /[A-Za-z0-9_.]/u.test(source[state.offset])) {
        state.offset += 1
      }
      tokens.push({ kind: "number", value: source.slice(start, state.offset), offset: start, line: state.line })
      state.atLineStart = false
      continue
    }
    const punctuator = punctuators.find((candidate) => source.startsWith(candidate, state.offset))
    if (punctuator !== undefined) {
      state.offset += punctuator.length
      tokens.push({ kind: "punctuator", value: punctuator, offset: start, line: state.line })
      state.atLineStart = false
      continue
    }
    state.offset += 1
    tokens.push({ kind: "punctuator", value, offset: start, line: state.line })
    state.atLineStart = false
  }
  return tokens
}

function delimiterPairs(tokens) {
  const pairs = new Map()
  const reverse = new Map()
  const stacks = new Map([["(", []], ["{", []], ["[", []]])
  const closing = new Map([[")", "("], ["}", "{"], ["]", "["]])
  for (let index = 0; index < tokens.length; index += 1) {
    const value = tokens[index].value
    if (stacks.has(value)) {
      stacks.get(value).push(index)
      continue
    }
    const opening = closing.get(value)
    if (opening === undefined) continue
    const stack = stacks.get(opening)
    if (stack.length === 0) continue
    const start = stack.pop()
    pairs.set(start, index)
    reverse.set(index, start)
  }
  return { pairs, reverse }
}

function previousBoundary(tokens, index) {
  for (let cursor = index - 1; cursor >= 0; cursor -= 1) {
    if ([";", "}", "{"].includes(tokens[cursor].value)) return cursor
  }
  return -1
}

function functionDefinitionCandidates(tokens, delimiters) {
  const candidates = []
  for (let brace = 0; brace < tokens.length; brace += 1) {
    if (tokens[brace].value !== "{") continue
    const closeParen = brace - 1
    if (tokens[closeParen]?.value !== ")") continue
    const openParen = delimiters.reverse.get(closeParen)
    if (openParen === undefined) continue
    const nameToken = tokens[openParen - 1]
    if (nameToken?.kind !== "identifier" || controlWords.has(nameToken.value)) continue
    const closeBrace = delimiters.pairs.get(brace)
    if (closeBrace === undefined) continue
    candidates.push({
      nameToken,
      brace,
      closeBrace,
      headerStart: previousBoundary(tokens, openParen),
      openParen,
    })
  }
  return candidates
}

function functionDefinitions(tokens, delimiters) {
  const functions = []
  for (const candidate of functionDefinitionCandidates(tokens, delimiters)) {
    const { nameToken, brace, closeBrace, headerStart, openParen } = candidate
    const header = tokens.slice(headerStart + 1, openParen - 1)
    const hasReturnType = nameToken.value === "main" ||
      header.some((token) => returnTypeWords.has(token.value))
    if (!hasReturnType) continue
    functions.push({
      name: nameToken.value,
      returnType: header.map((token) => token.value).join(" "),
      isBool: header.some((token) => token.value === "bool" || token.value === "_Bool"),
      line: lineNumberForToken(nameToken),
      bodyStart: brace + 1,
      bodyEnd: closeBrace,
    })
  }
  return functions
}

function mainDefinitionCandidates(tokens, delimiters) {
  const candidates = []
  for (let index = 0; index < tokens.length; index += 1) {
    if (tokens[index].kind !== "identifier" || tokens[index].value !== "main") continue
    const openParen = index + 1
    if (tokens[openParen]?.value !== "(") continue
    const closeParen = delimiters.pairs.get(openParen)
    if (closeParen === undefined || tokens[closeParen + 1]?.value !== "{") continue
    const closeBrace = delimiters.pairs.get(closeParen + 1)
    if (closeBrace === undefined) continue
    candidates.push({
      nameToken: tokens[index],
      brace: closeParen + 1,
      closeBrace,
    })
  }
  return candidates
}

function lineNumberForToken(token) {
  return token.line
}

function macroDefinitions(source) {
  const lines = source.split("\n")
  const macros = []
  for (let index = 0; index < lines.length; index += 1) {
    const match = /^\s*#\s*define\s+([A-Za-z_]\w*)(?:\s*\([^\n]*?\))?(?:\s+(.*))?\s*$/u.exec(lines[index])
    if (match === null) continue
    const startLine = index + 1
    const body = [match[2] ?? ""]
    while (body.at(-1)?.endsWith("\\")) {
      body[body.length - 1] = body.at(-1).slice(0, -1)
      index += 1
      if (index >= lines.length) break
      body.push(lines[index])
    }
    const macroTokens = tokenizeC(body.join(" "))
    macros.push({
      name: match[1],
      line: startLine,
      hasReturn: macroTokens.some((token) => token.value === "return"),
      returnsFalse: containsFalseReturn(macroTokens),
    })
  }
  return macros
}

function containsFalseReturn(tokens, start = 0, end = tokens.length) {
  for (let index = start; index < end; index += 1) {
    if (tokens[index].value !== "return") continue
    let cursor = index + 1
    if (tokens[cursor]?.value === "(") cursor += 1
    if (tokens[cursor]?.value !== "false") continue
    cursor += 1
    while (tokens[cursor]?.value === ")") cursor += 1
    if (tokens[cursor]?.value === ";" || cursor === end) return true
  }
  return false
}

function simpleFalseReturn(tokens, index, delimiters) {
  if (tokens[index]?.value !== "return") return false
  let cursor = index + 1
  if (tokens[cursor]?.value === "(") {
    const close = delimiters.pairs.get(cursor)
    if (close === undefined) return false
    if (tokens[cursor + 1]?.value !== "false" || close !== cursor + 2) return false
    cursor = close + 1
  } else if (tokens[cursor]?.value === "false") {
    cursor += 1
  } else {
    return false
  }
  return tokens[cursor]?.value === ";"
}

function directBoolReturn(tokens, index, boolNames, delimiters) {
  if (tokens[index]?.value !== "return") return false
  let cursor = index + 1
  let wrapped = false
  if (tokens[cursor]?.value === "(") {
    wrapped = true
    cursor += 1
  }
  const name = tokens[cursor]?.value
  if (!boolNames.has(name) || tokens[cursor + 1]?.value !== "(") return false
  const close = delimiters.pairs.get(cursor + 1)
  if (close === undefined) return false
  cursor = close + 1
  if (wrapped) {
    if (tokens[cursor]?.value !== ")") return false
    cursor += 1
  }
  return tokens[cursor]?.value === ";"
}

function boolVariableNames(tokens, start, end) {
  const names = new Set()
  for (let index = start; index + 1 < end; index += 1) {
    if (tokens[index].value !== "bool" && tokens[index].value !== "_Bool") continue
    if (tokens[index + 1].kind === "identifier") names.add(tokens[index + 1].value)
  }
  return names
}

function discardedBoolCall(tokens, index, bodyStart, bodyEnd, delimiters) {
  const open = index + 1
  if (tokens[open]?.value !== "(") return false
  const close = delimiters.pairs.get(open)
  if (close === undefined || tokens[close + 1]?.value !== ";") return false
  const previous = tokens[index - 1]?.value
  if (index === bodyStart || ["{", ";", "}", ":", "else", "do"].includes(previous)) return true
  if (previous !== ")") return false
  const conditionOpen = delimiters.reverse.get(index - 1)
  if (conditionOpen === undefined) return false
  const conditionWord = tokens[conditionOpen - 1]?.value
  if (controlWords.has(conditionWord)) return true
  if (tokens[conditionOpen + 1]?.value === "void") return true
  return false
}

function finding(file, token, kind, detail) {
  return { file, line: token.line, kind, ...detail }
}

export function analyzeCSource(source, file = "<memory>") {
  const tokens = tokenizeC(source)
  const delimiters = delimiterPairs(tokens)
  const candidates = mainDefinitionCandidates(tokens, delimiters)
  const functions = functionDefinitions(tokens, delimiters)
  const mains = functions.filter((item) => item.name === "main")
  const boolNames = new Set(functions.filter((item) => item.isBool).map((item) => item.name))
  const macros = macroDefinitions(source)
  const macroByName = new Map(macros.map((item) => [item.name, item]))
  const findings = []
  const coveredMains = new Set(mains.map((item) => `${item.bodyStart}:${item.bodyEnd}`))
  for (const candidate of candidates) {
    const key = `${candidate.brace + 1}:${candidate.closeBrace}`
    if (!coveredMains.has(key)) {
      findings.push(finding(file, candidate.nameToken, "uncovered-main-definition", {
        function: candidate.nameToken.value,
      }))
    }
  }
  for (const main of mains) {
    const variables = boolVariableNames(tokens, main.bodyStart, main.bodyEnd)
    for (let index = main.bodyStart; index < main.bodyEnd; index += 1) {
      const token = tokens[index]
      if (simpleFalseReturn(tokens, index, delimiters)) {
        findings.push(finding(file, token, "return-false-in-main", { function: main.name }))
      }
      if (directBoolReturn(tokens, index, boolNames, delimiters)) {
        findings.push(finding(file, token, "return-bool-helper-in-main", { function: main.name }))
      }
      if (token.value === "return" && tokens[index + 1]?.kind === "identifier" &&
          variables.has(tokens[index + 1].value) && tokens[index + 2]?.value === ";") {
        findings.push(finding(file, token, "return-bool-variable-in-main", { function: main.name }))
      }
      const macro = macroByName.get(token.value)
      if (macro !== undefined && tokens[index + 1]?.value === "(") {
        if (macro.name === "CHECK" || macro.name.startsWith("CHECK_") ||
            macro.hasReturn || macro.returnsFalse) {
          findings.push(finding(file, token, "macro-in-main", {
            function: main.name, macro: macro.name, hasReturn: macro.hasReturn,
            returnsFalse: macro.returnsFalse,
          }))
        }
      }
      if (boolNames.has(token.value) && discardedBoolCall(
        tokens, index, main.bodyStart, main.bodyEnd, delimiters)) {
        findings.push(finding(file, token, "discarded-bool-helper-in-main", {
          function: main.name, helper: token.value,
        }))
      }
    }
  }
  return { candidates, functions, mains, macros, findings }
}

async function cFiles() {
  const entries = await readdir(testsDirectory, { withFileTypes: true })
  return entries.filter((entry) => entry.isFile() && entry.name.endsWith(".c"))
    .map((entry) => join(testsDirectory, entry.name))
    .sort()
}

export async function analyzeSeedTestExits() {
  const analyses = []
  for (const path of await cFiles()) {
    const file = relative(root, path).replaceAll("\\", "/")
    analyses.push(analyzeCSource(await readFile(path, "utf8"), file))
  }
  return analyses
}

export function formatFinding(item) {
  return `${item.file}:${item.line}: ${item.kind}` +
    (item.macro === undefined ? "" : ` (${item.macro})`)
}

export async function checkSeedTestExits() {
  const analyses = await analyzeSeedTestExits()
  const mains = analyses.flatMap((item) => item.mains)
  const findings = analyses.flatMap((item) => item.findings)
  return { analyses, mains, findings }
}

async function main() {
  const result = await checkSeedTestExits()
  if (result.findings.length > 0) {
    for (const item of result.findings) console.error(formatFinding(item))
    throw new Error("Seed C test exits: unsafe failure path detected")
  }
  const fileCount = result.analyses.length
  console.log(`Seed C test exits: ${fileCount} C files, ${result.mains.length} main definitions, 0 unsafe paths.`)
}

if (import.meta.main) await main()
