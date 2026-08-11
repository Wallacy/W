// Allocator and Arena oracles for the Last Light restaurant.

import * from std.memory
import * from std.task

export struct MenuSnapshot {
  title: String
  dishes: Array<String>
}

export fn stageMenu(
  title: ref String,
  dishes menuDishes: ref Array<String>,
  memory destination: ref Allocator,
): MenuSnapshot throws AllocationError {
  var storage: [u8; 2<MiB>] = [0; 2<MiB>]
  var staging = Arena.fixed(inout storage)
  defer { staging.clear() }
  var stagedDishes = Array<String>(using: staging)
  try stagedDishes.tryReserve(minimumCapacity: menuDishes.count)

  for ref dish in menuDishes {
    let stagedDish = try dish.tryDuplicate(using: staging)
    stagedDishes.append(take stagedDish)
  }

  let staged = MenuSnapshot(
    title: try title.tryDuplicate(using: staging),
    dishes: take stagedDishes,
  )
  // Rehome changes allocation origins. It does not erase a borrow origin.
  // The result interface maps owned storage to the `memory` parameter.
  return try (take staged).rehome(using: destination)
}

export fn countEmergencyTokens(source: ref String): usize throws AllocationError {
  var storage: [u8; 64<KiB>] = [0; 64<KiB>]
  let scratch = Arena.fixed(inout storage)
  var separators = Array<usize>(using: scratch)
  try separators.tryReserve(minimumCapacity: source.bytes.count)

  var offset: usize = 0
  for byte in source.bytes {
    if byte == b' ' { separators.append(offset) }
    offset += 1
  }

  return if source.bytes.count == 0 { 0 } else { separators.count + 1 }
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
  processMemory: ref Allocator<(.crossDomain)>,
): usize throws AllocationError {
  let snapshot = try stageMenu(title, dishes: dishes, memory: processMemory)
  spawn<.compute> let count = snapshotBytes(take snapshot)
  return await count
}

test "a staged menu leaves its temporary Arena" for stageMenu {
  var storage: [u8; 64<KiB>] = [0; 64<KiB>]
  let destination = Arena.fixed(inout storage)
  let title = "Menu at the Observable Edge"
  let dishes = ["Photon soup", "Patient comet cake"]

  let snapshot = try stageMenu(title, dishes: dishes, memory: destination)

  expect snapshot.title == title
  expect snapshot.dishes == dishes

  // Compile-fail assay: fixed arena storage is local to this execution domain.
  // let count = await countStagedMenuInParallel(title, dishes, destination)
}
