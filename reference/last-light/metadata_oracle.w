// Pure oracle for metadata domain separation and reproducible recipe inputs.

enum MetadataDomain {
  packageManifest
  packageLock
  workspace
  buildRecipe
  toolchainPlan
  releaseEnvelope
  semanticInterface
  compilerCache
  servicePayload
}

enum MetadataEncoding {
  deterministicCbor
  wMeta1Candidate
  wWire
}

const fn expectedEncoding(for domain: MetadataDomain): MetadataEncoding {
  return switch domain {
    case .packageManifest: .deterministicCbor
    case .packageLock: .deterministicCbor
    case .workspace: .deterministicCbor
    case .buildRecipe: .deterministicCbor
    case .toolchainPlan: .deterministicCbor
    case .releaseEnvelope: .deterministicCbor
    case .semanticInterface: .wMeta1Candidate
    case .compilerCache: .wMeta1Candidate
    case .servicePayload: .wWire
  }
}

enum RecipeField {
  sourceTreeDigest
  packageLockDigest
  toolchainPlanDigest
  allowedEnvironment
  producedOutputDigest
  executorPath
  wallClock
}

enum RecipeFieldDisposition {
  declaredInput
  postBuildEvidence
  forbidden
}

const fn expectedRecipeFieldDisposition(
  for field: RecipeField,
): RecipeFieldDisposition {
  return switch field {
    case .sourceTreeDigest: .declaredInput
    case .packageLockDigest: .declaredInput
    case .toolchainPlanDigest: .declaredInput
    case .allowedEnvironment: .declaredInput
    case .producedOutputDigest: .postBuildEvidence
    case .executorPath: .forbidden
    case .wallClock: .forbidden
  }
}

test "records, interfaces, and payloads keep separate encodings" for expectedEncoding {
  expect expectedEncoding(for: .packageManifest) == .deterministicCbor
  expect expectedEncoding(for: .buildRecipe) == .deterministicCbor
  expect expectedEncoding(for: .releaseEnvelope) == .deterministicCbor
  expect expectedEncoding(for: .semanticInterface) == .wMeta1Candidate
  expect expectedEncoding(for: .compilerCache) == .wMeta1Candidate
  expect expectedEncoding(for: .servicePayload) == .wWire
}

test "a recipe records declared inputs and later evidence" for expectedRecipeFieldDisposition {
  expect expectedRecipeFieldDisposition(for: .sourceTreeDigest) == .declaredInput
  expect expectedRecipeFieldDisposition(for: .packageLockDigest) == .declaredInput
  expect expectedRecipeFieldDisposition(for: .toolchainPlanDigest) == .declaredInput
  expect expectedRecipeFieldDisposition(for: .allowedEnvironment) == .declaredInput
  expect expectedRecipeFieldDisposition(for: .producedOutputDigest) == .postBuildEvidence
  expect expectedRecipeFieldDisposition(for: .executorPath) == .forbidden
  expect expectedRecipeFieldDisposition(for: .wallClock) == .forbidden
}
