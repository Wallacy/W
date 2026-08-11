export class InterferenceLayoutModelError extends Error {
  constructor(code) {
    super(code)
    this.name = "InterferenceLayoutModelError"
    this.code = code
  }
}

function fail(code) {
  throw new InterferenceLayoutModelError(code)
}

export function expandInterferenceLayoutOperations(fixtures, entries, stack = []) {
  const result = []
  for (const entry of entries) {
    if (typeof entry !== "string") {
      result.push(entry)
      continue
    }
    if (!entry.startsWith("$")) fail(`interferenceFixtureInvalid:${entry}`)
    const name = entry.slice(1)
    if (stack.includes(name)) fail(`interferenceFixtureRecursive:${name}`)
    const fixture = fixtures?.[name]
    if (!Array.isArray(fixture)) fail(`interferenceFixtureMissing:${name}`)
    result.push(...expandInterferenceLayoutOperations(fixtures, fixture, [...stack, name]))
  }
  return result
}

function requireName(value, code = "interferenceNameMissing") {
  if (typeof value !== "string" || value.length === 0) fail(code)
}

function requirePositiveInteger(value, code = "interferenceIntegerInvalid") {
  if (!Number.isSafeInteger(value) || value <= 0) fail(code)
}

function alignUp(value, alignment) {
  return Math.ceil(value / alignment) * alignment
}

function overlaps(left, right) {
  return left.offset < right.offset + right.size && right.offset < left.offset + left.size
}

function harmfulPairs(accesses) {
  const pairs = []
  for (let leftIndex = 0; leftIndex < accesses.length; leftIndex += 1) {
    for (let rightIndex = leftIndex + 1; rightIndex < accesses.length; rightIndex += 1) {
      const left = accesses[leftIndex]
      const right = accesses[rightIndex]
      if (left.place === right.place) continue
      if (left.group === right.group) continue
      if (left.phase !== right.phase) continue
      if (!left.concurrent || !right.concurrent) continue
      if (left.mode !== "write" && right.mode !== "write") continue
      pairs.push(`${left.place}:${left.group}<->${right.place}:${right.group}`)
    }
  }
  return pairs
}

function hasDataRace(candidate, accesses) {
  if (candidate.storage === "atomic") return false
  for (let leftIndex = 0; leftIndex < accesses.length; leftIndex += 1) {
    for (let rightIndex = leftIndex + 1; rightIndex < accesses.length; rightIndex += 1) {
      const left = accesses[leftIndex]
      const right = accesses[rightIndex]
      if (left.place !== right.place) continue
      if (left.group === right.group) continue
      if (left.phase !== right.phase) continue
      if (!left.concurrent || !right.concurrent) continue
      if (left.mode === "write" || right.mode === "write") return true
    }
  }
  return false
}

function physicalLayout(candidate, granule) {
  const ordered = [...candidate.places].sort((left, right) => left.offset - right.offset)
  const offsets = {}
  let cursor = 0
  for (const place of ordered) {
    cursor = alignUp(cursor, granule)
    offsets[place.name] = cursor
    cursor += place.size
  }
  const perInstanceSize = alignUp(cursor, granule)
  const beforeBytes = candidate.aggregateSize * candidate.instanceCount
  const afterBytes = perInstanceSize * candidate.instanceCount
  return {
    offsets,
    perInstanceSize,
    beforeBytes,
    afterBytes,
    additionalBytes: afterBytes - beforeBytes,
  }
}

