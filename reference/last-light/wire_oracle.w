// Pure oracle for wWire profile selection, eligibility, and strict decoding.

enum WireSchemaRelation {
  sameWireSchema
  compatible
  incompatible
}

enum WireProfilePlan {
  exact
  compatible
  reject
}

const fn expectedProfile(for relation: WireSchemaRelation): WireProfilePlan {
  return switch relation {
    case .sameWireSchema: .exact
    case .compatible: .compatible
    case .incompatible: .reject
  }
}

enum WireTypeCase {
  fixedInteger
  refinedTargetIndex
  unboundedTargetIndex
  string
  staticTensor
  workSuspension
  serviceCapability
  localInstant
  borrowedView
  sharedGraph
}

enum WireEligibility {
  data
  capabilitySlot
  rejected
}

const fn expectedEligibility(for value: WireTypeCase): WireEligibility {
  return switch value {
    case .fixedInteger: .data
    case .refinedTargetIndex: .data
    case .unboundedTargetIndex: .rejected
    case .string: .data
    case .staticTensor: .data
    case .workSuspension: .data
    case .serviceCapability: .capabilitySlot
    case .localInstant: .rejected
    case .borrowedView: .rejected
    case .sharedGraph: .rejected
  }
}

const fn unsignedWireBytes(for maximum: u64): u8 {
  return switch maximum {
    case ..<256_u64: 1
    case 256_u64..<65_536_u64: 2
    case 65_536_u64..<4_294_967_296_u64: 4
    case 4_294_967_296_u64...: 8
  }
}

enum WireDefect {
  nonMinimalControlInteger
  duplicateFieldId
  unorderedFieldId
  invalidBool
  invalidUtf8
  invalidEnumSubset
  unusedPresenceBit
  truncatedBlock
  trailingData
  countOverflow
  traversalLimit
}

enum DecodeDisposition {
  codecFailure
}

const fn expectedDisposition(for defect: WireDefect): DecodeDisposition {
  return switch defect {
    case .nonMinimalControlInteger: .codecFailure
    case .duplicateFieldId: .codecFailure
    case .unorderedFieldId: .codecFailure
    case .invalidBool: .codecFailure
    case .invalidUtf8: .codecFailure
    case .invalidEnumSubset: .codecFailure
    case .unusedPresenceBit: .codecFailure
    case .truncatedBlock: .codecFailure
    case .trailingData: .codecFailure
    case .countOverflow: .codecFailure
    case .traversalLimit: .codecFailure
  }
}

enum UnknownFieldConsumer {
  ordinaryValue
  explicitRelay
}

enum UnknownFieldDisposition {
  discard
  preserveCanonicalBlock
}

const fn expectedUnknownFieldDisposition(
  for consumer: UnknownFieldConsumer,
): UnknownFieldDisposition {
  return switch consumer {
    case .ordinaryValue: .discard
    case .explicitRelay: .preserveCanonicalBlock
  }
}

test "wire schema relation selects the codec profile" for expectedProfile {
  expect expectedProfile(for: .sameWireSchema) == .exact
  expect expectedProfile(for: .compatible) == .compatible
  expect expectedProfile(for: .incompatible) == .reject
}

test "local time and borrowed storage do not cross wRPC" for expectedEligibility {
  expect expectedEligibility(for: .fixedInteger) == .data
  expect expectedEligibility(for: .refinedTargetIndex) == .data
  expect expectedEligibility(for: .unboundedTargetIndex) == .rejected
  expect expectedEligibility(for: .workSuspension) == .data
  expect expectedEligibility(for: .serviceCapability) == .capabilitySlot
  expect expectedEligibility(for: .localInstant) == .rejected
  expect expectedEligibility(for: .borrowedView) == .rejected
  expect expectedEligibility(for: .sharedGraph) == .rejected
}

test "a refinement can reduce a fixed wire width" for unsignedWireBytes {
  expect unsignedWireBytes(for: 128) == 1
  expect unsignedWireBytes(for: 4_096) == 2
  expect unsignedWireBytes(for: 4_294_967_295) == 4
  expect unsignedWireBytes(for: 4_294_967_296) == 8
}

test "strict decoding rejects alternate representations" for expectedDisposition {
  expect expectedDisposition(for: .nonMinimalControlInteger) == .codecFailure
  expect expectedDisposition(for: .duplicateFieldId) == .codecFailure
  expect expectedDisposition(for: .invalidUtf8) == .codecFailure
  expect expectedDisposition(for: .invalidEnumSubset) == .codecFailure
  expect expectedDisposition(for: .countOverflow) == .codecFailure
  expect expectedDisposition(for: .traversalLimit) == .codecFailure
}

test "unknown bytes require an explicit relay carrier" for expectedUnknownFieldDisposition {
  expect expectedUnknownFieldDisposition(for: .ordinaryValue) == .discard
  expect expectedUnknownFieldDisposition(for: .explicitRelay) == .preserveCanonicalBlock
}
