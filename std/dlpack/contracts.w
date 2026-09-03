// DLPack 1.3 carrier contracts for PYN4.
//
// This file is a parseable SDK draft. The provider is missing. ManagedTensor,
// imported owners and capsules are opaque, move-only and trusted in-process.

import tensor from std

export enum Flag: Copy & Equatable {
  readOnly
  producerCopied
  subbytePadded
}

export enum DLPackError: Error {
  providerMissing
  legacyTensorRejected
  majorVersionMismatch
  unknownMinorField
  unknownFlag
  unsupportedDType
  nonNativeEndian
  invalidLanes
  invalidRank
  invalidShape
  invalidStride
  invalidOffset
  overflow
  alignmentInsufficient
  provenanceMissing
  untrustedInput
  queueRequired
  queueMismatch
  queueNotReady
  deviceMismatch
  producerCopied
  layoutMismatch
  dynamicAlreadyBound
  capsuleConsumed
  releaseAlreadyCalled
  leaseAfterFinalization
  callbackEscaped
  inoutAliasUnknown
  limitExceeded
  closeFailed
  quarantineRequired
}

export enum ViewError<OperationFailure: Error>: Error {
  dlpack(DLPackError)
  operation(OperationFailure)
}

export struct Limits: Copy & Equatable {
  let maximumRank: usize<(1...)>
  let maximumDimension: u64<(1...)>
  let maximumElements: u64<(1...)>
  let maximumSpanBytes: u64<(1...)>
  let maximumMetadataBytes: u64<(1...)>
  let maximumControlBytes: u64<(1...)>
  let maximumLeases: usize<(1...)>
  let maximumReleaseJobs: usize<(1...)>
  let maximumWaitUnits: u64<(1...)>
  let maximumDeadlineUnits: u64<(1...)>

  export const init(
    maximumRank: usize<(1...)>,
    maximumDimension: u64<(1...)>,
    maximumElements: u64<(1...)>,
    maximumSpanBytes: u64<(1...)>,
    maximumMetadataBytes: u64<(1...)>,
    maximumControlBytes: u64<(1...)>,
    maximumLeases: usize<(1...)>,
    maximumReleaseJobs: usize<(1...)>,
    maximumWaitUnits: u64<(1...)>,
    maximumDeadlineUnits: u64<(1...)>,
  ) {
    self.maximumRank = maximumRank
    self.maximumDimension = maximumDimension
    self.maximumElements = maximumElements
    self.maximumSpanBytes = maximumSpanBytes
    self.maximumMetadataBytes = maximumMetadataBytes
    self.maximumControlBytes = maximumControlBytes
    self.maximumLeases = maximumLeases
    self.maximumReleaseJobs = maximumReleaseJobs
    self.maximumWaitUnits = maximumWaitUnits
    self.maximumDeadlineUnits = maximumDeadlineUnits
  }
}

export struct ManagedTensor {
  let handle: ManagedTensorHandle

  init(validatedHandle: ManagedTensorHandle) {
    self.handle = validatedHandle
  }

  // This synchronous drop is valid only for a provider-verified versioned,
  // unconsumed capsule with a synchronous-safe deleter. open and materialize
  // move the handle out, so this deinit never releases a consumed carrier.
  deinit { unsafe { stdDLPackDropUnconsumed(inout handle) } }
}

export struct ImportedTensor<Element, shape: StaticList<usize>> {
  let handle: ImportedTensorHandle

  init(validatedHandle: ImportedTensorHandle) {
    self.handle = validatedHandle
  }

  export mut async fn withView<Output, OperationFailure: Error>(
    body: some mut async fn(view Tensor<Element, shape>): Output throws OperationFailure,
  ): Output throws ViewError<OperationFailure> {
    return unsafe {
      try await stdDLPackWithView(inout handle, body)
    }
  }

  export take async fn close() throws DLPackError {
    unsafe { try await stdDLPackCloseImported(take handle) }
  }

}

export struct DynamicImportedTensor {
  let handle: DynamicImportedTensorHandle

  init(validatedHandle: DynamicImportedTensorHandle) {
    self.handle = validatedHandle
  }

  export take async fn bind<Element, shape: StaticList<usize>>():
    ImportedTensor<Element, shape> throws DLPackError {
    return ImportedTensor(validatedHandle: unsafe {
      try await stdDLPackBind(
        take handle,
        Element,
        shape,
      )
    })
  }

  export take async fn close() throws DLPackError {
    unsafe { try await stdDLPackCloseDynamic(take handle) }
  }
}

