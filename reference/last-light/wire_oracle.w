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

enum WireKind {
  invalid
  bool
  u8
  u16
  u32
  u64
  u128
  i8
  i16
  i32
  i64
  i128
  f32
  f64
  bytes
  string
  record
  tuple
  enumValue
  option
  result
  sequence
  map
  set
  tensor
  capability
}

const fn wireKindId(for kind: WireKind): u8 {
  return switch kind {
    case .invalid: 0
    case .bool: 1
    case .u8: 2
    case .u16: 3
    case .u32: 4
    case .u64: 5
    case .u128: 6
    case .i8: 7
    case .i16: 8
    case .i32: 9
    case .i64: 10
    case .i128: 11
    case .f32: 12
    case .f64: 13
    case .bytes: 14
    case .string: 15
    case .record: 16
    case .tuple: 17
    case .enumValue: 18
    case .option: 19
    case .result: 20
    case .sequence: 21
    case .map: 22
    case .set: 23
    case .tensor: 24
    case .capability: 25
  }
}

enum WireVector {
  exactMenuKeyAbsent
  exactMenuKeyPresent
  compatibleMenuKeyAbsent
  compatibleMenuKeyPresent
}

const fn expectedVector(for vector: WireVector): Array<u8> {
  return switch vector {
    case .exactMenuKeyAbsent: [0x00_u8, 0x2a_u8, 0x00_u8]
    case .exactMenuKeyPresent: [0x01_u8, 0x2a_u8, 0x00_u8, 0x01_u8]
    case .compatibleMenuKeyAbsent: [0x01_u8, 0x01_u8, 0x03_u8, 0x02_u8, 0x2a_u8, 0x00_u8]
    case .compatibleMenuKeyPresent: [
      0x02_u8,
      0x01_u8,
      0x03_u8,
      0x02_u8,
      0x01_u8,
      0x01_u8,
      0x01_u8,
      0x2a_u8,
      0x00_u8,
      0x01_u8
    ]
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

struct PortableOvenReady {
  remaining: Duration
}

struct LocalOvenReady {
  deadline: Instant
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

const maximumWireMessageBytes: u64 = 4_294_967_295

struct WireBudget {
  receivedBytes: u64
  logicalBytes: u64
  nodes: u64
  depth: u32
  allocationBytes: u64
}

struct WireLimits {
  maximumReceivedBytes: u64
  maximumLogicalBytes: u64
  maximumNodes: u64
  maximumDepth: u32
  maximumAllocationBytes: u64
}

const fn budgetFits(
  limits wireLimits: WireLimits,
  usage measuredUsage: WireBudget,
): Bool {
  return measuredUsage.receivedBytes <= wireLimits.maximumReceivedBytes &&
    measuredUsage.logicalBytes <= wireLimits.maximumLogicalBytes &&
    measuredUsage.nodes <= wireLimits.maximumNodes &&
    measuredUsage.depth <= wireLimits.maximumDepth &&
    measuredUsage.allocationBytes <= wireLimits.maximumAllocationBytes
}

const fn directoryLengthIsValid(
  named directoryBytes: u64,
  named blockBytes: u64,
  named recordBytes: u64,
): Bool {
  if directoryBytes > maximumWireMessageBytes ||
    blockBytes > maximumWireMessageBytes {
    return false
  }

  if directoryBytes > maximumWireMessageBytes - blockBytes {
    return false
  }

  return directoryBytes + blockBytes == recordBytes
}

enum WireDefect {
  nonMinimalControlInteger
  duplicateFieldId
  unorderedFieldId
  missingRequiredField
  invalidWireKind
  unsupportedWireKind
  invalidBool
  invalidUtf8
  invalidEnumSubset
  unusedPresenceBit
  truncatedBlock
  trailingData
  controlIntegerOverflow
  fieldIdOverflow
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
    case .missingRequiredField: .codecFailure
    case .invalidWireKind: .codecFailure
    case .unsupportedWireKind: .codecFailure
    case .invalidBool: .codecFailure
    case .invalidUtf8: .codecFailure
    case .invalidEnumSubset: .codecFailure
    case .unusedPresenceBit: .codecFailure
    case .truncatedBlock: .codecFailure
    case .trailingData: .codecFailure
    case .controlIntegerOverflow: .codecFailure
    case .fieldIdOverflow: .codecFailure
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

test "core wire kind IDs are independent of source enum order" for wireKindId {
  expect wireKindId(for: .invalid) == 0
  expect wireKindId(for: .bool) == 1
  expect wireKindId(for: .u16) == 3
  expect wireKindId(for: .i128) == 11
  expect wireKindId(for: .string) == 15
  expect wireKindId(for: .record) == 16
  expect wireKindId(for: .tensor) == 24
  expect wireKindId(for: .capability) == 25
}

test "wWire seed vectors remain canonical" for expectedVector {
  expect expectedVector(for: .exactMenuKeyAbsent) == [0x00_u8, 0x2a_u8, 0x00_u8]
  expect expectedVector(for: .exactMenuKeyPresent) == [0x01_u8, 0x2a_u8, 0x00_u8, 0x01_u8]
  expect expectedVector(for: .compatibleMenuKeyAbsent) == [0x01_u8, 0x01_u8, 0x03_u8, 0x02_u8, 0x2a_u8, 0x00_u8]
  expect expectedVector(for: .compatibleMenuKeyPresent) == [
    0x02_u8,
    0x01_u8,
    0x03_u8,
    0x02_u8,
    0x01_u8,
    0x01_u8,
    0x01_u8,
    0x2a_u8,
    0x00_u8,
    0x01_u8
  ]
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

test "decoder budgets are independent and bounded" for budgetFits {
  let limits = WireLimits(
    maximumReceivedBytes: 4_096,
    maximumLogicalBytes: 16_384,
    maximumNodes: 128,
    maximumDepth: 8,
    maximumAllocationBytes: 16_384,
  )
  let usage = WireBudget(
    receivedBytes: 512,
    logicalBytes: 2_048,
    nodes: 12,
    depth: 4,
    allocationBytes: 2_048,
  )
  expect budgetFits(limits: limits, usage: usage)
  expect !budgetFits(
    limits: limits,
    usage: WireBudget(
      receivedBytes: 512,
      logicalBytes: 16_384 + 1,
      nodes: 12,
      depth: 4,
      allocationBytes: 2_048,
    ),
  )
  expect !budgetFits(
    limits: limits,
    usage: WireBudget(
      receivedBytes: 512,
      logicalBytes: 2_048,
      nodes: 129,
      depth: 4,
      allocationBytes: 2_048,
    ),
  )
  expect !budgetFits(
    limits: limits,
    usage: WireBudget(
      receivedBytes: 512,
      logicalBytes: 2_048,
      nodes: 12,
      depth: 4,
      allocationBytes: 16_384 + 1,
    ),
  )
}

test "compatible directory lengths use checked equality" for directoryLengthIsValid {
  expect directoryLengthIsValid(
    directoryBytes: 4,
    blockBytes: 8,
    recordBytes: 12,
  )
  expect !directoryLengthIsValid(
    directoryBytes: 4,
    blockBytes: 8,
    recordBytes: 11,
  )
  expect !directoryLengthIsValid(
    directoryBytes: maximumWireMessageBytes,
    blockBytes: 1,
    recordBytes: 0,
  )
}

test "strict decoding rejects alternate representations" for expectedDisposition {
  expect expectedDisposition(for: .nonMinimalControlInteger) == .codecFailure
  expect expectedDisposition(for: .duplicateFieldId) == .codecFailure
  expect expectedDisposition(for: .missingRequiredField) == .codecFailure
  expect expectedDisposition(for: .invalidWireKind) == .codecFailure
  expect expectedDisposition(for: .unsupportedWireKind) == .codecFailure
  expect expectedDisposition(for: .invalidUtf8) == .codecFailure
  expect expectedDisposition(for: .invalidEnumSubset) == .codecFailure
  expect expectedDisposition(for: .countOverflow) == .codecFailure
  expect expectedDisposition(for: .traversalLimit) == .codecFailure
}

test "unknown bytes require an explicit relay carrier" for expectedUnknownFieldDisposition {
  expect expectedUnknownFieldDisposition(for: .ordinaryValue) == .discard
  expect expectedUnknownFieldDisposition(for: .explicitRelay) == .preserveCanonicalBlock
}
