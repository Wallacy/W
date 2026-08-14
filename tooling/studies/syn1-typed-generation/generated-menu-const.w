import * from std

export struct DishId {
  raw: u32
}

export const defaultDish: u32 = 7

export fn menuLookup(id: DishId): String {
  return "menu"
}