export async fn open<Element, shape: StaticList<usize>>(
  managed: take ManagedTensor,
  on queue: ref tensor.Queue?,
  limits: ref Limits,
): ImportedTensor<Element, shape> throws DLPackError {
  return ImportedTensor(validatedHandle: unsafe {
    try await stdDLPackOpen(
      take managed.handle,
      queue,
      limits,
      Element,
      shape,
    )
  })
}

export async fn openDynamic(
  managed: take ManagedTensor,
  on queue: ref tensor.Queue?,
  limits: ref Limits,
): DynamicImportedTensor throws DLPackError {
  return DynamicImportedTensor(validatedHandle: unsafe {
    try await stdDLPackOpenDynamic(
      take managed.handle,
      queue,
      limits,
    )
  })
}

export async fn materialize<Element, shape: StaticList<usize>>(
  managed: take ManagedTensor,
  target: ref tensor.Device,
  on queue: ref tensor.Queue?,
  limits: ref Limits,
): Tensor<Element, shape> throws DLPackError {
  return unsafe {
    try await stdDLPackMaterializeManaged(
      take managed.handle,
      target,
      queue,
      limits,
      Element,
      shape,
    )
  }
}

export async fn materialize<Element, shape: StaticList<usize>>(
  imported: take ImportedTensor<Element, shape>,
  target: ref tensor.Device,
  on queue: ref tensor.Queue?,
  limits: ref Limits,
): Tensor<Element, shape> throws DLPackError {
  return unsafe {
    try await stdDLPackMaterializeImported(
      take imported.handle,
      target,
      queue,
      limits,
    )
  }
}

export async fn export<Element, shape: StaticList<usize>>(
  value: take Tensor<Element, shape>,
  on queue: ref tensor.Queue?,
  limits: ref Limits,
): ManagedTensor throws DLPackError {
  return ManagedTensor(validatedHandle: unsafe {
    try await stdDLPackExport(
      take value,
      queue,
      limits,
    )
  })
}

foreign intrinsic from "std.dlpack@1" {
  type ManagedTensorHandle
  type ImportedTensorHandle
  type DynamicImportedTensorHandle

  fn stdDLPackDropUnconsumed(_ handle: inout ManagedTensorHandle)
  async fn stdDLPackOpen<Element, shape: StaticList<usize>>(
    _ managed: take ManagedTensorHandle,
    _ queue: ref tensor.Queue?,
    _ limits: ref Limits,
    _ element: Element,
    _ shape: shape,
  ): ImportedTensorHandle throws DLPackError
  async fn stdDLPackOpenDynamic(
    _ managed: take ManagedTensorHandle,
    _ queue: ref tensor.Queue?,
    _ limits: ref Limits,
  ): DynamicImportedTensorHandle throws DLPackError
  async fn stdDLPackBind<Element, shape: StaticList<usize>>(
    _ handle: take DynamicImportedTensorHandle,
    _ element: Element,
    _ shape: shape,
  ): ImportedTensorHandle throws DLPackError
  async fn stdDLPackWithView<Element, shape: StaticList<usize>, Output,
    OperationFailure: Error>(
    _ handle: inout ImportedTensorHandle,
    _ body: some mut async fn(view Tensor<Element, shape>): Output throws OperationFailure,
  ): Output throws ViewError<OperationFailure>
  async fn stdDLPackCloseImported(_ handle: take ImportedTensorHandle) throws DLPackError
  async fn stdDLPackCloseDynamic(_ handle: take DynamicImportedTensorHandle) throws DLPackError
  async fn stdDLPackMaterializeManaged<Element, shape: StaticList<usize>>(
    _ managed: take ManagedTensorHandle,
    _ target: ref tensor.Device,
    _ queue: ref tensor.Queue?,
    _ limits: ref Limits,
    _ element: Element,
    _ shape: shape,
  ): Tensor<Element, shape> throws DLPackError
  async fn stdDLPackMaterializeImported<Element, shape: StaticList<usize>>(
    _ imported: take ImportedTensorHandle,
    _ target: ref tensor.Device,
    _ queue: ref tensor.Queue?,
    _ limits: ref Limits,
  ): Tensor<Element, shape> throws DLPackError
  async fn stdDLPackExport<Element, shape: StaticList<usize>>(
    _ value: take Tensor<Element, shape>,
    _ queue: ref tensor.Queue?,
    _ limits: ref Limits,
  ): ManagedTensorHandle throws DLPackError
}
