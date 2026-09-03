// Draft Parquet adapter declarations for TAB1.
//
// The decoder consumes a stable positional snapshot.  Footer, row-group,
// column-chunk, page, checksum, encryption, and compression validation remain
// provider work.  No dataset discovery or ambient Hive directory is part of
// this module.

import io from std.io
import data from std.data

export enum DecodeProfile: Copy & Equatable {
  portable
  legacyCompatible
}

export enum EncodeProfile: Copy & Equatable {
  modern
  legacyCompatible
}

export enum ChecksumPolicy: Copy & Equatable {
  whenPresent
  require
  ignore
}

export enum Compression: Copy & Equatable {
  uncompressed
  snappy
  gzip
  zstd
  lz4Raw
  brotli
}

export enum DictionaryPolicy: Copy & Equatable {
  disabled
  enabled
}

export enum StatisticsPolicy: Copy & Equatable {
  disabled
  hintsOnly
  errorOnMalformed
}

// Custom key identifiers are owned strings.  EncryptionPolicy is therefore
// move-only and cannot claim Copy/Equatable.
export enum EncryptionPolicy {
  none
  keyId(String)
}

export enum LimitKind: Copy & Equatable {
  encodedBytes
  decodedBytes
  allocations
  footerBytes
  thriftStringBytes
  thriftContainers
  nesting
  rowGroups
  columnChunks
  pages
  dictionaries
  indexes
  bloomFilters
  compressionRatio
  keyBytes
}

export struct Limits: Copy & Equatable {
  let maximumEncodedBytes: u64<(1...)>
  let maximumDecodedBytes: u64<(1...)>
  let maximumAllocationBytes: u64<(1...)>
  let maximumFooterBytes: u64<(1...)>
  let maximumThriftStringBytes: u64<(1...)>
  let maximumThriftContainers: usize<(1...)>
  let maximumNesting: usize<(1...)>
  let maximumRowGroups: usize<(1...)>
  let maximumColumnChunks: usize<(1...)>
  let maximumPages: usize<(1...)>
  let maximumDictionaries: usize<(1...)>
  let maximumIndexes: usize<(1...)>
  let maximumBloomFilters: usize<(1...)>
  let maximumCompressionRatio: usize<(1...)>
  let maximumKeyBytes: u64<(1...)>

  export static fn standard(): Limits {
    return Limits(
      maximumEncodedBytes: 1_073_741_824,
      maximumDecodedBytes: 4_294_967_296,
      maximumAllocationBytes: 4_294_967_296,
      maximumFooterBytes: 16_777_216,
      maximumThriftStringBytes: 16_777_216,
      maximumThriftContainers: 1_000_000,
      maximumNesting: 64,
      maximumRowGroups: 1_000_000,
      maximumColumnChunks: 16_384_000,
      maximumPages: 10_000_000,
      maximumDictionaries: 1_000_000,
      maximumIndexes: 1_000_000,
      maximumBloomFilters: 1_000_000,
      maximumCompressionRatio: 100,
      maximumKeyBytes: 4096,
    )
  }

  export const init(
    maximumEncodedBytes: u64<(1...)>,
    maximumDecodedBytes: u64<(1...)>,
    maximumAllocationBytes: u64<(1...)>,
    maximumFooterBytes: u64<(1...)>,
    maximumThriftStringBytes: u64<(1...)>,
    maximumThriftContainers: usize<(1...)>,
    maximumNesting: usize<(1...)>,
    maximumRowGroups: usize<(1...)>,
    maximumColumnChunks: usize<(1...)>,
    maximumPages: usize<(1...)>,
    maximumDictionaries: usize<(1...)>,
    maximumIndexes: usize<(1...)>,
    maximumBloomFilters: usize<(1...)>,
    maximumCompressionRatio: usize<(1...)>,
    maximumKeyBytes: u64<(1...)>,
  ) {
    self.maximumEncodedBytes = maximumEncodedBytes
    self.maximumDecodedBytes = maximumDecodedBytes
    self.maximumAllocationBytes = maximumAllocationBytes
    self.maximumFooterBytes = maximumFooterBytes
    self.maximumThriftStringBytes = maximumThriftStringBytes
    self.maximumThriftContainers = maximumThriftContainers
    self.maximumNesting = maximumNesting
    self.maximumRowGroups = maximumRowGroups
    self.maximumColumnChunks = maximumColumnChunks
    self.maximumPages = maximumPages
    self.maximumDictionaries = maximumDictionaries
    self.maximumIndexes = maximumIndexes
    self.maximumBloomFilters = maximumBloomFilters
    self.maximumCompressionRatio = maximumCompressionRatio
    self.maximumKeyBytes = maximumKeyBytes
  }
}

export enum KeyResolverError: Error {
  scopeDenied
  keyUnavailable
  keyTooLarge
  providerFailure
}

export enum KeyScope: Copy & Equatable {
  parquetFooter
  parquetPage
}

