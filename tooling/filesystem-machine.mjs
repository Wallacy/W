const fileRights = new Set(["read", "write", "append"])
const creationModes = new Set(["openExisting", "create", "createNew", "replace"])
const durabilityModes = new Set(["none", "data", "all"])
const renamePolicies = new Set(["keepExisting", "replaceFile"])
const fileKinds = new Set(["regular", "directory", "symbolicLink", "other"])
const maximumU64 = (1n << 64n) - 1n
const filesystemRights = new Set([
  "read",
  "write",
  "append",
  "metadata",
  "list",
  "mutate",
  "durability",
  "scope",
])
// Fixture bounds. A real FileSystem receives finite bounds from its provider
// profile; these numbers are not language defaults.
const oracleResolutionLimits = Object.freeze({
  maximumPathUnits: 4_096,
  maximumComponents: 256,
  maximumSymlinkTraversals: 40,
})

function isNatural(value) {
  return Number.isSafeInteger(value) && value >= 0
}

function parseU64(value) {
  if (isNatural(value)) return BigInt(value)
  if (typeof value !== "string" || !/^(?:0|[1-9][0-9]*)$/.test(value)) return null
  const parsed = BigInt(value)
  return parsed <= maximumU64 ? parsed : null
}

function validByteArray(value) {
  return Array.isArray(value) && value.every((item) => isNatural(item) && item <= 0xff)
}

function validNative(value) {
  if (!value || !["unixBytes", "windowsUnits"].includes(value.kind)) return false
  const field = value.kind === "unixBytes" ? "bytes" : "units"
  const maximum = value.kind === "unixBytes" ? 0xff : 0xffff
  return Array.isArray(value[field])
    && value[field].every((item) => isNatural(item) && item <= maximum)
}

function containsNativeNul(value) {
  const field = value.kind === "unixBytes" ? "bytes" : "units"
  return value[field].includes(0)
}

function validateRights(rights) {
  if (!Array.isArray(rights) || rights.length === 0) return "emptyRights"
  const seen = new Set()
  for (const right of rights) {
    if (!fileRights.has(right)) return "unknownRight"
    if (seen.has(right)) return "duplicateRight"
    seen.add(right)
  }
  return null
}

function validateFilesystemRights(rights) {
  if (!Array.isArray(rights) || rights.length === 0) return "emptyFilesystemRights"
  const seen = new Set()
  for (const right of rights) {
    if (!filesystemRights.has(right)) return "unknownFilesystemRight"
    if (seen.has(right)) return "duplicateFilesystemRight"
    seen.add(right)
  }
  return null
}

function normalizeResolutionLimits(candidate) {
  const limits = candidate ?? oracleResolutionLimits
  if (!isNatural(limits.maximumPathUnits) || limits.maximumPathUnits < 1
    || !isNatural(limits.maximumComponents) || limits.maximumComponents < 1
    || !isNatural(limits.maximumSymlinkTraversals)) return null
  return limits
}

function applyResolutionStep(state, step, limits, providerTarget = false) {
  if (!step || typeof step !== "object") return "invalidResolutionStep"
  state.componentsVisited += 1
  if (state.componentsVisited > limits.maximumComponents) {
    return "componentLimitExceeded"
  }
  if (step.kind === "name") {
    if (!validNative(step.value)) return "invalidNativeName"
    if (containsNativeNul(step.value)) return "nulUnsupported"
    state.depth += 1
    state.names += 1
    state.pathUnits += countNativeUnits(step.value)
    if (state.pathUnits > limits.maximumPathUnits) return "pathUnitLimitExceeded"
    return null
  }
  if (step.kind === "parent") {
    if (!providerTarget || state.depth === 0) return "outsideRoot"
    state.depth -= 1
    return null
  }
  if (step.kind === "symlink") {
    if (step.absolute === true) return "outsideRoot"
    if (!Array.isArray(step.target)) return "invalidSymlinkTarget"
    state.symlinks += 1
    if (state.symlinks > limits.maximumSymlinkTraversals) {
      return "symlinkLimitExceeded"
    }
    for (const targetStep of step.target) {
      const reason = applyResolutionStep(state, targetStep, limits, true)
      if (reason) return reason
    }
    return null
  }
  return "invalidResolutionStep"
}

