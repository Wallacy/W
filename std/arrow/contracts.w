// Draft Arrow IPC and trusted C Data declarations for TAB1.
//
// IPC stream and file use separate source contracts.  C Data and C Stream use
// opaque move-only trusted handles.  Untrusted serialized bytes never cross
// the C bridge, and a device handle is never dereferenced as a CPU pointer.

import io from std.io
import data from std.data

export enum DeviceKind: Copy & Equatable {
  cpu
  foreign
}

export enum LimitKind: Copy & Equatable {
  metadataBytes
  messages
  buffers
  bodyBytes
  dictionaryEntries
  nesting
  compressionRatio
  allocations
  cCallbacks
}

export struct Limits: Copy & Equatable {
  maximumMetadataBytes: u64<(1...)>
  maximumMessages: usize<(1...)>
  maximumBuffers: usize<(1...)>
  maximumBodyBytes: u64<(1...)>
  maximumDictionaryEntries: usize<(1...)>
  maximumNesting: usize<(1...)>
  maximumCompressionRatio: usize<(1...)>
  maximumAllocationBytes: u64<(1...)>
  maximumCallbacks: usize<(1...)>

  export static fn standard(): Limits {
    return Limits(
      maximumMetadataBytes: 16_777_216,
      maximumMessages: 1_000_000,
      maximumBuffers: 10_000_000,
      maximumBodyBytes: 4_294_967_296,
      maximumDictionaryEntries: 1_000_000,
      maximumNesting: 64,
      maximumCompressionRatio: 100,
      maximumAllocationBytes: 4_294_967_296,
      maximumCallbacks: 1_000_000,
    )
  }

  export const init(
    maximumMetadataBytes: u64<(1...)>,
    maximumMessages: usize<(1...)>,
    maximumBuffers: usize<(1...)>,
    maximumBodyBytes: u64<(1...)>,
    maximumDictionaryEntries: usize<(1...)>,
    maximumNesting: usize<(1...)>,
    maximumCompressionRatio: usize<(1...)>,
    maximumAllocationBytes: u64<(1...)>,
    maximumCallbacks: usize<(1...)>,
  ) {
    self.maximumMetadataBytes = maximumMetadataBytes
    self.maximumMessages = maximumMessages
    self.maximumBuffers = maximumBuffers
    self.maximumBodyBytes = maximumBodyBytes
    self.maximumDictionaryEntries = maximumDictionaryEntries
    self.maximumNesting = maximumNesting
    self.maximumCompressionRatio = maximumCompressionRatio
    self.maximumAllocationBytes = maximumAllocationBytes
    self.maximumCallbacks = maximumCallbacks
  }
}

export struct CImportLimits: Copy & Equatable {
  maximumRows: usize<(1...)>
  maximumBuffers: usize<(1...)>
  maximumBytes: u64<(1...)>
  maximumCallbacks: usize<(1...)>

  export static fn standard(): CImportLimits {
    return CImportLimits(
      maximumRows: 1_000_000,
      maximumBuffers: 10_000_000,
      maximumBytes: 4_294_967_296,
      maximumCallbacks: 1_000_000,
    )
  }

  export const init(
    maximumRows: usize<(1...)>,
    maximumBuffers: usize<(1...)>,
    maximumBytes: u64<(1...)>,
    maximumCallbacks: usize<(1...)>,
  ) {
    self.maximumRows = maximumRows
    self.maximumBuffers = maximumBuffers
    self.maximumBytes = maximumBytes
    self.maximumCallbacks = maximumCallbacks
  }
}

export enum BlockingStrategy: Copy & Equatable {
  reject
  boundedWorker
}

// boundedWorker is admitted only when every finite quota is available.  The
// quota bounds concurrent callbacks, queued jobs, and total jobs; it is not a
// hidden unbounded executor.
export struct BlockingQuota: Copy & Equatable {
  maximumConcurrent: usize<(1...)>
  maximumQueued: usize<(1...)>
  maximumJobs: usize<(1...)>

