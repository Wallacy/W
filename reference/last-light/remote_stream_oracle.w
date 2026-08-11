// Pure oracle for service stream eligibility, credits, terminals, and routing.

export struct StreamCreditTotals {
  items: u64
  bytes: u64
}

export struct StreamSentTotals {
  items: u64
  bytes: u64
}

export enum StreamCreditDecision {
  send
  wait
  protocolFailure
}

export const fn expectedCreditDecision(
  named granted: StreamCreditTotals,
  named sent: StreamSentTotals,
  named nextLogicalBytes: u64,
): StreamCreditDecision {
  if sent.items > granted.items || sent.bytes > granted.bytes {
    return .protocolFailure
  }

  if sent.items == granted.items {
    return .wait
  }

  if nextLogicalBytes > granted.bytes - sent.bytes {
    return .wait
  }

  return .send
}

export const fn creditUpdateIsValid(
  named previous: StreamCreditTotals,
  named next: StreamCreditTotals,
): Bool {
  return next.items >= previous.items && next.bytes >= previous.bytes
}

export const fn aggregateGrantFits(
  named limit: StreamCreditTotals,
  named reserved: StreamCreditTotals,
  named requested: StreamCreditTotals,
): Bool {
  if reserved.items > limit.items || reserved.bytes > limit.bytes {
    return false
  }

  return requested.items <= limit.items - reserved.items &&
    requested.bytes <= limit.bytes - reserved.bytes
}

export enum RemoteStreamCandidate {
  ownedWireValueWithBoundaryFailure
  borrowedView
  neverFailure
  erasedStream
  nonWireValue
}

export const fn remoteStreamIsEligible(candidate: RemoteStreamCandidate): Bool {
  return switch candidate {
    case .ownedWireValueWithBoundaryFailure: true
    case .borrowedView: false
    case .neverFailure: false
    case .erasedStream: false
    case .nonWireValue: false
  }
}

export enum StreamTerminalInput {
  end
  applicationError
  boundaryError
  consumerReset
}

export enum StreamConsumerObservation {
  none
  failure
  canceledAndDrained
}

export const fn expectedConsumerObservation(
  terminal: StreamTerminalInput,
): StreamConsumerObservation {
  return switch terminal {
    case .end: .none
    case .applicationError: .failure
    case .boundaryError: .failure
    case .consumerReset: .canceledAndDrained
  }
}

export enum StreamRouteRelation {
  sameRoute
  differentRoute
}

export enum StreamRelayPlan {
  direct
  boundedRelay
}

export const fn expectedRelayPlan(
  relation: StreamRouteRelation,
): StreamRelayPlan {
  return switch relation {
    case .sameRoute: .direct
    case .differentRoute: .boundedRelay
  }
}

export enum StreamPhase {
  opening
  open
  draining
  ended
  failed
  canceled
  protocolFailed
}

export enum StreamEvent {
  opened
  openRejected
  item
  complete
  applicationError
  boundaryError
  reset
  drainCompleted
}

export enum StreamAction {
  opened
  openFailed
  itemDelivered
  terminalEnd
  terminalFailure
  resetRequested
  discardedDuringDrain
  canceledAndDrained
  noOp
  protocolFailure
}

export struct StreamState {
  phase: StreamPhase
  deliveredItems: u64
}

export struct StreamStep {
  state: StreamState
  action: StreamAction
}

