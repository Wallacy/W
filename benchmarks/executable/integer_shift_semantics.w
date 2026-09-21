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
// i8 -128/0/-64/64
// u8 128/0/64/64
// i16 -32768/0/-16384/16384
// u16 32768/0/16384/16384
// i32 -2147483648/0/-1073741824/1073741824
// u32 2147483648/0/1073741824/1073741824
// i64 -9223372036854775808/0/-4611686018427387904/4611686018427387904
// u64 9223372036854775808/0/4611686018427387904/4611686018427387904
// u64 small-value 2/64/64

entry {
  let checkedI8 = -64_i8
  print("i8 ${checkedI8 >> 2_u64}/${checkedI8 << 1_u64}")
  let checkedU8 = 128_u8
  print("u8 ${checkedU8 >> 2_u64}/${checkedU8 << 1_u64}")
  let checkedI16 = -16384_i16
  print("i16 ${checkedI16 >> 2_u64}/${checkedI16 << 1_u64}")
  let checkedU16 = 32768_u16
  print("u16 ${checkedU16 >> 2_u64}/${checkedU16 << 1_u64}")
  let checkedI32 = -1073741824_i32
  print("i32 ${checkedI32 >> 2_u64}/${checkedI32 << 1_u64}")
  let checkedU32 = 2147483648_u32
  print("u32 ${checkedU32 >> 2_u64}/${checkedU32 << 1_u64}")
  let checkedI64 = -4611686018427387904_i64
  print("i64 ${checkedI64 >> 2_u64}/${checkedI64 << 1_u64}")
  let checkedU64 = 9223372036854775808_u64
  print("u64 ${checkedU64 >> 2_u64}/${checkedU64 << 1_u64}")
  let checkedInt: Int = -4611686018427387904_i64
  print("Int ${checkedInt >> 2_u64}/${checkedInt << 1_u64}")
  let checkedUInt: UInt = 9223372036854775808_u64
  print("UInt ${checkedUInt >> 2_u64}/${checkedUInt << 1_u64}")

  let policyI8 = ~127_i8
  print("i8 ${i8.maskedShiftLeft(policyI8, 8_u64)}/${i8.maskedShiftLeft(policyI8, 9_u64)}/${i8.maskedShiftRight(policyI8, 9_u64)}/${i8.logicalShiftRight(policyI8, 1_u64)}")
  let policyU8 = 128_u8
  print("u8 ${u8.maskedShiftLeft(policyU8, 8_u64)}/${u8.maskedShiftLeft(policyU8, 9_u64)}/${u8.maskedShiftRight(policyU8, 9_u64)}/${u8.logicalShiftRight(policyU8, 1_u64)}")
  let policyI16 = ~32767_i16
  print("i16 ${i16.maskedShiftLeft(policyI16, 16_u64)}/${i16.maskedShiftLeft(policyI16, 17_u64)}/${i16.maskedShiftRight(policyI16, 17_u64)}/${i16.logicalShiftRight(policyI16, 1_u64)}")
  let policyU16 = 32768_u16
  print("u16 ${u16.maskedShiftLeft(policyU16, 16_u64)}/${u16.maskedShiftLeft(policyU16, 17_u64)}/${u16.maskedShiftRight(policyU16, 17_u64)}/${u16.logicalShiftRight(policyU16, 1_u64)}")
  let policyI32 = ~2147483647_i32
  print("i32 ${i32.maskedShiftLeft(policyI32, 32_u64)}/${i32.maskedShiftLeft(policyI32, 33_u64)}/${i32.maskedShiftRight(policyI32, 33_u64)}/${i32.logicalShiftRight(policyI32, 1_u64)}")
  let policyU32 = 2147483648_u32
  print("u32 ${u32.maskedShiftLeft(policyU32, 32_u64)}/${u32.maskedShiftLeft(policyU32, 33_u64)}/${u32.maskedShiftRight(policyU32, 33_u64)}/${u32.logicalShiftRight(policyU32, 1_u64)}")
  let policyI64 = ~9223372036854775807_i64
  print("i64 ${i64.maskedShiftLeft(policyI64, 64_u64)}/${i64.maskedShiftLeft(policyI64, 65_u64)}/${i64.maskedShiftRight(policyI64, 65_u64)}/${i64.logicalShiftRight(policyI64, 1_u64)}")
  let policyU64 = 9223372036854775808_u64
  print("u64 ${u64.maskedShiftLeft(policyU64, 64_u64)}/${u64.maskedShiftLeft(policyU64, 65_u64)}/${u64.maskedShiftRight(policyU64, 65_u64)}/${u64.logicalShiftRight(policyU64, 1_u64)}")
  print("u64 small-value ${u64.maskedShiftLeft(1_u64, 65_u64)}/${u64.maskedShiftRight(128_u64, 65_u64)}/${u64.logicalShiftRight(128_u64, 1_u64)}")
}
