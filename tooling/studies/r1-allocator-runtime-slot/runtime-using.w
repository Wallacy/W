import * from std.memory

// Current baseline. The runtime capability is a call argument.
export fn allocateNames(memory: ref Allocator): Array<String> {
  var names = Array<String>(using: memory)
  names.append("Dinner")
  return names
}
