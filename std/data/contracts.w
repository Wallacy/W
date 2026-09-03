// Draft TAB1 data carriers and nominal schema descriptors.
//
// The provider remains missing.  Safe W receives only validated, opaque
// handles.  There is no Any, reflection, user-implemented carrier protocol,
// or public constructor that can forge a descriptor or a cross-row column.

export protocol Row {}

export enum CopyPolicy: Copy & Equatable {
  never
  ifNeeded
  always
}

export enum BindingPolicy: Copy & Equatable {
  exact
  project
}

// A non-total format mapping is never inferred.  `.none` requires exact
// logical types; `.explicit` means the caller supplied a DTO/extension plan.
export enum MappingPolicy: Copy & Equatable {
  none
  explicit
}

export enum IntegerSign: Copy & Equatable {
  signed
  unsigned
}

export enum IntegerWidth: Copy & Equatable {
  bits8
  bits16
  bits32
  bits64
  bits128
}

export enum FloatWidth: Copy & Equatable {
  f16
  f32
  f64
}

export enum TemporalPrecision: Copy & Equatable {
  seconds
  milliseconds
  microseconds
  nanoseconds
}

// Logical descriptors are opaque validated handles.  They are not a
// recursive enum: option/list/map keep their child handles in provider-owned
// indirect storage.  Constructors enforce finite depth and canonical bounds
// before a descriptor can enter a SchemaField.
export struct FixedDecimalType {
  let handle: FixedDecimalHandle

  init(validatedHandle: FixedDecimalHandle) {
    self.handle = validatedHandle
  }

  export static fn make(precision: u16, scale: i16): FixedDecimalType throws SchemaError {
    return FixedDecimalType(validatedHandle: unsafe {
      try stdDataFixedDecimal(precision, scale)
    })
  }
}

export struct LogicalType {
  let handle: LogicalTypeHandle

  init(validatedHandle: LogicalTypeHandle) {
    self.handle = validatedHandle
  }

  export static fn boolean(): LogicalType {
    return LogicalType(validatedHandle: unsafe { stdDataLogicalBoolean() })
  }

  export static fn integer(width: IntegerWidth, sign: IntegerSign): LogicalType {
    return LogicalType(validatedHandle: unsafe {
      stdDataLogicalInteger(width, sign)
    })
  }

  export static fn float(width: FloatWidth): LogicalType {
    return LogicalType(validatedHandle: unsafe { stdDataLogicalFloat(width) })
  }

  export static fn fixedDecimal(
    precision: u16,
    scale: i16,
  ): LogicalType throws SchemaError {
    return LogicalType(validatedHandle: unsafe {
      try stdDataLogicalFixedDecimal(precision, scale)
    })
  }

  export static fn string(): LogicalType {
    return LogicalType(validatedHandle: unsafe { stdDataLogicalString() })
  }

  export static fn bytes(): LogicalType {
    return LogicalType(validatedHandle: unsafe { stdDataLogicalBytes() })
  }

  export static fn uuid(): LogicalType {
    return LogicalType(validatedHandle: unsafe { stdDataLogicalUuid() })
  }

  // A date is a calendar day.  It has no clock precision and no timezone.
  export static fn date(): LogicalType {
    return LogicalType(validatedHandle: unsafe { stdDataLogicalDate() })
  }

  // A time is a time-of-day.  It has precision but no timezone conversion.
  export static fn time(precision: TemporalPrecision): LogicalType {
    return LogicalType(validatedHandle: unsafe {
      stdDataLogicalTime(precision)
    })
  }

  // An instant is an absolute UTC value.  A timezone name is not a hidden
  // conversion policy.
  export static fn instant(precision: TemporalPrecision): LogicalType {
    return LogicalType(validatedHandle: unsafe {
      stdDataLogicalInstant(precision)
    })
  }

  // A localDateTime is a civil value.  It has no zone and never converts to an
  // instant in this module.
  export static fn localDateTime(precision: TemporalPrecision): LogicalType {
    return LogicalType(validatedHandle: unsafe {
      stdDataLogicalLocalDateTime(precision)
    })
  }

  export static fn option(
    of child: take LogicalType,
    limits: ref Limits = ref Limits.standard(),
  ): LogicalType throws SchemaError {
    return LogicalType(validatedHandle: unsafe {
      try stdDataLogicalOption(take child, limits)
    })
  }

