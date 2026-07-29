// Text and binary boundaries for the Last Light restaurant.

import std.ffi
import std.fs
import std.text

export struct TextShape {
  bytes: usize
  scalars: usize
  graphemes: usize
}

export fn textShape(value: ref String): TextShape {
  return TextShape(
    bytes: value.bytes.count,
    scalars: value.scalars.count,
    graphemes: value.graphemes.count,
  )
}

export fn scalarPrefix(value: ref String, count: usize): StringView {
  let start = value.scalars.start
  let end = value.scalars.index(start, offsetBy: count)
  return value.scalars[start..<end]
}

export fn decodeLabel(payload: ref Bytes): String throws Utf8Error {
  return try String.fromUtf8(payload)
}

export fn cLabel(value: ref String): CString throws CStringError {
  return try CString.from(value)
}

export fn utf8Path(value: ref Path): Utf8Path throws PathEncodingError {
  return try Utf8Path(value)
}

test "text units describe different facts" for textShape {
  let sign = "A🇧🇷e\u{301}"
  let shape = textShape(sign)

  expect shape.bytes == 12
  expect shape.scalars == 5
  expect shape.graphemes == 3
  expect scalarPrefix(sign, count: 3) == "A🇧🇷"
}

test "normalization and repair stay explicit" for decodeLabel {
  let composed = "é"
  let decomposed = "e\u{301}"

  expect composed != decomposed
  expect composed.normalized(.nfc) == decomposed.normalized(.nfc)

  do {
    let _ = try decodeLabel(b"\x66\x6f\x80")
    panic("invalid UTF-8 was accepted")
  } catch .invalidByte(let offset) {
    expect offset == 2
  }
}

test "raw and multiline strings keep exact content" {
  let raw = #"C:\last-light\${notInterpolation}"#
  let menu = """
    broth
      horizon-cake
    """

  expect raw == "C:\\last-light\\${notInterpolation}"
  expect menu == "broth\n  horizon-cake"
}

test "C strings reject an interior terminator" for cLabel {
  do {
    let _ = try cLabel("closing\0bell")
    panic("interior NUL was accepted")
  } catch .interiorNul(let offset) {
    expect offset == 7
  }
}
