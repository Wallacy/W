// Lexical allocator scope oracles for the Last Light restaurant.

import iec from std
import * from std.memory
import * from std.task

// Custom plan expressions conform to AllocatorPlan and publish an
// AllocatorPlanDescriptor; the compiler owns the AllocatorLease lifecycle.

export struct MenuSnapshot {
  title: String
  dishes: Array<String>
}

export fn stageMenu(
  allocator destination: ref Allocator,
  title: ref String,
  dishes menuDishes: ref Array<String>,
): MenuSnapshot throws AllocationError {
  // This fixture assumes the selected profile proves fixed admission
  // infallible. A dynamic reservation uses `try allocator`.
  // The body does not name this lease; the block is anonymous.
  allocator .fixed<capacity: 2<iec.MiB>> {
    var stagedDishes = Array<String>()
    try stagedDishes.tryReserve(minimumCapacity: menuDishes.count)

    for ref dish in menuDishes {
      let stagedDish = try dish.tryDuplicate()
      stagedDishes.append(take stagedDish)
    }

    let staged = MenuSnapshot(
      title: try title.tryDuplicate(),
      dishes: take stagedDishes,
    )
    // Rehome changes allocation origins. It does not erase a borrow origin.
    return try (take staged).rehome(allocator: destination)
  }
}

export fn countEmergencyTokens(source: ref String): usize throws AllocationError {
  // The body does not name the capability, so the scope is anonymous.
  allocator .fixed<capacity: 64<iec.KiB>> {
    var separators = Array<usize>()
    try separators.tryReserve(minimumCapacity: source.bytes.count)

    var offset: usize = 0
    for byte in source.bytes {
      if byte == b' ' { separators.append(offset) }
      offset += 1
    }

    return if source.bytes.count == 0 { 0 } else { separators.count + 1 }
  }
}

fn snapshotBytes(snapshot: take MenuSnapshot): usize {
  var total = snapshot.title.bytes.count
  for ref dish in snapshot.dishes {
    total += dish.bytes.count
  }
  return total
}

export async fn countStagedMenuInParallel(
  allocator processMemory: ref Allocator,
  title: ref String,
  dishes: ref Array<String>,
): usize throws AllocationError {
  // The callee publishes the contextual slot. The compiler inserts
  // `allocator: ref processMemory` at this call.
  let snapshot = try stageMenu(ref title, dishes: ref dishes)
  let count = spawn<.compute> snapshotBytes(take snapshot)
  return await count
}

fn nestedAllocatorScopes() {
  allocator outer: .fixed<capacity: 64<iec.KiB>> {
    allocator inner: .fixed<capacity: 64<iec.KiB>> {
      let local = Array<String>()
      // Explicit control argument overrides the innermost lease.
      let portable = Array<String>(allocator: outer)
    }
  }
}

fn rootDefaultConstruction(
  title: ref String,
  dishes: ref Array<String>,
): MenuSnapshot throws AllocationError {
  // The product/host general allocator is the root current allocator here.
  let names = Array<String>()
  // Root context also completes a contextual call when the profile publishes it.
  return try stageMenu(ref title, dishes: ref dishes)
}

fn ordinaryAllocatorParameter(allocator: ref Allocator) {
  // A common parameter named allocator is not a contextual slot.
  let names = Array<String>()
}

fn rootFallbackAfterIntermediary(
  title: ref String,
  dishes: ref Array<String>,
): MenuSnapshot throws AllocationError {
  // A function without the slot does not inherit the caller block. Its own
  // product root completes this call, or `.none` rejects it before the body.
  return try stageMenu(ref title, dishes: ref dishes)
}

type ContextualDecoder = fn(
  allocator memory: ref Allocator,
  ref String,
  ref Array<String>,
): MenuSnapshot throws AllocationError
const contextualDecoder: ContextualDecoder = stageMenu

test "a staged menu leaves its temporary allocator scope" for stageMenu {
  let title = "Menu at the Observable Edge"
  let dishes = ["Photon soup", "Patient comet cake"]

  allocator destination: .fixed<capacity: 8<iec.MiB>> {
    // The block is the current context for this contextual call.
    let snapshot = try stageMenu(ref title, dishes: ref dishes)
    expect snapshot.title == title
    expect snapshot.dishes == dishes
  }

  // Compile-fail assay: local fixed storage cannot cross an execution domain.
  // let local = Array<String>()
  // let invalid = spawn<.compute> consume(take local)
}

// Compile-fail assays kept source-shaped for the design oracle:
// - `memory: .none` rejects the omitted root allocator before the body.
// - a custom plan whose `open()` fails enters neither body nor binding.
// - an outer allocator is not captured by a stored/escaping closure without
//   an explicit capture or its own contextual slot.
