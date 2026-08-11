// R1 shared-construction study: every first owner is fallible.

struct MenuSection {
  title: String
  parent: weak MenuSection?
  children: Array<shared MenuSection>
}

fn makeRoot(
  title: String,
  memory: ref Allocator,
): shared MenuSection throws AllocationError {
  return try share(
    MenuSection(
      title: take title,
      parent: .none,
      children: [],
    ),
    using: memory,
  )
}

test "shared construction carries allocation recovery" {
  let root = try makeRoot("Dinner", memory: testAllocator)
  expect root.title == "Dinner"
}
