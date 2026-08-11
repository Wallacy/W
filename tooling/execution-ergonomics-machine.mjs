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
  const modifiers = new Set(["inout", "take", "ref", "copy", "shared", "weak", "view", "mut"])
  while (modifiers.has(tokens[0])) tokens = tokens.slice(1)
  let external = null
  let internal = null
  if (tokens[0] === "named") {
    internal = tokens[1]?.replace(/:.*/, "")
    return internal && identifier.test(internal)
      ? {
          index,
          internal,
          external: internal,
          policy: `required(${internal})`,
          forms: [`${internal}:`],
          hasDefault,
          variadic,
          named: true,
        }
      : null
  }
  if (tokens[0] === "_") {
    internal = tokens[1]?.replace(/:.*/, "")
    return internal && identifier.test(internal)
      ? { index, internal, policy: "optional(name)", forms: ["positional", `${internal}:`], hasDefault, variadic }
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
    return { index, internal, external, policy: `required(${external})`, forms: [`${external}:`], hasDefault, variadic }
  }
  const nameToken = tokens.find((token) => /^[A-Za-z_][A-Za-z0-9_]*:/.test(token))
  if (!nameToken) {
    return {
      index,
      internal: `$${index}`,
      policy: "positionalOnly",
      forms: ["positional"],
      hasDefault,
      variadic,
      unnamed: true,
    }
  }
  internal = nameToken.replace(/:.*/, "")
  if (!identifier.test(internal)) return null
  return { index, internal, policy: "positionalOnly", forms: ["positional"], hasDefault, variadic }
}

const leadingParameterKeywords = new Set([
  "const",
  "copy",
  "inout",
  "mut",
  "pin",
  "ref",
  "shared",
  "take",
  "view",
  "weak",
])

function parameterContractFacts(raw) {
  const cleaned = splitTopLevel(raw, "=")[0].trim()
  const leading = cleaned.match(/^([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.+)$/s)
  const leadingModifier = leadingParameterKeywords.has(leading?.[1])
    ? leading[1]
    : null
  const contractMode = cleaned.match(/:\s*(const|inout|ref|take)\b/)?.[1] ?? "value"
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
      const parameter = raw.match(/^named\s+([A-Za-z_][A-Za-z0-9_]*)\s*:/)
      if (parameter) {
        diagnostics.push({
          code: "W-LABEL-0007",
          declaration: "init",
          parameter: parameter[1],
          context: "initializer",
          reason: "record-label-already-required",
        })
      }
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

function parseCalls(source) {
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
      const operation = expression.match(/^(copy|inout|pin|ref|take)\b/)?.[1] ?? "value"
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
        start: cursor + 1 + argumentPart.start,
        end: cursor + 1 + argumentPart.end,
      })
    }

    const statementStart = Math.max(
      source.lastIndexOf("\n", start),
      source.lastIndexOf(";", start),
      source.lastIndexOf("{", start),
    ) + 1
    const statement = source.slice(statementStart, closing + 1)
    let callForm = "direct"
    if (/\btry\s+await\b|\bawait\b/.test(statement)) callForm = "await"
    else if (/\basync\s+let\b/.test(statement)) callForm = "async let"
    else if (/\bspawn\s*</.test(statement)) callForm = "spawn"

    let memberCursor = start - 1
    while (/\s/.test(mask[memberCursor] ?? "")) memberCursor -= 1
    calls.push({
      callee,
      arguments: argumentsWithContracts,
      end: closing + 1,
      labels,
      forms,
      member: mask[memberCursor] === ".",
      start,
      callForm,
      line: source.slice(0, start).split("\n").length,
      source: source.slice(start, closing + 1).replace(/\s+/g, " ").trim(),
    })
  }
  return calls
}

function completeCallShapes(parameters) {
  let shapes = [[]]
  for (const parameter of parameters) {
    if (parameter.variadic) {
      const extended = shapes.flatMap((shape) =>
        parameter.forms.map((form) => [...shape, `${form}...`]))
      shapes = [...shapes, ...extended]
      continue
    }
    const extended = shapes.flatMap((shape) => parameter.forms.map((form) => [...shape, form]))
    shapes = parameter.hasDefault ? [...shapes, ...extended] : extended
  }
  return shapes.map((shape) => shape.join("|"))
}

