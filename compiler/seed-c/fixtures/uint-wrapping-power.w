fn power(base: UInt, exponent: UInt): UInt {
  return u64.wrappingPower(base, exponent)
}

entry {
  let wrapped = power(base: 3_u64, exponent: 40_u64)
  print("Wrapped ${wrapped}")
}
