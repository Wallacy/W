import * from std.memory

object MenuSection {
  let title: String
}

// Retired alternative. W no longer provides an upgrade language call.
export fn acquireOwner(_ root: shared MenuSection): shared MenuSection? {
  let weakRoot = root.weak()
  guard let owner = weakRoot.upgrade() else return .none
  return .some(owner)
}
