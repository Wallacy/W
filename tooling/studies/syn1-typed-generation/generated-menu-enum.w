import * from std

export struct DishId {
  raw: u32
}

export enum ServiceMode {
  pickup
  serve
}

export fn menuLookup(id: DishId): String {
  return "menu"
}
