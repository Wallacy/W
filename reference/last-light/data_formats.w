// TAB1 telemetry routes for the Última Luz horizon.
//
// This source is a design oracle. It does not claim that W executes a codec.
// Every route uses the same generated Row schema, nominal SchemaIdentity, and
// bounded outcome summary.

import io from std.io
import data from std.data
import csv from std.csv
import parquet from std.parquet
import arrow from std.arrow

// Catalog symbols used by the draft SDK surface:
// data.Row Schema SchemaIdentity Batch DynamicBatch Column StringColumn
// BytesColumn FieldDescriptor CopyPolicy BindingPolicy MappingPolicy Limits
// EncodeProgress bind schema
// csv.Profile WriterProfile DecodeDialect EncodeDialect HeaderPolicy NullDecodePolicy
// NullEncodePolicy DecodeError EncodeError EncodeStreamError decode
// decodeDynamic decodeAll encode
// parquet.DecodeProfile EncodeProfile ChecksumPolicy KeyResolverCapability
// KeyResolver WriterPlan
// DecodeError EncodeError EncodeStreamError decode decodeDynamic decodeAll encode
// arrow DeviceKind Limits DecodeOptions CImportOptions CStreamImportOptions BlockingQuota
// CExportOptions DecodeError CImportError CExportError EncodeError
// EncodeStreamError decodeIpcStream decodeIpcFile decodeIpcStreamAll
// decodeIpcFileAll decodeIpcStreamDynamic decodeIpcFileDynamic
// encodeIpcStream encodeIpcFile importCArray importCArrayDynamic importCStream

export struct TabularTelemetryRow: data.Row {
  let sequence: u64
  let hawkingFlux: f64
  let warning: String?
}

export struct TabularOutcome {
  let rows: u64
  let schemaIdentity: data.SchemaIdentity
  let warningViewLength: u64
  let warningWasNull: Bool
  let warningCopy: String?
  let bytesCommitted: u64
  let completeRecords: u64
  let partialRecord: Bool
}

// Schema is generated and validated from the Row declaration.  No field
// names, logical-type strings, or user-created descriptors occur here.
export fn telemetrySchema(): data.Schema {
  return data.schema<TabularTelemetryRow>()
}

export fn summarize(
  batch: take data.Batch<TabularTelemetryRow>,
): TabularOutcome {
  let schema = batch.schema()
  let identity = schema.identity()
  let rows = batch.rowCount()
  if rows == 0 {
    return TabularOutcome(
      rows: 0,
      schemaIdentity: take identity,
      warningViewLength: 0,
      warningWasNull: true,
      warningCopy: .none,
      bytesCommitted: 0,
      completeRecords: 0,
      partialRecord: false,
    )
  }
  // `.warning` is a compiler-generated descriptor.  The returned column is a
  // loan tied to `batch`; neither the column nor its view is returned.
  // Nested or custom fields do not receive a universal view.  Their adapters
  // project a typed core field or materialize an owner explicitly.
  let warning: view data.StringColumn<TabularTelemetryRow> = batch.column(string: .warning)
  let warningView: view String? = warning.view(at: 0)
  let (warningViewLength, warningWasNull) = switch warningView {
    case .some(let value): (u64(value.bytes.count), false)
    case .none: (0, true)
  }
  let warningCopy = warning.copy(at: 0)

  return TabularOutcome(
    rows: u64(rows),
    schemaIdentity: take identity,
    warningViewLength: warningViewLength,
    warningWasNull: warningWasNull,
    warningCopy: take warningCopy,
    bytesCommitted: 0,
    completeRecords: u64(rows),
    partialRecord: false,
  )
}

// A CSV upload uses the typed overload and its explicit null policy.  The
// typed codec error remains visible to the caller; it is not flattened into a
// generic route failure.
export async fn uploadCsv<Failure: Error, Source: io.ByteSource<Failure>>(
  source: take Source,
  options: csv.DecodeOptions = csv.DecodeOptions.standard(),
): TabularOutcome throws csv.DecodeError<Failure> {
  let batch = try await csv.decodeAll<TabularTelemetryRow, Failure, Source>(
    source: take source,
    options: options,
  )
  return summarize(batch: take batch)
}

