import * from std

export struct DishId {
  let raw: u64
}

export fn menuLookup(id: DishId): String {
  return "menu"
}
