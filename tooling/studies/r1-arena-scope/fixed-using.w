import iec from std
import * from std.memory

// Current baseline. Fixed storage is bounded and does not request OS storage.
export fn arenaScope(_ payload: ref String, _ memory: ref Allocator): usize throws AllocationError {
  var storage: [u8; 64<iec.KiB>] = [0; 64<iec.KiB>]
  var scratch = Arena.fixed(inout storage)
  var tokens = Array<String>(allocator: ref scratch)
  try tokens.tryReserve(minimumCapacity: payload.bytes.count)
  let token = try payload.tryDuplicate(allocator: ref scratch)
  tokens.append(take token)
  let owned = try (take tokens).rehome(allocator: memory)
  scratch.reset()
  return owned.count
}
