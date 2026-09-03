// ATOM2 library composition witness. The generation never wraps.

module atom2_generational_handle

import atomic from std

export struct MenuHandle: Duplicable {
  let slot: u32
  let generation: u32
}

export object MenuOwnerTable {
  fn resolve(_ handle: MenuHandle): Menu? {
    // The table checks generation before it reads Menu.
    return .none
  }

  fn allocate(_ slot: u32, previousGeneration: u32): MenuHandle? {
    guard previousGeneration < 0xffffffff else return .none
    return .some(MenuHandle(slot: slot, generation: previousGeneration + 1))
  }
}

export struct Menu: Duplicable {
  let name: String
}
