// W-1517 makes `.bounded` a lexical design plan. ASC0 still models only the
// fixed/custom lifecycle; budget and backing receipts use the W-1517 oracle.
const activePlans = new Set(["fixed", "bounded", "custom"])

function isNoneAllocator(value) {
  return value === undefined || value === null || value === ".none" || value === "none"
}

function contextItem(identity, source, input = {}) {
  return {
    identity,
    origin: input.origin ?? identity,
    mobility: input.mobility ?? "local",
    requirement: input.requirement ?? "any",
    resolutionSource: source,
    name: input.name,
    named: input.name !== undefined,
    sourceBindingExpected: input.sourceBindingExpected ?? input.name !== undefined,
  }
}

function contextualStack(input) {
  const stack = []
  if (!isNoneAllocator(input.rootAllocator)) {
    stack.push(contextItem(
      input.rootAllocator,
      "productDefault",
      { mobility: input.rootMobility ?? "crossDomain", requirement: input.rootRequirement },
    ))
  }
  if (input.contextualParameter) {
    stack.push(contextItem(
      input.contextualParameter.identity ?? "parameter:allocator",
      "contextualParameter",
      input.contextualParameter,
    ))
  }
  let blocks = input.lexicalBlocks
  // The old ASC0 cases used `scope` as a lexical lease. Normalize that input
  // once at the boundary; all later resolution uses contextStack only.
  if (!Array.isArray(blocks) && input.scope !== undefined && input.scope !== null) {
    blocks = [{ identity: input.scope, name: input.scope, legacyScope: true }]
  }
  for (const block of blocks ?? []) {
    stack.push(contextItem(
      block.identity ?? block.name ?? "anonymous-lease",
      "lexicalBlock",
      block,
    ))
  }
  return stack
}

function availableAllocators(state) {
  return [...new Set(state.contextStack.map((item) => item.identity))]
}

function expectedRequirement(requirement) {
  return requirement && requirement !== "any" ? requirement : "Allocator"
}

function explicitAllocatorFacts(state, operation, identity) {
  const known = [...state.contextStack].reverse().find((item) => item.identity === identity)
  if (known) {
    return {
      origin: known.origin ?? known.identity,
      mobility: known.mobility,
      requirement: known.requirement,
      resolutionSource: "explicit",
    }
  }
  // An allocator supplied from outside the current stack carries no mobility
  // fact by inference. The caller must publish it on this operation, or a
  // later boundary must reject the unknown fact.
  return {
    origin: operation.explicitOrigin ?? identity,
    mobility: operation.explicitMobility ?? operation.mobility ?? "unknown",
    requirement: operation.explicitRequirement ?? operation.requirement ?? "unknown",
    resolutionSource: "explicit",
  }
}

function resolveCurrentAllocator(state, requirement = "Allocator") {
  const current = state.contextStack.at(-1)
  const expected = expectedRequirement(requirement)
  const available = availableAllocators(state)
  const constrained = requirement !== undefined
    && requirement !== "any"
    && requirement !== "Allocator"
  if (!current) {
    return {
      accepted: false,
      code: "W-ALLOCATOR-0010",
      reason: "missingCurrentAllocator",
      expected,
      available,
    }
  }
  if (constrained
    && current.requirement !== requirement
    && current.mobility !== requirement) {
    return {
      accepted: false,
      code: "W-ALLOCATOR-0010",
      reason: "incompatibleRequirement",
      expected,
      available,
    }
  }
  return {
    accepted: true,
    allocator: current.identity,
    origin: current.origin ?? current.identity,
    resolutionSource: current.resolutionSource,
    mobility: current.mobility,
  }
}

function isContextualSlot(slot) {
  return slot?.contextual === true || slot?.slot === true || slot?.kind === "contextual"
}

