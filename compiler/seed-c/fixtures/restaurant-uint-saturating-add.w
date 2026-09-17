fn clamp(value: UInt): UInt {
  return u64.saturatingAdd(value, 1_u64)
}

entry {
  let maximum = clamp(value: 18446744073709551615_u64)
  let ordinary = clamp(value: 10_u64)
  print("Saturated ${maximum}/${ordinary}")
}
