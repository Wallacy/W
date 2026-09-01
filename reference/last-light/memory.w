// Memory oracles for the Last Light restaurant.

import * from std.memory
import std.task

foreign c from "last_light_bell.h" {
  type ll_bell
  type ll_registration

  fn ll_bell_close(bell: c.ptr<ll_bell>)
  fn ll_bell_subscribe(
    bell: c.ptr<ll_bell>,
    context: c.ptr<c.void>,
  ): c.ptr<ll_registration>?
  fn ll_bell_unsubscribe(registration: c.ptr<ll_registration>)
}

export enum BellError: Error {
  closed
  registrationFailed
  allocation(AllocationError)
}

// This enum can use null as an internal niche. Source code does not request
// that layout, and every C boundary still receives a canonical pointer.
export enum BellTarget {
  open(c.ptr<ll_bell>)
  unavailable
}

// The return contract excludes corrupted. Adding it later makes callers review
// every exhaustive switch.
export enum BellSignal {
  ringing(shared BellState)
  silent
  unavailable
  corrupted(BellError)
}

// Children own descendants. A child does not keep its parent alive.
export object MenuSection {
  title: String
  parent: weak MenuSection?
  children: Array<shared MenuSection>

  fn parentTitle(): String? {
    guard let parent = parent else return .none
    return .some(copy parent.title)
  }
}

export fn weakUpgradeSuccess(root: shared MenuSection): String? {
  let weakRoot: weak MenuSection? = root
  guard let owner = weakRoot else return .none
  return .some(copy owner.title)
}

export fn weakAfterLastOwner(root: take shared MenuSection): weak MenuSection? {
  let weakRoot: weak MenuSection? = root
  return weakRoot
}

export fn weakPreserved(root: shared MenuSection): weak MenuSection? {
  let weakRoot: weak MenuSection? = root
  let other: weak MenuSection? = weakRoot
  return other
}

test "weak binding reads a new strong owner while payload lives" {
  let root: shared MenuSection = MenuSection(
    title: "Dinner",
    parent: .none,
    children: [],
  )
  let title = weakUpgradeSuccess(copy root)
  expect title == .some("Dinner")
}

test "weak binding reads none after the last strong owner" {
  let root: shared MenuSection = MenuSection(
    title: "Expired",
    parent: .none,
    children: [],
  )
  // `take root` consumes the last strong owner before this weak read.
  let weakRoot = weakAfterLastOwner(take root)
  if let _ = weakRoot {
    expect false
  } else {
    expect true
  }
}

export enum SharedCyclePhase {
  compile
  drainedBoundary
}

export enum SharedEdgeRelease {
  deinitOnly
  explicitClose
  lifecycleDrain
  weak
}

export enum SharedCycleDisposition {
  accepted
  dynamic
  breakRequired
  unbreakable
  auditBeforeDrain
  residual
}

export const fn expectedSharedCycleDisposition(
  named phase: SharedCyclePhase,
  named forward: SharedEdgeRelease,
  named backward: SharedEdgeRelease,
  named closed: Bool,
  named drained: Bool,
  named rootReachesCycle: Bool,
): SharedCycleDisposition {
  if forward == .weak || backward == .weak { return .accepted }

  return switch phase {
    case .compile:
      if !closed {
        .dynamic
      } else if forward == .deinitOnly && backward == .deinitOnly {
        .unbreakable
      } else {
        .breakRequired
      }
    case .drainedBoundary:
      if !drained {
        .auditBeforeDrain
      } else if rootReachesCycle {
        .accepted
      } else {
        .residual
      }
  }
}

export object MenuObserverHub {
  callbacks: shared Array<any fn(): ()>

  export init() {
    self.callbacks = []
  }

  fn observe(callback: take any fn(): ()) {
    lock callbacks as items {
      items.append(take callback)
    }
  }

  fn observerCount(): usize {
    return lock callbacks as items { items.count }
  }
}

// The hub owns the callback, but the callback does not own the hub. Replacing
// `weak` with `copy` would form a closed strong cycle whose two edges depend on
// the destructors inside that same cycle.
export fn installMenuObserver(hub: shared MenuObserverHub) {
  hub.observe(<[weak hub]> () => {
    guard let hub = hub else return ()
    if hub.observerCount() == 0 { return () }
  })
}

export fn sameMenuSection(left: ref MenuSection, right: ref MenuSection): Bool {
  return left.isSameInstance(as: right)
}

