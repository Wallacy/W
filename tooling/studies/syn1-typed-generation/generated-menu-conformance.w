import * from std

export struct DishId: Hashable {
  let raw: u32
}

export fn menuLookup(id: DishId): String {
  return "menu"
}
