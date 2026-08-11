const STRATEGIES = new Set(["reference-counting", "epoch", "hazard", "lock"])

export class SnapshotCellModelError extends Error {
  constructor(code) {
    super(code)
    this.name = "SnapshotCellModelError"
    this.code = code
  }
}

function fail(code) {
  throw new SnapshotCellModelError(code)
}

function requireString(value, code) {
  if (typeof value !== "string" || value.length === 0) fail(code)
}

function activeReaders(state, version = undefined) {
  return Object.values(state.readers).filter((reader) =>
    reader.phase === "active" && (version === undefined || reader.version === version))
}

function physicalReaders(state, version = undefined) {
  return Object.entries(state.physicalReaders).filter(([, readerVersion]) =>
    version === undefined || readerVersion === version)
}

function recordPhysicalRead(state, reader, version) {
  state.physicalReaders[reader] = version
  switch (state.strategy) {
    case "reference-counting":
      state.physicalTrace.push(`retain:${version}`)
      return
    case "epoch":
      state.physicalTrace.push(`enter-epoch:${reader}:${version}`)
      return
    case "hazard":
      state.physicalTrace.push(`protect:${reader}:${version}`, `validate:${reader}:${version}`)
      return
    case "lock":
      state.physicalTrace.push(`lock:read:${reader}`, `lease:${reader}:${version}`, `unlock:read:${reader}`)
      return
    default:
      fail("W-SNAPSHOT-0010")
  }
}

function releasePhysicalRead(state, reader, version) {
  delete state.physicalReaders[reader]
  switch (state.strategy) {
    case "reference-counting":
      state.physicalTrace.push(`release:${version}`)
      return
    case "epoch":
      state.physicalTrace.push(`leave-epoch:${reader}:${version}`)
      return
    case "hazard":
      state.physicalTrace.push(`clear:${reader}:${version}`)
      return
    case "lock":
      state.physicalTrace.push(`release-lease:${reader}:${version}`)
      return
    default:
      fail("W-SNAPSHOT-0010")
  }
}

function recordPhysicalPublish(state, previous, version) {
  switch (state.strategy) {
    case "reference-counting":
      state.physicalTrace.push(`atomic-swap:${previous}->${version}`)
      return
    case "epoch":
      state.physicalTrace.push(`advance-epoch:${previous}->${version}`)
      return
    case "hazard":
      state.physicalTrace.push(`hazard-swap:${previous}->${version}`)
      return
    case "lock":
      state.physicalTrace.push("lock:publish", `swap:${previous}->${version}`, "unlock:publish")
      return
    default:
      fail("W-SNAPSHOT-0010")
  }
}

function requireOpen(state) {
  if (state.phase !== "open") fail("W-SNAPSHOT-0009")
}

function requireReader(state, readerId) {
  const reader = state.readers[readerId]
  if (!reader || reader.phase !== "active") fail("snapshotReaderNotActive")
  return reader
}

function requireSnapshotFacts(operation) {
  if (
    operation.transferable !== true ||
    operation.shareable !== true ||
    operation.lifetimeIndependent !== true
  ) {
    fail("W-SNAPSHOT-0003")
  }
}

function reclaimVersion(state, version) {
  const record = state.versions[version]
  if (!record || record.phase !== "retired") return
  if (physicalReaders(state, version).length > 0) return
  if (record.dropCount !== 0) fail("snapshotDropRepeated")
  record.phase = "reclaimed"
  record.dropCount = 1
  state.drops.push(version)
  state.physicalTrace.push(`reclaim:${version}`)
}

