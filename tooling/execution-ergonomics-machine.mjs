const identifier = /^[A-Za-z_][A-Za-z0-9_]*$/

function withoutComments(source) {
  return source.replace(/\/\*[\s\S]*?\*\/|\/\/[^\r\n]*/g, (comment) =>
    comment.replace(/[^\r\n]/g, " "))
}

function splitTopLevelWithSpans(text, separator = ",") {
  const parts = []
  let start = 0
  const delimiters = []
  let quote = null
  let escaped = false
  for (let index = 0; index < text.length; index += 1) {
    const character = text[index]
    if (quote) {
      if (escaped) escaped = false
      else if (character === "\\") escaped = true
      else if (character === quote) quote = null
      continue
    }
    if (character === "\"" || character === "'") {
      quote = character
      continue
    }
    if ("([{".includes(character)) delimiters.push(character)
    else if (character === "<" && index > 0 && !/\s/.test(text[index - 1])) {
      delimiters.push(character)
    } else if (")]}".includes(character)) {
      const expected = { ")": "(", "]": "[", "}": "{" }[character]
      if (delimiters.at(-1) === expected) delimiters.pop()
    } else if (character === ">" && delimiters.at(-1) === "<") {
      delimiters.pop()
    } else if (character === separator && delimiters.length === 0) {
      const raw = text.slice(start, index)
      const leading = raw.length - raw.trimStart().length
      const trailing = raw.length - raw.trimEnd().length
      if (raw.trim()) {
        parts.push({
          end: index - trailing,
          start: start + leading,
          text: raw.trim(),
        })
      }
      start = index + 1
    }
  }
  const raw = text.slice(start)
  const leading = raw.length - raw.trimStart().length
  const trailing = raw.length - raw.trimEnd().length
  if (raw.trim()) {
    parts.push({
      end: text.length - trailing,
      start: start + leading,
      text: raw.trim(),
    })
  }
  return parts
}

export function splitTopLevel(text, separator = ",") {
  return splitTopLevelWithSpans(text, separator).map((part) => part.text)
}

function parseParameter(raw, index) {
  const hasDefault = splitTopLevel(raw, "=").length > 1
  const cleaned = splitTopLevel(raw, "=")[0].trim()
  if (!cleaned || cleaned === "...") return null
  const variadic = /\.\.\.\s*$/.test(cleaned)
  let tokens = cleaned.split(/\s+/)
  const type = cleaned.match(/:\s*(.+)$/s)?.[1]?.trim() ?? null
  const modifiers = new Set(["const", "inout", "take", "ref", "copy", "shared", "weak", "view", "mut"])
  while (modifiers.has(tokens[0])) tokens = tokens.slice(1)
  let external = null
  let internal = null
  if (tokens[0] === "_") {
    internal = tokens[1]?.replace(/:.*/, "")
    return internal && identifier.test(internal)
      ? { index, internal, type, policy: "positionalOnly", forms: ["positional"], hasDefault, variadic }
      : null
  }
  const inlineExternalInternal = tokens.length >= 3
    && identifier.test(tokens[0])
    && identifier.test(tokens[1].replace(/:.*/, ""))
    && /:$/.test(tokens[1])
  const separatedExternalInternal = tokens.length >= 4
    && identifier.test(tokens[0])
    && identifier.test(tokens[1])
    && tokens[2] === ":"
  if (inlineExternalInternal || separatedExternalInternal) {
    external = tokens[0]
    internal = tokens[1]
      .replace(/:.*/, "")
    const contextualAllocator = external === "allocator"
      && /\bref\s+Allocator(?:\b|\s|<)/.test(cleaned)
    return {
      index,
      internal,
      external: contextualAllocator ? "allocator" : external,
      policy: contextualAllocator ? "contextualAllocator" : `required(${external})`,
      forms: contextualAllocator ? ["allocator:"] : [`${external}:`],
      type,
      hasDefault,
      variadic,
      contextualAllocator,
    }
  }
  const inlineNameIndex = tokens.findIndex((token) => /^[A-Za-z_][A-Za-z0-9_]*:/.test(token))
  const separatedNameIndex = tokens.findIndex((token, tokenIndex) =>
    token === ":" && tokenIndex > 0 && identifier.test(tokens[tokenIndex - 1]))
  const nameIndex = inlineNameIndex >= 0 ? inlineNameIndex : separatedNameIndex - 1
  if (nameIndex < 0) {
    return {
      index,
      internal: `$${index}`,
      type,
      policy: "positionalOnly",
      forms: ["positional"],
      hasDefault,
      variadic,
      unnamed: true,
    }
  }
  internal = tokens[nameIndex].replace(/:.*/, "")
  if (!identifier.test(internal)) return null
  return {
    index,
    internal,
    external: internal,
    type,
    policy: `required(${internal})`,
    forms: [`${internal}:`],
    hasDefault,
    variadic,
  }
}

const leadingParameterKeywords = new Set([
  "const",
  "copy",
  "inout",
  "mut",
  "mut ref",
  "mut view",
  "pin",
  "ref",
  "shared",
  "take",
  "view",
  "weak",
])