function resolvePath(path, rootId, candidateLimits) {
  if (!path || path.factSource !== "provider") {
    return { accepted: false, reason: "providerContainmentRequired", providerCalled: false }
  }
  if (typeof rootId !== "string" || rootId.length === 0) {
    return { accepted: false, reason: "invalidRoot", providerCalled: false }
  }
  const limits = normalizeResolutionLimits(candidateLimits)
  if (!limits) {
    return { accepted: false, reason: "invalidResolutionLimits", providerCalled: false }
  }
  if (path.absolute === true) {
    return { accepted: false, reason: "outsideRoot", providerCalled: true }
  }
  if (!Array.isArray(path.steps)) {
    return { accepted: false, reason: "invalidPath", providerCalled: true }
  }
  const state = { depth: 0, names: 0, symlinks: 0, componentsVisited: 0, pathUnits: 0 }
  for (const step of path.steps) {
    const reason = applyResolutionStep(state, step, limits)
    if (reason) return { accepted: false, reason, providerCalled: true }
  }
  if (path.containment !== "inside") {
    return { accepted: false, reason: "containmentUnproved", providerCalled: true }
  }
  return {
    accepted: true,
    rootId,
    depth: state.depth,
    names: state.names,
    symlinks: state.symlinks,
    componentsVisited: state.componentsVisited,
    pathUnits: state.pathUnits,
    providerContainment: true,
    lexicalProofOnly: false,
  }
}

function decodeWindows(units) {
  for (let index = 0; index < units.length; index += 1) {
    const unit = units[index]
    if (unit >= 0xd800 && unit <= 0xdbff) {
      const next = units[index + 1]
      if (!(next >= 0xdc00 && next <= 0xdfff)) {
        return { accepted: false, reason: "unpairedWindowsUnit", unitOffset: index }
      }
      index += 1
    } else if (unit >= 0xdc00 && unit <= 0xdfff) {
      return { accepted: false, reason: "unpairedWindowsUnit", unitOffset: index }
    }
  }
  let text = ""
  for (let offset = 0; offset < units.length; offset += 4_096) {
    text += String.fromCharCode(...units.slice(offset, offset + 4_096))
  }
  return { accepted: true, text }
}

function decodeUnix(bytes) {
  for (let offset = 0; offset < bytes.length;) {
    const lead = bytes[offset]
    if (lead <= 0x7f) {
      offset += 1
      continue
    }
    let width = 0
    let secondMinimum = 0x80
    let secondMaximum = 0xbf
    if (lead >= 0xc2 && lead <= 0xdf) width = 2
    else if (lead >= 0xe0 && lead <= 0xef) {
      width = 3
      if (lead === 0xe0) secondMinimum = 0xa0
      if (lead === 0xed) secondMaximum = 0x9f
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      width = 4
      if (lead === 0xf0) secondMinimum = 0x90
      if (lead === 0xf4) secondMaximum = 0x8f
    } else return { accepted: false, reason: "invalidUnixUtf8", byteOffset: offset }
    if (offset + width > bytes.length) {
      return { accepted: false, reason: "invalidUnixUtf8", byteOffset: offset }
    }
    const second = bytes[offset + 1]
    if (second < secondMinimum || second > secondMaximum) {
      return { accepted: false, reason: "invalidUnixUtf8", byteOffset: offset }
    }
    for (let index = offset + 2; index < offset + width; index += 1) {
      if (bytes[index] < 0x80 || bytes[index] > 0xbf) {
        return { accepted: false, reason: "invalidUnixUtf8", byteOffset: offset }
      }
    }
    offset += width
  }
  return {
    accepted: true,
    text: new TextDecoder("utf-8", { fatal: true, ignoreBOM: true }).decode(
      Uint8Array.from(bytes),
    ),
  }
}

