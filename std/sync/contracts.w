// Scoped exclusive state and published immutable snapshots.
//
// Mutex, AsyncMutex, and ReadWriteLock expose the protected value only through
// never-suspending closures. The trusted wrapper is shareable; the payload does
// not need to be.
// SnapshotCell can use a target-specific reclamation strategy, but it preserves
// versions, drops, happens-before edges, and structured lifetime.

export enum LockAttempt<Result> {
  acquired(Result)
  busy
}

foreign intrinsic from "std.sync@1" {
  type MutexHandle
  type AsyncMutexHandle
  type ReadWriteLockHandle
  type SnapshotCellHandle

  fn stdMutexCreate<Value>(initial: take Value): MutexHandle

  fn stdMutexRead<Value, Result, Failure: Error>(
    handle: ref MutexHandle,
    operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure

  fn stdMutexModify<Value, Result, Failure: Error>(
    handle: ref MutexHandle,
    operation: some fn(inout Value): Result throws Failure,
  ): Result throws Failure

  fn stdMutexTryRead<Value, Result, Failure: Error>(
    handle: ref MutexHandle,
    operation: some fn(ref Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure

  fn stdMutexTryModify<Value, Result, Failure: Error>(
    handle: ref MutexHandle,
    operation: some fn(inout Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure

  fn stdMutexDrop(handle: inout MutexHandle)

  fn stdAsyncMutexCreate<Value>(initial: take Value): AsyncMutexHandle

  async fn stdAsyncMutexRead<Value, Result, Failure: Error>(
    handle: ref AsyncMutexHandle,
    operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure

  async fn stdAsyncMutexModify<Value, Result, Failure: Error>(
    handle: ref AsyncMutexHandle,
    operation: some fn(inout Value): Result throws Failure,
  ): Result throws Failure

  fn stdAsyncMutexTryRead<Value, Result, Failure: Error>(
    handle: ref AsyncMutexHandle,
    operation: some fn(ref Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure

  fn stdAsyncMutexTryModify<Value, Result, Failure: Error>(
    handle: ref AsyncMutexHandle,
    operation: some fn(inout Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure

  fn stdAsyncMutexDrop(handle: inout AsyncMutexHandle)

  fn stdReadWriteLockCreate<Value>(
    initial: take Value,
  ): ReadWriteLockHandle

  fn stdReadWriteLockRead<Value, Result, Failure: Error>(
    handle: ref ReadWriteLockHandle,
    operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure

  fn stdReadWriteLockWrite<Value, Result, Failure: Error>(
    handle: ref ReadWriteLockHandle,
    operation: some fn(inout Value): Result throws Failure,
  ): Result throws Failure

  fn stdReadWriteLockTryRead<Value, Result, Failure: Error>(
    handle: ref ReadWriteLockHandle,
    operation: some fn(ref Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure

  fn stdReadWriteLockTryWrite<Value, Result, Failure: Error>(
    handle: ref ReadWriteLockHandle,
    operation: some fn(inout Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure

  fn stdReadWriteLockDrop(handle: inout ReadWriteLockHandle)

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

struct TypedMutexHandle<Value> {
  raw: MutexHandle

  init(validatedRaw: MutexHandle) {
    self.raw = validatedRaw
  }
}

struct TypedAsyncMutexHandle<Value> {
  raw: AsyncMutexHandle

  init(validatedRaw: AsyncMutexHandle) {
    self.raw = validatedRaw
  }
}

struct TypedReadWriteLockHandle<Value> {
  raw: ReadWriteLockHandle

  init(validatedRaw: ReadWriteLockHandle) {
    self.raw = validatedRaw
  }
}

export struct Mutex<Value> {
  handle: TypedMutexHandle<Value>

  export init(
    _ initial: take Value<(.transferable && .lifetimeIndependent)>,
  ) {
    let raw = unsafe { stdMutexCreate(initial: take initial) }
    self.handle = TypedMutexHandle<Value>(validatedRaw: raw)
  }

  export fn withLock<Result, Failure: Error>(
    _ operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure {
    return unsafe { try stdMutexRead(handle: handle.raw, operation: operation) }
  }

  export fn withLock<Result, Failure: Error>(
    _ operation: some fn(inout Value): Result throws Failure,
  ): Result throws Failure {
    return unsafe { try stdMutexModify(handle: handle.raw, operation: operation) }
  }

  export fn tryWithLock<Result, Failure: Error>(
    _ operation: some fn(ref Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure {
    return unsafe { try stdMutexTryRead(handle: handle.raw, operation: operation) }
  }

  export fn tryWithLock<Result, Failure: Error>(
    _ operation: some fn(inout Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure {
    return unsafe { try stdMutexTryModify(handle: handle.raw, operation: operation) }
  }

  deinit {
    unsafe { stdMutexDrop(handle: inout handle.raw) }
  }
}

export struct AsyncMutex<Value> {
  handle: TypedAsyncMutexHandle<Value>

  export init(
    _ initial: take Value<(.transferable && .lifetimeIndependent)>,
  ) {
    let raw = unsafe { stdAsyncMutexCreate(initial: take initial) }
    self.handle = TypedAsyncMutexHandle<Value>(validatedRaw: raw)
  }

  export async fn withLock<Result, Failure: Error>(
    _ operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure {
    return unsafe { try await stdAsyncMutexRead(handle: handle.raw, operation: operation) }
  }

  export async fn withLock<Result, Failure: Error>(
    _ operation: some fn(inout Value): Result throws Failure,
  ): Result throws Failure {
    return unsafe { try await stdAsyncMutexModify(handle: handle.raw, operation: operation) }
  }

  export fn tryWithLock<Result, Failure: Error>(
    _ operation: some fn(ref Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure {
    return unsafe { try stdAsyncMutexTryRead(handle: handle.raw, operation: operation) }
  }

  export fn tryWithLock<Result, Failure: Error>(
    _ operation: some fn(inout Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure {
    return unsafe { try stdAsyncMutexTryModify(handle: handle.raw, operation: operation) }
  }

  deinit {
    unsafe { stdAsyncMutexDrop(handle: inout handle.raw) }
  }
}

export struct ReadWriteLock<Value> {
  handle: TypedReadWriteLockHandle<Value>

  export init(
    _ initial: take Value<(.transferable && .lifetimeIndependent)>,
  ) {
    let raw = unsafe { stdReadWriteLockCreate(initial: take initial) }
    self.handle = TypedReadWriteLockHandle<Value>(validatedRaw: raw)
  }

  export fn read<Result, Failure: Error>(
    _ operation: some fn(ref Value): Result throws Failure,
  ): Result throws Failure {
    return unsafe {
      try stdReadWriteLockRead(handle: handle.raw, operation: operation)
    }
  }

  export fn write<Result, Failure: Error>(
    _ operation: some fn(inout Value): Result throws Failure,
  ): Result throws Failure {
    return unsafe {
      try stdReadWriteLockWrite(handle: handle.raw, operation: operation)
    }
  }

  export fn tryRead<Result, Failure: Error>(
    _ operation: some fn(ref Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure {
    return unsafe {
      try stdReadWriteLockTryRead(handle: handle.raw, operation: operation)
    }
  }

  export fn tryWrite<Result, Failure: Error>(
    _ operation: some fn(inout Value): Result throws Failure,
  ): LockAttempt<Result> throws Failure {
    return unsafe {
      try stdReadWriteLockTryWrite(handle: handle.raw, operation: operation)
    }
  }

  deinit {
    unsafe { stdReadWriteLockDrop(handle: inout handle.raw) }
  }
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
