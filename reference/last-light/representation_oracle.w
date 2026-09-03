// Representation policy oracle for memory and ABI boundaries.

enum RepresentationBoundary {
  internal
  wExact
  foreignC
  wire
  persisted
  capability
}

enum RepresentationChoice {
  explicitTag
  provenNiche
  lowBit
  highBit
  nativeCarrier
}

const fn baselineAllows(
  boundary: RepresentationBoundary,
  choice: RepresentationChoice,
): Bool {
  return switch choice {
    case .explicitTag: true
    case .provenNiche:
      boundary != .foreignC && boundary != .wire && boundary != .persisted
    case .lowBit: boundary == .internal
    case .highBit: false
    case .nativeCarrier: boundary == .foreignC || boundary == .capability
  }
}

test "representation follows the boundary" for baselineAllows {
  expect baselineAllows(boundary: .internal, choice: .explicitTag)
  expect baselineAllows(boundary: .internal, choice: .provenNiche)
  expect baselineAllows(boundary: .internal, choice: .lowBit)
  expect !baselineAllows(boundary: .internal, choice: .highBit)

  expect baselineAllows(boundary: .wExact, choice: .provenNiche)
  expect !baselineAllows(boundary: .wExact, choice: .lowBit)
  expect !baselineAllows(boundary: .foreignC, choice: .provenNiche)
  expect !baselineAllows(boundary: .foreignC, choice: .lowBit)
  expect !baselineAllows(boundary: .wire, choice: .provenNiche)
  expect !baselineAllows(boundary: .persisted, choice: .lowBit)
  expect baselineAllows(boundary: .capability, choice: .explicitTag)
  expect baselineAllows(boundary: .capability, choice: .nativeCarrier)
  expect baselineAllows(boundary: .foreignC, choice: .nativeCarrier)
  expect !baselineAllows(boundary: .wire, choice: .nativeCarrier)
}
