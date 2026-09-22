// Expected exit: 0
// Expected stdout:
// i8/u8 -128/0
// i16/u16 32767/2
// i32/u32 -2/4294967295
// i64/u64 -9223372036854775808/0
// Int/UInt -9223372036854775808/18446744073709551615

entry {
  let signed8: i8 = i8.wrappingAdd(127_i8, 1_i8)
  let signed16: i16 = i16.wrappingSubtract(-32767_i16, 2_i16)
  let signed32: i32 = i32.wrappingMultiply(2147483647_i32, 2_i32)
  let signed64Min: i64 = i64.wrappingAdd(9223372036854775807_i64, 1_i64)
  let signed64: i64 = i64.wrappingNegate(signed64Min)
  let unsigned8: u8 = u8.wrappingPower(2_u8, 8_u64)
  let unsigned16: u16 = u16.wrappingShiftLeft(32769_u16, 1_u64)
  let unsigned32: u32 = u32.wrappingNegate(1_u32)
  let unsigned64: u64 = u64.wrappingAdd(18446744073709551615_u64, 1_u64)
  let signedAlias: Int = Int.wrappingPower(-2, 63_u64)
  let unsignedAlias: UInt = UInt.wrappingSubtract(0, 1)
  print("i8/u8 ${signed8}/${unsigned8}")
  print("i16/u16 ${signed16}/${unsigned16}")
  print("i32/u32 ${signed32}/${unsigned32}")
  print("i64/u64 ${signed64}/${unsigned64}")
  print("Int/UInt ${signedAlias}/${unsignedAlias}")
}
