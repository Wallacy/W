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
}

const fn expectedLink(for placement: OraclePlacement): OracleLink {
  return switch placement {
    case .sameUnit: .local
    case .wasmComponent: .component
    case .sameHost: .wrpcIpc
    case .remoteHost: .wrpcNetwork
  }
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

const fn expectedCapabilitySettlement(
  created: Bool,
  selected: Bool,
  pipelineSucceeded: Bool,
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

test "service evolution is directional" for expectedCompatibility {
  expect expectedCompatibility(for: .addOptionalOutput) == .compatible
  expect expectedCompatibility(for: .addOutputEnumCase) == .incompatible
  expect expectedCompatibility(for: .addBaseCaseExcludedBySubset) == .compatible
  expect expectedCompatibility(for: .renameWithStableId) == .compatible
  expect expectedCompatibility(for: .reuseRemovedId) == .incompatible
}
