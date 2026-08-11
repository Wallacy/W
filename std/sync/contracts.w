// Published immutable snapshots.
//
// `lock`, `await lock`, and `try lock` are language forms over `shared T`.
// They are not wrappers in std.sync. SnapshotCell keeps immutable versions and
// lets the provider select a target-specific reclamation strategy.

foreign intrinsic from "std.sync@1" {
  type SnapshotCellHandle

  fn stdSnapshotCellCreate<Value>(
    initial: take Value,
  ): SnapshotCellHandle

  fn stdSnapshotCellRead<Value, Result, Failure: Error>(
    named handle: ref SnapshotCellHandle,
    named operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure

  fn stdSnapshotCellPublish<Value>(
    named handle: ref SnapshotCellHandle,
    named next: take Value,
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
    let raw = unsafe { stdSnapshotCellCreate(take initial) }
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
    unsafe { stdSnapshotCellDrop(inout handle.raw) }
  }
}

extension<Value: Duplicable> SnapshotCell<Value> {
  export fn snapshot(): Value {
    return read((value: ref Value) => copy value)
  }
}
