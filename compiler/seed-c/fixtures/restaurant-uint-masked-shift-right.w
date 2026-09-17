fn shift(value: UInt, count: UInt): UInt {
  return u64.maskedShiftRight(value, count)
}

entry {
  let masked = shift(value: 128_u64, count: 65_u64)
  print("Masked ${masked}")
}
