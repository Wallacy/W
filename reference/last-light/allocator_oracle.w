import iec from std

// Physical allocation and reclamation oracles for the Last Light restaurant.
// `arena` is retained only as an implementation strategy label in this oracle;
// source code uses a lexical allocator block.

export enum AllocatorProgress {
  general
  bounded
  lockFree
  waitFree
}

export enum AllocatorMobility {
  local
  crossDomain
}

export enum ResizeCapability {
  none
  inPlace
  remap
}

export enum AllocationUse {
  ordinary
  realTime
  interrupt
}

export enum ResizeResult {
  resized
  unsupported
  failed
}

export enum StorageOwner {
  unique
  fixedScope
  sharedPayload
  sharedControlBlock
  pinned
  taskFrame
  serviceInstance
  channelNode
  foreign
  device
}

export enum ReclamationGate {
  typedDrop
  bulkRelease
  strongZero
  weakZero
  addressDrain
  taskDrain
  serviceDrain
  queueDrain
  foreignDestroy
  deviceQuiescence
}

export struct AllocationLayout {
  let size: usize
  let alignment: usize
}

export struct AllocatorProviderProfile {
  let allocationProgress: AllocatorProgress
  let deallocationProgress: AllocatorProgress
  let mobility: AllocatorMobility
  let resize: ResizeCapability
  let maximumBytes: usize
  let maximumAlignment: usize
}

export const fn isPowerOfTwo(value: usize): Bool {
  return value > 0 && (value & (value - 1)) == 0
}

export const fn acceptsLayout(
  profile: ref AllocatorProviderProfile,
  layout: AllocationLayout,
): Bool {
  return isPowerOfTwo(value: layout.alignment)
    && layout.size <= profile.maximumBytes
    && layout.alignment <= profile.maximumAlignment
}

export const fn allocationCallsProvider(layout: AllocationLayout): Bool {
  return layout.size > 0
}

export const fn oldReceiptSurvives(result: ResizeResult): Bool {
  return result.one(.unsupported, .failed)
}

export const fn progressAccepts(
  use: AllocationUse,
  progress capability: AllocatorProgress,
): Bool {
  return switch use {
    case .ordinary: true
    case .realTime: capability.one(.bounded, .waitFree)
    case .interrupt: capability == .waitFree
  }
}

export const fn baselineReclamation(owner: StorageOwner): ReclamationGate {
  return switch owner {
    case .unique: .typedDrop
    case .fixedScope: .bulkRelease
    case .sharedPayload: .strongZero
    case .sharedControlBlock: .weakZero
    case .pinned: .addressDrain
    case .taskFrame: .taskDrain
    case .serviceInstance: .serviceDrain
    case .channelNode: .queueDrain
    case .foreign: .foreignDestroy
    case .device: .deviceQuiescence
  }
}

test "allocation layout is bounded before provider access" for acceptsLayout {
  let fixed = AllocatorProviderProfile(
    allocationProgress: .bounded,
    deallocationProgress: .bounded,
    mobility: .local,
    resize: .none,
    maximumBytes: 64<iec.KiB>,
    maximumAlignment: 64,
  )

  expect acceptsLayout(profile: fixed, layout: AllocationLayout(size: 4096, alignment: 16))
  expect !acceptsLayout(profile: fixed, layout: AllocationLayout(size: 4096, alignment: 3))
  expect !acceptsLayout(profile: fixed, layout: AllocationLayout(size: 128<iec.KiB>, alignment: 16))
}

test "zero-size and resize preserve receipt authority" for oldReceiptSurvives {
  expect !allocationCallsProvider(layout: AllocationLayout(size: 0, alignment: 1))
  expect allocationCallsProvider(layout: AllocationLayout(size: 1, alignment: 1))
  expect !oldReceiptSurvives(result: .resized)
  expect oldReceiptSurvives(result: .unsupported)
  expect oldReceiptSurvives(result: .failed)
}

test "progress requirements remain contextual" for progressAccepts {
  expect progressAccepts(use: .ordinary, progress: .general)
  expect progressAccepts(use: .realTime, progress: .bounded)
  expect !progressAccepts(use: .realTime, progress: .lockFree)
  expect progressAccepts(use: .interrupt, progress: .waitFree)
  expect !progressAccepts(use: .interrupt, progress: .bounded)
}

test "each owner has one baseline reclamation gate" for baselineReclamation {
  expect baselineReclamation(owner: .unique) == .typedDrop
  expect baselineReclamation(owner: .fixedScope) == .bulkRelease
  expect baselineReclamation(owner: .sharedPayload) == .strongZero
  expect baselineReclamation(owner: .sharedControlBlock) == .weakZero
  expect baselineReclamation(owner: .pinned) == .addressDrain
  expect baselineReclamation(owner: .taskFrame) == .taskDrain
  expect baselineReclamation(owner: .serviceInstance) == .serviceDrain
  expect baselineReclamation(owner: .channelNode) == .queueDrain
  expect baselineReclamation(owner: .foreign) == .foreignDestroy
  expect baselineReclamation(owner: .device) == .deviceQuiescence
}
