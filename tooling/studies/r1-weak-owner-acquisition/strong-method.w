import * from std.memory

object MenuSection {
  title: String
}

// Retired alternative. The method spelling is rejected before W 1.0.
export fn acquireOwner(root: shared MenuSection): shared MenuSection? {
  let weakRoot = root.weak()
  let owner = weakRoot.strong()
  return owner
}
