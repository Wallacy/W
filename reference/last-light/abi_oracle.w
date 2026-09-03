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
  expect chooseAbiArtifact(exactArtifact: true, sourceAvailable: true, declaredBoundary: true, boundaryArtifact: true) == .exactArtifact
  expect chooseAbiArtifact(exactArtifact: false, sourceAvailable: true, declaredBoundary: true, boundaryArtifact: true) == .rebuildSource
  expect chooseAbiArtifact(exactArtifact: false, sourceAvailable: false, declaredBoundary: true, boundaryArtifact: true) == .canonicalBoundary
  expect chooseAbiArtifact(exactArtifact: false, sourceAvailable: false, declaredBoundary: false, boundaryArtifact: true) == .reject
  expect chooseAbiArtifact(exactArtifact: false, sourceAvailable: false, declaredBoundary: true, boundaryArtifact: false) == .reject
}

test "each boundary accepts only its declared carrier" for boundaryAccepts {
  expect boundaryAccepts(boundary: .internal, carrier: .wOpaque)
  expect boundaryAccepts(boundary: .wExact, carrier: .wFingerprint)
  expect boundaryAccepts(boundary: .foreignC, carrier: .cRecord)
  expect boundaryAccepts(boundary: .component, carrier: .schema)
  expect boundaryAccepts(boundary: .wire, carrier: .schema)
  expect !boundaryAccepts(boundary: .foreignC, carrier: .wFingerprint)
  expect !boundaryAccepts(boundary: .wire, carrier: .cPointer)
}
