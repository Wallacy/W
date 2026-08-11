// R1 shared-construction study: expected type performs the promotion.

struct MenuSection {
  title: String
  parent: weak MenuSection?
  children: Array<shared MenuSection>
}

fn makeRoot(title: String): shared MenuSection {
  let root: shared MenuSection = MenuSection(
    title: take title,
    parent: .none,
    children: [],
  )
  return root
}

test "shared construction relies on contextual promotion" {
  let root = makeRoot("Dinner")
  expect root.title == "Dinner"
}
