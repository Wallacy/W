// Public byte I/O contracts.

import { AllocationError } from std.memory

export enum IoOperation: Copy & Equatable & Hashable {
  resolve
  open
  close
  read
  write
  flush
  sync
  metadata
  list
  create
  remove
  rename
  connect
  listen
  accept
  send
  receive
  shutdown
  register
  other
}

export enum IoErrorKind: Copy & Equatable & Hashable {
  permissionDenied
  notFound
  alreadyExists
  notDirectory
  isDirectory
  directoryNotEmpty
  readOnly
  busy
  invalidInput
  invalidData
  unsupported
  resourceExhausted
  storageFull
  quotaExceeded
  timedOut
  connectionReset
  brokenPipe
  other
}

foreign intrinsic from "std.io@1" {
  type IoCauseHandle
  type SnapshotByteSourceProviderMarker
  type ReadBatchHandle
  type TransferPlanHandle

  fn stdIoCauseDuplicate(_ handle: ref IoCauseHandle): IoCauseHandle
  fn stdIoCauseDrop(_ handle: inout IoCauseHandle)

  fn stdIoReadBatchCreate(
    _ capacities: ref Array<usize<(1...)>>,
  ): ReadBatchHandle throws AllocationError
  fn stdIoReadBatchSegmentCount(_ handle: ref ReadBatchHandle): usize
  fn stdIoReadBatchFilledBytes(_ handle: ref ReadBatchHandle): usize
  fn stdIoReadBatchIsFull(_ handle: ref ReadBatchHandle): Bool
  fn stdIoReadBatchSegment(
    _ handle: ref ReadBatchHandle,
    _ index: usize,
  ): view Bytes
  fn stdIoReadBatchReset(_ handle: inout ReadBatchHandle)
  fn stdIoReadBatchIntoSegments(
    _ handle: take ReadBatchHandle,
  ): Array<Bytes>
  fn stdIoReadBatchDrop(_ handle: inout ReadBatchHandle)

  fn stdIoTransferPlanCreate(
    _ offset: u64,
    _ maximumBytes: u64,
    _ chunkBytes: usize<(1...)>,
  ): TransferPlanHandle throws TransferPlanError
  fn stdIoTransferPlanTransferred(_ handle: ref TransferPlanHandle): u64
  fn stdIoTransferPlanRemaining(_ handle: ref TransferPlanHandle): u64
  fn stdIoTransferPlanPendingBytes(_ handle: ref TransferPlanHandle): usize
  fn stdIoTransferPlanDrop(_ handle: inout TransferPlanHandle)

  async fn stdIoReadMany<
    Failure: Error,
    Source: ByteSource<Failure>,
  >(
    _ source: inout Source,
    _ destination: inout ReadBatchHandle,
  ): ScatterReadStep throws Failure

  async fn stdIoTransfer<
    ReadFailure: Error,
    WriteFailure: Error,
    Source: SnapshotByteSource<ReadFailure>,
    Destination: ByteSink<WriteFailure>,
  >(
    _ source: ref Source,
    _ destination: inout Destination,
    _ plan: inout TransferPlanHandle,
  ): TransferStep throws TransferError<ReadFailure, WriteFailure>
}

// IoCause is a bounded, redacted provider snapshot. It owns no file, socket,
// task, request, capability, or other live resource.
export struct IoCause: Duplicable {
  let handle: IoCauseHandle

  init(validatedHandle: IoCauseHandle) {
    self.handle = validatedHandle
  }

  export fn duplicate(): IoCause {
    let duplicate = unsafe { stdIoCauseDuplicate(ref handle) }
    return IoCause(validatedHandle: duplicate)
  }

  deinit {
    unsafe { stdIoCauseDrop(inout handle) }
  }
}

export struct IoError: Error & Duplicable {
  let kindValue: IoErrorKind
  let operationValue: IoOperation
  let causeValue: IoCause?

  init(
    validatedKind kind: IoErrorKind,
    operation: IoOperation,
    cause: IoCause?,
  ) {
    self.kindValue = kind
    self.operationValue = operation
    self.causeValue = take cause
  }

  export let kind: IoErrorKind {
    get => kindValue
  }

  export let operation: IoOperation {
    get => operationValue
  }

  export let cause: ref IoCause? {
    get => causeValue
  }

  export fn duplicate(): IoError {
    return IoError(
      validatedKind: kindValue,
      operation: operationValue,
      cause: copy causeValue,
    )
  }
}

export enum ReadStep {
  data(usize<(1...)>)
  end
}

// Positional reads use a separate step so a provider can report a short read
// without exposing a cursor.  A positive count is progress; .end is EOF.
export enum SnapshotReadStep {
  data(usize<(1...)>)
  end
}

export enum WriteStep {
  complete
  partial(usize<(1...)>)
}

export enum ScatterReadStep {
  data(usize<(1...)>)
  end
  full
}

export enum TransferStep {
  data(usize<(1...)>)
  sourceEnd
  limitReached
}

export enum TransferPlanError: Error {
  allocation(AllocationError)
  rangeOverflow
}

export enum TransferCause<ReadFailure: Error, WriteFailure: Error>: Error {
  read(ReadFailure)
  write(WriteFailure)
}

export struct TransferError<ReadFailure: Error, WriteFailure: Error>: Error {
  export let cause: TransferCause<ReadFailure, WriteFailure>
  export let committed: usize
}