function validWText(text) {
  if (typeof text !== "string") return false
  for (let index = 0; index < text.length; index += 1) {
    const unit = text.charCodeAt(index)
    if (unit >= 0xd800 && unit <= 0xdbff) {
      const next = text.charCodeAt(index + 1)
      if (!(next >= 0xdc00 && next <= 0xdfff)) return false
      index += 1
    } else if (unit >= 0xdc00 && unit <= 0xdfff) return false
  }
  return true
}

function derivePath(input) {
  if (input.operation === "resolve") {
    return resolvePath(input.path, input.rootId, input.rootLimits)
  }
  if (input.operation === "toUtf8") {
    if (!validNative(input.native)) return { accepted: false, reason: "invalidNativePath" }
    if (containsNativeNul(input.native)) {
      const values = input.native.kind === "unixBytes" ? input.native.bytes : input.native.units
      return { accepted: false, reason: "nulUnsupported", unitOffset: values.indexOf(0) }
    }
    if (input.native.kind === "unixBytes") {
      return { ...decodeUnix(input.native.bytes), lossy: false }
    }
    return { ...decodeWindows(input.native.units), lossy: false }
  }
  if (input.operation === "fromUtf8") {
    if (!validWText(input.text)) return { accepted: false, reason: "invalidWText" }
    const nul = input.text.indexOf("\0")
    if (nul >= 0) return { accepted: false, reason: "nulUnsupported", unitOffset: nul }
    if (input.hostKind === "unixBytes") {
      return {
        accepted: true,
        native: { kind: "unixBytes", bytes: [...new TextEncoder().encode(input.text)] },
        normalized: false,
      }
    }
    if (input.hostKind === "windowsUnits") {
      const units = []
      for (let index = 0; index < input.text.length; index += 1) {
        units.push(input.text.charCodeAt(index))
      }
      return { accepted: true, native: { kind: "windowsUnits", units }, normalized: false }
    }
    return { accepted: false, reason: "unknownHostKind" }
  }
  if (input.operation === "displayLossy") {
    if (!validNative(input.native)) return { accepted: false, reason: "invalidNativePath" }
    return {
      accepted: true,
      presentationOnly: true,
      identityUsed: false,
      lookupUsed: false,
    }
  }
  return { accepted: false, reason: "unknownPathOperation" }
}

function deriveOpen(input) {
  const rightReason = validateRights(input.rights)
  if (rightReason) {
    return { accepted: false, reason: rightReason, phase: "static", providerCalled: false }
  }
  if (!creationModes.has(input.creation)) {
    return { accepted: false, reason: "unknownCreation", phase: "static", providerCalled: false }
  }
  if (input.creation !== "openExisting"
    && !input.rights.some((right) => right === "write" || right === "append")) {
    return {
      accepted: false,
      reason: "creationRequiresWrite",
      phase: "static",
      providerCalled: false,
    }
  }
  const path = resolvePath(input.path, input.rootId, input.rootLimits)
  if (!path.accepted) return { ...path, phase: "resolution" }
  if (input.providerDisposition !== "opened") {
    const providerReasons = {
      denied: "permissionDenied",
      exhausted: "resourceExhausted",
      alreadyExists: "alreadyExists",
      missing: "notFound",
    }
    return {
      accepted: false,
      reason: providerReasons[input.providerDisposition] ?? "invalidProviderDisposition",
      phase: "provider",
      providerCalled: true,
      namespaceChanged: false,
    }
  }
  if ((input.fileKind ?? "regular") !== "regular") {
    return { accepted: false, reason: "notRegularFile", providerCalled: true }
  }
  return {
    accepted: true,
    rootId: input.rootId,
    rights: [...input.rights],
    creation: input.creation,
    providerCalled: true,
    cursor: false,
    moveFirst: true,
    exclusiveCreate: input.creation === "createNew",
    truncatesExisting: input.creation === "replace",
    atomicPublication: false,
  }
}

