import * from std

export fn menuLookup(id: DishId): String {
  return "menu"
}

export struct DishId {
  raw: u32
}
