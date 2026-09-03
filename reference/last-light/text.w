// Text and binary boundaries for the Last Light restaurant.

import * from std.ffi
import * from std.fs
import * from std.text

export type MenuGlyph = String<(.graphemes.count == 1)>
export type CompactGlyph =
  String<(.graphemes.count == 1 && .bytes.count <= 32)>

export struct TextShape {
  let bytes: usize
  let scalars: usize
  let graphemes: usize
}

export fn textShape(value: ref String): TextShape {
  return TextShape(
    bytes: value.bytes.count,
    scalars: value.scalars.count,
    graphemes: value.graphemes.count,
  )
}

export fn scalarPrefix(value: ref String, count scalarCount: usize): view String {
  let start = value.scalars.start
  let end = value.scalars.index(start, offsetBy: scalarCount)
  return value.scalars[start..<end]
}

export fn decodeLabel(payload: ref Bytes): String throws Utf8Error {
  return try String.fromUtf8(payload)
}

export fn borrowLabel(payload: ref Bytes): view String throws Utf8Error {
  return try String.viewFromUtf8(payload)
}

export fn adoptLabel(payload: take Bytes): Utf8Adoption {
  return String.adoptingUtf8(take payload)
}

export fn decodeFragments(
  first: ref Bytes,
  second finalFragment: ref Bytes,
): String throws Utf8Error {
  var decoder = Utf8Decoder()
  try decoder.push(first)
  try decoder.push(finalFragment)
  return try (take decoder).finish()
}

export fn replaceScalarTail(
  value: inout String,
  offset scalarOffset: usize,
  replacement tail: ref String,
): () {
  let start = value.scalars.index(value.scalars.start, offsetBy: scalarOffset)
  let end = value.scalars.end
  value.replace(scalars: start..<end, with: tail)
}

export fn cLabel(value: ref String): CString throws CStringError {
  return try CString.from(value)
}

export fn utf8Path(value: ref Path): Utf8Path throws PathEncodingError {
  return try Utf8Path(value)
}

test "text units describe different facts" for textShape {
  let sign = "A🇧🇷e\u{301}"
  let shape = textShape(value: sign)

  expect shape.bytes == 12
  expect shape.scalars == 5
  expect shape.graphemes == 3
  expect scalarPrefix(value: sign, count: 3) == "A🇧🇷"
}

test "normalization and repair stay explicit" for decodeLabel {
  let composed = "é"
  let decomposed = "e\u{301}"

  expect composed != decomposed
  expect composed.normalized(.nfc) == decomposed.normalized(.nfc)

  do {
    let _ = try decodeLabel(payload: b"\x66\x6f\x80")
    panic("invalid UTF-8 was accepted")
  } catch error {
    expect error.offset == 2
    expect error.length == 1
    expect error.reason == .invalidLeadingByte
  }

  expect String.replacingInvalidUtf8(b"\xF0\x80\x80A") == "���A"
}

test "borrow, adoption, and chunks preserve ownership" {
  let payload = b"Violet Horizon"
  let label = try borrowLabel(payload: payload)
  expect label == "Violet Horizon"

  let adopted = adoptLabel(payload: take Bytes(copying: payload))
  switch adopted {
    case .text(let text): expect text == "Violet Horizon"
    case .invalid(_, _): panic("valid UTF-8 was rejected")
  }

  let joined = try decodeFragments(first: b"table \xF0\x9F", second: b"\xAA\x90")
  expect joined == "table 🪐"
}

test "an incomplete final chunk has one stable error" for decodeFragments {
  do {
    let _ = try decodeFragments(first: b"table ", second: b"\xE1\x80")
    panic("incomplete UTF-8 was accepted")
  } catch error {
    expect error.offset == 6
    expect error.length == 2
    expect error.reason == .unexpectedEnd
  }
}

test "core UTF-8 preserves an initial byte-order signature" for decodeLabel {
  expect try decodeLabel(payload: b"\xEF\xBB\xBFmenu") == "\u{FEFF}menu"
}

test "a grapheme refinement does not imply byte capacity" {
  let glyph: MenuGlyph = "🪐"
  let compact: CompactGlyph = "🇧🇷"

  expect glyph.graphemes.count == 1
  expect compact.bytes.count <= 32
}

test "terminal index use permits a safe edit" for replaceScalarTail {
  var title = "Last Light"
  replaceScalarTail(value: inout title, offset: 5, replacement: "Course")
  expect title == "Last Course"
}

test "byte slices validate both scalar boundaries" {
  let flag = "🇧🇷"

  do {
    let _ = try flag.view(bytes: 1..<flag.bytes.count)
    panic("an interior scalar boundary was accepted")
  } catch .startInsideScalar(let offset) {
    expect offset == 1
  }
}

test "raw and multiline strings keep exact content" {
  let raw = #"C:\last-light\${notInterpolation}"#
  let seconds: u64 = 30
  let envelope = #"{"value":#${seconds},"unit":"s"}"#
  let menu = """
    broth
      horizon-cake
    """

  expect raw == "C:\\last-light\\${notInterpolation}"
  expect envelope == #"{"value":30,"unit":"s"}"#
  expect menu == "broth\n  horizon-cake"
}

test "C strings reject an interior terminator" for cLabel {
  let valid = try cLabel(value: "last-light")
  expect valid.bytes.count == 10
  expect try valid.decodeUtf8() == "last-light"

  do {
    let _ = try cLabel(value: "closing\0bell")
    panic("interior NUL was accepted")
  } catch .interiorNul(let offset) {
    expect offset == 7
  }
}

test "a C string does not claim UTF-8 before decoding" {
  let foreignBytes = try CString.from(b"\xFF")
  expect foreignBytes.bytes.count == 1

  do {
    let _ = try foreignBytes.decodeUtf8()
    panic("invalid foreign UTF-8 was accepted")
  } catch error {
    expect error.offset == 0
    expect error.reason == .outOfRange
  }
}
