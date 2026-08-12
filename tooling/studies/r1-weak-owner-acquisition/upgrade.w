import * from std.memory

object MenuSection {
  title: String
}

// Current baseline. weak never reads the payload. upgrade linearizes with
// the final strong release and returns an optional new owner.
export fn acquireOwner(root: shared MenuSection): shared MenuSection? {
  let weakRoot = root.weak()
  guard let owner = weakRoot.upgrade() else return .none
  return .some(owner)
}
