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
// facts, and ABI at its declared position. An effect row records it only when
// the declaration has one; it is not a hidden parameter on ordinary functions.
struct HirAllocatorSlot {
  let index: usize
  let name: String
  let kind: HirAllocatorSlotKind
  let abiVisible: Bool
  let resourceVisible: Bool
}

const fn validAllocatorSlots(slots: Array<HirAllocatorSlot>): Bool {
  if slots.count > 1 { return false }
  if slots.count == 0 { return true }
  let slot = slots[0]
  return slot.index >= 0 && slot.abiVisible && slot.resourceVisible
}

struct HirProjection {
  let kind: HirProjectionKind
  let name: String
  let start: usize
  let endExclusive: usize
}

struct HirPlaceId {
  let root: String
  let projections: Array<HirProjection>
}

struct HirLoanRecord {
  let id: u32
  let place: HirPlaceId
  let mode: HirLoanMode
  let emittedAt: u32
  let endedAt: u32?
  let stable: Bool
  let parent: u32?
  let childCount: u32
}

// An OriginSet is a deduplicated projection. Edges remain individual so two
// copies of the same origin keep the owner blocked until both drops.
struct HirDependencyEdge {
  let id: u32
  let ownerPlace: HirPlaceId?
  let ownerSlot: String?
  let mode: HirEdgeMode
  let origin: String
  let dynamic: Bool
}

struct HirDependentPayload {
  let edges: Array<HirDependencyEdge>
  let lifetimeIndependent: Bool
}

struct HirPinnedHandle {
  let payloadRoot: HirPlaceId
  let handleRoot: String
}

struct HirOwner {
  let state: HirOwnerState
  let borrowCount: u8
  let address: HirAddressState
}

struct HirMove {
  let source: HirOwner
  let destination: HirOwner
}

struct HirAbiKey {
  let target: u16
  let callingConvention: u8
  let representationPolicy: u8
  let runtimeAbi: u8
}

struct HirBoundaryValue {
  let boundary: HirBoundary
  let representation: HirRepresentation
  let ownership: HirBoundaryOwnership
  let allocatorKnown: Bool
}

struct HirResultMapping {
  let resultSlot: String
  let sources: Array<String>
}

// Borrow origins describe referents. Allocation origins describe storage and
// its deallocator. A value can be borrow-independent and storage-dependent.
struct HirAllocationOrigin {
  let allocator: String
  let mobility: HirAllocatorMobility
}

// SHC0 keeps the hidden control-block receipt distinct from payload storage.
// Provider profile facts join the descriptor before this record is published.
struct HirSharedControlOrigin {
  let contract: String
  let instance: String
  let lifetime: String
  let deallocator: String
  let mobility: HirAllocatorMobility
  let adoptionFamily: String
  let bulkReleaseOwner: String?
}

struct HirAllocationOriginMap {
  let storage: Array<HirAllocationOrigin>
  let controlBlock: HirSharedControlOrigin
}

struct HirStorageFacts {
  let origins: Array<HirAllocationOrigin>
}

