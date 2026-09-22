// Expected exit: 0
// Expected stdout:
// i8 -16/-128
// u8 32/128
// i16 -4096/-32768
// u16 8192/32768
// i32 -268435456/-2147483648
// u32 536870912/2147483648
// i64 -1152921504606846976/-9223372036854775808
// u64 2305843009213693952/9223372036854775808
// Int -1152921504606846976/-9223372036854775808
// UInt 2305843009213693952/9223372036854775808

fn shift_i8(rightValue: i8, leftValue: i8) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("i8 ${right}/${left}")
}

fn shift_u8(rightValue: u8, leftValue: u8) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("u8 ${right}/${left}")
}

fn shift_i16(rightValue: i16, leftValue: i16) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("i16 ${right}/${left}")
}

fn shift_u16(rightValue: u16, leftValue: u16) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("u16 ${right}/${left}")
}

fn shift_i32(rightValue: i32, leftValue: i32) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("i32 ${right}/${left}")
}

fn shift_u32(rightValue: u32, leftValue: u32) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("u32 ${right}/${left}")
}

fn shift_i64(rightValue: i64, leftValue: i64) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("i64 ${right}/${left}")
}

fn shift_u64(rightValue: u64, leftValue: u64) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("u64 ${right}/${left}")
}

fn shift_int(rightValue: Int, leftValue: Int) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("Int ${right}/${left}")
}

fn shift_uint(rightValue: UInt, leftValue: UInt) {
  let right = rightValue >> 2_u64
  let left = leftValue << 1_u64
  print("UInt ${right}/${left}")
}

entry {
  shift_i8(rightValue: -64_i8, leftValue: -64_i8)
  shift_u8(rightValue: 128_u8, leftValue: 64_u8)
  shift_i16(rightValue: -16384_i16, leftValue: -16384_i16)
  shift_u16(rightValue: 32768_u16, leftValue: 16384_u16)
  shift_i32(rightValue: -1073741824_i32, leftValue: -1073741824_i32)
  shift_u32(rightValue: 2147483648_u32, leftValue: 1073741824_u32)
  shift_i64(
    rightValue: -4611686018427387904_i64,
    leftValue: -4611686018427387904_i64,
  )
  shift_u64(
    rightValue: 9223372036854775808_u64,
    leftValue: 4611686018427387904_u64,
  )
  shift_int(
    rightValue: -4611686018427387904_i64,
    leftValue: -4611686018427387904_i64,
  )
  shift_uint(
    rightValue: 9223372036854775808_u64,
    leftValue: 4611686018427387904_u64,
  )
}
