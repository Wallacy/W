// Allocator capabilities and bounded arenas.
//
// Allocator is one opaque capability family. Arena is a distinct scoped
// monotonic capability. The compiler binds provider facts, origins, mobility,
// and dependent lifetimes to the validated handle. These facts are not source
// refinements. The private initializer prevents source from forging them.

foreign intrinsic from "std.memory@1" {
  type AllocatorHandle

  fn stdMemoryFixedArena<capacity: usize>(
    storage: inout [u8; capacity],
  ): AllocatorHandle

  fn stdMemoryResetArena(handle: inout AllocatorHandle)
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

  deinit {
    unsafe { stdMemoryDropAllocator(inout handle) }
  }
}

// Draft contract: Arena is intended as a nominal scoped capability. This
// source declaration does not prove sealing or refinement-to-base coercion;
// that interface remains provider/compiler work. Until that gate exists,
// allocating APIs receive Arena explicitly.
export struct Arena {
  handle: AllocatorHandle

  export static fn fixed<capacity: usize>(
    _ storage: inout [u8; capacity],
  ): Arena {
    let raw = unsafe { stdMemoryFixedArena(inout storage) }
    return Arena(handle: raw)
  }

  export mut fn reset() {
    unsafe { stdMemoryResetArena(inout handle) }
  }

  deinit {
    unsafe { stdMemoryDropAllocator(inout handle) }
  }
}
