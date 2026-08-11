// R1 shared-construction study: the written shared declaration is the operation.

struct MenuSection {
  title: String
  parent: weak MenuSection?
  children: Array<shared MenuSection>
}

alias MaybeMenuSectionOwner = shared MenuSection?
alias SharedOptionalMenuSection = shared Option<MenuSection>

fn makeRoot(title: String): shared MenuSection {
  let root: shared MenuSection = MenuSection(
    title: take title,
    parent: .none,
    children: [],
  )
  return root
}

test "shared construction is explicit in the declaration" {
  let root = makeRoot("Dinner")
  expect root.title == "Dinner"
}
