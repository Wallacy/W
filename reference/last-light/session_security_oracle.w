// Pure oracle for wRPC channel security, establishment, and replay rejection.

enum OracleChannelProfile {
  plaintext
  tls13ServerOnly
  tls13Mutual
  quicTls13Mutual
  ipcPathOnly
  ipcPeer
}

enum ChannelDecision {
  accept
  rejectSecurity
  rejectIdentity
  rejectFreshness
}

const fn expectedChannelDecision(
  profile: OracleChannelProfile,
  peerMatchesBinding: Bool,
  freshChannelBinding: Bool,
): ChannelDecision {
  if profile in (.plaintext, .tls13ServerOnly, .ipcPathOnly) {
    return .rejectSecurity
  }

  if !peerMatchesBinding {
    return .rejectIdentity
  }

  if !freshChannelBinding {
    return .rejectFreshness
  }

  return .accept
}

enum NegotiationEvidence {
  valid
  selectionOutsideOffer
  belowDeploymentMinimum
  transcriptMismatch
  channelBindingMismatch
}

enum EstablishmentDecision {
  ready
  rejectNegotiation
  rejectDowngrade
  rejectTranscript
}

const fn expectedEstablishmentDecision(
  for evidence: NegotiationEvidence,
): EstablishmentDecision {
  return switch evidence {
    case .valid: .ready
    case .selectionOutsideOffer: .rejectNegotiation
    case .belowDeploymentMinimum: .rejectDowngrade
    case .transcriptMismatch: .rejectTranscript
    case .channelBindingMismatch: .rejectTranscript
  }
}

enum OracleSessionPhase {
  transport
  negotiating
  ready
  closed
}

enum OracleFrameFamily {
  hello
  ready
  goAway
  call
  stream
  capability
  pipeline
}

enum FrameDecision {
  acceptSession
  acceptApplication
  rejectEarlyData
  rejectPhase
}

const fn expectedFrameDecision(
  phase: OracleSessionPhase,
  frame: OracleFrameFamily,
  earlyData: Bool,
): FrameDecision {
  if earlyData {
    return .rejectEarlyData
  }

  return switch (phase, frame) {
    case (.negotiating, .hello): .acceptSession
    case (.negotiating, .ready): .acceptSession
    case (.ready, .goAway): .acceptSession
    case (.ready, .call): .acceptApplication
    case (.ready, .stream): .acceptApplication
    case (.ready, .capability): .acceptApplication
    case (.ready, .pipeline): .acceptApplication
    case (_, _): .rejectPhase
  }
}

enum SequenceDecision {
  accept
  rejectSession
  rejectDuplicate
  rejectGap
}

const fn expectedSequenceDecision(
  sessionMatches: Bool,
  expected: u64,
  received: u64,
): SequenceDecision {
  if !sessionMatches {
    return .rejectSession
  }

  if received < expected {
    return .rejectDuplicate
  }

  if received > expected {
    return .rejectGap
  }

  return .accept
}

const fn canAllocateSessionTables(
  channelAccepted: Bool,
  helloBounded: Bool,
  transcriptConfirmed: Bool,
  bilateralReady: Bool,
): Bool {
  return channelAccepted && helloBounded && transcriptConfirmed && bilateralReady
}

enum SessionLifecycleEvent {
  transportKeyUpdate
  credentialRotation
  maximumAge
  authorityRevoked
  channelReplaced
}

enum SessionLifecycleDecision {
  keepSession
  keepUntilMaximumAge
  goAwayAndDrain
  terminate
  requireNewSession
}

const fn expectedLifecycleDecision(
  for event: SessionLifecycleEvent,
): SessionLifecycleDecision {
  return switch event {
    case .transportKeyUpdate: .keepSession
    case .credentialRotation: .keepUntilMaximumAge
    case .maximumAge: .goAwayAndDrain
    case .authorityRevoked: .terminate
    case .channelReplaced: .requireNewSession
  }
}

test "network and IPC channels fail closed" for expectedChannelDecision {
  expect expectedChannelDecision(
    profile: .plaintext,
    peerMatchesBinding: true,
    freshChannelBinding: true,
  ) == .rejectSecurity

  expect expectedChannelDecision(
    profile: .tls13ServerOnly,
    peerMatchesBinding: true,
    freshChannelBinding: true,
  ) == .rejectSecurity

  expect expectedChannelDecision(
    profile: .tls13Mutual,
    peerMatchesBinding: false,
    freshChannelBinding: true,
  ) == .rejectIdentity

  expect expectedChannelDecision(
    profile: .quicTls13Mutual,
    peerMatchesBinding: true,
    freshChannelBinding: true,
  ) == .accept

  expect expectedChannelDecision(
    profile: .ipcPeer,
    peerMatchesBinding: true,
    freshChannelBinding: true,
  ) == .accept
}

test "selection and transcript reject downgrade" for expectedEstablishmentDecision {
  expect expectedEstablishmentDecision(for: .valid) == .ready
  expect expectedEstablishmentDecision(for: .selectionOutsideOffer) == .rejectNegotiation
  expect expectedEstablishmentDecision(for: .belowDeploymentMinimum) == .rejectDowngrade
  expect expectedEstablishmentDecision(for: .transcriptMismatch) == .rejectTranscript
  expect expectedEstablishmentDecision(for: .channelBindingMismatch) == .rejectTranscript
}

test "application frames require bilateral ready and 1-RTT" for expectedFrameDecision {
  expect expectedFrameDecision(
    phase: .negotiating,
    frame: .call,
    earlyData: false,
  ) == .rejectPhase

  expect expectedFrameDecision(
    phase: .ready,
    frame: .call,
    earlyData: true,
  ) == .rejectEarlyData

  expect expectedFrameDecision(
    phase: .ready,
    frame: .pipeline,
    earlyData: false,
  ) == .acceptApplication
}

test "sequence and capability scope reject replay" for expectedSequenceDecision {
  expect expectedSequenceDecision(
    sessionMatches: true,
    expected: 42,
    received: 42,
  ) == .accept

  expect expectedSequenceDecision(
    sessionMatches: true,
    expected: 42,
    received: 41,
  ) == .rejectDuplicate

  expect expectedSequenceDecision(
    sessionMatches: true,
    expected: 42,
    received: 43,
  ) == .rejectGap

  expect expectedSequenceDecision(
    sessionMatches: false,
    expected: 0,
    received: 0,
  ) == .rejectSession
}

test "tables exist only after complete establishment" for canAllocateSessionTables {
  expect canAllocateSessionTables(
    channelAccepted: true,
    helloBounded: true,
    transcriptConfirmed: true,
    bilateralReady: true,
  )

  expect !canAllocateSessionTables(
    channelAccepted: true,
    helloBounded: true,
    transcriptConfirmed: true,
    bilateralReady: false,
  )
}

test "credential and channel lifecycle preserve session scope" for expectedLifecycleDecision {
  expect expectedLifecycleDecision(for: .transportKeyUpdate) == .keepSession
  expect expectedLifecycleDecision(for: .credentialRotation) == .keepUntilMaximumAge
  expect expectedLifecycleDecision(for: .maximumAge) == .goAwayAndDrain
  expect expectedLifecycleDecision(for: .authorityRevoked) == .terminate
  expect expectedLifecycleDecision(for: .channelReplaced) == .requireNewSession
}