  export static fn list(
    of child: take LogicalType,
    limits: ref Limits = ref Limits.standard(),
  ): LogicalType throws SchemaError {
    return LogicalType(validatedHandle: unsafe {
      try stdDataLogicalList(take child, limits)
    })
  }

  export static fn map(
    key: take LogicalType,
    value: take LogicalType,
    limits: ref Limits = ref Limits.standard(),
  ): LogicalType throws SchemaError {
    return LogicalType(validatedHandle: unsafe {
      try stdDataLogicalMap(take key, take value, limits)
    })
  }

  export static fn nested(
    identity: ref SchemaIdentity,
  ): LogicalType throws SchemaError {
    return LogicalType(validatedHandle: unsafe {
      try stdDataLogicalNested(identity)
    })
  }

  export static fn extension(
    extension: take SemanticExtension,
    limits: ref Limits = ref Limits.standard(),
  ): LogicalType throws SchemaError {
    return LogicalType(validatedHandle: unsafe {
      try stdDataLogicalExtension(take extension, limits)
    })
  }
}

export struct SemanticExtension {
  let handle: SemanticExtensionHandle

  init(validatedHandle: SemanticExtensionHandle) {
    self.handle = validatedHandle
  }

  export static fn make(
    id: take String,
    version: u32,
    parameters: take Array<Bytes>,
    limits: ref Limits = ref Limits.standard(),
  ): SemanticExtension throws SchemaError {
    return SemanticExtension(validatedHandle: unsafe {
      try stdDataSemanticExtension(
        take id,
        version,
        take parameters,
        limits,
      )
    })
  }
}

// Values use bounded canonical representations.  A decimal stores an
// integer coefficient with its validated type; UUID is exactly 16 bytes;
// temporal values use bounded calendar/clock/instant counts, not
// locale-dependent text.  Zoned or named timezone semantics are a later
// semantic extension with its own nominal ZoneId/version contract.
export struct DecimalValue {
  let handle: DecimalValueHandle

  init(validatedHandle: DecimalValueHandle) {
    self.handle = validatedHandle
  }

  export static fn make(
    decimalType: take FixedDecimalType,
    unscaled: i128,
  ): DecimalValue throws SchemaError {
    return DecimalValue(validatedHandle: unsafe {
      try stdDataDecimalValue(take decimalType, unscaled)
    })
  }
}

export struct UUIDValue {
  let handle: UUIDValueHandle

  init(validatedHandle: UUIDValueHandle) {
    self.handle = validatedHandle
  }

  export static fn make(bytes: take Bytes): UUIDValue throws SchemaError {
    return UUIDValue(validatedHandle: unsafe { try stdDataUuidValue(take bytes) })
  }
}

export struct DateValue {
  let handle: DateValueHandle

  init(validatedHandle: DateValueHandle) {
    self.handle = validatedHandle
  }

  export static fn make(daysSinceEpoch: i32): DateValue throws SchemaError {
    return DateValue(validatedHandle: unsafe {
      try stdDataDateValue(daysSinceEpoch)
    })
  }
}

export struct TimeValue {
  let handle: TimeValueHandle

  init(validatedHandle: TimeValueHandle) {
    self.handle = validatedHandle
  }

  export static fn make(nanosecondsSinceMidnight: u64): TimeValue throws SchemaError {
    return TimeValue(validatedHandle: unsafe {
      try stdDataTimeValue(nanosecondsSinceMidnight)
    })
  }
}

export struct InstantValue {
  let handle: InstantValueHandle

  init(validatedHandle: InstantValueHandle) {
    self.handle = validatedHandle
  }

  export static fn make(epochNanoseconds: i128): InstantValue throws SchemaError {
    return InstantValue(validatedHandle: unsafe {
      try stdDataInstantValue(epochNanoseconds)
    })
  }
}

export struct LocalDateTimeValue {
  let handle: LocalDateTimeValueHandle

  init(validatedHandle: LocalDateTimeValueHandle) {
    self.handle = validatedHandle
  }

  // The count is in a validated civil calendar representation.  It is not an
  // epoch and does not imply conversion to an Instant.
  export static fn make(civilTicks: i128): LocalDateTimeValue throws SchemaError {
    return LocalDateTimeValue(validatedHandle: unsafe {
      try stdDataLocalDateTimeValue(civilTicks)
    })
  }
}