  export static fn standard(): BlockingQuota {
    return BlockingQuota(
      maximumConcurrent: 1,
      maximumQueued: 64,
      maximumJobs: 65_536,
    )
  }

  export const init(
    maximumConcurrent: usize<(1...)>,
    maximumQueued: usize<(1...)>,
    maximumJobs: usize<(1...)>,
  ) {
    self.maximumConcurrent = maximumConcurrent
    self.maximumQueued = maximumQueued
    self.maximumJobs = maximumJobs
  }
}

export enum DrainPolicy: Copy & Equatable {
  cancelAndDrain
  drainToEnd
}

export struct CImportOptions: Copy & Equatable {
  copyPolicy: data.CopyPolicy
  limits: CImportLimits

  export static fn standard(): CImportOptions {
    return CImportOptions(copyPolicy: .ifNeeded, limits: CImportLimits.standard())
  }

  export const init(
    copyPolicy: data.CopyPolicy = .ifNeeded,
    limits: CImportLimits = CImportLimits.standard(),
  ) {
    self.copyPolicy = copyPolicy
    self.limits = limits
  }
}

export struct CStreamImportOptions: Copy & Equatable {
  copyPolicy: data.CopyPolicy
  limits: CImportLimits
  blocking: BlockingStrategy
  quota: BlockingQuota
  drain: DrainPolicy

  export static fn standard(): CStreamImportOptions {
    return CStreamImportOptions(
      copyPolicy: .ifNeeded,
      limits: CImportLimits.standard(),
      blocking: .boundedWorker,
      quota: BlockingQuota.standard(),
      drain: .cancelAndDrain,
    )
  }

  export const init(
    copyPolicy: data.CopyPolicy = .ifNeeded,
    limits: CImportLimits = CImportLimits.standard(),
    blocking: BlockingStrategy = .boundedWorker,
    quota: BlockingQuota = BlockingQuota.standard(),
    drain: DrainPolicy = .cancelAndDrain,
  ) {
    self.copyPolicy = copyPolicy
    self.limits = limits
    self.blocking = blocking
    self.quota = quota
    self.drain = drain
  }
}

export struct CExportOptions: Copy & Equatable {
  copyPolicy: data.CopyPolicy
  limits: CImportLimits

  export static fn standard(): CExportOptions {
    return CExportOptions(copyPolicy: .ifNeeded, limits: CImportLimits.standard())
  }

  export const init(
    copyPolicy: data.CopyPolicy = .ifNeeded,
    limits: CImportLimits = CImportLimits.standard(),
  ) {
    self.copyPolicy = copyPolicy
    self.limits = limits
  }
}