export const fn advanceStream(
  state currentState: StreamState,
  event nextEvent: StreamEvent,
): StreamStep {
  return switch (currentState.phase, nextEvent) {
    case (.opening, .opened): StreamStep(
      state: StreamState(phase: .open, deliveredItems: currentState.deliveredItems),
      action: .opened,
    )
    case (.opening, .openRejected): StreamStep(
      state: StreamState(phase: .failed, deliveredItems: currentState.deliveredItems),
      action: .openFailed,
    )
    case (.opening, .reset): StreamStep(
      state: StreamState(phase: .draining, deliveredItems: currentState.deliveredItems),
      action: .resetRequested,
    )
    case (.open, .item): StreamStep(
      state: StreamState(
        phase: .open,
        deliveredItems: currentState.deliveredItems + 1,
      ),
      action: .itemDelivered,
    )
    case (.open, .complete): StreamStep(
      state: StreamState(phase: .ended, deliveredItems: currentState.deliveredItems),
      action: .terminalEnd,
    )
    case (.open, .applicationError): StreamStep(
      state: StreamState(phase: .failed, deliveredItems: currentState.deliveredItems),
      action: .terminalFailure,
    )
    case (.open, .boundaryError): StreamStep(
      state: StreamState(phase: .failed, deliveredItems: currentState.deliveredItems),
      action: .terminalFailure,
    )
    case (.open, .reset): StreamStep(
      state: StreamState(phase: .draining, deliveredItems: currentState.deliveredItems),
      action: .resetRequested,
    )
    case (.draining, .item): StreamStep(
      state: currentState,
      action: .discardedDuringDrain,
    )
    case (.draining, .reset): StreamStep(
      state: currentState,
      action: .noOp,
    )
    case (.draining, .complete): StreamStep(
      state: currentState,
      action: .noOp,
    )
    case (.draining, .applicationError): StreamStep(
      state: currentState,
      action: .noOp,
    )
    case (.draining, .boundaryError): StreamStep(
      state: currentState,
      action: .noOp,
    )
    case (.draining, .drainCompleted): StreamStep(
      state: StreamState(phase: .canceled, deliveredItems: currentState.deliveredItems),
      action: .canceledAndDrained,
    )
    case (.ended, .reset): StreamStep(
      state: currentState,
      action: .noOp,
    )
    case (.failed, .reset): StreamStep(
      state: currentState,
      action: .noOp,
    )
    case (.canceled, .reset): StreamStep(
      state: currentState,
      action: .noOp,
    )
    case (.canceled, .drainCompleted): StreamStep(
      state: currentState,
      action: .noOp,
    )
    case (.ended, .item): StreamStep(
      state: StreamState(phase: .protocolFailed, deliveredItems: currentState.deliveredItems),
      action: .protocolFailure,
    )
    case (.failed, .item): StreamStep(
      state: StreamState(phase: .protocolFailed, deliveredItems: currentState.deliveredItems),
      action: .protocolFailure,
    )
    case (.canceled, .item): StreamStep(
      state: StreamState(phase: .protocolFailed, deliveredItems: currentState.deliveredItems),
      action: .protocolFailure,
    )
    case (.protocolFailed, _): StreamStep(
      state: currentState,
      action: .protocolFailure,
    )
    case (_, _): StreamStep(
      state: StreamState(phase: .protocolFailed, deliveredItems: currentState.deliveredItems),
      action: .protocolFailure,
    )
  }
}

export enum StreamFaultPoint {
  open
  decode
  close
}

export enum StreamFaultOutcome {
  openRejected
  boundaryFailure
  cleanupBoundary
}

export const fn expectedStreamFault(
  point: StreamFaultPoint,
): StreamFaultOutcome {
  return switch point {
    case .open: .openRejected
    case .decode: .boundaryFailure
    case .close: .cleanupBoundary
  }
}

test "stream credits require both dimensions" for expectedCreditDecision {
  let grant = StreamCreditTotals(items: 2, bytes: 128)

  expect expectedCreditDecision(
    granted: grant,
    sent: StreamSentTotals(items: 1, bytes: 64),
    nextLogicalBytes: 32,
  ) == .send
  expect expectedCreditDecision(
    granted: grant,
    sent: StreamSentTotals(items: 2, bytes: 64),
    nextLogicalBytes: 1,
  ) == .wait
  expect expectedCreditDecision(
    granted: grant,
    sent: StreamSentTotals(items: 1, bytes: 120),
    nextLogicalBytes: 16,
  ) == .wait
  expect expectedCreditDecision(
    granted: grant,
    sent: StreamSentTotals(items: 3, bytes: 64),
    nextLogicalBytes: 1,
  ) == .protocolFailure
}

