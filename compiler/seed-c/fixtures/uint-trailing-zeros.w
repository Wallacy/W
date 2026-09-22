fn count(value: UInt): UInt {
  return u64.countTrailingZeros(value)
}

entry {
  let trailing = count(value: 0x000000000000f000_u64)
  let zero = count(value: 0_u64)
  print("Trailing ${trailing}/${zero}")
}
