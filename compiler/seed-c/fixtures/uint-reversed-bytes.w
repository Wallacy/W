fn reverse(value: UInt): UInt {
  return u64.reversedBytes(value)
}

entry {
  let bytes = reverse(value: 0x0123456789abcdef_u64)
  print("Bytes ${bytes}")
}