struct HirSharedCounts {
  let strong: u32
  let weak: u32
  let payloadAlive: Bool
  let blockAlive: Bool
  let deinitCount: u32
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
  if !acceptsRepresentation(boundary: value.boundary, representation: value.representation) { return false }
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
  if kind == .initializer {
    return .none
  } else if kind == .instance {
    if !receiverCompatible { return .none }
    sources = ["receiver"]
  } else {
    if inputSlots.count != 1 { return .none }
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

test "SHC0 origin map names payload and hidden control block paths" {
  let map = HirAllocationOriginMap(
    storage: [HirAllocationOrigin(allocator: "request", mobility: .crossDomain)],
    controlBlock: HirSharedControlOrigin(
      contract: "restaurant-pool-v1",
      instance: "request",
      lifetime: "request",
      deallocator: "provider",
      mobility: .crossDomain,
      adoptionFamily: "shared-control",
      bulkReleaseOwner: "request",
    ),
  )
  expect map.storage[0].allocator == "request"
  expect map.controlBlock.deallocator == "provider"
  expect map.controlBlock.mobility == .crossDomain
  expect map.controlBlock.adoptionFamily == "shared-control"
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
  let first = newOwner(address: .unstable)
  let moved = moveOwner(owner: first)
  guard let moved = moved else { panic("move was rejected") }
  expect moved.source.state == .moved
  expect moved.destination.state == .owned

  let dropped = dropOwner(owner: moved.destination)
  guard let dropped = dropped else { panic("drop was rejected") }
  expect dropped.state == .dropped
  expect dropOwner(owner: dropped) == .none
}

test "M1 owner borrow blocks payload movement" {
  let borrowed = beginOwnerBorrow(owner: newOwner(address: .published))
  guard let borrowed = borrowed else { panic("borrow was rejected") }
  expect moveOwner(owner: borrowed) == .none
  expect dropOwner(owner: borrowed) == .none

  let released = endOwnerBorrow(owner: borrowed)
  guard let released = released else { panic("borrow end was rejected") }
  expect moveOwner(owner: released) != .none
}

test "M1 representation and ABI are checked before lowering" {
  expect acceptsRepresentation(boundary: .internal, representation: .lowBit)
  expect acceptsRepresentation(boundary: .wExact, representation: .provenNiche)
  expect !acceptsRepresentation(boundary: .foreignC, representation: .provenNiche)
  expect acceptsRepresentation(boundary: .foreignC, representation: .nativeCarrier)
  expect !acceptsRepresentation(boundary: .internal, representation: .highBit)
  expect acceptsBoundary(value: HirBoundaryValue(
    boundary: .wExact,
    representation: .provenNiche,
    ownership: .owned,
    allocatorKnown: true,
  ))
  expect !acceptsBoundary(value: HirBoundaryValue(
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
  expect sameAbi(left: key, right: key)
  expect !sameAbi(left: key, right: HirAbiKey(
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
  expect placeIsSubplace(child: child, parent: parent)
  expect !placeIsSubplace(child: sibling, parent: parent)
  expect !placeIsSubplace(child: HirPlaceId(root: "kitchen", projections: []), parent: parent)
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
    left: value,
    right: HirDependentPayload(edges: [dynamicCopy], lifetimeIndependent: false),
  )
  let origins = projectOrigins(value: ref surviving)
  expect !surviving.lifetimeIndependent
  expect surviving.edges.count == 3
  expect origins.count == 2
  expect dependentMayReturn(value: surviving, surviving: ["menu"])
  expect !dependentMayReturn(value: surviving, surviving: [])
  expect dependencyPermits(edge: dynamic, access: .read)
  expect !dependencyPermits(edge: dynamic, access: .write)
  expect dependencyPermits(edge: exclusive, access: .write)
}

test "M1 pinned handle moves while payload root stays stable" {
  let payload = HirPlaceId(root: "pin:state", projections: [])
  let handle = pinHandle(payloadRoot: payload, handleRoot: "handle")
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
  let moved = movePinnedHandle(handle: handle, destination: "movedHandle")
  expect moved.handleRoot == "movedHandle"
  expect moved.payloadRoot.root == handle.payloadRoot.root
  expect loanCanSuspend(loan: loan, referentStable: true)
}

test "M1 bodyless interface mapping requires one unique source" {
  let mapping = deriveBodylessMapping(
    kind: .free,
    receiverCompatible: false,
    inputSlots: ["parameter:0"],
    resultSlots: ["result.title", "result.body"],
  )
  guard let mapping = mapping else { panic("mapping was rejected") }
  expect mapping.count == 2
  expect mapping[0].sources == ["parameter:0"]

  let ambiguous = deriveBodylessMapping(
    kind: .free,
    receiverCompatible: false,
    inputSlots: ["parameter:0", "parameter:1"],
    resultSlots: ["result"],
  )
  expect ambiguous == .none

  let initializer = deriveBodylessMapping(
    kind: .initializer,
    receiverCompatible: false,
    inputSlots: ["parameter:0"],
    resultSlots: ["result"],
  )
  expect initializer == .none

  let receiver = deriveBodylessMapping(kind: .instance, receiverCompatible: true, inputSlots: ["parameter:0"], resultSlots: ["result"])
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
  expect loanCanSuspend(loan: loan, referentStable: true)
  expect !loanCanSuspend(loan: loan, referentStable: false)
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

  expect !canCreateShared(payload: ref borrowed)
  expect !storageCanCrossDomain(storage: ref localStorage)
  expect storageCanCrossDomain(storage: ref processStorage)
}

test "M1 weak handle keeps only the control block alive" {
  let initial = HirSharedCounts(
    strong: 1,
    weak: 1,
    payloadAlive: true,
    blockAlive: true,
    deinitCount: 0,
  )
  let releasedStrong = releaseStrong(counts: initial)
  guard let releasedStrong = releasedStrong else { panic("strong release was rejected") }
  expect !releasedStrong.payloadAlive
  expect releasedStrong.blockAlive
  expect releasedStrong.deinitCount == 1

  let releasedWeak = releaseWeak(counts: releasedStrong)
  guard let releasedWeak = releasedWeak else { panic("weak release was rejected") }
  expect !releasedWeak.blockAlive
  expect releasedWeak.deinitCount == 1
}

test "ASC0 allocator slot is unique, ABI-visible, and position-preserving" {
  let valid = [HirAllocatorSlot(
    index: 2,
    name: "memory",
    kind: .contextual,
    abiVisible: true,
    resourceVisible: true,
  )]
  let duplicate = [
    HirAllocatorSlot(index: 2, name: "memory", kind: .contextual, abiVisible: true, resourceVisible: true),
    HirAllocatorSlot(index: 3, name: "other", kind: .contextual, abiVisible: true, resourceVisible: true),
  ]
  let hidden = [HirAllocatorSlot(
    index: 2,
    name: "memory",
    kind: .contextual,
    abiVisible: false,
    resourceVisible: true,
  )]
  expect validAllocatorSlots(slots: valid)
  expect !validAllocatorSlots(slots: duplicate)
  expect !validAllocatorSlots(slots: hidden)
}
