// Allocator capability contracts.
//
// `allocator name: plan { ... }` is the source surface. The compiler binds
// provider facts, origins, mobility, and dependent lifetimes to the validated
// handle. A bump arena can be an implementation strategy, but it is not a
// public W type or constructor.

foreign intrinsic from "std.memory@1" {
  type AllocatorHandle

  fn stdMemoryDropAllocator(_ handle: inout AllocatorHandle)
}

export struct BudgetExceeded: Copy & Equatable {
  let limitBytes: usize
  let committedBytes: usize
  let requestedBytes: usize
}

export enum AllocationError: Error {
  outOfMemory
  budgetExceeded(BudgetExceeded)
  sizeOverflow
  invalidLayout(size: usize, alignment: usize)
  unsupportedAlignment(usize)
}

// These values form the source-neutral descriptor accepted by a custom plan.
// They describe the contract; they do not expose a raw provider or a public
// allocation operation. Adoption families, progress classes, object limits and
// allocation domains are provider-profile/recipe facts. A descriptor without
// that join cannot prove a fallible shared construction.
export enum AllocatorFailureMode: Copy & Equatable {
  infallible
  fallible
}

export enum AllocatorDeallocator: Copy & Equatable {
  provider
  backing
}

export enum AllocatorMobility: Copy & Equatable {
  local
  crossDomain
}

export struct AllocatorPlanDescriptor: Copy & Equatable {
  let providerDigest: [u8; 32]
  let version: u32<(1...)>
  let failure: AllocatorFailureMode
  let deallocator: AllocatorDeallocator
  let mobility: AllocatorMobility
}

export struct Allocator {
  let handle: AllocatorHandle

  init(validatedRaw: AllocatorHandle) {
    self.handle = validatedRaw
  }

  deinit {
    // AllocatorLease deinit closes the provider lease exactly once.
    unsafe { stdMemoryDropAllocator(inout handle) }
  }
}

// A lease is the scoped Allocator owner acquired by the compiler's plan
// lowering. The alias gives the logical consuming-open contract a stable
// name; source cannot construct it from a raw handle.
export alias AllocatorLease = Allocator

// Custom plan expressions conform to this descriptor shape. Only the descriptor
// facts are source-neutral data. `open()` is an executable consuming hook that
// the compiler invokes before the body; users do not call open or close
// manually. AllocatorLease deinit closes the provider lease. Providers and
// lowering remain missing gates.
export protocol AllocatorPlan {
  const descriptor: AllocatorPlanDescriptor
  take fn open(): AllocatorLease throws AllocationError
}

// The compiler/provider owns plan acquisition and lowering. A `fixed` plan
// reserves storage under target/profile gates. A custom plan publishes the
// source-neutral AllocatorPlan descriptor: providerDigest, version, failure,
// deallocator, and mobility. These data-only facts are separate from the
// executable open hook and are joined with the provider profile/recipe before
// any payload or hidden shared control-block site is admitted. The compiler
// runs open before the body and performs structured drain and typed drops
// before AllocatorLease deinit. The source contract deliberately does not
// expose reset or manual close: a common allocator block closes, drains, drops,
// and then reclaims its storage as one lifecycle.
