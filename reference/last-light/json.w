// JSON SDK0 oracle. The provider is still missing, so these cases document
// expected conformance and failure outcomes. They do not claim execution.

import json from std.json

struct Ticket: json.Codable {
  id: u64
  title: String
  garnish: String?
}

struct Escaped: json.Codable {
  text: String
}

enum CourseTag: json.Codable {
  broth
  cake
}

type TinyCount = u16<(1...3)>

struct Payload: json.Codable {
  orderId: u64
  amount: i32
}

struct Envelope: json.Codable {
  payload: Payload
  comment: String?

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    try writer.withObject((object) => {
      try object.field("payload_value", value: ref payload)
      try object.field("comment", value: ref comment)
    })
  }

  static fn decode(from reader: inout json.Reader): Envelope throws json.DecodeError {
    return try reader.withObject((object) => {
      let payload: Payload = try object.required("payload_value")
      let comment: String? = try object.optional("comment")
      return Envelope(payload: payload, comment: comment)
    })
  }
}

fn limits(maximumBytes: usize<(1...)>): json.Limits {
  return json.Limits(maximumBytes: maximumBytes)
}

fn decodeTicket(
  source: ref Bytes,
  unknownMembers: json.UnknownMemberPolicy = .reject,
): Ticket throws json.DecodeError {
  return try json.decode<Ticket>(
    ref source,
    limits: limits(4<KiB>),
    unknownMembers: unknownMembers,
  )
}

test "explicit conformance synthesizes a struct object" {
  let ticket = Ticket(id: 42, title: "horizon cake", garnish: .none)
  let bytes = try json.encode(ref ticket, limits: limits(4<KiB>))
  let restored = try json.decode<Ticket>(ref bytes, limits: limits(4<KiB>))

  expect restored.id == 42
  expect restored.title == "horizon cake"
  expect restored.garnish == .none
}

test "payloadless enum is a closed JSON witness" {
  let course = CourseTag.cake
  let bytes = try json.encode(ref course, limits: limits(1<KiB>))
  let restored = try json.decode<CourseTag>(ref bytes, limits: limits(1<KiB>))
  expect restored == .cake
}

test "manual witness can rename and encapsulate a field" {
  let source = Envelope(
    payload: Payload(orderId: 7, amount: 4242),
    comment: .some("late window"),
  )
  let bytes = try json.encode(ref source, limits: limits(4<KiB>))
  let restored = try json.decode<Envelope>(ref bytes, limits: limits(4<KiB>))

  expect restored.payload.orderId == 7
  expect restored.comment == .some("late window")
}

test "round trip preserves declaration order and optional null" {
  var source = Ticket(id: 9, title: "nebula broth", garnish: .some("salt"))
  let bytes = try json.encode(ref source, limits: limits(4<KiB>))
  let restored = try json.decode<Ticket>(ref bytes, limits: limits(4<KiB>))

  expect bytes == b"{\"id\":9,\"title\":\"nebula broth\",\"garnish\":\"salt\"}"
  expect restored.id == source.id
  expect restored.title == source.title
  expect restored.garnish == source.garnish
}

test "struct decode accepts members in any order and escaping stays stable" {
  var reversed: Bytes = b"{\"garnish\":null,\"title\":\"nebula\",\"id\":9}"
  let restored = try decodeTicket(ref reversed)
  expect restored.id == 9
  expect restored.garnish == .none

  let escaped = Escaped(text: "line\nquote\"")
  let bytes = try json.encode(ref escaped, limits: limits(1<KiB>))
  expect bytes == b"{\"text\":\"line\\nquote\\\"\"}"
}

test "escape vectors cover C0, slash, and direct Unicode" {
  // Provider-gated vector: it fixes escapes without claiming JCS identity.
  let escaped = Escaped(text: "\"\\/\b\t\n\f\r\u{0001}é😀")
  let bytes = try json.encode(ref escaped, limits: limits(1<KiB>))
  expect bytes == b"{\"text\":\"\\\"\\\\/\\b\\t\\n\\f\\r\\u0001\xC3\xA9\xF0\x9F\x98\x80\"}"
}

test "float vectors keep signed zero and exponent round-trip" {
  // Provider-gated vectors: these do not claim canonical JSON or JCS.
  var negativeZero = -0.0_f64
  var exponent = 1.25e-7_f64
  let zeroBytes = try json.encode(ref negativeZero, limits: limits(1<KiB>))
  let exponentBytes = try json.encode(ref exponent, limits: limits(1<KiB>))
  expect zeroBytes == b"-0"
  expect exponentBytes == b"1.25e-7"
}

test "Map<String, V> preserves insertion order" {
  var labels: Map<String, u64> = Map()
  labels["first"] = 1
  labels["second"] = 2

  let bytes = try json.encode(ref labels, limits: limits(1<KiB>))
  expect bytes == b"{\"first\":1,\"second\":2}"
}