function contextualCall(state, operation) {
  const slot = operation.slot ?? operation.callee ?? operation
  const explicit = operation.explicitAllocator
  if (!isContextualSlot(slot)) {
    if (explicit !== undefined && explicit !== null && explicit !== "omitted") {
      const facts = explicitAllocatorFacts(state, operation, explicit)
      return {
        accepted: true,
        allocator: explicit,
        origin: facts.origin,
        mobility: facts.mobility,
        requirement: facts.requirement,
        resolutionSource: "explicit",
        inserted: false,
      }
    }
    return {
      accepted: false,
      code: "W-LABEL-0005",
      reason: "ordinaryAllocatorParameter",
      declaration: operation.declaration ?? slot.declaration ?? "callable",
      label: operation.label ?? "allocator",
      acceptedForms: operation.acceptedForms ?? ["positional"],
    }
  }
  if (slot.count !== undefined && slot.count !== 1) {
    return {
      accepted: false,
      reason: "allocatorParameterUnique",
      code: "W-ALLOCATOR-0001",
      declaration: operation.declaration ?? slot.declaration ?? "callable",
      parameter: slot.parameter ?? slot.name ?? "allocator",
    }
  }
  const standardRef = slot.ref === undefined
    ? (slot.type === undefined || slot.type === "ref Allocator")
    : slot.ref === true || slot.ref === "ref"
  if (!standardRef || (slot.identity !== undefined && slot.identity !== "standard")) {
    return {
      accepted: false,
      code: "W-ALLOCATOR-0001",
      reason: "standardRefAllocatorRequired",
      declaration: operation.declaration ?? slot.declaration ?? "callable",
      parameter: slot.parameter ?? slot.name ?? "allocator",
    }
  }
  if (explicit !== undefined && explicit !== null && explicit !== "omitted") {
    const facts = explicitAllocatorFacts(state, operation, explicit)
    return {
      accepted: true,
      allocator: explicit,
      origin: facts.origin,
      mobility: facts.mobility,
      requirement: facts.requirement,
      resolutionSource: "explicit",
      inserted: false,
    }
  }
  const result = resolveCurrentAllocator(state, slot.requirement ?? "Allocator")
  if (!result.accepted) return result
  return { ...result, inserted: true }
}

function rootStack(state) {
  return state.rootStack.map((item) => ({ ...item }))
}

function enterCallee(state, slot, callResult) {
  if (isContextualSlot(slot)) {
    state.contextStack.push({
      identity: callResult.allocator,
      origin: callResult.origin ?? callResult.allocator,
      mobility: callResult.mobility ?? "unknown",
      requirement: callResult.requirement ?? "any",
      resolutionSource: "contextualParameter",
      named: true,
      sourceBindingExpected: true,
    })
  } else {
    // A callable without the slot starts with its own root. It never receives
    // the caller's lexical block as an ambient capture.
    state.contextStack = rootStack(state)
  }
}

function parameterSourceForms(parameter) {
  if (isContextualSlot(parameter)) return [""]
  const label = parameter.label ?? parameter.external ?? parameter.requiredLabel
  if (parameter.optional === true) {
    return ["positional", label ? `${label}:` : "positional"]
  }
  if (label) return [`${label}:`]
  return ["positional"]
}

function deriveSourceShapes(declaration) {
  const parameters = Array.isArray(declaration.parameters) ? declaration.parameters : []
  let shapes = [""]
  for (const parameter of parameters) {
    shapes = shapes.flatMap((shape) => parameterSourceForms(parameter).map((form) =>
      shape ? `${shape}|${form}` : form))
  }
  return shapes.sort()
}