// A Parquet archive uses the finite positional snapshot contract.
export async fn archiveParquet<Failure: Error,
  Source: io.SnapshotByteSource<Failure>>(
  source: take Source,
  options: parquet.DecodeOptions = parquet.DecodeOptions.standard(),
): TabularOutcome throws parquet.DecodeError<Failure> {
  let batch = try await parquet.decodeAll<TabularTelemetryRow, Failure, Source>(
    source: take source,
    options: options,
  )
  return summarize(batch: take batch)
}

// An Arrow IPC stream carries the same typed rows to a service handoff.  The
// all-batches convenience consumes every published batch before summarizing;
// a later source error remains the typed Arrow DecodeError.
export async fn handoffArrow<Failure: Error, Source: io.ByteSource<Failure>>(
  source: take Source,
  options: arrow.DecodeOptions = arrow.DecodeOptions.standard(),
): TabularOutcome throws arrow.DecodeError<Failure> {
  let batch = try await arrow.decodeIpcStreamAll<TabularTelemetryRow, Failure, Source>(
    source: take source,
    options: options,
  )
  return summarize(batch: take batch)
}

// C Data import is trusted in-process.  The raw Arrow schema travels with
// the array handle and is checked against the generated Row schema.
export fn importTrustedCArray(
  handle: take arrow.CArrayHandle,
  options: arrow.CImportOptions = arrow.CImportOptions.standard(),
): TabularOutcome throws arrow.CImportError {
  let batch = try arrow.importCArray<TabularTelemetryRow>(
    handle: take handle,
    options: options,
  )
  return summarize(batch: take batch)
}

export async fn encodeCsv<Failure: Error, Sink: io.ByteSink<Failure>>(
  batch: ref data.Batch<TabularTelemetryRow>,
  sink: inout Sink,
  options: csv.EncodeOptions = csv.EncodeOptions.standard(),
): data.EncodeProgress throws csv.EncodeError<Failure> {
  return try await csv.encode(
    batch: batch,
    sink: inout sink,
    options: options,
  )
}

// The machine/cases and R1 oracle bind these IDs to real route symbols.  This
// textual list is navigation only, not evidence that a parser ran.
export fn tabularAdversaries(): Array<String> {
  return [
    "TAB1-csv-multiline-quoted-split",
    "TAB1-csv-duplicate-header",
    "TAB1-csv-empty-vs-null",
    "TAB1-csv-invalid-utf8",
    "TAB1-csv-row-width",
    "TAB1-csv-bare-cr",
    "TAB1-csv-token-collision",
    "TAB1-csv-negative-finite-nan",
    "TAB1-csv-partial-encode",
    "TAB1-parquet-corrupt-footer-offset-size",
    "TAB1-parquet-decompression-bomb",
    "TAB1-parquet-thrift-limits",
    "TAB1-parquet-logical-mismatch",
    "TAB1-parquet-legacy-list",
    "TAB1-parquet-checksum",
    "TAB1-parquet-encrypted-no-key",
    "TAB1-parquet-source-instability",
    "TAB1-parquet-incomplete-footer",
    "TAB1-parquet-missing-deterministic-digest",
    "TAB1-arrow-schema-divergence",
    "TAB1-arrow-dictionary-before-definition",
    "TAB1-arrow-file-dictionary-replacement",
    "TAB1-arrow-non-native-endian",
    "TAB1-arrow-copy-never",
    "TAB1-arrow-alignment",
    "TAB1-arrow-untrusted-c",
    "TAB1-arrow-double-release",
    "TAB1-arrow-device-as-cpu",
    "TAB1-arrow-blocking-quota",
    "TAB1-arrow-cancel-after-progress",
  ]
}
