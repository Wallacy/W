// Expected exit: 0
// Expected stdout:
// i8 -7/-43
// i16 -7/-43
// i32 -7/-43
// i64 -7/-43
// Int -7/-43
// u8 170
// u16 65450
// u32 4294967210
// u64 18446744073709551530
// UInt 18446744073709551530
// literal -7

fn negate_i8(value: i8): i8 { return -value }
fn complement_i8(value: i8): i8 { return ~value }
fn negate_i16(value: i16): i16 { return -value }
fn complement_i16(value: i16): i16 { return ~value }
fn negate_i32(value: i32): i32 { return -value }
fn complement_i32(value: i32): i32 { return ~value }
fn negate_i64(value: i64): i64 { return -value }
fn complement_i64(value: i64): i64 { return ~value }
fn negate_int(value: Int): Int { return -value }
fn complement_int(value: Int): Int { return ~value }
fn complement_u8(value: u8): u8 { return ~value }
fn complement_u16(value: u16): u16 { return ~value }
fn complement_u32(value: u32): u32 { return ~value }
fn complement_u64(value: u64): u64 { return ~value }
fn complement_uint(value: UInt): UInt { return ~value }

entry {
  let neg_i8 = negate_i8(value: 7_i8)
  let not_i8 = complement_i8(value: 42_i8)
  let neg_i16 = negate_i16(value: 7_i16)
  let not_i16 = complement_i16(value: 42_i16)
  let neg_i32 = negate_i32(value: 7_i32)
  let not_i32 = complement_i32(value: 42_i32)
  let neg_i64 = negate_i64(value: 7_i64)
  let not_i64 = complement_i64(value: 42_i64)
  let neg_int = negate_int(value: 7_i64)
  let not_int = complement_int(value: 42_i64)
  let not_u8 = complement_u8(value: 85_u8)
  let not_u16 = complement_u16(value: 85_u16)
  let not_u32 = complement_u32(value: 85_u32)
  let not_u64 = complement_u64(value: 85_u64)
  let not_uint = complement_uint(value: 85_u64)
  print("i8 ${neg_i8}/${not_i8}")
  print("i16 ${neg_i16}/${not_i16}")
  print("i32 ${neg_i32}/${not_i32}")
  print("i64 ${neg_i64}/${not_i64}")
  print("Int ${neg_int}/${not_int}")
  print("u8 ${not_u8}")
  print("u16 ${not_u16}")
  print("u32 ${not_u32}")
  print("u64 ${not_u64}")
  print("UInt ${not_uint}")
  print("literal ${-7}")
}
