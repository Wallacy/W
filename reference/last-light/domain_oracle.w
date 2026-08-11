module domain_oracle<domains: [.concurrent(.catalog)]>

// Pure oracle for logical execution-domain selection and bounded profile admission.

enum DomainName {
  unselected
  main
  compute
  io
  network
  thermal
  catalog
}

enum DomainIntent {
  inheritedChild
  dispatchedChild
  parallelGroup
}

enum DispatchMode {
  ordinary
  barrier
}

enum DomainResolution {
  selected(DomainName)
  missingExplicitDomain
}

const fn resolveDomain(
  intent: DomainIntent,
  explicit: DomainName,
  inherited: DomainName,
): DomainResolution {
  if intent == .inheritedChild {
    if inherited != .unselected {
      return .selected(inherited)
    }

    return .selected(.main)
  }

  if explicit == .unselected {
    return .missingExplicitDomain
  }

  return .selected(explicit)
}

struct DomainContract {
  name: DomainName
  serial: Bool
  parallel: Bool
  barrierDispatch: Bool
  capacity: u16
}

enum AdmissionDecision {
  accepted
  requiresParallelDomain
  requiresBarrierDispatch
  barrierMaySuspend
  invalidCapabilities
  emptyCapacity
}

const fn admit(
  intent: DomainIntent,
  mode: DispatchMode,
  maySuspend: Bool,
  contract: DomainContract,
): AdmissionDecision {
  if contract.capacity == 0 {
    return .emptyCapacity
  }

  if contract.serial && contract.parallel {
    return .invalidCapabilities
  }

  if intent == .parallelGroup && !contract.parallel {
    return .requiresParallelDomain
  }

  if mode == .barrier {
    if maySuspend {
      return .barrierMaySuspend
    }

    if !contract.serial && !contract.barrierDispatch {
      return .requiresBarrierDispatch
    }
  }

  return .accepted
}

enum ReadyDecision {
  run
  queued
  queuedFifo
}

const fn scheduleReady(
  active: u16,
  contract: DomainContract,
): ReadyDecision {
  if active < contract.capacity {
    return .run
  }

  if contract.serial {
    return .queuedFifo
  }

  return .queued
}

enum BarrierGateDecision {
  runShared
  runExclusive
  waitForEarlierTickets
  waitForBarrier
}

enum StructuredTicketDecision {
  inheritParentTicket
  allocateTargetTicket
  barrierCannotCreateChild
}

const fn scheduleBarrierTicket(
  mode: DispatchMode,
  earlierOutstanding: u16,
  earlierBarrierPending: Bool,
): BarrierGateDecision {
  if mode == .barrier {
    if earlierOutstanding > 0 {
      return .waitForEarlierTickets
    }

    return .runExclusive
  }

  if earlierBarrierPending {
    return .waitForBarrier
  }

  return .runShared
}

const fn resolveStructuredTicket(
  sameDomain: Bool,
  parentMode: DispatchMode,
): StructuredTicketDecision {
  if parentMode == .barrier {
    return .barrierCannotCreateChild
  }

  if sameDomain {
    return .inheritParentTicket
  }

  return .allocateTargetTicket
}

enum CapacityDecision {
  reduced(u16)
  reject
}

enum DynamicLaneState {
  absent
  open
  closing
  drained
}

enum DynamicLaneDecision {
  opened
  admitted
  waitingForJobs
  closed
  liveBudgetExhausted
  aggregateBudgetExhausted
  aggregateFrameBudgetExhausted
  laneLimitExceeded
  laneFrameLimitExceeded
  admissionClosed
}

struct DynamicLaneTransition {
  state: DynamicLaneState
  decision: DynamicLaneDecision
}

const fn reduceCapacity(
  artifactMaximum: u16,
  deploymentMaximum: u16,
): CapacityDecision {
  if deploymentMaximum == 0 || deploymentMaximum > artifactMaximum {
    return .reject
  }

  return .reduced(deploymentMaximum)
}

const fn openDynamicSerial(
  live: u16,
  liveLimit: u16,
  requestedJobs: u16,
  requestedFrameBytes: u64,
  aggregateJobsAvailable: u16,
  aggregateFrameBytesAvailable: u64,
  laneMaximumJobs: u16,
  laneMaximumFrameBytes: u64,
): DynamicLaneTransition {
  if live >= liveLimit {
    return DynamicLaneTransition(state: .absent, decision: .liveBudgetExhausted)
  }

  if requestedJobs == 0 || requestedJobs > laneMaximumJobs {
    return DynamicLaneTransition(state: .absent, decision: .laneLimitExceeded)
  }

  if requestedFrameBytes == 0 || requestedFrameBytes > laneMaximumFrameBytes {
    return DynamicLaneTransition(state: .absent, decision: .laneFrameLimitExceeded)
  }

  if requestedJobs > aggregateJobsAvailable {
    return DynamicLaneTransition(state: .absent, decision: .aggregateBudgetExhausted)
  }

  if requestedFrameBytes > aggregateFrameBytesAvailable {
    return DynamicLaneTransition(state: .absent, decision: .aggregateFrameBudgetExhausted)
  }

  return DynamicLaneTransition(state: .open, decision: .opened)
}