function deriveScope(input) {
  if (input.childRootFactSource !== "provider"
    || typeof input.childRootId !== "string" || input.childRootId.length === 0) {
    return { accepted: false, reason: "providerChildRootRequired", providerCalled: false }
  }
  const parentRightsReason = validateFilesystemRights(input.parentRights)
  if (parentRightsReason) return { accepted: false, reason: parentRightsReason, providerCalled: false }
  const childRightsReason = validateFilesystemRights(input.childRights)
  if (childRightsReason) return { accepted: false, reason: childRightsReason, providerCalled: false }
  const parentRights = new Set(input.parentRights)
  if (input.childRights.some((right) => !parentRights.has(right))) {
    return { accepted: false, reason: "authorityAmplification", providerCalled: false }
  }
  const path = resolvePath(input.path, input.rootId, input.rootLimits)
  if (!path.accepted) return { ...path, phase: "resolution" }
  if (path.depth === 0) {
    return { accepted: false, reason: "scopeRequiresDescendant", providerCalled: false }
  }
  if (input.providerDisposition !== "opened") {
    return {
      accepted: false,
      reason: input.providerDisposition === "notDirectory" ? "notDirectory" : "io",
      providerCalled: true,
      authorityExpanded: false,
    }
  }
  return {
    accepted: true,
    parentRootId: input.rootId,
    rootId: input.childRootId,
    rights: [...input.childRights],
    providerCalled: true,
    authorityNarrowed: true,
    authorityExpanded: false,
  }
}

function ensureFile(input, requiredRight) {
  const reason = validateRights(input.rights)
  if (reason) return { accepted: false, reason }
  if (!input.rights.includes(requiredRight)) {
    return { accepted: false, reason: "rightMissing", requiredRight }
  }
  return null
}

