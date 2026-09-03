// Bounded JSON contracts for interoperability and HTTP bodies.
//
// JSON is a format-specific protocol. The intrinsic provider owns parsing,
// escaping, number range checks, and the opaque cursor handles. Safe W sees
// only typed values and scoped object/array cursors. There is no reflection,
// universal serializer, annotation, macro, or `Any` value in this module.

export enum Profile {
  interoperable
  rfc8259
}

export enum UnknownMemberPolicy {
  reject
  ignore
}

export enum LimitKind {
  bytes
  depth
  values
  stringBytes
  numberTokenBytes
  objectMembers
  allocationBytes
}

const fn smallerSize(left: usize, right: usize): usize {
  if left < right { return left }
  return right
}

export struct Limits: Copy & Equatable {
  storedMaximumBytes: usize<(1...)>
  storedMaximumDepth: usize<(1...)>
  storedMaximumValues: usize<(1...)>
  storedMaximumStringBytes: usize<(1...)>
  storedMaximumNumberTokenBytes: usize<(1...)>
  storedMaximumObjectMembers: usize<(1...)>
  storedMaximumAllocationBytes: usize<(1...)>

  export maximumBytes: usize<(1...)> { get => storedMaximumBytes }
  export maximumDepth: usize<(1...)> { get => storedMaximumDepth }
  export maximumValues: usize<(1...)> { get => storedMaximumValues }
  export maximumStringBytes: usize<(1...)> { get => storedMaximumStringBytes }
  export maximumNumberTokenBytes: usize<(1...)> {
    get => storedMaximumNumberTokenBytes
  }
  export maximumObjectMembers: usize<(1...)> { get => storedMaximumObjectMembers }
  export maximumAllocationBytes: usize<(1...)> {
    get => storedMaximumAllocationBytes
  }

  export const init(
    maximumBytes: usize<(1...)>,
    maximumDepth: usize<(1...)>,
    maximumValues: usize<(1...)>,
    maximumStringBytes: usize<(1...)>,
    maximumNumberTokenBytes: usize<(1...)>,
    maximumObjectMembers: usize<(1...)>,
    maximumAllocationBytes: usize<(1...)>,
  ) {
    self.storedMaximumBytes = maximumBytes
    self.storedMaximumDepth = maximumDepth
    self.storedMaximumValues = maximumValues
    self.storedMaximumStringBytes = maximumStringBytes
    self.storedMaximumNumberTokenBytes = maximumNumberTokenBytes
    self.storedMaximumObjectMembers = maximumObjectMembers
    self.storedMaximumAllocationBytes = maximumAllocationBytes
  }

  export const init(maximumBytes: usize<(1...)>) {
    self.storedMaximumBytes = maximumBytes
    self.storedMaximumDepth = 128
    self.storedMaximumValues = maximumBytes
    self.storedMaximumStringBytes = maximumBytes
    self.storedMaximumNumberTokenBytes = smallerSize(left: maximumBytes, right: 1024)
    self.storedMaximumObjectMembers = maximumBytes
    self.storedMaximumAllocationBytes = maximumBytes
  }
}

export enum EncodeError: Error {
  limitExceeded(kind: LimitKind, maximum: usize)
  integerOutOfRange(value: String, profile: Profile)
  nonFiniteFloat(profile: Profile)
}

export struct Location: Copy & Equatable {
  storedByteOffset: usize

  export const init(byteOffset: usize) {
    self.storedByteOffset = byteOffset
  }

  export byteOffset: usize { get => storedByteOffset }
}

export enum SyntaxKind: Copy & Equatable {
  document
  string
  number
  object
  array
  literal
  member
  end
}

// ValueKind classifies the JSON node found at a document location.  The set
// is closed so adapters can report a stable mismatch without exposing a
// runtime type name.
export enum ValueKind: Copy & Equatable {
  null
  boolean
  number
  string
  array
  object
}

