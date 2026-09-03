// R1 shared-construction study: separate recoverable verb.

struct MenuSection {
  let title: String
  let parent: weak MenuSection?
  let children: Array<shared MenuSection>
}

fn makeRoot(
  _ title: String,
  memory: ref Allocator,
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
