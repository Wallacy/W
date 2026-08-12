// Small independent M1 oracle. The host corpus owns full state transitions.
// This source records compiler facts. It does not describe runtime metadata.

enum HirOwnerState { owned moved dropped }
enum HirAddressState { unstable stable published }
enum HirLoanMode { shared exclusive }
enum HirProjectionKind { field tuple enumPayload index range dereference opaque }
enum HirEdgeMode { shared exclusive }
enum HirDependencyAccess { read write }
enum HirDeclarationKind { instance initializer typeMember free }
enum HirBoundary { internal wExact foreignC wire persisted capability }
enum HirRepresentation { explicitTag provenNiche lowBit highBit nativeCarrier }
enum HirBoundaryOwnership { borrowed owned }
enum HirAllocatorMobility { local crossDomain }
enum HirAllocatorSlotKind { contextual }

// ASC0 keeps the contextual allocator slot visible to HIR, resource/interface
// facts, and ABI. An effect row records it only when the declaration has one;
// it is not a hidden parameter on ordinary functions.
struct HirAllocatorSlot {
  index: usize
  name: String
  kind: HirAllocatorSlotKind
  abiVisible: Bool
  resourceVisible: Bool
}

const fn validAllocatorSlots(slots: Array<HirAllocatorSlot>): Bool {
  if slots.count > 1 { return false }
  if slots.count == 0 { return true }
  let slot = slots[0]
  return slot.index == 0 && slot.abiVisible && slot.resourceVisible
}

struct HirProjection {
  kind: HirProjectionKind
  name: String
  start: usize
  endExclusive: usize
}

struct HirPlaceId {
  root: String
  projections: Array<HirProjection>
}

struct HirLoanRecord {
  id: u32
  place: HirPlaceId
  mode: HirLoanMode
  emittedAt: u32
  endedAt: u32?
  stable: Bool
  parent: u32?
  childCount: u32
}

// An OriginSet is a deduplicated projection. Edges remain individual so two
// copies of the same origin keep the owner blocked until both drops.
struct HirDependencyEdge {
  id: u32
  ownerPlace: HirPlaceId?
  ownerSlot: String?
  mode: HirEdgeMode
  origin: String
  dynamic: Bool
}

struct HirDependentPayload {
  edges: Array<HirDependencyEdge>
  lifetimeIndependent: Bool
}