// ValueConstraint identifies a semantic constraint that a typed decoder
// could not satisfy.  Domain validation after decode uses a domain error.
export enum ValueConstraint: Copy & Equatable {
  enumCase
  refinement
  canonicalForm
}

export enum ValueError: Error {
  invalidNumber
  duplicateMember(name: String)
  limitExceeded(kind: LimitKind, maximum: usize)
}

export enum DecodeError: Error {
  syntax(kind: SyntaxKind, location: Location)
  trailingBytes(location: Location)
  invalidUnicode(location: Location)
  invalidSurrogate(location: Location)
  duplicateMember(name: String, location: Location)
  missingMember(name: String, location: Location)
  unknownMember(name: String, location: Location)
  unsafeInteger(token: String, location: Location)
  invalidNumber(location: Location)
  limitExceeded(kind: LimitKind, maximum: usize, location: Location)
  typeMismatch(expected: ValueKind, found: ValueKind, location: Location)
  invalidValue(constraint: ValueConstraint, location: Location)
}

// Number is nominal and invariant-preserving. Construction validates and
// normalizes a bounded RFC token; callers cannot fabricate an unchecked case.
export struct Number: Duplicable & Equatable {
  token: String

  export init(token: String, limits: Limits) throws ValueError {
    self.token = unsafe {
      try stdJsonNumberNormalize(ref token, ref limits)
    }
  }

  init(validatedToken: String) {
    self.token = take validatedToken
  }

  export static fn parse(
    token: ref String,
    limits: ref Limits,
  ): Number throws ValueError {
    let normalized = unsafe {
      try stdJsonNumberNormalize(token, limits)
    }
    return Number(validatedToken: take normalized)
  }

  fn encode(to writer: inout Writer) throws EncodeError {
    try writer.writeNumber(ref self)
  }

  static fn decode(from reader: inout Reader): Number throws DecodeError {
    return try reader.readNumber()
  }

  export fn text(): view String {
    return token
  }

  export fn duplicate(): Number {
    return Number(validatedToken: token.materialize())
  }

  export fn equals(other: ref Number): Bool {
    return unsafe { stdJsonNumberEquals(self, other) }
  }
}

export struct ObjectEntry: Duplicable & Equatable {
  storedName: String
  storedValue: Value

  export init(name: String, value: take Value) {
    self.storedName = take name
    self.storedValue = take value
  }

  export name: view String {
    get => storedName
  }

  export value: ref Value {
    get => storedValue
  }

  export fn duplicate(): ObjectEntry {
    return ObjectEntry(name: storedName.materialize(), value: copy storedValue)
  }

  export fn equals(other: ref ObjectEntry): Bool {
    return storedName == other.storedName && storedValue == other.storedValue
  }
}

// Object preserves source/insertion order for iteration and re-encoding. Its
// equality is map-like: member order does not affect the result.
export struct Object: Duplicable & Equatable {
  entries: Array<ObjectEntry>

  export init(
    entries: take Array<ObjectEntry>,
    limits: Limits,
  ) throws ValueError {
    guard entries.count <= limits.maximumObjectMembers else {
      throw .limitExceeded(.objectMembers, limits.maximumObjectMembers)
    }

    self.entries = []
    for entry in take entries {
      guard !contains(entry.storedName) else {
        throw .duplicateMember(entry.storedName.materialize())
      }
      self.entries.append(take entry)
    }
  }

  init(validatedEntries: take Array<ObjectEntry>) {
    self.entries = take validatedEntries
  }

  export fn duplicate(): Object {
    var copied: Array<ObjectEntry> = []
    for ref entry in entries {
      copied.append(entry.duplicate())
    }
    return Object(validatedEntries: take copied)
  }

  export count: usize {
    get => entries.count
  }

  export fn entry(at index: usize): ref ObjectEntry? {
    return entries.get(index)
  }

  export fn value(for name: ref String): ref Value? {
    for ref entry in entries {
      if entry.storedName == name { return entry.storedValue }
    }
    return .none
  }