test "Object equality ignores order but encoding preserves each order" {
  let firstEntries: Array<json.ObjectEntry> = [
    json.ObjectEntry(name: "a", value: .string("one")),
    json.ObjectEntry(name: "b", value: .string("two")),
  ]
  let secondEntries: Array<json.ObjectEntry> = [
    json.ObjectEntry(name: "b", value: .string("two")),
    json.ObjectEntry(name: "a", value: .string("one")),
  ]
  let first = try json.Object(
    take firstEntries,
    limits: limits(1<KiB>),
  )
  let second = try json.Object(
    take secondEntries,
    limits: limits(1<KiB>),
  )

  expect first == second

  var firstValue = json.Value.object(copy first)
  var secondValue = json.Value.object(copy second)
  let firstBytes = try json.encode(ref firstValue, limits: limits(1<KiB>))
  let secondBytes = try json.encode(ref secondValue, limits: limits(1<KiB>))
  expect firstBytes == b"{\"a\":\"one\",\"b\":\"two\"}"
  expect secondBytes == b"{\"b\":\"two\",\"a\":\"one\"}"
}

test "duplicate object names always fail" {
  var input: Bytes = b"{\"id\":1,\"id\":2,\"title\":\"duplicate\"}"
  do {
    let _ = try decodeTicket(ref input)
    panic("duplicate JSON member was accepted")
  } catch .duplicateMember(let name, let location) {
    expect name == "id"
    expect location.byteOffset == 8
  }
}

test "syntax diagnostics identify the offending byte" {
  var input: Bytes = b"{\"id\":1,}"
  do {
    let _ = try decodeTicket(ref input)
    panic("trailing comma was accepted")
  } catch .syntax(let kind, let location) {
    expect kind == .object
    expect location.byteOffset == 7
  }
}

test "unknown members follow the call policy" {
  var input: Bytes = b"{\"id\":1,\"title\":\"known\",\"extra\":true}"

  do {
    let _ = try decodeTicket(ref input, unknownMembers: .reject)
    panic("unknown JSON member was accepted by reject policy")
  } catch .unknownMember(let name, _) {
    expect name == "extra"
  }

  let ignored = try decodeTicket(ref input, unknownMembers: .ignore)
  expect ignored.id == 1
}

test "missing required field fails" {
  var input: Bytes = b"{\"title\":\"missing id\"}"
  do {
    let _ = try decodeTicket(ref input)
    panic("missing required field was accepted")
  } catch .missingMember(let name, _) {
    expect name == "id"
  }
}

test "decode reports JSON kind, enum, and refinement failures" {
  // Provider-gated vectors.  They do not claim execution without std.json@1.
  var object: Bytes = b"{}"
  do {
    let _ = try json.decode<u64>(ref object, limits: limits(1<KiB>))
    panic("object was accepted as an integer")
  } catch .typeMismatch(.number, .object, let location) {
    expect location.byteOffset == 0
  }

  var unknownCase: Bytes = b"\"unknown\""
  do {
    let _ = try json.decode<CourseTag>(ref unknownCase, limits: limits(1<KiB>))
    panic("unknown enum case was accepted")
  } catch .invalidValue(.enumCase, let location) {
    expect location.byteOffset == 0
  }

  var outOfRefinement: Bytes = b"4"
  do {
    let _ = try json.decode<TinyCount>(ref outOfRefinement, limits: limits(1<KiB>))
    panic("refinement failure was accepted")
  } catch .invalidValue(.refinement, let location) {
    expect location.byteOffset == 0
  }
}

test "Option accepts missing and explicit null as none" {
  var missing: Bytes = b"{\"id\":1,\"title\":\"missing option\"}"
  var explicitNull: Bytes = b"{\"id\":1,\"title\":\"null option\",\"garnish\":null}"

  expect (try decodeTicket(ref missing)).garnish == .none
  expect (try decodeTicket(ref explicitNull)).garnish == .none
}

test "interoperable profile rejects an unsafe integer" {
  var input: Bytes = b"9007199254740992"
  do {
    let _ = try json.decode<i64>(ref input, limits: limits(1<KiB>))
    panic("interoperable JSON accepted an unsafe integer")
  } catch .unsafeInteger(_, _) {
    // Expected I-JSON range failure.
  }
}

test "interoperable encoding reports an unsafe integer precisely" {
  var unsafeInteger: i64 = 9_007_199_254_740_992
  do {
    let _ = try json.encode(ref unsafeInteger, limits: limits(1<KiB>))
    panic("interoperable JSON encoded an unsafe integer")
  } catch .integerOutOfRange(let value, .interoperable) {
    expect value == "9007199254740992"
  }
}

