/// Generic protocols, primary associated types, and deterministic witnesses.
export protocol Source<Item> {
  fn first(): ref Item?
}

export protocol Counted {
  fn count(): usize
}

export protocol Catalog<Item>: Source<Item> & Counted {}

export struct Shelf<T> {
  values: Array<T>
}

extension<T: Display & Equatable> Shelf<T>: Catalog {
  alias Item = T

  fn first(): ref T? {
    return values.first
  }

  fn count(): usize {
    return values.count
  }
}

export fn firstEquals<T: Equatable, S: Source<T>>(
  source: ref S,
  expected: ref T,
): Bool {
  guard let ref item = source.first() else return false
  return item == expected
}

export fn renderFirst<T: Display, S: Source<T>>(source: ref S): String? {
  guard let ref item = source.first() else return .none
  return item.display()
}

test "generic inference uses the declared witness" for firstEquals {
  let shelf = Shelf(values: ["Pan-Galactic Broth", "Horizon Cake"])

  expect firstEquals(shelf, expected: "Pan-Galactic Broth")
  expect renderFirst(shelf) == "Pan-Galactic Broth"
  expect shelf.count() == 2
}
