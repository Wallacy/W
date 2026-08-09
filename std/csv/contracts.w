// Draft CSV adapter declarations for TAB1.
//
// The provider and parser remain missing.  Profiles are closed and validated
// before the source is consumed; no ambient charset, locale, formula policy,
// or schema inference is available.

import io from std.io
import data from std.data

export enum HeaderPolicy: Copy & Equatable {
  required
  forbidden
}

export enum WhitespacePolicy: Copy & Equatable {
  preserve
  trimUnquoted
}

// Decode token policy is not Copy: a custom policy owns bounded strings.
export enum BoolTokenPolicy {
  portable
  custom(trueToken: String, falseToken: String)
}

export enum FloatTokenPolicy {
  portable
  rejectNonfinite
  custom(nanToken: String, positiveInfinityToken: String, negativeInfinityToken: String)
}

export enum WriterFloatPolicy: Copy & Equatable {
  shortestRoundtrip
  rejectNonfinite
}

export enum FormulaPolicy: Copy & Equatable {
  lossless
  escapeForSpreadsheet
}

// Decode null recognition and encode null representation are separate.  An
// empty field is never null under .none, and token lists are bounded and
// validated against delimiter, quote, bool, and float tokens.
export enum NullDecodePolicy {
  none
  empty
  tokens(Array<String>)
}

export enum NullEncodePolicy {
  unavailable
  empty
  token(String)
}

export enum Profile {
  portable
  rfc4180
  custom(DecodeDialect)
}

export enum WriterProfile {
  canonical
  custom(EncodeDialect)
}

export enum LocationKind: Copy & Equatable {
  byte
  record
  field
  header
}

export struct Location: Copy & Equatable {
  kind: LocationKind
  value: u64

  export const init(kind: LocationKind, value: u64) {
    self.kind = kind
    self.value = value
  }
}

export enum LimitKind: Copy & Equatable {
  bytes
  records
  fields
  fieldBytes
  allocationBytes
  tokenBytes
  chunks
}

export struct Limits: Copy & Equatable {
  maximumBytes: u64<(1...)>
  maximumRecords: usize<(1...)>
  maximumFields: usize<(1...)>
  maximumFieldBytes: usize<(1...)>
  maximumAllocationBytes: u64<(1...)>
  maximumTokenBytes: usize<(1...)>
  maximumChunks: usize<(1...)>

  export static fn standard(): Limits {
    return Limits(
      maximumBytes: 1_073_741_824,
      maximumRecords: 1_000_000,
      maximumFields: 16_384,
      maximumFieldBytes: 16_777_216,
      maximumAllocationBytes: 1_073_741_824,
      maximumTokenBytes: 4096,
      maximumChunks: 1_000_000,
    )
  }

  export const init(
    maximumBytes: u64<(1...)>,
    maximumRecords: usize<(1...)>,
    maximumFields: usize<(1...)>,
    maximumFieldBytes: usize<(1...)>,
    maximumAllocationBytes: u64<(1...)>,
    maximumTokenBytes: usize<(1...)>,
    maximumChunks: usize<(1...)>,
  ) {
    self.maximumBytes = maximumBytes
    self.maximumRecords = maximumRecords
    self.maximumFields = maximumFields
    self.maximumFieldBytes = maximumFieldBytes
    self.maximumAllocationBytes = maximumAllocationBytes
    self.maximumTokenBytes = maximumTokenBytes
    self.maximumChunks = maximumChunks
  }
}

export enum ConfigError: Error {
  delimiterEqualsQuote
  controlDelimiter
  emptyToken
  tokenTooLong
  tokenCollision
  invalidProfile
  headerPolicyConflict
  formulaPolicyChangesData
}

// Decode and encode dialects are separate.  A reader never receives writer
// null/formula policy, and a writer never receives reader token policy.
export struct DecodeDialect {
  delimiter: u8
  quote: u8
  header: HeaderPolicy
  whitespace: WhitespacePolicy
  nullDecode: NullDecodePolicy
  boolTokens: BoolTokenPolicy
  floatTokens: FloatTokenPolicy

  init(validatedDelimiter: u8, validatedQuote: u8, validatedHeader: HeaderPolicy,
    validatedWhitespace: WhitespacePolicy, validatedNullDecode: NullDecodePolicy,
    validatedBoolTokens: BoolTokenPolicy, validatedFloatTokens: FloatTokenPolicy) {
    self.delimiter = validatedDelimiter
    self.quote = validatedQuote
    self.header = validatedHeader
    self.whitespace = validatedWhitespace
    self.nullDecode = validatedNullDecode
    self.boolTokens = validatedBoolTokens
    self.floatTokens = validatedFloatTokens
  }