  export fn equals(other: ref Object): Bool {
    guard entries.count == other.entries.count else return false
    for ref left in entries {
      var found = false
      for ref right in other.entries {
        if left.storedName == right.storedName {
          guard left.storedValue == right.storedValue else return false
          found = true
          break
        }
      }
      guard found else return false
    }
    return true
  }

  fn contains(name: ref String): Bool {
    for entry in entries {
      if entry.storedName == name { return true }
    }
    return false
  }
}

// Value is the bounded, data-only JSON tree. It is not the route used by
// typed synthesis and has no erased `Any` case.
export enum Value: Duplicable & Equatable {
  null
  bool(Bool)
  number(Number)
  string(String)
  array(Array<Value>)
  object(Object)

  fn encode(to writer: inout Writer) throws EncodeError {
    switch self {
      case .null:
        try writer.writeNull()
      case .bool(let value):
        try writer.writeBool(value)
      case .number(let value):
        try writer.writeNumber(ref value)
      case .string(let value):
        try writer.writeString(ref value)
      case .array(let values):
        try writer.withArray((array) => {
          for ref value in values {
            try array.element(value: ref value)
          }
        })
      case .object(let value):
        try writer.withObject((object) => {
          for ref entry in value.entries {
            try object.field(entry.storedName, value: ref entry.storedValue)
          }
        })
    }
  }

  static fn decode(from reader: inout Reader): Value throws DecodeError {
    return try reader.readValue()
  }

  export fn duplicate(): Value {
    return switch self {
      case .null: .null
      case .bool(let value): .bool(value)
      case .number(let value): .number(copy value)
      case .string(let value): .string(value.materialize())
      case .array(let value): .array(value.map((item) => copy item))
      case .object(let value): .object(copy value)
    }
  }

  export fn equals(other: ref Value): Bool {
    return switch (self, other) {
      case (.null, .null): true
      case (.bool(let left), .bool(let right)): left == right
      case (.number(let left), .number(let right)): left == right
      case (.string(let left), .string(let right)): left == right
      case (.array(let left), .array(let right)): left == right
      case (.object(let left), .object(let right)): left == right
      case (_, _): false
    }
  }
}

export protocol Encodable {
  fn encode(to writer: inout Writer) throws EncodeError
}

export protocol Decodable {
  static fn decode(from reader: inout Reader): Self throws DecodeError
}

export protocol Codable: Encodable & Decodable {}