export enum LimitKind: Copy & Equatable {
  rows
  columns
  fields
  buffers
  totalBytes
  allocationBytes
  nesting
  metadataBytes
  stringBytes
  chunks
  extensionParameters
}

export struct Limits: Copy & Equatable {
  let maximumRows: usize<(1...)>
  let maximumColumns: usize<(1...)>
  let maximumFields: usize<(1...)>
  let maximumBuffers: usize<(1...)>
  // Logical and encoded totals use u64 so the same contract is valid on
  // 32-bit and 64-bit targets.  A provider checks allocation limits against
  // the target usize before reserving memory.
  let maximumTotalBytes: u64<(1...)>
  let maximumAllocationBytes: u64<(1...)>
  let maximumNesting: usize<(1...)>
  let maximumMetadataBytes: u64<(1...)>
  let maximumStringBytes: u64<(1...)>
  let maximumChunks: usize<(1...)>
  let maximumExtensionParameters: usize<(1...)>

  export static fn standard(): Limits {
    return Limits(
      maximumRows: 1_000_000,
      maximumColumns: 4_096,
      maximumFields: 16_384,
      maximumBuffers: 1_000_000,
      maximumTotalBytes: 1_073_741_824,
      maximumAllocationBytes: 1_073_741_824,
      maximumNesting: 64,
      maximumMetadataBytes: 16_777_216,
      maximumStringBytes: 16_777_216,
      maximumChunks: 1_000_000,
      maximumExtensionParameters: 64,
    )
  }

  export const init(
    maximumRows: usize<(1...)>,
    maximumColumns: usize<(1...)>,
    maximumFields: usize<(1...)>,
    maximumBuffers: usize<(1...)>,
    maximumTotalBytes: u64<(1...)>,
    maximumAllocationBytes: u64<(1...)>,
    maximumNesting: usize<(1...)>,
    maximumMetadataBytes: u64<(1...)>,
    maximumStringBytes: u64<(1...)>,
    maximumChunks: usize<(1...)>,
    maximumExtensionParameters: usize<(1...)>,
  ) {
    self.maximumRows = maximumRows
    self.maximumColumns = maximumColumns
    self.maximumFields = maximumFields
    self.maximumBuffers = maximumBuffers
    self.maximumTotalBytes = maximumTotalBytes
    self.maximumAllocationBytes = maximumAllocationBytes
    self.maximumNesting = maximumNesting
    self.maximumMetadataBytes = maximumMetadataBytes
    self.maximumStringBytes = maximumStringBytes
    self.maximumChunks = maximumChunks
    self.maximumExtensionParameters = maximumExtensionParameters
  }
}

export enum SchemaError: Error {
  emptySchemaRequiresRowCount
  emptyName
  invalidName
  duplicateName(String)
  invalidBounds
  invalidDecimal
  invalidTemporal
  invalidExtension
  invalidNestedIdentity
  limitExceeded(LimitKind, u64)
}

