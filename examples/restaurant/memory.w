// Memory oracles for the Last Light restaurant.

import std.memory
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
    guard let parent = parent.upgrade() else return .none
    return .some(copy parent.title)
  }
}

export fn makeMenuRoot(
  title: String,
  memory: ref Allocator,
): shared MenuSection throws AllocationError {
  return try share(
    MenuSection(
      title: take title,
      parent: .none,
      children: [],
    ),
    using: memory,
  )
}

// The borrowed scalar view remains live across one suspension. The compiler
// must keep the owner stable in the task frame or reject the function.
export async fn announceAfterYield(section: ref MenuSection): String {
  let title = section.title.scalars
  await Task.yield()
  return title.toString()
}

export struct BellState {
  label: String
  var atomic rings: u64
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
