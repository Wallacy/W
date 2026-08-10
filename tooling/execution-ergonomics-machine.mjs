const identifier = /^[A-Za-z_][A-Za-z0-9_]*$/

function withoutComments(source) {
  return source
    .replace(/\/\*[\s\S]*?\*\//g, "")
    .replace(/(^|\s)\/\/.*$/gm, "$1")
}

export function splitTopLevel(text, separator = ",") {
  const parts = []
  let start = 0
  let depth = 0
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
    if ("([{<".includes(character)) depth += 1
    else if (") ]}>".replaceAll(" ", "").includes(character)) depth -= 1
    else if (character === separator && depth === 0) {
      parts.push(text.slice(start, index).trim())
      start = index + 1
    }
  }
  const tail = text.slice(start).trim()
  if (tail || text.trim() === "") parts.push(tail)
  return parts.filter(Boolean)
}

function parseParameter(raw, index) {
  const cleaned = raw.replace(/=.*/, "").trim()
  if (!cleaned || cleaned === "...") return null
  let tokens = cleaned.split(/\s+/)
  const modifiers = new Set(["inout", "take", "ref", "copy", "shared", "weak", "view", "mut"])
  while (modifiers.has(tokens[0])) tokens = tokens.slice(1)
  let external = null
  let internal = null
  if (tokens[0] === "_") {
    internal = tokens[1]?.replace(/:.*/, "")
    return internal && identifier.test(internal)
      ? { index, internal, policy: "optional(name)", forms: ["positional", `${internal}:`] }
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
    return { index, internal, external, policy: `required(${external})`, forms: [`${external}:`] }
  }
  const nameToken = tokens.find((token) => token.includes(":")) ?? tokens[0]
  internal = nameToken.replace(/:.*/, "")
  if (!identifier.test(internal)) return null
  if (index === 0) return { index, internal, policy: "positionalOnly", forms: ["positional"] }
  return { index, internal, external: internal, policy: `required(${internal})`, forms: [`${internal}:`] }
}

function parseParameters(raw) {
  return splitTopLevel(raw).map(parseParameter).filter(Boolean)
}

