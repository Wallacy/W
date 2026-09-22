fn wrap(value: UInt): UInt {
  return u64.wrappingNegate(value)
}

entry {
  let wrapped = wrap(value: 1_u64)
  print("Wrapped ${wrapped}")
}
