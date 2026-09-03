import * from std.memory

// Rejected alternative. The allocator control argument is not a generic slot.
export fn allocateNames(label: String, allocator memory: ref Allocator): Array<String> {
  var names = Array<String, allocator: memory>()
  names.append("Dinner")
  return names
}