struct HirPinnedHandle {
  payloadRoot: HirPlaceId
  handleRoot: String
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

struct HirResultMapping {
  resultSlot: String
  sources: Array<String>
}

// Borrow origins describe referents. Allocation origins describe storage and
// its deallocator. A value can be borrow-independent and storage-dependent.
struct HirAllocationOrigin {
  allocator: String
  mobility: HirAllocatorMobility
}

struct HirStorageFacts {
  origins: Array<HirAllocationOrigin>
}

struct HirSharedCounts {
  strong: u32
  weak: u32
  payloadAlive: Bool
  blockAlive: Bool
  deinitCount: u32
}

fn newOwner(address: HirAddressState): HirOwner {
  return HirOwner(state: .owned, borrowCount: 0, address: address)
}

fn beginOwnerBorrow(owner: HirOwner): HirOwner? {
  if owner.state != .owned { return .none }
  var next = owner
  next.borrowCount += 1
  return .some(next)
}

fn endOwnerBorrow(owner: HirOwner): HirOwner? {
  if owner.state != .owned || owner.borrowCount == 0 { return .none }
  var next = owner
  next.borrowCount -= 1
  return .some(next)
}

fn moveOwner(owner: HirOwner): HirMove? {
  if owner.state != .owned || owner.borrowCount != 0 { return .none }
  var source = owner
  source.state = .moved
  return .some(HirMove(source: source, destination: owner))
}

fn dropOwner(owner: HirOwner): HirOwner? {
  if owner.state != .owned || owner.borrowCount != 0 { return .none }
  var next = owner
  next.state = .dropped
  return .some(next)
}

const fn acceptsRepresentation(boundary: HirBoundary, representation: HirRepresentation): Bool {
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

const fn acceptsBoundary(value: HirBoundaryValue): Bool {
  if !acceptsRepresentation(value.boundary, value.representation) { return false }
  return value.ownership == .borrowed || value.allocatorKnown
}

const fn sameAbi(left: HirAbiKey, right: HirAbiKey): Bool {
  return left.target == right.target
    && left.callingConvention == right.callingConvention
    && left.representationPolicy == right.representationPolicy
    && left.runtimeAbi == right.runtimeAbi
}

fn knownFieldsDisjoint(left: HirProjection, right: HirProjection): Bool {
  return left.kind == .field
    && right.kind == .field
    && left.name != right.name
}

fn placeIsSubplace(child: HirPlaceId, parent: HirPlaceId): Bool {
  if child.root != parent.root || child.projections.count < parent.projections.count {
    return false
  }
  for index in 0..<parent.projections.count {
    if child.projections[index].name != parent.projections[index].name {
      return false
    }
  }
  return true
}

fn joinDependencies(left: HirDependentPayload, right: HirDependentPayload): HirDependentPayload {
  var joined = left
  for edge in right.edges {
    joined.edges.append(edge)
  }
  joined.lifetimeIndependent = true
  for edge in joined.edges {
    if edge.dynamic { joined.lifetimeIndependent = false }
  }
  return joined
}

fn projectOrigins(value: ref HirDependentPayload): Array<String> {
  var result: Array<String> = []
  for edge in value.edges {
    if !result.contains(edge.origin) {
      result.append(copy edge.origin)
    }
  }
  return result
}

fn dependentMayReturn(value: HirDependentPayload, surviving: Array<String>): Bool {
  for edge in value.edges {
    if edge.dynamic && !surviving.contains(edge.origin) { return false }
  }
  return true
}

fn dependencyPermits(edge: HirDependencyEdge, access: HirDependencyAccess): Bool {
  return access == .read || edge.mode == .exclusive
}

fn deriveBodylessMapping(
  kind: HirDeclarationKind,
  receiverCompatible: Bool,
  inputSlots: Array<String>,
  resultSlots: Array<String>,
): Array<HirResultMapping>? {
  if resultSlots.count == 0 { return [] }
  var sources: Array<String>
  if kind == .instance {
    if !receiverCompatible { return .none }
    sources = ["receiver"]
  } else {
    if inputSlots.count == 0 { return .none }
    sources = inputSlots
  }
  var mappings: Array<HirResultMapping> = []
  for slot in resultSlots {
    mappings.append(HirResultMapping(resultSlot: slot, sources: copy sources))
  }
  return mappings
}

fn loanCanSuspend(loan: HirLoanRecord, referentStable: Bool): Bool {
  return loan.stable && referentStable
}

fn pinHandle(payloadRoot: HirPlaceId, handleRoot: String): HirPinnedHandle {
  return HirPinnedHandle(payloadRoot: payloadRoot, handleRoot: handleRoot)
}

fn movePinnedHandle(handle: HirPinnedHandle, destination: String): HirPinnedHandle {
  return HirPinnedHandle(
    payloadRoot: handle.payloadRoot,
    handleRoot: destination,
  )
}

const fn storageCanCrossDomain(storage: ref HirStorageFacts): Bool {
  for origin in storage.origins {
    if origin.mobility != .crossDomain { return false }
  }
  return true
}

const fn canCreateShared(payload: ref HirDependentPayload): Bool {
  return payload.lifetimeIndependent
}

fn releaseStrong(counts: HirSharedCounts): HirSharedCounts? {
  if counts.strong == 0 || !counts.blockAlive { return .none }
  var next = counts
  next.strong -= 1
  if next.strong == 0 {
    next.payloadAlive = false
    next.deinitCount += 1
    if next.weak == 0 { next.blockAlive = false }
  }
  return .some(next)
}

fn releaseWeak(counts: HirSharedCounts): HirSharedCounts? {
  if counts.weak == 0 || !counts.blockAlive { return .none }
  var next = counts
  next.weak -= 1
  if next.strong == 0 && next.weak == 0 { next.blockAlive = false }
  return .some(next)
}

test "M1 owner moves and drops once" {
  let first = newOwner(.unstable)
  let moved = moveOwner(first)
  guard let moved = moved else { panic("move was rejected") }
  expect moved.source.state == .moved
  expect moved.destination.state == .owned

  let dropped = dropOwner(moved.destination)
  guard let dropped = dropped else { panic("drop was rejected") }
  expect dropped.state == .dropped
  expect dropOwner(dropped) == .none
}

test "M1 owner borrow blocks payload movement" {
  let borrowed = beginOwnerBorrow(newOwner(.published))
  guard let borrowed = borrowed else { panic("borrow was rejected") }
  expect moveOwner(borrowed) == .none
  expect dropOwner(borrowed) == .none

  let released = endOwnerBorrow(borrowed)
  guard let released = released else { panic("borrow end was rejected") }
  expect moveOwner(released) != .none
}

test "M1 representation and ABI are checked before lowering" {
  expect acceptsRepresentation(.internal, .lowBit)
  expect acceptsRepresentation(.wExact, .provenNiche)
  expect !acceptsRepresentation(.foreignC, .provenNiche)
  expect acceptsRepresentation(.foreignC, .nativeCarrier)
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

test "M1 place IDs require a child reborrow to be a subplace" {
  let parent = HirPlaceId(root: "kitchen", projections: [HirProjection(
    kind: .field,
    name: "westOven",
    start: 0,
    endExclusive: 0,
  )])
  let child = HirPlaceId(root: "kitchen", projections: [
    HirProjection(kind: .field, name: "westOven", start: 0, endExclusive: 0),
    HirProjection(kind: .field, name: "temperature", start: 0, endExclusive: 0),
  ])
  let sibling = HirPlaceId(root: "kitchen", projections: [HirProjection(
    kind: .field,
    name: "eastOven",
    start: 0,
    endExclusive: 0,
  )])
  expect placeIsSubplace(child, parent)
  expect !placeIsSubplace(sibling, parent)
  expect !placeIsSubplace(HirPlaceId(root: "kitchen", projections: []), parent)
}

test "M1 dependency edges project lifetime independence" {
  let dynamic = HirDependencyEdge(
    id: 1,
    ownerPlace: .none,
    ownerSlot: .some("menu"),
    mode: .shared,
    origin: "menu",
    dynamic: true,
  )
  let immortal = HirDependencyEdge(
    id: 2,
    ownerPlace: .none,
    ownerSlot: .none,
    mode: .shared,
    origin: "program",
    dynamic: false,
  )
  let dynamicCopy = HirDependencyEdge(
    id: 3,
    ownerPlace: .none,
    ownerSlot: .some("menu"),
    mode: .shared,
    origin: "menu",
    dynamic: true,
  )
  let exclusive = HirDependencyEdge(
    id: 4,
    ownerPlace: .none,
    ownerSlot: .some("oven"),
    mode: .exclusive,
    origin: "oven",
    dynamic: true,
  )
  let value = HirDependentPayload(edges: [dynamic, immortal], lifetimeIndependent: false)
  let surviving = joinDependencies(
    value,
    HirDependentPayload(edges: [dynamicCopy], lifetimeIndependent: false),
  )
  let origins = projectOrigins(ref surviving)
  expect !surviving.lifetimeIndependent
  expect surviving.edges.count == 3
  expect origins.count == 2
  expect dependentMayReturn(surviving, ["menu"])
  expect !dependentMayReturn(surviving, [])
  expect dependencyPermits(dynamic, .read)
  expect !dependencyPermits(dynamic, .write)
  expect dependencyPermits(exclusive, .write)
}

test "M1 pinned handle moves while payload root stays stable" {
  let payload = HirPlaceId(root: "pin:state", projections: [])
  let handle = pinHandle(payload, "handle")
  expect handle.payloadRoot.root == "pin:state"
  expect handle.handleRoot == "handle"
  let loan = HirLoanRecord(
    id: 1,
    place: payload,
    mode: .shared,
    emittedAt: 1,
    endedAt: .none,
    stable: true,
    parent: .none,
    childCount: 0,
  )
  let moved = movePinnedHandle(handle, "movedHandle")
  expect moved.handleRoot == "movedHandle"
  expect moved.payloadRoot.root == handle.payloadRoot.root
  expect loanCanSuspend(loan, true)
}

test "M1 bodyless interface mapping uses all compatible inputs" {
  let mapping = deriveBodylessMapping(
    .free,
    false,
    ["parameter:0", "parameter:1"],
    ["result.title", "result.body"],
  )
  guard let mapping = mapping else { panic("mapping was rejected") }
  expect mapping.count == 2
  expect mapping[0].sources.count == 2

  let receiver = deriveBodylessMapping(.instance, true, ["parameter:0"], ["result"])
  guard let receiver = receiver else { panic("receiver mapping was rejected") }
  expect receiver[0].sources == ["receiver"]
}

test "M1 await requires stable referent" {
  let loan = HirLoanRecord(
    id: 1,
    place: HirPlaceId(root: "menu", projections: []),
    mode: .shared,
    emittedAt: 1,
    endedAt: .none,
    stable: true,
    parent: .none,
    childCount: 0,
  )
  expect loanCanSuspend(loan, true)
  expect !loanCanSuspend(loan, false)
}

test "M1 borrow and storage origins remain independent" {
  let borrowed = HirDependentPayload(
    edges: [HirDependencyEdge(
      id: 1,
      ownerPlace: .none,
      ownerSlot: .some("menu"),
      mode: .shared,
      origin: "menu",
      dynamic: true,
    )],
    lifetimeIndependent: false,
  )
  let localStorage = HirStorageFacts(origins: [HirAllocationOrigin(
    allocator: "request",
    mobility: .local,
  )])
  let processStorage = HirStorageFacts(origins: [HirAllocationOrigin(
    allocator: "process",
    mobility: .crossDomain,
  )])

  expect !canCreateShared(ref borrowed)
  expect !storageCanCrossDomain(ref localStorage)
  expect storageCanCrossDomain(ref processStorage)
}

test "M1 weak handle keeps only the control block alive" {
  let initial = HirSharedCounts(
    strong: 1,
    weak: 1,
    payloadAlive: true,
    blockAlive: true,
    deinitCount: 0,
  )
  let releasedStrong = releaseStrong(initial)
  guard let releasedStrong = releasedStrong else { panic("strong release was rejected") }
  expect !releasedStrong.payloadAlive
  expect releasedStrong.blockAlive
  expect releasedStrong.deinitCount == 1

  let releasedWeak = releaseWeak(releasedStrong)
  guard let releasedWeak = releasedWeak else { panic("weak release was rejected") }
  expect !releasedWeak.blockAlive
  expect releasedWeak.deinitCount == 1
}

test "ASC0 allocator slot is first, unique, and ABI-visible" {
  let valid = [HirAllocatorSlot(
    index: 0,
    name: "memory",
    kind: .contextual,
    abiVisible: true,
    resourceVisible: true,
  )]
  let duplicate = [
    HirAllocatorSlot(index: 0, name: "memory", kind: .contextual, abiVisible: true, resourceVisible: true),
    HirAllocatorSlot(index: 1, name: "other", kind: .contextual, abiVisible: true, resourceVisible: true),
  ]
  let hidden = [HirAllocatorSlot(
    index: 0,
    name: "memory",
    kind: .contextual,
    abiVisible: false,
    resourceVisible: true,
  )]
  expect validAllocatorSlots(valid)
  expect !validAllocatorSlots(duplicate)
  expect !validAllocatorSlots(hidden)
}
