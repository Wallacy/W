fn clamp(value: UInt): UInt {
  return u64.saturatingSubtract(value, 1_u64)
}

entry {
  let zero = clamp(value: 0_u64)
  let ordinary = clamp(value: 11_u64)
  print("Saturated subtract ${zero}/${ordinary}")
}
