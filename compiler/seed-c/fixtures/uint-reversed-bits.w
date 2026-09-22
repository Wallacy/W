fn reverse(value: UInt): UInt {
  return u64.reversedBits(value)
}

entry {
  let reversed = reverse(value: 0x0123456789abcdef_u64)
  print("Bits ${reversed}")
}
