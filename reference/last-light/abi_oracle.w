// Logical oracle for exact W ABI reuse and canonical boundaries.
//
// This source does not expose object-file bytes. It records the decisions that
// the host-independent L0 reader checks before lowering or native linking.

export enum AbiArtifactChoice {
  exactArtifact
  rebuildSource
  canonicalBoundary
  reject
}

export const fn chooseAbiArtifact(
  exactArtifact: Bool,
  sourceAvailable: Bool,
  declaredBoundary: Bool,
  boundaryArtifact: Bool,
): AbiArtifactChoice {
  if exactArtifact { return .exactArtifact }
  if sourceAvailable { return .rebuildSource }
  if declaredBoundary && boundaryArtifact { return .canonicalBoundary }
  return .reject
}

export enum AbiBoundary {
  internal
  wExact
  foreignC
  component
  wire
}

export enum AbiCarrier {
  wOpaque
  wFingerprint
  cScalar
  cRecord
  cPointer
  schema
}

export const fn boundaryAccepts(
  boundary: AbiBoundary,
  carrier: AbiCarrier,
): Bool {
  return switch boundary {
    case .internal: true
    case .wExact: carrier == .wFingerprint
    case .foreignC: carrier.one(.cScalar, .cRecord, .cPointer)
    case .component: carrier == .schema
    case .wire: carrier == .schema
  }
}

// The consumer's own interface key is deliberately absent. An import compares
// its provider expectation with the provider interface that owns the symbol.
export struct AbiImportExpectation {
  symbol: String
  providerInterfaceKey: String
  semanticSignature: String
  physicalSignature: String
  representationFingerprint: String
}

export fn importMatchesProvider(
  expected: ref AbiImportExpectation,
  providerInterfaceKey: ref String,
  semanticSignature: ref String,
  physicalSignature: ref String,
  representationFingerprint: ref String,
): Bool {
  return expected.providerInterfaceKey == providerInterfaceKey &&
    expected.semanticSignature == semanticSignature &&
    expected.physicalSignature == physicalSignature &&
    expected.representationFingerprint == representationFingerprint
}

test "ABI recovery order is deterministic" for chooseAbiArtifact {
  expect chooseAbiArtifact(true, true, true, true) == .exactArtifact
  expect chooseAbiArtifact(false, true, true, true) == .rebuildSource
  expect chooseAbiArtifact(false, false, true, true) == .canonicalBoundary
  expect chooseAbiArtifact(false, false, false, true) == .reject
  expect chooseAbiArtifact(false, false, true, false) == .reject
}

test "each boundary accepts only its declared carrier" for boundaryAccepts {
  expect boundaryAccepts(.internal, .wOpaque)
  expect boundaryAccepts(.wExact, .wFingerprint)
  expect boundaryAccepts(.foreignC, .cRecord)
  expect boundaryAccepts(.component, .schema)
  expect boundaryAccepts(.wire, .schema)
  expect !boundaryAccepts(.foreignC, .wFingerprint)
  expect !boundaryAccepts(.wire, .cPointer)
}