function selectLayout(state, force) {
  const candidate = state.candidate
  if (!candidate) fail("interferenceCandidateMissing")
  if (state.selection) fail("interferenceSelectionDuplicate")
  if (hasDataRace(candidate, state.accesses)) fail("interferenceProgramHasRace")

  const pairs = harmfulPairs(state.accesses)
  let selection

  if (pairs.length === 0) {
    selection = { status: "notNeeded", reason: "noHarmfulPair", harmfulPairs: [] }
  } else if (candidate.layoutVisibility !== "privateOpaque") {
    selection = { status: "notApplied", reason: "publishedLayout", harmfulPairs: pairs }
  } else if (candidate.boundaries.length > 0) {
    selection = {
      status: "notApplied",
      reason: `boundary:${candidate.boundaries[0]}`,
      harmfulPairs: pairs,
    }
  } else if (candidate.instanceCount > 1 && candidate.layoutScope !== "allocation") {
    selection = { status: "notApplied", reason: "strideUnproven", harmfulPairs: pairs }
  } else if (!state.target || state.target.interferenceSize === null) {
    selection = { status: "notApplied", reason: "interferenceSizeUnknown", harmfulPairs: pairs }
  } else if (!state.target.recipeInput) {
    selection = { status: "notApplied", reason: "targetProfileUnpinned", harmfulPairs: pairs }
  } else if (state.target.maximumAlignment < state.target.interferenceSize) {
    selection = { status: "notApplied", reason: "alignmentUnavailable", harmfulPairs: pairs }
  } else if (!state.evidence || !state.evidence.contentionObserved || !state.evidence.hot) {
    selection = { status: "notApplied", reason: "contentionEvidenceMissing", harmfulPairs: pairs }
  } else if (!state.evidence.recipeInput) {
    selection = { status: "notApplied", reason: "evidenceUnpinned", harmfulPairs: pairs }
  } else {
    const layout = physicalLayout(candidate, state.target.interferenceSize)
    if (!state.budget) {
      selection = { status: "notApplied", reason: "footprintBudgetMissing", harmfulPairs: pairs }
    } else if (layout.additionalBytes > state.budget.maximumAdditionalBytes) {
      selection = {
        status: "notApplied",
        reason: "footprintBudgetExceeded",
        harmfulPairs: pairs,
        layout,
      }
    } else {
      selection = {
        status: "applied",
        reason: "privateOpaqueMeasuredContention",
        harmfulPairs: pairs,
        likelyMitigated: true,
        guaranteedExclusiveCacheLine: false,
        interferenceSize: state.target.interferenceSize,
        targetProfileDigest: state.target.digest,
        evidenceDigest: state.evidence.digest,
        layout,
      }
    }
  }

  selection.progressContract = candidate.lockFreeRequired ? "lockFree" : "general"
  if (force === true && selection.status !== "applied") fail("interferenceForcedLayoutBlocked")
  state.selection = selection
  state.trace.push(`layout:${selection.status}:${selection.reason}`)
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "declareCandidate": {
      if (state.candidate) fail("interferenceCandidateDuplicate")
      requireName(operation.id)
      if (!new Set(["atomic", "ordinary"]).has(operation.storage)) {
        fail("interferenceStorageInvalid")
      }
      if (!new Set(["privateOpaque", "published"]).has(operation.layoutVisibility)) {
        fail("interferenceVisibilityInvalid")
      }
      if (!new Set(["aggregate", "allocation"]).has(operation.layoutScope)) {
        fail("interferenceScopeInvalid")
      }
      requirePositiveInteger(operation.aggregateSize)
      requirePositiveInteger(operation.instanceCount)
      if (!Array.isArray(operation.places) || operation.places.length < 2) {
        fail("interferencePlacesMissing")
      }
      const names = new Set()
      for (const place of operation.places) {
        requireName(place.name)
        if (names.has(place.name)) fail("interferencePlaceDuplicate")
        names.add(place.name)
        if (!Number.isSafeInteger(place.offset) || place.offset < 0) {
          fail("interferenceOffsetInvalid")
        }
        requirePositiveInteger(place.size)
        if (place.offset + place.size > operation.aggregateSize) {
          fail("interferencePlaceOutsideAggregate")
        }
      }
      for (let left = 0; left < operation.places.length; left += 1) {
        for (let right = left + 1; right < operation.places.length; right += 1) {
          if (overlaps(operation.places[left], operation.places[right])) {
            fail("interferencePlacesOverlap")
          }
        }
      }
      const orderedPlaces = [...operation.places].sort((left, right) => left.offset - right.offset)
      let coveredBytes = 0
      for (const place of orderedPlaces) {
        if (place.offset !== coveredBytes) fail("interferenceLayoutIncomplete")
        coveredBytes += place.size
      }
      if (coveredBytes !== operation.aggregateSize) fail("interferenceLayoutIncomplete")
      const allowedBoundaries = new Set([
        "abi",
        "ffi",
        "sharedMemory",
        "persistence",
        "addressExposure",
        "physicalReflection",
        "dynamicLoading",
        "device",
      ])
      const boundaries = operation.boundaries ?? []
      if (!Array.isArray(boundaries) || boundaries.some((item) => !allowedBoundaries.has(item))) {
        fail("interferenceBoundaryInvalid")
      }
      state.candidate = {
        id: operation.id,
        storage: operation.storage,
        layoutVisibility: operation.layoutVisibility,
        layoutScope: operation.layoutScope,
        aggregateSize: operation.aggregateSize,
        instanceCount: operation.instanceCount,
        places: operation.places.map((place) => ({ ...place })),
        boundaries: [...boundaries],
        lockFreeRequired: operation.lockFreeRequired === true,
      }
      state.trace.push(`candidate:${operation.id}`)
      return
    }

    case "recordAccess": {
      if (!state.candidate) fail("interferenceCandidateMissing")
      requireName(operation.place)
      requireName(operation.group)
      requireName(operation.phase)
      if (!state.candidate.places.some((place) => place.name === operation.place)) {
        fail("interferenceAccessPlaceMissing")
      }
      if (!new Set(["read", "write"]).has(operation.mode)) fail("interferenceAccessModeInvalid")
      state.accesses.push({
        place: operation.place,
        group: operation.group,
        phase: operation.phase,
        mode: operation.mode,
        concurrent: operation.concurrent === true,
        order: operation.order ?? "ordinary",
      })
      return
    }

    case "targetProfile": {
      if (state.target) fail("interferenceTargetDuplicate")
      requireName(operation.digest)
      if (operation.interferenceSize !== null) {
        requirePositiveInteger(operation.interferenceSize)
        if (!Number.isInteger(Math.log2(operation.interferenceSize))) {
          fail("interferenceSizeInvalid")
        }
      }
      requirePositiveInteger(operation.maximumAlignment)
      state.target = {
        digest: operation.digest,
        interferenceSize: operation.interferenceSize,
        maximumAlignment: operation.maximumAlignment,
        recipeInput: operation.recipeInput === true,
      }
      return
    }

    case "contentionEvidence": {
      if (state.evidence) fail("interferenceEvidenceDuplicate")
      requireName(operation.digest)
      if (!new Set(["measurement", "targetCostModel"]).has(operation.kind)) {
        fail("interferenceEvidenceKindInvalid")
      }
      state.evidence = {
        digest: operation.digest,
        kind: operation.kind,
        contentionObserved: operation.contentionObserved === true,
        hot: operation.hot === true,
        recipeInput: operation.recipeInput === true,
      }
      return
    }

    case "footprintBudget":
      if (state.budget) fail("interferenceBudgetDuplicate")
      if (!Number.isSafeInteger(operation.maximumAdditionalBytes) || operation.maximumAdditionalBytes < 0) {
        fail("interferenceBudgetInvalid")
      }
      state.budget = { maximumAdditionalBytes: operation.maximumAdditionalBytes }
      return

    case "selectLayout":
      selectLayout(state, operation.force)
      return

    case "requestSourceCacheIsolation":
      fail("interferenceSourceCacheContractAbsent")

    case "requestSilentSharding":
      fail("interferenceSilentShardingChangesSemantics")

    case "partitionAndJoin": {
      requirePositiveInteger(operation.shards, "interferencePartitionShardsInvalid")
      requireName(operation.join, "interferencePartitionJoinMissing")
      if (
        operation.ownershipDisjoint !== true ||
        operation.joinExplicit !== true ||
        operation.overflowPolicyPreserved !== true ||
        operation.snapshotPolicyDeclared !== true
      ) {
        fail("interferencePartitionContractIncomplete")
      }
      state.partition = {
        status: "explicit",
        shards: operation.shards,
        join: operation.join,
      }
      state.trace.push(`partition:${operation.shards}:${operation.join}`)
      return
    }

    case "preserveGlobalAtomic":
      if (operation.silentSharding === true) fail("interferenceSilentShardingChangesSemantics")
      state.globalAtomicPreserved = true
      state.trace.push("atomic:preserved")
      return

    case "publishPhysicalLayout":
      requireName(operation.targetRecord, "interferencePhysicalTargetMissing")
      if (operation.targetRecordPinned !== true || operation.offsetsVerified !== true) {
        fail("interferencePhysicalLayoutUnverified")
      }
      if (operation.claimsExclusiveCacheLine === true) {
        fail("interferencePhysicalPerformanceClaim")
      }
      state.physicalBoundary = {
        status: "published",
        targetRecord: operation.targetRecord,
        performanceGuarantee: false,
      }
      state.trace.push(`boundary-layout:${operation.targetRecord}`)
      return

    case "claimImprovement":
      if (
        !state.evidence?.contentionObserved ||
        !state.evidence.recipeInput ||
        operation.baselineSemanticDigest !== operation.candidateSemanticDigest ||
        operation.recipeInputsExceptLayoutEqual !== true ||
        operation.baselineDigest === operation.candidateDigest ||
        typeof operation.baselineDigest !== "string" ||
        typeof operation.candidateDigest !== "string" ||
        operation.reproduced !== true
      ) {
        fail("interferenceImprovementUnproved")
      }
      state.performanceClaim = "measured"
      return

    default:
      fail(`interferenceOperationUnknown:${operation.op}`)
  }
}

function normalizedState(state) {
  return {
    candidate: state.candidate,
    accesses: state.accesses,
    target: state.target,
    evidence: state.evidence,
    budget: state.budget,
    selection: state.selection,
    partition: state.partition,
    globalAtomicPreserved: state.globalAtomicPreserved,
    physicalBoundary: state.physicalBoundary,
    performanceClaim: state.performanceClaim,
    trace: state.trace,
  }
}

export function runInterferenceLayoutOperations(operations) {
  const state = {
    candidate: null,
    accesses: [],
    target: null,
    evidence: null,
    budget: null,
    selection: null,
    partition: null,
    globalAtomicPreserved: false,
    physicalBoundary: null,
    performanceClaim: null,
    trace: [],
  }

  try {
    for (const operation of operations) applyOperation(state, operation)
    return { status: "accepted", error: null, state: normalizedState(state) }
  } catch (error) {
    if (!(error instanceof InterferenceLayoutModelError)) throw error
    return { status: "rejected", error: error.code, state: normalizedState(state) }
  }
}
