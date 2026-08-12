import * from std.memory

// Research alternative. Tree-sitter shapes this envelope. R1 evaluates a
// contextual runtime slot without promoting memory into type identity.
export fn allocateNames(memory: ref Allocator): Array<String> {
  var names = Array<String, using: memory>()
  names.append("Dinner")
  return names
}
