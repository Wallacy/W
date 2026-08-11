// Pure oracle for service routing, commit gates, pipelines, and schema changes.

enum OraclePlacement {
  sameUnit
  wasmComponent
  sameHost
  remoteHost
}

enum OracleLink {
  local
  component
  wrpcIpc
  wrpcNetwork
  foreign
}

const fn expectedLink(for placement: OraclePlacement): OracleLink {
  return switch placement {
    case .sameUnit: .local
    case .wasmComponent: .component
    case .sameHost: .wrpcIpc
    case .remoteHost: .wrpcNetwork
  }
}

enum LinkStack {
  mailboxThunk
  componentAbi
  wrpcSessionCodecTransport
  foreignAdapter
}

const fn expectedStack(for link: OracleLink): LinkStack {
  return switch link {
    case .local: .mailboxThunk
    case .component: .componentAbi
    case .wrpcIpc: .wrpcSessionCodecTransport
    case .wrpcNetwork: .wrpcSessionCodecTransport
    case .foreign: .foreignAdapter
  }
}

// W-1242/W-1243: core routes are nominal and adapter authorities are lock-fixed.
enum ServiceResolutionSource {
  nominalImport
  typedBinding
  runtimeName
}

const fn coreResolverAccepts(_ source: ServiceResolutionSource): Bool {
  return source.one(.nominalImport, .typedBinding)
}

enum AdapterAuthority {
  toolchainLock
  deploymentLock
  productLock
  runtimeRegistry
}

const fn coreAdapterAccepts(_ authority: AdapterAuthority): Bool {
  return authority.one(.toolchainLock, .deploymentLock, .productLock)
}

enum CommitEvidence {
  noDependencies
  confirmed
  failed
  uncertain
}

enum GateOutcome {
  delivered
  commitFailed
  unknownOutcome
}

const fn expectedGateOutcome(for evidence: CommitEvidence): GateOutcome {
  return switch evidence {
    case .noDependencies: .delivered
    case .confirmed: .delivered
    case .failed: .commitFailed
    case .uncertain: .unknownOutcome
  }
}

enum PipelineCapabilitySettlement {
  absent
  transferred
  released
}

enum PipelineBodyCase {
  linear
  diamond
  fanOut
  forwardReference
  runtimeBranch
  innerAwait
  borrowedInput
  noDependentEdge
}

enum PipelineBodyCheck {
  accepted
  rejected
  warning
}

const fn expectedPipelineBodyCheck(
  for body: PipelineBodyCase,
): PipelineBodyCheck {
  return switch body {
    case .linear: .accepted
    case .diamond: .accepted
    case .fanOut: .accepted
    case .forwardReference: .rejected
    case .runtimeBranch: .rejected
    case .innerAwait: .rejected
    case .borrowedInput: .rejected
    case .noDependentEdge: .warning
  }
}

enum PipelineNodeOutcome {
  success
  applicationError
  boundaryError
  unknownOutcome
}

enum PipelineAggregate {
  success
  applicationError
  boundaryError
  pipelineUnknown
}

const fn expectedPipelineAggregate(
  named first: PipelineNodeOutcome,
  named second: PipelineNodeOutcome,
): PipelineAggregate {
  return switch (first, second) {
    case (.unknownOutcome, _): .pipelineUnknown
    case (_, .unknownOutcome): .pipelineUnknown
    case (.applicationError, _): .applicationError
    case (.boundaryError, _): .boundaryError
    case (.success, .applicationError): .applicationError
    case (.success, .boundaryError): .boundaryError
    case (.success, .success): .success
  }
}

const fn expectedCapabilitySettlement(
  named created: Bool,
  named selected: Bool,
  named pipelineSucceeded: Bool,
): PipelineCapabilitySettlement {
  return switch (created, selected, pipelineSucceeded) {
    case (false, _, _): .absent
    case (true, true, true): .transferred
    case (true, _, _): .released
  }
}

enum ServiceSchemaChange {
  addOptionalOutput
  addOutputEnumCase
  addBaseCaseExcludedBySubset
  renameWithStableId
  reuseRemovedId
  narrowInput
  expandOutput
}