function verifyState(state) {
  if (state.currentVersion !== null) {
    const current = state.versions[state.currentVersion]
    if (!current || current.phase !== "current" || current.dropCount !== 0) {
      fail("snapshotCurrentVersionInvalid")
    }
  }

  for (const [versionText, record] of Object.entries(state.versions)) {
    const version = Number(versionText)
    const readers = activeReaders(state, version).length
    if (record.phase === "retired" && readers === 0) {
      fail("snapshotRetiredVersionUnbounded")
    }
    if (record.phase === "reclaimed" && (readers !== 0 || record.dropCount !== 1)) {
      fail("snapshotReclaimedVersionInvalid")
    }
  }

  const logical = activeReaders(state)
    .map((reader) => reader.version)
    .sort((left, right) => left - right)
  const physical = physicalReaders(state)
    .map(([, version]) => version)
    .sort((left, right) => left - right)
  if (JSON.stringify(logical) !== JSON.stringify(physical)) {
    fail("snapshotPhysicalReaderMismatch")
  }
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "create": {
      if (state.phase !== "uninitialized") fail("snapshotCellAlreadyCreated")
      if (!STRATEGIES.has(operation.strategy)) fail("W-SNAPSHOT-0010")
      if (operation.owned !== true) fail("W-SNAPSHOT-0006")
      requireSnapshotFacts(operation)
      requireString(operation.value, "snapshotValueMissing")
      state.strategy = operation.strategy
      state.phase = "open"
      state.currentVersion = 0
      state.nextVersion = 1
      state.versions[0] = {
        value: operation.value,
        phase: "current",
        dropCount: 0,
      }
      return
    }

    case "beginRead": {
      requireOpen(state)
      requireString(operation.reader, "snapshotReaderMissing")
      if (state.readers[operation.reader]) fail("snapshotReaderDuplicate")
      const version = state.currentVersion
      state.readers[operation.reader] = { version, phase: "active", outcome: null }
      recordPhysicalRead(state, operation.reader, version)
      state.observations.push({
        reader: operation.reader,
        version,
        value: state.versions[version].value,
      })
      if (version > 0) state.happensBefore.push(`publish:${version}->read:${operation.reader}`)
      return
    }

    case "finishRead": {
      const reader = requireReader(state, operation.reader)
      if (!new Set(["success", "error"]).has(operation.outcome)) {
        fail("snapshotReadOutcomeInvalid")
      }
      reader.phase = "closed"
      reader.outcome = operation.outcome
      releasePhysicalRead(state, operation.reader, reader.version)
      reclaimVersion(state, reader.version)
      return
    }

    case "publish": {
      requireOpen(state)
      if (operation.owned !== true) fail("W-SNAPSHOT-0006")
      requireSnapshotFacts(operation)
      requireString(operation.value, "snapshotValueMissing")
      if (operation.allocation === "fault") {
        state.physicalTrace.push("prepare-publication:fault")
        state.phase = "faulted"
        fail("snapshotPublicationAllocationFault")
      }
      if (operation.allocation !== undefined && operation.allocation !== "success") {
        fail("snapshotAllocationEvidenceInvalid")
      }

      const previous = state.currentVersion
      const version = state.nextVersion
      state.nextVersion += 1
      state.versions[previous].phase = "retired"
      state.versions[version] = {
        value: operation.value,
        phase: "current",
        dropCount: 0,
      }
      state.currentVersion = version
      state.publicationOrder.push(version)
      recordPhysicalPublish(state, previous, version)
      reclaimVersion(state, previous)
      return
    }

    case "copySnapshot": {
      requireOpen(state)
      if (operation.duplicable !== true) fail("W-SNAPSHOT-0007")
      const version = state.currentVersion
      state.copies.push({ version, value: state.versions[version].value })
      if (version > 0) state.happensBefore.push(`publish:${version}->copy:${state.copies.length - 1}`)
      return
    }

    case "escapeRead":
      requireReader(state, operation.reader)
      fail("W-SNAPSHOT-0001")

    case "suspendRead":
      requireReader(state, operation.reader)
      fail("W-SNAPSHOT-0002")

    case "mutateRead":
      requireReader(state, operation.reader)
      fail("W-SNAPSHOT-0004")

    case "copyCell":
      requireOpen(state)
      fail("W-SNAPSHOT-0005")

    case "close": {
      requireOpen(state)
      if (activeReaders(state).length > 0) fail("W-SNAPSHOT-0008")
      const current = state.currentVersion
      state.versions[current].phase = "retired"
      reclaimVersion(state, current)
      state.currentVersion = null
      state.phase = "closed"
      return
    }

    default:
      fail("snapshotOperationUnknown")
  }
}

export function logicalSnapshotProjection(state) {
  return {
    phase: state.phase,
    currentVersion: state.currentVersion,
    currentValue:
      state.currentVersion === null ? null : state.versions[state.currentVersion]?.value ?? null,
    versions: Object.fromEntries(
      Object.entries(state.versions).map(([version, record]) => [
        version,
        { value: record.value, phase: record.phase, dropCount: record.dropCount },
      ]),
    ),
    readers: state.readers,
    observations: state.observations,
    copies: state.copies,
    publicationOrder: state.publicationOrder,
    happensBefore: state.happensBefore,
    drops: state.drops,
  }
}

export function runSnapshotCellOperations(operations) {
  const state = {
    phase: "uninitialized",
    strategy: null,
    currentVersion: null,
    nextVersion: 0,
    versions: {},
    readers: {},
    physicalReaders: {},
    physicalTrace: [],
    observations: [],
    copies: [],
    publicationOrder: [],
    happensBefore: [],
    drops: [],
    trace: [],
  }
  let error = null

  for (const [index, operation] of operations.entries()) {
    try {
      applyOperation(state, operation)
      verifyState(state)
      state.trace.push({ index, op: operation.op, status: "accepted" })
    } catch (cause) {
      if (!(cause instanceof SnapshotCellModelError)) throw cause
      verifyState(state)
      error = cause.code
      state.trace.push({ index, op: operation.op, status: "rejected", code: cause.code })
      break
    }
  }

  return {
    status: error === null
      ? "accepted"
      : error === "snapshotPublicationAllocationFault"
        ? "fault"
        : "rejected",
    error,
    strategy: state.strategy,
    state: logicalSnapshotProjection(state),
    physical: {
      readers: state.physicalReaders,
      trace: state.physicalTrace,
    },
    trace: state.trace,
  }
}
