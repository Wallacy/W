// R1 Last Light end-relative access study variant.

fn lastLabel(_ items: ref Array<String>): ref String? {
  guard !items.isEmpty else return .none
  return .some(items[-1])
}

test "negative index preserves an optional empty outcome" for lastLabel {
  let menu = ["nebula broth", "horizon cake"]
  expect lastLabel(menu) == "horizon cake"
}
