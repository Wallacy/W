import * from std.memory

// Current baseline. allocator is reserved in the construction expression.
export fn allocateNames(label: String, allocator memory: ref Allocator): Array<String> {
  var names = Array<String>(allocator: memory)
  names.append("Dinner")
  return names
}

// CustomObj(allocator: memory, a: 1, b: 2) is valid when its contract publishes
// an allocatable storage site. A non-allocating constructor rejects allocator.
