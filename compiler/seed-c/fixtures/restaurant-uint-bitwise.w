fn combine(left: UInt, right: UInt): UInt {
  return left | right ^ left & right
}

entry {
  let high = 9223372036854775808_u64
  let lower = 9223372036854775807_u64
  let combined = combine(left: high, right: lower)
  print("UInt bits ${combined}")
}
