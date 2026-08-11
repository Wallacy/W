// Allocator capabilities and bounded arenas.
//
// Allocator is one opaque capability family. Arena is a transparent
// refinement of that family; it is not a second allocation protocol. The
// compiler binds `.arena`, `.crossDomain`, origins, and dependent lifetimes to
// the validated handle returned by the provider. The private initializer
// prevents source from forging these facts. Transparent alias member lookup
// makes `Arena.fixed` resolve to `Allocator.fixed`.

foreign intrinsic from "std.memory@1" {
  type AllocatorHandle

  fn stdMemoryFixedArena<capacity: usize>(
    storage: inout [u8; capacity],
  ): AllocatorHandle

  fn stdMemoryClearArena(handle: inout AllocatorHandle)
  fn stdMemoryDropAllocator(handle: inout AllocatorHandle)
}

export struct BudgetExceeded: Copy & Equatable {
  limitBytes: usize
  committedBytes: usize
  requestedBytes: usize
}

export enum AllocationError: Error {
  outOfMemory
  budgetExceeded(BudgetExceeded)
  sizeOverflow
  invalidLayout(size: usize, alignment: usize)
  unsupportedAlignment(usize)
}

export struct Allocator {
  handle: AllocatorHandle

  init(validatedRaw: AllocatorHandle) {
    self.handle = validatedRaw
  }

  export static fn fixed<capacity: usize>(
    _ storage: inout [u8; capacity],
  ): Arena {
    let raw = unsafe { stdMemoryFixedArena(inout storage) }
    return Allocator(validatedRaw: raw)
  }

  deinit {
    unsafe { stdMemoryDropAllocator(inout handle) }
  }
}

export alias Arena = Allocator<(.arena)>

extension Allocator<(.arena)> {
  export mut fn clear() {
    unsafe { stdMemoryClearArena(inout handle) }
  }
}