function epsilonClosure(parameters, initialStates) {
  const states = new Set(initialStates)
  const pending = [...states]
  while (pending.length > 0) {
    const state = pending.pop()
    const parameterIndex = Math.floor(state / 2)
    const parameter = parameters[parameterIndex]
    const continuation = state % 2 === 1
    if (!parameter) continue
    if (!continuation && !parameter.hasDefault && !parameter.variadic) continue
    const next = (parameterIndex + 1) * 2
    if (!states.has(next)) {
      states.add(next)
      pending.push(next)
    }
  }
  return states
}

function consumeForm(parameters, states, form) {
  const next = new Set()
  for (const state of epsilonClosure(parameters, states)) {
    const parameterIndex = Math.floor(state / 2)
    const parameter = parameters[parameterIndex]
    if (!parameter) continue
    const continuation = state % 2 === 1
    if (continuation) {
      if (parameter.variadic && form === "positional") next.add(state)
      continue
    }
    if (!parameter.forms.includes(form)) continue
    next.add(parameter.variadic ? state + 1 : state + 2)
  }
  return epsilonClosure(parameters, next)
}

export function acceptsCallShape(parameters, forms) {
  let states = epsilonClosure(parameters, new Set([0]))
  for (const form of forms) {
    states = consumeForm(parameters, states, form)
    if (states.size === 0) return false
  }
  return states.has(parameters.length * 2)
}

function operationMatchesContract(parameter, argument) {
  if (argument.operation === "ref") return parameter.contractMode === "ref"
  if (argument.operation === "inout") return parameter.contractMode === "inout"
  if (argument.operation === "take") return parameter.contractMode === "take"
  if (argument.operation === "value") return parameter.contractMode !== "inout"
  return parameter.contractMode !== "inout"
}

export function acceptsCallContract(parameters, args) {
  const memo = new Map()
  function visit(parameterIndex, argumentIndex, variadicStarted = false) {
    const key = `${parameterIndex}:${argumentIndex}:${variadicStarted}`
    if (memo.has(key)) return memo.get(key)
    if (parameterIndex === parameters.length) {
      const accepted = argumentIndex === args.length
      memo.set(key, accepted)
      return accepted
    }
    const parameter = parameters[parameterIndex]
    if (argumentIndex === args.length) {
      const accepted = (parameter.hasDefault || parameter.variadic)
        && visit(parameterIndex + 1, argumentIndex, false)
      memo.set(key, accepted)
      return accepted
    }
    const argument = args[argumentIndex]
    let accepted = false
    if ((parameter.hasDefault || parameter.variadic)
      && visit(parameterIndex + 1, argumentIndex, false)) {
      accepted = true
    }
    const formMatches = variadicStarted
      ? argument.form === "positional"
      : parameter.forms.includes(argument.form)
    if (!accepted
      && formMatches
      && operationMatchesContract(parameter, argument)) {
      accepted = parameter.variadic
        ? visit(parameterIndex, argumentIndex + 1, true)
          || visit(parameterIndex + 1, argumentIndex + 1, false)
        : visit(parameterIndex + 1, argumentIndex + 1, false)
    }
    memo.set(key, accepted)
    return accepted
  }
  return visit(0, 0)
}

function overlappingCallShape(left, right) {
  const alphabet = [...new Set(
    left.concat(right).flatMap((parameter) =>
      parameter.variadic ? [...parameter.forms, "positional"] : parameter.forms),
  )].sort()
  const initialLeft = epsilonClosure(left, new Set([0]))
  const initialRight = epsilonClosure(right, new Set([0]))
  const queue = [{ left: initialLeft, right: initialRight, forms: [] }]
  const visited = new Set()

  while (queue.length > 0) {
    const state = queue.shift()
    const key = `${[...state.left].sort().join(",")}|${[...state.right].sort().join(",")}`
    if (visited.has(key)) continue
    visited.add(key)
    if (state.left.has(left.length * 2) && state.right.has(right.length * 2)) {
      return state.forms.join("|")
    }
    for (const form of alphabet) {
      const nextLeft = consumeForm(left, state.left, form)
      const nextRight = consumeForm(right, state.right, form)
      if (nextLeft.size > 0 && nextRight.size > 0) {
        queue.push({ left: nextLeft, right: nextRight, forms: [...state.forms, form] })
      }
    }
  }
  return null
}

