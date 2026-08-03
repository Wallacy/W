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
  granted: StreamCreditTotals,
  sent: StreamSentTotals,
  nextLogicalBytes: u64,
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
  previous: StreamCreditTotals,
  next: StreamCreditTotals,
): Bool {
  return next.items >= previous.items && next.bytes >= previous.bytes
}

export const fn aggregateGrantFits(
  limit: StreamCreditTotals,
  reserved: StreamCreditTotals,
  requested: StreamCreditTotals,
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
