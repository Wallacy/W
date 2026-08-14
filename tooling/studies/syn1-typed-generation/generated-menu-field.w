import * from std

export struct DishId {
  raw: u64
}

export fn menuLookup(id: DishId): String {
  return "menu"
}
