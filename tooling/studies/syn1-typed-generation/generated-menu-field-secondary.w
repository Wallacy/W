import * from std

export struct DishId {
  raw: u64
}

export fn menuLookupField(id: DishId): String {
  return "menu"
}
