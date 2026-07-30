// Allocator and region oracles for the Last Light restaurant.

import std.memory

export struct MenuSnapshot {
  title: String
  dishes: Array<String>
}

export fn stageMenu(
  title: ref String,
  dishes: ref Array<String>,
  memory: ref Allocator,
): MenuSnapshot throws AllocationError {
  region staging(using: memory, limit: 2<MiB>) {
    var stagedDishes = Array<String>(using: staging)
    try stagedDishes.tryReserve(minimumCapacity: dishes.count)

    for ref dish in dishes {
      let stagedDish = try dish.tryDuplicate(using: staging)
      stagedDishes.append(take stagedDish)
    }

    let staged = MenuSnapshot(
      title: try title.tryDuplicate(using: staging),
      dishes: take stagedDishes,
    )
    return try (take staged).rehome(using: memory)
  }
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

test "a staged menu leaves its temporary region" for stageMenu {
  var storage: [u8; 64<KiB>] = [0; 64<KiB>]
  let destination = Arena.fixed(inout storage)
  let title = "Menu at the Observable Edge"
  let dishes = ["Photon soup", "Patient comet cake"]

  let snapshot = try stageMenu(title, dishes: dishes, memory: destination)

  expect snapshot.title == title
  expect snapshot.dishes == dishes
}