function runContextualChain(state, operation) {
  const savedStack = state.contextStack.map((item) => ({ ...item }))
  const savedOrigin = state.origin
  const savedMobility = state.mobility
  const transitions = []
  let final = null
  const links = operation.links ?? []
  if (links.length === 0) {
    const missing = resolveCurrentAllocator(state, operation.requirement ?? "Allocator")
    return missing.accepted ? { ...missing, chainLength: 0 } : missing
  }
  for (const [index, rawLink] of links.entries()) {
    const link = rawLink.slot || rawLink.contextual !== undefined ? rawLink : { slot: rawLink }
    const slot = link.slot ?? (link.contextual !== undefined ? link : null)
    let result
    if (slot && isContextualSlot(slot)) {
      result = contextualCall(state, {
        slot,
        explicitAllocator: link.explicitAllocator,
        declaration: link.declaration,
      })
      if (!result.accepted) {
        state.contextStack = savedStack
        state.origin = savedOrigin
        state.mobility = savedMobility
        return result
      }
    } else {
      result = {
        accepted: true,
        allocator: null,
        origin: null,
        resolutionSource: "none",
        inserted: false,
      }
    }
    transitions.push({
      index,
      declaration: link.declaration ?? `link${index + 1}`,
      inserted: result.inserted,
      allocator: result.allocator,
      resolutionSource: result.resolutionSource,
    })
    final = result
    enterCallee(state, slot, result)
    if (link.bodyCall) {
      const body = contextualCall(state, link.bodyCall)
      if (!body.accepted) {
        state.contextStack = savedStack
        state.origin = savedOrigin
        state.mobility = savedMobility
        return body
      }
      final = body
      transitions.push({
        index,
        declaration: link.declaration ?? `link${index + 1}`,
        bodyCall: true,
        inserted: body.inserted,
        allocator: body.allocator,
        resolutionSource: body.resolutionSource,
      })
    }
  }
  state.contextStack = savedStack
  state.origin = savedOrigin
  state.mobility = savedMobility
  return { ...final, chainLength: links.length, transitions }
}

