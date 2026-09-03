import * from std

export struct DishId {
  let raw: u64
}

export fn menuLookupField(id: DishId): String {
  return "menu"
}