foreign intrinsic from "std.data@1" {
  type FixedDecimalHandle
  type LogicalTypeHandle
  type SemanticExtensionHandle
  type DecimalValueHandle
  type UUIDValueHandle
  type DateValueHandle
  type TimeValueHandle
  type InstantValueHandle
  type LocalDateTimeValueHandle
  type DynamicExtensionValueHandle
  type DynamicListHandle
  type DynamicMapHandle
  type DynamicNestedHandle
  type SchemaFieldHandle
  type SchemaHandle
  type SchemaIdentityHandle
  type FieldDescriptorHandle
  type BatchHandle
  type DynamicBatchHandle
  type ColumnHandle
  type StringColumnHandle
  type BytesColumnHandle
  type DynamicColumnHandle

  fn stdDataFixedDecimal(_ precision: u16, _ scale: i16): FixedDecimalHandle throws SchemaError
  fn stdDataLogicalBoolean(): LogicalTypeHandle
  fn stdDataLogicalInteger(_ width: IntegerWidth, _ sign: IntegerSign): LogicalTypeHandle
  fn stdDataLogicalFloat(_ width: FloatWidth): LogicalTypeHandle
  fn stdDataLogicalFixedDecimal(
    _ precision: u16,
    _ scale: i16,
  ): LogicalTypeHandle throws SchemaError
  fn stdDataLogicalString(): LogicalTypeHandle
  fn stdDataLogicalBytes(): LogicalTypeHandle
  fn stdDataLogicalUuid(): LogicalTypeHandle
  fn stdDataLogicalDate(): LogicalTypeHandle
  fn stdDataLogicalTime(_ precision: TemporalPrecision): LogicalTypeHandle
  fn stdDataLogicalInstant(_ precision: TemporalPrecision): LogicalTypeHandle
  fn stdDataLogicalLocalDateTime(_ precision: TemporalPrecision): LogicalTypeHandle
  fn stdDataLogicalOption(
    _ child: take LogicalType,
    _ limits: ref Limits,
  ): LogicalTypeHandle throws SchemaError
  fn stdDataLogicalList(
    _ child: take LogicalType,
    _ limits: ref Limits,
  ): LogicalTypeHandle throws SchemaError
  fn stdDataLogicalMap(
    _ key: take LogicalType,
    _ value: take LogicalType,
    _ limits: ref Limits,
  ): LogicalTypeHandle throws SchemaError
  fn stdDataLogicalNested(_ identity: ref SchemaIdentity): LogicalTypeHandle throws SchemaError
  fn stdDataLogicalExtension(
    _ extension: take SemanticExtension,
    _ limits: ref Limits,
  ): LogicalTypeHandle throws SchemaError
  fn stdDataSemanticExtension(
    _ id: take String,
    _ version: u32,
    _ parameters: take Array<Bytes>,
    _ limits: ref Limits,
  ): SemanticExtensionHandle throws SchemaError
  fn stdDataDecimalValue(
    _ decimalType: take FixedDecimalType,
    _ unscaled: i128,
  ): DecimalValueHandle throws SchemaError
  fn stdDataUuidValue(_ bytes: take Bytes): UUIDValueHandle throws SchemaError
  fn stdDataDateValue(_ daysSinceEpoch: i32): DateValueHandle throws SchemaError
  fn stdDataTimeValue(_ nanosecondsSinceMidnight: u64): TimeValueHandle throws SchemaError
  fn stdDataInstantValue(_ epochNanoseconds: i128): InstantValueHandle throws SchemaError
  fn stdDataLocalDateTimeValue(_ civilTicks: i128): LocalDateTimeValueHandle throws SchemaError
  fn stdDataDynamicExtensionValue(
    _ logicalType: take LogicalType,
    _ extension: take SemanticExtension,
    _ storage: take Bytes,
    _ limits: ref Limits,
  ): DynamicExtensionValueHandle throws SchemaError
  fn stdDataDynamicList(
    _ values: take Array<DynamicValue>,
    _ limits: ref Limits,
  ): DynamicListHandle throws SchemaError
  fn stdDataDynamicMap(
    _ entries: take Array<DynamicEntry>,
    _ limits: ref Limits,
  ): DynamicMapHandle throws SchemaError
  fn stdDataDynamicNested(
    _ entries: take Array<DynamicEntry>,
    _ limits: ref Limits,
  ): DynamicNestedHandle throws SchemaError


  fn stdDataSchemaField(
    _ name: take String,
    _ logicalType: take LogicalType,
    _ nullable: Bool,
    _ limits: ref Limits,
  ): SchemaFieldHandle throws SchemaError

  fn stdDataSchema(
    _ fields: take Array<SchemaField>,
    _ rowCount: usize?,
    _ limits: ref Limits,
  ): SchemaHandle throws SchemaError

  fn stdDataSchemaIdentity(_ handle: ref SchemaHandle): SchemaIdentityHandle
  fn stdDataSchemaFieldCount(_ handle: ref SchemaHandle): usize

  fn stdDataGeneratedField<Owner: Row, Value>(
    _ name: String,
  ): FieldDescriptorHandle

  fn stdDataBatchSchema(_ handle: ref BatchHandle): SchemaHandle
  fn stdDataBatchRowCount(_ handle: ref BatchHandle): usize
  // Column handles are loans.  They have no retain/release operation and
  // cannot outlive the Batch passed to these intrinsics.
  fn stdDataBatchColumn(_ handle: ref BatchHandle, _ field: ref FieldDescriptorHandle): view ColumnHandle
  fn stdDataBatchStringColumn(_ handle: ref BatchHandle, _ field: ref FieldDescriptorHandle): view StringColumnHandle
  fn stdDataBatchBytesColumn(_ handle: ref BatchHandle, _ field: ref FieldDescriptorHandle): view BytesColumnHandle
  fn stdDataColumnCount(_ handle: view ColumnHandle): usize
  fn stdDataColumnCopy<Value>(_ handle: view ColumnHandle, _ index: usize): Value
  fn stdDataStringColumnCount(_ handle: view StringColumnHandle): usize
  fn stdDataStringColumnView(_ handle: view StringColumnHandle, _ index: usize): view String?
  fn stdDataStringColumnCopy(_ handle: view StringColumnHandle, _ index: usize): String?
  fn stdDataBytesColumnCount(_ handle: view BytesColumnHandle): usize
  fn stdDataBytesColumnView(_ handle: view BytesColumnHandle, _ index: usize): view Bytes?
  fn stdDataBytesColumnCopy(_ handle: view BytesColumnHandle, _ index: usize): Bytes?
  fn stdDataDynamicBatchSchema(_ handle: ref DynamicBatchHandle): SchemaHandle
  fn stdDataDynamicBatchRowCount(_ handle: ref DynamicBatchHandle): usize
  fn stdDataDynamicBatchColumn(_ handle: ref DynamicBatchHandle, _ name: ref String): DynamicColumnHandle
  fn stdDataDynamicColumnCount(_ handle: ref DynamicColumnHandle): usize
  fn stdDataDynamicColumnValue(_ handle: ref DynamicColumnHandle, _ index: usize): DynamicValue
  fn stdDataBind<Element: Row>(
    _ batch: take DynamicBatch,
    _ schema: ref Schema,
    _ policy: BindingPolicy,
    _ limits: ref Limits,
  ): Batch<Element> throws BindError
  fn stdDataSchemaFor<Element: Row>(): Schema
}

