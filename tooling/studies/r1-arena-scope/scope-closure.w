import * from std.memory

// Research alternative. The closure is the lexical cleanup boundary.
export fn arenaScope(payload: ref String, memory: ref Allocator): usize throws AllocationError {
  var storage: [u8; 64<KiB>] = [0; 64<KiB>]
  let run = <[take storage, ref payload, ref memory]> () => {
    var scratch = Arena.fixed(inout storage)
    var tokens = Array<String>(using: ref scratch)
    try tokens.tryReserve(minimumCapacity: payload.bytes.count)
    let token = try payload.tryDuplicate(using: ref scratch)
    tokens.append(take token)
    let owned = try (take tokens).rehome(using: memory)
    scratch.clear()
    return owned.count
  }
  return try run()
}
