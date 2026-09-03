import secret from hidden_menu

export struct DishId {
  let raw: u32
}

export fn menuLookup(id: DishId): String {
  return "menu"
}
