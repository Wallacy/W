// R1 shared-construction study: selected overloaded operation.

struct MenuSection {
  title: String
  parent: weak MenuSection?
  children: Array<shared MenuSection>
}

fn makeRoot(title: String): shared MenuSection {
  return share(MenuSection(
    title: take title,
    parent: .none,
    children: [],
  ))
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

test "shared construction separates normal and recoverable allocation" {
  let root = makeRoot("Dinner")
  let local = try makeRoot("Supper", using: testAllocator)
  let observer = copy root
  expect observer.title == "Dinner"
  expect local.title == "Supper"
}