function declarationBodies(source) {
  const declarations = []
  const pattern = /\b(?:(export)\s+)?((?:unsafe\s+)?(?:mut\s+|take\s+)?(?:async\s+)?)fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^>{}]*>)?\s*\(([\s\S]*?)\)\s*(?:[^\{\n]*)(\{)?/g
  for (const match of source.matchAll(pattern)) {
    const start = match.index ?? 0
    const line = source.slice(source.lastIndexOf("\n", start) + 1, start + match[0].length)
    const bodyStart = start + match[0].lastIndexOf("{")
    let body = ""
    if (match[5]) {
      let depth = 0
      for (let index = bodyStart; index < source.length; index += 1) {
        if (source[index] === "{") depth += 1
        else if (source[index] === "}") {
          depth -= 1
          if (depth === 0) {
            body = source.slice(bodyStart + 1, index)
            break
          }
        }
      }
    }
    declarations.push({
      name: match[3],
      params: parseParameters(match[4]),
      body,
      explicitAsync: /\basync\s+fn\b/.test(line),
      hasBody: Boolean(match[5]),
      exported: Boolean(match[1]),
    })
  }
  return declarations
}

function parseCalls(source) {
  const calls = []
  const lines = source.split(/\r?\n/)
  for (const [lineIndex, rawLine] of lines.entries()) {
    let line = rawLine.trim()
    if (!line || /^\/\//.test(line)) continue
    if (/\bfn\s+[A-Za-z_]/.test(line)) {
      const declaration = line.search(/\bfn\s+[A-Za-z_]/)
      const brace = line.indexOf("{", declaration)
      if (brace < 0) continue
      line = line.slice(brace + 1).trim()
    }
    for (const match of line.matchAll(/(?:\.|\b)([A-Za-z_][A-Za-z0-9_]*)\s*\(([^()]*)\)/g)) {
      const callee = match[1]
      if (["if", "for", "while", "switch", "catch", "return", "Task", "yield"].includes(callee)) continue
      const args = splitTopLevel(match[2])
      const labels = []
      for (const argument of args) {
        const labelMatch = argument.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*:/)
        if (labelMatch) labels.push(labelMatch[1])
      }
      const statementStart = line.lastIndexOf(";", match.index ?? 0) + 1
      const statement = line.slice(statementStart, (match.index ?? 0) + match[0].length)
      let callForm = "direct"
      if (/\btry\s+await\b|\bawait\b/.test(statement)) callForm = "await"
      else if (/\basync\s+let\b/.test(statement)) callForm = "async let"
      else if (/\bspawn\s*</.test(statement)) callForm = "spawn"
      calls.push({ callee, labels, callForm, line: lineIndex + 1, source: line })
    }
  }
  return calls
}

function completeCallShapes(parameters) {
  let shapes = [[]]
  for (const parameter of parameters) {
    shapes = shapes.flatMap((shape) => parameter.forms.map((form) => [...shape, form]))
  }
  return shapes.map((shape) => shape.join("|"))
}

function deriveCallableLabels(source, declarations) {
  const byName = new Map()
  const diagnostics = []
  for (const declaration of declarations) {
    const callShapes = completeCallShapes(declaration.params)
    const previous = byName.get(declaration.name) ?? []
    for (const prior of previous) {
      const overlap = callShapes.find((shape) => prior.callShapes.includes(shape))
      if (overlap) {
        diagnostics.push({
          code: "W-LABEL-0004",
          declaration: declaration.name,
          overlap,
          declarations: [prior.ordinal, declarations.indexOf(declaration)],
        })
      }
    }
    byName.set(declaration.name, [...previous, { callShapes, params: declaration.params, ordinal: declarations.indexOf(declaration) }])
  }
  const calls = parseCalls(source)
  for (const call of calls) {
    const declarationsForName = byName.get(call.callee) ?? []
    if (declarationsForName.length === 0) continue
    const acceptedLabels = declarationsForName.flatMap((declaration) => declaration.params.flatMap((parameter) => parameter.forms.filter((form) => form.endsWith(":"))))
    const duplicate = call.labels.find((label, index) => call.labels.indexOf(label) !== index)
    if (duplicate) diagnostics.push({ code: "W-LABEL-0006", declaration: call.callee, label: duplicate, slot: duplicate })
    const unknown = call.labels.find((label) => !acceptedLabels.includes(`${label}:`))
    if (unknown) diagnostics.push({ code: "W-LABEL-0005", declaration: call.callee, label: unknown, acceptedForms: acceptedLabels })
  }
  return {
    declarations: declarations.map((declaration, ordinal) => ({
      name: declaration.name,
      ordinal,
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

function derivePlacement(source) {
  const diagnostics = []
  const declarationPlacement = source.match(/spawn\s*<([^>]+)>\s*fn\s+([A-Za-z_][A-Za-z0-9_]*)/)
  if (declarationPlacement) diagnostics.push({ code: "W-PLACEMENT-0001", domain: declarationPlacement[1], declaration: declarationPlacement[2] })
  const slots = [...source.matchAll(/spawn\s*<([^>]+)>/g)].map((match) => match[1].trim())
  const normalizedSlots = slots.map((slot) => (slot.startsWith("domain:") ? slot.replace(/^domain:\s*/, "").trim() : slot))
  const serialDomains = [...source.matchAll(/\.serial\s*\(\s*(\.[A-Za-z_][A-Za-z0-9_]*)\s*\)/g)].map((match) => match[1])
  for (const domain of serialDomains) {
    if (normalizedSlots.includes(domain)) diagnostics.push({ code: "W-PLACEMENT-0002", domain, capacity: "1" })
  }
  return {
    diagnostics,
    slots: normalizedSlots,
    sameOptionalDomainForm: slots.includes(".compute") && slots.includes("domain: .compute"),
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
  return {
    labels: { ...labels, generic },
    suspension: deriveSuspension(clean, declarations, input),
    placement: derivePlacement(clean),
    process: deriveProcess(clean, input),
    doctest: deriveDoctest(source, input),
    std: deriveStd(input.std),
    forms: {
      direct: "same-task/neverSuspend",
      await: "same-task/maySuspend",
      "async let": "structured-child/current-domain",
      spawn: "structured-child/parallel-intent",
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
