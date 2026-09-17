fn wrap(value: UInt): UInt {
  return u64.wrappingSubtract(value, 1_u64)
}

entry {
  let wrapped = wrap(value: 0_u64)
  print("Wrapped ${wrapped}")
}
