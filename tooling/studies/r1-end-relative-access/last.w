// R1 Last Light end-relative access study variant.

fn lastLabel(items: ref Array<String>): ref String? {
  return items.last
}

test "last access preserves an optional empty outcome" for lastLabel {
  let menu = ["nebula broth", "horizon cake"]
  expect lastLabel(menu) == "horizon cake"
}
