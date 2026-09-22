fn clamp(left: UInt, right: UInt): UInt {
  return u64.saturatingMultiply(left, right)
}

entry {
  let maximum = clamp(left: 18446744073709551615_u64, right: 2_u64)
  let ordinary = clamp(left: 6_u64, right: 7_u64)
  print("Saturated multiply ${maximum}/${ordinary}")
}
