import * from std

export struct DishId {
  raw: u32
}

fn menuCache(id: DishId): String {
  return "cache"
}

export fn menuLookup(id: DishId): String {
  return menuCache(id: id)
}
