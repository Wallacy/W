// Minimal HIR oracle for ownership, suspension, representation, and ABI.
// It models compiler facts. It is not a runtime implementation.

enum HirOwnerState {
  owned
  moved
  dropped
}

enum HirAddressState {
  unstable
  stable
  published
}

enum HirBoundary {
  internal
  wExact
  foreignC
  wire
  persisted
  capability
}

enum HirRepresentation {
  explicitTag
  provenNiche
  lowBit
  highBit
  nativeCarrier
}

enum HirBoundaryOwnership {
  borrowed
  owned
}

struct HirOwner {
  state: HirOwnerState
  borrowCount: u8
  address: HirAddressState
}

struct HirMove {
  source: HirOwner
  destination: HirOwner
}

struct HirAbiKey {
  target: u16
  callingConvention: u8
  representationPolicy: u8
  runtimeAbi: u8
}

struct HirBoundaryValue {
  boundary: HirBoundary
  representation: HirRepresentation
  ownership: HirBoundaryOwnership
  allocatorKnown: Bool
}

fn newOwner(address: HirAddressState): HirOwner {
  return HirOwner(state: .owned, borrowCount: 0, address: address)
}

fn beginBorrow(owner: HirOwner): HirOwner? {
  if owner.state != .owned { return .none }

  var next = owner
  next.borrowCount += 1
  return .some(next)
}

fn endBorrow(owner: HirOwner): HirOwner? {
  if owner.state != .owned || owner.borrowCount == 0 {
    return .none
  }

  var next = owner
  next.borrowCount -= 1
  return .some(next)
}

fn moveOwner(owner: HirOwner): HirMove? {
  if owner.state != .owned || owner.borrowCount != 0 {
    return .none
  }

  var source = owner
  source.state = .moved
  return .some(HirMove(source: source, destination: owner))
}

fn dropOwner(owner: HirOwner): HirOwner? {
  if owner.state != .owned || owner.borrowCount != 0 {
    return .none
  }

  var next = owner
  next.state = .dropped
  return .some(next)
}

fn canSuspend(owner: HirOwner): Bool {
  return owner.borrowCount == 0
    || owner.address == .stable
    || owner.address == .published
}

const fn acceptsRepresentation(
  boundary: HirBoundary,
  representation: HirRepresentation,
): Bool {
  return switch representation {
    case .explicitTag: true
    case .provenNiche:
      boundary != .foreignC && boundary != .wire && boundary != .persisted
    case .lowBit: boundary == .internal
    case .highBit: false
    case .nativeCarrier:
      boundary == .foreignC || boundary == .capability
  }
}

const fn sameAbi(left: HirAbiKey, right: HirAbiKey): Bool {
  return left.target == right.target
    && left.callingConvention == right.callingConvention
    && left.representationPolicy == right.representationPolicy
    && left.runtimeAbi == right.runtimeAbi
}

const fn acceptsBoundary(value: HirBoundaryValue): Bool {
  if !acceptsRepresentation(value.boundary, value.representation) {
    return false
  }

  return value.ownership == .borrowed || value.allocatorKnown
}

test "move and drop have one owner edge" {
  let first = newOwner(.unstable)
  let moved = moveOwner(first)
  guard let moved = moved else { panic("move was rejected") }

  expect first.state == .owned
  expect moved.source.state == .moved
  expect moved.destination.state == .owned

  let dropped = dropOwner(moved.destination)
  guard let dropped = dropped else { panic("drop was rejected") }
  expect dropped.state == .dropped
  expect dropOwner(dropped) == .none
}

test "borrow blocks move and drop until it ends" {
  let borrowed = beginBorrow(newOwner(.unstable))
  guard let borrowed = borrowed else { panic("borrow was rejected") }

  expect moveOwner(borrowed) == .none
  expect dropOwner(borrowed) == .none

  let released = endBorrow(borrowed)
  guard let released = released else { panic("borrow end was rejected") }
  expect released.borrowCount == 0
  expect moveOwner(released) != .none
}

test "pinned storage permits suspension and handle movement" {
  let borrowed = beginBorrow(newOwner(.published))
  guard let borrowed = borrowed else { panic("borrow was rejected") }

  expect canSuspend(borrowed)

  let moved = moveOwner(borrowed)
  expect moved == .none

  let released = endBorrow(borrowed)
  guard let released = released else { panic("borrow end was rejected") }
  let movedHandle = moveOwner(released)
  guard let movedHandle = movedHandle else { panic("move was rejected") }
  expect movedHandle.source.address == .published
  expect movedHandle.destination.address == .published
}

test "representation and ABI are checked before lowering" {
  expect acceptsRepresentation(.internal, .lowBit)
  expect acceptsRepresentation(.wExact, .provenNiche)
  expect !acceptsRepresentation(.foreignC, .provenNiche)
  expect !acceptsRepresentation(.wire, .lowBit)
  expect acceptsRepresentation(.foreignC, .nativeCarrier)
  expect acceptsRepresentation(.capability, .nativeCarrier)
  expect !acceptsRepresentation(.internal, .highBit)

  expect acceptsBoundary(HirBoundaryValue(
    boundary: .wExact,
    representation: .provenNiche,
    ownership: .owned,
    allocatorKnown: true,
  ))
  expect !acceptsBoundary(HirBoundaryValue(
    boundary: .wire,
    representation: .explicitTag,
    ownership: .owned,
    allocatorKnown: false,
  ))

  let key = HirAbiKey(
    target: 1,
    callingConvention: 1,
    representationPolicy: 1,
    runtimeAbi: 1,
  )
  expect sameAbi(key, key)
  expect !sameAbi(key, HirAbiKey(
    target: 2,
    callingConvention: 1,
    representationPolicy: 1,
    runtimeAbi: 1,
  ))
}
