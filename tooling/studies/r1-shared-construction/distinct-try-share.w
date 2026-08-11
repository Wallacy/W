// R1 shared-construction study: separate recoverable verb.

struct MenuSection {
  title: String
  parent: weak MenuSection?
  children: Array<shared MenuSection>
}

fn makeRoot(
  title: String,
  named memory: ref Allocator,
): shared MenuSection throws AllocationError {
  return try tryShare(
    MenuSection(
      title: take title,
      parent: .none,
      children: [],
    ),
    using: memory,
  )
}

test "shared construction uses a separate recoverable verb" {
  let root = try makeRoot("Dinner", memory: testAllocator)
  expect root.title == "Dinner"
}
