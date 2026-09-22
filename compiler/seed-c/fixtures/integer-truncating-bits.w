// Expected exit: 0
// Expected stdout:
// Trunc 2/-7/-6/18446744073709551609/-1

fn narrow(value: i16): i8 {
  return i8(truncatingBits: value)
}

fn widen(value: i8): i16 {
  return i16(truncatingBits: value)
}

fn reinterpret(value: u8): i8 {
  return i8(truncatingBits: value)
}

fn intToUInt(value: Int): UInt {
  return UInt(truncatingBits: value)
}

fn uintToInt(value: UInt): Int {
  return Int(truncatingBits: value)
}

entry {
  let narrowResult = narrow(value: 258_i16)
  let widenResult = widen(value: -7_i8)
  let reinterpretResult = reinterpret(value: 250_u8)
  let intToUIntResult = intToUInt(value: -7)
  let uintToIntResult = uintToInt(value: 18446744073709551615_u64)
  print("Trunc ${narrowResult}/${widenResult}/${reinterpretResult}/${intToUIntResult}/${uintToIntResult}")
}
