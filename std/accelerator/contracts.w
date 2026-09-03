// Structured accelerator launch contracts.
//
// This file is a parseable SDK draft. The provider is missing.
// `accelerator.module` is a compiler synthesis head cataloged separately. It
// is not an exported runtime function. `open` is the provider boundary for a
// descriptor and artifact that the compiler already validated. KernelModule
// is a public constraint with compiler-owned conformance; user types cannot
// conform manually.

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
  let maximumInFlight: usize<(1...)>
  let maximumCommandBytes: u64<(1...)>
  let maximumArgumentBytes: u64<(1...)>
  let maximumResultBytes: u64<(1...)>
  let maximumDependencyEdges: usize<(1...)>
  let maximumRetainedDeviceBytes: u64<(1...)>
  let maximumCompletionRecords: usize<(1...)>
  let maximumCleanupSteps: u64<(1...)>

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
  let handle: ModuleIdentityHandle

  init(validatedHandle: ModuleIdentityHandle) {
    self.handle = validatedHandle
  }

  export fn same(as other: ref ModuleIdentity): Bool {
    return unsafe {
      stdAcceleratorModuleIdentitySame(
        ref handle,
        ref other.handle,
      )
    }
  }
}

export protocol KernelModule {
  fn identity(): ModuleIdentity
}

struct TypedLaunchHandle<Module: KernelModule> {
  let raw: LaunchHandle

  init(validatedRaw: LaunchHandle) {
    self.raw = validatedRaw
  }
}

export struct Launch<Module: KernelModule> {
  let handle: TypedLaunchHandle<Module>

  init(validatedHandle: TypedLaunchHandle<Module>) {
    self.handle = validatedHandle
  }

  export take async fn close() throws LaunchError {
    unsafe { try await stdAcceleratorClose(take handle.raw) }
  }
}

export async fn open<Module: KernelModule>(
  module: ref Module,
  on queue: ref tensor.Queue,
  limits: ref Limits,
): Launch<Module> throws LaunchError {
  let raw = unsafe {
    try await stdAcceleratorOpen(
      ref module,
      ref queue,
      ref limits,
    )
  }
  let handle = TypedLaunchHandle<Module>(validatedRaw: raw)
  return Launch(validatedHandle: handle)
}

foreign intrinsic from "std.accelerator@1" {
  type ModuleIdentityHandle
  type LaunchHandle

  fn stdAcceleratorModuleIdentitySame(
    _ left: ref ModuleIdentityHandle,
    _ right: ref ModuleIdentityHandle,
  ): Bool

  async fn stdAcceleratorOpen<Module: KernelModule>(
    _ module: ref Module,
    _ queue: ref tensor.Queue,
    _ limits: ref Limits,
  ): LaunchHandle throws LaunchError

  async fn stdAcceleratorClose(
    _ handle: take LaunchHandle,
  ) throws LaunchError
}
