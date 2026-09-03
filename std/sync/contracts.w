// Published immutable snapshots.
//
// `lock`, `await lock`, and `try lock` are language forms over `shared T`.
// They are not wrappers in std.sync. SnapshotCell keeps immutable versions and
// lets the provider select a target-specific reclamation strategy.

foreign intrinsic from "std.sync@1" {
  type SnapshotCellHandle

  fn stdSnapshotCellCreate<Value>(
    _ initial: take Value,
  ): SnapshotCellHandle

  fn stdSnapshotCellRead<Value, Result, Failure: Error>(
    _ handle: ref SnapshotCellHandle,
    _ operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure

  fn stdSnapshotCellPublish<Value>(
    _ handle: ref SnapshotCellHandle,
    _ next: take Value,
  )

  fn stdSnapshotCellDrop(_ handle: inout SnapshotCellHandle)
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
    initial: take Value<(
      .transferable && .shareable && .lifetimeIndependent
    )>,
  ) {
    let raw = unsafe { stdSnapshotCellCreate(take initial) }
    self.handle = TypedSnapshotCellHandle<Value>(validatedRaw: raw)
  }

  export fn read<Result, Failure: Error>(
    operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure {
    return unsafe {
      try stdSnapshotCellRead(
        handle.raw,
        operation,
      )
    }
  }

  export fn publish(
    next: take Value<(
      .transferable && .shareable && .lifetimeIndependent
    )>,
  ) {
    unsafe {
      stdSnapshotCellPublish(
        handle.raw,
        take next,
      )
    }
  }

  deinit {
    unsafe { stdSnapshotCellDrop(inout handle.raw) }
  }
}

extension<Value: Duplicable> SnapshotCell<Value> {
  export fn snapshot(): Value {
    return read((value: ref Value) => copy value)
  }
}