foreign intrinsic from "std.arrow@1" {
  type RawCArrayHandle
  type RawCStreamHandle

  fn stdArrowReleaseArray(handle: inout RawCArrayHandle)
  fn stdArrowReleaseStream(handle: inout RawCStreamHandle)

  fn stdArrowDecodeIpcStream<Row: data.Row, SourceFailure: Error,
    Source: io.ByteSource<SourceFailure>>(
    source: take Source,
    options: DecodeOptions,
  ): some Stream<data.Batch<Row>, DecodeError<SourceFailure>>

  fn stdArrowDecodeIpcFile<Row: data.Row, SourceFailure: Error,
    Source: io.SnapshotByteSource<SourceFailure>>(
    source: take Source,
    options: DecodeOptions,
  ): some Stream<data.Batch<Row>, DecodeError<SourceFailure>>

  async fn stdArrowDecodeIpcStreamAll<Row: data.Row, SourceFailure: Error,
    Source: io.ByteSource<SourceFailure>>(
    source: take Source,
    options: DecodeOptions,
  ): data.Batch<Row> throws DecodeError<SourceFailure>

  async fn stdArrowDecodeIpcFileAll<Row: data.Row, SourceFailure: Error,
    Source: io.SnapshotByteSource<SourceFailure>>(
    source: take Source,
    options: DecodeOptions,
  ): data.Batch<Row> throws DecodeError<SourceFailure>

  fn stdArrowDecodeIpcStreamDynamic<SourceFailure: Error,
    Source: io.ByteSource<SourceFailure>>(
    source: take Source,
    options: DecodeOptions,
  ): some Stream<data.DynamicBatch, DecodeError<SourceFailure>>

  fn stdArrowDecodeIpcFileDynamic<SourceFailure: Error,
    Source: io.SnapshotByteSource<SourceFailure>>(
    source: take Source,
    options: DecodeOptions,
  ): some Stream<data.DynamicBatch, DecodeError<SourceFailure>>

  async fn stdArrowEncodeIpcStreamBatch<Row: data.Row, SinkFailure: Error,
    Sink: io.ByteSink<SinkFailure>>(
    batch: ref data.Batch<Row>,
    sink: inout Sink,
    options: EncodeOptions,
  ): data.EncodeProgress throws EncodeError<SinkFailure>

  async fn stdArrowEncodeIpcStreamStream<Row: data.Row, BatchFailure: Error, SinkFailure: Error,
    Source: Stream<data.Batch<Row>, BatchFailure>, Sink: io.ByteSink<SinkFailure>>(
    batches: take Source,
    sink: inout Sink,
    options: EncodeOptions,
  ): data.EncodeProgress throws EncodeStreamError<BatchFailure, SinkFailure>

  async fn stdArrowEncodeIpcFileBatch<Row: data.Row, SinkFailure: Error,
    Sink: io.ByteSink<SinkFailure>>(
    batch: ref data.Batch<Row>,
    sink: inout Sink,
    options: EncodeOptions,
  ): data.EncodeProgress throws EncodeError<SinkFailure>

  async fn stdArrowEncodeIpcFileStream<Row: data.Row, BatchFailure: Error, SinkFailure: Error,
    Source: Stream<data.Batch<Row>, BatchFailure>, Sink: io.ByteSink<SinkFailure>>(
    batches: take Source,
    sink: inout Sink,
    options: EncodeOptions,
  ): data.EncodeProgress throws EncodeStreamError<BatchFailure, SinkFailure>

  fn stdArrowImportCArray<Row: data.Row>(
    handle: take CArrayHandle,
    options: CImportOptions,
  ): data.Batch<Row> throws CImportError

  fn stdArrowImportCArrayDynamic(
    handle: take CArrayHandle,
    options: CImportOptions,
  ): data.DynamicBatch throws CImportError

  fn stdArrowImportCStream<Row: data.Row>(
    handle: take CStreamHandle,
    options: CStreamImportOptions,
  ): some Stream<data.Batch<Row>, CImportError>

  fn stdArrowImportCStreamDynamic(
    handle: take CStreamHandle,
    options: CStreamImportOptions,
  ): some Stream<data.DynamicBatch, CImportError>

  fn stdArrowExportCArray<Row: data.Row>(
    batch: ref data.Batch<Row>,
    options: CExportOptions,
  ): CArrayHandle throws CExportError
}

// These wrappers contain private provider handles and have no public
// initializer.  The provider stores the raw Arrow schema beside the array or
// stream and release is idempotent at exactly one owner boundary.
export struct CArrayHandle {
  raw: RawCArrayHandle

  init(validatedRaw: RawCArrayHandle) {
    self.raw = validatedRaw
  }

  deinit {
    unsafe { stdArrowReleaseArray(inout raw) }
  }
}

export struct CStreamHandle {
  raw: RawCStreamHandle

  init(validatedRaw: RawCStreamHandle) {
    self.raw = validatedRaw
  }

  deinit {
    unsafe { stdArrowReleaseStream(inout raw) }
  }
}

export struct DecodeOptions: Copy & Equatable {
  binding: data.BindingPolicy
  mapping: data.MappingPolicy
  copyPolicy: data.CopyPolicy
  limits: Limits

