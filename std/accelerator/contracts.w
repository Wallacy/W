// Structured accelerator launch contracts.
//
// This file is a parseable SDK draft. The provider is missing. Module
// descriptors are synthesized by the compiler from a closed static record.

import tensor from std

export enum LaunchError: Error {
  providerMissing
  moduleMismatch
  queueMismatch
  unsupportedKernel
  admissionRejected
  limitExceeded
  numericModeUnsupported
  transferRequired
  deviceLost
  asynchronousFailure
  staleGeneration
  cleanupFailed
}

export struct Limits: Copy & Equatable {
  maximumInFlight: usize<(1...)>
  maximumCommandBytes: u64<(1...)>
  maximumArgumentBytes: u64<(1...)>
  maximumResultBytes: u64<(1...)>
  maximumDependencyEdges: usize<(1...)>
  maximumRetainedDeviceBytes: u64<(1...)>
  maximumCompletionRecords: usize<(1...)>
  maximumCleanupSteps: u64<(1...)>

  export const init(
    maximumInFlight: usize<(1...)>,
    maximumCommandBytes: u64<(1...)>,
    maximumArgumentBytes: u64<(1...)>,
    maximumResultBytes: u64<(1...)>,
    maximumDependencyEdges: usize<(1...)>,
    maximumRetainedDeviceBytes: u64<(1...)>,
    maximumCompletionRecords: usize<(1...)>,
    maximumCleanupSteps: u64<(1...)>,
  ) {
    self.maximumInFlight = maximumInFlight
    self.maximumCommandBytes = maximumCommandBytes
    self.maximumArgumentBytes = maximumArgumentBytes
    self.maximumResultBytes = maximumResultBytes
    self.maximumDependencyEdges = maximumDependencyEdges
    self.maximumRetainedDeviceBytes = maximumRetainedDeviceBytes
    self.maximumCompletionRecords = maximumCompletionRecords
    self.maximumCleanupSteps = maximumCleanupSteps
  }

  export static fn standard(): Limits {
    return Limits(
      maximumInFlight: 64,
      maximumCommandBytes: 1_048_576,
      maximumArgumentBytes: 1_048_576,
      maximumResultBytes: 16_777_216,
      maximumDependencyEdges: 256,
      maximumRetainedDeviceBytes: 268_435_456,
      maximumCompletionRecords: 256,
      maximumCleanupSteps: 1_000_000,
    )
  }
}

export struct ModuleIdentity: Copy & Equatable {
  handle: ModuleIdentityHandle

  init(validatedHandle: ModuleIdentityHandle) {
    self.handle = validatedHandle
  }

  export fn same(as other: ref ModuleIdentity): Bool {
    return unsafe {
      stdAcceleratorModuleIdentitySame(
        left: ref handle,
        right: ref other.handle,
      )
    }
  }
}

export protocol KernelModule {
  fn identity(): ModuleIdentity
}

struct TypedLaunchHandle<Module: KernelModule> {
  raw: LaunchHandle

  init(validatedRaw: LaunchHandle) {
    self.raw = validatedRaw
  }
}

export struct Launch<Module: KernelModule> {
  handle: TypedLaunchHandle<Module>

  init(validatedHandle: TypedLaunchHandle<Module>) {
    self.handle = validatedHandle
  }

  export take async fn close() throws LaunchError {
    unsafe { try await stdAcceleratorClose(handle: take handle.raw) }
  }
}

export async fn open<Module: KernelModule>(
  module: ref Module,
  on queue: ref tensor.Queue,
  limits: ref Limits,
): Launch<Module> throws LaunchError {
  let raw = unsafe {
    try await stdAcceleratorOpen(
      module: ref module,
      queue: ref queue,
      limits: ref limits,
    )
  }
  let handle = TypedLaunchHandle<Module>(validatedRaw: raw)
  return Launch(validatedHandle: handle)
}

foreign intrinsic from "std.accelerator@1" {
  type ModuleIdentityHandle
  type LaunchHandle

  fn stdAcceleratorModuleIdentitySame(
    left: ref ModuleIdentityHandle,
    right: ref ModuleIdentityHandle,
  ): Bool

  async fn stdAcceleratorOpen<Module: KernelModule>(
    module: ref Module,
    queue: ref tensor.Queue,
    limits: ref Limits,
  ): LaunchHandle throws LaunchError

  async fn stdAcceleratorClose(
    handle: take LaunchHandle,
  ) throws LaunchError
}
