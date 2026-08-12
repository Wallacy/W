import * from std.memory

// Current baseline. Fixed storage is bounded and does not request OS storage.
export fn arenaScope(payload: ref String, memory: ref Allocator): usize throws AllocationError {
  var storage: [u8; 64<KiB>] = [0; 64<KiB>]
  var scratch = Arena.fixed(inout storage)
  var tokens = Array<String>(using: ref scratch)
  try tokens.tryReserve(minimumCapacity: payload.bytes.count)
  let token = try payload.tryDuplicate(using: ref scratch)
  tokens.append(take token)
  let owned = try (take tokens).rehome(using: memory)
  scratch.clear()
  return owned.count
}
