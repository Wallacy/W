// ATOM1 current route for pointer-like identity.

module atom1_generational_handle

import atomic from std

export struct MenuHandle: Duplicable {
  let slot: u32
  let generation: u32
}

export struct Menu: Duplicable {
  let name: String
}

export struct MenuSlot {
  let generation: u32
  let menu: Menu
}

fn pack(_ handle: MenuHandle): u64 {
  return (u64(handle.generation) << 32) | u64(handle.slot)
}

fn unpack(_ packed: u64): MenuHandle {
  return MenuHandle(
    slot: u32(packed & 0xffffffff),
    generation: u32(packed >> 32),
  )
}

export object MenuOwnerTable {
  let slots: Array<MenuSlot>

  export init(_ initial: take Array<MenuSlot>) {
    self.slots = take initial
  }

  fn resolve(_ handle: MenuHandle): Menu? {
    guard handle.slot < slots.count else return .none
    let slot = slots[handle.slot]
    guard slot.generation == handle.generation else return .none
    let menu = slot.menu
    return .some(Menu(name: menu.name))
  }
}

export object MenuHandleIndex {
  var atomic current: u64 = 0

  export init() {}

  fn publish(_ handle: MenuHandle) {
    // The owner table stores Menu. This scalar stores both u32 handle fields.
    current.store<.release>(pack(handle))
  }

  fn observe(): MenuHandle {
    return unpack(current.load<.acquire>())
  }
}