test "weak capture and drained census classify cycles" for expectedSharedCycleDisposition {
  expect expectedSharedCycleDisposition(
    phase: .compile,
    forward: .deinitOnly,
    backward: .weak,
    closed: true,
    drained: false,
    rootReachesCycle: true,
  ) == .accepted
  expect expectedSharedCycleDisposition(
    phase: .compile,
    forward: .deinitOnly,
    backward: .deinitOnly,
    closed: true,
    drained: false,
    rootReachesCycle: true,
  ) == .unbreakable
  expect expectedSharedCycleDisposition(
    phase: .drainedBoundary,
    forward: .deinitOnly,
    backward: .explicitClose,
    closed: false,
    drained: true,
    rootReachesCycle: false,
  ) == .residual
}

// Common construction uses the product allocator and the normal OOM policy.
export fn makeMenuRoot(title: String): shared MenuSection {
  let root: shared MenuSection = MenuSection(
    title: take title,
    parent: .none,
    children: [],
  )
  return take root
}

// An expression that needs a shared owner creates a binding first. The move is
// explicit and no argument or return context promotes the owner implicitly.
export fn makeRequestMenuRoot(title: String): shared MenuSection {
  let owner = MenuSection(
    title: take title,
    parent: .none,
    children: [],
  )
  let root: shared MenuSection = take owner
  return take root
}

// The borrowed scalar view remains live across one suspension. The compiler
// must keep the owner stable in the task frame or reject the function.
export async fn announceAfterYield(section: ref MenuSection): String {
  let title = section.title.scalars
  await execution#yield()
  return title.materialize()
}

export struct BellState {
  label: String
  var atomic rings: u64
}

// Address observes the current storage. It does not keep state alive and cannot
// be converted back to a pointer without an existing provenance source.
export fn bellStorageAddress(state: ref BellState): Address {
  return address(of: state)
}

export fn foreignBellAddress(target: BellTarget): Address? {
  return switch target {
    case .open(let pointer): .some(pointer.address)
    case .unavailable: .none
  }
}

export fn classifyBell(state: shared BellState?): BellSignal<[.ringing, .silent, .unavailable]> {
  guard let state = state else return .unavailable
  if state.rings == 0 { return .silent }
  return .ringing(state)
}

export fn describeBell(signal: BellSignal<[.ringing, .silent, .unavailable]>): String {
  return switch signal {
    case .ringing(let state): "${state.label}: ${state.rings}"
    case .silent: "silent"
    case .unavailable: "unavailable"
  }
}

// These are three logical states. The compiler cannot collapse the two forms
// of absence when it selects a niche or an explicit tag.
export struct BellDiscovery {
  current: Option<Option<shared BellState>>
}

export object BellHandle {
  handle: c.ptr<ll_bell>?

  deinit {
    if let handle = handle {
      unsafe { ll_bell_close(handle) }
    }
  }
}

// Moving this lease may move the Pinned handle. It does not move BellState.
export object BellLease {
  bell: BellHandle
  registration: c.ptr<ll_registration>?
  state: Pinned<BellState>

  deinit {
    if let registration = registration {
      unsafe { ll_bell_unsubscribe(registration) }
    }
  }
}

export fn watchClosingBell(
  bell: take BellHandle,
  state: take BellState,
): BellLease throws BellError {
  guard let handle = bell.handle else throw .closed
  // Allocation failure consumes and drops `state`. No callback address has
  // been published at that point.
  let pinned = try (pin take state)
    .mapError((error) => .allocation(error))
  let registration = unsafe {
    ll_bell_subscribe(handle, pinned.asOpaqueCPtr())
  }

  guard let registration = registration else throw .registrationFailed
  return BellLease(
    bell: take bell,
    registration: registration,
    state: take pinned,
  )
}

// A direct constructor under `pin` initializes BellState at its final stable
// address. No complete BellState temporary exists before publication.
export fn watchNewClosingBell(
  bell: take BellHandle,
  label: String,
): BellLease throws BellError {
  guard let handle = bell.handle else throw .closed
  let pinned = try (pin BellState(label: take label, rings: 0))
    .mapError((error) => .allocation(error))
  let registration = unsafe {
    ll_bell_subscribe(handle, pinned.asOpaqueCPtr())
  }

  guard let registration = registration else throw .registrationFailed
  return BellLease(
    bell: take bell,
    registration: registration,
    state: take pinned,
  )
}

test "Address observes storage without creating authority" for bellStorageAddress {
  let state = BellState(label: "Closing bell", rings: 0)
  let first = bellStorageAddress(state)
  let second = bellStorageAddress(state)
  let sameBits = first.withBits(first.bits)

  expect first == second
  expect sameBits == first
  expect first.bits == second.bits

  // Compile-fail assay: W v0 has no Address.toPointer().
  // let forged = first.toPointer<BellState>()
}
