import * from std

export fn menuLookup(id: DishId): String {
  return "menu"
}

export struct DishId {
  let raw: u32
}