const fn admitDynamicSerial(state: DynamicLaneState): DynamicLaneDecision {
  if state != .open {
    return .admissionClosed
  }

  return .admitted
}

const fn closeDynamicSerial(
  state: DynamicLaneState,
  outstandingJobs: u16,
): DynamicLaneTransition {
  if state == .open && outstandingJobs > 0 {
    return DynamicLaneTransition(state: .closing, decision: .waitingForJobs)
  }

  if state == .open || (state == .closing && outstandingJobs == 0) {
    return DynamicLaneTransition(state: .drained, decision: .closed)
  }

  if state == .closing {
    return DynamicLaneTransition(state: .closing, decision: .waitingForJobs)
  }

  return DynamicLaneTransition(state: state, decision: .admissionClosed)
}

struct CatalogState {
  revision: u64
}

fn observeCatalog(state: ref CatalogState): u64 {
  return state.revision
}

fn replaceCatalog(state: inout CatalogState, revision: u64): u64 {
  state.revision = revision
  return state.revision
}

export async fn scheduleCatalogRevision(
  state: inout CatalogState,
  revision: u64,
): (u64, u64, u64) {
  spawn<.catalog> let before = observeCatalog(ref state)
  spawn<.catalog, .barrier> let update = replaceCatalog(inout state, revision: revision)
  spawn<.catalog> let after = observeCatalog(ref state)
  return await (before, update, after)
}

test "async inherits and spawn requires an explicit domain" for resolveDomain {
  expect resolveDomain(
    intent: .inheritedChild,
    explicit: .unselected,
    inherited: .compute,
  ) == .selected(.compute)

  expect resolveDomain(
    intent: .inheritedChild,
    explicit: .unselected,
    inherited: .unselected,
  ) == .selected(.main)

  expect resolveDomain(
    intent: .dispatchedChild,
    explicit: .thermal,
    inherited: .compute,
  ) == .selected(.thermal)

  expect resolveDomain(
    intent: .dispatchedChild,
    explicit: .unselected,
    inherited: .compute,
  ) == .missingExplicitDomain
}

test "spawn accepts serial domains and parallel groups require capability" for admit {
  expect admit(
    intent: .dispatchedChild,
    mode: .ordinary,
    maySuspend: true,
    contract: DomainContract(
      name: .thermal,
      serial: true,
      parallel: false,
      barrierDispatch: false,
      capacity: 1,
    ),
  ) == .accepted

  expect admit(
    intent: .parallelGroup,
    mode: .ordinary,
    maySuspend: true,
    contract: DomainContract(
      name: .thermal,
      serial: true,
      parallel: false,
      barrierDispatch: false,
      capacity: 1,
    ),
  ) == .requiresParallelDomain

  expect admit(
    intent: .parallelGroup,
    mode: .ordinary,
    maySuspend: true,
    contract: DomainContract(
      name: .compute,
      serial: false,
      parallel: true,
      barrierDispatch: true,
      capacity: 1,
    ),
  ) == .accepted
}

test "barrier dispatch is exclusive and never suspends" for admit {
  expect admit(
    intent: .dispatchedChild,
    mode: .barrier,
    maySuspend: false,
    contract: DomainContract(
      name: .catalog,
      serial: false,
      parallel: false,
      barrierDispatch: true,
      capacity: 4,
    ),
  ) == .accepted

  expect admit(
    intent: .dispatchedChild,
    mode: .barrier,
    maySuspend: true,
    contract: DomainContract(
      name: .catalog,
      serial: false,
      parallel: false,
      barrierDispatch: true,
      capacity: 4,
    ),
  ) == .barrierMaySuspend

  expect admit(
    intent: .dispatchedChild,
    mode: .barrier,
    maySuspend: false,
    contract: DomainContract(
      name: .compute,
      serial: false,
      parallel: true,
      barrierDispatch: false,
      capacity: 4,
    ),
  ) == .requiresBarrierDispatch
}

test "barrier tickets split concurrent read epochs" for scheduleBarrierTicket {
  expect scheduleBarrierTicket(
    mode: .ordinary,
    earlierOutstanding: 1,
    earlierBarrierPending: false,
  ) == .runShared

  expect scheduleBarrierTicket(
    mode: .barrier,
    earlierOutstanding: 2,
    earlierBarrierPending: false,
  ) == .waitForEarlierTickets

  expect scheduleBarrierTicket(
    mode: .barrier,
    earlierOutstanding: 0,
    earlierBarrierPending: false,
  ) == .runExclusive

  expect scheduleBarrierTicket(
    mode: .ordinary,
    earlierOutstanding: 0,
    earlierBarrierPending: true,
  ) == .waitForBarrier
}

