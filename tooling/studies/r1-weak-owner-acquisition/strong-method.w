import * from std.memory

object MenuSection {
  title: String
}

// Research alternative. The method must retain the same atomic boundary.
export fn acquireOwner(root: shared MenuSection): shared MenuSection? {
  let weakRoot = root.weak()
  let owner = weakRoot.strong()
  return owner
}
