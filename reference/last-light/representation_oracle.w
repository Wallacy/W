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
  expect baselineAllows(.internal, .explicitTag)
  expect baselineAllows(.internal, .provenNiche)
  expect baselineAllows(.internal, .lowBit)
  expect !baselineAllows(.internal, .highBit)

  expect baselineAllows(.wExact, .provenNiche)
  expect !baselineAllows(.wExact, .lowBit)
  expect !baselineAllows(.foreignC, .provenNiche)
  expect !baselineAllows(.foreignC, .lowBit)
  expect !baselineAllows(.wire, .provenNiche)
  expect !baselineAllows(.persisted, .lowBit)
  expect baselineAllows(.capability, .explicitTag)
  expect baselineAllows(.capability, .nativeCarrier)
  expect baselineAllows(.foreignC, .nativeCarrier)
  expect !baselineAllows(.wire, .nativeCarrier)
}
