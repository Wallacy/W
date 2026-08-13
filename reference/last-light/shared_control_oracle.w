import iec from std
import * from std.memory

// SHC0 restaurant fixture: shared construction publishes payload and control
// block together. This source is an expected-use assay, not a runtime.

struct MenuSection {
  title: String
  parent: weak MenuSection?
  children: Array<shared MenuSection>
}

struct OrderEnvelope {
  menu: shared MenuSection
  requestId: String
}

fn makeRequestMenu(
  allocator memory: ref Allocator,
  title: String,
): shared MenuSection throws AllocationError {
  // `memory` is an already-open capability. This construction does not open a
  // plan; `try` covers only the published initializer/control-block sites.
  let root: shared MenuSection = try MenuSection(
    allocator: memory,
    title: take title,
    parent: .none,
    children: [],
  )
  return take root
}

fn weakParent(root: shared MenuSection): weak MenuSection? {
  let parent: weak MenuSection? = root.parent
  return parent
}

fn promoteExisting(
  allocator memory: ref Allocator,
  owner: MenuSection,
): shared MenuSection throws AllocationError {
  // The lexical capability is already admitted. The consuming `try (take
  // owner)` chooses its control-block origin without opening a plan here.
  let root: shared MenuSection = try (take owner)
  return take root
}

fn sharedFailure(
  allocator memory: ref Allocator,
  title: String,
): shared MenuSection throws AllocationError {
  let root: shared MenuSection = try MenuSection(
    allocator: memory,
    title: take title,
    parent: .none,
    children: [],
  )
  return take root
}

fn crossDomainOrder(
  allocator processMemory: ref Allocator,
  draft: MenuSection,
): shared MenuSection throws AllocationError {
  let portable = try (take draft).rehome(allocator: processMemory)
  // Promotion consumes the complete rehomed value. It does not rebuild a
  // partial menu and therefore preserves parent and children fields.
  let root: shared MenuSection = try (take portable)
  return take root
}

// Textual adversarials belong to SHC0's host corpus. This fixture does not
// execute allocator plans or refer to a RestaurantPool instance. The FFI
// source contract remains memory.w::watchClosingBell/BellLease.
