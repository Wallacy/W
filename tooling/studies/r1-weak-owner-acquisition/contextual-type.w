import * from std.memory

object MenuSection {
  title: String
}

// Current baseline. The expected target supplies the weak context.
export fn acquireOwner(root: shared MenuSection): shared MenuSection? {
  let weakRoot: weak MenuSection? = root
  guard let owner = weakRoot else return .none
  return .some(owner)
}
