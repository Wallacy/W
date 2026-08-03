// Pure oracle for bootstrap.w0 closure, compiler layers, and stage convergence.

enum BootstrapStage {
  seedC
  stageA
  stageB
  stageC
  stageD
}

const fn parentStage(for stage: BootstrapStage): BootstrapStage? {
  return switch stage {
    case .seedC: .none
    case .stageA: .some(.seedC)
    case .stageB: .some(.stageA)
    case .stageC: .some(.stageB)
    case .stageD: .some(.stageC)
  }
}

const fn isSelfHosted(stage: BootstrapStage): Bool {
  return stage != .seedC
}

enum CompilerLayer {
  coreW0
  backendAdapter
  extended
}

const fn dependencyAllowed(from: CompilerLayer, to: CompilerLayer): Bool {
  return switch from {
    case .coreW0: to == .coreW0 || to == .backendAdapter
    case .backendAdapter: to == .backendAdapter
    case .extended: true
  }
}

enum BootstrapCapability {
  lexer
  parser
  typeChecker
  hir
  diagnostics
  serializer
  driver
  backendAdapter
  asyncFeature
  spawnFeature
  serviceFeature
  tensorFeature
  reflectionFeature
  inlineForeignLanguage
}

const fn belongsToW0(capability: BootstrapCapability): Bool {
  return switch capability {
    case .lexer: true
    case .parser: true
    case .typeChecker: true
    case .hir: true
    case .diagnostics: true
    case .serializer: true
    case .driver: true
    case .backendAdapter: true
    case .asyncFeature: false
    case .spawnFeature: false
    case .serviceFeature: false
    case .tensorFeature: false
    case .reflectionFeature: false
    case .inlineForeignLanguage: false
  }
}

struct BootstrapComparison {
  inputsComplete: Bool
  sourceSame: Bool
  interfaceSame: Bool
  hirSame: Bool
  diagnosticsSame: Bool
  payloadSame: Bool
  targetMetadataOnly: Bool
}

enum BootstrapVerdict {
  converged
  targetMetadataOnly
  reject
}

const fn compareStages(comparison: BootstrapComparison): BootstrapVerdict {
  if !comparison.inputsComplete {
    return .reject
  }

  if !comparison.sourceSame
    || !comparison.interfaceSame
    || !comparison.hirSame
    || !comparison.diagnosticsSame
    || !comparison.payloadSame {
    return .reject
  }

  if comparison.targetMetadataOnly {
    return .targetMetadataOnly
  }

  return .converged
}

test "bootstrap stages form one parent chain" for parentStage {
  expect parentStage(for: .seedC) == none
  expect parentStage(for: .stageA) == .some(.seedC)
  expect parentStage(for: .stageB) == .some(.stageA)
  expect parentStage(for: .stageC) == .some(.stageB)
  expect parentStage(for: .stageD) == .some(.stageC)
  expect !isSelfHosted(.seedC)
  expect isSelfHosted(.stageA)
  expect isSelfHosted(.stageD)
}

test "core W0 cannot depend on extended compiler code" for dependencyAllowed {
  expect dependencyAllowed(from: .coreW0, to: .coreW0)
  expect dependencyAllowed(from: .coreW0, to: .backendAdapter)
  expect !dependencyAllowed(from: .coreW0, to: .extended)
  expect dependencyAllowed(from: .extended, to: .coreW0)
}

test "W0 closes the compiler kernel before runtime features" for belongsToW0 {
  expect belongsToW0(.lexer)
  expect belongsToW0(.parser)
  expect belongsToW0(.typeChecker)
  expect belongsToW0(.hir)
  expect belongsToW0(.serializer)
  expect belongsToW0(.backendAdapter)
  expect !belongsToW0(.asyncFeature)
  expect !belongsToW0(.serviceFeature)
  expect !belongsToW0(.inlineForeignLanguage)
}

test "stage comparison permits only target metadata differences" for compareStages {
  let equal = BootstrapComparison(
    inputsComplete: true,
    sourceSame: true,
    interfaceSame: true,
    hirSame: true,
    diagnosticsSame: true,
    payloadSame: true,
    targetMetadataOnly: false,
  )
  expect compareStages(equal) == .converged

  let targetDifference = BootstrapComparison(
    inputsComplete: true,
    sourceSame: true,
    interfaceSame: true,
    hirSame: true,
    diagnosticsSame: true,
    payloadSame: true,
    targetMetadataOnly: true,
  )
  expect compareStages(targetDifference) == .targetMetadataOnly

  let drift = BootstrapComparison(
    inputsComplete: true,
    sourceSame: true,
    interfaceSame: true,
    hirSame: false,
    diagnosticsSame: true,
    payloadSame: true,
    targetMetadataOnly: false,
  )
  expect compareStages(drift) == .reject
}