function deriveCallableLabels(source, declarations) {
  const byScopedName = new Map()
  const byName = new Map()
  const diagnostics = recordLabelDiagnostics(source)
  for (const declaration of declarations) {
    for (const parameter of declaration.params) {
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
  const calls = parseCalls(source)
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
      acceptsCallShape(declaration.params, call.forms))
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
        || acceptsCallContract(declaration.params, call.arguments))) {
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
      const optional = token.match(/^_\s*([A-Za-z_][A-Za-z0-9_]*)\s*:/)
      const named = token.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:/)
      if (optional) return { index, name: optional[1], policy: "optional(name)", forms: ["positional", `${optional[1]}:`] }
      if (named) return { index, name: named[1], policy: "required(name)", forms: [`${named[1]}:`] }
      return { index, name: token, policy: "type", forms: ["positional"] }
    })
    heads.push({ name: match[1], parameters })
  }
  const diagnostics = []
  const identities = []
  for (const head of heads) {
    const uses = [...source.matchAll(new RegExp(`\\b${head.name}\\s*<([^>\n]+)>`, "g"))]
      .filter((match) => !new RegExp(`\\b(?:struct|enum|type|fn)\\s+$`).test(source.slice(0, match.index)))
    for (const use of uses) {
      const args = splitTopLevel(use[1])
      const suppliedLabels = args.map((argument) => argument.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:/)?.[1]).filter(Boolean)
      const duplicateLabel = suppliedLabels.find((label, index) => suppliedLabels.indexOf(label) !== index)
      if (duplicateLabel) diagnostics.push({ code: "W-LABEL-0006", declaration: head.name, label: duplicateLabel, slot: duplicateLabel })
      const values = head.parameters.map((parameter, index) => {
        const raw = args[index] ?? ""
        const named = raw.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.*)$/)
        if (named && named[1] !== parameter.name) {
          diagnostics.push({ code: "W-LABEL-0005", declaration: head.name, label: named[1], acceptedForms: parameter.forms })
        }
        return named ? named[2].trim() : raw.trim()
      })
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
  const may = new Set(input.functionTypes?.filter((item) => item.suspension === "may").map((item) => item.name) ?? [])
  for (const declaration of declarations) {
    if (declaration.explicitAsync || /\bawait\b|\basync\s+let\b|\bspawn\s*</.test(declaration.body)) may.add(declaration.name)
    for (const callee of bodyCalls(declaration.body)) if (byName.has(callee)) edges.get(declaration.name).add(callee)
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
  const calls = parseCalls(source).map((call) => {
    const suspension = may.has(call.callee) ? "may" : "never"
    const invalidBare = suspension === "may" && call.callForm === "direct"
    return { ...call, suspension, invalidBare }
  })
  const diagnostics = calls.filter((call) => call.invalidBare).map((call) => ({ code: "W-SUSPEND-0001", callee: call.callee, callForm: call.callForm }))
  const removable = calls.filter((call) => call.suspension === "never" && call.callForm === "await")
  for (const call of removable) diagnostics.push({ code: "W-SUSPEND-0002", callee: call.callee, callForm: call.callForm })
  if (/\bblockingWait\s*\(/.test(source)) diagnostics.push({ code: "W-SUSPEND-0003", operation: "blockingWait" })
  const children = calls.filter((call) => call.callForm === "async let" || call.callForm === "spawn").map((call) => ({
    form: call.callForm,
    callee: call.callee,
    accepts: ["never", "may"],
  }))
  const publicContract = input.publicContract
  const publicResult = publicContract
    ? {
        previous: publicContract.previous,
        current: publicContract.current,
        explicit: Boolean(publicContract.explicitAsync),
        widening: publicContract.previous === "never" && publicContract.current === "may",
        sourceBreaking: publicContract.exported !== false && publicContract.previous === "never" && publicContract.current === "may",
      }
    : null
  return {
    declarations: declarations.map((declaration) => ({ name: declaration.name, suspension: may.has(declaration.name) ? "may" : "never", sourceSpelling: declaration.explicitAsync ? "explicit" : may.has(declaration.name) ? "inferred" : "none" })),
    calls,
    children,
    staging: children.length > 0 ? ["callee", "arguments", "captures", "reserve", "publish"] : [],
    scc: components.map((members) => ({ members, suspension: members.some((member) => may.has(member)) ? "may" : "never" })),
    diagnostics,
    public: publicResult,
    tryOrthogonal: /\btry\b/.test(source),
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
    const lineEnd = source.indexOf("\n", offset)
    const statement = source.slice(offset, lineEnd < 0 ? source.length : lineEnd)
    const call = statement.match(/(?:let|var)\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*([A-Za-z_][A-Za-z0-9_]*)\s*\((.*)\)/)
    const accesses = call
      ? [...call[2].matchAll(/\b(ref|inout)\s+([A-Za-z_][A-Za-z0-9_.]*)/g)].map((match) => ({
          access: match[1] === "ref" ? "read" : "write",
          place: match[2],
        }))
      : []
    return { raw, offset, domain, mode, callee: call?.[1] ?? null, accesses }
  })
  const normalizedSlots = parsedSlots.map((slot) => slot.domain).filter(Boolean)
  const missingDomain = [...source.matchAll(/\bspawn\s+(?=(?:let|var)\b)/g)]
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

function deriveProcess(source, input = {}) {
  const projections = [...new Set([...source.matchAll(/process\.(args|context|ctx)\b/g)].map((match) => match[1]))]
  const root = input.root ?? /\bentry\b/.test(source)
  const profile = input.profile ?? (root ? "native-process" : "non-process")
  const diagnostics = []
  if (projections.includes("ctx")) diagnostics.push({ code: "W-PROCESS-0003", member: "process.ctx", canonical: "process.context" })
  if (projections.some((projection) => projection !== "ctx") && (!root || profile !== "native-process")) diagnostics.push({ code: "W-PROCESS-0001", profile })
  if (/return\s+(?:ref\s+)?process\.context\b/.test(source)) diagnostics.push({ code: "W-PROCESS-0002", reason: "entry-root escape" })
  if (/\bservice(?:\.[A-Za-z_][A-Za-z0-9_]*|[A-Za-z_][A-Za-z0-9_]*)?\s*\([^)]*\bprocess\.context\b/.test(source)) {
    diagnostics.push({ code: "W-PROCESS-0002", reason: "service crossing" })
  }
  if (/\bserialize(?:\.[A-Za-z_][A-Za-z0-9_]*|[A-Za-z_][A-Za-z0-9_]*)?\s*\([^)]*\bprocess\.context\b/.test(source)) {
    diagnostics.push({ code: "W-PROCESS-0002", reason: "serialization" })
  }
  return { root, profile, projections, readOnly: projections.some((projection) => projection !== "ctx"), diagnostics }
}

function deriveDoctest(source, input = {}) {
  const lines = source.split(/\r?\n/)
  const examples = []
  let current = null
  for (const line of lines) {
    const trimmedLine = line.trim()
    if (current && trimmedLine === "*/") {
      current = null
      continue
    }
    const normalized = line
      .replace(/^\s*\/\*\*?/, "")
      .replace(/\*\/\s*$/, "")
      .replace(/^\s*\/\/\//, "")
      .replace(/^\s*\*\s?/, "")
      .trim()
    if (normalized === "@example") {
      current = { call: null, terminals: [], lines: [] }
      examples.push(current)
      continue
    }
    if (!current) continue
    current.lines.push(normalized)
    const call = normalized.match(/^call:\s*(.+)$/)
    if (call) current.call = call[1]
    const terminal = normalized.match(/^(result|error):\s*(.+)$/)
    if (terminal) current.terminals.push({ kind: terminal[1], value: terminal[2] })
  }
  const diagnostics = []
  for (const example of examples) {
    if (!example.call || example.terminals.length !== 1) diagnostics.push({ code: "W-DOC-0003", reason: "terminal" })
    const ambient = /process\.(args|context)|fetch\s*\(|readFile\s*\(|clock\s*\(|network\s*\(/.test(example.call ?? "")
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
  const labels = deriveCallableLabels(clean, declarations)
  const generic = parseGenericHeads(clean)
  const suspension = deriveSuspension(clean, declarations, input)
  return {
    labels: { ...labels, generic },
    suspension,
    placement: derivePlacement(clean, input, suspension),
    process: deriveProcess(clean, input),
    doctest: deriveDoctest(source, input),
    dynamicSerial: deriveDynamicSerial(input.dynamicSerial),
    std: deriveStd(input.std),
    forms: {
      direct: "same-task/neverSuspend",
      await: "same-task/maySuspend",
      "async let": "structured-child/current-domain",
      spawn: "structured-child/explicit-domain",
    },
  }
}

export function summarizeDiagnostics(result) {
  return result.labels.diagnostics
    .concat(result.labels.generic.diagnostics)
    .concat(result.suspension.diagnostics)
    .concat(result.placement.diagnostics)
    .concat(result.process.diagnostics)
    .concat(result.doctest.diagnostics)
    .concat(result.std.hasTierField ? [{ code: "W-STD-0001" }] : [])
    .map((diagnostic) => diagnostic.code)
}
