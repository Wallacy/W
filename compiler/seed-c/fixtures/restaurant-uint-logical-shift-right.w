fn shift(value: UInt, count: UInt): UInt {
  return u64.logicalShiftRight(value, count)
}

entry {
  let shifted = shift(value: 128_u64, count: 1_u64)
  print("Logical ${shifted}")
}