  export static fn standard(): DecodeOptions {
    return DecodeOptions(
      binding: .exact,
      mapping: .none,
      copyPolicy: .ifNeeded,
      limits: Limits.standard(),
    )
  }

  export const init(
    binding: data.BindingPolicy = .exact,
    mapping: data.MappingPolicy = .none,
    copyPolicy: data.CopyPolicy = .ifNeeded,
    limits: Limits = Limits.standard(),
  ) {
    self.binding = binding
    self.mapping = mapping
    self.copyPolicy = copyPolicy
    self.limits = limits
  }
}

export struct EncodeOptions: Copy & Equatable {
  limits: Limits

  export static fn standard(): EncodeOptions {
    return EncodeOptions(limits: Limits.standard())
  }

  export const init(limits: Limits = Limits.standard()) {
    self.limits = limits
  }
}

export enum DecodeError<SourceFailure: Error>: Error {
  source(SourceFailure)
  invalidFlatBuffers
  invalidMessage
  invalidBuffer
  invalidOffset
  invalidNesting
  invalidBodySize
  dictionaryBeforeDefinition
  dictionaryReplacementInFile
  schemaMismatch
  footerMismatch
  unsupportedCompression
  nonNativeEndian
  copyRequired
  alignmentRequired
  limitExceeded(kind: LimitKind, maximum: u64)
}

export enum CImportError: Error {
  untrustedHandle
  invalidStructure
  invalidRawSchema
  invalidBuffer
  invalidOffset
  invalidUtf8
  schemaMismatch
  deviceNotCpu(DeviceKind)
  copyRequired
  alignmentRequired
  releaseAlreadyCalled
  callbackFailure
  quotaExceeded
}

export enum CExportError: Error {
  unsupportedType(String)
  deviceNotCpu(DeviceKind)
  alignmentRequired
  copyRequired
  ownerTransferred
  quotaExceeded
}

export enum EncodeError<SinkFailure: Error>: Error {
  sink(SinkFailure, data.EncodeProgress)
  unsupportedType(String, data.EncodeProgress)
  copyRequired(data.EncodeProgress)
  limitExceeded(kind: LimitKind, maximum: u64, progress: data.EncodeProgress)
}

export enum EncodeStreamError<BatchFailure: Error, SinkFailure: Error>: Error {
  batch(BatchFailure, data.EncodeProgress)
  sink(SinkFailure, data.EncodeProgress)
  unsupportedType(String, data.EncodeProgress)
  copyRequired(data.EncodeProgress)
  limitExceeded(kind: LimitKind, maximum: u64, progress: data.EncodeProgress)
}

export fn decodeIpcStream<Row: data.Row, SourceFailure: Error,
  Source: io.ByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): some Stream<data.Batch<Row>, DecodeError<SourceFailure>> {
  return unsafe { stdArrowDecodeIpcStream(source: take source, options: options) }
}

export fn decodeIpcFile<Row: data.Row, SourceFailure: Error,
  Source: io.SnapshotByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): some Stream<data.Batch<Row>, DecodeError<SourceFailure>> {
  return unsafe { stdArrowDecodeIpcFile(source: take source, options: options) }
}

export async fn decodeIpcStreamAll<Row: data.Row, SourceFailure: Error,
  Source: io.ByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): data.Batch<Row> throws DecodeError<SourceFailure> {
  return unsafe {
    try await stdArrowDecodeIpcStreamAll(source: take source, options: options)
  }
}

export async fn decodeIpcFileAll<Row: data.Row, SourceFailure: Error,
  Source: io.SnapshotByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): data.Batch<Row> throws DecodeError<SourceFailure> {
  return unsafe {
    try await stdArrowDecodeIpcFileAll(source: take source, options: options)
  }
}

export fn decodeIpcStreamDynamic<SourceFailure: Error,
  Source: io.ByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): some Stream<data.DynamicBatch, DecodeError<SourceFailure>> {
  return unsafe { stdArrowDecodeIpcStreamDynamic(source: take source, options: options) }
}

