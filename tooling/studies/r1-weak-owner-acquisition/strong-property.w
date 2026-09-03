import * from std.memory

object MenuSection {
  title: String
}

// Retired alternative. The property spelling is rejected before W 1.0.
export fn acquireOwner(_ root: shared MenuSection): shared MenuSection? {
  let weakRoot = root.weak()
  let owner = weakRoot.strong
  return owner
}
