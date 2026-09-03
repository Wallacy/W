// Pure recovery oracle for the restaurant coordinator and its durable effects.

export enum ServiceFaultPoint {
  beforeEnvelopeCommit
  beforeInputCommit
  beforeEffectDispatch
  afterEffectDispatch
  afterOutcomeCommit
  afterDelivery
}

export enum RecoveryEffectPolicy {
  repeatable
  idempotent
  transactional
  atMostOnce
}

export enum ProviderDecision {
  committed
  aborted
  unknown
}

export enum ServiceRecoveryAction {
  cleanupWithoutExecution
  retryAttempt
  replayTurn
  resolveCommitted
  resolveAborted
  replayOutcome
  returnUnknownOutcome
}

export const fn serviceRecoveryAction(
  point: ServiceFaultPoint,
  policy: RecoveryEffectPolicy,
  provider: ProviderDecision,
): ServiceRecoveryAction {
  return switch point {
    case .beforeEnvelopeCommit: .cleanupWithoutExecution
    case .beforeInputCommit: .retryAttempt
    case .beforeEffectDispatch: .replayTurn
    case .afterOutcomeCommit: .replayOutcome
    case .afterDelivery: .replayOutcome
    case .afterEffectDispatch: switch policy {
      case .repeatable: .retryAttempt
      case .idempotent: .retryAttempt
      case .atMostOnce: .returnUnknownOutcome
      case .transactional: switch provider {
        case .committed: .resolveCommitted
        case .aborted: .resolveAborted
        case .unknown: .returnUnknownOutcome
      }
    }
  }
}

export enum DedupRecordState {
  absent
  active
  committed
  tombstone
}

export enum DedupDecision {
  execute
  awaitExisting
  replayOutcome
  effectConflict
  retentionExpired
}

export const fn serviceDedupDecision(
  state: DedupRecordState,
  sameIdentity: Bool,
  withinRetention: Bool,
): DedupDecision {
  guard sameIdentity else { return .effectConflict }
  guard withinRetention else { return .retentionExpired }

  return switch state {
    case .absent: .execute
    case .active: .awaitExisting
    case .committed: .replayOutcome
    case .tombstone: .replayOutcome
  }
}

export struct RecoveryMailboxTicket {
  let sender: u32
  let ordinal: u64
}

export const fn mayStartRecoveryTurn(
  candidate: RecoveryMailboxTicket,
  earliestForSender: RecoveryMailboxTicket,
  anotherTurnActive: Bool,
): Bool {
  return !anotherTurnActive
    && candidate.sender == earliestForSender.sender
    && candidate.ordinal == earliestForSender.ordinal
}

export const fn acceptsRecoveryCompletion(
  currentGeneration activeGeneration: u64,
  receiptGeneration completedGeneration: u64,
): Bool {
  return activeGeneration == completedGeneration
}

test "recovery distinguishes a retry from an unknown at-most-once effect" {
  expect serviceRecoveryAction(
    point: .afterEffectDispatch,
    policy: .idempotent,
    provider: .unknown,
  ) == .retryAttempt

  expect serviceRecoveryAction(
    point: .afterEffectDispatch,
    policy: .atMostOnce,
    provider: .unknown,
  ) == .returnUnknownOutcome
}

test "a committed order outcome is replayed without another turn" {
  expect serviceRecoveryAction(
    point: .afterOutcomeCommit,
    policy: .atMostOnce,
    provider: .committed,
  ) == .replayOutcome

  expect serviceDedupDecision(
    state: .committed,
    sameIdentity: true,
    withinRetention: true,
  ) == .replayOutcome
}

test "mailbox FIFO is local to the sender" {
  let first = RecoveryMailboxTicket(sender: 7, ordinal: 41)
  let later = RecoveryMailboxTicket(sender: 7, ordinal: 42)

  expect mayStartRecoveryTurn(
    candidate: first,
    earliestForSender: first,
    anotherTurnActive: false,
  )
  expect !mayStartRecoveryTurn(
    candidate: later,
    earliestForSender: first,
    anotherTurnActive: false,
  )
}

test "a stale completion cannot cross an instance generation" {
  expect acceptsRecoveryCompletion(currentGeneration: 12, receiptGeneration: 12)
  expect !acceptsRecoveryCompletion(currentGeneration: 12, receiptGeneration: 11)
}

// Compile-fail assays:
// serviceDedupDecision(.active, sameIdentity: false, withinRetention: true)
// retry(effectId: connection.sequence) // Transport sequence is not EffectId.
// persist(taskFrame) // Recovery persists values, not frames or loans.
