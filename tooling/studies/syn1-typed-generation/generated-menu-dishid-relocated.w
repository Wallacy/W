import * from std

export struct DishId {
  raw: u32
}

export fn menuLookup(id: DishId): String {
  return "menu"
}
