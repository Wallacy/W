// Tensor carrier and provider-scoped device contracts for PYN4.
//
// This file is a parseable SDK draft. The provider is missing. Device and
// queue handles are opaque capabilities. No raw pointer is part of safe W.

export enum DeviceKind: Copy & Equatable {
  cpu
  cuda
  cudaHost
  opencl
  vpi
  extDev
  cudaManaged
  rocm
  rocmHost
  metal
  vulkan
  oneapi
  webgpu
  hexagon
  maia
  trainium
  tpu
  tpuHost
}

export enum DeviceError: Error {
  providerMissing
  identityInvalid
  providerMismatch
  unsupportedKind
}

export enum QueueError: Error {
  providerMissing
  deviceMismatch
  providerReceiptMissing
  cpuQueueNotRequired
}

export enum TensorError: Error {
  providerMissing
  deviceMismatch
  queueMismatch
  layoutMismatch
  limitExceeded
  transferCancelled
}

export struct Device {
  let handle: DeviceHandle

  init(validatedHandle: DeviceHandle) {
    self.handle = validatedHandle
  }

  export fn kind(): DeviceKind {
    return unsafe { stdTensorDeviceKind(ref handle) }
  }

  export fn same(as other: ref Device): Bool {
    return unsafe {
      stdTensorDeviceSame(ref handle, ref other.handle)
    }
  }
}

export struct Queue {
  let handle: QueueHandle

  init(validatedHandle: QueueHandle) {
    self.handle = validatedHandle
  }

  export fn device(): Device {
    return Device(validatedHandle: unsafe {
      stdTensorQueueDevice(ref handle)
    })
  }

}

export struct Limits: Copy & Equatable {
  let maximumElements: u64<(1...)>
  let maximumBytes: u64<(1...)>
  let maximumMetadataBytes: u64<(1...)>

  export const init(
    maximumElements: u64<(1...)>,
    maximumBytes: u64<(1...)>,
    maximumMetadataBytes: u64<(1...)>,
  ) {
    self.maximumElements = maximumElements
    self.maximumBytes = maximumBytes
    self.maximumMetadataBytes = maximumMetadataBytes
  }
}

// Tensor is a core W head. This module does not redefine its storage or make
// every Tensor depend on the provider. These adapters keep device operations
// explicit at the std boundary while the core owns Tensor<Element, shape>.
export async fn deviceOf<Element, shape: StaticList<usize>>(
  source: ref Tensor<Element, shape>,
): Device throws TensorError {
  return Device(validatedHandle: unsafe {
    try await stdTensorDevice(ref source)
  })
}

export async fn transfer<Element, shape: StaticList<usize>>(
  source: take Tensor<Element, shape>,
  target: ref Device,
  on queue: ref Queue?,
  limits: ref Limits,
): Tensor<Element, shape> throws TensorError {
  return unsafe {
    try await stdTensorTransfer(
      take source,
      target,
      queue,
      limits,
    )
  }
}

foreign intrinsic from "std.tensor@1" {
  type DeviceHandle
  type QueueHandle

  fn stdTensorDeviceKind(_ handle: ref DeviceHandle): DeviceKind
  fn stdTensorDeviceSame(
    _ left: ref DeviceHandle,
    _ right: ref DeviceHandle,
  ): Bool
  fn stdTensorQueueDevice(_ handle: ref QueueHandle): DeviceHandle
  async fn stdTensorDevice<Element, shape: StaticList<usize>>(
    _ source: ref Tensor<Element, shape>,
  ): DeviceHandle throws TensorError
  async fn stdTensorTransfer<Element, shape: StaticList<usize>>(
    _ source: take Tensor<Element, shape>,
    _ target: ref Device,
    _ queue: ref Queue?,
    _ limits: ref Limits,
  ): Tensor<Element, shape> throws TensorError
}