// These handles and cursor transitions are provider-private. The public
// wrappers below expose no handle, pointer, reflection metadata, or dynamic
// value. The provider validates UTF-8, duplicate names, depth, values,
// allocation, and profile rules before it publishes a result. Finish and
// drop commit a cursor to an inert state; drop is idempotent so the scoped
// wrapper may clean up after either success or failure without publishing
// partial JSON.
foreign intrinsic from "std.json@1" {
  type JsonWriterHandle
  type JsonReaderHandle
  type JsonObjectWriterHandle
  type JsonArrayWriterHandle
  type JsonObjectReaderHandle
  type JsonArrayReaderHandle

  fn stdJsonNumberNormalize(
    _ token: ref String,
    _ limits: ref Limits,
  ): String throws ValueError
  fn stdJsonNumberEquals(_ left: ref Number, _ right: ref Number): Bool

  fn stdJsonWriterCreate(
    _ limits: ref Limits,
    _ profile: Profile,
  ): JsonWriterHandle throws EncodeError
  fn stdJsonWriterFinish(_ handle: inout JsonWriterHandle): Bytes throws EncodeError
  fn stdJsonWriterDrop(_ handle: inout JsonWriterHandle)
  fn stdJsonWriterNull(_ handle: inout JsonWriterHandle) throws EncodeError
  fn stdJsonWriterBool(_ handle: inout JsonWriterHandle, _ value: Bool) throws EncodeError
  fn stdJsonWriterString(_ handle: inout JsonWriterHandle, _ value: ref String) throws EncodeError
  fn stdJsonWriterNumber(_ handle: inout JsonWriterHandle, _ value: ref Number) throws EncodeError
  fn stdJsonWriterObject(_ handle: inout JsonWriterHandle): JsonObjectWriterHandle throws EncodeError
  fn stdJsonWriterArray(_ handle: inout JsonWriterHandle): JsonArrayWriterHandle throws EncodeError
  // `name` is borrowed for this call; the provider emits it immediately and
  // never retains the caller's String storage.
  fn stdJsonObjectWriterField<Value: Encodable>(
    _ cursor: inout JsonObjectWriterHandle,
    _ name: ref String,
    _ value: ref Value,
  ) throws EncodeError
  fn stdJsonObjectWriterFinish(_ cursor: inout JsonObjectWriterHandle) throws EncodeError
  fn stdJsonObjectWriterDrop(_ cursor: inout JsonObjectWriterHandle)
  fn stdJsonArrayWriterElement<Value: Encodable>(
    _ cursor: inout JsonArrayWriterHandle,
    _ value: ref Value,
  ) throws EncodeError
  fn stdJsonArrayWriterFinish(_ cursor: inout JsonArrayWriterHandle) throws EncodeError
  fn stdJsonArrayWriterDrop(_ cursor: inout JsonArrayWriterHandle)

  fn stdJsonReaderCreate(
    _ bytes: ref Bytes,
    _ limits: ref Limits,
    _ profile: Profile,
    _ unknownMembers: UnknownMemberPolicy,
  ): JsonReaderHandle throws DecodeError
  fn stdJsonReaderFinish(_ handle: inout JsonReaderHandle) throws DecodeError
  fn stdJsonReaderDrop(_ handle: inout JsonReaderHandle)
  fn stdJsonReaderNull(_ handle: inout JsonReaderHandle) throws DecodeError
  fn stdJsonReaderBool(_ handle: inout JsonReaderHandle): Bool throws DecodeError
  fn stdJsonReaderString(_ handle: inout JsonReaderHandle): String throws DecodeError
  fn stdJsonReaderNumber(_ handle: inout JsonReaderHandle): Number throws DecodeError
  fn stdJsonReaderValue(_ handle: inout JsonReaderHandle): Value throws DecodeError
  fn stdJsonReaderObject(_ handle: inout JsonReaderHandle): JsonObjectReaderHandle throws DecodeError
  fn stdJsonReaderArray(_ handle: inout JsonReaderHandle): JsonArrayReaderHandle throws DecodeError
  fn stdJsonObjectReaderRequired<Value: Decodable>(
    _ cursor: inout JsonObjectReaderHandle,
    _ name: ref String,
  ): Value throws DecodeError
  fn stdJsonObjectReaderOptional<Value: Decodable>(
    _ cursor: inout JsonObjectReaderHandle,
    _ name: ref String,
  ): Value? throws DecodeError
  fn stdJsonObjectReaderFinish(_ cursor: inout JsonObjectReaderHandle) throws DecodeError
  fn stdJsonObjectReaderDrop(_ cursor: inout JsonObjectReaderHandle)
  fn stdJsonArrayReaderNext<Value: Decodable>(
    _ cursor: inout JsonArrayReaderHandle,
  ): Value? throws DecodeError
  fn stdJsonArrayReaderFinish(_ cursor: inout JsonArrayReaderHandle) throws DecodeError
  fn stdJsonArrayReaderDrop(_ cursor: inout JsonArrayReaderHandle)
}

export struct Writer {
  handle: JsonWriterHandle

  init(limits: ref Limits, profile: Profile) throws EncodeError {
    self.handle = unsafe {
      try stdJsonWriterCreate(limits, profile)
    }
  }

  export mut fn writeNull(): () throws EncodeError {
    unsafe { try stdJsonWriterNull(inout handle) }
  }

  export mut fn writeBool(value: Bool): () throws EncodeError {
    unsafe { try stdJsonWriterBool(inout handle, value) }
  }

  export mut fn writeString(value: ref String): () throws EncodeError {
    unsafe { try stdJsonWriterString(inout handle, value) }
  }