enum SchemaCompatibility {
  compatible
  incompatible
}

const fn expectedCompatibility(
  for change: ServiceSchemaChange,
): SchemaCompatibility {
  return switch change {
    case .addOptionalOutput: .compatible
    case .addOutputEnumCase: .incompatible
    case .addBaseCaseExcludedBySubset: .compatible
    case .renameWithStableId: .compatible
    case .reuseRemovedId: .incompatible
    case .narrowInput: .incompatible
    case .expandOutput: .incompatible
  }
}

test "placement selects one service link layer" for expectedLink {
  expect expectedLink(for: .sameUnit) == .local
  expect expectedLink(for: .wasmComponent) == .component
  expect expectedLink(for: .sameHost) == .wrpcIpc
  expect expectedLink(for: .remoteHost) == .wrpcNetwork
}

test "each link keeps its own implementation layer" for expectedStack {
  expect expectedStack(for: .local) == .mailboxThunk
  expect expectedStack(for: .component) == .componentAbi
  expect expectedStack(for: .wrpcIpc) == .wrpcSessionCodecTransport
  expect expectedStack(for: .wrpcNetwork) == .wrpcSessionCodecTransport
  expect expectedStack(for: .foreign) == .foreignAdapter
}

test "service resolution and adapters stay fixed before startup" {
  expect coreResolverAccepts(.nominalImport)
  expect coreResolverAccepts(.typedBinding)
  expect !coreResolverAccepts(.runtimeName)
  expect coreAdapterAccepts(.toolchainLock)
  expect coreAdapterAccepts(.deploymentLock)
  expect coreAdapterAccepts(.productLock)
  expect !coreAdapterAccepts(.runtimeRegistry)
}

test "a commit failure differs from missing evidence" for expectedGateOutcome {
  expect expectedGateOutcome(for: .confirmed) == .delivered
  expect expectedGateOutcome(for: .failed) == .commitFailed
  expect expectedGateOutcome(for: .uncertain) == .unknownOutcome
}

test "an orphan oven lease is released" for expectedCapabilitySettlement {
  expect expectedCapabilitySettlement(
    created: true,
    selected: true,
    pipelineSucceeded: true,
  ) == .transferred

  expect expectedCapabilitySettlement(
    created: true,
    selected: true,
    pipelineSucceeded: false,
  ) == .released

  expect expectedCapabilitySettlement(
    created: true,
    selected: false,
    pipelineSucceeded: true,
  ) == .released
}

test "a pipeline body is a static dependent call graph" for expectedPipelineBodyCheck {
  expect expectedPipelineBodyCheck(for: .linear) == .accepted
  expect expectedPipelineBodyCheck(for: .diamond) == .accepted
  expect expectedPipelineBodyCheck(for: .fanOut) == .accepted
  expect expectedPipelineBodyCheck(for: .forwardReference) == .rejected
  expect expectedPipelineBodyCheck(for: .runtimeBranch) == .rejected
  expect expectedPipelineBodyCheck(for: .innerAwait) == .rejected
  expect expectedPipelineBodyCheck(for: .borrowedInput) == .rejected
  expect expectedPipelineBodyCheck(for: .noDependentEdge) == .warning
}

test "an uncertain node dominates pipeline error selection" for expectedPipelineAggregate {
  expect expectedPipelineAggregate(
    first: .applicationError,
    second: .unknownOutcome,
  ) == .pipelineUnknown

  expect expectedPipelineAggregate(
    first: .unknownOutcome,
    second: .boundaryError,
  ) == .pipelineUnknown

  expect expectedPipelineAggregate(
    first: .applicationError,
    second: .boundaryError,
  ) == .applicationError
}

test "service evolution is directional" for expectedCompatibility {
  expect expectedCompatibility(for: .addOptionalOutput) == .compatible
  expect expectedCompatibility(for: .addOutputEnumCase) == .incompatible
  expect expectedCompatibility(for: .addBaseCaseExcludedBySubset) == .compatible
  expect expectedCompatibility(for: .renameWithStableId) == .compatible
  expect expectedCompatibility(for: .reuseRemovedId) == .incompatible
}
