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

const fn dependencyAllowed(from source: CompilerLayer, to target: CompilerLayer): Bool {
  return switch source {
    case .coreW0: target == .coreW0 || target == .backendAdapter
    case .backendAdapter: target == .backendAdapter
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
  let inputsComplete: Bool
  let sourceSame: Bool
  let interfaceSame: Bool
  let hirSame: Bool
  let diagnosticsSame: Bool
  let payloadSame: Bool
  let targetMetadataOnly: Bool
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
  expect !isSelfHosted(stage: .seedC)
  expect isSelfHosted(stage: .stageA)
  expect isSelfHosted(stage: .stageD)
}

test "core W0 cannot depend on extended compiler code" for dependencyAllowed {
  expect dependencyAllowed(from: .coreW0, to: .coreW0)
  expect dependencyAllowed(from: .coreW0, to: .backendAdapter)
  expect !dependencyAllowed(from: .coreW0, to: .extended)
  expect dependencyAllowed(from: .extended, to: .coreW0)
}

test "W0 closes the compiler kernel before runtime features" for belongsToW0 {
  expect belongsToW0(capability: .lexer)
  expect belongsToW0(capability: .parser)
  expect belongsToW0(capability: .typeChecker)
  expect belongsToW0(capability: .hir)
  expect belongsToW0(capability: .serializer)
  expect belongsToW0(capability: .backendAdapter)
  expect !belongsToW0(capability: .asyncFeature)
  expect !belongsToW0(capability: .serviceFeature)
  expect !belongsToW0(capability: .inlineForeignLanguage)
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
  expect compareStages(comparison: equal) == .converged

  let targetDifference = BootstrapComparison(
    inputsComplete: true,
    sourceSame: true,
    interfaceSame: true,
    hirSame: true,
    diagnosticsSame: true,
    payloadSame: true,
    targetMetadataOnly: true,
  )
  expect compareStages(comparison: targetDifference) == .targetMetadataOnly

  let drift = BootstrapComparison(
    inputsComplete: true,
    sourceSame: true,
    interfaceSame: true,
    hirSame: false,
    diagnosticsSame: true,
    payloadSame: true,
    targetMetadataOnly: false,
  )
  expect compareStages(comparison: drift) == .reject
}
