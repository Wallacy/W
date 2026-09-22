fn wrap(value: UInt): UInt {
  return u64.wrappingAdd(value, 1_u64)
}

entry {
  let wrapped = wrap(value: 18446744073709551615_u64)
  print("Wrapped ${wrapped}")
}
