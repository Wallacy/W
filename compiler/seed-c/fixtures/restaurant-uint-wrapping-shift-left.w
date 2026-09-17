fn shift(value: UInt, count: UInt): UInt {
  return u64.wrappingShiftLeft(value, count)
}

entry {
  let wrapped = shift(value: 18446744073709551615_u64, count: 1_u64)
  print("Wrapped ${wrapped}")
}