function deriveFile(input) {
  if (input.operation === "read") {
    const denied = ensureFile(input, "read")
    if (denied) return denied
    if (!validByteArray(input.bytes) || !isNatural(input.offset)
      || !isNatural(input.maximum) || input.maximum < 1) {
      return { accepted: false, reason: "invalidRead" }
    }
    const available = Math.max(0, input.bytes.length - input.offset)
    if (available === 0) {
      return {
        accepted: true,
        step: "end",
        data: [],
        positionChanged: false,
        endLatched: false,
      }
    }
    const providerMaximum = input.providerMaximum ?? input.maximum
    const count = Math.min(available, input.maximum, providerMaximum)
    if (!isNatural(count) || count < 1) return { accepted: false, reason: "invalidProgress" }
    return {
      accepted: true,
      step: "data",
      count,
      data: input.bytes.slice(input.offset, input.offset + count),
      positionChanged: false,
      endLatched: false,
      sharedAllowed: true,
    }
  }
  if (input.operation === "write") {
    const denied = ensureFile(input, "write")
    if (denied) return denied
    if (!validByteArray(input.bytes) || !validByteArray(input.source)
      || !isNatural(input.offset)) return { accepted: false, reason: "invalidWrite" }
    const committed = Math.min(input.source.length, input.providerMaximum ?? input.source.length)
    if (input.source.length > 0 && committed < 1) {
      return { accepted: false, reason: "invalidProgress" }
    }
    const output = [...input.bytes]
    while (output.length < input.offset) output.push(0)
    for (let index = 0; index < committed; index += 1) {
      output[input.offset + index] = input.source[index]
    }
    return {
      accepted: true,
      step: committed === input.source.length ? "complete" : "partial",
      committed,
      output,
      positionChanged: false,
      sharedAllowed: true,
    }
  }
  if (input.operation === "append") {
    const denied = ensureFile(input, "append")
    if (denied) return denied
    if (!validByteArray(input.bytes) || !validByteArray(input.source)
      || !validByteArray(input.concurrentPrefix ?? [])) {
      return { accepted: false, reason: "invalidAppend" }
    }
    const current = [...input.bytes, ...(input.concurrentPrefix ?? [])]
    const selectedOffset = current.length
    const committed = Math.min(input.source.length, input.providerMaximum ?? input.source.length)
    current.push(...input.source.slice(0, committed))
    return {
      accepted: true,
      step: committed === input.source.length ? "complete" : "partial",
      committed,
      selectedOffset,
      ignoredCallerLength: input.callerObservedLength ?? null,
      output: current,
      providerSelectedOffset: true,
    }
  }
  if (input.operation === "reader") {
    const denied = ensureFile(input, "read")
    if (denied) return denied
    if (input.shared === true) return { accepted: false, reason: "cursorNotShareable" }
    if (!isNatural(input.startOffset) || !Array.isArray(input.steps)) {
      return { accepted: false, reason: "invalidCursor" }
    }
    let offset = input.startOffset
    const offsets = [offset]
    for (const step of input.steps) {
      if (step.kind === "data") {
        if (!isNatural(step.count) || step.count < 1) {
          return { accepted: false, reason: "invalidProgress", offsets }
        }
        offset += step.count
        if (!Number.isSafeInteger(offset)) {
          return { accepted: false, reason: "offsetOverflow", offsets }
        }
        offsets.push(offset)
      } else if (step.kind === "end") {
        offsets.push(offset)
      } else if (step.kind === "error") {
        return { accepted: false, reason: "io", finalOffset: offset, offsets }
      } else return { accepted: false, reason: "unknownCursorStep", offsets }
    }
    return { accepted: true, finalOffset: offset, offsets, ownsCursor: true }
  }
  if (input.operation === "snapshot") {
    const denied = ensureFile(input, "read")
    if (denied) return denied
    if (!validByteArray(input.bytes) || !isNatural(input.maximumBytes)) {
      return { accepted: false, reason: "invalidSnapshot" }
    }
    if (input.bytes.length > input.maximumBytes) {
      return {
        accepted: false,
        reason: "limitExceeded",
        maximumBytes: input.maximumBytes,
        allocatedBeforeValidation: false,
      }
    }
    if (input.versionBefore !== input.versionAfter) {
      return { accepted: false, reason: "changedDuringRead", published: false }
    }
    return {
      accepted: true,
      byteCount: input.bytes.length,
      bytes: [...input.bytes],
      stable: true,
      explicitCopy: true,
      version: input.versionAfter,
    }
  }
  if (input.operation === "sync") {
    const rightReason = validateRights(input.rights)
    if (rightReason) return { accepted: false, reason: rightReason }
    if (!durabilityModes.has(input.durability)) {
      return { accepted: false, reason: "unknownDurability" }
    }
    if (!input.rights.some((right) => right === "write" || right === "append")) {
      return { accepted: false, reason: "rightMissing", requiredRight: "writeOrAppend" }
    }
    if (input.durability === "none") {
      return {
        accepted: true,
        durability: "none",
        providerCalled: false,
        durabilityRequested: false,
        providerConfirmed: false,
        externalCachePromised: false,
      }
    }
    if (input.providerSupports !== true) return { accepted: false, reason: "unsupported" }
    return {
      accepted: true,
      durability: input.durability,
      providerCalled: true,
      durabilityRequested: true,
      providerConfirmed: true,
      externalCachePromised: false,
    }
  }
  if (input.operation === "finish") {
    const rightReason = validateRights(input.rights)
    if (rightReason) return { accepted: false, reason: rightReason }
    if (input.shared === true) return { accepted: false, reason: "uniqueOwnershipRequired" }
    if (input.consumed === true) return { accepted: false, reason: "alreadyConsumed" }
    if (!durabilityModes.has(input.durability)) {
      return { accepted: false, reason: "unknownDurability" }
    }
    if (input.durability !== "none"
      && !input.rights.some((right) => right === "write" || right === "append")) {
      return { accepted: false, reason: "rightMissing", requiredRight: "writeOrAppend" }
    }
    if (input.durability !== "none" && input.providerSupports !== true) {
      return { accepted: false, reason: "unsupported" }
    }
    return {
      accepted: true,
      consumed: true,
      durability: input.durability,
      providerCalled: true,
      durabilityRequested: input.durability !== "none",
      providerConfirmed: input.durability !== "none",
      deinitErrorThrown: false,
    }
  }
  if (input.operation === "drop") {
    return {
      accepted: true,
      physicalCloseBestEffort: true,
      durabilityInserted: false,
      errorThrown: false,
    }
  }
  return { accepted: false, reason: "unknownFileOperation" }
}