export struct SchemaIdentity: Copy & Equatable {
  let handle: SchemaIdentityHandle

  init(validatedHandle: SchemaIdentityHandle) {
    self.handle = validatedHandle
  }
}

export struct SchemaField {
  let handle: SchemaFieldHandle

  init(validatedHandle: SchemaFieldHandle) {
    self.handle = validatedHandle
  }

  export static fn make(
    name: take String,
    logicalType: take LogicalType,
    nullable: Bool,
    limits: ref Limits = ref Limits.standard(),
  ): SchemaField throws SchemaError {
    let handle = unsafe {
      try stdDataSchemaField(
        take name,
        take logicalType,
        nullable,
        limits,
      )
    }
    return SchemaField(validatedHandle: handle)
  }
}

export struct Schema {
  let handle: SchemaHandle

  init(validatedHandle: SchemaHandle) {
    self.handle = validatedHandle
  }

  export static fn make(
    fields: take Array<SchemaField>,
    rowCount: usize? = .none,
    limits: ref Limits = ref Limits.standard(),
  ): Schema throws SchemaError {
    let handle = unsafe {
      try stdDataSchema(take fields, rowCount, limits)
    }
    return Schema(validatedHandle: handle)
  }

  export fn identity(): SchemaIdentity {
    return SchemaIdentity(
      validatedHandle: unsafe { stdDataSchemaIdentity(ref handle) },
    )
  }

  export fn fieldCount(): usize {
    return unsafe { stdDataSchemaFieldCount(ref handle) }
  }
}

// An extension value carries the validated logical type and extension
// identity together with bounded opaque storage.  Metadata alone is not a
// value and cannot be persisted as one.
export struct DynamicExtensionValue {
  let handle: DynamicExtensionValueHandle

  init(validatedHandle: DynamicExtensionValueHandle) {
    self.handle = validatedHandle
  }

  export static fn make(
    logicalType: take LogicalType,
    extension: take SemanticExtension,
    storage: take Bytes,
    limits: ref Limits = ref Limits.standard(),
  ): DynamicExtensionValue throws SchemaError {
    return DynamicExtensionValue(validatedHandle: unsafe {
      try stdDataDynamicExtensionValue(
        take logicalType,
        take extension,
        take storage,
        limits,
      )
    })
  }
}