// The host/service entry injects this move-only capability from an approved
// secret provider.  It is not ambient and has no public forgeable factory;
// the entry binding never exposes plaintext.
export struct KeyResolverCapability {
  let handle: ParquetKeyCapability

  init(validatedHandle: ParquetKeyCapability) {
    self.handle = validatedHandle
  }

}

// KeyResolver is an explicit, bounded capability.  It does not expose a
// plaintext key and cannot be inferred from a path, environment, or profile.
export struct KeyResolver {
  let handle: ParquetKeyResolverHandle

  init(validatedHandle: ParquetKeyResolverHandle) {
    self.handle = validatedHandle
  }

  export static fn from(
    capability: take KeyResolverCapability,
    scope: KeyScope,
    limits: ref Limits = ref Limits.standard(),
  ): KeyResolver throws KeyResolverError {
    return KeyResolver(validatedHandle: unsafe {
      try stdParquetKeyResolverFromCapability(
        take capability,
        scope,
        limits,
      )
    })
  }
}

// WriterPlan owns optional provider/codec digests and is therefore move-only.
// A provider rejects encryption or a deterministic claim when either digest
// is absent; no ambient codec or extension is selected.
export struct WriterPlan {
  let compression: Compression
  let rowGroupBytes: usize<(1...)>
  let pageBytes: usize<(1...)>
  let dictionary: DictionaryPolicy
  let statistics: StatisticsPolicy
  let checksum: ChecksumPolicy
  let encryption: EncryptionPolicy
  let codecDigest: String?
  let providerDigest: String?

  export static fn standard(): WriterPlan {
    return WriterPlan(
      compression: .zstd,
      rowGroupBytes: 134_217_728,
      pageBytes: 1_048_576,
      dictionary: .enabled,
      statistics: .hintsOnly,
      checksum: .whenPresent,
      encryption: .none,
      codecDigest: .none,
      providerDigest: .none,
    )
  }

  export const init(
    compression: Compression,
    rowGroupBytes: usize<(1...)>,
    pageBytes: usize<(1...)>,
    dictionary: DictionaryPolicy,
    statistics: StatisticsPolicy,
    checksum: ChecksumPolicy,
    encryption: take EncryptionPolicy,
    codecDigest: take String?,
    providerDigest: take String?,
  ) {
    self.compression = compression
    self.rowGroupBytes = rowGroupBytes
    self.pageBytes = pageBytes
    self.dictionary = dictionary
    self.statistics = statistics
    self.checksum = checksum
    self.encryption = take encryption
    self.codecDigest = take codecDigest
    self.providerDigest = take providerDigest
  }
}

export struct DecodeOptions {
  let profile: DecodeProfile
  let binding: data.BindingPolicy
  let mapping: data.MappingPolicy
  let checksum: ChecksumPolicy
  let limits: Limits
  let keyResolver: KeyResolver?

  export static fn standard(): DecodeOptions {
    return DecodeOptions(
      limits: Limits.standard(),
      profile: .portable,
      binding: .exact,
      mapping: .none,
      checksum: .whenPresent,
      keyResolver: .none,
    )
  }

  export const init(
    limits: Limits,
    profile: DecodeProfile = .portable,
    binding: data.BindingPolicy = .exact,
    mapping: data.MappingPolicy = .none,
    checksum: ChecksumPolicy = .whenPresent,
    keyResolver: take KeyResolver? = .none,
  ) {
    self.profile = profile
    self.binding = binding
    self.mapping = mapping
    self.checksum = checksum
    self.limits = limits
    self.keyResolver = take keyResolver
  }
}

export struct EncodeOptions {
  let profile: EncodeProfile
  let plan: WriterPlan
  let limits: Limits
  let keyResolver: KeyResolver?

  export static fn standard(): EncodeOptions {
    return EncodeOptions(
      limits: Limits.standard(),
      profile: .modern,
      plan: WriterPlan.standard(),
      keyResolver: .none,
    )
  }

  export const init(
    limits: Limits,
    profile: EncodeProfile = .modern,
    plan: take WriterPlan = WriterPlan.standard(),
    keyResolver: take KeyResolver? = .none,
  ) {
    self.profile = profile
    self.plan = take plan
    self.limits = limits
    self.keyResolver = take keyResolver
  }
}

export enum DecodeError<SourceFailure: Error>: Error {
  source(SourceFailure)
  sourceUnstable
  invalidMagic
  invalidFooter
  invalidOffset
  invalidSize
  invalidThrift
  invalidSchema
  schemaMismatch
  logicalTypeMismatch(String)
  mappingRequired
  unsupportedCompression(String)
  encryptedKeyRequired
  keyResolver(KeyResolverError)
  checksumMismatch
  malformedStatistics
  undefinedStatisticsOrdering
  malformedIndex
  malformedBloomFilter
  decompressionRatio
  limitExceeded(kind: LimitKind, maximum: u64)
}

