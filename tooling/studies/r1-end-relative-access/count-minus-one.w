// R1 Last Light end-relative access study variant.

fn lastLabel(items: ref Array<String>): ref String? {
  guard !items.isEmpty else return .none
  return items.get(items.count - 1)
}

test "count minus one preserves an optional empty outcome" for lastLabel {
  let menu = ["nebula broth", "horizon cake"]
  expect lastLabel(menu) == "horizon cake"
}