  export static fn make(
    delimiter: u8,
    quote: u8,
    header: HeaderPolicy,
    whitespace: WhitespacePolicy,
    nullDecode: NullDecodePolicy,
    boolTokens: BoolTokenPolicy,
    floatTokens: FloatTokenPolicy,
    limits: ref Limits,
  ): DecodeDialect throws ConfigError {
    return unsafe {
      try stdCsvDecodeDialect(
        delimiter: delimiter,
        quote: quote,
        header: header,
        whitespace: whitespace,
        nullDecode: nullDecode,
        boolTokens: boolTokens,
        floatTokens: floatTokens,
        limits: limits,
      )
    }
  }
}

export struct EncodeDialect {
  delimiter: u8
  quote: u8
  header: HeaderPolicy
  nullEncode: NullEncodePolicy
  formula: FormulaPolicy
  floatPolicy: WriterFloatPolicy

  init(validatedDelimiter: u8, validatedQuote: u8, validatedHeader: HeaderPolicy,
    validatedNullEncode: NullEncodePolicy, validatedFormula: FormulaPolicy,
    validatedFloatPolicy: WriterFloatPolicy) {
    self.delimiter = validatedDelimiter
    self.quote = validatedQuote
    self.header = validatedHeader
    self.nullEncode = validatedNullEncode
    self.formula = validatedFormula
    self.floatPolicy = validatedFloatPolicy
  }

  export static fn make(
    delimiter: u8,
    quote: u8,
    header: HeaderPolicy,
    nullEncode: NullEncodePolicy,
    formula: FormulaPolicy,
    floatPolicy: WriterFloatPolicy,
    limits: ref Limits,
  ): EncodeDialect throws ConfigError {
    return unsafe {
      try stdCsvEncodeDialect(
        delimiter: delimiter,
        quote: quote,
        header: header,
        nullEncode: nullEncode,
        formula: formula,
        floatPolicy: floatPolicy,
        limits: limits,
      )
    }
  }
}

foreign intrinsic from "std.csv@1" {
  fn stdCsvDecodeDialect(
    delimiter: u8,
    quote: u8,
    header: HeaderPolicy,
    whitespace: WhitespacePolicy,
    nullDecode: NullDecodePolicy,
    boolTokens: BoolTokenPolicy,
    floatTokens: FloatTokenPolicy,
    limits: ref Limits,
  ): DecodeDialect throws ConfigError
  fn stdCsvEncodeDialect(
    delimiter: u8,
    quote: u8,
    header: HeaderPolicy,
    nullEncode: NullEncodePolicy,
    formula: FormulaPolicy,
    floatPolicy: WriterFloatPolicy,
    limits: ref Limits,
  ): EncodeDialect throws ConfigError
}

export struct DecodeOptions {
  profile: Profile
  limits: Limits

  export static fn standard(): DecodeOptions {
    return DecodeOptions(limits: Limits.standard(), profile: .portable)
  }

  export const init(limits: Limits, profile: take Profile = .portable) {
    self.profile = take profile
    self.limits = limits
  }
}

export struct EncodeOptions {
  profile: WriterProfile
  limits: Limits

  export static fn standard(): EncodeOptions {
    return EncodeOptions(limits: Limits.standard(), profile: .canonical)
  }

  export const init(limits: Limits, profile: take WriterProfile = .canonical) {
    self.profile = take profile
    self.limits = limits
  }
}

export enum DecodeError<SourceFailure: Error>: Error {
  source(SourceFailure)
  invalidUtf8(Location)
  duplicateHeader(String, Location)
  emptyHeader(Location)
  rowWidth(expected: usize, found: usize, location: Location)
  fieldConversion(name: String, location: Location)
  invalidQuote(Location)
  invalidRecordTerminator(Location)
  nullTokenCollision(Location)
  invalidDialect(ConfigError)
  limitExceeded(kind: LimitKind, maximum: u64, location: Location)
  trailingBytes(Location)
}