export fn decodeIpcFileDynamic<SourceFailure: Error,
  Source: io.SnapshotByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): some Stream<data.DynamicBatch, DecodeError<SourceFailure>> {
  return unsafe { stdArrowDecodeIpcFileDynamic(source: take source, options: options) }
}

export async fn encodeIpcStream<Row: data.Row, SinkFailure: Error,
  Sink: io.ByteSink<SinkFailure>>(
  batch: ref data.Batch<Row>,
  sink: inout Sink,
  options: EncodeOptions = EncodeOptions.standard(),
): data.EncodeProgress throws EncodeError<SinkFailure> {
  return unsafe {
    try await stdArrowEncodeIpcStreamBatch(batch: batch, sink: inout sink, options: options)
  }
}

export async fn encodeIpcStream<Row: data.Row, BatchFailure: Error, SinkFailure: Error,
  Source: Stream<data.Batch<Row>, BatchFailure>, Sink: io.ByteSink<SinkFailure>>(
  batches: take Source,
  sink: inout Sink,
  options: EncodeOptions = EncodeOptions.standard(),
): data.EncodeProgress throws EncodeStreamError<BatchFailure, SinkFailure> {
  return unsafe {
    try await stdArrowEncodeIpcStreamStream(
      batches: take batches,
      sink: inout sink,
      options: options,
    )
  }
}

export async fn encodeIpcFile<Row: data.Row, SinkFailure: Error,
  Sink: io.ByteSink<SinkFailure>>(
  batch: ref data.Batch<Row>,
  sink: inout Sink,
  options: EncodeOptions = EncodeOptions.standard(),
): data.EncodeProgress throws EncodeError<SinkFailure> {
  return unsafe {
    try await stdArrowEncodeIpcFileBatch(batch: batch, sink: inout sink, options: options)
  }
}

export async fn encodeIpcFile<Row: data.Row, BatchFailure: Error, SinkFailure: Error,
  Source: Stream<data.Batch<Row>, BatchFailure>, Sink: io.ByteSink<SinkFailure>>(
  batches: take Source,
  sink: inout Sink,
  options: EncodeOptions = EncodeOptions.standard(),
): data.EncodeProgress throws EncodeStreamError<BatchFailure, SinkFailure> {
  return unsafe {
    try await stdArrowEncodeIpcFileStream(
      batches: take batches,
      sink: inout sink,
      options: options,
    )
  }
}

export fn importCArray<Row: data.Row>(
  handle: take CArrayHandle,
  options: CImportOptions = CImportOptions.standard(),
): data.Batch<Row> throws CImportError {
  return unsafe {
    // The handle carries the raw Arrow schema.  Typed import validates it
    // against the compiler-generated Row schema; callers cannot substitute a
    // second schema authority.
    try stdArrowImportCArray(take handle, options)
  }
}

export fn importCArrayDynamic(
  handle: take CArrayHandle,
  options: CImportOptions = CImportOptions.standard(),
): data.DynamicBatch throws CImportError {
  return unsafe {
    try stdArrowImportCArrayDynamic(take handle, options)
  }
}

export fn importCStream<Row: data.Row>(
  handle: take CStreamHandle,
  options: CStreamImportOptions = CStreamImportOptions.standard(),
): some Stream<data.Batch<Row>, CImportError> {
  return unsafe {
    stdArrowImportCStream(take handle, options)
  }
}

export fn importCStreamDynamic(
  handle: take CStreamHandle,
  options: CStreamImportOptions = CStreamImportOptions.standard(),
): some Stream<data.DynamicBatch, CImportError> {
  return unsafe {
    stdArrowImportCStreamDynamic(take handle, options)
  }
}

export fn exportCArray<Row: data.Row>(
  batch: ref data.Batch<Row>,
  options: CExportOptions = CExportOptions.standard(),
): CArrayHandle throws CExportError {
  return unsafe {
    try stdArrowExportCArray(batch, options)
  }
}
