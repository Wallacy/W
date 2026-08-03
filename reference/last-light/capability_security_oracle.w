// Pure oracle for wRPC root grants, attenuation, delegation, and revocation.

enum CapabilityOrigin {
  deploymentBinding
  explicitPayload
  authenticatedPeer
  guessedIndex
  opaqueBytes
  unknownField
}

enum GrantDecision {
  grant
  rejectNoAuthority
  rejectOpaqueAuthority
}

const fn expectedGrantDecision(for origin: CapabilityOrigin): GrantDecision {
  return switch origin {
    case .deploymentBinding: .grant
    case .explicitPayload: .grant
    case .authenticatedPeer: .rejectNoAuthority
    case .guessedIndex: .rejectNoAuthority
    case .opaqueBytes: .rejectOpaqueAuthority
    case .unknownField: .rejectOpaqueAuthority
  }
}

enum InterfaceProjection {
  same
  narrower
  wider
  unrelated
}

enum ProjectionDecision {
  grant
  rejectAmplification
}

const fn expectedProjectionDecision(
  for projection: InterfaceProjection,
): ProjectionDecision {
  return switch projection {
    case .same: .grant
    case .narrower: .grant
    case .wider: .rejectAmplification
    case .unrelated: .rejectAmplification
  }
}

enum CallAuthorityEvidence {
  valid
  wrongSession
  unknownIndex
  operationOutsideInterface
  staleGeneration
  notAdmittedBeforeRevoke
  admittedBeforeRevoke
}

enum CapabilityCallDecision {
  admit
  completeAdmitted
  unauthorized
}

const fn expectedCapabilityCallDecision(
  for evidence: CallAuthorityEvidence,
): CapabilityCallDecision {
  return switch evidence {
    case .valid: .admit
    case .admittedBeforeRevoke: .completeAdmitted
    case .wrongSession: .unauthorized
    case .unknownIndex: .unauthorized
    case .operationOutsideInterface: .unauthorized
    case .staleGeneration: .unauthorized
    case .notAdmittedBeforeRevoke: .unauthorized
  }
}

enum CapabilityKind {
  bindingRoot
  derivedResource
}

enum RestartDecision {
  resolveActiveGeneration
  rejectStaleGeneration
}

const fn expectedRestartDecision(for kind: CapabilityKind): RestartDecision {
  return switch kind {
    case .bindingRoot: .resolveActiveGeneration
    case .derivedResource: .rejectStaleGeneration
  }
}

struct CapabilityLimits {
  importSlots: u32
  exportSlots: u32
  ordinalsPerFrame: u16
  entriesPerPipeline: u16
}

const fn fitsCapabilityLimits(
  limits: CapabilityLimits,
  imports: u32,
  exports: u32,
  ordinals: u16,
  pipelineEntries: u16,
): Bool {
  return imports <= limits.importSlots
    && exports <= limits.exportSlots
    && ordinals <= limits.ordinalsPerFrame
    && pipelineEntries <= limits.entriesPerPipeline
}

test "identity and guessed indexes do not create authority" for expectedGrantDecision {
  expect expectedGrantDecision(for: .deploymentBinding) == .grant
  expect expectedGrantDecision(for: .explicitPayload) == .grant
  expect expectedGrantDecision(for: .authenticatedPeer) == .rejectNoAuthority
  expect expectedGrantDecision(for: .guessedIndex) == .rejectNoAuthority
  expect expectedGrantDecision(for: .opaqueBytes) == .rejectOpaqueAuthority
  expect expectedGrantDecision(for: .unknownField) == .rejectOpaqueAuthority
}

test "an observer cannot recover oven lease operations" for expectedProjectionDecision {
  expect expectedProjectionDecision(for: .same) == .grant
  expect expectedProjectionDecision(for: .narrower) == .grant
  expect expectedProjectionDecision(for: .wider) == .rejectAmplification
  expect expectedProjectionDecision(for: .unrelated) == .rejectAmplification
}

test "revocation closes admission without rollback" for expectedCapabilityCallDecision {
  expect expectedCapabilityCallDecision(for: .valid) == .admit
  expect expectedCapabilityCallDecision(for: .admittedBeforeRevoke) == .completeAdmitted
  expect expectedCapabilityCallDecision(for: .notAdmittedBeforeRevoke) == .unauthorized
  expect expectedCapabilityCallDecision(for: .operationOutsideInterface) == .unauthorized
  expect expectedCapabilityCallDecision(for: .wrongSession) == .unauthorized
}

test "resource capabilities do not rebind after restart" for expectedRestartDecision {
  expect expectedRestartDecision(for: .bindingRoot) == .resolveActiveGeneration
  expect expectedRestartDecision(for: .derivedResource) == .rejectStaleGeneration
}

test "capability tables remain bounded" for fitsCapabilityLimits {
  let limits = CapabilityLimits(
    importSlots: 2_048,
    exportSlots: 2_048,
    ordinalsPerFrame: 64,
    entriesPerPipeline: 128,
  )

  expect fitsCapabilityLimits(
    limits: limits,
    imports: 2_048,
    exports: 2_048,
    ordinals: 64,
    pipelineEntries: 128,
  )

  expect !fitsCapabilityLimits(
    limits: limits,
    imports: 2_048,
    exports: 2_048,
    ordinals: 65,
    pipelineEntries: 128,
  )
}
