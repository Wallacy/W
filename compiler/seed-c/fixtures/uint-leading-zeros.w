fn count(value: UInt): UInt {
  return u64.countLeadingZeros(value)
}

entry {
  let leading = count(value: 0x00000000000000f0_u64)
  let zero = count(value: 0_u64)
  print("Leading ${leading}/${zero}")
}