  export mut fn writeNumber(value: ref Number): () throws EncodeError {
    unsafe { try stdJsonWriterNumber(inout handle, value) }
  }

  export mut fn withObject(
    body: some take fn(inout ObjectWriter): () throws EncodeError,
  ): () throws EncodeError {
    var raw = unsafe { try stdJsonWriterObject(inout handle) }
    var object = ObjectWriter(validatedHandle: take raw)
    do {
      try (take body)(inout object)
      try object.finish()
    } catch error {
      object.abort()
      throw error
    }
  }

  export mut fn withArray(
    body: some take fn(inout ArrayWriter): () throws EncodeError,
  ): () throws EncodeError {
    var raw = unsafe { try stdJsonWriterArray(inout handle) }
    var array = ArrayWriter(validatedHandle: take raw)
    do {
      try (take body)(inout array)
      try array.finish()
    } catch error {
      array.abort()
      throw error
    }
  }

  fn finish(): Bytes throws EncodeError {
    return unsafe { try stdJsonWriterFinish(inout handle) }
  }

  deinit {
    unsafe { stdJsonWriterDrop(inout handle) }
  }
}

export struct ObjectWriter {
  handle: JsonObjectWriterHandle

  init(validatedHandle: JsonObjectWriterHandle) {
    self.handle = validatedHandle
  }

  export mut fn field<Value: Encodable>(
    name: ref String,
    value: ref Value,
  ): () throws EncodeError {
    unsafe {
      try stdJsonObjectWriterField(
        inout handle,
        name,
        value,
      )
    }
  }

  fn finish(): () throws EncodeError {
    unsafe { try stdJsonObjectWriterFinish(inout handle) }
  }

  fn abort() {
    unsafe { stdJsonObjectWriterDrop(inout handle) }
  }

  deinit {
    unsafe { stdJsonObjectWriterDrop(inout handle) }
  }
}

export struct ArrayWriter {
  handle: JsonArrayWriterHandle

  init(validatedHandle: JsonArrayWriterHandle) {
    self.handle = validatedHandle
  }

  export mut fn element<Value: Encodable>(value: ref Value): () throws EncodeError {
    unsafe {
      try stdJsonArrayWriterElement(inout handle, value)
    }
  }

  fn finish(): () throws EncodeError {
    unsafe { try stdJsonArrayWriterFinish(inout handle) }
  }

  fn abort() {
    unsafe { stdJsonArrayWriterDrop(inout handle) }
  }

  deinit {
    unsafe { stdJsonArrayWriterDrop(inout handle) }
  }
}

export struct Reader {
  handle: JsonReaderHandle

  init(
    bytes: ref Bytes,
    limits: ref Limits,
    profile: Profile,
    unknownMembers: UnknownMemberPolicy,
  ) throws DecodeError {
    self.handle = unsafe {
      try stdJsonReaderCreate(
        bytes,
        limits,
        profile,
        unknownMembers,
      )
    }
  }

  export mut fn readNull(): () throws DecodeError {
    unsafe { try stdJsonReaderNull(inout handle) }
  }

  export mut fn readBool(): Bool throws DecodeError {
    return unsafe { try stdJsonReaderBool(inout handle) }
  }

  export mut fn readString(): String throws DecodeError {
    return unsafe { try stdJsonReaderString(inout handle) }
  }

  export mut fn readNumber(): Number throws DecodeError {
    return unsafe { try stdJsonReaderNumber(inout handle) }
  }

  fn readValue(): Value throws DecodeError {
    return unsafe { try stdJsonReaderValue(inout handle) }
  }

  export mut fn withObject<Output>(
    body: some take fn(inout ObjectReader): Output throws DecodeError,
  ): Output throws DecodeError {
    var raw = unsafe { try stdJsonReaderObject(inout handle) }
    var object = ObjectReader(validatedHandle: take raw)
    do {
      let output = try (take body)(inout object)
      try object.finish()
      return output
    } catch error {
      object.abort()
      throw error
    }
  }

