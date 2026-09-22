// Expected exit: 0
// Expected stdout:
// i8 10/-81/-91
// u8 10/175/165
// i16 2570/-20561/-23131
// u16 2570/44975/42405
// i32 168430090/-1347440721/-1515870811
// u32 168430090/2947526575/2779096485
// i64 723401728380766730/-5787213827046133841/-6510615555426900571
// u64 723401728380766730/12659530246663417775/11936128518282651045
// Int 723401728380766730/-5787213827046133841/-6510615555426900571
// UInt 723401728380766730/12659530246663417775/11936128518282651045
// Widened -13
// Mixed 255

fn bitwise_i8(left: i8, right: i8) {
  print("i8 ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_u8(left: u8, right: u8) {
  print("u8 ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_i16(left: i16, right: i16) {
  print("i16 ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_u16(left: u16, right: u16) {
  print("u16 ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_i32(left: i32, right: i32) {
  print("i32 ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_u32(left: u32, right: u32) {
  print("u32 ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_i64(left: i64, right: i64) {
  print("i64 ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_u64(left: u64, right: u64) {
  print("u64 ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_int(left: Int, right: Int) {
  print("Int ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_uint(left: UInt, right: UInt) {
  print("UInt ${left & right}/${left | right}/${left ^ right}")
}

fn bitwise_widened(left: i8, right: i32): i32 { return left | right }

fn bitwise_mixed(left: u8, right: i16): i16 { return left | right }

entry {
  bitwise_i8(left: -86_i8, right: 15_i8)
  bitwise_u8(left: 170_u8, right: 15_u8)
  bitwise_i16(left: -21846_i16, right: 3855_i16)
  bitwise_u16(left: 43690_u16, right: 3855_u16)
  bitwise_i32(left: -1431655766_i32, right: 252645135_i32)
  bitwise_u32(left: 2863311530_u32, right: 252645135_u32)
  bitwise_i64(left: -6148914691236517206_i64, right: 1085102592571150095_i64)
  bitwise_u64(left: 12297829382473034410_u64, right: 1085102592571150095_u64)
  bitwise_int(left: -6148914691236517206_i64, right: 1085102592571150095_i64)
  bitwise_uint(left: 12297829382473034410_u64, right: 1085102592571150095_u64)
  let widened_result = bitwise_widened(left: -16_i8, right: 3_i32)
  print("Widened ${widened_result}")
  let mixed_result = bitwise_mixed(left: 240_u8, right: 15_i16)
  print("Mixed ${mixed_result}")
}
