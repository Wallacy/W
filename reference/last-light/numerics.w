// Numeric contracts for the Last Light restaurant.

import math from std
import { FixedDecimal } from std.decimal
import { Quantized, StaticRatio } from std.quant

export type SeatNumber = UInt<(1...128)>
export type TaxRate = FixedDecimal<i128, scale: 4>
export type FlavorQ =
  Quantized<
    i8,
    expressed: f32,
    scale: StaticRatio<1, 128>,
    zeroPoint: 0,
  >

export fn addPortions(left: u8, right: u16): u16 {
  return left + right
}

export fn checkedTrayCount(current: u8, added: u8): u8 throws ArithmeticError {
  return try u8.checkedAdd(current, added)
}

export fn narrowTrayCount(source: i64): u16 throws NumericConversionError {
  return try u16(exactly: source)
}

export fn truncateReading(source: f64): i32 throws NumericConversionError {
  return try i32(rounding: source, mode: .towardZero)
}

export fn compareReadings(left: f64, right: f64): Ordering? {
  return left.partialCompare(right)
}

export fn acceptsSeat(number: UInt): Bool {
  return number in 1...128
}

export fn strictHeatStep(
  gain: f64,
  error: f64,
  previous: f64,
): f64 {
  return math.fma(gain, error, previous)
}

export fn decimalDigitCount(value: u64): u8 {
  var remaining = value
  var digits: u8 = 0

  repeat {
    digits += 1
    remaining /= 10
  } while remaining > 0

  return digits
}

test "literal materialization keeps radix and exponent" {
  let permissions: u16 = 0o755
  let mask: u8 = 0b1111_0000
  let pigment: u32 = 0xff_40_00
  let stars: f64 = 6.022_140_76e23

  expect permissions == 493
  expect mask == 240
  expect pigment == 16_728_064
  expect stars > 6.0e23
}

// W-1253: radix is explicit and never changes canonical decimal Display.
test "integer radix parsing and formatting stay explicit" {
  expect try u16.parse("ff", radix: 16) == 255
  expect try i16.parse("-7f", radix: 16) == -127
  expect u16(255).format(radix: 16) == "ff"
  expect u16(255).format(radix: 16, uppercase: true) == "FF"
  expect u16(5).format(radix: 2) == "101"
  expect u16(255).display() == "255"
}

test "safe conversion is unique and value preserving" for addPortions {
  expect addPortions(250, right: 2) == 252
  expect try narrowTrayCount(65_535) == 65_535
  expect try truncateReading(-3.9) == -3
}

test "integer operators keep one policy in every profile" {
  expect -7 / 3 == -2
  expect -7 % 3 == -1
  expect i32.euclideanRemainder(-7, 3) == 2
  expect u8.wrappingAdd(u8.max, 1) == 0
  expect u8.saturatingAdd(u8.max, 1) == u8.max
}

test "power follows mathematical unary precedence" {
  expect -2.0 ** 2 == -4.0
  expect 2.0 ** -3 == 0.125
  expect 2 ** 3 ** 2 == 512
}

test "byte order is explicit at a boundary" {
  let wire = 0x0102_0304_u32.toBytes(order: .big)
  let restored = u32.fromBytes(wire, order: .big)

  expect wire == [0x01_u8, 0x02_u8, 0x03_u8, 0x04_u8]
  expect restored == 0x0102_0304_u32
}

test "float order is explicit when IEEE equality is partial" for compareReadings {
  expect f64.nan != f64.nan
  expect -0.0 == 0.0
  expect compareReadings(f64.nan, right: 1.0) == none
  expect f64.totalOrder(-0.0, 0.0) == .less
}

test "ranges are intervals and descending work uses stride" for acceptsSeat {
  expect acceptsSeat(1)
  expect acceptsSeat(128)
  expect !(3 in 5...1)

  var countdown: Array<Int> = []
  for value in stride(from: 5, through: 1, by: -1) {
    countdown.append(value)
  }
  expect countdown == [5, 4, 3, 2, 1]
}

test "decimal and quantized types keep their contracts" {
  let tax: TaxRate = 0.0825
  let signal: FlavorQ = try FlavorQ(exactly: 0.5_f32)

  expect tax == 0.0825
  expect signal.expressed == 0.5_f32
}

test "a post-test loop processes zero once" for decimalDigitCount {
  expect decimalDigitCount(0) == 1
  expect decimalDigitCount(9) == 1
  expect decimalDigitCount(42_424) == 5
}