export enum EncodeError<SinkFailure: Error>: Error {
  sink(SinkFailure, data.EncodeProgress)
  missingNullRepresentation(String, data.EncodeProgress)
  unsupportedType(String, data.EncodeProgress)
  nonFiniteFloat(Location, data.EncodeProgress)
  invalidDialect(ConfigError, data.EncodeProgress)
  formulaPolicyChangesData(data.EncodeProgress)
  limitExceeded(kind: LimitKind, maximum: u64, progress: data.EncodeProgress)
}

export enum EncodeStreamError<BatchFailure: Error, SinkFailure: Error>: Error {
  batch(BatchFailure, data.EncodeProgress)
  sink(SinkFailure, data.EncodeProgress)
  missingNullRepresentation(String, data.EncodeProgress)
  unsupportedType(String, data.EncodeProgress)
  nonFiniteFloat(Location, data.EncodeProgress)
  invalidDialect(ConfigError, data.EncodeProgress)
  formulaPolicyChangesData(data.EncodeProgress)
  limitExceeded(kind: LimitKind, maximum: u64, progress: data.EncodeProgress)
}

foreign intrinsic from "std.csv@1" {
  fn stdCsvDecode<Row: data.Row, SourceFailure: Error, Source: io.ByteSource<SourceFailure>>(
    source: take Source,
    options: DecodeOptions,
  ): some Stream<data.Batch<Row>, DecodeError<SourceFailure>>

  fn stdCsvDecodeDynamic<SourceFailure: Error, Source: io.ByteSource<SourceFailure>>(
    source: take Source,
    schema: take data.Schema,
    options: DecodeOptions,
  ): some Stream<data.DynamicBatch, DecodeError<SourceFailure>>

  async fn stdCsvDecodeAll<Row: data.Row, SourceFailure: Error, Source: io.ByteSource<SourceFailure>>(
    source: take Source,
    options: DecodeOptions,
  ): data.Batch<Row> throws DecodeError<SourceFailure>

  async fn stdCsvEncodeBatch<Row: data.Row, SinkFailure: Error, Sink: io.ByteSink<SinkFailure>>(
    batch: ref data.Batch<Row>,
    sink: inout Sink,
    options: EncodeOptions,
  ): data.EncodeProgress throws EncodeError<SinkFailure>

  async fn stdCsvEncodeStream<Row: data.Row, BatchFailure: Error, SinkFailure: Error,
    Source: Stream<data.Batch<Row>, BatchFailure>, Sink: io.ByteSink<SinkFailure>>(
    batches: take Source,
    sink: inout Sink,
    options: EncodeOptions,
  ): data.EncodeProgress throws EncodeStreamError<BatchFailure, SinkFailure>
}

export fn decode<Row: data.Row, SourceFailure: Error, Source: io.ByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): some Stream<data.Batch<Row>, DecodeError<SourceFailure>> {
  return unsafe { stdCsvDecode(source: take source, options: options) }
}

export fn decodeDynamic<SourceFailure: Error, Source: io.ByteSource<SourceFailure>>(
  source: take Source,
  schema: take data.Schema,
  options: DecodeOptions = DecodeOptions.standard(),
): some Stream<data.DynamicBatch, DecodeError<SourceFailure>> {
  return unsafe {
    stdCsvDecodeDynamic(source: take source, schema: take schema, options: options)
  }
}

export async fn decodeAll<Row: data.Row, SourceFailure: Error, Source: io.ByteSource<SourceFailure>>(
  source: take Source,
  options: DecodeOptions = DecodeOptions.standard(),
): data.Batch<Row> throws DecodeError<SourceFailure> {
  return unsafe { try await stdCsvDecodeAll(source: take source, options: options) }
}

export async fn encode<Row: data.Row, SinkFailure: Error, Sink: io.ByteSink<SinkFailure>>(
  batch: ref data.Batch<Row>,
  sink: inout Sink,
  options: EncodeOptions = EncodeOptions.standard(),
): data.EncodeProgress throws EncodeError<SinkFailure> {
  return unsafe {
    try await stdCsvEncodeBatch(batch: batch, sink: inout sink, options: options)
  }
}

export async fn encode<Row: data.Row, BatchFailure: Error, SinkFailure: Error,
  Source: Stream<data.Batch<Row>, BatchFailure>, Sink: io.ByteSink<SinkFailure>>(
  batches: take Source,
  sink: inout Sink,
  options: EncodeOptions = EncodeOptions.standard(),
): data.EncodeProgress throws EncodeStreamError<BatchFailure, SinkFailure> {
  return unsafe {
    try await stdCsvEncodeStream(batches: take batches, sink: inout sink, options: options)
  }
}