  export mut fn withArray<Output>(
    body: some take fn(inout ArrayReader): Output throws DecodeError,
  ): Output throws DecodeError {
    var raw = unsafe { try stdJsonReaderArray(inout handle) }
    var array = ArrayReader(validatedHandle: take raw)
    do {
      let output = try (take body)(inout array)
      try array.finish()
      return output
    } catch error {
      array.abort()
      throw error
    }
  }

  fn finish(): () throws DecodeError {
    unsafe { try stdJsonReaderFinish(inout handle) }
  }

  deinit {
    unsafe { stdJsonReaderDrop(inout handle) }
  }
}

export struct ObjectReader {
  handle: JsonObjectReaderHandle

  init(validatedHandle: JsonObjectReaderHandle) {
    self.handle = validatedHandle
  }

  export mut fn required<Value: Decodable>(name: ref String): Value throws DecodeError {
    return unsafe {
      try stdJsonObjectReaderRequired(inout handle, name)
    }
  }

  export mut fn optional<Value: Decodable>(name: ref String): Value? throws DecodeError {
    return unsafe {
      try stdJsonObjectReaderOptional(inout handle, name)
    }
  }

  fn finish(): () throws DecodeError {
    unsafe { try stdJsonObjectReaderFinish(inout handle) }
  }

  fn abort() {
    unsafe { stdJsonObjectReaderDrop(inout handle) }
  }

  deinit {
    unsafe { stdJsonObjectReaderDrop(inout handle) }
  }
}

export struct ArrayReader {
  handle: JsonArrayReaderHandle

  init(validatedHandle: JsonArrayReaderHandle) {
    self.handle = validatedHandle
  }

  export mut fn next<Value: Decodable>(): Value? throws DecodeError {
    return unsafe { try stdJsonArrayReaderNext(inout handle) }
  }

  fn finish(): () throws DecodeError {
    unsafe { try stdJsonArrayReaderFinish(inout handle) }
  }

  fn abort() {
    unsafe { stdJsonArrayReaderDrop(inout handle) }
  }

  deinit {
    unsafe { stdJsonArrayReaderDrop(inout handle) }
  }
}

export fn encode<Value: Encodable>(
  value: ref Value,
  limits: Limits,
  profile: Profile = .interoperable,
): Bytes throws EncodeError {
  var writer = try Writer(limits: limits, profile: profile)
  try value.encode(to: inout writer)
  return try writer.finish()
}

export fn decode<Decoded: Decodable>(
  bytes: ref Bytes,
  limits: Limits,
  profile: Profile = .interoperable,
  unknownMembers: UnknownMemberPolicy = .reject,
): Decoded throws DecodeError {
  var reader = try Reader(
    bytes: bytes,
    limits: limits,
    profile: profile,
    unknownMembers: unknownMembers,
  )
  let value = try Decoded.decode(from: inout reader)
  try reader.finish()
  return value
}

// Explicit witnesses for the closed built-in set. The provider supplies the
// primitive lowering, plus compiler-recognized witnesses for fixed arrays.
// Generic arrays, fixed arrays, Option, and Map<String, V> recurse through
// these same witnesses. Tuples are intentionally excluded as an ambiguous shape.
extension<T: Codable> Option<T>: Codable {}
extension Bool: Codable {}
extension String: Codable {}
extension i8: Codable {}
extension i16: Codable {}
extension i32: Codable {}
extension i64: Codable {}
extension i128: Codable {}
extension u8: Codable {}
extension u16: Codable {}
extension u32: Codable {}
extension u64: Codable {}
extension u128: Codable {}
// `Int` and `UInt` are aliases with the i64/u64 identity and inherit those
// witnesses. Tuple shapes, Set, Result, and non-String Map keys stay manual.
extension f32: Codable {}
extension f64: Codable {}
extension Number: Codable {}
extension Value: Codable {}

extension<T: Codable> Array<T>: Codable {}
extension<V: Codable> Map<String, V>: Codable {}
