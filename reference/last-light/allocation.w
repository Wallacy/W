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
  allocator scratch: .fixed<capacity: 2<iec.MiB>> {
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
  allocator scratch: .fixed<capacity: 64<iec.KiB>> {
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
  title: ref String,
  dishes: ref Array<String>,
  processMemory: ref Allocator,
): usize throws AllocationError {
  let snapshot = try stageMenu(allocator: ref processMemory, ref title, dishes: ref dishes)
  spawn<.compute> let count = snapshotBytes(take snapshot)
  return await count
}

test "a staged menu leaves its temporary allocator scope" for stageMenu {
  let title = "Menu at the Observable Edge"
  let dishes = ["Photon soup", "Patient comet cake"]

  allocator destination: .fixed<capacity: 8<iec.MiB>> {
    let snapshot = try stageMenu(allocator: ref destination, ref title, dishes: ref dishes)
    expect snapshot.title == title
    expect snapshot.dishes == dishes
  }

  // Compile-fail assay: local fixed storage cannot cross an execution domain.
  // let count = await countStagedMenuInParallel(
  //   ref title,
  //   dishes: ref dishes,
  //   processMemory: ref destination,
  // )
}
