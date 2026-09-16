fn identity(value: UInt): UInt {
  return value
}

entry {
  let value = identity(value: 18446744073709551615_u64)
  print("Unsigned ${value}")
}