export enum EncodeError<SinkFailure: Error>: Error {
  sink(SinkFailure, data.EncodeProgress)
  unsupportedType(String, data.EncodeProgress)
  invalidLogicalType(String, data.EncodeProgress)
  invalidSchemaMapping(data.EncodeProgress)
  invalidPlan(data.EncodeProgress)
  keyResolver(KeyResolverError, data.EncodeProgress)
  checksumFailure(data.EncodeProgress)
  missingDeterministicDigest(data.EncodeProgress)
  limitExceeded(kind: LimitKind, maximum: u64, progress: data.EncodeProgress)
}

export enum EncodeStreamError<BatchFailure: Error, SinkFailure: Error>: Error {
  batch(BatchFailure, data.EncodeProgress)
  sink(SinkFailure, data.EncodeProgress)
  unsupportedType(String, data.EncodeProgress)
  invalidLogicalType(String, data.EncodeProgress)
  invalidSchemaMapping(data.EncodeProgress)
  invalidPlan(data.EncodeProgress)
  keyResolver(KeyResolverError, data.EncodeProgress)
  checksumFailure(data.EncodeProgress)
  missingDeterministicDigest(data.EncodeProgress)
  limitExceeded(kind: LimitKind, maximum: u64, progress: data.EncodeProgress)
}

foreign intrinsic from "std.parquet@1" {
  type ParquetKeyResolverHandle
  type ParquetKeyCapability

  // Host/provider boundary: capability creation is explicit and scoped.
  fn stdParquetKeyResolverFromCapability(
    _ capability: take ParquetKeyCapability,
    _ scope: KeyScope,
    _ limits: ref Limits,
  ): ParquetKeyResolverHandle throws KeyResolverError

  fn stdParquetDecode<Row: data.Row, SourceFailure: Error,
    Source: io.SnapshotByteSource<SourceFailure>>(
    _ source: take Source,
    _ options: DecodeOptions,
  ): some Stream<data.Batch<Row>, DecodeError<SourceFailure>>

  fn stdParquetDecodeDynamic<SourceFailure: Error,
    Source: io.SnapshotByteSource<SourceFailure>>(
    _ source: take Source,
    _ options: DecodeOptions,
  ): some Stream<data.DynamicBatch, DecodeError<SourceFailure>>

  async fn stdParquetDecodeAll<Row: data.Row, SourceFailure: Error,
    Source: io.SnapshotByteSource<SourceFailure>>(
    _ source: take Source,
    _ options: DecodeOptions,
  ): data.Batch<Row> throws DecodeError<SourceFailure>

  async fn stdParquetEncodeBatch<Row: data.Row, SinkFailure: Error,
    Sink: io.ByteSink<SinkFailure>>(
    _ batch: ref data.Batch<Row>,
    _ sink: inout Sink,
    _ options: EncodeOptions,
  ): data.EncodeProgress throws EncodeError<SinkFailure>

  async fn stdParquetEncodeStream<Row: data.Row, BatchFailure: Error, SinkFailure: Error,
    Source: Stream<data.Batch<Row>, BatchFailure>, Sink: io.ByteSink<SinkFailure>>(
    _ batches: take Source,
    _ sink: inout Sink,
    _ options: EncodeOptions,
  ): data.EncodeProgress throws EncodeStreamError<BatchFailure, SinkFailure>
}

export fn decode<Row: data.Row, SourceFailure: Error,
  Source: io.SnapshotByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): some Stream<data.Batch<Row>, DecodeError<SourceFailure>> {
  return unsafe { stdParquetDecode(take source, options) }
}

export fn decodeDynamic<SourceFailure: Error,
  Source: io.SnapshotByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): some Stream<data.DynamicBatch, DecodeError<SourceFailure>> {
  return unsafe { stdParquetDecodeDynamic(take source, options) }
}

export async fn decodeAll<Row: data.Row, SourceFailure: Error,
  Source: io.SnapshotByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): data.Batch<Row> throws DecodeError<SourceFailure> {
  return unsafe { try await stdParquetDecodeAll(take source, options) }
}

export async fn encode<Row: data.Row, SinkFailure: Error, Sink: io.ByteSink<SinkFailure>>(
  batch: ref data.Batch<Row>,
  sink: inout Sink,
  options: EncodeOptions = EncodeOptions.standard(),
): data.EncodeProgress throws EncodeError<SinkFailure> {
  return unsafe {
    try await stdParquetEncodeBatch(batch, inout sink, options)
  }
}

export async fn encode<Row: data.Row, BatchFailure: Error, SinkFailure: Error,
  Source: Stream<data.Batch<Row>, BatchFailure>, Sink: io.ByteSink<SinkFailure>>(
  batches: take Source,
  sink: inout Sink,
  options: EncodeOptions = EncodeOptions.standard(),
): data.EncodeProgress throws EncodeStreamError<BatchFailure, SinkFailure> {
  return unsafe {
    try await stdParquetEncodeStream(
      take batches,
      inout sink,
      options,
    )
  }
}
