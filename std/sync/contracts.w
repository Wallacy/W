// Published immutable snapshots for read-heavy state.
//
// SnapshotCell is a safe synchronization carrier. The provider can use a
// target-specific reclamation strategy, but it must preserve versions, drops,
// happens-before edges, and structured lifetime. Only this validated wrapper
// receives the trusted shareable fact. A lock fallback retains the selected
// version under a short internal lock. It does not hold a writer lock while
// user code runs.

foreign intrinsic from "std.snapshot-cell@1" {
  type SnapshotCellHandle

  fn stdSnapshotCellCreate<Value>(
    initial: take Value,
  ): SnapshotCellHandle

  fn stdSnapshotCellRead<Value, Result, Failure: Error>(
    handle: ref SnapshotCellHandle,
    operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure

  fn stdSnapshotCellPublish<Value>(
    handle: ref SnapshotCellHandle,
    next: take Value,
  )

  fn stdSnapshotCellDrop(handle: inout SnapshotCellHandle)
}

struct TypedSnapshotCellHandle<Value> {
  raw: SnapshotCellHandle

  init(validatedRaw: SnapshotCellHandle) {
    self.raw = validatedRaw
  }
}

export struct SnapshotCell<Value> {
  handle: TypedSnapshotCellHandle<Value>

  export init(
    _ initial: take Value<(
      .transferable && .shareable && .lifetimeIndependent
    )>,
  ) {
    let raw = unsafe { stdSnapshotCellCreate(initial: take initial) }
    self.handle = TypedSnapshotCellHandle<Value>(validatedRaw: raw)
  }

  export fn read<Result, Failure: Error>(
    _ operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure {
    return unsafe {
      try stdSnapshotCellRead(
        handle: handle.raw,
        operation: operation,
      )
    }
  }

  export fn publish(
    _ next: take Value<(
      .transferable && .shareable && .lifetimeIndependent
    )>,
  ) {
    unsafe {
      stdSnapshotCellPublish(
        handle: handle.raw,
        next: take next,
      )
    }
  }

  deinit {
    unsafe { stdSnapshotCellDrop(handle: inout handle.raw) }
  }
}

extension<Value: Duplicable> SnapshotCell<Value> {
  export fn snapshot(): Value {
    return read((value: ref Value) => copy value)
  }
}