// Nested dynamic storage is indirect for the same reason as LogicalType:
// lists/maps/nested rows cannot recursively contain themselves by value.
export struct DynamicListValue {
  let handle: DynamicListHandle

  init(validatedHandle: DynamicListHandle) {
    self.handle = validatedHandle
  }

  export static fn make(
    values: take Array<DynamicValue>,
    limits: ref Limits = ref Limits.standard(),
  ): DynamicListValue throws SchemaError {
    return DynamicListValue(validatedHandle: unsafe {
      try stdDataDynamicList(take values, limits)
    })
  }
}

export struct DynamicMapValue {
  let handle: DynamicMapHandle

  init(validatedHandle: DynamicMapHandle) {
    self.handle = validatedHandle
  }

  export static fn make(
    entries: take Array<DynamicEntry>,
    limits: ref Limits = ref Limits.standard(),
  ): DynamicMapValue throws SchemaError {
    return DynamicMapValue(validatedHandle: unsafe {
      try stdDataDynamicMap(take entries, limits)
    })
  }
}

export struct DynamicNestedValue {
  let handle: DynamicNestedHandle

  init(validatedHandle: DynamicNestedHandle) {
    self.handle = validatedHandle
  }

  export static fn make(
    entries: take Array<DynamicEntry>,
    limits: ref Limits = ref Limits.standard(),
  ): DynamicNestedValue throws SchemaError {
    return DynamicNestedValue(validatedHandle: unsafe {
      try stdDataDynamicNested(take entries, limits)
    })
  }
}

// Dynamic values may contain move-only extension storage and nested arrays;
// callers must move or explicitly copy through a provider policy.
export enum DynamicValue {
  null
  bool(Bool)
  signed(i128)
  unsigned(u128)
  decimal(DecimalValue)
  f16(f16)
  f32(f32)
  f64(f64)
  string(String)
  bytes(Bytes)
  uuid(UUIDValue)
  date(DateValue)
  time(TimeValue)
  instant(InstantValue)
  localDateTime(LocalDateTimeValue)
  list(DynamicListValue)
  map(DynamicMapValue)
  nested(DynamicNestedValue)
  extension(DynamicExtensionValue)
}

export struct DynamicEntry {
  let key: String
  let value: DynamicValue

  export const init(key: String, value: DynamicValue) {
    self.key = key
    self.value = value
  }
}

export struct FieldDescriptor<Owner: Row, Value> {
  let handle: FieldDescriptorHandle

  init(validatedHandle: FieldDescriptorHandle) {
    self.handle = validatedHandle
  }
}

export struct Column<Owner: Row, Value> {
  // A view handle has no ownership.  The Batch remains the sole payload
  // owner and the compiler rejects this loan escaping its scope.
  let handle: view ColumnHandle

  init(validatedHandle: view ColumnHandle) {
    self.handle = validatedHandle
  }

  export fn count(): usize {
    return unsafe { stdDataColumnCount(handle) }
  }

  // Copy values return an owner; the source spelling `column[index]` lowers to
  // this checked operation. There is no generic view operation.
  export fn copy(at index: usize): Value {
    return unsafe { stdDataColumnCopy<Value>(handle, index) }
  }
}

export struct StringColumn<Owner: Row> {
  let handle: view StringColumnHandle

  init(validatedHandle: view StringColumnHandle) {
    self.handle = validatedHandle
  }

  export fn count(): usize {
    return unsafe { stdDataStringColumnCount(handle) }
  }

  // The borrowed value is tied to this column's Batch owner and cannot be
  // returned from a scope that does not keep that owner alive.
  export fn view(at index: usize): view String? {
    return unsafe { stdDataStringColumnView(handle, index) }
  }

  export fn copy(at index: usize): String? {
    return unsafe { stdDataStringColumnCopy(handle, index) }
  }
}

export struct BytesColumn<Owner: Row> {
  let handle: view BytesColumnHandle

  init(validatedHandle: view BytesColumnHandle) {
    self.handle = validatedHandle
  }

  export fn count(): usize {
    return unsafe { stdDataBytesColumnCount(handle) }
  }

  export fn view(at index: usize): view Bytes? {
    return unsafe { stdDataBytesColumnView(handle, index) }
  }

