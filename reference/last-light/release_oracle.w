// Pure oracle for deterministic release verification and registry metadata.

struct ReleasePolicy {
  requiredRebuilders: u16
  requiresPublicSource: Bool
  requiresTransparency: Bool
}

// The compact value models equality of a tagged cryptographic digest.
// The production record uses the selected digest algorithm and full bytes.
type EvidenceDigest = u64

struct BuilderEvidence {
  builderIdentity: EvidenceDigest
  operatorIdentity: EvidenceDigest
  credentialIdentity: EvidenceDigest
  executionRootIdentity: EvidenceDigest
}

enum IndependenceVerdict {
  independent
  sameBuilder
  sameOperator
  sameCredential
  sameExecutionRoot
}

const fn compareBuilderIndependence(
  first: BuilderEvidence,
  second: BuilderEvidence,
): IndependenceVerdict {
  if first.builderIdentity == second.builderIdentity {
    return .sameBuilder
  }

  if first.operatorIdentity == second.operatorIdentity {
    return .sameOperator
  }

  if first.credentialIdentity == second.credentialIdentity {
    return .sameCredential
  }

  if first.executionRootIdentity == second.executionRootIdentity {
    return .sameExecutionRoot
  }

  return .independent
}

enum QuorumDecision {
  verified
  insufficientEvidence
  duplicateBuilder
  duplicateOperator
  duplicateCredential
  duplicateExecutionRoot
}

struct BuildEvidence {
  inputsComplete: Bool
  outputsComplete: Bool
  sourceTreeDigest: EvidenceDigest
  packageLockDigest: EvidenceDigest
  recipeDigest: EvidenceDigest
  toolchainDigest: EvidenceDigest
  targetDigest: EvidenceDigest
  runtimeClosureDigest: EvidenceDigest
  environmentProjectionDigest: EvidenceDigest
  payloadDigest: EvidenceDigest
  artifactDigest: EvidenceDigest
  builderIdentity: EvidenceDigest
}

enum ReproductionVerdict {
  reproducible
  incompleteEvidence
  inputMismatch
  artifactMismatch
}

const fn compareBuildEvidence(
  first: BuildEvidence,
  second: BuildEvidence,
): ReproductionVerdict {
  if !first.inputsComplete || !second.inputsComplete {
    return .incompleteEvidence
  }

  if !first.outputsComplete || !second.outputsComplete {
    return .incompleteEvidence
  }

  if first.sourceTreeDigest != second.sourceTreeDigest
    || first.packageLockDigest != second.packageLockDigest
    || first.recipeDigest != second.recipeDigest
    || first.toolchainDigest != second.toolchainDigest
    || first.targetDigest != second.targetDigest
    || first.runtimeClosureDigest != second.runtimeClosureDigest
    || first.environmentProjectionDigest != second.environmentProjectionDigest {
    return .inputMismatch
  }

  if first.payloadDigest != second.payloadDigest
    || first.artifactDigest != second.artifactDigest {
    return .artifactMismatch
  }

  // Independent builder identities are evidence of quorum, not build inputs.
  return .reproducible
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
  quorum: QuorumDecision,
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

  if quorum != .verified {
    return .rejectReproduction
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
    quorum: .insufficientEvidence,
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
    quorum: .verified,
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
    quorum: .verified,
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
    quorum: .verified,
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
    quorum: .insufficientEvidence,
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
    quorum: .insufficientEvidence,
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

const fn builderEvidence(builderIdentity: EvidenceDigest): BuilderEvidence {
  return BuilderEvidence(
    builderIdentity: builderIdentity,
    operatorIdentity: builderIdentity + 100,
    credentialIdentity: builderIdentity + 200,
    executionRootIdentity: builderIdentity + 300,
  )
}

test "quorum requires distinct builders, operators, credentials, and roots" for compareBuilderIndependence {
  let first = builderEvidence(builderIdentity: 1)
  let second = builderEvidence(builderIdentity: 2)
  expect compareBuilderIndependence(first: first, second: second) == .independent

  let sameBuilder = builderEvidence(builderIdentity: 1)
  expect compareBuilderIndependence(first: first, second: sameBuilder) == .sameBuilder

  var sameOperator = builderEvidence(builderIdentity: 3)
  sameOperator.operatorIdentity = first.operatorIdentity
  expect compareBuilderIndependence(first: first, second: sameOperator) == .sameOperator

  var sameCredential = builderEvidence(builderIdentity: 4)
  sameCredential.credentialIdentity = first.credentialIdentity
  expect compareBuilderIndependence(first: first, second: sameCredential) == .sameCredential

  var sameRoot = builderEvidence(builderIdentity: 5)
  sameRoot.executionRootIdentity = first.executionRootIdentity
  expect compareBuilderIndependence(first: first, second: sameRoot) == .sameExecutionRoot
}

test "a counted but non-independent quorum cannot claim reproduction" for verifyRelease {
  let policy = ReleasePolicy(
    requiredRebuilders: 2,
    requiresPublicSource: true,
    requiresTransparency: true,
  )

  expect verifyRelease(
    policy: policy,
    maintainerThresholdMet: true,
    payloadDigestMatches: true,
    independentRebuilders: 2,
    quorum: .duplicateOperator,
    sourcePublic: true,
    transparencyRecorded: true,
    revoked: false,
    yanked: false,
  ) == .rejectReproduction
}

const fn completeEvidence(builderIdentity: EvidenceDigest): BuildEvidence {
  return BuildEvidence(
    inputsComplete: true,
    outputsComplete: true,
    sourceTreeDigest: 11,
    packageLockDigest: 12,
    recipeDigest: 13,
    toolchainDigest: 14,
    targetDigest: 15,
    runtimeClosureDigest: 16,
    environmentProjectionDigest: 17,
    payloadDigest: 18,
    artifactDigest: 19,
    builderIdentity: builderIdentity,
  )
}

test "reproduction compares declared inputs and complete outputs" for compareBuildEvidence {
  let first = completeEvidence(builderIdentity: 20)
  let independent = completeEvidence(builderIdentity: 21)

  expect compareBuildEvidence(first: first, second: independent) == .reproducible
}

test "same bytes with a different recipe are not reproducible" for compareBuildEvidence {
  let first = completeEvidence(builderIdentity: 30)
  var changedRecipe = completeEvidence(builderIdentity: 31)
  changedRecipe.recipeDigest = 133

  expect compareBuildEvidence(first: first, second: changedRecipe) == .inputMismatch
}

test "matching inputs with different bytes report an artifact mismatch" for compareBuildEvidence {
  let first = completeEvidence(builderIdentity: 40)
  var changedArtifact = completeEvidence(builderIdentity: 41)
  changedArtifact.payloadDigest = 118
  changedArtifact.artifactDigest = 119

  expect compareBuildEvidence(first: first, second: changedArtifact) == .artifactMismatch
}

test "missing evidence cannot claim reproduction" for compareBuildEvidence {
  var incomplete = completeEvidence(builderIdentity: 50)
  let complete = completeEvidence(builderIdentity: 51)
  incomplete.inputsComplete = false

  expect compareBuildEvidence(first: incomplete, second: complete) == .incompleteEvidence
}