function countNativeUnits(value) {
  return value.kind === "unixBytes" ? value.bytes.length : value.units.length
}

function deriveDirectory(input) {
  if (input.operation !== "entries") return { accepted: false, reason: "unknownDirectoryOperation" }
  const path = resolvePath(input.path, input.rootId, input.rootLimits)
  if (!path.accepted) return path
  if (input.recursive === true) return { accepted: false, reason: "recursionNotImplicit" }
  if (input.sorted === true) return { accepted: false, reason: "sortingNotImplicit" }
  const limits = input.limits ?? {}
  if (!isNatural(limits.maximumEntries) || limits.maximumEntries < 1
    || !isNatural(limits.maximumNameUnits) || limits.maximumNameUnits < 1
    || !isNatural(limits.maximumTotalNameUnits) || limits.maximumTotalNameUnits < 1) {
    return { accepted: false, reason: "invalidLimits" }
  }
  if (!Array.isArray(input.entries)) return { accepted: false, reason: "invalidEntries" }
  let total = 0
  for (const [index, entry] of input.entries.entries()) {
    if (index >= limits.maximumEntries) {
      return { accepted: false, reason: "entryLimitExceeded", publishedEntries: index }
    }
    if (entry.providerError === true) {
      return { accepted: false, reason: "io", publishedEntries: index }
    }
    if (!validNative(entry.name) || containsNativeNul(entry.name)
      || (entry.kindHint !== null && !fileKinds.has(entry.kindHint))) {
      return { accepted: false, reason: "invalidEntry", publishedEntries: index }
    }
    const units = countNativeUnits(entry.name)
    if (units > limits.maximumNameUnits) {
      return { accepted: false, reason: "nameLimitExceeded", publishedEntries: index }
    }
    total += units
    if (total > limits.maximumTotalNameUnits) {
      return { accepted: false, reason: "totalNameLimitExceeded", publishedEntries: index }
    }
  }
  return {
    accepted: true,
    entries: input.entries,
    order: "provider",
    recursive: false,
    sorted: false,
    totalNameUnits: total,
    singlePass: true,
  }
}

function deriveMetadata(input) {
  if (input.operation !== "read") return { accepted: false, reason: "unknownMetadataOperation" }
  const path = resolvePath(input.path, input.rootId, input.rootLimits)
  if (!path.accepted) return path
  if (!fileKinds.has(input.kind)) return { accepted: false, reason: "unknownFileKind" }
  if (input.byteLength !== null && !isNatural(input.byteLength)) {
    return { accepted: false, reason: "invalidByteLength" }
  }
  if (input.kind !== "regular" && input.byteLength !== null) {
    return { accepted: false, reason: "byteLengthNotApplicable" }
  }
  if (input.requestedTimestamps === true || input.requestedOwner === true) {
    return { accepted: false, reason: "metadataOutsidePortableContract" }
  }
  return {
    accepted: true,
    kind: input.kind,
    byteLength: input.kind === "regular" ? input.byteLength : null,
    timestampsExposed: false,
    ownerExposed: false,
    identityExposed: false,
  }
}