test "stream credit totals never decrease" for creditUpdateIsValid {
  expect creditUpdateIsValid(
    previous: StreamCreditTotals(items: 2, bytes: 128),
    next: StreamCreditTotals(items: 4, bytes: 256),
  )
  expect !creditUpdateIsValid(
    previous: StreamCreditTotals(items: 4, bytes: 256),
    next: StreamCreditTotals(items: 3, bytes: 256),
  )
  expect !creditUpdateIsValid(
    previous: StreamCreditTotals(items: 4, bytes: 256),
    next: StreamCreditTotals(items: 4, bytes: 255),
  )
}

test "aggregate limits prevent multiplication by open streams" for aggregateGrantFits {
  let limit = StreamCreditTotals(items: 256, bytes: 16MiB)

  expect aggregateGrantFits(
    limit: limit,
    reserved: StreamCreditTotals(items: 248, bytes: 15MiB),
    requested: StreamCreditTotals(items: 8, bytes: 1MiB),
  )
  expect !aggregateGrantFits(
    limit: limit,
    reserved: StreamCreditTotals(items: 256, bytes: 16MiB),
    requested: StreamCreditTotals(items: 1, bytes: 1),
  )
}

test "remote stream keeps ownership and boundary failure explicit" for remoteStreamIsEligible {
  expect remoteStreamIsEligible(.ownedWireValueWithBoundaryFailure)
  expect !remoteStreamIsEligible(.borrowedView)
  expect !remoteStreamIsEligible(.neverFailure)
  expect !remoteStreamIsEligible(.erasedStream)
  expect !remoteStreamIsEligible(.nonWireValue)
}

test "cross-route streaming requires a bounded relay" for expectedRelayPlan {
  expect expectedRelayPlan(.sameRoute) == .direct
  expect expectedRelayPlan(.differentRoute) == .boundedRelay
}

test "normal end remains distinct from either failure" for expectedConsumerObservation {
  expect expectedConsumerObservation(.end) == .none
  expect expectedConsumerObservation(.applicationError) == .failure
  expect expectedConsumerObservation(.boundaryError) == .failure
  expect expectedConsumerObservation(.consumerReset) == .canceledAndDrained
}

test "stream terminal and reset transitions are stable" for advanceStream {
  let opening = StreamState(phase: .opening, deliveredItems: 0)
  let opened = advanceStream(state: opening, event: .opened).state
  expect opened.phase == .open

  let delivered = advanceStream(state: opened, event: .item)
  expect delivered.action == .itemDelivered
  expect delivered.state.deliveredItems == 1

  let ended = advanceStream(state: delivered.state, event: .complete)
  expect ended.action == .terminalEnd
  expect ended.state.phase == .ended

  let lateItem = advanceStream(state: ended.state, event: .item)
  expect lateItem.action == .protocolFailure
  expect lateItem.state.phase == .protocolFailed

  let reset = advanceStream(state: opened, event: .reset)
  expect reset.action == .resetRequested
  let drained = advanceStream(state: reset.state, event: .drainCompleted)
  expect drained.action == .canceledAndDrained
  expect drained.state.phase == .canceled

  let discarded = advanceStream(state: reset.state, event: .item)
  expect discarded.action == .discardedDuringDrain
  expect discarded.state.phase == .draining

  let lateTerminal = advanceStream(state: reset.state, event: .complete)
  expect lateTerminal.action == .noOp
  expect lateTerminal.state.phase == .draining
}

test "stream fault points retain opening decode and cleanup outcomes" for expectedStreamFault {
  expect expectedStreamFault(.open) == .openRejected
  expect expectedStreamFault(.decode) == .boundaryFailure
  expect expectedStreamFault(.close) == .cleanupBoundary
}
