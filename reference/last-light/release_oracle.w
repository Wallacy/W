// Pure oracle for deterministic release verification and registry metadata.

struct ReleasePolicy {
  requiredRebuilders: u16
  requiresPublicSource: Bool
  requiresTransparency: Bool
}

enum ReleaseDecision {
  published
  reproducible
  privatelyReproducible
  rejectSignature
  rejectDigest
  rejectReproduction
  rejectSourceAccess
  rejectTransparency
  rejectRevoked
  rejectYanked
}

const fn verifyRelease(
  policy: ReleasePolicy,
  maintainerThresholdMet: Bool,
  payloadDigestMatches: Bool,
  independentRebuilders: u16,
  sourcePublic: Bool,
  transparencyRecorded: Bool,
  revoked: Bool,
  yanked: Bool,
): ReleaseDecision {
  if revoked {
    return .rejectRevoked
  }

  if yanked {
    return .rejectYanked
  }

  if !maintainerThresholdMet {
    return .rejectSignature
  }

  if !payloadDigestMatches {
    return .rejectDigest
  }

  if policy.requiresTransparency && !transparencyRecorded {
    return .rejectTransparency
  }

  if independentRebuilders < policy.requiredRebuilders {
    return .published
  }

  if policy.requiresPublicSource && !sourcePublic {
    return .rejectSourceAccess
  }

  if sourcePublic {
    return .reproducible
  }

  return .privatelyReproducible
}

enum MirrorEvidence {
  listedAndMatching
  unlisted
  digestMismatch
  staleMetadata
  rollback
}

enum MirrorDecision {
  accept
  reject
}

const fn expectedMirrorDecision(for evidence: MirrorEvidence): MirrorDecision {
  return switch evidence {
    case .listedAndMatching: .accept
    case .unlisted: .reject
    case .digestMismatch: .reject
    case .staleMetadata: .reject
    case .rollback: .reject
  }
}

enum SigningRole {
  maintainer
  builder
  registry
  platform
}

const fn rolesMayShareKey(first: SigningRole, second: SigningRole): Bool {
  return first == second
}

test "release verification separates signature from reproduction" for verifyRelease {
  let publicPolicy = ReleasePolicy(
    requiredRebuilders: 2,
    requiresPublicSource: true,
    requiresTransparency: true,
  )

  expect verifyRelease(
    policy: publicPolicy,
    maintainerThresholdMet: true,
    payloadDigestMatches: true,
    independentRebuilders: 0,
    sourcePublic: true,
    transparencyRecorded: true,
    revoked: false,
    yanked: false,
  ) == .published

  expect verifyRelease(
    policy: publicPolicy,
    maintainerThresholdMet: true,
    payloadDigestMatches: true,
    independentRebuilders: 2,
    sourcePublic: true,
    transparencyRecorded: true,
    revoked: false,
    yanked: false,
  ) == .reproducible

  expect verifyRelease(
    policy: publicPolicy,
    maintainerThresholdMet: true,
    payloadDigestMatches: true,
    independentRebuilders: 2,
    sourcePublic: false,
    transparencyRecorded: true,
    revoked: false,
    yanked: false,
  ) == .rejectSourceAccess
}

test "closed source keeps a separate reproduction claim" for verifyRelease {
  let privatePolicy = ReleasePolicy(
    requiredRebuilders: 2,
    requiresPublicSource: false,
    requiresTransparency: true,
  )

  expect verifyRelease(
    policy: privatePolicy,
    maintainerThresholdMet: true,
    payloadDigestMatches: true,
    independentRebuilders: 2,
    sourcePublic: false,
    transparencyRecorded: true,
    revoked: false,
    yanked: false,
  ) == .privatelyReproducible
}

test "revocation and digest mismatch fail before installation" for verifyRelease {
  let policy = ReleasePolicy(
    requiredRebuilders: 1,
    requiresPublicSource: false,
    requiresTransparency: false,
  )

  expect verifyRelease(
    policy: policy,
    maintainerThresholdMet: true,
    payloadDigestMatches: false,
    independentRebuilders: 1,
    sourcePublic: false,
    transparencyRecorded: false,
    revoked: false,
    yanked: false,
  ) == .rejectDigest

  expect verifyRelease(
    policy: policy,
    maintainerThresholdMet: true,
    payloadDigestMatches: true,
    independentRebuilders: 1,
    sourcePublic: false,
    transparencyRecorded: false,
    revoked: true,
    yanked: false,
  ) == .rejectRevoked
}

test "a mirror is transport and not trust" for expectedMirrorDecision {
  expect expectedMirrorDecision(for: .listedAndMatching) == .accept
  expect expectedMirrorDecision(for: .unlisted) == .reject
  expect expectedMirrorDecision(for: .digestMismatch) == .reject
  expect expectedMirrorDecision(for: .staleMetadata) == .reject
  expect expectedMirrorDecision(for: .rollback) == .reject
}

test "signing roles remain separate" for rolesMayShareKey {
  expect rolesMayShareKey(.maintainer, .maintainer)
  expect !rolesMayShareKey(.maintainer, .builder)
  expect !rolesMayShareKey(.builder, .platform)
  expect !rolesMayShareKey(.registry, .platform)
}