  export fn copy(at index: usize): Bytes? {
    return unsafe { stdDataBytesColumnCopy(handle, index) }
  }
}

export struct DynamicColumn {
  let handle: DynamicColumnHandle

  init(validatedHandle: DynamicColumnHandle) {
    self.handle = validatedHandle
  }

  export fn count(): usize {
    return unsafe { stdDataDynamicColumnCount(ref handle) }
  }

  export fn value(at index: usize): DynamicValue {
    return unsafe { stdDataDynamicColumnValue(ref handle, index) }
  }
}

export struct Batch<Element: Row> {
  let handle: BatchHandle

  init(validatedHandle: BatchHandle) {
    self.handle = validatedHandle
  }

  export fn schema(): Schema {
    return Schema(validatedHandle: unsafe { stdDataBatchSchema(ref handle) })
  }

  export fn rowCount(): usize {
    return unsafe { stdDataBatchRowCount(ref handle) }
  }

  // Compiler-generated descriptors carry Element.  The provider rejects a
  // descriptor whose owner does not match this Batch's Element.
  export fn column<Value>(
    field: FieldDescriptor<Element, Value>,
  ): view Column<Element, Value> {
    return view Column(validatedHandle: unsafe {
      stdDataBatchColumn(ref handle, ref field.handle)
    })
  }

  export fn column(
    string field: FieldDescriptor<Element, String?>,
  ): view StringColumn<Element> {
    return view StringColumn(validatedHandle: unsafe {
      stdDataBatchStringColumn(ref handle, ref field.handle)
    })
  }

  export fn column(
    bytes field: FieldDescriptor<Element, Bytes?>,
  ): view BytesColumn<Element> {
    return view BytesColumn(validatedHandle: unsafe {
      stdDataBatchBytesColumn(ref handle, ref field.handle)
    })
  }
}

export struct DynamicBatch {
  let handle: DynamicBatchHandle

  init(validatedHandle: DynamicBatchHandle) {
    self.handle = validatedHandle
  }

  export fn schema(): Schema {
    return Schema(validatedHandle: unsafe {
      stdDataDynamicBatchSchema(ref handle)
    })
  }

  export fn rowCount(): usize {
    return unsafe { stdDataDynamicBatchRowCount(ref handle) }
  }

  export fn column(name: ref String): DynamicColumn {
    return DynamicColumn(validatedHandle: unsafe {
      stdDataDynamicBatchColumn(ref handle, name)
    })
  }
}

export enum BindError: Error {
  schemaMismatch
  missingField(name: String)
  extraField(name: String)
  typeMismatch(name: String)
  nullabilityMismatch(name: String)
  explicitMappingRequired
  limitExceeded(kind: LimitKind, maximum: u64)
}

export enum ProgressError: Error {
  overflow
}

export struct EncodeProgress: Copy & Equatable {
  let bytesCommitted: u64
  let completeRecords: u64
  let partialRecord: Bool

  export const init(
    bytesCommitted: u64,
    completeRecords: u64,
    partialRecord: Bool,
  ) {
    self.bytesCommitted = bytesCommitted
    self.completeRecords = completeRecords
    self.partialRecord = partialRecord
  }

  export fn checkedAdding(
    bytes: u64,
    records: u64,
    partialRecord: Bool,
  ): EncodeProgress throws ProgressError {
    return unsafe {
      try stdDataProgressAdding(
        self,
        bytes,
        records,
        partialRecord,
      )
    }
  }
}

foreign intrinsic from "std.data@1" {
  fn stdDataProgressAdding(
    _ current: EncodeProgress,
    _ bytes: u64,
    _ records: u64,
    _ partialRecord: Bool,
  ): EncodeProgress throws ProgressError
}

// Row binding validates every field, nullability, extension and limit before
// publishing an immutable Batch.  The returned opaque carrier is move-only.
export fn bind<Element: Row>(
  batch: take DynamicBatch,
  to schema: ref Schema,
  policy: BindingPolicy = .exact,
  limits: ref Limits,
): Batch<Element> throws BindError {
  return unsafe {
    try stdDataBind(
      take batch,
      schema,
      policy,
      limits,
    )
  }
}

export fn schema<Element: Row>(): Schema {
  return unsafe { stdDataSchemaFor<Element>() }
}