test "same-domain structured children stay inside the parent ticket" for resolveStructuredTicket {
  expect resolveStructuredTicket(
    sameDomain: true,
    parentMode: .ordinary,
  ) == .inheritParentTicket
  expect resolveStructuredTicket(
    sameDomain: false,
    parentMode: .ordinary,
  ) == .allocateTargetTicket
  expect resolveStructuredTicket(
    sameDomain: true,
    parentMode: .barrier,
  ) == .barrierCannotCreateChild
}

test "an occupied serial domain queues admitted work in FIFO order" for scheduleReady {
  let thermal = DomainContract(
    name: .thermal,
    serial: true,
    parallel: false,
    barrierDispatch: false,
    capacity: 1,
  )
  let compute = DomainContract(
    name: .compute,
    serial: false,
    parallel: true,
    barrierDispatch: true,
    capacity: 2,
  )

  expect scheduleReady(active: 0, contract: thermal) == .run
  expect scheduleReady(active: 1, contract: thermal) == .queuedFifo
  expect scheduleReady(active: 1, contract: compute) == .run
  expect scheduleReady(active: 2, contract: compute) == .queued
}

test "deployment can reduce but cannot increase profile capacity" for reduceCapacity {
  expect reduceCapacity(artifactMaximum: 8, deploymentMaximum: 4) == .reduced(4)
  expect reduceCapacity(artifactMaximum: 8, deploymentMaximum: 8) == .reduced(8)
  expect reduceCapacity(artifactMaximum: 8, deploymentMaximum: 9) == .reject
  expect reduceCapacity(artifactMaximum: 8, deploymentMaximum: 0) == .reject
}

test "a dynamic serial lane is bounded and closes after drain" for openDynamicSerial {
  expect openDynamicSerial(
    live: 3,
    liveLimit: 4,
    requestedJobs: 16,
    requestedFrameBytes: 65_536,
    aggregateJobsAvailable: 32,
    aggregateFrameBytesAvailable: 131_072,
    laneMaximumJobs: 16,
    laneMaximumFrameBytes: 65_536,
  ) == DynamicLaneTransition(state: .open, decision: .opened)

  expect openDynamicSerial(
    live: 4,
    liveLimit: 4,
    requestedJobs: 1,
    requestedFrameBytes: 1,
    aggregateJobsAvailable: 32,
    aggregateFrameBytesAvailable: 131_072,
    laneMaximumJobs: 16,
    laneMaximumFrameBytes: 65_536,
  ) == DynamicLaneTransition(state: .absent, decision: .liveBudgetExhausted)

  expect openDynamicSerial(
    live: 3,
    liveLimit: 4,
    requestedJobs: 17,
    requestedFrameBytes: 1,
    aggregateJobsAvailable: 32,
    aggregateFrameBytesAvailable: 131_072,
    laneMaximumJobs: 16,
    laneMaximumFrameBytes: 65_536,
  ) == DynamicLaneTransition(state: .absent, decision: .laneLimitExceeded)

  expect openDynamicSerial(
    live: 3,
    liveLimit: 4,
    requestedJobs: 16,
    requestedFrameBytes: 1,
    aggregateJobsAvailable: 8,
    aggregateFrameBytesAvailable: 131_072,
    laneMaximumJobs: 16,
    laneMaximumFrameBytes: 65_536,
  ) == DynamicLaneTransition(state: .absent, decision: .aggregateBudgetExhausted)

  expect openDynamicSerial(
    live: 3,
    liveLimit: 4,
    requestedJobs: 16,
    requestedFrameBytes: 65_537,
    aggregateJobsAvailable: 32,
    aggregateFrameBytesAvailable: 131_072,
    laneMaximumJobs: 16,
    laneMaximumFrameBytes: 65_536,
  ) == DynamicLaneTransition(state: .absent, decision: .laneFrameLimitExceeded)

  expect openDynamicSerial(
    live: 3,
    liveLimit: 4,
    requestedJobs: 16,
    requestedFrameBytes: 65_536,
    aggregateJobsAvailable: 32,
    aggregateFrameBytesAvailable: 32_768,
    laneMaximumJobs: 16,
    laneMaximumFrameBytes: 65_536,
  ) == DynamicLaneTransition(state: .absent, decision: .aggregateFrameBudgetExhausted)

  expect admitDynamicSerial(state: .open) == .admitted
  expect closeDynamicSerial(
    state: .open,
    outstandingJobs: 2,
  ) == DynamicLaneTransition(state: .closing, decision: .waitingForJobs)
  expect admitDynamicSerial(state: .closing) == .admissionClosed
  expect closeDynamicSerial(
    state: .closing,
    outstandingJobs: 0,
  ) == DynamicLaneTransition(state: .drained, decision: .closed)
}

// Compile-fail assays:
// spawn<.catalog, .barrier> let invalid = suspendingWrite(inout state)
// spawn<.network, .barrier> let invalid = replaceCatalog(inout state, revision: 2)
