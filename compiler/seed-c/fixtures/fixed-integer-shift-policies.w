// Expected exit: 0
// Expected stdout:
// i8 -128/0/-64/64
// u8 128/0/64/64
// i16 -32768/0/-16384/16384
// u16 32768/0/16384/16384
// i32 -2147483648/0/-1073741824/1073741824
// u32 2147483648/0/1073741824/1073741824
// i64 -9223372036854775808/0/-4611686018427387904/4611686018427387904
// u64 9223372036854775808/0/4611686018427387904/4611686018427387904
// u64 small-value 2/64/64

fn row8() {
  let i8_value = ~127_i8
  let left_identity = i8.maskedShiftLeft(i8_value, 8_u64)
  let left_wrapped = i8.maskedShiftLeft(i8_value, 9_u64)
  let right_wrapped = i8.maskedShiftRight(i8_value, 9_u64)
  let logical = i8.logicalShiftRight(i8_value, 1_u64)
  print("i8 ${left_identity}/${left_wrapped}/${right_wrapped}/${logical}")

  let u8_value = 128_u8
  let uleft_identity = u8.maskedShiftLeft(u8_value, 8_u64)
  let uleft_wrapped = u8.maskedShiftLeft(u8_value, 9_u64)
  let uright_wrapped = u8.maskedShiftRight(u8_value, 9_u64)
  let ulogical = u8.logicalShiftRight(u8_value, 1_u64)
  print("u8 ${uleft_identity}/${uleft_wrapped}/${uright_wrapped}/${ulogical}")
}

fn row16() {
  let i16_value = ~32767_i16
  let left_identity = i16.maskedShiftLeft(i16_value, 16_u64)
  let left_wrapped = i16.maskedShiftLeft(i16_value, 17_u64)
  let right_wrapped = i16.maskedShiftRight(i16_value, 17_u64)
  let logical = i16.logicalShiftRight(i16_value, 1_u64)
  print("i16 ${left_identity}/${left_wrapped}/${right_wrapped}/${logical}")

  let u16_value = 32768_u16
  let uleft_identity = u16.maskedShiftLeft(u16_value, 16_u64)
  let uleft_wrapped = u16.maskedShiftLeft(u16_value, 17_u64)
  let uright_wrapped = u16.maskedShiftRight(u16_value, 17_u64)
  let ulogical = u16.logicalShiftRight(u16_value, 1_u64)
  print("u16 ${uleft_identity}/${uleft_wrapped}/${uright_wrapped}/${ulogical}")
}

fn row32() {
  let i32_value = ~2147483647_i32
  let left_identity = i32.maskedShiftLeft(i32_value, 32_u64)
  let left_wrapped = i32.maskedShiftLeft(i32_value, 33_u64)
  let right_wrapped = i32.maskedShiftRight(i32_value, 33_u64)
  let logical = i32.logicalShiftRight(i32_value, 1_u64)
  print("i32 ${left_identity}/${left_wrapped}/${right_wrapped}/${logical}")

  let u32_value = 2147483648_u32
  let uleft_identity = u32.maskedShiftLeft(u32_value, 32_u64)
  let uleft_wrapped = u32.maskedShiftLeft(u32_value, 33_u64)
  let uright_wrapped = u32.maskedShiftRight(u32_value, 33_u64)
  let ulogical = u32.logicalShiftRight(u32_value, 1_u64)
  print("u32 ${uleft_identity}/${uleft_wrapped}/${uright_wrapped}/${ulogical}")
}

fn row64() {
  let left_identity = i64.maskedShiftLeft(~9223372036854775807_i64, 64_u64)
  let left_wrapped = i64.maskedShiftLeft(~9223372036854775807_i64, 65_u64)
  let right_wrapped = i64.maskedShiftRight(~9223372036854775807_i64, 65_u64)
  let logical = i64.logicalShiftRight(~9223372036854775807_i64, 1_u64)
  print("i64 ${left_identity}/${left_wrapped}/${right_wrapped}/${logical}")

  let u64_value = 9223372036854775808_u64
  let uleft_identity = u64.maskedShiftLeft(u64_value, 64_u64)
  let uleft_wrapped = u64.maskedShiftLeft(u64_value, 65_u64)
  let uright_wrapped = u64.maskedShiftRight(u64_value, 65_u64)
  let ulogical = u64.logicalShiftRight(u64_value, 1_u64)
  print("u64 ${uleft_identity}/${uleft_wrapped}/${uright_wrapped}/${ulogical}")

  let small_masked_left = u64.maskedShiftLeft(1_u64, 65_u64)
  let small_masked_right = u64.maskedShiftRight(128_u64, 65_u64)
  let small_logical = u64.logicalShiftRight(128_u64, 1_u64)
  print("u64 small-value ${small_masked_left}/${small_masked_right}/${small_logical}")
}

entry {
  row8()
  row16()
  row32()
  row64()
}