// ReadBatch is the sole owner of fixed-capacity scatter segments. Public views
// expose initialized prefixes only; reset retains the reservations.
export struct ReadBatch {
  let handle: ReadBatchHandle

  export init(capacities: usize<(1...)>...) throws AllocationError {
    self.handle = unsafe { try stdIoReadBatchCreate(ref capacities) }
  }

  export let segmentCount: usize {
    get => unsafe { stdIoReadBatchSegmentCount(ref handle) }
  }

  export let filledBytes: usize {
    get => unsafe { stdIoReadBatchFilledBytes(ref handle) }
  }

  export let isFull: Bool {
    get => unsafe { stdIoReadBatchIsFull(ref handle) }
  }

  export fn segment(at index: usize): view Bytes {
    return unsafe { stdIoReadBatchSegment(ref handle, index) }
  }

  export mut fn reset() {
    unsafe { stdIoReadBatchReset(inout handle) }
  }

  export take fn intoSegments(): Array<Bytes> {
    return unsafe { stdIoReadBatchIntoSegments(take handle) }
  }

  deinit {
    unsafe { stdIoReadBatchDrop(inout handle) }
  }
}

// TransferPlan owns the bounded fallback scratch and tracks only bytes that a
// destination has committed. A successful initialization is allocation-free
// for every later transfer step.
export struct TransferPlan {
  let handle: TransferPlanHandle

  export init(
    at offset: u64,
    maximumBytes: u64,
    chunkBytes: usize<(1...)>,
  ) throws TransferPlanError {
    self.handle = unsafe {
      try stdIoTransferPlanCreate(offset, maximumBytes, chunkBytes)
    }
  }

  export let transferred: u64 {
    get => unsafe { stdIoTransferPlanTransferred(ref handle) }
  }

  export let remaining: u64 {
    get => unsafe { stdIoTransferPlanRemaining(ref handle) }
  }

  export let pendingBytes: usize {
    get => unsafe { stdIoTransferPlanPendingBytes(ref handle) }
  }

  deinit {
    unsafe { stdIoTransferPlanDrop(inout handle) }
  }
}

export struct WriteAllError<Cause: Error>: Error {
  export let cause: Cause
  export let committed: usize
}

export protocol ByteSource<Failure: Error> {
  mut async fn read(
    appendTo destination: inout Bytes,
    maximum: usize<(1...)>,
  ): ReadStep throws Failure
}

// SnapshotByteSource is a finite positional source. It has no cursor.
// The owner keeps byteCount and the logical content stable until release.
// Positional reads are safe to run in parallel; a provider may return a short
// positive read and the caller must issue another offset read. A decoder must
// consume the source and must not infer a path, provider, or ambient file.
export protocol SnapshotByteSource<Failure: Error> {
  fn byteCount(): u64

  async fn read(
    at offset: u64,
    appendTo destination: inout Bytes,
    maximum: usize<(1...)>,
  ): SnapshotReadStep throws Failure
}

export protocol ByteSink<Failure: Error> {
  mut async fn write(source: view Bytes): WriteStep throws Failure

  mut async fn writeAll(
    source: view Bytes,
  ): () throws WriteAllError<Failure> {
    var committed: usize = 0

    while committed < source.count {
      let remaining: view Bytes = source[committed...]

      do {
        switch try await self.write(remaining) {
          case .complete:
            committed = source.count
          case .partial(let count):
            committed += count
        }
      } catch error {
        throw WriteAllError(cause: error, committed: committed)
      }
    }
  }

  mut async fn writeMany(
    sources: view Bytes...,
  ): WriteStep throws Failure {
    var index: usize = 0

    while index < sources.count {
      let source: view Bytes = sources[index]
      index += 1

      if source.isEmpty { continue }

      switch try await self.write(source) {
        case .partial(let count):
          return .partial(count)
        case .complete:
          while index < sources.count {
            if !sources[index].isEmpty {
              return .partial(source.count)
            }

            index += 1
          }

          return .complete
      }
    }

    return .complete
  }
}

// readMany uses native scatter only when the provider has a compatible hidden
// capability. Its portable fallback reads into the first incomplete segment.
export async fn readMany<
  Failure: Error,
  Source: ByteSource<Failure>,
>(
  from source: inout Source,
  scatterInto destination: inout ReadBatch,
): ScatterReadStep throws Failure {
  return unsafe {
    try await stdIoReadMany(inout source, inout destination.handle)
  }
}

// transfer joins a stable positional source to a sink. Native data movement is
// an adapter choice; the TransferPlan scratch is the portable fallback.
export async fn transfer<
  ReadFailure: Error,
  WriteFailure: Error,
  Source: SnapshotByteSource<ReadFailure>,
  Destination: ByteSink<WriteFailure>,
>(
  from source: ref Source,
  to destination: inout Destination,
  using plan: inout TransferPlan,
): TransferStep throws TransferError<ReadFailure, WriteFailure> {
  return unsafe {
    try await stdIoTransfer(
      ref source,
      inout destination,
      inout plan.handle,
    )
  }
}

test "write progress is distinct from completion" {
  let complete: WriteStep = .complete
  let partial: WriteStep = .partial(1)
  expect complete != partial
}

test "EOF is distinct from positive read progress" {
  let end: ReadStep = .end
  let data: ReadStep = .data(1)
  expect end != data
}

test "I/O control outcomes are not portable errors" {
  expect IoErrorKind.timedOut != .other
  expect IoOperation.read != .write
}
