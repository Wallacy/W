// Expected exit: 0
// Expected stdout:
// i8 false/true/true/true/false/false
// u8 false/true/false/false/true/true
// i16 true/false/false/true/false/true
// u16 false/true/true/true/false/false
// i32 false/true/true/true/false/false
// u32 false/true/false/false/true/true
// i64 false/true/true/true/false/false
// u64 false/true/false/false/true/true
// Int false/true/true/true/false/false
// UInt true/false/false/true/false/true
// widen u8->i16 true

fn compareI8(left: i8, right: i8) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("i8 ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareU8(left: u8, right: u8) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("u8 ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareI16(left: i16, right: i16) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("i16 ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareU16(left: u16, right: u16) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("u16 ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareI32(left: i32, right: i32) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("i32 ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareU32(left: u32, right: u32) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("u32 ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareI64(left: i64, right: i64) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("i64 ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareU64(left: u64, right: u64) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("u64 ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareInt(left: Int, right: Int) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("Int ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareUInt(left: UInt, right: UInt) {
  let eq = left == right
  let ne = left != right
  let lt = left < right
  let le = left <= right
  let gt = left > right
  let ge = left >= right
  print("UInt ${eq}/${ne}/${lt}/${le}/${gt}/${ge}")
}

fn compareWidened(value: i16): Bool {
  return value > 199_i16
}

entry {
  compareI8(left: -1_i8, right: 1_i8)
  compareU8(left: 250_u8, right: 10_u8)
  compareI16(left: 77_i16, right: 77_i16)
  compareU16(left: 3_u16, right: 50000_u16)
  compareI32(left: -120000_i32, right: 120000_i32)
  compareU32(left: 3000000000_u32, right: 1_u32)
  compareI64(left: -9000000000_i64, right: 9000000000_i64)
  compareU64(left: 10_u64, right: 9_u64)
  compareInt(left: -7, right: 12)
  compareUInt(left: 7, right: 7)
  let widened = compareWidened(value: 200_u8)
  print("widen u8->i16 ${widened}")
}
