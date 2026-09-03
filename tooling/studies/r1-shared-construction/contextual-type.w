// R1 shared-construction study: the written shared declaration is the operation.

struct MenuSection {
  let title: String
  let parent: weak MenuSection?
  let children: Array<shared MenuSection>
}

alias MaybeMenuSectionOwner = shared MenuSection?
alias SharedOptionalMenuSection = shared Option<MenuSection>

fn makeRoot(_ title: String): shared MenuSection {
  let root: shared MenuSection = MenuSection(
    title: take title,
    parent: .none,
    children: [],
  )
  return take root
}

fn makeRequestRoot(
  _ title: String,
  allocator memory: ref Allocator,
): shared MenuSection throws AllocationError {
  let root: shared MenuSection = try MenuSection(
    allocator: memory,
    title: take title,
    parent: .none,
    children: [],
  )
  return take root
}

test "shared construction is explicit in the declaration" {
  let root = makeRoot("Dinner")
  expect root.title == "Dinner"
}