function parameterContractFacts(raw) {
  const cleaned = splitTopLevel(raw, "=")[0].trim()
  const leading = cleaned.match(/^(mut\s+ref|mut\s+view|[A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.+)$/s)
  const leadingModifier = leadingParameterKeywords.has(leading?.[1])
    ? leading[1]
    : null
  const contractMode = cleaned.match(/:\s*(const|inout|mut\s+ref|mut\s+view|ref|take)\b/)?.[1]?.replace(/\s+/g, " ") ?? "value"
  if (!leadingModifier) return { contractMode }
  const internal = leading[2]
  const type = leading[3].trim()
  const operationOnly = new Set(["copy", "mut", "pin"])
  const canonicalForm = operationOnly.has(leadingModifier)
    ? `${internal}: ${type}; ${leadingModifier} is not a parameter mode`
    : `${internal}: ${leadingModifier} ${type}`
  return { canonicalForm, contractMode, internal, leadingModifier }
}

function parseParameters(raw) {
  return splitTopLevel(raw)
    .map((parameter, index) => {
      const parsed = parseParameter(parameter, index)
      return parsed ? { ...parsed, ...parameterContractFacts(parameter) } : null
    })
    .filter(Boolean)
}

function recordLabelDiagnostics(source) {
  const diagnostics = []
  for (const match of source.matchAll(/\binit\s*\(/g)) {
    const opening = (match.index ?? 0) + match[0].lastIndexOf("(")
    const closing = matchingDelimiter(source, opening)
    if (closing < 0) continue
    for (const raw of splitTopLevel(source.slice(opening + 1, closing))) {
      const placement = parameterContractFacts(raw)
      if (placement.leadingModifier) {
        diagnostics.push({
          code: "W-OWNERSHIP-0016",
          declaration: "init",
          parameter: placement.internal,
          modifier: placement.leadingModifier,
          canonicalForm: placement.canonicalForm,
          context: "initializer",
          reason: "parameter-contract-before-binding",
        })
      }
    }
  }
  return diagnostics
}

function quotedPattern(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
}

/**
 * Derive the small, source-backed ordering contract for a synthesized
 * initializer. This is an ordering oracle only; it does not construct a W
 * value or claim checker/runtime support.
 */
export function deriveInitializerEvaluationPlan(source, typeName) {
  if (!typeName) return null
  const clean = withoutComments(source)
  const declaration = new RegExp(`\\b(?:struct|object)\\s+${quotedPattern(typeName)}\\b[^\\{]*\\{`, "g").exec(clean)
  if (!declaration) return null
  const opening = declaration.index + declaration[0].lastIndexOf("{")
  const closing = matchingDelimiter(clean, opening, "{", "}")
  if (closing < 0) return null
  const fields = []
  for (const line of clean.slice(opening + 1, closing).split(/\r?\n/)) {
    const field = line.trim().match(/^(?:let|var)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*([^=;]+?)(?:\s*=\s*(.+?))?;?$/)
    if (!field) continue
    fields.push({ name: field[1], type: field[2].trim(), defaultExpression: field[3]?.trim() ?? null })
  }
  const fieldByName = new Map(fields.map((field) => [field.name, field]))
  const callPattern = new RegExp(`\\b${quotedPattern(typeName)}\\s*\\(`, "g")
  let call = null
  for (const match of clean.matchAll(callPattern)) {
    const openingCall = (match.index ?? 0) + match[0].lastIndexOf("(")
    if (openingCall > opening && openingCall < closing) continue
    const closingCall = matchingDelimiter(clean, openingCall)
    if (closingCall >= 0) {
      call = { opening: openingCall, closing: closingCall }
      break
    }
  }
  if (!call) return { typeName, fields, explicitEvaluationOrder: [], defaultEvaluationOrder: [], installationOrder: fields.map((field) => field.name), diagnostics: [] }
  const explicitEvaluationOrder = []
  const supplied = new Set()
  const diagnostics = []
  for (const argument of splitTopLevelWithSpans(clean.slice(call.opening + 1, call.closing))) {
    const labelMatch = argument.text.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:/)
    const label = labelMatch?.[1] ?? null
    const expression = labelMatch ? argument.text.slice(labelMatch[0].length).trim() : argument.text.trim()
    if (!label) {
      diagnostics.push({
        code: "W-LABEL-0005",
        typeName,
        label: "positional",
        reason: "synthesized-initializer-requires-label",
      })
      continue
    }
    const field = fieldByName.get(label)
    if (!field) {
      diagnostics.push({ code: "W-LABEL-0005", typeName, label: label ?? "positional" })
      continue
    }
    if (supplied.has(field.name)) {
      diagnostics.push({ code: "W-LABEL-0006", typeName, label: label ?? field.name, slot: field.name })
      continue
    }
    supplied.add(field.name)
    explicitEvaluationOrder.push({ field: field.name, label: label ?? field.name, expression })
  }
  for (const field of fields) {
    if (!supplied.has(field.name) && field.defaultExpression === null) {
      diagnostics.push({
        code: "W-LABEL-0005",
        typeName,
        label: field.name,
        reason: "synthesized-initializer-required-field-missing",
      })
    }
  }
  const defaultEvaluationOrder = fields
    .filter((field) => !supplied.has(field.name) && field.defaultExpression !== null)
    .map((field) => ({ field: field.name, expression: field.defaultExpression }))
  return {
    typeName,
    fields,
    explicitEvaluationOrder,
    defaultEvaluationOrder,
    installationOrder: fields.map((field) => field.name),
    diagnostics,
  }
}

function matchingDelimiter(source, opening, open = "(", close = ")") {
  let depth = 0
  let quote = null
  let escaped = false
  for (let index = opening; index < source.length; index += 1) {
    const character = source[index]
    if (quote) {
      if (escaped) escaped = false
      else if (character === "\\") escaped = true
      else if (character === quote) quote = null
      continue
    }
    if (character === "\"" || character === "'") {
      quote = character
      continue
    }
    if (character === open) depth += 1
    else if (character === close) {
      depth -= 1
      if (depth === 0) return index
    }
  }
  return -1
}

function declarationScopes(source) {
  const scopes = []
  const patterns = [
    /\b(struct|object|protocol|enum|service)\s+([A-Za-z_][A-Za-z0-9_]*)[^\n{]*\{/g,
    /\b(extension)(?:<[^>{}]*>)?\s+([A-Za-z_][A-Za-z0-9_]*)[^\n{]*\{/g,
  ]
  for (const pattern of patterns) {
    for (const match of source.matchAll(pattern)) {
      const opening = (match.index ?? 0) + match[0].lastIndexOf("{")
      const closing = matchingDelimiter(source, opening, "{", "}")
      if (closing >= 0) {
        scopes.push({
          id: `nominal:${match[2]}`,
          kind: match[1],
          start: opening,
          end: closing,
        })
      }
    }
  }
  return scopes
}

function declarationBodies(source) {
  const declarations = []
  const scopes = declarationScopes(source)
  const foreignScopes = []
  for (const match of source.matchAll(/\bforeign\b[^{}]*\{/g)) {
    const opening = (match.index ?? 0) + match[0].lastIndexOf("{")
    const closing = matchingDelimiter(source, opening, "{", "}")
    if (closing >= 0) foreignScopes.push({ start: opening, end: closing })
  }
  const pattern = /\b(?:(export)\s+)?((?:static\s+|const\s+|unsafe\s+|mut\s+|take\s+|async\s+)*)fn\s+([A-Za-z_][A-Za-z0-9_]*)/g
  for (const match of source.matchAll(pattern)) {
    const start = match.index ?? 0
    let parametersStart = start + match[0].length
    while (/\s/.test(source[parametersStart] ?? "")) parametersStart += 1
    if (source[parametersStart] === "<") {
      const genericEnd = matchingDelimiter(source, parametersStart, "<", ">")
      if (genericEnd < 0) continue
      parametersStart = genericEnd + 1
      while (/\s/.test(source[parametersStart] ?? "")) parametersStart += 1
    }
    if (source[parametersStart] !== "(") continue
    const parametersEnd = matchingDelimiter(source, parametersStart)
    if (parametersEnd < 0) continue
    const lineEnd = source.indexOf("\n", parametersEnd)
    const tailEnd = lineEnd < 0 ? source.length : lineEnd
    const tail = source.slice(parametersEnd + 1, tailEnd)
    const relativeBodyStart = tail.indexOf("{")
    const bodyStart = relativeBodyStart < 0 ? -1 : parametersEnd + 1 + relativeBodyStart
    let body = ""
    if (bodyStart >= 0) {
      const bodyEnd = matchingDelimiter(source, bodyStart, "{", "}")
      if (bodyEnd >= 0) body = source.slice(bodyStart + 1, bodyEnd)
    }
    const owner = scopes
      .filter((scope) => scope.start < start && start < scope.end)
      .sort((left, right) => (left.end - left.start) - (right.end - right.start))[0]
    declarations.push({
      boundary: foreignScopes.some((scope) => scope.start < start && start < scope.end)
        ? "foreign"
        : "w",
      name: match[3],
      params: parseParameters(source.slice(parametersStart + 1, parametersEnd)),
      line: source.slice(0, start).split("\n").length,
      source: source.slice(start, parametersEnd + 1).replace(/\s+/g, " ").trim(),
      body,
      explicitAsync: /\basync\s*$/.test(match[2]),
      hasBody: bodyStart >= 0,
      exported: Boolean(match[1]),
      scope: owner?.id ?? "module",
      scopeKind: owner?.kind ?? "module",
    })
  }
  return declarations
}

function braceDepth(source, start, end) {
  let depth = 0
  let quote = null
  let escaped = false
  for (let index = start; index < end; index += 1) {
    const character = source[index]
    if (quote) {
      if (escaped) escaped = false
      else if (character === "\\") escaped = true
      else if (character === quote) quote = null
      continue
    }
    if (character === "\"" || character === "'") quote = character
    else if (character === "{") depth += 1
    else if (character === "}") depth -= 1
  }
  return depth
}

function codeMask(source) {
  let quote = null
  let escaped = false
  let mask = ""
  for (let index = 0; index < source.length; index += 1) {
    const character = source[index]
    if (quote) {
      if (escaped) escaped = false
      else if (character === "\\") escaped = true
      else if (character === quote) quote = null
      mask += character === "\n" || character === "\r" ? character : " "
      continue
    }
    if (character === "\"" || character === "'") {
      quote = character
      mask += " "
      continue
    }
    mask += character
  }
  return mask
}

function parseCalls(source, input = {}) {
  const calls = []
  const mask = codeMask(source)
  const enumScopes = declarationScopes(source).filter((scope) => scope.kind === "enum")
  const ignored = new Set(["fn", "if", "for", "while", "switch", "catch", "return", "yield"])

  for (const match of mask.matchAll(/\b([A-Za-z_][A-Za-z0-9_]*)/g)) {
    const callee = match[1]
    const start = match.index ?? 0
    if (ignored.has(callee)) continue
    const prefix = mask.slice(Math.max(0, start - 32), start)
    if (/\bfn\s*$/.test(prefix)) continue

    let cursor = start + callee.length
    if (mask[cursor] === "<") {
      const genericEnd = matchingDelimiter(mask, cursor, "<", ">")
      if (genericEnd < 0) continue
      cursor = genericEnd + 1
    }
    while (/\s/.test(mask[cursor] ?? "")) cursor += 1
    if (mask[cursor] !== "(") continue
    const closing = matchingDelimiter(source, cursor)
    if (closing < 0) continue

    const enclosingEnum = enumScopes.find((scope) =>
      scope.start < start && start < scope.end
      && braceDepth(source, scope.start + 1, start) === 0)
    if (enclosingEnum) continue

    const argumentSource = source.slice(cursor + 1, closing)
    const args = splitTopLevelWithSpans(argumentSource)
    const labels = []
    const forms = []
    const argumentsWithContracts = []
    for (const argumentPart of args) {
      const argument = argumentPart.text
      const labelMatch = argument.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:/)
      const expression = labelMatch
        ? argument.slice(labelMatch[0].length).trim()
        : argument.trim()
      const operationMatch = expression.match(/^(mut\s+ref|mut\s+view|mut\s+object|mut|copy|inout|pin|ref|take)\b/)
      const operation = operationMatch
        ? (operationMatch[1] === "mut" ? "mut object" : operationMatch[1].replace(/\s+/g, " "))
        : "value"
      const expressionOffset = argument.indexOf(expression)
      if (labelMatch) {
        labels.push(labelMatch[1])
        forms.push(`${labelMatch[1]}:`)
      } else forms.push("positional")
      argumentsWithContracts.push({
        expression,
        expressionStart: cursor + 1 + argumentPart.start + expressionOffset,
        form: labelMatch ? `${labelMatch[1]}:` : "positional",
        label: labelMatch?.[1] ?? null,
        operation,
        place: expression.replace(/^(?:mut\s+ref|mut\s+view|mut\s+object|mut|copy|inout|pin|ref|take)\s+/, "").trim(),
        placeKind: input.placeKinds?.[expression.replace(/^(?:mut\s+ref|mut\s+view|mut\s+object|mut|copy|inout|pin|ref|take)\s+/, "").trim()] ?? null,
        mutablePlace: input.mutablePlaces?.includes(expression.replace(/^(?:mut\s+ref|mut\s+view|mut\s+object|mut|copy|inout|pin|ref|take)\s+/, "").trim())
          ? true
          : input.mutablePlaces ? false : null,
        start: cursor + 1 + argumentPart.start,
        end: cursor + 1 + argumentPart.end,
      })
    }

    const statementStart = Math.max(
      source.lastIndexOf("\n", start),
      source.lastIndexOf(";", start),
      source.lastIndexOf("{", start),
    ) + 1
    const pipe = /\|>\s*(?:(?:try|await)\s+)?(?:\.\s*)?$/.test(mask.slice(statementStart, start))
    const statement = source.slice(statementStart, closing + 1)
    let callForm = "direct"
    if (/\btry\s+await\b|\bawait\b/.test(statement)) callForm = "await"
    else if (/\bsync\s*$/.test(source.slice(statementStart, start))) callForm = "sync"
    else {
      const launcher = statement.match(/^\s*let\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*(async|spawn\s*<[^>]+>)\s*/)
      const launcherRoot = launcher ? statementStart + launcher[0].length : -1
      if (launcher && start === launcherRoot) callForm = launcher[1].startsWith("spawn") ? "spawn" : "async"
    }

    let memberCursor = start - 1
    while (/\s/.test(mask[memberCursor] ?? "")) memberCursor -= 1
    calls.push({
      callee,
      arguments: argumentsWithContracts,
      end: closing + 1,
      labels,
      forms,
      member: mask[memberCursor] === ".",
      pipe,
      start,
      callForm,
      line: source.slice(0, start).split("\n").length,
      source: source.slice(start, closing + 1).replace(/\s+/g, " ").trim(),
    })
  }
  return calls
}

function parameterOptional(parameter) {
  return parameter.hasDefault || parameter.contextualAllocator || parameter.variadic
}

function namedOptions(named) {
  let options = [[]]
  for (const parameter of named) {
    const form = parameter.forms[0]
    if (!form) continue
    const withParameter = options.map((option) => [...option, form])
    options = parameterOptional(parameter)
      ? [...options, ...withParameter]
      : withParameter
  }
  return options.map((option) => option.sort())
}

function completeCallShapes(parameters) {
  const named = parameters.filter((parameter) => parameter.policy !== "positionalOnly" && !parameter.unnamed)
  const positional = parameters.filter((parameter) => parameter.policy === "positionalOnly" || parameter.unnamed)
  let positionalOptions = [[]]
  for (const parameter of positional) {
    const token = parameter.variadic ? "positional..." : "positional"
    positionalOptions = parameterOptional(parameter)
      ? positionalOptions.flatMap((option) => [option, [...option, token]])
      : positionalOptions.map((option) => [...option, token])
  }
  const shapes = namedOptions(named).flatMap((names) => positionalOptions.map((positions) => {
    const parts = []
    if (names.length > 0) parts.push(names.join("&"))
    parts.push(...positions)
    return parts.join("|")
  }))
  return [...new Set(shapes)]
}

function labelMap(parameters) {
  const map = new Map()
  for (const parameter of parameters) {
    if (parameter.policy === "positionalOnly" || parameter.unnamed) continue
    for (const form of parameter.forms) map.set(form, parameter)
  }
  return map
}

function bindCallForms(parameters, forms, options = {}) {
  const labels = labelMap(parameters)
  const positional = parameters.filter((parameter) => parameter.policy === "positionalOnly" || parameter.unnamed)
  const seen = new Set()
  const bindings = []
  const holes = []
  const addHole = (parameter) => {
    if (!holes.includes(parameter)) holes.push(parameter)
  }
  let positionalIndex = 0
  let activeVariadic = null
  for (const form of forms) {
    if (form === "positional") {
      if (activeVariadic) {
        bindings.push({ parameter: activeVariadic, form })
        continue
      }
      const parameter = positional[positionalIndex]
      if (!parameter) return null
      if (parameter.variadic) activeVariadic = parameter
      else positionalIndex += 1
      bindings.push({ parameter, form })
      continue
    }
    const parameter = labels.get(form)
    if (!parameter || seen.has(form)) return null
    seen.add(form)
    bindings.push({ parameter, form })
  }
  for (const parameter of parameters) {
    if (parameter.policy === "positionalOnly" || parameter.unnamed) continue
    const form = parameter.forms[0]
    if (form && !seen.has(form) && !parameterOptional(parameter)) {
      if (!options.allowPipeHole) return null
      addHole(parameter)
    }
  }
  for (const parameter of positional) {
    if (!parameterOptional(parameter)
      && !bindings.some((binding) => binding.parameter === parameter)
      && !holes.includes(parameter)) addHole(parameter)
  }
  if (options.allowPipeHole && holes.length !== 1) return null
  if (!options.allowPipeHole && holes.length > 0) return null
  return { bindings, holes }
}

export function acceptsCallShape(parameters, forms) {
  return bindCallForms(parameters, forms) !== null
}

export function acceptsPipeCallShape(parameters, forms) {
  return bindCallForms(parameters, forms, { allowPipeHole: true }) !== null
}

function operationMatchesContract(parameter, argument) {
  const mode = parameter.contractMode
  if (mode === "inout") return argument.operation === "inout"
  if (mode === "ref") return ["ref", "value"].includes(argument.operation)
  if (mode === "mut ref") {
    if (argument.operation === "mut ref") return argument.mutablePlace !== false
    if (argument.operation !== "mut object") return false
    if (argument.placeKind === "struct" || argument.placeKind === "enum") return false
    return parameter.objectType !== false && argument.mutablePlace !== false
  }
  if (mode === "mut view") return ["mut view", "mut ref", "ref", "value"].includes(argument.operation)
  if (mode === "take") return ["take", "value"].includes(argument.operation)
  return argument.operation !== "inout"
}

export function acceptsCallContract(parameters, args) {
  const bindings = bindCallForms(parameters, args.map((argument) => argument.form))
  if (!bindings || bindings.bindings.length !== args.length) return false
  return bindings.bindings.every((binding, index) => operationMatchesContract(binding.parameter, args[index]))
}

export function acceptsPipeCallContract(parameters, args) {
  const bindings = bindCallForms(parameters, args.map((argument) => argument.form), { allowPipeHole: true })
  if (!bindings || bindings.bindings.length !== args.length) return false
  return bindings.bindings.every((binding, index) => operationMatchesContract(binding.parameter, args[index]))
}

function witnessForms(parameters, limit = 8) {
  const named = parameters.filter((parameter) => parameter.policy !== "positionalOnly" && !parameter.unnamed)
  const positional = parameters.filter((parameter) => parameter.policy === "positionalOnly" || parameter.unnamed)
  let positionalForms = [[]]
  for (const parameter of positional) {
    const counts = parameter.variadic
      ? Array.from({ length: limit + 1 }, (_, index) => index)
      : parameterOptional(parameter) ? [0, 1] : [1]
    positionalForms = positionalForms.flatMap((prefix) => counts.map((count) => [
      ...prefix,
      ...Array.from({ length: count }, () => "positional"),
    ]))
  }
  return namedOptions(named).flatMap((names) => positionalForms.map((positions) => [...names, ...positions]))
}

function overlappingCallShape(left, right) {
  const leftShapes = new Set(completeCallShapes(left))
  for (const shape of completeCallShapes(right)) {
    if (leftShapes.has(shape)) return shape
  }
  for (const candidate of witnessForms(left).concat(witnessForms(right))) {
    if (acceptsCallShape(left, candidate) && acceptsCallShape(right, candidate)) return candidate.join("|")
  }
  return null
}

function deriveCallableLabels(source, declarations, input = {}) {
  const byScopedName = new Map()
  const byName = new Map()
  const diagnostics = recordLabelDiagnostics(source)
  const objectNames = new Set([...source.matchAll(/\bobject\s+([A-Za-z_][A-Za-z0-9_]*)/g)].map((match) => match[1]))
  const nominalNames = new Set([...source.matchAll(/\b(?:object|struct|enum)\s+([A-Za-z_][A-Za-z0-9_]*)/g)].map((match) => match[1]))
  for (const declaration of declarations) {
    const externalLabels = new Map()
    for (const parameter of declaration.params) {
      const typeName = parameter.type
        ?.replace(/^(?:mut\s+ref|mut\s+view|ref|inout|take|const|shared|weak|view)\s+/, "")
        .match(/^([A-Za-z_][A-Za-z0-9_]*)/)?.[1]
      parameter.objectType = nominalNames.size === 0 ? null : objectNames.has(typeName)
      for (const form of parameter.forms.filter((candidate) => candidate.endsWith(":"))) {
        const label = form.slice(0, -1)
        if (externalLabels.has(label)) {
          diagnostics.push({
            code: "W-LABEL-0006",
            declaration: declaration.name,
            label,
            slot: label,
            reason: "duplicate-external-label-in-declaration",
          })
        } else externalLabels.set(label, parameter)
      }
      if (!parameter.leadingModifier) continue
      diagnostics.push({
        code: "W-OWNERSHIP-0016",
        declaration: declaration.name,
        parameter: parameter.internal,
        modifier: parameter.leadingModifier,
        canonicalForm: parameter.canonicalForm,
        context: "callable",
        reason: "parameter-contract-before-binding",
      })
    }
    const callShapes = completeCallShapes(declaration.params)
    const scopedName = `${declaration.scope}|${declaration.name}`
    const previous = byScopedName.get(scopedName) ?? []
    for (const prior of previous) {
      const overlap = overlappingCallShape(declaration.params, prior.params)
      if (overlap !== null) {
        diagnostics.push({
          code: "W-LABEL-0004",
          declaration: declaration.name,
          overlap,
          declarations: [prior.ordinal, declarations.indexOf(declaration)],
        })
      }
    }
    const item = {
      boundary: declaration.boundary,
      callShapes,
      params: declaration.params,
      ordinal: declarations.indexOf(declaration),
      scope: declaration.scope,
    }
    byScopedName.set(scopedName, [...previous, item])
    byName.set(declaration.name, [...(byName.get(declaration.name) ?? []), item])
  }
  const calls = parseCalls(source, input)
  for (const call of calls) {
    // This oracle has no type checker. A member name can resolve to a type that
    // is not declared in the same source file, so only direct calls are
    // validated here. S0 owns member lookup and witness conformance.
    if (call.member) continue
    const candidates = byName.get(call.callee) ?? []
    const declarationsForName = candidates.filter((declaration) => declaration.scope === "module")
    if (declarationsForName.length === 0) continue
    const acceptedLabels = declarationsForName.flatMap((declaration) => declaration.params.flatMap((parameter) => parameter.forms.filter((form) => form.endsWith(":"))))
    const duplicate = call.labels.find((label, index) => call.labels.indexOf(label) !== index)
    if (duplicate) diagnostics.push({ code: "W-LABEL-0006", declaration: call.callee, label: duplicate, slot: duplicate })
    const suppliedShape = call.forms.join("|")
    const acceptedShapes = declarationsForName.flatMap((declaration) => declaration.callShapes)
    const accepted = declarationsForName.some((declaration) =>
      call.pipe
        ? acceptsPipeCallShape(declaration.params, call.forms)
        : acceptsCallShape(declaration.params, call.forms))
    if (!duplicate && !accepted) {
      const unknown = call.labels.find((label) => !acceptedLabels.includes(`${label}:`))
      diagnostics.push({
        code: "W-LABEL-0005",
        declaration: call.callee,
        label: unknown ?? suppliedShape,
        acceptedForms: acceptedShapes,
      })
    } else if (!duplicate
      && declarationsForName.some((declaration) => declaration.boundary !== "foreign")
      && !declarationsForName.some((declaration) => declaration.boundary === "foreign"
        || (call.pipe
          ? acceptsPipeCallContract(declaration.params, call.arguments)
          : acceptsCallContract(declaration.params, call.arguments)))) {
      diagnostics.push({
        code: "W-OWNERSHIP-0017",
        declaration: call.callee,
        expectedContracts: declarationsForName.map((declaration) =>
          declaration.params.map((parameter) => parameter.contractMode)),
        suppliedOperations: call.arguments.map((argument) => argument.operation),
        line: call.line,
        reason: "call-site-operation-does-not-match-parameter-contract",
        source: call.source,
      })
    }
  }
  return {
    declarations: declarations.map((declaration, ordinal) => ({
      name: declaration.name,
      boundary: declaration.boundary,
      ordinal,
      scope: declaration.scope,
      line: declaration.line,
      source: declaration.source,
      params: declaration.params,
      callShapes: completeCallShapes(declaration.params),
    })),
    diagnostics,
    calls,
  }
}

function parseGenericHeads(source) {
  const heads = []
  for (const match of source.matchAll(/\b(?:struct|enum|type|fn)\s+([A-Za-z_][A-Za-z0-9_]*)\s*<([^>{}]*)>/g)) {
    const parameters = splitTopLevel(match[2]).map((raw, index) => {
      const token = raw.trim()
      const positional = token.match(/^_\s*([A-Za-z_][A-Za-z0-9_]*)\s*:/)
      const externalInternal = token.match(/^([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:/)
      const named = token.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:/)
      if (positional) {
        return {
          index,
          name: positional[1],
          internal: positional[1],
          external: null,
          kind: "value",
          anchor: true,
          policy: "positionalOnly",
          forms: ["positional"],
        }
      }
      if (externalInternal) {
        return {
          index,
          name: externalInternal[2],
          internal: externalInternal[2],
          external: externalInternal[1],
          kind: "value",
          anchor: false,
          policy: `required(${externalInternal[1]})`,
          forms: [`${externalInternal[1]}:`],
        }
      }
      if (named && !/^[A-Z]/.test(named[1])) {
        return {
          index,
          name: named[1],
          internal: named[1],
          external: named[1],
          kind: "value",
          anchor: false,
          policy: `required(${named[1]})`,
          forms: [`${named[1]}:`],
        }
      }
      const typeName = named ? named[1] : token
      return {
        index,
        name: typeName,
        internal: typeName,
        external: null,
        kind: "type",
        anchor: true,
        policy: "type",
        forms: ["positional"],
      }
    })
    heads.push({ name: match[1], parameters })
  }
  const diagnostics = []
  for (const head of heads) {
    const externalLabels = new Set()
    for (const parameter of head.parameters) {
      for (const form of parameter.forms.filter((candidate) => candidate.endsWith(":"))) {
        const label = form.slice(0, -1)
        if (externalLabels.has(label)) {
          diagnostics.push({
            code: "W-LABEL-0006",
            declaration: head.name,
            label,
            slot: label,
            reason: "duplicate-external-label-in-generic-head",
          })
        } else externalLabels.add(label)
      }
    }
  }
  const identities = []
  const bind = (head, args, declaration) => {
    const bound = Array(head.parameters.length).fill(undefined)
    const assigned = new Set()
    const suppliedLabels = args.map((argument) => argument.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:/)?.[1]).filter(Boolean)
    for (const label of suppliedLabels) {
      if (suppliedLabels.indexOf(label) !== suppliedLabels.lastIndexOf(label)) {
        diagnostics.push({ code: "W-LABEL-0006", declaration, label, slot: label })
      }
    }
    const anchors = head.parameters.filter((parameter) => parameter.anchor)
    let nextAnchor = 0
    for (const raw of args) {
      const named = raw.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.*)$/)
      if (named) {
        const label = named[1]
        const candidate = head.parameters.find((parameter) => parameter.external === label)
        if (!candidate || candidate.anchor) {
          diagnostics.push({
            code: "W-LABEL-0005",
            declaration,
            label,
            acceptedForms: head.parameters.filter((parameter) => !parameter.anchor).flatMap((parameter) => parameter.forms),
            reason: "generic-label-unknown-or-positional-only",
          })
          continue
        }
        if (assigned.has(candidate.index)) {
          diagnostics.push({ code: "W-LABEL-0006", declaration, label, slot: label })
          continue
        }
        assigned.add(candidate.index)
        bound[candidate.index] = named[2].trim()
        continue
      }
      while (nextAnchor < anchors.length && assigned.has(anchors[nextAnchor].index)) nextAnchor += 1
      const anchor = anchors[nextAnchor]
      if (!anchor || !anchor.anchor) {
        diagnostics.push({ code: "W-LABEL-0005", declaration, reason: "generic-positional-without-anchor" })
        continue
      }
      assigned.add(anchor.index)
      bound[anchor.index] = raw.trim()
      nextAnchor += 1
    }
    for (const parameter of head.parameters) {
      if (!assigned.has(parameter.index)) {
        diagnostics.push({ code: "W-GENERIC-0002", declaration, slot: parameter.name, reason: "generic-slot-missing" })
      }
    }
    return bound.map((value) => value ?? "")
  }
  for (const head of heads) {
    const uses = [...source.matchAll(new RegExp(`\\b${head.name}\\s*<([^>\n]+)>`, "g"))]
      .filter((match) => !new RegExp(`\\b(?:struct|enum|type|fn)\\s+$`).test(source.slice(0, match.index)))
    for (const use of uses) {
      const args = splitTopLevel(use[1])
      const values = bind(head, args, head.name)
      identities.push({ head: head.name, values })
    }
  }
  const oven = identities.filter((identity) => identity.head === "OvenSession")
  if (oven.length >= 2 && oven[0].values.join("|") === oven[1].values.join("|")) {
    identities[0].sameAsNext = true
  }
  return { heads, identities, diagnostics }
}

function bodyCalls(body) {
  return [...body.matchAll(/\b([A-Za-z_][A-Za-z0-9_]*)\s*\(/g)]
    .map((match) => match[1])
    .filter((name) => !["if", "for", "while", "switch", "return", "Task"].includes(name))
}

function directEntryLocalBlockers(declaration) {
  const blockers = []
  if (!declaration.explicitAsync) blockers.push("source-spelling-not-explicit-async")
  if (!declaration.hasBody) blockers.push("body-unavailable")
  if (declaration.boundary === "foreign") blockers.push("foreign-boundary")
  if (/\bawait\b/.test(declaration.body)) blockers.push("potential-await")
  if (/\bTask\s*\.\s*yield\s*\(/.test(declaration.body)) blockers.push("task-yield")
  if (/\blet\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*(?:async\b|spawn\s*<)/.test(declaration.body)) blockers.push("child-initializer")
  if (/(?:\.|\b)join\s*\(/.test(declaration.body)) blockers.push("task-join")
  if (/\bdefer\s+async\b/.test(declaration.body)) blockers.push("async-defer")
  return [...new Set(blockers)]
}

function deriveDirectEntries(declarations, suspensionByName, input = {}) {
  const byName = new Map(declarations.map((declaration) => [declaration.name, declaration]))
  const external = new Map((input.functionTypes ?? []).map((summary) => [summary.name, summary]))
  const blockers = new Map(declarations.map((declaration) => [declaration.name, directEntryLocalBlockers(declaration)]))
  const available = new Set(declarations
    .filter((declaration) => blockers.get(declaration.name).length === 0)
    .map((declaration) => declaration.name))

  // Start with every locally eligible declaration and remove declarations
  // whose sync-call dependencies lose their direct entry. This greatest fixed
  // point accepts recursive sync-call SCCs without proving that they terminate.
  let changed = true
  while (changed) {
    changed = false
    for (const declaration of declarations) {
      if (!available.has(declaration.name)) continue
      const callBlockers = []
      for (const call of parseCalls(declaration.body)) {
        const local = byName.get(call.callee)
        if (local) {
          if (call.callForm === "sync") {
            if (local.explicitAsync && available.has(local.name)) continue
            callBlockers.push(`invalid-sync-call:${call.callee}`)
            continue
          }
          const suspension = suspensionByName.get(local.name) ?? "never"
          if (suspension === "never") continue
          callBlockers.push(`may-suspend-call:${call.callee}`)
          continue
        }
        const summary = external.get(call.callee)
        if (summary) {
          if (call.callForm === "sync") {
            if (summary.suspension === "may"
              && summary.sourceSpelling === "explicit"
              && summary.directEntry === "available") continue
            callBlockers.push(`invalid-sync-call:${call.callee}`)
            continue
          }
          if (summary.suspension !== "may") continue
          callBlockers.push(`may-suspend-call:${call.callee}`)
          continue
        }
        // Nominal constructors are value formation in this bounded oracle.
        // Unknown lower-case functions and member calls are conservatively
        // treated as potentially suspending until a function-type summary is
        // present.
        if (call.callForm === "sync") {
          callBlockers.push(`invalid-sync-call:${call.callee}`)
          continue
        }
        if (!call.member && /^[A-Z]/.test(call.callee)) continue
        callBlockers.push(`unknown-call-summary:${call.callee}`)
      }
      if (callBlockers.length > 0) {
        blockers.set(declaration.name, [...blockers.get(declaration.name), ...new Set(callBlockers)])
        available.delete(declaration.name)
        changed = true
      }
    }
  }

  return { available, blockers, external }
}

function stronglyConnected(nodes, edges) {
  let index = 0
  const stack = []
  const onStack = new Set()
  const indices = new Map()
  const low = new Map()
  const components = []
  function visit(node) {
    indices.set(node, index)
    low.set(node, index)
    index += 1
    stack.push(node)
    onStack.add(node)
    for (const next of edges.get(node) ?? []) {
      if (!indices.has(next)) {
        visit(next)
        low.set(node, Math.min(low.get(node), low.get(next)))
      } else if (onStack.has(next)) low.set(node, Math.min(low.get(node), indices.get(next)))
    }
    if (low.get(node) === indices.get(node)) {
      const component = []
      let next
      do {
        next = stack.pop()
        onStack.delete(next)
        component.push(next)
      } while (next !== node)
      components.push(component.sort())
    }
  }
  for (const node of nodes) if (!indices.has(node)) visit(node)
  return components
}

function deriveSuspension(source, declarations, input = {}) {
  const names = declarations.map((declaration) => declaration.name)
  const byName = new Map(declarations.map((declaration) => [declaration.name, declaration]))
  const edges = new Map(names.map((name) => [name, new Set()]))
  const external = new Map((input.functionTypes ?? []).map((summary) => [summary.name, summary]))
  const may = new Set(input.functionTypes?.filter((item) => item.suspension === "may").map((item) => item.name) ?? [])
  for (const declaration of declarations) {
    if (declaration.explicitAsync || /\bawait\b|\blet\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*(?:async|spawn\s*<)/.test(declaration.body)) may.add(declaration.name)
    for (const call of parseCalls(declaration.body)) {
      if (call.callForm !== "sync" && byName.has(call.callee)) edges.get(declaration.name).add(call.callee)
    }
  }
  let changed = true
  while (changed) {
    changed = false
    for (const [name, callees] of edges) {
      if (may.has(name)) continue
      if ([...callees].some((callee) => may.has(callee))) {
        may.add(name)
        changed = true
      }
    }
  }
  const components = stronglyConnected(names, edges)
  const directEntryEdges = new Map(names.map((name) => [name, new Set()]))
  for (const declaration of declarations) {
    for (const call of parseCalls(declaration.body)) {
      if (call.callForm === "sync" && byName.has(call.callee)) {
        directEntryEdges.get(declaration.name).add(call.callee)
      }
    }
  }
  const directEntryComponents = stronglyConnected(names, directEntryEdges)
  const suspensionByName = new Map([
    ...names.map((name) => [name, may.has(name) ? "may" : "never"]),
    ...(input.functionTypes ?? []).map((summary) => [summary.name, summary.suspension ?? "never"]),
  ])
  const direct = deriveDirectEntries(declarations, suspensionByName, input)
  const calls = parseCalls(source).map((call) => {
    const declaration = byName.get(call.callee)
    const summary = external.get(call.callee)
    const suspension = suspensionByName.get(call.callee) ?? "never"
    const sourceSpelling = declaration?.explicitAsync === true
      ? "explicit"
      : summary?.sourceSpelling
        ?? (suspension === "may" ? "inferred" : "none")
    const directEntry = declaration
      ? direct.available.has(call.callee) ? "available" : "absent"
      : summary?.directEntry ?? "absent"
    const invalidBare = suspension === "may" && call.callForm === "direct"
    const syncEligible = call.callForm === "sync"
      && suspension === "may"
      && sourceSpelling === "explicit"
      && directEntry === "available"
    return { ...call, suspension, sourceSpelling, directEntry, invalidBare, syncEligible }
  })
  const diagnostics = calls.filter((call) => call.invalidBare).map((call) => ({ code: "W-SUSPEND-0001", callee: call.callee, callForm: call.callForm }))
  for (const call of calls.filter((candidate) => candidate.callForm === "sync" && !candidate.syncEligible)) {
    diagnostics.push({
      code: "W-SUSPEND-0005",
      callee: call.callee,
      sourceSpelling: call.sourceSpelling,
      suspension: call.suspension,
      directEntry: call.directEntry,
      reason: "sync requires explicit async fn with directEntry available",
    })
  }
  const removable = calls.filter((call) => call.suspension === "never" && call.callForm === "await")
  for (const call of removable) diagnostics.push({ code: "W-SUSPEND-0002", callee: call.callee, callForm: call.callForm })
  if (/\bblockingWait\s*\(/.test(source)) diagnostics.push({ code: "W-SUSPEND-0003", operation: "blockingWait" })
  const placementDiagnostics = []
  const oldSurface = /\basync\s+let\b|\bspawn\s*<[^>]+>\s*let\b|\bspawn\s+let\b/.exec(source)
  if (oldSurface) placementDiagnostics.push({ code: "W-PLACEMENT-0004", reason: "child launcher must be in a let initializer", offset: oldSurface.index })
  for (const match of source.matchAll(/(?:^|[;{}\n])\s*(let|var)\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*(async|spawn\s*(?:<[^>]+>)?)\s*([^;\n}]*)/g)) {
    const bindingKind = match[1]
    const operand = match[3].trim()
    if (bindingKind !== "let") placementDiagnostics.push({ code: "W-PLACEMENT-0004", reason: "child launcher requires lexical let", bindingKind })
    if (!/^[A-Za-z_][A-Za-z0-9_.]*\s*\(/.test(operand)) placementDiagnostics.push({ code: "W-PLACEMENT-0004", reason: "child launcher requires one callable root" })
    if (/\b(?:async|spawn)\s*(?:<[^>]+>)?\b/.test(operand)) placementDiagnostics.push({ code: "W-PLACEMENT-0004", reason: "nested child launcher" })
  }
  for (const match of source.matchAll(/(?:^|[;{}\n])\s*(return\s+)?(?:async\s+(?!fn\b)|spawn\s*(?:<[^>]+>)?\s+[A-Za-z_])/g)) {
    if (!match[1]) placementDiagnostics.push({ code: "W-PLACEMENT-0004", reason: "child launcher requires let binding" })
    else placementDiagnostics.push({ code: "W-PLACEMENT-0004", reason: "child launcher cannot be returned" })
  }
  const children = calls.filter((call) => call.callForm === "async" || call.callForm === "spawn").map((call) => ({
    form: call.callForm,
    callee: call.callee,
    accepts: ["never", "may"],
  }))
  const publicContract = input.publicContract
  const publicResult = publicContract
    ? (() => {
      const widening = publicContract.previous === "never" && publicContract.current === "may"
      const directEntryRemoved = publicContract.previousDirectEntry === "available"
        && publicContract.currentDirectEntry === "absent"
      const exported = publicContract.exported !== false
      return {
        previous: publicContract.previous,
        current: publicContract.current,
        explicit: Boolean(publicContract.explicitAsync),
        previousDirectEntry: publicContract.previousDirectEntry ?? "absent",
        currentDirectEntry: publicContract.currentDirectEntry ?? "absent",
        widening,
        directEntryRemoved,
        sourceBreaking: exported && (widening || directEntryRemoved),
        semanticInterfaceKeyChanged: exported && directEntryRemoved,
      }
    })()
    : null
  return {
    declarations: declarations.map((declaration) => ({
      name: declaration.name,
      suspension: may.has(declaration.name) ? "may" : "never",
      asyncEntrySuspension: declaration.explicitAsync ? "may" : null,
      sourceSpelling: declaration.explicitAsync ? "explicit" : may.has(declaration.name) ? "inferred" : "none",
      directEntry: direct.available.has(declaration.name) ? "available" : "absent",
      directEntrySuspension: direct.available.has(declaration.name) ? "never" : null,
      directEntryProof: {
        scope: "declaration-wide",
        beforeSpecialization: true,
        dynamicReadinessUsed: false,
        blockers: direct.blockers.get(declaration.name) ?? [],
      },
      boundary: declaration.boundary,
      hasBody: declaration.hasBody,
      exported: declaration.exported,
    })),
    calls,
    children,
    staging: children.length > 0 ? ["callee", "arguments", "captures", "reserve", "publish"] : [],
    scc: components.map((members) => ({ members, suspension: members.some((member) => may.has(member)) ? "may" : "never" })),
    directEntryScc: directEntryComponents.map((members) => {
      const recursive = members.length > 1
        || (members.length === 1 && directEntryEdges.get(members[0])?.has(members[0]))
      return {
        members,
        directEntry: members.every((member) => direct.available.has(member)) ? "available" : "absent",
        recursive,
        terminationProven: recursive ? false : null,
        evaluationPerformed: false,
      }
    }),
    diagnostics: [...diagnostics, ...placementDiagnostics],
    syncCalls: calls.filter((call) => call.callForm === "sync").map((call) => ({
      callee: call.callee,
      eligible: call.syncEligible,
      sourceSpelling: call.sourceSpelling,
      directEntry: call.directEntry,
      publishedSuspension: call.suspension,
      selectedEntry: call.syncEligible ? "direct" : null,
      selectedEntrySuspension: call.syncEligible ? "never" : null,
      blocksThread: false,
      createsTask: false,
      suspendsTask: false,
      sameTask: true,
      sameContext: true,
      sameDomain: true,
      runtimeFallback: false,
      partialEffectsBeforeRejection: false,
    })),
    public: publicResult,
    tryOrthogonal: /\btry\b/.test(source),
    overloadResolutionBeforeDirectEntry: true,
    optimizerChangesSourceValidity: false,
  }
}

function derivePlacement(source, input = {}, suspension = { declarations: [] }) {
  const diagnostics = []
  const declarationPlacement = source.match(/spawn\s*<([^>]+)>\s*fn\s+([A-Za-z_][A-Za-z0-9_]*)/)
  if (declarationPlacement) diagnostics.push({ code: "W-PLACEMENT-0001", domain: declarationPlacement[1], declaration: declarationPlacement[2] })
  const slotTexts = [...source.matchAll(/spawn\s*<([^>]+)>/g)].map((match) => ({
    raw: match[1].trim(),
    offset: match.index ?? 0,
  }))
  const parsedSlots = slotTexts.map(({ raw, offset }) => {
    const arguments_ = splitTopLevel(raw)
    let domain = null
    let mode = ".ordinary"
    const labels = new Set()
    let positional = 0
    for (const argument of arguments_) {
      const named = argument.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.+)$/)
      if (named) {
        if (labels.has(named[1])) diagnostics.push({ code: "W-LABEL-0006", declaration: "spawn", label: named[1], slot: named[1] })
        labels.add(named[1])
        if (named[1] === "domain") domain = named[2].trim()
        else if (named[1] === "mode") mode = named[2].trim()
        else diagnostics.push({ code: "W-LABEL-0005", declaration: "spawn", label: named[1], acceptedForms: ["domain:", "mode:"] })
        continue
      }
      if (positional === 0) domain = argument.trim()
      else if (positional === 1) mode = argument.trim()
      else diagnostics.push({ code: "W-LABEL-0005", declaration: "spawn", label: `position-${positional}`, acceptedForms: ["domain:", "mode:"] })
      positional += 1
    }
    if (![".ordinary", ".barrier"].includes(mode)) {
      diagnostics.push({ code: "W-PLACEMENT-0003", reason: "unknown dispatch mode", mode })
    }
    const lineStart = source.lastIndexOf("\n", offset) + 1
    const lineEnd = source.indexOf("\n", offset)
    const statement = source.slice(lineStart, lineEnd < 0 ? source.length : lineEnd)
    const call = statement.match(/(?:let|var)\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*spawn\s*<[^>]+>\s*([A-Za-z_][A-Za-z0-9_.]*)\s*\((.*)\)/)
    const accesses = call
      ? [...call[2].matchAll(/\b(ref|inout)\s+([A-Za-z_][A-Za-z0-9_.]*)/g)].map((match) => ({
          access: match[1] === "ref" ? "read" : "write",
          place: match[2],
        }))
      : []
    return { raw, offset, domain, mode, callee: call?.[1] ?? null, accesses }
  })
  const normalizedSlots = parsedSlots.map((slot) => slot.domain).filter(Boolean)
  const missingDomain = [
    ...source.matchAll(/\b(?:let|var)\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*spawn\s+(?=[A-Za-z_])/g),
    ...source.matchAll(/\bspawn\s+(?=(?:let|var)\b)/g),
  ]
  for (const match of missingDomain) {
    diagnostics.push({ code: "W-PLACEMENT-0002", reason: "missing explicit domain", offset: match.index ?? 0 })
  }
  const availableDomains = new Set(input.availableDomains ?? [])
  if (availableDomains.size > 0) {
    for (const domain of normalizedSlots) {
      if (!availableDomains.has(domain)) diagnostics.push({ code: "W-PLACEMENT-0002", reason: "unknown domain", domain })
    }
  }
  const serialDomains = new Set([
    ".main",
    ...(input.serialDomains ?? []),
    ...[...source.matchAll(/\.serial\s*\(\s*(\.[A-Za-z_][A-Za-z0-9_]*)\s*\)/g)].map((match) => match[1]),
  ])
  const suspensionByName = new Map(suspension.declarations.map((declaration) => [declaration.name, declaration.suspension]))
  const capabilities = input.domainCapabilities ?? {}
  const ticketsByDomain = new Map()
  const dispatches = parsedSlots.filter((slot) => slot.domain).map((slot) => {
    const ticket = ticketsByDomain.get(slot.domain) ?? 0
    ticketsByDomain.set(slot.domain, ticket + 1)
    const serial = serialDomains.has(slot.domain)
    const bindingCapabilities = capabilities[slot.domain]
    const barrierSupported = serial || bindingCapabilities?.includes("barrierDispatch")
    if (slot.mode === ".barrier" && Array.isArray(bindingCapabilities) && !barrierSupported) {
      diagnostics.push({ code: "W-PLACEMENT-0003", reason: "missing barrierDispatch", domain: slot.domain })
    }
    if (slot.mode === ".barrier" && slot.callee && suspensionByName.get(slot.callee) === "may") {
      diagnostics.push({ code: "W-SUSPEND-0004", reason: "barrier body may suspend", callee: slot.callee })
    }
    return {
      domain: slot.domain,
      mode: slot.mode,
      ticket,
      scheduling: serial ? "serial-fifo" : slot.mode === ".barrier" ? "exclusive-barrier" : "domain-contract",
      overlapWithinTarget: serial ? false : slot.mode === ".barrier" ? false : "capability-dependent",
      barrierSupport: slot.mode !== ".barrier" ? "not-required" : serial ? "serial" : barrierSupported ? "bound" : "required",
      callee: slot.callee,
      accesses: slot.accesses,
    }
  })
  const accessByPlace = new Map()
  for (const dispatch of dispatches) {
    for (const access of dispatch.accesses) {
      const entries = accessByPlace.get(access.place) ?? []
      entries.push({ ...access, domain: dispatch.domain, mode: dispatch.mode, ticket: dispatch.ticket })
      accessByPlace.set(access.place, entries)
      if (access.access === "write" && dispatch.mode !== ".barrier") {
        diagnostics.push({ code: "W-OWNERSHIP-0012", reason: "write requires barrier", place: access.place, domain: dispatch.domain })
      }
    }
  }
  const openPlaces = new Set(input.openPlaces ?? [])
  const loanSequences = []
  for (const [place, accesses] of accessByPlace) {
    const domains = [...new Set(accesses.map((access) => access.domain))]
    let closed = domains.length === 1 && !openPlaces.has(place)
    if (domains.length !== 1) {
      diagnostics.push({ code: "W-OWNERSHIP-0012", reason: "multiple domains", place, domains })
      closed = false
    }
    if (openPlaces.has(place)) {
      diagnostics.push({ code: "W-OWNERSHIP-0012", reason: "open access graph", place, domain: domains[0] })
      closed = false
    }
    if (accesses.some((access) => access.access === "write" && access.mode !== ".barrier")) closed = false
    const edges = []
    for (const barrier of accesses.filter((access) => access.mode === ".barrier")) {
      for (const earlier of accesses.filter((access) => access.ticket < barrier.ticket)) {
        edges.push(`${earlier.ticket}.complete->${barrier.ticket}.start`)
      }
      for (const later of accesses.filter((access) => access.ticket > barrier.ticket)) {
        edges.push(`${barrier.ticket}.complete->${later.ticket}.start`)
      }
    }
    loanSequences.push({ place, domains, accesses, edges, closed })
  }
  return {
    diagnostics,
    slots: normalizedSlots,
    dispatches,
    loanSequences,
    sameOptionalDomainForm: slotTexts.some((slot) => slot.raw === ".compute") && slotTexts.some((slot) => slot.raw === "domain: .compute"),
  }
}

function deriveExecution(source, input = {}) {
  const members = [...new Set([...source.matchAll(/\bexecution\.([A-Za-z_][A-Za-z0-9_]*)\b/g)].map((match) => match[1]))]
  const facets = [...new Set([...source.matchAll(/\bexecution#(yield|checkCancellation)\b/g)].map((match) => match[1]))]
  const root = input.root ?? (members.length > 0 || facets.length > 0 || /\bentry\b/.test(source))
  const profile = input.profile ?? (root ? "runtime" : "library")
  const diagnostics = []
  if ((members.length > 0 || facets.length > 0) && !root) {
    diagnostics.push({ code: "W-EXECUTION-0001", profile, reason: "runtime root unavailable" })
  }
  for (const member of [...members, ...facets]) {
    if ((input.unavailableMembers ?? []).includes(member)) {
      diagnostics.push({ code: "W-EXECUTION-0003", member, profile, reason: "target capability unavailable" })
    }
  }
  if (/return\s+(?:ref\s+)?execution\b/.test(source)) diagnostics.push({ code: "W-EXECUTION-0002", reason: "execution-root reification" })
  if (/\bservice(?:\.[A-Za-z_][A-Za-z0-9_]*|[A-Za-z_][A-Za-z0-9_]*)?\s*\([^)]*\bexecution\b/.test(source)) {
    diagnostics.push({ code: "W-EXECUTION-0002", reason: "service crossing" })
  }
  if (/\bserialize(?:\.[A-Za-z_][A-Za-z0-9_]*|[A-Za-z_][A-Za-z0-9_]*)?\s*\([^)]*\bexecution\b/.test(source)) {
    diagnostics.push({ code: "W-EXECUTION-0002", reason: "serialization" })
  }
  return { root, profile, members, facets, contextual: members.length > 0 || facets.length > 0, diagnostics }
}

function deriveDoctest(source, input = {}) {
  const lines = source.split(/\r?\n/)
  const examples = []
  let current = null
  let inDocBlock = false
  for (const line of lines) {
    const startsBlock = /^\s*\/\*\*/.test(line)
    const isLineDoc = /^\s*\/\/\//.test(line)
    if (!inDocBlock && !startsBlock && !isLineDoc) {
      current = null
      continue
    }
    if (startsBlock) inDocBlock = true
    const endsBlock = inDocBlock && /\*\/\s*$/.test(line)
    const normalized = line
      .replace(/^\s*\/\*\*/, "")
      .replace(/\*\/\s*$/, "")
      .replace(/^\s*\/\/\//, "")
      .replace(/^\s*\*\s?/, "")
      .trim()
    const call = normalized.match(/^call:\s*(.+)$/)
    if (call) {
      current = { call: null, terminals: [], lines: [] }
      current.call = call[1]
      current.lines.push(normalized)
      examples.push(current)
    } else if (current) {
      const terminal = normalized.match(/^(result|error):\s*(.+)$/)
      if (terminal) {
        current.lines.push(normalized)
        current.terminals.push({ kind: terminal[1], value: terminal[2] })
      }
    }
    if (endsBlock) {
      inDocBlock = false
      current = null
    }
  }
  const diagnostics = []
  for (const example of examples) {
    if (!example.call || example.terminals.length !== 1) diagnostics.push({ code: "W-DOC-0003", reason: "terminal" })
    const ambient = /execution(?:\.|#)|fetch\s*\(|readFile\s*\(|clock\s*\(|network\s*\(/.test(example.call ?? "")
    if (ambient && !input.fixture) diagnostics.push({ code: "W-DOC-0005", reason: "ambient effect" })
  }
  return { examples, diagnostics, hermetic: examples.length > 0 && diagnostics.length === 0, releasePayload: false }
}

export function deriveDynamicSerial(input = null) {
  if (!input) return { status: "not-requested", phase: "absent", trace: [] }
  const profile = input.profile ?? {}
  const request = input.request ?? {}
  const operations = input.operations ?? []
  const positiveInteger = (value) => Number.isSafeInteger(value) && value > 0
  const nonnegativeInteger = (value) => Number.isSafeInteger(value) && value >= 0
  const state = {
    phase: "absent",
    live: input.live ?? 0,
    aggregateReadyUsed: input.aggregateReadyUsed ?? 0,
    aggregateFrameBytesUsed: input.aggregateFrameBytesUsed ?? 0,
    reservedJobs: 0,
    reservedFrameBytes: 0,
    outstandingJobs: [],
    outstandingFrameBytes: 0,
    readyQueue: [],
    activeSegment: null,
  }
  const trace = []
  const reject = (error, operation) => {
    const rejectedOperation = operations[operation]
    return {
      status: "rejected",
      error,
      operation,
      phase: state.phase,
      poolReuse: true,
      referenceExtendsOwner: false,
      recoveredInput: rejectedOperation?.op === "admit" ? rejectedOperation.input ?? null : null,
      state,
      trace,
    }
  }
  for (const [index, operation] of operations.entries()) {
    const before = structuredClone(state)
    if (operation.op === "open") {
      if (!input.authority || !profile.enabled) return reject("authorityUnavailable", index)
      if (request.kind !== "serial") return reject("unsupportedKind", index)
      if (typeof profile.pool !== "string" || profile.pool.length === 0) return reject("poolUnavailable", index)
      if (![profile.liveLimit, profile.aggregateReadyJobs, profile.aggregateFrameBytes,
        profile.laneMaximumJobs, profile.laneMaximumFrameBytes].every(positiveInteger)) {
        return reject("invalidProfile", index)
      }
      if (![state.live, state.aggregateReadyUsed, state.aggregateFrameBytesUsed].every(nonnegativeInteger)) {
        return reject("invalidState", index)
      }
      if (state.live >= profile.liveLimit) return reject("liveBudgetExhausted", index)
      if (!positiveInteger(request.readyJobs) || request.readyJobs > profile.laneMaximumJobs) {
        return reject("laneLimitExceeded", index)
      }
      if (!positiveInteger(request.frameBytes) || request.frameBytes > profile.laneMaximumFrameBytes) {
        return reject("laneFrameLimitExceeded", index)
      }
      if (state.aggregateReadyUsed + request.readyJobs > profile.aggregateReadyJobs) {
        return reject("aggregateReadyExhausted", index)
      }
      if (state.aggregateFrameBytesUsed + request.frameBytes > profile.aggregateFrameBytes) {
        return reject("aggregateFrameBytesExhausted", index)
      }
      state.phase = "open"
      state.live += 1
      state.aggregateReadyUsed += request.readyJobs
      state.aggregateFrameBytesUsed += request.frameBytes
      state.reservedJobs = request.readyJobs
      state.reservedFrameBytes = request.frameBytes
    } else if (operation.op === "admit") {
      if (state.phase !== "open") return reject("closedDomain", index)
      if (state.outstandingJobs.length >= state.reservedJobs) return reject("readyBudgetExhausted", index)
      if (!positiveInteger(operation.frameBytes)) return reject("invalidFrameBytes", index)
      if (!("input" in operation)) return reject("missingInput", index)
      if (state.outstandingJobs.some((job) => job.id === operation.job)) return reject("duplicateJob", index)
      if (state.outstandingFrameBytes + operation.frameBytes > state.reservedFrameBytes) {
        return reject("frameBudgetExhausted", index)
      }
      state.outstandingJobs.push({
        id: operation.job,
        frameBytes: operation.frameBytes,
        input: operation.input,
        state: "ready",
      })
      state.outstandingFrameBytes += operation.frameBytes
      state.readyQueue.push(operation.job)
    } else if (operation.op === "start") {
      if (state.phase !== "open" && state.phase !== "closing") return reject("closedDomain", index)
      if (state.activeSegment !== null) return reject("laneBusy", index)
      const job = state.outstandingJobs.find((candidate) => candidate.id === operation.job)
      if (!job) return reject("unknownJob", index)
      if (job.state !== "ready") return reject("jobNotReady", index)
      if (state.readyQueue[0] !== operation.job) return reject("fifoViolation", index)
      state.readyQueue.shift()
      job.state = "active"
      state.activeSegment = operation.job
    } else if (operation.op === "suspend") {
      if (state.activeSegment !== operation.job) return reject("jobNotActive", index)
      const job = state.outstandingJobs.find((candidate) => candidate.id === operation.job)
      if (!job) return reject("unknownJob", index)
      job.state = "suspended"
      state.activeSegment = null
    } else if (operation.op === "resume") {
      if (state.phase !== "open" && state.phase !== "closing") return reject("closedDomain", index)
      if (state.activeSegment !== null) return reject("laneBusy", index)
      const job = state.outstandingJobs.find((candidate) => candidate.id === operation.job)
      if (!job) return reject("unknownJob", index)
      if (job.state !== "suspended") return reject("jobNotSuspended", index)
      job.state = "active"
      state.activeSegment = operation.job
    } else if (operation.op === "complete") {
      if (state.activeSegment !== operation.job) return reject("jobNotActive", index)
      const job = state.outstandingJobs.findIndex((candidate) => candidate.id === operation.job)
      if (job < 0) return reject("unknownJob", index)
      state.outstandingFrameBytes -= state.outstandingJobs[job].frameBytes
      state.outstandingJobs.splice(job, 1)
      state.activeSegment = null
    } else if (operation.op === "close") {
      if (state.phase !== "open") return reject("closeRequiresOpen", index)
      state.phase = "closing"
    } else if (operation.op === "drain") {
      if (state.phase !== "closing") return reject("drainRequiresClosing", index)
      if (state.outstandingJobs.length > 0 || state.readyQueue.length > 0 || state.activeSegment !== null) {
        return reject("drainPending", index)
      }
      state.phase = "drained"
      state.live -= 1
      state.aggregateReadyUsed -= state.reservedJobs
      state.aggregateFrameBytesUsed -= state.reservedFrameBytes
      state.reservedJobs = 0
      state.reservedFrameBytes = 0
    } else return reject("unknownOperation", index)
    trace.push({ operation: operation.op, before, after: structuredClone(state) })
  }
  return {
    status: "accepted",
    phase: state.phase,
    poolReuse: true,
    referenceExtendsOwner: false,
    state,
    trace,
  }
}

function deriveStd(input = {}) {
  const modules = Array.isArray(input.modules) ? input.modules : []
  const hasTierField = modules.some((module) => Object.prototype.hasOwnProperty.call(module, "tier"))
  const authorities = modules.length === 0
    ? []
    : ["targetFacts", "capabilities", "provider", "reachability"].filter((field) => modules.every((module) => field in module || (field === "provider" && "providerStatus" in module)))
  return { hasTierField, authorities, moduleCount: modules.length }
}

export function deriveExecutionErgonomics(source, input = {}) {
  const clean = withoutComments(source)
  const declarations = declarationBodies(clean)
  const labels = deriveCallableLabels(clean, declarations, input)
  const generic = parseGenericHeads(clean)
  const suspension = deriveSuspension(clean, declarations, input)
  return {
    labels: { ...labels, generic },
    suspension,
    placement: derivePlacement(clean, input, suspension),
    execution: deriveExecution(clean, input),
    doctest: deriveDoctest(source, input),
    dynamicSerial: deriveDynamicSerial(input.dynamicSerial),
    std: deriveStd(input.std),
    forms: {
      direct: "same-task/neverSuspend",
      await: "same-task/maySuspend",
      sync: "same-task/directEntry/neverSuspend",
      async: "structured-child/current-domain",
      spawn: "structured-child/explicit-domain",
    },
  }
}

export function summarizeDiagnostics(result) {
  return result.labels.diagnostics
    .concat(result.labels.generic.diagnostics)
    .concat(result.suspension.diagnostics)
    .concat(result.placement.diagnostics)
    .concat(result.execution.diagnostics)
    .concat(result.doctest.diagnostics)
    .concat(result.std.hasTierField ? [{ code: "W-STD-0001" }] : [])
    .map((diagnostic) => diagnostic.code)
}