export function runAllocatorScope(input) {
  if (!input || typeof input !== "object") return { accepted: false, reason: "invalidInput" }
  const initialStack = contextualStack(input)
  const state = {
    plan: input.plan ?? "fixed",
    capacity: input.capacity ?? null,
    origin: input.origin ?? "local",
    mobility: input.mobility ?? "local",
    children: input.children ?? 0,
    waits: input.waits ?? 0,
    loans: input.loans ?? 0,
    dependents: input.dependents ?? 0,
    customPlanValidated: false,
    customLeaseCreated: false,
    customLeaseOpen: false,
    customLeaseClosed: false,
    // These counters are transition outputs, never case inputs. A successful
    // open creates one lease; scope close performs one provider close and one
    // lease deinit. A second close is rejected below.
    customProviderCloseCount: 0,
    customDeinitCount: 0,
    contextStack: initialStack,
    rootStack: initialStack.length > 0 && initialStack[0].resolutionSource === "productDefault"
      ? [initialStack[0]]
      : [],
    lastResolution: null,
  }
  if (!activePlans.has(state.plan)) return { accepted: false, reason: "unknownPlan" }
  let lastResult = null
  for (const operation of input.operations ?? []) {
    if (operation.op === "planAdmission") {
      if (operation.admitted !== true) {
        return {
          accepted: false,
          reason: "planAdmissionFailed",
          bodyEntered: false,
          bindingCreated: false,
          contextPublished: false,
          sourceBindingExpected: operation.sourceBindingExpected ?? operation.name !== undefined,
        }
      }
      continue
    }
    if (operation.op === "fixedAdmission") {
      const provenInfallible = operation.reservation === "static"
        && operation.admission === "infallible"
        && operation.recursionClosed === true
      if (provenInfallible) continue
      if (operation.syntaxTry !== true) {
        return {
          accepted: false,
          reason: "fixedAdmissionRequiresTry",
          bodyEntered: false,
          bindingCreated: false,
          contextPublished: false,
          sourceBindingExpected: operation.sourceBindingExpected ?? operation.name !== undefined,
        }
      }
      if (operation.admitted !== true) {
        return {
          accepted: false,
          reason: "planAdmissionFailed",
          bodyEntered: false,
          bindingCreated: false,
          contextPublished: false,
          sourceBindingExpected: operation.sourceBindingExpected ?? operation.name !== undefined,
        }
      }
      continue
    }
    if (operation.op === "parameterSlots") {
      if (operation.count !== 1) {
        return { accepted: false, reason: "allocatorParameterUnique" }
      }
      continue
    }
    if (operation.op === "foreignAbi") {
      if (operation.slotPublished !== true) {
        return { accepted: false, reason: "allocatorAbiOmission" }
      }
      continue
    }
    if (operation.op === "contextualCall") {
      const result = contextualCall(state, operation)
      if (!result.accepted) return result
      if (result.origin !== undefined && result.origin !== null) state.origin = result.origin
      if (result.mobility !== undefined) state.mobility = result.mobility
      state.lastResolution = result
      continue
    }
    if (operation.op === "contextualChain") {
      const result = runContextualChain(state, operation)
      if (!result.accepted) return result
      state.lastResolution = result
      continue
    }
    if (operation.op === "overloadCollision") {
      const declarations = operation.declarations ?? []
      const derived = declarations.flatMap((declaration, index) =>
        deriveSourceShapes(declaration).map((shape) => ({
          id: declaration.id ?? `declaration${index + 1}`,
          shape,
        })))
      const duplicate = derived.find((entry, index) =>
        entry.shape && derived.findIndex((candidate) => candidate.shape === entry.shape) !== index)
      if (duplicate) {
        return {
          accepted: false,
          code: "W-LABEL-0004",
          reason: "contextualOmissionOverloadCollision",
          declarations: derived.filter((entry) => entry.shape === duplicate.shape).map((entry) => entry.id),
          forms: [duplicate.shape],
        }
      }
      continue
    }
    if (operation.op === "initializerSlot") {
      if (operation.declared === true) {
        return { accepted: false, code: "W-ALLOCATOR-0011", reason: "contextualAllocatorInInitializer" }
      }
      continue
    }
    if (operation.op === "functionValue") {
      const signature = operation.signature ?? {}
      const parameters = signature.parameters ?? []
      const contextual = parameters.filter(isContextualSlot)
      const preserved = contextual.length === 1
        && isContextualSlot(parameters[0])
        && (contextual[0].identity === undefined || contextual[0].identity === "standard")
      state.lastResolution = {
        accepted: preserved,
        slotPreserved: preserved,
        invocationCanFill: preserved,
        throws: signature.throws ?? null,
        typeIdentity: signature.typeIdentity ?? "function",
      }
      if (!state.lastResolution.accepted) return { accepted: false, reason: "contextualSlotNotPreserved" }
      continue
    }
      if (operation.op === "closure") {
      const parameters = operation.parameters ?? []
      const contextual = parameters.filter(isContextualSlot)
      const references = operation.bodyReferences ?? []
      const captures = operation.captureList ?? []
      const namedOuter = state.contextStack
        .filter((item) => item.named)
        .map((item) => item.name ?? item.identity)
      const referencedOuter = references.filter((reference) =>
        namedOuter.includes(reference))
      const capturedNames = captures.map((capture) => capture.name)
      const destination = operation.destination ?? "closure"
      const escaping = operation.escaping === true
        || operation.stored === true
        || ["stored", "returned", "spawn", "service", "callback", "channel"].includes(destination)
      const missingCapture = referencedOuter.find((reference) => !capturedNames.includes(reference))
      if (missingCapture !== undefined && escaping) {
        // `take` is only a capture mode candidate; W-ALLOCATOR-0002 still
        // governs whether that owner may escape its lexical scope after
        // capture.
        const validModes = operation.copyable === true ? ["copy", "take"] : ["take"]
        return {
          accepted: false,
          code: "W-OWNERSHIP-0015",
          reason: "missingExplicitCapture",
          capture: missingCapture,
          destination,
          ownerType: "Allocator",
          validModes,
        }
      }
      const hasOwnSlot = contextual.length === 1 && isContextualSlot(parameters[0])
      state.lastResolution = {
        accepted: true,
        captured: captures.length > 0,
        outerBlockCaptured: capturedNames.some((name) => namedOuter.includes(name)),
        slotPreserved: hasOwnSlot,
        invocationCanFill: hasOwnSlot,
        bodyResolutionSource: hasOwnSlot ? "contextualParameter" : (state.rootStack.length ? "productDefault" : "none"),
        bodyReferenceResolution: missingCapture !== undefined ? "inferredNonescapingRef" : (capturedNames.length ? "explicitCapture" : "none"),
        escaping,
      }
      continue
    }
    if (operation.op === "pushContext") {
      const identity = operation.identity ?? operation.name
      if (!identity) return { accepted: false, reason: "contextIdentityMissing" }
      state.contextStack.push(contextItem(identity, operation.resolutionSource ?? "lexicalBlock", operation))
      continue
    }
    if (operation.op === "popContext") {
      if (state.contextStack.length <= state.rootStack.length) {
        return { accepted: false, reason: "contextStackUnderflow" }
      }
      state.contextStack.pop()
      continue
    }
    if (operation.op === "construct") {
      const explicit = operation.explicitAllocator
      if (explicit !== undefined && explicit !== null && explicit !== "none" && explicit !== "omitted") {
        const facts = explicitAllocatorFacts(state, operation, explicit)
        state.origin = facts.origin
        state.mobility = facts.mobility
        state.lastResolution = {
          resolutionSource: "explicit",
          allocator: explicit,
          origin: facts.origin,
          mobility: facts.mobility,
        }
      } else {
        const result = resolveCurrentAllocator(state, operation.requirement ?? "Allocator")
        if (!result.accepted) return result
        state.origin = result.origin
        state.mobility = result.mobility
        state.lastResolution = {
          resolutionSource: result.resolutionSource,
          allocator: result.allocator,
          origin: result.origin,
        }
      }
      continue
    }
    if (operation.op === "call") {
      if (operation.transitiveAllocator === true) return { accepted: false, reason: "ambientPropagation" }
      continue
    }
    if (operation.op === "rehome") {
      if (operation.source !== undefined && operation.source !== state.origin) {
        return { accepted: false, reason: "invalidRehomeSource" }
      }
      state.origin = operation.destination ?? "process"
      state.mobility = operation.mobility ?? "crossDomain"
      continue
    }
    if (operation.op === "escape") {
      return { accepted: false, reason: "scopeEscape" }
    }
    if (operation.op === "boundary") {
      if (state.mobility !== "crossDomain") {
        if (operation.kind === "spawn") {
          return {
            accepted: false,
            code: "W-ALLOCATOR-0003",
            reason: state.mobility === "unknown" ? "unknownMobilityBoundary" : "localOriginBoundary",
            allocator: state.origin,
            boundary: operation.kind,
            origin: state.origin,
          }
        }
        return { accepted: false, reason: "localOriginBoundary" }
      }
      continue
    }
    if (operation.op === "shadow") {
      // Allocator names use the general lexical nominal binding rule. This
      // oracle does not close the language-wide same-name shadow decision.
      if (operation.name !== undefined) {
        state.contextStack.push(contextItem(operation.name, "lexicalBlock", operation))
      }
      continue
    }
    if (operation.op === "nested") {
      if (operation.name !== undefined) {
        state.contextStack.push(contextItem(operation.name, "lexicalBlock", operation))
      }
      continue
    }
    if (operation.op === "await") {
      if (operation.stable !== true || (operation.ownerInTaskFrame !== undefined && operation.ownerInTaskFrame !== true)) {
        return { accepted: false, reason: "unstableOwnerAcrossAwait" }
      }
      continue
    }
    if (operation.op === "capacity") {
      if (state.plan !== "fixed") return { accepted: false, reason: "capacityOnlyFixed" }
      if (!Number.isSafeInteger(operation.capacity) || operation.capacity <= 0) {
        return { accepted: false, reason: "capacityOverflow" }
      }
      if (operation.supported !== true) return { accepted: false, reason: "fixedUnsupportedTarget" }
      continue
    }
    if (operation.op === "customContract") {
      // Descriptor validation is data-only. Opening the executable plan is a
      // separate compiler-invoked transition below.
      const digest = operation.providerDigest
      const validDigest = Array.isArray(digest)
        && digest.length === 32
        && digest.every((byte) => Number.isInteger(byte) && byte >= 0 && byte <= 255)
        && digest.some((byte) => byte !== 0)
      if (!validDigest) return { accepted: false, reason: "customProviderDigest" }
      if (!Number.isSafeInteger(operation.version) || operation.version <= 0) {
        return { accepted: false, reason: "customProviderVersion" }
      }
      if (!["infallible", "fallible"].includes(operation.failure)) {
        return { accepted: false, reason: "customProviderFailureMode" }
      }
      if (!["provider", "backing"].includes(operation.deallocator)) {
        return { accepted: false, reason: "customProviderDeallocator" }
      }
      if (!["local", "crossDomain"].includes(operation.mobility)) {
        return { accepted: false, reason: "customProviderMobility" }
      }
      if (operation.backingOutlivesLease !== true) {
        return { accepted: false, reason: "leaseBackingLifetime" }
      }
      state.mobility = operation.mobility
      state.customPlanValidated = true
      continue
    }
    if (operation.op === "open") {
      // A failed open admits neither body nor binding and creates no lease.
      if (state.plan !== "custom" || state.customPlanValidated !== true) {
        return { accepted: false, reason: "customDescriptorRequired" }
      }
      if (state.customLeaseCreated) {
        return { accepted: false, reason: "customPlanOpenedTwice" }
      }
      if (!["success", "failure"].includes(operation.outcome)) {
        return { accepted: false, reason: "customPlanOpenOutcome" }
      }
      if (operation.outcome !== "success") {
        return {
          accepted: false,
          reason: "planAdmissionFailed",
          bodyEntered: false,
          bindingCreated: false,
          leaseCreated: false,
          contextPublished: false,
          sourceBindingExpected: operation.sourceBindingExpected ?? operation.name !== undefined,
          providerCloseCount: 0,
          deinitCount: 0,
        }
      }
      state.customLeaseCreated = true
      state.customLeaseOpen = true
      state.customLeaseClosed = false
      state.customProviderCloseCount = 0
      state.customDeinitCount = 0
      if (operation.context) {
        state.contextStack.push(contextItem(
          operation.context.identity ?? operation.name ?? "anonymous-lease",
          "lexicalBlock",
          {
            ...operation.context,
            name: operation.name,
            sourceBindingExpected: operation.sourceBindingExpected ?? operation.name !== undefined,
          },
        ))
      }
      continue
    }
    if (operation.op === "close") {
      if (state.customLeaseOpen && state.customLeaseClosed) {
        return { accepted: false, reason: "customLeaseClosedTwice" }
      }
      const active = []
      if (state.children > 0) active.push("child")
      if (state.waits > 0) active.push("wait")
      if (state.loans > 0) active.push("loan")
      if (state.dependents > 0) active.push("dependent")
      if (active.length > 0) {
        return {
          accepted: false,
          reason: "undrainedClose",
          active,
          typedDropsBeforeReclaim: false,
          reclaimed: false,
        }
      }
      state.customLeaseClosed = state.customLeaseOpen
      if (state.customLeaseOpen) {
        state.customProviderCloseCount += 1
        state.customDeinitCount += 1
      }
      lastResult = {
        accepted: true,
        closed: true,
        typedDrops: operation.typedDrops ?? 0,
        typedDropsBeforeReclaim: true,
        reclaimed: true,
        ...(state.customLeaseOpen
          ? {
              leaseCreated: state.customLeaseCreated,
              leaseClosed: true,
              providerCloseCount: state.customProviderCloseCount,
              deinitCount: state.customDeinitCount,
              mobility: state.mobility,
              plan: state.plan,
            }
          : {}),
      }
      continue
    }
    return { accepted: false, reason: "unknownOperation" }
  }
  if (lastResult) return lastResult
  if (state.customLeaseCreated) {
    return {
      accepted: true,
      closed: false,
      origin: state.origin,
      mobility: state.mobility,
      plan: state.plan,
      leaseCreated: true,
      leaseClosed: state.customLeaseClosed,
      providerCloseCount: state.customProviderCloseCount,
      deinitCount: state.customDeinitCount,
      ...(state.lastResolution ?? {}),
    }
  }
  return {
    accepted: true,
    closed: false,
    origin: state.origin,
    mobility: state.mobility,
    plan: state.plan,
    ...(state.lastResolution ?? {}),
  }
}