function deriveNamespace(input) {
  if (input.operation === "syncNamespace") {
    const path = resolvePath(input.path, input.rootId, input.rootLimits)
    if (!path.accepted) return path
    if ((input.pathKind ?? "directory") !== "directory") {
      return { accepted: false, reason: "notDirectory" }
    }
    if (input.providerSupports !== true) return { accepted: false, reason: "unsupported" }
    return {
      accepted: true,
      namespaceDurable: true,
      providerConfirmed: true,
      externalCachePromised: false,
    }
  }
  if (!["createDirectory", "removeFile", "removeEmptyDirectory", "rename"].includes(input.operation)) {
    return { accepted: false, reason: "unknownNamespaceOperation" }
  }
  const source = resolvePath(input.sourcePath, input.rootId, input.rootLimits)
  if (!source.accepted) return source
  if (input.operation === "rename") {
    if (input.destinationRootId !== input.rootId) {
      return { accepted: false, reason: "crossRoot", namespaceChanged: false }
    }
    const sourceMountId = input.sourceMountId ?? input.rootId
    const destinationMountId = input.destinationMountId ?? input.destinationRootId
    if (sourceMountId !== destinationMountId) {
      return { accepted: false, reason: "crossMount", namespaceChanged: false }
    }
    const destination = resolvePath(
      input.destinationPath,
      input.destinationRootId,
      input.rootLimits,
    )
    if (!destination.accepted) return destination
    if (!renamePolicies.has(input.replacement)) {
      return { accepted: false, reason: "unknownRenamePolicy", namespaceChanged: false }
    }
    if (input.destinationExists === true && input.replacement === "keepExisting") {
      return { accepted: false, reason: "alreadyExists", namespaceChanged: false }
    }
    if (input.destinationExists === true && input.replacement === "replaceFile"
      && input.destinationKind !== "regular") {
      return { accepted: false, reason: "replacementNotRegular", namespaceChanged: false }
    }
  }
  if (input.providerOutcome === "unknown") {
    return {
      accepted: false,
      reason: "unknownOutcome",
      retrySafe: false,
      namespaceChanged: null,
    }
  }
  if (input.providerOutcome === "rejected") {
    return { accepted: false, reason: "io", namespaceChanged: false }
  }
  if (input.providerOutcome !== "committed") {
    return { accepted: false, reason: "invalidProviderOutcome", namespaceChanged: false }
  }
  return {
    accepted: true,
    namespaceChanged: true,
    namespaceAtomic: true,
    durable: false,
    recursive: false,
    sameRoot: true,
  }
}

function deriveCancellation(input) {
  if (input.operation !== "drain") return { accepted: false, reason: "unknownCancellationOperation" }
  if (!Array.isArray(input.events)) return { accepted: false, reason: "invalidEvents" }
  const cancel = input.events.indexOf("cancel")
  const completion = input.events.indexOf("providerCompletion")
  const release = input.events.indexOf("releaseBorrow")
  if (cancel < 0) return { accepted: false, reason: "cancellationNotRequested" }
  if (completion < 0) return { accepted: false, reason: "providerNotDrained" }
  if (release < 0 || release < completion) {
    return { accepted: false, reason: "releaseBeforeDrain" }
  }
  return {
    accepted: true,
    providerDrained: true,
    borrowReleasedAfterDrain: true,
    completionWonRace: completion < cancel,
  }
}

function rangesOverlap(left, right) {
  return left.offset < right.offset + right.length
    && right.offset < left.offset + left.length
}

