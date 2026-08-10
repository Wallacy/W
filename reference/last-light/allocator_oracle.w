// Physical allocation and reclamation oracles for the Last Light restaurant.

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
  arena
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
  size: usize
  alignment: usize
}

export struct AllocatorProviderProfile {
  allocationProgress: AllocatorProgress
  deallocationProgress: AllocatorProgress
  mobility: AllocatorMobility
  resize: ResizeCapability
  maximumBytes: usize
  maximumAlignment: usize
}

export const fn isPowerOfTwo(value: usize): Bool {
  return value > 0 && (value & (value - 1)) == 0
}

export const fn acceptsLayout(
  profile: ref AllocatorProviderProfile,
  layout: AllocationLayout,
): Bool {
  return isPowerOfTwo(layout.alignment)
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
  progress: AllocatorProgress,
): Bool {
  return switch use {
    case .ordinary: true
    case .realTime: progress.one(.bounded, .waitFree)
    case .interrupt: progress == .waitFree
  }
}

export const fn baselineReclamation(owner: StorageOwner): ReclamationGate {
  return switch owner {
    case .unique: .typedDrop
    case .arena: .bulkRelease
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
    maximumBytes: 64<KiB>,
    maximumAlignment: 64,
  )

  expect acceptsLayout(fixed, layout: AllocationLayout(size: 4096, alignment: 16))
  expect !acceptsLayout(fixed, layout: AllocationLayout(size: 4096, alignment: 3))
  expect !acceptsLayout(fixed, layout: AllocationLayout(size: 128<KiB>, alignment: 16))
}

test "zero-size and resize preserve receipt authority" for oldReceiptSurvives {
  expect !allocationCallsProvider(AllocationLayout(size: 0, alignment: 1))
  expect allocationCallsProvider(AllocationLayout(size: 1, alignment: 1))
  expect !oldReceiptSurvives(.resized)
  expect oldReceiptSurvives(.unsupported)
  expect oldReceiptSurvives(.failed)
}

test "progress requirements remain contextual" for progressAccepts {
  expect progressAccepts(.ordinary, progress: .general)
  expect progressAccepts(.realTime, progress: .bounded)
  expect !progressAccepts(.realTime, progress: .lockFree)
  expect progressAccepts(.interrupt, progress: .waitFree)
  expect !progressAccepts(.interrupt, progress: .bounded)
}

test "each owner has one baseline reclamation gate" for baselineReclamation {
  expect baselineReclamation(.unique) == .typedDrop
  expect baselineReclamation(.arena) == .bulkRelease
  expect baselineReclamation(.sharedPayload) == .strongZero
  expect baselineReclamation(.sharedControlBlock) == .weakZero
  expect baselineReclamation(.pinned) == .addressDrain
  expect baselineReclamation(.taskFrame) == .taskDrain
  expect baselineReclamation(.serviceInstance) == .serviceDrain
  expect baselineReclamation(.channelNode) == .queueDrain
  expect baselineReclamation(.foreign) == .foreignDestroy
  expect baselineReclamation(.device) == .deviceQuiescence
}
