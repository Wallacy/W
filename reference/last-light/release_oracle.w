// Pure oracle for deterministic release verification and registry metadata.

struct ReleasePolicy {
  let requiredRebuilders: u16
  let requiresPublicSource: Bool
  let requiresTransparency: Bool
}

// The compact value models equality of a tagged cryptographic digest.
// The production record uses the selected digest algorithm and full bytes.
type EvidenceDigest = u64

struct BuilderEvidence {
  let builderIdentity: EvidenceDigest
  let operatorIdentity: EvidenceDigest
  let credentialIdentity: EvidenceDigest
  let executionRootIdentity: EvidenceDigest
}

enum IndependenceVerdict {
  independent
  sameBuilder
  sameOperator
  sameCredential
  sameExecutionRoot
}

const fn compareBuilderIndependence(
  first left: BuilderEvidence,
  second right: BuilderEvidence,
): IndependenceVerdict {
  if left.builderIdentity == right.builderIdentity {
    return .sameBuilder
  }

  if left.operatorIdentity == right.operatorIdentity {
    return .sameOperator
  }

  if left.credentialIdentity == right.credentialIdentity {
    return .sameCredential
  }

  if left.executionRootIdentity == right.executionRootIdentity {
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

struct ProvenanceEvidence {
  let releaseRecipeDigest: EvidenceDigest
  let attestedRecipeDigest: EvidenceDigest
  let recipeToolchainDigest: EvidenceDigest
  let attestedToolchainDigest: EvidenceDigest
  let artifactDigest: EvidenceDigest
  let platformArtifactDigest: EvidenceDigest
  let maintainerIdentity: EvidenceDigest
  let builderIdentity: EvidenceDigest
  let toolchainProviderIdentity: EvidenceDigest
  let platformSignerIdentity: EvidenceDigest
}

enum ProvenanceVerdict {
  verified
  recipeMismatch
  toolchainMismatch
  artifactMismatch
  roleCollision
}

const fn verifyProvenance(evidence: ProvenanceEvidence): ProvenanceVerdict {
  if evidence.maintainerIdentity == evidence.builderIdentity
    || evidence.maintainerIdentity == evidence.toolchainProviderIdentity
    || evidence.maintainerIdentity == evidence.platformSignerIdentity
    || evidence.builderIdentity == evidence.toolchainProviderIdentity
    || evidence.builderIdentity == evidence.platformSignerIdentity
    || evidence.toolchainProviderIdentity == evidence.platformSignerIdentity {
    return .roleCollision
  }

  if evidence.releaseRecipeDigest != evidence.attestedRecipeDigest {
    return .recipeMismatch
  }

  if evidence.recipeToolchainDigest != evidence.attestedToolchainDigest {
    return .toolchainMismatch
  }

  if evidence.artifactDigest != evidence.platformArtifactDigest {
    return .artifactMismatch
  }

  return .verified
}

struct BuildEvidence {
  let inputsComplete: Bool
  let outputsComplete: Bool
  let sourceTreeDigest: EvidenceDigest
  let packageLockDigest: EvidenceDigest
  let recipeDigest: EvidenceDigest
  let toolchainDigest: EvidenceDigest
  let targetDigest: EvidenceDigest
  let runtimeClosureDigest: EvidenceDigest
  let environmentProjectionDigest: EvidenceDigest
  let payloadDigest: EvidenceDigest
  let artifactDigest: EvidenceDigest
  let builderIdentity: EvidenceDigest
}

enum ReproductionVerdict {
  reproducible
  incompleteEvidence
  inputMismatch
  artifactMismatch
}

const fn compareBuildEvidence(
  first left: BuildEvidence,
  second right: BuildEvidence,
): ReproductionVerdict {
  if !left.inputsComplete || !right.inputsComplete {
    return .incompleteEvidence
  }

  if !left.outputsComplete || !right.outputsComplete {
    return .incompleteEvidence
  }

  if left.sourceTreeDigest != right.sourceTreeDigest
    || left.packageLockDigest != right.packageLockDigest
    || left.recipeDigest != right.recipeDigest
    || left.toolchainDigest != right.toolchainDigest
    || left.targetDigest != right.targetDigest
    || left.runtimeClosureDigest != right.runtimeClosureDigest
    || left.environmentProjectionDigest != right.environmentProjectionDigest {
    return .inputMismatch
  }

  if left.payloadDigest != right.payloadDigest
    || left.artifactDigest != right.artifactDigest {
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
  provenance: ProvenanceVerdict,
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

  if provenance != .verified {
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
    provenance: .verified,
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
    provenance: .verified,
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
    provenance: .verified,
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
    provenance: .verified,
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
    provenance: .verified,
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
    provenance: .verified,
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
  expect rolesMayShareKey(first: .maintainer, second: .maintainer)
  expect !rolesMayShareKey(first: .maintainer, second: .builder)
  expect !rolesMayShareKey(first: .builder, second: .platform)
  expect !rolesMayShareKey(first: .registry, second: .platform)
}

const fn builderEvidence(builderIdentity identity: EvidenceDigest): BuilderEvidence {
  return BuilderEvidence(
    builderIdentity: identity,
    operatorIdentity: identity + 100,
    credentialIdentity: identity + 200,
    executionRootIdentity: identity + 300,
  )
}

const fn validProvenance(): ProvenanceEvidence {
  return ProvenanceEvidence(
    releaseRecipeDigest: 401,
    attestedRecipeDigest: 401,
    recipeToolchainDigest: 402,
    attestedToolchainDigest: 402,
    artifactDigest: 403,
    platformArtifactDigest: 403,
    maintainerIdentity: 404,
    builderIdentity: 405,
    toolchainProviderIdentity: 406,
    platformSignerIdentity: 407,
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

test "provenance links recipe, toolchain, artifact, and platform envelope" for verifyProvenance {
  let valid = validProvenance()
  expect verifyProvenance(evidence: valid) == .verified

  var changedRecipe = valid
  changedRecipe.attestedRecipeDigest = 501
  expect verifyProvenance(evidence: changedRecipe) == .recipeMismatch

  var changedToolchain = valid
  changedToolchain.attestedToolchainDigest = 502
  expect verifyProvenance(evidence: changedToolchain) == .toolchainMismatch

  var changedArtifact = valid
  changedArtifact.platformArtifactDigest = 503
  expect verifyProvenance(evidence: changedArtifact) == .artifactMismatch

  var reusedRole = valid
  reusedRole.platformSignerIdentity = valid.builderIdentity
  expect verifyProvenance(evidence: reusedRole) == .roleCollision
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
    provenance: .verified,
    sourcePublic: true,
    transparencyRecorded: true,
    revoked: false,
    yanked: false,
  ) == .rejectReproduction
}

test "a provenance mismatch blocks a verified release" for verifyRelease {
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
    quorum: .verified,
    provenance: .toolchainMismatch,
    sourcePublic: true,
    transparencyRecorded: true,
    revoked: false,
    yanked: false,
  ) == .rejectReproduction
}

const fn completeEvidence(builderIdentity identity: EvidenceDigest): BuildEvidence {
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
    builderIdentity: identity,
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