function deriveInterference(input) {
  if (!Array.isArray(input.operations) || input.operations.length < 2) {
    return { accepted: false, reason: "insufficientOperations" }
  }
  const positional = []
  const append = []
  const operationIds = new Set()
  for (const operation of input.operations) {
    if (typeof operation.id !== "string" || operation.id.length === 0) {
      return { accepted: false, reason: "invalidOperation" }
    }
    if (operationIds.has(operation.id)) return { accepted: false, reason: "duplicateOperation" }
    operationIds.add(operation.id)
    if (operation.kind === "append") {
      const length = parseU64(operation.length)
      if (length === null) return { accepted: false, reason: "invalidRange" }
      append.push({ ...operation, length })
      continue
    }
    const offset = parseU64(operation.offset)
    const length = parseU64(operation.length)
    if (!["read", "write"].includes(operation.kind) || offset === null || length === null
      || offset + length > maximumU64) {
      return { accepted: false, reason: "invalidRange" }
    }
    positional.push({ ...operation, offset, length })
  }
  const edges = new Map([...operationIds].map((id) => [id, []]))
  for (const edge of input.happensBefore ?? []) {
    if (!Array.isArray(edge) || edge.length !== 2
      || !operationIds.has(edge[0]) || !operationIds.has(edge[1]) || edge[0] === edge[1]) {
      return { accepted: false, reason: "invalidHappensBefore" }
    }
    edges.get(edge[0]).push(edge[1])
  }
  function ordered(left, right) {
    const pending = [...edges.get(left)]
    const seen = new Set()
    while (pending.length > 0) {
      const next = pending.pop()
      if (next === right) return true
      if (seen.has(next)) continue
      seen.add(next)
      pending.push(...edges.get(next))
    }
    return false
  }
  if ([...operationIds].some((id) => ordered(id, id))) {
    return { accepted: false, reason: "cyclicHappensBefore" }
  }
  const conflicts = []
  for (let leftIndex = 0; leftIndex < positional.length; leftIndex += 1) {
    for (let rightIndex = leftIndex + 1; rightIndex < positional.length; rightIndex += 1) {
      const left = positional[leftIndex]
      const right = positional[rightIndex]
      if (!rangesOverlap(left, right) || (left.kind === "read" && right.kind === "read")) continue
      const isOrdered = ordered(left.id, right.id) || ordered(right.id, left.id)
      if (!isOrdered) conflicts.push([left.id, right.id])
    }
  }
  for (const positionalOperation of positional) {
    for (const appendOperation of append) {
      const isOrdered = ordered(positionalOperation.id, appendOperation.id)
        || ordered(appendOperation.id, positionalOperation.id)
      if (!isOrdered) conflicts.push([positionalOperation.id, appendOperation.id])
    }
  }
  let appendOrderNondeterministic = false
  for (let leftIndex = 0; leftIndex < append.length; leftIndex += 1) {
    for (let rightIndex = leftIndex + 1; rightIndex < append.length; rightIndex += 1) {
      if (!ordered(append[leftIndex].id, append[rightIndex].id)
        && !ordered(append[rightIndex].id, append[leftIndex].id)) {
        appendOrderNondeterministic = true
      }
    }
  }
  return {
    accepted: true,
    languageDataRace: false,
    providerConflicts: conflicts,
    disjointOrOrdered: conflicts.length === 0,
    deterministic: conflicts.length === 0 && !appendOrderNondeterministic,
    appendOffsetSelectionAtomic: append.length > 0,
    appendPayloadOrderDeterministic: !appendOrderNondeterministic,
    lockInserted: false,
    warningEligible: conflicts.length > 0 || appendOrderNondeterministic,
  }
}

export function deriveFilesystem(input) {
  if (input.subject === "path") return derivePath(input)
  if (input.subject === "scope") return deriveScope(input)
  if (input.subject === "open") return deriveOpen(input)
  if (input.subject === "file") return deriveFile(input)
  if (input.subject === "directory") return deriveDirectory(input)
  if (input.subject === "metadata") return deriveMetadata(input)
  if (input.subject === "namespace") return deriveNamespace(input)
  if (input.subject === "cancellation") return deriveCancellation(input)
  if (input.subject === "interference") return deriveInterference(input)
  return { accepted: false, reason: "unknownSubject" }
}