test "rfc8259 profile preserves an exact integer token" {
  var input: Bytes = b"9007199254740993"
  let value = try json.decode<json.Number>(
    ref input,
    limits: limits(1<KiB>),
    profile: .rfc8259,
  )

  expect value.text() == "9007199254740993"
}

test "nonfinite floats are rejected" {
  var nan = f64.nan
  do {
    let _ = try json.encode(ref nan, limits: limits(1<KiB>))
    panic("JSON encoded NaN")
  } catch .nonFiniteFloat(.interoperable) {
    // JSON has no NaN or Infinity value.
  }
}

test "invalid surrogate escape is rejected" {
  var input: Bytes = b"\"\\ud800\""
  do {
    let _ = try json.decode<String>(ref input, limits: limits(1<KiB>))
    panic("JSON accepted a lone surrogate")
  } catch .invalidSurrogate(let location) {
    // W String contains Unicode scalar values only.
    expect location.byteOffset == 1
  }
}

test "input, depth, value, and allocation limits are independent" {
  var tooManyBytes: Bytes = b"\"123456789\""
  do {
    let _ = try json.decode<String>(ref tooManyBytes, limits: limits(4))
    panic("input byte limit was ignored")
  } catch .limitExceeded(.bytes, _, let location) {
    expect location.byteOffset == 4
  }

  var tooDeep: Bytes = b"[[[[0]]]]"
  do {
    let _ = try json.decode<json.Value>(
      ref tooDeep,
      limits: json.Limits(
        maximumBytes: 1<KiB>,
        maximumDepth: 2,
        maximumValues: 32,
        maximumStringBytes: 1<KiB>,
        maximumNumberTokenBytes: 32,
        maximumObjectMembers: 16,
        maximumAllocationBytes: 1<KiB>,
      ),
    )
    panic("depth limit was ignored")
  } catch .limitExceeded(.depth, _, _) {}

  var tooManyValues: Bytes = b"[1,2,3,4]"
  do {
    let _ = try json.decode<json.Value>(
      ref tooManyValues,
      limits: json.Limits(
        maximumBytes: 1<KiB>,
        maximumDepth: 8,
        maximumValues: 2,
        maximumStringBytes: 1<KiB>,
        maximumNumberTokenBytes: 32,
        maximumObjectMembers: 16,
        maximumAllocationBytes: 1<KiB>,
      ),
    )
    panic("value limit was ignored")
  } catch .limitExceeded(.values, _, _) {}

  var tooMuchAllocation: Bytes = b"{\"long\":\"0123456789\"}"
  do {
    let _ = try json.decode<json.Value>(
      ref tooMuchAllocation,
      limits: json.Limits(
        maximumBytes: 1<KiB>,
        maximumDepth: 8,
        maximumValues: 16,
        maximumStringBytes: 1<KiB>,
        maximumNumberTokenBytes: 32,
        maximumObjectMembers: 16,
        maximumAllocationBytes: 4,
      ),
    )
    panic("allocation limit was ignored")
  } catch .limitExceeded(.allocationBytes, _, _) {}
}

test "string, number-token, and object-member limits remain independent" {
  var tooLongString: Bytes = b"\"123456789\""
  do {
    let _ = try json.decode<String>(
      ref tooLongString,
      limits: json.Limits(
        maximumBytes: 1<KiB>,
        maximumDepth: 8,
        maximumValues: 16,
        maximumStringBytes: 4,
        maximumNumberTokenBytes: 32,
        maximumObjectMembers: 16,
        maximumAllocationBytes: 1<KiB>,
      ),
    )
    panic("string byte limit was ignored")
  } catch .limitExceeded(.stringBytes, _, _) {}

  var tooLongNumber: Bytes = b"123456789"
  do {
    let _ = try json.decode<json.Number>(
      ref tooLongNumber,
      limits: json.Limits(
        maximumBytes: 1<KiB>,
        maximumDepth: 8,
        maximumValues: 16,
        maximumStringBytes: 1<KiB>,
        maximumNumberTokenBytes: 4,
        maximumObjectMembers: 16,
        maximumAllocationBytes: 1<KiB>,
      ),
    )
    panic("number token limit was ignored")
  } catch .limitExceeded(.numberTokenBytes, _, _) {}

  var tooManyMembers: Bytes = b"{\"a\":1,\"b\":2}"
  do {
    let _ = try json.decode<json.Value>(
      ref tooManyMembers,
      limits: json.Limits(
        maximumBytes: 1<KiB>,
        maximumDepth: 8,
        maximumValues: 16,
        maximumStringBytes: 1<KiB>,
        maximumNumberTokenBytes: 32,
        maximumObjectMembers: 1,
        maximumAllocationBytes: 1<KiB>,
      ),
    )
    panic("object member limit was ignored")
  } catch .limitExceeded(.objectMembers, _, _) {}
}
